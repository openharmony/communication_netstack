/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "socket_ani.h"
#include "ffi_convert.h"
#include "netstack_common_utils.h"
#include "netstack_log.h"

namespace OHOS {
namespace NetStackAni {

using namespace NetStack;


std::unique_ptr<TcpSocketContext> CreateTcpSocketContext()
{
    auto context = std::make_unique<TcpSocketContext>();
    context->socket_ = std::make_shared<NetStack::Socket::TCPSocket>();
    return context;
}

int32_t TcpSocketBind(TcpSocketContext& client, const FFINetAddress& addr)
{
    NetStack::Socket::NetAddress netAddr;
    netAddr.SetFamilyByJsValue(addr.family);
    netAddr.SetPort(addr.port);
    netAddr.SetIpAddress(std::string(addr.address));
    return client.socket_->Bind(netAddr);
}

int32_t TcpSocketConnect(TcpSocketContext& client, const FFITcpConnectOptions& opts)
{
    NetStack::Socket::TcpConnectOptions connectOptions;
    connectOptions.address.SetFamilyByJsValue(opts.address.family);
    connectOptions.address.SetAddress(std::string(opts.address.address));
    connectOptions.address.SetPort(opts.address.port);
    connectOptions.SetTimeout(static_cast<uint32_t>(opts.timeout));
    NetStack::Socket::ProxyOptions proxyOptions;
    return client.socket_->Connect(connectOptions, proxyOptions);
}

int32_t TcpSocketSend(TcpSocketContext& client, const FFITcpSendOptions& opts)
{
    NetStack::Socket::TCPSendOptions sendOptions;
    sendOptions.SetData(const_cast<void*>(static_cast<const void*>(opts.data.data())), opts.data.size());
    sendOptions.SetEncoding(std::string(opts.encoding));
    return client.socket_->Send(sendOptions);
}

int32_t TcpSocketClose(TcpSocketContext& client)
{
    return client.socket_->Close();
}

int32_t TcpSocketGetState(TcpSocketContext& client, bool& isBound, bool& isClose, bool& isConnected)
{
    NetStack::Socket::SocketStateBase state;
    auto errCode = client.socket_->GetState(state);
    if (errCode == NetStack::Socket::SOCKET_ERROR_OK) {
        isBound = state.IsBound();
        isClose = state.IsClose();
        isConnected = state.IsConnected();
    }
    return errCode;
}

int32_t TcpSocketGetRemoteAddress(TcpSocketContext& client, rust::String& address, int32_t& family, int32_t& port)
{
    NetStack::Socket::NetAddress netAddr;
    auto errCode = client.socket_->GetRemoteAddress(netAddr);
    if (errCode == NetStack::Socket::SOCKET_ERROR_OK) {
        address = netAddr.GetAddress();
        family = netAddr.GetJsValueFamily();
        port = netAddr.GetPort();
    }
    return errCode;
}

int32_t TcpSocketGetLocalAddress(TcpSocketContext& client, rust::String& address, int32_t& family, int32_t& port)
{
    NetStack::Socket::NetAddress netAddr;
    auto errCode = client.socket_->GetLocalAddress(netAddr);
    if (errCode == NetStack::Socket::SOCKET_ERROR_OK) {
        address = netAddr.GetAddress();
        family = netAddr.GetJsValueFamily();
        port = netAddr.GetPort();
    }
    return errCode;
}

int32_t TcpSocketSetExtraOptions(TcpSocketContext& client, const FFITcpExtraOptions& opts)
{
    NetStack::Socket::TCPExtraOptions extraOptions;

    FFITcpExtraOptionsToTCPExtraOptions(opts, extraOptions);

    return client.socket_->SetExtraOptions(extraOptions);
}

int32_t TcpClientGetSocketFd(TcpSocketContext& client, int32_t& fd)
{
    return client.socket_->GetSocketFd(fd);
}

int32_t TcpSocketOnMessage(TcpSocketContext& client, rust::Box<TcpSocketMessageBox> callbackBox)
{
    client.messageBoxHolder_ = std::make_shared<rust::Box<TcpSocketMessageBox>>(std::move(callbackBox));
    std::weak_ptr<rust::Box<TcpSocketMessageBox>> messageBoxWeak = client.messageBoxHolder_;
    client.socket_->OnMessage(
        [messageBoxWeak](const std::string &data, const NetStack::Socket::SocketRemoteInfo &remoteInfo) {
        if (auto messageBox = messageBoxWeak.lock()) {
            rust::Vec<uint8_t> vecData;
            for (const auto &byte : data) {
                vecData.push_back(static_cast<uint8_t>(byte));
            }
            rust::String address(remoteInfo.GetAddress());
            rust::String family(remoteInfo.GetFamily());
            int32_t port = static_cast<int32_t>(remoteInfo.GetPort());
            int32_t size = static_cast<int32_t>(remoteInfo.GetSize());
            (*messageBox)->on_message(vecData, address, family, port, size);
        }
    });
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t TcpSocketOnConnect(TcpSocketContext& client, rust::Box<TcpSocketConnectBox> callbackBox)
{
    client.connectBoxHolder_ = std::make_shared<rust::Box<TcpSocketConnectBox>>(std::move(callbackBox));
    std::weak_ptr<rust::Box<TcpSocketConnectBox>> connectBoxWeak = client.connectBoxHolder_;
    client.socket_->OnConnect([connectBoxWeak]() {
        if (auto connectBox = connectBoxWeak.lock()) {
            (*connectBox)->on_connect();
        }
    });
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t TcpSocketOnClose(TcpSocketContext& client, rust::Box<TcpSocketCloseBox> callbackBox)
{
    client.closeBoxHolder_ = std::make_shared<rust::Box<TcpSocketCloseBox>>(std::move(callbackBox));
    std::weak_ptr<rust::Box<TcpSocketCloseBox>> closeBoxWeak = client.closeBoxHolder_;
    client.socket_->OnClose([closeBoxWeak]() {
        if (auto closeBox = closeBoxWeak.lock()) {
            (*closeBox)->on_close();
        }
    });
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t TcpSocketOnError(TcpSocketContext& client, rust::Box<TcpSocketErrorBox> callbackBox)
{
    client.errorBoxHolder_ = std::make_shared<rust::Box<TcpSocketErrorBox>>(std::move(callbackBox));
    std::weak_ptr<rust::Box<TcpSocketErrorBox>> errorBoxWeak = client.errorBoxHolder_;
    client.socket_->OnError([errorBoxWeak](int32_t err, const std::string &errString) {
        if (auto errorBox = errorBoxWeak.lock()) {
            rust::String errorString(errString);
            (*errorBox)->on_error(err, errorString);
        }
    });
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t TcpSocketOffMessage(TcpSocketContext& client)
{
    client.socket_->OffMessage();
    client.messageBoxHolder_ = nullptr;
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t TcpSocketOffConnect(TcpSocketContext& client)
{
    client.socket_->OffConnect();
    client.connectBoxHolder_ = nullptr;
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t TcpSocketOffClose(TcpSocketContext& client)
{
    client.socket_->OffClose();
    client.closeBoxHolder_ = nullptr;
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t TcpSocketOffError(TcpSocketContext& client)
{
    client.socket_->OffError();
    client.errorBoxHolder_ = nullptr;
    return NetStack::Socket::SOCKET_ERROR_OK;
}

// TCP Server
std::unique_ptr<TcpSocketServerContext> CreateTcpSocketServerContext()
{
    auto context = std::make_unique<TcpSocketServerContext>();
    context->socket_ = std::make_shared<NetStack::Socket::TCPSocketServer>();
    return context;
}

int32_t TcpSocketServerListen(TcpSocketServerContext& server, const FFINetAddress& addr)
{
    NetStack::Socket::NetAddress netAddr;
    netAddr.SetFamilyByJsValue(addr.family);
    netAddr.SetPort(addr.port);
    netAddr.SetIpAddress(std::string(addr.address));
    return server.socket_->Listen(netAddr);
}

int32_t TcpSocketServerGetState(TcpSocketServerContext& server, bool& isBound, bool& isClose, bool& isConnected)
{
    NetStack::Socket::SocketStateBase state;
    auto errCode = server.socket_->GetState(state);
    if (errCode == NetStack::Socket::SOCKET_ERROR_OK) {
        isBound = state.IsBound();
        isClose = state.IsClose();
        isConnected = state.IsConnected();
    }
    return errCode;
}

int32_t TcpSocketServerSetExtraOptions(TcpSocketServerContext& server, const FFITcpExtraOptions& opts)
{
    NetStack::Socket::TCPExtraOptions extraOptions;
    FFITcpExtraOptionsToTCPExtraOptions(opts, extraOptions);
    if (static_cast<int32_t>(extraOptions.GetSocketTimeout()) < 0) {
        extraOptions.SetTimeoutFlag(false);
    }
    return server.socket_->SetExtraOptions(extraOptions);
}

int32_t TcpSocketServerGetSocketFd(TcpSocketServerContext& server, int32_t& fd)
{
    return server.socket_->GetSocketFd(fd);
}

int32_t TcpSocketServerClose(TcpSocketServerContext& server)
{
    return server.socket_->Close();
}

int32_t TcpSocketServerGetLocalAddress(TcpSocketServerContext& server, rust::String& address,
    int32_t& family, int32_t& port)
{
    NetStack::Socket::NetAddress netAddr;
    auto errCode = server.socket_->GetLocalAddress(netAddr);
    if (errCode == NetStack::Socket::SOCKET_ERROR_OK) {
        address = netAddr.GetAddress();
        family = netAddr.GetJsValueFamily();
        port = netAddr.GetPort();
    }
    return errCode;
}

int32_t TcpSocketServerOnConnect(TcpSocketServerContext& server, rust::Box<TcpSocketServerConnectBox> callbackBox)
{
    server.connectBoxHolder_ = std::make_shared<rust::Box<TcpSocketServerConnectBox>>(std::move(callbackBox));
    std::weak_ptr<rust::Box<TcpSocketServerConnectBox>> connectBoxWeak = server.connectBoxHolder_;
    server.socket_->OnConnect([connectBoxWeak](const int &clientId) {
        if (auto connectBox = connectBoxWeak.lock()) {
            (*connectBox)->on_connect(clientId);
        }
    });
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t TcpSocketServerOnError(TcpSocketServerContext& server, rust::Box<TcpSocketServerErrorBox> callbackBox)
{
    server.errorBoxHolder_ = std::make_shared<rust::Box<TcpSocketServerErrorBox>>(std::move(callbackBox));
    std::weak_ptr<rust::Box<TcpSocketServerErrorBox>> errorBoxWeak = server.errorBoxHolder_;
    server.socket_->OnError([errorBoxWeak](int32_t err, const std::string &errString) {
        if (auto errorBox = errorBoxWeak.lock()) {
            rust::String errorString(errString);
            (*errorBox)->on_error(err, errorString);
        }
    });
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t TcpSocketServerOffConnect(TcpSocketServerContext& server)
{
    server.socket_->OffConnect();
    server.connectBoxHolder_ = nullptr;
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t TcpSocketServerOffError(TcpSocketServerContext& server)
{
    server.socket_->OffError();
    server.errorBoxHolder_ = nullptr;
    return NetStack::Socket::SOCKET_ERROR_OK;
}

// TCP Connect
std::unique_ptr<TcpSocketConnectionContext> CreateTcpSocketConnectionContext(
    TcpSocketServerContext& server, int32_t clientId)
{
    auto context = std::make_unique<TcpSocketConnectionContext>();
    context->clientId_ = clientId;
    context->connection_ = server.socket_->GetConnectionByClientID(clientId);
    return context;
}

int32_t TcpSocketConnectionSend(TcpSocketConnectionContext& connection, const FFITcpSendOptions& opts)
{
    if (auto conn = connection.connection_.lock()) {
        NetStack::Socket::TCPSendOptions sendOptions;
        sendOptions.SetData(const_cast<void*>(static_cast<const void*>(opts.data.data())), opts.data.size());
        sendOptions.SetEncoding(std::string(opts.encoding));
        return conn->Send(sendOptions);
    }
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t TcpSocketConnectionClose(TcpSocketConnectionContext& connection)
{
    if (auto conn = connection.connection_.lock()) {
        return conn->Close();
    }
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t TcpSocketConnectionGetRemoteAddress(TcpSocketConnectionContext& connection, rust::String& address,
                                            int32_t& family, int32_t& port)
{
    if (auto conn = connection.connection_.lock()) {
        NetStack::Socket::NetAddress netAddr;
        auto errCode = conn->GetRemoteAddress(netAddr);
        if (errCode == NetStack::Socket::SOCKET_ERROR_OK) {
            address = netAddr.GetAddress();
            family = netAddr.GetJsValueFamily();
            port = netAddr.GetPort();
        }
        return errCode;
    }
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t TcpSocketConnectionGetLocalAddress(TcpSocketConnectionContext& connection, rust::String& address,
                                           int32_t& family, int32_t& port)
{
    if (auto conn = connection.connection_.lock()) {
        NetStack::Socket::NetAddress netAddr;
        auto errCode = conn->GetLocalAddress(netAddr);
        if (errCode == NetStack::Socket::SOCKET_ERROR_OK) {
            address = netAddr.GetAddress();
            family = netAddr.GetJsValueFamily();
            port = netAddr.GetPort();
        }
        return errCode;
    }
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t TcpSocketConnectionGetSocketFd(TcpSocketConnectionContext& connection, int32_t &fd)
{
    if (auto conn = connection.connection_.lock()) {
        return conn->GetSocketFd(fd);
    }
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t TcpSocketConnectionOnMessage(TcpSocketConnectionContext& connection,
                                     rust::Box<TcpSocketConnectionMessageBox> callbackBox)
{
    connection.messageBoxHolder_ = std::make_shared<rust::Box<TcpSocketConnectionMessageBox>>(std::move(callbackBox));
    auto conn = connection.connection_.lock();
    if (conn == nullptr) {
        return NetStack::Socket::SOCKET_ERROR_OK;
    }
    std::weak_ptr<rust::Box<TcpSocketConnectionMessageBox>> messageBoxWeak = connection.messageBoxHolder_;
    conn->OnMessage([messageBoxWeak]
        (const std::string &data, const NetStack::Socket::SocketRemoteInfo &remoteInfo) {
        if (auto messageBox = messageBoxWeak.lock()) {
            rust::Vec<uint8_t> vecData;
            for (const auto &byte : data) {
                vecData.push_back(static_cast<uint8_t>(byte));
            }
            (*messageBox)->on_message(vecData, remoteInfo.GetAddress(), remoteInfo.GetFamily(),
                remoteInfo.GetPort(), remoteInfo.GetSize());
        }
    });
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t TcpSocketConnectionOnClose(TcpSocketConnectionContext& connection,
                                   rust::Box<TcpSocketConnectionCloseBox> callbackBox)
{
    connection.closeBoxHolder_ = std::make_shared<rust::Box<TcpSocketConnectionCloseBox>>(std::move(callbackBox));
    std::weak_ptr<rust::Box<TcpSocketConnectionCloseBox>> closeBoxWeak = connection.closeBoxHolder_;
    if (auto conn = connection.connection_.lock()) {
        conn->OnClose([closeBoxWeak]() {
            if (auto closeBox = closeBoxWeak.lock()) {
                (*closeBox)->on_close();
            }
        });
    }
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t TcpSocketConnectionOnError(TcpSocketConnectionContext& connection,
                                   rust::Box<TcpSocketConnectionErrorBox> callbackBox)
{
    connection.errorBoxHolder_ = std::make_shared<rust::Box<TcpSocketConnectionErrorBox>>(std::move(callbackBox));
    if (auto conn = connection.connection_.lock()) {
        std::weak_ptr<rust::Box<TcpSocketConnectionErrorBox>> errorBoxWeak = connection.errorBoxHolder_;
        conn->OnError([errorBoxWeak](int32_t err, const std::string &errString) {
            if (auto errorBox = errorBoxWeak.lock()) {
                rust::String errorString(errString);
                (*errorBox)->on_error(err, errorString);
            }
        });
    }
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t TcpSocketConnectionOffMessage(TcpSocketConnectionContext& connection)
{
    if (auto conn = connection.connection_.lock()) {
        conn->OffMessage();
    }
    connection.messageBoxHolder_ =nullptr;
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t TcpSocketConnectionOffClose(TcpSocketConnectionContext& connection)
{
    if (auto conn = connection.connection_.lock()) {
        conn->OffClose();
    }
    connection.closeBoxHolder_ = nullptr;
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t TcpSocketConnectionOffError(TcpSocketConnectionContext& connection)
{
    if (auto conn = connection.connection_.lock()) {
        conn->OffError();
    }
    connection.errorBoxHolder_ = nullptr;
    return NetStack::Socket::SOCKET_ERROR_OK;
}

} // namespace NetStackAni
} // namespace OHOS