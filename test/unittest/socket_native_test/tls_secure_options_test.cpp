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

#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "tls_socket_client_innerapi.h"
#include "secure_data.h"
#include "socket_native_test_utils.h"

namespace OHOS {
namespace NetStack {
namespace TlsSocket {
namespace {
using namespace testing::ext;
} // namespace

class TlsSecureOptionsTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    virtual void SetUp() {}
    virtual void TearDown() {}
};

/**
 * @tc.name: TlsSecureOptionsTest_CaChain
 * @tc.desc: Test SetCaChain and GetCaChain
 * @tc.type: FUNC
 */
HWTEST_F(TlsSecureOptionsTest, CaChain, TestSize.Level1)
{
    TLSSecureOptions options;
    std::vector<std::string> caChain;
    caChain.push_back(CA_CERT_PEM);
    options.SetCaChain(caChain);

    const std::vector<std::string> &retrieved = options.GetCaChain();
    EXPECT_EQ(retrieved.size(), 1U);
    EXPECT_EQ(retrieved[0], CA_CERT_PEM);
}

/**
 * @tc.name: TlsSecureOptionsTest_EmptyCaChain
 * @tc.desc: Test CaChain with empty vector
 * @tc.type: FUNC
 */
HWTEST_F(TlsSecureOptionsTest, EmptyCaChain, TestSize.Level1)
{
    TLSSecureOptions options;
    std::vector<std::string> emptyChain;
    options.SetCaChain(emptyChain);

    const std::vector<std::string> &retrieved = options.GetCaChain();
    EXPECT_TRUE(retrieved.empty());
}

/**
 * @tc.name: TlsSecureOptionsTest_CertChain
 * @tc.desc: Test SetCertChain and GetCertChain
 * @tc.type: FUNC
 */
HWTEST_F(TlsSecureOptionsTest, CertChain, TestSize.Level1)
{
    TLSSecureOptions options;
    std::vector<std::string> certChain;
    certChain.push_back(CLIENT_CERT_PEM);
    options.SetCertChain(certChain);

    const std::vector<std::string> &retrieved = options.GetCertChain();
    EXPECT_EQ(retrieved.size(), 1U);
    EXPECT_EQ(retrieved[0], CLIENT_CERT_PEM);
}

/**
 * @tc.name: TlsSecureOptionsTest_MultipleCertChain
 * @tc.desc: Test cert chain with multiple certificates
 * @tc.type: FUNC
 */
HWTEST_F(TlsSecureOptionsTest, MultipleCertChain, TestSize.Level1)
{
    TLSSecureOptions options;
    std::vector<std::string> certChain;
    certChain.push_back(CLIENT_CERT_PEM);
    certChain.push_back(CA_CERT_PEM);
    options.SetCertChain(certChain);

    const std::vector<std::string> &retrieved = options.GetCertChain();
    EXPECT_EQ(retrieved.size(), 2U);
    EXPECT_EQ(retrieved[0], CLIENT_CERT_PEM);
    EXPECT_EQ(retrieved[1], CA_CERT_PEM);
}

/**
 * @tc.name: TlsSecureOptionsTest_KeyAndPass
 * @tc.desc: Test SetKey and SetKeyPass
 * @tc.type: FUNC
 */
HWTEST_F(TlsSecureOptionsTest, KeyAndPass, TestSize.Level1)
{
    TLSSecureOptions options;
    SecureData key(PRIVATE_KEY_PEM);
    SecureData keyPass(std::string("password123"));

    options.SetKey(key);
    options.SetKeyPass(keyPass);

    const SecureData &retrievedKey = options.GetKey();
    const SecureData &retrievedKeyPass = options.GetKeyPass();

    EXPECT_EQ(retrievedKey.Length(), key.Length());
    EXPECT_EQ(retrievedKeyPass.Length(), keyPass.Length());
}

/**
 * @tc.name: TlsSecureOptionsTest_ProtocolChain
 * @tc.desc: Test SetProtocolChain and GetProtocolChain
 * @tc.type: FUNC
 */
HWTEST_F(TlsSecureOptionsTest, ProtocolChain, TestSize.Level1)
{
    TLSSecureOptions options;
    std::vector<std::string> protocols;
    protocols.push_back(PROTOCOL_TLS_V12);
    protocols.push_back(PROTOCOL_TLS_V13);
    options.SetProtocolChain(protocols);

    const std::vector<std::string> &retrieved = options.GetProtocolChain();
    EXPECT_EQ(retrieved.size(), 2U);
    EXPECT_EQ(retrieved[0], PROTOCOL_TLS_V12);
    EXPECT_EQ(retrieved[1], PROTOCOL_TLS_V13);
}

/**
 * @tc.name: TlsSecureOptionsTest_UseRemoteCipherPrefer
 * @tc.desc: Test Set/Get UseRemoteCipherPrefer
 * @tc.type: FUNC
 */
HWTEST_F(TlsSecureOptionsTest, UseRemoteCipherPrefer, TestSize.Level1)
{
    TLSSecureOptions options;

    // Default should be false
    EXPECT_FALSE(options.UseRemoteCipherPrefer());

    options.SetUseRemoteCipherPrefer(true);
    EXPECT_TRUE(options.UseRemoteCipherPrefer());

    options.SetUseRemoteCipherPrefer(false);
    EXPECT_FALSE(options.UseRemoteCipherPrefer());
}

/**
 * @tc.name: TlsSecureOptionsTest_SignatureAlgorithms
 * @tc.desc: Test SetSignatureAlgorithms and GetSignatureAlgorithms
 * @tc.type: FUNC
 */
HWTEST_F(TlsSecureOptionsTest, SignatureAlgorithms, TestSize.Level1)
{
    TLSSecureOptions options;
    std::string sigAlgs = "rsa_pss_rsae_sha256:ECDSA+SHA256";
    options.SetSignatureAlgorithms(sigAlgs);

    EXPECT_EQ(options.GetSignatureAlgorithms(), sigAlgs);
}

/**
 * @tc.name: TlsSecureOptionsTest_CipherSuite
 * @tc.desc: Test SetCipherSuite and GetCipherSuite
 * @tc.type: FUNC
 */
HWTEST_F(TlsSecureOptionsTest, CipherSuite, TestSize.Level1)
{
    TLSSecureOptions options;
    std::string cipher = "AES256-SHA256";
    options.SetCipherSuite(cipher);

    EXPECT_EQ(options.GetCipherSuite(), cipher);
}

/**
 * @tc.name: TlsSecureOptionsTest_CrlChain
 * @tc.desc: Test SetCrlChain and GetCrlChain
 * @tc.type: FUNC
 */
HWTEST_F(TlsSecureOptionsTest, CrlChain, TestSize.Level1)
{
    TLSSecureOptions options;
    std::vector<std::string> crlChain;
    crlChain.push_back("mock_crl_data");
    options.SetCrlChain(crlChain);

    const std::vector<std::string> &retrieved = options.GetCrlChain();
    EXPECT_EQ(retrieved.size(), 1U);
    EXPECT_EQ(retrieved[0], "mock_crl_data");
}

/**
 * @tc.name: TlsSecureOptionsTest_VerifyMode
 * @tc.desc: Test SetVerifyMode and GetVerifyMode
 * @tc.type: FUNC
 */
HWTEST_F(TlsSecureOptionsTest, VerifyMode, TestSize.Level1)
{
    TLSSecureOptions options;

    // Default should be ONE_WAY_MODE (0)
    EXPECT_EQ(options.GetVerifyMode(), VerifyMode::ONE_WAY_MODE);

    options.SetVerifyMode(VerifyMode::TWO_WAY_MODE);
    EXPECT_EQ(options.GetVerifyMode(), VerifyMode::TWO_WAY_MODE);

    options.SetVerifyMode(VerifyMode::ONE_WAY_MODE);
    EXPECT_EQ(options.GetVerifyMode(), VerifyMode::ONE_WAY_MODE);
}

/**
 * @tc.name: TlsSecureOptionsTest_CopyConstructor
 * @tc.desc: Test copy construction of TLSSecureOptions
 * @tc.type: FUNC
 */
HWTEST_F(TlsSecureOptionsTest, CopyConstructor, TestSize.Level1)
{
    TLSSecureOptions original;
    std::vector<std::string> caChain = {CA_CERT_PEM};
    original.SetCaChain(caChain);
    original.SetCipherSuite("AES128-SHA256");
    original.SetUseRemoteCipherPrefer(true);

    TLSSecureOptions copied(original);
    EXPECT_EQ(copied.GetCaChain().size(), 1U);
    EXPECT_EQ(copied.GetCipherSuite(), "AES128-SHA256");
    EXPECT_TRUE(copied.UseRemoteCipherPrefer());
}

/**
 * @tc.name: TlsSecureOptionsTest_Assignment
 * @tc.desc: Test assignment of TLSSecureOptions
 * @tc.type: FUNC
 */
HWTEST_F(TlsSecureOptionsTest, Assignment, TestSize.Level1)
{
    TLSSecureOptions original;
    original.SetCipherSuite("ECDHE-RSA-AES256-GCM-SHA384");

    TLSSecureOptions assigned;
    assigned = original;
    EXPECT_EQ(assigned.GetCipherSuite(), "ECDHE-RSA-AES256-GCM-SHA384");
}

} // namespace TlsSocket
} // namespace NetStack
} // namespace OHOS
