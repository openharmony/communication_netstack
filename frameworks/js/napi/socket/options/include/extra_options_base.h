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

#ifndef COMMUNICATIONNETSTACK_EXTRA_OPTIONS_BASE_H
#define COMMUNICATIONNETSTACK_EXTRA_OPTIONS_BASE_H

#include <cstdint>

namespace OHOS::NetStack::Socket {
class ExtraOptionsBase {
public:
    ExtraOptionsBase();

    ~ExtraOptionsBase() = default;

    void SetReceiveBufferSize(uint32_t receiveBufferSize);

    void SetSendBufferSize(uint32_t sendBufferSize);

    void SetReuseAddress(bool reuseAddress);

    void SetSocketTimeout(uint32_t socketTimeout);

    [[nodiscard]] uint32_t GetReceiveBufferSize() const;

    [[nodiscard]] uint32_t GetSendBufferSize() const;

    [[nodiscard]] bool IsReuseAddress() const;

    [[nodiscard]] uint32_t GetSocketTimeout() const;

    [[nodiscard]] bool AlreadySetRecvBufSize() const;

    void SetRecvBufSizeFlag(bool flag);

    [[nodiscard]] bool AlreadySetSendBufSize() const;

    void SetSendBufSizeFlag(bool flag);

    [[nodiscard]] bool AlreadySetTimeout() const;

    void SetTimeoutFlag(bool flag);

    [[nodiscard]] bool AlreadySetReuseAddr() const;

    void SetReuseaddrFlag(bool flag);

private:
    uint32_t receiveBufferSize_;

    uint32_t sendBufferSize_;

    bool reuseAddress_;

    uint32_t socketTimeout_;

    bool recvBufSizeFlag_ = false;

    bool sendBufSizeFlag_ = false;

    bool timeoutFlag_ = false;

    bool reuseAddrFlag_ = false;
};
} // namespace OHOS::NetStack::Socket

#endif /* COMMUNICATIONNETSTACK_EXTRA_OPTIONS_BASE_H */
