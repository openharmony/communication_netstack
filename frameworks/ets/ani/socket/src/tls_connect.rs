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
    objects::{AniFnObject, AniAsyncCallback, AniErrorCallback, AniRef, GlobalRefCallback, GlobalRefAsyncCallback, GlobalRefErrorCallback},
    AniEnv,
};
use crate::error::{SUCCESS, ERROR_INTERNAL};
use crate::{bridge, wrapper::TlsConnectWrapper, wrapper::TlsServerWrapper};

pub fn construct_tls_connect_instance<'local>(env: &AniEnv<'local>, server: &mut TlsServerWrapper,
    client_id: i32) -> Result<AniRef<'local>, BusinessError> {
    static TLS_CONNECT_CLASS: &CStr = unsafe {
        CStr::from_bytes_with_nul_unchecked(b"@ohos.net.socket.socket.TLSSocketConnectionInner\0")
    };
    static CTOR_SIGNATURE: &CStr = unsafe { CStr::from_bytes_with_nul_unchecked(b"li:\0") };

    let wrapper = TlsConnectWrapper::new(server, client_id);
    let ptr = Box::into_raw(Box::new(Mutex::new(wrapper))) as i64;

    let class = env.find_class(TLS_CONNECT_CLASS)?;
    let obj = env
        .new_object_with_signature(&class, CTOR_SIGNATURE, (ptr, client_id))
        .map_err(|e| BusinessError::new(ERROR_INTERNAL, e.to_string()))?;
    Ok(obj.into())
}

#[ani_rs::native]
pub(crate) fn destroy_tls_connect_instance(mut ptr: i64) -> Result<(), BusinessError> {
    if ptr == 0 {
        return Ok(());
    }
    let _ = unsafe { Box::from_raw(ptr as *mut Mutex<TlsConnectWrapper>) };
    ptr = 0;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn send_sync(this: bridge::TLSSocketConnect, data: bridge::UnionData) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsConnectWrapper>) };
    let mut socket = mutex.lock().unwrap();
    let options = bridge::TCPSendOptions {
        data,
        encoding: None,
    };
    socket.send(&options).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn close_sync(this: bridge::TLSSocketConnect) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsConnectWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.close().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_remote_address_sync(this: bridge::TLSSocketConnect) -> Result<bridge::NetAddress, BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsConnectWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_remote_address().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_local_address_sync(this: bridge::TLSSocketConnect) -> Result<bridge::NetAddress, BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsConnectWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_local_address().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_remote_certificate_sync(this: bridge::TLSSocketConnect) -> Result<bridge::X509CertRawData, BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsConnectWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_remote_certificate().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_signature_algorithms_sync(this: bridge::TLSSocketConnect) -> Result<Vec<String>, BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsConnectWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_signature_algorithms().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_socket_fd_sync(this: bridge::TLSSocketConnect) -> Result<i32, BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsConnectWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_socket_fd().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_cipher_suite_sync(this: bridge::TLSSocketConnect) -> Result<Vec<String>, BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsConnectWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_cipher_suite().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn on_message_inner(env: &AniEnv, this: bridge::TLSSocketConnect, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsConnectWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_message = Some(callback.into_global_callback(env).unwrap());
    socket.on_message_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_message_inner(env: &AniEnv, this: bridge::TLSSocketConnect) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsConnectWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_message = None;
    socket.off_message_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn on_close_inner(env: &AniEnv, this: bridge::TLSSocketConnect, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsConnectWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_close = Some(callback.into_global_callback(env).unwrap());
    socket.on_close_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_close_inner(env: &AniEnv, this: bridge::TLSSocketConnect) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsConnectWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_close = None;
    socket.off_close_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn on_error_inner(env: &AniEnv, this: bridge::TLSSocketConnect, error_callback: AniErrorCallback) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsConnectWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_error = Some(error_callback.into_global_callback(env).unwrap());
    socket.on_error_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_error_inner(env: &AniEnv, this: bridge::TLSSocketConnect) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsConnectWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_error = None;
    socket.off_error_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

pub struct TlsConnectMessageBox {
    pub on_message: Option<GlobalRefCallback<(bridge::SocketMessageInfo,)>>,
}

impl TlsConnectMessageBox {
    pub fn new(connection_wrapper: &mut TlsConnectWrapper) -> Box<Self> {
        Box::new(TlsConnectMessageBox { on_message: connection_wrapper.on_message.take() })
    }

    pub fn on_message(&self, data: Vec<u8>, address: String, family: String, port: i32, size: i32) -> i32 {
        if let Some(callback) = &self.on_message {
            let array_buffer = ani_rs::typed_array::ArrayBuffer::new_with_vec(data.clone());
            let message_info = bridge::SocketMessageInfo {
                message: array_buffer,
                remote_info: bridge::SocketRemoteInfo {
                    address,
                    family,
                    port,
                    size,
                },
            };
            callback.execute_spawn_thread((message_info,));
            return 0;
        }
        return -1;
    }
}

pub struct TlsConnectCloseBox {
    pub on_close: Option<GlobalRefCallback<()>>,
}

impl TlsConnectCloseBox {
    pub fn new(connection_wrapper: &mut TlsConnectWrapper) -> Box<Self> {
        Box::new(TlsConnectCloseBox { on_close: connection_wrapper.on_close.take() })
    }

    pub fn on_close(&self) -> i32 {
        if let Some(callback) = &self.on_close {
            callback.execute_spawn_thread(());
            return 0;
        }
        return -1;
    }
}

pub struct TlsConnectErrorBox {
    pub on_error: Option<GlobalRefErrorCallback>,
}

impl TlsConnectErrorBox {
    pub fn new(connection_wrapper: &mut TlsConnectWrapper) -> Box<Self> {
        Box::new(TlsConnectErrorBox { on_error: connection_wrapper.on_error.take() })
    }

    pub fn on_error(&self, error_code: i32, error_string: String) -> i32 {
        if let Some(callback) = &self.on_error {
            let business_error = ani_rs::business_error::BusinessError::new(error_code, error_string);
            callback.execute_spawn_thread(business_error);
            return 0;
        }
        return -1;
    }
}
