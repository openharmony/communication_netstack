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

#include "constant.h"
#include "netstack_common_utils.h"
#include "netstack_log.h"

#include "net_http_response.h"

namespace OHOS::NetStack::Http {
HttpResponse::HttpResponse() : responseCode_(0) {}

void HttpResponse::AppendResult(const void *data, size_t length)
{
    result_.append(static_cast<const char *>(data), length);
}

void HttpResponse::AppendRawHeader(const void *data, size_t length)
{
    rawHeader_.append(static_cast<const char *>(data), length);
}

void HttpResponse::SetResponseCode(uint32_t responseCode)
{
    responseCode_ = responseCode;
}

void HttpResponse::ParseHeaders()
{
    std::vector<std::string> vec = CommonUtils::Split(rawHeader_, HTTP_LINE_SEPARATOR);
    for (const auto &header : vec) {
        if (CommonUtils::Strip(header).empty()) {
            continue;
        }
        auto index = header.find(HTTP_HEADER_SEPARATOR);
        if (index == std::string::npos) {
            NETSTACK_LOGI("HEAD: %{public}s", CommonUtils::Strip(header).c_str());
            continue;
        }
        if (CommonUtils::ToLower(CommonUtils::Strip(header.substr(0, index))) ==
            RESPONSE_KEY_SET_COOKIE) {
            setCookie_.push_back(CommonUtils::Strip(header.substr(index + 1)));
            continue;
        }
        header_[CommonUtils::ToLower(CommonUtils::Strip(header.substr(0, index)))] =
            CommonUtils::Strip(header.substr(index + 1));
    }
}

void HttpResponse::AppendCookies(const void *data, size_t length)
{
    cookies_.append(static_cast<const char *>(data), length);
}

const std::string &HttpResponse::GetResult() const
{
    return result_;
}

uint32_t HttpResponse::GetResponseCode() const
{
    return responseCode_;
}

const std::map<std::string, std::string> &HttpResponse::GetHeader() const
{
    return header_;
}

const std::vector<std::string> &HttpResponse::GetsetCookie() const
{
    return setCookie_;
}

const std::string &HttpResponse::GetCookies() const
{
    return cookies_;
}

void HttpResponse::SetRawHeader(const std::string &header)
{
    rawHeader_ = header;
}

const std::string &HttpResponse::GetRawHeader() const
{
    return rawHeader_;
}

void HttpResponse::SetResult(const std::string &res)
{
    result_ = res;
}

void HttpResponse::SetCookies(const std::string &cookies)
{
    cookies_ = cookies;
}

void HttpResponse::SetResponseTime(const std::string &time)
{
    responseTime_ = time;
}

const std::string &HttpResponse::GetResponseTime() const
{
    return responseTime_;
}

void HttpResponse::SetRequestTime(const std::string &time)
{
    requestTime_ = time;
}

const std::string &HttpResponse::GetRequestTime() const
{
    return requestTime_;
}

void HttpResponse::SetWarning(const std::string &val)
{
    header_[WARNING] = val;
}
} // namespace OHOS::NetStack::Http