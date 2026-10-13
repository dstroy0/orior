// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The container's comms layer: every message between orior's parts goes through it, each on a named
//! channel. A part registers and is given a key, drawn from the system's own source of secrets; the
//! key names the channels the part may listen on and those it may speak on, and the container knows
//! the process each key was given to. A part grants the parts it starts their channels, never more
//! than its own. A message on a channel its speaker's key does not name is refused and counted, and a
//! listener hears only the channels its key names.
//!
//! The container keeps each channel's traffic: how many messages and bytes it has carried, how many of
//! those bytes went through the transfer layer, how many messages it refused, who listens, and the
//! last few messages, for a view of every part and what it said.
//!
//! A channel's name is words joined by slashes, as lsp/rust or db/rows. A grant is a name, or a name
//! ending in /* for every channel under it, or * alone for every channel.

pub mod door;

use std::collections::{HashMap, VecDeque};
use std::sync::mpsc::{self, Receiver, Sender};
use std::sync::Mutex;

use serde::Serialize;
use serde_json::Value;

/// How many of each channel's last messages the container keeps.
const KEPT: usize = 20;

/// A part's key: its secret, which only the container and the part hold.
#[derive(Clone, PartialEq, Eq, Hash, Debug)]
pub struct Key(String);

impl Key {
    pub fn text(&self) -> &str {
        &self.0
    }

    pub fn from_text(text: &str) -> Key {
        Key(text.to_string())
    }
}

/// A message as a listener hears it: its channel, the name of the part that said it, and its body.
#[derive(Clone, Debug, Serialize)]
pub struct Message {
    pub channel: String,
    pub from: String,
    pub body: Value,
}

struct Part {
    name: String,
    process: Option<u32>,
    /// Whether the door takes the key from any process that holds it.
    held: bool,
    listen: Vec<String>,
    speak: Vec<String>,
    said: u64,
    refused: u64,
}

#[derive(Default)]
struct Traffic {
    messages: u64,
    bytes: u64,
    transferred: u64,
    refused: u64,
    last: VecDeque<Message>,
}

/// How a listener hears: through a channel it reads when it will, or by a call made in the speaker's
/// own thread, which keeps the order of what one thread says, as a window that hears a call's
/// events before its answer needs.
#[derive(Clone)]
enum Hears {
    Queue(Sender<Message>),
    Call(std::sync::Arc<dyn Fn(&Message) + Send + Sync>),
}

#[derive(Default)]
struct Inner {
    parts: HashMap<Key, Part>,
    listeners: Vec<(Key, String, Hears)>,
    traffic: HashMap<String, Traffic>,
}

/// A part as the view lists it.
#[derive(Serialize, Debug, Clone)]
pub struct PartShown {
    pub name: String,
    pub process: Option<u32>,
    pub listen: Vec<String>,
    pub speak: Vec<String>,
    pub said: u64,
    pub refused: u64,
}

/// A channel as the view lists it.
#[derive(Serialize, Debug, Clone)]
pub struct ChannelShown {
    pub name: String,
    pub messages: u64,
    pub bytes: u64,
    /// The bytes of its bodies that went through the transfer layer, to or from a process.
    pub transferred: u64,
    pub refused: u64,
    pub listeners: Vec<String>,
    pub last: Vec<Message>,
}

#[derive(Default)]
pub struct Comms {
    inner: Mutex<Inner>,
}

/// Whether the grant `grant` names the channel `channel`.
pub fn grants(grant: &str, channel: &str) -> bool {
    grant == "*" || grant == channel || grant.strip_suffix("/*").is_some_and(|under| channel.strip_prefix(under).is_some_and(|rest| rest.starts_with('/')))
}

/// Whether the grant `inner` names only channels the grant `outer` names.
fn within(inner: &str, outer: &str) -> bool {
    if outer == "*" {
        return true;
    }
    if inner == "*" {
        return false;
    }
    match (inner.strip_suffix("/*"), outer.strip_suffix("/*")) {
        (Some(inner_under), Some(outer_under)) => inner_under == outer_under || grants(outer, inner_under),
        (Some(_), None) => false,
        (None, _) => grants(outer, inner),
    }
}

/// Bytes from the system's own source of secrets.
pub fn secret(count: usize) -> Result<Vec<u8>, String> {
    let mut out = vec![0u8; count];
    #[cfg(windows)]
    {
        use windows_sys::Win32::Security::Cryptography::{BCryptGenRandom, BCRYPT_USE_SYSTEM_PREFERRED_RNG};
        // SAFETY: the buffer is as long as the length told.
        let status = unsafe { BCryptGenRandom(std::ptr::null_mut(), out.as_mut_ptr(), out.len() as u32, BCRYPT_USE_SYSTEM_PREFERRED_RNG) };
        if status != 0 {
            return Err(format!("the system gave no secret: status {status:#x}"));
        }
    }
    #[cfg(not(windows))]
    {
        use std::io::Read;
        std::fs::File::open("/dev/urandom").and_then(|mut source| source.read_exact(&mut out)).map_err(|error| format!("the system gave no secret: {error}"))?;
    }
    Ok(out)
}

