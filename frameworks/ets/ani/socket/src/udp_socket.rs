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
    objects::{AniFnObject, AniAsyncCallback, AniErrorCallback, AniRef, GlobalRefCallback, GlobalRefErrorCallback},
    AniEnv,
};
use cxx::UniquePtr;

use crate::{
    bridge,
    error::SUCCESS,
    wrapper::ffi,
};

// ==================== UDP Socket Client or Multicast Socket Client Wrapper ====================
pub struct UdpSocketWrapper {
    cpp_context: UniquePtr<ffi::UdpSocketContext>,
    pub on_message: Option<GlobalRefCallback<(bridge::SocketMessageInfo,)>>,
    pub on_listening: Option<GlobalRefCallback<()>>,
    pub on_close: Option<GlobalRefCallback<()>>,
    pub on_error: Option<GlobalRefErrorCallback>,
}

impl UdpSocketWrapper {
    pub fn new(is_multicast: bool) -> Self {
        let context = if is_multicast {
            ffi::CreateMulticastClientContext()
        } else {
            ffi::CreateUdpSocketContext()
        };

        UdpSocketWrapper {
            cpp_context: context,
            on_message: None,
            on_listening: None,
            on_close: None,
            on_error: None,
        }
    }

    pub fn bind(&mut self, address: &bridge::NetAddress) -> Result<(), i32> {
        let addr = ffi::FFINetAddress::from(address);
        let ret = ffi::UdpClientBind(self.cpp_context.pin_mut(), &addr);
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }

    pub fn send(&mut self, options: &bridge::UDPSendOptions) -> Result<(), i32> {
        let send_opts = ffi::FFIUdpSendOptions::from(options);
        let proxy_opts = options.proxy.as_ref()
            .map(|p| ffi::FFIProxyOptions::from(p))
            .unwrap_or_default();
        let ret = ffi::UdpClientSend(self.cpp_context.pin_mut(), &send_opts, &proxy_opts);
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }

    pub fn close(&mut self) -> Result<(), i32> {
        let ret = ffi::UdpClientClose(self.cpp_context.pin_mut());
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }

    pub fn get_state(&mut self) -> Result<bridge::SocketStateBase, i32> {
        let mut is_bound = false;
        let mut is_close = false;
        let mut is_connected = false;
        let ret = ffi::UdpClientGetState(self.cpp_context.pin_mut(), &mut is_bound, &mut is_close, &mut is_connected);
        if ret != SUCCESS { return Err(ret); }
        Ok(bridge::SocketStateBase { is_close, is_bound, is_connected })
    }

    pub fn get_remote_address(&mut self) -> Result<bridge::NetAddress, i32> {
        Ok(bridge::NetAddress { address: String::new(), family: None, port: None })
    }

    pub fn set_extra_options(&mut self, options: &bridge::UDPExtraOptions) -> Result<(), i32> {
        let opts = ffi::FFIUdpExtraOptions::from(options);
        let ret = ffi::UdpClientSetExtraOptions(self.cpp_context.pin_mut(), &opts);
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }

    pub fn get_socket_fd(&mut self) -> Result<i32, i32> {
        let mut fd = -1;
        let ret = ffi::UdpClientGetSocketFd(self.cpp_context.pin_mut(), &mut fd);
        if ret != SUCCESS { Err(ret) } else { Ok(fd) }
    }

    pub fn get_local_address(&mut self) -> Result<bridge::NetAddress, i32> {
        let mut address = String::new();
        let mut family = 0;
        let mut port = 0;
        let ret = ffi::UdpClientGetLocalAddress(self.cpp_context.pin_mut(), &mut address, &mut family, &mut port);
        if ret != SUCCESS { return Err(ret); }
        Ok(bridge::NetAddress { address, family: Some(family), port: Some(port) })
    }

    pub fn add_membership(&mut self, addr: &str, family: i32, port: i32) -> Result<(), i32> {
        let ret = ffi::UdpClientAddMembership(self.cpp_context.pin_mut(), addr, family, port);
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }

    pub fn drop_membership(&mut self, addr: &str, family: i32, port: i32) -> Result<(), i32> {
        let ret = ffi::UdpClientDropMembership(self.cpp_context.pin_mut(), addr, family, port);
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }

    pub fn set_ttl(&mut self, ttl: i32) -> Result<(), i32> {
        let ret = ffi::UdpClientSetTtl(self.cpp_context.pin_mut(), ttl);
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }

