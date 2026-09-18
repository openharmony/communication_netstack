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

use std::sync::Mutex;
use std::ffi::CStr;
use cxx::UniquePtr;
use ani_rs::{
    business_error::BusinessError,
    objects::{AniFnObject, AniErrorCallback, AniRef, GlobalRefCallback, GlobalRefErrorCallback},
    AniEnv,
};
use crate::{
    bridge,
    error::SUCCESS,
    wrapper::ffi,
    tcp_socket_server::TcpSocketServerWrapper,
};

pub struct TcpSocketConnectionWrapper {
    cpp_context: UniquePtr<ffi::TcpSocketConnectionContext>,
    pub on_message: Option<GlobalRefCallback<(bridge::SocketMessageInfo,)>>,
    pub on_close: Option<GlobalRefCallback<()>>,
    pub on_error: Option<GlobalRefErrorCallback>,
}

impl TcpSocketConnectionWrapper {
    pub fn new(server: &mut TcpSocketServerWrapper, client_id: i32) -> Self {
        TcpSocketConnectionWrapper {
            cpp_context: ffi::CreateTcpSocketConnectionContext(server.cpp_context.pin_mut(), client_id),
            on_message: None,
            on_close: None,
            on_error: None,
        }
    }
    pub fn send(&mut self, options: &bridge::TCPSendOptions) -> Result<(), i32> {
        let opts = ffi::FFITcpSendOptions::from(options);
        let ret = ffi::TcpSocketConnectionSend(self.cpp_context.pin_mut(), &opts);
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }
    pub fn close(&mut self) -> Result<(), i32> {
        let ret = ffi::TcpSocketConnectionClose(self.cpp_context.pin_mut());
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }
    pub fn get_remote_address(&mut self) -> Result<bridge::NetAddress, i32> {
        let mut address = String::new();
        let mut family = 0;
        let mut port = 0;
        let ret = ffi::TcpSocketConnectionGetRemoteAddress(self.cpp_context.pin_mut(), &mut address, &mut family, &mut port);
        if ret != SUCCESS { return Err(ret); }
        Ok(bridge::NetAddress { address, family: Some(family), port: Some(port) })
    }
    pub fn get_local_address(&mut self) -> Result<bridge::NetAddress, i32> {
        let mut address = String::new();
        let mut family = 0;
        let mut port = 0;
        let ret = ffi::TcpSocketConnectionGetLocalAddress(self.cpp_context.pin_mut(), &mut address, &mut family, &mut port);
        if ret != SUCCESS { return Err(ret); }
        Ok(bridge::NetAddress { address, family: Some(family), port: Some(port) })
    }
    pub fn get_socket_fd(&mut self) -> Result<i32, i32> {
        let mut fd = -1;
        let ret = ffi::TcpSocketConnectionGetSocketFd(self.cpp_context.pin_mut(), &mut fd);
        if ret < 0 { Err(ret) } else { Ok(fd) }
    }
    pub fn on_message_native(&mut self) -> Result<(), i32> {
        let callback_box = TcpSocketConnectionMessageBox::new(self);
        let ret = ffi::TcpSocketConnectionOnMessage(self.cpp_context.pin_mut(), callback_box);
        if ret != SUCCESS { return Err(ret); }
        Ok(())
    }
    pub fn off_message_native(&mut self) -> Result<(), i32> {
        let ret = ffi::TcpSocketConnectionOffMessage(self.cpp_context.pin_mut());
        if ret != SUCCESS { return Err(ret); }
        Ok(())
    }
    pub fn on_close_native(&mut self) -> Result<(), i32> {
        let callback_box = TcpSocketConnectionCloseBox::new(self);
        let ret = ffi::TcpSocketConnectionOnClose(self.cpp_context.pin_mut(), callback_box);
        if ret != SUCCESS { return Err(ret); }
        Ok(())
    }
    pub fn off_close_native(&mut self) -> Result<(), i32> {
        let ret = ffi::TcpSocketConnectionOffClose(self.cpp_context.pin_mut());
        if ret != SUCCESS { return Err(ret); }
        Ok(())
    }
    pub fn on_error_native(&mut self) -> Result<(), i32> {
        let callback_box = TcpSocketConnectionErrorBox::new(self);
        let ret = ffi::TcpSocketConnectionOnError(self.cpp_context.pin_mut(), callback_box);
        if ret != SUCCESS { return Err(ret); }
        Ok(())
    }
    pub fn off_error_native(&mut self) -> Result<(), i32> {
        let ret = ffi::TcpSocketConnectionOffError(self.cpp_context.pin_mut());
        if ret != SUCCESS { return Err(ret); }
        Ok(())
    }
}

pub struct TcpSocketConnectionMessageBox {
    pub on_message: Option<GlobalRefCallback<(bridge::SocketMessageInfo,)>>,
}

impl TcpSocketConnectionMessageBox {
    pub fn new(connect_wrapper: &mut TcpSocketConnectionWrapper) -> Box<Self> {
        Box::new(TcpSocketConnectionMessageBox { on_message: connect_wrapper.on_message.take() })
    }

