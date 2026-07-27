/*
 * Copyright (C) 2026 Huawei Device Co., Ltd.
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

#ifndef NET_FFI_CONVERT_H
#define NET_FFI_CONVERT_H

#include "socket_ani.h"
#include "wrapper.rs.h"
#include "socket_state_base.h"
#include "extra_options_base.h"
#include "tcp_connect_options.h"
#include "tcp_extra_options.h"
#include "tcp_send_options.h"
#include "udp_extra_options.h"
#include "net_address.h"
#include "proxy_options.h"
#include "socket_constant.h"
#include "socket_exec_error.h"
#include "udp_send_options.h"
#include "udp_socket_innerapi.h"
#include "local_socket_client_innerapi.h"
#include "local_socket_server_innerapi.h"
#include "tcp_socket_client_innerapi.h"
#include "tcp_socket_server_innerapi.h"
#include <string>

#include "socket_error.h"

namespace OHOS {
namespace NetStackAni {

std::vector<std::string> RustVecToStdVector(const rust::Vec<rust::String>& src);

NetStack::Socket::ProxyType FFIProxyTypeToProxyType(FFIProxyTypes ffiType);
std::string FFIProtocolToStdString(FFIProtocol proto);
FFIEncodingFormat EncodingFormatToFFIEncodingFormat(NetStack::TlsSocket::EncodingFormat fmt);
NetStack::TlsSocket::VerifyMode FFIBidirectionalAuthToVerifyMode(bool isBidirectional);

void FFINetAddressToNetAddress(const FFINetAddress& ffi, NetStack::Socket::NetAddress& addr);
void NetAddressToFFINetAddress(const NetStack::Socket::NetAddress& addr, FFINetAddress& ffi);
void FFITcpExtraOptionsToTCPExtraOptions(const FFITcpExtraOptions& ffi, NetStack::Socket::TCPExtraOptions& opts);
void FFITlsSecureOptionsToTLSSecureOptions(const FFITlsSecureOptions& ffi, NetStack::TlsSocket::TLSSecureOptions& opts);
void FFIProxyOptionsToProxyOptions(const FFIProxyOptions& ffi, NetStack::Socket::ProxyOptions& opts);
void ApplyAddressAndSecure(const FFITlsConnectOptions& ffi, NetStack::TlsSocket::TLSConnectOptions& opts);
void ApplyTlsProxyOption(const FFITlsConnectOptions& ffi, NetStack::TlsSocket::TLSConnectOptions& opts);
void ApplyTlsOptionalFields(const FFITlsConnectOptions& ffi, NetStack::TlsSocket::TLSConnectOptions& opts);
void FFITlsConnectOptionsToTLSConnectOptions(const FFITlsConnectOptions& ffi,
                                             NetStack::TlsSocket::TLSConnectOptions& opts);
void FFITcpSendOptionsToTCPSendOptions(const FFITcpSendOptions& ffi, NetStack::Socket::TCPSendOptions& opts);
void SocketStateBaseToFFISocketStateBase(const NetStack::Socket::SocketStateBase& state, FFISocketStateBase& ffi);
void X509CertRawDataToFFIX509CertRawData(const NetStack::TlsSocket::X509CertRawData& cert, FFIX509CertRawData& ffi);

} // namespace NetStackAni
} // namespace OHOS
#endif // NET_FFI_CONVERT_H