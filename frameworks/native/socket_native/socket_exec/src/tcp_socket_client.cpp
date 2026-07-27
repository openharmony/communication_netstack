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

#include "net_address.h"
#include "tcp_socket_client_innerapi.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <memory>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>
#include "securec.h"
#include "netstack_log.h"
#include "socket_exec_common.h"
#include "netstack_common_utils.h"
#include "proxy_options.h"
#include "socket_constant.h"
#include "socket_exec_error.h"
#include "socks5.h"
#include "socks5_instance.h"
#include "socks5_utils.h"
#include "connect_monitor.h"

namespace OHOS {
namespace NetStack {
namespace Socket {
static constexpr const char *SOCKET_EXEC_CONNECT = "OS_NET_SockTCON";

static bool CheckClosed(int sock, int &opt)
{
    socklen_t optLen = sizeof(int);
    if (getsockopt(sock, SOL_SOCKET, SO_TYPE, &opt, &optLen) < 0) {
        return true;
    }
    return false;
}

static void SetIsBound(sockaddr_storage &sockAddr, SocketStateBase &state)
{
    auto family = sockAddr.ss_family;
    if (family == AF_INET) {
        auto *addr4 = reinterpret_cast<sockaddr_in *>(&sockAddr);
        state.SetIsBound(ntohs(addr4->sin_port) != 0);
    } else if (family == AF_INET6) {
        auto *addr6 = reinterpret_cast<sockaddr_in6 *>(&sockAddr);
        state.SetIsBound(ntohs(addr6->sin6_port) != 0);
    }
}

static bool GetSocketState(int sock, SocketStateBase &state)
{
    if (sock < 0) {
        NETSTACK_LOGE("fd is nullptr or closed");
        state.SetIsClose(true);
        return true;
    }
    int opt;
    if (CheckClosed(sock, opt)) {
        state.SetIsClose(true);
        return true;
    }
    sockaddr_storage sockAddr = {};
    socklen_t sockLen = sizeof(sockAddr);
    int ret = getsockname(sock, reinterpret_cast<sockaddr *>(&sockAddr), &sockLen);
    if (ret < 0) {
        return false;
    }
    SetIsBound(sockAddr, state);
    if (opt != SOCK_STREAM) {
        return true;
    }
    if (getpeername(sock, reinterpret_cast<sockaddr *>(&sockAddr), &sockLen) < 0) {
        if (errno != ENOTCONN) {
            return false;
        }
        return true;
    }
    auto family = sockAddr.ss_family;
    if (family == AF_INET) {
        auto *peer4 = reinterpret_cast<sockaddr_in *>(&sockAddr);
        state.SetIsConnected(ntohs(peer4->sin_port) != 0);
    } else if (family == AF_INET6) {
        auto *peer6 = reinterpret_cast<sockaddr_in6 *>(&sockAddr);
        state.SetIsConnected(ntohs(peer6->sin6_port) != 0);
    }
    return true;
}

static bool CheckIsConnected(int sock)
{
    sockaddr_storage sockAddr = {};
    socklen_t len = sizeof(sockAddr);
    if (getsockname(sock, reinterpret_cast<sockaddr *>(&sockAddr), &len) < 0) {
        NETSTACK_LOGE("getsockname failed, sock:%{public}d, errno:%{public}d", sock, errno);
        return false;
    }
    if (sockAddr.ss_family == AF_INET) {
        sockaddr_in addr4 = {};
        socklen_t len4 = sizeof(addr4);
        int ret = getpeername(sock, reinterpret_cast<sockaddr *>(&addr4), &len4);
        return ret >= 0 && addr4.sin_port != 0;
    } else if (sockAddr.ss_family == AF_INET6) {
        sockaddr_in6 addr6 = {};
        socklen_t len6 = sizeof(addr6);
        int ret = getpeername(sock, reinterpret_cast<sockaddr *>(&addr6), &len6);
        return ret >= 0 && addr6.sin6_port != 0;
    }
    NETSTACK_LOGE("sa_family %{public}d is not supported", sockAddr.ss_family);
    return false;
}

static bool DoBindWithRetry(int sockFd, sockaddr_storage &ss, socklen_t addrLen)
{
    constexpr int bindRetryTimes = 5;
    constexpr int bindRetryIntervalUs = 200000;
    for (int retry = 0; retry <= bindRetryTimes; ++retry) {
        if (bind(sockFd, reinterpret_cast<sockaddr *>(&ss), addrLen) >= 0) {
            if (retry > 0) {
                NETSTACK_LOGI("bind success after retry, socket:%{public}d, retry:%{public}d", sockFd, retry);
            }
            return true;
        }
        if (errno != EADDRINUSE) {
            if  (errno != EACCES) {
                return false;
            }
            break;
        }
        if (retry < bindRetryTimes) {
            NETSTACK_LOGI("bind failed, socket:%{public}d, errno:%{public}d, retrying", sockFd, errno);
            usleep(bindRetryIntervalUs);
        }
    }
    NETSTACK_LOGE("bind failed after retry, socket:%{public}d, errno:%{public}d", sockFd, errno);
    if (ss.ss_family == AF_INET) {
        reinterpret_cast<sockaddr_in *>(&ss)->sin_port = 0;
    } else if (ss.ss_family == AF_INET6) {
        reinterpret_cast<sockaddr_in6 *>(&ss)->sin6_port = 0;
    }
    if (bind(sockFd, reinterpret_cast<sockaddr *>(&ss), addrLen) < 0) {
        NETSTACK_LOGE("rebind failed, socket:%{public}d, errno:%{public}d", sockFd, errno);
        return false;
    }
    NETSTACK_LOGI("rebind success with random port");
    return true;
}

TCPSocket::TCPSocket() {}

TCPSocket::~TCPSocket()
{
    Close();
}

int TCPSocket::Bind(const NetAddress &address)
{
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }
    if (!ExecCommonUtils::ValidateAddress(address)) {
        return PARAM_ERROR_CODE;
    }
    if (socketFd_ >= 0 && state_.IsBound()) {
        NETSTACK_LOGI("socket has bound, sock:%{public}d", socketFd_);
        return SOCKET_ERROR_OK;
    }
    if (socketFd_ < 0) {
        socketFd_ = ExecCommonUtils::MakeTcpSocket(address.GetSaFamily());
        if (socketFd_ < 0) {
            NETSTACK_LOGE("make tcp socket failed, errno: %{public}d", errno);
            return SYSTEM_INTERNAL_ERROR;
        }
        state_.SetIsClose(false);
    }

