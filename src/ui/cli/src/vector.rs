// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The inner loops of the page's slow modules, over text held in UTF-16 units as the page's own
//! strings hold it, each written in the processor's vector instructions through `std::arch`: SSE2,
//! and AVX2 where the machine has it, on x86-64; NEON on aarch64; and a plain loop where neither is
//! there. Each path answers as the plain loop does, which the tests hold each one to on every path
//! the machine runs.
//!
//! A search without case folds as the page's own does with the `i` and `u` flags: each unit of the
//! text and of what is sought to its simple case fold. Only text sought in ASCII is searched here;
//! two units outside ASCII fold into it, `ſ` to `s` and the Kelvin sign to `k`, and are folded too.

use std::sync::OnceLock;

/// A way the loops run: the plain loop, or one of the processor's sets of vector instructions.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum Path {
    Plain,
    Sse2,
    Avx2,
    Neon,
}

/// The paths this machine runs, the plain loop first and the widest last.
pub fn paths() -> Vec<Path> {
    #[allow(unused_mut)]
    let mut found = vec![Path::Plain];
    #[cfg(target_arch = "x86_64")]
    {
        found.push(Path::Sse2);
        if std::arch::is_x86_feature_detected!("avx2") {
            found.push(Path::Avx2);
        }
    }
    #[cfg(target_arch = "aarch64")]
    found.push(Path::Neon);
    found
}

/// The widest path this machine runs, read once.
pub fn best() -> Path {
    static BEST: OnceLock<Path> = OnceLock::new();
    *BEST.get_or_init(|| *paths().last().unwrap_or(&Path::Plain))
}

const LONG_S: u16 = 0x017F;
const KELVIN: u16 = 0x212A;

/// A unit folded as the page's search without case folds it, for units that fold into ASCII.
pub fn fold(unit: u16) -> u16 {
    match unit {
        0x41..=0x5A => unit | 0x20,
        LONG_S => b's' as u16,
        KELVIN => b'k' as u16,
        _ => unit,
    }
}

/// Where `unit` first stands in `text`.
pub fn find_unit(text: &[u16], unit: u16) -> Option<usize> {
    find_unit_on(best(), text, unit)
}

/// Where `sought` first stands in `text` from `from` on, unit for unit.
pub fn find(text: &[u16], sought: &[u16], from: usize) -> Option<usize> {
    find_on(best(), text, sought, from)
}

/// Where `sought`, text in ASCII, first stands in `text` from `from` on, without case.
pub fn find_folded(text: &[u16], sought: &[u16], from: usize) -> Option<usize> {
    find_folded_on(best(), text, sought, from)
}

/// Whether two runs of units are the same.
pub fn same(a: &[u16], b: &[u16]) -> bool {
    same_on(best(), a, b)
}

/// How many of the numbers at the starts of `a` and `b` are the same, one for one.
pub fn common(a: &[u64], b: &[u64]) -> usize {
    common_on(best(), a, b)
}

/// `common` on the path given.
pub fn common_on(path: Path, a: &[u64], b: &[u64]) -> usize {
    let length = a.len().min(b.len());
    let (a, b) = (&a[..length], &b[..length]);
    match path {
        #[cfg(target_arch = "x86_64")]
        Path::Sse2 => unsafe { x86::common_sse2(a, b) },
        #[cfg(target_arch = "x86_64")]
        Path::Avx2 if std::arch::is_x86_feature_detected!("avx2") => unsafe { x86::common_avx2(a, b) },
        #[cfg(target_arch = "aarch64")]
        Path::Neon => unsafe { arm::common_neon(a, b) },
        _ => a.iter().zip(b).take_while(|(one, other)| one == other).count(),
    }
}

/// `find_unit` on the path given.
pub fn find_unit_on(path: Path, text: &[u16], unit: u16) -> Option<usize> {
    match path {
        #[cfg(target_arch = "x86_64")]
        Path::Sse2 => unsafe { x86::find_unit_sse2(text, unit) },
        #[cfg(target_arch = "x86_64")]
        Path::Avx2 if std::arch::is_x86_feature_detected!("avx2") => unsafe { x86::find_unit_avx2(text, unit) },
        #[cfg(target_arch = "aarch64")]
        Path::Neon => unsafe { arm::find_unit_neon(text, unit) },
        _ => text.iter().position(|&one| one == unit),
    }
}

