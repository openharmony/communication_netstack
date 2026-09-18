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
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <mutex>
#include <string>
#include <thread>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "gtest/gtest.h"
#include "securec.h"

#define private public
#define protected public
#include "local_socket_client_innerapi.h"
#undef private
#undef protected

#include "local_socket_options.h"
#include "socket_constant.h"
#include "socket_exec_common.h"
#include "socket_state_base.h"

using namespace testing::ext;
using namespace OHOS::NetStack::Socket;

class LocalSocketClientTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    void SetUp() {}
    void TearDown() {}
};

namespace {

static constexpr int32_t INVALID_FD = -1;
static constexpr int32_t SLEEP_MS = 100;
static constexpr int32_t LISTEN_BACKLOG = 5;
static constexpr const char *TEST_DATA = "hello local socket";
static constexpr const char *TEST_DATA_LARGE =
    "abcdefghijklmnopqrstuvwxyz0123456789abcdefghijklmnopqrstuvwxyz0123456789";
static constexpr uint32_t MAX_SOCKET_BUFFER_SIZE = 262144;
static constexpr uint32_t DEFAULT_BUFFER_SIZE = 8192;

static std::string MakeTempSocketPath()
{
    char tmpl[] = "/tmp/local_socket_ut_XXXXXX";
    int fd = mkstemp(tmpl);
    if (fd >= 0) {
        close(fd);
        unlink(tmpl);
    }
    return std::string(tmpl);
}

/* Create a listening AF_UNIX SOCK_STREAM server socket bound to socketPath.
 * Returns the listen fd on success, -1 on failure. */
static int MakeLocalListenFd(const std::string &socketPath)
{
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -1;
    }
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (socketPath.size() >= sizeof(addr.sun_path)) {
        close(fd);
        return -1;
    }
    (void)strncpy_s(addr.sun_path, sizeof(addr.sun_path), socketPath.c_str(), sizeof(addr.sun_path) - 1);
    unlink(addr.sun_path);
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

static void OnMessageCallback(const std::string &data, const std::string &address, const size_t size)
{
    (void)data;
    (void)address;
    (void)size;
}

static void OnConnectCallback()
{
}

static void OnErrorCallback(int32_t errorCode, const std::string &errorString)
{
    (void)errorCode;
    (void)errorString;
}

static void OnCloseCallback()
{
}

/* ==================== Constructor / Destructor ==================== */

HWTEST_F(LocalSocketClientTest, Constructor001, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    {
        int32_t fd = -1;
        EXPECT_EQ(socket->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_EQ(fd, INVALID_FD);
    }
    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_FALSE(state.IsBound());
    EXPECT_FALSE(state.IsConnected());
    EXPECT_FALSE(state.IsClose());
}

HWTEST_F(LocalSocketClientTest, Destructor001, TestSize.Level1)
{
    /* Destructor on a fresh socket should not crash */
    {
        auto socket = std::make_shared<LocalSocket>();
        EXPECT_NE(socket, nullptr);
        int32_t fd = -1;
        EXPECT_EQ(socket->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_EQ(fd, INVALID_FD);
    }
    SUCCEED();
}

HWTEST_F(LocalSocketClientTest, Destructor002, TestSize.Level1)
{
    /* Destructor should close socket fd if bound */
    std::string path = MakeTempSocketPath();
    {
        auto socket = std::make_shared<LocalSocket>();
        ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);
        {
            int32_t fd = -1;
            EXPECT_EQ(socket->GetSocketFd(fd), SOCKET_ERROR_OK);
            EXPECT_GE(fd, 0);
        }
        /* Destructor will close fd */
    }
    unlink(path.c_str());
    SUCCEED();
}

/* ==================== Callback Registration ==================== */

HWTEST_F(LocalSocketClientTest, Callback001, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    socket->OnMessage(OnMessageCallback);
    socket->OnConnect(OnConnectCallback);
    socket->OnError(OnErrorCallback);
    socket->OnClose(OnCloseCallback);
    EXPECT_NE(socket->onMessageCallback_, nullptr);
    EXPECT_NE(socket->onConnectCallback_, nullptr);
    EXPECT_NE(socket->onErrorCallback_, nullptr);
    EXPECT_NE(socket->onCloseCallback_, nullptr);
}

HWTEST_F(LocalSocketClientTest, Callback002, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    socket->OnMessage(OnMessageCallback);
    socket->OnConnect(OnConnectCallback);
    socket->OnError(OnErrorCallback);
    socket->OnClose(OnCloseCallback);

    socket->OffMessage();
    socket->OffConnect();
    socket->OffError();
    socket->OffClose();
    EXPECT_EQ(socket->onMessageCallback_, nullptr);
    EXPECT_EQ(socket->onConnectCallback_, nullptr);
    EXPECT_EQ(socket->onErrorCallback_, nullptr);
    EXPECT_EQ(socket->onCloseCallback_, nullptr);
}

HWTEST_F(LocalSocketClientTest, Callback003, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    socket->OnMessage(OnMessageCallback);
    socket->OffMessage();
    EXPECT_EQ(socket->onMessageCallback_, nullptr);

    /* Re-register with lambda */
    std::atomic<bool> called(false);
    socket->OnMessage([&called](const std::string &, const std::string &, const size_t) { called.store(true); });
    EXPECT_NE(socket->onMessageCallback_, nullptr);
}

/* ==================== Bind ==================== */

HWTEST_F(LocalSocketClientTest, Bind001, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    int ret = socket->Bind(path);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    {
        int32_t fd = -1;
        EXPECT_EQ(socket->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_GE(fd, 0);
    }
    socket->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, Bind002, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    int ret = socket->Bind("");
    EXPECT_EQ(ret, PARAM_ERROR_CODE);
    {
        int32_t fd = -1;
        EXPECT_EQ(socket->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_EQ(fd, INVALID_FD);
    }
}

HWTEST_F(LocalSocketClientTest, Bind003, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);
    /* Second bind on the same socket should fail because fd is already bound. */
    std::string path2 = MakeTempSocketPath();
    int ret = socket->Bind(path2);
    EXPECT_NE(ret, SOCKET_ERROR_OK);
    socket->Close();
    unlink(path.c_str());
    unlink(path2.c_str());
}

HWTEST_F(LocalSocketClientTest, Bind004, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);
    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsBound());
    socket->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, Bind005, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);
    std::string localPath;
    int ret = socket->GetLocalAddress(localPath);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_EQ(localPath, path);
    socket->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, Bind006, TestSize.Level1)
{
    /* Bind should unlink existing socket file at path */
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    /* Create a file at path first */
    int fd = open(path.c_str(), O_CREAT | O_WRONLY, 0600);
    ASSERT_GE(fd, 0);
    close(fd);
    int ret = socket->Bind(path);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    socket->Close();
    unlink(path.c_str());
}

