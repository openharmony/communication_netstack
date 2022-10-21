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

#include <chrono>
#include <netinet/tcp.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <regex>
#include <securec.h>
#include <thread>

#include "netstack_common_utils.h"
#include "netstack_log.h"
#include "tls.h"

#include "tls_socket.h"

namespace OHOS {
namespace NetStack {
namespace {
constexpr int WAIT_MS = 10;
constexpr int TIMEOUT_MS = 10000;
constexpr int REMOTE_CERT_LEN = 8192;
constexpr int COMMON_NAME_BUF_SIZE = 256;
constexpr int BUF_SIZE = 2048;
constexpr int SSL_RET_CODE = 0;
constexpr const char *SPLIT_ALT_NAMES = ",";
constexpr const char *SPLIT_HOST_NAME = ".";

int ConvertErrno()
{
    return TlsSocketError::SOCKET_ERROR_ERRNO_BASE + errno;
}

std::string MakeErrnoString()
{
    return strerror(errno);
}

int ConvertSSLError(ssl_st *ssl)
{
    if (!ssl) {
        return -1;
    }
    return TlsSocketError::SOCKET_ERROR_SSL_BASE + SSL_get_error(ssl, SSL_RET_CODE);
}

std::string MakeSSLErrorString(int error)
{
    char err[MAX_ERR_LEN] = {0};
    ERR_error_string_n(error - TlsSocketError::SOCKET_ERROR_SSL_BASE, err, sizeof(err));
    return err;
}
} // namespace

TLSSecureOptions &TLSSecureOptions::operator=(const TLSSecureOptions &tlsSecureOptions)
{
    key_ = tlsSecureOptions.GetKey();
    caChain_ = tlsSecureOptions.GetCaChain();
    cert_ = tlsSecureOptions.GetCert();
    protocolChain_ = tlsSecureOptions.GetProtocolChain();
    crlChain_ = tlsSecureOptions.GetCrlChain();
    keyPass_ = tlsSecureOptions.GetKeyPass();
    signatureAlgorithms_ = tlsSecureOptions.GetSignatureAlgorithms();
    cipherSuite_ = tlsSecureOptions.GetCipherSuite();
    useRemoteCipherPrefer_ = tlsSecureOptions.UseRemoteCipherPrefer();
    return *this;
}

void TLSSecureOptions::SetCaChain(const std::vector<std::string> &caChain)
{
    caChain_ = caChain;
}

void TLSSecureOptions::SetCert(const std::string &cert)
{
    cert_ = cert;
}

void TLSSecureOptions::SetKey(const std::string &key)
{
    key_ = key;
}

void TLSSecureOptions::SetKeyPass(const std::string &keyPass)
{
    keyPass_ = keyPass;
}

void TLSSecureOptions::SetProtocolChain(const std::vector<std::string> &protocolChain)
{
    protocolChain_ = protocolChain;
}

void TLSSecureOptions::SetUseRemoteCipherPrefer(bool useRemoteCipherPrefer)
{
    useRemoteCipherPrefer_ = useRemoteCipherPrefer;
}

void TLSSecureOptions::SetSignatureAlgorithms(const std::string &signatureAlgorithms)
{
    signatureAlgorithms_ = signatureAlgorithms;
}

void TLSSecureOptions::SetCipherSuite(const std::string &cipherSuite)
{
    cipherSuite_ = cipherSuite;
}

void TLSSecureOptions::SetCrlChain(const std::vector<std::string> &crlChain)
{
    crlChain_ = crlChain;
}

const std::vector<std::string> &TLSSecureOptions::GetCaChain() const
{
    return caChain_;
}

const std::string &TLSSecureOptions::GetCert() const
{
    return cert_;
}

const std::string &TLSSecureOptions::GetKey() const
{
    return key_;
}

const std::string &TLSSecureOptions::GetKeyPass() const
{
    return keyPass_;
}

const std::vector<std::string> &TLSSecureOptions::GetProtocolChain() const
{
    return protocolChain_;
}

bool TLSSecureOptions::UseRemoteCipherPrefer() const
{
    return useRemoteCipherPrefer_;
}

const std::string &TLSSecureOptions::GetSignatureAlgorithms() const
{
    return signatureAlgorithms_;
}

const std::string &TLSSecureOptions::GetCipherSuite() const
{
    return cipherSuite_;
}

const std::vector<std::string> &TLSSecureOptions::GetCrlChain() const
{
    return crlChain_;
}

void TLSConnectOptions::SetNetAddress(const NetAddress &address)
{
    address_.SetAddress(address.GetAddress());
    address_.SetPort(address.GetPort());
    address_.SetFamilyBySaFamily(address.GetSaFamily());
}

void TLSConnectOptions::SetTlsSecureOptions(TLSSecureOptions &tlsSecureOptions)
{
    tlsSecureOptions_ = tlsSecureOptions;
}

void TLSConnectOptions::SetCheckServerIdentity(const CheckServerIdentity &checkServerIdentity)
{
    checkServerIdentity_ = checkServerIdentity;
}

void TLSConnectOptions::SetAlpnProtocols(const std::vector<std::string> &alpnProtocols)
{
    alpnProtocols_ = alpnProtocols;
}

NetAddress TLSConnectOptions::GetNetAddress() const
{
    return address_;
}

TLSSecureOptions TLSConnectOptions::GetTlsSecureOptions() const
{
    return tlsSecureOptions_;
}

CheckServerIdentity TLSConnectOptions::GetCheckServerIdentity() const
{
    return checkServerIdentity_;
}

const std::vector<std::string> &TLSConnectOptions::GetAlpnProtocols() const
{
    return alpnProtocols_;
}

std::string TLSSocket::MakeAddressString(sockaddr *addr)
{
    if (addr->sa_family == AF_INET) {
        auto *addr4 = reinterpret_cast<sockaddr_in *>(addr);
        const char *str = inet_ntoa(addr4->sin_addr);
        if (str == nullptr || strlen(str) == 0) {
            return {};
        }
        return str;
    } else if (addr->sa_family == AF_INET6) {
        auto *addr6 = reinterpret_cast<sockaddr_in6 *>(addr);
        char str[INET6_ADDRSTRLEN] = {0};
        if (inet_ntop(AF_INET6, &addr6->sin6_addr, str, INET6_ADDRSTRLEN) == nullptr || strlen(str) == 0) {
            return {};
        }
        return str;
    }
    return {};
}

void TLSSocket::GetAddr(const NetAddress &address, sockaddr_in *addr4, sockaddr_in6 *addr6, sockaddr **addr,
                        socklen_t *len)
{
    sa_family_t family = address.GetSaFamily();
    if (family == AF_INET) {
        addr4->sin_family = AF_INET;
        addr4->sin_port = htons(address.GetPort());
        addr4->sin_addr.s_addr = inet_addr(address.GetAddress().c_str());
        *addr = reinterpret_cast<sockaddr *>(addr4);
        *len = sizeof(sockaddr_in);
    } else if (family == AF_INET6) {
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(address.GetPort());
        inet_pton(AF_INET6, address.GetAddress().c_str(), &addr6->sin6_addr);
        *addr = reinterpret_cast<sockaddr *>(addr6);
        *len = sizeof(sockaddr_in6);
    }
}

void TLSSocket::MakeIpSocket(sa_family_t family)
{
    if (family != AF_INET && family != AF_INET6) {
        return;
    }
    int sock = socket(family, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        int resErr = ConvertErrno();
        NETSTACK_LOGE("make ip socket is error, error is %{public}s %{public}d", MakeErrnoString().c_str(),
                      errno);
        CallOnErrorCallback(resErr, MakeErrnoString());
        return;
    }
    sockFd_ = sock;
}

void TLSSocket::StartReadMessage()
{
    std::thread thread([this]() {
        isRunning_ = true;
        isRunOver_ = false;
        char buffer[MAX_BUFFER_SIZE];
        bzero(buffer, MAX_BUFFER_SIZE);
        while (isRunning_) {
            int len = tlsSocketInternal_.Recv(buffer, MAX_BUFFER_SIZE);
            if (!isRunning_) {
                break;
            }
            if (len < 0) {
                int resErr = ConvertSSLError(tlsSocketInternal_.GetSSL());
                NETSTACK_LOGE("SSL_read function read error, errno is %{public}d, errno info is %{public}s", resErr,
                              MakeSSLErrorString(resErr).c_str());
                CallOnErrorCallback(resErr, MakeSSLErrorString(resErr));
                break;
            }

            if (len == 0) {
                continue;
            }

            SocketRemoteInfo remoteInfo;
            remoteInfo.SetSize(len);
            tlsSocketInternal_.MakeRemoteInfo(remoteInfo);
            CallOnMessageCallback(buffer, remoteInfo);
        }
        isRunOver_ = true;
    });
    thread.detach();
}

void TLSSocket::CallOnMessageCallback(const std::string &data, const OHOS::NetStack::SocketRemoteInfo &remoteInfo)
{
    OnMessageCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (onMessageCallback_) {
            func = onMessageCallback_;
        }
    }

