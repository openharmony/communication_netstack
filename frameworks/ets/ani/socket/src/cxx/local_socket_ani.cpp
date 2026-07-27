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

std::unique_ptr<LocalSocketContext> CreateLocalSocketContext()
{
    auto context = std::make_unique<LocalSocketContext>();
    context->socket_ = std::make_shared<NetStack::Socket::LocalSocket>();
    return context;
}

int32_t LocalSocketBind(LocalSocketContext& client, const FFILocalAddress& addr)
{
    return client.socket_->Bind(std::string(addr.address));
}

int32_t LocalSocketConnect(LocalSocketContext& client, const FFILocalConnectOptions& opts)
{
    return client.socket_->Connect(std::string(opts.address.address), opts.timeout);
}

int32_t LocalSocketSend(LocalSocketContext& client, const FFILocalSendOptions& opts)
{
    NetStack::Socket::LocalSocketOptions options;
    options.SetBuffer(const_cast<void*>(static_cast<const void*>(opts.data.data())), opts.data.size());
    options.SetEncoding(std::string(opts.encoding));
    return client.socket_->Send(options);
}

int32_t LocalSocketClose(LocalSocketContext& client)
{
    return client.socket_->Close();
}

int32_t LocalSocketGetState(LocalSocketContext& client, bool& isBound, bool& isClose, bool& isConnected)
{
    NetStack::Socket::SocketStateBase state;
    auto errorCode = client.socket_->GetState(state);
    if (errorCode == NetStack::Socket::SOCKET_ERROR_OK) {
        isBound = state.IsBound();
        isClose = state.IsClose();
        isConnected = state.IsConnected();
    }
    return errorCode;
}

int32_t LocalSocketGetSocketFd(LocalSocketContext& client, int32_t& fd)
{
    return client.socket_->GetSocketFd(fd);
}

int32_t LocalSocketSetExtraOptions(LocalSocketContext& client, const FFIExtraOptionsBase& opts)
{
    NetStack::Socket::LocalExtraOptions options;
    options.SetReceiveBufferSize(opts.receive_buffer_size);
    options.SetSendBufferSize(opts.send_buffer_size);
    options.SetReuseAddress(opts.reuse_address);
    options.SetSocketTimeout(opts.socket_timeout);
    return client.socket_->SetExtraOptions(options);
}

int32_t LocalSocketGetExtraOptions(LocalSocketContext& client, FFIExtraOptionsBase& opts)
{
    NetStack::Socket::LocalExtraOptions options;
    auto errorCode = client.socket_->GetExtraOptions(options);
    if (errorCode == NetStack::Socket::SOCKET_ERROR_OK) {
        opts.has_receive_buffer_size = true;
        opts.receive_buffer_size = options.GetReceiveBufferSize();
        opts.has_send_buffer_size = true;
        opts.send_buffer_size = options.GetSendBufferSize();
        opts.has_reuse_address = true;
        opts.reuse_address = options.IsReuseAddress();
        opts.has_socket_timeout = options.GetSocketTimeout() > 0;
        opts.socket_timeout = options.GetSocketTimeout();
    }
    return errorCode;
}

int32_t LocalSocketGetLocalAddress(LocalSocketContext& client, rust::String& address)
{
    std::string addr{};
    auto errorCode = client.socket_->GetLocalAddress(addr);
    address = addr;
    return errorCode;
}

