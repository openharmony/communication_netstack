/*
 * Copyright (c) 2025-2026 Huawei Device Co., Ltd.
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

#ifndef COMMUNICATIONNETSTACK_TCP_SOCKET_H
#define COMMUNICATIONNETSTACK_TCP_SOCKET_H

#include <functional>
#include <memory>
#include <thread>
#include <unordered_map>
#include <vector>
#include <poll.h>
#include <utility>

#include "net_address.h"
#include "socket_remote_info.h"
#include "socket_state_base.h"
#include "tcp_connect_options.h"
#include "tcp_extra_options.h"
#include "tcp_send_options.h"
#include "proxy_options.h"

namespace OHOS {
namespace NetStack {
namespace Socks5 {
class Socks5Instance;
class Socks5TcpInstance;
} // Socks5
namespace Socket {

class TCPSocket;
using TCPSocketOnMessageCallback = std::function<void(const std::string &data, const SocketRemoteInfo &remoteInfo)>;
using TCPSocketOnConnectCallback = std::function<void(void)>;
using TCPSocketOnCloseCallback = std::function<void(void)>;
using TCPSocketOnErrorCallback = std::function<void(int32_t errorNumber, const std::string &errorString)>;

class TCPSocket : public std::enable_shared_from_this<TCPSocket> {
public:
    TCPSocket();
    virtual ~TCPSocket();

    int Bind(const NetAddress &address);
    int Connect(const TcpConnectOptions &connectOptions, const ProxyOptions &proxyOptions);
    int Send(const TCPSendOptions &options);
    int Close();
    int GetState(SocketStateBase &state);
    int GetRemoteAddress(NetAddress &address);
    int GetLocalAddress(NetAddress &address);
    int SetExtraOptions(const TCPExtraOptions &options);
    int GetSocketFd(int32_t &socketFd) const;
    int TransferFd(int32_t& socketFd, std::shared_ptr<Socks5::Socks5Instance>& socks5Inst);

    void OnMessage(const TCPSocketOnMessageCallback &messageCallback);
    void OnConnect(const TCPSocketOnConnectCallback &connectCallback);
    void OnError(const TCPSocketOnErrorCallback &OnErrorCallback);
    void OnClose(const TCPSocketOnCloseCallback &closeCallback);
    void OffMessage();
    void OffConnect();
    void OffError();
    void OffClose();

private:
    using SocketRecvCallback = bool (TCPSocket::*)(int socketId,
        std::pair<std::unique_ptr<char[]> &, int> &bufInfo,
        std::pair<sockaddr *, socklen_t> &addrInfo);

    int RegisterAsyncConnect(uint32_t timeoutMs);
    void OnAsyncConnectComplete(int errCode);
    bool IsTCPSocket(int sockfd);
    int HandleNonProxyConnection(const TcpConnectOptions &connectOptions);
    int HandleTcpProxyOptions(const ProxyOptions &options);
    bool HandleTcpProxyAuth();
    bool GetAsyncConnecting() { return asyncConnecting_; }
    void SetAsyncConnecting(bool asyncConnecting) { asyncConnecting_ = asyncConnecting; }

    void RunRecvThread();
    void PollRecvFinish();
    void ProcessPollResult(int currentFd);
    int UpdateRecvBuffer(int sock, int &bufferSize, std::unique_ptr<char[]> &buf);
    int ExitOrAbnormal(int sock, ssize_t recvLen);
    void FillRemoteInfo(sockaddr *addr, size_t payloadLen, SocketRemoteInfo &remoteInfo);
    bool SocketRecvHandle(int socketId, std::pair<std::unique_ptr<char[]> &, int> &bufInfo,
        std::pair<sockaddr *, socklen_t> &addrInfo);
    bool ProcessRecvFds(std::pair<std::unique_ptr<char[]> &, int> &bufInfo,
        std::pair<sockaddr *, socklen_t> &addrInfo, std::vector<pollfd> &fds,
        std::unordered_map<int, SocketRecvCallback> &socketCallbackMap);
    bool ProxyTcpSocketRecvHandle(int socketId, std::pair<std::unique_ptr<char[]> &, int> &bufInfo,
        std::pair<sockaddr *, socklen_t> &addrInfo);
    bool PreparePollFds(int &currentFd, std::vector<pollfd> &fds,
        std::unordered_map<int, SocketRecvCallback> &socketCallbackMap);
    void PollRecvData();
    std::shared_ptr<Socks5::Socks5TcpInstance> InitSocks5TcpInstance(const ProxyOptions &options);

    void CallOnMessageCallback(const std::string &data, const SocketRemoteInfo &remoteInfo);
    void CallOnConnectCallback();
    void CallOnCloseCallback();
    void CallOnErrorCallback(int32_t err, const std::string &errString);
private:
    int socketFd_ = -1;
    SocketStateBase state_;
    NetAddress address_;
    std::shared_ptr<Socks5::Socks5TcpInstance> socks5Instance_;
    bool reuseAddr_ = false;
    bool asyncConnecting_ = false;
    bool isRecvThreadRun_ = false;
    std::mutex cvMutex_;
    std::condition_variable cvRecvThreadRun_;

    std::mutex mutex_;
    TCPSocketOnMessageCallback onMessageCallback_ = nullptr;
    TCPSocketOnConnectCallback onConnectCallback_ = nullptr;
    TCPSocketOnCloseCallback onCloseCallback_ = nullptr;
    TCPSocketOnErrorCallback onErrorCallback_ = nullptr;
};

} // namespace Socket
} // namespace NetStack
} // namespace OHOS

#endif /* COMMUNICATIONNETSTACK_TCP_SOCKET_H */