    if (func) {
        func(data, remoteInfo);
    }
}

void TLSSocket::CallOnConnectCallback()
{
    OnConnectCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (onConnectCallback_) {
            func = onConnectCallback_;
        }
    }

    if (func) {
        func();
    }
}

void TLSSocket::CallOnCloseCallback()
{
    OnCloseCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (onCloseCallback_) {
            func = onCloseCallback_;
        }
    }

    if (func) {
        func();
    }
}

void TLSSocket::CallOnErrorCallback(int32_t err, const std::string &errString)
{
    OnErrorCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (onErrorCallback_) {
            func = onErrorCallback_;
        }
    }

    if (func) {
        func(err, errString);
    }
}

void TLSSocket::CallBindCallback(bool ok, BindCallback callback)
{
    bindCallback_ = callback;
    BindCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (bindCallback_) {
            func = bindCallback_;
        }
    }

    if (func) {
        func(ok);
    }
}

void TLSSocket::CallConnectCallback(bool ok, ConnectCallback callback)
{
    connectCallback_ = callback;
    ConnectCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (connectCallback_) {
            func = connectCallback_;
        }
    }

    if (func) {
        func(ok);
    }
}

void TLSSocket::CallSendCallback(bool ok, SendCallback callback)
{
    sendCallback_ = callback;
    SendCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (sendCallback_) {
            func = sendCallback_;
        }
    }

    if (func) {
        func(ok);
    }
}

void TLSSocket::CallCloseCallback(bool ok, CloseCallback callback)
{
    closeCallback_ = callback;
    CloseCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (closeCallback_) {
            func = closeCallback_;
        }
    }

    if (func) {
        func(ok);
    }
}

void TLSSocket::CallGetRemoteAddressCallback(bool ok, const NetAddress &address, GetRemoteAddressCallback callback)
{
    getRemoteAddressCallback_ = callback;
    GetRemoteAddressCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (getRemoteAddressCallback_) {
            func = getRemoteAddressCallback_;
        }
    }

    if (func) {
        func(ok, address);
    }
}

void TLSSocket::CallGetStateCallback(bool ok, const SocketStateBase &state, GetStateCallback callback)
{
    getStateCallback_ = callback;
    GetStateCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (getStateCallback_) {
            func = getStateCallback_;
        }
    }

    if (func) {
        func(ok, state);
    }
}

void TLSSocket::CallSetExtraOptionsCallback(bool ok, SetExtraOptionsCallback callback)
{
    setExtraOptionsCallback_ = callback;
    SetExtraOptionsCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (setExtraOptionsCallback_) {
            func = setExtraOptionsCallback_;
        }
    }

    if (func) {
        func(ok);
    }
}

void TLSSocket::CallGetCertificateCallback(bool ok, const std::string &cert, GetCertificateCallback callback)
{
    GetCertificateCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (getCertificateCallback_) {
            func = getCertificateCallback_;
        }
    }

    if (func) {
        func(ok, cert);
    }
}

