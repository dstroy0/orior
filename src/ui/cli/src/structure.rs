// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Compare by structure: two texts set side by side by their tokens, the names, numbers, strings and
//! signs of the code, and not by their characters. White space and line breaks between tokens are
//! layout, and a line whose tokens did not change is reshaped and not changed.
//!
//! The lines are compared first, each as the tokens it holds. A line taken out in one place and put
//! in at another with the same tokens, MOVED_TOKENS of them at least, is moved. What is left of each
//! change is compared token by token: a name changed to one other name everywhere it changed is
//! renamed, and every other token is taken out or put in. Past MOST_EDITS edits, at either step, the
//! two texts read as too far apart.

use std::collections::{HashMap, VecDeque};

use serde::Serialize;

/// The most edits either comparison makes before the texts read as too far apart.
const MOST_EDITS: usize = 3000;

/// The fewest tokens a line holds for it to read as moved.
const MOVED_TOKENS: usize = 2;

/// What a line, or a token of it, is: the same in both texts, changed, moved, reshaped, or changed
/// only by names renamed.
pub const SAME: u8 = 0;
pub const CHANGED: u8 = 1;
pub const MOVED: u8 = 2;
pub const RESHAPED: u8 = 3;
pub const RENAMED: u8 = 4;

/// A token: its line, where it starts and ends in the line in UTF-16 units as the window counts, and
/// where it stands in the text in bytes.
#[derive(Clone, Copy)]
struct Token {
    line: usize,
    from: usize,
    to: usize,
    start: usize,
    end: usize,
}

/// The two texts compared. `rows` sets their lines side by side, a line of one with no line across
/// from it in the other standing beside nothing, and `starts` holds the row each change starts at.
/// `left` and `right` say what each line of each text is, and their marks what each token that is not
/// the same is, as `[line, from, to, kind]`. `renames` holds each name renamed, what it became, and in
/// how many places.
#[derive(Serialize, Debug)]
pub struct Compared {
    pub rows: Vec<(Option<usize>, Option<usize>)>,
    pub starts: Vec<usize>,
    pub left: Vec<u8>,
    pub right: Vec<u8>,
    pub left_marks: Vec<[usize; 4]>,
    pub right_marks: Vec<[usize; 4]>,
    pub renames: Vec<(String, String, usize)>,
    pub moved: usize,
    pub reshaped: usize,
}

fn starts_name(letter: char) -> bool {
    letter.is_alphabetic() || letter == '_' || letter == '$'
}

fn in_name(letter: char) -> bool {
    letter.is_alphanumeric() || letter == '_' || letter == '$'
}

/// The tokens of a text, line by line: a name, a number, a string to its closing quote or the line's
/// end, or one sign.
fn lex(text: &str) -> Vec<Token> {
    let mut tokens = Vec::new();
    let mut line = 0;
    let mut col = 0;
    let mut letters = text.char_indices().peekable();
    while let Some((start, letter)) = letters.next() {
        if letter == '\n' {
            line += 1;
            col = 0;
            continue;
        }
        if letter.is_whitespace() {
            col += letter.len_utf16();
            continue;
        }
        let from = col;
        col += letter.len_utf16();
        let mut end = start + letter.len_utf8();
        let mut take = |keep: &mut dyn FnMut(char) -> bool, col: &mut usize| {
            while let Some(&(at, next)) = letters.peek() {
                if next == '\n' || !keep(next) {
                    break;
                }
                *col += next.len_utf16();
                end = at + next.len_utf8();
                letters.next();
            }
        };
        if starts_name(letter) {
            take(&mut in_name, &mut col);
        } else if letter.is_ascii_digit() {
            take(&mut |next| next.is_alphanumeric() || next == '.' || next == '_', &mut col);
        } else if matches!(letter, '"' | '\'' | '`') {
            let mut escaped = false;
            let mut closed = false;
            take(
                &mut |next| {
                    if closed {
                        return false;
                    }
                    if !escaped && next == letter {
                        closed = true;
                    }
                    escaped = !escaped && next == '\\';
                    true
                },
                &mut col,
            );
        }
        tokens.push(Token { line, from, to: col, start, end });
    }
    tokens
}