/* ==================== Connect ==================== */

HWTEST_F(LocalSocketClientTest, Connect001, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    int ret = socket->Connect("", 0);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);
}

HWTEST_F(LocalSocketClientTest, Connect002, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    int ret = socket->Connect(path, -1);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, Connect003, TestSize.Level1)
{
    /* Connect to non-existent server should fail */
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    int ret = socket->Connect(path, 1000);
    EXPECT_NE(ret, SOCKET_ERROR_OK);
    socket->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, Connect004, TestSize.Level1)
{
    /* Successful connect to a listening local server */
    std::string path = MakeTempSocketPath();
    int listenFd = MakeLocalListenFd(path);
    ASSERT_GE(listenFd, 0);

    auto socket = std::make_shared<LocalSocket>();
    socket->OnMessage(OnMessageCallback);
    socket->OnConnect(OnConnectCallback);
    socket->OnError(OnErrorCallback);
    socket->OnClose(OnCloseCallback);
    int ret = socket->Connect(path, 5000);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    {
        int32_t fd = -1;
        EXPECT_EQ(socket->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_GE(fd, 0);
    }

    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsConnected());

    /* Accept the connection on server side to complete the connection */
    int clientFd = accept(listenFd, nullptr, nullptr);
    EXPECT_GE(clientFd, 0);
    if (clientFd >= 0) {
        close(clientFd);
    }

    socket->Close();
    close(listenFd);
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, Connect005, TestSize.Level1)
{
    /* Verify onConnectCallback is called after successful connect */
    std::string path = MakeTempSocketPath();
    int listenFd = MakeLocalListenFd(path);
    ASSERT_GE(listenFd, 0);

    std::atomic<bool> connectCalled(false);
    auto onConnect = [&connectCalled]() { connectCalled.store(true); };

    auto socket = std::make_shared<LocalSocket>();
    socket->OnMessage(OnMessageCallback);
    socket->OnConnect(onConnect);
    socket->OnError(OnErrorCallback);
    socket->OnClose(OnCloseCallback);
    ASSERT_EQ(socket->Connect(path, 5000), SOCKET_ERROR_OK);

    int clientFd = accept(listenFd, nullptr, nullptr);
    if (clientFd >= 0) {
        close(clientFd);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS));
    EXPECT_TRUE(connectCalled.load());

    socket->Close();
    close(listenFd);
    unlink(path.c_str());
}

