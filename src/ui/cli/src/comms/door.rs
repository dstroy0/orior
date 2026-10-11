// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The comms layer reached from another process: a named pipe of the container's own on Windows, which
//! takes no one from another machine, and elsewhere a socket in a folder only the reader can open.
//!
//! A frame is the length of its JSON in four bytes, least first, then the JSON. A process joins by its
//! first frame, {"key": its key}; the container takes it only where the key is a part's and the
//! key was given to the process at the other end, as the system says, or to a process it stands
//! under, as a shell of the window's is given a key for every program run in it. Then the process
//! asks {"listen": channel} and {"say": channel, "body": what}, each answered with {"ok": what was
//! done} or {"refused": why}, and each message said on a channel it listens on comes as
//! {"message": {"channel", "from", "body"}}.
//!
//! A body longer than the transfer layer's SMALL goes through it: the frame holds {"transfer":
//! {"length"}} where the body would be, and the region handed over with the frame holds the body's
//! JSON. On Windows the transfer names the region's handle too, the one the speaking process has, which
//! the container takes from it, or the one the container made in the listening process.

use std::io::{Read, Write};
use std::sync::mpsc::{self, Receiver, Sender};
use std::sync::{Arc, Mutex};
use std::time::Duration;

use serde_json::{json, Value};

use super::{Comms, Key, Message};
use crate::transfer::{self, Region};

#[cfg(unix)]
use crate::transfer::Fds;

/// The descriptors that come with the bytes read, which only a socket carries.
#[cfg(windows)]
#[derive(Default)]
struct Fds;

/// The longest frame taken, in bytes; a longer body goes through the transfer layer.
const MOST: usize = 16 << 20;

fn frame_of(value: &Value) -> Vec<u8> {
    let data = value.to_string().into_bytes();
    let mut frame = (data.len() as u32).to_le_bytes().to_vec();
    frame.extend_from_slice(&data);
    frame
}

fn write_frame<W: Write + ?Sized>(out: &mut W, value: &Value) -> std::io::Result<()> {
    out.write_all(&frame_of(value))?;
    out.flush()
}

fn read_frame(input: &mut impl Read) -> std::io::Result<Value> {
    let mut length = [0u8; 4];
    input.read_exact(&mut length)?;
    let length = u32::from_le_bytes(length) as usize;
    if length > MOST {
        return Err(std::io::Error::new(std::io::ErrorKind::InvalidData, format!("a frame of {length} bytes is past the most a frame takes")));
    }
    let mut data = vec![0u8; length];
    input.read_exact(&mut data)?;
    serde_json::from_slice(&data).map_err(|error| std::io::Error::new(std::io::ErrorKind::InvalidData, error))
}

/// The writing half of a connection, which hands a region over with a frame.
trait Outlet: Write + Send {
    /// Writes the whole of `frame`, the region `region` going to the other end with it.
    fn write_with(&mut self, frame: &[u8], region: &Region) -> std::io::Result<()>;
}

/// What a frame says of the region it hands over: its length, and on Windows its handle in the
/// process the frame goes to, which the container makes there where `to` names that process.
fn handed(region: &Region, to: Option<u32>) -> Result<Value, String> {
    #[cfg(windows)]
    {
        let handle = match to {
            Some(process) => region.give(process)?,
            None => region.handle(),
        };
        Ok(json!({"length": region.len(), "handle": handle}))
    }
    #[cfg(unix)]
    {
        let _ = to;
        Ok(json!({"length": region.len()}))
    }
}

/// The region a frame hands over as `transfer` says: on Windows taken from the process `from` where
/// the container takes it, else the handle the container made here; elsewhere the next descriptor
/// that came with the bytes read.
fn handed_over(transfer: &Value, from: Option<u32>, fds: &Fds) -> Result<Region, String> {
    let length = transfer["length"].as_u64().ok_or("a transfer names its length")? as usize;
    #[cfg(windows)]
    {
        let _ = fds;
        let handle = transfer["handle"].as_u64().ok_or("a transfer names its handle")?;
        match from {
            Some(process) => Region::take(process, handle, length),
            None => Region::given(handle, length),
        }
    }
    #[cfg(unix)]
    {
        let _ = from;
        let fd = fds.take().ok_or("a transfer came with no region")?;
        Region::received(fd, length)
    }
}