    int reuse = reuseAddr_ ? 1 : 0;
    if (setsockopt(socketFd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<void *>(&reuse), sizeof(reuse)) < 0) {
        NETSTACK_LOGE("set SO_REUSEADDR failed, fd: %{public}d, errno: %{public}d", socketFd_, errno);
        Close();
        return ConvertSocketClientErrno(errno);
    }

    sockaddr_storage ss = {};
    socklen_t addrLen = 0;
    if (!ExecCommonUtils::MakeSockAddr(address, ss, addrLen)) {
        NETSTACK_LOGE("MakeSockAddr failed, errno: %{public}d", errno);
        return PARAM_ERROR_CODE;
    }

    if (!DoBindWithRetry(socketFd_, ss, addrLen)) {
        Close();
        return ConvertSocketClientErrno(errno);
    }
    NETSTACK_LOGI("bind success, sock:%{public}d", socketFd_);
    state_.SetIsBound(true);
    return SOCKET_ERROR_OK;
}

bool TCPSocket::IsTCPSocket(int sockfd)
{
    int optval;
    socklen_t optlen = sizeof(optval);

#if defined(MAC_PLATFORM) || defined(IOS_PLATFORM)
    if (getsockopt(sockfd, SOL_SOCKET, SO_TYPE, &optval, &optlen) != 0) {
        return false;
    }
    return optval == SOCK_STREAM;
#else
    if (getsockopt(sockfd, SOL_SOCKET, SO_PROTOCOL, &optval, &optlen) != 0) {
        return false;
    }
    return optval == IPPROTO_TCP;
#endif
}

void TCPSocket::PollRecvFinish()
{
    {
        std::lock_guard<std::mutex> lock(cvMutex_);
        isRecvThreadRun_ = false;
    }
    cvRecvThreadRun_.notify_all();
    NETSTACK_LOGI("PollRecvFinish, socket is %{public}d", socketFd_);
}

void TCPSocket::ProcessPollResult(int currentFd)
{
    if (currentFd > 0 && !state_.IsClose()) {
        NETSTACK_LOGE("poll to recv failed, socket is %{public}d, errno is %{public}d", currentFd, errno);
        auto errCode = ConvertSocketClientErrno(errno);
        CallOnErrorCallback(errCode, GetSocketErrorMessage(errCode));

        if (socks5Instance_ != nullptr) {
            socks5Instance_->OnProxySocketError();
        }
    }
}

int TCPSocket::UpdateRecvBuffer(int sock, int &bufferSize, std::unique_ptr<char[]> &buf)
{
    int currentRecvBufferSize = ExecCommonUtils::ConfirmBufferSize(sock);
    if (currentRecvBufferSize != bufferSize) {
        bufferSize = currentRecvBufferSize;
        if (bufferSize <= 0 || bufferSize > MAX_SOCKET_BUFFER_SIZE) {
            NETSTACK_LOGD("buffer size is out of range, size: %{public}d", bufferSize);
            bufferSize = DEFAULT_BUFFER_SIZE;
        }
        buf.reset(new (std::nothrow) char[bufferSize]);
        if (buf == nullptr) {
            CallOnErrorCallback(SYSTEM_INTERNAL_ERROR, GetSocketErrorMessage(SYSTEM_INTERNAL_ERROR));
            return -1;
        }
    }
    (void)memset_s(buf.get(), bufferSize, 0, bufferSize);
    return 0;
}

int TCPSocket::ExitOrAbnormal(int sock, ssize_t recvLen)
{
    if (!IsTCPSocket(sock) && errno != EBADF) {
        NETSTACK_LOGI("not tcpsocket, continue loop, recvLen: %{public}zd, err: %{public}d", recvLen, errno);
        if (errno == ENOTSOCK) {
            return -1;
        }
        return 0;
    }
    if (recvLen == 0) {
        NETSTACK_LOGI("closed by peer, socket:%{public}d, recvLen:%{public}zd", sock, recvLen);
        return -1;
    }
    if (errno == EAGAIN || errno == EINTR) {
        return 0;
    }

    if (sock > 0 && !state_.IsClose()) {
        NETSTACK_LOGE("recv fail, socket:%{public}d, recvLen:%{public}zd, errno:%{public}d", sock, recvLen, errno);
        auto errCode = ConvertSocketClientErrno(errno);
        CallOnErrorCallback(errCode, GetSocketErrorMessage(errCode));
    }
    return -1;
}

void TCPSocket::FillRemoteInfo(sockaddr *addr, size_t payloadLen, SocketRemoteInfo &remoteInfo)
{
    char addrBuf[INET6_ADDRSTRLEN] = {};
    if (addr->sa_family == AF_INET) {
        auto *addr4 = reinterpret_cast<sockaddr_in *>(addr);
        inet_ntop(AF_INET, &addr4->sin_addr, addrBuf, sizeof(addrBuf));
        remoteInfo.SetAddress(std::string(addrBuf));
        remoteInfo.SetPort(ntohs(addr4->sin_port));
        remoteInfo.SetFamily(AF_INET);
    } else if (addr->sa_family == AF_INET6) {
        auto *addr6 = reinterpret_cast<sockaddr_in6 *>(addr);
        inet_ntop(AF_INET6, &addr6->sin6_addr, addrBuf, sizeof(addrBuf));
        remoteInfo.SetAddress(std::string(addrBuf));
        remoteInfo.SetPort(ntohs(addr6->sin6_port));
        remoteInfo.SetFamily(AF_INET6);
    }
    remoteInfo.SetSize(payloadLen);
}

bool TCPSocket::SocketRecvHandle(int socketId, std::pair<std::unique_ptr<char[]> &, int> &bufInfo,
    std::pair<sockaddr *, socklen_t> &addrInfo)
{
    if (UpdateRecvBuffer(socketId, bufInfo.second, bufInfo.first) < 0) {
        return false;
    }

    socklen_t tempAddrLen = addrInfo.second;
    auto recvLen = recvfrom(socketId, bufInfo.first.get(), bufInfo.second, 0, addrInfo.first, &tempAddrLen);
    if (recvLen <= 0) {
        if (ExitOrAbnormal(socketId, recvLen) < 0) {
            return false;
        }
        return true;
    }
    if (state_.IsClose()) {
        return false;
    }

    size_t payloadLen = static_cast<size_t>(recvLen);
    if (socks5Instance_ != nullptr && addrInfo.first != nullptr &&
        !socks5Instance_->RemoveHeader(bufInfo.first.get(), payloadLen, addrInfo.first->sa_family)) {
        NETSTACK_LOGE("remove socks5 udp header failed");
    }
    if (payloadLen == 0) {
        return true;
    }

    SocketRemoteInfo remoteInfo;
    FillRemoteInfo(addrInfo.first, payloadLen, remoteInfo);
    std::string bufContent(bufInfo.first.get(), payloadLen);
    CallOnMessageCallback(bufContent, remoteInfo);
    return true;
}

bool TCPSocket::ProcessRecvFds(std::pair<std::unique_ptr<char[]> &, int> &bufInfo,
    std::pair<sockaddr *, socklen_t> &addrInfo, std::vector<pollfd> &fds,
    std::unordered_map<int, SocketRecvCallback> &socketCallbackMap)
{
    for (auto &fd : fds) {
        if ((static_cast<uint16_t>(fd.revents) & POLLERR) ||
            (static_cast<uint16_t>(fd.revents) & POLLNVAL)) {
            NETSTACK_LOGE("recv fail, socket:%{public}d, errno:%{public}d, revent:%{public}x",
                fd.fd, errno, fd.revents);

            int socketError = 0;
            socklen_t errlen = sizeof(socketError);

            if (getsockopt(fd.fd, SOL_SOCKET, SO_ERROR, &socketError, &errlen) == 0) {
                NETSTACK_LOGE("get socket error: fd=%{public}d, socket_error=%{public}d", fd.fd, socketError);
            }
            if (socketFd_ > 0 && !state_.IsClose()) {
                auto errCode = ConvertSocketClientErrno(socketError);
                CallOnErrorCallback(errCode, GetSocketErrorMessage(errCode));
            }
            return false;
        }
        if ((static_cast<uint16_t>(fd.revents) & POLLIN) == 0) {
            continue;
        }
        auto it = socketCallbackMap.find(fd.fd);
        if (it == socketCallbackMap.end()) {
            continue;
        }
        auto cb = it->second;
        if (cb != nullptr && !(this->*cb)(fd.fd, bufInfo, addrInfo)) {
            return false;
        }
    }
    return true;
}

bool TCPSocket::ProxyTcpSocketRecvHandle(int socketId, std::pair<std::unique_ptr<char[]> &, int> &bufInfo,
    std::pair<sockaddr *, socklen_t> &addrInfo)
{
    (void)addrInfo;
    const auto recvLen = recv(socketId, bufInfo.first.get(), bufInfo.second, 0);
    if (recvLen > 0) {
        return true;
    }
    const int32_t errCode{errno};
    if ((errCode == EAGAIN) || (errCode == EINTR)) {
        return true;
    }
    Socks5::Socks5Utils::PrintRecvErrMsg(socketId, errCode, recvLen, "SocketRecvHandle");

    if (socks5Instance_ != nullptr) {
        socks5Instance_->OnProxySocketError();
    } else {
        NETSTACK_LOGE("socks5 instance is null");
    }
    return false;
}

bool TCPSocket::PreparePollFds(int &currentFd, std::vector<pollfd> &fds,
    std::unordered_map<int, SocketRecvCallback> &socketCallbackMap)
{
    socketCallbackMap.clear();
    fds.clear();

    currentFd = socketFd_;
    if (currentFd <= 0) {
        NETSTACK_LOGE("currentFd: %{public}d is error", currentFd);
        return false;
    }

    socketCallbackMap[currentFd] = &TCPSocket::SocketRecvHandle;
    fds.push_back({currentFd, POLLIN, 0});

    if (socks5Instance_ != nullptr && socks5Instance_->IsConnected()) {
        if (socks5Instance_->GetSocketId() != -1 && socks5Instance_->GetSocketId() != currentFd) {
            socketCallbackMap[socks5Instance_->GetSocketId()] = &TCPSocket::ProxyTcpSocketRecvHandle;
            fds.push_back({socks5Instance_->GetSocketId(), POLLIN, 0});
        }
    }

    return true;
}

void TCPSocket::PollRecvData()
{
    {
        std::lock_guard<std::mutex> lock(cvMutex_);
        isRecvThreadRun_ = true;
    }
    int socketfd = socketFd_;
    if (socketfd < 0) {
        PollRecvFinish();
        NETSTACK_LOGE("fd is nullptr or closed");
        return;
    }
    int bufferSize = ExecCommonUtils::ConfirmBufferSize(socketfd);
    auto buf = std::make_unique<char[]>(bufferSize);
    std::pair<std::unique_ptr<char[]> &, int> bufInfo{buf, bufferSize};
    sockaddr_in addr4 = {0};
    sockaddr_in6 addr6 = {0};
    sockaddr *addr = nullptr;
    socklen_t addrLen = 0;
    if (address_.GetSaFamily() == AF_INET) {
        addr = reinterpret_cast<sockaddr *>(&addr4);
        addrLen = sizeof(addr4);
    } else {
        addr = reinterpret_cast<sockaddr *>(&addr6);
        addrLen = sizeof(addr6);
    }
    std::pair<sockaddr *, socklen_t> addrInfo{addr, addrLen};
    std::unordered_map<int, SocketRecvCallback> socketCallbackMap{};
    std::vector<pollfd> fds{};

    while (true) {
        int currentFd = -1;
        if (!PreparePollFds(currentFd, fds, socketCallbackMap)) {
            break;
        }
        int ret = poll(fds.data(), fds.size(), DEFAULT_POLL_TIMEOUT);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            ProcessPollResult(currentFd);
            break;
        } else if (ret == 0) {
            continue;
        }
        if (!ProcessRecvFds(bufInfo, addrInfo, fds, socketCallbackMap)) {
            break;
        }
    }
    PollRecvFinish();
}

std::shared_ptr<Socks5::Socks5TcpInstance> TCPSocket::InitSocks5TcpInstance(const ProxyOptions &options)
{
    const std::shared_ptr<Socks5::Socks5Option> opt{std::make_shared<Socks5::Socks5Option>()};
    opt->username_ = options.username_;
    opt->password_ = options.password_;
    opt->proxyAddress_.netAddress_ = options.address_;
    socklen_t len;
    ExecCommonUtils::GetSocketAddr(&opt->proxyAddress_.netAddress_, &opt->proxyAddress_.addrV4_,
        &opt->proxyAddress_.addrV6_, &opt->proxyAddress_.addr_, &len);
    if (opt->proxyAddress_.addr_ == nullptr) {
        NETSTACK_LOGE("addr family error, address invalid");
        return nullptr;
    }

    auto socks5Tcp = std::make_shared<Socks5::Socks5TcpInstance>(socketFd_);
    socks5Tcp->SetDestAddress(address_);
    socks5Tcp->SetSocks5Option(opt);
    return socks5Tcp;
}

int TCPSocket::HandleNonProxyConnection(const TcpConnectOptions &connectOptions)
{
    if (socketFd_ < 0) {
        NETSTACK_LOGE("fd is nullptr or closed");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }

    sockaddr_storage ss{};
    socklen_t addrLen = 0;
    if (!ExecCommonUtils::MakeSockAddr(address_, ss, addrLen)) {
        NETSTACK_LOGE("MakeSockAddr failed. errno: %{public}d", errno);
        return PARAM_ERROR_CODE;
    }

#ifdef OHOS_PLATFORM
    int ret = connect(socketFd_, reinterpret_cast<sockaddr *>(&ss), addrLen);
    if (ret == 0) {
        SetAsyncConnecting(false);
        return SOCKET_ERROR_OK;
    }
    if (errno == EINPROGRESS || errno == EALREADY) {
        SetAsyncConnecting(true);
        return SOCKET_ERROR_OK;
    }
    NETSTACK_LOGE("connect errno %{public}d", errno);
    return ConvertSocketClientErrno(errno);
#else
    uint32_t timeout = connectOptions.GetTimeout();
    if (!ExecCommonUtils::NonBlockConnect(socketFd_, reinterpret_cast<sockaddr *>(&ss), addrLen, timeout)) {
        NETSTACK_LOGE("connect errno %{public}d", errno);
        return ConvertSocketClientErrno(errno);
    }
    return SOCKET_ERROR_OK;
#endif
}

#ifdef OHOS_PLATFORM
int TCPSocket::HandleTcpProxyOptions(const ProxyOptions &options)
{
    if (socketFd_ < 0) {
        NETSTACK_LOGE("fd is nullptr or closed");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }

    if (options.type_ != ProxyType::SOCKS5) {
        NETSTACK_LOGE("unsupport proxy type");
        return SOCKET_ERROR_OK;
    }

    if (socks5Instance_ == nullptr) {
        socks5Instance_ = InitSocks5TcpInstance(options);
        if (socks5Instance_ == nullptr) {
            return PARAM_ERROR_CODE;
        }
        socks5Instance_->SetSocks5Instance(socks5Instance_);
    }
    socks5Instance_->SetDestAddress(address_);

    NetAddress proxyAddr = options.address_;
    proxyAddr.SetRawAddress(ExecCommonUtils::ConvertAddressToIp(proxyAddr.GetAddress(), proxyAddr.GetSaFamily()));
    sockaddr_storage ss{};
    socklen_t addrLen = 0;
    if (!ExecCommonUtils::MakeSockAddr(proxyAddr, ss, addrLen)) {
        NETSTACK_LOGE("MakeSockAddr failed. errno: %{public}d", errno);
        return ConvertSocketClientErrno(errno);
    }
    int ret = connect(socketFd_, reinterpret_cast<sockaddr *>(&ss), addrLen);
    if (ret == 0 || errno == EISCONN || errno == EINPROGRESS || errno == EALREADY) {
        SetAsyncConnecting(true);
        return SOCKET_ERROR_OK;
    }

    NETSTACK_LOGE("proxy connect errno %{public}d", errno);
    return ConvertSocketClientErrno(errno);
}
#else
int TCPSocket::HandleTcpProxyOptions(const ProxyOptions &options)
{
    if (options.type_ != ProxyType::SOCKS5) {
        NETSTACK_LOGE("unsupport proxy type");
        return SOCKET_ERROR_OK;
    }

    if (socks5Instance_ == nullptr) {
        socks5Instance_ = InitSocks5TcpInstance(options);
        if (socks5Instance_ == nullptr) {
            return PARAM_ERROR_CODE;
        }
        socks5Instance_->SetSocks5Instance(socks5Instance_);
    }
    socks5Instance_->SetDestAddress(address_);

    if (!socks5Instance_->IsConnected()) {
        if (!socks5Instance_->Connect()) {
            NETSTACK_LOGE("socks5 tcp connect failed");
            int32_t socks5Err = socks5Instance_->GetErrorCode();
            return ConvertSocketClientErrno(socks5Err != 0 ? socks5Err : UNKNOWN_ERROR);
        }
    }

    return SOCKET_ERROR_OK;
}
#endif

bool TCPSocket::HandleTcpProxyAuth()
{
    if (socks5Instance_ == nullptr || socks5Instance_->IsConnected()) {
        return true;
    }
    if (socks5Instance_->Connect()) {
        return true;
    }
    NETSTACK_LOGE("socks5 proxy auth failed, sock:%{public}d", socketFd_);
    return false;
}

int TCPSocket::Connect(const TcpConnectOptions &connectOptions, const ProxyOptions &proxyOptions)
{
    state_.SetIsConnected(false);
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }

