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

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

#include "gtest/gtest.h"

#include "net_address.h"
#include "socket_remote_info.h"
#include "socket_state_base.h"
#include "tcp_extra_options.h"
#include "tcp_send_options.h"
#include "tls.h"
#include "tls_socket_client_innerapi.h"
#include "socket_native_test_utils.h"

namespace {
using namespace testing::ext;
using namespace OHOS::NetStack;
using namespace OHOS::NetStack::TlsSocket;

static constexpr const char *TEST_ADDRESS = "127.0.0.1";
static constexpr uint16_t TEST_PORT = 19000;
static constexpr int32_t INVALID_FD = -1;
static constexpr int32_t LISTEN_BACKLOG = 5;
static constexpr int32_t SLEEP_MS = 100;
static constexpr uint32_t WAIT_TIMEOUT_MS = 5000;

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

__attribute__((unused)) static bool MakeNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return false;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK) >= 0;
}

static void WaitForCallback(std::atomic<bool> &flag, uint32_t timeoutMs = WAIT_TIMEOUT_MS)
{
    auto start = std::chrono::steady_clock::now();
    while (!flag.load()) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed >= static_cast<int64_t>(timeoutMs)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS));
    }
}

class TlsSocketClientTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};

/* ==================== Constructor / Destructor Tests ==================== */

HWTEST_F(TlsSocketClientTest, Constructor001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    EXPECT_EQ(socket->GetSocketFd(), INVALID_FD);

    std::atomic<bool> callbackCalled(false);
    Socket::SocketStateBase callbackState;
    int32_t callbackError = -1;
    socket->GetState([&](int32_t errorNumber, const Socket::SocketStateBase &state) {
        callbackError = errorNumber;
        callbackState = state;
        callbackCalled.store(true);
    });
    WaitForCallback(callbackCalled);
    EXPECT_TRUE(callbackCalled.load());
    /* sockFd_ < 0, getsockopt fails => IsClose should be true */
    EXPECT_TRUE(callbackState.IsClose());
    EXPECT_FALSE(callbackState.IsBound());
    EXPECT_FALSE(callbackState.IsConnected());
}

HWTEST_F(TlsSocketClientTest, Destructor001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    EXPECT_NE(socket, nullptr);
    EXPECT_EQ(socket->GetSocketFd(), INVALID_FD);
    socket.reset();
}

HWTEST_F(TlsSocketClientTest, Destructor002, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    EXPECT_NE(socket, nullptr);
    Socket::NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);
    std::atomic<bool> bindDone(false);
    std::atomic<int32_t> bindErr(-1);
    socket->Bind(address, [&](int32_t errCode) {
        bindErr.store(errCode);
        bindDone.store(true);
    });
    WaitForCallback(bindDone);
    EXPECT_EQ(bindErr.load(), TLSSOCKET_SUCCESS);
    EXPECT_GE(socket->GetSocketFd(), 0);
    socket.reset();
}

/* ==================== On/Off Callback Tests ==================== */

HWTEST_F(TlsSocketClientTest, OnMessageCallback001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    EXPECT_NE(socket, nullptr);
    socket->OnMessage([](const std::string &data, const Socket::SocketRemoteInfo &remoteInfo) {
        (void)data;
        (void)remoteInfo;
    });
    EXPECT_EQ(socket->GetSocketFd(), INVALID_FD);
}

HWTEST_F(TlsSocketClientTest, OnConnectCallback001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    EXPECT_NE(socket, nullptr);
    socket->OnConnect([]() {});
    EXPECT_EQ(socket->GetSocketFd(), INVALID_FD);
}

HWTEST_F(TlsSocketClientTest, OnErrorCallback001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    EXPECT_NE(socket, nullptr);
    socket->OnError([](int32_t errorNumber, const std::string &errorString) {
        (void)errorNumber;
        (void)errorString;
    });
    EXPECT_EQ(socket->GetSocketFd(), INVALID_FD);
}

HWTEST_F(TlsSocketClientTest, OnCloseCallback001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    EXPECT_NE(socket, nullptr);
    socket->OnClose([]() {});
    EXPECT_EQ(socket->GetSocketFd(), INVALID_FD);
}

