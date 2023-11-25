/*
 * Copyright (c) 2021-2023 Huawei Device Co., Ltd.
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

#include "socket_module.h"

#include <cstdint>
#include <initializer_list>
#include <new>
#include <unistd.h>
#include <utility>

#include "bind_context.h"
#include "common_context.h"
#include "connect_context.h"
#include "context_key.h"
#include "event_list.h"
#include "event_manager.h"
#include "module_template.h"
#include "multicast_get_loopback_context.h"
#include "multicast_get_ttl_context.h"
#include "multicast_membership_context.h"
#include "multicast_set_loopback_context.h"
#include "multicast_set_ttl_context.h"
#include "napi/native_api.h"
#include "napi/native_common.h"
#include "napi_utils.h"
#include "net_address.h"
#include "netstack_common_utils.h"
#include "netstack_log.h"
#include "node_api.h"
#include "socket_async_work.h"
#include "socket_exec.h"
#include "tcp_extra_context.h"
#include "tcp_send_context.h"
#include "tcp_server_common_context.h"
#include "tcp_server_extra_context.h"
#include "tcp_server_listen_context.h"
#include "tcp_server_send_context.h"
#include "tlssocket_module.h"
#if !defined(CROSS_PLATFORM)
#include "tlssocketserver_module.h"
#endif
#include "udp_extra_context.h"
#include "udp_send_context.h"

static constexpr const char *SOCKET_MODULE_NAME = "net.socket";

static const char *UDP_BIND_NAME = "UdpBind";
static const char *UDP_SEND_NAME = "UdpSend";
static const char *UDP_CLOSE_NAME = "UdpClose";
static const char *UDP_GET_STATE = "UdpGetState";
static const char *UDP_SET_EXTRA_OPTIONS_NAME = "UdpSetExtraOptions";
static constexpr const char *UDP_GET_SOCKET_FD = "UdpGetSocketFd";

static constexpr const char *UDP_ADD_MEMBERSHIP = "UdpAddMembership";
static constexpr const char *UDP_DROP_MEMBERSHIP = "UdpDropMembership";
static constexpr const char *UDP_SET_MULTICAST_TTL = "UdpSetMulticastTTL";
static constexpr const char *UDP_GET_MULTICAST_TTL = "UdpGetMulticastTTL";
static constexpr const char *UDP_SET_LOOPBACK_MODE = "UdpSetLoopbackMode";
static constexpr const char *UDP_GET_LOOPBACK_MODE = "UdpGetLoopbackMode";

static const char *TCP_BIND_NAME = "TcpBind";
static const char *TCP_CONNECT_NAME = "TcpConnect";
static const char *TCP_SEND_NAME = "TcpSend";
static const char *TCP_CLOSE_NAME = "TcpClose";
static const char *TCP_GET_STATE = "TcpGetState";
static const char *TCP_GET_REMOTE_ADDRESS = "TcpGetRemoteAddress";
static const char *TCP_SET_EXTRA_OPTIONS_NAME = "TcpSetExtraOptions";
static constexpr const char *TCP_GET_SOCKET_FD = "TcpGetSocketFd";

static constexpr const char *TCP_SERVER_LISTEN_NAME = "TcpServerListen";
static constexpr const char *TCP_SERVER_GET_STATE = "TcpServerGetState";
static constexpr const char *TCP_SERVER_SET_EXTRA_OPTIONS_NAME = "TcpServerSetExtraOptions";

static constexpr const char *TCP_CONNECTION_SEND_NAME = "TcpConnectionSend";
static constexpr const char *TCP_CONNECTION_CLOSE_NAME = "TcpConnectionClose";
static constexpr const char *TCP_CONNECTION_GET_REMOTE_ADDRESS = "TcpConnectionGetRemoteAddress";

static constexpr const char *KEY_SOCKET_FD = "socketFd";

#define SOCKET_INTERFACE(Context, executor, callback, work, name) \
    ModuleTemplate::Interface<Context>(env, info, name, work, SocketAsyncWork::executor, SocketAsyncWork::callback)

namespace OHOS::NetStack::Socket {
void Finalize(napi_env, void *data, void *)
{
    NETSTACK_LOGI("socket handle is finalized");
    auto manager = static_cast<EventManager *>(data);
    if (manager != nullptr) {
        int sock = static_cast<int>(reinterpret_cast<uint64_t>(manager->GetData()));
        if (sock != 0) {
            SocketExec::SingletonSocketConfig::GetInstance().RemoveServerSocket(sock);
            close(sock);
        }
        EventManager::SetInvalid(manager);
    }
}

static bool SetSocket(napi_env env, napi_value thisVal, BaseContext *context, int sock)
{
    if (sock < 0) {
        napi_value error = NapiUtils::CreateObject(env);
        if (NapiUtils::GetValueType(env, error) != napi_object) {
            return false;
        }
        NapiUtils::SetUint32Property(env, error, KEY_ERROR_CODE, errno);
        context->Emit(EVENT_ERROR, std::make_pair(NapiUtils::GetUndefined(env), error));
        return false;
    }

    EventManager *manager = nullptr;
    if (napi_unwrap(env, thisVal, reinterpret_cast<void **>(&manager)) != napi_ok || manager == nullptr) {
        return false;
    }

    manager->SetData(reinterpret_cast<void *>(sock));
    NapiUtils::SetInt32Property(env, thisVal, KEY_SOCKET_FD, sock);
    return true;
}

static bool MakeTcpClientBindSocket(napi_env env, napi_value thisVal, BindContext *context)
{
    if (!context->IsParseOK()) {
        context->SetErrorCode(PARSE_ERROR_CODE);
        return false;
    }
    if (!CommonUtils::HasInternetPermission()) {
        context->SetPermissionDenied(true);
        return false;
    }
    NETSTACK_LOGD("bind ip family is %{public}d", context->address_.GetSaFamily());
    if (context->GetManager()->GetData() != nullptr) {
        NETSTACK_LOGE("tcp connect has been called");
        return true;
    }
    int sock = SocketExec::MakeTcpSocket(context->address_.GetSaFamily());
    if (!SetSocket(env, thisVal, context, sock)) {
        return false;
    }
    context->SetExecOK(true);
    return true;
}

static bool MakeTcpClientConnectSocket(napi_env env, napi_value thisVal, ConnectContext *context)
{
    if (!context->IsParseOK()) {
        context->SetErrorCode(PARSE_ERROR_CODE);
        return false;
    }
    if (!CommonUtils::HasInternetPermission()) {
        context->SetPermissionDenied(true);
        return false;
    }
    NETSTACK_LOGD("connect ip family is %{public}d", context->options.address.GetSaFamily());
    if (context->GetManager()->GetData() != nullptr) {
        NETSTACK_LOGD("tcp bind has been called");
        return true;
    }
    int sock = SocketExec::MakeTcpSocket(context->options.address.GetSaFamily());
    if (!SetSocket(env, thisVal, context, sock)) {
        return false;
    }
    context->SetExecOK(true);
    return true;
}

static bool MakeTcpServerSocket(napi_env env, napi_value thisVal, TcpServerListenContext *context)
{
    if (!context->IsParseOK()) {
        context->SetErrorCode(PARSE_ERROR_CODE);
        return false;
    }
    if (!CommonUtils::HasInternetPermission()) {
        context->SetPermissionDenied(true);
        return false;
    }
    int sock = SocketExec::MakeTcpSocket(context->address_.GetSaFamily());
    if (!SetSocket(env, thisVal, context, sock)) {
        return false;
    }
    context->SetExecOK(true);
    return true;
}

static bool MakeUdpSocket(napi_env env, napi_value thisVal, BindContext *context)
{
    if (!context->IsParseOK()) {
        context->SetErrorCode(PARSE_ERROR_CODE);
        return false;
    }
    if (!CommonUtils::HasInternetPermission()) {
        context->SetPermissionDenied(true);
        return false;
    }
    int sock = SocketExec::MakeUdpSocket(context->address_.GetSaFamily());
    if (!SetSocket(env, thisVal, context, sock)) {
        return false;
    }
    context->SetExecOK(true);
    return true;
}

static bool MakeMulticastUdpSocket(napi_env env, napi_value thisVal, MulticastMembershipContext *context)
{
    if (!CommonUtils::HasInternetPermission()) {
        context->SetPermissionDenied(true);
        return false;
    }
    if (context->GetSocketFd() > 0) {
        NETSTACK_LOGI("socket exist: %{public}d", context->GetSocketFd());
        return false;
    }
    if (!context->IsParseOK()) {
        context->SetErrorCode(PARSE_ERROR_CODE);
        return false;
    }
    int sock = SocketExec::MakeUdpSocket(context->address_.GetSaFamily());
    if (!SetSocket(env, thisVal, context, sock)) {
        return false;
    }
    context->SetExecOK(true);
    return true;
}

napi_value SocketModuleExports::InitSocketModule(napi_env env, napi_value exports)
{
    TlsSocket::TLSSocketModuleExports::InitTLSSocketModule(env, exports);
#if !defined(CROSS_PLATFORM)
    TlsSocketServer::TLSSocketServerModuleExports::InitTLSSocketServerModule(env, exports);
#endif
    DefineUDPSocketClass(env, exports);
    DefineMulticastSocketClass(env, exports);
    DefineTCPServerSocketClass(env, exports);
    DefineTCPSocketClass(env, exports);
    InitSocketProperties(env, exports);

    return exports;
}

napi_value SocketModuleExports::ConstructUDPSocketInstance(napi_env env, napi_callback_info info)
{
    return ModuleTemplate::NewInstance(env, info, INTERFACE_UDP_SOCKET, Finalize);
}

napi_value SocketModuleExports::ConstructMulticastSocketInstance(napi_env env, napi_callback_info info)
{
    return ModuleTemplate::NewInstance(env, info, INTERFACE_MULTICAST_SOCKET, Finalize);
}

void SocketModuleExports::DefineUDPSocketClass(napi_env env, napi_value exports)
{
    std::initializer_list<napi_property_descriptor> properties = {
        DECLARE_NAPI_FUNCTION(UDPSocket::FUNCTION_BIND, UDPSocket::Bind),
        DECLARE_NAPI_FUNCTION(UDPSocket::FUNCTION_SEND, UDPSocket::Send),
        DECLARE_NAPI_FUNCTION(UDPSocket::FUNCTION_CLOSE, UDPSocket::Close),
        DECLARE_NAPI_FUNCTION(UDPSocket::FUNCTION_GET_STATE, UDPSocket::GetState),
        DECLARE_NAPI_FUNCTION(UDPSocket::FUNCTION_SET_EXTRA_OPTIONS, UDPSocket::SetExtraOptions),
        DECLARE_NAPI_FUNCTION(UDPSocket::FUNCTION_GET_SOCKET_FD, UDPSocket::GetSocketFd),
        DECLARE_NAPI_FUNCTION(UDPSocket::FUNCTION_ON, UDPSocket::On),
        DECLARE_NAPI_FUNCTION(UDPSocket::FUNCTION_OFF, UDPSocket::Off),
    };
    ModuleTemplate::DefineClass(env, exports, properties, INTERFACE_UDP_SOCKET);
}

void SocketModuleExports::DefineMulticastSocketClass(napi_env env, napi_value exports)
{
    std::initializer_list<napi_property_descriptor> properties = {
        DECLARE_NAPI_FUNCTION(MulticastSocket::FUNCTION_BIND, MulticastSocket::Bind),
        DECLARE_NAPI_FUNCTION(MulticastSocket::FUNCTION_SEND, MulticastSocket::Send),
        DECLARE_NAPI_FUNCTION(MulticastSocket::FUNCTION_CLOSE, MulticastSocket::Close),
        DECLARE_NAPI_FUNCTION(MulticastSocket::FUNCTION_GET_STATE, MulticastSocket::GetState),
        DECLARE_NAPI_FUNCTION(MulticastSocket::FUNCTION_SET_EXTRA_OPTIONS, MulticastSocket::SetExtraOptions),
        DECLARE_NAPI_FUNCTION(MulticastSocket::FUNCTION_GET_SOCKET_FD, MulticastSocket::GetSocketFd),
        DECLARE_NAPI_FUNCTION(MulticastSocket::FUNCTION_ON, MulticastSocket::On),
        DECLARE_NAPI_FUNCTION(MulticastSocket::FUNCTION_OFF, MulticastSocket::Off),
        DECLARE_NAPI_FUNCTION(MulticastSocket::FUNCTION_ADD_MEMBER_SHIP, MulticastSocket::AddMembership),
        DECLARE_NAPI_FUNCTION(MulticastSocket::FUNCTION_DROP_MEMBER_SHIP, MulticastSocket::DropMembership),
        DECLARE_NAPI_FUNCTION(MulticastSocket::FUNCTION_SET_MULTICAST_TTL, MulticastSocket::SetMulticastTTL),
        DECLARE_NAPI_FUNCTION(MulticastSocket::FUNCTION_GET_MULTICAST_TTL, MulticastSocket::GetMulticastTTL),
        DECLARE_NAPI_FUNCTION(MulticastSocket::FUNCTION_SET_LOOPBACK_MODE, MulticastSocket::SetLoopbackMode),
        DECLARE_NAPI_FUNCTION(MulticastSocket::FUNCTION_GET_LOOPBACK_MODE, MulticastSocket::GetLoopbackMode),
    };
    ModuleTemplate::DefineClass(env, exports, properties, INTERFACE_MULTICAST_SOCKET);
}

napi_value SocketModuleExports::ConstructTCPSocketInstance(napi_env env, napi_callback_info info)
{
    return ModuleTemplate::NewInstance(env, info, INTERFACE_TCP_SOCKET, Finalize);
}

void SocketModuleExports::DefineTCPSocketClass(napi_env env, napi_value exports)
{
    std::initializer_list<napi_property_descriptor> properties = {
        DECLARE_NAPI_FUNCTION(TCPSocket::FUNCTION_BIND, TCPSocket::Bind),
        DECLARE_NAPI_FUNCTION(TCPSocket::FUNCTION_CONNECT, TCPSocket::Connect),
        DECLARE_NAPI_FUNCTION(TCPSocket::FUNCTION_SEND, TCPSocket::Send),
        DECLARE_NAPI_FUNCTION(TCPSocket::FUNCTION_CLOSE, TCPSocket::Close),
        DECLARE_NAPI_FUNCTION(TCPSocket::FUNCTION_GET_REMOTE_ADDRESS, TCPSocket::GetRemoteAddress),
        DECLARE_NAPI_FUNCTION(TCPSocket::FUNCTION_GET_STATE, TCPSocket::GetState),
        DECLARE_NAPI_FUNCTION(TCPSocket::FUNCTION_SET_EXTRA_OPTIONS, TCPSocket::SetExtraOptions),
        DECLARE_NAPI_FUNCTION(TCPSocket::FUNCTION_GET_SOCKET_FD, TCPSocket::GetSocketFd),
        DECLARE_NAPI_FUNCTION(TCPSocket::FUNCTION_ON, TCPSocket::On),
        DECLARE_NAPI_FUNCTION(TCPSocket::FUNCTION_OFF, TCPSocket::Off),
    };
    ModuleTemplate::DefineClass(env, exports, properties, INTERFACE_TCP_SOCKET);
}

napi_value SocketModuleExports::ConstructTCPSocketServerInstance(napi_env env, napi_callback_info info)
{
    return ModuleTemplate::NewInstance(env, info, INTERFACE_TCP_SOCKET_SERVER, Finalize);
}

void SocketModuleExports::DefineTCPServerSocketClass(napi_env env, napi_value exports)
{
    std::initializer_list<napi_property_descriptor> properties = {
        DECLARE_NAPI_FUNCTION(TCPServerSocket::FUNCTION_LISTEN, TCPServerSocket::Listen),
        DECLARE_NAPI_FUNCTION(TCPServerSocket::FUNCTION_GET_STATE, TCPServerSocket::GetState),
        DECLARE_NAPI_FUNCTION(TCPServerSocket::FUNCTION_SET_EXTRA_OPTIONS, TCPServerSocket::SetExtraOptions),
        DECLARE_NAPI_FUNCTION(TCPServerSocket::FUNCTION_ON, TCPServerSocket::On),
        DECLARE_NAPI_FUNCTION(TCPServerSocket::FUNCTION_OFF, TCPServerSocket::Off),
    };
    ModuleTemplate::DefineClass(env, exports, properties, INTERFACE_TCP_SOCKET_SERVER);
}

void SocketModuleExports::InitSocketProperties(napi_env env, napi_value exports)
{
    std::initializer_list<napi_property_descriptor> properties = {
        DECLARE_NAPI_FUNCTION(FUNCTION_CONSTRUCTOR_UDP_SOCKET_INSTANCE, ConstructUDPSocketInstance),
        DECLARE_NAPI_FUNCTION(FUNCTION_CONSTRUCTOR_MULTICAST_SOCKET_INSTANCE, ConstructMulticastSocketInstance),
        DECLARE_NAPI_FUNCTION(FUNCTION_CONSTRUCTOR_TCP_SOCKET_SERVER_INSTANCE, ConstructTCPSocketServerInstance),
        DECLARE_NAPI_FUNCTION(FUNCTION_CONSTRUCTOR_TCP_SOCKET_INSTANCE, ConstructTCPSocketInstance),
    };
    NapiUtils::DefineProperties(env, exports, properties);
}

/* udp async works */
napi_value SocketModuleExports::UDPSocket::Bind(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(BindContext, ExecUdpBind, BindCallback, MakeUdpSocket, UDP_BIND_NAME);
}

