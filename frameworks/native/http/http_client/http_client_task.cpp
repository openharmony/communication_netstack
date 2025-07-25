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

#include "http_client_task.h"
#include "request_tracer.h"
#include "trace_events.h"
#include <unistd.h>
#ifdef HTTP_MULTIPATH_CERT_ENABLE
#include <openssl/ssl.h>
#endif
#include <iostream>
#include <memory>
#include "http_client.h"
#include "http_client_constant.h"
#include "http_client_time.h"
#include "net_conn_client.h"
#include "network_security_config.h"
#include "netstack_common_utils.h"
#include "netstack_log.h"
#include "timing.h"
#if HAS_NETMANAGER_BASE
#include "http_client_network_message.h"
#include "netsys_client.h"
#endif
#include "netstack_hisysevent.h"

#define NETSTACK_CURL_EASY_SET_OPTION(handle, opt, data)                                                 \
    do {                                                                                                 \
        CURLcode result = curl_easy_setopt(handle, opt, data);                                           \
        if (result != CURLE_OK) {                                                                        \
            const char *err = curl_easy_strerror(result);                                                \
            error_.SetCURLResult(result);                                                                \
            NETSTACK_LOGE("Failed to set option: %{public}s, %{public}s %{public}d", #opt, err, result); \
            return false;                                                                                \
        }                                                                                                \
    } while (0)

namespace OHOS {
namespace NetStack {
namespace HttpClient {

static const size_t MAX_LIMIT = HttpConstant::MAX_DATA_LIMIT;
static constexpr const char *HTTP_AF_ONLYV4 = "ONLY_V4";
static constexpr const char *HTTP_AF_ONLYV6 = "ONLY_V6";

std::atomic<uint32_t> HttpClientTask::nextTaskId_(0);

bool CheckFilePath(const std::string &fileName, std::string &realPath)
{
    char tmpPath[PATH_MAX] = {0};
    if (!realpath(static_cast<const char *>(fileName.c_str()), tmpPath)) {
        NETSTACK_LOGE("file name is error");
        return false;
    }

    realPath = tmpPath;
    return true;
}

HttpClientTask::HttpClientTask(const HttpClientRequest &request)
    : HttpClientTask(request, DEFAULT, std::string())
{
}

HttpClientTask::HttpClientTask(const HttpClientRequest &request, TaskType type, const std::string &filePath)
    : request_(request),
      type_(type),
      status_(IDLE),
      taskId_(nextTaskId_++),
      curlHeaderList_(nullptr),
      canceled_(false),
      filePath_(filePath),
      file_(nullptr),
      trace_(std::make_unique<RequestTracer::Trace>("HttpClientTask" + std::to_string(taskId_)))
{
    curlHandle_ = curl_easy_init();
    if (!curlHandle_) {
        NETSTACK_LOGE("Failed to create task!");
        return;
    }

    SetCurlOptions();
#if HAS_NETMANAGER_BASE
    networkProfilerUtils_ = std::make_unique<NetworkProfilerUtils>();
#endif
}

HttpClientTask::~HttpClientTask()
{
    NETSTACK_LOGD("Destroy: taskId_=%{public}d", taskId_);
    if (curlHeaderList_ != nullptr) {
        curl_slist_free_all(curlHeaderList_);
        curlHeaderList_ = nullptr;
    }

    if (curlHandle_) {
        curl_easy_cleanup(curlHandle_);
        curlHandle_ = nullptr;
    }

    if (file_ != nullptr) {
        fclose(file_);
        file_ = nullptr;
    }
}

uint32_t HttpClientTask::GetHttpVersion(HttpProtocol ptcl) const
{
    if (ptcl == HttpProtocol::HTTP1_1) {
        NETSTACK_LOGD("CURL_HTTP_VERSION_1_1");
        return CURL_HTTP_VERSION_1_1;
    } else if (ptcl == HttpProtocol::HTTP2) {
        NETSTACK_LOGD("CURL_HTTP_VERSION_2_0");
        return CURL_HTTP_VERSION_2_0;
    } else if (ptcl == HttpProtocol::HTTP3) {
        NETSTACK_LOGD("CURL_HTTP_VERSION_3");
        return CURL_HTTP_VERSION_3;
    }
    return CURL_HTTP_VERSION_NONE;
}

void HttpClientTask::GetHttpProxyInfo(std::string &host, int32_t &port, std::string &exclusions, bool &tunnel)
{
    if (request_.GetHttpProxyType() == HttpProxyType::USE_SPECIFIED) {
        HttpProxy proxy = request_.GetHttpProxy();
        host = proxy.host;
        port = proxy.port;
        exclusions = proxy.exclusions;
        tunnel = proxy.tunnel;
    } else {
        using namespace NetManagerStandard;
        NetManagerStandard::HttpProxy httpProxy;
        NetConnClient::GetInstance().GetDefaultHttpProxy(httpProxy);
        host = httpProxy.GetHost();
        port = httpProxy.GetPort();
        exclusions = CommonUtils::ToString(httpProxy.GetExclusionList());
    }
}

[[maybe_unused]] void TrustUser0AndUserCa(std::vector<std::string> &certs)
{
#ifdef HTTP_MULTIPATH_CERT_ENABLE
    if (NetManagerStandard::NetworkSecurityConfig::GetInstance().TrustUser0Ca()) {
        certs.emplace_back(HttpConstant::USER_CERT_ROOT_PATH);
    }
    if (NetManagerStandard::NetworkSecurityConfig::GetInstance().TrustUserCa()) {
        certs.emplace_back(HttpConstant::USER_CERT_BASE_PATH +
                           std::to_string(getuid() / HttpConstant::UID_TRANSFORM_DIVISOR));
    }
#endif
}

CURLcode HttpClientTask::SslCtxFunction(CURL *curl, void *sslCtx)
{
#ifdef HTTP_MULTIPATH_CERT_ENABLE
    if (sslCtx == nullptr) {
        NETSTACK_LOGE("sslCtx is null");
        return CURLE_SSL_CERTPROBLEM;
    }
    std::vector<std::string> certs;
    TrustUser0AndUserCa(certs);
    certs.emplace_back(HttpConstant::HTTP_PREPARE_CA_PATH);

    for (const auto &path : certs) {
        if (path.empty() || access(path.c_str(), F_OK) != 0) {
            NETSTACK_LOGD("certificate directory path is not exist");
            continue;
        }
        if (!SSL_CTX_load_verify_locations(static_cast<SSL_CTX *>(sslCtx), nullptr, path.c_str())) {
            NETSTACK_LOGE("loading certificates from directory error.");
            continue;
        }
    }

    if (access(request_.GetCaPath().c_str(), F_OK) != 0) {
        NETSTACK_LOGD("certificate directory path is not exist");
    } else if (!SSL_CTX_load_verify_locations(static_cast<SSL_CTX *>(sslCtx), request_.GetCaPath().c_str(), nullptr)) {
        NETSTACK_LOGE("loading certificates from context cert error.");
    }
#endif // HTTP_MULTIPATH_CERT_ENABLE
    return CURLE_OK;
}

bool HttpClientTask::SetSSLCertOption(CURL *handle)
{
#ifndef WINDOWS_PLATFORM
#ifdef HTTP_MULTIPATH_CERT_ENABLE
    curl_ssl_ctx_callback sslCtxFunc = [](CURL *curl, void *sslCtx, void *parm) -> CURLcode {
        HttpClientTask *task = static_cast<HttpClientTask *>(parm);
        if (!task) {
            return CURLE_SSL_CERTPROBLEM;
        }
        task->GetTrace().Tracepoint(TraceEvents::TLS);
        return task->SslCtxFunction(curl, sslCtx);
    };
    NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_SSL_CTX_FUNCTION, sslCtxFunc);
    NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_SSL_CTX_DATA, this);
    NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_CAINFO, nullptr);
    NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_SSL_VERIFYPEER, 1L);
    NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_SSL_VERIFYHOST, 2L);
