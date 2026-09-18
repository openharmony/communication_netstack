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

#include "socket_constant.h"
#include "udp_socket_innerapi.h"
#include "net_address.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <poll.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include "netstack_log.h"
#include "securec.h"
#include "socket_exec_common.h"
#include "netstack_common_utils.h"
#include "proxy_options.h"
#include "socket_exec_error.h"
#include "socks5.h"
#include "socks5_instance.h"
#include "socks5_utils.h"

namespace OHOS {
namespace NetStack {
namespace Socket {
static constexpr const char *SOCKET_EXEC_UDP_BIND = "OS_NET_SockUPRD";
static constexpr const char *SOCKET_RECV_FROM_MULTI_CAST = "OS_NET_SockMPRD";

static void SetIsBound(sa_family_t family, SocketStateBase &state, const sockaddr_in *addr4,
                       const sockaddr_in6 *addr6)
{
    if (family == AF_INET) {
        state.SetIsBound(ntohs(addr4->sin_port) != 0);
    } else if (family == AF_INET6) {
        state.SetIsBound(ntohs(addr6->sin6_port) != 0);
    }
}

static void SelectSockAddr(const sockaddr &sockAddr, sockaddr_in &addr4, sockaddr_in6 &addr6,
                           sockaddr* &addr, socklen_t &addrlen)
{
    if (sockAddr.sa_family == AF_INET) {
        addr = reinterpret_cast<sockaddr *>(&addr4);
        addrlen = sizeof(addr4);
    } else if (sockAddr.sa_family == AF_INET6) {
        addr = reinterpret_cast<sockaddr *>(&addr6);
        addrlen = sizeof(addr6);
    }
}

static bool CheckClosed(int sock, int &opt)
{
    socklen_t optLen = sizeof(int);
    if (getsockopt(sock, SOL_SOCKET, SO_TYPE, &opt, &optLen) < 0) {
        return true;
    }
    return false;
}

static bool CheckSocketFd(int sock, sockaddr &sockAddr)
{
    socklen_t len = sizeof(sockaddr);
    int ret = getsockname(sock, &sockAddr, &len);
    if (ret < 0) {
        return false;
    }
    return true;
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

    sockaddr sockAddr = {0};
    if (!CheckSocketFd(sock, sockAddr)) {
        return false;
    }

    sockaddr_in addr4 = {0};
    sockaddr_in6 addr6 = {0};
    sockaddr *addr = nullptr;
    socklen_t addrLen;
    SelectSockAddr(sockAddr, addr4, addr6, addr, addrLen);

    if (addr == nullptr) {
        NETSTACK_LOGE("addr family error, address invalid");
        return false;
    }

    (void)memset_s(addr, addrLen, 0, addrLen);
    socklen_t len = addrLen;
    int ret = getsockname(sock, addr, &len);
    if (ret < 0) {
        return false;
    }

    SetIsBound(sockAddr.sa_family, state, &addr4, &addr6);
    return true;
}

static inline int GetSockFamily(int fd)
{
    sockaddr sockAddr = {0};
    socklen_t len = sizeof(sockaddr);
    return (getsockname(fd, &sockAddr, &len) < 0) ? -1 : sockAddr.sa_family;
}

static bool IsTCPSocket(int sockfd)
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

static void FillRemoteInfoFromAddr(sockaddr *addr, size_t payloadLen, SocketRemoteInfo &remoteInfo)
{
    char addrBuf[INET6_ADDRSTRLEN] = {0};
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

void UDPSocket::PollRecvFinish()
{
    NETSTACK_LOGI("PollRecvFinish, socket is %{public}d", socketFd_);
}

void UDPSocket::ProcessPollResult(int currentFd)
{
    if (currentFd > 0 && !IsClose()) {
        NETSTACK_LOGE("poll to recv failed, socket is %{public}d, errno is %{public}d", currentFd, errno);
        auto errocode = ConvertSocketClientErrno(errno);
        CallOnErrorCallback(errocode, GetSocketErrorMessage(errocode));

        auto inst = GetSocks5Instance();
        if (inst != nullptr) {
            inst->OnProxySocketError();
        }
    }
}

int UDPSocket::UpdateRecvBuffer(int sock, int &bufferSize, std::unique_ptr<char[]> &buf)
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

int UDPSocket::ExitOrAbnormal(int sock, ssize_t recvLen)
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

    if (sock > 0) {
        NETSTACK_LOGE("recv fail, socket:%{public}d, recvLen:%{public}zd, errno:%{public}d",
            sock, recvLen, errno);
        auto errCode = ConvertSocketClientErrno(errno);
        CallOnErrorCallback(errCode, GetSocketErrorMessage(errCode));
    }
    return -1;
}

bool UDPSocket::SocketRecvHandle(int socketId, std::pair<std::unique_ptr<char[]> &, int> &bufInfo,
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

    size_t payloadLen = static_cast<size_t>(recvLen);
    auto socks5Inst = GetSocks5Instance();
    if (socks5Inst != nullptr &&
        !socks5Inst->RemoveHeader(bufInfo.first.get(), payloadLen, addrInfo.first->sa_family)) {
        NETSTACK_LOGE("remove socks5 udp header failed");
    }
    if (payloadLen == 0) {
        return true;
    }

    SocketRemoteInfo remoteInfo;
    FillRemoteInfoFromAddr(addrInfo.first, payloadLen, remoteInfo);
    std::string bufContent(bufInfo.first.get(), payloadLen);
    CallOnMessageCallback(bufContent, remoteInfo);
    return true;
}

bool UDPSocket::ProcessRecvFds(std::pair<std::unique_ptr<char[]> &, int> &bufInfo,
    std::pair<sockaddr *, socklen_t> &addrInfo, std::vector<pollfd> &fds,
    std::unordered_map<int, SocketRecvCallback> &socketCallbackMap)
{
    for (auto &fd : fds) {
        if ((static_cast<uint16_t>(fd.revents) & POLLERR) ||
            (static_cast<uint16_t>(fd.revents) & POLLNVAL)) {
            NETSTACK_LOGE("recv fail, socket:%{public}d, errno:%{public}d, revent:%{public}x",
                          fd.fd, errno, fd.revents);
            if (socketFd_ > 0 && !IsClose()) {
                auto errCode = ConvertSocketClientErrno(errno);
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
        if (cb != nullptr && !cb(fd.fd, bufInfo, addrInfo)) {
            return false;
        }
    }
    return true;
}

bool UDPSocket::ProxyUdpSocketRecvHandle(int socketId, std::pair<std::unique_ptr<char[]> &, int> &bufInfo,
    std::pair<sockaddr *, socklen_t> &addrInfo)
{
    const auto recvLen = recv(socketId, bufInfo.first.get(), bufInfo.second, 0);
    if (recvLen > 0) {
        return true;
    }
    const int32_t errCode{errno};
    if ((errCode == EAGAIN) || (errCode == EINTR)) {
        return true;
    }
    Socks5::Socks5Utils::PrintRecvErrMsg(socketId, errCode, recvLen, "SocketRecvHandle");

    if (!IsClose()) {
        auto socks5Inst = GetSocks5Instance();
        if (socks5Inst != nullptr) {
            socks5Inst->OnProxySocketError();
        } else {
            NETSTACK_LOGE("socks5 instance is null");
        }
    }
    return false;
}

bool UDPSocket::PreparePollFds(int &currentFd, std::vector<pollfd> &fds,
                               std::unordered_map<int, SocketRecvCallback> &socketCallbackMap)
{
    socketCallbackMap.clear();
    fds.clear();
    currentFd = socketFd_;
    if (currentFd <= 0) {
        NETSTACK_LOGE("currentFd: %{public}d is error", currentFd);
        return false;
    }

    socketCallbackMap[currentFd] = [this](int socketId,
        std::pair<std::unique_ptr<char[]> &, int> &bufInfo,
        std::pair<sockaddr *, socklen_t> &addrInfo) {
        return SocketRecvHandle(socketId, bufInfo, addrInfo);
    };
    fds.push_back({currentFd, POLLIN, 0});
    auto inst = GetSocks5Instance();
    if (inst != nullptr && inst->IsConnected()) {
        if (inst->GetSocketId() != -1 && inst->GetSocketId() != currentFd) {
            socketCallbackMap[inst->GetSocketId()] = [this](int socketId,
                std::pair<std::unique_ptr<char[]> &, int> &bufInfo,
                std::pair<sockaddr *, socklen_t> &addrInfo) {
                return ProxyUdpSocketRecvHandle(socketId, bufInfo, addrInfo);
            };
            fds.push_back({inst->GetSocketId(), POLLIN, 0});
        }
    }

    return true;
}

void UDPSocket::PollRecvData(sockaddr *addr, socklen_t addrLen)
{
    if (socketFd_ < 0) {
        PollRecvFinish();
        NETSTACK_LOGE("fd is nullptr or closed");
        return;
    }
    int bufferSize = ExecCommonUtils::ConfirmBufferSize(socketFd_);
    auto buf = std::make_unique<char[]>(bufferSize);

    std::pair<std::unique_ptr<char[]> &, int> bufInfo{buf, bufferSize};
    std::pair<sockaddr *, socklen_t> addrInfo{addr, addrLen};
    std::unordered_map<int, SocketRecvCallback> socketCallbackMap{};
    std::vector<pollfd> fds{};

    while (!IsClose()) {
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

static constexpr const char *WILD_ADDRESS = "0.0.0.0";

std::shared_ptr<Socks5::Socks5UdpInstance> UDPSocket::InitSocks5UdpInstance(const NetAddress &destAddress,
    const ProxyOptions &proxyOptions)
{
    const std::shared_ptr<Socks5::Socks5Option> opt{std::make_shared<Socks5::Socks5Option>()};
    opt->username_ = proxyOptions.username_;
    opt->password_ = proxyOptions.password_;
    opt->proxyAddress_.netAddress_ = proxyOptions.address_;
    socklen_t len = 0;
    ExecCommonUtils::GetSocketAddr(&opt->proxyAddress_.netAddress_,
        &opt->proxyAddress_.addrV4_, &opt->proxyAddress_.addrV6_, &opt->proxyAddress_.addr_, &len);
    if (opt->proxyAddress_.addr_ == nullptr) {
        NETSTACK_LOGE("socks5 udp instance: addr family error, address invalid");
        return nullptr;
    }

    auto socks5Udp = std::make_shared<Socks5::Socks5UdpInstance>();
    socks5Udp->SetDestAddress(destAddress);
    socks5Udp->AddHeader();
    socks5Udp->SetSocks5Option(opt);
    return socks5Udp;
}

int UDPSocket::HandleUdpProxyOptions(UDPSocket *socket, UDPSendOptions &sendOptions,
    const ProxyOptions &proxyOptions)
{
    if (proxyOptions.type_ != ProxyType::SOCKS5) {
        NETSTACK_LOGE("unsupport proxy type");
        return SOCKET_ERROR_OK;
    }

    auto socks5Udp = GetSocks5Instance();
    if (socks5Udp == nullptr) {
        socks5Udp = InitSocks5UdpInstance(sendOptions.address, proxyOptions);
        if (socks5Udp == nullptr) {
            return SYSTEM_INTERNAL_ERROR;
        }
        socks5Udp->SetSocks5Instance(socks5Udp);
        socket->SetSocks5Instance(socks5Udp);
    }

    if (!socks5Udp->IsConnected()) {
        if (!socks5Udp->Connect()) {
            const int32_t errCode = socks5Udp->GetErrorCode();
            const std::string errMsg = socks5Udp->GetErrorMessage();
            NETSTACK_LOGE("socks5 auth failed, errCode:%{public}d, errMsg::%{public}s", errCode, errMsg.c_str());
            return errCode;
        }
    }

    socks5Udp->SetDestAddress(sendOptions.address);
    socks5Udp->AddHeader();

    Socket::NetAddress bindAddr = socks5Udp->GetProxyBindAddress();
    sendOptions.SetData(socks5Udp->GetHeader() + sendOptions.GetData());

    /* process wild address from some socks5 server */
    if (bindAddr.GetAddress() == WILD_ADDRESS) {
        bindAddr.SetAddress(proxyOptions.address_.GetAddress());
    }
    sendOptions.address = bindAddr;
    return SOCKET_ERROR_OK;
}

UDPSocket::UDPSocket() {}

UDPSocket::~UDPSocket()
{
    Close();
}

const char *UDPSocket::GetRecvThreadName() const
{
    return SOCKET_EXEC_UDP_BIND;
}

int UDPSocket::Bind(const NetAddress &address)
{
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }
    if (!ExecCommonUtils::ValidateAddress(address)) {
        return PARAM_ERROR_CODE;
    }

    socketFd_ = ExecCommonUtils::MakeUdpSocket(address.GetSaFamily());
    if (socketFd_ < 0) {
        NETSTACK_LOGE("make udp socket failed. errno: %{public}d.", errno);
        return ConvertSocketClientErrno(errno);
    }

    state_.SetIsClose(false);
    state_.SetIsBound(false);
    state_.SetIsConnected(false);

    int reuse = reuseAddr_ ? 1 : 0;
    if (setsockopt(socketFd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0) {
        NETSTACK_LOGE("set SO_REUSEADDR failed, fd: %{public}d, errno: %{public}d.", socketFd_, errno);
        int32_t savedErrno = errno;
        close(socketFd_);
        socketFd_ = -1;
        return ConvertSocketClientErrno(savedErrno);
    }

    bindAddr_ = {};
    bindAddrLen_ = 0;
    if (!ExecCommonUtils::MakeSockAddr(address, bindAddr_, bindAddrLen_)) {
        NETSTACK_LOGE("MakeSockAddr failed, errno: %{public}d.", errno);
        int32_t savedErrno = errno;
        close(socketFd_);
        socketFd_ = -1;
        return ConvertSocketClientErrno(savedErrno);
    }
    if (!DoBindWithRetry()) {
        int32_t savedErrno = errno;
        close(socketFd_);
        socketFd_ = -1;
        return ConvertSocketClientErrno(savedErrno);
    }
    NETSTACK_LOGI("bind success, sock:%{public}d", socketFd_);
    state_.SetIsBound(true);
    RunRecvThread();
    CallOnListeningCallback();
    return SOCKET_ERROR_OK;
}

bool UDPSocket::DoBindWithRetry()
{
    if (bind(socketFd_, reinterpret_cast<sockaddr *>(&bindAddr_), bindAddrLen_) == 0) {
        return true;
    }
    NETSTACK_LOGE("udp bind failed, errno=%{public}d", errno);
    if (errno != EADDRINUSE) {
        return false;
    }
    if (bindAddr_.ss_family == AF_INET) {
        NETSTACK_LOGI("distribute a random port");
        reinterpret_cast<sockaddr_in *>(&bindAddr_)->sin_port = 0;
    } else if (bindAddr_.ss_family == AF_INET6) {
        NETSTACK_LOGI("distribute a random port");
        reinterpret_cast<sockaddr_in6 *>(&bindAddr_)->sin6_port = 0;
    }
    if (bind(socketFd_, reinterpret_cast<sockaddr *>(&bindAddr_), bindAddrLen_) != 0) {
        NETSTACK_LOGE("udp bind retry failed, errno=%{public}d", errno);
        return false;
    }
    NETSTACK_LOGI("rebind success");
    return true;
}

int UDPSocket::GetLocalAddress(NetAddress &address)
{
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }
    if (socketFd_ < 0) {
        NETSTACK_LOGE("socket fd is invalid.");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }
    if (!ExecCommonUtils::FillLocalAddress(socketFd_, address)) {
        NETSTACK_LOGE("FillLocalAddress, errno: %{public}d.", errno);
        return ConvertSocketClientErrno(errno);
    }
    return SOCKET_ERROR_OK;
}

int UDPSocket::Send(const UDPSendOptions &sendOptions, const ProxyOptions &proxyOptions)
{
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }
    if (sendOptions.GetData().empty()) {
        NETSTACK_LOGE("UDPSendOptions data is empty.");
        return PARAM_ERROR_CODE;
    }
    if (!ExecCommonUtils::ValidateAddress(sendOptions.address)) {
        if (sendOptions.address.GetFamily() == NetAddress::Family::DOMAIN_NAME) {
            return ConvertSocketClientErrno(EINVAL);
        }
        return PARAM_ERROR_CODE;
    }
    if (sendOptions.address.GetPort() <= 0) {
        NETSTACK_LOGE("socket fd is invalid.");
        return ConvertSocketClientErrno(EINVAL);
    }
    if (socketFd_ <= 0) {
        NETSTACK_LOGE("socket fd is invalid.");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }

    UDPSendOptions options = sendOptions;
    int errcode = HandleUdpProxyOptions(this, options, proxyOptions);
    if (errcode != SOCKET_ERROR_OK) {
        return errcode;
    }
    return DoSend(options);
}

int UDPSocket::DoSend(const UDPSendOptions &sendOptions)
{
    NetAddress destAddr = sendOptions.address;
    destAddr.SetRawAddress(ExecCommonUtils::ConvertAddressToIp(destAddr.GetAddress(), destAddr.GetSaFamily()));

    sockaddr_storage ss{};
    socklen_t addrLen = 0;
    if (!ExecCommonUtils::MakeSockAddr(destAddr, ss, addrLen)) {
        NETSTACK_LOGE("udp send: MakeSockAddr failed");
        return SYSTEM_INTERNAL_ERROR;
    }
    if (!ExecCommonUtils::PollSendData(socketFd_, sendOptions.GetData().c_str(), sendOptions.GetData().size(),
                                       reinterpret_cast<sockaddr *>(&ss), addrLen)) {
        NETSTACK_LOGE("udp send failed, socket is %{public}d, errno is %{public}d", socketFd_, errno);
        return ConvertSocketClientErrno(errno);
    }
    return SOCKET_ERROR_OK;
}

int UDPSocket::Close()
{
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }
    if (state_.IsClose()) {
        return SOCKET_ERROR_OK;
    }
    if (socketFd_ <= 0) {
        NETSTACK_LOGE("socket fd is invalid.");
        return SOCKET_FD_INVALID_CODE;
    }

    state_.SetIsClose(true);
    if (socks5Instance_ != nullptr) {
        socks5Instance_->Close();
        socks5Instance_.reset();
    }
    if (socketFd_ >= 0) {
        close(socketFd_);
        socketFd_ = -1;
    }
    state_.SetIsBound(false);
    state_.SetIsConnected(false);

    CallOnCloseCallback();
    return SOCKET_ERROR_OK;
}

int UDPSocket::GetState(SocketStateBase &state)
{
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }
    if (socketFd_ <= 0) {
        state.SetIsClose(state_.IsClose());
        return SOCKET_ERROR_OK;
    }
    if (!GetSocketState(socketFd_, state)) {
        NETSTACK_LOGE("GetSocketState failed, errno: %{public}d.", errno);
        return SYSTEM_INTERNAL_ERROR;
    }
    return SOCKET_ERROR_OK;
}

int UDPSocket::SetExtraOptions(const UDPExtraOptions &options)
{
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }
    if (options.AlreadySetTimeout() && static_cast<int32_t>(options.GetSocketTimeout()) < 0) {
        return ConvertSocketClientErrno(EDOM);
    }
    if (socketFd_ <= 0) {
        NETSTACK_LOGE("socket fd is invalid.");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }
    if (!ExecCommonUtils::SetExtraOptionsBase(socketFd_, static_cast<const ExtraOptionsBase &>(options))) {
        return PARAM_ERROR_CODE;
    }
    if (options.AlreadySetBroadcast()) {
        int broadcast = options.IsBroadcast() ? 1 : 0;
        if (setsockopt(socketFd_, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) != 0) {
            NETSTACK_LOGE("setsockopt SO_BROADCAST failed, errno: %{public}d.", errno);
            return PARAM_ERROR_CODE;
        }
    }
    return SOCKET_ERROR_OK;
}

int UDPSocket::GetSocketFd(int &socketFd) const
{
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }
    socketFd = socketFd_;
    return SOCKET_ERROR_OK;
}

void UDPSocket::RunRecvThread()
{
    std::weak_ptr<UDPSocket> weak = shared_from_this();
    std::thread recvThread([weak]() {
        auto socket = weak.lock();
        if (socket == nullptr) {
            return;
        }
        socket->PollRecvData(reinterpret_cast<sockaddr *>(&socket->bindAddr_), socket->bindAddrLen_);
    });
#if defined(MAC_PLATFORM) || defined(IOS_PLATFORM)
    pthread_setname_np(GetRecvThreadName());
#else
    pthread_setname_np(recvThread.native_handle(), GetRecvThreadName());
#endif
    recvThread.detach();
}

MulticastSocket::MulticastSocket() : UDPSocket() {}

MulticastSocket::~MulticastSocket() {}

const char *MulticastSocket::GetRecvThreadName() const
{
    return SOCKET_RECV_FROM_MULTI_CAST;
}

int MulticastSocket::AddMembership(const NetAddress &multicastAddress)
{
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }
    if (!ExecCommonUtils::ValidateAddress(multicastAddress)) {
        NETSTACK_LOGE("ExecCommonUtils::ValidateAddress failed.");
        return ConvertSocketClientErrno(EINVAL);
    }
    if (socketFd_ <= 0) {
        socketFd_ = ExecCommonUtils::MakeUdpSocket(multicastAddress.GetSaFamily());
        if (socketFd_ < 0) {
            NETSTACK_LOGE("make multicast udp socket failed");
            return ConvertSocketClientErrno(errno);
        }
    }
    state_.SetIsClose(false);
    state_.SetIsBound(false);
    state_.SetIsConnected(false);

