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
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include "gtest/gtest.h"

#include "net_address.h"
#include "socket_constant.h"
#include "socket_remote_info.h"
#include "socket_state_base.h"
#include "tcp_connect_options.h"
#include "tcp_extra_options.h"
#include "tcp_send_options.h"
#include "proxy_options.h"
#include "tcp_socket_client_innerapi.h"

namespace {
using namespace testing::ext;
using namespace OHOS::NetStack::Socket;

static constexpr int32_t INVALID_FD = -1;
static constexpr uint16_t TEST_PORT = 18090;
static constexpr const char *TEST_ADDRESS = "127.0.0.1";
static constexpr const char *TEST_DATA = "hello tcp client";
static constexpr const char *TEST_DATA_LARGE =
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789";
static constexpr int32_t SLEEP_MS = 100;
static constexpr int32_t LISTEN_BACKLOG = 5;
static constexpr uint32_t TEST_TIMEOUT_MS = 5000;
static constexpr uint32_t WAIT_TIMEOUT_MS = 10000;

static int MakeTcpListenFd(uint16_t port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    int reuse = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, LISTEN_BACKLOG) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static int GetListenPort(int listenFd)
{
    sockaddr_in addr = {0};
    socklen_t len = sizeof(addr);
    if (getsockname(listenFd, reinterpret_cast<sockaddr *>(&addr), &len) < 0) {
        return -1;
    }
    return ntohs(addr.sin_port);
}

static int32_t GetSocketFd(const std::shared_ptr<TCPSocket> &socket)
{
    int32_t fd = INVALID_FD;
    (void)socket->GetSocketFd(fd);
    return fd;
}

class TcpSocketClientTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};

/* Callbacks must match the typedefs in tcp_socket_client_innerapi.h:
 *   TCPSocketOnMessageCallback = void(const std::string &, const SocketRemoteInfo &)
 *   TCPSocketOnConnectCallback = void(void)
 *   TCPSocketOnCloseCallback   = void(void)
 *   TCPSocketOnErrorCallback   = void(int32_t, const std::string &)
 */
static void OnMessageCallback(const std::string &data, const SocketRemoteInfo &remoteInfo)
{
    (void)data;
    (void)remoteInfo;
}

static void OnConnectCallback()
{
}

static void OnErrorCallback(int32_t errorNumber, const std::string &errorString)
{
    (void)errorNumber;
    (void)errorString;
}

static void OnCloseCallback()
{
}

/* ==================== Constructor / Destructor Tests ==================== */

HWTEST_F(TcpSocketClientTest, Constructor001, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsClose());
    EXPECT_FALSE(state.IsBound());
    EXPECT_FALSE(state.IsConnected());
}

HWTEST_F(TcpSocketClientTest, Constructor002, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_FALSE(state.IsBound());
    EXPECT_FALSE(state.IsConnected());
    EXPECT_TRUE(state.IsClose());
}

HWTEST_F(TcpSocketClientTest, Destructor001, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    EXPECT_NE(socket, nullptr);
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
    socket.reset();
}

HWTEST_F(TcpSocketClientTest, Destructor002, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    socket.reset();
}

HWTEST_F(TcpSocketClientTest, Destructor003, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    socket->Close();
    socket.reset();
}

/* ==================== On/Off Callback Tests ==================== */

HWTEST_F(TcpSocketClientTest, OnMessageCallback001, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    EXPECT_NE(socket, nullptr);
    socket->OnMessage(OnMessageCallback);
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
}

HWTEST_F(TcpSocketClientTest, OnConnectCallback001, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    EXPECT_NE(socket, nullptr);
    socket->OnConnect(OnConnectCallback);
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
}

HWTEST_F(TcpSocketClientTest, OnErrorCallback001, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    EXPECT_NE(socket, nullptr);
    socket->OnError(OnErrorCallback);
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
}

HWTEST_F(TcpSocketClientTest, OnCloseCallback001, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    EXPECT_NE(socket, nullptr);
    socket->OnClose(OnCloseCallback);
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
}