#else
    NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_CAINFO, nullptr);
    NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_SSL_VERIFYPEER, 0L);
    NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_SSL_VERIFYHOST, 0L);
#endif // HTTP_MULTIPATH_CERT_ENABLE
#else
    NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_SSL_VERIFYPEER, 0L);
    NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_SSL_VERIFYHOST, 0L);
#endif // WINDOWS_PLATFORM
    SetServerSSLCertOption(handle);

    HttpClientCert clientCert = request_.GetClientCert();
    if (clientCert.certPath.empty()) {
        NETSTACK_LOGD("SetSSLCertOption param is empty.");
        return false;
    }
    NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_SSLCERT, clientCert.certPath.c_str());
    if (!clientCert.keyPath.empty()) {
        NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_SSLKEY, clientCert.keyPath.c_str());
    }
    if (!clientCert.certType.empty()) {
        NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_SSLCERTTYPE, clientCert.certType.c_str());
    }
    if (clientCert.keyPassword.length() > 0) {
        NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_KEYPASSWD, clientCert.keyPassword.c_str());
    }
    return true;
}

bool HttpClientTask::SetRequestOption(CURL *handle)
{
    NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_TIMEOUT_MS, request_.GetTimeout());
    NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_CONNECTTIMEOUT_MS, request_.GetConnectTimeout());
    const std::string range = GetRangeString();
    if (!range.empty()) {
        if (request_.GetMethod() == HttpConstant::HTTP_METHOD_PUT) {
            error_.SetErrorCode(HttpErrorCode::HTTP_CURLE_RANGE_ERROR);
            NETSTACK_LOGE("For HTTP PUT uploads this option should not be used");
            return false;
        }
        NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_RANGE, range.c_str());
    } else {
        // Some servers don't like requests that are made without a user-agent field, so we provide one
        NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_USERAGENT, HttpConstant::HTTP_DEFAULT_USER_AGENT);
    }
    SetSSLCertOption(handle);
    SetDnsCacheOption(handle);
    SetIpResolve(handle);
    return true;
}