    pub fn on_message(&self, data: Vec<u8>, address: String, family: String, port: i32, size: i32) -> i32 {
        if let Some(callback) = &self.on_message {
            let array_buffer = ani_rs::typed_array::ArrayBuffer::new_with_vec(data);
            let message_info = bridge::SocketMessageInfo {
                message: array_buffer,
                remote_info: bridge::SocketRemoteInfo { address, family, port, size },
            };
            callback.execute((message_info,));
            return 0;
        }
        -1
    }
}

pub struct TcpSocketConnectionCloseBox {
    pub on_close: Option<GlobalRefCallback<()>>,
}

impl TcpSocketConnectionCloseBox {
    pub fn new(connect_wrapper: &mut TcpSocketConnectionWrapper) -> Box<Self> {
        Box::new(TcpSocketConnectionCloseBox { on_close: connect_wrapper.on_close.take() })
    }

    pub fn on_close(&self) -> i32 {
        if let Some(callback) = &self.on_close {
            callback.execute(());
            return 0;
        }
        -1
    }
}

pub struct TcpSocketConnectionErrorBox {
    pub on_error: Option<GlobalRefErrorCallback>,
}

impl TcpSocketConnectionErrorBox {
    pub fn new(connect_wrapper: &mut TcpSocketConnectionWrapper) -> Box<Self> {
        Box::new(TcpSocketConnectionErrorBox { on_error: connect_wrapper.on_error.take() })
    }

    pub fn on_error(&self, error_code: i32, error_string: String) -> i32 {
        if let Some(callback) = &self.on_error {
            let business_error = BusinessError::new(error_code, error_string);
            callback.execute(business_error);
            return 0;
        }
        -1
    }
}

pub fn construct_tcp_socket_connection_instance<'local>(env: &AniEnv<'local>, server: &mut TcpSocketServerWrapper,
    client_id: i32) -> Result<AniRef<'local>, BusinessError> {
    info!("Constructing TcpSocketConnection instance");
    static LOCAL_SOCKET_CONNECTION_CLASS: &CStr = unsafe {
        CStr::from_bytes_with_nul_unchecked(b"@ohos.net.socket.socket.TCPSocketConnectionInner\0")
    };
    static CTOR_SIGNATURE: &CStr = unsafe { CStr::from_bytes_with_nul_unchecked(b"li:\0") };

    let wrapper = TcpSocketConnectionWrapper::new(server, client_id);
    let ptr = Box::into_raw(Box::new(Mutex::new(wrapper))) as i64;

    let class = env.find_class(LOCAL_SOCKET_CONNECTION_CLASS).unwrap();
    let obj = env
        .new_object_with_signature(&class, CTOR_SIGNATURE, (ptr, client_id))
        .unwrap();
    Ok(obj.into())
}

#[ani_rs::native]
pub(crate) fn destroy_tcp_socket_connection_instance(ptr: i64) -> Result<(), BusinessError> {
    if ptr == 0 {
        return Ok(());
    }
    let _ = unsafe { Box::from_raw(ptr as *mut Mutex<TcpSocketConnectionWrapper>) };
    Ok(())
}

#[ani_rs::native]
pub(crate) fn send_sync(this: bridge::TCPSocketConnect, options: bridge::TCPSendOptions) -> Result<(), BusinessError> {
    info!("TCPSocketConnect send_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketConnectionWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.send(&options).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn close_sync(this: bridge::TCPSocketConnect) -> Result<(), BusinessError> {
    info!("TCPSocketConnect close_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketConnectionWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.close().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_remote_address_sync(this: bridge::TCPSocketConnect) -> Result<bridge::NetAddress, BusinessError> {
    info!("TCPSocketConnect get_remote_address_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketConnectionWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_remote_address().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_local_address_sync(this: bridge::TCPSocketConnect) -> Result<bridge::NetAddress, BusinessError> {
    info!("TCPSocketConnect get_local_address_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketConnectionWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_local_address().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_socket_fd_sync(this: bridge::TCPSocketConnect) -> Result<i32, BusinessError> {
    info!("TCPSocketConnect get_socket_fd_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketConnectionWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_socket_fd().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn on_message_inner(env: &AniEnv, this: bridge::TCPSocketConnect, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketConnectionWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_message = Some(callback.into_global_callback(env).unwrap());
    socket.on_message_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn on_close_inner(env: &AniEnv, this: bridge::TCPSocketConnect, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketConnectionWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_close = Some(callback.into_global_callback(env).unwrap());
    socket.on_close_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}


#[ani_rs::native]
pub(crate) fn on_error_inner(env: &AniEnv, this: bridge::TCPSocketConnect, error_callback: AniErrorCallback) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketConnectionWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_error = Some(error_callback.into_global_callback(env).unwrap());
    socket.on_error_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}
#[ani_rs::native]
pub(crate) fn off_message_inner(env: &AniEnv, this: bridge::TCPSocketConnect) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketConnectionWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_message = None;
    socket.off_message_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}
#[ani_rs::native]
pub(crate) fn off_close_inner(env: &AniEnv, this: bridge::TCPSocketConnect) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketConnectionWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_close = None;
    socket.off_close_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}
#[ani_rs::native]
pub(crate) fn off_error_inner(env: &AniEnv, this: bridge::TCPSocketConnect) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketConnectionWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_error = None;
    socket.off_error_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}