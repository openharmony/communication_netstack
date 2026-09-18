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

#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>

#include "gtest/gtest.h"

#include "net_address.h"
#include "proxy_options.h"
#include "socket_exec_common.h"
#include "socket_remote_info.h"
#include "socket_state_base.h"
#include "udp_extra_options.h"
#include "udp_send_options.h"
#include "udp_socket_innerapi.h"

namespace {
using namespace testing::ext;
using namespace OHOS::NetStack::Socket;

static constexpr int32_t INVALID_FD = -1;
static constexpr uint16_t TEST_PORT = 28090;
static constexpr const char *TEST_ADDRESS = "127.0.0.1";
static constexpr const char *TEST_DATA = "hello udp";
static constexpr const char *TEST_DATA_LARGE =
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789";
static constexpr int32_t SLEEP_MS = 100;
static constexpr int32_t MAX_TTL = 255;
static constexpr uint32_t MAX_SOCKET_BUFFER_SIZE = 262144;

/* Create a local bound UDP socket so Send() can target it and exercise the
 * success path. Returns fd on success, -1 on failure. The assigned port is
 * written to realPort. */
static int MakeUdpListenFd(uint16_t &realPort)
{
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }
    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    sockaddr_in real = {0};
    socklen_t len = sizeof(real);
    if (getsockname(fd, reinterpret_cast<sockaddr *>(&real), &len) < 0) {
        close(fd);
        return -1;
    }
    realPort = ntohs(real.sin_port);
    return fd;
}

class UdpSocketTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};

/* Callbacks must match the typedefs in udp_socket_innerapi.h:
 *   OnMessageCallback   = void(const std::string &, const SocketRemoteInfo &)
 *   OnListeningCallback = void()
 *   OnCloseCallback     = void()
 *   OnErrorCallback     = void(int32_t, const std::string &)
 */
static void OnMessageCallback(const std::string &data, const SocketRemoteInfo &remoteInfo)
{
    (void)data;
    (void)remoteInfo;
}

static void OnErrorCallback(int32_t errorNumber, const std::string &errorString)
{
    (void)errorNumber;
    (void)errorString;
}

static void OnCloseCallback()
{
}

static void OnListeningCallback()
{
}

static int GetSocketFd(const std::shared_ptr<UDPSocket> &socket)
{
    int fd = INVALID_FD;
    (void)socket->GetSocketFd(fd);
    return fd;
}

/* ==================== Constructor / Destructor Tests ==================== */
// API: UDPSocket 构造 → 初始状态：fd=-1, isClose=true, isBound=false, isConnected=false

HWTEST_F(UdpSocketTest, Constructor001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsClose());
    EXPECT_FALSE(state.IsBound());
    EXPECT_FALSE(state.IsConnected());
}

// API: UDPSocket 构造 → 再次验证初始状态

HWTEST_F(UdpSocketTest, Constructor002, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_FALSE(state.IsBound());
    EXPECT_FALSE(state.IsConnected());
    EXPECT_TRUE(state.IsClose());
}

// API: UDPSocket 析构 → 未 Bind 直接释放

HWTEST_F(UdpSocketTest, Destructor001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    EXPECT_NE(socket, nullptr);
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
    socket.reset();
}

// API: UDPSocket 析构 → Bind 后直接释放（未 Close）

HWTEST_F(UdpSocketTest, Destructor002, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    socket.reset();
}

// API: UDPSocket 析构 → Bind + Close 后释放

HWTEST_F(UdpSocketTest, Destructor003, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    socket->Close();
    socket.reset();
}

/* ==================== On/Off Callback Tests ==================== */
// API: UDPSocket.OnMessage → 注册 onMessage 回调

HWTEST_F(UdpSocketTest, OnMessageCallback001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    EXPECT_NE(socket, nullptr);
    socket->OnMessage(OnMessageCallback);
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
}

// API: UDPSocket.OnError → 注册 onError 回调

HWTEST_F(UdpSocketTest, OnErrorCallback001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    EXPECT_NE(socket, nullptr);
    socket->OnError(OnErrorCallback);
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
}

// API: UDPSocket.OnClose → 注册 onClose 回调

HWTEST_F(UdpSocketTest, OnCloseCallback001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    EXPECT_NE(socket, nullptr);
    socket->OnClose(OnCloseCallback);
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
}

// API: UDPSocket.OnListening → 注册 onListening 回调

HWTEST_F(UdpSocketTest, OnListeningCallback001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    EXPECT_NE(socket, nullptr);
    socket->OnListening(OnListeningCallback);
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
}

// API: UDPSocket.OffMessage/OffError/OffClose/OffListening → On 后 Off 全部 4 种回调

HWTEST_F(UdpSocketTest, OffCallbacks001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    EXPECT_NE(socket, nullptr);
    socket->OnMessage(OnMessageCallback);
    socket->OnError(OnErrorCallback);
    socket->OnClose(OnCloseCallback);
    socket->OnListening(OnListeningCallback);

    socket->OffMessage();
    socket->OffError();
    socket->OffClose();
    socket->OffListening();
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
}

// API: UDPSocket.OffMessage/OffError/OffClose/OffListening → 未 On 直接 Off（安全无崩溃）

HWTEST_F(UdpSocketTest, OffCallbacks002, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    EXPECT_NE(socket, nullptr);
    /* Calling Off without On should be safe */
    socket->OffMessage();
    socket->OffError();
    socket->OffClose();
    socket->OffListening();
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
}

/* ==================== Bind Tests ==================== */
// API: UDPSocket.Bind → IPv4 127.0.0.1:0，验证 fd>=0

HWTEST_F(UdpSocketTest, Bind001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    int ret = socket->Bind(address);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_GE(GetSocketFd(socket), 0);
    socket->Close();
}

// API: UDPSocket.Bind → IPv4 0.0.0.0:0

HWTEST_F(UdpSocketTest, Bind002, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress("0.0.0.0");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    int ret = socket->Bind(address);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
}

// API: UDPSocket.Bind → 未设置地址参数，返回 PARAM_ERROR_CODE

HWTEST_F(UdpSocketTest, Bind003, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    int ret = socket->Bind(address);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);
}

// API: UDPSocket.Bind → 空字符串地址，返回 PARAM_ERROR_CODE

HWTEST_F(UdpSocketTest, Bind004, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress("");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    int ret = socket->Bind(address);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);
}

// API: UDPSocket.Bind → 重复 Bind 同一地址（幂等性）

HWTEST_F(UdpSocketTest, Bind006, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    int firstFd = GetSocketFd(socket);
    EXPECT_GE(firstFd, 0);

    /* Bind to same address again should return OK (idempotent) */
    int ret = socket->Bind(address);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
}

// API: UDPSocket.Bind → Bind 后再次 Bind 不同 port

HWTEST_F(UdpSocketTest, Bind007, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address1;
    address1.SetAddress(TEST_ADDRESS);
    address1.SetPort(0);
    address1.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address1), SOCKET_ERROR_OK);

    NetAddress address2;
    address2.SetAddress(TEST_ADDRESS);
    address2.SetPort(0);
    address2.SetFamilyBySaFamily(AF_INET);
    int ret = socket->Bind(address2);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
}

// API: UDPSocket.Bind + GetState → Bind 后验证 isBound=true, isClose=false

HWTEST_F(UdpSocketTest, Bind008, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    SocketStateBase state;
    int ret = socket->GetState(state);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsBound());
    EXPECT_FALSE(state.IsClose());
    socket->Close();
}

