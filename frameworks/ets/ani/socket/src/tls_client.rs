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

use crate::{
    bridge,
    wrapper::TlsClientWrapper,
    tcp_socket::TcpSocketWrapper,
};

#[ani_rs::native]
pub(crate) fn construct_tls_client_instance<'local>(env: &AniEnv<'local>, tcp_socket: Option<bridge::TCPSocketClient>) -> Result<AniRef<'local>, BusinessError> {
    static TLS_CLIENT_CLASS: &CStr = unsafe {
        CStr::from_bytes_with_nul_unchecked(
            b"@ohos.net.socket.socket.TLSSocketInner\0",
        )
    };
    static CTOR_SIGNATURE: &CStr = unsafe { CStr::from_bytes_with_nul_unchecked(b"l:\0") };

    let wrapper = if let Some(tcp) = tcp_socket {
        let mutex = unsafe { &*(tcp.native_ptr as *const Mutex<TcpSocketWrapper>) };
        let mut tcp_wrapper = mutex.lock().unwrap();

        TlsClientWrapper::new_from_tcp(&mut *tcp_wrapper)
            .map_err(|code| bridge::convert_to_business_error(code))?
    } else {
        TlsClientWrapper::new()
            .map_err(|code| bridge::convert_to_business_error(code))?
    };

    let ptr = Box::into_raw(Box::new(Mutex::new(wrapper)));
    let class = env.find_class(TLS_CLIENT_CLASS).unwrap();
    let obj = env
        .new_object_with_signature(&class, CTOR_SIGNATURE, (ptr as i64,))
        .unwrap();
    Ok(obj.into())
}

#[ani_rs::native]
pub(crate) fn destroy_tls_client_instance(mut ptr: i64) -> Result<(), BusinessError> {
    if ptr == 0 {
        return Ok(());
    }
    let _ = unsafe { Box::from_raw(ptr as *mut Mutex<TlsClientWrapper>) };
    ptr = 0;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn bind_sync(this: bridge::TLSSocketClient, address: bridge::NetAddress) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    let ret = socket.bind(&address).map_err(|e| bridge::convert_to_business_error(e));
    return ret;
}

#[ani_rs::native]
pub(crate) fn connect_sync(this: bridge::TLSSocketClient, options: bridge::TLSConnectOptions) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    let ret = socket.connect(&options).map_err(|e| bridge::convert_to_business_error(e));
    return ret;
}

#[ani_rs::native]
pub(crate) fn send_sync(this: bridge::TLSSocketClient, data: bridge::UnionData) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    let options = bridge::TCPSendOptions {
        data,
        encoding: None,
    };
    let ret = socket.send(&options).map_err(|e| bridge::convert_to_business_error(e));
    return ret;
}

#[ani_rs::native]
pub(crate) fn close_sync(this: bridge::TLSSocketClient) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    let ret = socket.close().map_err(|e| bridge::convert_to_business_error(e));
    return ret;
}

#[ani_rs::native]
pub(crate) fn get_remote_address_sync(this: bridge::TLSSocketClient) -> Result<bridge::NetAddress, BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    let ret = socket.get_remote_address().map_err(|e| bridge::convert_to_business_error(e));
    return ret;
}

#[ani_rs::native]
pub(crate) fn get_local_address_sync(this: bridge::TLSSocketClient) -> Result<bridge::NetAddress, BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    let ret = socket.get_local_address().map_err(|e| bridge::convert_to_business_error(e));
    return ret;
}

#[ani_rs::native]
pub(crate) fn get_state_sync(this: bridge::TLSSocketClient) -> Result<bridge::SocketStateBase, BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    let ret = socket.get_state().map_err(|e| bridge::convert_to_business_error(e));
    return ret;
}

#[ani_rs::native]
pub(crate) fn get_certificate_sync(this: bridge::TLSSocketClient) -> Result<bridge::X509CertRawData, BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    let ret = socket.get_certificate().map_err(|e| bridge::convert_to_business_error(e));
    return ret;
}

#[ani_rs::native]
pub(crate) fn get_remote_certificate_sync(this: bridge::TLSSocketClient) -> Result<bridge::X509CertRawData, BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    let ret = socket.get_remote_certificate().map_err(|e| bridge::convert_to_business_error(e));
    return ret;
}

#[ani_rs::native]
pub(crate) fn get_cipher_suite_sync(this: bridge::TLSSocketClient) -> Result<Vec<String>, BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    let ret = socket.get_cipher_suite().map_err(|e| bridge::convert_to_business_error(e));
    return ret;
}

