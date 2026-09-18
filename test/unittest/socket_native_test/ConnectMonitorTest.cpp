/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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
#include <limits>
#include <thread>

#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#include "gtest/gtest.h"

#define private public
#include "connect_monitor.h"
#undef private

namespace {
using namespace testing::ext;
using namespace OHOS::NetStack::Socket;

static constexpr int SLEEP_TIME_TEN = 10;

static OHOS::sptr<ConnectWatchData> CreateConnectWatchData(int64_t deadlineMs = 200)
{
    auto data = OHOS::sptr<ConnectWatchData>(new ConnectWatchData());
    data->deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(deadlineMs);
    return data;
}

static void WaitMonitorStopped(ConnectMonitor &monitor, int32_t timeoutMs = 1000)
{
    for (int32_t waited = 0; waited < timeoutMs && monitor.running_.load(); waited += SLEEP_TIME_TEN) {
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME_TEN));
    }
}

static void ResetConnectMonitorState(ConnectMonitor &monitor)
{
    {
        std::lock_guard<std::mutex> lifecycleLock(monitor.lifecycleMutex_);
        monitor.running_.store(false);
        monitor.WakeUp();
        if (monitor.thread_.joinable()) {
            monitor.thread_.join();
        }
    }
    std::lock_guard<std::mutex> lock(monitor.mutex_);
    monitor.pendings_.clear();
    monitor.deadlines_.clear();
    monitor.deadlineIters_.clear();
}

class ConnectMonitorTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}

    virtual void SetUp() {}
    virtual void TearDown() {}
};

HWTEST_F(ConnectMonitorTest, CalcNearestTimeoutMsBranches, TestSize.Level2)
{
    auto &monitor = ConnectMonitor::GetInstance();
    ResetConnectMonitorState(monitor);

    EXPECT_EQ(monitor.CalcNearestTimeoutMs(), -1);

    monitor.deadlines_.emplace(std::chrono::steady_clock::now() - std::chrono::milliseconds(1), 1001);
    EXPECT_EQ(monitor.CalcNearestTimeoutMs(), 0);
    monitor.deadlines_.clear();

    auto maxTimeout = static_cast<int64_t>(std::numeric_limits<int>::max()) + 1000;
    monitor.deadlines_.emplace(std::chrono::steady_clock::now() + std::chrono::milliseconds(maxTimeout), 1002);
    EXPECT_EQ(monitor.CalcNearestTimeoutMs(), std::numeric_limits<int>::max());
    monitor.deadlines_.clear();

    monitor.deadlines_.emplace(std::chrono::steady_clock::now() + std::chrono::milliseconds(50), 1003);
    int timeoutMs = monitor.CalcNearestTimeoutMs();
    EXPECT_GE(timeoutMs, 0);
    EXPECT_LE(timeoutMs, 1000);

    ResetConnectMonitorState(monitor);
}

HWTEST_F(ConnectMonitorTest, RegisterAndUnregisterSuccess, TestSize.Level2)
{
    auto &monitor = ConnectMonitor::GetInstance();
    ResetConnectMonitorState(monitor);

    int sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    ASSERT_GE(sockfd, 0);
    auto data = CreateConnectWatchData(500);
    ASSERT_NE(data, nullptr);

    EXPECT_TRUE(monitor.Register(sockfd, data));
    monitor.Unregister(sockfd);
    close(sockfd);

    ResetConnectMonitorState(monitor);
}

HWTEST_F(ConnectMonitorTest, RegisterReuseDeadlineIter, TestSize.Level2)
{
    auto &monitor = ConnectMonitor::GetInstance();
    ResetConnectMonitorState(monitor);

    int sockfd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    ASSERT_GE(sockfd, 0);
    auto oldDeadline = monitor.deadlines_.emplace(
        std::chrono::steady_clock::now() + std::chrono::seconds(2), sockfd);
    monitor.deadlineIters_[sockfd] = oldDeadline;

    auto data = CreateConnectWatchData(500);
    ASSERT_NE(data, nullptr);
    EXPECT_TRUE(monitor.Register(sockfd, data));
    monitor.Unregister(sockfd);
    close(sockfd);

    ResetConnectMonitorState(monitor);
}

HWTEST_F(ConnectMonitorTest, CheckTimeoutsBranches, TestSize.Level2)
{
    auto &monitor = ConnectMonitor::GetInstance();
    ResetConnectMonitorState(monitor);

    int activeFd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    ASSERT_GE(activeFd, 0);
    int missingFd = activeFd + 10000;
    auto now = std::chrono::steady_clock::now() - std::chrono::milliseconds(1);

    auto data = CreateConnectWatchData(100);
    ASSERT_NE(data, nullptr);
    monitor.pendings_[activeFd] = data;
    monitor.deadlineIters_[activeFd] = monitor.deadlines_.emplace(now, activeFd);
    monitor.deadlineIters_[missingFd] = monitor.deadlines_.emplace(now, missingFd);

    monitor.CheckTimeouts();
    EXPECT_EQ(monitor.pendings_.find(activeFd), monitor.pendings_.end());
    EXPECT_TRUE(monitor.deadlineIters_.empty());
    EXPECT_TRUE(monitor.deadlines_.empty());

    close(activeFd);
    ResetConnectMonitorState(monitor);
}

