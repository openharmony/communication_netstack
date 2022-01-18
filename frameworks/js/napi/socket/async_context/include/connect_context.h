/*
 * Copyright (C) 2021-2022 Huawei Device Co., Ltd.
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

#ifndef COMMUNICATIONNETSTACK_CONNECT_CONTEXT_H
#define COMMUNICATIONNETSTACK_CONNECT_CONTEXT_H

#include "netstack_base_context.h"
#include "noncopyable.h"
#include "tcp_connect_options.h"

namespace OHOS::NetStack {
class ConnectContext final : public BaseContext {
public:
    ACE_DISALLOW_COPY_AND_MOVE(ConnectContext);

    ConnectContext() = delete;

    explicit ConnectContext(napi_env env, EventManager *manager);

    void ParseParams(napi_value *params, size_t paramsCount);

    [[nodiscard]] int GetSocketFd() const;

    TcpConnectOptions options;

private:
    bool CheckParamsType(napi_value *params, size_t paramsCount);
};
} // namespace OHOS::NetStack

#endif /* COMMUNICATIONNETSTACK_CONNECT_CONTEXT_H */