    NetAddress connectAddr = connectOptions.address;
    connectAddr.SetRawAddress(ExecCommonUtils::ConvertAddressToIp(
        connectAddr.GetAddress(), connectAddr.GetSaFamily()));
    address_ = connectAddr;

    if (socketFd_ <= 0) {
        socketFd_ = ExecCommonUtils::MakeTcpSocket(connectAddr.GetSaFamily());
        if (socketFd_ < 0) {
            NETSTACK_LOGE("make tcp socket failed, errno: %{public}d", errno);
            return SYSTEM_INTERNAL_ERROR;
        }
    }

    int errCode = SOCKET_ERROR_OK;
    if (proxyOptions.type_ == ProxyType::SOCKS5) {
        errCode = HandleTcpProxyOptions(proxyOptions);
    } else {
        errCode = HandleNonProxyConnection(connectOptions);
    }
    if (errCode != SOCKET_ERROR_OK) {
        return errCode;
    }

#ifdef OHOS_PLATFORM
    if (GetAsyncConnecting()) {
        return RegisterAsyncConnect(connectOptions.GetTimeout());
    }
#endif

    NETSTACK_LOGI("connect success, sock:%{public}d", socketFd_);
    state_.SetIsConnected(true);
    RunRecvThread();
    CallOnConnectCallback();
    return SOCKET_ERROR_OK;
}