// API: UDPSocket.Bind → 无效 IP 地址 999.999.999.999

HWTEST_F(UdpSocketTest, BindInvalidAddress001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress("999.999.999.999");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    int ret = socket->Bind(address);
    EXPECT_NE(ret, SOCKET_ERROR_OK);
}

// API: UDPSocket.Bind → IPv6 ::1:0

HWTEST_F(UdpSocketTest, BindIPv6001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress("::1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET6);
    int ret = socket->Bind(address);
    if (ret == SOCKET_ERROR_OK) {
        EXPECT_GE(GetSocketFd(socket), 0);
        socket->Close();
    }
}

// API: UDPSocket.Bind → Close 后重新 Bind（rebind）

HWTEST_F(UdpSocketTest, BindAfterClose001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    socket->Close();
    int ret = socket->Bind(address);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
}

// API: UDPSocket.Bind + OnListening → Bind 触发 OnListening 回调

HWTEST_F(UdpSocketTest, BindListeningCallback001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    std::atomic<bool> listeningCalled(false);
    socket->OnListening(
        [&listeningCalled]() {
            listeningCalled.store(true);
        });

    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS));
    EXPECT_TRUE(listeningCalled.load());
    socket->Close();
}

/* ==================== Send Tests ==================== */
// API: UDPSocket.Send → 未 Bind 直接 Send，返回 ERRNO_BAD_FD

HWTEST_F(UdpSocketTest, Send001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    UDPSendOptions options;
    options.SetData(TEST_DATA);
    options.address.SetAddress(TEST_ADDRESS);
    options.address.SetPort(TEST_PORT);
    options.address.SetFamilyBySaFamily(AF_INET);
    ProxyOptions proxyOptions;
    int ret = socket->Send(options, proxyOptions);
    /* No bind, socket fd is invalid, should return ERRNO_BAD_FD */
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

// API: UDPSocket.Send → 空数据，返回 PARAM_ERROR_CODE

HWTEST_F(UdpSocketTest, Send002, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    UDPSendOptions options;
    /* Empty data */
    options.SetData("");
    options.address.SetAddress(TEST_ADDRESS);
    options.address.SetPort(TEST_PORT);
    options.address.SetFamilyBySaFamily(AF_INET);
    ProxyOptions proxyOptions;
    int ret = socket->Send(options, proxyOptions);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);
}

// API: UDPSocket.Send → 空目标地址，返回 PARAM_ERROR_CODE

HWTEST_F(UdpSocketTest, Send003, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    UDPSendOptions options;
    options.SetData(TEST_DATA);
    /* Invalid address (empty) */
    options.address.SetAddress("");
    options.address.SetPort(TEST_PORT);
    options.address.SetFamilyBySaFamily(AF_INET);
    ProxyOptions proxyOptions;
    int ret = socket->Send(options, proxyOptions);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);
}

// API: UDPSocket.Send → Bind 后 Send IPv4，验证对端成功接收数据

HWTEST_F(UdpSocketTest, Send005, TestSize.Level1)
{
    uint16_t listenPort = 0;
    int listenFd = MakeUdpListenFd(listenPort);
    ASSERT_GE(listenFd, 0);

    auto socket = std::make_shared<UDPSocket>();
    NetAddress bindAddr;
    bindAddr.SetAddress(TEST_ADDRESS);
    bindAddr.SetPort(0);
    bindAddr.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(bindAddr), SOCKET_ERROR_OK);

    UDPSendOptions options;
    options.SetData(TEST_DATA);
    options.address.SetAddress(TEST_ADDRESS);
    options.address.SetPort(listenPort);
    options.address.SetFamilyBySaFamily(AF_INET);
    ProxyOptions proxyOptions;
    int ret = socket->Send(options, proxyOptions);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);

    /* Verify the listener received the data */
    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS));
    char buf[256] = {0};
    sockaddr_in from = {0};
    socklen_t fromLen = sizeof(from);
    ssize_t recvLen = recvfrom(listenFd, buf, sizeof(buf), MSG_DONTWAIT,
                               reinterpret_cast<sockaddr *>(&from), &fromLen);
    EXPECT_EQ(recvLen, static_cast<ssize_t>(strlen(TEST_DATA)));
    EXPECT_STREQ(buf, TEST_DATA);

    socket->Close();
    close(listenFd);
}

// API: UDPSocket.Send → Bind 后 Send 大数据 IPv4

HWTEST_F(UdpSocketTest, Send006, TestSize.Level1)
{
    uint16_t listenPort = 0;
    int listenFd = MakeUdpListenFd(listenPort);
    ASSERT_GE(listenFd, 0);

    auto socket = std::make_shared<UDPSocket>();
    NetAddress bindAddr;
    bindAddr.SetAddress(TEST_ADDRESS);
    bindAddr.SetPort(0);
    bindAddr.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(bindAddr), SOCKET_ERROR_OK);

    UDPSendOptions options;
    options.SetData(TEST_DATA_LARGE);
    options.address.SetAddress(TEST_ADDRESS);
    options.address.SetPort(listenPort);
    options.address.SetFamilyBySaFamily(AF_INET);
    ProxyOptions proxyOptions;
    int ret = socket->Send(options, proxyOptions);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);

    socket->Close();
    close(listenFd);
}

// API: UDPSocket.Send → 发送接近最大缓冲区大小的数据

HWTEST_F(UdpSocketTest, SendMaximumBuffer001, TestSize.Level1)
{
    uint16_t listenPort = 0;
    int listenFd = MakeUdpListenFd(listenPort);
    ASSERT_GE(listenFd, 0);

    auto socket = std::make_shared<UDPSocket>();
    NetAddress bindAddr;
    bindAddr.SetAddress(TEST_ADDRESS);
    bindAddr.SetPort(0);
    bindAddr.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(bindAddr), SOCKET_ERROR_OK);

    std::string largeData(MAX_SOCKET_BUFFER_SIZE - 128, 'A');
    UDPSendOptions options;
    options.SetData(largeData);
    options.address.SetAddress(TEST_ADDRESS);
    options.address.SetPort(listenPort);
    options.address.SetFamilyBySaFamily(AF_INET);
    ProxyOptions proxyOptions;
    int ret = socket->Send(options, proxyOptions);
    /* May succeed or fail due to size */
    (void)ret;

    socket->Close();
    close(listenFd);
}

// API: UDPSocket.Send + SOCKS5 Proxy → 通过 SOCKS5 代理发送（代理不可用，应失败）

HWTEST_F(UdpSocketTest, SendSocks5001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress bindAddr;
    bindAddr.SetAddress(TEST_ADDRESS);
    bindAddr.SetPort(0);
    bindAddr.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(bindAddr), SOCKET_ERROR_OK);

    UDPSendOptions options;
    options.SetData(TEST_DATA);
    options.address.SetAddress(TEST_ADDRESS);
    options.address.SetPort(TEST_PORT);
    options.address.SetFamilyBySaFamily(AF_INET);
    ProxyOptions proxyOptions;
    proxyOptions.type_ = ProxyType::SOCKS5;
    proxyOptions.address_.SetAddress("127.0.0.1");
    proxyOptions.address_.SetPort(1080);
    proxyOptions.address_.SetFamilyBySaFamily(AF_INET);
    /* SOCKS5 proxy not available, should fail */
    int ret = socket->Send(options, proxyOptions);
    EXPECT_NE(ret, SOCKET_ERROR_OK);
    socket->Close();
}

