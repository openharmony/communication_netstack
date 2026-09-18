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

#include "local_socket_server_innerapi.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

#if !defined(MAC_PLATFORM) && !defined(IOS_PLATFORM)
#include <sys/epoll.h>
#endif

#include "netstack_log.h"
#include "securec.h"
#include "socket_exec_common.h"
#include "socket_constant.h"
#include "socket_exec_error.h"

namespace OHOS {
namespace NetStack {
namespace Socket {
constexpr int BACKLOG = 32;
constexpr char LOCAL_SOCKET_SERVER_ACCEPT_RECV_DATA[] = "OS_NET_LSAccRD";
constexpr char LOCAL_SOCKET_SERVER_HANDLE_CLIENT[] = "OS_NET_LSAcc";
constexpr int MAX_EVENTS = 10;

static bool LocalSocketServerBind(int sockfd, const std::string &socketPath)
{
    unlink(socketPath.c_str());
    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;
    if (strcpy_s(addr.sun_path, sizeof(addr.sun_path) - 1, socketPath.c_str()) != 0) {
        NETSTACK_LOGE("failed to copy socket path");
        return false;
    }
    if (bind(sockfd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) == -1) {
        NETSTACK_LOGE("failed to bind local socket, fd: %{public}d, errno: %{public}d", sockfd, errno);
        return false;
    }
    NETSTACK_LOGI("local socket server bind success");
    return true;
}

LocalSocketConnection::LocalSocketConnection(int32_t clientId) : clientId_(clientId)
{
}

LocalSocketConnection::~LocalSocketConnection()
{
    Close();
}

int LocalSocketConnection::Send(const LocalSocketOptions &options)
{
    auto buffer = options.GetBufferRef();
    if (buffer.empty()) {
        NETSTACK_LOGE("LocalSocketOptions buffer string is empty.");
        return PARAM_ERROR_CODE;
    }
    if (socketFd_ < 0) {
        NETSTACK_LOGE("fd is invalid or closed");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }

    if (!ExecCommonUtils::PollSendData(socketFd_, buffer.c_str(), buffer.size(), nullptr, 0)) {
        NETSTACK_LOGE("send failed, socket is %{public}d, errno is %{public}d", socketFd_, errno);
        return ConvertSocketClientErrno(errno);
    }

    return SOCKET_ERROR_OK;
}

int LocalSocketConnection::Close()
{
    if (socketFd_ < 0) {
        NETSTACK_LOGE("fd is invalid or close");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }
    if (shutdown(socketFd_, SHUT_RDWR) != 0) {
        NETSTACK_LOGE("socket shutdown failed, socket is %{public}d, errno is %{public}d", socketFd_, errno);
    }
    if (close(socketFd_) < 0) {
        NETSTACK_LOGE("sock closed failed, socket is %{public}d, errno is %{public}d", socketFd_, errno);
    }
    socketFd_ = -1;
    CallOnCloseCallback();
    return SOCKET_ERROR_OK;
}

int LocalSocketConnection::GetLocalAddress(std::string &socketPath)
{
    if (socketFd_ < 0) {
        NETSTACK_LOGE("invalid socket fd or socket has closed");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }
    sockaddr_un addr = {};
    socklen_t addrLen = sizeof(addr);
    if (getsockname(socketFd_, reinterpret_cast<sockaddr *>(&addr), &addrLen) != 0) {
        NETSTACK_LOGE("local socket get socket name fail, socket fd:%{public}d, errno: %{public}d", socketFd_, errno);
        return ConvertSocketClientErrno(errno);
    }
    socketPath = addr.sun_path;
    return SOCKET_ERROR_OK;
}

int LocalSocketConnection::GetSocketFd(int32_t &socketFd) const
{
    socketFd = socketFd_;
    return SOCKET_ERROR_OK;
}

void LocalSocketConnection::CallOnMessageCallback(const std::string &data, const std::string &address,
    const size_t size)
{
    LocalSocketConnectionOnMessageCallback callback = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (onMessageCallback_) {
            callback = onMessageCallback_;
        }
    }
    if (callback) {
        callback(data, address, size);
    }
}

void LocalSocketConnection::CallOnCloseCallback()
{
    LocalSocketConnectionOnCloseCallback callback = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (onCloseCallback_) {
            callback = onCloseCallback_;
        }
    }
    if (callback) {
        callback();
    }
}

void LocalSocketConnection::CallOnErrorCallback(const int32_t err, const std::string &errMsg)
{
    LocalSocketConnectionOnErrorCallback callback = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (onErrorCallback_) {
            callback = onErrorCallback_;
        }
    }
    if (callback) {
        callback(err, errMsg);
    }
}

