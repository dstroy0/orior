// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! A TLS client over a connected socket, through the system's own TLS: Schannel on Windows, Secure
//! Transport on macOS, and on Linux and the other Unix systems the system's libssl, found as it runs.
//! Nothing of TLS is linked from outside the system. The peer's certificate is checked by the system
//! against the roots it trusts and against the host's name, unless the caller asks for no check.
//! Schannel is given the credentials of TLS 1.2 and before, which every server still speaks; libssl
//! speaks TLS 1.3 as well.

use std::io::{Read, Write};
use std::net::TcpStream;

/// A TLS session over a socket: what is written to it goes out sealed, and each read opens what came.
pub struct Tls {
    inner: imp::Session,
}

impl Tls {
    /// Opens TLS to `host` over `socket`, the peer's certificate checked where `verify`.
    pub fn connect(socket: TcpStream, host: &str, verify: bool) -> Result<Tls, String> {
        imp::Session::connect(socket, host, verify).map(|inner| Tls { inner })
    }
}

impl Read for Tls {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        self.inner.read(buf)
    }
}

impl Write for Tls {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        self.inner.write(buf)
    }

    fn flush(&mut self) -> std::io::Result<()> {
        Ok(())
    }
}

#[cfg(windows)]
mod imp {
    use std::ffi::c_void;
    use std::io::{Read, Write};
    use std::net::TcpStream;
    use std::ptr::{null, null_mut};

    type Status = i32;

    #[repr(C)]
    #[derive(Clone, Copy, Default)]
    struct Handle {
        lower: usize,
        upper: usize,
    }

    #[repr(C)]
    struct Buffer {
        size: u32,
        kind: u32,
        data: *mut c_void,
    }

    #[repr(C)]
    struct Buffers {
        version: u32,
        count: u32,
        buffers: *mut Buffer,
    }

    #[repr(C)]
    struct Credentials {
        version: u32,
        creds: u32,
        cred: *mut c_void,
        root_store: *mut c_void,
        mappers: u32,
        mapper: *mut c_void,
        algorithms: u32,
        algorithm: *mut u32,
        protocols: u32,
        minimum_strength: u32,
        maximum_strength: u32,
        lifespan: u32,
        flags: u32,
        format: u32,
    }

    #[repr(C)]
    #[derive(Default)]
    struct Sizes {
        header: u32,
        trailer: u32,
        most: u32,
        buffers: u32,
        block: u32,
    }

    #[link(name = "secur32")]
    unsafe extern "system" {
        fn AcquireCredentialsHandleW(principal: *const u16, package: *const u16, usage: u32, logon: *mut c_void, data: *mut c_void, key: *mut c_void, argument: *mut c_void, credential: *mut Handle, expiry: *mut i64) -> Status;
        fn InitializeSecurityContextW(credential: *mut Handle, context: *mut Handle, target: *const u16, wanted: u32, reserved: u32, representation: u32, input: *mut Buffers, reserved2: u32, made: *mut Handle, output: *mut Buffers, given: *mut u32, expiry: *mut i64) -> Status;
        fn DeleteSecurityContext(context: *mut Handle) -> Status;
        fn FreeCredentialsHandle(credential: *mut Handle) -> Status;
        fn FreeContextBuffer(buffer: *mut c_void) -> Status;
        fn QueryContextAttributesW(context: *mut Handle, attribute: u32, buffer: *mut c_void) -> Status;
        fn EncryptMessage(context: *mut Handle, quality: u32, message: *mut Buffers, sequence: u32) -> Status;
        fn DecryptMessage(context: *mut Handle, message: *mut Buffers, sequence: u32, quality: *mut u32) -> Status;
    }

    const OK: Status = 0;
    const CONTINUE_NEEDED: Status = 0x0009_0312;
    const INCOMPLETE_CREDENTIALS: Status = 0x0009_0320;
    const CONTEXT_EXPIRED: Status = 0x0009_0317;
    const RENEGOTIATE: Status = 0x0009_0321;
    const INCOMPLETE_MESSAGE: Status = 0x8009_0318_u32 as i32;