HWTEST_F(TcpSocketClientTest, OffCallbacks001, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    EXPECT_NE(socket, nullptr);
    socket->OnMessage(OnMessageCallback);
    socket->OnConnect(OnConnectCallback);
    socket->OnError(OnErrorCallback);
    socket->OnClose(OnCloseCallback);

    socket->OffMessage();
    socket->OffConnect();
    socket->OffError();
    socket->OffClose();
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
}

HWTEST_F(TcpSocketClientTest, OffCallbacks002, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    EXPECT_NE(socket, nullptr);
    /* Calling Off without On should be safe */
    socket->OffMessage();
    socket->OffConnect();
    socket->OffError();
    socket->OffClose();
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
}

/* ==================== Bind Tests ==================== */

HWTEST_F(TcpSocketClientTest, Bind001, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    int ret = socket->Bind(address);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_GE(GetSocketFd(socket), 0);
    socket->Close();
}

HWTEST_F(TcpSocketClientTest, Bind002, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    NetAddress address;
    address.SetAddress("0.0.0.0");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    int ret = socket->Bind(address);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
}

HWTEST_F(TcpSocketClientTest, Bind003, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    NetAddress address;
    int ret = socket->Bind(address);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);
}

HWTEST_F(TcpSocketClientTest, Bind004, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    NetAddress address;
    address.SetAddress("");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    int ret = socket->Bind(address);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);
}

HWTEST_F(TcpSocketClientTest, Bind006, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(TEST_PORT);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    int firstFd = GetSocketFd(socket);
    EXPECT_GE(firstFd, 0);

    /* Bind to same address again should return OK (idempotent) */
    int ret = socket->Bind(address);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
}

HWTEST_F(TcpSocketClientTest, Bind007, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
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

HWTEST_F(TcpSocketClientTest, Bind008, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    SocketStateBase state;
    int ret = socket->GetState(state);
    EXPECT_NE(ret, SOCKET_ERROR_OK);
    socket->Close();
}

HWTEST_F(TcpSocketClientTest, BindInvalidAddress001, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    NetAddress address;
    address.SetAddress("999.999.999.999");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    int ret = socket->Bind(address);
    EXPECT_NE(ret, SOCKET_ERROR_OK);
}

HWTEST_F(TcpSocketClientTest, BindIPv6001, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
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

HWTEST_F(TcpSocketClientTest, BindAfterClose001, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
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

/* ==================== Connect Tests ==================== */

static bool WaitForConnected(const std::shared_ptr<TCPSocket> &socket, uint32_t timeoutMs = WAIT_TIMEOUT_MS)
{
    auto start = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();
    while (elapsed < static_cast<int64_t>(timeoutMs)) {
        SocketStateBase state;
        (void)socket->GetState(state);
        if (state.IsConnected()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS));
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
    }
    return false;
}

HWTEST_F(TcpSocketClientTest, Connect001, TestSize.Level1)
{
    int listenFd = MakeTcpListenFd(0);
    ASSERT_GE(listenFd, 0);
    uint16_t realPort = static_cast<uint16_t>(GetListenPort(listenFd));

    auto socket = std::make_shared<TCPSocket>();
    std::atomic<bool> connectCalled(false);
    socket->OnConnect([&connectCalled]() { connectCalled.store(true); });

    TcpConnectOptions connectOptions;
    connectOptions.address.SetAddress(TEST_ADDRESS);
    connectOptions.address.SetPort(realPort);
    connectOptions.address.SetFamilyBySaFamily(AF_INET);
    connectOptions.SetTimeout(TEST_TIMEOUT_MS);

    ProxyOptions proxyOptions;
    int ret = socket->Connect(connectOptions, proxyOptions);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);

    EXPECT_TRUE(WaitForConnected(socket));
    /* On OHOS platform, OnConnect callback may fire asynchronously via ConnectMonitor;
     * poll briefly to allow the async notification to arrive */
    if (!connectCalled.load()) {
        auto start = std::chrono::steady_clock::now();
        while (!connectCalled.load()) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if (elapsed >= SLEEP_MS * 5) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS / 10));
        }
    }
    EXPECT_TRUE(connectCalled.load());
    socket->Close();
    close(listenFd);
}