void LocalSocketConnection::OnMessage(const LocalSocketConnectionOnMessageCallback &messageCallback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onMessageCallback_ = messageCallback;
}

void LocalSocketConnection::OnClose(const LocalSocketConnectionOnCloseCallback &closeCallback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onCloseCallback_ = closeCallback;
}

void LocalSocketConnection::OnError(const LocalSocketConnectionOnErrorCallback &onErrorCallback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onErrorCallback_ = onErrorCallback;
}

void LocalSocketConnection::OffMessage()
{
    std::lock_guard<std::mutex> lock(mutex_);
    onMessageCallback_ = nullptr;
}

void LocalSocketConnection::OffClose()
{
    std::lock_guard<std::mutex> lock(mutex_);
    onCloseCallback_ = nullptr;
}

void LocalSocketConnection::OffError()
{
    std::lock_guard<std::mutex> lock(mutex_);
    onErrorCallback_ = nullptr;
}

LocalSocketServer::LocalSocketServer() {}

LocalSocketServer::~LocalSocketServer()
{
    Close();
}

int LocalSocketServer::Listen(const std::string &socketPath)
{
    if (socketPath.empty()) {
        NETSTACK_LOGE("socketPath is empty");
        return PARAM_ERROR_CODE;
    }
    if (listenFd_ > 0) {
        NETSTACK_LOGE("local socket server is already listening, fd: %{public}d", listenFd_);
        return PARAM_ERROR_CODE;
    }
#if !defined(MAC_PLATFORM) && !defined(IOS_PLATFORM)
    if (StartEpoll() < 0) {
        NETSTACK_LOGE("failed to StartEpoll, errno: %{public}d", errno);
        ReleaseListenAndEpoll();
        return ConvertSocketClientErrno(errno);
    }
#endif
    socketPath_ = socketPath;
    listenFd_ = ExecCommonUtils::MakeLocalSocket(SOCK_STREAM);
    if (listenFd_ < 0) {
        NETSTACK_LOGE("failed to MakeLocalSocket");
        ReleaseListenAndEpoll();
        return ConvertSocketClientErrno(errno);
    }
    if (!LocalSocketServerBind(listenFd_, socketPath_)) {
        NETSTACK_LOGE("failed to LocalSocketServerBind");
        ReleaseListenAndEpoll();
        return ConvertSocketClientErrno(errno);
    }
    if (listen(listenFd_, BACKLOG) < 0) {
        NETSTACK_LOGE("local socket server listen error, fd: %{public}d", listenFd_);
        ReleaseListenAndEpoll();
        return ConvertSocketClientErrno(errno);
    }
    NETSTACK_LOGI("local socket server listen success");
    state_.SetIsClose(false);
    state_.SetIsBound(true);
    RunAcceptThread();
    return SOCKET_ERROR_OK;
}

int LocalSocketServer::Close()
{
    if (state_.IsClose()) {
        return SOCKET_ERROR_OK;
    }
    state_.SetIsClose(true);

    if (listenFd_ >= 0) {
        shutdown(listenFd_, SHUT_RDWR);
        close(listenFd_);
        listenFd_ = -1;
    }
    if (epollFd_ >= 0) {
        close(epollFd_);
        epollFd_ = -1;
    }

    {
        std::unique_lock<std::shared_mutex> connLock(connMapMutex_);
        for (auto &pair : connectionMap_) {
            pair.second->Close();
        }
    }

    WaitForEndingLoop();
    connectionMap_.clear();
    if (!socketPath_.empty()) {
        unlink(socketPath_.c_str());
    }
    state_.SetIsBound(false);
    state_.SetIsConnected(false);
    return SOCKET_ERROR_OK;
}