    const EMPTY: u32 = 0;
    const DATA: u32 = 1;
    const TOKEN: u32 = 2;
    const EXTRA: u32 = 5;
    const TRAILER: u32 = 6;
    const HEADER: u32 = 7;

    const OUTBOUND: u32 = 2;
    const CREDENTIALS_VERSION: u32 = 4;
    const AUTO_VALIDATION: u32 = 0x20;
    const MANUAL_VALIDATION: u32 = 0x8;
    const NO_DEFAULT_CREDS: u32 = 0x10;
    const STRONG_CRYPTO: u32 = 0x0040_0000;
    const WANTED: u32 = 0x4 | 0x8 | 0x10 | 0x100 | 0x4000 | 0x8000;
    const STREAM_SIZES: u32 = 4;

    fn wide(text: &str) -> Vec<u16> {
        text.encode_utf16().chain([0]).collect()
    }

    /// What a status Schannel gives says, in words where it is one a reader meets.
    fn said(status: Status) -> String {
        match status as u32 {
            0x8009_0325 => "the server's certificate is not signed by a root this system trusts".into(),
            0x8009_0322 => "the server's certificate names another host".into(),
            0x8009_0328 => "the server's certificate has expired".into(),
            0x8009_0331 => "the server and this system share no TLS version or cipher".into(),
            0x8009_0326 => "the server sent a message TLS does not allow".into(),
            0x8009_0327 => "the server's certificate could not be read".into(),
            other => format!("Schannel said 0x{other:08x}"),
        }
    }

    pub struct Session {
        socket: TcpStream,
        credential: Handle,
        context: Handle,
        sizes: Sizes,
        sealed: Vec<u8>,
        plain: Vec<u8>,
        ended: bool,
    }

    unsafe impl Send for Session {}

    impl Session {
        pub fn connect(socket: TcpStream, host: &str, verify: bool) -> Result<Session, String> {
            let mut session = Session { socket, credential: Handle::default(), context: Handle::default(), sizes: Sizes::default(), sealed: Vec::new(), plain: Vec::new(), ended: false };
            let mut credentials = Credentials {
                version: CREDENTIALS_VERSION,
                creds: 0,
                cred: null_mut(),
                root_store: null_mut(),
                mappers: 0,
                mapper: null_mut(),
                algorithms: 0,
                algorithm: null_mut(),
                protocols: 0,
                minimum_strength: 0,
                maximum_strength: 0,
                lifespan: 0,
                flags: NO_DEFAULT_CREDS | STRONG_CRYPTO | if verify { AUTO_VALIDATION } else { MANUAL_VALIDATION },
                format: 0,
            };
            let package = wide("Microsoft Unified Security Protocol Provider");
            let status = unsafe { AcquireCredentialsHandleW(null(), package.as_ptr(), OUTBOUND, null_mut(), (&raw mut credentials).cast(), null_mut(), null_mut(), &mut session.credential, null_mut()) };
            if status != OK {
                return Err(format!("TLS: {}", said(status)));
            }
            session.handshake(host)?;
            let status = unsafe { QueryContextAttributesW(&mut session.context, STREAM_SIZES, (&raw mut session.sizes).cast()) };
            if status != OK {
                return Err(format!("TLS: {}", said(status)));
            }
            Ok(session)
        }

        fn fill(&mut self) -> Result<usize, String> {
            let mut chunk = [0u8; 16 * 1024];
            let read = self.socket.read(&mut chunk).map_err(|error| format!("TLS: {error}"))?;
            self.sealed.extend_from_slice(&chunk[..read]);
            Ok(read)
        }

