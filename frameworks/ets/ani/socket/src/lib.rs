// Copyright (C) 2026 Huawei Device Co., Ltd.
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

const LOG_LABEL: hilog_rust::HiLogLabel = hilog_rust::HiLogLabel {
    log_type: hilog_rust::LogType::LogCore,
    domain: 0xD0015B0,
    tag: "SocketAni",
};

#[macro_use]
extern crate netstack_common;

mod bridge;
mod error;
mod wrapper;
mod udp_socket;
mod multicast_socket;
mod local_socket;
mod local_socket_connection;
mod local_socket_server;
mod tcp_socket;
mod tcp_socket_connection;
mod tcp_socket_server;
mod tls_client;
mod tls_connect;
mod tls_server;


ani_rs::ani_constructor! {
    namespace "@ohos.net.socket.socket"
    [
        "constructUDPSocketInstance" : udp_socket::construct_udp_socket_instance,
        "constructMulticastSocketInstance" : multicast_socket::construct_multicast_socket_instance,
        "constructLocalSocketInstance" : local_socket::construct_local_socket_instance,
        "constructLocalSocketServerInstance" : local_socket_server::construct_local_socket_server_instance,
        "constructTCPSocketInstance" : tcp_socket::construct_tcp_socket_instance,
        "constructTCPSocketServerInstance" : tcp_socket_server::construct_tcp_socket_server_instance,
        "constructTLSSocketInstanceSync" : tls_client::construct_tls_client_instance,        
        "constructTLSSocketServerInstance" : tls_server::construct_tls_server_instance,

        "destroyUDPSocketInstance" : udp_socket::destroy_udp_socket_instance,
        "destroyLocalSocketInstance" : local_socket::destroy_local_socket_instance,
        "destroyLocalSocketServerInstance" : local_socket_server::destroy_local_socket_server_instance,
        "destroyLocalSocketConnectionInstance" : local_socket_connection::destroy_local_socket_connection_instance,
        "destroyTCPSocketInstance" : tcp_socket::destroy_tcp_socket_instance,
        "destroyTCPSocketServerInstance" : tcp_socket_server::destroy_tcp_socket_server_instance,
        "destroyTCPSocketConnectionInstance" : tcp_socket_connection::destroy_tcp_socket_connection_instance,
        "destroyTLSSocketInstance" : tls_client::destroy_tls_client_instance,
        "destroyTLSSocketServerInstance" : tls_server::destroy_tls_server_instance,
        "destroyTLSSocketConnectionInstance" : tls_connect::destroy_tls_connect_instance,
    ]
    class "@ohos.net.socket.socket.UDPSocketInner"
    [
        "bindSync" : udp_socket::bind_sync,
        "getLocalAddressSync" : udp_socket::get_local_address_sync,
        "sendSync" : udp_socket::send_sync,
        "closeSync" : udp_socket::close_sync,
        "getStateSync" : udp_socket::get_state_sync,
        "setExtraOptionsSync" : udp_socket::set_extra_options_sync,
        "getSocketFdSync" : udp_socket::get_socket_fd_sync,
        "onMessageInner" : udp_socket::on_message_inner,
        "offMessageInner" : udp_socket::off_message_inner,
        "onListeningInner" : udp_socket::on_listening_inner,
        "offListeningInner" : udp_socket::off_listening_inner,
        "onCloseInner" : udp_socket::on_close_inner,
        "offCloseInner" : udp_socket::off_close_inner,
        "onErrorInner" : udp_socket::on_error_inner,
        "offErrorInner" : udp_socket::off_error_inner,
    ]
    class "@ohos.net.socket.socket.MulticastSocketInner"
    [
        "addMembershipSync" : multicast_socket::add_membership_sync,
        "dropMembershipSync" : multicast_socket::drop_membership_sync,
        "setLoopbackModeSync" : multicast_socket::set_loopback_mode_sync,
        "getLoopbackModeSync" : multicast_socket::get_loopback_mode_sync,
        "setMulticastTTLSync" : multicast_socket::set_ttl_sync,
        "getMulticastTTLSync" : multicast_socket::get_ttl_sync,
        "setReuseAddressSync" : multicast_socket::set_reuse_address_sync,
    ]
    class "@ohos.net.socket.socket.LocalSocketInner"
    [
        "bindSync" : local_socket::bind_sync,
        "connectSync" : local_socket::connect_sync,
        "sendSync" : local_socket::send_sync,
        "closeSync" : local_socket::close_sync,
        "getStateSync" : local_socket::get_state_sync,
        "getSocketFdSync" : local_socket::get_socket_fd_sync,
        "setExtraOptionsSync" : local_socket::set_extra_options_sync,
        "getExtraOptionsSync" : local_socket::get_extra_options_sync,
        "getLocalAddressSync" : local_socket::get_local_address_sync,
        "onMessageInner" : local_socket::on_message_inner,
        "offMessageInner" : local_socket::off_message_inner,
        "onConnectInner" : local_socket::on_connect_inner,
        "offConnectInner" : local_socket::off_connect_inner,
        "onCloseInner" : local_socket::on_close_inner,
        "offCloseInner" : local_socket::off_close_inner,
        "onErrorInner" : local_socket::on_error_inner,
        "offErrorInner" : local_socket::off_error_inner,
    ]
    class "@ohos.net.socket.socket.LocalSocketConnectionInner"
    [
        "sendSync" : local_socket_connection::send_sync,
        "closeSync" : local_socket_connection::close_sync,
        "getLocalAddressSync" : local_socket_connection::get_local_address_sync,
        "onMessageInner" : local_socket_connection::on_message_inner,
        "offMessageInner" : local_socket_connection::off_message_inner,
        "onCloseInner" : local_socket_connection::on_close_inner,
        "offCloseInner" : local_socket_connection::off_close_inner,
        "onErrorInner" : local_socket_connection::on_error_inner,
        "offErrorInner" : local_socket_connection::off_error_inner,
        "getSocketFdSync" : local_socket_connection::get_socket_fd_sync,
    ]
    class "@ohos.net.socket.socket.LocalSocketServerInner"
    [
        "listenSync" : local_socket_server::listen_sync,
        "getStateSync" : local_socket_server::get_state_sync,
        "setExtraOptionsSync" : local_socket_server::set_extra_options_sync,
        "getExtraOptionsSync" : local_socket_server::get_extra_options_sync,
        "getLocalAddressSync" : local_socket_server::get_local_address_sync,
        "closeSync" : local_socket_server::close_sync,
        "onConnectInner" : local_socket_server::on_connect_inner,
        "offConnectInner" : local_socket_server::off_connect_inner,
        "onErrorInner" : local_socket_server::on_error_inner,
        "offErrorInner" : local_socket_server::off_error_inner,
        "getSocketFdSync" : local_socket_server::get_socket_fd_sync,
    ]
    class "@ohos.net.socket.socket.TCPSocketInner"
    [
        "bindSync" : tcp_socket::bind_sync,
        "connectSync" : tcp_socket::connect_sync,
        "sendSync" : tcp_socket::send_sync,
        "closeSync" : tcp_socket::close_sync,
        "getRemoteAddressSync" : tcp_socket::get_remote_address_sync,
        "getLocalAddressSync" : tcp_socket::get_local_address_sync,
        "getStateSync" : tcp_socket::get_state_sync,
        "setExtraOptionsSync" : tcp_socket::set_extra_options_sync,
        "getSocketFdSync" : tcp_socket::get_socket_fd_sync,
        "onMessageInner" : tcp_socket::on_message_inner,
        "offMessageInner" : tcp_socket::off_message_inner,
        "onConnectInner" : tcp_socket::on_connect_inner,
        "offConnectInner" : tcp_socket::off_connect_inner,
        "onCloseInner" : tcp_socket::on_close_inner,
        "offCloseInner" : tcp_socket::off_close_inner,
        "onErrorInner" : tcp_socket::on_error_inner,
        "offErrorInner" : tcp_socket::off_error_inner,
    ]
    class "@ohos.net.socket.socket.TCPSocketConnectionInner"
    [
        "sendSync" : tcp_socket_connection::send_sync,
        "closeSync" : tcp_socket_connection::close_sync,
        "getRemoteAddressSync" : tcp_socket_connection::get_remote_address_sync,
        "getLocalAddressSync" : tcp_socket_connection::get_local_address_sync,
        "getSocketFdSync" : tcp_socket_connection::get_socket_fd_sync,
        "onMessageInner" : tcp_socket_connection::on_message_inner,
        "offMessageInner" : tcp_socket_connection::off_message_inner,
        "onCloseInner" : tcp_socket_connection::on_close_inner,
        "offCloseInner" : tcp_socket_connection::off_close_inner,
        "onErrorInner" : tcp_socket_connection::on_error_inner,
        "offErrorInner" : tcp_socket_connection::off_error_inner,
    ]
    class "@ohos.net.socket.socket.TCPSocketServerInner"
    [
        "listenSync" : tcp_socket_server::listen_sync,
        "getStateSync" : tcp_socket_server::get_state_sync,
        "getSocketFdSync" : tcp_socket_server::get_socket_fd_sync,
        "setExtraOptionsSync" : tcp_socket_server::set_extra_options_sync,
        "closeSync" : tcp_socket_server::close_sync,
        "getLocalAddressSync" : tcp_socket_server::get_local_address_sync,
        "onConnectInner" : tcp_socket_server::on_connect_inner,
        "offConnectInner" : tcp_socket_server::off_connect_inner,
        "onErrorInner" : tcp_socket_server::on_error_inner,
        "offErrorInner" : tcp_socket_server::off_error_inner,
    ]
    class "@ohos.net.socket.socket.TLSSocketInner"
    [
        "bindSync" : tls_client::bind_sync,
        "connectSync" : tls_client::connect_sync,
        "sendSync" : tls_client::send_sync,
        "closeSync" : tls_client::close_sync,
        "getRemoteAddressSync" : tls_client::get_remote_address_sync,
        "getLocalAddressSync" : tls_client::get_local_address_sync,
        "getStateSync" : tls_client::get_state_sync,
        "getCertificateSync" : tls_client::get_certificate_sync,
        "getRemoteCertificateSync" : tls_client::get_remote_certificate_sync,
        "getCipherSuiteSync" : tls_client::get_cipher_suite_sync,
        "getProtocolSync" : tls_client::get_protocol_sync,
        "getSocketFdSync" : tls_client::get_socket_fd_sync,
        "setExtraOptionsSync": tls_client::set_extra_options_sync,
        "getSignatureAlgorithmsSync": tls_client::get_signature_algorithms_sync,
        "onMessageInner" : tls_client::on_message_inner,
        "onConnectInner" : tls_client::on_connect_inner,
        "onCloseInner" : tls_client::on_close_inner,
        "onErrorInner" : tls_client::on_error_inner,
        "offMessageInner" : tls_client::off_message_inner,
        "offConnectInner" : tls_client::off_connect_inner,
        "offCloseInner" : tls_client::off_close_inner,
        "offErrorInner" : tls_client::off_error_inner,
    ]
    class "@ohos.net.socket.socket.TLSSocketConnectionInner"
    [
        "sendSync" : tls_connect::send_sync,
        "closeSync" : tls_connect::close_sync,
        "getRemoteAddressSync" : tls_connect::get_remote_address_sync,
        "getLocalAddressSync" : tls_connect::get_local_address_sync,
        "getRemoteCertificateSync" : tls_connect::get_remote_certificate_sync,
        "getCipherSuiteSync" : tls_connect::get_cipher_suite_sync,
        "getSignatureAlgorithmsSync" : tls_connect::get_signature_algorithms_sync,
        "getSocketFdSync" : tls_connect::get_socket_fd_sync,
        "onMessageInner" : tls_connect::on_message_inner,
        "offMessageInner" : tls_connect::off_message_inner,
        "onCloseInner" : tls_connect::on_close_inner,
        "offCloseInner" : tls_connect::off_close_inner,
        "onErrorInner" : tls_connect::on_error_inner,
        "offErrorInner" : tls_connect::off_error_inner,
    ]
    class "@ohos.net.socket.socket.TLSSocketServerInner"
    [
        "listenSync" : tls_server::listen_sync,
        "getStateSync" : tls_server::get_state_sync,
        "getSocketFdSync" : tls_server::get_socket_fd_sync,
        "closeSync" : tls_server::close_sync,
        "setExtraOptionsSync" : tls_server::set_extra_options_sync,
        "getCertificateSync" : tls_server::get_certificate_sync,
        "getProtocolSync" : tls_server::get_protocol_sync,
        "getLocalAddressSync" : tls_server::get_local_address_sync,
        "onConnectInner" : tls_server::on_connect_inner,
        "offConnectInner" : tls_server::off_connect_inner,
        "onErrorInner" : tls_server::on_error_inner,
        "offErrorInner" : tls_server::off_error_inner,
    ]
}

#[used]
#[link_section = ".init_array"]
static SOCKET_PANIC_HOOK: extern "C" fn() = {
    #[link_section = ".text.startup"]
    extern "C" fn init() {
        std::panic::set_hook(Box::new(|info| {
            error!("Panic occurred: {:?}", info);
        }));
    }
    init
};
