// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The lines of a text that differ from an older text of it, as the page's own diff marks them, step
//! for step: added, changed, and where lines were taken out. The lines the two share at their start
//! and end are set aside first, compared a block of units at a time, and Myers' shortest edit runs on
//! what is left, each line read as its hash, its runs of the same lines found a block of hashes at a
//! time and each line of a run checked against the other's, so that two lines that hash alike and
//! differ end the run. Past MOST_EDITS the texts read as too far apart to mark.

use crate::document::{Changes, Hunk};
use crate::vector;

const MOST_EDITS: usize = 3000;

/// A line's hash: its units four at a time, each word multiplied and turned in, then its length.
fn hash_of(line: &[u16]) -> u64 {
    let mut hash: u64 = 0xCBF2_9CE4_8422_2325;
    let mut words = line.chunks_exact(4);
    for word in &mut words {
        let packed = word[0] as u64 | (word[1] as u64) << 16 | (word[2] as u64) << 32 | (word[3] as u64) << 48;
        hash = (hash ^ packed).wrapping_mul(0x9E37_79B9_7F4A_7C15).rotate_left(29);
    }
    for &unit in words.remainder() {
        hash = (hash ^ unit as u64).wrapping_mul(0x9E37_79B9_7F4A_7C15).rotate_left(29);
    }
    (hash ^ line.len() as u64).wrapping_mul(0xFF51_AFD7_ED55_8CCD)
}

/// How many lines of `then` from `x` and of `now` from `y` are the same, one for one: the run their
/// hashes share, as far as each line of it is the same as the other's.
fn run(hashes: (&[u64], &[u64]), lines: (&[Vec<u16>], &[Vec<u16>]), x: usize, y: usize) -> usize {
    let alike = vector::common(&hashes.0[x..], &hashes.1[y..]);
    (0..alike).take_while(|&at| vector::same(&lines.0[x + at], &lines.1[y + at])).count()
}

/// The shortest edit from `a` to `b` as [kind, index] in order: kind 0 keeps a[i], 1 takes a[i] out,
/// 2 puts b[i] in. None past MOST_EDITS.
fn shortest(a: &[Vec<u16>], b: &[Vec<u16>]) -> Option<Vec<(u8, usize)>> {
    let hashes: (Vec<u64>, Vec<u64>) = (a.iter().map(|line| hash_of(line)).collect(), b.iter().map(|line| hash_of(line)).collect());
    let n = a.len() as i64;
    let m = b.len() as i64;
    let most = (n + m).min(MOST_EDITS as i64);
    let middle = most + 1;
    let mut v = vec![0i64; (2 * most + 3) as usize];
    let mut trace: Vec<Vec<i64>> = Vec::new();
    let mut found = None;
    let at = |v: &[i64], k: i64| v[(middle + k) as usize];
    'rounds: for d in 0..=most {
        trace.push(v[(middle - d) as usize..(middle + d + 1) as usize].to_vec());
        let mut k = -d;
        while k <= d {
            let mut x = if k == -d || (k != d && at(&v, k - 1) < at(&v, k + 1)) { at(&v, k + 1) } else { at(&v, k - 1) + 1 };
            let mut y = x - k;
            if x < n && y < m {
                let same = run((&hashes.0, &hashes.1), (a, b), x as usize, y as usize) as i64;
                x += same;
                y += same;
            }
            v[(middle + k) as usize] = x;
            if x >= n && y >= m {
                found = Some(d);
                break 'rounds;
            }
            k += 2;
        }
    }
    let found = found?;
    let mut steps = Vec::new();
    let (mut x, mut y) = (n, m);
    for d in (1..=found).rev() {
        let band = &trace[d as usize];
        let at = |k: i64| band[(k + d) as usize];
        let k = x - y;
        let back = if k == -d || (k != d && at(k - 1) < at(k + 1)) { k + 1 } else { k - 1 };
        let back_x = at(back);
        let back_y = back_x - back;
        while x > back_x && y > back_y {
            x -= 1;
            y -= 1;
            steps.push((0, x as usize));
        }
        if x == back_x {
            y -= 1;
            steps.push((2, y as usize));
        } else {
            x -= 1;
            steps.push((1, x as usize));
        }
    }
    while x > 0 && y > 0 {
        x -= 1;
        y -= 1;
        steps.push((0, x as usize));
    }
    steps.reverse();
    Some(steps)
}