/// The body a region holds.
fn body_of(region: &Region) -> Result<Value, String> {
    serde_json::from_slice(region.bytes()).map_err(|error| format!("the region holds no JSON: {error}"))
}

/// Tells the process `peer` of `message`, through the transfer layer where its body is long, and
/// gives whether it went.
fn tell(out: &mut dyn Outlet, message: &Message, peer: Option<u32>, comms: &Comms) -> bool {
    let body = message.body.to_string();
    if body.len() <= transfer::SMALL {
        return write_frame(out, &json!({"message": message})).is_ok();
    }
    let sent = Region::with(body.as_bytes()).and_then(|region| {
        let frame = json!({"message": {"channel": message.channel, "from": message.from}, "transfer": handed(&region, peer)?});
        out.write_with(&frame_of(&frame), &region).map_err(|error| error.to_string())
    });
    if sent.is_ok() {
        comms.transferred(&message.channel, body.len() as u64);
    }
    sent.is_ok()
}

/// One end of a connection: its reading half, its writing half, the process at the other end, and
/// the descriptors that came with what was read.
struct Joined {
    input: Box<dyn Read + Send>,
    output: Box<dyn Outlet>,
    peer: Option<u32>,
    fds: Arc<Fds>,
}

/// Opens the door to `comms`, and gives the address a process joins at.
pub fn open(comms: Arc<Comms>) -> Result<String, String> {
    let (address, accept) = listener()?;
    std::thread::spawn(move || loop {
        match accept() {
            Ok(joined) => {
                let comms = comms.clone();
                std::thread::spawn(move || serve(comms, joined));
            }
            Err(_) => std::thread::sleep(Duration::from_millis(100)),
        }
    });
    Ok(address)
}

fn serve(comms: Arc<Comms>, joined: Joined) {
    let Joined { mut input, output, peer, fds } = joined;
    let output = Arc::new(Mutex::new(output));
    let answer = |value: Value| output.lock().map(|mut out| write_frame(&mut **out, &value).is_ok()).unwrap_or(false);
    let Ok(hello) = read_frame(&mut input) else { return };
    let key = Key::from_text(hello["key"].as_str().unwrap_or(""));
    let Some(name) = comms.name_of(&key) else {
        answer(json!({"refused": "that key is no part's"}));
        return;
    };
    let Some(given) = comms.process_of(&key) else {
        answer(json!({"refused": format!("the key of {name} was given to no process")}));
        return;
    };
    if !peer.is_some_and(|peer| peer == given || crate::container::under(given, peer, &crate::container::parents())) {
        answer(json!({"refused": format!("the key of {name} was given to another process")}));
        return;
    }
    if !answer(json!({"ok": "joined", "name": name})) {
        return;
    }
    while let Ok(asked) = read_frame(&mut input) {
        if let Some(channel) = asked["listen"].as_str() {
            match comms.listen(&key, channel) {
                Ok(heard) => {
                    let to = output.clone();
                    let counted = comms.clone();
                    std::thread::spawn(move || {
                        for message in heard {
                            let sent = to.lock().map(|mut out| tell(&mut **out, &message, peer, &counted)).unwrap_or(false);
                            if !sent {
                                break;
                            }
                        }
                    });
                    answer(json!({"ok": "listen", "channel": channel}));
                }
                Err(why) => {
                    answer(json!({"refused": why}));
                }
            }
        } else if let Some(channel) = asked["say"].as_str() {
            let body = match asked.get("transfer") {
                None => Ok((asked["body"].clone(), 0)),
                Some(transfer) => handed_over(transfer, peer, &fds).and_then(|region| Ok((body_of(&region)?, region.len()))),
            };
            match body.and_then(|(body, moved)| Ok((comms.say(&key, channel, body)?, moved))) {
                Ok((heard, moved)) => {
                    if moved > 0 {
                        comms.transferred(channel, moved as u64);
                    }
                    answer(json!({"ok": "say", "heard": heard}))
                }
                Err(why) => answer(json!({"refused": why})),
            };
        } else {
            answer(json!({"refused": "a frame asks to listen or to say"}));
        }
    }
}

/// A process's end of the door: what it joined as, the answers to what it asks, and the messages on
/// the channels it listens on.
pub struct Client {
    output: Mutex<Box<dyn Outlet>>,
    answers: Mutex<Receiver<Value>>,
    messages: Mutex<Receiver<Message>>,
    pub name: String,
}

