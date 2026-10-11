// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Where a needle of bytes stands in a text, the engine's sift: the text read a window at a time, the
//! bytes of each window counted, and the needle's anchors placed where the window says they refute
//! most. The candidate offsets are ranked by the census, the rarest byte first, and the anchors are
//! then placed one level at a time, each at the offset that leaves fewest of the alignments the
//! anchors above it left standing, a level ending the descent where it leaves standing every one of
//! them. The
//! first two anchors are tested a block of alignments at a time in the processor's vector
//! instructions, the rest one alignment at a time, and every alignment all of them agree with is
//! compared whole.
//!
//! The census and the plan steer and never decide: an alignment stands only where every anchor
//! agrees and the whole needle is there, whatever placed the anchors. A census taken on a sample and
//! a plan scored on a sample cost speed and never a match. The tests hold every path to the plain
//! loop.
//!
//! A needle searched without case is searched in ASCII case: each letter of the text and of the
//! needle to its lower case.

use crate::vector::{best, Path};

/// The most anchors a plan places.
pub const ANCHORS: usize = 4;

/// The bytes a window holds, the last needle's length less one of them the next window's first too.
pub const WINDOW: usize = 1 << 20;

/// The most candidate offsets a level weighs: the rarest of the needle's, by the census.
const CANDIDATES: usize = 16;

/// The most alignments a level scores its candidates on, every so many of the window's taken.
const SCORED: usize = 4096;

/// The most bytes of a window the census counts, every so many of them taken.
const COUNTED: usize = 65536;

/// How often each byte stands in a window, read on every so many of its bytes.
pub struct Census {
    pub occurrences: [u64; 256],
    pub total: u64,
}

/// A byte as a search without case reads it.
fn folded(byte: u8, fold: bool) -> u8 {
    if fold { byte.to_ascii_lowercase() } else { byte }
}

/// The census of `window`, of every so many of its bytes as keeps the count to COUNTED.
pub fn census(window: &[u8], fold: bool) -> Census {
    let stride = (window.len() / COUNTED).max(1);
    let mut occurrences = [0u64; 256];
    let mut total = 0;
    for &byte in window.iter().step_by(stride) {
        occurrences[folded(byte, fold) as usize] += 1;
        total += 1;
    }
    Census { occurrences, total }
}

/// How rare a byte is in the census, larger the rarer: the bytes counted that are other bytes.
pub fn magnitude(census: &Census, byte: u8) -> u64 {
    census.total - census.occurrences[byte as usize]
}

/// The anchors placed on a needle for a window: offsets into the needle, the first refuting most.
pub fn plan(window: &[u8], needle: &[u8], fold: bool) -> Vec<usize> {
    if needle.is_empty() || needle.len() > window.len() {
        return Vec::new();
    }
    let census = census(window, fold);
    // The candidates: each byte of the needle once, at its first offset, the rarest first; a byte the
    // needle repeats tests what its first offset tests at another place, which the descent may want.
    let mut candidates: Vec<usize> = (0..needle.len()).collect();
    candidates.sort_by_key(|&offset| std::cmp::Reverse(magnitude(&census, folded(needle[offset], fold))));
    candidates.truncate(CANDIDATES);
    let alignments = window.len() - needle.len() + 1;
    let stride = (alignments / SCORED).max(1);
    let mut standing: Vec<usize> = (0..alignments).step_by(stride).collect();
    let mut placed = Vec::new();
    while placed.len() < ANCHORS {
        let agree = |offset: usize, at: usize| folded(window[at + offset], fold) == folded(needle[offset], fold);
        let best = candidates.iter().filter(|offset| !placed.contains(*offset)).map(|&offset| (standing.iter().filter(|&&at| agree(offset, at)).count(), offset)).min();
        let Some((left, offset)) = best else { break };
        // A level that refutes nothing the levels above it did not ends the descent, and the first
        // level is always placed.
        if !placed.is_empty() && left >= standing.len() {
            break;
        }
        standing.retain(|&at| agree(offset, at));
        placed.push(offset);
    }
    placed
}

