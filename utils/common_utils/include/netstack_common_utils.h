/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#ifndef COMMUNICATIONNETSTACK_COMMON_UTILS_H
#define COMMUNICATIONNETSTACK_COMMON_UTILS_H

#include <iosfwd>
#include <list>
#include <vector>
#include <string>

namespace OHOS::NetStack::CommonUtils {
static const std::string DOMAIN_TYPE_HTTP_REQUEST = "httpRequest";
static const std::string DOMAIN_TYPE_WEBSOCKET_REQUEST = "webSocket";
enum SdkVersion {
    FIVE = 5,
    SIX,
    SEVEN,
    EIGHT,
    NINE,
    TEN,
    ELEVEN,
    TWELVE
};

std::vector<std::string> Split(const std::string &str, const std::string &sep);

std::vector<std::string> Split(const std::string &str, const std::string &sep, size_t size);

std::string Strip(const std::string &str, char ch = ' ');

std::string ToLower(const std::string &s);

std::string ToString(const std::list<std::string> &lists, char tab = ',');

bool HasInternetPermission();

bool IsAtomicService(std::string &bundleName);

bool IsAllowedHostname(const std::string &bundleName, const std::string &domainType, const std::string &url);

bool EndsWith(const std::string &str, const std::string &suffix);

std::string Trim(std::string str);

bool IsMatch(const std::string &str, const std::string &patternStr);

std::string InsertCharBefore(const std::string &input, const char from, const char preChar, const char nextChar);

std::string ReplaceCharacters(const std::string &input);

bool IsRegexValid(const std::string &regex);

std::string GetProtocolFromURL(const std::string &url);

std::string GetPortFromURL(const std::string &url);

std::string GetHostnameFromURL(const std::string& url);

std::string GetHostnameWithProtocolAndPortFromURL(const std::string& url);

bool IsExcluded(const std::string &str, const std::string &exclusions, const std::string &split);

bool IsHostNameExcluded(const std::string &url, const std::string &exclusions, const std::string &split);

bool IsValidIP(const std::string& ip, int af);

bool IsValidIPV4(const std::string &ip);

bool IsValidIPV6(const std::string &ip);

std::string MaskIpv4(std::string &maskedResult);

std::string MaskIpv6(std::string &maskedResult);

std::string AnonymizeIp(std::string &input);

std::string GetFileDataFromFilePath(const std::string& filePath);
} // namespace OHOS::NetStack::CommonUtils
#endif /* COMMUNICATIONNETSTACK_COMMON_UTILS_H */