/// The variables a process of the container's is started with: the door's address, and its key.
pub const ADDRESS_VARIABLE: &str = "ORIOR_COMMS";
pub const KEY_VARIABLE: &str = "ORIOR_KEY";

impl Client {
    /// Joins the door ADDRESS_VARIABLE names with the key KEY_VARIABLE holds, where this process was
    /// started with both.
    pub fn from_env() -> Option<Client> {
        let address = std::env::var(ADDRESS_VARIABLE).ok()?;
        let key = std::env::var(KEY_VARIABLE).ok()?;
        Client::join(&address, &Key::from_text(&key)).ok()
    }

    /// Joins the door at `address` with `key`.
    pub fn join(address: &str, key: &Key) -> Result<Client, String> {
        let Joined { mut input, mut output, fds, .. } = connect(address)?;
        write_frame(&mut *output, &json!({"key": key.text()})).map_err(|error| error.to_string())?;
        let hello = read_frame(&mut input).map_err(|error| error.to_string())?;
        if let Some(why) = hello["refused"].as_str() {
            return Err(why.to_string());
        }
        let name = hello["name"].as_str().unwrap_or("").to_string();
        let (tell_answer, answers): (Sender<Value>, Receiver<Value>) = mpsc::channel();
        let (tell_message, messages) = mpsc::channel();
        std::thread::spawn(move || {
            while let Ok(frame) = read_frame(&mut input) {
                if let Some(message) = frame.get("message") {
                    let body = match frame.get("transfer") {
                        None => Ok(message["body"].clone()),
                        Some(transfer) => handed_over(transfer, None, &fds).and_then(|region| body_of(&region)),
                    };
                    let Ok(body) = body else { continue };
                    let message = Message { channel: message["channel"].as_str().unwrap_or("").into(), from: message["from"].as_str().unwrap_or("").into(), body };
                    if tell_message.send(message).is_err() {
                        break;
                    }
                } else if tell_answer.send(frame).is_err() {
                    break;
                }
            }
        });
        Ok(Client { output: Mutex::new(output), answers: Mutex::new(answers), messages: Mutex::new(messages), name })
    }

    /// Asks `asked`, the region `region` going with it where there is one, and gives the answer. The
    /// region is held until the answer comes, by which time the container has read it.
    fn ask(&self, asked: Value, region: Option<&Region>) -> Result<Value, String> {
        let mut output = self.output.lock().map_err(|_| "the door is held")?;
        let wrote = match region {
            Some(region) => output.write_with(&frame_of(&asked), region),
            None => write_frame(&mut **output, &asked),
        };
        wrote.map_err(|error| error.to_string())?;
        let answer = self.answers.lock().map_err(|_| "the door is held")?.recv_timeout(Duration::from_secs(10)).map_err(|_| "the container gave no answer".to_string())?;
        match answer["refused"].as_str() {
            Some(why) => Err(why.to_string()),
            None => Ok(answer),
        }
    }

    pub fn listen(&self, channel: &str) -> Result<(), String> {
        self.ask(json!({"listen": channel}), None).map(|_| ())
    }

    /// Says `body` on `channel`, through the transfer layer where it is long, and gives how many
    /// listeners heard it.
    pub fn say(&self, channel: &str, body: Value) -> Result<u64, String> {
        let text = body.to_string();
        let answer = if text.len() <= transfer::SMALL {
            self.ask(json!({"say": channel, "body": body}), None)?
        } else {
            let region = Region::with(text.as_bytes())?;
            self.ask(json!({"say": channel, "transfer": handed(&region, None)?}), Some(&region))?
        };
        Ok(answer["heard"].as_u64().unwrap_or(0))
    }

    /// The next message on a channel this listens on, within `wait`.
    pub fn next(&self, wait: Duration) -> Option<Message> {
        self.messages.lock().ok()?.recv_timeout(wait).ok()
    }
}

type Accept = Box<dyn Fn() -> Result<Joined, String> + Send>;

#[cfg(windows)]
impl Outlet for windows::Pipe {
    /// The region's handle is in the frame already: the pipe carries the frame alone.
    fn write_with(&mut self, frame: &[u8], _region: &Region) -> std::io::Result<()> {
        self.write_all(frame)
    }
}