/// A step of an edit from one sequence to another.
#[derive(Clone, Copy, PartialEq, Debug)]
enum Step {
    Keep(usize, usize),
    Out(usize),
    In(usize),
}

/// The shortest edit from `a` to `b`, the ends they share set aside first, or None past MOST_EDITS.
fn shortest(a: &[u32], b: &[u32]) -> Option<Vec<Step>> {
    let head = a.iter().zip(b).take_while(|(x, y)| x == y).count();
    let tail = a[head..].iter().rev().zip(b[head..].iter().rev()).take_while(|(x, y)| x == y).count();
    let core = myers(&a[head..a.len() - tail], &b[head..b.len() - tail])?;
    let mut steps: Vec<Step> = (0..head).map(|at| Step::Keep(at, at)).collect();
    steps.extend(core.into_iter().map(|step| match step {
        Step::Keep(x, y) => Step::Keep(x + head, y + head),
        Step::Out(x) => Step::Out(x + head),
        Step::In(y) => Step::In(y + head),
    }));
    steps.extend((0..tail).map(|back| Step::Keep(a.len() - tail + back, b.len() - tail + back)));
    Some(steps)
}

/// Myers' shortest edit. It keeps one band of its frontier for each edit, and what it keeps grows with
/// the edits and not with the sequences.
fn myers(a: &[u32], b: &[u32]) -> Option<Vec<Step>> {
    let (n, m) = (a.len() as i64, b.len() as i64);
    let most = (n + m).min(MOST_EDITS as i64);
    let middle = most + 1;
    let mut v = vec![0i64; (2 * most + 3) as usize];
    let mut trace: Vec<Vec<i64>> = Vec::new();
    let mut found = None;
    'search: for d in 0..=most {
        trace.push(v[(middle - d) as usize..=(middle + d) as usize].to_vec());
        let mut k = -d;
        while k <= d {
            let at = (middle + k) as usize;
            let mut x = if k == -d || (k != d && v[at - 1] < v[at + 1]) { v[at + 1] } else { v[at - 1] + 1 };
            let mut y = x - k;
            while x < n && y < m && a[x as usize] == b[y as usize] {
                x += 1;
                y += 1;
            }
            v[at] = x;
            if x >= n && y >= m {
                found = Some(d);
                break 'search;
            }
            k += 2;
        }
    }
    let found = found?;
    let (mut x, mut y) = (n, m);
    let mut steps = Vec::new();
    for d in (1..=found).rev() {
        let before = &trace[d as usize];
        let get = |k: i64| before[(k + d) as usize];
        let k = x - y;
        let down = k == -d || (k != d && get(k - 1) < get(k + 1));
        let back_k = if down { k + 1 } else { k - 1 };
        let back_x = get(back_k);
        let back_y = back_x - back_k;
        while x > back_x && y > back_y {
            x -= 1;
            y -= 1;
            steps.push(Step::Keep(x as usize, y as usize));
        }
        if down {
            y -= 1;
            steps.push(Step::In(y as usize));
        } else {
            x -= 1;
            steps.push(Step::Out(x as usize));
        }
    }
    while x > 0 && y > 0 {
        x -= 1;
        y -= 1;
        steps.push(Step::Keep(x as usize, y as usize));
    }
    steps.reverse();
    Some(steps)
}

/// A number for each distinct piece of text, the same for the same text in either side.
#[derive(Default)]
struct Names(HashMap<String, u32>);

impl Names {
    fn of(&mut self, text: &str) -> u32 {
        let next = self.0.len() as u32;
        *self.0.entry(text.to_string()).or_insert(next)
    }
}

/// One side: its text, its tokens, and the tokens of each line.
struct Side<'a> {
    text: &'a str,
    tokens: Vec<Token>,
    lines: Vec<Vec<usize>>,
}

impl<'a> Side<'a> {
    fn new(text: &'a str) -> Self {
        let tokens = lex(text);
        let count = text.split('\n').count();
        let mut lines = vec![Vec::new(); count];
        for (at, token) in tokens.iter().enumerate() {
            lines[token.line].push(at);
        }
        Side { text, tokens, lines }
    }

