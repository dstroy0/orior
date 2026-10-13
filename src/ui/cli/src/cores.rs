// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The machine's cores orior's own work spreads over: every core the machine has less those kept for
//! the reader, never fewer than one kept and never fewer than one to work on. How many are kept is the
//! setting `cores-kept`, one where nothing sets it, the reader's to raise in Preferences or in their
//! settings file; a tree's file does not set it. The window gives it as it starts and as it changes,
//! and the command line reads it from the reader's settings the first time it is asked.

use std::sync::atomic::{AtomicUsize, Ordering};

/// The setting's key, as settings.json keeps it.
pub const KEY: &str = "cores-kept";

/// The fewest cores kept for the reader.
pub const KEPT_LEAST: usize = 1;

/// The cores kept, or zero before anything has given them.
static KEPT: AtomicUsize = AtomicUsize::new(0);

/// The cores the machine has.
pub fn machine() -> usize {
    std::thread::available_parallelism().map_or(1, |count| count.get())
}

/// Keeps `kept` cores for the reader, KEPT_LEAST at the fewest.
pub fn keep(kept: usize) {
    KEPT.store(kept.max(KEPT_LEAST), Ordering::Relaxed);
}

/// The cores kept for the reader: as given, or as the reader's settings set them.
pub fn kept() -> usize {
    match KEPT.load(Ordering::Relaxed) {
        0 => {
            let reader = crate::settings::reader();
            let layers = crate::settings::layers(&reader, &crate::settings::Chain::default(), "");
            let own = reader.files.last().and_then(|file| file.values.get(KEY)).map(crate::kept::read_back);
            let set = layers.over.get(KEY).cloned().or(own).or_else(|| layers.under.get(KEY).cloned());
            let kept = set.and_then(|text| text.trim().parse::<usize>().ok()).unwrap_or(KEPT_LEAST).max(KEPT_LEAST);
            KEPT.store(kept, Ordering::Relaxed);
            kept
        }
        kept => kept,
    }
}

/// The cores orior's work may take: the machine's less those kept, one at the fewest.
pub fn for_work() -> usize {
    machine().saturating_sub(kept()).max(1)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn at_least_one_core_is_kept_and_at_least_one_is_worked_on() {
        keep(0);
        assert_eq!(kept(), 1, "none asked for keeps one");
        assert_eq!(for_work(), machine().saturating_sub(1).max(1));
        keep(machine() + 4);
        assert_eq!(for_work(), 1, "keeping more than the machine has leaves one to work on");
        keep(2);
        assert_eq!(for_work(), machine().saturating_sub(2).max(1));
        keep(KEPT_LEAST);
    }
}