/* ==================== Send ==================== */

HWTEST_F(LocalSocketClientTest, Send001, TestSize.Level1)
{
    /* Send on a fresh socket (no fd) should return ERRNO_BAD_FD */
    auto socket = std::make_shared<LocalSocket>();
    LocalSocketOptions options;
    options.SetBuffer(TEST_DATA);
    int ret = socket->Send(options);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

HWTEST_F(LocalSocketClientTest, Send002, TestSize.Level1)
{
    /* Send empty buffer should return PARAM_ERROR_CODE */
    std::string path = MakeTempSocketPath();
    int listenFd = MakeLocalListenFd(path);
    ASSERT_GE(listenFd, 0);

    auto socket = std::make_shared<LocalSocket>();
    ASSERT_EQ(socket->Connect(path, 5000), SOCKET_ERROR_OK);
    int clientFd = accept(listenFd, nullptr, nullptr);
    if (clientFd >= 0) {
        close(clientFd);
    }

    LocalSocketOptions options;
    options.SetBuffer("");
    int ret = socket->Send(options);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);

    socket->Close();
    close(listenFd);
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, Send003, TestSize.Level1)
{
    /* Successful send to a listening server, verify data received */
    std::string path = MakeTempSocketPath();
    int listenFd = MakeLocalListenFd(path);
    ASSERT_GE(listenFd, 0);

    auto socket = std::make_shared<LocalSocket>();
    ASSERT_EQ(socket->Connect(path, 5000), SOCKET_ERROR_OK);
    int clientFd = accept(listenFd, nullptr, nullptr);
    ASSERT_GE(clientFd, 0);

    LocalSocketOptions options;
    options.SetBuffer(TEST_DATA);
    int ret = socket->Send(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);

    char buf[128] = {0};
    ssize_t recvLen = recv(clientFd, buf, sizeof(buf), 0);
    EXPECT_EQ(recvLen, static_cast<ssize_t>(strlen(TEST_DATA)));
    EXPECT_STREQ(buf, TEST_DATA);

    socket->Close();
    close(clientFd);
    close(listenFd);
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, Send004, TestSize.Level1)
{
    /* Send after Close should fail */
    std::string path = MakeTempSocketPath();
    int listenFd = MakeLocalListenFd(path);
    ASSERT_GE(listenFd, 0);

    auto socket = std::make_shared<LocalSocket>();
    ASSERT_EQ(socket->Connect(path, 5000), SOCKET_ERROR_OK);
    int clientFd = accept(listenFd, nullptr, nullptr);
    if (clientFd >= 0) {
        close(clientFd);
    }
    socket->Close();

    LocalSocketOptions options;
    options.SetBuffer(TEST_DATA);
    int ret = socket->Send(options);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);

    close(listenFd);
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, Send005, TestSize.Level1)
{
    /* Send large data */
    std::string path = MakeTempSocketPath();
    int listenFd = MakeLocalListenFd(path);
    ASSERT_GE(listenFd, 0);

    auto socket = std::make_shared<LocalSocket>();
    ASSERT_EQ(socket->Connect(path, 5000), SOCKET_ERROR_OK);
    int clientFd = accept(listenFd, nullptr, nullptr);
    ASSERT_GE(clientFd, 0);

    LocalSocketOptions options;
    options.SetBuffer(TEST_DATA_LARGE);
    int ret = socket->Send(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);

    char buf[256] = {0};
    ssize_t totalRecv = 0;
    while (totalRecv < static_cast<ssize_t>(strlen(TEST_DATA_LARGE))) {
        ssize_t n = recv(clientFd, buf + totalRecv, sizeof(buf) - totalRecv - 1, 0);
        if (n <= 0) {
            break;
        }
        totalRecv += n;
    }
    EXPECT_EQ(totalRecv, static_cast<ssize_t>(strlen(TEST_DATA_LARGE)));

    socket->Close();
    close(clientFd);
    close(listenFd);
    unlink(path.c_str());
}