    fn token(&self, at: usize) -> &str {
        &self.text[self.tokens[at].start..self.tokens[at].end]
    }

    fn mark(&self, at: usize, kind: u8) -> [usize; 4] {
        let token = self.tokens[at];
        [token.line, token.from, token.to, kind as usize]
    }
}

/// Compares `then`, the older text, with `now` by structure, or gives None where they are too far
/// apart.
pub fn compare(then: &str, now: &str) -> Option<Compared> {
    let (old, new) = (Side::new(then), Side::new(now));
    let mut names = Names::default();
    let keys = |side: &Side, names: &mut Names| -> Vec<u32> { side.lines.iter().map(|line| names.of(&line.iter().map(|&at| side.token(at)).collect::<Vec<_>>().join("\u{1}"))).collect() };
    let (old_keys, new_keys) = (keys(&old, &mut names), keys(&new, &mut names));
    let steps = shortest(&old_keys, &new_keys)?;

    // The changes: each run of lines taken out and put in between lines kept, with the row it starts at.
    let mut rows = Vec::new();
    let mut starts = Vec::new();
    let mut hunks: Vec<(Vec<usize>, Vec<usize>)> = Vec::new();
    let mut at = 0;
    while at < steps.len() {
        if let Step::Keep(x, y) = steps[at] {
            rows.push((Some(x), Some(y)));
            at += 1;
            continue;
        }
        let (mut outs, mut ins) = (Vec::new(), Vec::new());
        while at < steps.len() {
            match steps[at] {
                Step::Out(x) => outs.push(x),
                Step::In(y) => ins.push(y),
                Step::Keep(..) => break,
            }
            at += 1;
        }
        starts.push(rows.len());
        for row in 0..outs.len().max(ins.len()) {
            rows.push((outs.get(row).copied(), ins.get(row).copied()));
        }
        hunks.push((outs, ins));
    }

    // Moved: a line taken out and one put in elsewhere with the same tokens, matched in order.
    let mut left = vec![SAME; old.lines.len()];
    let mut right = vec![SAME; new.lines.len()];
    let mut gone: HashMap<u32, VecDeque<usize>> = HashMap::new();
    for (outs, _) in &hunks {
        for &line in outs.iter().filter(|&&line| old.lines[line].len() >= MOVED_TOKENS) {
            gone.entry(old_keys[line]).or_default().push_back(line);
        }
    }
    let mut moved = 0;
    for (_, ins) in &hunks {
        for &line in ins.iter().filter(|&&line| new.lines[line].len() >= MOVED_TOKENS) {
            if let Some(from) = gone.get_mut(&new_keys[line]).and_then(VecDeque::pop_front) {
                left[from] = MOVED;
                right[line] = MOVED;
                moved += 1;
            }
        }
    }
    // A moved line takes along a changed line beside it with tokens, where one beside a moved line on
    // the other side holds the same tokens: a block's closing sign moves with the block.
    let beside_moved = |kinds: &[u8], line: usize| (line > 0 && kinds[line - 1] == MOVED) || kinds.get(line + 1) == Some(&MOVED);
    loop {
        let mut near: HashMap<u32, VecDeque<usize>> = HashMap::new();
        for (outs, _) in &hunks {
            for &line in outs.iter().filter(|&&line| left[line] != MOVED && !old.lines[line].is_empty() && beside_moved(&left, line)) {
                near.entry(old_keys[line]).or_default().push_back(line);
            }
        }
        let mut grown = false;
        for (_, ins) in &hunks {
            for &line in ins {
                if right[line] == MOVED || new.lines[line].is_empty() || !beside_moved(&right, line) {
                    continue;
                }
                if let Some(from) = near.get_mut(&new_keys[line]).and_then(VecDeque::pop_front) {
                    left[from] = MOVED;
                    right[line] = MOVED;
                    moved += 1;
                    grown = true;
                }
            }
        }
        if !grown {
            break;
        }
    }
    let mut left_marks = Vec::new();
    let mut right_marks = Vec::new();
    for (line, kind) in left.iter().enumerate().filter(|(_, kind)| **kind == MOVED) {
        left_marks.extend(old.lines[line].iter().map(|&at| old.mark(at, *kind)));
    }
    for (line, kind) in right.iter().enumerate().filter(|(_, kind)| **kind == MOVED) {
        right_marks.extend(new.lines[line].iter().map(|&at| new.mark(at, *kind)));
    }

    // What is left of each change, token by token, and the names changed in pairs.
    let mut pairs: Vec<(usize, usize)> = Vec::new();
    let mut touched: Vec<(Vec<usize>, Vec<usize>)> = Vec::new();
    for (outs, ins) in &hunks {
        let out_tokens: Vec<usize> = outs.iter().filter(|&&line| left[line] != MOVED).flat_map(|&line| old.lines[line].clone()).collect();
        let in_tokens: Vec<usize> = ins.iter().filter(|&&line| right[line] != MOVED).flat_map(|&line| new.lines[line].clone()).collect();
        let a: Vec<u32> = out_tokens.iter().map(|&at| names.of(old.token(at))).collect();
        let b: Vec<u32> = in_tokens.iter().map(|&at| names.of(new.token(at))).collect();
        let edit = shortest(&a, &b)?;
        let (mut run_out, mut run_in) = (Vec::new(), Vec::new());
        let mut flush = |run_out: &mut Vec<usize>, run_in: &mut Vec<usize>| {
            let named = |side: &Side, at: usize| side.token(at).chars().next().is_some_and(starts_name);
            if run_out.len() == run_in.len() && run_out.iter().zip(run_in.iter()).all(|(&x, &y)| named(&old, x) && named(&new, y)) {
                pairs.extend(run_out.iter().copied().zip(run_in.iter().copied()));
            }
            touched.push((std::mem::take(run_out), std::mem::take(run_in)));
        };
        for step in edit {
            match step {
                Step::Out(x) => run_out.push(out_tokens[x]),
                Step::In(y) => run_in.push(in_tokens[y]),
                Step::Keep(..) => {
                    if !run_out.is_empty() || !run_in.is_empty() {
                        flush(&mut run_out, &mut run_in);
                    }
                }
            }
        }
        if !run_out.is_empty() || !run_in.is_empty() {
            flush(&mut run_out, &mut run_in);
        }
    }

    // A name is renamed where every place it changed in a pair it became the same other name.
    let mut became: HashMap<String, Option<(String, usize)>> = HashMap::new();
    for &(x, y) in &pairs {
        let (from, to) = (old.token(x).to_string(), new.token(y).to_string());
        let entry = became.entry(from).or_insert_with(|| Some((to.clone(), 0)));
        *entry = entry.take().filter(|(was, _)| *was == to).map(|(was, count)| (was, count + 1));
    }
    let renamed = |x: usize, y: usize| became.get(old.token(x)).and_then(Option::as_ref).is_some_and(|(to, _)| to == new.token(y));
    let mut renames: Vec<(String, String, usize)> = became.iter().filter_map(|(from, to)| to.as_ref().map(|(to, count)| (from.clone(), to.clone(), *count))).collect();
    renames.sort();
    for (outs, ins) in &touched {
        let paired = outs.len() == ins.len();
        for (place, &x) in outs.iter().enumerate() {
            let kind = if paired && renamed(x, ins[place]) { RENAMED } else { CHANGED };
            left_marks.push(old.mark(x, kind));
        }
        for (place, &y) in ins.iter().enumerate() {
            let kind = if paired && renamed(outs[place], y) { RENAMED } else { CHANGED };
            right_marks.push(new.mark(y, kind));
        }
    }

    // Each line of a change that is not moved: changed where a token of it changed, renamed where only
    // names were renamed, and reshaped where none of its tokens changed.
    let mut reshaped = 0;
    let kinds = |marks: &[[usize; 4]], count: usize| {
        let mut worst = vec![SAME; count];
        for mark in marks {
            let kind = mark[3] as u8;
            if kind == CHANGED || worst[mark[0]] == SAME {
                worst[mark[0]] = kind;
            }
        }
        worst
    };
    let (left_kinds, right_kinds) = (kinds(&left_marks, old.lines.len()), kinds(&right_marks, new.lines.len()));
    for (outs, ins) in &hunks {
        for &line in outs {
            left[line] = if left_kinds[line] == SAME { RESHAPED } else { left_kinds[line] };
        }
        for &line in ins {
            right[line] = if right_kinds[line] == SAME { RESHAPED } else { right_kinds[line] };
            reshaped += usize::from(right[line] == RESHAPED);
        }
    }
    left_marks.sort_unstable();
    right_marks.sort_unstable();
    Some(Compared { rows, starts, left, right, left_marks, right_marks, renames, moved, reshaped })
}

