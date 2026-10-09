// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The terminal's scrollback on disk, as a large file is: every line that scrolls off the top of the
//! terminal's main screen, as the page drew it, one to a line of a file. The page holds only the
//! lines nearest the screen and reads the rest back from here as the reader scrolls to them. Memory
//! holds where each line starts, and nothing of the lines themselves.
//!
//! The file goes with the app. On Windows the system deletes it once the app closes it or ends, and
//! elsewhere it loses its name the moment it is made. A crash leaves nothing behind either.

use std::collections::HashMap;
use std::fs::{File, OpenOptions};
use std::io::{Read, Seek, SeekFrom, Write};
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Mutex;

struct Scrollback {
    file: File,
    starts: Vec<u64>,
    end: u64,
}

/// The scrollbacks open, by number, one for each terminal screen.
#[derive(Default)]
pub struct Scrollbacks {
    next: AtomicU64,
    open: Mutex<HashMap<u64, Scrollback>>,
}

fn made() -> Result<File, String> {
    let path = std::env::temp_dir().join(format!("orior-scrollback-{}-{}.txt", std::process::id(), std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|since| since.as_nanos()).unwrap_or(0)));
    let mut options = OpenOptions::new();
    options.read(true).write(true).create_new(true);
    #[cfg(windows)]
    {
        use std::os::windows::fs::OpenOptionsExt;
        // FILE_FLAG_DELETE_ON_CLOSE, with sharing that lets the system delete it.
        options.custom_flags(0x0400_0000).share_mode(0x7);
    }
    let file = options.open(&path).map_err(|error| format!("{}: {error}", path.display()))?;
    #[cfg(not(windows))]
    {
        let _ = std::fs::remove_file(&path);
    }
    Ok(file)
}

impl Scrollbacks {
    /// Makes a scrollback and returns its number, one for each terminal screen.
    pub fn open(&self) -> Result<u64, String> {
        let id = self.next.fetch_add(1, Ordering::SeqCst) + 1;
        let file = made()?;
        self.open.lock().map_err(|error| error.to_string())?.insert(id, Scrollback { file, starts: Vec::new(), end: 0 });
        Ok(id)
    }

    /// Lets every scrollback go, as a page loaded again asks: the screens they were for are gone.
    pub fn reset(&self) {
        if let Ok(mut open) = self.open.lock() {
            open.clear();
        }
    }

    /// Adds `lines` at the end of a scrollback, and returns how many lines it holds.
    pub fn keep(&self, id: u64, lines: &[String]) -> Result<u64, String> {
        let mut open = self.open.lock().map_err(|error| error.to_string())?;
        let kept = open.get_mut(&id).ok_or_else(|| format!("no scrollback {id}"))?;
        let mut text = String::new();
        for line in lines {
            kept.starts.push(kept.end + text.len() as u64);
            text.push_str(&line.replace('\n', " "));
            text.push('\n');
        }
        kept.file.seek(SeekFrom::Start(kept.end)).and_then(|_| kept.file.write_all(text.as_bytes())).map_err(|error| error.to_string())?;
        kept.end += text.len() as u64;
        Ok(kept.starts.len() as u64)
    }

    /// The lines of a scrollback from line `from`, as many as `count` and as many as it holds.
    pub fn read(&self, id: u64, from: u64, count: u64) -> Result<Vec<String>, String> {
        let mut open = self.open.lock().map_err(|error| error.to_string())?;
        let kept = open.get_mut(&id).ok_or_else(|| format!("no scrollback {id}"))?;
        let held = kept.starts.len() as u64;
        let from = from.min(held);
        let to = from.saturating_add(count).min(held);
        if from == to {
            return Ok(Vec::new());
        }
        let start = kept.starts[from as usize];
        let stop = kept.starts.get(to as usize).copied().unwrap_or(kept.end);
        let mut bytes = vec![0; (stop - start) as usize];
        kept.file.seek(SeekFrom::Start(start)).and_then(|_| kept.file.read_exact(&mut bytes)).map_err(|error| error.to_string())?;
        let text = String::from_utf8_lossy(&bytes);
        Ok(text.strip_suffix('\n').unwrap_or(&text).split('\n').map(str::to_string).collect())
    }

    /// Lets a scrollback go, which deletes its file.
    pub fn close(&self, id: u64) {
        if let Ok(mut open) = self.open.lock() {
            open.remove(&id);
        }
    }
}

#[cfg(test)]
mod kept {
    use super::Scrollbacks;

    #[test]
    fn lines_kept_read_back_from_anywhere() {
        let all = Scrollbacks::default();
        let id = all.open().unwrap();
        let lines: Vec<String> = (0..1000).map(|n| format!("<span>line {n}</span>")).collect();
        assert_eq!(all.keep(id, &lines[..600]).unwrap(), 600);
        assert_eq!(all.keep(id, &lines[600..]).unwrap(), 1000);
        assert_eq!(all.read(id, 595, 10).unwrap(), lines[595..605]);
        assert_eq!(all.read(id, 990, 50).unwrap(), lines[990..]);
        assert!(all.read(id, 1000, 5).unwrap().is_empty());
        all.close(id);
        assert!(all.read(id, 0, 1).is_err());
    }
}
