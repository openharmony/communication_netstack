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
#include <mutex>
#include <string>
#include <thread>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "gtest/gtest.h"

#include "local_socket_options.h"
#include "socket_exec_common.h"
#include "socket_exec_error.h"
#include "socket_state_base.h"
#include "socket_constant.h"

#define private public
#define protected public
#include "local_socket_server_innerapi.h"
#undef private
#undef protected

#include "securec.h"

using namespace testing::ext;
using namespace OHOS::NetStack::Socket;

class LocalSocketServerTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};

namespace {

static constexpr int32_t INVALID_FD = -1;
static constexpr int32_t TEST_CLIENT_ID = 100;
static constexpr int32_t SLEEP_MS = 100;
static constexpr const char *TEST_DATA = "hello local server";
static constexpr const char *TEST_DATA_LARGE =
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789";
static constexpr uint32_t MAX_SOCKET_BUFFER_SIZE = 262144;
static constexpr uint32_t DEFAULT_BUFFER_SIZE = 8192;

static std::string MakeTempSocketPath()
{
    char tmpl[] = "/tmp/local_socket_srv_ut_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd >= 0) {
        close(fd);
        unlink(tmpl);
    }
    return std::string(tmpl);
}

static std::shared_ptr<LocalSocketConnection> MakeConnection(int32_t clientId, int32_t fd)
{
    auto conn = std::make_shared<LocalSocketConnection>(clientId);
    conn->SetSocketFd(fd);
    return conn;
}

/* ==================== LocalSocketConnection Tests ==================== */

