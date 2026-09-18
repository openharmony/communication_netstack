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
    objects::{AniFnObject, AniErrorCallback, AniRef, GlobalRefCallback, GlobalRefErrorCallback},
    AniEnv,
};
use cxx::UniquePtr;
use crate::{
    bridge,
    error::SUCCESS,
    wrapper::ffi,
};

pub struct LocalSocketWrapper {
    cpp_context: UniquePtr<ffi::LocalSocketContext>,
    pub on_message: Option<GlobalRefCallback<(bridge::LocalSocketMessageInfo,)>>,
    pub on_listening: Option<GlobalRefCallback<()>>,
    pub on_connect: Option<GlobalRefCallback<()>>,
    pub on_close: Option<GlobalRefCallback<()>>,
    pub on_error: Option<GlobalRefErrorCallback>,
}

impl LocalSocketWrapper {
    pub fn new() -> Self {
        LocalSocketWrapper {
            cpp_context: ffi::CreateLocalSocketContext(),
            on_message: None,
            on_listening: None,
            on_connect: None,
            on_close: None,
            on_error: None,
        }
    }

    pub fn bind(&mut self, address: &bridge::LocalAddress) -> Result<(), i32> {
        let addr = ffi::FFILocalAddress::from(address);
        let ret = ffi::LocalSocketBind(self.cpp_context.pin_mut(), &addr);
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }

    pub fn connect(&mut self, options: &bridge::LocalConnectOptions) -> Result<(), i32> {
        let opts = ffi::FFILocalConnectOptions::from(options);
        let ret = ffi::LocalSocketConnect(self.cpp_context.pin_mut(), &opts);
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }

    pub fn send(&mut self, options: &bridge::LocalSendOptions) -> Result<(), i32> {
        let opts = ffi::FFILocalSendOptions::from(options);
        let ret = ffi::LocalSocketSend(self.cpp_context.pin_mut(), &opts);
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }

    pub fn close(&mut self) -> Result<(), i32> {
        let ret = ffi::LocalSocketClose(self.cpp_context.pin_mut());
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }

    pub fn get_state(&mut self) -> Result<bridge::SocketStateBase, i32> {
        let mut is_bound = false;
        let mut is_close = false;
        let mut is_connected = false;
        let ret = ffi::LocalSocketGetState(self.cpp_context.pin_mut(), &mut is_bound, &mut is_close, &mut is_connected);
        if ret != SUCCESS { return Err(ret); }
        Ok(bridge::SocketStateBase { is_bound, is_close, is_connected })
    }

    pub fn get_socket_fd(&mut self) -> Result<i32, i32> {
        let mut fd = -1;
        let ret = ffi::LocalSocketGetSocketFd(self.cpp_context.pin_mut(), &mut fd);
        if ret != SUCCESS { Err(ret) } else { Ok(fd) }
    }

    pub fn set_extra_options(&mut self, options: &bridge::ExtraOptionsBase) -> Result<(), i32> {
        let opts = ffi::FFIExtraOptionsBase::from(options);
        let ret = ffi::LocalSocketSetExtraOptions(self.cpp_context.pin_mut(), &opts);
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }

    pub fn get_extra_options(&mut self) -> Result<bridge::ExtraOptionsBase, i32> {
        let mut opts = ffi::FFIExtraOptionsBase {
            has_receive_buffer_size: false, receive_buffer_size: 0,
            has_send_buffer_size: false, send_buffer_size: 0,
            has_reuse_address: false, reuse_address: false,
            has_socket_timeout: false, socket_timeout: 0,
        };
        let ret = ffi::LocalSocketGetExtraOptions(self.cpp_context.pin_mut(), &mut opts);
        if ret != SUCCESS { return Err(ret); }
        Ok(bridge::ExtraOptionsBase::from(&opts))
    }