        fn handshake(&mut self, host: &str) -> Result<(), String> {
            let target = wide(host);
            let mut first = true;
            loop {
                let mut input = [Buffer { size: self.sealed.len() as u32, kind: TOKEN, data: self.sealed.as_mut_ptr().cast() }, Buffer { size: 0, kind: EMPTY, data: null_mut() }];
                let mut output = [Buffer { size: 0, kind: TOKEN, data: null_mut() }];
                let mut input_list = Buffers { version: 0, count: 2, buffers: input.as_mut_ptr() };
                let mut output_list = Buffers { version: 0, count: 1, buffers: output.as_mut_ptr() };
                let mut given = 0u32;
                let status = unsafe {
                    InitializeSecurityContextW(
                        &mut self.credential,
                        if first { null_mut() } else { &mut self.context },
                        target.as_ptr(),
                        WANTED,
                        0,
                        0,
                        if first { null_mut() } else { &mut input_list },
                        0,
                        &mut self.context,
                        &mut output_list,
                        &mut given,
                        null_mut(),
                    )
                };
                first = false;
                if !output[0].data.is_null() {
                    if output[0].size > 0 {
                        let token = unsafe { std::slice::from_raw_parts(output[0].data as *const u8, output[0].size as usize) };
                        let sent = self.socket.write_all(token);
                        unsafe { FreeContextBuffer(output[0].data) };
                        sent.map_err(|error| format!("TLS: {error}"))?;
                    } else {
                        unsafe { FreeContextBuffer(output[0].data) };
                    }
                }
                let extra = (input[1].kind == EXTRA).then_some(input[1].size as usize);
                match status {
                    OK => {
                        let keep = extra.unwrap_or(0);
                        self.sealed.drain(..self.sealed.len() - keep);
                        return Ok(());
                    }
                    INCOMPLETE_CREDENTIALS => {}
                    CONTINUE_NEEDED => {
                        let keep = extra.unwrap_or(0);
                        self.sealed.drain(..self.sealed.len() - keep);
                        if self.sealed.is_empty() && self.fill()? == 0 {
                            return Err("TLS: the server closed the connection in the handshake".into());
                        }
                    }
                    INCOMPLETE_MESSAGE => {
                        if self.fill()? == 0 {
                            return Err("TLS: the server closed the connection in the handshake".into());
                        }
                    }
                    other => return Err(format!("TLS: {}", said(other))),
                }
            }
        }

        pub fn read(&mut self, out: &mut [u8]) -> std::io::Result<usize> {
            while self.plain.is_empty() && !self.ended {
                if self.sealed.is_empty() && self.fill().map_err(std::io::Error::other)? == 0 {
                    self.ended = true;
                    break;
                }
                let mut buffers = [
                    Buffer { size: self.sealed.len() as u32, kind: DATA, data: self.sealed.as_mut_ptr().cast() },
                    Buffer { size: 0, kind: EMPTY, data: null_mut() },
                    Buffer { size: 0, kind: EMPTY, data: null_mut() },
                    Buffer { size: 0, kind: EMPTY, data: null_mut() },
                ];
                let mut list = Buffers { version: 0, count: 4, buffers: buffers.as_mut_ptr() };
                let status = unsafe { DecryptMessage(&mut self.context, &mut list, 0, null_mut()) };
                match status {
                    OK | RENEGOTIATE => {
                        for buffer in &buffers {
                            if buffer.kind == DATA && buffer.size > 0 {
                                self.plain.extend_from_slice(unsafe { std::slice::from_raw_parts(buffer.data as *const u8, buffer.size as usize) });
                            }
                        }
                        let extra = buffers.iter().find(|buffer| buffer.kind == EXTRA).map(|buffer| buffer.size as usize).unwrap_or(0);
                        self.sealed.drain(..self.sealed.len() - extra);
                        if status == RENEGOTIATE && self.plain.is_empty() {
                            return Err(std::io::Error::other("TLS: the server asked to renegotiate"));
                        }
                    }
                    INCOMPLETE_MESSAGE => {
                        if self.fill().map_err(std::io::Error::other)? == 0 {
                            self.ended = true;
                        }
                    }
                    CONTEXT_EXPIRED => self.ended = true,
                    other => return Err(std::io::Error::other(format!("TLS: {}", said(other)))),
                }
            }
            let taken = out.len().min(self.plain.len());
            out[..taken].copy_from_slice(&self.plain[..taken]);
            self.plain.drain(..taken);
            Ok(taken)
        }