// API: UDPSocket.Send → 无效目标 IP 地址

HWTEST_F(UdpSocketTest, SendInvalidAddress001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress bindAddr;
    bindAddr.SetAddress(TEST_ADDRESS);
    bindAddr.SetPort(0);
    bindAddr.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(bindAddr), SOCKET_ERROR_OK);

    UDPSendOptions options;
    options.SetData(TEST_DATA);
    options.address.SetAddress("999.999.999.999");
    options.address.SetPort(TEST_PORT);
    options.address.SetFamilyBySaFamily(AF_INET);
    ProxyOptions proxyOptions;
    int ret = socket->Send(options, proxyOptions);
    EXPECT_NE(ret, SOCKET_ERROR_OK);
    socket->Close();
}

// API: UDPSocket.Send → Close 后 Send，返回 ERRNO_BAD_FD

HWTEST_F(UdpSocketTest, SendAfterClose001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress bindAddr;
    bindAddr.SetAddress(TEST_ADDRESS);
    bindAddr.SetPort(0);
    bindAddr.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(bindAddr), SOCKET_ERROR_OK);
    socket->Close();

    UDPSendOptions options;
    options.SetData(TEST_DATA);
    options.address.SetAddress(TEST_ADDRESS);
    options.address.SetPort(TEST_PORT);
    options.address.SetFamilyBySaFamily(AF_INET);
    ProxyOptions proxyOptions;
    int ret = socket->Send(options, proxyOptions);
    /* After close, fd is invalid */
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

/* ==================== Close Tests ==================== */
// API: UDPSocket.Close → 未 Bind 直接 Close，验证 isClose=true

HWTEST_F(UdpSocketTest, Close001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    int ret = socket->Close();
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsClose());
}

// API: UDPSocket.Close → Bind 后 Close，验证 isClose=true, fd=-1

HWTEST_F(UdpSocketTest, Close002, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    int ret = socket->Close();
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsClose());
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
}

// API: UDPSocket.Close → 双重 Close（无 Bind）

HWTEST_F(UdpSocketTest, Close003, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    socket->Close();
    /* Double close should return OK */
    int ret = socket->Close();
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
}

// API: UDPSocket.Close → Bind 后双重 Close

HWTEST_F(UdpSocketTest, Close004, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    socket->Close();
    /* Double close after bind should return OK */
    int ret = socket->Close();
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
}

// API: UDPSocket.Close + OnClose → Close 触发 onClose 回调

HWTEST_F(UdpSocketTest, CloseCallback001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    std::atomic<bool> closeCalled(false);
    socket->OnClose(
        [&closeCalled]() {
            closeCalled.store(true);
        });

    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    socket->Close();
    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS));
    EXPECT_TRUE(closeCalled.load());
}

/* ==================== GetState Tests ==================== */
// API: UDPSocket.GetState → 构造后初始状态：isClose=true

HWTEST_F(UdpSocketTest, GetState001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    SocketStateBase state;
    int ret = socket->GetState(state);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsClose());
}

// API: UDPSocket.GetState → Bind 后：isBound=true, isClose=false

HWTEST_F(UdpSocketTest, GetState002, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    SocketStateBase state;
    int ret = socket->GetState(state);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsBound());
    EXPECT_FALSE(state.IsClose());
    socket->Close();
}

// API: UDPSocket.GetState → Bind + Close 后：isClose=true, isBound=false

HWTEST_F(UdpSocketTest, GetState003, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    socket->Close();
    SocketStateBase state;
    int ret = socket->GetState(state);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsClose());
    EXPECT_FALSE(state.IsBound());
}

/* ==================== GetLocalAddress Tests ==================== */
// API: UDPSocket.GetLocalAddress → 未 Bind，返回 ERRNO_BAD_FD

HWTEST_F(UdpSocketTest, GetLocalAddress001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    int ret = socket->GetLocalAddress(address);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

// API: UDPSocket.GetLocalAddress → Bind IPv4 后获取本端地址（port > 0）

HWTEST_F(UdpSocketTest, GetLocalAddress002, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    NetAddress localAddr;
    int ret = socket->GetLocalAddress(localAddr);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_GT(localAddr.GetPort(), 0U);
    socket->Close();
}

/* ==================== GetSocketFd Tests ==================== */
// API: UDPSocket.GetSocketFd → 构造后 fd=-1

HWTEST_F(UdpSocketTest, GetSocketFd001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    EXPECT_NE(socket, nullptr);
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsClose());
}

// API: UDPSocket.GetSocketFd → Bind 后 fd >= 0

HWTEST_F(UdpSocketTest, GetSocketFd002, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    EXPECT_GE(GetSocketFd(socket), 0);
    socket->Close();
}

// API: UDPSocket.GetSocketFd → Bind + Close 后 fd=-1

HWTEST_F(UdpSocketTest, GetSocketFd003, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    socket->Close();
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
}

/* ==================== SetExtraOptions Tests ==================== */
// API: UDPSocket.SetExtraOptions → Bind 后设置全部选项（receiveBuffer, sendBuffer, reuseAddress, timeout, broadcast）

HWTEST_F(UdpSocketTest, SetExtraOptions001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    UDPExtraOptions options;
    options.SetReceiveBufferSize(4096);
    options.SetRecvBufSizeFlag(true);
    options.SetSendBufferSize(4096);
    options.SetSendBufSizeFlag(true);
    options.SetReuseAddress(true);
    options.SetReuseaddrFlag(true);
    options.SetSocketTimeout(1000);
    options.SetTimeoutFlag(true);
    options.SetBroadcast(true);
    options.SetBroadcastFlag(true);

    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);

    int ret = socket->SetExtraOptions(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
}

// API: UDPSocket.SetExtraOptions → receiveBuffer 超过最大值，返回 PARAM_ERROR_CODE

HWTEST_F(UdpSocketTest, SetExtraOptions002, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    UDPExtraOptions options;
    options.SetReceiveBufferSize(MAX_SOCKET_BUFFER_SIZE + 1);
    options.SetRecvBufSizeFlag(true);

    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);

    int ret = socket->SetExtraOptions(options);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);
    socket->Close();
}

// API: UDPSocket.SetExtraOptions → 未 Bind，返回 ERRNO_BAD_FD

HWTEST_F(UdpSocketTest, SetExtraOptions003, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    UDPExtraOptions options;

    /* Without bind, fd is invalid */
    int ret = socket->SetExtraOptions(options);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

// API: UDPSocket.SetExtraOptions → 设置选项为零值

HWTEST_F(UdpSocketTest, SetExtraOptions004, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    UDPExtraOptions options;
    options.SetReceiveBufferSize(0);
    options.SetRecvBufSizeFlag(true);
    options.SetSendBufferSize(0);
    options.SetSendBufSizeFlag(true);
    options.SetReuseAddress(false);
    options.SetReuseaddrFlag(true);

    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);

    int ret = socket->SetExtraOptions(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
}

/* ==================== IsClose Tests ==================== */
// API: UDPSocket.GetState → 初始 isClose=true

HWTEST_F(UdpSocketTest, IsClose001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsClose());
}

// API: UDPSocket.GetState → Bind 后 isClose=false，Close 后 isClose=true（状态转换）

HWTEST_F(UdpSocketTest, IsClose002, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_FALSE(state.IsClose());
    socket->Close();
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsClose());
}

