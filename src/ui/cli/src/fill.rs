// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Fill Paragraph: the paragraph of documentation at a line written again to a margin, its words
//! filled into as few lines as hold them.
//!
//! A paragraph is the lines around the line that share its comment's mark and its indent, as `#`,
//! `//`, `///` and `--` write a comment a line, up to a line of the mark alone; the lines of a
//! Python docstring around it, up to a blank line or the docstring's quotes, the opening quotes kept
//! on the first line and the closing quotes on the last; or, in plain text, the lines around it up to
//! a blank line. A comment of `/** */` is written again whole: each paragraph of its text filled, and
//! each run of its tags of one kind lined up, `@param` and the rest, the names, then the
//! descriptions, starting in one column, a description that runs past the margin going on under its
//! own first word.

use crate::servers::{Place, TextEdit};

/// The marks a comment a line starts with, the longest first.
const MARKS: [&str; 6] = ["///", "//!", "//", "##", "#", "--"];

/// The tags whose first word after a type is a name.
const NAMED_TAGS: [&str; 7] = ["param", "arg", "argument", "property", "prop", "template", "typedef"];

/// Words filled into lines no wider than `width` columns, each line after `first` starting with
/// `rest`.
fn fill_words(words: &[&str], first: &str, rest: &str, width: usize) -> Vec<String> {
    let mut lines = Vec::new();
    let mut line = first.to_string();
    let mut empty = true;
    for word in words {
        let lead = if empty { line.chars().count() } else { line.chars().count() + 1 };
        if !empty && lead + word.chars().count() > width {
            lines.push(line.trim_end().to_string());
            line = rest.to_string();
            empty = true;
        }
        if !empty {
            line.push(' ');
        }
        line.push_str(word);
        empty = false;
    }
    lines.push(line.trim_end().to_string());
    lines
}

/// The paragraph of documentation at `line` of `text` filled to `width` columns: the edit that
/// writes it again.
pub fn fill(text: &str, line: u32, width: usize) -> Result<TextEdit, String> {
    let lines: Vec<&str> = text.split('\n').collect();
    let at = line as usize;
    let here = *lines.get(at).ok_or("No line is there.")?;
    if let Some(edit) = doc_comment(&lines, at, width) {
        return Ok(edit);
    }
    let trimmed = here.trim_start();
    let indent = &here[..here.len() - trimmed.len()];
    if let Some(mark) = MARKS.iter().find(|mark| trimmed.starts_with(**mark)) {
        let prefix = format!("{indent}{mark}");
        let in_paragraph = |line: &str| body_of(line, &prefix).is_some_and(|rest| !rest.is_empty()) && !MARKS.iter().any(|other| other.len() > mark.len() && line.trim_start().starts_with(*other));
        if !in_paragraph(here) {
            return Err("Fill Paragraph fills the paragraph the cursor is in.".into());
        }
        let mut first = at;
        while first > 0 && in_paragraph(lines[first - 1]) {
            first -= 1;
        }
        let mut last = at;
        while last + 1 < lines.len() && in_paragraph(lines[last + 1]) {
            last += 1;
        }
        let words: Vec<&str> = lines[first..=last].iter().flat_map(|line| body_of(line, &prefix).unwrap_or_default().split_whitespace()).collect();
        let lead = format!("{prefix} ");
        return Ok(edit_of(&lines, first, last, fill_words(&words, &lead, &lead, width)));
    }
    if let Some(edit) = docstring(&lines, at, width) {
        return Ok(edit);
    }
    if trimmed.is_empty() {
        return Err("Fill Paragraph fills the paragraph the cursor is in.".into());
    }
    let mut first = at;
    while first > 0 && !lines[first - 1].trim().is_empty() {
        first -= 1;
    }
    let mut last = at;
    while last + 1 < lines.len() && !lines[last + 1].trim().is_empty() {
        last += 1;
    }
    let lead = &lines[first][..lines[first].len() - lines[first].trim_start().len()];
    let words: Vec<&str> = lines[first..=last].iter().flat_map(|line| line.split_whitespace()).collect();
    Ok(edit_of(&lines, first, last, fill_words(&words, lead, lead, width)))
}