/// `same` on the path given.
pub fn same_on(path: Path, a: &[u16], b: &[u16]) -> bool {
    if a.len() != b.len() {
        return false;
    }
    match path {
        #[cfg(target_arch = "x86_64")]
        Path::Sse2 => unsafe { x86::same_sse2(a, b) },
        #[cfg(target_arch = "x86_64")]
        Path::Avx2 if std::arch::is_x86_feature_detected!("avx2") => unsafe { x86::same_avx2(a, b) },
        #[cfg(target_arch = "aarch64")]
        Path::Neon => unsafe { arm::same_neon(a, b) },
        _ => a == b,
    }
}

/// `find` on the path given: each place both the first and the last unit sought stand where they
/// would, found a block at a time, then checked whole.
pub fn find_on(path: Path, text: &[u16], sought: &[u16], from: usize) -> Option<usize> {
    if sought.is_empty() {
        return (from <= text.len()).then_some(from);
    }
    if from >= text.len() || text.len() - from < sought.len() {
        return None;
    }
    let rest = &text[from..];
    if sought.len() == 1 {
        return find_unit_on(path, rest, sought[0]).map(|at| at + from);
    }
    let found = match path {
        #[cfg(target_arch = "x86_64")]
        Path::Sse2 => unsafe { x86::find_sse2(rest, sought, false) },
        #[cfg(target_arch = "x86_64")]
        Path::Avx2 if std::arch::is_x86_feature_detected!("avx2") => unsafe { x86::find_avx2(rest, sought, false) },
        #[cfg(target_arch = "aarch64")]
        Path::Neon => unsafe { arm::find_neon(rest, sought, false) },
        _ => plain_find(rest, sought, false, 0),
    };
    found.map(|at| at + from)
}

/// `find_folded` on the path given.
pub fn find_folded_on(path: Path, text: &[u16], sought: &[u16], from: usize) -> Option<usize> {
    let sought: Vec<u16> = sought.iter().map(|&unit| fold(unit)).collect();
    if sought.is_empty() {
        return (from <= text.len()).then_some(from);
    }
    if from >= text.len() || text.len() - from < sought.len() {
        return None;
    }
    let rest = &text[from..];
    let found = match path {
        #[cfg(target_arch = "x86_64")]
        Path::Sse2 => unsafe { x86::find_sse2(rest, &sought, true) },
        #[cfg(target_arch = "x86_64")]
        Path::Avx2 if std::arch::is_x86_feature_detected!("avx2") => unsafe { x86::find_avx2(rest, &sought, true) },
        #[cfg(target_arch = "aarch64")]
        Path::Neon => unsafe { arm::find_neon(rest, &sought, true) },
        _ => plain_find(rest, &sought, true, 0),
    };
    found.map(|at| at + from)
}

/// Whether `sought` stands in `text` at `at`, folded where `folded` is set; `sought` is folded already.
fn stands_at(text: &[u16], sought: &[u16], at: usize, folded: bool) -> bool {
    let here = &text[at..at + sought.len()];
    if folded { here.iter().zip(sought).all(|(&one, &other)| fold(one) == other) } else { here == sought }
}

/// The plain loop of `find`, from `start` on.
fn plain_find(text: &[u16], sought: &[u16], folded: bool, start: usize) -> Option<usize> {
    let last = text.len().checked_sub(sought.len())?;
    (start..=last).find(|&at| stands_at(text, sought, at, folded))
}

#[cfg(target_arch = "x86_64")]
mod x86 {
    use super::{plain_find, stands_at, KELVIN, LONG_S};
    use std::arch::x86_64::*;

    /// Each unit of a block of eight folded: A to Z to a to z, `ſ` to s and the Kelvin sign to k.
    #[target_feature(enable = "sse2")]
    unsafe fn fold8(block: __m128i) -> __m128i {
        let past = _mm_sub_epi16(block, _mm_set1_epi16(0x41));
        let upper = _mm_cmpeq_epi16(_mm_subs_epu16(past, _mm_set1_epi16(25)), _mm_setzero_si128());
        let lowered = _mm_or_si128(block, _mm_and_si128(upper, _mm_set1_epi16(0x20)));
        let long_s = _mm_cmpeq_epi16(block, _mm_set1_epi16(LONG_S as i16));
        let kelvin = _mm_cmpeq_epi16(block, _mm_set1_epi16(KELVIN as i16));
        let lowered = _mm_or_si128(_mm_andnot_si128(long_s, lowered), _mm_and_si128(long_s, _mm_set1_epi16(b's' as i16)));
        _mm_or_si128(_mm_andnot_si128(kelvin, lowered), _mm_and_si128(kelvin, _mm_set1_epi16(b'k' as i16)))
    }