HWTEST_F(TcpSocketClientTest, Connect002, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    TcpConnectOptions connectOptions;
    connectOptions.address.SetAddress("");
    connectOptions.address.SetPort(0);
    connectOptions.address.SetFamilyBySaFamily(AF_INET);
    ProxyOptions proxyOptions;
    int ret = socket->Connect(connectOptions, proxyOptions);
    EXPECT_NE(ret, SOCKET_ERROR_OK);
}

HWTEST_F(TcpSocketClientTest, ConnectInvalidAddress001, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    TcpConnectOptions connectOptions;
    connectOptions.address.SetAddress("999.999.999.999");
    connectOptions.address.SetPort(TEST_PORT);
    connectOptions.address.SetFamilyBySaFamily(AF_INET);
    ProxyOptions proxyOptions;
    int ret = socket->Connect(connectOptions, proxyOptions);
    EXPECT_NE(ret, SOCKET_ERROR_OK);
    socket->Close();
}

HWTEST_F(TcpSocketClientTest, ConnectAfterBind001, TestSize.Level1)
{
    int listenFd = MakeTcpListenFd(0);
    ASSERT_GE(listenFd, 0);
    uint16_t realPort = static_cast<uint16_t>(GetListenPort(listenFd));

    auto socket = std::make_shared<TCPSocket>();
    NetAddress bindAddr;
    bindAddr.SetAddress(TEST_ADDRESS);
    bindAddr.SetPort(0);
    bindAddr.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(bindAddr), SOCKET_ERROR_OK);

    TcpConnectOptions connectOptions;
    connectOptions.address.SetAddress(TEST_ADDRESS);
    connectOptions.address.SetPort(realPort);
    connectOptions.address.SetFamilyBySaFamily(AF_INET);
    connectOptions.SetTimeout(TEST_TIMEOUT_MS);
    int ret = socket->Connect(connectOptions, ProxyOptions{});
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_TRUE(WaitForConnected(socket));
    socket->Close();
    close(listenFd);
}

HWTEST_F(TcpSocketClientTest, ConnectAfterClose001, TestSize.Level1)
{
    int listenFd = MakeTcpListenFd(0);
    ASSERT_GE(listenFd, 0);
    uint16_t realPort = static_cast<uint16_t>(GetListenPort(listenFd));

    auto socket = std::make_shared<TCPSocket>();
    TcpConnectOptions connectOptions;
    connectOptions.address.SetAddress(TEST_ADDRESS);
    connectOptions.address.SetPort(realPort);
    connectOptions.address.SetFamilyBySaFamily(AF_INET);
    connectOptions.SetTimeout(TEST_TIMEOUT_MS);
    ASSERT_EQ(socket->Connect(connectOptions, ProxyOptions{}), SOCKET_ERROR_OK);
    EXPECT_TRUE(WaitForConnected(socket));
    socket->Close();

    /* Reconnect after close */
    int ret = socket->Connect(connectOptions, ProxyOptions{});
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_TRUE(WaitForConnected(socket));
    socket->Close();
    close(listenFd);
}

HWTEST_F(TcpSocketClientTest, ConnectTimeout001, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    EXPECT_NE(socket, nullptr);
    TcpConnectOptions connectOptions;
    connectOptions.address.SetAddress(TEST_ADDRESS);
    connectOptions.address.SetPort(TEST_PORT);
    connectOptions.address.SetFamilyBySaFamily(AF_INET);
    connectOptions.SetTimeout(0);
    ProxyOptions proxyOptions;
    /* Connecting to a port that's not listening should fail or timeout */
    int ret = socket->Connect(connectOptions, proxyOptions);
    EXPECT_NE(ret, SOCKET_ERROR_OK);
    socket->Close();
}