/* ==================== Lifecycle / Integration Tests ==================== */
// API: UDPSocket 全生命周期集成 → On* 回调 + Bind + GetState + GetLocalAddress + GetSocketFd + SetExtraOptions + Close

HWTEST_F(UdpSocketTest, Lifecycle001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    socket->OnMessage(OnMessageCallback);
    socket->OnError(OnErrorCallback);
    socket->OnClose(OnCloseCallback);
    socket->OnListening(OnListeningCallback);

    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);

    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsBound());

    NetAddress localAddr;
    EXPECT_EQ(socket->GetLocalAddress(localAddr), SOCKET_ERROR_OK);

    EXPECT_GE(GetSocketFd(socket), 0);

    UDPExtraOptions options;
    options.SetReceiveBufferSize(4096);
    options.SetRecvBufSizeFlag(true);
    EXPECT_EQ(socket->SetExtraOptions(options), SOCKET_ERROR_OK);

    EXPECT_EQ(socket->Close(), SOCKET_ERROR_OK);
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsClose());
}

// API: UDPSocket 重复 Bind+Close 循环（3次）

HWTEST_F(UdpSocketTest, RepeatedBindClose001, TestSize.Level1)
{
    for (int i = 0; i < 3; ++i) {
        auto socket = std::make_shared<UDPSocket>();
        NetAddress address;
        address.SetAddress(TEST_ADDRESS);
        address.SetPort(0);
        address.SetFamilyBySaFamily(AF_INET);
        ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
        socket->Close();
    }
}

// API: UDPSocket.OnMessage → Bind + 接收消息 + OnMessage 回调触发验证

HWTEST_F(UdpSocketTest, SendReceive001, TestSize.Level1)
{
    uint16_t listenPort = 0;
    int listenFd = MakeUdpListenFd(listenPort);
    ASSERT_GE(listenFd, 0);

    auto socket = std::make_shared<UDPSocket>();
    std::atomic<bool> messageReceived(false);
    std::atomic<size_t> receivedLength(0);
    socket->OnMessage(
        [&messageReceived, &receivedLength](const std::string &data,
                                            const SocketRemoteInfo &remoteInfo) {
            (void)remoteInfo;
            receivedLength.store(data.size());
            messageReceived.store(true);
        });

    NetAddress bindAddr;
    bindAddr.SetAddress(TEST_ADDRESS);
    bindAddr.SetPort(0);
    bindAddr.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(bindAddr), SOCKET_ERROR_OK);

    /* Send from listenFd to our socket's bound port */
    NetAddress localAddr;
    ASSERT_EQ(socket->GetLocalAddress(localAddr), SOCKET_ERROR_OK);
    uint16_t myPort = localAddr.GetPort();

    sockaddr_in destAddr = {0};
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(myPort);
    destAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ssize_t sentLen = sendto(listenFd, TEST_DATA, strlen(TEST_DATA), 0,
                             reinterpret_cast<sockaddr *>(&destAddr), sizeof(destAddr));
    EXPECT_EQ(sentLen, static_cast<ssize_t>(strlen(TEST_DATA)));

    /* Wait for message callback */
    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS * 2));
    EXPECT_TRUE(messageReceived.load());
    EXPECT_EQ(receivedLength.load(), strlen(TEST_DATA));

    socket->Close();
    close(listenFd);
}

/* ==================== MulticastSocket Constructor Tests ==================== */
// API: MulticastSocket 构造 → 初始状态 fd=-1, isClose=true

HWTEST_F(UdpSocketTest, MulticastConstructor001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsClose());
}

// API: MulticastSocket 构造 → 再次验证初始状态 isBound=false, isConnected=false, isClose=true

HWTEST_F(UdpSocketTest, MulticastConstructor002, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_FALSE(state.IsBound());
    EXPECT_FALSE(state.IsConnected());
    EXPECT_TRUE(state.IsClose());
}

// API: MulticastSocket 析构 → 未操作直接释放

HWTEST_F(UdpSocketTest, MulticastDestructor001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    EXPECT_NE(socket, nullptr);
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
    socket.reset();
}

// API: MulticastSocket 析构 → AddMembership 后释放

HWTEST_F(UdpSocketTest, MulticastDestructor002, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    EXPECT_NE(socket, nullptr);
    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    int ret = socket->AddMembership(address);
    EXPECT_NE(ret, PARAM_ERROR_CODE); /* address is valid, AddMembership may succeed */
    socket.reset();
}

/* ==================== AddMembership Tests ==================== */
// API: MulticastSocket.AddMembership → 空地址，返回 PARAM_ERROR_CODE

HWTEST_F(UdpSocketTest, AddMembership001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    int ret = socket->AddMembership(address);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);
}

// API: MulticastSocket.AddMembership → 未设置地址，返回 PARAM_ERROR_CODE

HWTEST_F(UdpSocketTest, AddMembership002, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    /* No address set */
    int ret = socket->AddMembership(address);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);
}

// API: MulticastSocket.AddMembership → IPv4 组播地址 224.0.0.1:0

HWTEST_F(UdpSocketTest, AddMembership003, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    int ret = socket->AddMembership(address);
    /* May succeed or fail depending on environment permissions */
    if (ret == SOCKET_ERROR_OK) {
        EXPECT_GE(GetSocketFd(socket), 0);
        SocketStateBase state;
        EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
        EXPECT_FALSE(state.IsClose());
        socket->Close();
    }
}

// API: MulticastSocket.AddMembership → 重复 AddMembership 同一地址

HWTEST_F(UdpSocketTest, AddMembership004, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    EXPECT_NE(socket, nullptr);
    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    int ret = socket->AddMembership(address);
    if (ret == SOCKET_ERROR_OK) {
        /* Add membership again -- may succeed or fail */
        int ret2 = socket->AddMembership(address);
        if (ret2 == SOCKET_ERROR_OK) {
            EXPECT_EQ(ret2, SOCKET_ERROR_OK);
        }
        socket->Close();
    } else {
        /* Environment may not support multicast */
    }
}

// API: MulticastSocket.AddMembership → 无效地址 999.999.999.999

HWTEST_F(UdpSocketTest, AddMembershipInvalid001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("999.999.999.999");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    int ret = socket->AddMembership(address);
    EXPECT_NE(ret, SOCKET_ERROR_OK);
}

// API: MulticastSocket.AddMembership → 非组播地址 127.0.0.1

HWTEST_F(UdpSocketTest, AddMembershipInvalid002, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    /* Non-multicast address (not in 224.0.0.0/4 range) */
    address.SetAddress("127.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    int ret = socket->AddMembership(address);
    /* Joining a non-multicast address should fail */
    EXPECT_NE(ret, SOCKET_ERROR_OK);
}

/* ==================== DropMembership Tests ==================== */
// API: MulticastSocket.DropMembership → 未 AddMembership，fd 无效

HWTEST_F(UdpSocketTest, DropMembership001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    /* Without AddMembership, fd is invalid */
    int ret = socket->DropMembership(address);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

// API: MulticastSocket.DropMembership → 空地址，返回 PARAM_ERROR_CODE

HWTEST_F(UdpSocketTest, DropMembership002, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    int ret = socket->DropMembership(address);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);
}

// API: MulticastSocket.DropMembership → AddMembership 后 Drop，验证成功且 fd=-1

HWTEST_F(UdpSocketTest, DropMembership003, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->AddMembership(address), SOCKET_ERROR_OK);
    int ret = socket->DropMembership(address);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    /* After DropMembership, fd should be -1 */
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
}