void TLSSocket::CallGetRemoteCertificateCallback(bool ok, const std::string &cert,
                                                 GetRemoteCertificateCallback callback)
{
    GetRemoteCertificateCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (getRemoteCertificateCallback_) {
            func = getRemoteCertificateCallback_;
        }
    }

    if (func) {
        func(ok, cert);
    }
}

void TLSSocket::CallGetProtocolCallback(bool ok, const std::string &protocol, GetProtocolCallback callback)
{
    GetProtocolCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (getProtocolCallback_) {
            func = getProtocolCallback_;
        }
    }

    if (func) {
        func(ok, protocol);
    }
}

void TLSSocket::CallGetCipherSuiteCallback(bool ok, const std::vector<std::string> &suite,
                                           GetCipherSuiteCallback callback)
{
    GetCipherSuiteCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (getCipherSuiteCallback_) {
            func = getCipherSuiteCallback_;
        }
    }

    if (func) {
        func(ok, suite);
    }
}

void TLSSocket::CallGetSignatureAlgorithmsCallback(bool ok, const std::vector<std::string> &algorithms,
                                                   GetSignatureAlgorithmsCallback callback)
{
    GetSignatureAlgorithmsCallback func = nullptr;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (getSignatureAlgorithmsCallback_) {
            func = getSignatureAlgorithmsCallback_;
        }
    }

    if (func) {
        func(ok, algorithms);
    }
}

void TLSSocket::Bind(const OHOS::NetStack::NetAddress &address, const OHOS::NetStack::BindCallback &callback)
{
    if (sockFd_ >= 0) {
        CallBindCallback(true, callback);
        return;
    }

    MakeIpSocket(address.GetSaFamily());
    if (sockFd_ < 0) {
        int resErr = ConvertErrno();
        NETSTACK_LOGE("make tcp socket failed errno is %{public}d %{public}s", errno, MakeErrnoString().c_str());
        CallOnErrorCallback(resErr, MakeErrnoString());
        CallBindCallback(false, callback);
        return;
    }

    sockaddr_in addr4 = {0};
    sockaddr_in6 addr6 = {0};
    sockaddr *addr = nullptr;
    socklen_t len;
    GetAddr(address, &addr4, &addr6, &addr, &len);
    if (addr == nullptr) {
        NETSTACK_LOGE("TLSSocket::Bind Address Is Invalid");
        CallOnErrorCallback(-1, "Address Is Invalid");
        CallBindCallback(false, callback);
        return;
    }
    CallBindCallback(true, callback);
}

void TLSSocket::Connect(OHOS::NetStack::TLSConnectOptions &tlsConnectOptions,
                        const OHOS::NetStack::ConnectCallback &callback)
{
    if (sockFd_ < 0) {
        int resErr = ConvertErrno();
        NETSTACK_LOGE("connect error is %{public}s %{public}d", MakeErrnoString().c_str(), errno);
        CallOnErrorCallback(resErr, MakeErrnoString());
        callback(false);
        return;
    }

    auto res = tlsSocketInternal_.TlsConnectToHost(sockFd_, tlsConnectOptions);
    if (!res) {
        int resErr = ConvertSSLError(tlsSocketInternal_.GetSSL());
        CallOnErrorCallback(resErr, MakeSSLErrorString(resErr));
        callback(false);
        return;
    }

    StartReadMessage();
    CallOnConnectCallback();
    callback(true);
}

void TLSSocket::Send(const OHOS::NetStack::TCPSendOptions &tcpSendOptions, const OHOS::NetStack::SendCallback &callback)
{
    (void)tcpSendOptions;

    auto res = tlsSocketInternal_.Send(tcpSendOptions.GetData());
    if (!res) {
        int resErr = ConvertSSLError(tlsSocketInternal_.GetSSL());
        CallOnErrorCallback(resErr, MakeSSLErrorString(resErr));
        CallSendCallback(false, callback);
        return;
    }
    CallSendCallback(true, callback);
}

bool WaitConditionWithTimeout(bool *flag, int32_t timeoutMs)
{
    int MAX_WAIT_CNT = timeoutMs / WAIT_MS;
    int cnt = 0;
    while (!(*flag)) {
        if (cnt >= MAX_WAIT_CNT) {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(WAIT_MS));
        cnt++;
    }
    return true;
}

void TLSSocket::Close(const OHOS::NetStack::CloseCallback &callback)
{
    if (!WaitConditionWithTimeout(&isRunning_, TIMEOUT_MS)) {
        callback(false);
        return;
    }
    isRunning_ = false;
    if (!WaitConditionWithTimeout(&isRunOver_, TIMEOUT_MS)) {
        callback(false);
        return;
    }
    auto res = tlsSocketInternal_.Close();
    if (!res) {
        int resErr = ConvertSSLError(tlsSocketInternal_.GetSSL());
        NETSTACK_LOGE("close error is %{public}s %{public}d", MakeSSLErrorString(resErr).c_str(), resErr);
        CallOnErrorCallback(resErr, MakeSSLErrorString(resErr));
        callback(false);
        return;
    }
    CallOnCloseCallback();
    callback(true);
}

void TLSSocket::GetRemoteAddress(const OHOS::NetStack::GetRemoteAddressCallback &callback)
{
    sa_family_t family;
    socklen_t len = sizeof(family);
    int ret = getsockname(sockFd_, reinterpret_cast<sockaddr *>(&family), &len);
    if (ret < 0) {
        int resErr = ConvertErrno();
        NETSTACK_LOGE("getsockname failed errno %{public}d", resErr);
        CallOnErrorCallback(resErr, MakeErrnoString());
        CallGetRemoteAddressCallback(false, {}, callback);
        return;
    }

    if (family == AF_INET) {
        GetIp4RemoteAddress(callback);
    } else if (family == AF_INET6) {
        GetIp6RemoteAddress(callback);
    }
}