bool HttpClientTask::SetOtherCurlOption(CURL *handle)
{
    // set proxy
    std::string host;
    std::string exclusions;
    int32_t port = 0;
    bool tunnel = false;
    std::string url = request_.GetURL();
    GetHttpProxyInfo(host, port, exclusions, tunnel);
    if (!host.empty() && !CommonUtils::IsHostNameExcluded(url, exclusions, ",")) {
        NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_PROXY, host.c_str());
        NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_PROXYPORT, port);
        auto curlTunnelValue = (url.find("https://") != std::string::npos) ? 1L : 0L;
        NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_HTTPPROXYTUNNEL, curlTunnelValue);
        auto proxyType = (host.find("https://") != std::string::npos) ? CURLPROXY_HTTPS : CURLPROXY_HTTP;
        NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_PROXYTYPE, proxyType);
    }

    const std::string range = GetRangeString();
    if (!range.empty()) {
        if (request_.GetMethod() == HttpConstant::HTTP_METHOD_PUT) {
            error_.SetErrorCode(HttpErrorCode::HTTP_CURLE_RANGE_ERROR);
            NETSTACK_LOGE("For HTTP PUT uploads this option should not be used");
            return false;
        }
        NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_RANGE, range.c_str());
    } else {
        // Some servers don't like requests that are made without a user-agent field, so we provide one
        NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_USERAGENT, HttpConstant::HTTP_DEFAULT_USER_AGENT);
    }

    SetSSLCertOption(handle);
    SetDnsCacheOption(handle);
    SetIpResolve(handle);

#ifdef HTTP_CURL_PRINT_VERBOSE
    NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_VERBOSE, 1L);
#endif

#ifndef WINDOWS_PLATFORM
    NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_ACCEPT_ENCODING, "");
#endif

    return true;
}

bool HttpClientTask::SetIpResolve(CURL *handle)
{
    std::string addressFamily = request_.GetAddressFamily();
    if (addressFamily.empty()) {
#if HAS_NETMANAGER_BASE
        if (!NetSysIsIpv6Enable(0)) {
            NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
        }
#endif
        return true;
    }
    if (addressFamily.compare(HTTP_AF_ONLYV4) == 0) {
        NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);
    }
    if (addressFamily.compare(HTTP_AF_ONLYV6) == 0) {
        NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V6);
    }
    return true;
}

std::string HttpClientTask::GetRangeString() const
{
    bool isSetFrom = request_.GetResumeFrom() >= MIN_RESUM_NUMBER;
    bool isSetTo = request_.GetResumeTo() >= MIN_RESUM_NUMBER;
    if (!isSetTo && !isSetFrom) {
        return "";
    } else if (!isSetTo && isSetFrom) {
        return std::to_string(request_.GetResumeFrom()) + '-';
    } else if (isSetTo && !isSetFrom) {
        return '-' + std::to_string(request_.GetResumeTo());
    } else if (request_.GetResumeTo() <= request_.GetResumeFrom()) {
        return "";
    } else {
        return std::to_string(request_.GetResumeFrom()) + '-' + std::to_string(request_.GetResumeTo());
    }
}