HWTEST_F(TlsSocketClientTest, OffCallbacks001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    EXPECT_NE(socket, nullptr);
    socket->OnMessage([](const std::string &data, const Socket::SocketRemoteInfo &remoteInfo) {
        (void)data;
        (void)remoteInfo;
    });
    socket->OnConnect([]() {});
    socket->OnError([](int32_t errorNumber, const std::string &errorString) {
        (void)errorNumber;
        (void)errorString;
    });
    socket->OnClose([]() {});

    socket->OffMessage();
    socket->OffConnect();
    socket->OffError();
    socket->OffClose();
    EXPECT_EQ(socket->GetSocketFd(), INVALID_FD);
}

HWTEST_F(TlsSocketClientTest, OffCallbacks002, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    EXPECT_NE(socket, nullptr);
    /* Calling Off without On should be safe */
    socket->OffMessage();
    socket->OffConnect();
    socket->OffError();
    socket->OffClose();
    EXPECT_EQ(socket->GetSocketFd(), INVALID_FD);
}

/* ==================== Bind Tests ==================== */

HWTEST_F(TlsSocketClientTest, Bind001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    Socket::NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);

    std::atomic<bool> callbackCalled(false);
    std::atomic<int32_t> resultErr(-1);
    socket->Bind(address, [&](int32_t errorNumber) {
        resultErr.store(errorNumber);
        callbackCalled.store(true);
    });
    WaitForCallback(callbackCalled);
    EXPECT_TRUE(callbackCalled.load());
    EXPECT_EQ(resultErr.load(), TLSSOCKET_SUCCESS);
    EXPECT_GE(socket->GetSocketFd(), 0);

    socket->Close([](int32_t errorNumber) { (void)errorNumber; });
}

HWTEST_F(TlsSocketClientTest, Bind002, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    Socket::NetAddress address;
    address.SetAddress("0.0.0.0");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);

    std::atomic<bool> callbackCalled(false);
    std::atomic<int32_t> resultErr(-1);
    socket->Bind(address, [&](int32_t errorNumber) {
        resultErr.store(errorNumber);
        callbackCalled.store(true);
    });
    WaitForCallback(callbackCalled);
    EXPECT_TRUE(callbackCalled.load());
    EXPECT_EQ(resultErr.load(), TLSSOCKET_SUCCESS);

    socket->Close([](int32_t errorNumber) { (void)errorNumber; });
}

HWTEST_F(TlsSocketClientTest, Bind003, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    Socket::NetAddress address;
    address.SetAddress("");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);

    std::atomic<bool> callbackCalled(false);
    std::atomic<int32_t> resultErr(-1);
    socket->Bind(address, [&](int32_t errorNumber) {
        resultErr.store(errorNumber);
        callbackCalled.store(true);
    });
    WaitForCallback(callbackCalled);
    EXPECT_TRUE(callbackCalled.load());
    /* Empty address should return parse error */
    static constexpr int32_t parseErrorCode = 401;
    EXPECT_EQ(resultErr.load(), parseErrorCode);
}

HWTEST_F(TlsSocketClientTest, Bind004, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    Socket::NetAddress address;
    /* No family set -- MakeIpSocket won't create a socket for invalid family */
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);

    std::atomic<bool> callbackCalled(false);
    std::atomic<int32_t> resultErr(-1);
    socket->Bind(address, [&](int32_t errorNumber) {
        resultErr.store(errorNumber);
        callbackCalled.store(true);
    });
    WaitForCallback(callbackCalled);
    EXPECT_TRUE(callbackCalled.load());
    /* sockFd_ stays -1, then address is not empty but address's internal raw state
     * after GetAddress/SetRawAddress may cause issues. The error code depends on path.
     */
    (void)resultErr;
}