impl Comms {
    /// Registers a part named `name`, granted the channels `listen` and `speak` by the container, and
    /// gives its key. The key is given to the process `process`; a part with no process is one of the
    /// container's own, whose key the door never takes.
    pub fn register(&self, name: &str, process: Option<u32>, listen: &[&str], speak: &[&str]) -> Result<Key, String> {
        let key = Key(secret(32)?.iter().map(|byte| format!("{byte:02x}")).collect());
        let mut inner = self.inner.lock().map_err(|_| "the comms layer is held")?;
        inner.parts.insert(key.clone(), Part { name: name.to_string(), process, held: false, listen: listen.iter().map(|one| one.to_string()).collect(), speak: speak.iter().map(|one| one.to_string()).collect(), said: 0, refused: 0 });
        Ok(key)
    }

    /// Registers a part whose key the door takes from any process that holds it, as register does
    /// otherwise: the key is kept where only the reader can read it, and holding it is the grant.
    pub fn register_held(&self, name: &str, listen: &[&str], speak: &[&str]) -> Result<Key, String> {
        let key = self.register(name, None, listen, speak)?;
        if let Some(part) = self.inner.lock().map_err(|_| "the comms layer is held")?.parts.get_mut(&key) {
            part.held = true;
        }
        Ok(key)
    }

    /// Whether the door takes the key of `key`'s part from any process that holds it.
    pub fn held(&self, key: &Key) -> bool {
        self.inner.lock().ok().and_then(|inner| inner.parts.get(key).map(|part| part.held)).unwrap_or(false)
    }

    /// Registers a part the part of `parent` starts, granted channels of the parent's own alone.
    pub fn register_under(&self, parent: &Key, name: &str, process: Option<u32>, listen: &[&str], speak: &[&str]) -> Result<Key, String> {
        {
            let inner = self.inner.lock().map_err(|_| "the comms layer is held")?;
            let granter = inner.parts.get(parent).ok_or("that key is no part's")?;
            let beyond = |asked: &[&str], held: &[String]| asked.iter().find(|one| !held.iter().any(|grant| within(one, grant))).map(|one| one.to_string());
            if let Some(channel) = beyond(listen, &granter.listen).or_else(|| beyond(speak, &granter.speak)) {
                return Err(format!("{} may not grant {channel}, which is not its own", granter.name));
            }
        }
        self.register(name, process, listen, speak)
    }

    /// Gives the part of `key` to the process `process`, once it has started: the door takes the
    /// key from that process and from every process under it, and from no process before.
    pub fn bind(&self, key: &Key, process: u32) {
        if let Some(part) = self.inner.lock().ok().as_mut().and_then(|inner| inner.parts.get_mut(key)) {
            part.process = Some(process);
        }
    }

    /// Takes the part of `key` away, and every channel it listened on with it.
    pub fn leave(&self, key: &Key) {
        if let Ok(mut inner) = self.inner.lock() {
            inner.parts.remove(key);
            inner.listeners.retain(|(of, _, _)| of != key);
        }
    }

    /// The process the part of `key` was registered for, where it was given one.
    pub fn process_of(&self, key: &Key) -> Option<u32> {
        self.inner.lock().ok()?.parts.get(key)?.process
    }

    /// The name of the part of `key`.
    pub fn name_of(&self, key: &Key) -> Option<String> {
        Some(self.inner.lock().ok()?.parts.get(key)?.name.clone())
    }

    /// Listens on `channel`, a channel or a grant, which the key must name; every message said there
    /// from now on comes to the receiver given.
    pub fn listen(&self, key: &Key, channel: &str) -> Result<Receiver<Message>, String> {
        let (tell, hear) = mpsc::channel();
        self.hear(key, channel, Hears::Queue(tell))?;
        Ok(hear)
    }

    /// Listens on `channel` as listen does, each message handed to `call` in the thread that said
    /// it, once the container has let go of its own hold.
    pub fn listen_with(&self, key: &Key, channel: &str, call: impl Fn(&Message) + Send + Sync + 'static) -> Result<(), String> {
        self.hear(key, channel, Hears::Call(std::sync::Arc::new(call)))
    }