int LocalSocketServer::GetState(SocketStateBase &state)
{
    if (listenFd_ < 0) {
        state = state_;
        return SOCKET_ERROR_OK;
    }
    struct sockaddr_un unAddr{};
    socklen_t len = sizeof(unAddr);
    if (getsockname(listenFd_, reinterpret_cast<struct sockaddr *>(&unAddr), &len) == 0) {
        state.SetIsBound(true);
    }
    state.SetIsConnected(connectionMap_.size() > 0);
    return SOCKET_ERROR_OK;
}

int LocalSocketServer::SetExtraOptions(const LocalExtraOptions &options)
{
    if (options.GetSendBufferSize() > MAX_SOCKET_BUFFER_SIZE ||
        options.GetReceiveBufferSize() > MAX_SOCKET_BUFFER_SIZE) {
        NETSTACK_LOGI("options buffer size is great than %{public}d.", MAX_SOCKET_BUFFER_SIZE);
        return PARAM_ERROR_CODE;
    }
    if (listenFd_ < 0) {
        NETSTACK_LOGE("listenFd is invalid or closed");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }
    {
        std::shared_lock<std::shared_mutex> lock(connMapMutex_);
        for (const auto &pair : connectionMap_) {
            int fd = -1;
            pair.second->GetSocketFd(fd);
            if (fd < 0) {
                NETSTACK_LOGE("fd is invalid or closed");
                return ConvertSocketClientErrno(ERRNO_BAD_FD);
            }
            if (!ExecCommonUtils::SetLocalSocketOptions(fd, options)) {
                return ConvertSocketClientErrno(errno);
            }
        }
    }
    extraOptions_ = options;
    alreadySetExtraOptions_ = true;
    return SOCKET_ERROR_OK;
}

int LocalSocketServer::GetExtraOptions(LocalExtraOptions &options)
{
    if (alreadySetExtraOptions_) {
        options = extraOptions_;
        return SOCKET_ERROR_OK;
    }
    if (listenFd_ < 0) {
        NETSTACK_LOGE("listenFd is invalid or closed");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }
    if (!ExecCommonUtils::GetLocalSocketOptions(listenFd_, options)) {
        return ConvertSocketClientErrno(errno);
    }
    options.SetReceiveBufferSize(DEFAULT_BUFFER_SIZE);
    options.SetSendBufferSize(DEFAULT_BUFFER_SIZE);
    return SOCKET_ERROR_OK;
}

int LocalSocketServer::GetLocalAddress(std::string &socketPath)
{
    if (listenFd_ < 0) {
        NETSTACK_LOGE("listenFd is invalid or closed");
        return ConvertSocketClientErrno(ERRNO_BAD_FD);
    }
    struct sockaddr_un unAddr = {};
    socklen_t len = sizeof(unAddr);
    if (getsockname(listenFd_, reinterpret_cast<struct sockaddr *>(&unAddr), &len) != 0) {
        return ConvertSocketClientErrno(errno);
    }
    socketPath = unAddr.sun_path;
    return SOCKET_ERROR_OK;
}

int LocalSocketServer::GetSocketFd(int32_t &socketFd) const
{
    socketFd = listenFd_;
    return SOCKET_ERROR_OK;
}

void LocalSocketServer::RemoveConnection(int32_t clientId)
{
    std::unique_lock<std::shared_mutex> lock(connMapMutex_);
    connectionMap_.erase(clientId);
}