/// The marks for `now` against `then`, as the page's `lineChanges` gives them. None where the two
/// are too far apart to mark.
pub fn line_changes(then: &[Vec<u16>], now: &[Vec<u16>]) -> Option<Changes> {
    let mut start = 0;
    while start < then.len() && start < now.len() && vector::same(&then[start], &now[start]) {
        start += 1;
    }
    let mut end = 0;
    while end < then.len() - start && end < now.len() - start && vector::same(&then[then.len() - 1 - end], &now[now.len() - 1 - end]) {
        end += 1;
    }
    let steps = shortest(&then[start..then.len() - end], &now[start..now.len() - end])?;
    let mut added = Vec::new();
    let mut changed = Vec::new();
    let mut removed = Vec::new();
    let mut hunks = Vec::new();
    let mut line = start;
    let mut then_line = start;
    let mut out = 0;
    let mut put: Vec<usize> = Vec::new();
    let mut close = |line: usize, then_line: usize, out: &mut usize, put: &mut Vec<usize>| {
        if !put.is_empty() {
            if *out > 0 { changed.extend(put.iter().copied()) } else { added.extend(put.iter().copied()) }
        } else if *out > 0 {
            removed.push(line);
        }
        if !put.is_empty() || *out > 0 {
            hunks.push(Hunk { now: [line - put.len(), line], then: [then_line - *out, then_line], then_lines: Vec::new() });
        }
        *out = 0;
        put.clear();
    };
    for (kind, _) in steps {
        match kind {
            0 => {
                close(line, then_line, &mut out, &mut put);
                line += 1;
                then_line += 1;
            }
            1 => {
                out += 1;
                then_line += 1;
            }
            _ => {
                put.push(line);
                line += 1;
            }
        }
    }
    close(line, then_line, &mut out, &mut put);
    Some(Changes { added, changed, removed, hunks })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::document::lines_of;

    fn hunk(now: [usize; 2], then: [usize; 2]) -> Hunk {
        Hunk { now, then, then_lines: Vec::new() }
    }

    fn changes(then: &str, now: &str) -> Option<Changes> {
        line_changes(&lines_of(then), &lines_of(now))
    }

    #[test]
    fn lines_put_in_taken_out_and_changed_are_marked_as_the_page_marks_them() {
        let found = changes("a\nb\nc\nd\ne", "a\nB\nc\nnew\nd").unwrap();
        assert_eq!(found.changed, vec![1]);
        assert_eq!(found.added, vec![3]);
        assert_eq!(found.removed, vec![5], "e taken out below the last line");
        assert_eq!(found.hunks, vec![hunk([1, 2], [1, 2]), hunk([3, 4], [3, 3]), hunk([5, 5], [4, 5])]);
    }

    #[test]
    fn the_same_text_has_no_marks_and_texts_too_far_apart_have_none_at_all() {
        let same = changes("a\nb", "a\nb").unwrap();
        assert!(same.added.is_empty() && same.changed.is_empty() && same.removed.is_empty() && same.hunks.is_empty());
        let then: String = (0..4000).map(|at| format!("old {at}\n")).collect();
        let now: String = (0..4000).map(|at| format!("new {at}\n")).collect();
        assert_eq!(changes(&then, &now), None);
    }

    #[test]
    fn lines_that_hash_alike_and_differ_are_told_apart() {
        let then = vec![vec![1u16, 2], vec![3]];
        let now = vec![vec![1u16, 2], vec![4]];
        let hashes = (vec![7u64, 7], vec![7u64, 7]);
        assert_eq!(run((&hashes.0, &hashes.1), (&then, &now), 0, 0), 1, "the run ends at the line that differs");
    }

    #[test]
    fn a_long_text_with_lines_changed_far_apart_marks_each() {
        let then: Vec<String> = (0..50_000).map(|at| format!("x_{at} = {at} * 2")).collect();
        let mut now = then.clone();
        for at in (1000..50_000).step_by(5000) {
            now[at].push_str(" changed");
        }
        let found = changes(&then.join("\n"), &now.join("\n")).unwrap();
        assert_eq!(found.changed, (1000..50_000).step_by(5000).collect::<Vec<_>>());
        assert!(found.added.is_empty() && found.removed.is_empty());
    }
}