napi_value SocketModuleExports::UDPSocket::Send(napi_env env, napi_callback_info info)
{
    return ModuleTemplate::InterfaceWithOutAsyncWork<UdpSendContext>(
        env, info,
        [](napi_env, napi_value, UdpSendContext *context) -> bool {
#ifdef ENABLE_EVENT_HANDLER
            auto manager = context->GetManager();
            if (!manager->InitNetstackEventHandler()) {
                return false;
            }
#endif
            SocketAsyncWork::ExecUdpSend(context->GetEnv(), context);
            return true;
        },
        UDP_SEND_NAME, SocketAsyncWork::ExecUdpSend, SocketAsyncWork::UdpSendCallback);
}

napi_value SocketModuleExports::UDPSocket::Close(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(CloseContext, ExecClose, CloseCallback, nullptr, UDP_CLOSE_NAME);
}

napi_value SocketModuleExports::UDPSocket::GetState(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(GetStateContext, ExecGetState, GetStateCallback, nullptr, UDP_GET_STATE);
}

napi_value SocketModuleExports::UDPSocket::SetExtraOptions(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(UdpSetExtraOptionsContext, ExecUdpSetExtraOptions, UdpSetExtraOptionsCallback, nullptr,
                            UDP_SET_EXTRA_OPTIONS_NAME);
}