    pub fn get_local_address(&mut self) -> Result<String, i32> {
        let mut address = String::new();
        let ret = ffi::LocalSocketGetLocalAddress(self.cpp_context.pin_mut(), &mut address);
        if ret != SUCCESS { Err(ret) } else { Ok(address) }
    }

    pub fn on_message_native(&mut self) -> Result<(), i32> {
        let callback_box = LocalSocketMessageBox::new(self);
        let ret = ffi::LocalSocketOnMessage(self.cpp_context.pin_mut(), callback_box);
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }

    pub fn off_message_native(&mut self) -> Result<(), i32>  {
        let ret = ffi::LocalSocketOffMessage(self.cpp_context.pin_mut());
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }

    pub fn on_connect_native(&mut self) -> Result<(), i32> {
        let callback_box = LocalSocketConnectBox::new(self);
        let ret = ffi::LocalSocketOnConnect(self.cpp_context.pin_mut(), callback_box);
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }

    pub fn off_connect_native(&mut self) -> Result<(), i32> {
        let ret = ffi::LocalSocketOffConnect(self.cpp_context.pin_mut());
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }

    pub fn on_close_native(&mut self) -> Result<(), i32> {
        let callback_box = LocalSocketCloseBox::new(self);
        let ret = ffi::LocalSocketOnClose(self.cpp_context.pin_mut(), callback_box);
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }

    pub fn off_close_native(&mut self) -> Result<(), i32> {
        let ret = ffi::LocalSocketOffClose(self.cpp_context.pin_mut());
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }

    pub fn on_error_native(&mut self) -> Result<(), i32> {
        let callback_box = LocalSocketErrorBox::new(self);
        let ret = ffi::LocalSocketOnError(self.cpp_context.pin_mut(), callback_box);
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }

    pub fn off_error_native(&mut self) -> Result<(), i32> {
        let ret = ffi::LocalSocketOffError(self.cpp_context.pin_mut());
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }
}

pub struct LocalSocketMessageBox {
    pub on_message: Option<GlobalRefCallback<(bridge::LocalSocketMessageInfo,)>>,
}

