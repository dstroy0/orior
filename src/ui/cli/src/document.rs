// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The texts open in the page's editors, held on this side as the page holds them, for the page's
//! slow work on a large text to run here and hand back only its answer. Each text is a list of lines
//! of UTF-16 units, as the page's own strings hold them, split as the page splits a file it reads: at
//! each "\n", a "\r" before it taken off. The page writes each change it makes to a text here as it
//! makes it, in the edits its document writes, and a text held here goes once the page lets it go.
//!
//! Find runs here only where its answer is the page's own: text sought as it is, with case, or
//! without case where it is ASCII, and whole words where each match stands between ASCII. A pattern
//! is the page's own: the page's engine matches the tests' long file several times as fast as this
//! side's, and any other search is the page's own too. Find reads the text's lines joined, each ended
//! by "\n", which no line holds and nothing sought holds, in one pass a block of units at a time,
//! kept until the text next changes.

use std::collections::HashMap;
use std::sync::{Mutex, OnceLock};

use serde::{Deserialize, Serialize};

use crate::vector;

/// A place in a text: its line and its col, both from 0, the col in UTF-16 units.
#[derive(Debug, Clone, Copy, Deserialize, Serialize, PartialEq, Eq)]
pub struct Place {
    pub line: usize,
    pub col: usize,
}

/// One edit of a change, as the page's document writes it: the span it removes and what it writes.
#[derive(Debug, Clone, Deserialize)]
pub struct Edit {
    pub from: Place,
    pub to: Place,
    pub text: String,
}

/// How many lines a text holds and how many units in all, for the page to compare with its own.
#[derive(Debug, Clone, Copy, Serialize, PartialEq, Eq)]
pub struct Shape {
    pub lines: usize,
    pub units: usize,
}

/// A text held, and where it was read, the text of its file as the last commit holds it, None where
/// the commit holds no such file.
#[derive(Default)]
struct Held {
    lines: Vec<Vec<u16>>,
    head: Option<Option<Vec<Vec<u16>>>>,
    given: String,
    joined: Option<Joined>,
}

/// A text's lines joined, each ended by "\n", and the unit each starts at, the units' count last.
struct Joined {
    units: Vec<u16>,
    starts: Vec<usize>,
}

fn joined(lines: &[Vec<u16>]) -> Joined {
    let mut units = Vec::with_capacity(lines.iter().map(|line| line.len() + 1).sum());
    let mut starts = Vec::with_capacity(lines.len() + 1);
    for line in lines {
        starts.push(units.len());
        units.extend_from_slice(line);
        units.push(b'\n' as u16);
    }
    starts.push(units.len());
    Joined { units, starts }
}

fn held() -> &'static Mutex<HashMap<String, Held>> {
    static HELD: OnceLock<Mutex<HashMap<String, Held>>> = OnceLock::new();
    HELD.get_or_init(Default::default)
}

/// A text's lines, split as the page splits a file it reads.
pub fn lines_of(text: &str) -> Vec<Vec<u16>> {
    text.split('\n').map(|line| line.strip_suffix('\r').unwrap_or(line).encode_utf16().collect()).collect()
}

fn shape_of(lines: &[Vec<u16>]) -> Shape {
    Shape { lines: lines.len(), units: lines.iter().map(Vec::len).sum() }
}

/// Holds `text` under `key` in place of what was held there.
pub fn hold(key: &str, text: &str) -> Shape {
    let lines = lines_of(text);
    let shape = shape_of(&lines);
    held().lock().unwrap().insert(key.to_string(), Held { lines, ..Held::default() });
    shape
}

/// Holds a text the page gives a part at a time: each part is kept, and the last, `done`, holds them all.
pub fn hold_part(key: &str, part: &str, first: bool, done: bool) -> Option<Shape> {
    let mut all = held().lock().unwrap();
    let one = all.entry(key.to_string()).or_default();
    if first {
        one.given.clear();
    }
    one.given.push_str(part);
    if !done {
        return None;
    }
    let text = std::mem::take(&mut one.given);
    one.lines = lines_of(&text);
    one.head = None;
    one.joined = None;
    Some(shape_of(&one.lines))
}

/// Lets go of the text held under `key`.
pub fn release(key: &str) {
    held().lock().unwrap().remove(key);
}

