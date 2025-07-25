/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
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

#include <iostream>
#include <cstring>
#include "openssl/ssl.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "http_client_constant.h"
#include "netstack_log.h"
#include "netstack_common_utils.h"

#define private public
#include "http_client_task.h"
#include "http_client.h"
#include "http_client_error.h"
#include <curl/curl.h>
#include "http_client_request.h"
#include "http_client_response.h"
#if HAS_NETMANAGER_BASE
#include "net_conn_client.h"
#include "network_security_config.h"
#endif

using namespace OHOS::NetStack::HttpClient;
using namespace testing;
using namespace testing::ext;

class HttpClientTaskTest : public testing::Test {
public:
    static void SetUpTestCase() {}

    static void TearDownTestCase() {}

    virtual void SetUp() {}

    virtual void TearDown() {}

    void ProcesslTaskCb(std::shared_ptr<HttpClientTask> task);
};

namespace {
using namespace std;
using namespace testing::ext;

static void testCallbackconst(const HttpClientRequest &request, std::map<std::string,
    std::string> headerWithSetCookie)
{
    NETSTACK_LOGI("testCallbackconst function called!");
}

HWTEST_F(HttpClientTaskTest, GetHttpVersionTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "https://www.baidu.com";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    uint32_t httpVersionTest = task->GetHttpVersion(HttpProtocol::HTTP_NONE);
    EXPECT_EQ(httpVersionTest, CURL_HTTP_VERSION_NONE);
}

HWTEST_F(HttpClientTaskTest, GetHttpVersionTest002, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "https://www.baidu.com";
    httpReq.SetURL(url);
    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    uint32_t httpVersionTest = task->GetHttpVersion(HttpProtocol::HTTP1_1);
    EXPECT_EQ(httpVersionTest, CURL_HTTP_VERSION_1_1);
}

HWTEST_F(HttpClientTaskTest, GetHttpVersionTest003, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "https://www.baidu.com";
    httpReq.SetURL(url);
    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    uint32_t httpVersionTest = task->GetHttpVersion(HttpProtocol::HTTP2);
    EXPECT_EQ(httpVersionTest, CURL_HTTP_VERSION_2_0);
    httpVersionTest = task->GetHttpVersion(HttpProtocol::HTTP3);
    EXPECT_EQ(httpVersionTest, CURL_HTTP_VERSION_3);
}

HWTEST_F(HttpClientTaskTest, SetOtherCurlOptionTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;

    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);
    httpReq.SetHttpProxyType(NOT_USE);
    HttpProxy proxy;
    proxy.host = "192.168.147.60";
    proxy.port = 8888;
    proxy.exclusions = "www.httpbin.org";
    proxy.tunnel = false;
    httpReq.SetHttpProxy(proxy);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    bool result = task->SetOtherCurlOption(task->curlHandle_);
    EXPECT_TRUE(result);
}

HWTEST_F(HttpClientTaskTest, SetOtherCurlOptionTest002, TestSize.Level1)
{
    HttpClientRequest httpReq;

    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);
    httpReq.SetHttpProxyType(NOT_USE);
    HttpProxy proxy;
    proxy.host = "192.168.147.60";
    proxy.port = 8888;
    proxy.tunnel = false;
    httpReq.SetHttpProxy(proxy);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    EXPECT_TRUE(task->SetOtherCurlOption(task->curlHandle_));
}

HWTEST_F(HttpClientTaskTest, SetOtherCurlOptionTest003, TestSize.Level1)
{
    HttpClientRequest httpReq;

    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);
    httpReq.SetHttpProxyType(NOT_USE);
    HttpProxy proxy;
    proxy.host = "192.168.147.60";
    proxy.port = 8888;
    proxy.tunnel = false;
    httpReq.SetHttpProxy(proxy);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    EXPECT_TRUE(task->SetOtherCurlOption(task->curlHandle_));
}

HWTEST_F(HttpClientTaskTest, SetOtherCurlOptionTest004, TestSize.Level1)
{
    HttpClientRequest httpReq;

    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);
    httpReq.SetHttpProxyType(NOT_USE);
    HttpProxy proxy;
    proxy.host = "192.168.147.60";
    proxy.port = 8888;
    proxy.exclusions = "www.httpbin.org";
    proxy.tunnel = true;
    httpReq.SetHttpProxy(proxy);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    EXPECT_TRUE(task->SetOtherCurlOption(task->curlHandle_));
    curl_easy_cleanup(task->curlHandle_);
    task->curlHandle_ = nullptr;
}