HWTEST_F(TcpSocketClientTest, ConnectIPv6001, TestSize.Level1)
{
    int listenFd = socket(AF_INET6, SOCK_STREAM, 0);
    if (listenFd < 0) {
        return;
    }
    int reuse = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    sockaddr_in6 addr6 = {0};
    addr6.sin6_family = AF_INET6;
    addr6.sin6_port = 0;
    addr6.sin6_addr = in6addr_loopback;
    if (bind(listenFd, reinterpret_cast<sockaddr *>(&addr6), sizeof(addr6)) < 0) {
        close(listenFd);
        return;
    }
    if (listen(listenFd, LISTEN_BACKLOG) < 0) {
        close(listenFd);
        return;
    }
    sockaddr_in6 realAddr = {0};
    socklen_t len = sizeof(realAddr);
    getsockname(listenFd, reinterpret_cast<sockaddr *>(&realAddr), &len);
    uint16_t realPort = ntohs(realAddr.sin6_port);

    auto socket = std::make_shared<TCPSocket>();
    TcpConnectOptions connectOptions;
    connectOptions.address.SetAddress("::1");
    connectOptions.address.SetPort(realPort);
    connectOptions.address.SetFamilyBySaFamily(AF_INET6);
    connectOptions.SetTimeout(TEST_TIMEOUT_MS);
    int ret = socket->Connect(connectOptions, ProxyOptions{});
    if (ret == SOCKET_ERROR_OK) {
        EXPECT_GE(GetSocketFd(socket), 0);
        EXPECT_TRUE(WaitForConnected(socket));
        socket->Close();
    }
    close(listenFd);
}

/* ==================== Send Tests ==================== */

HWTEST_F(TcpSocketClientTest, Send001, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    TCPSendOptions options;
    options.SetData(TEST_DATA);
    int ret = socket->Send(options);
    /* No connection, should fail */
    EXPECT_NE(ret, SOCKET_ERROR_OK);
}

HWTEST_F(TcpSocketClientTest, Send002, TestSize.Level1)
{
    int listenFd = MakeTcpListenFd(0);
    ASSERT_GE(listenFd, 0);
    uint16_t realPort = static_cast<uint16_t>(GetListenPort(listenFd));

    auto socket = std::make_shared<TCPSocket>();
    TcpConnectOptions connectOptions;
    connectOptions.address.SetAddress(TEST_ADDRESS);
    connectOptions.address.SetPort(realPort);
    connectOptions.address.SetFamilyBySaFamily(AF_INET);
    connectOptions.SetTimeout(TEST_TIMEOUT_MS);
    ASSERT_EQ(socket->Connect(connectOptions, ProxyOptions{}), SOCKET_ERROR_OK);
    EXPECT_TRUE(WaitForConnected(socket));

    TCPSendOptions options;
    options.SetData(TEST_DATA);
    int ret = socket->Send(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
    close(listenFd);
}

HWTEST_F(TcpSocketClientTest, Send003, TestSize.Level1)
{
    int listenFd = MakeTcpListenFd(0);
    ASSERT_GE(listenFd, 0);
    uint16_t realPort = static_cast<uint16_t>(GetListenPort(listenFd));

    auto socket = std::make_shared<TCPSocket>();
    TcpConnectOptions connectOptions;
    connectOptions.address.SetAddress(TEST_ADDRESS);
    connectOptions.address.SetPort(realPort);
    connectOptions.address.SetFamilyBySaFamily(AF_INET);
    connectOptions.SetTimeout(TEST_TIMEOUT_MS);
    ASSERT_EQ(socket->Connect(connectOptions, ProxyOptions{}), SOCKET_ERROR_OK);
    EXPECT_TRUE(WaitForConnected(socket));

    TCPSendOptions options;
    options.SetData("");
    int ret = socket->Send(options);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);
    socket->Close();
    close(listenFd);
}