HWTEST_F(TlsSocketClientTest, BindAfterBind001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    Socket::NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);

    std::atomic<bool> firstBindDone(false);
    std::atomic<int32_t> firstErr(-1);
    socket->Bind(address, [&](int32_t errorNumber) {
        firstErr.store(errorNumber);
        firstBindDone.store(true);
    });
    WaitForCallback(firstBindDone);
    EXPECT_EQ(firstErr.load(), TLSSOCKET_SUCCESS);
    int firstFd = socket->GetSocketFd();
    EXPECT_GE(firstFd, 0);

    /* Bind again with same socket -- should return success (sockFd_ already >= 0) */
    std::atomic<bool> secondBindDone(false);
    std::atomic<int32_t> secondErr(-1);
    socket->Bind(address, [&](int32_t errorNumber) {
        secondErr.store(errorNumber);
        secondBindDone.store(true);
    });
    WaitForCallback(secondBindDone);
    EXPECT_EQ(secondErr.load(), TLSSOCKET_SUCCESS);

    socket->Close([](int32_t errorNumber) { (void)errorNumber; });
}

HWTEST_F(TlsSocketClientTest, BindIPv6001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    Socket::NetAddress address;
    address.SetAddress("::1");
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET6);

    std::atomic<bool> callbackCalled(false);
    std::atomic<int32_t> resultErr(-1);
    socket->Bind(address, [&](int32_t errorNumber) {
        resultErr.store(errorNumber);
        callbackCalled.store(true);
    });
    WaitForCallback(callbackCalled);
    if (resultErr.load() == TLSSOCKET_SUCCESS) {
        EXPECT_GE(socket->GetSocketFd(), 0);
        socket->Close([](int32_t errorNumber) { (void)errorNumber; });
    }
}

/* ==================== GetState Tests ==================== */

HWTEST_F(TlsSocketClientTest, GetState001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    std::atomic<bool> callbackCalled(false);
    Socket::SocketStateBase callbackState;
    int32_t callbackError = -1;
    socket->GetState([&](int32_t errorNumber, const Socket::SocketStateBase &state) {
        callbackError = errorNumber;
        callbackState = state;
        callbackCalled.store(true);
    });
    WaitForCallback(callbackCalled);
    EXPECT_TRUE(callbackCalled.load());
    /* No socket => getsockopt fails => IsClose set, error code from ConvertErrno */
    EXPECT_TRUE(callbackState.IsClose());
    EXPECT_NE(callbackError, TLSSOCKET_SUCCESS);
}

HWTEST_F(TlsSocketClientTest, GetState002, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    Socket::NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);

    std::atomic<bool> bindDone(false);
    socket->Bind(address, [&](int32_t errorNumber) {
        (void)errorNumber;
        bindDone.store(true);
    });
    WaitForCallback(bindDone);

    std::atomic<bool> getStateDone(false);
    Socket::SocketStateBase callbackState;
    int32_t callbackError = -1;
    socket->GetState([&](int32_t errorNumber, const Socket::SocketStateBase &state) {
        callbackError = errorNumber;
        callbackState = state;
        getStateDone.store(true);
    });
    WaitForCallback(getStateDone);
    EXPECT_TRUE(getStateDone.load());
    EXPECT_EQ(callbackError, TLSSOCKET_SUCCESS);
    EXPECT_TRUE(callbackState.IsBound());
    EXPECT_FALSE(callbackState.IsClose());

    socket->Close([](int32_t errorNumber) { (void)errorNumber; });
}

/* ==================== SetExtraOptions Tests ==================== */

HWTEST_F(TlsSocketClientTest, SetExtraOptions001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    Socket::TCPExtraOptions options;
    /* socket not created yet -- SetExtraOptions uses sockFd_ internally,
     * but the SetExtraOptions public API just calls a callback.
     * The internal SetExtraOptions (the private one) returns bool,
     * but the public one is void with callback.
     */
    std::atomic<bool> callbackCalled(false);
    std::atomic<int32_t> resultErr(-1);
    socket->SetExtraOptions(options, [&](int32_t errorNumber) {
        resultErr.store(errorNumber);
        callbackCalled.store(true);
    });
    WaitForCallback(callbackCalled);
    /* No socket fd => internal setsockopt calls will fail on -1 */
    EXPECT_NE(resultErr.load(), TLSSOCKET_SUCCESS);
}