#[cfg(test)]
mod tests {
    use super::{CHANGED, MOVED, RENAMED, RESHAPED, SAME, compare, lex};

    #[test]
    fn tokens_are_names_numbers_strings_and_signs() {
        let text = "let x_1 = 3.5 + \"a \\\" b\";\n  é(y)";
        let tokens: Vec<&str> = lex(text).iter().map(|token| &text[token.start..token.end]).collect();
        assert_eq!(tokens, ["let", "x_1", "=", "3.5", "+", "\"a \\\" b\"", ";", "é", "(", "y", ")"]);
        let last = lex(text).last().copied().unwrap();
        assert_eq!((last.line, last.from, last.to), (1, 5, 6));
    }

    #[test]
    fn layout_alone_is_no_change() {
        let then = "f(a, b);\nreturn x;\n";
        let now = "f(\n    a,\n    b\n);\n\nreturn   x;\n";
        let found = compare(then, now).unwrap();
        assert!(found.left_marks.is_empty() && found.right_marks.is_empty(), "{found:?}");
        assert!(found.right.iter().all(|&kind| kind == SAME || kind == RESHAPED));
        assert!(found.reshaped > 0);
    }

    #[test]
    fn a_line_moved_reads_as_moved_and_a_name_changed_everywhere_as_renamed() {
        let then = "fn one() {}\nfn two() {}\nlet count = count + 1;\nprint(count);\n";
        let now = "fn two() {}\nlet total = total + 1;\nprint(total);\nfn one() {}\n";
        let found = compare(then, now).unwrap();
        assert_eq!(found.moved, 1, "{found:?}");
        assert_eq!(found.left[0], MOVED);
        assert_eq!(found.right[3], MOVED);
        assert_eq!(found.renames, [("count".to_string(), "total".to_string(), 3)]);
        assert_eq!(found.right[1], RENAMED);
        assert_eq!(found.right[2], RENAMED);
        assert!(found.right_marks.iter().filter(|mark| mark[0] == 1).all(|mark| mark[3] == RENAMED as usize));
    }

