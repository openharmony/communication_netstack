/*
 * Copyright (c) 2022-2024 Huawei Device Co., Ltd.
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

#include "net_address.h"
#include "secure_data.h"
#include "socket_error.h"
#include "socket_state_base.h"
#include "tls.h"
#include "tls_certificate.h"
#include "tls_configuration.h"
#include "tls_key.h"
#include "tls_socket.h"
#include "tls_utils_test.h"

namespace OHOS {
namespace NetStack {
namespace TlsSocket {
void MockNetAddress(Socket::NetAddress &address)
{
    address.SetAddress(TlsUtilsTest::GetIp(TlsUtilsTest::ChangeToFile(IP_ADDRESS)));
    address.SetPort(std::atoi(TlsUtilsTest::ChangeToFile(PORT).c_str()));
    address.SetFamilyBySaFamily(AF_INET);
}

void MockTlsSocketParamOptions(Socket::NetAddress &address, TLSSecureOptions &secureOption, TLSConnectOptions &options)
{
    secureOption.SetKey(SecureData(TlsUtilsTest::ChangeToFile(PRIVATE_KEY_PEM)));
    secureOption.SetCert(TlsUtilsTest::ChangeToFile(CLIENT_CRT));

    MockNetAddress(address);
    options.SetTlsSecureOptions(secureOption);
    options.SetNetAddress(address);
}

void SetSocketHwTestShortParam(TLSSocket &server)
{
    TLSConnectOptions options;
    Socket::NetAddress address;
    TLSSecureOptions secureOption;
    std::vector<std::string> caVec1 = {TlsUtilsTest::ChangeToFile(CA_DER)};
    secureOption.SetCaChain(caVec1);
    MockTlsSocketParamOptions(address, secureOption, options);

    server.Bind(address, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    server.Connect(options, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

void SetSocketHwTestLongParam(TLSSocket &server)
{
    TLSConnectOptions options;
    TLSSecureOptions secureOption;
    secureOption.SetCipherSuite("AES256-SHA256");
    std::string protocolV13 = "TLSv1.3";
    std::vector<std::string> protocolVec = {protocolV13};
    secureOption.SetProtocolChain(protocolVec);
    std::vector<std::string> caVect = {TlsUtilsTest::ChangeToFile(CA_DER)};
    secureOption.SetCaChain(caVect);
    Socket::NetAddress address;
    MockTlsSocketParamOptions(address, secureOption, options);

    server.Bind(address, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    server.Connect(options, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, bindInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaFileExistence("bindInterface")) {
        return;
    }

    Socket::NetAddress address;
    TLSSocket bindTestServer;
    MockNetAddress(address);
    bindTestServer.Bind(address, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, connectInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaFileExistence("connectInterface")) {
        return;
    }
    TLSSocket testService;
    SetSocketHwTestShortParam(testService);

    const std::string data = "how do you do? this is connectInterface";
    Socket::TCPSendOptions tcpSendOptions;
    tcpSendOptions.SetData(data);
    testService.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    sleep(2);

    (void)testService.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    sleep(2);
}

HWTEST_F(TlsSocketTest, startReadMessageInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaFileExistence("startReadMessageInterface")) {
        return;
    }
    TLSSocket testService;
    SetSocketHwTestShortParam(testService);

    const std::string data = "how do you do? this is startReadMessageInterface";
    Socket::TCPSendOptions tcpSendOptions;
    tcpSendOptions.SetData(data);
    testService.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    sleep(2);

    (void)testService.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, readMessageInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaFileExistence("readMessageInterface")) {
        return;
    }
    TLSSocket testService;
    SetSocketHwTestShortParam(testService);

    const std::string data = "how do you do? this is readMessageInterface";
    Socket::TCPSendOptions tcpSendOptions;
    tcpSendOptions.SetData(data);
    testService.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    sleep(2);

    (void)testService.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, closeInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaFileExistence("closeInterface")) {
        return;
    }

    TLSSocket testService;
    SetSocketHwTestShortParam(testService);

    const std::string data = "how do you do? this is closeInterface";
    Socket::TCPSendOptions tcpSendOptions;
    tcpSendOptions.SetData(data);

    testService.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    sleep(2);

    (void)testService.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, sendInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaFileExistence("sendInterface")) {
        return;
    }
    TLSSocket testService;
    SetSocketHwTestShortParam(testService);

    const std::string data = "how do you do? this is sendInterface";
    Socket::TCPSendOptions tcpSendOptions;
    tcpSendOptions.SetData(data);

    testService.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    sleep(2);

    (void)testService.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, getRemoteAddressInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaFileExistence("getRemoteAddressInterface")) {
        return;
    }

    TLSConnectOptions options;
    TLSSocket testService;
    TLSSecureOptions secureOption;
    Socket::NetAddress address;
    std::vector<std::string> caVec = {TlsUtilsTest::ChangeToFile(CA_DER)};
    secureOption.SetCaChain(caVec);
    MockTlsSocketParamOptions(address, secureOption, options);

    testService.Bind(address, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    testService.Connect(options, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });

    Socket::NetAddress netAddress;
    testService.GetRemoteAddress([&netAddress](int32_t errCode, const Socket::NetAddress &address) {
        EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS);
        netAddress.SetAddress(address.GetAddress());
        netAddress.SetFamilyBySaFamily(address.GetSaFamily());
        netAddress.SetPort(address.GetPort());
    });
    EXPECT_STREQ(netAddress.GetAddress().c_str(), TlsUtilsTest::GetIp(TlsUtilsTest::ChangeToFile(IP_ADDRESS)).c_str());
    EXPECT_EQ(address.GetPort(), std::atoi(TlsUtilsTest::ChangeToFile(PORT).c_str()));
    EXPECT_EQ(netAddress.GetSaFamily(), AF_INET);

    const std::string data = "how do you do? this is getRemoteAddressInterface";
    Socket::TCPSendOptions tcpSendOptions;
    tcpSendOptions.SetData(data);

    testService.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });

    (void)testService.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, getStateInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaFileExistence("getRemoteAddressInterface")) {
        return;
    }

    TLSSocket testService;
    SetSocketHwTestShortParam(testService);

    Socket::SocketStateBase TlsSocketstate;
    testService.GetState([&TlsSocketstate](int32_t errCode, const Socket::SocketStateBase &state) {
        EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS);
        TlsSocketstate = state;
    });
    std::cout << "TlsSocketTest TlsSocketstate.IsClose(): " << TlsSocketstate.IsClose() << std::endl;
    EXPECT_TRUE(TlsSocketstate.IsBound());
    EXPECT_TRUE(!TlsSocketstate.IsClose());
    EXPECT_TRUE(TlsSocketstate.IsConnected());

    const std::string tlsSocketTestData = "how do you do? this is getStateInterface";
    Socket::TCPSendOptions tcpSendOptions;
    tcpSendOptions.SetData(tlsSocketTestData);
    testService.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });

    sleep(2);

    (void)testService.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, getCertificateInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaFileExistence("getCertificateInterface")) {
        return;
    }
    TLSSocket testService;
    SetSocketHwTestShortParam(testService);

    const std::string data = "how do you do? This is UT test getCertificateInterface";
    Socket::TCPSendOptions tcpSendOptions;
    tcpSendOptions.SetData(data);
    testService.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });

    testService.GetCertificate(
        [](int32_t errCode, const X509CertRawData &cert) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });

    sleep(2);
    (void)testService.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, getRemoteCertificateInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaFileExistence("getRemoteCertificateInterface")) {
        return;
    }
    TLSSocket testService;
    SetSocketHwTestShortParam(testService);

    Socket::TCPSendOptions tcpSendOptions;
    const std::string data = "how do you do? This is UT test getRemoteCertificateInterface";
    tcpSendOptions.SetData(data);

    testService.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });

    testService.GetRemoteCertificate(
        [](int32_t errCode, const X509CertRawData &cert) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });

    sleep(2);
    (void)testService.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, protocolInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaFileExistence("protocolInterface")) {
        return;
    }
    TLSSocket testService;
    SetSocketHwTestLongParam(testService);

    const std::string data = "how do you do? this is protocolInterface";
    Socket::TCPSendOptions tcpSendOptions;
    tcpSendOptions.SetData(data);

    testService.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    std::string protocolVal;
    testService.GetProtocol([&protocolVal](int32_t errCode, const std::string &protocol) {
        EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS);
        protocolVal = protocol;
    });
    EXPECT_STREQ(protocolVal.c_str(), "TLSv1.3");

    Socket::SocketStateBase socketStateBase;
    testService.GetState([&socketStateBase](int32_t errCode, Socket::SocketStateBase state) {
        if (errCode == TLSSOCKET_SUCCESS) {
            EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS);
            socketStateBase.SetIsBound(state.IsBound());
            socketStateBase.SetIsClose(state.IsClose());
            socketStateBase.SetIsConnected(state.IsConnected());
        }
    });
    EXPECT_TRUE(socketStateBase.IsConnected());
    sleep(2);

    (void)testService.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, getCipherSuiteInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaFileExistence("getCipherSuiteInterface")) {
        return;
    }
    TLSSocket testService;
    SetSocketHwTestLongParam(testService);

    bool flag = false;
    const std::string data = "how do you do? This is getCipherSuiteInterface";
    Socket::TCPSendOptions tcpSendOptions;
    tcpSendOptions.SetData(data);
    testService.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });

    std::vector<std::string> cipherSuite;
    testService.GetCipherSuite([&cipherSuite](int32_t errCode, const std::vector<std::string> &suite) {
        if (errCode == TLSSOCKET_SUCCESS) {
            cipherSuite = suite;
        }
    });

    for (auto const &iter : cipherSuite) {
        if (iter == "AES256-SHA256") {
            flag = true;
        }
    }

    EXPECT_TRUE(flag);
    sleep(2);

    (void)testService.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, getSignatureAlgorithmsInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaFileExistence("getSignatureAlgorithmsInterface")) {
        return;
    }
    TLSSocket testService;
    TLSConnectOptions options;
    TLSSecureOptions secureOption;
    Socket::NetAddress address;
    std::string signatureAlgorithmVec = {"rsa_pss_rsae_sha256:ECDSA+SHA256"};
    secureOption.SetSignatureAlgorithms(signatureAlgorithmVec);
    std::string protocolV13 = "TLSv1.3";
    std::vector<std::string> protocolVec = {protocolV13};
    secureOption.SetProtocolChain(protocolVec);
    std::vector<std::string> caVec = {TlsUtilsTest::ChangeToFile(CA_DER)};
    secureOption.SetCaChain(caVec);
    MockTlsSocketParamOptions(address, secureOption, options);

    testService.Bind(address, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    testService.Connect(options, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });

    bool flag = false;
    const std::string data = "how do you do? this is getSignatureAlgorithmsInterface";
    Socket::TCPSendOptions tcpSendOptions;
    tcpSendOptions.SetData(data);
    testService.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });

    std::vector<std::string> signatureAlgorithms;
    testService.GetSignatureAlgorithms(
        [&signatureAlgorithms](int32_t errCode, const std::vector<std::string> &algorithms) {
            if (errCode == TLSSOCKET_SUCCESS) {
                signatureAlgorithms = algorithms;
            }
        });

    for (auto const &iter : signatureAlgorithms) {
        if (iter == "ECDSA+SHA256") {
            flag = true;
        }
    }
    EXPECT_TRUE(flag);
    sleep(2);
    (void)testService.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, onMessageDataInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaFileExistence("tlsSocketOnMessageData")) {
        return;
    }
    std::string getData = "server->client";
    TLSSocket testService;
    SetSocketHwTestLongParam(testService);

    testService.OnMessage([&getData](const std::string &data, const Socket::SocketRemoteInfo &remoteInfo) {
        if (data == getData) {
            EXPECT_TRUE(true);
        } else {
            EXPECT_TRUE(false);
        }
    });

    const std::string data = "how do you do? this is tlsSocketOnMessageData";
    Socket::TCPSendOptions tcpSendOptions;
    tcpSendOptions.SetData(data);
    testService.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });

    sleep(2);
    (void)testService.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, upgradeInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaFileExistence("upgradeInterface")) {
        return;
    }

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    EXPECT_TRUE(sock > 0);

    sockaddr_in addr4 = {0};
    Socket::NetAddress address;
    MockNetAddress(address);
    addr4.sin_family = AF_INET;
    addr4.sin_port = htons(address.GetPort());
    addr4.sin_addr.s_addr = inet_addr(address.GetAddress().c_str());

    int ret = connect(sock, reinterpret_cast<sockaddr *>(&addr4), sizeof(sockaddr_in));
    EXPECT_TRUE(ret >= 0);

    TLSSocket testService(sock);
    SetSocketHwTestShortParam(testService);

    const std::string data = "how do you do? this is upgradeInterface";
    Socket::TCPSendOptions tcpSendOptions;
    tcpSendOptions.SetData(data);
    testService.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    sleep(2);

    (void)testService.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    sleep(2);
}

HWTEST_F(TlsSocketTest, getSocketFdInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaFileExistence("getSocketFdInterface")) {
        return;
    }
    TLSSocket testService;
    SetSocketHwTestShortParam(testService);
    EXPECT_TRUE(testService.GetSocketFd() != -1);

    sleep(2);
    (void)testService.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}
} // namespace TlsSocket
} // namespace NetStack
} // namespace OHOS