HWTEST_F(LocalSocketServerTest, ConnectionConstructor001, TestSize.Level1)
{
    auto conn = MakeConnection(TEST_CLIENT_ID, INVALID_FD);
    EXPECT_EQ(conn->GetClientId(), TEST_CLIENT_ID);
    {
        int32_t fd = -1;
        EXPECT_EQ(conn->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_EQ(fd, INVALID_FD);
    }
}

HWTEST_F(LocalSocketServerTest, ConnectionConstructor002, TestSize.Level1)
{
    int sv[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
    auto conn = MakeConnection(TEST_CLIENT_ID, sv[0]);
    EXPECT_EQ(conn->GetClientId(), TEST_CLIENT_ID);
    {
        int32_t fd = -1;
        EXPECT_EQ(conn->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_EQ(fd, sv[0]);
    }
    close(sv[0]);
    close(sv[1]);
}

HWTEST_F(LocalSocketServerTest, ConnectionOnMessage001, TestSize.Level1)
{
    auto conn = MakeConnection(TEST_CLIENT_ID, INVALID_FD);
    LocalSocketConnectionOnMessageCallback msgCb = [](const std::string &, const std::string &, const size_t) {};
    conn->OnMessage(msgCb);
    EXPECT_NE(conn->onMessageCallback_, nullptr);
    conn->OffMessage();
    EXPECT_EQ(conn->onMessageCallback_, nullptr);
}

HWTEST_F(LocalSocketServerTest, ConnectionOnClose001, TestSize.Level1)
{
    auto conn = MakeConnection(TEST_CLIENT_ID, INVALID_FD);
    LocalSocketConnectionOnCloseCallback closeCb = []() {};
    conn->OnClose(closeCb);
    EXPECT_NE(conn->onCloseCallback_, nullptr);
    conn->OffClose();
    EXPECT_EQ(conn->onCloseCallback_, nullptr);
}

HWTEST_F(LocalSocketServerTest, ConnectionOnError001, TestSize.Level1)
{
    auto conn = MakeConnection(TEST_CLIENT_ID, INVALID_FD);
    LocalSocketConnectionOnErrorCallback errCb = [](int32_t, const std::string &) {};
    conn->OnError(errCb);
    EXPECT_NE(conn->onErrorCallback_, nullptr);
    conn->OffError();
    EXPECT_EQ(conn->onErrorCallback_, nullptr);
}

HWTEST_F(LocalSocketServerTest, ConnectionSend001, TestSize.Level1)
{
    /* Send on invalid fd should return ERRNO_BAD_FD */
    auto conn = MakeConnection(TEST_CLIENT_ID, INVALID_FD);
    LocalSocketOptions options;
    options.SetBuffer(TEST_DATA);
    int ret = conn->Send(options);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

HWTEST_F(LocalSocketServerTest, ConnectionSend002, TestSize.Level1)
{
    /* Send empty buffer should return PARAM_ERROR_CODE */
    int sv[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
    auto conn = MakeConnection(TEST_CLIENT_ID, sv[0]);
    LocalSocketOptions options;
    options.SetBuffer("");
    int ret = conn->Send(options);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);
    close(sv[0]);
    close(sv[1]);
}

HWTEST_F(LocalSocketServerTest, ConnectionSend003, TestSize.Level1)
{
    /* Successful send, verify data received on the other end */
    int sv[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
    auto conn = MakeConnection(TEST_CLIENT_ID, sv[0]);
    LocalSocketOptions options;
    options.SetBuffer(TEST_DATA);
    int ret = conn->Send(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);

    char buf[64] = {0};
    ssize_t recvLen = recv(sv[1], buf, sizeof(buf), 0);
    EXPECT_EQ(recvLen, static_cast<ssize_t>(strlen(TEST_DATA)));
    EXPECT_STREQ(buf, TEST_DATA);
    close(sv[0]);
    close(sv[1]);
}

HWTEST_F(LocalSocketServerTest, ConnectionSend004, TestSize.Level1)
{
    /* Send large data */
    int sv[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
    auto conn = MakeConnection(TEST_CLIENT_ID, sv[0]);
    LocalSocketOptions options;
    options.SetBuffer(TEST_DATA_LARGE);
    int ret = conn->Send(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);

    char buf[256] = {0};
    ssize_t totalRecv = 0;
    while (totalRecv < static_cast<ssize_t>(strlen(TEST_DATA_LARGE))) {
        ssize_t n = recv(sv[1], buf + totalRecv, sizeof(buf) - totalRecv - 1, 0);
        if (n <= 0) {
            break;
        }
        totalRecv += n;
    }
    EXPECT_EQ(totalRecv, static_cast<ssize_t>(strlen(TEST_DATA_LARGE)));
    close(sv[0]);
    close(sv[1]);
}

HWTEST_F(LocalSocketServerTest, ConnectionClose001, TestSize.Level1)
{
    /* Close on invalid fd should return ERRNO_BAD_FD */
    auto conn = MakeConnection(TEST_CLIENT_ID, INVALID_FD);
    int ret = conn->Close();
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

HWTEST_F(LocalSocketServerTest, ConnectionClose002, TestSize.Level1)
{
    int sv[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
    auto conn = MakeConnection(TEST_CLIENT_ID, sv[0]);
    int ret = conn->Close();
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    {
        int32_t fd = -1;
        EXPECT_EQ(conn->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_EQ(fd, INVALID_FD);
    }
    close(sv[1]);
}

HWTEST_F(LocalSocketServerTest, ConnectionClose003, TestSize.Level1)
{
    /* Double close: second close should return ERRNO_BAD_FD */
    int sv[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
    auto conn = MakeConnection(TEST_CLIENT_ID, sv[0]);
    ASSERT_EQ(conn->Close(), SOCKET_ERROR_OK);
    int ret = conn->Close();
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
    close(sv[1]);
}

HWTEST_F(LocalSocketServerTest, ConnectionClose004, TestSize.Level1)
{
    /* Close should trigger onCloseCallback */
    int sv[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
    std::atomic<bool> closeCalled(false);
    auto conn = MakeConnection(TEST_CLIENT_ID, sv[0]);
    conn->OnClose([&closeCalled]() { closeCalled.store(true); });
    ASSERT_EQ(conn->Close(), SOCKET_ERROR_OK);
    EXPECT_TRUE(closeCalled.load());
    close(sv[1]);
}

HWTEST_F(LocalSocketServerTest, ConnectionGetSocketFd001, TestSize.Level1)
{
    auto conn = MakeConnection(TEST_CLIENT_ID, INVALID_FD);
    {
        int32_t fd = -1;
        EXPECT_EQ(conn->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_EQ(fd, INVALID_FD);
    }
}

HWTEST_F(LocalSocketServerTest, ConnectionGetSocketFd002, TestSize.Level1)
{
    int sv[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
    auto conn = MakeConnection(TEST_CLIENT_ID, sv[0]);
    {
        int32_t fd = -1;
        EXPECT_EQ(conn->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_EQ(fd, sv[0]);
    }
    close(sv[0]);
    close(sv[1]);
}

HWTEST_F(LocalSocketServerTest, ConnectionGetClientId001, TestSize.Level1)
{
    auto conn = MakeConnection(TEST_CLIENT_ID, INVALID_FD);
    EXPECT_NE(conn, nullptr);
    EXPECT_EQ(conn->GetClientId(), TEST_CLIENT_ID);
    int32_t fd = -1;
    EXPECT_EQ(conn->GetSocketFd(fd), SOCKET_ERROR_OK);
    EXPECT_EQ(fd, INVALID_FD);
}

HWTEST_F(LocalSocketServerTest, ConnectionGetClientId002, TestSize.Level1)
{
    auto conn = MakeConnection(200, INVALID_FD);
    EXPECT_NE(conn, nullptr);
    EXPECT_EQ(conn->GetClientId(), 200);
    int32_t fd = -1;
    EXPECT_EQ(conn->GetSocketFd(fd), SOCKET_ERROR_OK);
    EXPECT_EQ(fd, INVALID_FD);
}

HWTEST_F(LocalSocketServerTest, ConnectionGetLocalAddress001, TestSize.Level1)
{
    auto conn = MakeConnection(TEST_CLIENT_ID, INVALID_FD);
    std::string path;
    int ret = conn->GetLocalAddress(path);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

HWTEST_F(LocalSocketServerTest, ConnectionGetLocalAddress002, TestSize.Level1)
{
    int sv[2] = {-1, -1};
    ASSERT_EQ(socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);
    auto conn = MakeConnection(TEST_CLIENT_ID, sv[0]);
    std::string path;
    int ret = conn->GetLocalAddress(path);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    close(sv[0]);
    close(sv[1]);
}

/* ==================== LocalSocketServer Tests ==================== */

HWTEST_F(LocalSocketServerTest, ServerConstructor001, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    {
        int32_t fd = -1;
        EXPECT_EQ(server->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_EQ(fd, INVALID_FD);
    }
    SocketStateBase state;
    EXPECT_EQ(server->GetState(state), SOCKET_ERROR_OK);
    EXPECT_FALSE(state.IsBound());
    EXPECT_FALSE(state.IsConnected());
    EXPECT_FALSE(state.IsClose());
}

HWTEST_F(LocalSocketServerTest, ServerConstructor002, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    EXPECT_EQ(server->onConnectCallback_, nullptr);
    EXPECT_EQ(server->onErrorCallback_, nullptr);
}

HWTEST_F(LocalSocketServerTest, ServerDestructor001, TestSize.Level1)
{
    {
        auto server = std::make_shared<LocalSocketServer>();
        EXPECT_NE(server, nullptr);
        int32_t fd = -1;
        EXPECT_EQ(server->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_EQ(fd, INVALID_FD);
    }
    SUCCEED();
}

HWTEST_F(LocalSocketServerTest, ServerDestructor002, TestSize.Level1)
{
    /* Destructor should close a listening server */
    std::string path = MakeTempSocketPath();
    {
        auto server = std::make_shared<LocalSocketServer>();
        ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);
    }
    unlink(path.c_str());
    SUCCEED();
}

HWTEST_F(LocalSocketServerTest, ServerOnConnect001, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    LocalSocketServerOnConnectCallback connectCb = [](const int &) {};
    server->OnConnect(connectCb);
    EXPECT_NE(server->onConnectCallback_, nullptr);
    server->OffConnect();
    EXPECT_EQ(server->onConnectCallback_, nullptr);
}

HWTEST_F(LocalSocketServerTest, ServerOnError001, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    LocalSocketServerOnErrorCallback errCb = [](int32_t, const std::string &) {};
    server->OnError(errCb);
    EXPECT_NE(server->onErrorCallback_, nullptr);
    server->OffError();
    EXPECT_EQ(server->onErrorCallback_, nullptr);
}

HWTEST_F(LocalSocketServerTest, ServerListen001, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    int ret = server->Listen("");
    EXPECT_EQ(ret, PARAM_ERROR_CODE);
}

HWTEST_F(LocalSocketServerTest, ServerListen002, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    int ret = server->Listen(path);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    {
        int32_t fd = -1;
        EXPECT_EQ(server->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_GE(fd, 0);
    }
    server->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, ServerListen003, TestSize.Level1)
{
    /* Double listen should fail with PARAM_ERROR_CODE */
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);
    std::string path2 = MakeTempSocketPath();
    int ret = server->Listen(path2);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);
    server->Close();
    unlink(path.c_str());
    unlink(path2.c_str());
}

HWTEST_F(LocalSocketServerTest, ServerListen004, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);
    SocketStateBase state;
    EXPECT_EQ(server->GetState(state), SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsBound());
    EXPECT_FALSE(state.IsConnected());
    server->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, ServerListen005, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);
    std::string localPath;
    int ret = server->GetLocalAddress(localPath);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_EQ(localPath, path);
    server->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, ServerClose001, TestSize.Level1)
{
    /* Close on a fresh server (not listening) should return SOCKET_ERROR_OK */
    auto server = std::make_shared<LocalSocketServer>();
    int ret = server->Close();
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
}

HWTEST_F(LocalSocketServerTest, ServerClose002, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);
    int ret = server->Close();
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    {
        int32_t fd = -1;
        EXPECT_EQ(server->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_EQ(fd, INVALID_FD);
    }
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, ServerClose003, TestSize.Level1)
{
    /* Double close: second close should return SOCKET_ERROR_OK (already closed) */
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);
    ASSERT_EQ(server->Close(), SOCKET_ERROR_OK);
    int ret = server->Close();
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, ServerGetState001, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    SocketStateBase state;
    int ret = server->GetState(state);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_FALSE(state.IsBound());
}

HWTEST_F(LocalSocketServerTest, ServerGetState002, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);
    SocketStateBase state;
    EXPECT_EQ(server->GetState(state), SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsBound());
    EXPECT_FALSE(state.IsConnected());
    server->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, ServerGetState003, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);
    server->Close();
    SocketStateBase state;
    EXPECT_EQ(server->GetState(state), SOCKET_ERROR_OK);
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, ServerGetSocketFd001, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    {
        int32_t fd = -1;
        EXPECT_EQ(server->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_EQ(fd, INVALID_FD);
    }
}

HWTEST_F(LocalSocketServerTest, ServerGetSocketFd002, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);
    {
        int32_t fd = -1;
        EXPECT_EQ(server->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_GE(fd, 0);
    }
    server->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, ServerGetSocketFd003, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);
    server->Close();
    {
        int32_t fd = -1;
        EXPECT_EQ(server->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_EQ(fd, INVALID_FD);
    }
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, ServerGetLocalAddress001, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    std::string path;
    int ret = server->GetLocalAddress(path);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

HWTEST_F(LocalSocketServerTest, ServerGetLocalAddress002, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);
    std::string localPath;
    int ret = server->GetLocalAddress(localPath);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_EQ(localPath, path);
    server->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, ServerGetLocalAddress003, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);
    server->Close();
    std::string localPath;
    int ret = server->GetLocalAddress(localPath);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, ServerSetExtraOptions001, TestSize.Level1)
{
    /* SetExtraOptions on a fresh server (not listening) should return ERRNO_BAD_FD */
    auto server = std::make_shared<LocalSocketServer>();
    LocalExtraOptions options;
    int ret = server->SetExtraOptions(options);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

HWTEST_F(LocalSocketServerTest, ServerSetExtraOptions002, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);

    LocalExtraOptions options;
    options.SetReceiveBufferSize(DEFAULT_BUFFER_SIZE);
    options.SetRecvBufSizeFlag(true);
    int ret = server->SetExtraOptions(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);

    server->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, ServerSetExtraOptions003, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);

    LocalExtraOptions options;
    options.SetSendBufferSize(DEFAULT_BUFFER_SIZE);
    options.SetSendBufSizeFlag(true);
    int ret = server->SetExtraOptions(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);

    server->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, ServerSetExtraOptions004, TestSize.Level1)
{
    /* Buffer size exceeding MAX_SOCKET_BUFFER_SIZE should return PARAM_ERROR_CODE */
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);

    LocalExtraOptions options;
    options.SetReceiveBufferSize(MAX_SOCKET_BUFFER_SIZE + 1);
    int ret = server->SetExtraOptions(options);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);

    server->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, ServerSetExtraOptions005, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);
    server->Close();

    LocalExtraOptions options;
    int ret = server->SetExtraOptions(options);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, ServerSetExtraOptions006, TestSize.Level1)
{
    /* Set all valid options together */
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);

    LocalExtraOptions options;
    options.SetReceiveBufferSize(DEFAULT_BUFFER_SIZE);
    options.SetRecvBufSizeFlag(true);
    options.SetSendBufferSize(DEFAULT_BUFFER_SIZE * 2);
    options.SetSendBufSizeFlag(true);
    options.SetSocketTimeout(2000);
    options.SetTimeoutFlag(true);
    int ret = server->SetExtraOptions(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);

    server->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, ServerSetExtraOptions007, TestSize.Level1)
{
    /* Send buffer size exceeding MAX_SOCKET_BUFFER_SIZE should return PARAM_ERROR_CODE */
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);

    LocalExtraOptions options;
    options.SetSendBufferSize(MAX_SOCKET_BUFFER_SIZE + 1);
    int ret = server->SetExtraOptions(options);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);

    server->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, ServerGetExtraOptions001, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    LocalExtraOptions options;
    int ret = server->GetExtraOptions(options);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

HWTEST_F(LocalSocketServerTest, ServerGetExtraOptions002, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);

    LocalExtraOptions options;
    int ret = server->GetExtraOptions(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);

    server->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, ServerGetExtraOptions003, TestSize.Level1)
{
    /* GetExtraOptions should return previously set options */
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);

    LocalExtraOptions setOptions;
    setOptions.SetReceiveBufferSize(DEFAULT_BUFFER_SIZE);
    setOptions.SetRecvBufSizeFlag(true);
    setOptions.SetSendBufferSize(DEFAULT_BUFFER_SIZE * 2);
    setOptions.SetSendBufSizeFlag(true);
    ASSERT_EQ(server->SetExtraOptions(setOptions), SOCKET_ERROR_OK);

    LocalExtraOptions getOptions;
    int ret = server->GetExtraOptions(getOptions);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_EQ(getOptions.GetReceiveBufferSize(), DEFAULT_BUFFER_SIZE);
    EXPECT_EQ(getOptions.GetSendBufferSize(), DEFAULT_BUFFER_SIZE * 2);

    server->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, ServerGetExtraOptions004, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);
    server->Close();

    LocalExtraOptions options;
    int ret = server->GetExtraOptions(options);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, ServerGenerateClientId001, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    int id1 = server->GenerateClientId();
    int id2 = server->GenerateClientId();
    EXPECT_EQ(id1, 1);
    EXPECT_EQ(id2, 2);
}

HWTEST_F(LocalSocketServerTest, ServerGenerateClientId002, TestSize.Level1)
{
    auto server = std::make_shared<LocalSocketServer>();
    for (int i = 0; i < 10; ++i) {
        server->GenerateClientId();
    }
    EXPECT_EQ(server->GenerateClientId(), 11);
}

/* ==================== OnConnect Detailed Tests ==================== */

HWTEST_F(LocalSocketServerTest, ServerOnConnect002, TestSize.Level1)
{
    /* Verify server reports correct clientId and the connection is retrievable */
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();

    std::atomic<int> reportedClientId(-1);
    std::atomic<bool> connValid(false);
    server->OnConnect([&server, &reportedClientId, &connValid](const int &clientId) {
        reportedClientId.store(clientId);
        auto conn = server->GetConnectionByClientID(clientId);
        if (conn != nullptr) {
            connValid.store(true);
            conn->OnError([](int32_t, const std::string &) {});
            conn->OnClose([]() {});
        }
    });
    server->OnError([](int32_t, const std::string &) {});

    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);

    int clientFd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_GE(clientFd, 0);
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    (void)strncpy_s(addr.sun_path, sizeof(addr.sun_path), path.c_str(), sizeof(addr.sun_path) - 1);
    ASSERT_EQ(connect(clientFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS * 2));
    EXPECT_GT(reportedClientId.load(), 0);
    EXPECT_TRUE(connValid.load());

    close(clientFd);
    server->Close();
    unlink(path.c_str());
}