HWTEST_F(TcpSocketClientTest, Send004, TestSize.Level1)
{
    int listenFd = MakeTcpListenFd(0);
    ASSERT_GE(listenFd, 0);
    uint16_t realPort = static_cast<uint16_t>(GetListenPort(listenFd));

    auto socket = std::make_shared<TCPSocket>();
    TcpConnectOptions connectOptions;
    connectOptions.address.SetAddress(TEST_ADDRESS);
    connectOptions.address.SetPort(realPort);
    connectOptions.address.SetFamilyBySaFamily(AF_INET);
    connectOptions.SetTimeout(TEST_TIMEOUT_MS);
    ASSERT_EQ(socket->Connect(connectOptions, ProxyOptions{}), SOCKET_ERROR_OK);
    EXPECT_TRUE(WaitForConnected(socket));

    TCPSendOptions options;
    options.SetData(TEST_DATA_LARGE);
    int ret = socket->Send(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
    close(listenFd);
}

/* ==================== Close Tests ==================== */

HWTEST_F(TcpSocketClientTest, Close001, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    int ret = socket->Close();
    EXPECT_EQ(ret, UNKNOWN_ERROR);
}

HWTEST_F(TcpSocketClientTest, Close002, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
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
}

HWTEST_F(TcpSocketClientTest, Close003, TestSize.Level1)
{
    int listenFd = MakeTcpListenFd(0);
    ASSERT_GE(listenFd, 0);
    uint16_t realPort = static_cast<uint16_t>(GetListenPort(listenFd));

    auto socket = std::make_shared<TCPSocket>();
    TcpConnectOptions connectOptions;
    connectOptions.address.SetAddress(TEST_ADDRESS);
    connectOptions.address.SetPort(realPort);
    connectOptions.address.SetFamilyBySaFamily(AF_INET);
    connectOptions.SetTimeout(TEST_TIMEOUT_MS);
    ASSERT_EQ(socket->Connect(connectOptions, ProxyOptions{}), SOCKET_ERROR_OK);
    EXPECT_TRUE(WaitForConnected(socket));

    std::atomic<bool> closeCalled(false);
    socket->OnClose([&closeCalled]() { closeCalled.store(true); });
    int ret = socket->Close();
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_TRUE(closeCalled.load());
    close(listenFd);
}

HWTEST_F(TcpSocketClientTest, Close004, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    socket->Close();
    int ret = socket->Close();
    EXPECT_EQ(ret, UNKNOWN_ERROR);
}

/* ==================== GetState Tests ==================== */

HWTEST_F(TcpSocketClientTest, GetState001, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    SocketStateBase state;
    int ret = socket->GetState(state);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsClose());
}

HWTEST_F(TcpSocketClientTest, GetState002, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    SocketStateBase state;
    int ret = socket->GetState(state);
    EXPECT_NE(ret, SOCKET_ERROR_OK);
    socket->Close();
}

HWTEST_F(TcpSocketClientTest, GetState003, TestSize.Level1)
{
    int listenFd = MakeTcpListenFd(0);
    ASSERT_GE(listenFd, 0);
    uint16_t realPort = static_cast<uint16_t>(GetListenPort(listenFd));

    auto socket = std::make_shared<TCPSocket>();
    TcpConnectOptions connectOptions;
    connectOptions.address.SetAddress(TEST_ADDRESS);
    connectOptions.address.SetPort(realPort);
    connectOptions.address.SetFamilyBySaFamily(AF_INET);
    connectOptions.SetTimeout(TEST_TIMEOUT_MS);
    ASSERT_EQ(socket->Connect(connectOptions, ProxyOptions{}), SOCKET_ERROR_OK);
    EXPECT_TRUE(WaitForConnected(socket));

    SocketStateBase state;
    int ret = socket->GetState(state);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsConnected());
    socket->Close();
    close(listenFd);
}

/* ==================== GetRemoteAddress / GetLocalAddress Tests ==================== */