/// Every offset `needle` stands at in `text`, read a window at a time.
pub fn find_all(text: &[u8], needle: &[u8], fold: bool) -> Vec<usize> {
    find_all_on(best(), text, needle, fold)
}

/// `find_all` on the path given.
pub fn find_all_on(path: Path, text: &[u8], needle: &[u8], fold: bool) -> Vec<usize> {
    let mut found = Vec::new();
    if needle.is_empty() || needle.len() > text.len() {
        return found;
    }
    let needle: Vec<u8> = needle.iter().map(|&byte| folded(byte, fold)).collect();
    let step = WINDOW.max(needle.len() * 2) - (needle.len() - 1);
    let mut start = 0;
    while start + needle.len() <= text.len() {
        let end = (start + step + needle.len() - 1).min(text.len());
        let window = &text[start..end];
        let anchors = plan(window, &needle, fold);
        found.extend(window_on(path, window, &needle, &anchors, fold).into_iter().map(|at| at + start));
        start += step;
    }
    found
}

/// Whether the whole needle stands at `at`; the needle is folded already where `fold` is set.
fn stands_at(window: &[u8], needle: &[u8], at: usize, fold: bool) -> bool {
    let here = &window[at..at + needle.len()];
    if fold { here.iter().zip(needle).all(|(&one, &other)| one.to_ascii_lowercase() == other) } else { here == needle }
}

/// Whether the anchors from `from` on agree at `at`, and the whole needle stands there.
fn holds(window: &[u8], needle: &[u8], anchors: &[usize], from: usize, at: usize, fold: bool) -> bool {
    anchors[from..].iter().all(|&offset| folded(window[at + offset], fold) == needle[offset]) && stands_at(window, needle, at, fold)
}

/// The offsets the needle stands at in one window, under the anchors placed for it.
fn window_on(path: Path, window: &[u8], needle: &[u8], anchors: &[usize], fold: bool) -> Vec<usize> {
    let alignments = window.len() - needle.len() + 1;
    if anchors.is_empty() {
        return (0..alignments).filter(|&at| stands_at(window, needle, at, fold)).collect();
    }
    match path {
        #[cfg(target_arch = "x86_64")]
        Path::Avx2 if std::arch::is_x86_feature_detected!("avx2") => unsafe { x86::window_avx2(window, needle, anchors, fold) },
        #[cfg(target_arch = "x86_64")]
        Path::Sse2 | Path::Avx2 => unsafe { x86::window_sse2(window, needle, anchors, fold) },
        _ => plain_window(window, needle, anchors, fold, 0),
    }
}

/// The plain loop of a window, from the alignment `start` on.
fn plain_window(window: &[u8], needle: &[u8], anchors: &[usize], fold: bool, start: usize) -> Vec<usize> {
    let alignments = window.len() - needle.len() + 1;
    (start..alignments).filter(|&at| holds(window, needle, anchors, 0, at, fold)).collect()
}

#[cfg(target_arch = "x86_64")]
mod x86 {
    use super::{holds, plain_window};
    use std::arch::x86_64::*;

    /// A block of sixteen bytes as a search without case reads them where `fold` is set: each
    /// capital to its small letter.
    #[target_feature(enable = "sse2")]
    unsafe fn fold16(block: __m128i, fold: bool) -> __m128i {
        if !fold {
            return block;
        }
        let past = _mm_sub_epi8(block, _mm_set1_epi8(b'A' as i8));
        let upper = _mm_cmpeq_epi8(_mm_subs_epu8(past, _mm_set1_epi8(25)), _mm_setzero_si128());
        _mm_or_si128(block, _mm_and_si128(upper, _mm_set1_epi8(0x20)))
    }