    pub fn get_ttl(&mut self) -> Result<i32, i32> {
        let mut ttl = 0;
        let ret = ffi::UdpClientGetTtl(self.cpp_context.pin_mut(), &mut ttl);
        if ret != SUCCESS { Err(ret) } else { Ok(ttl) }
    }

    pub fn set_loopback_mode(&mut self, loopback: bool) -> Result<(), i32> {
        let ret = ffi::UdpClientSetLoopbackMode(self.cpp_context.pin_mut(), loopback);
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }

    pub fn set_reuse_address(&mut self, reuse: bool) -> Result<(), i32> {
        let ret = ffi::UdpClientSetReuseAddress(self.cpp_context.pin_mut(), reuse);
        if ret != SUCCESS { Err(ret) } else { Ok(()) }
    }

    pub fn get_loopback_mode(&mut self) -> Result<bool, i32> {
        let mut loopback = false;
        let ret = ffi::UdpClientGetLoopbackMode(self.cpp_context.pin_mut(), &mut loopback);
        if ret != SUCCESS { Err(ret) } else { Ok(loopback) }
    }

    pub fn on_message_native(&mut self) -> Result<(), i32> {
        let callback_box = UdpSocketMessageBox::new(self);
        let ret = ffi::UdpSocketOnMessage(self.cpp_context.pin_mut(), callback_box);
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }

    pub fn off_message_native(&mut self) -> Result<(), i32> {
        let ret = ffi::UdpSocketOffMessage(self.cpp_context.pin_mut());
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }

    pub fn on_listening_native(&mut self) -> Result<(), i32> {
        let callback_box = UdpSocketListeningBox::new(self);
        let ret = ffi::UdpSocketOnListening(self.cpp_context.pin_mut(), callback_box);
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }

    pub fn off_listening_native(&mut self) -> Result<(), i32> {
        let ret = ffi::UdpSocketOffListening(self.cpp_context.pin_mut());
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }

    pub fn on_close_native(&mut self) -> Result<(), i32> {
        let callback_box = UdpSocketCloseBox::new(self);
        let ret = ffi::UdpSocketOnClose(self.cpp_context.pin_mut(), callback_box);
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }
    
    pub fn off_close_native(&mut self) -> Result<(), i32> {
        let ret = ffi::UdpSocketOffClose(self.cpp_context.pin_mut());
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }

    pub fn on_error_native(&mut self) -> Result<(), i32> {
        let callback_box = UdpSocketErrorBox::new(self);
        let ret = ffi::UdpSocketOnError(self.cpp_context.pin_mut(), callback_box);
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }

    pub fn off_error_native(&mut self) -> Result<(), i32> {
        let ret = ffi::UdpSocketOffError(self.cpp_context.pin_mut());
        if ret != SUCCESS {
            return Err(ret);
        }
        Ok(())
    }
}

#[ani_rs::native]
pub fn construct_udp_socket_instance<'local>(env: &AniEnv<'local>) -> Result<AniRef<'local>, BusinessError> {
    info!("Constructing UDPSocketClient instance");
    static UDP_SOCKET_CLASS: &CStr = unsafe {
        CStr::from_bytes_with_nul_unchecked(b"@ohos.net.socket.socket.UDPSocketInner\0")
    };
    static CTOR_SIGNATURE: &CStr = unsafe { CStr::from_bytes_with_nul_unchecked(b"l:\0") };
    let wrapper = UdpSocketWrapper::new(false);
    let ptr = Box::into_raw(Box::new(Mutex::new(wrapper))) as i64;
    let class = env.find_class(UDP_SOCKET_CLASS).unwrap();
    let obj = env
        .new_object_with_signature(&class, CTOR_SIGNATURE, (ptr,))
        .unwrap();
    Ok(obj.into())
}

#[ani_rs::native]
pub(crate) fn destroy_udp_socket_instance(ptr: i64) -> Result<(), BusinessError> {
    if ptr == 0 {
        return Ok(());
    }
    let _ = unsafe { Box::from_raw(ptr as *mut Mutex<UdpSocketWrapper>) };
    Ok(())
}