    fn hear(&self, key: &Key, channel: &str, hears: Hears) -> Result<(), String> {
        let mut inner = self.inner.lock().map_err(|_| "the comms layer is held")?;
        let part = inner.parts.get(key).ok_or("that key is no part's")?;
        if !part.listen.iter().any(|grant| within(channel, grant)) {
            return Err(format!("{} may not listen on {channel}", part.name));
        }
        inner.listeners.push((key.clone(), channel.to_string(), hears));
        Ok(())
    }

    /// Says `body` on `channel` as the part of `key`, and gives how many listeners heard it.
    pub fn say(&self, key: &Key, channel: &str, body: Value) -> Result<usize, String> {
        let mut inner = self.inner.lock().map_err(|_| "the comms layer is held")?;
        let Some(part) = inner.parts.get_mut(key) else {
            return Err("that key is no part's".into());
        };
        let name = part.name.clone();
        if !part.speak.iter().any(|grant| grants(grant, channel)) {
            part.refused += 1;
            inner.traffic.entry(channel.to_string()).or_default().refused += 1;
            return Err(format!("{name} may not speak on {channel}"));
        }
        part.said += 1;
        let message = Message { channel: channel.to_string(), from: name, body };
        let bytes = message.body.to_string().len() as u64;
        let traffic = inner.traffic.entry(channel.to_string()).or_default();
        traffic.messages += 1;
        traffic.bytes += bytes;
        traffic.last.push_back(message.clone());
        while traffic.last.len() > KEPT {
            traffic.last.pop_front();
        }
        let mut heard = 0;
        let mut calls = Vec::new();
        // A listener whose receiver is gone is let go.
        inner.listeners.retain(|(_, pattern, hears)| {
            if !grants(pattern, channel) {
                return true;
            }
            match hears {
                Hears::Queue(tell) => {
                    let kept = tell.send(message.clone()).is_ok();
                    heard += usize::from(kept);
                    kept
                }
                Hears::Call(call) => {
                    calls.push(call.clone());
                    heard += 1;
                    true
                }
            }
        });
        drop(inner);
        for call in calls {
            call(&message);
        }
        Ok(heard)
    }

    /// Counts `bytes` of a body on `channel` that went through the transfer layer.
    pub fn transferred(&self, channel: &str, bytes: u64) {
        if let Ok(mut inner) = self.inner.lock() {
            inner.traffic.entry(channel.to_string()).or_default().transferred += bytes;
        }
    }

    /// Every part, as the view lists it.
    pub fn parts(&self) -> Vec<PartShown> {
        let Ok(inner) = self.inner.lock() else { return Vec::new() };
        let mut parts: Vec<PartShown> = inner.parts.values().map(|part| PartShown { name: part.name.clone(), process: part.process, listen: part.listen.clone(), speak: part.speak.clone(), said: part.said, refused: part.refused }).collect();
        parts.sort_by(|a, b| a.name.cmp(&b.name));
        parts
    }

