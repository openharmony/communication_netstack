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

#include "tcp_server_send_context.h"

#include "context_key.h"
#include "event_manager.h"
#include "napi_utils.h"
#include "netstack_log.h"
#include "socket_constant.h"

namespace OHOS::NetStack::Socket {
TcpServerSendContext::TcpServerSendContext(napi_env env, const std::shared_ptr<EventManager> &manager)
    : BaseContext(env, manager) {}

void TcpServerSendContext::ParseParams(napi_value *params, size_t paramsCount)
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

    if (NapiUtils::HasNamedProperty(GetEnv(), params[0], KEY_ENCODING)) {
        std::string encoding = NapiUtils::GetStringPropertyUtf8(GetEnv(), params[0], KEY_ENCODING);
        options.SetEncoding(encoding);
    }
    if (!GetData(params[0])) {
        if (paramsCount == PARAM_OPTIONS_AND_CALLBACK && SetCallback(params[1]) != napi_ok) {
            NETSTACK_LOGE("failed to set callback");
        }
        return;
    }

    if (paramsCount == PARAM_OPTIONS_AND_CALLBACK) {
        SetParseOK(SetCallback(params[1]) == napi_ok);
        return;
    }
    SetParseOK(true);
}

int TcpServerSendContext::GetSocketFd() const
{
    return sharedManager_->GetData() ? static_cast<int>(reinterpret_cast<uint64_t>(sharedManager_->GetData())) : -1;
}

bool TcpServerSendContext::CheckParamsType(napi_value *params, size_t paramsCount)
{
    if (paramsCount == PARAM_JUST_OPTIONS) {
        if (NapiUtils::GetValueType(GetEnv(), params[0]) != napi_object) {
            NETSTACK_LOGE("invalid parameter");
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

bool TcpServerSendContext::GetData(napi_value udpSendOptions)
{
    napi_value jsData = NapiUtils::GetNamedProperty(GetEnv(), udpSendOptions, KEY_DATA);
    if (NapiUtils::GetValueType(GetEnv(), jsData) == napi_string) {
        std::string data = NapiUtils::GetStringFromValueUtf8(GetEnv(), jsData);
        if (data.empty()) {
            NETSTACK_LOGE("string data is empty");
            return true;
        }
        options.SetData(data);
        return true;
    }

    if (NapiUtils::ValueIsArrayBuffer(GetEnv(), jsData)) {
        size_t length = 0;
        void *data = NapiUtils::GetInfoFromArrayBufferValue(GetEnv(), jsData, &length);
        if (data == nullptr) {
            NETSTACK_LOGE("arraybuffer data is empty");
            return true;
        }
        options.SetData(data, length);
        return true;
    }
    return false;
}

int32_t TcpServerSendContext::GetErrorCode() const
{
    if (BaseContext::IsPermissionDenied()) {
        return PERMISSION_DENIED_CODE;
    }

    auto err = BaseContext::GetErrorCode();
    if (err == PARSE_ERROR_CODE) {
        return PARSE_ERROR_CODE;
    }
    return err + SOCKET_SERVER_ERROR_CODE_BASE;
}

std::string TcpServerSendContext::GetErrorMessage() const
{
    if (BaseContext::IsPermissionDenied()) {
        return PERMISSION_DENIED_MSG;
    }

    auto errCode = BaseContext::GetErrorCode();
    if (errCode == PARSE_ERROR_CODE) {
        return PARSE_ERROR_MSG;
    }
    char err[MAX_ERR_NUM] = {0};
    (void)strerror_r(errCode, err, MAX_ERR_NUM);
    return err;
}
} // namespace OHOS::NetStack::Socket
