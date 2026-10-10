// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! ZeroMQ's wire protocol, ZMTP 3.0 with its NULL mechanism, as a Jupyter kernel speaks it on the
//! machine's own loopback: the greeting, the READY command that names a socket's type, and messages
//! of one or more frames. orior is the connecting side of each of a kernel's sockets, a DEALER to
//! its ROUTERs, a SUB to its PUB and a REQ to its REP.
//!
//! A SUB subscribes as ZMTP 3.0 has it, by a message whose first byte is 1, which a peer that speaks
//! 3.1 takes from a 3.0 peer as well.

use std::io::{Read, Write};
use std::net::TcpStream;
use std::time::Duration;

/// A connection to one of a peer's sockets.
pub struct Socket {
    stream: TcpStream,
}

const MORE: u8 = 0x01;
const LONG: u8 = 0x02;
const COMMAND: u8 = 0x04;

impl Socket {
    /// Connects to `host:port` as a socket of the type named, DEALER, SUB or REQ, and greets the peer.
    pub fn connect(host: &str, port: u16, kind: &str) -> Result<Socket, String> {
        let stream = TcpStream::connect((host, port)).map_err(|error| format!("{host}:{port}: {error}"))?;
        stream.set_nodelay(true).ok();
        let mut socket = Socket { stream };
        socket.greet(kind)?;
        if kind == "SUB" {
            socket.send(&[vec![1u8]])?;
        }
        Ok(socket)
    }

    /// A second handle on the same connection, for one thread to read while another writes.
    pub fn split(&self) -> Result<Socket, String> {
        Ok(Socket { stream: self.stream.try_clone().map_err(|error| error.to_string())? })
    }

    pub fn set_timeout(&self, wait: Option<Duration>) {
        let _ = self.stream.set_read_timeout(wait);
    }

    fn greet(&mut self, kind: &str) -> Result<(), String> {
        let mut greeting = [0u8; 64];
        greeting[0] = 0xff;
        greeting[9] = 0x7f;
        greeting[10] = 3;
        greeting[11] = 0;
        greeting[12..16].copy_from_slice(b"NULL");
        self.stream.write_all(&greeting).map_err(|error| error.to_string())?;
        let mut theirs = [0u8; 64];
        self.stream.read_exact(&mut theirs).map_err(|error| format!("the kernel's greeting: {error}"))?;
        if theirs[0] != 0xff || theirs[9] != 0x7f || theirs[10] < 3 {
            return Err("the peer speaks no ZMTP 3".to_string());
        }
        if &theirs[12..16] != b"NULL" {
            return Err(format!("the peer asks for the {} mechanism, where orior speaks NULL", String::from_utf8_lossy(&theirs[12..32]).trim_end_matches('\0')));
        }
        let mut ready = vec![5u8];
        ready.extend_from_slice(b"READY");
        ready.push(11);
        ready.extend_from_slice(b"Socket-Type");
        ready.extend_from_slice(&(kind.len() as u32).to_be_bytes());
        ready.extend_from_slice(kind.as_bytes());
        self.write_frame(&ready, COMMAND)?;
        let (flags, body) = self.read_frame()?;
        if flags & COMMAND == 0 || !body.starts_with(b"\x05READY") {
            return Err("the peer answered the greeting with no READY".to_string());
        }
        Ok(())
    }

    fn write_frame(&mut self, body: &[u8], flags: u8) -> Result<(), String> {
        let mut head = Vec::with_capacity(9);
        if body.len() > 255 {
            head.push(flags | LONG);
            head.extend_from_slice(&(body.len() as u64).to_be_bytes());
        } else {
            head.push(flags);
            head.push(body.len() as u8);
        }
        self.stream.write_all(&head).and_then(|_| self.stream.write_all(body)).map_err(|error| error.to_string())
    }

    fn read_frame(&mut self) -> Result<(u8, Vec<u8>), String> {
        let mut flags = [0u8; 1];
        self.stream.read_exact(&mut flags).map_err(|error| error.to_string())?;
        let size = if flags[0] & LONG != 0 {
            let mut size = [0u8; 8];
            self.stream.read_exact(&mut size).map_err(|error| error.to_string())?;
            u64::from_be_bytes(size) as usize
        } else {
            let mut size = [0u8; 1];
            self.stream.read_exact(&mut size).map_err(|error| error.to_string())?;
            size[0] as usize
        };
        let mut body = vec![0u8; size];
        self.stream.read_exact(&mut body).map_err(|error| error.to_string())?;
        Ok((flags[0], body))
    }

    /// Sends a message of the frames given.
    pub fn send(&mut self, frames: &[Vec<u8>]) -> Result<(), String> {
        for (index, frame) in frames.iter().enumerate() {
            let more = if index + 1 < frames.len() { MORE } else { 0 };
            self.write_frame(frame, more)?;
        }
        Ok(())
    }

    /// The next message's frames, the commands between messages passed over.
    pub fn receive(&mut self) -> Result<Vec<Vec<u8>>, String> {
        let mut frames = Vec::new();
        loop {
            let (flags, body) = self.read_frame()?;
            if flags & COMMAND != 0 {
                continue;
            }
            frames.push(body);
            if flags & MORE == 0 {
                return Ok(frames);
            }
        }
    }
}