void LocalSocketServer::SetSocketDefaultBufferSize(int sockfd)
{
    uint32_t recvSize = DEFAULT_BUFFER_SIZE;
    if (alreadySetExtraOptions_ && extraOptions_.AlreadySetRecvBufSize()) {
        recvSize = extraOptions_.GetReceiveBufferSize();
    }
    (void)setsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<void *>(&recvSize), sizeof(recvSize));
    uint32_t sendSize = DEFAULT_BUFFER_SIZE;
    if (alreadySetExtraOptions_ && extraOptions_.AlreadySetSendBufSize()) {
        sendSize = extraOptions_.GetSendBufferSize();
    }
    (void)setsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<void *>(&sendSize), sizeof(sendSize));
}

std::shared_ptr<LocalSocketConnection> LocalSocketServer::AddConnection(int32_t clientId)
{
    auto conn = std::make_shared<LocalSocketConnection>(clientId);
    std::unique_lock<std::shared_mutex> lock(connMapMutex_);
    connectionMap_[clientId] = conn;
    return conn;
}

void LocalSocketServer::NotifyLoopFinished()
{
    std::lock_guard<std::mutex> lock(finishMutex_);
    isLoopFinished_ = true;
    finishCond_.notify_all();
}

void LocalSocketServer::WaitForEndingLoop()
{
    std::unique_lock<std::mutex> lock(finishMutex_);
    finishCond_.wait(lock, [this] { return isLoopFinished_; });
}

void LocalSocketServer::ReleaseListenAndEpoll()
{
    if (listenFd_ >= 0) {
        close(listenFd_);
        listenFd_ = -1;
    }
#if !defined(MAC_PLATFORM) && !defined(IOS_PLATFORM)
    if (epollFd_ >= 0) {
        close(epollFd_);
        epollFd_ = -1;
    }
#endif
}

void LocalSocketServer::RunAcceptThread()
{
    auto self = shared_from_this();
    std::thread acceptThread([self]() {
#if defined(MAC_PLATFORM) || defined(IOS_PLATFORM)
        pthread_setname_np(LOCAL_SOCKET_SERVER_ACCEPT_RECV_DATA);
#else
        pthread_setname_np(pthread_self(), LOCAL_SOCKET_SERVER_ACCEPT_RECV_DATA);
#endif
        {
            std::lock_guard<std::mutex> lock(self->finishMutex_);
            self->isLoopFinished_ = false;
        }
        self->DoAcceptLoop();
        self->NotifyLoopFinished();
    });

    acceptThread.detach();
}

#if defined(MAC_PLATFORM) || defined(IOS_PLATFORM)
void LocalSocketServer::DoAcceptLoop()
{
    struct sockaddr_un clientAddress;
    socklen_t clientAddrLength = sizeof(clientAddress);
    struct pollfd fds[1] = {{.fd = listenFd_, .events = POLLIN}};
    nfds_t num = 1;
    while (!state_.IsClose()) {
        int ret = poll(fds, num, DEFAULT_POLL_TIMEOUT);
        if (ret < 0) {
            NETSTACK_LOGE("poll to accept failed, socket is %{public}d, errno is %{public}d", listenFd_, errno);
            auto errorCode = ConvertSocketClientErrno(errno);
            CallOnErrorCallback(errorCode, GetSocketErrorMessage(errorCode));
            break;
        }
        if (state_.IsClose()) {
            NETSTACK_LOGI("server object destruct, exit the loop");
            break;
        }
        if (fds[0].revents & POLLIN) {
            int connectFd = accept(listenFd_, reinterpret_cast<sockaddr *>(&clientAddress), &clientAddrLength);
            if (connectFd < 0) {
                continue;
            }
            if (static_cast<int>(connectionMap_.size()) >= MAX_CLIENTS) {
                NETSTACK_LOGE("local socket server max number of clients reached, sockfd: %{public}d", listenFd_);
                close(connectFd);
                continue;
            }
            SetSocketDefaultBufferSize(connectFd);
            if (alreadySetExtraOptions_) {
                (void)ExecCommonUtils::SetLocalSocketOptions(connectFd, extraOptions_);
            }
            if (!state_.IsClose()) {
                std::thread handlerThread(&LocalSocketServer::RecvHandler, this, connectFd);
                pthread_setname_np(LOCAL_SOCKET_SERVER_HANDLE_CLIENT);
                handlerThread.detach();
            }
        }
    }
}