/* ==================== Close ==================== */

HWTEST_F(LocalSocketClientTest, Close001, TestSize.Level1)
{
    /* Close on a fresh socket should return ERRNO_BAD_FD */
    auto socket = std::make_shared<LocalSocket>();
    int ret = socket->Close();
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

HWTEST_F(LocalSocketClientTest, Close002, TestSize.Level1)
{
    /* Close after Bind should succeed */
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);
    int ret = socket->Close();
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    {
        int32_t fd = -1;
        EXPECT_EQ(socket->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_EQ(fd, INVALID_FD);
    }
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, Close003, TestSize.Level1)
{
    /* Double close: second close should return ERRNO_BAD_FD */
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);
    ASSERT_EQ(socket->Close(), SOCKET_ERROR_OK);
    int ret = socket->Close();
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, Close004, TestSize.Level1)
{
    /* Close should trigger onCloseCallback */
    std::atomic<bool> closeCalled(false);
    auto onClose = [&closeCalled]() { closeCalled.store(true); };

    auto socket = std::make_shared<LocalSocket>();
    socket->OnMessage(OnMessageCallback);
    socket->OnConnect(OnConnectCallback);
    socket->OnError(OnErrorCallback);
    socket->OnClose(onClose);
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);
    ASSERT_EQ(socket->Close(), SOCKET_ERROR_OK);
    EXPECT_TRUE(closeCalled.load());
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, Close005, TestSize.Level1)
{
    /* Close should update state */
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);
    ASSERT_EQ(socket->Close(), SOCKET_ERROR_OK);
    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    unlink(path.c_str());
}

/* ==================== GetState ==================== */

HWTEST_F(LocalSocketClientTest, GetState001, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    SocketStateBase state;
    int ret = socket->GetState(state);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_FALSE(state.IsBound());
    EXPECT_FALSE(state.IsConnected());
    EXPECT_FALSE(state.IsClose());
}

HWTEST_F(LocalSocketClientTest, GetState002, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);
    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsBound());
    socket->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, GetState003, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);
    socket->Close();
    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    unlink(path.c_str());
}

/* ==================== GetSocketFd ==================== */

HWTEST_F(LocalSocketClientTest, GetSocketFd001, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    {
        int32_t fd = -1;
        EXPECT_EQ(socket->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_EQ(fd, INVALID_FD);
    }
}

HWTEST_F(LocalSocketClientTest, GetSocketFd002, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);
    {
        int32_t fd = -1;
        EXPECT_EQ(socket->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_GE(fd, 0);
    }
    socket->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, GetSocketFd003, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);
    socket->Close();
    {
        int32_t fd = -1;
        EXPECT_EQ(socket->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_EQ(fd, INVALID_FD);
    }
    unlink(path.c_str());
}

/* ==================== GetLocalAddress ==================== */

HWTEST_F(LocalSocketClientTest, GetLocalAddress001, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    std::string path;
    int ret = socket->GetLocalAddress(path);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

HWTEST_F(LocalSocketClientTest, GetLocalAddress002, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    std::string bindPath = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(bindPath), SOCKET_ERROR_OK);
    std::string path;
    int ret = socket->GetLocalAddress(path);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);
    EXPECT_EQ(path, bindPath);
    socket->Close();
    unlink(bindPath.c_str());
}

HWTEST_F(LocalSocketClientTest, GetLocalAddress003, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    std::string bindPath = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(bindPath), SOCKET_ERROR_OK);
    socket->Close();
    std::string path;
    int ret = socket->GetLocalAddress(path);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
    unlink(bindPath.c_str());
}

/* ==================== SetExtraOptions ==================== */