napi_value SocketModuleExports::UDPSocket::GetSocketFd(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(GetSocketFdContext, ExecUdpGetSocketFd, UdpGetSocketFdCallback, nullptr, UDP_GET_SOCKET_FD);
}

napi_value SocketModuleExports::UDPSocket::On(napi_env env, napi_callback_info info)
{
    return ModuleTemplate::On(env, info, {EVENT_MESSAGE, EVENT_LISTENING, EVENT_ERROR, EVENT_CLOSE}, false);
}

napi_value SocketModuleExports::UDPSocket::Off(napi_env env, napi_callback_info info)
{
    return ModuleTemplate::Off(env, info, {EVENT_MESSAGE, EVENT_LISTENING, EVENT_ERROR, EVENT_CLOSE});
}

/* udp multicast */
napi_value SocketModuleExports::MulticastSocket::AddMembership(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(MulticastMembershipContext, ExecUdpAddMembership, UdpAddMembershipCallback,
                            MakeMulticastUdpSocket, UDP_ADD_MEMBERSHIP);
}

napi_value SocketModuleExports::MulticastSocket::DropMembership(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(MulticastMembershipContext, ExecUdpDropMembership, UdpDropMembershipCallback,
                            nullptr, UDP_DROP_MEMBERSHIP);
}