    #[target_feature(enable = "avx2")]
    unsafe fn fold16(block: __m256i) -> __m256i {
        let past = _mm256_sub_epi16(block, _mm256_set1_epi16(0x41));
        let upper = _mm256_cmpeq_epi16(_mm256_subs_epu16(past, _mm256_set1_epi16(25)), _mm256_setzero_si256());
        let lowered = _mm256_or_si256(block, _mm256_and_si256(upper, _mm256_set1_epi16(0x20)));
        let long_s = _mm256_cmpeq_epi16(block, _mm256_set1_epi16(LONG_S as i16));
        let kelvin = _mm256_cmpeq_epi16(block, _mm256_set1_epi16(KELVIN as i16));
        let lowered = _mm256_blendv_epi8(lowered, _mm256_set1_epi16(b's' as i16), long_s);
        _mm256_blendv_epi8(lowered, _mm256_set1_epi16(b'k' as i16), kelvin)
    }

    #[target_feature(enable = "sse2")]
    pub unsafe fn find_unit_sse2(text: &[u16], unit: u16) -> Option<usize> {
        unsafe {
            let wanted = _mm_set1_epi16(unit as i16);
            let mut at = 0;
            while at + 8 <= text.len() {
                let block = _mm_loadu_si128(text.as_ptr().add(at) as *const __m128i);
                let mask = _mm_movemask_epi8(_mm_cmpeq_epi16(block, wanted)) as u32;
                if mask != 0 {
                    return Some(at + mask.trailing_zeros() as usize / 2);
                }
                at += 8;
            }
            text[at..].iter().position(|&one| one == unit).map(|found| at + found)
        }
    }

    #[target_feature(enable = "avx2")]
    pub unsafe fn find_unit_avx2(text: &[u16], unit: u16) -> Option<usize> {
        unsafe {
            let wanted = _mm256_set1_epi16(unit as i16);
            let mut at = 0;
            while at + 16 <= text.len() {
                let block = _mm256_loadu_si256(text.as_ptr().add(at) as *const __m256i);
                let mask = _mm256_movemask_epi8(_mm256_cmpeq_epi16(block, wanted)) as u32;
                if mask != 0 {
                    return Some(at + mask.trailing_zeros() as usize / 2);
                }
                at += 16;
            }
            find_unit_sse2(&text[at..], unit).map(|found| at + found)
        }
    }

    /// Two numbers at a time, each the same where all its eight bytes are, then the plain loop; `a`
    /// and `b` are as long as each other.
    #[target_feature(enable = "sse2")]
    pub unsafe fn common_sse2(a: &[u64], b: &[u64]) -> usize {
        unsafe {
            let mut at = 0;
            while at + 2 <= a.len() {
                let one = _mm_loadu_si128(a.as_ptr().add(at) as *const __m128i);
                let other = _mm_loadu_si128(b.as_ptr().add(at) as *const __m128i);
                let same = _mm_movemask_epi8(_mm_cmpeq_epi8(one, other)) as u32;
                if same != 0xFFFF {
                    return at + (!same).trailing_zeros() as usize / 8;
                }
                at += 2;
            }
            at + a[at..].iter().zip(&b[at..]).take_while(|(one, other)| one == other).count()
        }
    }

    #[target_feature(enable = "avx2")]
    pub unsafe fn common_avx2(a: &[u64], b: &[u64]) -> usize {
        unsafe {
            let mut at = 0;
            while at + 4 <= a.len() {
                let one = _mm256_loadu_si256(a.as_ptr().add(at) as *const __m256i);
                let other = _mm256_loadu_si256(b.as_ptr().add(at) as *const __m256i);
                let same = _mm256_movemask_epi8(_mm256_cmpeq_epi8(one, other)) as u32;
                if same != u32::MAX {
                    return at + (!same).trailing_zeros() as usize / 8;
                }
                at += 4;
            }
            at + common_sse2(&a[at..], &b[at..])
        }
    }