HWTEST_F(HttpClientTaskTest, SetOtherCurlOptionTest005, TestSize.Level1)
{
    HttpClientRequest httpReq;

    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);
    httpReq.SetHttpProxyType(USE_SPECIFIED);
    HttpProxy proxy;
    proxy.host = "192.168.147.60";
    proxy.port = 8888;
    proxy.exclusions = "www.test.org";
    proxy.tunnel = true;
    httpReq.SetHttpProxy(proxy);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);
    EXPECT_TRUE(task->SetOtherCurlOption(task->curlHandle_));
    curl_easy_cleanup(task->curlHandle_);
    task->curlHandle_ = nullptr;
}

HWTEST_F(HttpClientTaskTest, SetUploadOptionsTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/put";
    httpReq.SetURL(url);
    std::string method = "PUT";
    httpReq.SetMethod(method);

    HttpSession &session = HttpSession::GetInstance();
    std::string filePath = "/bin/who";
    auto task = session.CreateTask(httpReq, UPLOAD, filePath);

    EXPECT_TRUE(task->SetUploadOptions(task->curlHandle_));
}

HWTEST_F(HttpClientTaskTest, SetUploadOptionsTest002, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/put";
    httpReq.SetURL(url);
    std::string method = "PUT";
    httpReq.SetMethod(method);

    HttpSession &session = HttpSession::GetInstance();
    std::string filePath = "";
    auto task = session.CreateTask(httpReq, UPLOAD, filePath);

    EXPECT_FALSE(task->SetUploadOptions(task->curlHandle_));
}

HWTEST_F(HttpClientTaskTest, SetUploadOptionsTest003, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/put";
    httpReq.SetURL(url);
    std::string method = "PUT";
    httpReq.SetMethod(method);

    HttpSession &session = HttpSession::GetInstance();
    std::string filePath = "unavailable";
    auto task = session.CreateTask(httpReq, UPLOAD, filePath);

    EXPECT_FALSE(task->SetUploadOptions(task->curlHandle_));
}

HWTEST_F(HttpClientTaskTest, SetCurlOptionsTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    EXPECT_TRUE(task->SetCurlOptions());
}

HWTEST_F(HttpClientTaskTest, SetCurlOptionsTest002, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    task->request_.SetMethod(HttpConstant::HTTP_METHOD_HEAD);

    EXPECT_TRUE(task->SetCurlOptions());
}

HWTEST_F(HttpClientTaskTest, SetCurlOptionsTest003, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/put";
    httpReq.SetURL(url);
    std::string method = "PUT";
    httpReq.SetMethod(method);

    HttpSession &session = HttpSession::GetInstance();
    std::string filePath = "/bin/who";
    auto task = session.CreateTask(httpReq, UPLOAD, filePath);

    task->curlHandle_ = nullptr;
    EXPECT_FALSE(task->SetCurlOptions());
}

HWTEST_F(HttpClientTaskTest, SetCurlOptionsTest004, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    task->request_.SetMethod(HttpConstant::HTTP_METHOD_POST);

    EXPECT_TRUE(task->SetCurlOptions());
}

HWTEST_F(HttpClientTaskTest, SetCurlOptionsTest005, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    task->curlHandle_ = nullptr;

    EXPECT_FALSE(task->SetCurlOptions());
}

HWTEST_F(HttpClientTaskTest, SetCurlOptionsTest006, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    task->curlHandle_ = nullptr;
    task->curlHeaderList_ = nullptr;
    std::string headerStr = "Connection:keep-alive";
    task->curlHeaderList_ = curl_slist_append(task->curlHeaderList_, headerStr.c_str());

    EXPECT_FALSE(task->SetCurlOptions());
}

HWTEST_F(HttpClientTaskTest, SetCurlOptionsTest007, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    task->request_.SetMethod(HttpConstant::HTTP_METHOD_PUT);
    task->request_.SetResumeFrom(1000);

    EXPECT_FALSE(task->SetCurlOptions());
}