impl LocalSocketMessageBox {
    pub fn new(client_wrapper: &mut LocalSocketWrapper) -> Box<Self> {
        Box::new(LocalSocketMessageBox { on_message: client_wrapper.on_message.take() })
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

pub struct LocalSocketConnectBox {
    pub on_connect: Option<GlobalRefCallback<()>>,
}

impl LocalSocketConnectBox {
    pub fn new(client_wrapper: &mut LocalSocketWrapper) -> Box<Self> {
        Box::new(LocalSocketConnectBox { on_connect: client_wrapper.on_connect.take() })
    }

    pub fn on_connect(&self) -> i32 {
        if let Some(callback) = &self.on_connect {
            callback.execute(());
            return 0;
        }
        return -1;
    }
}

pub struct LocalSocketCloseBox {
    pub on_close: Option<GlobalRefCallback<()>>,
}

impl LocalSocketCloseBox {
    pub fn new(client_wrapper: &mut LocalSocketWrapper) -> Box<Self> {
        Box::new(LocalSocketCloseBox { on_close: client_wrapper.on_close.take() })
    }

    pub fn on_close(&self) -> i32 {
        if let Some(callback) = &self.on_close {
            callback.execute(());
            return 0;
        }
        return -1;
    }
}

pub struct LocalSocketErrorBox {
    pub on_error: Option<GlobalRefErrorCallback>,
}

impl LocalSocketErrorBox {
    pub fn new(client_wrapper: &mut LocalSocketWrapper) -> Box<Self> {
        Box::new(LocalSocketErrorBox { on_error: client_wrapper.on_error.take() })
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

#[ani_rs::native]
pub fn construct_local_socket_instance<'local>(env: &AniEnv<'local>) -> Result<AniRef<'local>, BusinessError> {
    info!("Constructing LocalSocket instance");
    static LOCAL_SOCKET_CLASS: &CStr = unsafe {
        CStr::from_bytes_with_nul_unchecked(b"@ohos.net.socket.socket.LocalSocketInner\0")
    };
    static CTOR_SIGNATURE: &CStr = unsafe { CStr::from_bytes_with_nul_unchecked(b"l:\0") };
    let wrapper = LocalSocketWrapper::new();
    let ptr = Box::into_raw(Box::new(Mutex::new(wrapper))) as i64;
    let class = env.find_class(LOCAL_SOCKET_CLASS).unwrap();
    let obj = env
        .new_object_with_signature(&class, CTOR_SIGNATURE, (ptr,))
        .unwrap();
    Ok(obj.into())
}

#[ani_rs::native]
pub(crate) fn destroy_local_socket_instance(ptr: i64) -> Result<(), BusinessError> {
    if ptr == 0 {
        return Ok(());
    }
    let _ = unsafe { Box::from_raw(ptr as *mut Mutex<LocalSocketWrapper>) };
    Ok(())
}

#[ani_rs::native]
pub(crate) fn bind_sync(this: bridge::LocalSocketClient, address: bridge::LocalAddress) -> Result<(), BusinessError> {
    info!("LocalSocketClient bind_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.bind(&address).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn connect_sync(this: bridge::LocalSocketClient, options: bridge::LocalConnectOptions) -> Result<(), BusinessError> {
    info!("LocalSocketClient connect_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.connect(&options).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn send_sync(this: bridge::LocalSocketClient, options: bridge::LocalSendOptions) -> Result<(), BusinessError> {
    info!("LocalSocketClient send_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.send(&options).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn close_sync(this: bridge::LocalSocketClient) -> Result<(), BusinessError> {
    info!("LocalSocketClient close_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.close().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_state_sync(this: bridge::LocalSocketClient) -> Result<bridge::SocketStateBase, BusinessError> {
    info!("LocalSocketClient get_state_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_state().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_socket_fd_sync(this: bridge::LocalSocketClient) -> Result<i32, BusinessError> {
    info!("LocalSocketClient get_socket_fd_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_socket_fd().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn set_extra_options_sync(this: bridge::LocalSocketClient, options: bridge::ExtraOptionsBase) -> Result<(), BusinessError> {
    info!("LocalSocketClient set_extra_options_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.set_extra_options(&options).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_extra_options_sync(this: bridge::LocalSocketClient) -> Result<bridge::ExtraOptionsBase, BusinessError> {
    info!("LocalSocketClient get_extra_options_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_extra_options().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_local_address_sync(this: bridge::LocalSocketClient) -> Result<String, BusinessError> {
    info!("LocalSocketClient get_local_address_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_local_address().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn on_message_inner(env: &AniEnv, this: bridge::LocalSocketClient, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_message = Some(callback.into_global_callback(env).unwrap());
    socket.on_message_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn on_connect_inner(env: &AniEnv, this: bridge::LocalSocketClient, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_connect = Some(callback.into_global_callback(env).unwrap());
    socket.on_connect_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn on_close_inner(env: &AniEnv, this: bridge::LocalSocketClient, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_close = Some(callback.into_global_callback(env).unwrap());
    socket.on_close_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn on_error_inner(env: &AniEnv, this: bridge::LocalSocketClient, error_callback: AniErrorCallback) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_error = Some(error_callback.into_global_callback(env).unwrap());
    socket.on_error_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_message_inner(env: &AniEnv, this: bridge::LocalSocketClient) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_message = None;
    socket.off_message_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_connect_inner(env: &AniEnv, this: bridge::LocalSocketClient) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_connect = None;
    socket.off_connect_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_close_inner(env: &AniEnv, this: bridge::LocalSocketClient) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_close = None;
    socket.off_close_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_error_inner(env: &AniEnv, this: bridge::LocalSocketClient) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_error = None;
    socket.off_error_native();
    Ok(())
}
