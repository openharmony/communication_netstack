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

#include "secure_data.h"
#include "socket_native_test_utils.h"

namespace OHOS {
namespace NetStack {
namespace TlsSocket {
namespace {
using namespace testing::ext;
} // namespace

class SecureDataTest : public testing::Test {
public:
    static void SetUpTestCase() {}
    static void TearDownTestCase() {}
    virtual void SetUp() {}
    virtual void TearDown() {}
};

/**
 * @tc.name: SecureDataTest_DefaultConstructor
 * @tc.desc: Test that default constructed SecureData is empty
 * @tc.type: FUNC
 */
HWTEST_F(SecureDataTest, DefaultConstructor, TestSize.Level1)
{
    SecureData data;
    EXPECT_EQ(data.Length(), 0U);
    EXPECT_NE(data.Data(), nullptr);
}

/**
 * @tc.name: SecureDataTest_StringConstructor
 * @tc.desc: Test constructing SecureData from a std::string
 * @tc.type: FUNC
 */
HWTEST_F(SecureDataTest, StringConstructor, TestSize.Level1)
{
    std::string testStr = "test_secure_data";
    SecureData data(testStr);
    EXPECT_EQ(data.Length(), testStr.length());
    EXPECT_STREQ(data.Data(), testStr.c_str());
}

/**
 * @tc.name: SecureDataTest_EmptyStringConstructor
 * @tc.desc: Test constructing SecureData from an empty string
 * @tc.type: FUNC
 */
HWTEST_F(SecureDataTest, EmptyStringConstructor, TestSize.Level1)
{
    std::string emptyStr;
    SecureData data(emptyStr);
    EXPECT_EQ(data.Length(), 0U);
}

/**
 * @tc.name: SecureDataTest_BufferConstructor
 * @tc.desc: Test constructing SecureData from a raw buffer
 * @tc.type: FUNC
 */
HWTEST_F(SecureDataTest, BufferConstructor, TestSize.Level1)
{
    const uint8_t rawData[] = {'h', 'e', 'l', 'l', 'o'};
    SecureData data(rawData, sizeof(rawData));
    EXPECT_EQ(data.Length(), sizeof(rawData));
    EXPECT_EQ(memcmp(data.Data(), rawData, sizeof(rawData)), 0);
}

/**
 * @tc.name: SecureDataTest_CopyConstructor
 * @tc.desc: Test copy construction of SecureData
 * @tc.type: FUNC
 */
HWTEST_F(SecureDataTest, CopyConstructor, TestSize.Level1)
{
    std::string testStr = "copy_test";
    SecureData original(testStr);
    SecureData copied(original);
    EXPECT_EQ(copied.Length(), original.Length());
    EXPECT_NE(copied.Data(), original.Data()); // deep copy
    EXPECT_STREQ(copied.Data(), original.Data());
}

/**
 * @tc.name: SecureDataTest_AssignmentOperator
 * @tc.desc: Test assignment operator of SecureData
 * @tc.type: FUNC
 */
HWTEST_F(SecureDataTest, AssignmentOperator, TestSize.Level1)
{
    std::string testStr = "assign_test";
    SecureData original(testStr);
    SecureData assigned;
    assigned = original;
    EXPECT_EQ(assigned.Length(), original.Length());
    EXPECT_STREQ(assigned.Data(), original.Data());
}

/**
 * @tc.name: SecureDataTest_SelfAssignment
 * @tc.desc: Test self-assignment of SecureData
 * @tc.type: FUNC
 */
HWTEST_F(SecureDataTest, SelfAssignment, TestSize.Level1)
{
    SecureData data(std::string("self_assign"));
    size_t originalLength = data.Length();
    // Self assignment via pointer indirection (should not crash)
    SecureData* ptr = &data;
    data = *ptr;
    EXPECT_EQ(data.Length(), originalLength);
}

/**
 * @tc.name: SecureDataTest_PrivateKeyData
 * @tc.desc: Test SecureData with private key PEM data
 * @tc.type: FUNC
 */
HWTEST_F(SecureDataTest, PrivateKeyData, TestSize.Level1)
{
    SecureData key(PRIVATE_KEY_PEM);
    EXPECT_GT(key.Length(), 0U);
    EXPECT_NE(key.Data(), nullptr);
}

/**
 * @tc.name: SecureDataTest_CertificateData
 * @tc.desc: Test SecureData with certificate PEM data
 * @tc.type: FUNC
 */
HWTEST_F(SecureDataTest, CertificateData, TestSize.Level1)
{
    SecureData cert(CLIENT_CERT_PEM);
    EXPECT_GT(cert.Length(), 0U);
    EXPECT_NE(cert.Data(), nullptr);
}

} // namespace TlsSocket
} // namespace NetStack
} // namespace OHOS