HWTEST_F(HttpClientTaskTest, SetCurlOptionsTest008, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    task->request_.SetMethod(HttpConstant::HTTP_METHOD_GET);
    task->request_.SetResumeFrom(1000);

    EXPECT_TRUE(task->SetCurlOptions());
}

HWTEST_F(HttpClientTaskTest, SetCurlOptionsTest009, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    task->request_.SetResumeFrom(1000);
    task->request_.SetResumeTo(100);

    EXPECT_TRUE(task->SetCurlOptions());
}

HWTEST_F(HttpClientTaskTest, GetType001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "https://www.baidu.com";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    EXPECT_EQ(TaskType::DEFAULT, task->GetType());
}

HWTEST_F(HttpClientTaskTest, GetFilePathTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/put";
    httpReq.SetURL(url);
    std::string method = "PUT";
    httpReq.SetMethod(method);

    HttpSession &session = HttpSession::GetInstance();
    std::string filePath = "/bin/who";
    auto task = session.CreateTask(httpReq, UPLOAD, filePath);

    EXPECT_EQ(task->GetFilePath(), "/bin/who");
}
HWTEST_F(HttpClientTaskTest, GetTaskIdTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    unsigned int taskId = task->GetTaskId();
    EXPECT_TRUE(taskId >= 0);
}

HWTEST_F(HttpClientTaskTest, OnSuccessTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/delete";
    std::string method = "DELETE";
    httpReq.SetURL(url);
    httpReq.SetMethod(method);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);
    task->OnSuccess([task](const HttpClientRequest &request, const HttpClientResponse &response) {});
    task->Start();

    char *curlMethod = nullptr;
    while (task->GetCurlHandle() == nullptr) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    curl_easy_getinfo(task->GetCurlHandle(), CURLINFO_EFFECTIVE_METHOD, &curlMethod);

    while (task->GetStatus() != TaskStatus::IDLE) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (task->GetResponse().GetResponseCode() == 200) {
        if (curlMethod == method) {
            NETSTACK_LOGI("TestMethod() = %{public}s OK!", curlMethod);
        }
        const std::map<std::string, std::string> &headers = task->GetResponse().GetHeaders();
        for (const auto &entry : headers) {
            NETSTACK_LOGI("TestMethod() HEAD = %{public}s : %{public}s", entry.first.c_str(),  entry.second.c_str());
        }
    } else if (task->GetResponse().GetResponseCode() == 405) {
        NETSTACK_LOGI("TestMethod() %{public}s don't support, response 405", curlMethod);
    } else {
        NETSTACK_LOGI("TestMethod() = failed %{public}s", curlMethod);
    }

    EXPECT_TRUE(task->onSucceeded_ != nullptr);
}

HWTEST_F(HttpClientTaskTest, OnSuccessTest002, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    std::string method = "GET";
    httpReq.SetURL(url);
    httpReq.SetMethod(method);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);
    task->OnSuccess([task](const HttpClientRequest &request, const HttpClientResponse &response) {});
    task->Start();

    char *curlMethod = nullptr;
    while (task->GetCurlHandle() == nullptr) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    curl_easy_getinfo(task->GetCurlHandle(), CURLINFO_EFFECTIVE_METHOD, &curlMethod);

    while (task->GetStatus() != TaskStatus::IDLE) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (task->GetResponse().GetResponseCode() == 200) {
        if (curlMethod == method) {
            NETSTACK_LOGI("TestMethod() = %{public}s OK!", curlMethod);
        }
        const std::map<std::string, std::string> &headers = task->GetResponse().GetHeaders();
        for (const auto &entry : headers) {
            NETSTACK_LOGI("TestMethod() HEAD = %{public}s : %{public}s", entry.first.c_str(),  entry.second.c_str());
        }
    } else if (task->GetResponse().GetResponseCode() == 405) {
        NETSTACK_LOGI("TestMethod() %{public}s don't support, response 405", curlMethod);
    } else {
        NETSTACK_LOGI("TestMethod() = failed %{public}s", curlMethod);
    }

    EXPECT_TRUE(task->onSucceeded_ != nullptr);
}