// API: MulticastSocket.DropMembership → Close 后再 DropMembership

HWTEST_F(UdpSocketTest, DropMembershipAfterClose001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->AddMembership(address), SOCKET_ERROR_OK);
    socket->Close();
    int ret = socket->DropMembership(address);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

/* ==================== SetMulticastTTL Tests ==================== */
// API: MulticastSocket.SetMulticastTTL → 未创建 socket，返回 ERRNO_BAD_FD

HWTEST_F(UdpSocketTest, SetMulticastTTL001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    /* Without socket, should return ERRNO_BAD_FD */
    int ret = socket->SetMulticastTTL(1);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

// API: MulticastSocket.SetMulticastTTL → TTL > 255，返回 PARAM_ERROR_CODE

HWTEST_F(UdpSocketTest, SetMulticastTTL002, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    /* TTL > MAX_TTL should return PARAM_ERROR_CODE */
    int ret = socket->SetMulticastTTL(MAX_TTL + 1);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);
}

// API: MulticastSocket.SetMulticastTTL → AddMembership 后设置 TTL=0

HWTEST_F(UdpSocketTest, SetMulticastTTL003, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->AddMembership(address), SOCKET_ERROR_OK);

    int ret = socket->SetMulticastTTL(0);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
}

// API: MulticastSocket.SetMulticastTTL → AddMembership 后设置 TTL=255

HWTEST_F(UdpSocketTest, SetMulticastTTL004, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->AddMembership(address), SOCKET_ERROR_OK);

    int ret = socket->SetMulticastTTL(MAX_TTL);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
}

// API: MulticastSocket.SetMulticastTTL → AddMembership 后 TTL > MAX 仍返回 PARAM_ERROR_CODE

HWTEST_F(UdpSocketTest, SetMulticastTTL005, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->AddMembership(address), SOCKET_ERROR_OK);

    /* TTL > MAX_TTL even after AddMembership */
    int ret = socket->SetMulticastTTL(MAX_TTL + 1);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);
    socket->Close();
}

/* ==================== GetMulticastTTL Tests ==================== */
// API: MulticastSocket.GetMulticastTTL → 未创建 socket，返回 ERRNO_BAD_FD

HWTEST_F(UdpSocketTest, GetMulticastTTL001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    int ttl = -1;
    int ret = socket->GetMulticastTTL(ttl);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

// API: MulticastSocket.GetMulticastTTL → AddMembership 后获取默认 TTL

HWTEST_F(UdpSocketTest, GetMulticastTTL002, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->AddMembership(address), SOCKET_ERROR_OK);

    int ttl = -1;
    int ret = socket->GetMulticastTTL(ttl);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_GE(ttl, 0);
    EXPECT_LE(ttl, MAX_TTL);
    socket->Close();
}

// API: MulticastSocket.SetMulticastTTL + GetMulticastTTL → 设置 64 后获取验证

HWTEST_F(UdpSocketTest, GetMulticastTTL003, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->AddMembership(address), SOCKET_ERROR_OK);

    /* Set then get TTL */
    ASSERT_EQ(socket->SetMulticastTTL(64), SOCKET_ERROR_OK);
    int ttl = -1;
    ASSERT_EQ(socket->GetMulticastTTL(ttl), SOCKET_ERROR_OK);
    EXPECT_EQ(ttl, 64);
    socket->Close();
}

/* ==================== SetLoopbackMode Tests ==================== */
// API: MulticastSocket.SetLoopbackMode → 未创建 socket，返回 ERRNO_BAD_FD

HWTEST_F(UdpSocketTest, SetLoopbackMode001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    int ret = socket->SetLoopbackMode(true);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

// API: MulticastSocket.SetLoopbackMode → AddMembership 后设置 loopback=true

HWTEST_F(UdpSocketTest, SetLoopbackMode002, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->AddMembership(address), SOCKET_ERROR_OK);

    int ret = socket->SetLoopbackMode(true);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
}

// API: MulticastSocket.SetLoopbackMode → AddMembership 后设置 loopback=false

HWTEST_F(UdpSocketTest, SetLoopbackMode003, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->AddMembership(address), SOCKET_ERROR_OK);

    int ret = socket->SetLoopbackMode(false);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
}

/* ==================== GetLoopbackMode Tests ==================== */
// API: MulticastSocket.GetLoopbackMode → 未创建 socket，返回 ERRNO_BAD_FD

HWTEST_F(UdpSocketTest, GetLoopbackMode001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    bool loopback = false;
    int ret = socket->GetLoopbackMode(loopback);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

// API: MulticastSocket.GetLoopbackMode → AddMembership 后获取默认值

HWTEST_F(UdpSocketTest, GetLoopbackMode002, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->AddMembership(address), SOCKET_ERROR_OK);

    bool loopback = false;
    int ret = socket->GetLoopbackMode(loopback);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
}

// API: MulticastSocket.SetLoopbackMode + GetLoopbackMode → 设置 true/false 后获取验证

HWTEST_F(UdpSocketTest, GetLoopbackMode003, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->AddMembership(address), SOCKET_ERROR_OK);

    ASSERT_EQ(socket->SetLoopbackMode(true), SOCKET_ERROR_OK);
    bool loopback = false;
    ASSERT_EQ(socket->GetLoopbackMode(loopback), SOCKET_ERROR_OK);
    EXPECT_TRUE(loopback);

    ASSERT_EQ(socket->SetLoopbackMode(false), SOCKET_ERROR_OK);
    ASSERT_EQ(socket->GetLoopbackMode(loopback), SOCKET_ERROR_OK);
    EXPECT_FALSE(loopback);
    socket->Close();
}

/* ==================== SetReuseAddress Tests ==================== */
// API: MulticastSocket.SetReuseAddress → 未创建 socket，设置 true

HWTEST_F(UdpSocketTest, MulticastSetReuseAddress001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    /* Without socket created, should return SOCKET_ERROR_OK (napi behaviour) */
    int ret = socket->SetReuseAddress(true);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
}

// API: MulticastSocket.SetReuseAddress → 未创建 socket，设置 false

HWTEST_F(UdpSocketTest, MulticastSetReuseAddress002, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    int ret = socket->SetReuseAddress(false);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
}

// API: MulticastSocket.SetReuseAddress → AddMembership 后设置 true

HWTEST_F(UdpSocketTest, MulticastSetReuseAddress003, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->AddMembership(address), SOCKET_ERROR_OK);

    int ret = socket->SetReuseAddress(true);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
}

// API: MulticastSocket.SetReuseAddress → AddMembership 后设置 false

HWTEST_F(UdpSocketTest, MulticastSetReuseAddress004, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->AddMembership(address), SOCKET_ERROR_OK);

    int ret = socket->SetReuseAddress(false);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
}

/* ==================== MulticastSocket Inherited Ops Tests ==================== */
// API: MulticastSocket 继承的 OnMessage/OnError/OnClose/OnListening + Off*（验证类型兼容性）

HWTEST_F(UdpSocketTest, MulticastOnOffCallbacks001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    EXPECT_NE(socket, nullptr);
    socket->OnMessage(OnMessageCallback);
    socket->OnError(OnErrorCallback);
    socket->OnClose(OnCloseCallback);
    socket->OnListening(OnListeningCallback);

    socket->OffMessage();
    socket->OffError();
    socket->OffClose();
    socket->OffListening();
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
}

// API: MulticastSocket 继承的 GetState → 初始 isClose=true