void TLSSocket::GetIp4RemoteAddress(const OHOS::NetStack::GetRemoteAddressCallback &callback)
{
    sockaddr_in addr4 = {0};
    socklen_t len4 = sizeof(sockaddr_in);

    int ret = getpeername(sockFd_, reinterpret_cast<sockaddr *>(&addr4), &len4);
    if (ret < 0) {
        int resErr = ConvertErrno();
        NETSTACK_LOGE("GetIp4RemoteAddress failed errno %{public}d", resErr);
        CallOnErrorCallback(resErr, MakeErrnoString());
        CallGetRemoteAddressCallback(false, {}, callback);
        return;
    }

    std::string address = MakeAddressString(reinterpret_cast<sockaddr *>(&addr4));
    if (address.empty()) {
        NETSTACK_LOGE("GetIp4RemoteAddress failed errno %{public}d", errno);
        CallOnErrorCallback(-1, "Address is invalid");
        CallGetRemoteAddressCallback(false, {}, callback);
        return;
    }
    NetAddress netAddress;
    netAddress.SetAddress(address);
    netAddress.SetFamilyBySaFamily(AF_INET);
    netAddress.SetPort(ntohs(addr4.sin_port));
    CallGetRemoteAddressCallback(true, netAddress, callback);
}

void TLSSocket::GetIp6RemoteAddress(const OHOS::NetStack::GetRemoteAddressCallback &callback)
{
    sockaddr_in6 addr6 = {0};
    socklen_t len6 = sizeof(sockaddr_in6);

    int ret = getpeername(sockFd_, reinterpret_cast<sockaddr *>(&addr6), &len6);
    if (ret < 0) {
        int resErr = ConvertErrno();
        NETSTACK_LOGE("GetIp6RemoteAddress failed errno %{public}d", resErr);
        CallOnErrorCallback(resErr, MakeErrnoString());
        CallGetRemoteAddressCallback(false, {}, callback);
        return;
    }

    std::string address = MakeAddressString(reinterpret_cast<sockaddr *>(&addr6));
    if (address.empty()) {
        NETSTACK_LOGE("GetIp6RemoteAddress failed errno %{public}d", errno);
        CallOnErrorCallback(-1, "Address is invalid");
        CallGetRemoteAddressCallback(false, {}, callback);
        return;
    }
    NetAddress netAddress;
    netAddress.SetAddress(address);
    netAddress.SetFamilyBySaFamily(AF_INET6);
    netAddress.SetPort(ntohs(addr6.sin6_port));
    CallGetRemoteAddressCallback(true, netAddress, callback);
}

void TLSSocket::GetState(const OHOS::NetStack::GetStateCallback &callback)
{
    int opt;
    socklen_t optLen = sizeof(int);
    int r = getsockopt(sockFd_, SOL_SOCKET, SO_TYPE, &opt, &optLen);
    if (r < 0) {
        SocketStateBase state;
        state.SetIsClose(true);
        CallGetStateCallback(false, state, callback);
        return;
    }
    sa_family_t family;
    socklen_t len = sizeof(family);
    SocketStateBase state;
    int ret = getsockname(sockFd_, reinterpret_cast<sockaddr *>(&family), &len);
    state.SetIsBound(ret == 0);
    ret = getpeername(sockFd_, reinterpret_cast<sockaddr *>(&family), &len);
    state.SetIsConnected(ret == 0);
    CallGetStateCallback(true, state, callback);
}

bool TLSSocket::SetBaseOptions(const ExtraOptionsBase &option) const
{
    if (option.GetReceiveBufferSize() != 0) {
        int size = (int)option.GetReceiveBufferSize();
        if (setsockopt(sockFd_, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<void *>(&size), sizeof(size)) < 0) {
            return false;
        }
    }

    if (option.GetSendBufferSize() != 0) {
        int size = (int)option.GetSendBufferSize();
        if (setsockopt(sockFd_, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<void *>(&size), sizeof(size)) < 0) {
            return false;
        }
    }

    if (option.IsReuseAddress()) {
        int reuse = 1;
        if (setsockopt(sockFd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<void *>(&reuse), sizeof(reuse)) < 0) {
            return false;
        }
    }

    if (option.GetSocketTimeout() != 0) {
        timeval timeout = {(int)option.GetSocketTimeout(), 0};
        if (setsockopt(sockFd_, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<void *>(&timeout), sizeof(timeout)) < 0) {
            return false;
        }
        if (setsockopt(sockFd_, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<void *>(&timeout), sizeof(timeout)) < 0) {
            return false;
        }
    }

    return true;
}

bool TLSSocket::SetExtraOptions(const TCPExtraOptions &option) const
{
    if (option.IsKeepAlive()) {
        int keepalive = 1;
        if (setsockopt(sockFd_, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive)) < 0) {
            return false;
        }
    }

    if (option.IsOOBInline()) {
        int oobInline = 1;
        if (setsockopt(sockFd_, SOL_SOCKET, SO_OOBINLINE, &oobInline, sizeof(oobInline)) < 0) {
            return false;
        }
    }

    if (option.IsTCPNoDelay()) {
        int tcpNoDelay = 1;
        if (setsockopt(sockFd_, IPPROTO_TCP, TCP_NODELAY, &tcpNoDelay, sizeof(tcpNoDelay)) < 0) {
            return false;
        }
    }

    linger soLinger = {0};
    soLinger.l_onoff = option.socketLinger.IsOn();
    soLinger.l_linger = (int)option.socketLinger.GetLinger();
    if (setsockopt(sockFd_, SOL_SOCKET, SO_LINGER, &soLinger, sizeof(soLinger)) < 0) {
        return false;
    }

    return true;
}

void TLSSocket::SetExtraOptions(const OHOS::NetStack::TCPExtraOptions &tcpExtraOptions,
                                const OHOS::NetStack::SetExtraOptionsCallback &callback)
{
    if (!SetBaseOptions(tcpExtraOptions)) {
        NETSTACK_LOGE("SetExtraOptions errno %{public}d", errno);
        CallOnErrorCallback(errno, MakeErrnoString());
        CallSetExtraOptionsCallback(false, callback);
        return;
    }

    if (!SetExtraOptions(tcpExtraOptions)) {
        NETSTACK_LOGE("SetExtraOptions errno %{public}d", errno);
        CallOnErrorCallback(errno, MakeErrnoString());
        CallSetExtraOptionsCallback(false, callback);
        return;
    }

    CallSetExtraOptionsCallback(true, callback);
}