bool HttpClientTask::SetServerSSLCertOption(CURL *curl)
{
    auto hostname = CommonUtils::GetHostnameFromURL(request_.GetURL());
    if (!NetManagerStandard::NetworkSecurityConfig::GetInstance().IsPinOpenMode(hostname)) {
        std::string pins;
        auto ret = NetManagerStandard::NetworkSecurityConfig::GetInstance().GetPinSetForHostName(hostname, pins);
        if (ret != 0 || pins.empty()) {
            NETSTACK_LOGD("Get no pin set by host name invalid");
        } else {
            NETSTACK_CURL_EASY_SET_OPTION(curl, CURLOPT_PINNEDPUBLICKEY, pins.c_str());
        }
    }

    return true;
}

bool HttpClientTask::SetUploadOptions(CURL *handle)
{
    if (filePath_.empty()) {
        NETSTACK_LOGE("HttpClientTask::SetUploadOptions() filePath_ is empty");
        error_.SetErrorCode(HttpErrorCode::HTTP_UPLOAD_FAILED);
        return false;
    }

    std::string realPath;
    if (!CheckFilePath(filePath_, realPath)) {
        NETSTACK_LOGE("filePath_ does not exist! ");
        error_.SetErrorCode(HttpErrorCode::HTTP_UPLOAD_FAILED);
        return false;
    }

    file_ = fopen(realPath.c_str(), "rb");
    if (file_ == nullptr) {
        NETSTACK_LOGE("HttpClientTask::SetUploadOptions() Failed to open file");
        error_.SetErrorCode(HttpErrorCode::HTTP_UPLOAD_FAILED);
        return false;
    }

    fseek(file_, 0, SEEK_END);
    long size = ftell(file_);
    rewind(file_);

    // Set the file data and file size to upload
    NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_READDATA, file_);
    NETSTACK_LOGD("CURLOPT_INFILESIZE=%{public}ld", size);
    NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_INFILESIZE, size);
    NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_UPLOAD, 1L);

    return true;
}

bool HttpClientTask::SetTraceOptions(CURL *curl)
{
    NETSTACK_CURL_EASY_SET_OPTION(curl, CURLOPT_RESOLVER_START_DATA, this);
    NETSTACK_CURL_EASY_SET_OPTION(curl, CURLOPT_RESOLVER_START_FUNCTION,
                                  +[](void *, void *, void *clientp) {
        if (!clientp) {
            NETSTACK_LOGE("resolver_start_function clientp pointer is null");
            return 0;
        }
        HttpClientTask *task = static_cast<HttpClientTask *>(clientp);
        task->GetTrace().Tracepoint(TraceEvents::DNS);
        return 0;
    });

    NETSTACK_CURL_EASY_SET_OPTION(curl, CURLOPT_SOCKOPTDATA, this);
    NETSTACK_CURL_EASY_SET_OPTION(curl, CURLOPT_SOCKOPTFUNCTION,
                                  +[](void *clientp, curl_socket_t, curlsocktype) {
        if (!clientp) {
            NETSTACK_LOGE("sockopt_functon clientp pointer is null");
            return 0;
        }
        HttpClientTask *task = static_cast<HttpClientTask *>(clientp);
        task->GetTrace().Tracepoint(TraceEvents::TCP);
        return CURL_SOCKOPT_OK;
    });

    //this option may be overriden if HTTP_MULTIPATH_CERT_ENABLE enabled
    NETSTACK_CURL_EASY_SET_OPTION(curl, CURLOPT_SSL_CTX_DATA, this);
    NETSTACK_CURL_EASY_SET_OPTION(curl, CURLOPT_SSL_CTX_FUNCTION,
                                  +[](CURL *, void *, void *clientp) {
        if (!clientp) {
            NETSTACK_LOGE("ssl_ctx func clientp pointer is null");
            return 0;
        }
        HttpClientTask *task = static_cast<HttpClientTask *>(clientp);
        task->GetTrace().Tracepoint(TraceEvents::TLS);
        return CURL_SOCKOPT_OK;
    });

    NETSTACK_CURL_EASY_SET_OPTION(curl, CURLOPT_PREREQDATA, this);
    NETSTACK_CURL_EASY_SET_OPTION(curl, CURLOPT_PREREQFUNCTION,
                                  +[](void *clientp, char *, char *, int, int) {
        if (!clientp) {
            NETSTACK_LOGE("prereq_functon clientp pointer is null");
            return CURL_PREREQFUNC_OK;
        }
        HttpClientTask *task = static_cast<HttpClientTask *>(clientp);
        task->GetTrace().Tracepoint(TraceEvents::SENDING);
        return CURL_PREREQFUNC_OK;
    });
    return true;
}

