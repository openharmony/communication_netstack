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

#include "tls_server_send_contex.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "constant.h"
#include "napi_utils.h"
#include "netstack_log.h"

namespace OHOS {
namespace NetStack {
namespace TlsSocketServer {
namespace {
//constexpr const char *DATA = "data";
constexpr std::string_view PARSE_ERROR = "options is not type of TLSServerSendOptions";
}

TLSServerSendContext::TLSServerSendContext(napi_env env, EventManager *manager) : BaseContext(env, manager) {}

void TLSServerSendContext::ParseParams(napi_value *params, size_t paramsCount)
{
	if (!CheckParamsType(params, paramsCount)) {
		return;
	}


	m_sendData = NapiUtils::GetStringFromValueUtf8(GetEnv(), params[0]);

	if (paramsCount == TlsSocket::PARAM_OPTIONS_AND_CALLBACK) {
		SetParseOK(SetCallback(params[TlsSocket::ARG_INDEX_1]) == napi_ok);
		return;
	}
	SetParseOK(true);
}

bool TLSServerSendContext::CheckParamsType(napi_value *params, size_t paramsCount)
{
	if (paramsCount == TlsSocket::PARAM_JUST_OPTIONS) {
		if (NapiUtils::GetValueType(GetEnv(), params[TlsSocket::ARG_INDEX_0]) != napi_string) {
			NETSTACK_LOGE("first param is not string");
			SetNeedThrowException(true);
			SetError(PARSE_ERROR_CODE, PARSE_ERROR.data());
			return false;
		}
		return true;
	}

	if (paramsCount == TlsSocket::PARAM_OPTIONS_AND_CALLBACK) {
		if (NapiUtils::GetValueType(GetEnv(), params[TlsSocket::ARG_INDEX_0]) != napi_string) {
			NETSTACK_LOGE("first param is not string");
			SetNeedThrowException(true);
			SetError(PARSE_ERROR_CODE, PARSE_ERROR.data());
			return false;
		}
		if (NapiUtils::GetValueType(GetEnv(), params[TlsSocket::ARG_INDEX_1]) != napi_function) {
			NETSTACK_LOGE("second param is not function");
			return false;
		}
		return true;
	}
	return false;
}

//TLSServerSendOptions TLSServerSendContext::ReadTLSServerSendOptions(napi_env env, napi_value *params)
//{
//    TLSServerSendOptions options;
//    int clientFd = NapiUtils::GetInt32FromValue(GetEnv(), params[0]);
//    std::string data = NapiUtils::GetStringPropertyUtf8(GetEnv(), params[0], DATA);
//    options.SetSocket(clientFd);
//    options.SetSendData(data);
//    return options;
//}

} // namespace TlsSocketServer
} // namespace NetStack
} // namespace OHOS