/// A comment line's text after its mark and its indent, where the line starts with them.
fn body_of<'a>(line: &'a str, prefix: &str) -> Option<&'a str> {
    line.strip_prefix(prefix).map(str::trim)
}

/// The edit that writes the lines `first` to `last` as `written`.
fn edit_of(lines: &[&str], first: usize, last: usize, written: Vec<String>) -> TextEdit {
    let end = lines[last].encode_utf16().count() as u32;
    TextEdit { from: Place { line: first as u32, col: 0 }, to: Place { line: last as u32, col: end }, text: written.join("\n") }
}

/// The paragraph of a Python docstring at `at`, where the line stands inside one: up to a blank line
/// or a line of the closing quotes alone, the opening quotes kept at the head of its first line and the
/// closing quotes at the end of its last.
fn docstring(lines: &[&str], at: usize, width: usize) -> Option<TextEdit> {
    // The triple-quoted strings up to the line, each its quote and its first and last lines.
    let mut strings: Vec<(&str, usize, usize)> = Vec::new();
    let mut open: Option<(&str, usize)> = None;
    for (index, line) in lines.iter().enumerate() {
        if index > at && open.is_none() {
            break;
        }
        let mut rest = *line;
        loop {
            match open {
                Some((quote, start)) => match rest.find(quote) {
                    Some(close) => {
                        strings.push((quote, start, index));
                        open = None;
                        rest = &rest[close + 3..];
                    }
                    None => break,
                },
                None => match ["\"\"\"", "'''"].iter().filter_map(|mark| rest.find(*mark).map(|found| (found, *mark))).min() {
                    Some((found, mark)) => {
                        open = Some((mark, index));
                        rest = &rest[found + 3..];
                    }
                    None => break,
                },
            }
        }
    }
    let &(quote, start, end) = strings.iter().find(|(_, start, end)| *start <= at && at <= *end)?;
    let blank = |index: usize| lines[index].trim().is_empty() || lines[index].trim() == quote;
    if blank(at) {
        return None;
    }
    let mut first = at;
    while first > start && !blank(first - 1) {
        first -= 1;
    }
    let mut last = at;
    while last < end && !blank(last + 1) {
        last += 1;
    }
    let indent_of = |line: &str| line[..line.len() - line.trim_start().len()].to_string();
    let lead = indent_of(lines[first]);
    let opening = if first == start { lines[first].trim_start()[..lines[first].trim_start().find(quote)? + 3].to_string() } else { String::new() };
    let closing = last == end;
    let mut words: Vec<&str> = Vec::new();
    for (offset, line) in lines[first..=last].iter().enumerate() {
        let mut body = line.trim();
        if offset == 0 && !opening.is_empty() {
            body = body[opening.len()..].trim_start();
        }
        if offset == last - first && closing {
            body = body.strip_suffix(quote).unwrap_or(body).trim_end();
        }
        words.extend(body.split_whitespace());
    }
    let mut written = fill_words(&words, &format!("{lead}{opening}"), &lead, width);
    if closing {
        // The closing quotes stay inside the margin: the last word goes down where they would not.
        if let Some(line) = written.last().cloned() {
            let start_of_words = if written.len() == 1 { lead.len() + opening.len() } else { lead.len() };
            if line.chars().count() + 3 > width && line[start_of_words..].contains(' ') {
                let cut = line.rfind(' ').unwrap_or(line.len());
                let word = line[cut + 1..].to_string();
                let count = written.len();
                written[count - 1] = line[..cut].to_string();
                written.push(format!("{lead}{word}"));
            }
        }
        if let Some(line) = written.last_mut() {
            line.push_str(quote);
        }
    }
    Some(edit_of(lines, first, last, written))
}

