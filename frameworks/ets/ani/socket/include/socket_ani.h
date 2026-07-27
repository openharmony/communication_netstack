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

#ifndef NET_SOCKET_ANI_H
#define NET_SOCKET_ANI_H

#include <cstdint>
#include <memory>
#include <string>

#include "cxx.h"
#include "net_address.h"
#include "socket_state_base.h"
#include "tcp_connect_options.h"
#include "tcp_extra_options.h"
#include "tcp_send_options.h"
#include "tcp_socket_client_innerapi.h"
#include "tcp_socket_server_innerapi.h"
#include "udp_socket_innerapi.h"
#include "local_socket_client_innerapi.h"
#include "local_socket_server_innerapi.h"
#include "tls_socket_client_innerapi.h"
#include "tls_socket_server_innerapi.h"
#include "socks5_instance.h"

namespace OHOS {
namespace NetStackAni {

using TlsSocketServerConnection = OHOS::NetStack::TlsSocketServer::TLSSocketServer::Connection;

struct FFINetAddress;
struct FFIProxyOptions;
struct FFITcpExtraOptions;
struct FFITlsConnectOptions;
struct FFILocalAddress;
struct FFILocalConnectOptions;
struct FFILocalSendOptions;
struct FFITcpSendOptions;
struct FFITcpConnectOptions;
struct FFIExtraOptionsBase;
struct FFIUdpExtraOptions;
struct FFIUdpSendOptions;
struct FFISocketStateBase;
struct FFIX509CertRawData;

rust::String GetErrorCodeAndMessage(int32_t &errorCode);

struct UdpSocketMessageBox;
struct UdpSocketListeningBox;
struct UdpSocketCloseBox;
struct UdpSocketErrorBox;
struct UdpSocketContext {
    virtual ~UdpSocketContext() = default;
    virtual bool isMulticast() const { return false; }
    std::shared_ptr<NetStack::Socket::UDPSocket> socket_;
    std::shared_ptr<rust::Box<UdpSocketMessageBox>> messageBoxHolder_;
    std::shared_ptr<rust::Box<UdpSocketListeningBox>> listeningBoxHolder_;
    std::shared_ptr<rust::Box<UdpSocketCloseBox>> closeBoxHolder_;
    std::shared_ptr<rust::Box<UdpSocketErrorBox>> errorBoxHolder_;
};

struct MulticastClientContext : public UdpSocketContext {
    virtual ~MulticastClientContext() = default;
    virtual bool isMulticast() const override { return true; }
};

std::unique_ptr<UdpSocketContext> CreateUdpSocketContext();
std::unique_ptr<UdpSocketContext> CreateMulticastClientContext();
int32_t UdpClientBind(UdpSocketContext& client, const FFINetAddress& addr);
int32_t UdpClientSend(UdpSocketContext& client, const FFIUdpSendOptions& sendOpts, const FFIProxyOptions& proxyOpts);
int32_t UdpClientClose(UdpSocketContext& client);
int32_t UdpClientGetState(UdpSocketContext& client, bool& isBound, bool& isClose, bool& isConnected);
int32_t UdpClientSetExtraOptions(UdpSocketContext& client, const FFIUdpExtraOptions& opts);
int32_t UdpClientGetSocketFd(UdpSocketContext& client, int32_t& fd);
int32_t UdpClientGetLocalAddress(UdpSocketContext& client, rust::String& address, int32_t& family, int32_t& port);

int32_t UdpClientAddMembership(UdpSocketContext& client, rust::Str multicastAddr,
    const int32_t family, const int32_t port);
int32_t UdpClientDropMembership(UdpSocketContext& client, rust::Str multicastAddr,
    const int32_t family, const int32_t port);
int32_t UdpClientSetTtl(UdpSocketContext& client, int32_t ttl);
int32_t UdpClientGetTtl(UdpSocketContext& client, int32_t& ttl);
int32_t UdpClientSetLoopbackMode(UdpSocketContext& client, bool loopback);
int32_t UdpClientGetLoopbackMode(UdpSocketContext& client, bool& loopback);
int32_t UdpClientSetReuseAddress(UdpSocketContext& client, bool reuse);

int32_t UdpSocketOnMessage(UdpSocketContext& client, rust::Box<UdpSocketMessageBox> callbackBox);
int32_t UdpSocketOnListening(UdpSocketContext& client, rust::Box<UdpSocketListeningBox> callbackBox);
int32_t UdpSocketOnClose(UdpSocketContext& client, rust::Box<UdpSocketCloseBox> callbackBox);
int32_t UdpSocketOnError(UdpSocketContext& client, rust::Box<UdpSocketErrorBox> callbackBox);
int32_t UdpSocketOffMessage(UdpSocketContext& client);
int32_t UdpSocketOffListening(UdpSocketContext& client);
int32_t UdpSocketOffClose(UdpSocketContext& client);
int32_t UdpSocketOffError(UdpSocketContext& client);

struct LocalSocketMessageBox;
struct LocalSocketConnectBox;
struct LocalSocketCloseBox;
struct LocalSocketErrorBox;
struct LocalSocketContext {
    virtual ~LocalSocketContext() = default;
    std::shared_ptr<NetStack::Socket::LocalSocket> socket_;
    std::shared_ptr<rust::Box<LocalSocketMessageBox>> messageBoxHolder_;
    std::shared_ptr<rust::Box<LocalSocketConnectBox>> connectBoxHolder_;
    std::shared_ptr<rust::Box<LocalSocketCloseBox>> closeBoxHolder_;
    std::shared_ptr<rust::Box<LocalSocketErrorBox>> errorBoxHolder_;
};
std::unique_ptr<LocalSocketContext> CreateLocalSocketContext();
int32_t LocalSocketBind(LocalSocketContext& client, const FFILocalAddress& addr);
int32_t LocalSocketConnect(LocalSocketContext& client, const FFILocalConnectOptions& opts);
int32_t LocalSocketSend(LocalSocketContext& client, const FFILocalSendOptions& opts);
int32_t LocalSocketClose(LocalSocketContext& client);
int32_t LocalSocketGetState(LocalSocketContext& client, bool& isBound, bool& isClose, bool& isConnected);
int32_t LocalSocketGetSocketFd(LocalSocketContext& client, int32_t& fd);
int32_t LocalSocketSetExtraOptions(LocalSocketContext& client, const FFIExtraOptionsBase& opts);
int32_t LocalSocketGetExtraOptions(LocalSocketContext& client, FFIExtraOptionsBase& opts);
int32_t LocalSocketGetLocalAddress(LocalSocketContext& client, rust::String& address);
int32_t LocalSocketOnMessage(LocalSocketContext& client, rust::Box<LocalSocketMessageBox> callbackBox);
int32_t LocalSocketOnConnect(LocalSocketContext& client, rust::Box<LocalSocketConnectBox> callbackBox);
int32_t LocalSocketOnClose(LocalSocketContext& client, rust::Box<LocalSocketCloseBox> callbackBox);
int32_t LocalSocketOnError(LocalSocketContext& client, rust::Box<LocalSocketErrorBox> callbackBox);
int32_t LocalSocketOffMessage(LocalSocketContext& client);
int32_t LocalSocketOffConnect(LocalSocketContext& client);
int32_t LocalSocketOffClose(LocalSocketContext& client);
int32_t LocalSocketOffError(LocalSocketContext& client);

struct LocalSocketServerConnectBox;
struct LocalSocketServerErrorBox;
struct LocalSocketServerContext {
    virtual ~LocalSocketServerContext() = default;
    std::shared_ptr<NetStack::Socket::LocalSocketServer> socket_;
    std::shared_ptr<rust::Box<LocalSocketServerConnectBox>> connectBoxHolder_;
    std::shared_ptr<rust::Box<LocalSocketServerErrorBox>> errorBoxHolder_;
};
struct TlsServerConnectBox;
struct TlsServerErrorBox;
std::unique_ptr<LocalSocketServerContext> CreateLocalSocketServerContext();
int32_t LocalSocketServerListen(LocalSocketServerContext& server, const FFILocalAddress& addr);
int32_t LocalSocketServerGetState(LocalSocketServerContext& server, bool& isBound, bool& isClose, bool& isConnected);
int32_t LocalSocketServerGetSocketFd(LocalSocketServerContext& server, int32_t& fd);
int32_t LocalSocketServerSetExtraOptions(LocalSocketServerContext& server, const FFIExtraOptionsBase& opts);
int32_t LocalSocketServerGetExtraOptions(LocalSocketServerContext& server, FFIExtraOptionsBase& opts);
int32_t LocalSocketServerGetLocalAddress(LocalSocketServerContext& server, rust::String& address);
int32_t LocalSocketServerClose(LocalSocketServerContext& server);
int32_t LocalSocketServerOnConnect(LocalSocketServerContext& server,
    rust::Box<LocalSocketServerConnectBox> callbackBox);
int32_t LocalSocketServerOnError(LocalSocketServerContext& server, rust::Box<LocalSocketServerErrorBox> callbackBox);
int32_t LocalSocketServerOffConnect(LocalSocketServerContext& server);
int32_t LocalSocketServerOffError(LocalSocketServerContext& server);

struct LocalSocketConnectionMessageBox;
struct LocalSocketConnectionCloseBox;
struct LocalSocketConnectionErrorBox;
struct LocalSocketConnectionContext {
    virtual ~LocalSocketConnectionContext() = default;
    int32_t clientId_;
    std::weak_ptr<NetStack::Socket::LocalSocketServer> socket_;
    std::shared_ptr<rust::Box<LocalSocketConnectionMessageBox>> messageBoxHolder_;
    std::shared_ptr<rust::Box<LocalSocketConnectionCloseBox>> closeBoxHolder_;
    std::shared_ptr<rust::Box<LocalSocketConnectionErrorBox>> errorBoxHolder_;
};
std::unique_ptr<LocalSocketConnectionContext> CreateLocalSocketConnectionContext(
    LocalSocketServerContext &server, int32_t clientId);
int32_t LocalSocketConnectionSend(LocalSocketConnectionContext& connection, const FFILocalSendOptions& opts);
int32_t LocalSocketConnectionClose(LocalSocketConnectionContext& connection);
int32_t LocalSocketConnectionGetLocalAddress(LocalSocketConnectionContext& connection, rust::String& address);
int32_t LocalSocketConnectionGetSocketFd(LocalSocketConnectionContext& connection, int32_t& fd);
int32_t LocalSocketConnectionOnMessage(LocalSocketConnectionContext& client,
    rust::Box<LocalSocketConnectionMessageBox> callbackBox);
int32_t LocalSocketConnectionOnClose(LocalSocketConnectionContext& client,
    rust::Box<LocalSocketConnectionCloseBox> callbackBox);
int32_t LocalSocketConnectionOnError(LocalSocketConnectionContext& client,
    rust::Box<LocalSocketConnectionErrorBox> callbackBox);
int32_t LocalSocketConnectionOffMessage(LocalSocketConnectionContext& client);
int32_t LocalSocketConnectionOffClose(LocalSocketConnectionContext& client);
int32_t LocalSocketConnectionOffError(LocalSocketConnectionContext& client);

struct TcpSocketMessageBox;
struct TcpSocketConnectBox;
struct TcpSocketCloseBox;
struct TcpSocketErrorBox;
struct TcpSocketContext {
    std::shared_ptr<NetStack::Socket::TCPSocket> socket_;
    std::shared_ptr<rust::Box<TcpSocketMessageBox>> messageBoxHolder_;
    std::shared_ptr<rust::Box<TcpSocketConnectBox>> connectBoxHolder_;
    std::shared_ptr<rust::Box<TcpSocketCloseBox>> closeBoxHolder_;
    std::shared_ptr<rust::Box<TcpSocketErrorBox>> errorBoxHolder_;
};

std::unique_ptr<TcpSocketContext> CreateTcpSocketContext();
int32_t TcpSocketBind(TcpSocketContext& client, const FFINetAddress& addr);
int32_t TcpSocketConnect(TcpSocketContext& client, const FFITcpConnectOptions& opts);
int32_t TcpSocketSend(TcpSocketContext& client, const FFITcpSendOptions& opts);
int32_t TcpSocketClose(TcpSocketContext& client);
int32_t TcpSocketGetState(TcpSocketContext& client, bool& isBound, bool& isClose, bool& isConnected);
int32_t TcpSocketGetRemoteAddress(TcpSocketContext& client, rust::String& address, int32_t& family, int32_t& port);
int32_t TcpSocketGetLocalAddress(TcpSocketContext& client, rust::String& address, int32_t& family, int32_t& port);
int32_t TcpSocketSetExtraOptions(TcpSocketContext& client, const FFITcpExtraOptions& opts);
int32_t TcpClientGetSocketFd(TcpSocketContext& client, int32_t& fd);
int32_t TcpSocketOnMessage(TcpSocketContext& client, rust::Box<TcpSocketMessageBox> callbackBox);
int32_t TcpSocketOnConnect(TcpSocketContext& client, rust::Box<TcpSocketConnectBox> callbackBox);
int32_t TcpSocketOnClose(TcpSocketContext& client, rust::Box<TcpSocketCloseBox> callbackBox);
int32_t TcpSocketOnError(TcpSocketContext& client, rust::Box<TcpSocketErrorBox> callbackBox);
int32_t TcpSocketOffMessage(TcpSocketContext& client);
int32_t TcpSocketOffConnect(TcpSocketContext& client);
int32_t TcpSocketOffClose(TcpSocketContext& client);
int32_t TcpSocketOffError(TcpSocketContext& client);

struct TcpSocketServerConnectBox;
struct TcpSocketServerErrorBox;
struct TcpSocketServerContext {
    std::shared_ptr<NetStack::Socket::TCPSocketServer> socket_;
    std::shared_ptr<rust::Box<TcpSocketServerConnectBox>> connectBoxHolder_;
    std::shared_ptr<rust::Box<TcpSocketServerErrorBox>> errorBoxHolder_;
};
std::unique_ptr<TcpSocketServerContext> CreateTcpSocketServerContext();
int32_t TcpSocketServerListen(TcpSocketServerContext& server, const FFINetAddress& addr);
int32_t TcpSocketServerGetState(TcpSocketServerContext& server, bool& isBound, bool& isClose, bool& isConnected);
int32_t TcpSocketServerSetExtraOptions(TcpSocketServerContext& server, const FFITcpExtraOptions& opts);
int32_t TcpSocketServerGetSocketFd(TcpSocketServerContext& server, int32_t& fd);
int32_t TcpSocketServerClose(TcpSocketServerContext& server);
int32_t TcpSocketServerGetLocalAddress(TcpSocketServerContext& server, rust::String& address,
    int32_t& family, int32_t& port);
int32_t TcpSocketServerOnConnect(TcpSocketServerContext& server, rust::Box<TcpSocketServerConnectBox> callbackBox);
int32_t TcpSocketServerOnError(TcpSocketServerContext& server, rust::Box<TcpSocketServerErrorBox> callbackBox);
int32_t TcpSocketServerOffConnect(TcpSocketServerContext& server);
int32_t TcpSocketServerOffError(TcpSocketServerContext& server);

struct TcpSocketConnectionMessageBox;
struct TcpSocketConnectionCloseBox;
struct TcpSocketConnectionErrorBox;
struct TcpSocketConnectionContext {
    int32_t clientId_ = -1;
    std::weak_ptr<NetStack::Socket::TCPSocketConnection> connection_;
    std::shared_ptr<rust::Box<TcpSocketConnectionMessageBox>> messageBoxHolder_;
    std::shared_ptr<rust::Box<TcpSocketConnectionCloseBox>> closeBoxHolder_;
    std::shared_ptr<rust::Box<TcpSocketConnectionErrorBox>> errorBoxHolder_;
};
std::unique_ptr<TcpSocketConnectionContext> CreateTcpSocketConnectionContext(
    TcpSocketServerContext& server, int32_t clientId);
int32_t TcpSocketConnectionSend(TcpSocketConnectionContext& connection, const FFITcpSendOptions& opts);
int32_t TcpSocketConnectionClose(TcpSocketConnectionContext& connection);
int32_t TcpSocketConnectionGetRemoteAddress(TcpSocketConnectionContext& connection, rust::String& address,
    int32_t& family, int32_t& port);
int32_t TcpSocketConnectionGetLocalAddress(TcpSocketConnectionContext& connection, rust::String& address,
    int32_t& family, int32_t& port);
int32_t TcpSocketConnectionGetSocketFd(TcpSocketConnectionContext& connection, int32_t& fd);
int32_t TcpSocketConnectionOnMessage(TcpSocketConnectionContext& connection,
    rust::Box<TcpSocketConnectionMessageBox> callbackBox);
int32_t TcpSocketConnectionOnClose(TcpSocketConnectionContext& connection,
    rust::Box<TcpSocketConnectionCloseBox> callbackBox);
int32_t TcpSocketConnectionOnError(TcpSocketConnectionContext& connection,
    rust::Box<TcpSocketConnectionErrorBox> callbackBox);
int32_t TcpSocketConnectionOffMessage(TcpSocketConnectionContext& connection);
int32_t TcpSocketConnectionOffClose(TcpSocketConnectionContext& connection);
int32_t TcpSocketConnectionOffError(TcpSocketConnectionContext& connection);

struct TlsClientMessageBox;
struct TlsClientConnectBox;
struct TlsClientCloseBox;
struct TlsClientErrorBox;
struct TlsClientContext {
    std::shared_ptr<NetStack::TlsSocket::TLSSocket> tlsSocketClient_;
    std::shared_ptr<NetStack::Socks5::Socks5Instance> socks5Proxy_;
    std::shared_ptr<rust::Box<TlsClientMessageBox>> messageBoxHolder_;
    std::shared_ptr<rust::Box<TlsClientConnectBox>> connectBoxHolder_;
    std::shared_ptr<rust::Box<TlsClientCloseBox>> closeBoxHolder_;
    std::shared_ptr<rust::Box<TlsClientErrorBox>> errorBoxHolder_;
};

std::unique_ptr<TlsClientContext> CreateTlsClientContext(int32_t &errCode);
std::unique_ptr<TlsClientContext> CreateTlsClientContextFromTcpClientContext(TcpSocketContext& client,
    int32_t &errCode);
int32_t TlsClientBind(TlsClientContext& client, const FFINetAddress& addr);
int32_t TlsClientConnect(TlsClientContext& client, const FFITlsConnectOptions& opts);
int32_t TlsClientSend(TlsClientContext& client, FFITcpSendOptions opts);
int32_t TlsClientClose(TlsClientContext& client);
int32_t TlsClientGetState(TlsClientContext& client, FFISocketStateBase& status);
int32_t TlsClientGetRemoteAddress(TlsClientContext& client, FFINetAddress& address);
int32_t TlsClientGetLocalAddress(TlsClientContext& client, FFINetAddress& address);
int32_t TlsClientGetSocketFd(TlsClientContext& client, int32_t& fd);
int32_t TlsClientGetCertificate(TlsClientContext& client, FFIX509CertRawData& cert);
int32_t TlsClientGetRemoteCertificate(TlsClientContext& client, FFIX509CertRawData& cert);
int32_t TlsClientGetProtocol(TlsClientContext& client, rust::String& protocol);
int32_t TlsClientGetCipherSuite(TlsClientContext& client, rust::Vec<rust::String>& cipherSuite);
int32_t TlsClientGetSignatureAlgorithms(TlsClientContext& client, rust::Vec<rust::String>& signatureAlgorithms);
int32_t TlsClientSetExtraOptions(TlsClientContext& client, const FFITcpExtraOptions& opts);
int32_t TlsClientOnMessage(TlsClientContext& client, rust::Box<TlsClientMessageBox> callbackBox);
int32_t TlsClientOnConnect(TlsClientContext& client, rust::Box<TlsClientConnectBox> callbackBox);
int32_t TlsClientOnClose(TlsClientContext& client, rust::Box<TlsClientCloseBox> callbackBox);
int32_t TlsClientOnError(TlsClientContext& client, rust::Box<TlsClientErrorBox> callbackBox);
int32_t TlsClientOffMessage(TlsClientContext& client);
int32_t TlsClientOffConnect(TlsClientContext& client);
int32_t TlsClientOffClose(TlsClientContext& client);
int32_t TlsClientOffError(TlsClientContext& client);

struct TlsServerConnectBox;
struct TlsServerErrorBox;

struct TlsServerContext {
    std::shared_ptr<NetStack::TlsSocketServer::TLSSocketServer> tlsSocketServer_;
    std::shared_ptr<rust::Box<TlsServerConnectBox>> connectBoxHolder_;
    std::shared_ptr<rust::Box<TlsServerErrorBox>> errorBoxHolder_;
};

std::unique_ptr<TlsServerContext> CreateTlsServerContext();
int32_t TlsServerListen(TlsServerContext& server, const FFITlsConnectOptions& opts);
int32_t TlsServerGetState(TlsServerContext& server, FFISocketStateBase& status);
int32_t TlsServerGetSocketFd(TlsServerContext& server, int32_t& fd);
int32_t TlsServerClose(TlsServerContext& server);
int32_t TlsServerSetExtraOptions(TlsServerContext& server, const FFITcpExtraOptions& opts);
int32_t TlsServerGetCertificate(TlsServerContext& server, FFIX509CertRawData& cert);
int32_t TlsServerGetProtocol(TlsServerContext& server, rust::String& protocol);
int32_t TlsServerGetLocalAddress(TlsServerContext& server, FFINetAddress& address);
int32_t TlsServerOnConnect(TlsServerContext& server, rust::Box<TlsServerConnectBox> callbackBox);
int32_t TlsServerOnError(TlsServerContext& server, rust::Box<TlsServerErrorBox> callbackBox);
int32_t TlsServerOffConnect(TlsServerContext& server);
int32_t TlsServerOffError(TlsServerContext& server);

struct TlsConnectMessageBox;
struct TlsConnectCloseBox;
struct TlsConnectErrorBox;

struct TlsConnectContext {
    ~TlsConnectContext();
    int32_t clientId_;
    std::weak_ptr<NetStack::TlsSocketServer::TLSSocketServer> tlsSocketServer_;
    std::shared_ptr<rust::Box<TlsConnectMessageBox>> messageBoxHolder_;
    std::shared_ptr<rust::Box<TlsConnectCloseBox>> closeBoxHolder_;
    std::shared_ptr<rust::Box<TlsConnectErrorBox>> errorBoxHolder_;
};

std::unique_ptr<TlsConnectContext> CreateTlsConnectContext(TlsServerContext& server, int32_t clientId);
int32_t TlsConnectSend(TlsConnectContext& connection, FFITcpSendOptions opts);
int32_t TlsConnectClose(TlsConnectContext& connection);
int32_t TlsConnectGetRemoteAddress(TlsConnectContext& connection, FFINetAddress& address);
int32_t TlsConnectGetLocalAddress(TlsConnectContext& connection, FFINetAddress& address);
int32_t TlsConnectGetSocketFd(TlsConnectContext& connection, int& fd);
int32_t TlsConnectGetRemoteCertificate(TlsConnectContext& connection, FFIX509CertRawData& cert);
int32_t TlsConnectGetProtocol(TlsConnectContext& connection, rust::String& protocol);
int32_t TlsConnectGetCipherSuites(TlsConnectContext& connection, rust::Vec<rust::String>& cipherSuites);
int32_t TlsConnectGetSignatureAlgorithms(TlsConnectContext& connection, rust::Vec<rust::String>& signatureAlgorithms);
int32_t TlsConnectOnMessage(TlsConnectContext& connection, rust::Box<TlsConnectMessageBox> callbackBox);
int32_t TlsConnectOnClose(TlsConnectContext& connection, rust::Box<TlsConnectCloseBox> callbackBox);
int32_t TlsConnectOnError(TlsConnectContext& connection, rust::Box<TlsConnectErrorBox> callbackBox);
int32_t TlsConnectOffMessage(TlsConnectContext& connection);
int32_t TlsConnectOffClose(TlsConnectContext& connection);
int32_t TlsConnectOffError(TlsConnectContext& connection);

} // namespace NetStackAni
} // namespace OHOS

#endif
