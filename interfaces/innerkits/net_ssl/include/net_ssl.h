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

#ifndef COMMUNICATIONNETSTACK_NET_SSL_H
#define COMMUNICATIONNETSTACK_NET_SSL_H

#include "net_ssl_type.h"
#include "net_ssl_verify_cert.h"

namespace OHOS {
namespace NetStack {
namespace Ssl {
/**
 * Verifying Certificates with System Prefabricated Certificates
 * @param cert Certificates to be verified
 * @return Returns 0, verify the certificate successfully, otherwise it will fail
 */
uint32_t NetStackVerifyCertification(const CertBlob *cert);

/**
 * Verifying Certificates with Certificate that user specified
 * @param cert Certificates to be verified
 * @param caCert Certificates that user specified
 * @return Returns 0, verify the certificate successfully, otherwise it will fail
 */
uint32_t NetStackVerifyCertification(const CertBlob *cert, const CertBlob *caCert);

} // namespace Ssl
} // namespace NetStack
} // namespace OHOS

#endif // COMMUNICATIONNETSTACK_NET_SSL_H