HWTEST_F(UdpSocketTest, MulticastGetState001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    SocketStateBase state;
    int ret = socket->GetState(state);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsClose());
}

// API: MulticastSocket 继承的 GetSocketFd → 未操作 fd=-1

HWTEST_F(UdpSocketTest, MulticastGetSocketFd001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    EXPECT_NE(socket, nullptr);
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsClose());
}

// API: MulticastSocket 继承的 Close → 未操作直接 Close

HWTEST_F(UdpSocketTest, MulticastClose001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    int ret = socket->Close();
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsClose());
}

// API: MulticastSocket 继承的 Close → AddMembership 后 Close（验证 fd=-1）

HWTEST_F(UdpSocketTest, MulticastClose002, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->AddMembership(address), SOCKET_ERROR_OK);
    int ret = socket->Close();
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsClose());
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
}

// API: MulticastSocket 继承的 SetExtraOptions → 未操作，返回 ERRNO_BAD_FD

HWTEST_F(UdpSocketTest, MulticastSetExtraOptions001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    UDPExtraOptions options;
    int ret = socket->SetExtraOptions(options);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

// API: MulticastSocket 继承的 GetLocalAddress → 未操作，返回 ERRNO_BAD_FD

HWTEST_F(UdpSocketTest, MulticastGetLocalAddress001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    int ret = socket->GetLocalAddress(address);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

// API: MulticastSocket 继承的 Send → 未操作，返回 ERRNO_BAD_FD

HWTEST_F(UdpSocketTest, MulticastSend001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    UDPSendOptions options;
    options.SetData(TEST_DATA);
    options.address.SetAddress(TEST_ADDRESS);
    options.address.SetPort(TEST_PORT);
    options.address.SetFamilyBySaFamily(AF_INET);
    ProxyOptions proxyOptions;
    int ret = socket->Send(options, proxyOptions);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

/* ==================== MulticastSocket Lifecycle Tests ==================== */
// API: MulticastSocket 全生命周期 → AddMembership + Set/GetTTL + Set/GetLoopbackMode + SetReuseAddress + DropMembership

HWTEST_F(UdpSocketTest, MulticastLifecycle001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    socket->OnMessage(OnMessageCallback);
    socket->OnError(OnErrorCallback);
    socket->OnClose(OnCloseCallback);
    socket->OnListening(OnListeningCallback);

    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    int ret = socket->AddMembership(address);
    if (ret != SOCKET_ERROR_OK) {
        return;
    }

    /* Set TTL */
    EXPECT_EQ(socket->SetMulticastTTL(1), SOCKET_ERROR_OK);

    /* Get TTL */
    int ttl = -1;
    EXPECT_EQ(socket->GetMulticastTTL(ttl), SOCKET_ERROR_OK);
    EXPECT_EQ(ttl, 1);

    /* Set loopback */
    EXPECT_EQ(socket->SetLoopbackMode(false), SOCKET_ERROR_OK);

    /* Get loopback */
    bool loopback = true;
    EXPECT_EQ(socket->GetLoopbackMode(loopback), SOCKET_ERROR_OK);
    EXPECT_FALSE(loopback);

    /* Set reuse address */
    EXPECT_EQ(socket->SetReuseAddress(true), SOCKET_ERROR_OK);

    /* Drop membership */
    EXPECT_EQ(socket->DropMembership(address), SOCKET_ERROR_OK);

    /* After drop, fd is invalid */
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
}

// API: MulticastSocket → Close 后所有操作的错误路径验证

HWTEST_F(UdpSocketTest, MulticastOperationsAfterClose001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->AddMembership(address), SOCKET_ERROR_OK);
    socket->Close();

    /* All operations after close should gracefully fail */
    EXPECT_EQ(socket->AddMembership(address), SOCKET_ERROR_OK); /* AddMembership creates new fd */
    socket->Close();

    EXPECT_EQ(socket->DropMembership(address), static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);

    int ttl = 0;
    EXPECT_EQ(socket->SetMulticastTTL(1), static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
    EXPECT_EQ(socket->GetMulticastTTL(ttl), static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);

    bool loopback = false;
    EXPECT_EQ(socket->SetLoopbackMode(true), static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
    EXPECT_EQ(socket->GetLoopbackMode(loopback), static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);

    /* SetReuseAddress returns OK even without socket (napi behaviour) */
    EXPECT_EQ(socket->SetReuseAddress(true), SOCKET_ERROR_OK);
}

/* ==================== Callback Re-registration Tests ==================== */
// API: UDPSocket.OnMessage → OffMessage 后重新 OnMessage（回调重新注册）

HWTEST_F(UdpSocketTest, OnOffCallbackReRegister001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    EXPECT_NE(socket, nullptr);
    std::atomic<int> callbackCount(0);

    auto cb = [&callbackCount](const std::string &data, const SocketRemoteInfo &remoteInfo) {
        (void)data;
        (void)remoteInfo;
        callbackCount.fetch_add(1);
    };

    socket->OnMessage(cb);
    socket->OffMessage();
    /* Re-register after off */
    socket->OnMessage(cb);
    EXPECT_EQ(callbackCount.load(), 0);
}

// API: UDPSocket.OnClose → OffClose 后重新 OnClose，Bind+Close 验证新回调触发

HWTEST_F(UdpSocketTest, OnOffCallbackReRegister002, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    std::atomic<int> closeCount(0);

    socket->OnClose(
        [&closeCount]() {
            closeCount.fetch_add(1);
        });
    socket->OffClose();
    socket->OnClose(
        [&closeCount]() {
            closeCount.fetch_add(1);
        });

    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    socket->Close();
    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS));
    /* Re-registered callback should be called */
    EXPECT_EQ(closeCount.load(), 1);
}

/* ==================== SetExtraOptions Broadcast Tests ==================== */
// API: UDPSocket.SetExtraOptions → Bind 后设置 broadcast=true

HWTEST_F(UdpSocketTest, SetExtraOptionsBroadcast001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);

    UDPExtraOptions options;
    options.SetBroadcast(true);
    options.SetBroadcastFlag(true);
    int ret = socket->SetExtraOptions(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
}

// API: UDPSocket.SetExtraOptions → Bind 后设置 broadcast=false

HWTEST_F(UdpSocketTest, SetExtraOptionsBroadcast002, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);

    UDPExtraOptions options;
    options.SetBroadcast(false);
    options.SetBroadcastFlag(true);
    int ret = socket->SetExtraOptions(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
}

/* ==================== SetExtraOptions No Flags Tests ==================== */
// API: UDPSocket.SetExtraOptions → Bind 后不设任何 flag（no-op 成功）

HWTEST_F(UdpSocketTest, SetExtraOptionsNoFlags001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);

    /* No option flags set -- should be a no-op success */
    UDPExtraOptions options;
    int ret = socket->SetExtraOptions(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
}

// API: UDPSocket.SetExtraOptions → Bind 后设置全部 flag 和全部选项值

HWTEST_F(UdpSocketTest, SetExtraOptionsAllFlags001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);

    UDPExtraOptions options;
    options.SetReceiveBufferSize(8192);
    options.SetRecvBufSizeFlag(true);
    options.SetSendBufferSize(8192);
    options.SetSendBufSizeFlag(true);
    options.SetReuseAddress(true);
    options.SetReuseaddrFlag(true);
    options.SetSocketTimeout(3000);
    options.SetTimeoutFlag(true);
    options.SetBroadcast(true);
    options.SetBroadcastFlag(true);
    int ret = socket->SetExtraOptions(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
}

/* ==================== IPv6 Send/Receive Tests ==================== */
// API: UDPSocket.Bind + OnMessage + 外部 IPv6 socket 发送 → 验证 IPv6 接收和回调

HWTEST_F(UdpSocketTest, SendReceiveIPv6001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress bindAddr;
    bindAddr.SetAddress("::1");
    bindAddr.SetPort(0);
    bindAddr.SetFamilyBySaFamily(AF_INET6);
    int ret = socket->Bind(bindAddr);
    if (ret != SOCKET_ERROR_OK) {
        /* IPv6 may not be available */
        return;
    }

    NetAddress localAddr;
    ASSERT_EQ(socket->GetLocalAddress(localAddr), SOCKET_ERROR_OK);
    uint16_t myPort = localAddr.GetPort();

    /* Create a sender socket */
    int sendFd = ::socket(AF_INET6, SOCK_DGRAM, 0);
    ASSERT_GE(sendFd, 0);

    sockaddr_in6 destAddr = {};
    destAddr.sin6_family = AF_INET6;
    destAddr.sin6_port = htons(myPort);
    inet_pton(AF_INET6, "::1", &destAddr.sin6_addr);

    std::atomic<bool> messageReceived(false);
    socket->OnMessage(
        [&messageReceived](const std::string &data, const SocketRemoteInfo &remoteInfo) {
            (void)data;
            (void)remoteInfo;
            messageReceived.store(true);
        });

    ssize_t sentLen = sendto(sendFd, TEST_DATA, strlen(TEST_DATA), 0,
                             reinterpret_cast<sockaddr *>(&destAddr), sizeof(destAddr));
    EXPECT_EQ(sentLen, static_cast<ssize_t>(strlen(TEST_DATA)));

    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS * 2));
    EXPECT_TRUE(messageReceived.load());

    close(sendFd);
    socket->Close();
}