bool HttpClientTask::SetCurlOptions()
{
    auto method = request_.GetMethod();
    if (method == HttpConstant::HTTP_METHOD_HEAD) {
        NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_NOBODY, 1L);
    }
    SetTraceOptions(curlHandle_);

    NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_URL, request_.GetURL().c_str());

    if (type_ == TaskType::UPLOAD) {
        if (!SetUploadOptions(curlHandle_)) {
            return false;
        }
    } else {
        if (!method.empty()) {
            NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_CUSTOMREQUEST, request_.GetMethod().c_str());
        }

        if ((method.empty() || method == HttpConstant::HTTP_METHOD_POST || method == HttpConstant::HTTP_METHOD_PUT) &&
            !request_.GetBody().empty()) {
            NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_POST, 1L);
            NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_POSTFIELDS, request_.GetBody().c_str());
            NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_POSTFIELDSIZE, request_.GetBody().size());
        }
    }

    NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
    NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_XFERINFODATA, this);
    NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_NOPROGRESS, 0L);

    NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_WRITEFUNCTION, DataReceiveCallback);
    NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_WRITEDATA, this);

    NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_HEADERFUNCTION, HeaderReceiveCallback);
    NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_HEADERDATA, this);

    if (curlHeaderList_ != nullptr) {
        curl_slist_free_all(curlHeaderList_);
        curlHeaderList_ = nullptr;
    }
    for (const auto &header : request_.GetHeaders()) {
        std::string headerStr = header.first + HttpConstant::HTTP_HEADER_SEPARATOR + header.second;
        curlHeaderList_ = curl_slist_append(curlHeaderList_, headerStr.c_str());
    }
    NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_HTTPHEADER, curlHeaderList_);

    NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_FOLLOWLOCATION, 1L);

    /* first #undef CURL_DISABLE_COOKIES in curl config */
    NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_COOKIEFILE, "");

    NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_NOSIGNAL, 1L);

    NETSTACK_CURL_EASY_SET_OPTION(curlHandle_, CURLOPT_HTTP_VERSION, GetHttpVersion(request_.GetHttpProtocol()));

    if (!SetRequestOption(curlHandle_)) {
        return false;
    }

    if (!SetOtherCurlOption(curlHandle_)) {
        return false;
    }

    return true;
}

bool HttpClientTask::Start()
{
    if (GetStatus() != TaskStatus::IDLE) {
        NETSTACK_LOGD("task is running, taskId_=%{public}d", taskId_);
        return false;
    }

    if (!CommonUtils::HasInternetPermission()) {
        NETSTACK_LOGE("Don't Has Internet Permission()");
        error_.SetErrorCode(HttpErrorCode::HTTP_PERMISSION_DENIED_CODE);
        return false;
    }

    if (!CommonUtils::IsCleartextPermitted(request_.GetURL(), "http://")) {
        NETSTACK_LOGE("Cleartext not permitted");
        error_.SetErrorCode(HttpErrorCode::HTTP_CLEARTEXT_NOT_PERMITTED);
        return false;
    }

    if (error_.GetErrorCode() != HttpErrorCode::HTTP_NONE_ERR) {
        NETSTACK_LOGE("error_.GetErrorCode()=%{public}d", error_.GetErrorCode());
        return false;
    }

    request_.SetRequestTime(HttpTime::GetNowTimeGMT());

    HttpSession &session = HttpSession::GetInstance();
    NETSTACK_LOGD("taskId_=%{public}d", taskId_);
    canceled_ = false;

    response_.SetRequestTime(HttpTime::GetNowTimeGMT());

    auto task = shared_from_this();
    session.StartTask(task);
    return true;
}

void HttpClientTask::Cancel()
{
    canceled_ = true;
}

void HttpClientTask::SetStatus(TaskStatus status)
{
    status_ = status;
}

TaskStatus HttpClientTask::GetStatus()
{
    return status_;
}

TaskType HttpClientTask::GetType()
{
    return type_;
}

const std::string &HttpClientTask::GetFilePath()
{
    return filePath_;
}

unsigned int HttpClientTask::GetTaskId()
{
    return taskId_;
}

void HttpClientTask::OnSuccess(
    const std::function<void(const HttpClientRequest &request, const HttpClientResponse &response)> &onSucceeded)
{
    onSucceeded_ = onSucceeded;
}

void HttpClientTask::OnCancel(
    const std::function<void(const HttpClientRequest &request, const HttpClientResponse &response)> &onCanceled)
{
    onCanceled_ = onCanceled;
}