    if (!JoinMulticastGroup(multicastAddress) || !BindMulticastAddr(multicastAddress)) {
        close(socketFd_);
        socketFd_ = -1;
        return ConvertSocketClientErrno(errno);
    }
    NETSTACK_LOGI("addmembership ok, sock:%{public}d", socketFd_);
    RunRecvThread();
    return SOCKET_ERROR_OK;
}

bool MulticastSocket::JoinMulticastGroup(const NetAddress &multicastAddress)
{
    if (multicastAddress.GetFamily() == NetAddress::Family::IPv4) {
        ip_mreq mreq = {};
        mreq.imr_multiaddr.s_addr = inet_addr(multicastAddress.GetAddress().c_str());
        mreq.imr_interface.s_addr = INADDR_ANY;
        if (setsockopt(socketFd_, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) != 0) {
            NETSTACK_LOGE("ipv4 addmembership err: %{public}d", errno);
            return false;
        }
        return true;
    }
    ipv6_mreq mreq6 = {};
    if (inet_pton(AF_INET6, multicastAddress.GetAddress().c_str(), &mreq6.ipv6mr_multiaddr) != 1) {
        NETSTACK_LOGE("inet_pton failed.");
        return false;
    }
    mreq6.ipv6mr_interface = 0;
    if (setsockopt(socketFd_, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq6, sizeof(mreq6)) != 0) {
        NETSTACK_LOGE("ipv6 addmembership err: %{public}d", errno);
        return false;
    }
    return true;
}