HWTEST_F(TcpSocketClientTest, GetRemoteAddress001, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    NetAddress address;
    int ret = socket->GetRemoteAddress(address);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

HWTEST_F(TcpSocketClientTest, GetRemoteAddress002, TestSize.Level1)
{
    int listenFd = MakeTcpListenFd(0);
    ASSERT_GE(listenFd, 0);
    uint16_t realPort = static_cast<uint16_t>(GetListenPort(listenFd));

    auto socket = std::make_shared<TCPSocket>();
    TcpConnectOptions connectOptions;
    connectOptions.address.SetAddress(TEST_ADDRESS);
    connectOptions.address.SetPort(realPort);
    connectOptions.address.SetFamilyBySaFamily(AF_INET);
    connectOptions.SetTimeout(TEST_TIMEOUT_MS);
    ASSERT_EQ(socket->Connect(connectOptions, ProxyOptions{}), SOCKET_ERROR_OK);
    EXPECT_TRUE(WaitForConnected(socket));

    NetAddress address;
    int ret = socket->GetRemoteAddress(address);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_EQ(address.GetPort(), realPort);
    socket->Close();
    close(listenFd);
}

HWTEST_F(TcpSocketClientTest, GetLocalAddress001, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    NetAddress address;
    int ret = socket->GetLocalAddress(address);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

HWTEST_F(TcpSocketClientTest, GetLocalAddress002, TestSize.Level1)
{
    int listenFd = MakeTcpListenFd(0);
    ASSERT_GE(listenFd, 0);
    uint16_t realPort = static_cast<uint16_t>(GetListenPort(listenFd));

    auto socket = std::make_shared<TCPSocket>();
    TcpConnectOptions connectOptions;
    connectOptions.address.SetAddress(TEST_ADDRESS);
    connectOptions.address.SetPort(realPort);
    connectOptions.address.SetFamilyBySaFamily(AF_INET);
    connectOptions.SetTimeout(TEST_TIMEOUT_MS);
    ASSERT_EQ(socket->Connect(connectOptions, ProxyOptions{}), SOCKET_ERROR_OK);
    EXPECT_TRUE(WaitForConnected(socket));

    NetAddress address;
    int ret = socket->GetLocalAddress(address);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
    close(listenFd);
}

/* ==================== SetExtraOptions Tests ==================== */

HWTEST_F(TcpSocketClientTest, SetExtraOptions001, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    TCPExtraOptions options;
    int ret = socket->SetExtraOptions(options);
    /* socket not created yet */
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

HWTEST_F(TcpSocketClientTest, SetExtraOptions002, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    TCPExtraOptions options;
    options.SetKeepAlive(true);
    options.SetKeepAliveFlag(true);
    options.SetTCPNoDelay(true);
    int ret = socket->SetExtraOptions(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
}

HWTEST_F(TcpSocketClientTest, SetExtraOptions003, TestSize.Level1)
{
    int listenFd = MakeTcpListenFd(0);
    ASSERT_GE(listenFd, 0);
    uint16_t realPort = static_cast<uint16_t>(GetListenPort(listenFd));

    auto socket = std::make_shared<TCPSocket>();
    TcpConnectOptions connectOptions;
    connectOptions.address.SetAddress(TEST_ADDRESS);
    connectOptions.address.SetPort(realPort);
    connectOptions.address.SetFamilyBySaFamily(AF_INET);
    connectOptions.SetTimeout(TEST_TIMEOUT_MS);
    ASSERT_EQ(socket->Connect(connectOptions, ProxyOptions{}), SOCKET_ERROR_OK);
    EXPECT_TRUE(WaitForConnected(socket));

    TCPExtraOptions options;
    options.SetOOBInline(true);
    options.SetOobInlineFlag(true);
    int ret = socket->SetExtraOptions(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
    close(listenFd);
}

/* ==================== GetSocketFd Tests ==================== */

HWTEST_F(TcpSocketClientTest, GetSocketFd001, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    EXPECT_NE(socket, nullptr);
    EXPECT_EQ(GetSocketFd(socket), INVALID_FD);
    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsClose());
}

HWTEST_F(TcpSocketClientTest, GetSocketFd002, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    ASSERT_EQ(socket->Bind(address), SOCKET_ERROR_OK);
    EXPECT_GE(GetSocketFd(socket), 0);
    socket->Close();
}

HWTEST_F(TcpSocketClientTest, GetSocketFd003, TestSize.Level1)
{
    int listenFd = MakeTcpListenFd(0);
    ASSERT_GE(listenFd, 0);
    uint16_t realPort = static_cast<uint16_t>(GetListenPort(listenFd));

    auto socket = std::make_shared<TCPSocket>();
    TcpConnectOptions connectOptions;
    connectOptions.address.SetAddress(TEST_ADDRESS);
    connectOptions.address.SetPort(realPort);
    connectOptions.address.SetFamilyBySaFamily(AF_INET);
    connectOptions.SetTimeout(TEST_TIMEOUT_MS);
    ASSERT_EQ(socket->Connect(connectOptions, ProxyOptions{}), SOCKET_ERROR_OK);
    EXPECT_GE(GetSocketFd(socket), 0);
    socket->Close();
    close(listenFd);
}

/* ==================== Message Receive Tests ==================== */

HWTEST_F(TcpSocketClientTest, ReceiveMessage001, TestSize.Level1)
{
    int listenFd = MakeTcpListenFd(0);
    ASSERT_GE(listenFd, 0);
    uint16_t realPort = static_cast<uint16_t>(GetListenPort(listenFd));

    auto socket = std::make_shared<TCPSocket>();
    std::mutex msgMutex;
    std::condition_variable msgCv;
    bool msgReceived = false;
    std::string receivedData;

    socket->OnMessage([&](const std::string &data, const SocketRemoteInfo &remoteInfo) {
        std::lock_guard<std::mutex> lock(msgMutex);
        receivedData = data;
        msgReceived = true;
        (void)remoteInfo;
        msgCv.notify_one();
    });

    TcpConnectOptions connectOptions;
    connectOptions.address.SetAddress(TEST_ADDRESS);
    connectOptions.address.SetPort(realPort);
    connectOptions.address.SetFamilyBySaFamily(AF_INET);
    connectOptions.SetTimeout(TEST_TIMEOUT_MS);
    ASSERT_EQ(socket->Connect(connectOptions, ProxyOptions{}), SOCKET_ERROR_OK);
    EXPECT_TRUE(WaitForConnected(socket));

    int clientFd = accept(listenFd, nullptr, nullptr);
    ASSERT_GE(clientFd, 0);
    ssize_t sent = send(clientFd, TEST_DATA, strlen(TEST_DATA), 0);
    EXPECT_GT(sent, 0);

    {
        std::unique_lock<std::mutex> lock(msgMutex);
        EXPECT_TRUE(msgCv.wait_for(lock, std::chrono::milliseconds(WAIT_TIMEOUT_MS),
            [&msgReceived]() { return msgReceived; }));
    }
    EXPECT_EQ(receivedData, TEST_DATA);

    close(clientFd);
    socket->Close();
    close(listenFd);
}

HWTEST_F(TcpSocketClientTest, ReceiveLargeMessage001, TestSize.Level1)
{
    int listenFd = MakeTcpListenFd(0);
    ASSERT_GE(listenFd, 0);
    uint16_t realPort = static_cast<uint16_t>(GetListenPort(listenFd));

    auto socket = std::make_shared<TCPSocket>();
    std::mutex msgMutex;
    std::condition_variable msgCv;
    bool msgReceived = false;
    std::string receivedData;

    socket->OnMessage([&](const std::string &data, const SocketRemoteInfo &remoteInfo) {
        std::lock_guard<std::mutex> lock(msgMutex);
        receivedData = data;
        msgReceived = true;
        (void)remoteInfo;
        msgCv.notify_one();
    });

    TcpConnectOptions connectOptions;
    connectOptions.address.SetAddress(TEST_ADDRESS);
    connectOptions.address.SetPort(realPort);
    connectOptions.address.SetFamilyBySaFamily(AF_INET);
    connectOptions.SetTimeout(TEST_TIMEOUT_MS);
    ASSERT_EQ(socket->Connect(connectOptions, ProxyOptions{}), SOCKET_ERROR_OK);
    EXPECT_TRUE(WaitForConnected(socket));

    int clientFd = accept(listenFd, nullptr, nullptr);
    ASSERT_GE(clientFd, 0);
    ssize_t sent = send(clientFd, TEST_DATA_LARGE, strlen(TEST_DATA_LARGE), 0);
    EXPECT_GT(sent, 0);

    {
        std::unique_lock<std::mutex> lock(msgMutex);
        EXPECT_TRUE(msgCv.wait_for(lock, std::chrono::milliseconds(WAIT_TIMEOUT_MS),
            [&msgReceived]() { return msgReceived; }));
    }
    EXPECT_EQ(receivedData, TEST_DATA_LARGE);

    close(clientFd);
    socket->Close();
    close(listenFd);
}

/* ==================== Error Callback Tests ==================== */

HWTEST_F(TcpSocketClientTest, ErrorCallback001, TestSize.Level1)
{
    auto socket = std::make_shared<TCPSocket>();
    std::atomic<bool> errorCalled(false);
    socket->OnError([&errorCalled](int32_t errCode, const std::string &errMsg) {
        (void)errCode;
        (void)errMsg;
        errorCalled.store(true);
    });

    TCPSendOptions options;
    options.SetData(TEST_DATA);
    (void)socket->Send(options);

    EXPECT_FALSE(errorCalled.load());
}

} // namespace