void HttpClientTask::OnFail(
    const std::function<void(const HttpClientRequest &request, const HttpClientResponse &response,
                             const HttpClientError &error)> &onFailed)
{
    onFailed_ = onFailed;
}

void HttpClientTask::OnDataReceive(
    const std::function<void(const HttpClientRequest &request, const uint8_t *data, size_t length)> &onDataReceive)
{
    onDataReceive_ = onDataReceive;
}

void HttpClientTask::OnProgress(const std::function<void(const HttpClientRequest &request, u_long dlTotal, u_long dlNow,
                                                         u_long ulTotal, u_long ulNow)> &onProgress)
{
    onProgress_ = onProgress;
}

void HttpClientTask::OnHeadersReceive(const std::function<void(const HttpClientRequest &request,
        std::map<std::string, std::string> headersWithSetCookie)> &onHeadersReceive)
{
    onHeadersReceive_ = onHeadersReceive;
}

size_t HttpClientTask::DataReceiveCallback(const void *data, size_t size, size_t memBytes, void *userData)
{
    auto task = static_cast<HttpClientTask *>(userData);
    NETSTACK_LOGD("taskId=%{public}d size=%{public}zu memBytes=%{public}zu", task->taskId_, size, memBytes);

    if (task->canceled_) {
        NETSTACK_LOGD("canceled");
        return 0;
    }
    if (task->onDataReceive_) {
        HttpClientRequest request = task->request_;
        task->onDataReceive_(request, static_cast<const uint8_t *>(data), size * memBytes);
    }

    if (task->response_.GetResult().size() < MAX_LIMIT) {
        task->response_.AppendResult(data, size * memBytes);
    }

    return size * memBytes;
}

int HttpClientTask::ProgressCallback(void *userData, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal,
                                     curl_off_t ulnow)
{
    auto task = static_cast<HttpClientTask *>(userData);
    NETSTACK_LOGD("taskId=%{public}d dltotal=%{public}" CURL_FORMAT_CURL_OFF_T " dlnow=%{public}" CURL_FORMAT_CURL_OFF_T
                  " ultotal=%{public}" CURL_FORMAT_CURL_OFF_T " ulnow=%{public}" CURL_FORMAT_CURL_OFF_T,
                  task->taskId_, dltotal, dlnow, ultotal, ulnow);

    if (task->canceled_) {
        NETSTACK_LOGD("canceled");
        return CURLE_ABORTED_BY_CALLBACK;
    }

    if (task->onProgress_) {
        task->onProgress_(task->request_, dltotal, dlnow, ultotal, ulnow);
    }

    return 0;
}

size_t HttpClientTask::HeaderReceiveCallback(const void *data, size_t size, size_t memBytes, void *userData)
{
    auto task = static_cast<HttpClientTask *>(userData);
    task->GetTrace().Tracepoint(TraceEvents::RECEIVING);
    NETSTACK_LOGD("taskId=%{public}d size=%{public}zu memBytes=%{public}zu", task->taskId_, size, memBytes);

    if (size * memBytes > MAX_LIMIT) {
        NETSTACK_LOGE("size * memBytes(%{public}zu) > MAX_LIMIT(%{public}zu)", size * memBytes, MAX_LIMIT);
        return 0;
    }

    task->response_.AppendHeader(static_cast<const char *>(data), size * memBytes);
    if (!task->canceled_ && task->onHeadersReceive_ &&
        CommonUtils::EndsWith(task->response_.GetHeader(), HttpConstant::HTTP_RESPONSE_HEADER_SEPARATOR)) {
        task->response_.ParseHeaders();
        std::map<std::string, std::string> headerWithSetCookie = task->response_.GetHeaders();
        std::string setCookies;
        size_t loop = 0;
        for (const auto &setCookie : task->response_.GetsetCookie()) {
            setCookies += setCookie;
            if (loop + 1 < task->response_.GetsetCookie().size()) {
                setCookies += HttpConstant::RESPONSE_KEY_SET_COOKIE_SEPARATOR;
            }
            ++loop;
        }
        headerWithSetCookie[HttpConstant::RESPONSE_KEY_SET_COOKIE] = setCookies;
        task->onHeadersReceive_(task->request_, headerWithSetCookie);
    }

    return size * memBytes;
}