int TCPSocket::RegisterAsyncConnect(uint32_t timeoutMs)
{
    if (timeoutMs == 0) {
        timeoutMs = DEFAULT_CONNECT_TIMEOUT;
    }
    auto watchData = sptr<ConnectWatchData>::MakeSptr();
    if (watchData == nullptr) {
        NETSTACK_LOGE("failed to create ConnectWatchData");
        Close();
        return SYSTEM_INTERNAL_ERROR;
    }
    watchData->sockfd = socketFd_;
    watchData->deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    auto self = shared_from_this();
    watchData->onComplete = [self](int errCode) { self->OnAsyncConnectComplete(errCode); };
    if (!ConnectMonitor::GetInstance().Register(socketFd_, watchData)) {
        NETSTACK_LOGE("connect monitor register failed, sock:%{public}d", socketFd_);
        Close();
        return SYSTEM_INTERNAL_ERROR;
    }
    NETSTACK_LOGI("async connect registered, sock:%{public}d", socketFd_);
    return SOCKET_ERROR_OK;
}

void TCPSocket::OnAsyncConnectComplete(int errCode)
{
    if (state_.IsClose() || socketFd_ <= 0) {
        NETSTACK_LOGE("async connect complete ignored, socket already closed, sock:%{public}d", socketFd_);
        return;
    }

    if (errCode != 0) {
        NETSTACK_LOGE("async connect failed, errCode:%{public}d, sock:%{public}d", errCode, socketFd_);
        auto nativeErr = ConvertSocketClientErrno(errCode > 0 ? errCode : UNKNOWN_ERROR);
        CallOnErrorCallback(nativeErr, GetSocketErrorMessage(nativeErr));
        Close();
        return;
    }
    if (!HandleTcpProxyAuth()) {
        NETSTACK_LOGE("socks5 proxy auth failed, sock:%{public}d", socketFd_);
        int32_t socks5Err = socks5Instance_ ? socks5Instance_->GetErrorCode() : UNKNOWN_ERROR;
        int32_t nativeErr = ConvertSocketClientErrno(socks5Err != 0 ? socks5Err : UNKNOWN_ERROR);
        CallOnErrorCallback(nativeErr, GetSocketErrorMessage(nativeErr));
        Close();
        return;
    }

    NETSTACK_LOGI("async connect success, sock:%{public}d", socketFd_);
    state_.SetIsConnected(true);
    RunRecvThread();
    CallOnConnectCallback();
}

