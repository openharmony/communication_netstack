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

#include "tcp_server_listen_context.h"

#include "context_key.h"
#include "event_manager.h"
#include "napi_utils.h"
#include "netstack_log.h"
#include "socket_constant.h"

namespace OHOS::NetStack::Socket {
TcpServerListenContext::TcpServerListenContext(napi_env env, const std::shared_ptr<EventManager> &manager)
    : BaseContext(env, manager) {}

void TcpServerListenContext::ParseParams(napi_value *params, size_t paramsCount)
{
    bool valid = CheckParamsType(params, paramsCount);
    if (!valid) {
        if (paramsCount == PARAM_JUST_CALLBACK) {
            if (NapiUtils::GetValueType(GetEnv(), params[0]) == napi_function) {
                SetCallback(params[0]);
            }
            return;
        }
        if (paramsCount == PARAM_OPTIONS_AND_CALLBACK) {
            if (NapiUtils::GetValueType(GetEnv(), params[1]) == napi_function) {
                SetCallback(params[1]);
            }
            return;
        }
        return;
    }

    std::string addr = NapiUtils::GetStringPropertyUtf8(GetEnv(), params[0], KEY_ADDRESS);
    if (addr.empty()) {
        NETSTACK_LOGE("address is empty");
    }

    if (NapiUtils::HasNamedProperty(GetEnv(), params[0], KEY_FAMILY)) {
        uint32_t family = NapiUtils::GetUint32Property(GetEnv(), params[0], KEY_FAMILY);
        address_.SetFamilyByJsValue(family);
    }
    address_.SetIpAddress(addr);
    if (address_.GetAddress().empty()) {
        if (paramsCount == PARAM_OPTIONS_AND_CALLBACK && SetCallback(params[1]) != napi_ok) {
            NETSTACK_LOGE("failed to set callback");
        }
        return;
    }

    if (NapiUtils::HasNamedProperty(GetEnv(), params[0], KEY_PORT)) {
        uint16_t port = static_cast<uint16_t>(NapiUtils::GetUint32Property(GetEnv(), params[0], KEY_PORT));
        address_.SetPort(port);
    }

    if (paramsCount == PARAM_OPTIONS_AND_CALLBACK) {
        SetParseOK(SetCallback(params[1]) == napi_ok);
        return;
    }
    SetParseOK(true);
}

int TcpServerListenContext::GetSocketFd() const
{
    return sharedManager_->GetData() ? static_cast<int>(reinterpret_cast<uint64_t>(sharedManager_->GetData())) : -1;
}

bool TcpServerListenContext::CheckParamsType(napi_value *params, size_t paramsCount)
{
    if (paramsCount == PARAM_JUST_OPTIONS) {
        if (NapiUtils::GetValueType(GetEnv(), params[0]) != napi_object) {
            NETSTACK_LOGE("first param is not NetAddress");
            SetNeedThrowException(true);
            SetError(PARSE_ERROR_CODE, PARSE_ERROR_MSG);
            return false;
        }
        return true;
    }

    if (paramsCount == PARAM_OPTIONS_AND_CALLBACK) {
        if (NapiUtils::GetValueType(GetEnv(), params[0]) != napi_object ||
            NapiUtils::GetValueType(GetEnv(), params[1]) != napi_function) {
            NETSTACK_LOGE("invalid parameter");
            SetNeedThrowException(true);
            SetError(PARSE_ERROR_CODE, PARSE_ERROR_MSG);
            return false;
        }
        return true;
    }
    return false;
}

int32_t TcpServerListenContext::GetErrorCode() const
{
    if (BaseContext::IsPermissionDenied()) {
        return PERMISSION_DENIED_CODE;
    }

    auto err = BaseContext::GetErrorCode();
    if (err == PARSE_ERROR_CODE) {
        return PARSE_ERROR_CODE;
    }
#if defined(IOS_PLATFORM)
    err = ErrCodePlatformAdapter::GetOHOSErrCode(err);
#endif
    return err + SOCKET_SERVER_ERROR_CODE_BASE;
}

std::string TcpServerListenContext::GetErrorMessage() const
{
    if (BaseContext::IsPermissionDenied()) {
        return PERMISSION_DENIED_MSG;
    }

    auto errCode = BaseContext::GetErrorCode();
    if (errCode == PARSE_ERROR_CODE) {
        return PARSE_ERROR_MSG;
    }
#if defined(IOS_PLATFORM)
    std::string errMessage;
    ErrCodePlatformAdapter::GetOHOSErrMessage(errCode, errMessage);
    return errMessage;
#else
    char err[MAX_ERR_NUM] = {0};
    (void)strerror_r(errCode, err, MAX_ERR_NUM);
    return err;
#endif
}
} // namespace OHOS::NetStack::Socket