    #[target_feature(enable = "avx2")]
    unsafe fn fold32(block: __m256i, fold: bool) -> __m256i {
        if !fold {
            return block;
        }
        let past = _mm256_sub_epi8(block, _mm256_set1_epi8(b'A' as i8));
        let upper = _mm256_cmpeq_epi8(_mm256_subs_epu8(past, _mm256_set1_epi8(25)), _mm256_setzero_si256());
        _mm256_or_si256(block, _mm256_and_si256(upper, _mm256_set1_epi8(0x20)))
    }

    /// The window sixteen alignments at a time: the first two anchors in one compare each, the rest
    /// and the whole needle on each alignment both agree with.
    #[target_feature(enable = "sse2")]
    pub unsafe fn window_sse2(window: &[u8], needle: &[u8], anchors: &[usize], fold: bool) -> Vec<usize> {
        unsafe {
            let alignments = window.len() - needle.len() + 1;
            let first = anchors[0];
            let second = *anchors.get(1).unwrap_or(&first);
            let want_first = _mm_set1_epi8(needle[first] as i8);
            let want_second = _mm_set1_epi8(needle[second] as i8);
            let mut found = Vec::new();
            let mut at = 0;
            while at + 16 <= alignments {
                let a = fold16(_mm_loadu_si128(window.as_ptr().add(at + first) as *const __m128i), fold);
                let b = fold16(_mm_loadu_si128(window.as_ptr().add(at + second) as *const __m128i), fold);
                let mut mask = _mm_movemask_epi8(_mm_and_si128(_mm_cmpeq_epi8(a, want_first), _mm_cmpeq_epi8(b, want_second))) as u32;
                while mask != 0 {
                    let here = at + mask.trailing_zeros() as usize;
                    if holds(window, needle, anchors, 2.min(anchors.len()), here, fold) {
                        found.push(here);
                    }
                    mask &= mask - 1;
                }
                at += 16;
            }
            found.extend(plain_window(window, needle, anchors, fold, at));
            found
        }
    }