void LocalSocketServer::RecvHandler(int connectFd)
{
    int32_t clientId = GenerateClientId();
    auto conn = AddConnection(clientId);
    if (conn == nullptr) {
        NETSTACK_LOGE("add connect fd err, fd:%{public}d", connectFd);
        CallOnErrorCallback(SYSTEM_INTERNAL_ERROR, GetSocketErrorMessage(SYSTEM_INTERNAL_ERROR));
        close(connectFd);
        return;
    }
    conn->SetSocketFd(connectFd);
    CallOnConnectCallback(clientId);
    int sockRecvSize = ExecCommonUtils::ConfirmBufferSize(connectFd);
    auto buffer = std::make_unique<char[]>(sockRecvSize);
    if (buffer == nullptr) {
        NETSTACK_LOGE("failed to malloc, connectFd: %{public}d, malloc size: %{public}d", connectFd, sockRecvSize);
        conn->CallOnErrorCallback(SYSTEM_INTERNAL_ERROR, GetSocketErrorMessage(SYSTEM_INTERNAL_ERROR));
        RemoveConnection(clientId);
        return;
    }
    while (!state_.IsClose()) {
        if (memset_s(buffer.get(), sockRecvSize, 0, sockRecvSize) != EOK) {
            NETSTACK_LOGE("memset_s failed, connectFd: %{public}d, clientId: %{public}d", connectFd, clientId);
            continue;
        }
        int32_t recvSize = recv(connectFd, buffer.get(), sockRecvSize, 0);
        if (recvSize == 0) {
            NETSTACK_LOGI("session closed, err:%{public}d, fd:%{public}d, id:%{public}d", errno, connectFd, clientId);
            conn->CallOnCloseCallback();
            break;
        } else if (recvSize < 0) {
            if (errno != EINTR && errno != EAGAIN) {
                NETSTACK_LOGE("recv error, err:%{public}d, fd:%{public}d, id:%{public}d", errno, connectFd, clientId);
                auto errorCode = ConvertSocketClientErrno(errno);
                conn->CallOnErrorCallback(errorCode, GetSocketErrorMessage(errorCode));
                break;
            }
        } else {
            NETSTACK_LOGD("recv, fd:%{public}d, size:%{public}d", connectFd, recvSize);
            std::string socketPath;
            conn->GetLocalAddress(socketPath);
            std::string bufContent(buffer.get(), recvSize);
            conn->CallOnMessageCallback(bufContent, socketPath, static_cast<size_t>(recvSize));
        }
    }
    conn->Close();
    RemoveConnection(clientId);
}
#else
int LocalSocketServer::StartEpoll()
{
    epollFd_ = epoll_create1(0);
    if (epollFd_ < 0) {
        NETSTACK_LOGE("err, epoll_create1 fail, errno:%{public}d", errno);
        return -1;
    }
    return 0;
}

int LocalSocketServer::RegisterEpollEvent(int sockfd, uint32_t events)
{
    if (sockfd < 0) {
        NETSTACK_LOGE("err, fd < 0");
        return -1;
    }
    struct epoll_event ev;
    ev.events = events;
    ev.data.fd = sockfd;
    if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, sockfd, &ev) < 0) {
        NETSTACK_LOGE("err, epoll_ctl fail, errno:%{public}d", errno);
        return -1;
    }
    return 0;
}

