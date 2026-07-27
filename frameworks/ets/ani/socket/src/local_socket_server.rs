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
use std::mem;
use ani_rs::{
    business_error::BusinessError,
    objects::{AniFnObject, AniErrorCallback, AniRef, GlobalRefCallback, GlobalRefErrorCallback},
    AniEnv, AniVm,
};
use crate::{
    bridge,
    error::SUCCESS,
    wrapper::ffi,
    local_socket_connection::construct_local_socket_connection_instance,
};

pub struct LocalSocketServerWrapper {
    pub cpp_context: UniquePtr<ffi::LocalSocketServerContext>,
    pub on_connect: Option<GlobalRefCallback<(AniRef<'static>,)>>,
    pub on_error: Option<GlobalRefErrorCallback>,
}

impl LocalSocketServerWrapper {
    pub fn new() -> Self {
        LocalSocketServerWrapper {
            cpp_context: ffi::CreateLocalSocketServerContext(),
            on_connect: None,
            on_error: None,
        }
    }

    pub fn listen(&mut self, address: &bridge::LocalAddress) -> Result<(), i32> {
        let addr = ffi::FFILocalAddress::from(address);
        let ret = ffi::LocalSocketServerListen(self.cpp_context.pin_mut(), &addr);
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }

    pub fn get_state(&mut self) -> Result<bridge::SocketStateBase, i32> {
        let mut is_bound = false;
        let mut is_close = false;
        let mut is_connected = false;
        let ret = ffi::LocalSocketServerGetState(self.cpp_context.pin_mut(), &mut is_bound, &mut is_close, &mut is_connected);
        if ret != SUCCESS { return Err(ret); }
        Ok(bridge::SocketStateBase { is_bound, is_close, is_connected })
    }

    pub fn get_socket_fd(&mut self) -> Result<i32, i32> {
        let mut fd = -1;
        let ret = ffi::LocalSocketServerGetSocketFd(self.cpp_context.pin_mut(), &mut fd);
        if ret != SUCCESS { Err(ret) } else { Ok(fd) }
    }

    pub fn set_extra_options(&mut self, options: &bridge::ExtraOptionsBase) -> Result<(), i32> {
        let opts = ffi::FFIExtraOptionsBase::from(options);
        let ret = ffi::LocalSocketServerSetExtraOptions(self.cpp_context.pin_mut(), &opts);
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }

    pub fn get_extra_options(&mut self) -> Result<bridge::ExtraOptionsBase, i32> {
        let mut opts = ffi::FFIExtraOptionsBase {
            has_receive_buffer_size: false, receive_buffer_size: 0,
            has_send_buffer_size: false, send_buffer_size: 0,
            has_reuse_address: false, reuse_address: false,
            has_socket_timeout: false, socket_timeout: 0,
        };
        let ret = ffi::LocalSocketServerGetExtraOptions(self.cpp_context.pin_mut(), &mut opts);
        if ret != SUCCESS { return Err(ret); }
        Ok(bridge::ExtraOptionsBase::from(&opts))
    }

    pub fn get_local_address(&mut self) -> Result<String, i32> {
        let mut address = String::new();
        let ret = ffi::LocalSocketServerGetLocalAddress(self.cpp_context.pin_mut(), &mut address);
        if ret != SUCCESS { Err(ret) } else { Ok(address) }
    }

    pub fn close(&mut self) -> Result<(), i32> {
        let ret = ffi::LocalSocketServerClose(self.cpp_context.pin_mut());
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }

    pub fn on_connect_native(&mut self, server_mutex_ptr: *mut Mutex<LocalSocketServerWrapper>) -> Result<(), i32> {
        let callback = self.on_connect.take();
        let callback_box = LocalSocketServerConnectBox::new(callback, server_mutex_ptr);
        let ret = ffi::LocalSocketServerOnConnect(self.cpp_context.pin_mut(), callback_box);
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }

    pub fn on_error_native(&mut self) -> Result<(), i32> {
        let callback_box = LocalSocketServerErrorBox::new(self);
        let ret = ffi::LocalSocketServerOnError(self.cpp_context.pin_mut(), callback_box);
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }
    
    pub fn off_connect_native(&mut self) -> Result<(), i32> {
        let ret = ffi::LocalSocketServerOffConnect(self.cpp_context.pin_mut());
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }
    
    pub fn off_error_native(&mut self) -> Result<(), i32> {
        let ret = ffi::LocalSocketServerOffError(self.cpp_context.pin_mut());
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }
}

pub struct LocalSocketServerConnectBox {
    pub on_connect: Option<GlobalRefCallback<(AniRef<'static>,)>>,
    server_mutex_ptr: *mut Mutex<LocalSocketServerWrapper>,
}

impl LocalSocketServerConnectBox {
    pub fn new(
        on_connect: Option<GlobalRefCallback<(AniRef<'static>,)>>,
        server_mutex_ptr: *mut Mutex<LocalSocketServerWrapper>,
    ) -> Box<Self> {
        Box::new(LocalSocketServerConnectBox {
            on_connect,
            server_mutex_ptr,
        })
    }

    pub fn on_connect(&self, client_id: i32) -> i32 {
        let vm = AniVm::get_instance();
        let env = match vm.get_env().or_else(|_| vm.attach_current_thread()) {
            Ok(env) => env,
            Err(e) => {
                error!("Failed to get VM environment: {}", e);
                return -1;
            }
        };

        let conn_ref = {
            let mutex = unsafe { &*(self.server_mutex_ptr as *const Mutex<LocalSocketServerWrapper>) };
            let mut wrapper = match mutex.lock() {
                Ok(guard) => guard,
                Err(poisoned) => {
                    error!("Mutex poisoned, recovering...");
                    poisoned.into_inner()
                }
            };
            
            match construct_local_socket_connection_instance(&env, &mut *wrapper, client_id) {
                Ok(conn) => conn,
                Err(e) => {
                    error!("Failed to construct instance: code={}, msg={}", e.code(), e.message());
                    return -1;
                }
            }
        };

        if let Some(callback) = &self.on_connect {
            let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
                callback.execute((conn_ref,));
            }));
            
