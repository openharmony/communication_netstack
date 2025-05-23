/*
 * Copyright (c) 2021-2022 Huawei Device Co., Ltd.
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

#include "tcp_extra_context.h"

#include "context_key.h"
#include "socket_constant.h"
#include "event_manager.h"
#include "napi_utils.h"
#include "netstack_log.h"

namespace OHOS::NetStack::Socket {
TcpSetExtraOptionsContext::TcpSetExtraOptionsContext(napi_env env, const std::shared_ptr<EventManager> &manager)
    : BaseContext(env, manager) {}

void TcpSetExtraOptionsContext::ParseContextKey(napi_value *params)
{
    if (NapiUtils::HasNamedProperty(GetEnv(), params[0], KEY_RECEIVE_BUFFER_SIZE)) {
        options_.SetReceiveBufferSize(NapiUtils::GetUint32Property(GetEnv(), params[0], KEY_RECEIVE_BUFFER_SIZE));
        options_.SetRecvBufSizeFlag(true);
    }

    if (NapiUtils::HasNamedProperty(GetEnv(), params[0], KEY_SEND_BUFFER_SIZE)) {
        options_.SetSendBufferSize(NapiUtils::GetUint32Property(GetEnv(), params[0], KEY_SEND_BUFFER_SIZE));
        options_.SetSendBufSizeFlag(true);
    }

    if (NapiUtils::HasNamedProperty(GetEnv(), params[0], KEY_REUSE_ADDRESS)) {
        options_.SetReuseAddress(NapiUtils::GetBooleanProperty(GetEnv(), params[0], KEY_REUSE_ADDRESS));
        options_.SetReuseaddrFlag(true);
    }

    if (NapiUtils::HasNamedProperty(GetEnv(), params[0], KEY_SOCKET_TIMEOUT)) {
        options_.SetSocketTimeout(NapiUtils::GetUint32Property(GetEnv(), params[0], KEY_SOCKET_TIMEOUT));
        options_.SetTimeoutFlag(true);
    }

    if (NapiUtils::HasNamedProperty(GetEnv(), params[0], KEY_KEEP_ALIVE)) {
        options_.SetKeepAlive(NapiUtils::GetBooleanProperty(GetEnv(), params[0], KEY_KEEP_ALIVE));
        options_.SetKeepAliveFlag(true);
    }

    if (NapiUtils::HasNamedProperty(GetEnv(), params[0], KEY_OOB_INLINE)) {
        options_.SetOOBInline(NapiUtils::GetBooleanProperty(GetEnv(), params[0], KEY_OOB_INLINE));
        options_.SetOobInlineFlag(true);
    }

    if (NapiUtils::HasNamedProperty(GetEnv(), params[0], KEY_TCP_NO_DELAY)) {
        options_.SetTCPNoDelay(NapiUtils::GetBooleanProperty(GetEnv(), params[0], KEY_TCP_NO_DELAY));
        options_.SetTcpNoDelayFlag(true);
    }

    if (NapiUtils::HasNamedProperty(GetEnv(), params[0], KEY_SOCKET_LINGER)) {
        napi_value socketLinger = NapiUtils::GetNamedProperty(GetEnv(), params[0], KEY_SOCKET_LINGER);
        if (NapiUtils::GetValueType(GetEnv(), params[0]) == napi_object) {
            if (NapiUtils::HasNamedProperty(GetEnv(), socketLinger, KEY_SOCKET_LINGER_ON)) {
                options_.socketLinger.SetOn(
                    NapiUtils::GetBooleanProperty(GetEnv(), socketLinger, KEY_SOCKET_LINGER_ON));
            }
            if (NapiUtils::HasNamedProperty(GetEnv(), socketLinger, KEY_SOCKET_LINGER_LINGER)) {
                options_.socketLinger.SetLinger(
                    NapiUtils::GetUint32Property(GetEnv(), socketLinger, KEY_SOCKET_LINGER_LINGER));
            }
        }
        options_.SetLingerFlag(true);
    }
}

void TcpSetExtraOptionsContext::ParseParams(napi_value *params, size_t paramsCount)
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

    ParseContextKey(params);

    if (paramsCount == PARAM_OPTIONS_AND_CALLBACK) {
        SetParseOK(SetCallback(params[1]) == napi_ok);
        return;
    }
    SetParseOK(true);
}

int TcpSetExtraOptionsContext::GetSocketFd() const
{
    return sharedManager_->GetData() ? static_cast<int>(reinterpret_cast<uint64_t>(sharedManager_->GetData())) : -1;
}

bool TcpSetExtraOptionsContext::CheckParamsType(napi_value *params, size_t paramsCount)
{
    if (paramsCount == PARAM_JUST_OPTIONS) {
        return NapiUtils::GetValueType(GetEnv(), params[0]) == napi_object;
    }

    if (paramsCount == PARAM_OPTIONS_AND_CALLBACK) {
        return NapiUtils::GetValueType(GetEnv(), params[0]) == napi_object &&
               NapiUtils::GetValueType(GetEnv(), params[1]) == napi_function;
    }
    return false;
}

int32_t TcpSetExtraOptionsContext::GetErrorCode() const
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
    return err + SOCKET_ERROR_CODE_BASE;
}

std::string TcpSetExtraOptionsContext::GetErrorMessage() const
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
