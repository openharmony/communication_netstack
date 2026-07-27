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

#ifndef COMMUNICATIONNETSTACK_UDP_SOCKET_H
#define COMMUNICATIONNETSTACK_UDP_SOCKET_H

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <poll.h>

#include "net_address.h"
#include "socket_remote_info.h"
#include "socket_state_base.h"
#include "udp_extra_options.h"
#include "udp_send_options.h"
#include "proxy_options.h"
#include "socket_constant.h"

namespace OHOS {
namespace NetStack {
namespace Socks5 {
class Socks5UdpInstance;
class Socks5TcpInstance;
} // Socks5
namespace Socket {

using UDPSocketOnMessageCallback = std::function<void(const std::string &data, const SocketRemoteInfo &remoteInfo)>;
using UDPSocketOnListeningCallback = std::function<void(void)>;
using UDPSocketOnCloseCallback = std::function<void(void)>;
using UDPSocketOnErrorCallback = std::function<void(int32_t errorNumber, const std::string &errorString)>;
using SocketRecvCallback = std::function<bool(int socketId,
        std::pair<std::unique_ptr<char[]> &, int> &bufInfo,
        std::pair<sockaddr *, socklen_t> &addrInfo)>;

class UDPSocket : public std::enable_shared_from_this<UDPSocket> {
public:
    UDPSocket();
    virtual ~UDPSocket();

    int Bind(const NetAddress &address);
    int GetLocalAddress(NetAddress &address);
    int Send(const UDPSendOptions &sendOptions, const ProxyOptions &proxyOptions);
    int Close();
    int GetState(SocketStateBase &state);
    int SetExtraOptions(const UDPExtraOptions &options);
    int GetSocketFd(int &socketFd) const;

    void OnMessage(const UDPSocketOnMessageCallback &messageCallback);
    void OnListening(const UDPSocketOnListeningCallback &listeningCallback);
    void OnError(const UDPSocketOnErrorCallback &OnErrorCallback);
    void OnClose(const UDPSocketOnCloseCallback &closeCallback);
    void OffMessage();
    void OffListening();
    void OffError();
    void OffClose();

protected:
    void RunRecvThread();
    virtual const char *GetRecvThreadName() const;
    void PollRecvData(sockaddr *addr, socklen_t addrLen);
    void PollRecvFinish();
    bool PreparePollFds(int &currentFd, std::vector<pollfd> &fds,
        std::unordered_map<int, SocketRecvCallback> &socketCallbackMap);
    void ProcessPollResult(int currentFd);
    bool ProcessRecvFds(std::pair<std::unique_ptr<char[]> &, int> &bufInfo,
        std::pair<sockaddr *, socklen_t> &addrInfo, std::vector<pollfd> &fds,
        std::unordered_map<int, SocketRecvCallback> &socketCallbackMap);
    int UpdateRecvBuffer(int sock, int &bufferSize, std::unique_ptr<char[]> &buf);
    int ExitOrAbnormal(int sock, ssize_t recvLen);
    bool SocketRecvHandle(int socketId, std::pair<std::unique_ptr<char[]> &, int> &bufInfo,
        std::pair<sockaddr *, socklen_t> &addrInfo);
    bool ProxyUdpSocketRecvHandle(int socketId, std::pair<std::unique_ptr<char[]> &, int> &bufInfo,
        std::pair<sockaddr *, socklen_t> &addrInfo);

    std::shared_ptr<Socks5::Socks5UdpInstance> GetSocks5Instance() const { return socks5Instance_; }
    void SetSocks5Instance(const std::shared_ptr<Socks5::Socks5UdpInstance> instance) { socks5Instance_ = instance; }
    bool IsClose() const { return state_.IsClose(); }

    void CallOnMessageCallback(const std::string &data, const Socket::SocketRemoteInfo &remoteInfo);
    void CallOnListeningCallback();
    void CallOnCloseCallback();
    void CallOnErrorCallback(int32_t err, const std::string &errString);

    sockaddr_storage bindAddr_ = {};
    socklen_t bindAddrLen_ = 0;
    int socketFd_ = -1;
    SocketStateBase state_;
    bool reuseAddr_ = false;

private:
    bool DoBindWithRetry();
    int DoSend(const UDPSendOptions &sendOptions);
    std::shared_ptr<Socks5::Socks5UdpInstance> InitSocks5UdpInstance(const NetAddress &destAddress,
        const ProxyOptions &proxyOptions);
    int HandleUdpProxyOptions(UDPSocket *socket, UDPSendOptions &sendOptions, const ProxyOptions &proxyOptions);

private:
    std::shared_ptr<Socks5::Socks5UdpInstance> socks5Instance_;
    std::mutex mutex_;
    UDPSocketOnMessageCallback onMessageCallback_ = nullptr;
    UDPSocketOnListeningCallback onListeningCallback_ = nullptr;
    UDPSocketOnCloseCallback onCloseCallback_ = nullptr;
    UDPSocketOnErrorCallback onErrorCallback_ = nullptr;
};

class MulticastSocket : public UDPSocket {
public:
    MulticastSocket();
    ~MulticastSocket() override;

    int AddMembership(const NetAddress &multicastAddress);
    int DropMembership(const NetAddress &multicastAddress);
    int SetMulticastTTL(int ttl);
    int GetMulticastTTL(int &ttl);
    int SetLoopbackMode(bool loopback);
    int GetLoopbackMode(bool &loopback);
    int SetReuseAddress(bool reuse);

private:
    bool JoinMulticastGroup(const NetAddress &multicastAddress);
    bool BindMulticastAddr(const NetAddress &multicastAddress);
    const char *GetRecvThreadName() const override;
};

} // namespace Socket
} // namespace NetStack
} // namespace OHOS

#endif /* COMMUNICATIONNETSTACK_UDP_SOCKET_H */