/// The `/** */` comment around `at`, its paragraphs filled and its tags lined up.
fn doc_comment(lines: &[&str], at: usize, width: usize) -> Option<TextEdit> {
    let mut first = at;
    loop {
        let line = lines[first].trim_start();
        if line.starts_with("/*") {
            break;
        }
        if !line.starts_with('*') || line.starts_with("*/") && first != at || first == 0 {
            return None;
        }
        first -= 1;
    }
    let mut last = at;
    while !lines[last].trim_end().ends_with("*/") {
        last += 1;
        if last >= lines.len() || !lines[last].trim_start().starts_with('*') {
            return None;
        }
    }
    if first == last {
        return None;
    }
    let indent = &lines[first][..lines[first].len() - lines[first].trim_start().len()];
    let open = lines[first].trim().to_string();
    let close = lines[last].trim().to_string();
    let lead = format!("{indent} * ");
    // The comment's lines, each without its star, and whether the opening and closing lines hold text.
    let mut body: Vec<String> = Vec::new();
    let opening_text = open.trim_start_matches('/').trim_start_matches('*').trim();
    if !opening_text.is_empty() {
        body.push(opening_text.to_string());
    }
    for line in &lines[first + 1..last] {
        body.push(line.trim_start().trim_start_matches('*').trim().to_string());
    }
    let closing_text = if last > first { close.trim_end_matches('/').trim_end_matches('*').trim_start_matches('*').trim().to_string() } else { String::new() };
    if !closing_text.is_empty() {
        body.push(closing_text);
    }
    // Paragraphs of text, and tags, each with the lines that go on with it.
    let mut parts: Vec<Vec<String>> = Vec::new();
    for line in body {
        if line.is_empty() {
            parts.push(Vec::new());
        } else if line.starts_with('@') || parts.last().is_none_or(|part| part.is_empty()) {
            parts.push(vec![line]);
        } else if let Some(part) = parts.last_mut() {
            part.push(line);
        }
    }
    let mut written = vec![format!("{indent}/**")];
    let mut at_part = 0;
    while at_part < parts.len() {
        let part = &parts[at_part];
        if part.is_empty() {
            if written.last().is_some_and(|line| line.trim() != "*" && line.trim() != "/**") {
                written.push(format!("{indent} *"));
            }
            at_part += 1;
            continue;
        }
        if !part[0].starts_with('@') {
            let words: Vec<&str> = part.iter().flat_map(|line| line.split_whitespace()).collect();
            written.extend(fill_words(&words, &lead, &lead, width));
            at_part += 1;
            continue;
        }
        // A run of tags of one kind, lined up.
        let kind = tag_of(&part[0]).0;
        let mut run = Vec::new();
        while at_part < parts.len() && !parts[at_part].is_empty() && parts[at_part][0].starts_with('@') && tag_of(&parts[at_part][0]).0 == kind {
            run.push(parts[at_part].join(" "));
            at_part += 1;
        }
        let read: Vec<(String, String, String, String)> = run.iter().map(|tag| split_tag(tag)).collect();
        let tag_width = read.iter().map(|one| one.0.chars().count()).max().unwrap_or(0);
        let type_width = read.iter().map(|one| one.1.chars().count()).max().unwrap_or(0);
        let name_width = read.iter().map(|one| one.2.chars().count()).max().unwrap_or(0);
        for (tag, kind_of, name, description) in &read {
            let mut head = format!("{lead}{tag:<tag_width$}");
            if type_width > 0 {
                head.push_str(&format!(" {kind_of:<type_width$}"));
            }
            if name_width > 0 {
                head.push_str(&format!(" {name:<name_width$}"));
            }
            let words: Vec<&str> = description.split_whitespace().collect();
            if words.is_empty() {
                written.push(head.trim_end().to_string());
                continue;
            }
            head.push(' ');
            let hang = format!("{lead}{}", " ".repeat(head.chars().count() - lead.chars().count()));
            written.extend(fill_words(&words, &head, &hang, width));
        }
    }
    while written.last().is_some_and(|line| line.trim() == "*") {
        written.pop();
    }
    written.push(format!("{indent} */"));
    Some(edit_of(lines, first, last, written))
}