bool MulticastSocket::BindMulticastAddr(const NetAddress &multicastAddress)
{
    if (multicastAddress.GetFamily() == NetAddress::Family::IPv4) {
        sockaddr_in addr4 = {0};
        addr4.sin_family = AF_INET;
        addr4.sin_port = htons(multicastAddress.GetPort());
        addr4.sin_addr.s_addr = htonl(INADDR_ANY);
        if (bind(socketFd_, reinterpret_cast<sockaddr *>(&addr4), sizeof(addr4)) < 0) {
            NETSTACK_LOGE("multicast v4 bind err, port:%{public}d, errno:%{public}d",
                multicastAddress.GetPort(), errno);
            return false;
        }
        bindAddr_ = {};
        (void)memcpy_s(&bindAddr_, sizeof(bindAddr_), &addr4, sizeof(addr4));
        bindAddrLen_ = sizeof(addr4);
        return true;
    }
    sockaddr_in6 addr6 = {0};
    addr6.sin6_family = AF_INET6;
    addr6.sin6_port = htons(multicastAddress.GetPort());
    addr6.sin6_addr = in6addr_any;
    if (bind(socketFd_, reinterpret_cast<sockaddr *>(&addr6), sizeof(addr6)) < 0) {
        NETSTACK_LOGE("multicast v6 bind err, port:%{public}d, errno:%{public}d",
            multicastAddress.GetPort(), errno);
        return false;
    }
    bindAddr_ = {};
    (void)memcpy_s(&bindAddr_, sizeof(bindAddr_), &addr6, sizeof(addr6));
    bindAddrLen_ = sizeof(addr6);
    return true;
}