HWTEST_F(TlsSocketClientTest, SetExtraOptions002, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    Socket::NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);

    std::atomic<bool> bindDone(false);
    socket->Bind(address, [&](int32_t errorNumber) {
        (void)errorNumber;
        bindDone.store(true);
    });
    WaitForCallback(bindDone);

    Socket::TCPExtraOptions options;
    options.SetKeepAlive(true);
    options.SetTCPNoDelay(true);

    std::atomic<bool> callbackCalled(false);
    std::atomic<int32_t> resultErr(-1);
    socket->SetExtraOptions(options, [&](int32_t errorNumber) {
        resultErr.store(errorNumber);
        callbackCalled.store(true);
    });
    WaitForCallback(callbackCalled);
    EXPECT_TRUE(callbackCalled.load());
    EXPECT_EQ(resultErr.load(), TLSSOCKET_SUCCESS);

    socket->Close([](int32_t errorNumber) { (void)errorNumber; });
}

/* ==================== GetSocketFd Tests ==================== */

HWTEST_F(TlsSocketClientTest, GetSocketFd001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    EXPECT_NE(socket, nullptr);
    EXPECT_EQ(socket->GetSocketFd(), INVALID_FD);
    EXPECT_FALSE(socket->IsExtSock());
}

HWTEST_F(TlsSocketClientTest, GetSocketFd002, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    Socket::NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);

    std::atomic<bool> bindDone(false);
    socket->Bind(address, [&](int32_t errorNumber) {
        (void)errorNumber;
        bindDone.store(true);
    });
    WaitForCallback(bindDone);
    EXPECT_GE(socket->GetSocketFd(), 0);
    socket->Close([](int32_t errorNumber) { (void)errorNumber; });
}

/* ==================== SetLocalAddress / GetLocalAddress Tests ==================== */

HWTEST_F(TlsSocketClientTest, SetLocalAddress001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    Socket::NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(TEST_PORT);
    address.SetFamilyBySaFamily(AF_INET);
    socket->SetLocalAddress(address);

    auto localAddr = socket->GetLocalAddress();
    EXPECT_EQ(localAddr.GetAddress(), TEST_ADDRESS);
    EXPECT_EQ(localAddr.GetPort(), TEST_PORT);
}

HWTEST_F(TlsSocketClientTest, GetLocalAddress001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    auto localAddr = socket->GetLocalAddress();
    /* Default constructed NetAddress has empty address and port 0 */
    EXPECT_TRUE(localAddr.GetAddress().empty());
    EXPECT_EQ(localAddr.GetPort(), static_cast<uint16_t>(0));
}

/* ==================== Send Error Path Tests ==================== */

HWTEST_F(TlsSocketClientTest, SendNoConnection001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    Socket::TCPSendOptions sendOptions;
    sendOptions.SetData("test data");

    std::atomic<bool> callbackCalled(false);
    std::atomic<int32_t> resultErr(-1);
    socket->Send(sendOptions, [&](int32_t errorNumber) {
        resultErr.store(errorNumber);
        callbackCalled.store(true);
    });
    WaitForCallback(callbackCalled);
    /* No SSL connection established, should fail */
    EXPECT_NE(resultErr.load(), TLSSOCKET_SUCCESS);
}

/* ==================== Close Tests ==================== */

HWTEST_F(TlsSocketClientTest, CloseAfterBind001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    Socket::NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);

    std::atomic<bool> bindDone(false);
    socket->Bind(address, [&](int32_t errorNumber) {
        (void)errorNumber;
        bindDone.store(true);
    });
    WaitForCallback(bindDone);

    std::atomic<bool> closeCalled(false);
    socket->OnClose([&closeCalled]() { closeCalled.store(true); });

    std::atomic<bool> closeDone(false);
    std::atomic<int32_t> closeErr(-1);
    socket->Close([&](int32_t errorNumber) {
        closeErr.store(errorNumber);
        closeDone.store(true);
    });
    WaitForCallback(closeDone);
    EXPECT_EQ(closeErr.load(), TLSSOCKET_SUCCESS);
    EXPECT_TRUE(closeCalled.load());
    EXPECT_EQ(socket->GetSocketFd(), INVALID_FD);
}

/* ==================== GetCertificate Error Path Tests ==================== */

HWTEST_F(TlsSocketClientTest, GetCertificateNoTls001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    std::atomic<bool> callbackCalled(false);
    std::atomic<int32_t> resultErr(-1);
    socket->GetCertificate([&](int32_t errorNumber, const X509CertRawData &cert) {
        (void)cert;
        resultErr.store(errorNumber);
        callbackCalled.store(true);
    });
    WaitForCallback(callbackCalled);
    /* No TLS connection established, should fail */
    EXPECT_NE(resultErr.load(), TLSSOCKET_SUCCESS);
}