        pub fn write(&mut self, data: &[u8]) -> std::io::Result<usize> {
            let part = &data[..data.len().min(self.sizes.most as usize)];
            let (header, trailer) = (self.sizes.header as usize, self.sizes.trailer as usize);
            let mut message = vec![0u8; header + part.len() + trailer];
            message[header..header + part.len()].copy_from_slice(part);
            let base = message.as_mut_ptr();
            let mut buffers = [
                Buffer { size: header as u32, kind: HEADER, data: base.cast() },
                Buffer { size: part.len() as u32, kind: DATA, data: unsafe { base.add(header) }.cast() },
                Buffer { size: trailer as u32, kind: TRAILER, data: unsafe { base.add(header + part.len()) }.cast() },
                Buffer { size: 0, kind: EMPTY, data: null_mut() },
            ];
            let mut list = Buffers { version: 0, count: 4, buffers: buffers.as_mut_ptr() };
            let status = unsafe { EncryptMessage(&mut self.context, 0, &mut list, 0) };
            if status != OK {
                return Err(std::io::Error::other(format!("TLS: {}", said(status))));
            }
            let length = (buffers[0].size + buffers[1].size + buffers[2].size) as usize;
            self.socket.write_all(&message[..length])?;
            Ok(part.len())
        }
    }

    impl Drop for Session {
        fn drop(&mut self) {
            unsafe {
                DeleteSecurityContext(&mut self.context);
                FreeCredentialsHandle(&mut self.credential);
            }
        }
    }
}

#[cfg(target_os = "macos")]
mod imp {
    use std::ffi::{c_char, c_void};
    use std::io::{Read, Write};
    use std::net::TcpStream;

    type Status = i32;
    type Context = *mut c_void;
    type Reader = extern "C" fn(*const c_void, *mut c_void, *mut usize) -> Status;
    type Writer = extern "C" fn(*const c_void, *const c_void, *mut usize) -> Status;

    #[link(name = "Security", kind = "framework")]
    unsafe extern "C" {
        fn SSLCreateContext(allocator: *const c_void, side: u32, kind: u32) -> Context;
        fn SSLSetIOFuncs(context: Context, read: Reader, write: Writer) -> Status;
        fn SSLSetConnection(context: Context, connection: *const c_void) -> Status;
        fn SSLSetPeerDomainName(context: Context, name: *const c_char, length: usize) -> Status;
        fn SSLSetSessionOption(context: Context, option: u32, value: u8) -> Status;
        fn SSLHandshake(context: Context) -> Status;
        fn SSLRead(context: Context, data: *mut c_void, length: usize, processed: *mut usize) -> Status;
        fn SSLWrite(context: Context, data: *const c_void, length: usize, processed: *mut usize) -> Status;
        fn SSLClose(context: Context) -> Status;
    }

    #[link(name = "CoreFoundation", kind = "framework")]
    unsafe extern "C" {
        fn CFRelease(object: *const c_void);
    }

    const WOULD_BLOCK: Status = -9803;
    const CLOSED_GRACEFUL: Status = -9805;
    const CLOSED_ABORT: Status = -9806;
    const SERVER_AUTH_COMPLETED: Status = -9841;