int TCPSocket::Send(const TCPSendOptions &options)
{
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }
    const std::string &data = options.GetData();
    if (data.empty()) {
        NETSTACK_LOGE("TCPSendOptions data is empty.");
        return PARAM_ERROR_CODE;
    }
    if (socketFd_ <= 0) {
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }
    bool connected = CheckIsConnected(socketFd_);
    bool tfoEnabled = ExecCommonUtils::IsTfoEnabled(socketFd_);
    if (!connected && !tfoEnabled) {
        NETSTACK_LOGE("sock is not connect to remote, socket is %{public}d", socketFd_);
        return SYSTEM_INTERNAL_ERROR;
    }

    if (!ExecCommonUtils::PollSendData(socketFd_, data.c_str(), data.size(), nullptr, 0)) {
        NETSTACK_LOGE("tcp send failed, socket is %{public}d, errno is %{public}d", socketFd_, errno);
        return ConvertSocketClientErrno(errno);
    }
    return SOCKET_ERROR_OK;
}

int TCPSocket::Close()
{
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }

    if (state_.IsClose()) {
        return SOCKET_ERROR_OK;
    }

    if (socks5Instance_ != nullptr) {
        socks5Instance_->Close();
        socks5Instance_.reset();
    }

    state_.SetIsClose(true);

    if (socketFd_ < 0) {
        NETSTACK_LOGE("sock %{public}d is previous closed", socketFd_);
        return SOCKET_FD_INVALID_CODE;
    }

    int sockfd = socketFd_;
    socketFd_ = -1;