HWTEST_F(HttpClientTaskTest, OnSuccessTest003, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/post";
    std::string method = "POST";
    httpReq.SetURL(url);
    httpReq.SetMethod(method);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);
    task->OnSuccess([task](const HttpClientRequest &request, const HttpClientResponse &response) {});
    task->Start();

    char *curlMethod = nullptr;
    while (task->GetCurlHandle() == nullptr) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    curl_easy_getinfo(task->GetCurlHandle(), CURLINFO_EFFECTIVE_METHOD, &curlMethod);

    while (task->GetStatus() != TaskStatus::IDLE) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (task->GetResponse().GetResponseCode() == 200) {
        if (curlMethod == method) {
            NETSTACK_LOGI("TestMethod() = %{public}s OK!", curlMethod);
        }
        const std::map<std::string, std::string> &headers = task->GetResponse().GetHeaders();
        for (const auto &entry : headers) {
            NETSTACK_LOGI("TestMethod() HEAD = %{public}s : %{public}s", entry.first.c_str(),  entry.second.c_str());
        }
    } else if (task->GetResponse().GetResponseCode() == 405) {
        NETSTACK_LOGI("TestMethod() %{public}s don't support, response 405", curlMethod);
    } else {
        NETSTACK_LOGI("TestMethod() = failed %{public}s", curlMethod);
    }

    EXPECT_TRUE(task->onSucceeded_ != nullptr);
}

HWTEST_F(HttpClientTaskTest, OnSuccessTest004, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/put";
    std::string method = "PUT";
    httpReq.SetURL(url);
    httpReq.SetMethod(method);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);
    task->OnSuccess([task](const HttpClientRequest &request, const HttpClientResponse &response) {});
    task->Start();

    char *curlMethod = nullptr;
    while (task->GetCurlHandle() == nullptr) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    curl_easy_getinfo(task->GetCurlHandle(), CURLINFO_EFFECTIVE_METHOD, &curlMethod);

    while (task->GetStatus() != TaskStatus::IDLE) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (task->GetResponse().GetResponseCode() == 200) {
        if (curlMethod == method) {
            NETSTACK_LOGI("TestMethod() = %{public}s OK!", curlMethod);
        }
        const std::map<std::string, std::string> &headers = task->GetResponse().GetHeaders();
        for (const auto &entry : headers) {
            NETSTACK_LOGI("TestMethod() HEAD = %{public}s : %{public}s", entry.first.c_str(),  entry.second.c_str());
        }
    } else if (task->GetResponse().GetResponseCode() == 405) {
        NETSTACK_LOGI("TestMethod() %{public}s don't support, response 405", curlMethod);
    } else {
        NETSTACK_LOGI("TestMethod() = failed %{public}s", curlMethod);
    }

    EXPECT_TRUE(task->onSucceeded_ != nullptr);
}

HWTEST_F(HttpClientTaskTest, OnCancelTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    task->OnCancel([](const HttpClientRequest &request, const HttpClientResponse &response) {});

    EXPECT_TRUE(task->onCanceled_ != nullptr);
}

HWTEST_F(HttpClientTaskTest, OnFailTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    task->OnFail(
        [](const HttpClientRequest &request, const HttpClientResponse &response, const HttpClientError &error) {});

    EXPECT_TRUE(task->onFailed_ != nullptr);
}

HWTEST_F(HttpClientTaskTest, OnDataReceiveTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    task->OnDataReceive([](const HttpClientRequest &request, const uint8_t *data, size_t length) {});

    EXPECT_TRUE(task->onDataReceive_ != nullptr);
}

HWTEST_F(HttpClientTaskTest, OnProgressTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    task->OnProgress(
        [](const HttpClientRequest &request, u_long dltotal, u_long dlnow, u_long ultotal, u_long ulnow) {});

    EXPECT_TRUE(task->onProgress_ != nullptr);
}

HWTEST_F(HttpClientTaskTest, DataReceiveCallbackTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "https://www.baidu.com";
    const char *data = "https://www.baidu.com";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    auto *userData = task.get();
    size_t size = 10;
    size_t memBytes = 1;

    task->OnDataReceive([](const HttpClientRequest &request, const uint8_t *data, size_t length) {});
    size_t result = task->DataReceiveCallback(data, size, memBytes, userData);

    EXPECT_EQ(result, size * memBytes);
}