HWTEST_F(ConnectMonitorTest, HandleReadySuccessAndFailure, TestSize.Level2)
{
    auto &monitor = ConnectMonitor::GetInstance();
    ResetConnectMonitorState(monitor);

    int validFd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    ASSERT_GE(validFd, 0);
    auto validData = CreateConnectWatchData(1000);
    ASSERT_NE(validData, nullptr);
    monitor.pendings_[validFd] = validData;
    monitor.deadlineIters_[validFd] = monitor.deadlines_.emplace(validData->deadline, validFd);
    monitor.HandleReady(validFd, EPOLLOUT);
    EXPECT_EQ(monitor.pendings_.find(validFd), monitor.pendings_.end());

    int invalidFd = -1;
    auto invalidData = CreateConnectWatchData(1000);
    ASSERT_NE(invalidData, nullptr);
    monitor.pendings_[invalidFd] = invalidData;
    monitor.deadlineIters_[invalidFd] = monitor.deadlines_.emplace(invalidData->deadline, invalidFd);
    monitor.HandleReady(invalidFd, EPOLLERR);
    EXPECT_EQ(monitor.pendings_.find(invalidFd), monitor.pendings_.end());

    close(validFd);
    ResetConnectMonitorState(monitor);
}

HWTEST_F(ConnectMonitorTest, CompleteConnectNotFound, TestSize.Level2)
{
    auto &monitor = ConnectMonitor::GetInstance();
    ResetConnectMonitorState(monitor);

    int fd = 34567;
    monitor.deadlineIters_[fd] = monitor.deadlines_.emplace(std::chrono::steady_clock::now(), fd);
    monitor.CompleteConnect(fd, 0);
    EXPECT_EQ(monitor.deadlineIters_.find(fd), monitor.deadlineIters_.end());
    EXPECT_TRUE(monitor.pendings_.empty());

    ResetConnectMonitorState(monitor);
}

HWTEST_F(ConnectMonitorTest, EnsureThreadJoinableBranch, TestSize.Level2)
{
    auto &monitor = ConnectMonitor::GetInstance();
    ResetConnectMonitorState(monitor);

    int fd1 = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    ASSERT_GE(fd1, 0);
    auto data1 = CreateConnectWatchData(20);
    ASSERT_NE(data1, nullptr);
    ASSERT_TRUE(monitor.Register(fd1, data1));
    WaitMonitorStopped(monitor, 2000);
    ASSERT_FALSE(monitor.running_.load());
    ASSERT_TRUE(monitor.thread_.joinable());

    int fd2 = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    ASSERT_GE(fd2, 0);
    auto data2 = CreateConnectWatchData(300);
    ASSERT_NE(data2, nullptr);
    EXPECT_TRUE(monitor.Register(fd2, data2));
    monitor.Unregister(fd2);

    close(fd1);
    close(fd2);
    ResetConnectMonitorState(monitor);
}

HWTEST_F(ConnectMonitorTest, EnsureThreadRunningConcurrentRestart, TestSize.Level2)
{
    auto &monitor = ConnectMonitor::GetInstance();
    ResetConnectMonitorState(monitor);

    std::atomic<bool> oldThreadRunning(false);
    std::atomic<bool> stopOldThread(false);
    monitor.thread_ = std::thread([&oldThreadRunning, &stopOldThread]() {
        oldThreadRunning.store(true);
        while (!stopOldThread.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    for (int32_t waited = 0; waited < 1000 && !oldThreadRunning.load(); waited += SLEEP_TIME_TEN) {
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME_TEN));
    }
    ASSERT_TRUE(oldThreadRunning.load());
    monitor.running_.store(false);
    auto pendingData = CreateConnectWatchData(5000);
    ASSERT_NE(pendingData, nullptr);
    std::unique_lock<std::mutex> lifecycleLock(monitor.lifecycleMutex_);
    {
        std::lock_guard<std::mutex> lock(monitor.mutex_);
        monitor.pendings_[54321] = pendingData;
        monitor.deadlineIters_[54321] = monitor.deadlines_.emplace(pendingData->deadline, 54321);
    }

    std::atomic<int32_t> callerReady(0);
    std::thread callerA([&monitor, &callerReady]() {
        callerReady.fetch_add(1);
        monitor.EnsureThreadRunning();
    });
    std::thread callerB([&monitor, &callerReady]() {
        callerReady.fetch_add(1);
        monitor.EnsureThreadRunning();
    });
    for (int32_t waited = 0; waited < 1000 && callerReady.load() < 2; waited += SLEEP_TIME_TEN) {
        std::this_thread::sleep_for(std::chrono::milliseconds(SLEEP_TIME_TEN));
    }
    stopOldThread.store(true);
    lifecycleLock.unlock();
    callerA.join();
    callerB.join();
    EXPECT_EQ(callerReady.load(), 2);
    EXPECT_TRUE(monitor.running_.load());
    EXPECT_TRUE(monitor.thread_.joinable());

    ResetConnectMonitorState(monitor);
}

HWTEST_F(ConnectMonitorTest, MonitorLoopEpollErrorBranch, TestSize.Level2)
{
    auto &monitor = ConnectMonitor::GetInstance();
    ResetConnectMonitorState(monitor);

    int oldEpollFd = monitor.epollFd_;
    monitor.epollFd_ = -1;
    monitor.running_.store(true);
    monitor.MonitorLoop();
    EXPECT_FALSE(monitor.running_.load());
    monitor.epollFd_ = oldEpollFd;

    ResetConnectMonitorState(monitor);
}

} // namespace
