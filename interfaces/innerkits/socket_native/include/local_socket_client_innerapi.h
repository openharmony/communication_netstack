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

#ifndef COMMUNICATIONNETSTACK_LOCAL_SOCKET_H
#define COMMUNICATIONNETSTACK_LOCAL_SOCKET_H

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "local_socket_options.h"
#include "socket_state_base.h"

namespace OHOS {
namespace NetStack {
namespace Socket {
using LocalSocketOnMessageCallback = std::function<void(const std::string &data,
    const std::string &address, const size_t size)>;
using LocalSocketOnConnectCallback = std::function<void(void)>;
using LocalSocketOnCloseCallback = std::function<void(void)>;
using LocalSocketOnErrorCallback = std::function<void(const int32_t errorNumber, const std::string &errorString)>;

class LocalSocket : public std::enable_shared_from_this<LocalSocket> {
public:
    LocalSocket();
    ~LocalSocket();

    int32_t Bind(const std::string &socketPath);
    int32_t Connect(const std::string &socketPath, int32_t timeout = 0);
    int32_t Send(const LocalSocketOptions &options);
    int32_t Close();
    int32_t GetState(SocketStateBase &state);
    int32_t GetSocketFd(int32_t &sockFd) const;
    int32_t SetExtraOptions(const LocalExtraOptions &options);
    int32_t GetExtraOptions(LocalExtraOptions &options);
    int32_t GetLocalAddress(std::string &socketPath);

    void OnMessage(const LocalSocketOnMessageCallback &messageCallback);
    void OnConnect(const LocalSocketOnConnectCallback &connectCallback);
    void OnError(const LocalSocketOnErrorCallback &OnErrorCallback);
    void OnClose(const LocalSocketOnCloseCallback &closeCallback);
    void OffMessage();
    void OffConnect();
    void OffError();
    void OffClose();
private:
    void RunRecvThread();
    bool DoRecvIteration();
    void CallOnMessageCallback(const std::string &data, const std::string &address, const size_t size);
    void CallOnConnectCallback();
    void CallOnCloseCallback();
    void CallOnErrorCallback(int32_t err, const std::string &errString);
private:
    int socketFd_ = -1;
    std::string address_;
    SocketStateBase state_;
    std::mutex mutex_;
    LocalSocketOnMessageCallback onMessageCallback_ = nullptr;
    LocalSocketOnConnectCallback onConnectCallback_ = nullptr;
    LocalSocketOnErrorCallback onErrorCallback_ = nullptr;
    LocalSocketOnCloseCallback onCloseCallback_ = nullptr;
};

} // namespace Socket
} // namespace NetStack
} // namespace OHOS

#endif /* COMMUNICATIONNETSTACK_LOCAL_SOCKET_H */