HWTEST_F(HttpClientTaskTest, DataReceiveCallbackTest002, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "https://www.baidu.com";
    const char *data = "https://www.baidu.com";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    auto *userData = task.get();
    size_t size = 10;
    size_t memBytes = 1;
    size_t result = task->DataReceiveCallback(data, size, memBytes, userData);

    EXPECT_EQ(result, size * memBytes);
}

HWTEST_F(HttpClientTaskTest, DataReceiveCallbackTest003, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    const char *data = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    auto *userData = task.get();
    size_t size = 10;
    size_t memBytes = 1;
    task->canceled_ = true;
    size_t result = task->DataReceiveCallback(data, size, memBytes, userData);

    EXPECT_EQ(result, 0);
    task->canceled_ = false;
}

HWTEST_F(HttpClientTaskTest, DataReceiveCallbackTest004, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    const char *data = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    auto *userData = task.get();
    size_t size = 10;
    size_t memBytes = 1;
    size_t result = task->DataReceiveCallback(data, size, memBytes, userData);

    EXPECT_EQ(result, size * memBytes);
}

HWTEST_F(HttpClientTaskTest, DataReceiveCallbackTest005, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    const char *data = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    auto *userData = task.get();
    size_t size = 10;
    size_t memBytes = 1;
    size_t result = task->DataReceiveCallback(data, size, memBytes, userData);

    EXPECT_EQ(result, size * memBytes);
}

HWTEST_F(HttpClientTaskTest, DataReceiveCallbackTest006, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    const char *data = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    HttpClientResponse resp;
    resp.result_ = "result1";
    task->SetResponse(resp);

    auto *userData = task.get();
    size_t size = 1;
    size_t memBytes = 1;
    size_t result = task->DataReceiveCallback(data, size, memBytes, userData);

    EXPECT_EQ(result, size * memBytes);
}

HWTEST_F(HttpClientTaskTest, ProgressCallbackTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "https://www.baidu.com";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    auto *userData = task.get();
    curl_off_t dltotal = 100;
    curl_off_t dlnow = 50;
    curl_off_t ultotal = 200;
    curl_off_t ulnow = 100;
    int result;

    result = task->ProgressCallback(userData, dltotal, dlnow, ultotal, ulnow);
    EXPECT_EQ(result, 0);
}

HWTEST_F(HttpClientTaskTest, ProgressCallbackTest002, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "https://www.baidu.com";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    auto *userData = task.get();
    curl_off_t dltotal = 100;
    curl_off_t dlnow = 50;
    curl_off_t ultotal = 200;
    curl_off_t ulnow = 100;
    int result;

    task->Cancel();
    result = task->ProgressCallback(userData, dltotal, dlnow, ultotal, ulnow);
    EXPECT_EQ(result, CURLE_ABORTED_BY_CALLBACK);
}

HWTEST_F(HttpClientTaskTest, ProgressCallbackTest003, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "https://www.baidu.com";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    auto *userData = task.get();
    curl_off_t dltotal = 100;
    curl_off_t dlnow = 50;
    curl_off_t ultotal = 200;
    curl_off_t ulnow = 100;

    int result = task->ProgressCallback(userData, dltotal, dlnow, ultotal, ulnow);
    EXPECT_EQ(result, 0);
}

HWTEST_F(HttpClientTaskTest, HeaderReceiveCallbackTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "https://www.baidu.com";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    auto *userData = task.get();
    const char *data = "Test Header";
    size_t size = 5;
    size_t memBytes = 2;
    size_t result = task->HeaderReceiveCallback(data, size, memBytes, userData);
    EXPECT_EQ(result, size * memBytes);
}

HWTEST_F(HttpClientTaskTest, HeaderReceiveCallbackTest002, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "https://www.baidu.com";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    auto *userData = task.get();

    const char *data = "Test Header";
    size_t size = HttpConstant::MAX_DATA_LIMIT + 1;
    size_t memBytes = 1;
    size_t result = task->HeaderReceiveCallback(data, size, memBytes, userData);
    EXPECT_EQ(result, 0);
}