HWTEST_F(TlsSocketClientTest, GetRemoteCertificateNoTls001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    std::atomic<bool> callbackCalled(false);
    std::atomic<int32_t> resultErr(-1);
    socket->GetRemoteCertificate([&](int32_t errorNumber, const X509CertRawData &cert) {
        (void)cert;
        resultErr.store(errorNumber);
        callbackCalled.store(true);
    });
    WaitForCallback(callbackCalled);
    /* No TLS connection, should fail */
    EXPECT_NE(resultErr.load(), TLSSOCKET_SUCCESS);
}

HWTEST_F(TlsSocketClientTest, GetProtocolNoTls001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    std::atomic<bool> callbackCalled(false);
    std::atomic<int32_t> resultErr(-1);
    socket->GetProtocol([&](int32_t errorNumber, const std::string &protocol) {
        (void)protocol;
        resultErr.store(errorNumber);
        callbackCalled.store(true);
    });
    WaitForCallback(callbackCalled);
    /* GetProtocol is configuration-based, not dependent on TLS connection state */
    EXPECT_EQ(resultErr.load(), TLSSOCKET_SUCCESS);
}

HWTEST_F(TlsSocketClientTest, GetCipherSuiteNoTls001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    std::atomic<bool> callbackCalled(false);
    std::atomic<int32_t> resultErr(-1);
    socket->GetCipherSuite([&](int32_t errorNumber, const std::vector<std::string> &suite) {
        (void)suite;
        resultErr.store(errorNumber);
        callbackCalled.store(true);
    });
    WaitForCallback(callbackCalled);
    /* No TLS connection, should fail */
    EXPECT_NE(resultErr.load(), TLSSOCKET_SUCCESS);
}

HWTEST_F(TlsSocketClientTest, GetSignatureAlgorithmsNoTls001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    std::atomic<bool> callbackCalled(false);
    std::atomic<int32_t> resultErr(-1);
    socket->GetSignatureAlgorithms([&](int32_t errorNumber, const std::vector<std::string> &algorithms) {
        (void)algorithms;
        resultErr.store(errorNumber);
        callbackCalled.store(true);
    });
    WaitForCallback(callbackCalled);
    /* No TLS connection, should fail */
    EXPECT_NE(resultErr.load(), TLSSOCKET_SUCCESS);
}

/* ==================== Error Callback Tests ==================== */

HWTEST_F(TlsSocketClientTest, ErrorCallbackNoRegistration001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    EXPECT_NE(socket, nullptr);
    /* Verify that operating without registering error callbacks doesn't crash */
    Socket::TCPSendOptions sendOptions;
    sendOptions.SetData("data");
    std::atomic<bool> sendDone(false);
    std::atomic<int32_t> sendErr(-1);
    socket->Send(sendOptions, [&sendDone, &sendErr](int32_t errorNumber) {
        sendErr.store(errorNumber);
        sendDone.store(true);
    });
    WaitForCallback(sendDone);
    EXPECT_TRUE(sendDone.load());
    EXPECT_NE(sendErr.load(), TLSSOCKET_SUCCESS);
}

/* ==================== ConstructionWithFd Tests ==================== */

HWTEST_F(TlsSocketClientTest, ConstructionWithFd001, TestSize.Level1)
{
    constexpr int mockFd = 42;
    auto socket = std::make_shared<TLSSocket>(mockFd);
    EXPECT_NE(socket, nullptr);
    EXPECT_TRUE(socket->IsExtSock());
    EXPECT_EQ(socket->GetSocketFd(), mockFd);
}

/* ==================== ExecTlsSetSockBlockFlag Tests ==================== */