#ifdef OHOS_PLATFORM
    ConnectMonitor::GetInstance().Unregister(sockfd);
#endif
    int ret = close(sockfd);
    sockfd = -1;
    if (ret < 0) {
        NETSTACK_LOGE("sock closed failed, socket is %{public}d, errno is %{public}d", sockfd, errno);
        return ConvertSocketClientErrno(UNKNOWN_ERROR);
    }

    state_.SetIsBound(false);
    state_.SetIsConnected(false);
    address_ = {};

    CallOnCloseCallback();
    return SOCKET_ERROR_OK;
}

int TCPSocket::GetState(SocketStateBase &state)
{
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }
    if (socketFd_ < 0) {
        state.SetIsClose(state_.IsClose());
        return SOCKET_ERROR_OK;
    }
    if (!GetSocketState(socketFd_, state)) {
        return ConvertSocketClientErrno(errno);
    }
    return SOCKET_ERROR_OK;
}

int TCPSocket::GetRemoteAddress(NetAddress &address)
{
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }
    if (socketFd_ <= 0) {
        NETSTACK_LOGE("socket fd is closed or invalid.");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }
    if (ExecCommonUtils::FillRemoteAddress(socketFd_, address)) {
        return SOCKET_ERROR_OK;
    }
    return SYSTEM_INTERNAL_ERROR;
}

