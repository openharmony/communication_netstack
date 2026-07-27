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

std::unique_ptr<UdpSocketContext> CreateUdpSocketContext()
{
    auto context = std::make_unique<UdpSocketContext>();
    context->socket_ = std::make_shared<NetStack::Socket::UDPSocket>();
    return context;
}

std::unique_ptr<UdpSocketContext> CreateMulticastClientContext()
{
    auto context = std::make_unique<MulticastClientContext>();
    context->socket_ = std::make_shared<NetStack::Socket::MulticastSocket>();
    return context;
}

int32_t UdpClientBind(UdpSocketContext& client, const FFINetAddress& addr)
{
    NetStack::Socket::NetAddress netAddr;
    if (addr.has_family) {
        netAddr.SetFamilyByJsValue(static_cast<sa_family_t>(addr.family));
    }
    if (addr.has_port) {
        netAddr.SetPort(static_cast<uint16_t>(addr.port));
    }
    netAddr.SetIpAddress(std::string(addr.address));
    return client.socket_->Bind(netAddr);
}

int32_t UdpClientSend(UdpSocketContext& client, const FFIUdpSendOptions& sendOpts, const FFIProxyOptions& proxyOpts)
{
    NetStack::Socket::UDPSendOptions sendOptions;
    NetStack::Socket::ProxyOptions proxyOptions;
    sendOptions.SetData(const_cast<void*>(static_cast<const void*>(sendOpts.data.data())), sendOpts.data.size());
    sendOptions.address.SetAddress(std::string(sendOpts.address.address));
    sendOptions.address.SetFamilyByJsValue(sendOpts.address.family);
    sendOptions.address.SetPort(sendOpts.address.port);

    FFIProxyOptionsToProxyOptions(proxyOpts, proxyOptions);

    return client.socket_->Send(sendOptions, proxyOptions);
}

int32_t UdpClientClose(UdpSocketContext& client)
{
    return client.socket_->Close();
}

int32_t UdpClientGetState(UdpSocketContext& client, bool& isBound, bool& isClose, bool& isConnected)
{
    NetStack::Socket::SocketStateBase state;
    auto errCode = client.socket_->GetState(state);
    if (errCode ==  NetStack::Socket::SOCKET_ERROR_OK) {
        isBound = state.IsBound();
        isClose = state.IsClose();
        isConnected = state.IsConnected();
    }
    return errCode;
}

int32_t UdpClientSetExtraOptions(UdpSocketContext& client, const FFIUdpExtraOptions& opts)
{
    NetStack::Socket::UDPExtraOptions extraOptions;
    if (opts.has_receive_buffer_size) {
        extraOptions.SetReceiveBufferSize(static_cast<int32_t>(opts.receive_buffer_size));
        extraOptions.SetRecvBufSizeFlag(true);
    }
    if (opts.has_send_buffer_size) {
        extraOptions.SetSendBufferSize(static_cast<int32_t>(opts.send_buffer_size));
        extraOptions.SetSendBufSizeFlag(true);
    }
    if (opts.has_reuse_address) {
        extraOptions.SetReuseAddress(static_cast<int32_t>(opts.reuse_address));
        extraOptions.SetReuseaddrFlag(true);
    }
    if (opts.has_socket_timeout) {
        extraOptions.SetSocketTimeout(static_cast<int32_t>(opts.socket_timeout));
        extraOptions.SetTimeoutFlag(true);
    }
    if (opts.has_broadcast) {
        extraOptions.SetBroadcast(static_cast<int32_t>(opts.broadcast));
        extraOptions.SetBroadcastFlag(true);
    }
    return client.socket_->SetExtraOptions(extraOptions);
}

int32_t UdpClientGetSocketFd(UdpSocketContext& client, int32_t& fd)
{
    return client.socket_->GetSocketFd(fd);
}

int32_t UdpClientGetLocalAddress(UdpSocketContext& client, rust::String& address, int32_t& family, int32_t& port)
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

int32_t UdpClientAddMembership(UdpSocketContext& client, rust::Str multicastAddr,
                               const int32_t family, const int32_t port)
{
    if (!client.isMulticast()) {
        return NetStack::Socket::PARAM_ERROR_CODE;
    }
    auto socket = static_cast<NetStack::Socket::MulticastSocket *>(client.socket_.get());
    NetStack::Socket::NetAddress addr;
    addr.SetAddress(std::string(multicastAddr));
    addr.SetFamilyByJsValue(family);
    addr.SetPort(port);
    return socket->AddMembership(addr);
}

int32_t UdpClientDropMembership(UdpSocketContext& client, rust::Str multicastAddr,
                                const int32_t family, const int32_t port)
{
    if (!client.isMulticast()) {
        return NetStack::Socket::PARAM_ERROR_CODE;
    }
    auto socket = static_cast<NetStack::Socket::MulticastSocket *>(client.socket_.get());
    NetStack::Socket::NetAddress addr;
    addr.SetAddress(std::string(multicastAddr));
    addr.SetFamilyByJsValue(family);
    addr.SetPort(port);
    return socket->DropMembership(addr);
}