#[ani_rs::native]
pub(crate) fn bind_sync(this: bridge::UDPSocketClient, address: bridge::NetAddress) -> Result<(), BusinessError> {
    info!("UDPSocketClient bind_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.bind(&address).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_local_address_sync(this: bridge::UDPSocketClient) -> Result<bridge::NetAddress, BusinessError> {
    info!("UDPSocketClient get_local_address_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_local_address().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn send_sync(this: bridge::UDPSocketClient, options: bridge::UDPSendOptions) -> Result<(), BusinessError> {
    info!("UDPSocketClient send_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.send(&options).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn close_sync(this: bridge::UDPSocketClient) -> Result<(), BusinessError> {
    info!("UDPSocketClient close_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.close().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_state_sync(this: bridge::UDPSocketClient) -> Result<bridge::SocketStateBase, BusinessError> {
    info!("UDPSocketClient get_state_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_state().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn set_extra_options_sync(this: bridge::UDPSocketClient, options: bridge::UDPExtraOptions) -> Result<(), BusinessError> {
    info!("UDPSocketClient set_extra_options_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.set_extra_options(&options).map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn get_socket_fd_sync(this: bridge::UDPSocketClient) -> Result<i32, BusinessError> {
    info!("UDPSocketClient get_socket_fd_sync");
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.get_socket_fd().map_err(|e| bridge::convert_to_business_error(e))
}

#[ani_rs::native]
pub(crate) fn on_message_inner(env: &AniEnv, this: bridge::UDPSocketClient, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_message = Some(callback.into_global_callback(env).unwrap());
    socket.on_message_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn on_listening_inner(env: &AniEnv, this: bridge::UDPSocketClient, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_listening = Some(callback.into_global_callback(env).unwrap());
    socket.on_listening_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn on_close_inner(env: &AniEnv, this: bridge::UDPSocketClient, callback: AniFnObject) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_close = Some(callback.into_global_callback(env).unwrap());
    socket.on_close_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn on_error_inner(env: &AniEnv, this: bridge::UDPSocketClient, error_callback: AniErrorCallback) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_error = Some(error_callback.into_global_callback(env).unwrap());
    socket.on_error_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_message_inner(env: &AniEnv, this: bridge::UDPSocketClient) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_message = None;
    socket.off_message_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_listening_inner(env: &AniEnv, this: bridge::UDPSocketClient) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_listening = None;
    socket.off_listening_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_close_inner(env: &AniEnv, this: bridge::UDPSocketClient) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_close = None;
    socket.off_close_native();
    Ok(())
}

#[ani_rs::native]
pub(crate) fn off_error_inner(env: &AniEnv, this: bridge::UDPSocketClient) -> Result<(), BusinessError> {
    let mutex = unsafe { &*(this.native_ptr as *const Mutex<UdpSocketWrapper>) };
    let mut socket = mutex.lock().unwrap();
    socket.on_error = None;
    socket.off_error_native();
    Ok(())
}

pub struct UdpSocketMessageBox {
    pub on_message: Option<GlobalRefCallback<(bridge::SocketMessageInfo,)>>,
}

impl UdpSocketMessageBox {
    pub fn new(client_wrapper: &mut UdpSocketWrapper) -> Box<Self> {
        Box::new(UdpSocketMessageBox { on_message: client_wrapper.on_message.take() })
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
        } else {
            return -1;
        }
    }
}

pub struct UdpSocketListeningBox {
    pub on_listening: Option<GlobalRefCallback<()>>,
}

impl UdpSocketListeningBox {
    pub fn new(client_wrapper: &mut UdpSocketWrapper) -> Box<Self> {
        Box::new(UdpSocketListeningBox { on_listening: client_wrapper.on_listening.take() })
    }

    pub fn on_listening(&self) -> i32 {
        if let Some(callback) = &self.on_listening {
            callback.execute(());
            return 0;
        }
        return -1;
    }
}

pub struct UdpSocketCloseBox {
    pub on_close: Option<GlobalRefCallback<()>>,
}

impl UdpSocketCloseBox {
    pub fn new(client_wrapper: &mut UdpSocketWrapper) -> Box<Self> {
        Box::new(UdpSocketCloseBox { on_close: client_wrapper.on_close.take() })
    }

    pub fn on_close(&self) -> i32 {
        if let Some(callback) = &self.on_close {
            callback.execute(());
            return 0;
        }
        return -1;
    }
}

pub struct UdpSocketErrorBox {
    pub on_error: Option<GlobalRefErrorCallback>,
}

impl UdpSocketErrorBox {
    pub fn new(client_wrapper: &mut UdpSocketWrapper) -> Box<Self> {
        Box::new(UdpSocketErrorBox { on_error: client_wrapper.on_error.take() })
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