napi_value SocketModuleExports::MulticastSocket::SetMulticastTTL(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(MulticastSetTTLContext, ExecSetMulticastTTL, UdpSetMulticastTTLCallback, nullptr,
                            UDP_SET_MULTICAST_TTL);
}

napi_value SocketModuleExports::MulticastSocket::GetMulticastTTL(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(MulticastGetTTLContext, ExecGetMulticastTTL, UdpGetMulticastTTLCallback, nullptr,
                            UDP_GET_MULTICAST_TTL);
}

napi_value SocketModuleExports::MulticastSocket::SetLoopbackMode(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(MulticastSetLoopbackContext, ExecSetLoopbackMode, UdpSetLoopbackModeCallback, nullptr,
                            UDP_SET_LOOPBACK_MODE);
}

napi_value SocketModuleExports::MulticastSocket::GetLoopbackMode(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(MulticastGetLoopbackContext, ExecGetLoopbackMode, UdpGetLoopbackModeCallback, nullptr,
                            UDP_GET_LOOPBACK_MODE);
}

/* tcp async works */
napi_value SocketModuleExports::TCPSocket::Bind(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(BindContext, ExecTcpBind, BindCallback, MakeTcpClientBindSocket, TCP_BIND_NAME);
}

napi_value SocketModuleExports::TCPSocket::Connect(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(ConnectContext, ExecConnect, ConnectCallback, MakeTcpClientConnectSocket, TCP_CONNECT_NAME);
}