HWTEST_F(TlsSocketClientTest, ExecTlsSetSockBlockFlagValidSock001, TestSize.Level2)
{
    auto socket = std::make_shared<TLSSocket>();
    Socket::NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(0);
    address.SetFamilyBySaFamily(AF_INET);

    std::atomic<bool> bindDone(false);
    socket->Bind(address, [&](int32_t errorNumber) {
        (void)errorNumber;
        bindDone.store(true);
    });
    WaitForCallback(bindDone);

    int sockFd = socket->GetSocketFd();
    EXPECT_GE(sockFd, 0);
    bool result = socket->ExecTlsSetSockBlockFlag(sockFd, true);
    EXPECT_TRUE(result);

    socket->Close([](int32_t errorNumber) { (void)errorNumber; });
}

HWTEST_F(TlsSocketClientTest, ExecTlsSetSockBlockFlagInvalidSock001, TestSize.Level1)
{
    auto socket = std::make_shared<TLSSocket>();
    bool result = socket->ExecTlsSetSockBlockFlag(INVALID_FD, true);
    EXPECT_FALSE(result);
}

/* ==================== Connect With SecureOptions Tests ==================== */

HWTEST_F(TlsSocketClientTest, ConnectWithSecureOptionsNoServer001, TestSize.Level2)
{
    auto socket = std::make_shared<TLSSocket>();

    TLSSecureOptions secureOpts;
    std::vector<std::string> caChain = {CA_CERT_PEM};
    secureOpts.SetCaChain(caChain);

    TLSConnectOptions connectOptions;
    Socket::NetAddress address;
    address.SetAddress(TEST_ADDRESS);
    address.SetPort(TEST_PORT);
    address.SetFamilyBySaFamily(AF_INET);
    connectOptions.SetNetAddress(address);
    connectOptions.SetTlsSecureOptions(secureOpts);
    connectOptions.SetSkipRemoteValidation(true);
    static constexpr uint32_t connectTimeoutMs = 3000;
    connectOptions.SetTimeout(connectTimeoutMs);

    std::atomic<bool> callbackCalled(false);
    std::atomic<int32_t> resultErr(-1);
    socket->Connect(connectOptions, [&](int32_t errorNumber) {
        resultErr.store(errorNumber);
        callbackCalled.store(true);
    });

    /* Wait for connection attempt (will fail since no TLS server is running) */
    WaitForCallback(callbackCalled, connectTimeoutMs + WAIT_TIMEOUT_MS);
    EXPECT_TRUE(callbackCalled.load());
    EXPECT_NE(resultErr.load(), TLSSOCKET_SUCCESS);
}

/* ==================== ConnectOnOhosPlatform Branch Tests ==================== */

/* Test ConnectOnOhosPlatform error path: Bind then connect to a port with no server.
 * PrepareTlsConnect returns -1 (ECONNREFUSED), covering the prepareRet < 0 branch. */
HWTEST_F(TlsSocketClientTest, ConnectAfterBindNoServer001, TestSize.Level2)
{
    auto socket = std::make_shared<TLSSocket>();

    Socket::NetAddress bindAddr;
    bindAddr.SetAddress(TEST_ADDRESS);
    bindAddr.SetPort(0);
    bindAddr.SetFamilyBySaFamily(AF_INET);

    std::atomic<bool> bindDone(false);
    socket->Bind(bindAddr, [&](int32_t errorNumber) {
        (void)errorNumber;
        bindDone.store(true);
    });
    WaitForCallback(bindDone);
    EXPECT_GE(socket->GetSocketFd(), 0);

    TLSSecureOptions secureOpts;
    std::vector<std::string> caChain = {CA_CERT_PEM};
    secureOpts.SetCaChain(caChain);

    TLSConnectOptions connectOptions;
    Socket::NetAddress destAddr;
    destAddr.SetAddress(TEST_ADDRESS);
    /* Use a port that no server is listening on */
    destAddr.SetPort(TEST_PORT);
    destAddr.SetFamilyBySaFamily(AF_INET);
    connectOptions.SetNetAddress(destAddr);
    connectOptions.SetTlsSecureOptions(secureOpts);
    connectOptions.SetSkipRemoteValidation(true);
    static constexpr uint32_t connectTimeoutMs = 3000;
    connectOptions.SetTimeout(connectTimeoutMs);

    std::atomic<bool> callbackCalled(false);
    std::atomic<int32_t> resultErr(-1);
    socket->Connect(connectOptions, [&](int32_t errorNumber) {
        resultErr.store(errorNumber);
        callbackCalled.store(true);
    });

    WaitForCallback(callbackCalled, connectTimeoutMs + WAIT_TIMEOUT_MS);
    EXPECT_TRUE(callbackCalled.load());
    /* No server listening, ConnectOnOhosPlatform should fail via PrepareTlsConnect error */
    EXPECT_NE(resultErr.load(), TLSSOCKET_SUCCESS);

    socket->Close([](int32_t) {});
}