    extern "C" fn read_from(connection: *const c_void, data: *mut c_void, length: *mut usize) -> Status {
        let socket = unsafe { &mut *(connection as *mut TcpStream) };
        let wanted = unsafe { *length };
        let buffer = unsafe { std::slice::from_raw_parts_mut(data as *mut u8, wanted) };
        let mut got = 0;
        let status = loop {
            if got == wanted {
                break 0;
            }
            match socket.read(&mut buffer[got..]) {
                Ok(0) => break CLOSED_GRACEFUL,
                Ok(read) => got += read,
                Err(error) if matches!(error.kind(), std::io::ErrorKind::WouldBlock | std::io::ErrorKind::TimedOut) => break WOULD_BLOCK,
                Err(_) => break CLOSED_ABORT,
            }
        };
        unsafe { *length = got };
        status
    }

    extern "C" fn write_to(connection: *const c_void, data: *const c_void, length: *mut usize) -> Status {
        let socket = unsafe { &mut *(connection as *mut TcpStream) };
        let buffer = unsafe { std::slice::from_raw_parts(data as *const u8, *length) };
        match socket.write_all(buffer) {
            Ok(()) => 0,
            Err(_) => {
                unsafe { *length = 0 };
                CLOSED_ABORT
            }
        }
    }

    pub struct Session {
        context: Context,
        socket: Box<TcpStream>,
    }

    unsafe impl Send for Session {}

    impl Session {
        pub fn connect(socket: TcpStream, host: &str, verify: bool) -> Result<Session, String> {
            let socket = Box::new(socket);
            let context = unsafe { SSLCreateContext(std::ptr::null(), 1, 0) };
            if context.is_null() {
                return Err("TLS: Secure Transport made no session".into());
            }
            let session = Session { context, socket };
            unsafe {
                SSLSetIOFuncs(context, read_from, write_to);
                SSLSetConnection(context, (&*session.socket as *const TcpStream).cast());
                SSLSetPeerDomainName(context, host.as_ptr().cast(), host.len());
                if !verify {
                    SSLSetSessionOption(context, 0, 1);
                }
            }
            loop {
                match unsafe { SSLHandshake(context) } {
                    0 => return Ok(session),
                    SERVER_AUTH_COMPLETED if !verify => continue,
                    WOULD_BLOCK => return Err("TLS: the server took too long in the handshake".into()),
                    status => return Err(format!("TLS: Secure Transport said {status}")),
                }
            }
        }

        pub fn read(&mut self, out: &mut [u8]) -> std::io::Result<usize> {
            let mut processed = 0;
            match unsafe { SSLRead(self.context, out.as_mut_ptr().cast(), out.len(), &mut processed) } {
                0 | CLOSED_GRACEFUL | CLOSED_ABORT => Ok(processed),
                WOULD_BLOCK if processed > 0 => Ok(processed),
                WOULD_BLOCK => Err(std::io::ErrorKind::TimedOut.into()),
                status => Err(std::io::Error::other(format!("TLS: Secure Transport said {status}"))),
            }
        }

        pub fn write(&mut self, data: &[u8]) -> std::io::Result<usize> {
            let mut processed = 0;
            match unsafe { SSLWrite(self.context, data.as_ptr().cast(), data.len(), &mut processed) } {
                0 => Ok(processed),
                status => Err(std::io::Error::other(format!("TLS: Secure Transport said {status}"))),
            }
        }
    }

    impl Drop for Session {
        fn drop(&mut self) {
            unsafe {
                SSLClose(self.context);
                CFRelease(self.context);
            }
        }
    }
}

#[cfg(all(unix, not(target_os = "macos")))]
mod imp {
    use std::ffi::{CString, c_char, c_int, c_long, c_void};
    use std::net::TcpStream;
    use std::os::fd::AsRawFd;
    use std::sync::OnceLock;

    #[cfg_attr(target_os = "linux", link(name = "dl"))]
    unsafe extern "C" {
        fn dlopen(file: *const c_char, mode: c_int) -> *mut c_void;
        fn dlsym(handle: *mut c_void, name: *const c_char) -> *mut c_void;
    }