void TLSSocket::GetCertificate(const GetCertificateCallback &callback)
{
    const auto &cert = tlsSocketInternal_.GetCertificate();
    if (cert.empty()) {
        int resErr = ConvertSSLError(tlsSocketInternal_.GetSSL());
        NETSTACK_LOGE("GetCertificate errno %{public}d, %{public}s", resErr, MakeSSLErrorString(resErr).c_str());
        CallOnErrorCallback(resErr, MakeSSLErrorString(resErr));
        callback(false, "");
        return;
    }
    callback(true, cert);
}

void TLSSocket::GetRemoteCertificate(const GetRemoteCertificateCallback &callback)
{
    const auto &remoteCert = tlsSocketInternal_.GetRemoteCertificate();
    if (remoteCert.empty()) {
        int resErr = ConvertSSLError(tlsSocketInternal_.GetSSL());
        NETSTACK_LOGE("GetRemoteCertificate errno %{public}d, %{public}s", resErr, MakeSSLErrorString(resErr).c_str());
        CallOnErrorCallback(resErr, MakeSSLErrorString(resErr));
        callback(false, "");
        return;
    }
    callback(true, remoteCert);
}

void TLSSocket::GetProtocol(const GetProtocolCallback &callback)
{
    const auto &protocol = tlsSocketInternal_.GetProtocol();
    if (protocol.empty()) {
        NETSTACK_LOGE("GetProtocol errno %{public}d", errno);
        int resErr = ConvertSSLError(tlsSocketInternal_.GetSSL());
        CallOnErrorCallback(resErr, MakeSSLErrorString(resErr));
        callback(false, "");
        return;
    }
    callback(true, protocol);
}

void TLSSocket::GetCipherSuite(const GetCipherSuiteCallback &callback)
{
    const auto &cipherSuite = tlsSocketInternal_.GetCipherSuite();
    if (cipherSuite.empty()) {
        NETSTACK_LOGE("GetCipherSuite errno %{public}d", errno);
        int resErr = ConvertSSLError(tlsSocketInternal_.GetSSL());
        CallOnErrorCallback(resErr, MakeSSLErrorString(resErr));
        callback(false, cipherSuite);
        return;
    }
    callback(true, cipherSuite);
}

void TLSSocket::GetSignatureAlgorithms(const GetSignatureAlgorithmsCallback &callback)
{
    const auto &signatureAlgorithms = tlsSocketInternal_.GetSignatureAlgorithms();
    if (signatureAlgorithms.empty()) {
        NETSTACK_LOGE("GetSignatureAlgorithms errno %{public}d", errno);
        int resErr = ConvertSSLError(tlsSocketInternal_.GetSSL());
        CallOnErrorCallback(resErr, MakeSSLErrorString(resErr));
        callback(false, {});
        return;
    }
    callback(true, signatureAlgorithms);
}

void TLSSocket::OnMessage(const OHOS::NetStack::OnMessageCallback &onMessageCallback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onMessageCallback_ = onMessageCallback;
}

void TLSSocket::OffMessage()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (onMessageCallback_) {
        onMessageCallback_ = nullptr;
    }
}

void TLSSocket::OnConnect(const OnConnectCallback &onConnectCallback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onConnectCallback_ = onConnectCallback;
}

void TLSSocket::OffConnect()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (onConnectCallback_) {
        onConnectCallback_ = nullptr;
    }
}

void TLSSocket::OnClose(const OnCloseCallback &onCloseCallback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onCloseCallback_ = onCloseCallback;
}

void TLSSocket::OffClose()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (onCloseCallback_) {
        onCloseCallback_ = nullptr;
    }
}

void TLSSocket::OnError(const OnErrorCallback &onErrorCallback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    onErrorCallback_ = onErrorCallback;
}

void TLSSocket::OffError()
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (onErrorCallback_) {
        onErrorCallback_ = nullptr;
    }
}

bool ExecSocketConnect(const std::string &hostName, int port, sa_family_t family, int socketDescriptor)
{
    struct sockaddr_in dest = {0};
    bzero(&dest, sizeof(dest));
    dest.sin_family = family;
    dest.sin_port = htons(port);
    if (inet_aton(hostName.c_str(), (struct in_addr *)&dest.sin_addr.s_addr) == 0) {
        NETSTACK_LOGE("inet_aton is error, hostName is %s", hostName.c_str());
        return false;
    }
    int connectResult = connect(socketDescriptor, reinterpret_cast<struct sockaddr *>(&dest), sizeof(dest));
    if (connectResult == -1) {
        NETSTACK_LOGE("socket connect error!The error code is %{public}d, The error message is %{public}s", errno,
                      strerror(errno));
        return false;
    }
    return true;
}

bool TLSSocket::TLSSocketInternal::TlsConnectToHost(int sock, const TLSConnectOptions &options)
{
    configuration_.SetPrivateKey(options.GetTlsSecureOptions().GetKey(), options.GetTlsSecureOptions().GetKeyPass());
    configuration_.SetCaCertificate(options.GetTlsSecureOptions().GetCaChain());
    configuration_.SetLocalCertificate(options.GetTlsSecureOptions().GetCert());
    std::string cipherSuite = options.GetTlsSecureOptions().GetCipherSuite();
    if (!cipherSuite.empty()) {
        configuration_.SetCipherSuite(cipherSuite);
    }
    std::string signatureAlgorithms = options.GetTlsSecureOptions().GetSignatureAlgorithms();
    if (!signatureAlgorithms.empty()) {
        configuration_.SetSignatureAlgorithms(signatureAlgorithms);
    }
    const auto protocolVec = options.GetTlsSecureOptions().GetProtocolChain();
    if (!protocolVec.empty()) {
        configuration_.SetProtocol(protocolVec);
    }

    hostName_ = options.GetNetAddress().GetAddress();
    port_ = options.GetNetAddress().GetPort();
    family_ = options.GetNetAddress().GetSaFamily();
    socketDescriptor_ = sock;
    if (!ExecSocketConnect(options.GetNetAddress().GetAddress(), options.GetNetAddress().GetPort(),
                           options.GetNetAddress().GetSaFamily(), socketDescriptor_)) {
        return false;
    }
    return StartTlsConnected(options);
}

