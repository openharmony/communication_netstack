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
    local_socket_server::LocalSocketServerWrapper,
};

pub struct LocalSocketConnectionWrapper {
    cpp_context: UniquePtr<ffi::LocalSocketConnectionContext>,
    client_id: i32,
    pub on_message: Option<GlobalRefCallback<(bridge::LocalSocketMessageInfo,)>>,
    pub on_close: Option<GlobalRefCallback<()>>,
    pub on_error: Option<GlobalRefErrorCallback>,
}

impl LocalSocketConnectionWrapper {
    pub(crate) fn new(server: &mut LocalSocketServerWrapper, client_id: i32) -> Self {
        LocalSocketConnectionWrapper {
            cpp_context: ffi::CreateLocalSocketConnectionContext(server.cpp_context.pin_mut(), client_id),
            client_id,
            on_message: None,
            on_close: None,
            on_error: None,
        }
    }

    pub fn send(&mut self, options: &bridge::LocalSendOptions) -> Result<(), i32> {
        let opts = ffi::FFILocalSendOptions::from(options);
        let ret = ffi::LocalSocketConnectionSend(self.cpp_context.pin_mut(), &opts);
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }

    pub fn close(&mut self) -> Result<(), i32> {
        let ret = ffi::LocalSocketConnectionClose(self.cpp_context.pin_mut());
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }

    pub fn get_local_address(&mut self) -> Result<String, i32> {
        let mut address = String::new();
        let ret = ffi::LocalSocketConnectionGetLocalAddress(self.cpp_context.pin_mut(), &mut address);
        if ret != SUCCESS { Err(ret) } else { Ok(address) }
    }

    pub fn get_socket_fd(&mut self) -> Result<i32, i32> {
        let mut fd = -1;
        let ret = ffi::LocalSocketConnectionGetSocketFd(self.cpp_context.pin_mut(), &mut fd);
        if ret < 0 { Err(ret) } else { Ok(fd) }
    }

    pub fn on_message_native(&mut self) -> Result<(), i32> {
        let callback_box = LocalSocketConnectionMessageBox::new(self);
        let ret = ffi::LocalSocketConnectionOnMessage(self.cpp_context.pin_mut(), callback_box);
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }

    pub fn off_message_native(&mut self) -> Result<(), i32>  {
        let ret = ffi::LocalSocketConnectionOffMessage(self.cpp_context.pin_mut());
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }

    pub fn on_close_native(&mut self) -> Result<(), i32> {
        let callback_box = LocalSocketConnectionCloseBox::new(self);
        let ret = ffi::LocalSocketConnectionOnClose(self.cpp_context.pin_mut(), callback_box);
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }

    pub fn off_close_native(&mut self) -> Result<(), i32> {
        let ret = ffi::LocalSocketConnectionOffClose(self.cpp_context.pin_mut());
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }

    pub fn on_error_native(&mut self) -> Result<(), i32> {
        let callback_box = LocalSocketConnectionErrorBox::new(self);
        let ret = ffi::LocalSocketConnectionOnError(self.cpp_context.pin_mut(), callback_box);
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }

    pub fn off_error_native(&mut self) -> Result<(), i32> {
        let ret = ffi::LocalSocketConnectionOffError(self.cpp_context.pin_mut());
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }
}

pub struct LocalSocketConnectionMessageBox {
    pub on_message: Option<GlobalRefCallback<(bridge::LocalSocketMessageInfo,)>>,
}

impl LocalSocketConnectionMessageBox {
    pub fn new(client_wrapper: &mut LocalSocketConnectionWrapper) -> Box<Self> {
        Box::new(LocalSocketConnectionMessageBox { on_message: client_wrapper.on_message.take() })
    }

    pub fn on_message(&self, data: Vec<u8>, address: String, size: i32) -> i32 {
        if let Some(callback) = &self.on_message {
            let array_buffer = ani_rs::typed_array::ArrayBuffer::new_with_vec(data);
            let message_info = bridge::LocalSocketMessageInfo {
                message: array_buffer,
                address: address,
                size: size,
            };
            callback.execute((message_info,));
            return 0;
        } else {
            return -1;
        }
    }
}

pub struct LocalSocketConnectionCloseBox {
    pub on_close: Option<GlobalRefCallback<()>>,
}