napi_value SocketModuleExports::TCPSocket::Send(napi_env env, napi_callback_info info)
{
    return ModuleTemplate::InterfaceWithOutAsyncWork<TcpSendContext>(
        env, info,
        [](napi_env, napi_value, TcpSendContext *context) -> bool {
#ifdef ENABLE_EVENT_HANDLER
            auto manager = context->GetManager();
            if (!manager->InitNetstackEventHandler()) {
                return false;
            }
#endif
            SocketAsyncWork::ExecTcpSend(context->GetEnv(), context);
            return true;
        },
        TCP_SEND_NAME, SocketAsyncWork::ExecTcpSend, SocketAsyncWork::TcpSendCallback);
}

napi_value SocketModuleExports::TCPSocket::Close(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(CloseContext, ExecClose, CloseCallback, nullptr, TCP_CLOSE_NAME);
}

napi_value SocketModuleExports::TCPSocket::GetRemoteAddress(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(GetRemoteAddressContext, ExecGetRemoteAddress, GetRemoteAddressCallback, nullptr,
                            TCP_GET_REMOTE_ADDRESS);
}

napi_value SocketModuleExports::TCPSocket::GetState(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(GetStateContext, ExecGetState, GetStateCallback, nullptr, TCP_GET_STATE);
}

napi_value SocketModuleExports::TCPSocket::SetExtraOptions(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(TcpSetExtraOptionsContext, ExecTcpSetExtraOptions, TcpSetExtraOptionsCallback, nullptr,
                            TCP_SET_EXTRA_OPTIONS_NAME);
}