int32_t UdpClientSetTtl(UdpSocketContext& client, int32_t ttl)
{
    if (!client.isMulticast()) {
        return NetStack::Socket::PARAM_ERROR_CODE;
    }
    auto socket = static_cast<NetStack::Socket::MulticastSocket *>(client.socket_.get());
    return socket->SetMulticastTTL(ttl);
}

int32_t UdpClientGetTtl(UdpSocketContext& client, int32_t& ttl)
{
    if (!client.isMulticast()) {
        return NetStack::Socket::PARAM_ERROR_CODE;
    }
    auto socket = static_cast<NetStack::Socket::MulticastSocket *>(client.socket_.get());
    ttl = 0;
    return socket->GetMulticastTTL(ttl);
}

int32_t UdpClientSetLoopbackMode(UdpSocketContext& client, bool loopback)
{
    if (!client.isMulticast()) {
        return NetStack::Socket::PARAM_ERROR_CODE;
    }
    auto socket = static_cast<NetStack::Socket::MulticastSocket *>(client.socket_.get());
    return socket->SetLoopbackMode(loopback);
}

int32_t UdpClientSetReuseAddress(UdpSocketContext& client, bool reuse)
{
    if (!client.isMulticast()) {
        return NetStack::Socket::PARAM_ERROR_CODE;
    }
    auto socket = static_cast<NetStack::Socket::MulticastSocket *>(client.socket_.get());
    return socket->SetReuseAddress(reuse);
}

int32_t UdpClientGetLoopbackMode(UdpSocketContext& client, bool& loopback)
{
    if (!client.isMulticast()) {
        return NetStack::Socket::PARAM_ERROR_CODE;
    }
    auto socket = static_cast<NetStack::Socket::MulticastSocket *>(client.socket_.get());
    loopback = false;
    return socket->GetLoopbackMode(loopback);
}

int32_t UdpSocketOnMessage(UdpSocketContext& client, rust::Box<UdpSocketMessageBox> callbackBox)
{
    client.messageBoxHolder_ = std::make_shared<rust::Box<UdpSocketMessageBox>>(std::move(callbackBox));
    std::weak_ptr<rust::Box<UdpSocketMessageBox>> messageBoxWeak = client.messageBoxHolder_;
    client.socket_->OnMessage([messageBoxWeak]
        (const std::string &data, const NetStack::Socket::SocketRemoteInfo &remoteInfo) {
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

int32_t UdpSocketOnListening(UdpSocketContext& client, rust::Box<UdpSocketListeningBox> callbackBox)
{
    client.listeningBoxHolder_ = std::make_shared<rust::Box<UdpSocketListeningBox>>(std::move(callbackBox));
    std::weak_ptr<rust::Box<UdpSocketListeningBox>> listeningBoxWeak = client.listeningBoxHolder_;
    client.socket_->OnListening([listeningBoxWeak]() {
        if (auto listeningBox = listeningBoxWeak.lock()) {
            (*listeningBox)->on_listening();
        }
    });
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t UdpSocketOnClose(UdpSocketContext& client, rust::Box<UdpSocketCloseBox> callbackBox)
{
    client.closeBoxHolder_ = std::make_shared<rust::Box<UdpSocketCloseBox>>(std::move(callbackBox));
    std::weak_ptr<rust::Box<UdpSocketCloseBox>> closeBoxWeak = client.closeBoxHolder_;
    client.socket_->OnClose([closeBoxWeak]() {
        if (auto closeBox = closeBoxWeak.lock()) {
            (*closeBox)->on_close();
        }
    });
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t UdpSocketOnError(UdpSocketContext& client, rust::Box<UdpSocketErrorBox> callbackBox)
{
    client.errorBoxHolder_ = std::make_shared<rust::Box<UdpSocketErrorBox>>(std::move(callbackBox));
    std::weak_ptr<rust::Box<UdpSocketErrorBox>> errorBoxWeak = client.errorBoxHolder_;
    client.socket_->OnError([errorBoxWeak](int32_t err, const std::string &errString) {
        if (auto errorBox = errorBoxWeak.lock()) {
            rust::String errorString(errString);
            (*errorBox)->on_error(err, errorString);
        }
    });
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t UdpSocketOffMessage(UdpSocketContext& client)
{
    client.socket_->OffMessage();
    client.messageBoxHolder_ = nullptr;
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t UdpSocketOffListening(UdpSocketContext& client)
{
    client.socket_->OffListening();
    client.listeningBoxHolder_ = nullptr;
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t UdpSocketOffClose(UdpSocketContext& client)
{
    client.socket_->OffClose();
    client.closeBoxHolder_ = nullptr;
    return NetStack::Socket::SOCKET_ERROR_OK;
}

int32_t UdpSocketOffError(UdpSocketContext& client)
{
    client.socket_->OffError();
    client.errorBoxHolder_ = nullptr;
    return NetStack::Socket::SOCKET_ERROR_OK;
}

} // namespace NetStackAni
} // namespace OHOS