    #[target_feature(enable = "sse2")]
    pub unsafe fn same_sse2(a: &[u16], b: &[u16]) -> bool {
        unsafe {
            let mut at = 0;
            while at + 8 <= a.len() {
                let one = _mm_loadu_si128(a.as_ptr().add(at) as *const __m128i);
                let other = _mm_loadu_si128(b.as_ptr().add(at) as *const __m128i);
                if _mm_movemask_epi8(_mm_cmpeq_epi16(one, other)) != 0xFFFF {
                    return false;
                }
                at += 8;
            }
            a[at..] == b[at..]
        }
    }

    #[target_feature(enable = "avx2")]
    pub unsafe fn same_avx2(a: &[u16], b: &[u16]) -> bool {
        unsafe {
            let mut at = 0;
            while at + 16 <= a.len() {
                let one = _mm256_loadu_si256(a.as_ptr().add(at) as *const __m256i);
                let other = _mm256_loadu_si256(b.as_ptr().add(at) as *const __m256i);
                if _mm256_movemask_epi8(_mm256_cmpeq_epi16(one, other)) != -1 {
                    return false;
                }
                at += 16;
            }
            same_sse2(&a[at..], &b[at..])
        }
    }

    /// Each place in a block of eight where the first unit sought stands and the last stands where it
    /// would, each checked whole; folded where `folded` is set. Past the last whole block, the plain loop.
    #[target_feature(enable = "sse2")]
    pub unsafe fn find_sse2(text: &[u16], sought: &[u16], folded: bool) -> Option<usize> {
        unsafe {
            let span = sought.len() - 1;
            let first = _mm_set1_epi16(sought[0] as i16);
            let last = _mm_set1_epi16(sought[span] as i16);
            let mut at = 0;
            while at + span + 8 <= text.len() {
                let mut head = _mm_loadu_si128(text.as_ptr().add(at) as *const __m128i);
                let mut tail = _mm_loadu_si128(text.as_ptr().add(at + span) as *const __m128i);
                if folded {
                    head = fold8(head);
                    tail = fold8(tail);
                }
                let both = _mm_and_si128(_mm_cmpeq_epi16(head, first), _mm_cmpeq_epi16(tail, last));
                let mut mask = _mm_movemask_epi8(both) as u32 & 0x5555;
                while mask != 0 {
                    let place = at + mask.trailing_zeros() as usize / 2;
                    if stands_at(text, sought, place, folded) {
                        return Some(place);
                    }
                    mask &= mask - 1;
                }
                at += 8;
            }
            plain_find(text, sought, folded, at)
        }
    }

    #[target_feature(enable = "avx2")]
    pub unsafe fn find_avx2(text: &[u16], sought: &[u16], folded: bool) -> Option<usize> {
        unsafe {
            let span = sought.len() - 1;
            let first = _mm256_set1_epi16(sought[0] as i16);
            let last = _mm256_set1_epi16(sought[span] as i16);
            let mut at = 0;
            while at + span + 16 <= text.len() {
                let mut head = _mm256_loadu_si256(text.as_ptr().add(at) as *const __m256i);
                let mut tail = _mm256_loadu_si256(text.as_ptr().add(at + span) as *const __m256i);
                if folded {
                    head = fold16(head);
                    tail = fold16(tail);
                }
                let both = _mm256_and_si256(_mm256_cmpeq_epi16(head, first), _mm256_cmpeq_epi16(tail, last));
                let mut mask = _mm256_movemask_epi8(both) as u32 & 0x5555_5555;
                while mask != 0 {
                    let place = at + mask.trailing_zeros() as usize / 2;
                    if stands_at(text, sought, place, folded) {
                        return Some(place);
                    }
                    mask &= mask - 1;
                }
                at += 16;
            }
            find_sse2(&text[at..], sought, folded).map(|found| at + found)
        }
    }
}

#[cfg(target_arch = "aarch64")]
mod arm {
    use super::{plain_find, stands_at, KELVIN, LONG_S};
    use std::arch::aarch64::*;

    /// Which of eight lanes are set, each a byte of 0xFF or 0 in the answer's eight bytes.
    #[target_feature(enable = "neon")]
    unsafe fn lanes(mask: uint16x8_t) -> u64 {
        vget_lane_u64::<0>(vreinterpret_u64_u8(vshrn_n_u16::<4>(mask)))
    }

