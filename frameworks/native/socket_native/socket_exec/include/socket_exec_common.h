/*
 * Copyright (c) 2024-2026 Huawei Device Co., Ltd.
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

#ifndef SOCKET_EXEC_COMMON_H
#define SOCKET_EXEC_COMMON_H

#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <poll.h>
#include <unistd.h>

#include "local_socket_options.h"
#include "netstack_log.h"
#include "net_address.h"

namespace OHOS::NetStack::Socket {
static constexpr int MAX_SOCKET_BUFFER_SIZE = 262144;
static constexpr const int UNIT_CONVERSION_1000 = 1000;
constexpr int32_t DEFAULT_BUFFER_SIZE = 8192;
constexpr int32_t DEFAULT_POLL_TIMEOUT = 500;
constexpr int MAX_CLIENTS = 1024;

class ExecCommonUtils {
public:
static bool MakeNonBlock(int sock);

static int MakeTcpSocket(sa_family_t family, bool needNonblock = true);

static int MakeUdpSocket(sa_family_t family);

static int MakeLocalSocket(int socketType, bool needNonblock = true);

static std::string ConvertAddressToIp(const std::string &address, sa_family_t family);

static bool IpMatchFamily(const std::string &address, sa_family_t family);

static bool NonBlockConnect(int sock, sockaddr *addr, socklen_t addrLen, uint32_t timeoutMSec);

static bool PollSendData(int sock, const char *data, size_t size, sockaddr *addr, socklen_t addrLen);

static int ConfirmSocketTimeoutMs(int sock, int type, int defaultValue);

static int ConfirmBufferSize(int sock);

static bool SetSocketBufferSize(int sockfd, int type, uint32_t size);

static bool SetLocalSocketOptions(int sockfd, const OHOS::NetStack::Socket::LocalExtraOptions &options);

static bool GetLocalSocketOptions(int sockfd, OHOS::NetStack::Socket::LocalExtraOptions &options);

static bool PollFd(pollfd *fds, nfds_t num, int timeout);

static bool IsTfoEnabled(int sock);

static bool ValidateAddress(const NetAddress &address);

static bool MakeSockAddr(const NetAddress &address, sockaddr_storage &ss, socklen_t &addrLen);

static bool FillLocalAddress(int sock, NetAddress &outAddr);

static bool FillRemoteAddress(int sock, NetAddress &outAddr);

static void GetSocketAddr(const NetAddress *address, sockaddr_in *addr4, sockaddr_in6 *addr6,
    sockaddr **addr, socklen_t *len);

static bool SetExtraOptionsBase(int sock, const ExtraOptionsBase &options);
};

} // namespace OHOS::NetStack::Socket

#endif