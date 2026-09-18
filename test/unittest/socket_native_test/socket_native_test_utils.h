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

#ifndef SOCKET_NATIVE_TEST_UTILS_H
#define SOCKET_NATIVE_TEST_UTILS_H

#include <string>
#include <vector>

namespace OHOS {
namespace NetStack {
namespace TlsSocket {

constexpr const char CLIENT_CERT_PEM[] =
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIIDezCCAmMCFD6h5R4QvySV9q9mC6s31qQFLX14MA0GCSqGSIb3DQEBCwUAMHgx\r\n"
    "CzAJBgNVBAYTAkNOMQswCQYDVQQIDAJHRDELMAkGA1UEBwwCU1oxDDAKBgNVBAoM\r\n"
    "A0NPTTEMMAoGA1UECwwDTlNQMQswCQYDVQQDDAJDQTEmMCQGCSqGSIb3DQEJARYX\r\n"
    "emhhbmd6aGV3ZWkwMTAzQDE2My5jb20wHhcNMjIwNDI0MDIwMjU3WhcNMjMwNDI0\r\n"
    "MDIwMjU3WjB8MQswCQYDVQQGEwJDTjELMAkGA1UECAwCR0QxCzAJBgNVBAcMAlNa\r\n"
    "MQwwCgYDVQQKDANDT00xDDAKBgNVBAsMA05TUDEPMA0GA1UEAwwGQ0xJRU5UMSYw\r\n"
    "JAYJKoZIhvcNAQkBFhd6aGFuZ3poZXdlaTAxMDNAMTYzLmNvbTCCASIwDQYJKoZI\r\n"
    "hvcNAQEBBQADggEPADCCAQoCggEBAKlc63+j5C7tLoaecpdhzzZtLy8iNSi6oLHc\r\n"
    "+bPib1XWz1zcQ4On5ncGuuLSV2Tyse4tSsDbPycd8b9Teq6gdGrvirtGXau82zAq\r\n"
    "no+t0mxVtV1r0OkSe+hnIrYKxTE5UDeAM319MSxWlCR0bg0uEAuVBPQpld5A9PQT\r\n"
    "YCLbv4cTwB0sIKupsnNbrn2AsAlCFd288XeuTN+N87m05cDkprAkqkCJfAtRnejV\r\n"
    "k+vbS+H6toR3P9PVQJXC77oM7cDOjR8AwpkRRA890XUWoQLwhHXvDpGPwKK+lLnG\r\n"
    "FswiaHy3silUIOidwk7E/81BOqXSk77oUG6UQrVilkmu6g79VssCAwEAATANBgkq\r\n"
    "hkiG9w0BAQsFAAOCAQEAOeqp+hFVRs4YB3UjU/3bvAUFQLS97gapCp2lk6jS88jt\r\n"
    "uNeyvwulOAtZEbcoIIvzzNxvBDOVibTJ6gZU9P9g0WyRu2RTgy+UggNwH8u8KZzM\r\n"
    "DT8sxuoYvRcEWbOhlNQgACa7AlQSLQifo8nvEMS2i9o8WHoHu42MRDYOHYVIwWXH\r\n"
    "h6mZzfo+zrPyv3NFlwlWqaNiTGgnGCXzlVK3p5YYqLbNVYpy0U5FBxQ7fITsqcbK\r\n"
    "PusAAEZzPxm8Epo647M28gNkdEEM/7bqhSTJO+jfkojgyQt2ghlw+NGCmG4dJGZb\r\n"
    "yA7Z3PBj8aqEwmRUF8SAR1bxWBGk2IYRwgStuwvusg==\r\n"
    "-----END CERTIFICATE-----\r\n";

constexpr const char CA_CERT_PEM[] =
    "-----BEGIN CERTIFICATE-----\r\n"
    "MIIDazCCAlOgAwIBAgIBATANBgkqhkiG9w0BAQsFADBwMQswCQYDVQQGEwJDTjEQ\r\n"
    "MA4GA1UECAwHYmVpamluZzEdMBsGA1UECgwUR2xvYmFsIEdvb2dsZSBDQSBJbmMx\r\n"
    "EDAOBgNVBAsMB1Jvb3QgQ0ExHjAcBgNVBAMMFUdsb2JhbCBHb29nbGUgUm9vdCBD\r\n"
    "QTAeFw0yMjA4MjMwNzMzNTVaFw0yMzA4MjMwNzMzNTVaMHAxCzAJBgNVBAYTAkNO\r\n"
    "MRAwDgYDVQQIDAdiZWlqaW5nMR0wGwYDVQQKDBRHbG9iYWwgR29vZ2xlIENBIElu\r\n"
    "YzEQMA4GA1UECwwHUm9vdCBDQTEeMBwGA1UEAwwVR2xvYmFsIEdvb2dsZSBSb290\r\n"
    "IENBMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAnd9o93t4CyHzbyRg\r\n"
    "784CkCTfxNPz5CZsxxK/KM04LT+rdhFkzmv2B/01HrnsInIDTevSlEktgkRsclkU\r\n"
    "q+cMcjI+rfqdUtokjemkENfdNGbffuAOZlOL7pEHms4qhSUJdz1fdRyhs6uGOyEo\r\n"
    "+EOq8At9TfnfhTNKO//kA1klYqHp2pJjApO9+d9uxlen0uZ7NxSpumlxDMVPZv5n\r\n"
    "ZlyN1wRN2PMLwAt9SetolCj2MQ8NKgNwp5f5OJA21Es5S1OlLDJy8kGGMhM8QC0/\r\n"
    "6GPTjIqDedMg9rzNlz6UkU48dI2a+inexKX34eIGVeZsQQ9gO5DeOoTvOnd5JwAj\r\n"
    "VWbKgQIDAQABoxAwDjAMBgNVHRMEBTADAQH/MA0GCSqGSIb3DQEBCwUAA4IBAQBh\r\n"
    "Pjlxf7FQ3XGXzdypS3KWChLBGP01teCXG3ZYIo3NdVEPugQAlGpG1TrFrOp9nOxv\r\n"
    "GbbxKwbpu8tJJDQLVb0CGSQZhbvkpID01pCCfoFcm4nUFe06t6I3WUDbtBglkC6u\r\n"
    "gvmoDJ29x4xUhe0H0XAd7qGSvRKXg02enrcBtValHzFuoUhopE8c+rA4J0cS61Wj\r\n"
    "RffjGLrXhTwfLB5eOHVegIr9HIRPm++Ft3mJ10Pr1PvFUVuEbw4GMlQT5KfiIC24\r\n"
    "+i0J+I/dARk5zCPA0TkZmvd8U2O/6r4Em68+bh53yLkLeOkOYqdR2x7AY01NFP/K\r\n"
    "RH8V5PqYHj1YwrZaZGjQ\r\n"
    "-----END CERTIFICATE-----\r\n";

constexpr const char PRIVATE_KEY_PEM[] =
    "-----BEGIN RSA PRIVATE KEY-----"
    "MIIEowIBAAKCAQEAqVzrf6PkLu0uhp5yl2HPNm0vLyI1KLqgsdz5s+JvVdbPXNxD"
    "g6fmdwa64tJXZPKx7i1KwNs/Jx3xv1N6rqB0au+Ku0Zdq7zbMCqej63SbFW1XWvQ"
    "6RJ76GcitgrFMTlQN4AzfX0xLFaUJHRuDS4QC5UE9CmV3kD09BNgItu/hxPAHSwg"
    "q6myc1uufYCwCUIV3bzxd65M343zubTlwOSmsCSqQIl8C1Gd6NWT69tL4fq2hHc/"
    "09VAlcLvugztwM6NHwDCmRFEDz3RdRahAvCEde8OkY/Aor6UucYWzCJofLeyKVQg"
    "6J3CTsT/zUE6pdKTvuhQbpRCtWKWSa7qDv1WywIDAQABAoIBAFGpbCPvcmbuFjDy"
    "1W4Iy1EC9G1VoSwyUKlyUzRZSjWpjfLIggVJP+bEZ/hWU61pGEIvtIupK5pA5f/K"
    "0KzC0V9+gPYrx563QTjIVAwTVBLIgNq60dCQCQ7WK/Z62voRGIyqVCl94+ftFyE8"
    "wpO4UiRDhk/0fT7dMz882G32ZzNJmY9eHu+yOaRctJW2gRBROHpQfDGBCz7w8s2j"
    "ulIcnvwGOrvVllsL+vgY95M0LOq0W8ObbUSlawTnNTSRxFL68Hz5EaVJ19EYvEcC"
    "eWnpEqIfF8OhQ+mYbdrAutXCkqJLz3rdu5P2Lbk5Ht5ETfr7rtUzvb4+ExIcxVOs"
    "eys8EgECgYEA29tTxJOy2Cb4DKB9KwTErD1sFt9Ed+Z/A3RGmnM+/h75DHccqS8n"
    "g9DpvHVMcMWYFVYGlEHC1F+bupM9CgxqQcVhGk/ysJ5kXF6lSTnOQxORnku3HXnV"
    "4QzgKtLfHbukW1Y2RZM3aCz+Hg+bJrpacWyWZ4tRWNYsO58JRaubZjsCgYEAxTSP"
    "yUBleQejl5qO76PGUUs2W8+GPr492NJGb63mEiM1zTYLVN0uuDJ2JixzHb6o1NXZ"
    "6i00pSksT3+s0eiBTRnF6BJ0y/8J07ZnfQQXRAP8ypiZtd3jdOnUxEHfBw2QaIdP"
    "tVdUc2mpIhosAYT9sWpHYvlUqTCdeLwhkYfgeLECgYBoajjVcmQM3i0OKiZoCOKy"
    "/pTYI/8rho+p/04MylEPdXxIXEWDYD6/DrgDZh4ArQc2kt2bCcRTAnk+WfEyVYUd"
    "aXVdfry+/uqhJ94N8eMw3hlZeZIk8JkQQgIwtGd8goJjUoWB85Hr6vphIn5IHVcY"
    "6T5hPLxMmaL2SeioawDpwwKBgQCFXjDH6Hc3zQTEKND2HIqou/b9THH7yOlG056z"
    "NKZeKdXe/OfY8uT/yZDB7FnGCgVgO2huyTfLYvcGpNAZ/eZEYGPJuYGn3MmmlruS"
    "fsvFQfUahu2dY3zKusEcIXhV6sR5DNnJSFBi5VhvKcgNFwYDkF7K/thUu/4jgwgo"
    "xf33YQKBgDQffkP1jWqT/pzlVLFtF85/3eCC/uedBfxXknVMrWE+CM/Vsx9cvBZw"
    "hi15LA5+hEdbgvj87hmMiCOc75e0oz2Rd12ZoRlBVfbncH9ngfqBNQElM7Bueqoc"
    "JOpKV+gw0gQtiu4beIdFnYsdZoZwrTjC4rW7OI0WYoLJabMFFh3I"
    "-----END RSA PRIVATE KEY-----";

} // namespace TlsSocket
} // namespace NetStack
} // namespace OHOS
#endif // SOCKET_NATIVE_TEST_UTILS_H