#[cfg(windows)]
fn listener() -> Result<(String, Accept), String> {
    let secret: String = super::secret(8)?.iter().map(|byte| format!("{byte:02x}")).collect();
    let address = format!(r"\\.\pipe\orior-comms-{}-{secret}", std::process::id());
    let name = address.clone();
    // An instance waits from the start, and the next is made as each is joined: a process that joins
    // finds one ready.
    let waiting = Mutex::new(windows::Pipe::make(&name, true)?);
    let accept: Accept = Box::new(move || {
        let mut waiting = waiting.lock().map_err(|_| "the door is held")?;
        if let Err(error) = waiting.wait() {
            *waiting = windows::Pipe::make(&name, false)?;
            return Err(error);
        }
        let joined = std::mem::replace(&mut *waiting, windows::Pipe::make(&name, false)?);
        let peer = joined.peer();
        Ok(Joined { input: Box::new(joined.clone()), output: Box::new(joined), peer, fds: Arc::default() })
    });
    Ok((address, accept))
}

#[cfg(windows)]
fn connect(address: &str) -> Result<Joined, String> {
    let pipe = windows::Pipe::connect(address)?;
    Ok(Joined { input: Box::new(pipe.clone()), output: Box::new(pipe), peer: None, fds: Arc::default() })
}

/// A socket's writing half, which hands a region's descriptor over with a frame's first byte.
#[cfg(unix)]
struct Socket(std::os::unix::net::UnixStream);

#[cfg(unix)]
impl Write for Socket {
    fn write(&mut self, buffer: &[u8]) -> std::io::Result<usize> {
        self.0.write(buffer)
    }

    fn flush(&mut self) -> std::io::Result<()> {
        self.0.flush()
    }
}

#[cfg(unix)]
impl Outlet for Socket {
    fn write_with(&mut self, frame: &[u8], region: &Region) -> std::io::Result<()> {
        use std::os::fd::AsRawFd;
        transfer::send_with(self.0.as_raw_fd(), frame, region.fd())
    }
}

/// A socket's reading half, which keeps each descriptor that comes with the bytes it reads.
#[cfg(unix)]
struct Incoming {
    socket: std::os::unix::net::UnixStream,
    fds: Arc<Fds>,
}

#[cfg(unix)]
impl Read for Incoming {
    fn read(&mut self, buffer: &mut [u8]) -> std::io::Result<usize> {
        use std::os::fd::AsRawFd;
        transfer::receive(self.socket.as_raw_fd(), buffer, &self.fds)
    }
}

#[cfg(unix)]
fn joined(stream: std::os::unix::net::UnixStream, peer: Option<u32>) -> Result<Joined, String> {
    let output = stream.try_clone().map_err(|error| error.to_string())?;
    let fds = Arc::new(Fds::default());
    Ok(Joined { input: Box::new(Incoming { socket: stream, fds: fds.clone() }), output: Box::new(Socket(output)), peer, fds })
}

#[cfg(unix)]
fn listener() -> Result<(String, Accept), String> {
    use std::os::unix::fs::DirBuilderExt;
    let secret: String = super::secret(8)?.iter().map(|byte| format!("{byte:02x}")).collect();
    let folder = std::env::temp_dir().join(format!("orior-comms-{}-{secret}", std::process::id()));
    std::fs::DirBuilder::new().mode(0o700).create(&folder).map_err(|error| format!("{}: {error}", folder.display()))?;
    let path = folder.join("door");
    let listener = std::os::unix::net::UnixListener::bind(&path).map_err(|error| format!("{}: {error}", path.display()))?;
    let accept: Accept = Box::new(move || {
        let (stream, _) = listener.accept().map_err(|error| error.to_string())?;
        let peer = unix::peer(&stream);
        joined(stream, peer)
    });
    Ok((path.display().to_string(), accept))
}

#[cfg(unix)]
fn connect(address: &str) -> Result<Joined, String> {
    let stream = std::os::unix::net::UnixStream::connect(address).map_err(|error| format!("{address}: {error}"))?;
    joined(stream, None)
}

#[cfg(unix)]
mod unix {
    use std::ffi::c_void;
    use std::os::fd::AsRawFd;

    unsafe extern "C" {
        fn getsockopt(socket: i32, level: i32, name: i32, value: *mut c_void, length: *mut u32) -> i32;
    }