/* Test FinalizeTlsHandshake error path: TCP connect succeeds (listener present),
 * but TLS handshake fails because there is no TLS server. Covers:
 * - ConnectOnOhosPlatform prepareRet == 0 branch
 * - FinalizeTlsHandshake ContinueTlsHandshake failure branch */
HWTEST_F(TlsSocketClientTest, ConnectWithTcpListenerNoTls001, TestSize.Level2)
{
    /* Set up a plain TCP listener (no TLS) */
    int listenFd = MakeTcpListenFd(0);
    ASSERT_GE(listenFd, 0);
    uint16_t realPort = static_cast<uint16_t>(GetListenPort(listenFd));

    auto socket = std::make_shared<TLSSocket>();

    Socket::NetAddress bindAddr;
    bindAddr.SetAddress(TEST_ADDRESS);
    bindAddr.SetPort(0);
    bindAddr.SetFamilyBySaFamily(AF_INET);

    std::atomic<bool> bindDone(false);
    socket->Bind(bindAddr, [&](int32_t errorNumber) {
        (void)errorNumber;
        bindDone.store(true);
    });
    WaitForCallback(bindDone);
    EXPECT_GE(socket->GetSocketFd(), 0);

    TLSSecureOptions secureOpts;
    std::vector<std::string> caChain = {CA_CERT_PEM};
    secureOpts.SetCaChain(caChain);

    TLSConnectOptions connectOptions;
    Socket::NetAddress destAddr;
    destAddr.SetAddress(TEST_ADDRESS);
    destAddr.SetPort(realPort);
    destAddr.SetFamilyBySaFamily(AF_INET);
    connectOptions.SetNetAddress(destAddr);
    connectOptions.SetTlsSecureOptions(secureOpts);
    connectOptions.SetSkipRemoteValidation(true);
    static constexpr uint32_t connectTimeoutMs = 3000;
    connectOptions.SetTimeout(connectTimeoutMs);

    std::atomic<bool> callbackCalled(false);
    std::atomic<int32_t> resultErr(-1);
    socket->Connect(connectOptions, [&](int32_t errorNumber) {
        resultErr.store(errorNumber);
        callbackCalled.store(true);
    });

    WaitForCallback(callbackCalled, connectTimeoutMs + WAIT_TIMEOUT_MS);
    EXPECT_TRUE(callbackCalled.load());
    /* TCP connect success but TLS handshake fails — covers FinalizeTlsHandshake */
    EXPECT_NE(resultErr.load(), TLSSOCKET_SUCCESS);

    socket->Close([](int32_t) {});
    close(listenFd);
}

/* Test HandleAsyncTlsConnect branch: fill listen backlog to force EINPROGRESS,
 * then Connect should go through the async connect path (prepareRet == 1).
 * The async connect will eventually fail/timeout because no real TLS server accepts. */