    #[test]
    fn a_block_moved_takes_its_closing_sign_along() {
        let then = "function one() {\n  return 1;\n}\nfunction two() {\n  return 2;\n}\n";
        let now = "function two() {\n  return 2;\n}\nfunction one() {\n  return 1;\n}\n";
        let found = compare(then, now).unwrap();
        assert_eq!(found.moved, 3, "{found:?}");
        assert_eq!(&found.left[..3], [MOVED, MOVED, MOVED], "{found:?}");
        // Every closing sign is alike: the one that moved on the right is whichever the lines kept leave.
        assert_eq!(found.right.iter().filter(|&&kind| kind == MOVED).count(), 3);
        assert!(found.left_marks.iter().chain(&found.right_marks).all(|mark| mark[3] == MOVED as usize));
    }

    #[test]
    fn a_token_changed_is_marked_where_it_stands() {
        let found = compare("let a = 1;\n", "let a = 2;\n").unwrap();
        assert_eq!(found.left_marks, [[0, 8, 9, CHANGED as usize]]);
        assert_eq!(found.right_marks, [[0, 8, 9, CHANGED as usize]]);
        assert_eq!(found.rows, [(Some(0), Some(0)), (Some(1), Some(1))]);
        assert_eq!(found.starts, [0]);
        assert!(found.renames.is_empty());
    }

    #[test]
    fn a_name_changed_two_ways_is_not_renamed() {
        let found = compare("a(x);\nb(x);\n", "a(y);\nb(z);\n").unwrap();
        assert!(found.renames.is_empty());
        assert!(found.right_marks.iter().all(|mark| mark[3] == CHANGED as usize));
    }
}