    /// Every channel that has carried or refused a message, as the view lists it.
    pub fn channels(&self) -> Vec<ChannelShown> {
        let Ok(inner) = self.inner.lock() else { return Vec::new() };
        let mut channels: Vec<ChannelShown> = inner
            .traffic
            .iter()
            .map(|(name, traffic)| ChannelShown {
                name: name.clone(),
                messages: traffic.messages,
                bytes: traffic.bytes,
                transferred: traffic.transferred,
                refused: traffic.refused,
                listeners: inner.listeners.iter().filter(|(_, pattern, _)| grants(pattern, name)).filter_map(|(key, _, _)| inner.parts.get(key).map(|part| part.name.clone())).collect(),
                last: traffic.last.iter().cloned().collect(),
            })
            .collect();
        channels.sort_by(|a, b| a.name.cmp(&b.name));
        channels
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;
    use std::time::Duration;

    #[test]
    fn a_grant_names_its_channel_and_every_channel_under_it() {
        assert!(grants("*", "lsp/rust"));
        assert!(grants("lsp/rust", "lsp/rust"));
        assert!(grants("lsp/*", "lsp/rust"));
        assert!(grants("lsp/*", "lsp/rust/diagnostics"));
        assert!(!grants("lsp/*", "lsp"));
        assert!(!grants("lsp/*", "lspx/rust"));
        assert!(!grants("lsp/rust", "lsp/python"));
        assert!(within("lsp/rust", "lsp/*"));
        assert!(within("lsp/rust/*", "lsp/*"));
        assert!(!within("lsp/*", "lsp/rust"));
        assert!(!within("*", "lsp/*"));
        assert!(within("anything/at/all", "*"));
    }

    #[test]
    fn a_key_is_a_secret_of_the_system_s_own_and_no_two_are_alike() {
        let comms = Comms::default();
        let one = comms.register("one", None, &[], &[]).unwrap();
        let two = comms.register("two", None, &[], &[]).unwrap();
        assert_eq!(one.text().len(), 64);
        assert_ne!(one, two);
        assert_eq!(comms.name_of(&two).as_deref(), Some("two"));
    }

    #[test]
    fn a_listener_hears_what_is_said_on_its_channels_and_nothing_else() {
        let comms = Comms::default();
        let window = comms.register("window", None, &["lsp/*", "runs"], &[]).unwrap();
        let server = comms.register("rust server", Some(42), &[], &["lsp/rust"]).unwrap();
        let heard = comms.listen(&window, "lsp/*").unwrap();
        assert_eq!(comms.say(&server, "lsp/rust", json!({"n": 1})).unwrap(), 1);
        let message = heard.recv_timeout(Duration::from_secs(1)).unwrap();
        assert_eq!((message.channel.as_str(), message.from.as_str(), message.body["n"].as_i64()), ("lsp/rust", "rust server", Some(1)));
        assert_eq!(comms.process_of(&server), Some(42));
    }

    #[test]
    fn a_channel_the_key_does_not_name_is_refused_to_speaker_and_listener_alike() {
        let comms = Comms::default();
        let window = comms.register("window", None, &["lsp/*"], &[]).unwrap();
        let server = comms.register("rust server", None, &[], &["lsp/rust"]).unwrap();
        assert!(comms.listen(&window, "db/rows").unwrap_err().contains("may not listen on db/rows"));
        assert!(comms.say(&server, "db/rows", json!(null)).unwrap_err().contains("may not speak on db/rows"));
        assert!(comms.say(&Key::from_text("forged"), "lsp/rust", json!(null)).is_err());
        let rows = comms.channels().into_iter().find(|one| one.name == "db/rows").unwrap();
        assert_eq!((rows.messages, rows.refused), (0, 1));
        assert_eq!(comms.parts().iter().find(|part| part.name == "rust server").unwrap().refused, 1);
    }

    #[test]
    fn a_part_grants_what_it_starts_only_channels_of_its_own() {
        let comms = Comms::default();
        let runs = comms.register("runs", None, &["runs/*"], &["runs/*"]).unwrap();
        let run = comms.register_under(&runs, "run 7", Some(7), &["runs/7"], &["runs/7"]).unwrap();
        assert_eq!(comms.process_of(&run), Some(7));
        assert!(comms.register_under(&runs, "greedy", None, &["lsp/rust"], &[]).unwrap_err().contains("may not grant lsp/rust"));
        assert!(comms.register_under(&runs, "greedy", None, &[], &["*"]).is_err());
    }

    #[test]
    fn the_container_keeps_each_channel_s_traffic_and_its_last_messages() {
        let comms = Comms::default();
        let window = comms.register("window", None, &["*"], &[]).unwrap();
        let server = comms.register("server", None, &[], &["files"]).unwrap();
        let _heard = comms.listen(&window, "files").unwrap();
        for at in 0..25 {
            comms.say(&server, "files", json!(at)).unwrap();
        }
        let files = comms.channels().into_iter().find(|one| one.name == "files").unwrap();
        assert_eq!(files.messages, 25);
        assert_eq!(files.last.len(), KEPT);
        assert_eq!(files.last[0].body, json!(5));
        assert_eq!(files.listeners, vec!["window".to_string()]);
        assert!(files.bytes >= 25);
    }

    #[test]
    fn a_listener_by_call_hears_in_the_speaker_s_thread_in_the_order_said() {
        let comms = Comms::default();
        let window = comms.register("window", None, &["*"], &[]).unwrap();
        let server = comms.register("server", None, &[], &["*"]).unwrap();
        let heard = std::sync::Arc::new(Mutex::new(Vec::new()));
        let kept = heard.clone();
        let speaker = std::thread::current().id();
        comms.listen_with(&window, "*", move |message| kept.lock().unwrap().push((message.channel.clone(), std::thread::current().id() == speaker))).unwrap();
        comms.say(&server, "run/started", json!(1)).unwrap();
        comms.say(&server, "run/line", json!(2)).unwrap();
        assert_eq!(*heard.lock().unwrap(), vec![("run/started".to_string(), true), ("run/line".to_string(), true)]);
    }

    #[test]
    fn a_part_that_leaves_hears_nothing_more_and_its_key_opens_nothing() {
        let comms = Comms::default();
        let window = comms.register("window", None, &["*"], &[]).unwrap();
        let server = comms.register("server", None, &[], &["files"]).unwrap();
        let heard = comms.listen(&window, "files").unwrap();
        comms.leave(&window);
        assert_eq!(comms.say(&server, "files", json!(1)).unwrap(), 0);
        assert!(heard.recv_timeout(Duration::from_millis(50)).is_err());
        assert!(comms.listen(&window, "files").is_err());
    }
}