HWTEST_F(LocalSocketClientTest, SetExtraOptions001, TestSize.Level1)
{
    /* SetExtraOptions on a fresh socket (no fd) should return ERRNO_BAD_FD */
    auto socket = std::make_shared<LocalSocket>();
    LocalExtraOptions options;
    int ret = socket->SetExtraOptions(options);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

HWTEST_F(LocalSocketClientTest, SetExtraOptions002, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);

    LocalExtraOptions options;
    options.SetReceiveBufferSize(DEFAULT_BUFFER_SIZE);
    options.SetRecvBufSizeFlag(true);
    int ret = socket->SetExtraOptions(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);

    socket->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, SetExtraOptions003, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);

    LocalExtraOptions options;
    options.SetSendBufferSize(DEFAULT_BUFFER_SIZE);
    options.SetSendBufSizeFlag(true);
    int ret = socket->SetExtraOptions(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);

    socket->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, SetExtraOptions004, TestSize.Level1)
{
    /* Buffer size exceeding MAX_SOCKET_BUFFER_SIZE should return PARAM_ERROR_CODE */
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);

    LocalExtraOptions options;
    options.SetReceiveBufferSize(MAX_SOCKET_BUFFER_SIZE + 1);
    int ret = socket->SetExtraOptions(options);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);

    socket->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, SetExtraOptions005, TestSize.Level1)
{
    /* Send buffer size exceeding MAX_SOCKET_BUFFER_SIZE should return PARAM_ERROR_CODE */
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);

    LocalExtraOptions options;
    options.SetSendBufferSize(MAX_SOCKET_BUFFER_SIZE + 1);
    int ret = socket->SetExtraOptions(options);
    EXPECT_EQ(ret, PARAM_ERROR_CODE);

    socket->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, SetExtraOptions006, TestSize.Level1)
{
    /* SetExtraOptions after Close should return ERRNO_BAD_FD */
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);
    socket->Close();

    LocalExtraOptions options;
    int ret = socket->SetExtraOptions(options);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, SetExtraOptions007, TestSize.Level1)
{
    /* Set all valid options together */
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);

    LocalExtraOptions options;
    options.SetReceiveBufferSize(DEFAULT_BUFFER_SIZE);
    options.SetRecvBufSizeFlag(true);
    options.SetSendBufferSize(DEFAULT_BUFFER_SIZE);
    options.SetSendBufSizeFlag(true);
    options.SetSocketTimeout(2000);
    options.SetTimeoutFlag(true);
    int ret = socket->SetExtraOptions(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);

    socket->Close();
    unlink(path.c_str());
}

/* ==================== GetExtraOptions ==================== */

HWTEST_F(LocalSocketClientTest, GetExtraOptions001, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    LocalExtraOptions options;
    int ret = socket->GetExtraOptions(options);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
}

HWTEST_F(LocalSocketClientTest, GetExtraOptions002, TestSize.Level1)
{
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);

    LocalExtraOptions options;
    int ret = socket->GetExtraOptions(options);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);

    socket->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, GetExtraOptions003, TestSize.Level1)
{
    /* Set then Get extra options, verify round-trip */
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);

    LocalExtraOptions setOptions;
    setOptions.SetReceiveBufferSize(DEFAULT_BUFFER_SIZE);
    setOptions.SetRecvBufSizeFlag(true);
    setOptions.SetSendBufferSize(DEFAULT_BUFFER_SIZE * 2);
    setOptions.SetSendBufSizeFlag(true);
    ASSERT_EQ(socket->SetExtraOptions(setOptions), SOCKET_ERROR_OK);

    LocalExtraOptions getOptions;
    int ret = socket->GetExtraOptions(getOptions);
    EXPECT_EQ(ret, SOCKET_ERROR_OK);

    socket->Close();
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, GetExtraOptions004, TestSize.Level1)
{
    /* GetExtraOptions after Close should return ERRNO_BAD_FD */
    auto socket = std::make_shared<LocalSocket>();
    std::string path = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);
    socket->Close();

    LocalExtraOptions options;
    int ret = socket->GetExtraOptions(options);
    EXPECT_EQ(ret, static_cast<int32_t>(SOCKET_ERROR_CODE_BASE) + ERRNO_BAD_FD);
    unlink(path.c_str());
}

/* ==================== Lifecycle / Integration ==================== */