napi_value SocketModuleExports::TCPSocket::GetSocketFd(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(GetSocketFdContext, ExecTcpGetSocketFd, TcpGetSocketFdCallback, nullptr, TCP_GET_SOCKET_FD);
}

napi_value SocketModuleExports::TCPSocket::On(napi_env env, napi_callback_info info)
{
    return ModuleTemplate::On(env, info, {EVENT_MESSAGE, EVENT_CONNECT, EVENT_ERROR, EVENT_CLOSE}, false);
}

napi_value SocketModuleExports::TCPSocket::Off(napi_env env, napi_callback_info info)
{
    return ModuleTemplate::Off(env, info, {EVENT_MESSAGE, EVENT_CONNECT, EVENT_ERROR, EVENT_CLOSE});
}

/* tcp connection async works */
napi_value SocketModuleExports::TCPConnection::Send(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(
        TcpServerSendContext, ExecTcpConnectionSend, TcpConnectionSendCallback,
        [](napi_env theEnv, napi_value thisVal, TcpServerSendContext *context) -> bool {
            context->clientId_ = NapiUtils::GetInt32Property(theEnv, thisVal, PROPERTY_CLIENT_ID);
            return true;
        },
        TCP_CONNECTION_SEND_NAME);
}

napi_value SocketModuleExports::TCPConnection::Close(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(
        TcpServerCloseContext, ExecTcpConnectionClose, TcpConnectionCloseCallback,
        [](napi_env theEnv, napi_value thisVal, TcpServerCloseContext *context) -> bool {
            context->clientId_ = NapiUtils::GetInt32Property(theEnv, thisVal, PROPERTY_CLIENT_ID);
            return true;
        },
        TCP_CONNECTION_CLOSE_NAME);
}