void LocalSocketServer::AcceptHandler(int fd)
{
    pthread_setname_np(pthread_self(), LOCAL_SOCKET_SERVER_HANDLE_CLIENT);
    if (fd < 0) {
        NETSTACK_LOGE("accept a invalid fd");
        return;
    }
    if (static_cast<int>(connectionMap_.size()) >= MAX_CLIENTS) {
        NETSTACK_LOGE("local socket server max number of clients reached, sockfd: %{public}d", listenFd_);
        close(fd);
        return;
    }
    int32_t clientId = GenerateClientId();
    auto conn = AddConnection(clientId);
    if (conn == nullptr) {
        NETSTACK_LOGE("add connect fd err, fd:%{public}d", fd);
        CallOnErrorCallback(SYSTEM_INTERNAL_ERROR, GetSocketErrorMessage(SYSTEM_INTERNAL_ERROR));
        close(fd);
        return;
    }
    if (RegisterEpollEvent(fd, EPOLLIN) == -1) {
        NETSTACK_LOGE("new connection register err, fd:%{public}d, errno:%{public}d", fd, errno);
        auto errorCode = ConvertSocketClientErrno(errno);
        CallOnErrorCallback(errorCode, GetSocketErrorMessage(errorCode));
        conn->Close();
        RemoveConnection(clientId);
        return;
    }
    conn->SetSocketFd(fd);
    CallOnConnectCallback(clientId);
    SetSocketDefaultBufferSize(fd);
    if (alreadySetExtraOptions_) {
        (void)ExecCommonUtils::SetLocalSocketOptions(fd, extraOptions_);
    }
}

void LocalSocketServer::RecvHandler(int connectFd)
{
    auto conn = FindConnectionByFd(connectFd);
    if (conn == nullptr) {
        NETSTACK_LOGI("can not found connection for fd %{public}d", connectFd);
        epoll_ctl(epollFd_, EPOLL_CTL_DEL, connectFd, nullptr);
        close(connectFd);
        return;
    }

    int32_t clientId = conn->GetClientId();
    int sockRecvSize = ExecCommonUtils::ConfirmBufferSize(connectFd);
    auto buffer = std::make_unique<char[]>(sockRecvSize);
    if (buffer == nullptr) {
        NETSTACK_LOGE("failed to malloc, connectFd: %{public}d, malloc size: %{public}d", connectFd, sockRecvSize);
        conn->CallOnErrorCallback(SYSTEM_INTERNAL_ERROR, GetSocketErrorMessage(SYSTEM_INTERNAL_ERROR));
        CloseEpollConnection(connectFd, clientId, conn);
        return;
    }

    int32_t recvSize = recv(connectFd, buffer.get(), sockRecvSize, 0);
    if (recvSize == 0) {
        NETSTACK_LOGI("session closed, errno:%{public}d, fd:%{public}d, id:%{public}d", errno, connectFd, clientId);
        conn->CallOnCloseCallback();
        CloseEpollConnection(connectFd, clientId, conn);
    } else if (recvSize < 0) {
        if (errno == EINTR || errno == EAGAIN) {
            return;
        }
        NETSTACK_LOGE("recv error, errno:%{public}d, fd:%{public}d, id:%{public}d", errno, connectFd, clientId);
        auto errorCode = ConvertSocketClientErrno(errno);
        conn->CallOnErrorCallback(errorCode, GetSocketErrorMessage(errorCode));
        CloseEpollConnection(connectFd, clientId, conn);
    } else {
        NETSTACK_LOGI("recv, fd:%{public}d, size:%{public}d", connectFd, recvSize);
        std::string socketPath;
        conn->GetLocalAddress(socketPath);
        std::string bufContent(buffer.get(), recvSize);
        conn->CallOnMessageCallback(bufContent, socketPath, static_cast<size_t>(recvSize));
    }
}

std::shared_ptr<LocalSocketConnection> LocalSocketServer::FindConnectionByFd(int connectFd)
{
    std::shared_lock<std::shared_mutex> lock(connMapMutex_);
    auto it = std::find_if(connectionMap_.begin(), connectionMap_.end(),
        [connectFd](const auto &pair) {
            int32_t fd = -1;
            pair.second->GetSocketFd(fd);
            return fd == connectFd;
        });
    if (it == connectionMap_.end()) {
        return nullptr;
    }
    return it->second;
}