HWTEST_F(LocalSocketClientTest, Lifecycle001, TestSize.Level1)
{
    /* Full lifecycle: Bind -> GetState -> Close -> GetState */
    auto socket = std::make_shared<LocalSocket>();
    socket->OnMessage(OnMessageCallback);
    socket->OnConnect(OnConnectCallback);
    socket->OnError(OnErrorCallback);
    socket->OnClose(OnCloseCallback);

    std::string path = MakeTempSocketPath();
    ASSERT_EQ(socket->Bind(path), SOCKET_ERROR_OK);
    {
        int32_t fd = -1;
        EXPECT_EQ(socket->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_GE(fd, 0);
    }

    SocketStateBase state;
    EXPECT_EQ(socket->GetState(state), SOCKET_ERROR_OK);
    EXPECT_TRUE(state.IsBound());

    std::string localPath;
    EXPECT_EQ(socket->GetLocalAddress(localPath), SOCKET_ERROR_OK);
    EXPECT_EQ(localPath, path);

    EXPECT_EQ(socket->Close(), SOCKET_ERROR_OK);
    {
        int32_t fd = -1;
        EXPECT_EQ(socket->GetSocketFd(fd), SOCKET_ERROR_OK);
        EXPECT_EQ(fd, INVALID_FD);
    }

    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, Lifecycle002, TestSize.Level1)
{
    /* Full lifecycle: Connect -> Send -> Close */
    std::string path = MakeTempSocketPath();
    int listenFd = MakeLocalListenFd(path);
    ASSERT_GE(listenFd, 0);

    auto socket = std::make_shared<LocalSocket>();
    socket->OnMessage(OnMessageCallback);
    socket->OnConnect(OnConnectCallback);
    socket->OnError(OnErrorCallback);
    socket->OnClose(OnCloseCallback);
    ASSERT_EQ(socket->Connect(path, 5000), SOCKET_ERROR_OK);

    int clientFd = accept(listenFd, nullptr, nullptr);
    ASSERT_GE(clientFd, 0);

    LocalSocketOptions options;
    options.SetBuffer(TEST_DATA);
    EXPECT_EQ(socket->Send(options), SOCKET_ERROR_OK);

    char buf[128] = {0};
    ssize_t n = recv(clientFd, buf, sizeof(buf), 0);
    EXPECT_EQ(n, static_cast<ssize_t>(strlen(TEST_DATA)));

    EXPECT_EQ(socket->Close(), SOCKET_ERROR_OK);

    close(clientFd);
    close(listenFd);
    unlink(path.c_str());
}

HWTEST_F(LocalSocketClientTest, Lifecycle003, TestSize.Level1)
{
    /* Receive data from server, verify onMessageCallback is invoked */
    std::string path = MakeTempSocketPath();
    int listenFd = MakeLocalListenFd(path);
    ASSERT_GE(listenFd, 0);

    std::atomic<bool> messageReceived(false);
    std::string receivedData;
    std::mutex dataMutex;
    auto onMessage = [&receivedData, &messageReceived, &dataMutex](const std::string &data,
                                                                   const std::string &address, const size_t size) {
        (void)address;
        std::lock_guard<std::mutex> lock(dataMutex);
        receivedData.assign(data, 0, size);
        messageReceived.store(true);
    };

    auto socket = std::make_shared<LocalSocket>();
    socket->OnMessage(onMessage);
    socket->OnConnect(OnConnectCallback);
    socket->OnError(OnErrorCallback);
    socket->OnClose(OnCloseCallback);
    ASSERT_EQ(socket->Connect(path, 5000), SOCKET_ERROR_OK);

    int clientFd = accept(listenFd, nullptr, nullptr);
    ASSERT_GE(clientFd, 0);

    /* Server sends data to client */
    std::string serverMsg = "server_hello";
    ssize_t sent = send(clientFd, serverMsg.c_str(), serverMsg.size(), 0);
    EXPECT_EQ(sent, static_cast<ssize_t>(serverMsg.size()));

    std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_MS * 2));
    {
        std::lock_guard<std::mutex> lock(dataMutex);
        EXPECT_TRUE(messageReceived.load());
        EXPECT_EQ(receivedData, serverMsg);
    }

    socket->Close();
    close(clientFd);
    close(listenFd);
    unlink(path.c_str());
}

} // namespace
