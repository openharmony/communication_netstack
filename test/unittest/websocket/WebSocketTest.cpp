/*
 * Copyright (c) 2023-2024 Huawei Device Co., Ltd.
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

#include "netstack_log.h"
#include "gtest/gtest.h"
#include <cstring>
#include <iostream>

#include "close_context.h"
#include "connect_context.h"
#include "send_context.h"
#include "websocket_async_work.h"
#include "websocket_exec.h"
#include "websocket_module.h"

class WebSocketTest : public testing::Test {
public:
    static void SetUpTestCase() {}

    static void TearDownTestCase() {}

    virtual void SetUp() {}

    virtual void TearDown() {}
};

namespace {
using namespace testing::ext;
using namespace OHOS::NetStack::Websocket;

HWTEST_F(WebSocketTest, WebSocketTest001, TestSize.Level1)
{
    napi_env env = nullptr;
    OHOS::NetStack::EventManager eventManager;
    ConnectContext context(env, &eventManager);
    bool ret = WebSocketExec::ExecConnect(&context);
    EXPECT_EQ(ret, false);
}

HWTEST_F(WebSocketTest, WebSocketTest002, TestSize.Level1)
{
    napi_env env = nullptr;
    OHOS::NetStack::EventManager eventManager;
    SendContext context(env, &eventManager);
    bool ret = WebSocketExec::ExecSend(&context);
    EXPECT_EQ(ret, false);
}

HWTEST_F(WebSocketTest, WebSocketTest003, TestSize.Level1)
{
    napi_env env = nullptr;
    OHOS::NetStack::EventManager eventManager;
    CloseContext context(env, &eventManager);
    bool ret = WebSocketExec::ExecClose(&context);
    EXPECT_EQ(ret, false);
}

HWTEST_F(WebSocketTest, WebSocketTest004, TestSize.Level1)
{
    bool ret = WebSocketExec::ExecConnect(nullptr);
    EXPECT_EQ(ret, false);
}

HWTEST_F(WebSocketTest, WebSocketTest005, TestSize.Level1)
{
    napi_env env = nullptr;
    OHOS::NetStack::EventManager eventManager;
    ConnectContext context(env, &eventManager);
    context.caPath_ = "/etc/ssl/certs/test_ca.crt";
    bool ret = WebSocketExec::ExecConnect(&context);
    EXPECT_EQ(ret, false);
}

HWTEST_F(WebSocketTest, WebSocketTest006, TestSize.Level1)
{
    napi_env env = nullptr;
    OHOS::NetStack::EventManager eventManager;
    ConnectContext context(env, &eventManager);
    context.caPath_ = "";
    bool ret = WebSocketExec::ExecConnect(&context);
    EXPECT_EQ(ret, false);
}

} // namespace