impl LocalSocketConnectionCloseBox {
    pub fn new(client_wrapper: &mut LocalSocketConnectionWrapper) -> Box<Self> {
        Box::new(LocalSocketConnectionCloseBox { on_close: client_wrapper.on_close.take() })
    }

    pub fn on_close(&self) -> i32 {
        if let Some(callback) = &self.on_close {
            callback.execute(());
            return 0;
        }
        return -1;
    }
}

pub struct LocalSocketConnectionErrorBox {
    pub on_error: Option<GlobalRefErrorCallback>,
}

impl LocalSocketConnectionErrorBox {
    pub fn new(client_wrapper: &mut LocalSocketConnectionWrapper) -> Box<Self> {
        Box::new(LocalSocketConnectionErrorBox { on_error: client_wrapper.on_error.take() })
    }

    pub fn on_error(&self, error_code: i32, error_string: String) -> i32 {
        if let Some(callback) = &self.on_error {
            let business_error = ani_rs::business_error::BusinessError::new(error_code, error_string);
            callback.execute(business_error);
            return 0;
        }
        return -1;
    }
}

pub fn construct_local_socket_connection_instance<'local>(env: &AniEnv<'local>, server: &mut LocalSocketServerWrapper,
    client_id: i32) -> Result<AniRef<'local>, BusinessError> {
    info!("Constructing LocalSocketConnection instance");
    static LOCAL_SOCKET_CONNECTION_CLASS: &CStr = unsafe {
        CStr::from_bytes_with_nul_unchecked(b"@ohos.net.socket.socket.LocalSocketConnectionInner\0")
    };
    static CTOR_SIGNATURE: &CStr = unsafe { CStr::from_bytes_with_nul_unchecked(b"li:\0") };

    let wrapper = LocalSocketConnectionWrapper::new(server, client_id);
    let ptr = Box::into_raw(Box::new(Mutex::new(wrapper))) as i64;

    let class = env.find_class(LOCAL_SOCKET_CONNECTION_CLASS).unwrap();
    let obj = env
        .new_object_with_signature(&class, CTOR_SIGNATURE, (ptr, client_id))
        .unwrap();
    Ok(obj.into())
}

#[ani_rs::native]
pub(crate) fn destroy_local_socket_connection_instance(ptr: i64) -> Result<(), BusinessError> {
    if ptr == 0 {
        return Ok(());
    }
    let _ = unsafe { Box::from_raw(ptr as *mut Mutex<LocalSocketConnectionWrapper>) };
    Ok(())
}

#[ani_rs::native]
pub(crate) fn send_sync(this: bridge::LocalSocketConnection, options: bridge::LocalSendOptions) -> Result<(), BusinessError> {
    info!("LocalSocketConnection send_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketConnectionWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.send(&options).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn close_sync(this: bridge::LocalSocketConnection) -> Result<(), BusinessError> {
    info!("LocalSocketConnection close_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketConnectionWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.close().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_local_address_sync(this: bridge::LocalSocketConnection) -> Result<String, BusinessError> {
    info!("LocalSocketConnection get_local_address_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketConnectionWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_local_address().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_socket_fd_sync(this: bridge::LocalSocketConnection) -> Result<i32, BusinessError> {
    info!("LocalSocketConnection get_socket_fd_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketConnectionWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_socket_fd().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn on_message_inner(env: &AniEnv, this: bridge::LocalSocketConnection, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketConnectionWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_message = Some(callback.into_global_callback(env).unwrap());
    socket.on_message_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn on_close_inner(env: &AniEnv, this: bridge::LocalSocketConnection, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketConnectionWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_close = Some(callback.into_global_callback(env).unwrap());
    socket.on_close_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn on_error_inner(env: &AniEnv, this: bridge::LocalSocketConnection,  error_callback: AniErrorCallback) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketConnectionWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_error = Some(error_callback.into_global_callback(env).unwrap());
    socket.on_error_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_message_inner(env: &AniEnv, this: bridge::LocalSocketConnection) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketConnectionWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_message = None;
    socket.off_message_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_close_inner(env: &AniEnv, this: bridge::LocalSocketConnection) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketConnectionWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_close = None;
    socket.off_close_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_error_inner(env: &AniEnv, this: bridge::LocalSocketConnection) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketConnectionWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_error = None;
    socket.off_error_native();
    Ok(())
}