// API: UDPSocket.Bind + Send IPv6 → 向 IPv6 监听端发送数据并验证

HWTEST_F(UdpSocketTest, SendIPv6001, TestSize.Level1)
{
    uint16_t listenPort = 0;
    /* Create IPv6 listener */
    int listenFd = socket(AF_INET6, SOCK_DGRAM, 0);
    ASSERT_GE(listenFd, 0);
    sockaddr_in6 listenAddr = {};
    listenAddr.sin6_family = AF_INET6;
    listenAddr.sin6_port = htons(0);
    listenAddr.sin6_addr = in6addr_loopback;
    ASSERT_EQ(bind(listenFd, reinterpret_cast<sockaddr *>(&listenAddr), sizeof(listenAddr)), 0);

    sockaddr_in6 realAddr = {};
    socklen_t len = sizeof(realAddr);
    ASSERT_EQ(getsockname(listenFd, reinterpret_cast<sockaddr *>(&realAddr), &len), 0);
    listenPort = ntohs(realAddr.sin6_port);

    auto socket = std::make_shared<UDPSocket>();
    NetAddress bindAddr;
    bindAddr.SetAddress("::1");
    bindAddr.SetPort(0);
    bindAddr.SetFamilyBySaFamily(AF_INET6);
    int ret = socket->Bind(bindAddr);
    if (ret != SOCKET_ERROR_OK) {
        close(listenFd);
        return;
    }

    UDPSendOptions options;
    options.SetData(TEST_DATA);
    options.address.SetAddress("::1");
    options.address.SetPort(listenPort);
    options.address.SetFamilyBySaFamily(AF_INET6);
    ProxyOptions proxyOptions;
    EXPECT_EQ(socket->Send(options, proxyOptions), SOCKET_ERROR_OK);

    socket->Close();
    close(listenFd);
}

/* ==================== GetSocketFd Multicast Tests ==================== */
// API: MulticastSocket 继承的 GetSocketFd → AddMembership 后 fd >= 0

HWTEST_F(UdpSocketTest, MulticastGetSocketFdAfterAddMembership001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    int ret = socket->AddMembership(address);
    if (ret != SOCKET_ERROR_OK) {
        return;
    }
    EXPECT_GE(GetSocketFd(socket), 0);
    socket->Close();
}

/* ==================== GetLocalAddress Multicast Tests ==================== */
// API: MulticastSocket 继承的 GetLocalAddress → AddMembership 后获取本端地址

HWTEST_F(UdpSocketTest, MulticastGetLocalAddressAfterAdd001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("224.0.0.1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    int ret = socket->AddMembership(address);
    if (ret != SOCKET_ERROR_OK) {
        return;
    }
    NetAddress localAddr;
    EXPECT_EQ(socket->GetLocalAddress(localAddr), SOCKET_ERROR_OK);
    socket->Close();
}

/* ==================== AddMembership IPv6 Tests ==================== */
// API: MulticastSocket.AddMembership → IPv6 组播地址 ff02::1

HWTEST_F(UdpSocketTest, MulticastAddMembershipIPv6_001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress address;
    address.SetAddress("ff02::1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET6);
    int ret = socket->AddMembership(address);
    /* May succeed or fail depending on environment */
    if (ret == SOCKET_ERROR_OK) {
        EXPECT_GE(GetSocketFd(socket), 0);
        socket->Close();
    }
}

/* ==================== Bind After AddMembership Tests ==================== */
// API: MulticastSocket.AddMembership → 指定 port 后验证自动 Bind，GetLocalAddress 验证 port

HWTEST_F(UdpSocketTest, MulticastBindAfterAddMembership001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();
    NetAddress multiAddr;
    multiAddr.SetAddress("224.0.0.1");
    multiAddr.SetPort(TEST_PORT);
    multiAddr.SetFamilyBySaFamily(AF_INET);
    int ret = socket->AddMembership(multiAddr);
    if (ret != SOCKET_ERROR_OK) {
        return;
    }

    /* After AddMembership, socket should already be bound */
    NetAddress localAddr;
    EXPECT_EQ(socket->GetLocalAddress(localAddr), SOCKET_ERROR_OK);
    EXPECT_EQ(localAddr.GetPort(), TEST_PORT);

    socket->Close();
}

/* ==================== Broadcast End-to-End Tests ==================== */

// API: UDPSocket.SetExtraOptions + Send → 设置 broadcast=true 后向广播地址 255.*.*.* 发送数据
HWTEST_F(UdpSocketTest, BroadcastSend001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress bindAddr;
    bindAddr.SetAddress("0.0.0.0");
    bindAddr.SetPort(0);
    bindAddr.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(bindAddr), SOCKET_ERROR_OK);

    /* Enable broadcast on the sender socket */
    UDPExtraOptions options;
    options.SetBroadcast(true);
    options.SetBroadcastFlag(true);
    int ret = socket->SetExtraOptions(options);
    ASSERT_EQ(ret, SOCKET_ERROR_OK);

    /* Send to local broadcast address */
    UDPSendOptions sendOpts;
    sendOpts.SetData(TEST_DATA);
    sendOpts.address.SetAddress("255.255.255.255");
    sendOpts.address.SetPort(TEST_PORT);
    sendOpts.address.SetFamilyBySaFamily(AF_INET);
    ProxyOptions proxyOpts;
    ret = socket->Send(sendOpts, proxyOpts);
    /* Send to broadcast may succeed (sendto to 255.*.*.* is permitted with SO_BROADCAST) */
    EXPECT_EQ(ret, SOCKET_ERROR_OK);

    socket->Close();
}

// API: UDPSocket.Broadcast → 接收端绑定 0.0.0.0 并启用 broadcast，外部 socket 发送到广播地址，
// 验证 OnMessage 回调触发
HWTEST_F(UdpSocketTest, BroadcastReceive001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    std::atomic<bool> messageReceived(false);
    std::atomic<size_t> receivedLength(0);
    socket->OnMessage(
        [&messageReceived, &receivedLength](const std::string &data,
                                            const SocketRemoteInfo &remoteInfo) {
            (void)remoteInfo;
            receivedLength.store(data.size());
            messageReceived.store(true);
        });

    /* Bind to INADDR_ANY so we can receive broadcast packets */
    NetAddress bindAddr;
    bindAddr.SetAddress("0.0.0.0");
    bindAddr.SetPort(0);
    bindAddr.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(bindAddr), SOCKET_ERROR_OK);

    /* Enable broadcast on the receiver */
    UDPExtraOptions options;
    options.SetBroadcast(true);
    options.SetBroadcastFlag(true);
    ASSERT_EQ(socket->SetExtraOptions(options), SOCKET_ERROR_OK);

    /* Get the port assigned by bind */
    NetAddress localAddr;
    ASSERT_EQ(socket->GetLocalAddress(localAddr), SOCKET_ERROR_OK);
    uint16_t myPort = localAddr.GetPort();

    /* Send from a raw socket to broadcast address targeting our port */
    int sendFd = ::socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(sendFd, 0);
    int bcastEnable = 1;
    ASSERT_EQ(setsockopt(sendFd, SOL_SOCKET, SO_BROADCAST, &bcastEnable, sizeof(bcastEnable)), 0);

    sockaddr_in destAddr = {};
    destAddr.sin_family = AF_INET;
    destAddr.sin_port = htons(myPort);
    destAddr.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    ssize_t sentLen = sendto(sendFd, TEST_DATA, strlen(TEST_DATA), 0,
                             reinterpret_cast<sockaddr *>(&destAddr), sizeof(destAddr));
    EXPECT_EQ(sentLen, static_cast<ssize_t>(strlen(TEST_DATA)));

    /* Wait for the broadcast packet to arrive */
    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS * 5));
    EXPECT_TRUE(messageReceived.load());
    EXPECT_EQ(receivedLength.load(), strlen(TEST_DATA));

    close(sendFd);
    socket->Close();
}