void HttpClientTask::ProcessCookie(CURL *handle)
{
    struct curl_slist *cookies = nullptr;
    if (handle == nullptr) {
        NETSTACK_LOGE("HttpClientTask::ProcessCookie() handle == nullptr");
        return;
    }

    CURLcode res = curl_easy_getinfo(handle, CURLINFO_COOKIELIST, &cookies);
    if (res != CURLE_OK) {
        NETSTACK_LOGE("HttpClientTask::ProcessCookie() curl_easy_getinfo() error! res = %{public}d", res);
        return;
    }

    while (cookies) {
        response_.AppendCookies(cookies->data, strlen(cookies->data));
        if (cookies->next != nullptr) {
            response_.AppendCookies(HttpConstant::HTTP_LINE_SEPARATOR, strlen(HttpConstant::HTTP_LINE_SEPARATOR));
        }
        cookies = cookies->next;
    }
}

bool HttpClientTask::ProcessResponseCode()
{
    int64_t result = 0;
    CURLcode code = curl_easy_getinfo(curlHandle_, CURLINFO_RESPONSE_CODE, &result);
    if (code != CURLE_OK) {
        error_.SetCURLResult(code);
        return false;
    }
    auto resultCode = static_cast<ResponseCode>(result);
    NETSTACK_LOGD("id=%{public}d, code=%{public}d", taskId_, resultCode);
    response_.SetResponseCode(resultCode);

    return true;
}

double HttpClientTask::GetTimingFromCurl(CURL *handle, CURLINFO info) const
{
    curl_off_t timing;
    CURLcode result = curl_easy_getinfo(handle, info, &timing);
    if (result != CURLE_OK) {
        NETSTACK_LOGE("Failed to get timing: %{public}d, %{public}s", info, curl_easy_strerror(result));
        return 0;
    }
    return Timing::TimeUtils::Microseconds2Milliseconds(timing);
}

curl_off_t HttpClientTask::GetSizeFromCurl(CURL *handle) const
{
    auto info = CURLINFO_SIZE_DOWNLOAD_T;
    auto method = request_.GetMethod();
    if (((method.empty() || method == HttpConstant::HTTP_METHOD_POST || method == HttpConstant::HTTP_METHOD_PUT) &&
        !request_.GetBody().empty()) || type_ == TaskType::UPLOAD) {
        info = CURLINFO_SIZE_UPLOAD_T;
    }
    curl_off_t size = 0;
    CURLcode result = curl_easy_getinfo(handle, info, &size);
    if (result != CURLE_OK) {
        NETSTACK_LOGE("curl_easy_getinfo failed, %{public}d, %{public}s", info, curl_easy_strerror(result));
        return 0;
    }
    return size;
}