int TCPSocket::GetLocalAddress(NetAddress &address)
{
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }
    if (socketFd_ <= 0) {
        NETSTACK_LOGE("socket fd is closed or invalid.");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }
    if (!ExecCommonUtils::FillLocalAddress(socketFd_, address)) {
        NETSTACK_LOGE("FillLocalAddress failed.");
        return SYSTEM_INTERNAL_ERROR;
    }
    return SOCKET_ERROR_OK;
}

static bool SetTcpSocketExtraOptions(int sockFd, const TCPExtraOptions &options)
{
    if (options.AlreadySetKeepAlive()) {
        int keepAlive = options.IsKeepAlive() ? 1 : 0;
        if (setsockopt(sockFd, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(keepAlive)) != 0) {
            NETSTACK_LOGE("setsockopt SO_KEEPALIVE failed");
            return false;
        }
    }
    if (options.AlreadySetOobInline()) {
        int oobInline = options.IsOOBInline() ? 1 : 0;
        if (setsockopt(sockFd, SOL_SOCKET, SO_OOBINLINE, &oobInline, sizeof(oobInline)) != 0) {
            NETSTACK_LOGE("setsockopt SO_OOBINLINE failed");
            return false;
        }
    }
    if (options.AlreadySetTcpNoDelay()) {
        int noDelay = options.IsTCPNoDelay() ? 1 : 0;
        if (setsockopt(sockFd, IPPROTO_TCP, TCP_NODELAY, &noDelay, sizeof(noDelay)) != 0) {
            NETSTACK_LOGE("setsockopt TCP_NODELAY failed");
            return false;
        }
    }
#if !defined(IOS_PLATFORM)
    if (options.IsTCPFastOpen()) {
        int fastOpen = 1;
        if (setsockopt(sockFd, SOL_TCP, TCP_FASTOPEN_CONNECT, &fastOpen, sizeof(fastOpen)) < 0) {
            NETSTACK_LOGE("set SOL_TCP TFO failed! fd=%{public}d, errno=%{public}d", sockFd, errno);
            return false;
        }
    }
#endif
    if (options.AlreadySetLinger()) {
        struct linger ling = {};
        ling.l_onoff = options.socketLinger.IsOn() ? 1 : 0;
        ling.l_linger = static_cast<int>(options.socketLinger.GetLinger());
        if (setsockopt(sockFd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling)) != 0) {
            NETSTACK_LOGE("setsockopt SO_LINGER failed");
            return false;
        }
    }
    return true;
}

