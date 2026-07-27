/*
 * Copyright (c) 2024-2025 Huawei Device Co., Ltd.
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

#include "socket_exec_common.h"

#include <algorithm>
#include <arpa/inet.h>
#include <chrono>
#include <netdb.h>
#include <netinet/tcp.h>

#include "netstack_log.h"
#include "tcp_connect_options.h"

namespace OHOS::NetStack::Socket {
static constexpr const int DEFAULT_TIMEOUT_MS = 20000;

static bool GetSendBufferSize(int sock, int &bufferSize, int &sockType)
{
    int opt = 0;
    socklen_t optLen = sizeof(opt);
    bufferSize = DEFAULT_BUFFER_SIZE;

    if (getsockopt(sock, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<void *>(&opt), &optLen) >= 0 && opt > 0) {
        bufferSize = opt;
    }

    sockType = 0;
    optLen = sizeof(sockType);
    if (getsockopt(sock, SOL_SOCKET, SO_TYPE, reinterpret_cast<void *>(&sockType), &optLen) < 0) {
        return false;
    }
    return true;
}

static bool HandlePollEvent(struct pollfd *fds)
{
    if (fds == nullptr) {
        return false;
    }
    if (static_cast<uint16_t>(fds[0].revents) & (POLLNVAL | POLLHUP | POLLERR)) {
        NETSTACK_LOGE("NonBlockConnect poll failed, socket is %{public}d, revents is %{public}x",
            fds[0].fd, fds[0].revents);
        return false;
    }

    int err = 0;
    socklen_t optLen = sizeof(err);
    int ret = getsockopt(fds[0].fd, SOL_SOCKET, SO_ERROR, reinterpret_cast<void *>(&err), &optLen);
    if (ret < 0) {
        return false;
    }
    if (err != 0) {
        NETSTACK_LOGE("NonBlockConnect exec failed, socket is %{public}d, err is %{public}d", fds[0].fd, err);
        return false;
    }
    return true;
}

bool ExecCommonUtils::MakeNonBlock(int sock)
{
    int flags = fcntl(sock, F_GETFL, 0);
    while (flags == -1 && errno == EINTR) {
        flags = fcntl(sock, F_GETFL, 0);
    }
    if (flags == -1) {
        NETSTACK_LOGE("make non block failed, socket is %{public}d, errno is %{public}d", sock, errno);
        return false;
    }
    int ret = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    while (ret == -1 && errno == EINTR) {
        ret = fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    }
    if (ret == -1) {
        NETSTACK_LOGE("make non block failed, socket is %{public}d, errno is %{public}d", sock, errno);
        return false;
    }
    return true;
}

int ExecCommonUtils::MakeTcpSocket(sa_family_t family, bool needNonblock)
{
    if (family != AF_INET && family != AF_INET6) {
        return -1;
    }
    int sock = socket(family, SOCK_STREAM, IPPROTO_TCP);
    NETSTACK_LOGI("new tcp socket is %{public}d", sock);
    if (sock < 0) {
        NETSTACK_LOGE("make tcp socket failed, errno is %{public}d", errno);
        return -1;
    }
    if (needNonblock && !MakeNonBlock(sock)) {
        close(sock);
        return -1;
    }
    return sock;
}

int ExecCommonUtils::MakeUdpSocket(sa_family_t family)
{
    if (family != AF_INET && family != AF_INET6) {
        return -1;
    }
    int sock = socket(family, SOCK_DGRAM, IPPROTO_UDP);
    NETSTACK_LOGI("new udp socket is %{public}d", sock);
    if (sock < 0) {
        NETSTACK_LOGE("make udp socket failed, errno is %{public}d", errno);
        return -1;
    }
    if (!MakeNonBlock(sock)) {
        close(sock);
        return -1;
    }
    return sock;
}

int ExecCommonUtils::MakeLocalSocket(int socketType, bool needNonblock)
{
    int sock = socket(AF_UNIX, socketType, 0);
    NETSTACK_LOGI("new local socket is %{public}d", sock);
    if (sock < 0) {
        NETSTACK_LOGE("make local socket failed, errno is %{public}d", errno);
        return -1;
    }
    if (needNonblock && !MakeNonBlock(sock)) {
        close(sock);
        return -1;
    }
    return sock;
}

bool ExecCommonUtils::IsTfoEnabled(int sock)
{
    int tfoEnabled = 0;
#if !defined(IOS_PLATFORM)
    socklen_t tfoLen = sizeof(tfoEnabled);
    if (getsockopt(sock, SOL_TCP, TCP_FASTOPEN_CONNECT, &tfoEnabled, &tfoLen) != 0) {
        NETSTACK_LOGE("get TFO failed, fd=%{public}d, errno=%{public}d", sock, errno);
        tfoEnabled = 0;
    }
#endif
    return tfoEnabled != 0;
}

std::string ExecCommonUtils::ConvertAddressToIp(const std::string &address, sa_family_t family)
{
    if (address.empty()) {
        return {};
    }
    addrinfo hints{};
    hints.ai_family = family;
    char ipStr[INET6_ADDRSTRLEN] = {0};
    addrinfo *res = nullptr;
    auto status = getaddrinfo(address.c_str(), nullptr, &hints, &res);
    if (status != 0 || res == nullptr) {
        return {};
    }
    std::string ip;
    if (res->ai_family == AF_INET) {
        auto *ipv4 = reinterpret_cast<struct sockaddr_in *>(res->ai_addr);
        auto addr = &(ipv4->sin_addr);
        inet_ntop(res->ai_family, addr, ipStr, sizeof(ipStr));
        ip = ipStr;
    } else {
        auto *ipv6 = reinterpret_cast<struct sockaddr_in6 *>(res->ai_addr);
        auto addr = &(ipv6->sin6_addr);
        inet_ntop(res->ai_family, addr, ipStr, sizeof(ipStr));
        ip = ipStr;
    }
    freeaddrinfo(res);
    return ip;
}

bool ExecCommonUtils::IpMatchFamily(const std::string &address, sa_family_t family)
{
    if (family == AF_INET6) {
        in_addr ipv4{};
        if (inet_pton(AF_INET, address.c_str(), &(ipv4.s_addr)) > 0) {
            return false;
        }
    } else if (family == AF_INET) {
        in6_addr ipv6{};
        if (inet_pton(AF_INET6, address.c_str(), &ipv6) > 0) {
            return false;
        }
    }
    return true;
}

bool ExecCommonUtils::NonBlockConnect(int sock, sockaddr *addr, socklen_t addrLen, uint32_t timeoutMSec)
{
    int ret = connect(sock, addr, addrLen);
    if (ret >= 0) {
        return true;
    }
    if (errno != EINPROGRESS) {
        return false;
    }
    struct pollfd fds[1] = {{.fd = sock, .events = POLLOUT}};
    int timeoutMs = (timeoutMSec == 0) ? DEFAULT_CONNECT_TIMEOUT : timeoutMSec;
    while (true) {
        auto startTime = std::chrono::steady_clock::now();
        ret = poll(fds, 1, timeoutMs);
        if (ret > 0) {
            break;
        } else if (ret == 0) {
            NETSTACK_LOGE("connect poll timeout, socket is %{public}d", sock);
            return false;
        }

        if (errno == EINTR) {
            auto endTime = std::chrono::steady_clock::now();
            auto intervalMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
            timeoutMs -= static_cast<int>(intervalMs.count());
            if (timeoutMs <= 0) {
                NETSTACK_LOGE("invalid timeout");
                return false;
            }
            continue;
        }
        NETSTACK_LOGE("connect poll failed, socket is %{public}d, errno is %{public}d", sock, errno);
        return false;
    }

    return HandlePollEvent(fds);
}

bool ExecCommonUtils::PollSendData(int sock, const char *data, size_t size, sockaddr *addr, socklen_t addrLen)
{
    NETSTACK_LOGD("js send RawSize: %{public}zu", size);
    int bufferSize = DEFAULT_BUFFER_SIZE;
    int sockType = 0;
    if (!GetSendBufferSize(sock, bufferSize, sockType)) {
        NETSTACK_LOGI("get sock opt sock type failed, socket is %{public}d, errno is %{public}d", sock, errno);
        return false;
    }

    auto curPos = data;
    auto leftSize = size;
    nfds_t num = 1;
    pollfd fds[1] = {{0}};
    fds[0].fd = sock;
    fds[0].events = 0;
    fds[0].events |= POLLOUT;
    int sendTimeoutMs = ConfirmSocketTimeoutMs(sock, SO_SNDTIMEO, DEFAULT_TIMEOUT_MS);
    if (sendTimeoutMs < 0) {
        return false;
    }

    bool tfoEnabled = IsTfoEnabled(sock);

    while (leftSize > 0) {
        if (!PollFd(fds, num, sendTimeoutMs)) {
            if (errno != EINTR) {
                return false;
            }
        }
        size_t sendSize = (sockType == SOCK_STREAM ? leftSize : std::min<size_t>(leftSize, bufferSize));
        ssize_t sendLen = tfoEnabled ? send(sock, curPos, sendSize, 0)
            : sendto(sock, curPos, sendSize, 0, addr, addrLen);
        NETSTACK_LOGD("socketFD: %{public}d, send len: %{public}zu", sock, sendLen);
        if (sendLen < 0) {
            if (errno == EAGAIN || errno == EINTR) {
                continue;
            }
            NETSTACK_LOGE("send failed, socket is %{public}d, errno is %{public}d", sock, errno);
            return false;
        }
        if (sendLen == 0) {
            break;
        }
        curPos += sendLen;
        leftSize -= sendLen;
    }

    if (leftSize != 0) {
        NETSTACK_LOGE("send not complete, socket is %{public}d, errno is %{public}d", sock, errno);
        return false;
    }
    return true;
}

int ExecCommonUtils::ConfirmSocketTimeoutMs(int sock, int type, int defaultValue)
{
    timeval timeout;
    socklen_t optlen = sizeof(timeout);
    if (getsockopt(sock, SOL_SOCKET, type, reinterpret_cast<void *>(&timeout), &optlen) < 0) {
        NETSTACK_LOGE("get timeout failed, type: %{public}d, sock: %{public}d, errno: %{public}d", type, sock, errno);
        if (errno == ENOTSOCK && type == SO_RCVTIMEO) {
            return -1;
        }
        return defaultValue;
    }
    auto socketTimeoutMs = timeout.tv_sec * UNIT_CONVERSION_1000 + timeout.tv_usec / UNIT_CONVERSION_1000;
    return socketTimeoutMs == 0 ? defaultValue : socketTimeoutMs;
}

int ExecCommonUtils::ConfirmBufferSize(int sock)
{
    int bufferSize = -1;
    int opt = 0;
    socklen_t optLen = sizeof(opt);
    if (getsockopt(sock, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<void *>(&opt), &optLen) >= 0 && opt > 0) {
        bufferSize = opt;
    }
    if (bufferSize <= 0 || bufferSize > MAX_SOCKET_BUFFER_SIZE) {
        NETSTACK_LOGE("buffer size is out of range, size: %{public}d", bufferSize);
        bufferSize = DEFAULT_BUFFER_SIZE;
    }
    return bufferSize;
}

bool ExecCommonUtils::SetSocketBufferSize(int sockfd, int type, uint32_t size)
{
    if (size > MAX_SOCKET_BUFFER_SIZE) {
        NETSTACK_LOGE("invalid socket buffer size: %{public}u", size);
        return false;
    }
    if (setsockopt(sockfd, SOL_SOCKET, type, reinterpret_cast<void *>(&size), sizeof(size)) < 0) {
        NETSTACK_LOGE("localsocket set sock size failed, sock: %{public}d, type: %{public}d, size: %{public}u",
                      sockfd, type, size);
        return false;
    }
    return true;
}

bool ExecCommonUtils::SetLocalSocketOptions(int sockfd, const OHOS::NetStack::Socket::LocalExtraOptions &options)
{
    if (options.AlreadySetRecvBufSize()) {
        uint32_t recvBufSize = options.GetReceiveBufferSize();
        if (!SetSocketBufferSize(sockfd, SO_RCVBUF, recvBufSize)) {
            return false;
        }
    }
    if (options.AlreadySetSendBufSize()) {
        uint32_t sendBufSize = options.GetSendBufferSize();
        if (!SetSocketBufferSize(sockfd, SO_SNDBUF, sendBufSize)) {
            return false;
        }
    }
    if (options.AlreadySetTimeout()) {
        uint32_t timeMs = options.GetSocketTimeout();
        timeval timeout = {timeMs / UNIT_CONVERSION_1000, (timeMs % UNIT_CONVERSION_1000) * UNIT_CONVERSION_1000};
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<void *>(&timeout), sizeof(timeout)) < 0) {
            NETSTACK_LOGE("localsocket setsockopt error, SO_RCVTIMEO, fd: %{public}d", sockfd);
            return false;
        }
        if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<void *>(&timeout), sizeof(timeout)) < 0) {
            NETSTACK_LOGE("localsocket setsockopt error, SO_SNDTIMEO, fd: %{public}d", sockfd);
            return false;
        }
    }
    return true;
}

bool ExecCommonUtils::GetLocalSocketOptions(int sockfd, OHOS::NetStack::Socket::LocalExtraOptions &options)
{
    int result = 0;
    socklen_t len = sizeof(result);
    if (getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, &result, &len) == -1) {
        NETSTACK_LOGE("getsockopt error, SO_RCVBUF");
        return false;
    }
    options.SetReceiveBufferSize(result);
    options.SetRecvBufSizeFlag(true);
    if (getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, &result, &len) == -1) {
        NETSTACK_LOGE("getsockopt error, SO_SNDBUF");
        return false;
    }
    options.SetSendBufferSize(result);
    options.SetSendBufSizeFlag(true);
    timeval timeout;
    socklen_t timeLen = sizeof(timeout);
    if (getsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, &timeLen) == -1) {
        NETSTACK_LOGE("getsockopt error, SO_SNDTIMEO");
        return false;
    }
    options.SetSocketTimeout(timeout.tv_sec * UNIT_CONVERSION_1000 + timeout.tv_usec / UNIT_CONVERSION_1000);
    options.SetTimeoutFlag(true);
    return true;
}

bool ExecCommonUtils::PollFd(pollfd *fds, nfds_t num, int timeout)
{
    int ret = poll(fds, num, timeout);
    if (ret == -1) {
        NETSTACK_LOGE("poll to send failed, socket is %{public}d, errno is %{public}d", fds->fd, errno);
        return false;
    }
    if (ret == 0) {
        NETSTACK_LOGE("poll to send timeout, socket is %{public}d, timeout is %{public}d", fds->fd, timeout);
        return false;
    }
    return true;
}

bool ExecCommonUtils::ValidateAddress(const NetAddress &address)
{
    auto family = address.GetFamily();
    if (family != NetAddress::Family::IPv4 && family != NetAddress::Family::IPv6) {
        NETSTACK_LOGE("address family is unspec.");
        return false;
    }
    if (address.GetAddress().empty()) {
        NETSTACK_LOGE("address is empty");
        return false;
    }
    return true;
}

bool ExecCommonUtils::MakeSockAddr(const NetAddress &address, sockaddr_storage &ss, socklen_t &addrLen)
{
    sa_family_t family = address.GetSaFamily();
    std::string addr = address.GetAddress();
    uint16_t port = address.GetPort();

    if (family == AF_INET) {
        auto *addr4 = reinterpret_cast<sockaddr_in *>(&ss);
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(port);
        if (inet_pton(AF_INET, addr.c_str(), &addr4->sin_addr) != 1) {
            NETSTACK_LOGE("inet_pton failed.");
            return false;
        }
        addrLen = sizeof(sockaddr_in);
    } else if (family == AF_INET6) {
        auto *addr6 = reinterpret_cast<sockaddr_in6 *>(&ss);
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(port);
        if (inet_pton(AF_INET6, addr.c_str(), &addr6->sin6_addr) != 1) {
            NETSTACK_LOGE("inet_pton failed.");
            return false;
        }
        addrLen = sizeof(sockaddr_in6);
    } else {
        NETSTACK_LOGE("can not support family %{public}d", family);
        return false;
    }
    return true;
}

bool ExecCommonUtils::FillLocalAddress(int sock, NetAddress &outAddr)
{
    sockaddr_storage ss{};
    socklen_t addrLen = sizeof(ss);
    if (getsockname(sock, reinterpret_cast<sockaddr *>(&ss), &addrLen) != 0) {
        return false;
    }
    char addrBuf[INET6_ADDRSTRLEN] = {0};
    if (ss.ss_family == AF_INET) {
        auto *addr4 = reinterpret_cast<sockaddr_in *>(&ss);
        if (inet_ntop(AF_INET, &addr4->sin_addr, addrBuf, sizeof(addrBuf)) == nullptr) {
            NETSTACK_LOGE("inet_ntop failed. family = %{public}d", AF_INET6);
            return false;
        }
        outAddr.SetFamilyByJsValue(static_cast<uint32_t>(NetAddress::Family::IPv4));
        outAddr.SetAddress(std::string(addrBuf));
        outAddr.SetPort(ntohs(addr4->sin_port));
    } else if (ss.ss_family == AF_INET6) {
        auto *addr6 = reinterpret_cast<sockaddr_in6 *>(&ss);
        if (inet_ntop(AF_INET6, &addr6->sin6_addr, addrBuf, sizeof(addrBuf)) == nullptr) {
            NETSTACK_LOGE("inet_ntop failed. family = %{public}d", AF_INET6);
            return false;
        }
        outAddr.SetFamilyByJsValue(static_cast<uint32_t>(NetAddress::Family::IPv6));
        outAddr.SetAddress(std::string(addrBuf));
        outAddr.SetPort(ntohs(addr6->sin6_port));
    } else {
        NETSTACK_LOGE("can not support family %{public}d", ss.ss_family);
        return false;
    }
    return true;
}

bool ExecCommonUtils::FillRemoteAddress(int sock, NetAddress &outAddr)
{
    sockaddr_storage ss{};
    socklen_t addrLen = sizeof(ss);
    if (getpeername(sock, reinterpret_cast<sockaddr *>(&ss), &addrLen) != 0) {
        return false;
    }
    char addrBuf[INET6_ADDRSTRLEN] = {0};
    if (ss.ss_family == AF_INET) {
        auto *addr4 = reinterpret_cast<sockaddr_in *>(&ss);
        if (inet_ntop(AF_INET, &addr4->sin_addr, addrBuf, sizeof(addrBuf)) == nullptr) {
            NETSTACK_LOGE("inet_ntop failed. family = %{public}d", AF_INET6);
            return false;
        }
        outAddr.SetFamilyByJsValue(static_cast<uint32_t>(NetAddress::Family::IPv4));
        outAddr.SetAddress(std::string(addrBuf));
        outAddr.SetPort(ntohs(addr4->sin_port));
    } else if (ss.ss_family == AF_INET6) {
        auto *addr6 = reinterpret_cast<sockaddr_in6 *>(&ss);
        if (inet_ntop(AF_INET6, &addr6->sin6_addr, addrBuf, sizeof(addrBuf)) == nullptr) {
            NETSTACK_LOGE("inet_ntop failed. family = %{public}d", AF_INET6);
            return false;
        }
        outAddr.SetFamilyByJsValue(static_cast<uint32_t>(NetAddress::Family::IPv6));
        outAddr.SetAddress(std::string(addrBuf));
        outAddr.SetPort(ntohs(addr6->sin6_port));
    } else {
        NETSTACK_LOGE("can not support family %{public}d", ss.ss_family);
        return false;
    }
    return true;
}

void ExecCommonUtils::GetSocketAddr(const NetAddress *address, sockaddr_in *addr4, sockaddr_in6 *addr6,
    sockaddr **addr, socklen_t *len)
{
    sa_family_t family = address->GetSaFamily();
    if (family == AF_INET) {
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(address->GetPort());
        if (inet_pton(AF_INET, address->GetAddress().c_str(), &addr4->sin_addr) != 1) {
            NETSTACK_LOGE("inet_pton failed.");
            return;
        }
        *addr = reinterpret_cast<sockaddr *>(addr4);
        *len = sizeof(sockaddr_in);
    } else if (family == AF_INET6) {
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(address->GetPort());
        if (inet_pton(AF_INET6, address->GetAddress().c_str(), &addr6->sin6_addr) != 1) {
            NETSTACK_LOGE("inet_pton failed.");
            return;
        }
        *addr = reinterpret_cast<sockaddr *>(addr6);
        *len = sizeof(sockaddr_in6);
    }
}

bool ExecCommonUtils::SetExtraOptionsBase(int sock, const ExtraOptionsBase &options)
{
    if (options.AlreadySetRecvBufSize()) {
        int size = static_cast<int>(options.GetReceiveBufferSize());
        if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) != 0) {
            NETSTACK_LOGE("setsockopt SO_RCVBUF failed, errno=%{public}d", errno);
            return false;
        }
    }
    if (options.AlreadySetSendBufSize()) {
        int size = static_cast<int>(options.GetSendBufferSize());
        if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) != 0) {
            NETSTACK_LOGE("setsockopt SO_SNDBUF failed, errno=%{public}d", errno);
            return false;
        }
    }
    if (options.AlreadySetReuseAddr()) {
        int reuse = options.IsReuseAddress() ? 1 : 0;
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0) {
            NETSTACK_LOGE("setsockopt SO_REUSEADDR failed, errno=%{public}d", errno);
            return false;
        }
    }
    if (options.AlreadySetTimeout()) {
        int value = static_cast<int>(options.GetSocketTimeout());
        struct timeval tv = {value / UNIT_CONVERSION_1000, (value % UNIT_CONVERSION_1000) * UNIT_CONVERSION_1000};
        if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
            NETSTACK_LOGE("setsockopt SO_RCVTIMEO failed, errno=%{public}d", errno);
            return false;
        }
        if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) != 0) {
            NETSTACK_LOGE("setsockopt SO_SNDTIMEO failed, errno=%{public}d", errno);
            return false;
        }
    }
    return true;
}


} // namespace OHOS::NetStack::Socket