    /// The process at the other end of `stream`, as the system says.
    pub fn peer(stream: &std::os::unix::net::UnixStream) -> Option<u32> {
        #[cfg(target_os = "linux")]
        {
            // SO_PEERCRED of SOL_SOCKET gives a ucred: the process, then its user and group.
            let mut credentials = [0i32; 3];
            let mut length = std::mem::size_of_val(&credentials) as u32;
            // SAFETY: the value is as long as the length told.
            let read = unsafe { getsockopt(stream.as_raw_fd(), 1, 17, credentials.as_mut_ptr().cast(), &mut length) };
            (read == 0).then_some(credentials[0] as u32)
        }
        #[cfg(target_os = "macos")]
        {
            // LOCAL_PEERPID of SOL_LOCAL gives the process.
            let mut pid = 0i32;
            let mut length = std::mem::size_of::<i32>() as u32;
            // SAFETY: the value is as long as the length told.
            let read = unsafe { getsockopt(stream.as_raw_fd(), 0, 2, (&raw mut pid).cast(), &mut length) };
            (read == 0).then_some(pid as u32)
        }
        #[cfg(not(any(target_os = "linux", target_os = "macos")))]
        {
            let _ = stream;
            None
        }
    }
}

#[cfg(windows)]
mod windows {
    use std::sync::Arc;
    use windows_sys::Win32::Foundation::{CloseHandle, GetLastError, ERROR_IO_PENDING, ERROR_PIPE_BUSY, ERROR_PIPE_CONNECTED, GENERIC_READ, GENERIC_WRITE, HANDLE, INVALID_HANDLE_VALUE};
    use windows_sys::Win32::Storage::FileSystem::{CreateFileW, ReadFile, WriteFile, FILE_FLAG_FIRST_PIPE_INSTANCE, FILE_FLAG_OVERLAPPED, FILE_SHARE_NONE, OPEN_EXISTING, PIPE_ACCESS_DUPLEX};
    use windows_sys::Win32::System::IO::{GetOverlappedResult, OVERLAPPED};
    use windows_sys::Win32::System::Pipes::{ConnectNamedPipe, CreateNamedPipeW, GetNamedPipeClientProcessId, PIPE_READMODE_BYTE, PIPE_REJECT_REMOTE_CLIENTS, PIPE_TYPE_BYTE, PIPE_UNLIMITED_INSTANCES, PIPE_WAIT, WaitNamedPipeW};
    use windows_sys::Win32::System::Threading::CreateEventW;

    struct Handle(HANDLE);

    // SAFETY: a pipe handle opened for overlapped work is used from any thread, each operation with
    // its own OVERLAPPED and event.
    unsafe impl Send for Handle {}
    unsafe impl Sync for Handle {}

    impl Drop for Handle {
        fn drop(&mut self) {
            // SAFETY: the handle is this value's alone and closed once.
            unsafe { CloseHandle(self.0) };
        }
    }

    /// A pipe opened for overlapped work: a read and a write go on at once, each waited for alone.
    #[derive(Clone)]
    pub struct Pipe(Arc<Handle>);

    fn wide(text: &str) -> Vec<u16> {
        text.encode_utf16().chain(std::iter::once(0)).collect()
    }

    /// Runs one overlapped operation on `handle` and waits for it; `start` begins it with the
    /// OVERLAPPED it is given. `done_already` is an error that means the operation needed no wait.
    fn overlapped(handle: HANDLE, done_already: u32, start: impl FnOnce(*mut OVERLAPPED) -> i32) -> std::io::Result<u32> {
        // SAFETY: the event and the OVERLAPPED live until the operation is waited for, and the event
        // is closed on every path.
        unsafe {
            let event = CreateEventW(std::ptr::null(), 1, 0, std::ptr::null());
            if event.is_null() {
                return Err(std::io::Error::last_os_error());
            }
            let mut work: OVERLAPPED = std::mem::zeroed();
            work.hEvent = event;
            let started = start(&mut work);
            if started == 0 {
                let error = GetLastError();
                if error == done_already && done_already != 0 {
                    CloseHandle(event);
                    return Ok(0);
                }
                if error != ERROR_IO_PENDING {
                    CloseHandle(event);
                    return Err(std::io::Error::from_raw_os_error(error as i32));
                }
            }
            let mut moved = 0u32;
            let waited = GetOverlappedResult(handle, &work, &mut moved, 1);
            let failed = std::io::Error::last_os_error();
            CloseHandle(event);
            if waited == 0 { Err(failed) } else { Ok(moved) }
        }
    }

