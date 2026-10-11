// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The bytes each call of the tree's side takes and keeps, counted by the allocator itself.
//!
//! The program's allocator is the system's, with a count kept on each thread of the bytes it has
//! taken and given back. While the count is on, each call is measured on the thread that answers it,
//! the difference of those counts over the call, and kept by the call's name: how many times it was
//! made, the bytes it took, the bytes it kept, which it took and did not give back before it ended,
//! and the most one call took. Threads a call starts are not counted in it. src/ui/test's memory
//! watcher turns the count on, reads it after each command and sets it back to nothing.

use std::alloc::{GlobalAlloc, Layout, System};
use std::cell::Cell;
use std::collections::BTreeMap;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::Mutex;

pub struct Counting;

thread_local! {
    static TAKEN: Cell<u64> = const { Cell::new(0) };
    static GIVEN: Cell<u64> = const { Cell::new(0) };
}

fn took(bytes: usize) {
    let _ = TAKEN.try_with(|taken| taken.set(taken.get().wrapping_add(bytes as u64)));
}

fn gave(bytes: usize) {
    let _ = GIVEN.try_with(|given| given.set(given.get().wrapping_add(bytes as u64)));
}

// SAFETY: every request goes to the system's allocator as it came; the counts are thread-local
// cells, which allocate nothing.
unsafe impl GlobalAlloc for Counting {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        took(layout.size());
        unsafe { System.alloc(layout) }
    }

    unsafe fn alloc_zeroed(&self, layout: Layout) -> *mut u8 {
        took(layout.size());
        unsafe { System.alloc_zeroed(layout) }
    }

    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        gave(layout.size());
        unsafe { System.dealloc(ptr, layout) }
    }

    unsafe fn realloc(&self, ptr: *mut u8, layout: Layout, new_size: usize) -> *mut u8 {
        gave(layout.size());
        took(new_size);
        unsafe { System.realloc(ptr, layout, new_size) }
    }
}

/// What the calls of one name took and kept.
#[derive(Clone, Default, serde::Serialize)]
pub struct Calls {
    pub count: u64,
    pub taken: u64,
    pub kept: i64,
    pub most: u64,
}

static ON: AtomicBool = AtomicBool::new(false);
static COUNTED: Mutex<BTreeMap<String, Calls>> = Mutex::new(BTreeMap::new());

fn now() -> (u64, u64) {
    (TAKEN.try_with(Cell::get).unwrap_or(0), GIVEN.try_with(Cell::get).unwrap_or(0))
}

/// Runs `work`, the call `name`, counting what it takes and keeps while the count is on.
pub fn measured<T>(name: &str, work: impl FnOnce() -> T) -> T {
    if !ON.load(Ordering::Relaxed) {
        return work();
    }
    let (taken, given) = now();
    let out = work();
    let (taken_after, given_after) = now();
    let took = taken_after.wrapping_sub(taken);
    let kept = took as i64 - given_after.wrapping_sub(given) as i64;
    if let Ok(mut counted) = COUNTED.lock() {
        let calls = counted.entry(name.to_string()).or_default();
        calls.count += 1;
        calls.taken += took;
        calls.kept += kept;
        calls.most = calls.most.max(took);
    }
    out
}

/// Turns the count on or off where `on` says, and gives what it has counted, by name, emptied where
/// `reset` is set.
pub fn read(on: Option<bool>, reset: bool) -> BTreeMap<String, Calls> {
    if let Some(on) = on {
        ON.store(on, Ordering::Relaxed);
    }
    let Ok(mut counted) = COUNTED.lock() else { return BTreeMap::new() };
    if reset {
        std::mem::take(&mut *counted)
    } else {
        counted.clone()
    }
}
