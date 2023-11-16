/*
 * Copyright (c) 2022-2023 Huawei Device Co., Ltd.
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

namespace OHOS::NetStack::Websocket
{
    const char *ContextKey::HEADER = "header";

    const char *const ContextKey::CAPATH = "caPath";
    
    const char *const ContextKey::PARAM_KEY_CLINENT_CERT = "clientCert";

    const char *const ContextKey::WEBSCOKET_CLINENT_CERT = "certPath";

    const char *const ContextKey::WEBSCOKET_CLINENT_KEY = "keyPath";

    const char *const ContextKey::WEBSCOKET_CLINENT_PASSWD = "keyPasswd";

    const char *ContextKey::CODE = "code";

    const char *ContextKey::REASON = "reason";

    const char *EventName::EVENT_OPEN = "open";

    const char *EventName::EVENT_MESSAGE = "message";

    const char *EventName::EVENT_CLOSE = "close";

    const char *EventName::EVENT_ERROR = "error";

    const char *EventName::EVENT_DATA_END = "dataEnd";
} // namespace OHOS::NetStack::Websocket