int TCPSocket::SetExtraOptions(const TCPExtraOptions &options)
{
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }
    if (socketFd_ <= 0) {
        NETSTACK_LOGE("socket fd is closed or invalid.");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }
    if (!ExecCommonUtils::SetExtraOptionsBase(socketFd_, static_cast<const ExtraOptionsBase &>(options))) {
        return ConvertSocketClientErrno(errno);
    }
    if (!SetTcpSocketExtraOptions(socketFd_, options)) {
        return ConvertSocketClientErrno(errno);
    }

    return SOCKET_ERROR_OK;
}

int TCPSocket::GetSocketFd(int32_t &socketFd) const
{
    socketFd = socketFd_;
    return SOCKET_ERROR_OK;
}

void TCPSocket::RunRecvThread()
{
    std::weak_ptr<TCPSocket> weak = shared_from_this();
    std::thread recvThread([weak]() {
        auto socket = weak.lock();
        if (socket == nullptr) {
            return;
        }
        socket->PollRecvData();
    });
#if defined(MAC_PLATFORM) || defined(IOS_PLATFORM)
    pthread_setname_np(SOCKET_EXEC_CONNECT);
#else
    pthread_setname_np(recvThread.native_handle(), SOCKET_EXEC_CONNECT);
#endif
    recvThread.detach();
}

void TCPSocket::OnMessage(const TCPSocketOnMessageCallback &messageCallback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onMessageCallback_ = messageCallback;
}

void TCPSocket::OffMessage()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (onMessageCallback_) {
        onMessageCallback_ = nullptr;
    }
}

void TCPSocket::OnConnect(const TCPSocketOnConnectCallback &listeningCallback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onConnectCallback_ = listeningCallback;
}

void TCPSocket::OffConnect()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (onConnectCallback_) {
        onConnectCallback_ = nullptr;
    }
}

void TCPSocket::OnError(const TCPSocketOnErrorCallback &onErrorCallback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onErrorCallback_ = onErrorCallback;
}

void TCPSocket::OffError()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (onErrorCallback_) {
        onErrorCallback_ = nullptr;
    }
}

void TCPSocket::OnClose(const TCPSocketOnCloseCallback &closeCallback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onCloseCallback_ = closeCallback;
}

void TCPSocket::OffClose()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (onCloseCallback_) {
        onCloseCallback_ = nullptr;
    }
}

void TCPSocket::CallOnMessageCallback(const std::string &data, const Socket::SocketRemoteInfo &remoteInfo)
{
    TCPSocketOnMessageCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (onMessageCallback_) {
            func = onMessageCallback_;
        }
    }

    if (func) {
        func(data, remoteInfo);
    }
}

void TCPSocket::CallOnConnectCallback()
{
    TCPSocketOnConnectCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (onConnectCallback_) {
            func = onConnectCallback_;
        }
    }

    if (func) {
        func();
    }
}

void TCPSocket::CallOnCloseCallback()
{
    TCPSocketOnCloseCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (onCloseCallback_) {
            func = onCloseCallback_;
        }
    }

    if (func) {
        func();
    }
}

void TCPSocket::CallOnErrorCallback(int32_t err, const std::string &errString)
{
    TCPSocketOnErrorCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (onErrorCallback_) {
            func = onErrorCallback_;
        }
    }

    if (func) {
        func(err, errString);
    }
}

int TCPSocket::TransferFd(int32_t& socketFd, std::shared_ptr<Socks5::Socks5Instance>& socks5Inst)
{
#ifdef OHOS_PLATFORM
    ConnectMonitor::GetInstance().Unregister(socketFd_);
#endif

    if (socks5Instance_ != nullptr) {
        socks5Inst = socks5Instance_;
        socks5Instance_.reset();
    }

    state_.SetIsClose(true);
    state_.SetIsBound(false);
    state_.SetIsConnected(false);
    address_ = {};

    socketFd = socketFd_;
    socketFd_ = -1;

    std::unique_lock<std::mutex> cvLock(cvMutex_);
    auto wp = std::weak_ptr<TCPSocket>(shared_from_this());
    cvRecvThreadRun_.wait(cvLock, [wp]() -> bool {
        auto tcpSocket = wp.lock();
        if (tcpSocket == nullptr) {
            return true;
        }
        return !tcpSocket->isRecvThreadRun_;
    });

    return SOCKET_ERROR_OK;
}

} // namespace Socket
} // namespace NetStack
} // namespace OHOS