int MulticastSocket::DropMembership(const NetAddress &multicastAddress)
{
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }
    if (!ExecCommonUtils::ValidateAddress(multicastAddress)) {
        NETSTACK_LOGE("ExecCommonUtils::ValidateAddress failed.");
        return ConvertSocketClientErrno(EADDRNOTAVAIL);
    }
    if (socketFd_ < 0) {
        NETSTACK_LOGE("socket fd is invalid.");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }

    auto family = multicastAddress.GetFamily();
    if (family == NetAddress::Family::IPv4) {
        ip_mreq mreq = {};
        mreq.imr_multiaddr.s_addr = inet_addr(multicastAddress.GetAddress().c_str());
        mreq.imr_interface.s_addr = INADDR_ANY;
        if (setsockopt(socketFd_, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mreq, sizeof(mreq)) != 0) {
            NETSTACK_LOGE("ipv4 dropmembership err: %{public}d", errno);
            return ConvertSocketClientErrno(errno);
        }
    } else if (family == NetAddress::Family::IPv6) {
        ipv6_mreq mreq6 = {};
        if (inet_pton(AF_INET6, multicastAddress.GetAddress().c_str(), &mreq6.ipv6mr_multiaddr) != 1) {
            NETSTACK_LOGE("inet_pton failed.");
            return PARAM_ERROR_CODE;
        }
        mreq6.ipv6mr_interface = 0;
        if (setsockopt(socketFd_, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &mreq6, sizeof(mreq6)) != 0) {
            NETSTACK_LOGE("ipv6 dropmembership err: %{public}d", errno);
            return ConvertSocketClientErrno(errno);
        }
    } else {
        NETSTACK_LOGE("address family is unspec.");
        return PARAM_ERROR_CODE;
    }

    if (close(socketFd_) < 0) {
        NETSTACK_LOGE("sock closed failed, socket is %{public}d, errno is %{public}d", socketFd_, errno);
        return ConvertSocketClientErrno(errno);
    }
    socketFd_ = -1;
    return SOCKET_ERROR_OK;
}