napi_value SocketModuleExports::TCPConnection::GetRemoteAddress(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(
        TcpServerGetRemoteAddressContext, ExecTcpConnectionGetRemoteAddress, TcpConnectionGetRemoteAddressCallback,
        [](napi_env theEnv, napi_value thisVal, TcpServerGetRemoteAddressContext *context) -> bool {
            context->clientId_ = NapiUtils::GetInt32Property(theEnv, thisVal, PROPERTY_CLIENT_ID);
            return true;
        },
        TCP_CONNECTION_GET_REMOTE_ADDRESS);
}

napi_value SocketModuleExports::TCPConnection::On(napi_env env, napi_callback_info info)
{
    napi_value ret = ModuleTemplate::On(env, info, {EVENT_MESSAGE, EVENT_CONNECT, EVENT_ERROR, EVENT_CLOSE}, false);
    SocketExec::NotifyRegisterEvent();
    return ret;
}

napi_value SocketModuleExports::TCPConnection::Off(napi_env env, napi_callback_info info)
{
    return ModuleTemplate::Off(env, info, {EVENT_MESSAGE, EVENT_CONNECT, EVENT_ERROR, EVENT_CLOSE});
}

/* tcp server async works */
napi_value SocketModuleExports::TCPServerSocket::Listen(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(TcpServerListenContext, ExecTcpServerListen, ListenCallback, MakeTcpServerSocket,
                            TCP_SERVER_LISTEN_NAME);
}

napi_value SocketModuleExports::TCPServerSocket::GetState(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(TcpServerGetStateContext, ExecTcpServerGetState, TcpServerGetStateCallback, nullptr,
                            TCP_SERVER_GET_STATE);
}

napi_value SocketModuleExports::TCPServerSocket::SetExtraOptions(napi_env env, napi_callback_info info)
{
    return SOCKET_INTERFACE(TcpServerSetExtraOptionsContext, ExecTcpServerSetExtraOptions,
                            TcpServerSetExtraOptionsCallback, nullptr, TCP_SERVER_SET_EXTRA_OPTIONS_NAME);
}

napi_value SocketModuleExports::TCPServerSocket::On(napi_env env, napi_callback_info info)
{
    return ModuleTemplate::On(env, info, {EVENT_MESSAGE, EVENT_CONNECT, EVENT_ERROR, EVENT_CLOSE}, false);
}

napi_value SocketModuleExports::TCPServerSocket::Off(napi_env env, napi_callback_info info)
{
    return ModuleTemplate::Off(env, info, {EVENT_MESSAGE, EVENT_CONNECT, EVENT_ERROR, EVENT_CLOSE});
}

static napi_module g_socketModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = SocketModuleExports::InitSocketModule,
    .nm_modname = SOCKET_MODULE_NAME,
    .nm_priv = nullptr,
    .reserved = {nullptr},
};
/*
 * Module register function
 */
extern "C" __attribute__((constructor)) void RegisterSocketModule(void)
{
    napi_module_register(&g_socketModule);
}
} // namespace OHOS::NetStack::Socket