/// Whether a text is held under `key`, and its shape.
pub fn shape(key: &str) -> Option<Shape> {
    held().lock().unwrap().get(key).map(|one| shape_of(&one.lines))
}

/// Writes changes to the text under `key` in order. Each change is the edits the page's document
/// wrote, none overlapping, each placed against the text as it stood before any, written from the
/// last to the first as the page writes them. An edit that does not fit the text lets the text go,
/// and says so, for the page to trust its own.
pub fn edit(key: &str, changes: &[Vec<Edit>]) -> Result<Shape, String> {
    let mut all = held().lock().unwrap();
    let Some(one) = all.get_mut(key) else {
        return Err(format!("no text is held under {key}"));
    };
    if !changes.is_empty() {
        one.joined = None;
    }
    for edit in changes.iter().flat_map(|change| change.iter().rev()) {
        let fits = |place: &Place| one.lines.get(place.line).is_some_and(|line| place.col <= line.len());
        if !fits(&edit.from) || !fits(&edit.to) || (edit.to.line, edit.to.col) < (edit.from.line, edit.from.col) {
            all.remove(key);
            return Err(format!("an edit does not fit the text held under {key}"));
        }
        let head: Vec<u16> = one.lines[edit.from.line][..edit.from.col].to_vec();
        let tail: Vec<u16> = one.lines[edit.to.line][edit.to.col..].to_vec();
        let mut parts: Vec<Vec<u16>> = edit.text.split('\n').map(|part| part.encode_utf16().collect()).collect();
        let first = parts.first_mut().expect("split gives a part");
        first.splice(0..0, head);
        parts.last_mut().expect("split gives a part").extend(tail);
        one.lines.splice(edit.from.line..=edit.to.line, parts);
    }
    Ok(shape_of(&one.lines))
}

/// One change of the text from the last commit's: the lines it spans in each, [from, to), and the
/// lines it spans in the last commit's, for the page to show and to put back.
#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct Hunk {
    pub now: [usize; 2],
    pub then: [usize; 2],
    #[serde(rename = "thenLines", skip_serializing_if = "Vec::is_empty")]
    pub then_lines: Vec<String>,
}

/// The lines of the text that differ from the last commit's, as the page's diff marks them.
#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct Changes {
    pub added: Vec<usize>,
    pub changed: Vec<usize>,
    pub removed: Vec<usize>,
    pub hunks: Vec<Hunk>,
}

/// What the gutter marks for a text: whether the last commit holds its file, and where it does, the
/// lines that differ from it, None where the two are too far apart to mark.
#[derive(Debug, Clone, Serialize, PartialEq, Eq)]
pub struct Marks {
    pub head: bool,
    pub changes: Option<Changes>,
}

/// The marks of the text under `key` against its file as the last commit holds it. `head`, where
/// given, is that file, or None where the commit holds none, kept beside the text in place of what
/// was kept. None where no text is held or none was ever given.
pub fn marks(key: &str, head: Option<Option<&str>>) -> Option<Marks> {
    let mut all = held().lock().unwrap();
    let one = all.get_mut(key)?;
    if let Some(head) = head {
        one.head = Some(head.map(lines_of));
    }
    let Some(then) = one.head.as_ref()? else {
        return Some(Marks { head: false, changes: None });
    };
    let changes = crate::diff::line_changes(then, &one.lines).map(|mut found| {
        for hunk in &mut found.hunks {
            hunk.then_lines = then[hunk.then[0]..hunk.then[1]].iter().map(|line| String::from_utf16_lossy(line)).collect();
        }
        found
    });
    Some(Marks { head: true, changes })
}

/// Whether the text under `key` keeps the last commit's file beside it, or that it holds none.
pub fn has_head(key: &str) -> bool {
    held().lock().unwrap().get(key).is_some_and(|one| one.head.is_some())
}

/// What find seeks and how, as the page's find bar sets it.
#[derive(Debug, Clone, Deserialize)]
pub struct Sought {
    pub query: String,
    pub regex: bool,
    pub case: bool,
    pub word: bool,
    /// Whether the page read the pattern with the `u` flag.
    pub unicode: bool,
}