#[ani_rs::native]
pub(crate) fn get_protocol_sync(this: bridge::TLSSocketClient) -> Result<String, BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    let ret = socket.get_protocol().map_err(|e| bridge::convert_to_business_error(e));
    return ret;
}

#[ani_rs::native]
pub(crate) fn get_socket_fd_sync(this: bridge::TLSSocketClient) -> Result<i32, BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    let ret = socket.get_socket_fd().map_err(|e| bridge::convert_to_business_error(e));
    return ret;
}

#[ani_rs::native]
pub(crate) fn set_extra_options_sync(this: bridge::TLSSocketClient, options: bridge::TCPExtraOptions) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    let ret = socket.set_extra_options(&options).map_err(|e| bridge::convert_to_business_error(e));
    return ret;
}

#[ani_rs::native]
pub(crate) fn get_signature_algorithms_sync(this: bridge::TLSSocketClient) -> Result<Vec<String>, BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    let ret = socket.get_signature_algorithms().map_err(|e| bridge::convert_to_business_error(e));
    return ret;
}

#[ani_rs::native]
pub(crate) fn on_message_inner(env: &AniEnv, this: bridge::TLSSocketClient, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_message = Some(callback.into_global_callback(env).unwrap());
    socket.on_message_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn on_connect_inner(env: &AniEnv, this: bridge::TLSSocketClient, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_connect = Some(callback.into_global_callback(env).unwrap());
    socket.on_connect_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn on_close_inner(env: &AniEnv, this: bridge::TLSSocketClient, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_close = Some(callback.into_global_callback(env).unwrap());
    socket.on_close_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn on_error_inner(env: &AniEnv, this: bridge::TLSSocketClient, error_callback: AniErrorCallback) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_error = Some(error_callback.into_global_callback(env).unwrap());
    socket.on_error_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_message_inner(env: &AniEnv, this: bridge::TLSSocketClient) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_message = None;
    socket.off_message_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_connect_inner(env: &AniEnv, this: bridge::TLSSocketClient) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_connect = None;
    socket.off_connect_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_close_inner(env: &AniEnv, this: bridge::TLSSocketClient) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_close = None;
    socket.off_close_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_error_inner(env: &AniEnv, this: bridge::TLSSocketClient) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TlsClientWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_error = None;
    socket.off_error_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

pub struct TlsClientMessageBox {
    pub on_message: Option<GlobalRefCallback<(bridge::SocketMessageInfo,)>>,
}

impl TlsClientMessageBox {
    pub fn new(client_wrapper: &mut TlsClientWrapper) -> Box<Self> {
        Box::new(TlsClientMessageBox { on_message: client_wrapper.on_message.take() })
    }

    pub fn on_message(&self, data: Vec<u8>, address: String, family: String, port: i32, size: i32) -> i32 {
        if let Some(callback) = &self.on_message {
            let array_buffer = ani_rs::typed_array::ArrayBuffer::new_with_vec(data);
            let message_info = bridge::SocketMessageInfo {
                message: array_buffer,
                remote_info: bridge::SocketRemoteInfo { address, family, port, size },
            };
            callback.execute_spawn_thread((message_info,));
            return 0;
        }
        return -1;
    }
}

pub struct TlsClientConnectBox {
    pub on_connect: Option<GlobalRefCallback<()>>,
}

impl TlsClientConnectBox {
    pub fn new(client_wrapper: &mut TlsClientWrapper) -> Box<Self> {
        Box::new(TlsClientConnectBox { on_connect: client_wrapper.on_connect.take() })
    }

    pub fn on_connect(&self) -> i32 {
        if let Some(callback) = &self.on_connect {
            callback.execute_spawn_thread(());
            return 0;
        }        
        return -1;
    }
}

pub struct TlsClientCloseBox {
    pub on_close: Option<GlobalRefCallback<()>>,
}

impl TlsClientCloseBox {
    pub fn new(client_wrapper: &mut TlsClientWrapper) -> Box<Self> {
        Box::new(TlsClientCloseBox { on_close: client_wrapper.on_close.take() })
    }

    pub fn on_close(&self) -> i32 {
        if let Some(callback) = &self.on_close {
            callback.execute_spawn_thread(());
            return 0;
        }
        return -1;
    }
}

pub struct TlsClientErrorBox {
    pub on_error: Option<GlobalRefErrorCallback>,
}

impl TlsClientErrorBox {
    pub fn new(client_wrapper: &mut TlsClientWrapper) -> Box<Self> {
        Box::new(TlsClientErrorBox { on_error: client_wrapper.on_error.take() })
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
