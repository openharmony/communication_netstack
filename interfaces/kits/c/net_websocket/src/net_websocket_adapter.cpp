/*
 * Copyright (c) 2023 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <iostream>

#include "net_websocket.h"
#include "websocket_client_innerapi.h"

namespace OHOS::NetStack::WebSocketClient {

std::map<WebSocket *, WebSocketClient *> g_clientMap;
std::map<std::string, std::string> globalheaders;

WebSocketClient *GetInnerClientAdapter(WebSocket *key)
{
    auto it = g_clientMap.find(key);
    if (it != g_clientMap.end()) {
        return it->second;
    } else {
        return nullptr;
    }
}

WebSocket *GetNdkClientAdapter(WebSocketClient *websocketClient)
{
    for (const auto &pair : g_clientMap) {
        if (pair.second == websocketClient) {
            return pair.first;
        }
    }
    return nullptr;
}

int32_t Conv2RequestOptions(struct OpenOptions *openOptions,
                            struct WebSocket_RequestOptions requestOptions)
{
    if (openOptions == nullptr) {
        return -1;
    }

    struct WebSocket_Header *currentHeader = requestOptions.headers;

    while (currentHeader != nullptr) {
        std::string fieldName(currentHeader->fieldName);
        std::string fieldValue(currentHeader->fieldValue);
        openOptions->headers[fieldName] = fieldValue;
        currentHeader = currentHeader->next;
    }

    return 0;
}

int32_t Conv2CloseOptions(struct CloseOption *closeOption,
                          struct WebSocket_CloseOption requestOptions)
{
    closeOption->code = requestOptions.code;
    closeOption->reason = requestOptions.reason;
    return 0;
}

int32_t Conv2CloseResult(struct CloseResult closeResult, struct WebSocket_CloseResult *OH_CloseResult)
{
    OH_CloseResult->code = closeResult.code;
    OH_CloseResult->reason = closeResult.reason;
    return 0;
}

int32_t Conv2ErrorResult(struct ErrorResult error, struct WebSocket_ErrorResult *OH_ErrorResult)
{
    OH_ErrorResult->errorCode = error.errorCode;
    OH_ErrorResult->errorMessage = error.errorMessage;
    return 0;
}

int32_t Conv2OpenResult(struct OpenResult openResult, struct WebSocket_OpenResult *OH_OpenResult)
{
    OH_OpenResult->code = openResult.status;
    OH_OpenResult->reason = openResult.message;

    return 0;
}

} // namespace OHOS::NetStack::WebSocketClient