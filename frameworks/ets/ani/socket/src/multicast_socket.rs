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

use std::ffi::CStr;
use std::sync::Mutex;
use ani_rs::{
    business_error::BusinessError,
    objects::{AniFnObject, AniAsyncCallback, AniErrorCallback, AniRef},
    AniEnv,
};

use crate::{
    bridge,
    udp_socket::UdpSocketWrapper,
};

#[ani_rs::native]
pub fn construct_multicast_socket_instance<'local>(env: &AniEnv<'local>) -> Result<AniRef<'local>, BusinessError> {
    info!("Constructing MulticastClient instance");
    static MULTICAST_SOCKET_CLASS: &CStr = unsafe {
        CStr::from_bytes_with_nul_unchecked(b"@ohos.net.socket.socket.MulticastSocketInner\0")
    };
    static CTOR_SIGNATURE: &CStr = unsafe { CStr::from_bytes_with_nul_unchecked(b"l:\0") };
    let wrapper = UdpSocketWrapper::new(true);
    let ptr = Box::into_raw(Box::new(Mutex::new(wrapper))) as i64;
    let class = env.find_class(MULTICAST_SOCKET_CLASS).unwrap();
    let obj = env
        .new_object_with_signature(&class, CTOR_SIGNATURE, (ptr,))
        .unwrap();
    Ok(obj.into())
}

#[ani_rs::native]
pub(crate) fn bind_sync(this: bridge::MulticastSocketClient, address: bridge::NetAddress) -> Result<(), BusinessError> {
    info!("MulticastSocketClient bind_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.bind(&address).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_local_address_sync(this: bridge::MulticastSocketClient) -> Result<bridge::NetAddress, BusinessError> {
    info!("MulticastSocketClient get_local_address_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_local_address().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn send_sync(this: bridge::MulticastSocketClient, options: bridge::UDPSendOptions) -> Result<(), BusinessError> {
    info!("MulticastSocketClient send_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.send(&options).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn close_sync(this: bridge::MulticastSocketClient) -> Result<(), BusinessError> {
    info!("MulticastSocketClient close_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.close().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_state_sync(this: bridge::MulticastSocketClient) -> Result<bridge::SocketStateBase, BusinessError> {
    info!("MulticastSocketClient get_state_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_state().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn set_extra_options_sync(this: bridge::MulticastSocketClient, options: bridge::UDPExtraOptions) -> Result<(), BusinessError> {
    info!("MulticastSocketClient set_extra_options_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.set_extra_options(&options).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_socket_fd_sync(this: bridge::MulticastSocketClient) -> Result<i32, BusinessError> {
    info!("MulticastSocketClient get_socket_fd_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_socket_fd().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn add_membership_sync(this: bridge::MulticastSocketClient, multicast_address: bridge::NetAddress) -> Result<(), BusinessError> {
    info!("MulticastSocketClient add_membership_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    let address = multicast_address.address;
    let family = multicast_address.family.unwrap_or(1);
    let port = multicast_address.port.unwrap_or(0);
    socket.add_membership(&address, family,  port).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn drop_membership_sync(this: bridge::MulticastSocketClient, multicast_address: bridge::NetAddress) -> Result<(), BusinessError> {
    info!("MulticastSocketClient drop_membership_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    let address = multicast_address.address;
    let family = multicast_address.family.unwrap_or(1);
    let port = multicast_address.port.unwrap_or(0);
    socket.drop_membership(&address, family,  port).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn set_loopback_mode_sync(this: bridge::MulticastSocketClient, loopback: bool) -> Result<(), BusinessError> {
    info!("MulticastSocketClient set_loopback_mode_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.set_loopback_mode(loopback).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_loopback_mode_sync(this: bridge::MulticastSocketClient) -> Result<bool, BusinessError> {
    info!("MulticastSocketClient get_loopback_mode_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_loopback_mode().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn set_reuse_address_sync(this: bridge::MulticastSocketClient, reuse: bool) -> Result<(), BusinessError> {
    info!("MulticastSocketClient set_reuse_address_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.set_reuse_address(reuse).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn set_ttl_sync(this: bridge::MulticastSocketClient, ttl: i32) -> Result<(), BusinessError> {
    info!("MulticastSocketClient set_ttl_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.set_ttl(ttl).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_ttl_sync(this: bridge::MulticastSocketClient) -> Result<i32, BusinessError> {
    info!("MulticastSocketClient get_ttl_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_ttl().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn on_message_inner(env: &AniEnv, this: bridge::MulticastSocketClient, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_message = Some(callback.into_global_callback(env).unwrap());
    socket.on_message_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn on_listening_inner(env: &AniEnv, this: bridge::MulticastSocketClient, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_listening = Some(callback.into_global_callback(env).unwrap());
    socket.on_listening_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn on_close_inner(env: &AniEnv, this: bridge::MulticastSocketClient, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_close = Some(callback.into_global_callback(env).unwrap());
    socket.on_close_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn on_error_inner(env: &AniEnv, this: bridge::MulticastSocketClient, error_callback: AniErrorCallback) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_error = Some(error_callback.into_global_callback(env).unwrap());
    socket.on_error_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_message_inner(env: &AniEnv, this: bridge::MulticastSocketClient) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_message = None;
    socket.off_message_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_listening_inner(env: &AniEnv, this: bridge::MulticastSocketClient) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_listening = None;
    socket.off_listening_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_close_inner(env: &AniEnv, this: bridge::MulticastSocketClient) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_close = None;
    socket.off_close_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_error_inner(env: &AniEnv, this: bridge::MulticastSocketClient) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_error = None;
    socket.off_error_native();
    Ok(())
}