int MulticastSocket::SetMulticastTTL(int ttl)
{
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }
    if (socketFd_ < 0) {
        NETSTACK_LOGE("socket fd is invalid.");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }
    int ttlVal = ttl;
    int family = GetSockFamily(socketFd_);
    if (setsockopt(socketFd_, (family == AF_INET) ? IPPROTO_IP : IPPROTO_IPV6, (family == AF_INET) ?
        IP_MULTICAST_TTL : IPV6_MULTICAST_HOPS, reinterpret_cast<void *>(&ttlVal), sizeof(ttlVal)) != 0) {
        NETSTACK_LOGE("set ttl err, ttl:%{public}d, family:%{public}d, errno:%{public}d", ttl, family, errno);
        return ConvertSocketClientErrno(errno);
    }
    return SOCKET_ERROR_OK;
}

int MulticastSocket::GetMulticastTTL(int &ttl)
{
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }
    if (socketFd_ < 0) {
        NETSTACK_LOGE("socket fd is invalid.");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }
    int ttlVal = 0;
    socklen_t ttlLen = sizeof(ttlVal);
    int family = GetSockFamily(socketFd_);
    if (getsockopt(socketFd_, (family == AF_INET) ? IPPROTO_IP : IPPROTO_IPV6, (family == AF_INET) ?
        IP_MULTICAST_TTL : IPV6_MULTICAST_HOPS, reinterpret_cast<void *>(&ttlVal), &ttlLen) != 0) {
        NETSTACK_LOGE("get ttl err, family:%{public}d, errno:%{public}d", family, errno);
        return ConvertSocketClientErrno(errno);
    }

    ttl = ttlVal;
    return SOCKET_ERROR_OK;
}