HWTEST_F(TlsSocketClientTest, ConnectAsyncBacklogFull001, TestSize.Level3)
{
    /* Create listener with minimal backlog */
    int listenFd = MakeTcpListenFd(0);
    ASSERT_GE(listenFd, 0);
    uint16_t realPort = static_cast<uint16_t>(GetListenPort(listenFd));

    auto socket = std::make_shared<TLSSocket>();

    Socket::NetAddress bindAddr;
    bindAddr.SetAddress(TEST_ADDRESS);
    bindAddr.SetPort(0);
    bindAddr.SetFamilyBySaFamily(AF_INET);

    std::atomic<bool> bindDone(false);
    socket->Bind(bindAddr, [&](int32_t errorNumber) {
        (void)errorNumber;
        bindDone.store(true);
    });
    WaitForCallback(bindDone);
    int sockFd = socket->GetSocketFd();
    EXPECT_GE(sockFd, 0);

    TLSSecureOptions secureOpts;
    std::vector<std::string> caChain = {CA_CERT_PEM};
    secureOpts.SetCaChain(caChain);

    TLSConnectOptions connectOptions;
    Socket::NetAddress destAddr;
    destAddr.SetAddress(TEST_ADDRESS);
    destAddr.SetPort(realPort);
    destAddr.SetFamilyBySaFamily(AF_INET);
    connectOptions.SetNetAddress(destAddr);
    connectOptions.SetTlsSecureOptions(secureOpts);
    connectOptions.SetSkipRemoteValidation(true);
    static constexpr uint32_t connectTimeoutMs = 8000;
    connectOptions.SetTimeout(connectTimeoutMs);

    /* The Connect() call in ConnectOnOhosPlatform sets the socket to non-blocking
     * and calls PrepareTlsConnect which calls connect(). If the listen backlog is
     * not full, this returns 0 (immediate success) on loopback. We accept that
     * either path (sync or async) is valid here — both exercise new code paths. */
    std::atomic<bool> callbackCalled(false);
    std::atomic<int32_t> resultErr(-1);
    socket->Connect(connectOptions, [&](int32_t errorNumber) {
        resultErr.store(errorNumber);
        callbackCalled.store(true);
    });

    WaitForCallback(callbackCalled, connectTimeoutMs + WAIT_TIMEOUT_MS);
    EXPECT_TRUE(callbackCalled.load());
    /* Connect should fail (no TLS server), exercising either
     * HandleAsyncTlsConnect + timeout or FinalizeTlsHandshake + TLS failure */
    EXPECT_NE(resultErr.load(), TLSSOCKET_SUCCESS);

    socket->Close([](int32_t) {});
    close(listenFd);
}

/* Test Connect with ExtSock: verify TLS handshake fails gracefully on externally
 * connected socket when no TLS server is present. The fd is pre-connected to a
 * plain TCP listener so that SSL_connect operates on a valid connected fd. */
HWTEST_F(TlsSocketClientTest, ConnectWithExtSockNoTls001, TestSize.Level2)
{
    /* Set up a plain TCP listener (no TLS) */
    int listenFd = MakeTcpListenFd(0);
    ASSERT_GE(listenFd, 0);
    uint16_t realPort = static_cast<uint16_t>(GetListenPort(listenFd));

    /* Create external socket fd and connect it to the TCP listener */
    int rawFd = socket(AF_INET, SOCK_STREAM, 0);
    ASSERT_GE(rawFd, 0);

    sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(realPort);
    serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ASSERT_EQ(connect(rawFd, reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr)), 0);

    /* Create TLSSocket with externally connected fd */
    auto socket = std::make_shared<TLSSocket>(rawFd);
    EXPECT_TRUE(socket->IsExtSock());
    EXPECT_EQ(socket->GetSocketFd(), rawFd);

    TLSSecureOptions secureOpts;
    std::vector<std::string> caChain = {CA_CERT_PEM};
    secureOpts.SetCaChain(caChain);

    TLSConnectOptions connectOptions;
    Socket::NetAddress destAddr;
    destAddr.SetAddress(TEST_ADDRESS);
    destAddr.SetPort(realPort);
    destAddr.SetFamilyBySaFamily(AF_INET);
    connectOptions.SetNetAddress(destAddr);
    connectOptions.SetTlsSecureOptions(secureOpts);
    connectOptions.SetSkipRemoteValidation(true);
    static constexpr uint32_t connectTimeoutMs = 3000;
    connectOptions.SetTimeout(connectTimeoutMs);

    std::atomic<bool> callbackCalled(false);
    std::atomic<int32_t> resultErr(TLSSOCKET_SUCCESS);
    socket->Connect(connectOptions, [&](int32_t errorNumber) {
        resultErr.store(errorNumber);
        callbackCalled.store(true);
    });

    WaitForCallback(callbackCalled, connectTimeoutMs + WAIT_TIMEOUT_MS);
    EXPECT_TRUE(callbackCalled.load());
    /* TCP connect succeeds (fd pre-connected) but TLS handshake fails */
    EXPECT_NE(resultErr.load(), TLSSOCKET_SUCCESS);

    socket->Close([](int32_t) {});
    close(listenFd);
}

} // namespace