            match result {
                Ok(_) => 0,
                Err(panic_info) => {
                    error!("Callback panicked in on_connect: {:#?}", panic_info);
                    return -1
                }
            }
        } else {
            error!("on_connect callback not set");
            return -1
        }
    }
}

pub struct LocalSocketServerErrorBox {
    pub on_error: Option<GlobalRefErrorCallback>,
}

impl LocalSocketServerErrorBox {
    pub fn new(server_wrapper: &mut LocalSocketServerWrapper) -> Box<Self> {
        Box::new(LocalSocketServerErrorBox {
            on_error: server_wrapper.on_error.take(),
        })
    }

    pub fn on_error(&self, error_code: i32, error_string: String) -> i32 {
        if let Some(callback) = &self.on_error {
            let business_error = BusinessError::new(error_code, error_string);
            callback.execute(business_error);
            0
        } else {
            -1
        }
    }
}

#[ani_rs::native]
pub fn construct_local_socket_server_instance<'local>(env: &AniEnv<'local>) -> Result<AniRef<'local>, BusinessError> {
    info!("Constructing LocalSocketServer instance");
    static LOCAL_SOCKET_SERVER_CLASS: &CStr = unsafe {
        CStr::from_bytes_with_nul_unchecked(b"@ohos.net.socket.socket.LocalSocketServerInner\0")
    };
    static CTOR_SIGNATURE: &CStr = unsafe { CStr::from_bytes_with_nul_unchecked(b"l:\0") };
    let wrapper = LocalSocketServerWrapper::new();
    let ptr = Box::into_raw(Box::new(Mutex::new(wrapper))) as i64;
    let class = env.find_class(LOCAL_SOCKET_SERVER_CLASS).unwrap();
    let obj = env
        .new_object_with_signature(&class, CTOR_SIGNATURE, (ptr,))
        .unwrap();
    Ok(obj.into())
}

#[ani_rs::native]
pub(crate) fn destroy_local_socket_server_instance(ptr: i64) -> Result<(), BusinessError> {
    if ptr == 0 {
        return Ok(());
    }
    let _ = unsafe { Box::from_raw(ptr as *mut Mutex<LocalSocketServerWrapper>) };
    Ok(())
}

#[ani_rs::native]
pub(crate) fn listen_sync(this: bridge::LocalSocketServer, address: bridge::LocalAddress) -> Result<(), BusinessError> {
    info!("LocalSocketServer listen_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketServerWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.listen(&address).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_state_sync(this: bridge::LocalSocketServer) -> Result<bridge::SocketStateBase, BusinessError> {
    info!("LocalSocketServer get_state_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketServerWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_state().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_socket_fd_sync(this: bridge::LocalSocketServer) -> Result<i32, BusinessError> {
    info!("LocalSocketServer get_socket_fd_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketServerWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_socket_fd().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn set_extra_options_sync(this: bridge::LocalSocketServer, options: bridge::ExtraOptionsBase) -> Result<(), BusinessError> {
    info!("LocalSocketServer set_extra_options_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketServerWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.set_extra_options(&options).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_extra_options_sync(this: bridge::LocalSocketServer) -> Result<bridge::ExtraOptionsBase, BusinessError> {
    info!("LocalSocketServer get_extra_options_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketServerWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_extra_options().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_local_address_sync(this: bridge::LocalSocketServer) -> Result<String, BusinessError> {
    info!("LocalSocketServer get_local_address_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketServerWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_local_address().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn close_sync(this: bridge::LocalSocketServer) -> Result<(), BusinessError> {
    info!("LocalSocketServer close_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketServerWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.close().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn on_connect_inner(env: &AniEnv, this: bridge::LocalSocketServer, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex_ptr = this.native_ptr as *mut Mutex<LocalSocketServerWrapper>;
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketServerWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_connect = Some(callback.into_global_callback(env).unwrap());
    socket.on_connect_native(mutex_ptr).map_err(|e| bridge::convert_to_business_error(e))?;
    Ok(())
}

#[ani_rs::native]
pub(crate) fn on_error_inner(env: &AniEnv, this: bridge::LocalSocketServer, error_callback: AniErrorCallback) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketServerWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_error = Some(error_callback.into_global_callback(env).unwrap());
    socket.on_error_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_connect_inner(env: &AniEnv, this: bridge::LocalSocketServer) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketServerWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_connect = None;
    socket.off_connect_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_error_inner(env: &AniEnv, this: bridge::LocalSocketServer) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<LocalSocketServerWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_error = None;
    socket.off_error_native();
    Ok(())
}