/// The matches find gives for a text: each [line, from col, to col], lines searched until `most` are
/// found, each line searched whole, as the page searches.
pub fn find(key: &str, sought: &Sought, most: usize) -> Result<Vec<[usize; 3]>, String> {
    let mut all = held().lock().unwrap();
    let Some(one) = all.get_mut(key) else {
        return Err(format!("no text is held under {key}"));
    };
    let lines = &one.lines;
    find_joined(one.joined.get_or_insert_with(|| joined(lines)), sought, most)
}

/// A word's letter as the page's find reads one for whole words, where it is ASCII: a letter, a
/// digit, `_` or `$`. None for a unit outside ASCII, which the page reads by its own tables.
fn word_unit(unit: u16) -> Option<bool> {
    (unit < 0x80).then(|| (unit as u8).is_ascii_alphanumeric() || unit == b'_' as u16 || unit == b'$' as u16)
}

/// Whether the span [from, to) of `line` is a whole word, or None where a unit beside it is outside
/// ASCII.
fn whole(line: &[u16], from: usize, to: usize) -> Option<bool> {
    let before = if from == 0 { Some(false) } else { word_unit(line[from - 1]) };
    let after = if to >= line.len() { Some(false) } else { word_unit(line[to]) };
    Some(!before? && !after?)
}

/// The search not the page's own here.
const PAGE_OWN: &str = "this search is the page's own";

/// `find` over lines given.
pub fn find_in(lines: &[Vec<u16>], sought: &Sought, most: usize) -> Result<Vec<[usize; 3]>, String> {
    find_joined(&joined(lines), sought, most)
}