int32_t LocalSocketOnMessage(LocalSocketContext& client, rust::Box<LocalSocketMessageBox> callbackBox)
{
    client.messageBoxHolder_ = std::make_shared<rust::Box<LocalSocketMessageBox>>(std::move(callbackBox));
    std::weak_ptr<rust::Box<LocalSocketMessageBox>> messageBoxWeak = client.messageBoxHolder_;
    client.socket_->OnMessage([messageBoxWeak]
        (const std::string &data, const std::string address, const int32_t size) {
        if (auto messageBox = messageBoxWeak.lock()) {
            rust::Vec<uint8_t> vecData;
            for (const auto &byte : data) {
                vecData.push_back(static_cast<uint8_t>(byte));
            }
            (*messageBox)->on_message(vecData, rust::String(address), size);
        }
    });
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t LocalSocketOnConnect(LocalSocketContext& client, rust::Box<LocalSocketConnectBox> callbackBox)
{
    client.connectBoxHolder_ = std::make_shared<rust::Box<LocalSocketConnectBox>>(std::move(callbackBox));
    std::weak_ptr<rust::Box<LocalSocketConnectBox>> connectBoxWeak = client.connectBoxHolder_;
    client.socket_->OnConnect([connectBoxWeak]() {
        if (auto connectBox = connectBoxWeak.lock()) {
            (*connectBox)->on_connect();
        }
    });
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t LocalSocketOnClose(LocalSocketContext& client, rust::Box<LocalSocketCloseBox> callbackBox)
{
    client.closeBoxHolder_ = std::make_shared<rust::Box<LocalSocketCloseBox>>(std::move(callbackBox));
    std::weak_ptr<rust::Box<LocalSocketCloseBox>> closeBoxWeak = client.closeBoxHolder_;
    client.socket_->OnClose([closeBoxWeak]() {
        if (auto closeBox = closeBoxWeak.lock()) {
            (*closeBox)->on_close();
        }
    });
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t LocalSocketOnError(LocalSocketContext& client, rust::Box<LocalSocketErrorBox> callbackBox)
{
    client.errorBoxHolder_ = std::make_shared<rust::Box<LocalSocketErrorBox>>(std::move(callbackBox));
    std::weak_ptr<rust::Box<LocalSocketErrorBox>> errorBoxWeak = client.errorBoxHolder_;
    client.socket_->OnError([errorBoxWeak](int32_t err, const std::string &errString) {
        if (auto errorBox = errorBoxWeak.lock()) {
            rust::String errorString(errString);
            (*errorBox)->on_error(err, errorString);
        }
    });
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t LocalSocketOffMessage(LocalSocketContext& client)
{
    client.socket_->OffMessage();
    client.messageBoxHolder_ = nullptr;
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t LocalSocketOffConnect(LocalSocketContext& client)
{
    client.socket_->OffConnect();
    client.connectBoxHolder_ = nullptr;
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t LocalSocketOffClose(LocalSocketContext& client)
{
    client.socket_->OffClose();
    client.closeBoxHolder_ = nullptr;
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t LocalSocketOffError(LocalSocketContext& client)
{
    client.socket_->OffError();
    client.errorBoxHolder_ = nullptr;
    return NetStack::Socket::SOCKET_ERROR_OK;
}

std::unique_ptr<LocalSocketConnectionContext> CreateLocalSocketConnectionContext(
    LocalSocketServerContext &server, int32_t clientId)
{
    if (server.socket_ == nullptr) {
        return nullptr;
    }
    auto context = std::make_unique<LocalSocketConnectionContext>();
    context->clientId_ = clientId;
    context->socket_ = server.socket_;
    return context;
}

int32_t LocalSocketConnectionSend(LocalSocketConnectionContext& connection, const FFILocalSendOptions& opts)
{
    auto server = connection.socket_.lock();
    if (server != nullptr) {
        NetStack::Socket::LocalSocketOptions options;
        options.SetBuffer(const_cast<void*>(static_cast<const void*>(opts.data.data())), opts.data.size());
        options.SetEncoding(std::string(opts.encoding));
        if (auto localConn = server->GetConnectionByClientID(connection.clientId_)) {
            return localConn->Send(options);
        }
    }
    return OHOS::NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t LocalSocketConnectionClose(LocalSocketConnectionContext& connection)
{
    auto server = connection.socket_.lock();
    if (server != nullptr) {
        if (auto localConn = server->GetConnectionByClientID(connection.clientId_)) {
            return localConn->Close();
        }
    }
    return OHOS::NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t LocalSocketConnectionGetLocalAddress(LocalSocketConnectionContext& connection, rust::String& address)
{
    auto server = connection.socket_.lock();
    if (server != nullptr) {
        std::string addr;
        if (auto localConn = server->GetConnectionByClientID(connection.clientId_)) {
            auto errorCode = localConn->GetLocalAddress(addr);
            if (errorCode == NetStack::Socket::SOCKET_ERROR_OK) {
                address = addr;
            }
            return errorCode;
        }
    }
    return OHOS::NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t LocalSocketConnectionGetSocketFd(LocalSocketConnectionContext& connection, int32_t &fd)
{
    auto server = connection.socket_.lock();
    if (server != nullptr) {
        if (auto localConn = server->GetConnectionByClientID(connection.clientId_)) {
            return localConn->GetSocketFd(fd);
        }
    }
    return OHOS::NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t LocalSocketConnectionOnMessage(LocalSocketConnectionContext& client,
    rust::Box<LocalSocketConnectionMessageBox> callbackBox)
{
    client.messageBoxHolder_ = std::make_shared<rust::Box<LocalSocketConnectionMessageBox>>(std::move(callbackBox));
    auto server = client.socket_.lock();
    if (server == nullptr) {
        return NetStack::Socket::SOCKET_ERROR_OK;
    }
    auto localConn = server->GetConnectionByClientID(client.clientId_);
    if (localConn == nullptr) {
        return NetStack::Socket::SOCKET_ERROR_OK;
    }
    std::weak_ptr<rust::Box<LocalSocketConnectionMessageBox>> messageBoxWeak = client.messageBoxHolder_;
    localConn->OnMessage([messageBoxWeak]
        (const std::string &data, const std::string address, const int32_t size) {
        if (auto messageBox = messageBoxWeak.lock()) {
            rust::Vec<uint8_t> vecData;
            for (const auto &byte : data) {
                vecData.push_back(static_cast<uint8_t>(byte));
            }
            (*messageBox)->on_message(vecData, rust::String(address), size);
        }
    });
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t LocalSocketConnectionOnClose(LocalSocketConnectionContext& client,
    rust::Box<LocalSocketConnectionCloseBox> callbackBox)
{
    client.closeBoxHolder_ = std::make_shared<rust::Box<LocalSocketConnectionCloseBox>>(std::move(callbackBox));
    auto server = client.socket_.lock();
    if (server != nullptr) {
        auto localConn = server->GetConnectionByClientID(client.clientId_);
        if (localConn == nullptr) {
            return NetStack::Socket::SOCKET_ERROR_OK;
        }
        std::weak_ptr<rust::Box<LocalSocketConnectionCloseBox>> closeBoxWeak = client.closeBoxHolder_;
        localConn->OnClose([closeBoxWeak]() {
            if (auto closeBox = closeBoxWeak.lock()) {
                (*closeBox)->on_close();
            }
        });
    }
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t LocalSocketConnectionOnError(LocalSocketConnectionContext& client,
    rust::Box<LocalSocketConnectionErrorBox> callbackBox)
{
    client.errorBoxHolder_ = std::make_shared<rust::Box<LocalSocketConnectionErrorBox>>(std::move(callbackBox));
    auto server = client.socket_.lock();
    if (server != nullptr) {
        auto localConn = server->GetConnectionByClientID(client.clientId_);
        if (localConn == nullptr) {
            return NetStack::Socket::SOCKET_ERROR_OK;
        }
        std::weak_ptr<rust::Box<LocalSocketConnectionErrorBox>> errorBoxWeak = client.errorBoxHolder_;
        localConn->OnError([errorBoxWeak](int32_t err, const std::string &errString) {
            if (auto errorBox = errorBoxWeak.lock()) {
                rust::String errorString(errString);
                (*errorBox)->on_error(err, errorString);
            }
        });
    }
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t LocalSocketConnectionOffMessage(LocalSocketConnectionContext& client)
{
    auto server = client.socket_.lock();
    if (server != nullptr) {
        if (auto localConn = server->GetConnectionByClientID(client.clientId_)) {
            localConn->OffMessage();
        }
    }
    client.messageBoxHolder_ = nullptr;
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t LocalSocketConnectionOffClose(LocalSocketConnectionContext& client)
{
    auto server = client.socket_.lock();
    if (server != nullptr) {
        if (auto localConn = server->GetConnectionByClientID(client.clientId_)) {
            localConn->OffClose();
        }
    }
    client.closeBoxHolder_ = nullptr;
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t LocalSocketConnectionOffError(LocalSocketConnectionContext& client)
{
    auto server = client.socket_.lock();
    if (server != nullptr) {
        if (auto localConn = server->GetConnectionByClientID(client.clientId_)) {
            localConn->OffError();
        }
    }
    client.errorBoxHolder_ = nullptr;
    return NetStack::Socket::SOCKET_ERROR_OK;
}

std::unique_ptr<LocalSocketServerContext> CreateLocalSocketServerContext()
{
    auto context = std::make_unique<LocalSocketServerContext>();
    context->socket_ = std::make_shared<NetStack::Socket::LocalSocketServer>();
    return context;
}

int32_t LocalSocketServerListen(LocalSocketServerContext& server, const FFILocalAddress& addr)
{
    if (!server.socket_) {
        return NetStack::Socket::PARAM_ERROR_CODE;
    }
    return server.socket_->Listen(std::string(addr.address));
}

int32_t LocalSocketServerGetState(LocalSocketServerContext& server, bool& isBound, bool& isClose, bool& isConnected)
{
    if (!server.socket_) {
        return NetStack::Socket::PARAM_ERROR_CODE;
    }
    NetStack::Socket::SocketStateBase state;
    auto errorCode = server.socket_->GetState(state);
    if (errorCode == NetStack::Socket::SOCKET_ERROR_OK) {
        isBound = state.IsBound();
        isClose = state.IsClose();
        isConnected = state.IsConnected();
    }
    return errorCode;
}

int32_t LocalSocketServerGetSocketFd(LocalSocketServerContext& server, int32_t& fd)
{
    if (!server.socket_) {
        return NetStack::Socket::PARAM_ERROR_CODE;
    }
    return server.socket_->GetSocketFd(fd);
}

int32_t LocalSocketServerSetExtraOptions(LocalSocketServerContext& server, const FFIExtraOptionsBase& opts)
{
    if (!server.socket_) {
        return NetStack::Socket::PARAM_ERROR_CODE;
    }
    NetStack::Socket::LocalExtraOptions options;
    options.SetRecvBufSizeFlag(opts.receive_buffer_size);
    options.SetSendBufferSize(opts.send_buffer_size);
    options.SetReuseAddress(opts.reuse_address);
    options.SetSocketTimeout(opts.socket_timeout);

    return server.socket_->SetExtraOptions(options);
}

int32_t LocalSocketServerGetExtraOptions(LocalSocketServerContext& server, FFIExtraOptionsBase& opts)
{
    if (!server.socket_) {
        return NetStack::Socket::PARAM_ERROR_CODE;
    }
    NetStack::Socket::LocalExtraOptions options;
    auto errerCode = server.socket_->GetExtraOptions(options);
    if (errerCode == NetStack::Socket::SOCKET_ERROR_OK) {
        opts.has_receive_buffer_size = true;
        opts.receive_buffer_size = options.GetReceiveBufferSize();
        opts.has_send_buffer_size = true;
        opts.send_buffer_size = options.GetSendBufferSize();
        opts.has_reuse_address = true;
        opts.reuse_address = options.IsReuseAddress();
        opts.has_socket_timeout = options.GetSocketTimeout() >0;
        opts.socket_timeout = options.GetSocketTimeout();
    }

    return errerCode;
}

int32_t LocalSocketServerGetLocalAddress(LocalSocketServerContext& server, rust::String& address)
{
    if (!server.socket_) {
        return NetStack::Socket::PARAM_ERROR_CODE;
    }
    std::string addr;
    auto errorCode = server.socket_->GetLocalAddress(addr);
    if (errorCode == NetStack::Socket::SOCKET_ERROR_OK) {
        address = addr;
    }

    return errorCode;
}

int32_t LocalSocketServerClose(LocalSocketServerContext& server)
{
    if (!server.socket_) {
        return NetStack::Socket::PARAM_ERROR_CODE;
    }

    int32_t fd = -1;
    server.socket_->GetSocketFd(fd);
    NetStack::Socket::SocketStateBase state;
    server.socket_->GetState(state);
    if (fd < 0 && !state.IsClose()) {
        return NetStack::Socket::SYSTEM_INTERNAL_ERROR;
    }

    return server.socket_->Close();
}

int32_t LocalSocketServerOnConnect(LocalSocketServerContext& server, rust::Box<LocalSocketServerConnectBox> callbackBox)
{
    if (!server.socket_) {
        return NetStack::Socket::PARAM_ERROR_CODE;
    }

    server.connectBoxHolder_ = std::make_shared<rust::Box<LocalSocketServerConnectBox>>(std::move(callbackBox));
    std::weak_ptr<rust::Box<LocalSocketServerConnectBox>> connectBoxWeak = server.connectBoxHolder_;
    server.socket_->OnConnect([connectBoxWeak](const int &clientId) {
        if (auto connectBox = connectBoxWeak.lock()) {
            (*connectBox)->on_connect(clientId);
        }
    });
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t LocalSocketServerOnError(LocalSocketServerContext& server, rust::Box<LocalSocketServerErrorBox> callbackBox)
{
    if (!server.socket_) {
        return NetStack::Socket::PARAM_ERROR_CODE;
    }

    server.errorBoxHolder_ = std::make_shared<rust::Box<LocalSocketServerErrorBox>>(std::move(callbackBox));
    std::weak_ptr<rust::Box<LocalSocketServerErrorBox>> errorBoxWeak = server.errorBoxHolder_;
    server.socket_->OnError([errorBoxWeak](int32_t err, const std::string &errString) {
        if (auto errorBox = errorBoxWeak.lock()) {
            (*errorBox)->on_error(err, rust::String(errString));
        }
    });
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t LocalSocketServerOffConnect(LocalSocketServerContext& server)
{
    server.connectBoxHolder_ = nullptr;
    if (!server.socket_) {
        return NetStack::Socket::PARAM_ERROR_CODE;
    }
    server.socket_->OffConnect();
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t LocalSocketServerOffError(LocalSocketServerContext& server)
{
    server.errorBoxHolder_ = nullptr;
    if (!server.socket_) {
        return NetStack::Socket::PARAM_ERROR_CODE;
    }
    server.socket_->OffError();
    return NetStack::Socket::SOCKET_ERROR_OK;
}

} // namespace NetStackAni
} // namespace OHOS