    /// The window thirty-two alignments at a time, as `window_sse2` does sixteen.
    #[target_feature(enable = "avx2")]
    pub unsafe fn window_avx2(window: &[u8], needle: &[u8], anchors: &[usize], fold: bool) -> Vec<usize> {
        unsafe {
            let alignments = window.len() - needle.len() + 1;
            let first = anchors[0];
            let second = *anchors.get(1).unwrap_or(&first);
            let want_first = _mm256_set1_epi8(needle[first] as i8);
            let want_second = _mm256_set1_epi8(needle[second] as i8);
            let mut found = Vec::new();
            let mut at = 0;
            while at + 32 <= alignments {
                let a = fold32(_mm256_loadu_si256(window.as_ptr().add(at + first) as *const __m256i), fold);
                let b = fold32(_mm256_loadu_si256(window.as_ptr().add(at + second) as *const __m256i), fold);
                let mut mask = _mm256_movemask_epi8(_mm256_and_si256(_mm256_cmpeq_epi8(a, want_first), _mm256_cmpeq_epi8(b, want_second))) as u32;
                while mask != 0 {
                    let here = at + mask.trailing_zeros() as usize;
                    if holds(window, needle, anchors, 2.min(anchors.len()), here, fold) {
                        found.push(here);
                    }
                    mask &= mask - 1;
                }
                at += 32;
            }
            found.extend(plain_window(window, needle, anchors, fold, at));
            found
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::vector::paths;

    /// Where the needle stands, by comparing at every alignment.
    fn every(text: &[u8], needle: &[u8], fold: bool) -> Vec<usize> {
        if needle.is_empty() || needle.len() > text.len() {
            return Vec::new();
        }
        let low = |bytes: &[u8]| bytes.iter().map(|&byte| folded(byte, fold)).collect::<Vec<u8>>();
        let needle = low(needle);
        (0..=text.len() - needle.len()).filter(|&at| low(&text[at..at + needle.len()]) == needle).collect()
    }

    /// A run of bytes from a small generator, over `alphabet` symbols from `base`.
    fn made(length: usize, alphabet: u8, base: u8, seed: u64) -> Vec<u8> {
        let mut state = seed;
        (0..length)
            .map(|_| {
                state = state.wrapping_mul(6364136223846793005).wrapping_add(1442695040888963407);
                base + ((state >> 33) % alphabet as u64) as u8
            })
            .collect()
    }

    #[test]
    fn every_path_finds_what_the_plain_comparison_finds_on_texts_flat_skewed_and_repeating() {
        let mut texts = vec![made(5000, 4, b'a', 1), made(5000, 26, b'a', 2), made(5000, 2, b'x', 3), b"ab".repeat(2500), b"the quick brown fox jumps over the lazy dog\n".repeat(120)];
        let mut skewed = made(5000, 3, b'a', 4);
        for at in (0..5000).step_by(97) {
            skewed[at] = b'Q';
        }
        texts.push(skewed);
        for (index, text) in texts.iter().enumerate() {
            for (from, length) in [(10, 1), (100, 2), (200, 3), (300, 5), (1000, 8), (2000, 17), (40, 33), (3000, 64)] {
                let needle = &text[from..from + length];
                for path in paths() {
                    assert_eq!(find_all_on(path, text, needle, false), every(text, needle, false), "text {index}, needle {needle:?}, {path:?}");
                }
            }
            for path in paths() {
                assert_eq!(find_all_on(path, text, b"zzzz", false), every(text, b"zzzz", false), "text {index}, a needle the text lacks, {path:?}");
            }
        }
    }

    #[test]
    fn a_search_without_case_finds_each_letter_in_either_case_and_only_letters_fold() {
        let text = b"Fox fox FOX fOx f@x F`X fox\n".repeat(50);
        for path in paths() {
            assert_eq!(find_all_on(path, &text, b"fox", true), every(&text, b"fox", true), "{path:?}");
            assert_eq!(find_all_on(path, &text, b"FOX", true).len(), 4 * 50 + 50, "{path:?}");
            assert_eq!(find_all_on(path, &text, b"f@x", true).len(), 50, "@ is not ` in either case, {path:?}");
        }
    }

    #[test]
    fn a_needle_across_two_windows_is_found_once() {
        let mut text = vec![b'.'; WINDOW * 2 + 100];
        let needle = b"across the seam";
        for at in [WINDOW - 7, WINDOW * 2 - 3, 0, text.len() - needle.len()] {
            text[at..at + needle.len()].copy_from_slice(needle);
        }
        for path in paths() {
            let mut want = every(&text, needle, false);
            want.dedup();
            assert_eq!(find_all_on(path, &text, needle, false), want, "{path:?}");
        }
    }

    #[test]
    fn the_first_anchor_stands_on_the_rarest_byte_and_a_level_that_refutes_nothing_more_ends_the_descent() {
        let mut text = b"aaaaaaaaab".repeat(400);
        text.extend_from_slice(b"aaaQaaaa");
        let anchors = plan(&text, b"aaaQaaaa", false);
        assert_eq!(anchors[0], 3, "the Q");
        assert_eq!(anchors.len(), 1, "the text's one Q leaves the one alignment that holds the needle, which no anchor refutes: {anchors:?}");
        let flat = made(20000, 26, b'a', 9);
        assert_eq!(plan(&flat, &flat[100..108], false).len(), 3, "three anchors of twenty-six letters leave the needle's own alignment, and a fourth refutes nothing more");
    }

    #[test]
    fn empty_and_too_long_needles_stand_nowhere() {
        assert!(find_all(b"text", b"", false).is_empty());
        assert!(find_all(b"text", b"longer text", false).is_empty());
        assert!(find_all(b"", b"x", false).is_empty());
    }
}