    #[target_feature(enable = "neon")]
    unsafe fn fold8(block: uint16x8_t) -> uint16x8_t {
        let upper = vcleq_u16(vsubq_u16(block, vdupq_n_u16(0x41)), vdupq_n_u16(25));
        let lowered = vorrq_u16(block, vandq_u16(upper, vdupq_n_u16(0x20)));
        let lowered = vbslq_u16(vceqq_u16(block, vdupq_n_u16(LONG_S)), vdupq_n_u16(b's' as u16), lowered);
        vbslq_u16(vceqq_u16(block, vdupq_n_u16(KELVIN)), vdupq_n_u16(b'k' as u16), lowered)
    }

    #[target_feature(enable = "neon")]
    pub unsafe fn find_unit_neon(text: &[u16], unit: u16) -> Option<usize> {
        unsafe {
            let wanted = vdupq_n_u16(unit);
            let mut at = 0;
            while at + 8 <= text.len() {
                let found = lanes(vceqq_u16(vld1q_u16(text.as_ptr().add(at)), wanted));
                if found != 0 {
                    return Some(at + found.trailing_zeros() as usize / 8);
                }
                at += 8;
            }
            text[at..].iter().position(|&one| one == unit).map(|found| at + found)
        }
    }

    #[target_feature(enable = "neon")]
    pub unsafe fn common_neon(a: &[u64], b: &[u64]) -> usize {
        unsafe {
            let mut at = 0;
            while at + 2 <= a.len() {
                let same = vceqq_u64(vld1q_u64(a.as_ptr().add(at)), vld1q_u64(b.as_ptr().add(at)));
                if vgetq_lane_u64::<0>(same) != u64::MAX {
                    return at;
                }
                if vgetq_lane_u64::<1>(same) != u64::MAX {
                    return at + 1;
                }
                at += 2;
            }
            at + a[at..].iter().zip(&b[at..]).take_while(|(one, other)| one == other).count()
        }
    }

    #[target_feature(enable = "neon")]
    pub unsafe fn same_neon(a: &[u16], b: &[u16]) -> bool {
        unsafe {
            let mut at = 0;
            while at + 8 <= a.len() {
                if vminvq_u16(vceqq_u16(vld1q_u16(a.as_ptr().add(at)), vld1q_u16(b.as_ptr().add(at)))) != 0xFFFF {
                    return false;
                }
                at += 8;
            }
            a[at..] == b[at..]
        }
    }