void TLSSocket::TLSSocketInternal::SetTlsConfiguration(const TLSConnectOptions &config)
{
    configuration_.SetPrivateKey(config.GetTlsSecureOptions().GetKey(), config.GetTlsSecureOptions().GetKeyPass());
    configuration_.SetLocalCertificate(config.GetTlsSecureOptions().GetCert());
    configuration_.SetCaCertificate(config.GetTlsSecureOptions().GetCaChain());
}

bool TLSSocket::TLSSocketInternal::Send(const std::string &data)
{
    NETSTACK_LOGD("data to send :%{public}s", data.c_str());
    if (data.empty()) {
        NETSTACK_LOGE("data is empty");
        return false;
    }
    if (!ssl_) {
        NETSTACK_LOGE("ssl is null");
        return false;
    }
    int len = SSL_write(ssl_, data.c_str(), data.length());
    if (len < 0) {
        int resErr = ConvertSSLError(GetSSL());
        NETSTACK_LOGE("data '%{public}s' send failed!The error code is %{public}d, The error message is'%{public}s'",
                      data.c_str(), resErr, MakeSSLErrorString(resErr).c_str());
        return false;
    }
    NETSTACK_LOGD("data '%{public}s' Sent successfully,sent in total %{public}d bytes!", data.c_str(), len);
    return true;
}
int TLSSocket::TLSSocketInternal::Recv(char *buffer, int MAX_BUFFER_SIZE)
{
    if (!ssl_) {
        NETSTACK_LOGE("ssl is null");
        return -1;
    }
    return SSL_read(ssl_, buffer, MAX_BUFFER_SIZE);
}

bool TLSSocket::TLSSocketInternal::Close()
{
    if (!ssl_) {
        NETSTACK_LOGE("ssl is null");
        return false;
    }
    int result = SSL_shutdown(ssl_);
    if (result < 0) {
        int resErr = ConvertSSLError(GetSSL());
        NETSTACK_LOGE("Error in shutdown, errno is %{public}d, error info is %{public}s", resErr,
                      MakeSSLErrorString(resErr).c_str());
        return false;
    }
    SSL_free(ssl_);
    close(socketDescriptor_);
    tlsContextPointer_->CloseCtx();
    return true;
}

bool TLSSocket::TLSSocketInternal::SetAlpnProtocols(const std::vector<std::string> &alpnProtocols)
{
    if (!ssl_) {
        NETSTACK_LOGE("ssl is null");
        return false;
    }
    size_t len = 0;
    size_t pos = 0;
    for (const auto &str : alpnProtocols) {
        len += str.length();
    }
    auto result = std::make_unique<unsigned char[]>(alpnProtocols.size() + len);
    for (const auto &str : alpnProtocols) {
        len = str.length();
        result[pos++] = len;
        if (!strcpy_s(reinterpret_cast<char *>(&result[pos]), len, str.c_str())) {
            NETSTACK_LOGE("strcpy_s failed");
            return false;
        }
        pos += len;
    }
    result[pos] = '\0';

    NETSTACK_LOGI("alpnProtocols after splicing %{public}s", result.get());
    if (SSL_set_alpn_protos(ssl_, result.get(), pos)) {
        int resErr = ConvertSSLError(GetSSL());
        NETSTACK_LOGE("Failed to set negotiable protocol list, errno is %{public}d, error info is %{public}s", resErr,
                      MakeSSLErrorString(resErr).c_str());
        return false;
    }
    return true;
}

void TLSSocket::TLSSocketInternal::MakeRemoteInfo(SocketRemoteInfo &remoteInfo)
{
    remoteInfo.SetAddress(hostName_);
    remoteInfo.SetPort(port_);
    remoteInfo.SetFamily(family_);
}

TLSConfiguration TLSSocket::TLSSocketInternal::GetTlsConfiguration() const
{
    return configuration_;
}

std::vector<std::string> TLSSocket::TLSSocketInternal::GetCipherSuite() const
{
    if (!ssl_) {
        NETSTACK_LOGE("ssl in null");
        return {};
    }
    STACK_OF(SSL_CIPHER) *sk = SSL_get_ciphers(ssl_);
    if (!sk) {
        NETSTACK_LOGE("get ciphers failed");
        return {};
    }
    CipherSuite cipherSuite;
    std::vector<std::string> cipherSuiteVec;
    for (int i = 0; i < sk_SSL_CIPHER_num(sk); i++) {
        const SSL_CIPHER *c = sk_SSL_CIPHER_value(sk, i);
        cipherSuite.cipherId_ = SSL_CIPHER_get_id(c);
        cipherSuite.cipherName_ = SSL_CIPHER_get_name(c);
        cipherSuiteVec.push_back(cipherSuite.cipherName_);
        NETSTACK_LOGI("SSL_CIPHER_get_id = %{public}lu, SSL_CIPHER_get_name = %{public}s", cipherSuite.cipherId_,
                      cipherSuite.cipherName_.c_str());
    }
    return cipherSuiteVec;
}

std::string TLSSocket::TLSSocketInternal::GetRemoteCertificate() const
{
    return remoteCert_;
}

std::string TLSSocket::TLSSocketInternal::GetCertificate() const
{
    return configuration_.GetCertificate();
}

std::vector<std::string> TLSSocket::TLSSocketInternal::GetSignatureAlgorithms() const
{
    return signatureAlgorithms_;
}