HWTEST_F(HttpClientTaskTest, HeaderReceiveCallbackTest003, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    auto *userData = task.get();

    const char *data = "Test Header";
    size_t size = 5;
    size_t memBytes = 2;
    size_t result = task->HeaderReceiveCallback(data, size, memBytes, userData);
    EXPECT_EQ(result, size * memBytes);
}

HWTEST_F(HttpClientTaskTest, HeaderReceiveCallbackTest004, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);
    task->canceled_  = true;

    HttpClientResponse resp;
    const char *realHead = "test:data\r\n\r\n";
    resp.AppendHeader(realHead, strlen(realHead));
    task->SetResponse(resp);
    
    auto *userData = task.get();
    const char *data = "Test Header";
    size_t size = 5;
    size_t memBytes = 2;
    size_t result = task->HeaderReceiveCallback(data, size, memBytes, userData);
    EXPECT_TRUE(resp.headers_.empty());
    EXPECT_EQ(result, size * memBytes);
}

HWTEST_F(HttpClientTaskTest, HeaderReceiveCallbackTest005, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);
    task->onHeadersReceive_ = testCallbackconst;

    HttpClientResponse resp;
    const char *errorHead = "test:data\r\n";
    resp.AppendHeader(errorHead, strlen(errorHead));
    task->SetResponse(resp);
    
    auto *userData = task.get();
    const char *data = "Test Header";
    size_t size = 5;
    size_t memBytes = 2;
    size_t result = task->HeaderReceiveCallback(data, size, memBytes, userData);
    EXPECT_TRUE(resp.headers_.empty());
    EXPECT_EQ(result, size * memBytes);
}

HWTEST_F(HttpClientTaskTest, HeaderReceiveCallbackTest006, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    HttpClientResponse resp;
    const char *realHead = "test:data\r\n\r\n";
    resp.AppendHeader(realHead, strlen(realHead));
    task->SetResponse(resp);
    
    auto *userData = task.get();
    const char *data = "Test Header";
    size_t size = 5;
    size_t memBytes = 2;
    size_t result = task->HeaderReceiveCallback(data, size, memBytes, userData);
    EXPECT_TRUE(resp.headers_.empty());
    EXPECT_EQ(result, size * memBytes);
}

HWTEST_F(HttpClientTaskTest, ProcessResponseCodeTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "https://www.baidu.com";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);
    task->Start();

    while (task->GetStatus() != TaskStatus::IDLE) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    EXPECT_TRUE(task->ProcessResponseCode());
}

HWTEST_F(HttpClientTaskTest, ProcessResponseTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "https://www.baidu.com";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    CURLMsg msg;
    msg.data.result = CURLE_ABORTED_BY_CALLBACK;
    task->OnCancel([](const HttpClientRequest &request, const HttpClientResponse &response) {});
    task->ProcessResponse(&msg);
    EXPECT_TRUE(task->onCanceled_);

    msg.data.result = CURLE_FAILED_INIT;
    task->OnFail(
        [](const HttpClientRequest &request, const HttpClientResponse &response, const HttpClientError &error) {});
    task->ProcessResponse(&msg);
    EXPECT_TRUE(task->onFailed_);

    msg.data.result = CURLE_OK;
    task->response_.SetResponseCode(ResponseCode::NOT_MODIFIED);
    task->OnSuccess([task](const HttpClientRequest &request, const HttpClientResponse &response) {});
    task->ProcessResponse(&msg);
    EXPECT_TRUE(task->onSucceeded_);

    task->curlHandle_ = nullptr;
    task->OnFail(
        [](const HttpClientRequest &request, const HttpClientResponse &response, const HttpClientError &error) {});
    task->ProcessResponse(&msg);
    EXPECT_TRUE(task->onFailed_);
}

HWTEST_F(HttpClientTaskTest, SetResponseTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "https://www.baidu.com";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    HttpClientResponse resp;
    resp.result_ = "result1";
    task->SetResponse(resp);

    EXPECT_EQ(task->response_.result_, "result1");
}