void LocalSocketServer::CloseEpollConnection(int connectFd, int32_t clientId,
    std::shared_ptr<LocalSocketConnection> conn)
{
    epoll_ctl(epollFd_, EPOLL_CTL_DEL, connectFd, nullptr);
    conn->Close();
    RemoveConnection(clientId);
}

void LocalSocketServer::DoAcceptLoop()
{
    pthread_setname_np(pthread_self(), LOCAL_SOCKET_SERVER_ACCEPT_RECV_DATA);
    struct sockaddr_un clientAddress;
    socklen_t clientAddrLength = sizeof(clientAddress);
    if (RegisterEpollEvent(listenFd_, EPOLLIN) == -1) {
        NETSTACK_LOGE("register listen fd err, fd:%{public}d, errno:%{public}d", listenFd_, errno);
        auto errorCode = ConvertSocketClientErrno(errno);
        CallOnErrorCallback(errorCode, GetSocketErrorMessage(errorCode));
        return;
    }
    while (!state_.IsClose()) {
        struct epoll_event events[MAX_EVENTS];
        int eventNum = epoll_wait(epollFd_, events, MAX_EVENTS, DEFAULT_POLL_TIMEOUT);
        if (eventNum == -1) {
            if (errno == EINTR) {
                continue;
            }
            NETSTACK_LOGE("epoll wait err, fd:%{public}d, errno:%{public}d", listenFd_, errno);
            auto errorCode = ConvertSocketClientErrno(errno);
            CallOnErrorCallback(errorCode, GetSocketErrorMessage(errorCode));
            break;
        }
        if (state_.IsClose()) {
            NETSTACK_LOGI("server object destruct, exit the loop");
            break;
        }
        for (int i = 0; i < eventNum; ++i) {
            if ((events[i].data.fd == listenFd_) && (events[i].events & EPOLLIN)) {
                int connectFd = accept(listenFd_, reinterpret_cast<sockaddr *>(&clientAddress), &clientAddrLength);
                AcceptHandler(connectFd);
            } else if ((events[i].data.fd != listenFd_) && (events[i].events & EPOLLIN)) {
                RecvHandler(events[i].data.fd);
            }
        }
    }
}
#endif

std::shared_ptr<LocalSocketConnection> LocalSocketServer::GetConnectionByClientID(const int32_t clientId)
{
    std::shared_ptr<LocalSocketConnection> ptrConnection = nullptr;
    std::shared_lock<std::shared_mutex> lock(connMapMutex_);
    auto it = connectionMap_.find(clientId);
    if (it != connectionMap_.end()) {
        ptrConnection = it->second;
    }
    return ptrConnection;
}

void LocalSocketServer::CallOnConnectCallback(int32_t clientId)
{
    LocalSocketServerOnConnectCallback callback = nullptr;
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (onConnectCallback_) {
            callback = onConnectCallback_;
        }
    }
    if (callback) {
        callback(clientId);
    }
}

void LocalSocketServer::CallOnErrorCallback(const int32_t err, const std::string &errMsg)
{
    LocalSocketServerOnErrorCallback callback = nullptr;
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (onErrorCallback_) {
            callback = onErrorCallback_;
        }
    }
    if (callback) {
        callback(err, errMsg);
    }
}

void LocalSocketServer::OnConnect(const LocalSocketServerOnConnectCallback &connectCallback)
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    onConnectCallback_ = connectCallback;
}

void LocalSocketServer::OnError(const LocalSocketServerOnErrorCallback &onErrorCallback)
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    onErrorCallback_ = onErrorCallback;
}

void LocalSocketServer::OffConnect()
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    onConnectCallback_ = nullptr;
}

void LocalSocketServer::OffError()
{
    std::lock_guard<std::mutex> lock(callbackMutex_);
    onErrorCallback_ = nullptr;
}

} // namespace Socket
} // namespace NetStack
} // namespace OHOS