/// A tag's kind, by its name without the `@`.
fn tag_of(line: &str) -> (String, &str) {
    let tag = line.split_whitespace().next().unwrap_or_default();
    (tag.trim_start_matches('@').to_string(), tag)
}

/// A tag's parts: the tag, its type in braces, its name where the tag names one, and its description.
fn split_tag(line: &str) -> (String, String, String, String) {
    let (kind, tag) = tag_of(line);
    let mut rest = line[tag.len()..].trim_start();
    let mut kind_of = String::new();
    if rest.starts_with('{') {
        let mut depth = 0;
        let mut end = 0;
        for (at, c) in rest.char_indices() {
            match c {
                '{' => depth += 1,
                '}' => {
                    depth -= 1;
                    if depth == 0 {
                        end = at + 1;
                        break;
                    }
                }
                _ => {}
            }
        }
        if end > 0 {
            kind_of = rest[..end].to_string();
            rest = rest[end..].trim_start();
        }
    }
    let mut name = String::new();
    if NAMED_TAGS.contains(&kind.as_str()) {
        let word = rest.split_whitespace().next().unwrap_or_default();
        name = word.to_string();
        rest = rest[word.len()..].trim_start();
    }
    (tag.to_string(), kind_of, name, rest.trim_start_matches('-').trim().to_string())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn filled(text: &str, line: u32, width: usize) -> String {
        crate::servers::apply(text, &[fill(text, line, width).unwrap()])
    }

    #[test]
    fn a_comment_s_paragraph_is_filled_to_the_margin_and_its_neighbours_left() {
        let text = "x = 1\n    # one two three four five six seven\n    # eight nine\n    #\n    # ten\ny = 2\n";
        assert_eq!(filled(text, 2, 24), "x = 1\n    # one two three four\n    # five six seven\n    # eight nine\n    #\n    # ten\ny = 2\n");
    }

    #[test]
    fn a_docstring_s_paragraph_keeps_its_quotes_where_they_stand() {
        let text = "def f():\n    \"\"\"Return the area of the shape that the\n    reader draws.\n\n    More text that runs on and on past the margin.\n    \"\"\"\n";
        assert_eq!(filled(text, 1, 30), "def f():\n    \"\"\"Return the area of the\n    shape that the reader\n    draws.\n\n    More text that runs on and on past the margin.\n    \"\"\"\n");
        assert_eq!(filled(text, 4, 30), "def f():\n    \"\"\"Return the area of the shape that the\n    reader draws.\n\n    More text that runs on and\n    on past the margin.\n    \"\"\"\n");
        let one = "def g():\n    \"\"\"One two three four five six seven eight.\"\"\"\n";
        assert_eq!(filled(one, 1, 30), "def g():\n    \"\"\"One two three four five\n    six seven eight.\"\"\"\n");
    }

    #[test]
    fn a_doc_comment_s_tags_are_lined_up_and_its_text_filled() {
        let text = "/**\n * The area of a shape, as the reader draws it on the page.\n *\n * @param {number} width the width\n * @param height the height of the shape, which runs long\n * @returns {number} the area\n */\nfunction area(width, height) {}\n";
        assert_eq!(
            filled(text, 4, 50),
            "/**\n * The area of a shape, as the reader draws it on\n * the page.\n *\n * @param {number} width  the width\n * @param          height the height of the shape,\n *                        which runs long\n * @returns {number} the area\n */\nfunction area(width, height) {}\n"
        );
    }

    #[test]
    fn plain_text_fills_by_blank_lines() {
        let text = "One two three\nfour five six seven.\n\nNext.\n";
        assert_eq!(filled(text, 0, 15), "One two three\nfour five six\nseven.\n\nNext.\n");
    }
}