fn find_joined(text: &Joined, sought: &Sought, most: usize) -> Result<Vec<[usize; 3]>, String> {
    let mut found = Vec::new();
    if sought.regex || (!sought.case && !sought.query.is_ascii()) {
        return Err(PAGE_OWN.into());
    }
    let wanted: Vec<u16> = sought.query.encode_utf16().collect();
    if wanted.is_empty() || wanted.contains(&(b'\n' as u16)) {
        return Ok(found);
    }
    // The page searches each line whole, and stops before the first line it starts with `most` found.
    let mut line = 0;
    let mut last = usize::MAX;
    let mut at = 0;
    loop {
        let hit = if sought.case { vector::find(&text.units, &wanted, at) } else { vector::find_folded(&text.units, &wanted, at) };
        let Some(hit) = hit else { break };
        while text.starts[line + 1] <= hit {
            line += 1;
        }
        if line > last {
            break;
        }
        let from = hit - text.starts[line];
        let to = from + wanted.len();
        if sought.word && !whole(&text.units[text.starts[line]..text.starts[line + 1] - 1], from, to).ok_or(PAGE_OWN)? {
            at = hit + 1;
            continue;
        }
        found.push([line, from, to]);
        if found.len() >= most && last == usize::MAX {
            last = line;
        }
        at = hit + wanted.len();
    }
    Ok(found)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn text(lines: &[Vec<u16>]) -> String {
        lines.iter().map(|line| String::from_utf16_lossy(line)).collect::<Vec<_>>().join("\n")
    }

    #[test]
    fn a_text_is_split_as_the_page_splits_a_file() {
        let lines = lines_of("one\r\ntwo\nthree\r\r\n\u{1F600}");
        assert_eq!(lines.len(), 4);
        assert_eq!(String::from_utf16_lossy(&lines[2]), "three\r");
        assert_eq!(lines[3].len(), 2, "a char past the basic plane is two units");
    }

    #[test]
    fn edits_are_written_from_the_last_to_the_first_as_the_page_writes_them() {
        hold("edits", "first line\nsecond line\nthird");
        let place = |line, col| Place { line, col };
        let written = edit(
            "edits",
            &[
                vec![Edit { from: place(0, 0), to: place(0, 5), text: "1st".into() }, Edit { from: place(1, 7), to: place(2, 2), text: "joined\nand ".into() }],
                vec![Edit { from: place(2, 0), to: place(2, 0), text: "".into() }],
            ],
        )
        .unwrap();
        let all = held().lock().unwrap();
        assert_eq!(text(&all["edits"].lines), "1st line\nsecond joined\nand ird");
        assert_eq!(written, Shape { lines: 3, units: "1st line".len() + "second joined".len() + "and ird".len() });
        drop(all);
        assert!(edit("edits", &[vec![Edit { from: place(9, 0), to: place(9, 0), text: "x".into() }]]).is_err(), "an edit past the text");
        assert!(shape("edits").is_none(), "and the text is let go");
    }

    #[test]
    fn the_marks_say_whether_the_commit_holds_the_file_and_carry_each_change_s_lines_as_it_held_them() {
        hold("marks", "one\nTWO\nthree");
        assert_eq!(marks("marks", None), None, "no commit's text given yet");
        let found = marks("marks", Some(Some("one\ntwo\nthree"))).unwrap();
        assert!(found.head && has_head("marks"));
        let changes = found.changes.unwrap();
        assert_eq!(changes.changed, vec![1]);
        assert_eq!(changes.hunks[0].then_lines, vec!["two".to_string()]);
        assert_eq!(marks("marks", None).unwrap().changes.unwrap().changed, vec![1], "kept beside the text");
        assert_eq!(marks("marks", Some(None)), Some(Marks { head: false, changes: None }), "a commit that holds no such file");
        release("marks");
    }

    #[test]
    fn a_text_given_in_parts_is_held_once_the_last_comes() {
        assert_eq!(hold_part("parts", "one\ntw", true, false), None);
        assert_eq!(hold_part("parts", "o\nthree", false, true), Some(Shape { lines: 3, units: 11 }));
        release("parts");
        assert!(shape("parts").is_none());
    }

    fn sought(query: &str, regex: bool, case: bool, word: bool) -> Sought {
        Sought { query: query.into(), regex, case, word, unicode: true }
    }

    #[test]
    fn find_gives_each_match_of_text_sought_with_case_and_without() {
        let lines = lines_of("Line one line\nnoline LINE\nſtar KING");
        assert_eq!(find_in(&lines, &sought("line", false, true, false), 100).unwrap(), vec![[0, 9, 13], [1, 2, 6]]);
        assert_eq!(find_in(&lines, &sought("line", false, false, false), 100).unwrap(), vec![[0, 0, 4], [0, 9, 13], [1, 2, 6], [1, 7, 11]]);
        assert_eq!(find_in(&lines, &sought("line", false, false, true), 100).unwrap(), vec![[0, 0, 4], [0, 9, 13], [1, 7, 11]], "whole words");
        assert_eq!(find_in(&lines, &sought("star king", false, false, false), 100).unwrap(), vec![[2, 0, 9]], "ſ and the Kelvin sign fold");
        assert_eq!(find_in(&lines, &sought("line", false, false, false), 1).unwrap().len(), 2, "the line that reaches the most is searched whole");
        assert!(find_in(&lines, &sought("é", false, false, false), 100).is_err(), "without case, text outside ASCII is the page's");
    }

    #[test]
    fn whole_words_beside_text_outside_ascii_are_the_page_s() {
        let lines = lines_of("a line\u{064B} here");
        assert!(find_in(&lines, &sought("line", false, true, true), 100).is_err());
    }

    #[test]
    fn a_pattern_is_the_page_s_own_and_text_sought_finds_across_a_line_s_units() {
        let lines = lines_of("x_1 = 10\nx_22 = 3\n\u{1F600}ab");
        assert!(find_in(&lines, &sought("x_\\d+", true, true, false), 100).is_err());
        assert_eq!(find_in(&lines, &sought("ab", false, true, false), 100).unwrap(), vec![[2, 2, 4]], "cols in UTF-16 units");
        assert_eq!(find_in(&lines, &sought("X_", false, false, false), 100).unwrap(), vec![[0, 0, 2], [1, 0, 2]], "without case");
        assert_eq!(find_in(&lines, &sought("10\nx", false, true, false), 100).unwrap(), Vec::<[usize; 3]>::new(), "nothing runs over a line's end");
    }

    #[test]
    fn a_held_text_s_joined_lines_follow_its_edits() {
        hold("joined", "one two\nthree");
        let find = |query: &str| find("joined", &sought(query, false, true, false), 100).unwrap();
        assert_eq!(find("three"), vec![[1, 0, 5]]);
        let place = |line, col| Place { line, col };
        edit("joined", &[vec![Edit { from: place(0, 3), to: place(1, 0), text: "\n".into() }]]).unwrap();
        assert_eq!(find("three"), vec![[1, 0, 5]]);
        assert_eq!(find("two"), Vec::<[usize; 3]>::new(), "the joined lines are made again after an edit");
        release("joined");
    }
}