std::string TLSSocket::TLSSocketInternal::GetProtocol() const
{
    if (!ssl_) {
        NETSTACK_LOGE("ssl in null");
        return "UNKNOW_PROTOCOL";
    }
    if (configuration_.GetProtocol() == TLS_V1_3) {
        return PROTOCOL_TLS_V13;
    }
    return PROTOCOL_TLS_V12;
}

bool TLSSocket::TLSSocketInternal::SetSharedSigals()
{
    if (!ssl_) {
        NETSTACK_LOGE("ssl is null");
        return false;
    }
    int number = SSL_get_shared_sigalgs(ssl_, 0, nullptr, nullptr, nullptr, nullptr, nullptr);
    if (!number) {
        NETSTACK_LOGE("SSL_get_shared_sigalgs return value error");
        return false;
    }
    for (int i = 0; i < number; i++) {
        int hash_nid;
        int sign_nid;
        std::string sig_with_md;
        SSL_get_shared_sigalgs(ssl_, i, &sign_nid, &hash_nid, nullptr, nullptr, nullptr);
        switch (sign_nid) {
            case EVP_PKEY_RSA:
                sig_with_md = "RSA+";
                break;
            case EVP_PKEY_RSA_PSS:
                sig_with_md = "RSA-PSS+";
                break;
            case EVP_PKEY_DSA:
                sig_with_md = "DSA+";
                break;
            case EVP_PKEY_EC:
                sig_with_md = "ECDSA+";
                break;
            case NID_ED25519:
                sig_with_md = "Ed25519+";
                break;
            case NID_ED448:
                sig_with_md = "Ed448+";
                break;
            default:
                const char *sn = OBJ_nid2sn(sign_nid);
                if (sn != nullptr) {
                    sig_with_md = std::string(sn) + "+";
                } else {
                    sig_with_md = "UNDEF+";
                }
                break;
        }
        const char *sn_hash = OBJ_nid2sn(hash_nid);
        if (sn_hash != nullptr) {
            sig_with_md += std::string(sn_hash);
        } else {
            sig_with_md += "UNDEF";
        }
        signatureAlgorithms_.push_back(sig_with_md);
    }
    return true;
}

bool TLSSocket::TLSSocketInternal::StartTlsConnected(const TLSConnectOptions &options)
{
    if (!CreatTlsContext()) {
        return false;
    }
    if (!StartShakingHands(options)) {
        return false;
    }
    return true;
}

bool TLSSocket::TLSSocketInternal::CreatTlsContext()
{
    tlsContextPointer_ = TLSContext::CreateConfiguration(SSL_CLIENT_MODE, configuration_);
    if (!tlsContextPointer_) {
        return false;
    }
    if (!(ssl_ = tlsContextPointer_->CreateSsl())) {
        NETSTACK_LOGD("Error creating SSL session");
        return false;
    }
    SSL_set_fd(ssl_, socketDescriptor_);
    SSL_set_connect_state(ssl_);
    return true;
}

std::vector<std::string> SplitEscapedAltNames(std::string &altNames)
{
    std::vector<std::string> result;
    std::string currentToken;
    int offset = 0;
    constexpr int OFFSET = 2;
    while (offset != altNames.length()) {
        int nextSep = altNames.find_first_of(", ");
        int nextQuote = altNames.find_first_of('\"');
        if (nextQuote != -1 && (nextSep != -1 || nextQuote < nextSep)) {
            currentToken += altNames.substr(offset, nextQuote);
            std::regex jsonStringPattern(R"(/^"(?:[^"\\\u0000-\u001f]|\\(?:["\\/bfnrt]|u[0-9a-fA-F]{4}))*"/)");
            std::smatch result;
            std::string altNameSubStr = altNames.substr(nextQuote);
            bool ret = regex_match(altNameSubStr, result, jsonStringPattern);
            if (!ret) {
                return {""};
            }
            currentToken += result[0];
            offset = nextQuote + result[0].length();
        } else if (nextSep != -1) {
            currentToken += altNames.substr(offset, nextSep);
            result.push_back(currentToken);
            currentToken = "";
            offset = nextSep + OFFSET;
        } else {
            currentToken += altNames.substr(offset);
            offset = altNames.length();
        }
    }
    result.push_back(currentToken);
    return result;
}

bool IsIP(const std::string &ip)
{
    std::regex pattern(
        "((25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|"
        "2[0-4][0-9]|[01]?[0-9][0-9]?)");
    std::smatch res;
    if (regex_match(ip, res, pattern)) {
        return true;
    }
    return false;
}

std::vector<std::string> SplitHostName(std::string &hostName)
{
    transform(hostName.begin(), hostName.end(), hostName.begin(), ::tolower);
    auto vec = CommonUtils::Split(hostName, SPLIT_HOST_NAME);
    return vec;
}

bool SeekIntersection(std::vector<std::string> &vecA, std::vector<std::string> &vecB)
{
    std::vector<std::string> result;
    set_intersection(vecA.begin(), vecA.end(), vecB.begin(), vecB.end(), inserter(result, result.begin()));
    if (result.empty()) {
        return false;
    }
    return true;
}