    const NOW: c_int = 2;
    const SET_HOST_NAME: c_int = 55;
    const IGNORE_UNEXPECTED_EOF: u64 = 1 << 7;
    const ERROR_WANT_READ: c_int = 2;
    const ERROR_WANT_WRITE: c_int = 3;
    const ERROR_SYSCALL: c_int = 5;
    const ERROR_ZERO_RETURN: c_int = 6;

    /// The calls of libssl orior makes, found in the system's libssl as it runs.
    struct Ssl {
        init: unsafe extern "C" fn(u64, *const c_void) -> c_int,
        client_method: unsafe extern "C" fn() -> *const c_void,
        context_new: unsafe extern "C" fn(*const c_void) -> *mut c_void,
        context_free: unsafe extern "C" fn(*mut c_void),
        default_paths: unsafe extern "C" fn(*mut c_void) -> c_int,
        set_verify: unsafe extern "C" fn(*mut c_void, c_int, *const c_void),
        set_options: Option<unsafe extern "C" fn(*mut c_void, u64) -> u64>,
        new: unsafe extern "C" fn(*mut c_void) -> *mut c_void,
        free: unsafe extern "C" fn(*mut c_void),
        set_fd: unsafe extern "C" fn(*mut c_void, c_int) -> c_int,
        control: unsafe extern "C" fn(*mut c_void, c_int, c_long, *mut c_void) -> c_long,
        set_host: unsafe extern "C" fn(*mut c_void, *const c_char) -> c_int,
        connect: unsafe extern "C" fn(*mut c_void) -> c_int,
        error: unsafe extern "C" fn(*const c_void, c_int) -> c_int,
        read: unsafe extern "C" fn(*mut c_void, *mut c_void, c_int) -> c_int,
        write: unsafe extern "C" fn(*mut c_void, *const c_void, c_int) -> c_int,
        shutdown: unsafe extern "C" fn(*mut c_void) -> c_int,
        verify_result: unsafe extern "C" fn(*const c_void) -> c_long,
        verify_text: Option<unsafe extern "C" fn(c_long) -> *const c_char>,
    }

    static SSL: OnceLock<Result<Ssl, String>> = OnceLock::new();