/* ==================== Integration Tests ==================== */

HWTEST_F(LocalSocketServerTest, Integration001, TestSize.Level1)
{
    /* Server listens, a raw client connects, verify onConnectCallback is invoked */
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();

    std::atomic<bool> connectCalled(false);
    std::atomic<int> reportedClientId(-1);
    server->OnConnect([&connectCalled, &reportedClientId](const int &clientId) {
        reportedClientId.store(clientId);
        connectCalled.store(true);
    });
    server->OnError([](int32_t, const std::string &) {});

    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);

    /* Raw client connect */
    int clientFd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_GE(clientFd, 0);
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    (void)strncpy_s(addr.sun_path, sizeof(addr.sun_path), path.c_str(), sizeof(addr.sun_path) - 1);
    ASSERT_EQ(connect(clientFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS * 2));
    EXPECT_TRUE(connectCalled.load());
    EXPECT_GT(reportedClientId.load(), 0);

    close(clientFd);
    server->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, Integration002, TestSize.Level1)
{
    /* Server listens, client connects and sends data, server receives via callback */
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();

    std::atomic<bool> messageReceived(false);
    std::string receivedData;
    std::mutex dataMutex;
    auto onConnMessage = [&receivedData, &messageReceived, &dataMutex](const std::string &data,
                                                                       const std::string &, const size_t length) {
        std::lock_guard<std::mutex> lock(dataMutex);
        receivedData.assign(data, 0, length);
        messageReceived.store(true);
    };

    server->OnConnect([&server, &onConnMessage](const int &clientId) {
        auto conn = server->GetConnectionByClientID(clientId);
        if (conn != nullptr) {
            conn->OnMessage(onConnMessage);
            conn->OnError([](int32_t, const std::string &) {});
            conn->OnClose([]() {});
        }
    });
    server->OnError([](int32_t, const std::string &) {});

    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);

    int clientFd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_GE(clientFd, 0);
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    (void)strncpy_s(addr.sun_path, sizeof(addr.sun_path), path.c_str(), sizeof(addr.sun_path) - 1);
    ASSERT_EQ(connect(clientFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)), 0);

    /* Wait for server to accept the connection */
    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS * 2));

    /* Client sends data */
    std::string msg = "client_to_server";
    ssize_t sent = send(clientFd, msg.c_str(), msg.size(), 0);
    EXPECT_EQ(sent, static_cast<ssize_t>(msg.size()));

    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS * 3));
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        EXPECT_TRUE(messageReceived.load());
        EXPECT_EQ(receivedData, msg);
    }

    close(clientFd);
    server->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketServerTest, Integration003, TestSize.Level1)
{
    /* Verify state reflects active connections */
    auto server = std::make_shared<LocalSocketServer>();
    std::string path = MakeTempSocketPath();
    server->OnConnect([](const int &) {});
    server->OnError([](int32_t, const std::string &) {});
    ASSERT_EQ(server->Listen(path), SOCKET_ERROR_OK);

    SocketStateBase stateBefore;
    EXPECT_EQ(server->GetState(stateBefore), SOCKET_ERROR_OK);
    EXPECT_FALSE(stateBefore.IsConnected());

    int clientFd = socket(AF_UNIX, SOCK_STREAM, 0);
    ASSERT_GE(clientFd, 0);
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    (void)strncpy_s(addr.sun_path, sizeof(addr.sun_path), path.c_str(), sizeof(addr.sun_path) - 1);
    ASSERT_EQ(connect(clientFd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)), 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS * 2));

    SocketStateBase stateAfter;
    EXPECT_EQ(server->GetState(stateAfter), SOCKET_ERROR_OK);
    EXPECT_TRUE(stateAfter.IsConnected());

    close(clientFd);
    server->Close();
    unlink(path.c_str());
}

} // namespace
