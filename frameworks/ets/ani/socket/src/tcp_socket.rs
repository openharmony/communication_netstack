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
};

pub struct TcpSocketWrapper {
    pub cpp_context: UniquePtr<ffi::TcpSocketContext>,
    pub on_message: Option<GlobalRefCallback<(bridge::SocketMessageInfo,)>>,
    pub on_connect: Option<GlobalRefCallback<()>>,
    pub on_close: Option<GlobalRefCallback<()>>,
    pub on_error: Option<GlobalRefErrorCallback>,
}

impl TcpSocketWrapper {
    pub fn new() -> Self {
        TcpSocketWrapper {
            cpp_context: ffi::CreateTcpSocketContext(),
            on_message: None,
            on_connect: None,
            on_close: None,
            on_error: None,
        }
    }
    pub fn bind(&mut self, address: &bridge::NetAddress) -> Result<(), i32> {
        let addr = ffi::FFINetAddress::from(address);
        let ret = ffi::TcpSocketBind(self.cpp_context.pin_mut(), &addr);
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }
    pub fn connect(&mut self, options: &bridge::TCPConnectOptions) -> Result<(), i32> {
        let opts = ffi::FFITcpConnectOptions::from(options);
        let ret = ffi::TcpSocketConnect(self.cpp_context.pin_mut(), &opts);
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }
    pub fn send(&mut self, options: &bridge::TCPSendOptions) -> Result<(), i32> {
        let opts = ffi::FFITcpSendOptions::from(options);
        let ret = ffi::TcpSocketSend(self.cpp_context.pin_mut(), &opts);
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }
    pub fn close(&mut self) -> Result<(), i32> {
        let ret = ffi::TcpSocketClose(self.cpp_context.pin_mut());
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }
    pub fn get_state(&mut self) -> Result<bridge::SocketStateBase, i32> {
        let mut is_bound = false;
        let mut is_close = false;
        let mut is_connected = false;
        let ret = ffi::TcpSocketGetState(self.cpp_context.pin_mut(), &mut is_bound, &mut is_close, &mut is_connected);
        if ret != SUCCESS { return Err(ret); }
        Ok(bridge::SocketStateBase { is_bound, is_close, is_connected })
    }
    pub fn get_remote_address(&mut self) -> Result<bridge::NetAddress, i32> {
        let mut address = String::new();
        let mut family = 0;
        let mut port = 0;
        let ret = ffi::TcpSocketGetRemoteAddress(self.cpp_context.pin_mut(), &mut address, &mut family, &mut port);
        if ret != SUCCESS { return Err(ret); }
        Ok(bridge::NetAddress { address, family: Some(family), port: Some(port) })
    }
    pub fn get_local_address(&mut self) -> Result<bridge::NetAddress, i32> {
        let mut address = String::new();
        let mut family = 0;
        let mut port = 0;
        let ret = ffi::TcpSocketGetLocalAddress(self.cpp_context.pin_mut(), &mut address, &mut family, &mut port);
        if ret != SUCCESS { return Err(ret); }
        Ok(bridge::NetAddress { address, family: Some(family), port: Some(port) })
    }
    pub fn set_extra_options(&mut self, options: &bridge::TCPExtraOptions) -> Result<(), i32> {
        let opts = ffi::FFITcpExtraOptions::from(options);
        let ret = ffi::TcpSocketSetExtraOptions(self.cpp_context.pin_mut(), &opts);
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }
    pub fn get_socket_fd(&mut self) -> Result<i32, i32> {
        let mut fd = -1;
        let ret = ffi::TcpClientGetSocketFd(self.cpp_context.pin_mut(), &mut fd);
        if ret != SUCCESS { Err(ret) } else { Ok(fd) }
    }
    pub fn on_message_native(&mut self) -> Result<(), i32> {
        let callback_box = TcpSocketMessageBox::new(self);
        let ret = ffi::TcpSocketOnMessage(self.cpp_context.pin_mut(), callback_box);
        if ret != SUCCESS { return Err(ret); }
        Ok(())
    }
    pub fn off_message_native(&mut self) -> Result<(), i32> {
        let ret = ffi::TcpSocketOffMessage(self.cpp_context.pin_mut());
        if ret != SUCCESS { return Err(ret); }
        Ok(())
    }
    pub fn on_connect_native(&mut self) -> Result<(), i32> {
        let callback_box = TcpSocketConnectBox::new(self);
        let ret = ffi::TcpSocketOnConnect(self.cpp_context.pin_mut(), callback_box);
        if ret != SUCCESS { return Err(ret); }
        Ok(())
    }
    pub fn off_connect_native(&mut self) -> Result<(), i32> {
        let ret = ffi::TcpSocketOffConnect(self.cpp_context.pin_mut());
        if ret != SUCCESS { return Err(ret); }
        Ok(())
    }
    pub fn on_close_native(&mut self) -> Result<(), i32> {
        let callback_box = TcpSocketCloseBox::new(self);
        let ret = ffi::TcpSocketOnClose(self.cpp_context.pin_mut(), callback_box);
        if ret != SUCCESS { return Err(ret); }
        Ok(())
    }
    pub fn off_close_native(&mut self) -> Result<(), i32> {
        let ret = ffi::TcpSocketOffClose(self.cpp_context.pin_mut());
        if ret != SUCCESS { return Err(ret); }
        Ok(())
    }
    pub fn on_error_native(&mut self) -> Result<(), i32> {
        let callback_box = TcpSocketErrorBox::new(self);
        let ret = ffi::TcpSocketOnError(self.cpp_context.pin_mut(), callback_box);
        if ret != SUCCESS { return Err(ret); }
        Ok(())
    }
    pub fn off_error_native(&mut self) -> Result<(), i32> {
        let ret = ffi::TcpSocketOffError(self.cpp_context.pin_mut());
        if ret != SUCCESS { return Err(ret); }
        Ok(())
    }
}

