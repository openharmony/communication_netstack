/*
 * Copyright (c) 2022 Huawei Device Co., Ltd.
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

#include "tls_context.h"

#include <cerrno>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <string>

#include "netstack_log.h"
#include "openssl/evp.h"

namespace OHOS {
namespace NetStack {
std::unique_ptr<TLSContext> TLSContext::CreateConfiguration(TlsMode mode, const TLSConfiguration &configuration)
{
    auto tlsContext = std::make_unique<TLSContext>();
    if (!InitTlsContext(tlsContext.get(), mode, configuration)) {
        return nullptr;
    }
    return tlsContext;
}

void InitEnv()
{
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
}

void TLSContext::SetCipherList(TLSContext *tlsContext, const TLSConfiguration &configuration)
{
    NETSTACK_LOGD("GetCipherSuite = %{public}s", configuration.GetCipherSuite().c_str());
    if (configuration.GetCipherSuite().empty()) {
        return;
    }
    if (SSL_CTX_set_cipher_list(tlsContext->ctx_, configuration.GetCipherSuite().c_str()) <= 0) {
        NETSTACK_LOGE("Error setting the cipher list");
    }
}

void TLSContext::GetCiphers(TLSContext *tlsContext)
{
    if (!tlsContext) {
        NETSTACK_LOGE("TLSContext::GetCiphers: tlsContext is null");
        return;
    }
    std::vector<CipherSuite> cipherSuiteVec;
    STACK_OF(SSL_CIPHER) *sk = SSL_CTX_get_ciphers(tlsContext->ctx_);
    if (!sk) {
        NETSTACK_LOGE("TLSContext::GetCiphers: sk is null");
        return;
    }
    CipherSuite cipherSuite;
    for (int i = 0; i < sk_SSL_CIPHER_num(sk); i++) {
        const SSL_CIPHER *c = sk_SSL_CIPHER_value(sk, i);
        cipherSuite.cipherId_ = SSL_CIPHER_get_id(c);
        cipherSuite.cipherName_ = SSL_CIPHER_get_name(c);
        cipherSuiteVec.push_back(cipherSuite);
        NETSTACK_LOGD("SSL_CIPHER_get_id = %{public}lu, SSL_CIPHER_get_name = %{public}s", cipherSuite.cipherId_,
                      cipherSuite.cipherName_.c_str());
    }
}

void TLSContext::SetSignatureAlgorithms(TLSContext *tlsContext, const TLSConfiguration &configuration)
{
    if (configuration.GetSignatureAlgorithms().empty()) {
        return;
    }
    if (!SSL_CTX_set1_sigalgs_list(tlsContext->ctx_, configuration.GetSignatureAlgorithms().c_str())) {
        NETSTACK_LOGE("Error setting the Signature Algorithms");
    }
}

void TLSContext::UseRemoteCipher(TLSContext *tlsContext)
{
    if (!tlsContext) {
        NETSTACK_LOGE("TLSContext::UseRemoteCipher: tlsContext is null");
        return;
    }
    if (tlsContext->tlsConfiguration_.GetUseRemoteCipherPrefer()) {
        SSL_CTX_set_options(tlsContext->ctx_, SSL_OP_CIPHER_SERVER_PREFERENCE);
    }
    NETSTACK_LOGI("SSL_CTX_get_options = %{public}lx", SSL_CTX_get_options(tlsContext->ctx_));
}

void TLSContext::SetMinAndMaxProtocol(TLSContext *tlsContext)
{
    if (!tlsContext) {
        NETSTACK_LOGE("TLSContext::SetMinAndMaxProtocol: tlsContext is null");
        return;
    }
    const long anyVersion = TLS_ANY_VERSION;
    long minVersion = anyVersion;
    long maxVersion = anyVersion;

    switch (tlsContext->tlsConfiguration_.GetMinProtocol()) {
        case TLS_V1_2:
            minVersion = TLS1_2_VERSION;
            break;
        case TLS_V1_3:
            minVersion = TLS1_3_VERSION;
            break;
        case UNKNOW_PROTOCOL:
            break;
    }

    switch (tlsContext->tlsConfiguration_.GetMaxProtocol()) {
        case TLS_V1_2:
            maxVersion = TLS1_2_VERSION;
            break;
        case TLS_V1_3:
            maxVersion = TLS1_3_VERSION;
            break;
        case UNKNOW_PROTOCOL:
            break;
    }

    if (minVersion != anyVersion && !SSL_CTX_set_min_proto_version(tlsContext->ctx_, minVersion)) {
        NETSTACK_LOGE("Error while setting the minimal protocol version");
        return;
    }

    if (maxVersion != anyVersion && !SSL_CTX_set_max_proto_version(tlsContext->ctx_, maxVersion)) {
        NETSTACK_LOGE("Error while setting the maximum protocol version");
        return;
    }

    NETSTACK_LOGD("minProtocol = %{public}lx, maxProtocol = %{public}lx",
                  SSL_CTX_get_min_proto_version(tlsContext->ctx_), SSL_CTX_get_max_proto_version(tlsContext->ctx_));
}

bool TLSContext::SetCaAndVerify(TLSContext *tlsContext, const TLSConfiguration &configuration)
{
    if (!tlsContext) {
        NETSTACK_LOGE("TLSContext::SetCaAndVerify: tlsContext is null");
        return false;
    }
    for (const auto &cert : configuration.GetCaCertificate()) {
        TLSCertificate ca(cert, CA_CERT);
        if (!X509_STORE_add_cert(SSL_CTX_get_cert_store(tlsContext->ctx_), (X509 *)ca.handle())) {
            return false;
        }
    }
    return true;
}

bool TLSContext::SetLocalCertificate(TLSContext *tlsContext, const TLSConfiguration &configuration)
{
    if (!tlsContext) {
        NETSTACK_LOGE("TLSContext::SetLocalCertificate: tlsContext is null");
        return false;
    }
    if (!SSL_CTX_use_certificate(tlsContext->ctx_, (X509 *)configuration.GetLocalCertificate().handle())) {
        NETSTACK_LOGE("Error loading local certificate");
        return false;
    }
    return true;
}

bool TLSContext::SetKeyAndCheck(TLSContext *tlsContext, const TLSConfiguration &configuration)
{
    if (!tlsContext) {
        NETSTACK_LOGE("The parameter tlsContext is null");
        return false;
    }
    if (configuration.GetPrivateKey().Algorithm() == OPAQUE) {
        tlsContext->pkey_ = reinterpret_cast<EVP_PKEY *>(configuration.GetPrivateKey().handle());
    } else {
        tlsContext->pkey_ = EVP_PKEY_new();
        if (configuration.GetPrivateKey().Algorithm() == ALGORITHM_RSA) {
            EVP_PKEY_set1_RSA(tlsContext->pkey_, reinterpret_cast<RSA *>(configuration.GetPrivateKey().handle()));
        } else if (tlsContext->tlsConfiguration_.GetPrivateKey().Algorithm() == ALGORITHM_DSA) {
            EVP_PKEY_set1_DSA(tlsContext->pkey_, reinterpret_cast<DSA *>(configuration.GetPrivateKey().handle()));
        }
    }

    if (configuration.GetPrivateKey().Algorithm() == OPAQUE) {
        tlsContext->pkey_ = nullptr;
    }
    auto pkey_ = tlsContext->pkey_;
    if (!SSL_CTX_use_PrivateKey(tlsContext->ctx_, pkey_)) {
        NETSTACK_LOGE("SSL_CTX_use_PrivateKey is error");
        return false;
    }

    if (!configuration.GetPrivateKey().GetKeyPass().empty()) {
        std::string password = tlsContext->tlsConfiguration_.GetPrivateKey().GetKeyPass();
        const char *pass = password.c_str();
        SSL_CTX_set_default_passwd_cb_userdata(tlsContext->ctx_, reinterpret_cast<void *>(const_cast<char *>(pass)));
    }
    // Check if the certificate matches the private key.
    if (!SSL_CTX_check_private_key(tlsContext->ctx_)) {
        NETSTACK_LOGE("Check if the certificate matches the private key is error");
        return false;
    }
    return true;
}

void TLSContext::SetVerify(TLSContext *tlsContext)
{
    if (!tlsContext) {
        NETSTACK_LOGE("TLSContext::SetVerify: tlsContext is null");
        return;
    }
    SSL_CTX_set_verify(tlsContext->ctx_, SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT, nullptr);
}

bool TLSContext::InitTlsContext(TLSContext *tlsContext, TlsMode mode, const TLSConfiguration &configuration)
{
    if (!tlsContext) {
        NETSTACK_LOGE("TLSContext::InitTlsContext: tlsContext is null");
        return false;
    }
    InitEnv();
    tlsContext->tlsConfiguration_ = configuration;
    tlsContext->ctx_ = SSL_CTX_new(TLS_client_method());
    if (tlsContext->ctx_ == nullptr) {
        ERR_print_errors_fp(stdout);
        NETSTACK_LOGE("tlsContext->ctx_ is nullptr");
        return false;
    }

    SetCipherList(tlsContext, configuration);
    GetCiphers(tlsContext);
    SetSignatureAlgorithms(tlsContext, configuration);
    UseRemoteCipher(tlsContext);
    SetMinAndMaxProtocol(tlsContext);
    SetVerify(tlsContext);
    if (!SetCaAndVerify(tlsContext, configuration)) {
        return false;
    }
    if (!SetLocalCertificate(tlsContext, configuration)) {
        return false;
    }
    if (!SetKeyAndCheck(tlsContext, configuration)) {
        return false;
    }
    return true;
}
SSL *TLSContext::CreateSsl()
{
    ctxSsl_ = SSL_new(ctx_);
    return ctxSsl_;
}

void TLSContext::CloseCtx()
{
    SSL_CTX_free(ctx_);
}
} // namespace NetStack
} // namespace OHOS