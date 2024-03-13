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
void SetCertChainOneWayHwTestShortParam(TLSSocket &server)
{
    TLSConnectOptions options;
    TLSSecureOptions secureOption;
    Socket::NetAddress address;

    address.SetAddress(TlsUtilsTest::GetIp(TlsUtilsTest::TlsUtilsTest::ChangeToFile(IP_ADDRESS)));
    address.SetPort(std::atoi(TlsUtilsTest::TlsUtilsTest::ChangeToFile(PORT).c_str()));
    address.SetFamilyBySaFamily(AF_INET);

    secureOption.SetKey(SecureData(TlsUtilsTest::TlsUtilsTest::ChangeToFile(PRIVATE_KEY_PEM_CHAIN)));
    std::vector<std::string> caVec = { TlsUtilsTest::TlsUtilsTest::ChangeToFile(CA_PATH_CHAIN),
        TlsUtilsTest::TlsUtilsTest::ChangeToFile(MID_CA_PATH_CHAIN) };
    secureOption.SetCaChain(caVec);
    secureOption.SetCert(TlsUtilsTest::TlsUtilsTest::ChangeToFile(CLIENT_CRT_CHAIN));

    options.SetNetAddress(address);
    options.SetTlsSecureOptions(secureOption);

    server.Bind(address, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    server.Connect(options, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

void SetCertChainOneWayHwTestLongParam(TLSSocket &server)
{
    TLSConnectOptions options;
    TLSSecureOptions secureOption;
    Socket::NetAddress address;

    address.SetAddress(TlsUtilsTest::GetIp(TlsUtilsTest::TlsUtilsTest::ChangeToFile(IP_ADDRESS)));
    address.SetPort(std::atoi(TlsUtilsTest::TlsUtilsTest::ChangeToFile(PORT).c_str()));
    address.SetFamilyBySaFamily(AF_INET);

    secureOption.SetKey(SecureData(TlsUtilsTest::TlsUtilsTest::ChangeToFile(PRIVATE_KEY_PEM_CHAIN)));
    std::vector<std::string> caVec = { TlsUtilsTest::TlsUtilsTest::ChangeToFile(CA_PATH_CHAIN),
        TlsUtilsTest::TlsUtilsTest::ChangeToFile(MID_CA_PATH_CHAIN) };
    std::string protocolV13 = "TLSv1.3";
    std::vector<std::string> protocolVec = { protocolV13 };
    secureOption.SetCaChain(caVec);
    secureOption.SetCert(TlsUtilsTest::TlsUtilsTest::ChangeToFile(CLIENT_CRT_CHAIN));
    secureOption.SetCipherSuite("AES256-SHA256");
    secureOption.SetProtocolChain(protocolVec);

    options.SetTlsSecureOptions(secureOption);
    options.SetNetAddress(address);

    server.Bind(address, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    server.Connect(options, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, bindInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaPathChainExistence("bindInterface")) {
        return;
    }

    TLSSocket srv;
    Socket::NetAddress address;

    address.SetAddress(TlsUtilsTest::GetIp(TlsUtilsTest::TlsUtilsTest::ChangeToFile(IP_ADDRESS)));
    address.SetPort(std::atoi(TlsUtilsTest::TlsUtilsTest::ChangeToFile(PORT).c_str()));
    address.SetFamilyBySaFamily(AF_INET);

    srv.Bind(address, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, connectInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaPathChainExistence("connectInterface")) {
        return;
    }
    TLSSocket server;
    SetCertChainOneWayHwTestShortParam(server);

    const std::string data = "how do you do? this is connectInterface";
    Socket::TCPSendOptions tcpSendOptions;
    tcpSendOptions.SetData(data);
    server.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    sleep(2);

    (void)server.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    sleep(2);
}

HWTEST_F(TlsSocketTest, closeInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaPathChainExistence("closeInterface")) {
        return;
    }
    TLSSocket server;
    SetCertChainOneWayHwTestShortParam(server);

    const std::string data = "how do you do? this is closeInterface";
    Socket::TCPSendOptions tcpSendOptions;
    tcpSendOptions.SetData(data);

    server.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    sleep(2);

    (void)server.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, sendInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaPathChainExistence("sendInterface")) {
        return;
    }
    TLSSocket server;
    SetCertChainOneWayHwTestShortParam(server);

    const std::string data = "how do you do? this is sendInterface";
    Socket::TCPSendOptions tcpSendOptions;
    tcpSendOptions.SetData(data);

    server.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    sleep(2);

    (void)server.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, getRemoteAddressInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaPathChainExistence("getRemoteAddressInterface")) {
        return;
    }
    TLSSocket server;
    TLSConnectOptions options;
    TLSSecureOptions secureOption;
    Socket::NetAddress address;

    address.SetAddress(TlsUtilsTest::GetIp(TlsUtilsTest::TlsUtilsTest::ChangeToFile(IP_ADDRESS)));
    address.SetPort(std::atoi(TlsUtilsTest::TlsUtilsTest::ChangeToFile(PORT).c_str()));
    address.SetFamilyBySaFamily(AF_INET);

    secureOption.SetKey(SecureData(TlsUtilsTest::TlsUtilsTest::ChangeToFile(PRIVATE_KEY_PEM_CHAIN)));
    std::vector<std::string> caVec = { TlsUtilsTest::TlsUtilsTest::ChangeToFile(CA_PATH_CHAIN),
        TlsUtilsTest::TlsUtilsTest::ChangeToFile(MID_CA_PATH_CHAIN) };
    secureOption.SetCaChain(caVec);
    secureOption.SetCert(TlsUtilsTest::TlsUtilsTest::ChangeToFile(CLIENT_CRT_CHAIN));

    options.SetTlsSecureOptions(secureOption);
    options.SetNetAddress(address);

    server.Bind(address, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    server.Connect(options, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });

    Socket::NetAddress netAddress;
    server.GetRemoteAddress([&netAddress](int32_t errCode, const Socket::NetAddress &address) {
        EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS);
        netAddress.SetPort(address.GetPort());
        netAddress.SetAddress(address.GetAddress());
        netAddress.SetFamilyBySaFamily(address.GetSaFamily());
    });
    EXPECT_STREQ(netAddress.GetAddress().c_str(),
        TlsUtilsTest::GetIp(TlsUtilsTest::TlsUtilsTest::ChangeToFile(IP_ADDRESS)).c_str());
    EXPECT_EQ(address.GetPort(), std::atoi(TlsUtilsTest::TlsUtilsTest::ChangeToFile(PORT).c_str()));
    EXPECT_EQ(netAddress.GetSaFamily(), AF_INET);

    const std::string data = "how do you do? this is getRemoteAddressInterface";
    Socket::TCPSendOptions tcpSendOptions;
    tcpSendOptions.SetData(data);

    server.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    sleep(2);

    (void)server.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, getStateInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaPathChainExistence("getRemoteAddressInterface")) {
        return;
    }
    TLSSocket server;
    SetCertChainOneWayHwTestShortParam(server);

    Socket::SocketStateBase TlsSocketstate;
    server.GetState([&TlsSocketstate](int32_t errCode, const Socket::SocketStateBase &state) {
        EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS);
        TlsSocketstate = state;
    });
    std::cout << "TlsSocketstate.IsClose(): " << TlsSocketstate.IsClose() << std::endl;
    EXPECT_TRUE(TlsSocketstate.IsBound());
    EXPECT_TRUE(!TlsSocketstate.IsClose());
    EXPECT_TRUE(TlsSocketstate.IsConnected());

    const std::string data = "how do you do? this is getStateInterface";
    Socket::TCPSendOptions tcpSendOptions;
    tcpSendOptions.SetData(data);
    server.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });

    sleep(2);

    (void)server.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, getRemoteCertificateInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaPathChainExistence("getRemoteCertificateInterface")) {
        return;
    }
    TLSSocket server;
    SetCertChainOneWayHwTestShortParam(server);
    Socket::TCPSendOptions tcpSendOptions;

    const std::string data = "how do you do? This is UT test getRemoteCertificateInterface";
    tcpSendOptions.SetData(data);
    server.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });

    server.GetRemoteCertificate(
        [](int32_t errCode, const X509CertRawData &cert) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });

    sleep(2);
    (void)server.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, protocolInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaPathChainExistence("protocolInterface")) {
        return;
    }
    TLSSocket server;
    SetCertChainOneWayHwTestLongParam(server);

    const std::string data = "how do you do? this is protocolInterface";
    Socket::TCPSendOptions tcpSendOptions;
    tcpSendOptions.SetData(data);

    server.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
    std::string getProtocolVal;
    server.GetProtocol([&getProtocolVal](int32_t errCode, const std::string &protocol) {
        EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS);
        getProtocolVal = protocol;
    });
    EXPECT_STREQ(getProtocolVal.c_str(), "TLSv1.3");
    sleep(2);

    (void)server.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, getCipherSuiteInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaPathChainExistence("getCipherSuiteInterface")) {
        return;
    }
    TLSSocket server;
    SetCertChainOneWayHwTestLongParam(server);

    bool flag = false;
    const std::string data = "how do you do? This is getCipherSuiteInterface";
    Socket::TCPSendOptions tcpSendOptions;
    tcpSendOptions.SetData(data);
    server.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });

    std::vector<std::string> cipherSuite;
    server.GetCipherSuite([&cipherSuite](int32_t errCode, const std::vector<std::string> &suite) {
        EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS);
        cipherSuite = suite;
    });

    for (auto const &iter : cipherSuite) {
        if (iter == "AES256-SHA256") {
            flag = true;
        }
    }

    EXPECT_TRUE(flag);
    sleep(2);

    (void)server.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}

HWTEST_F(TlsSocketTest, onMessageDataInterface, testing::ext::TestSize.Level2)
{
    if (!TlsUtilsTest::CheckCaPathChainExistence("tlsSocketOnMessageData")) {
        return;
    }
    std::string getData = "server->client";
    TLSSocket server;
    SetCertChainOneWayHwTestLongParam(server);

    server.OnMessage([&getData](const std::string &data, const Socket::SocketRemoteInfo &remoteInfo) {
        if (data == getData) {
            EXPECT_TRUE(true);
        } else {
            EXPECT_TRUE(false);
        }
    });

    const std::string data = "how do you do? this is tlsSocketOnMessageData";
    Socket::TCPSendOptions tcpSendOptions;
    tcpSendOptions.SetData(data);
    server.Send(tcpSendOptions, [](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });

    sleep(2);
    (void)server.Close([](int32_t errCode) { EXPECT_TRUE(errCode == TLSSOCKET_SUCCESS); });
}
} // namespace TlsSocket
} // namespace NetStack
} // namespace OHOS