static bool StartsWith(const std::string &s, const std::string &prefix)
{
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

std::tuple<bool, std::string> CheckIpAndDnsName(const std::string &hostName, std::vector<std::string> dnsNames,
                                                std::vector<std::string> ips, const X509 *x509Certificates)
{
    bool valid = false;
    std::string hostname = hostName;
    hostname = "" + hostname;
    std::string reason = "Unknown reason";
    int index = X509_get_ext_by_NID(x509Certificates, NID_commonName, -1);
    if (IsIP(hostName)) {
        auto it = find(ips.begin(), ips.end(), hostName);
        if (it == ips.end()) {
            reason = "IP: " + hostName + " is not in the cert's list";
        }
        return {valid, reason};
    }
    if (!dnsNames.empty() || index > 0) {
        std::vector<std::string> hostParts = SplitHostName(hostname);
        if (!dnsNames.empty()) {
            valid = SeekIntersection(hostParts, dnsNames);
            if (!valid) {
                reason = "Host: " + hostname + ". is not in the cert's altnames";
            }
        } else {
            char commonNameBuf[COMMON_NAME_BUF_SIZE] = {0};
            X509_NAME *pSubName = nullptr;
            int len = X509_NAME_get_text_by_NID(pSubName, NID_commonName, commonNameBuf, COMMON_NAME_BUF_SIZE);
            if (len > 0) {
                std::vector<std::string> commonNameVec;
                commonNameVec.emplace_back(commonNameBuf);
                valid = SeekIntersection(hostParts, commonNameVec);
                if (!valid) {
                    reason = "Host: " + hostname + ". is not cert's CN";
                }
            }
        }
        return {valid, reason};
    }
    reason = "Cert does not contain a DNS name";
    return {valid, reason};
}

std::string TLSSocket::TLSSocketInternal::CheckServerIdentityLegal(const std::string &hostName,
                                                                   const X509 *x509Certificates)
{
    std::string hostname = hostName;

    X509_NAME *subjectName = X509_get_subject_name(x509Certificates);
    char subNameBuf[BUF_SIZE] = {0};
    std::string subName = X509_NAME_oneline(subjectName, subNameBuf, BUF_SIZE);

    int index = X509_get_ext_by_NID(x509Certificates, NID_subject_alt_name, -1);
    if (index < 0) {
        return "X509 get ext nid error";
    }
    X509_EXTENSION *ext = X509_get_ext(x509Certificates, index);
    if (ext == nullptr) {
        return "X509 get ext error";
    }
    ASN1_OBJECT *obj = nullptr;
    obj = X509_EXTENSION_get_object(ext);
    char subAltNameBuf[BUF_SIZE] = {0};
    OBJ_obj2txt(subAltNameBuf, BUF_SIZE, obj, 0);
    NETSTACK_LOGD("extions obj : %s\n", subAltNameBuf);

    ASN1_OCTET_STRING *data = X509_EXTENSION_get_data(ext);
    std::string altNames = (char *)data->data;

    BIO *bio = BIO_new(BIO_s_file());
    BIO_set_fp(bio, stdout, BIO_NOCLOSE);
    ASN1_STRING_print(bio, data);

    std::vector<std::string> dnsNames = {};
    std::vector<std::string> ips = {};
    std::vector<std::string> splitAltNames = {};
    constexpr int DNS_NAME_IDX = 4;
    constexpr int IP_NAME_IDX = 11;
    hostname = "" + hostname;
    if (!altNames.empty()) {
        if (altNames.find('\"') != std::string::npos) {
            splitAltNames = SplitEscapedAltNames(altNames);
        } else {
            splitAltNames = CommonUtils::Split(altNames, SPLIT_ALT_NAMES);
        }
        for (auto const &iter : splitAltNames) {
            if (StartsWith(iter, "DNS:")) {
                dnsNames.push_back(iter.substr(DNS_NAME_IDX));
            } else if (StartsWith(iter, "IP Address:")) {
                ips.push_back(iter.substr(IP_NAME_IDX));
            }
        }
    }
    auto [ret, reason] = CheckIpAndDnsName(hostName, dnsNames, ips, x509Certificates);
    if (!ret) {
        return "Hostname/IP does not match certificate's altnames: " + reason;
    }
    return "Host: " + hostname + ". is cert's CN";
}

bool TLSSocket::TLSSocketInternal::StartShakingHands(const TLSConnectOptions &options)
{
    if (!ssl_) {
        NETSTACK_LOGE("ssl is null");
        return false;
    }
    int result = SSL_connect(ssl_);
    if (result == -1) {
        int errorStatus = ConvertSSLError(ssl_);
        NETSTACK_LOGE("SSL connect is error, errno is %{public}d, error info is %{public}s", errorStatus,
                      MakeSSLErrorString(errorStatus).c_str());
        return false;
    }

    std::string list = SSL_get_cipher_list(ssl_, 0);
    NETSTACK_LOGI("SSL_get_cipher_list: %{public}s", list.c_str());
    configuration_.SetCipherSuite(list);
    if (!SetSharedSigals()) {
        NETSTACK_LOGE("set sharedSigalgs is false");
    }
    if (!GetRemoteCertificateFromPeer()) {
        NETSTACK_LOGE("get remote certificate is false");
    }
    if (!peerX509_) {
        NETSTACK_LOGE("peer x509Certificates is null");
        return false;
    }
    CheckServerIdentity checkServerIdentity = options.GetCheckServerIdentity();
    if (!checkServerIdentity) {
        CheckServerIdentityLegal(hostName_, peerX509_);
    } else {
        checkServerIdentity(hostName_, {remoteCert_});
    }
    NETSTACK_LOGI("SSL Get Version: %{public}s, SSL Get Cipher: %{public}s", SSL_get_version(ssl_),
                  SSL_get_cipher(ssl_));
    return true;
}

bool TLSSocket::TLSSocketInternal::GetRemoteCertificateFromPeer()
{
    peerX509_ = SSL_get_peer_certificate(ssl_);
    if (peerX509_ == nullptr) {
        int resErr = ConvertSSLError(GetSSL());
        NETSTACK_LOGE("open fail errno, errno is %{public}d, error info is %{public}s", resErr,
                      MakeSSLErrorString(resErr).c_str());
        return false;
    }
    BIO *bio = BIO_new(BIO_s_mem());
    X509_print(bio, peerX509_);
    if (!bio) {
        NETSTACK_LOGE("TlsSocket::SetRemoteCertificate bio is null");
        return false;
    }
    char data[REMOTE_CERT_LEN];
    if (!BIO_read(bio, data, REMOTE_CERT_LEN)) {
        NETSTACK_LOGE("BIO_read function returns error");
        BIO_free(bio);
        return false;
    }
    BIO_free(bio);
    remoteCert_ = std::string(data);
    NETSTACK_LOGI("Remote certificate content is %{public}s", remoteCert_.c_str());
    return true;
}

ssl_st *TLSSocket::TLSSocketInternal::GetSSL() const
{
    return ssl_;
}
} // namespace NetStack
} // namespace OHOS