HWTEST_F(HttpClientTaskTest, GetHttpProxyInfoTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;

    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);
    httpReq.SetHttpProxyType(USE_SPECIFIED);
    HttpProxy proxy;
    proxy.host = "192.168.147.60";
    proxy.port = 8888;
    proxy.exclusions = "www.httpbin.org";
    proxy.tunnel = false;
    httpReq.SetHttpProxy(proxy);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    std::string host;
    std::string exclusions;
    int32_t port = 0;
    bool tunnel = false;
    task->GetHttpProxyInfo(host, port, exclusions, tunnel);

    EXPECT_EQ(host, "192.168.147.60");
    EXPECT_EQ(port, 8888);
    EXPECT_EQ(exclusions, "www.httpbin.org");
    EXPECT_FALSE(tunnel);
}

HWTEST_F(HttpClientTaskTest, GetHttpProxyInfoTest002, TestSize.Level1)
{
    HttpClientRequest httpReq;

    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);
    httpReq.SetHttpProxyType(NOT_USE);
    HttpProxy proxy;
    proxy.host = "192.168.147.60";
    proxy.port = 8888;
    proxy.exclusions = "www.httpbin.org";
    proxy.tunnel = false;
    httpReq.SetHttpProxy(proxy);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    std::string host;
    std::string  exclusions;
    int32_t port = 0;
    bool tunnel = false;
    task->GetHttpProxyInfo(host, port, exclusions, tunnel);

    EXPECT_EQ(host, "");
    EXPECT_EQ(port, 0);
    EXPECT_EQ(exclusions, "");
    EXPECT_FALSE(tunnel);
}

HWTEST_F(HttpClientTaskTest, SslCtxFunctionTest001, TestSize.Level1)
{
    CURL *curl = nullptr;
    SSL_CTX *sslCtx = nullptr;
    HttpClientRequest httpReq;
    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);
    EXPECT_EQ(task->SslCtxFunction(curl, sslCtx), CURLE_SSL_CERTPROBLEM);
}

HWTEST_F(HttpClientTaskTest, SslCtxFunctionTest002, TestSize.Level1)
{
    CURL *curl = nullptr;
    SSL_CTX *sslCtx = SSL_CTX_new(TLS_client_method());
    HttpClientRequest httpReq;
    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);
    EXPECT_EQ(task->SslCtxFunction(curl, sslCtx), CURLE_OK);
    SSL_CTX_free(sslCtx);
}

HWTEST_F(HttpClientTaskTest, SetStatus001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    task->SetStatus(RUNNING);
    EXPECT_EQ(RUNNING, task->GetStatus());
}

HWTEST_F(HttpClientTaskTest, GetStatusTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    EXPECT_EQ(IDLE, task->GetStatus());
}

HWTEST_F(HttpClientTaskTest, GetStatusTest002, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "https://www.baidu.com";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    task->SetStatus(TaskStatus::RUNNING);

    EXPECT_EQ(task->GetStatus(), RUNNING);
}

HWTEST_F(HttpClientTaskTest, StartTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.baidu.com";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    task->SetStatus(TaskStatus::RUNNING);
    bool result = task->Start();
    EXPECT_FALSE(result);
}

HWTEST_F(HttpClientTaskTest, StartTest002, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.baidu.com";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    task->error_.SetErrorCode(HttpErrorCode::HTTP_UNSUPPORTED_PROTOCOL);
    bool result = task->Start();
    EXPECT_FALSE(result);
}

HWTEST_F(HttpClientTaskTest, StartTest003, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.baidu.com";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    bool result = task->Start();
    EXPECT_TRUE(result);
}

HWTEST_F(HttpClientTaskTest, StartTest004, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "https://www.baidu.com";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    task->error_.SetErrorCode(HttpErrorCode::HTTP_UNSUPPORTED_PROTOCOL);
    bool result = task->Start();
    EXPECT_FALSE(result);
}

HWTEST_F(HttpClientTaskTest, ProcessCookieTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/cookies/set/name1/value1";
    httpReq.SetURL(url);
    httpReq.SetHeader("content-type", "text/plain");
    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);
    task->Start();

    while (task->GetStatus() != TaskStatus::IDLE) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    auto ret = task->GetResponse().GetResponseCode();
    EXPECT_TRUE(ret == ResponseCode::OK || ret == 0);
}

HWTEST_F(HttpClientTaskTest, ProcessCookieTest002, TestSize.Level1)
{
    HttpClientRequest httpReq;
    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);
    task->ProcessCookie(task->curlHandle_);
    EXPECT_NE(task->curlHandle_, nullptr);
}

