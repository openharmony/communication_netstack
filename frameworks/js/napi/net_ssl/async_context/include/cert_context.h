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

#ifndef COMMUNICATIONNETSTACK_CERT_CONTEXT_H
#define COMMUNICATIONNETSTACK_CERT_CONTEXT_H

#include "base_context.h"
#include "net_ssl.h"
#include <mutex>
#include <queue>

namespace OHOS::NetStack::Ssl {
class CertContext final : public BaseContext {
public:
    CertContext() = delete;

    CertContext(napi_env env, EventManager *manager);

    ~CertContext() override;

    void ParseParams(napi_value *params, size_t paramsCount) override;

    bool CheckParamsType(napi_value *params, size_t paramsCount);

    CertBlob *GetCertBlob();

    CertBlob *GetCertBlobClient();

    void SetVerifyResult(const uint32_t &verifyResult);

    uint32_t GetVerifyResult();

private:
    CertBlob *ParseCertBlobFromParams(napi_env env, napi_value value);

    CertBlob *certBlob_;

    CertBlob *certBlobClient_;

    uint32_t verifyResult_;
};
} // namespace OHOS::NetStack::Ssl

#endif /* COMMUNICATIONNETSTACK_CERT_CONTEXT_H */
