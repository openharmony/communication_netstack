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

#ifndef TCP_SERVER_SOCKET_CONTEXT_H
#define TCP_SERVER_SOCKET_CONTEXT_H

#include <shared_mutex>
#include <set>
#include "net_address.h"
#include "tcp_extra_options.h"
#include "tcp_send_options.h"
#include "safe_map.h"

namespace OHOS::NetStack::Socket {
struct TcpConnectionInfo {
    int32_t clientId = -1;
    int socketFd = -1;
    NetAddress remoteAddress;
    NetAddress localAddress;
};

class SocketConfig {
public:
    SocketConfig() {}
    ~SocketConfig() {}
    void SetTcpExtraOptions(int listenFd, const TCPExtraOptions& option)
    {
        tcpExtraOptions_.EnsureInsert(listenFd, option);
    }

    bool GetTcpExtraOptions(int listenFd, TCPExtraOptions& option)
    {
        return tcpExtraOptions_.Find(listenFd, option);
    }

    void AddNewListenSocket(int listenFd)
    {
        tcpClients_.Insert(listenFd, {});
    }

    void AddNewAcceptSocket(int listenFd, int acceptFd)
    {
        std::set<int> fdSet;
        auto fn = [&](std::set<int> &value) -> void {
            value.emplace(acceptFd);
        };
        if (tcpClients_.Find(listenFd, fdSet)) {
            tcpClients_.ChangeValueByLambda(listenFd, fn);
        }
    }

    void RemoveAcceptSocket(int acceptFd)
    {
        tcpClients_.Iterate([acceptFd](int listenFd, std::set<int> &fdSet) {
            if (auto ite = fdSet.find(acceptFd); ite != fdSet.end()) {
                fdSet.erase(ite);
            }
        });
    }

    std::set<int> GetClients(int listenFd)
    {
        std::set<int> fdSet;
        tcpClients_.Find(listenFd, fdSet);
        return fdSet;
    }

    void RemoveServerSocket(int listenFd)
    {
        tcpExtraOptions_.Erase(listenFd);
        tcpClients_.Erase(listenFd);
    }

    void ShutdownAllSockets()
    {
        tcpClients_.Iterate([](const int key, std::set<int>&) { shutdown(key, SHUT_RDWR); });
        tcpExtraOptions_.Clear();
        tcpClients_.Clear();
    }
private:
    SocketConfig(const SocketConfig& singleton) = delete;
    SocketConfig& operator=(const SocketConfig& singleton) = delete;

    SafeMap<int, TCPExtraOptions> tcpExtraOptions_;
    SafeMap<int, std::set<int>> tcpClients_;
};

} // namespace OHOS::NetStack::Socket

#endif /* TCP_SERVER_SOCKET_CONTEXT_H */