pub struct TcpSocketMessageBox {
    pub on_message: Option<GlobalRefCallback<(bridge::SocketMessageInfo,)>>,
}

impl TcpSocketMessageBox {
    pub fn new(client_wrapper: &mut TcpSocketWrapper) -> Box<Self> {
        Box::new(TcpSocketMessageBox { on_message: client_wrapper.on_message.take() })
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

pub struct TcpSocketConnectBox {
    pub on_connect: Option<GlobalRefCallback<()>>,
}

impl TcpSocketConnectBox {
    pub fn new(client_wrapper: &mut TcpSocketWrapper) -> Box<Self> {
        Box::new(TcpSocketConnectBox { on_connect: client_wrapper.on_connect.take() })
    }

    pub fn on_connect(&self) -> i32 {
        if let Some(callback) = &self.on_connect {
            callback.execute(());
            return 0;
        }
        -1
    }
}

pub struct TcpSocketCloseBox {
    pub on_close: Option<GlobalRefCallback<()>>,
}

impl TcpSocketCloseBox {
    pub fn new(client_wrapper: &mut TcpSocketWrapper) -> Box<Self> {
        Box::new(TcpSocketCloseBox { on_close: client_wrapper.on_close.take() })
    }

    pub fn on_close(&self) -> i32 {
        if let Some(callback) = &self.on_close {
            callback.execute(());
            return 0;
        }
        -1
    }
}

pub struct TcpSocketErrorBox {
    pub on_error: Option<GlobalRefErrorCallback>,
}

impl TcpSocketErrorBox {
    pub fn new(client_wrapper: &mut TcpSocketWrapper) -> Box<Self> {
        Box::new(TcpSocketErrorBox { on_error: client_wrapper.on_error.take() })
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

#[ani_rs::native]
pub fn construct_tcp_socket_instance<'local>(env: &AniEnv<'local>) -> Result<AniRef<'local>, BusinessError> {
    info!("Constructing TCPSocketClient instance");
    static TCP_SOCKET_CLASS: &CStr = unsafe {
        CStr::from_bytes_with_nul_unchecked(b"@ohos.net.socket.socket.TCPSocketInner\0")
    };
    static CTOR_SIGNATURE: &CStr = unsafe { CStr::from_bytes_with_nul_unchecked(b"l:\0") };
    let wrapper = TcpSocketWrapper::new();
    let ptr = Box::into_raw(Box::new(Mutex::new(wrapper))) as i64;
    let class = env.find_class(TCP_SOCKET_CLASS).unwrap();
    let obj = env
        .new_object_with_signature(&class, CTOR_SIGNATURE, (ptr,))
        .unwrap();
    Ok(obj.into())
}

#[ani_rs::native]
pub(crate) fn destroy_tcp_socket_instance(ptr: i64) -> Result<(), BusinessError> {
    if ptr == 0 {
        return Ok(());
    }
    let _ = unsafe { Box::from_raw(ptr as *mut Mutex<TcpSocketWrapper>) };
    Ok(())
}

#[ani_rs::native]
pub(crate) fn bind_sync(this: bridge::TCPSocketClient, address: bridge::NetAddress) -> Result<(), BusinessError> {
    info!("TCPSocket bind_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.bind(&address).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn connect_sync(this: bridge::TCPSocketClient, options: bridge::TCPConnectOptions) -> Result<(), BusinessError> {
    info!("TCPSocket connect_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.connect(&options).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn send_sync(this: bridge::TCPSocketClient, options: bridge::TCPSendOptions) -> Result<(), BusinessError> {
    info!("TCPSocket send_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.send(&options).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn close_sync(this: bridge::TCPSocketClient) -> Result<(), BusinessError> {
    info!("TCPSocket close_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.close().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_remote_address_sync(this: bridge::TCPSocketClient) -> Result<bridge::NetAddress, BusinessError> {
    info!("TCPSocket get_remote_address_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_remote_address().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_local_address_sync(this: bridge::TCPSocketClient) -> Result<bridge::NetAddress, BusinessError> {
    info!("TCPSocket get_local_address_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_local_address().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_state_sync(this: bridge::TCPSocketClient) -> Result<bridge::SocketStateBase, BusinessError> {
    info!("TCPSocket get_state_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_state().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn set_extra_options_sync(this: bridge::TCPSocketClient, options: bridge::TCPExtraOptions) -> Result<(), BusinessError> {
    info!("TCPSocket set_extra_options_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.set_extra_options(&options).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_socket_fd_sync(this: bridge::TCPSocketClient) -> Result<i32, BusinessError> {
    info!("TCPSocket get_socket_fd_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_socket_fd().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn on_message_inner(env: &AniEnv, this: bridge::TCPSocketClient, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_message = Some(callback.into_global_callback(env).unwrap());
    socket.on_message_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn on_connect_inner(env: &AniEnv, this: bridge::TCPSocketClient, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_connect = Some(callback.into_global_callback(env).unwrap());
    socket.on_connect_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn on_close_inner(env: &AniEnv, this: bridge::TCPSocketClient, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_close = Some(callback.into_global_callback(env).unwrap());
    socket.on_close_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn on_error_inner(env: &AniEnv, this: bridge::TCPSocketClient, error_callback: AniErrorCallback) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_error = Some(error_callback.into_global_callback(env).unwrap());
    socket.on_error_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_message_inner(env: &AniEnv, this: bridge::TCPSocketClient) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_message = None;
    socket.off_message_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_connect_inner(env: &AniEnv, this: bridge::TCPSocketClient) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_connect = None;
    socket.off_connect_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_close_inner(env: &AniEnv, this: bridge::TCPSocketClient) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_close = None;
    socket.off_close_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_error_inner(env: &AniEnv, this: bridge::TCPSocketClient) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<TcpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_error = None;
    socket.off_error_native().map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}