    #[target_feature(enable = "neon")]
    pub unsafe fn find_neon(text: &[u16], sought: &[u16], folded: bool) -> Option<usize> {
        unsafe {
            let span = sought.len() - 1;
            let first = vdupq_n_u16(sought[0]);
            let last = vdupq_n_u16(sought[span]);
            let mut at = 0;
            while at + span + 8 <= text.len() {
                let mut head = vld1q_u16(text.as_ptr().add(at));
                let mut tail = vld1q_u16(text.as_ptr().add(at + span));
                if folded {
                    head = fold8(head);
                    tail = fold8(tail);
                }
                let mut found = lanes(vandq_u16(vceqq_u16(head, first), vceqq_u16(tail, last)));
                while found != 0 {
                    let place = at + found.trailing_zeros() as usize / 8;
                    if stands_at(text, sought, place, folded) {
                        return Some(place);
                    }
                    found &= !(0xFFu64 << (found.trailing_zeros() / 8 * 8));
                }
                at += 8;
            }
            plain_find(text, sought, folded, at)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Text of every kind the loops meet: ASCII of both cases, letters outside ASCII, the two that
    /// fold into it, and units of surrogate pairs, from a seed.
    fn text_of(seed: u64, length: usize) -> Vec<u16> {
        let kinds: [u16; 16] = [b'a' as u16, b'b' as u16, b'A' as u16, b'B' as u16, b's' as u16, b'S' as u16, b'k' as u16, b'K' as u16, LONG_S, KELVIN, 0x00E9, 0x00C9, 0xD83D, 0xDE00, b' ' as u16, 0xFFFF];
        let mut state = seed.wrapping_mul(0x9E37_79B9_7F4A_7C15) | 1;
        (0..length)
            .map(|_| {
                state ^= state << 13;
                state ^= state >> 7;
                state ^= state << 17;
                kinds[(state % 16) as usize]
            })
            .collect()
    }

    #[test]
    fn every_path_finds_a_unit_where_the_plain_loop_does() {
        for seed in 0..300 {
            let text = text_of(seed, (seed as usize * 7) % 70);
            for unit in [b'a' as u16, b'K' as u16, KELVIN, 0xDE00, 0xFFFF, b'z' as u16] {
                let plain = find_unit_on(Path::Plain, &text, unit);
                for path in paths() {
                    assert_eq!(find_unit_on(path, &text, unit), plain, "{path:?} {seed} {unit:#x}");
                }
            }
        }
    }

    #[test]
    fn every_path_finds_a_run_with_case_and_without_where_the_plain_loop_does() {
        for seed in 0..400 {
            let text = text_of(seed, 10 + (seed as usize * 13) % 90);
            let start = (seed as usize * 3) % 8;
            let length = 1 + (seed as usize) % 6;
            let sought: Vec<u16> = if seed % 3 == 0 { text_of(seed + 1000, length) } else { text.iter().skip(start + 5).take(length).copied().collect() };
            for from in [0, 1, 9] {
                let plain = find_on(Path::Plain, &text, &sought, from);
                let ascii: Vec<u16> = sought.iter().map(|&unit| if unit < 0x80 { unit } else { b'x' as u16 }).collect();
                let plain_folded = find_folded_on(Path::Plain, &text, &ascii, from);
                for path in paths() {
                    assert_eq!(find_on(path, &text, &sought, from), plain, "{path:?} {seed} {from}");
                    assert_eq!(find_folded_on(path, &text, &ascii, from), plain_folded, "{path:?} {seed} {from} without case");
                }
            }
        }
    }

    #[test]
    fn a_search_without_case_folds_as_the_page_does() {
        let text: Vec<u16> = "Xx ſtar KING".encode_utf16().collect();
        let sought = |said: &str| said.encode_utf16().collect::<Vec<u16>>();
        for path in paths() {
            assert_eq!(find_folded_on(path, &text, &sought("star"), 0), Some(3), "{path:?}: ſ folds to s");
            assert_eq!(find_folded_on(path, &text, &sought("king"), 0), Some(8), "{path:?}: the Kelvin sign folds to k");
            assert_eq!(find_folded_on(path, &text, &sought("XX"), 0), Some(0), "{path:?}");
            assert_eq!(find_on(path, &text, &sought("star"), 0), None, "{path:?}: with case, ſ is not s");
        }
    }

    #[test]
    fn every_path_compares_runs_as_the_plain_loop_does() {
        for seed in 0..300 {
            let a = text_of(seed, (seed as usize) % 50);
            let mut b = a.clone();
            if seed % 2 == 1 && !b.is_empty() {
                let at = (seed as usize * 11) % b.len();
                b[at] ^= 1;
            }
            for path in paths() {
                assert_eq!(same_on(path, &a, &b), a == b, "{path:?} {seed}");
            }
        }
    }

    #[test]
    fn every_path_counts_the_numbers_two_runs_start_with_as_the_plain_loop_does() {
        for seed in 0..300u64 {
            let a: Vec<u64> = text_of(seed, (seed as usize * 5) % 60).iter().map(|&unit| (unit as u64 % 3) << ((seed % 8) * 8)).collect();
            let mut b = a.clone();
            if seed % 3 > 0 && !b.is_empty() {
                let at = (seed as usize * 7) % b.len();
                b[at] ^= 1 << ((seed % 8) * 8);
            }
            if seed % 5 == 0 {
                b.truncate(b.len() / 2);
            }
            let plain = common_on(Path::Plain, &a, &b);
            for path in paths() {
                assert_eq!(common_on(path, &a, &b), plain, "{path:?} {seed}");
            }
        }
    }

    #[test]
    fn the_machine_runs_its_widest_path() {
        let found = paths();
        assert_eq!(found[0], Path::Plain);
        if cfg!(target_arch = "x86_64") {
            assert!(found.contains(&Path::Sse2));
        }
        if cfg!(target_arch = "aarch64") {
            assert!(found.contains(&Path::Neon));
        }
        assert_eq!(best(), *found.last().unwrap());
    }
}
