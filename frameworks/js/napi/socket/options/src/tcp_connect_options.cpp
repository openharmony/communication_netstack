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

#include "tcp_connect_options.h"

namespace OHOS::NetStack::Socket {
TcpConnectOptions::TcpConnectOptions() : timeout_(DEFAULT_CONNECT_TIMEOUT) {}

void TcpConnectOptions::SetTimeout(uint32_t timeout)
{
    timeout_ = timeout;
}

uint32_t TcpConnectOptions::GetTimeout() const
{
    return timeout_;
}
} // namespace OHOS::NetStack::Socket