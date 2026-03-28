/*
 * Copyright (c) 2026 Huawei Device Co., Ltd.
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

#ifndef COMMUNICATIONNETSTACK_HTTP_DEADFLOW_INFO_H
#define COMMUNICATIONNETSTACK_HTTP_DEADFLOW_INFO_H

#include <cstdint>
#include <string>

struct HttpDeadFlowInfo {
    HttpDeadFlowInfo() = default;

    /* Sport of dead stream, split by semicolon */
    std::string sPortStr = "";

    /* Indexing whether the request reuses the dead stream */
    bool isReused = false;

    /* Sock of dead stream */
    int32_t sock = 0;

    /* HTTP retry count */
    int32_t retryCount = 0;

    /* Http block time on dead stream before retrying, split by semicolon */
    std::string tdiff = "";
};
#endif  // COMMUNICATIONNETSTACK_HTTP_DEADFLOW_INFO_H