HWTEST_F(HttpClientTaskTest, GetTimingFromCurlTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);
    task->curlHandle_ = nullptr;
    // CURLINFO info;
    auto ret = task->GetTimingFromCurl(task->curlHandle_, CURLINFO_TOTAL_TIME_T);
    EXPECT_EQ(ret, 0);
}

HWTEST_F(HttpClientTaskTest, DumpHttpPerformanceTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);
    task->DumpHttpPerformance();
    EXPECT_NE(task->curlHandle_, nullptr);
}

HWTEST_F(HttpClientTaskTest, CancelTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "https://www.baidu.com";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    task->Cancel();
    EXPECT_TRUE(task->canceled_);
}

HWTEST_F(HttpClientTaskTest, SetServerSSLCertOption001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "https://www.baidu.com";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    bool result = task->SetServerSSLCertOption(task->curlHandle_);
    EXPECT_TRUE(result);
}

HWTEST_F(HttpClientTaskTest, SetServerSSLCertOption002, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "";
    httpReq.SetURL(url);

    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);

    bool result = task->SetServerSSLCertOption(task->curlHandle_);
    EXPECT_TRUE(result);
}

#if HAS_NETMANAGER_BASE
HWTEST_F(HttpClientTaskTest, SetServerSSLCertOption_ShouldReturnTrue_WhenGetPinSetForHostNameReturnsZeroAndPinsIsEmpty,
         TestSize.Level2)
{
    auto configInstance = OHOS::NetManagerStandard::NetworkSecurityConfig::GetInstance();
    OHOS::NetManagerStandard::DomainConfig config = {};
    OHOS::NetManagerStandard::Domain domain;
    domain.domainName_ = "https://www.example.com";
    domain.includeSubDomains_ = false;
    config.domains_.push_back(domain);
    OHOS::NetManagerStandard::Pin pin;
    pin.digestAlgorithm_ = "TEST";
    pin.digest_ = "TEST";
    config.pinSet_.pins_.push_back(pin);
    configInstance.domainConfigs_.push_back(config);
    HttpClientRequest httpReq;
    std::string url = "https://www.example.com";
    httpReq.SetURL(url);
    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);
    EXPECT_TRUE(task->SetServerSSLCertOption(task->curlHandle_));
}
#endif

class MockCurl {
public:
    MOCK_METHOD0(easy_init, CURL *());
};

HWTEST_F(HttpClientTaskTest, HttpClientTask_ShouldNotCreate_WhenCurlInitFails, TestSize.Level0)
{
    MockCurl mockCurl;
    ON_CALL(mockCurl, easy_init).WillByDefault(Return(nullptr));

    HttpClientRequest request;
    HttpClientTask httpClientTask(request);
    ASSERT_EQ(httpClientTask.GetStatus(), IDLE);
    ASSERT_EQ(httpClientTask.GetType(), DEFAULT);
    ASSERT_EQ(httpClientTask.canceled_, false);

    HttpSession &session = HttpSession::GetInstance();
    auto httpClientTask2 = session.CreateTask(request, UPLOAD, "testFakePath");
    ASSERT_EQ(httpClientTask2->GetType(), UPLOAD);
}

HWTEST_F(HttpClientTaskTest, ProcessRequestTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://www.httpbin.org/get";
    httpReq.SetURL(url);
    httpReq.SetHeader("content-type", "application/json");
    
    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);
    task->Start();

    while (task->GetStatus() != TaskStatus::IDLE) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    const HttpClientRequest &request = task->GetRequest();
    EXPECT_EQ(request.GetURL(), url);
}

HWTEST_F(HttpClientTaskTest, ProcessErrorTest001, TestSize.Level1)
{
    HttpClientRequest httpReq;
    std::string url = "http://nonexistenturl:8080";
    httpReq.SetURL(url);
    httpReq.SetHeader("content-type", "text/plain");
    
    HttpSession &session = HttpSession::GetInstance();
    auto task = session.CreateTask(httpReq);
    task->Start();

    while (task->GetStatus() != TaskStatus::IDLE) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    const HttpClientError &error = task->GetError();
    EXPECT_NE(error.GetErrorCode(), 0);
    EXPECT_FALSE(error.GetErrorMessage().empty());
}
} // namespace
