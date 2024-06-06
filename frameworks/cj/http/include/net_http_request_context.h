/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
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

#ifndef NET_HTTP_REQUEST_CONTEXT_H
#define NET_HTTP_REQUEST_CONTEXT_H

#include <queue>
#include <mutex>
#include <map>
#include <vector>
#include "curl/curl.h"
#include "ffi_remote_data.h"
#include "net_http_request.h"
#include "net_http_response.h"
#include "net_http_utils.h"
#include "ffi_structs.h"

namespace OHOS::NetStack::Http {

struct LoadBytes {
    LoadBytes() : nLen(0), tLen(0) {};
    LoadBytes(curl_off_t nowLen, curl_off_t totalLen): nLen(nowLen), tLen(totalLen) {};

    ~LoadBytes() = default;
    curl_off_t nLen;
    curl_off_t tLen;
};

struct RequestCallback {
    std::vector<std::function<void(CArrString)>> headersReceive;
    std::vector<std::function<void(CArrString)>> headersReceiveOnce;
    std::vector<std::function<void(char*)>> dataReceive;
    std::vector<std::function<void()>> dataEnd;
    std::vector<std::function<void(CDataReceiveProgressInfo)>> dataReceiveProgress;
    std::vector<std::function<void(CDataSendProgressInfo)>> dataSendProgress;
};

class RequestContext {
public:
    RequestContext();

    ~RequestContext();

    void StartTiming();

    void ParseParams(std::string url, CHttpRequestOptions *ops);

    HttpRequest options;

    HttpResponse response;

    [[nodiscard]] bool IsUsingCache() const;

    void SetCurlHeaderList(struct curl_slist *curlHeaderList);

    struct curl_slist *GetCurlHeaderList();

    void SetCacheResponse(const HttpResponse &cacheResponse);

    void SetResponseByCache();

    [[nodiscard]] int32_t GetErrorCode() const;

    [[nodiscard]] std::string GetErrorMessage() const;

    void SetErrorCode(int32_t code);

    void EnableRequestInStream();

    [[nodiscard]] bool IsRequestInStream() const;

    void SetDlLen(curl_off_t nowLen, curl_off_t totalLen);

    LoadBytes GetDlLen();

    void SetUlLen(curl_off_t nowLen, curl_off_t totalLen);

    LoadBytes GetUlLen();

    bool CompareWithLastElement(curl_off_t nowLen, curl_off_t totalLen);

    void SetTempData(const void *data, size_t size);

    std::string GetTempData();

    void PopTempData();

    void CachePerformanceTimingItem(const std::string &key, double value);

    void StopAndCachePerformanceTiming(const char *key);

    void SetPerformanceTimingToResult(CHttpResponse &resp);

    void SetMultipart(curl_mime *multipart);

    void SetParseOK();

    bool IsParseOK() const;

    void SetExecOK(bool ok);

    bool IsExecOK() const;

    void SetPermissionDenied(bool deny);

    bool IsPermissionDenied() const;

    void Destroy();

    bool IsDestroyed() const;

    void SendResponse();

    std::function<void(CHttpResponse)> respCallback;

    std::shared_ptr<RequestCallback> streamingCallback{nullptr};
private:
    bool usingCache_ = true;
    bool requestInStream_ = false;
    std::mutex dlLenLock_;
    std::mutex ulLenLock_;
    std::mutex tempDataLock_;
    std::queue<std::string> tempData_;
    HttpResponse cacheResponse_;
    std::queue<LoadBytes> dlBytes_;
    std::queue<LoadBytes> ulBytes_;
    struct curl_slist *curlHeaderList_ = nullptr;
    TimerMap timerMap_;
    std::map<std::string, double> performanceTimingMap_;
    curl_mime *multipart_ = nullptr;
    int32_t errCode_ = 0;
    std::string errMsg_;
    bool parseok_ = false;
    bool requestOK_ = false;
    bool permissionDenied_ = false;
    bool isDestroyed_ = false;

    bool GetRequestBody(CArrUI8 extraData);
    void HandleMethodForGet(CArrUI8 extraData);
    void ParseUsingHttpProxy(CHttpProxy* proxy, bool useDefault);
    bool ParseExtraData(CArrUI8 data);
    void ParseDnsServers(CArrString dns);
    void ParseMultiFormData(CArrMultiFormData multi);
    void ParseHeader(CArrString header);
};

class HttpRequestProxy : public OHOS::FFI::FFIData {
private:
    friend class OHOS::FFI::RuntimeType;
    friend class OHOS::FFI::TypeBase;
    static OHOS::FFI::RuntimeType *GetClassType()
    {
        static OHOS::FFI::RuntimeType runtimeType =
            OHOS::FFI::RuntimeType::Create<OHOS::FFI::FFIData>("HttpRequestProxy");
        return &runtimeType;
    }
public:
    OHOS::FFI::RuntimeType *GetRuntimeType() override
    {
        return GetClassType();
    }
public:
    RequestContext* Request(std::string url, CHttpRequestOptions *ops, bool isInStream);
    RequestContext* RequestInStream(std::string url, CHttpRequestOptions *ops);
    void Destroy();

    std::shared_ptr<RequestCallback> callbacks = std::make_shared<RequestCallback>();

    bool isDestroyed = false;
};

}
#endif