    impl Pipe {
        /// Opens the pipe `name` to one more process.
        pub fn make(name: &str, first: bool) -> Result<Pipe, String> {
            let name = wide(name);
            let mode = PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED | if first { FILE_FLAG_FIRST_PIPE_INSTANCE } else { 0 };
            // SAFETY: the name is a wide string ending in a nul, and the handle is checked.
            let handle = unsafe { CreateNamedPipeW(name.as_ptr(), mode, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, PIPE_UNLIMITED_INSTANCES, 1 << 16, 1 << 16, 0, std::ptr::null()) };
            if handle == INVALID_HANDLE_VALUE {
                return Err(std::io::Error::last_os_error().to_string());
            }
            Ok(Pipe(Arc::new(Handle(handle))))
        }

        /// Waits for a process to join this instance.
        pub fn wait(&self) -> Result<(), String> {
            let handle = self.0 .0;
            // SAFETY: the pipe's handle and the OVERLAPPED overlapped() gives.
            overlapped(handle, ERROR_PIPE_CONNECTED, |work| unsafe { ConnectNamedPipe(handle, work) }).map(|_| ()).map_err(|error| error.to_string())
        }

        /// Joins the pipe `name`, waiting up to five seconds while every instance is taken.
        pub fn connect(name: &str) -> Result<Pipe, String> {
            let wide_name = wide(name);
            let until = std::time::Instant::now() + std::time::Duration::from_secs(5);
            loop {
                // SAFETY: the name is a wide string ending in a nul, and the handle is checked.
                let handle = unsafe { CreateFileW(wide_name.as_ptr(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_NONE, std::ptr::null(), OPEN_EXISTING, FILE_FLAG_OVERLAPPED, std::ptr::null_mut()) };
                if handle != INVALID_HANDLE_VALUE {
                    return Ok(Pipe(Arc::new(Handle(handle))));
                }
                let error = std::io::Error::last_os_error();
                if error.raw_os_error() != Some(ERROR_PIPE_BUSY as i32) || std::time::Instant::now() >= until {
                    return Err(format!("{name}: {error}"));
                }
                // SAFETY: the name is a wide string ending in a nul.
                unsafe { WaitNamedPipeW(wide_name.as_ptr(), 1000) };
            }
        }

        /// The process at the other end, as the system says.
        pub fn peer(&self) -> Option<u32> {
            let mut id = 0u32;
            // SAFETY: the handle is a pipe's, and the id a plain number the call fills.
            (unsafe { GetNamedPipeClientProcessId(self.0 .0, &mut id) } != 0).then_some(id)
        }
    }

    impl std::io::Read for Pipe {
        fn read(&mut self, buffer: &mut [u8]) -> std::io::Result<usize> {
            let handle = self.0 .0;
            // SAFETY: the buffer outlives the operation, which overlapped() waits for.
            overlapped(handle, 0, |work| unsafe { ReadFile(handle, buffer.as_mut_ptr(), buffer.len() as u32, std::ptr::null_mut(), work) }).map(|moved| moved as usize)
        }
    }

    impl std::io::Write for Pipe {
        fn write(&mut self, buffer: &[u8]) -> std::io::Result<usize> {
            let handle = self.0 .0;
            // SAFETY: the buffer outlives the operation, which overlapped() waits for.
            overlapped(handle, 0, |work| unsafe { WriteFile(handle, buffer.as_ptr(), buffer.len() as u32, std::ptr::null_mut(), work) }).map(|moved| moved as usize)
        }