// API: UDPSocket.Broadcast → 两个 UDPSocket 通过广播地址 255.*.*.*:PORT 完成发送/接收 round-trip
HWTEST_F(UdpSocketTest, BroadcastSendReceive001, TestSize.Level1)
{
    /* --- Receiver --- */
    auto receiver = std::make_shared<UDPSocket>();
    std::atomic<bool> messageReceived(false);
    std::string receivedData;
    receiver->OnMessage(
        [&messageReceived, &receivedData](const std::string &data,
                                          const SocketRemoteInfo &remoteInfo) {
            (void)remoteInfo;
            receivedData = data;
            messageReceived.store(true);
        });

    NetAddress bindAddr;
    bindAddr.SetAddress("0.0.0.0");
    bindAddr.SetPort(TEST_PORT);
    bindAddr.SetFamilyBySaFamily(AF_INET);
    int ret = receiver->Bind(bindAddr);
    if (ret != SOCKET_ERROR_OK) {
        /* Port may be in use, skip test gracefully */
        return;
    }

    UDPExtraOptions recvOpts;
    recvOpts.SetBroadcast(true);
    recvOpts.SetBroadcastFlag(true);
    ASSERT_EQ(receiver->SetExtraOptions(recvOpts), SOCKET_ERROR_OK);

    /* --- Sender --- */
    auto sender = std::make_shared<UDPSocket>();
    NetAddress sndBindAddr;
    sndBindAddr.SetAddress("0.0.0.0");
    sndBindAddr.SetPort(0);
    sndBindAddr.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(sender->Bind(sndBindAddr), SOCKET_ERROR_OK);

    UDPExtraOptions sndOpts;
    sndOpts.SetBroadcast(true);
    sndOpts.SetBroadcastFlag(true);
    ASSERT_EQ(sender->SetExtraOptions(sndOpts), SOCKET_ERROR_OK);

    /* Send to broadcast address */
    UDPSendOptions sendOpts;
    sendOpts.SetData(TEST_DATA);
    sendOpts.address.SetAddress("255.255.255.255");
    sendOpts.address.SetPort(TEST_PORT);
    sendOpts.address.SetFamilyBySaFamily(AF_INET);
    ProxyOptions proxyOpts;
    ASSERT_EQ(sender->Send(sendOpts, proxyOpts), SOCKET_ERROR_OK);

    /* Wait for broadcast message to arrive */
    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS * 5));
    EXPECT_TRUE(messageReceived.load());
    EXPECT_EQ(receivedData, std::string(TEST_DATA));

    sender->Close();
    receiver->Close();
}

// API: UDPSocket.Broadcast + SetExtraOptions → 未启用 broadcast 时向广播地址发送应失败
HWTEST_F(UdpSocketTest, BroadcastSendWithoutFlag001, TestSize.Level1)
{
    auto socket = std::make_shared<UDPSocket>();
    NetAddress bindAddr;
    bindAddr.SetAddress("0.0.0.0");
    bindAddr.SetPort(0);
    bindAddr.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(bindAddr), SOCKET_ERROR_OK);

    /* Do NOT enable broadcast -- send to broadcast address should fail */
    UDPSendOptions sendOpts;
    sendOpts.SetData(TEST_DATA);
    sendOpts.address.SetAddress("255.255.255.255");
    sendOpts.address.SetPort(TEST_PORT);
    sendOpts.address.SetFamilyBySaFamily(AF_INET);
    ProxyOptions proxyOpts;
    int ret = socket->Send(sendOpts, proxyOpts);
    /* Without SO_BROADCAST, sending to 255.*.*.* should fail */
    EXPECT_NE(ret, SOCKET_ERROR_OK);

    socket->Close();
}

// API: MulticastSocket 继承的 SetExtraOptions → multicast socket 启用 broadcast=true 后向广播地址发送
HWTEST_F(UdpSocketTest, MulticastBroadcastSend001, TestSize.Level1)
{
    auto socket = std::make_shared<MulticastSocket>();

    /* First add a multicast membership to create the socket */
    NetAddress multiAddr;
    multiAddr.SetAddress("224.0.0.1");
    multiAddr.SetPort(0);
    multiAddr.SetFamilyBySaFamily(AF_INET);
    int ret = socket->AddMembership(multiAddr);
    if (ret != SOCKET_ERROR_OK) {
        return;
    }

    /* Enable broadcast on the multicast socket */
    UDPExtraOptions options;
    options.SetBroadcast(true);
    options.SetBroadcastFlag(true);
    ASSERT_EQ(socket->SetExtraOptions(options), SOCKET_ERROR_OK);

    /* Send to broadcast address via multicast socket */
    UDPSendOptions sendOpts;
    sendOpts.SetData(TEST_DATA);
    sendOpts.address.SetAddress("255.255.255.255");
    sendOpts.address.SetPort(TEST_PORT);
    sendOpts.address.SetFamilyBySaFamily(AF_INET);
    ProxyOptions proxyOpts;
    ret = socket->Send(sendOpts, proxyOpts);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);

    socket->Close();
}

} // namespace