int MulticastSocket::SetLoopbackMode(bool loopback)
{
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }
    if (socketFd_ < 0) {
        NETSTACK_LOGE("socket fd is invalid.");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }
    int mode = static_cast<int>(loopback);
    int family = GetSockFamily(socketFd_);
    if (setsockopt(socketFd_, (family == AF_INET) ? IPPROTO_IP : IPPROTO_IPV6, (family == AF_INET) ?
        IP_MULTICAST_LOOP : IPV6_MULTICAST_LOOP, reinterpret_cast<void *>(&mode), sizeof(mode)) != 0) {
        NETSTACK_LOGE("setloopback err, loop mode:%{public}d, family:%{public}d, err:%{public}d",
                      mode, family, errno);
        return ConvertSocketClientErrno(errno);
    }

    return SOCKET_ERROR_OK;
}

int MulticastSocket::GetLoopbackMode(bool &loopback)
{
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }
    if (socketFd_ < 0) {
        NETSTACK_LOGE("socket fd is invalid.");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }
    int mode = 0;
    socklen_t len = sizeof(mode);
    int family = GetSockFamily(socketFd_);
    if (getsockopt(socketFd_, (family == AF_INET) ? IPPROTO_IP : IPPROTO_IPV6, (family == AF_INET) ?
        IP_MULTICAST_LOOP : IPV6_MULTICAST_LOOP, reinterpret_cast<void *>(&mode), &len) == -1) {
        NETSTACK_LOGE("getloopback err, family:%{public}d, errno:%{public}d", family, errno);
        return ConvertSocketClientErrno(errno);
    }

    loopback = (mode != 0);
    return SOCKET_ERROR_OK;
}