void HttpClientTask::DumpHttpPerformance()
{
    if (curlHandle_ == nullptr) {
        NETSTACK_LOGE("Ignore dumping http performance, curlHandle_ == nullptr");
        return;
    }
    auto dnsTime = GetTimingFromCurl(curlHandle_, CURLINFO_NAMELOOKUP_TIME_T);
    auto connectTime = GetTimingFromCurl(curlHandle_, CURLINFO_CONNECT_TIME_T);
    auto tlsTime = GetTimingFromCurl(curlHandle_, CURLINFO_APPCONNECT_TIME_T);
    auto firstSendTime = GetTimingFromCurl(curlHandle_, CURLINFO_PRETRANSFER_TIME_T);
    auto firstRecvTime = GetTimingFromCurl(curlHandle_, CURLINFO_STARTTRANSFER_TIME_T);
    auto totalTime = GetTimingFromCurl(curlHandle_, CURLINFO_TOTAL_TIME_T);
    auto redirectTime = GetTimingFromCurl(curlHandle_, CURLINFO_REDIRECT_TIME_T);

    response_.performanceInfo_.dnsTiming = dnsTime;
    response_.performanceInfo_.connectTiming = connectTime;
    response_.performanceInfo_.tlsTiming = tlsTime;
    response_.performanceInfo_.firstSendTiming = firstSendTime;
    response_.performanceInfo_.firstReceiveTiming = firstRecvTime;
    response_.performanceInfo_.totalTiming = totalTime;
    response_.performanceInfo_.redirectTiming = redirectTime;

    int64_t responseCode = 0;
    (void)curl_easy_getinfo(curlHandle_,  CURLINFO_RESPONSE_CODE, &responseCode);

    /*
    CURL_HTTP_VERSION_NONE         0
    CURL_HTTP_VERSION_1_0          1
    CURL_HTTP_VERSION_1_1          2
    CURL_HTTP_VERSION_2            3
    */
    int64_t httpVer = CURL_HTTP_VERSION_NONE;
    (void)curl_easy_getinfo(curlHandle_,  CURLINFO_HTTP_VERSION, &httpVer);
    long osErr = 0;
    (void)curl_easy_getinfo(curlHandle_,  CURLINFO_OS_ERRNO, &osErr);

    curl_off_t size = GetSizeFromCurl(curlHandle_);
    char *ip = nullptr;
    curl_easy_getinfo(curlHandle_, CURLINFO_PRIMARY_IP, &ip);
    NETSTACK_LOGI(
        "taskid=%{public}d"
        ", size:%{public}" CURL_FORMAT_CURL_OFF_T
        ", dns:%{public}.3f"
        ", connect:%{public}.3f"
        ", tls:%{public}.3f"
        ", firstSend:%{public}.3f"
        ", firstRecv:%{public}.3f"
        ", total:%{public}.3f"
        ", redirect:%{public}.3f"
        ", errCode:%{public}d"
        ", RespCode:%{public}s"
        ", httpVer:%{public}s"
        ", method:%{public}s"
        ", osErr:%{public}ld",
        taskId_, size, dnsTime, connectTime == 0 ? 0 : connectTime - dnsTime,
        tlsTime == 0 ? 0 : tlsTime - connectTime,
        firstSendTime == 0 ? 0 : firstSendTime - std::max({dnsTime, connectTime, tlsTime}),
        firstRecvTime == 0 ? 0 : firstRecvTime - firstSendTime, totalTime, redirectTime,
        error_.GetErrorCode(), std::to_string(responseCode).c_str(), std::to_string(httpVer).c_str(),
        request_.GetMethod().c_str(), osErr);

    if (EventReport::GetInstance().IsValid()) {
        HttpPerfInfo httpPerfInfo;
        httpPerfInfo.totalTime = totalTime;
        httpPerfInfo.size = static_cast<int64_t>(size);
        httpPerfInfo.dnsTime = dnsTime;
        httpPerfInfo.tlsTime = tlsTime == 0 ? 0 : tlsTime - connectTime;
        httpPerfInfo.tcpTime = connectTime == 0 ? 0 : connectTime - dnsTime;
        httpPerfInfo.firstRecvTime = firstRecvTime == 0 ? 0 : firstRecvTime - firstSendTime;
        httpPerfInfo.responseCode = responseCode;
        httpPerfInfo.version = std::to_string(httpVer);
        httpPerfInfo.osErr = static_cast<int64_t>(osErr);
        httpPerfInfo.errCode = error_.GetErrorCode();
        httpPerfInfo.ipType = CommonUtils::DetectIPType((ip != nullptr) ? ip : "");
        EventReport::GetInstance().ProcessHttpPerfHiSysevent(httpPerfInfo);
    }
}

void HttpClientTask::ProcessResponse(CURLMsg *msg)
{
    trace_->Finish();
    CURLcode code = msg->data.result;
    NETSTACK_LOGD("taskid=%{public}d code=%{public}d", taskId_, code);
    error_.SetCURLResult(code);
    response_.SetResponseTime(HttpTime::GetNowTimeGMT());

    DumpHttpPerformance();
    if (CURLE_ABORTED_BY_CALLBACK == code) {
        (void)ProcessResponseCode();
        if (onCanceled_) {
            onCanceled_(request_, response_);
        }
        return;
    }

    if (code != CURLE_OK) {
        if (onFailed_) {
            onFailed_(request_, response_, error_);
        }
        return;
    }

    ProcessCookie(curlHandle_);
    response_.ParseHeaders();

    if (ProcessResponseCode()) {
        if (onSucceeded_) {
            onSucceeded_(request_, response_);
        }
    } else if (onFailed_) {
        onFailed_(request_, response_, error_);
    }
#if HAS_NETMANAGER_BASE
    HttpClientNetworkMessage httpClientNetworkMessage(std::to_string(GetTaskId()), request_, response_, curlHandle_);
    networkProfilerUtils_->NetworkProfiling(httpClientNetworkMessage);
#endif
}

void HttpClientTask::SetResponse(const HttpClientResponse &response)
{
    response_ = response;
}

RequestTracer::Trace &HttpClientTask::GetTrace()
{
    return *trace_;
}


bool HttpClientTask::SetDnsCacheOption(CURL *handle)
{
#if HAS_NETMANAGER_BASE
    NETSTACK_CURL_EASY_SET_OPTION(handle, CURLOPT_DNS_CACHE_TIMEOUT, 0);
#endif
    return true;
}
} // namespace HttpClient
} // namespace NetStack
} // namespace OHOS