        fn flush(&mut self) -> std::io::Result<()> {
            Ok(())
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_process_joins_by_its_key_and_hears_and_speaks_on_its_channels() {
        let comms = Arc::new(Comms::default());
        let address = open(comms.clone()).unwrap();
        let here = comms.register("tracker", Some(std::process::id()), &["runs/*"], &["runs/notes"]).unwrap();
        let runner = comms.register("runner", None, &["runs/notes"], &["runs/*"]).unwrap();
        let notes = comms.listen(&runner, "runs/notes").unwrap();
        let client = Client::join(&address, &here).unwrap();
        assert_eq!(client.name, "tracker");
        client.listen("runs/*").unwrap();
        assert_eq!(comms.say(&runner, "runs/7", json!({"state": "live"})).unwrap(), 1);
        let heard = client.next(Duration::from_secs(5)).unwrap();
        assert_eq!((heard.channel.as_str(), heard.from.as_str(), heard.body["state"].as_str()), ("runs/7", "runner", Some("live")));
        // The tracker listens on runs/*, and so hears its own note beside the runner.
        assert_eq!(client.say("runs/notes", json!("seen")).unwrap(), 2);
        assert_eq!(notes.recv_timeout(Duration::from_secs(5)).unwrap().from, "tracker");
        assert_eq!(client.next(Duration::from_secs(5)).unwrap().channel, "runs/notes");
        assert!(client.say("runs/7", json!(null)).unwrap_err().contains("may not speak on runs/7"));
        assert!(client.listen("lsp/rust").unwrap_err().contains("may not listen on lsp/rust"));
    }

    #[test]
    fn a_key_given_to_another_process_or_to_no_part_is_refused_at_the_door() {
        let comms = Arc::new(Comms::default());
        let address = open(comms.clone()).unwrap();
        let elsewhere = comms.register("someone else's", Some(std::process::id().wrapping_add(1)), &["*"], &["*"]).unwrap();
        let refused = Client::join(&address, &elsewhere).err().unwrap();
        assert!(refused.contains("given to another process"), "{refused}");
        let refused = Client::join(&address, &Key::from_text("not a key")).err().unwrap();
        assert!(refused.contains("no part's"), "{refused}");
        let in_the_container = comms.register("the window", None, &["*"], &[]).unwrap();
        let refused = Client::join(&address, &in_the_container).err().unwrap();
        assert!(refused.contains("given to no process"), "{refused}");
    }

    #[test]
    fn a_key_given_to_a_process_is_taken_from_every_process_under_it() {
        let comms = Arc::new(Comms::default());
        let address = open(comms.clone()).unwrap();
        let own = std::process::id();
        let parent = crate::container::parents().iter().find(|(child, _)| *child == own).map(|(_, parent)| *parent).unwrap();
        let shell = comms.register("terminal", None, &["runs/*"], &["runs/*"]).unwrap();
        comms.bind(&shell, parent);
        assert_eq!(Client::join(&address, &shell).unwrap().name, "terminal");
    }

    #[test]
    fn a_body_past_the_most_a_frame_takes_goes_through_the_transfer_layer_each_way() {
        let comms = Arc::new(Comms::default());
        let address = open(comms.clone()).unwrap();
        let tracker = comms.register("tracker", None, &["runs/*"], &["runs/*"]).unwrap();
        comms.bind(&tracker, std::process::id());
        let window = comms.register("window", None, &["*"], &["*"]).unwrap();
        let heard = comms.listen(&window, "runs/big").unwrap();
        let client = Client::join(&address, &tracker).unwrap();
        client.listen("runs/back").unwrap();
        let big = "orior ".repeat(MOST / 5);
        assert!(big.len() > MOST);
        assert_eq!(client.say("runs/big", json!(big)).unwrap(), 1);
        assert_eq!(heard.recv_timeout(Duration::from_secs(10)).unwrap().body.as_str().map(str::len), Some(big.len()));
        comms.say(&window, "runs/back", json!(big)).unwrap();
        let back = client.next(Duration::from_secs(10)).unwrap();
        assert_eq!((back.channel.as_str(), back.from.as_str(), back.body.as_str().map(str::len)), ("runs/back", "window", Some(big.len())));
        assert_eq!(client.say("runs/big", json!("small")).unwrap(), 1, "a short body goes in its frame");
        let channels = comms.channels();
        let moved = |name: &str| channels.iter().find(|one| one.name == name).unwrap().transferred;
        assert_eq!((moved("runs/big"), moved("runs/back")), (big.len() as u64 + 2, big.len() as u64 + 2));
    }

    #[test]
    fn a_frame_past_the_most_a_frame_takes_is_not_read() {
        let mut frame = ((MOST + 1) as u32).to_le_bytes().to_vec();
        frame.extend_from_slice(b"{}");
        assert!(read_frame(&mut frame.as_slice()).is_err());
        let mut out = Vec::new();
        write_frame(&mut out, &json!({"a": 1})).unwrap();
        assert_eq!(read_frame(&mut out.as_slice()).unwrap(), json!({"a": 1}));
    }
}
