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

#include "local_socket_client_innerapi.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <unistd.h>

#include "netstack_log.h"
#include "securec.h"
#include "socket_constant.h"
#include "socket_exec_common.h"
#include "socket_exec_error.h"

namespace OHOS {
namespace NetStack {
namespace Socket {
constexpr char LOCAL_SOCKET_CONNECT[] = "OS_NET_LSCon";

static bool NonBlockConnect(int32_t sock, sockaddr *addr, socklen_t addrLen, int32_t timeoutMSec)
{
    if (connect(sock, addr, addrLen) == -1) {
        pollfd fds[1] = {{.fd = sock, .events = POLLOUT}};
        if (errno != EINPROGRESS) {
            NETSTACK_LOGE("connect error, fd: %{public}d, errno: %{public}d", sock, errno);
            return false;
        }
        int32_t pollResult = poll(fds, 1, timeoutMSec);
        if (pollResult == 0) {
            NETSTACK_LOGE("connection timeout, fd: %{public}d, timeout: %{public}d", sock, timeoutMSec);
            return false;
        } else if (pollResult == -1) {
            NETSTACK_LOGE("poll connect error, fd: %{public}d, errno: %{public}d", sock, errno);
            return false;
        }
        int32_t error = 0;
        socklen_t errorLen = sizeof(error);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &errorLen) < 0 || error != 0) {
            NETSTACK_LOGE("failed to get socket so_error, fd: %{public}d, errno: %{public}d", sock, errno);
            return false;
        }
    }
    return true;
}

static bool LocalSocketSendEvent(int32_t sock, const std::string &buffer)
{
    if (!ExecCommonUtils::PollSendData(sock, buffer.c_str(), buffer.size(), nullptr, 0)) {
        NETSTACK_LOGE("send failed, socket is %{public}d, errno is %{public}d", sock, errno);
        return false;
    }
    return true;
}

LocalSocket::LocalSocket() {}

LocalSocket::~LocalSocket()
{
    Close();
}

int32_t LocalSocket::Bind(const std::string &socketPath)
{
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    if (strcpy_s(addr.sun_path, sizeof(addr.sun_path) - 1, socketPath.c_str()) != 0) {
        NETSTACK_LOGE("failed to copy socket path.");
        return PARAM_ERROR_CODE;
    }
    unlink(addr.sun_path);

    if (socketFd_ < 0 && (socketFd_ = ExecCommonUtils::MakeLocalSocket(SOCK_STREAM)) < 0) {
        return ConvertSocketClientErrno(errno);
    }

    if (bind(socketFd_, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == -1) {
        NETSTACK_LOGE("failed to bind local socket, sockfd: %{public}d, errno: %{public}d", socketFd_, errno);
        close(socketFd_);
        socketFd_ = -1;
        return ConvertSocketClientErrno(errno);
    }

    address_ = socketPath;
    state_.SetIsBound(true);
    return SOCKET_ERROR_OK;
}

int32_t LocalSocket::Connect(const std::string &socketPath, int32_t timeout)
{
    struct sockaddr_un addr;
    memset_s(&addr, sizeof(addr), 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    if (socketFd_ < 0 && (socketFd_ = ExecCommonUtils::MakeLocalSocket(SOCK_STREAM, false)) < 0) {
        return ConvertSocketClientErrno(errno);
    }

    ExecCommonUtils::SetSocketBufferSize(socketFd_, SO_RCVBUF,
        static_cast<uint32_t>(DEFAULT_BUFFER_SIZE));
    if (strcpy_s(addr.sun_path, sizeof(addr.sun_path) - 1, socketPath.c_str()) != 0) {
        NETSTACK_LOGE("failed to copy local socket path, sockfd: %{public}d", socketFd_);
        return ConvertSocketClientErrno(UNKNOWN_ERROR);
    }

    NETSTACK_LOGI("local socket client fd: %{public}d", socketFd_);
    if (!NonBlockConnect(socketFd_, reinterpret_cast<sockaddr *>(&addr), sizeof(addr), timeout)) {
        NETSTACK_LOGE("failed to connect local socket, errno: %{public}d, %{public}s", errno, strerror(errno));
        return ConvertSocketClientErrno(errno);
    }

    state_.SetIsConnected(true);
    RunRecvThread();

    CallOnConnectCallback();
    return SOCKET_ERROR_OK;
}

int32_t LocalSocket::Send(const LocalSocketOptions &options)
{
    if (options.GetBufferRef().empty()) {
        NETSTACK_LOGE("LocalSocketOptions buffer string is empty.");
        return PARAM_ERROR_CODE;
    }
    if (socketFd_ < 0) {
        NETSTACK_LOGE("fd is invalid or closed");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }

    if (!LocalSocketSendEvent(socketFd_, options.GetBufferRef())) {
        NETSTACK_LOGE("send failed, socket is %{public}d, errno is %{public}d", socketFd_, errno);
        return ConvertSocketClientErrno(errno);
    }
    return SOCKET_ERROR_OK;
}

int32_t LocalSocket::Close()
{
    if (socketFd_ < 0) {
        NETSTACK_LOGE("fd is invalid or closed");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }

    shutdown(socketFd_, SHUT_RDWR);
    if (close(socketFd_) < 0) {
        if (onErrorCallback_) {
            auto errCode = ConvertSocketClientErrno(errno);
            onErrorCallback_(errCode, GetSocketErrorMessage(errCode));
        }
        NETSTACK_LOGE("failed to closed localsock, fd: %{public}d, errno: %{public}d", socketFd_, errno);
        return ConvertSocketClientErrno(errno);
    }
    socketFd_ = -1;
    address_ = "";
    state_.SetIsClose(true);
    state_.SetIsBound(false);
    state_.SetIsConnected(false);

    CallOnCloseCallback();
    return SOCKET_ERROR_OK;
}

int32_t LocalSocket::GetState(SocketStateBase &state)
{
    if (socketFd_ < 0) {
        state = state_;
        return SOCKET_ERROR_OK;
    }

    struct sockaddr_un unAddr{};
    socklen_t len = sizeof(unAddr);
    if (getsockname(socketFd_, reinterpret_cast<struct sockaddr *>(&unAddr), &len) < 0) {
        NETSTACK_LOGI("local socket do not bind or socket has closed");
        state.SetIsBound(false);
    } else {
        state.SetIsBound(strlen(unAddr.sun_path) > 0);
    }
    state.SetIsConnected(state_.IsConnected());

    return SOCKET_ERROR_OK;
}

int32_t LocalSocket::GetSocketFd(int32_t &socketFd) const
{
    socketFd = socketFd_;
    return SOCKET_ERROR_OK;
}

int32_t LocalSocket::SetExtraOptions(const LocalExtraOptions &options)
{
    if (options.GetSendBufferSize() > MAX_SOCKET_BUFFER_SIZE ||
        options.GetReceiveBufferSize() > MAX_SOCKET_BUFFER_SIZE) {
        NETSTACK_LOGI("options buffer size is great than %{public}d.", MAX_SOCKET_BUFFER_SIZE);
        return PARAM_ERROR_CODE;
    }
    if (socketFd_ < 0) {
        NETSTACK_LOGE("fd is invalid or closed");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }
    if (!ExecCommonUtils::SetLocalSocketOptions(socketFd_, options)) {
        return ConvertSocketClientErrno(errno);
    }

    return SOCKET_ERROR_OK;
}

int32_t LocalSocket::GetExtraOptions(LocalExtraOptions &options)
{
    if (socketFd_ < 0) {
        NETSTACK_LOGE("fd is invalid or closed");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }
    if (!ExecCommonUtils::GetLocalSocketOptions(socketFd_, options)) {
        return ConvertSocketClientErrno(errno);
    }

    return SOCKET_ERROR_OK;
}

int32_t LocalSocket::GetLocalAddress(std::string &socketPath)
{
    if (socketFd_ < 0) {
        NETSTACK_LOGE("fd is invalid or closed");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }

    struct sockaddr_un unAddr{};
    socklen_t len = sizeof(unAddr);
    if (getsockname(socketFd_, (struct sockaddr *)&unAddr, &len) != 0) {
        NETSTACK_LOGE("local socket get socket name fail, fd: %{public}d, errno: %{public}d", socketFd_, errno);
        return ConvertSocketClientErrno(errno);
    }

    socketPath = unAddr.sun_path;
    return SOCKET_ERROR_OK;
}

void LocalSocket::RunRecvThread()
{
    std::weak_ptr<LocalSocket> weak = shared_from_this();

    std::thread recvThread([weak]() {
#if defined(MAC_PLATFORM) || defined(IOS_PLATFORM)
        pthread_setname_np(LOCAL_SOCKET_CONNECT);
#else
        pthread_setname_np(pthread_self(), LOCAL_SOCKET_CONNECT);
#endif
        while (auto socket = weak.lock()) {
            if (!socket->DoRecvIteration()) {
                break;
            }
        }
    });

    recvThread.detach();
}

bool LocalSocket::DoRecvIteration()
{
    int sock = socketFd_;
    int32_t bufferSize = ExecCommonUtils::ConfirmBufferSize(sock);
    auto buf = std::make_unique<char[]>(bufferSize);
    if (buf == nullptr) {
        auto errCode = SYSTEM_INTERNAL_ERROR;
        CallOnErrorCallback(errCode, GetSocketErrorMessage(errCode));
        return false;
    }
    pollfd fds[1] = {{.fd = sock, .events = POLLIN}};
    int32_t timeoutMs = ExecCommonUtils::ConfirmSocketTimeoutMs(sock, SO_RCVTIMEO, DEFAULT_POLL_TIMEOUT);
    int32_t ret = poll(fds, 1, timeoutMs);
    if (ret < 0) {
        NETSTACK_LOGE("poll failed, sock:%{public}d errno:%{public}d", sock, errno);
        auto errCode = ConvertSocketClientErrno(errno);
        CallOnErrorCallback(errCode, GetSocketErrorMessage(errCode));
        return false;
    } else if (ret == 0) {
        return true;
    }
    auto recvLen = recv(sock, buf.get(), bufferSize, 0);
    if (recvLen < 0) {
        if (errno == EAGAIN || errno == EINTR) {
            return true;
        }
        NETSTACK_LOGE("recv failed, sock:%{public}d errno:%{public}d", sock, errno);
        auto errCode = ConvertSocketClientErrno(errno);
        CallOnErrorCallback(errCode, GetSocketErrorMessage(errCode));
        return false;
    } else if (recvLen == 0) {
        CallOnCloseCallback();
        return false;
    }
    std::string bufContent(buf.get(), recvLen);
    CallOnMessageCallback(bufContent, address_, recvLen);
    return true;
}

void LocalSocket::OnMessage(const LocalSocketOnMessageCallback &messageCallback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onMessageCallback_ = messageCallback;
}

void LocalSocket::OffMessage()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (onMessageCallback_) {
        onMessageCallback_ = nullptr;
    }
}

void LocalSocket::OnConnect(const LocalSocketOnConnectCallback &connectCallback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onConnectCallback_ = connectCallback;
}

void LocalSocket::OffConnect()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (onConnectCallback_) {
        onConnectCallback_ = nullptr;
    }
}

void LocalSocket::OnError(const LocalSocketOnErrorCallback &onErrorCallback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onErrorCallback_ = onErrorCallback;
}

void LocalSocket::OffError()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (onErrorCallback_) {
        onErrorCallback_ = nullptr;
    }
}

void LocalSocket::OnClose(const LocalSocketOnCloseCallback &closeCallback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onCloseCallback_ = closeCallback;
}

void LocalSocket::OffClose()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (onCloseCallback_) {
        onCloseCallback_ = nullptr;
    }
}

void LocalSocket::CallOnMessageCallback(const std::string &data, const std::string &address, const size_t size)
{
    LocalSocketOnMessageCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (onMessageCallback_) {
            func = onMessageCallback_;
        }
    }

    if (func) {
        func(data, address, size);
    }
}

void LocalSocket::CallOnConnectCallback()
{
    LocalSocketOnConnectCallback func = nullptr;
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


void LocalSocket::CallOnCloseCallback()
{
    LocalSocketOnCloseCallback func = nullptr;
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

void LocalSocket::CallOnErrorCallback(int32_t err, const std::string &errString)
{
    LocalSocketOnErrorCallback func = nullptr;
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