    fn library() -> Result<&'static Ssl, String> {
        SSL.get_or_init(|| unsafe { load() }).as_ref().map_err(Clone::clone)
    }

    unsafe fn load() -> Result<Ssl, String> {
        let handle = ["libssl.so.3", "libssl.so.1.1", "libssl.so"]
            .iter()
            .filter_map(|name| CString::new(*name).ok())
            .map(|name| unsafe { dlopen(name.as_ptr(), NOW) })
            .find(|handle| !handle.is_null())
            .ok_or("no libssl is installed, and TLS on this system goes through it")?;
        let find = |name: &str| -> *mut c_void { CString::new(name).map(|name| unsafe { dlsym(handle, name.as_ptr()) }).unwrap_or(std::ptr::null_mut()) };
        macro_rules! needed {
            ($name:literal) => {{
                let found = find($name);
                if found.is_null() {
                    return Err(format!("the system's libssl has no {}", $name));
                }
                unsafe { std::mem::transmute::<*mut c_void, _>(found) }
            }};
        }
        macro_rules! wanted {
            ($name:literal) => {{
                let found = find($name);
                (!found.is_null()).then(|| unsafe { std::mem::transmute::<*mut c_void, _>(found) })
            }};
        }
        Ok(Ssl {
            init: needed!("OPENSSL_init_ssl"),
            client_method: needed!("TLS_client_method"),
            context_new: needed!("SSL_CTX_new"),
            context_free: needed!("SSL_CTX_free"),
            default_paths: needed!("SSL_CTX_set_default_verify_paths"),
            set_verify: needed!("SSL_CTX_set_verify"),
            set_options: wanted!("SSL_CTX_set_options"),
            new: needed!("SSL_new"),
            free: needed!("SSL_free"),
            set_fd: needed!("SSL_set_fd"),
            control: needed!("SSL_ctrl"),
            set_host: needed!("SSL_set1_host"),
            connect: needed!("SSL_connect"),
            error: needed!("SSL_get_error"),
            read: needed!("SSL_read"),
            write: needed!("SSL_write"),
            shutdown: needed!("SSL_shutdown"),
            verify_result: needed!("SSL_get_verify_result"),
            verify_text: wanted!("X509_verify_cert_error_string"),
        })
    }

    pub struct Session {
        lib: &'static Ssl,
        context: *mut c_void,
        ssl: *mut c_void,
        _socket: TcpStream,
    }

    unsafe impl Send for Session {}

    impl Session {
        pub fn connect(socket: TcpStream, host: &str, verify: bool) -> Result<Session, String> {
            let lib = library()?;
            let name = CString::new(host).map_err(|_| format!("{host} is not a host's name"))?;
            unsafe {
                (lib.init)(0, std::ptr::null());
                let context = (lib.context_new)((lib.client_method)());
                if context.is_null() {
                    return Err("TLS: libssl made no context".into());
                }
                if let Some(set_options) = lib.set_options {
                    set_options(context, IGNORE_UNEXPECTED_EOF);
                }
                if verify {
                    (lib.default_paths)(context);
                }
                (lib.set_verify)(context, i32::from(verify), std::ptr::null());
                let ssl = (lib.new)(context);
                if ssl.is_null() {
                    (lib.context_free)(context);
                    return Err("TLS: libssl made no session".into());
                }
                let session = Session { lib, context, ssl, _socket: socket };
                (lib.set_fd)(ssl, session._socket.as_raw_fd());
                if host.parse::<std::net::IpAddr>().is_err() {
                    (lib.control)(ssl, SET_HOST_NAME, 0, name.as_ptr() as *mut c_void);
                }
                if verify {
                    (lib.set_host)(ssl, name.as_ptr());
                }
                if (lib.connect)(ssl) != 1 {
                    let code = (lib.verify_result)(ssl);
                    let why = match (code, lib.verify_text) {
                        (0, _) => "the handshake failed".to_string(),
                        (code, Some(text)) => std::ffi::CStr::from_ptr(text(code)).to_string_lossy().into_owned(),
                        (code, None) => format!("the certificate did not check ({code})"),
                    };
                    return Err(format!("TLS: {why}"));
                }
                Ok(session)
            }
        }

        fn failed(&self, returned: c_int) -> Option<std::io::Result<usize>> {
            match unsafe { (self.lib.error)(self.ssl, returned) } {
                ERROR_WANT_READ | ERROR_WANT_WRITE => None,
                ERROR_ZERO_RETURN => Some(Ok(0)),
                ERROR_SYSCALL if returned == 0 => Some(Ok(0)),
                ERROR_SYSCALL => Some(Err(std::io::Error::last_os_error())),
                code => Some(Err(std::io::Error::other(format!("TLS: libssl said {code}")))),
            }
        }

        pub fn read(&mut self, out: &mut [u8]) -> std::io::Result<usize> {
            loop {
                let read = unsafe { (self.lib.read)(self.ssl, out.as_mut_ptr().cast(), out.len().min(i32::MAX as usize) as c_int) };
                if read > 0 {
                    return Ok(read as usize);
                }
                if let Some(result) = self.failed(read) {
                    return result;
                }
            }
        }

        pub fn write(&mut self, data: &[u8]) -> std::io::Result<usize> {
            loop {
                let written = unsafe { (self.lib.write)(self.ssl, data.as_ptr().cast(), data.len().min(i32::MAX as usize) as c_int) };
                if written > 0 {
                    return Ok(written as usize);
                }
                if let Some(result) = self.failed(written) {
                    return result;
                }
            }
        }
    }

    impl Drop for Session {
        fn drop(&mut self) {
            unsafe {
                (self.lib.shutdown)(self.ssl);
                (self.lib.free)(self.ssl);
                (self.lib.context_free)(self.context);
            }
        }
    }
}