int MulticastSocket::SetReuseAddress(bool reuse)
{
    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("INTERNET permission denied.");
        return PERMISSION_DENIED_CODE;
    }
    reuseAddr_ = reuse;
    if (socketFd_ < 0) {
        NETSTACK_LOGI("setReuseAddress socket not created, will apply on bind, reuse:%{public}d", reuse);
        return SOCKET_ERROR_OK;
    }
    int reuseVal = reuse ? 1 : 0;
    if (setsockopt(socketFd_, SOL_SOCKET, SO_REUSEADDR, &reuseVal, sizeof(reuseVal)) != 0) {
        NETSTACK_LOGE("setsockopt SO_REUSEADDR failed, errno=%{public}d", errno);
        return ConvertSocketClientErrno(errno);
    }
    NETSTACK_LOGI("setReuseAddress setsockopt success, fd:%{public}d, reuse:%{public}d", socketFd_, reuse);
    return SOCKET_ERROR_OK;
}

void UDPSocket::OnMessage(const UDPSocketOnMessageCallback &messageCallback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onMessageCallback_ = messageCallback;
}

void UDPSocket::OffMessage()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (onMessageCallback_) {
        onMessageCallback_ = nullptr;
    }
}

void UDPSocket::OnListening(const UDPSocketOnListeningCallback &listeningCallback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onListeningCallback_ = listeningCallback;
}

