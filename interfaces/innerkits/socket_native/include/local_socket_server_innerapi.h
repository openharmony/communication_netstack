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

#ifndef COMMUNICATIONNETSTACK_LOCAL_SOCKET_SERVER_H
#define COMMUNICATIONNETSTACK_LOCAL_SOCKET_SERVER_H

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include "local_socket_options.h"
#include "socket_state_base.h"

namespace OHOS {
namespace NetStack {
namespace Socket {

using LocalSocketServerOnConnectCallback =  std::function<void(const int &clientId)>;
using LocalSocketServerOnErrorCallback = std::function<void(const int32_t errorNumber,
    const std::string &errorString)>;

using LocalSocketConnectionOnMessageCallback = std::function<void(const std::string &data,
    const std::string &address, const size_t size)>;
using LocalSocketConnectionOnCloseCallback = std::function<void(void)>;
using LocalSocketConnectionOnErrorCallback = std::function<void(const int32_t errorNumber,
    const std::string &errorString)>;

class LocalSocketConnection : public std::enable_shared_from_this<LocalSocketConnection> {
public:
    explicit LocalSocketConnection(int32_t clientId);
    ~LocalSocketConnection();

    int Send(const LocalSocketOptions &options);
    int Close();
    int GetLocalAddress(std::string &socketPath);
    int GetSocketFd(int32_t &socketFd) const;

    void OnMessage(const LocalSocketConnectionOnMessageCallback &messageCallback);
    void OnClose(const LocalSocketConnectionOnCloseCallback &closeCallback);
    void OnError(const LocalSocketConnectionOnErrorCallback &OnErrorCallback);
    void OffMessage();
    void OffClose();
    void OffError();

    void SetSocketFd(const int32_t sockFd) { socketFd_ = sockFd; }
    int32_t GetClientId() const { return clientId_; }

private:
    void CallOnMessageCallback(const std::string &data, const std::string &address, const size_t size);
    void CallOnCloseCallback();
    void CallOnErrorCallback(const int32_t err, const std::string &errMsg);

    std::mutex mutex_;
    LocalSocketConnectionOnMessageCallback onMessageCallback_ = nullptr;
    LocalSocketConnectionOnCloseCallback onCloseCallback_ = nullptr;
    LocalSocketConnectionOnErrorCallback onErrorCallback_ = nullptr;

    int32_t clientId_ = -1;
    int socketFd_ = -1;

    friend class LocalSocketServer;
};

class LocalSocketServer : public std::enable_shared_from_this<LocalSocketServer> {
public:
    LocalSocketServer();
    ~LocalSocketServer();

    int Listen(const std::string &socketPath);
    int Close();
    int GetState(SocketStateBase &state);
    int SetExtraOptions(const LocalExtraOptions &options);
    int GetExtraOptions(LocalExtraOptions &options);
    int GetLocalAddress(std::string &socketPath);
    int GetSocketFd(int32_t &socketFd) const;

    void OnConnect(const LocalSocketServerOnConnectCallback &connectCallback);
    void OnError(const LocalSocketServerOnErrorCallback &OnErrorCallback);
    void OffConnect();
    void OffError();

    std::shared_ptr<LocalSocketConnection> GetConnectionByClientID(const int32_t clientId);

private:
    void CallOnConnectCallback(const int32_t clientId);
    void CallOnErrorCallback(const int32_t err, const std::string &errMsg);

    void ReleaseListenAndEpoll();
    void SetSocketDefaultBufferSize(int sockfd);
    std::shared_ptr<LocalSocketConnection> AddConnection(int32_t clientId);
    void RemoveConnection(int32_t clientId);
    int GenerateClientId() { return ++clientCounter_; }

    void NotifyLoopFinished();
    void WaitForEndingLoop();
    void RunAcceptThread();
    void DoAcceptLoop();
    void RecvHandler(int connectFd);
#if !defined(MAC_PLATFORM) && !defined(IOS_PLATFORM)
    void AcceptHandler(int fd);
    std::shared_ptr<LocalSocketConnection> FindConnectionByFd(int connectFd);
    void CloseEpollConnection(int connectFd, int32_t clientId, std::shared_ptr<LocalSocketConnection> conn);
    int StartEpoll();
    int RegisterEpollEvent(int sockfd, uint32_t events);
#endif

private:
    LocalSocketServerOnConnectCallback onConnectCallback_;
    LocalSocketServerOnErrorCallback onErrorCallback_;

    mutable std::shared_mutex connMapMutex_;
    std::mutex callbackMutex_;
    std::unordered_map<int32_t, std::shared_ptr<LocalSocketConnection>> connectionMap_;
    int listenFd_ = -1;
    int epollFd_ = -1;
    bool isLoopFinished_ = true;
    std::mutex finishMutex_;
    std::condition_variable finishCond_;
    std::string socketPath_;
    LocalExtraOptions extraOptions_;
    bool alreadySetExtraOptions_ = false;
    SocketStateBase state_;
    std::atomic_int32_t clientCounter_{0};
};

} // namespace Socket
} // namespace NetStack
} // namespace OHOS

#endif /* COMMUNICATIONNETSTACK_LOCAL_SOCKET_SERVER_H */