void UDPSocket::OffListening()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (onListeningCallback_) {
        onListeningCallback_ = nullptr;
    }
}

void UDPSocket::OnError(const UDPSocketOnErrorCallback &onErrorCallback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onErrorCallback_ = onErrorCallback;
}

void UDPSocket::OffError()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (onErrorCallback_) {
        onErrorCallback_ = nullptr;
    }
}

void UDPSocket::OnClose(const UDPSocketOnCloseCallback &closeCallback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onCloseCallback_ = closeCallback;
}

void UDPSocket::OffClose()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (onCloseCallback_) {
        onCloseCallback_ = nullptr;
    }
}

void UDPSocket::CallOnMessageCallback(const std::string &data, const Socket::SocketRemoteInfo &remoteInfo)
{
    UDPSocketOnMessageCallback func = nullptr;
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

void UDPSocket::CallOnListeningCallback()
{
    UDPSocketOnListeningCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (onListeningCallback_) {
            func = onListeningCallback_;
        }
    }

    if (func) {
        func();
    }
}

void UDPSocket::CallOnCloseCallback()
{
    UDPSocketOnCloseCallback func = nullptr;
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

void UDPSocket::CallOnErrorCallback(int32_t err, const std::string &errString)
{
    UDPSocketOnErrorCallback func = nullptr;
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

} // namespace Socket
} // namespace NetStack
} // namespace OHOS
