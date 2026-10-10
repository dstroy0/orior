// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Regular expressions as the page's grammars write them, in JavaScript's syntax, matched at one place
//! of a line as a sticky pattern is. What is read: characters and escapes (`\d \w \s` and their
//! opposites, `\b \B`, `\t \n \r \f \v \0`, `\xHH`, `\uHHHH`, `\u{H...}`, an escaped sign), `.`,
//! classes with ranges and escapes, `^` and `$` at the line's ends, groups that capture and groups
//! that do not, lookahead that holds and that does not, alternation, the quantifiers `* + ? {n}
//! {n,} {n,m}` greedy and lazy, and backreferences. The flags read are `i`, which compares letters
//! without case, and `u`, under which `\p{...}` and `\P{...}` read letters, numbers, and upper and
//! lower case; any other flag is no matter for a single line. Lookbehind and named groups are
//! refused, and a grammar that holds one is colored by the page instead.
//!
//! Matching backtracks. A quantifier over one character steps along without going deeper for each
//! character it takes, and a long run costs no stack; any other repeats by going one level deeper an
//! iteration. A match that takes more than STEPS steps is given up as no match.

/// The most steps one attempt at a match takes before it is given up.
const STEPS: usize = 200_000;

#[derive(Debug, Clone)]
enum Item {
    Range(char, char),
    Digit(bool),
    Word(bool),
    Space(bool),
    Property(Property, bool),
}

/// The Unicode properties `\p{...}` reads under the `u` flag: letters, numbers, upper and lower case.
#[derive(Debug, Clone, Copy)]
enum Property {
    Letter,
    Number,
    Upper,
    Lower,
}

impl Property {
    fn holds(self, letter: char) -> bool {
        match self {
            Property::Letter => letter.is_alphabetic(),
            Property::Number => letter.is_numeric(),
            Property::Upper => letter.is_uppercase(),
            Property::Lower => letter.is_lowercase(),
        }
    }
}

#[derive(Debug, Clone)]
enum Node {
    Empty,
    Char(char),
    Any,
    Class(Vec<Item>, bool),
    Start,
    End,
    Boundary(bool),
    Group(Box<Node>, Option<usize>),
    Look(Box<Node>, bool),
    Concat(Vec<Node>),
    Alt(Vec<Node>, Vec<First>),
    Repeat(Box<Node>, usize, Option<usize>, bool),
    Back(usize),
}

/// A pattern read and ready to match.
#[derive(Debug, Clone)]
pub struct Regexp {
    node: Node,
    groups: usize,
    fold: bool,
    first: First,
}

/// The characters a match can start with, worked out as a pattern is read: each ASCII one, whether
/// any other can, and whether a match can take nothing. A place whose character none can start at is
/// passed over without a match begun.
#[derive(Debug, Clone, Copy, Default)]
struct First {
    ascii: u128,
    other: bool,
    empty: bool,
}

impl First {
    fn union(self, other: First) -> First {
        First { ascii: self.ascii | other.ascii, other: self.other || other.other, empty: self.empty || other.empty }
    }

    fn allows(&self, letter: char) -> bool {
        if letter.is_ascii() { self.ascii & (1u128 << letter as u32) != 0 } else { self.other }
    }
}

fn is_word(letter: char) -> bool {
    letter.is_ascii_alphanumeric() || letter == '_'
}

fn is_space(letter: char) -> bool {
    matches!(letter, '\t' | '\n' | '\u{b}' | '\u{c}' | '\r' | ' ' | '\u{a0}' | '\u{1680}' | '\u{2028}' | '\u{2029}' | '\u{202f}' | '\u{205f}' | '\u{3000}' | '\u{feff}') || ('\u{2000}'..='\u{200a}').contains(&letter)
}

fn ends_line(letter: char) -> bool {
    matches!(letter, '\n' | '\r' | '\u{2028}' | '\u{2029}')
}

fn lower(letter: char) -> char {
    letter.to_lowercase().next().unwrap_or(letter)
}

struct Reader<'a> {
    letters: Vec<char>,
    at: usize,
    groups: usize,
    source: &'a str,
    unicode: bool,
}

impl Reader<'_> {
    fn peek(&self) -> Option<char> {
        self.letters.get(self.at).copied()
    }

    fn next(&mut self) -> Option<char> {
        let letter = self.peek();
        self.at += usize::from(letter.is_some());
        letter
    }

    fn eat(&mut self, letter: char) -> bool {
        if self.peek() == Some(letter) {
            self.at += 1;
            true
        } else {
            false
        }
    }

    fn fail<T>(&self, why: &str) -> Result<T, String> {
        Err(format!("/{}/: {why} at {}", self.source, self.at))
    }

    fn alternation(&mut self) -> Result<Node, String> {
        let mut options = vec![self.sequence()?];
        while self.eat('|') {
            options.push(self.sequence()?);
        }
        Ok(if options.len() == 1 { options.pop().unwrap_or(Node::Empty) } else { Node::Alt(options, Vec::new()) })
    }

    fn sequence(&mut self) -> Result<Node, String> {
        let mut parts = Vec::new();
        while let Some(letter) = self.peek() {
            if letter == '|' || letter == ')' {
                break;
            }
            let atom = self.atom()?;
            parts.push(self.quantified(atom)?);
        }
        Ok(match parts.len() {
            0 => Node::Empty,
            1 => parts.pop().unwrap_or(Node::Empty),
            _ => Node::Concat(parts),
        })
    }

    fn number(&mut self) -> Option<usize> {
        let start = self.at;
        while self.peek().is_some_and(|letter| letter.is_ascii_digit()) {
            self.at += 1;
        }
        self.letters[start..self.at].iter().collect::<String>().parse().ok()
    }

    fn quantified(&mut self, atom: Node) -> Result<Node, String> {
        let (min, max) = match self.peek() {
            Some('*') => (0, None),
            Some('+') => (1, None),
            Some('?') => (0, Some(1)),
            Some('{') => {
                let back = self.at;
                self.at += 1;
                let Some(min) = self.number() else {
                    self.at = back;
                    return Ok(atom);
                };
                let max = if self.eat(',') { if self.peek() == Some('}') { None } else { self.number() } } else { Some(min) };
                if !self.eat('}') {
                    self.at = back;
                    return Ok(atom);
                }
                self.at -= 1;
                (min, max)
            }
            _ => return Ok(atom),
        };
        self.at += 1;
        if matches!(atom, Node::Start | Node::End | Node::Boundary(_) | Node::Look(..)) {
            return self.fail("a quantifier after an assertion");
        }
        let greedy = !self.eat('?');
        Ok(Node::Repeat(Box::new(atom), min, max, greedy))
    }

    fn hex(&mut self, count: usize) -> Option<char> {
        let start = self.at;
        for _ in 0..count {
            if !self.peek().is_some_and(|letter| letter.is_ascii_hexdigit()) {
                self.at = start;
                return None;
            }
            self.at += 1;
        }
        u32::from_str_radix(&self.letters[start..self.at].iter().collect::<String>(), 16).ok().and_then(char::from_u32)
    }

    // An escape after its backslash: a class item, or one character.
    fn escape(&mut self, in_class: bool) -> Result<Result<Item, char>, String> {
        let Some(letter) = self.next() else {
            return self.fail("a backslash at the end");
        };
        Ok(match letter {
            'd' => Ok(Item::Digit(false)),
            'D' => Ok(Item::Digit(true)),
            'w' => Ok(Item::Word(false)),
            'W' => Ok(Item::Word(true)),
            's' => Ok(Item::Space(false)),
            'S' => Ok(Item::Space(true)),
            't' => Err('\t'),
            'n' => Err('\n'),
            'r' => Err('\r'),
            'f' => Err('\u{c}'),
            'v' => Err('\u{b}'),
            '0' => Err('\0'),
            'b' if in_class => Err('\u{8}'),
            'x' => Err(self.hex(2).unwrap_or('x')),
            'u' => {
                if self.eat('{') {
                    let start = self.at;
                    while self.peek().is_some_and(|one| one.is_ascii_hexdigit()) {
                        self.at += 1;
                    }
                    let code = u32::from_str_radix(&self.letters[start..self.at].iter().collect::<String>(), 16).ok().and_then(char::from_u32);
                    if !self.eat('}') {
                        return self.fail("an unclosed \\u{");
                    }
                    Err(code.unwrap_or('u'))
                } else {
                    Err(self.hex(4).unwrap_or('u'))
                }
            }
            'c' => match self.next() {
                Some(control) if control.is_ascii_alphabetic() => Err(char::from((control as u8) % 32)),
                _ => return self.fail("a control escape"),
            },
            'p' | 'P' if self.unicode => {
                if !self.eat('{') {
                    return self.fail("a property with no name");
                }
                let start = self.at;
                while self.peek().is_some_and(|one| one != '}') {
                    self.at += 1;
                }
                let name: String = self.letters[start..self.at].iter().collect();
                if !self.eat('}') {
                    return self.fail("an unclosed property");
                }
                let property = match name.as_str() {
                    "L" | "Letter" => Property::Letter,
                    "N" | "Number" => Property::Number,
                    "Lu" | "Uppercase_Letter" => Property::Upper,
                    "Ll" | "Lowercase_Letter" => Property::Lower,
                    _ => return self.fail("a property not read here"),
                };
                Ok(Item::Property(property, letter == 'P'))
            }
            'k' => return self.fail("an escape not read here"),
            other => Err(other),
        })
    }

    fn class(&mut self) -> Result<Node, String> {
        let negated = self.eat('^');
        let mut items = Vec::new();
        loop {
            let Some(letter) = self.next() else {
                return self.fail("an unclosed class");
            };
            if letter == ']' {
                break;
            }
            let low = if letter == '\\' {
                match self.escape(true)? {
                    Ok(item) => {
                        items.push(item);
                        continue;
                    }
                    Err(one) => one,
                }
            } else {
                letter
            };
            if self.peek() == Some('-') && self.letters.get(self.at + 1).is_some_and(|&next| next != ']') {
                self.at += 1;
                let high = match self.next() {
                    Some('\\') => match self.escape(true)? {
                        Ok(item) => {
                            items.push(Item::Range(low, low));
                            items.push(Item::Range('-', '-'));
                            items.push(item);
                            continue;
                        }
                        Err(one) => one,
                    },
                    Some(one) => one,
                    None => return self.fail("an unclosed class"),
                };
                if high < low {
                    return self.fail("a range out of order");
                }
                items.push(Item::Range(low, high));
            } else {
                items.push(Item::Range(low, low));
            }
        }
        Ok(Node::Class(items, negated))
    }

    fn atom(&mut self) -> Result<Node, String> {
        let Some(letter) = self.next() else {
            return self.fail("nothing to read");
        };
        Ok(match letter {
            '.' => Node::Any,
            '^' => Node::Start,
            '$' => Node::End,
            '[' => self.class()?,
            '(' => {
                if self.eat('?') {
                    match self.next() {
                        Some(':') => {
                            let inner = self.alternation()?;
                            if !self.eat(')') {
                                return self.fail("an unclosed group");
                            }
                            Node::Group(Box::new(inner), None)
                        }
                        Some(way @ ('=' | '!')) => {
                            let inner = self.alternation()?;
                            if !self.eat(')') {
                                return self.fail("an unclosed lookahead");
                            }
                            Node::Look(Box::new(inner), way == '!')
                        }
                        _ => return self.fail("a group not read here"),
                    }
                } else {
                    self.groups += 1;
                    let index = self.groups;
                    let inner = self.alternation()?;
                    if !self.eat(')') {
                        return self.fail("an unclosed group");
                    }
                    Node::Group(Box::new(inner), Some(index))
                }
            }
            '\\' => match self.peek() {
                Some('b') => {
                    self.at += 1;
                    Node::Boundary(false)
                }
                Some('B') => {
                    self.at += 1;
                    Node::Boundary(true)
                }
                Some(digit) if ('1'..='9').contains(&digit) => Node::Back(self.number().unwrap_or(0)),
                _ => match self.escape(false)? {
                    Ok(item) => Node::Class(vec![item], false),
                    Err(one) => Node::Char(one),
                },
            },
            '*' | '+' | '?' => return self.fail("a quantifier with nothing before it"),
            other => Node::Char(other),
        })
    }
}

/// Where a match stands: each group's span, the steps taken, and whether the steps ran out.
struct Run<'a> {
    text: &'a [char],
    groups: Vec<Option<(usize, usize)>>,
    steps: usize,
}

impl Regexp {
    pub fn new(source: &str, flags: &str) -> Result<Regexp, String> {
        let mut reader = Reader { letters: source.chars().collect(), at: 0, groups: 0, source, unicode: flags.contains('u') };
        let node = reader.alternation()?;
        if reader.at < reader.letters.len() {
            return reader.fail("an unmatched )");
        }
        let mut made = Regexp { node: Node::Empty, groups: reader.groups, fold: flags.contains('i'), first: First::default() };
        let mut node = node;
        made.fill(&mut node);
        made.first = made.first_of(&node);
        made.node = node;
        Ok(made)
    }

    // Gives each alternative of every alternation its first characters, which a place whose
    // character none of them can start with passes over.
    fn fill(&self, node: &mut Node) {
        match node {
            Node::Alt(options, firsts) => {
                for option in options.iter_mut() {
                    self.fill(option);
                }
                *firsts = options.iter().map(|option| self.first_of(option)).collect();
            }
            Node::Group(inner, _) | Node::Look(inner, _) | Node::Repeat(inner, ..) => self.fill(inner),
            Node::Concat(parts) => parts.iter_mut().for_each(|part| self.fill(part)),
            _ => {}
        }
    }

    // The characters a node's match can start with.
    fn first_of(&self, node: &Node) -> First {
        let ascii = |test: &dyn Fn(char) -> bool| (0u8..128).filter(|&code| test(char::from(code))).fold(0u128, |set, code| set | (1u128 << code));
        match node {
            Node::Empty | Node::Start | Node::End | Node::Boundary(_) | Node::Look(..) => First { empty: true, ..First::default() },
            Node::Char(_) | Node::Any | Node::Class(..) => First { ascii: ascii(&|letter| self.one(node, letter) == Some(true)), other: true, empty: false },
            Node::Group(inner, _) => self.first_of(inner),
            Node::Alt(options, _) => options.iter().fold(First::default(), |set, option| set.union(self.first_of(option))),
            Node::Concat(parts) => {
                let mut set = First::default();
                for part in parts {
                    let own = self.first_of(part);
                    set = set.union(First { empty: false, ..own });
                    if !own.empty {
                        return set;
                    }
                }
                First { empty: true, ..set }
            }
            Node::Repeat(inner, min, _, _) => {
                let own = self.first_of(inner);
                First { empty: own.empty || *min == 0, ..own }
            }
            Node::Back(_) => First { ascii: u128::MAX, other: true, empty: true },
        }
    }

    /// The match that starts at `at` in `text`, as each group's span, the whole match first, where
    /// one starts there.
    pub fn match_at(&self, text: &[char], at: usize) -> Option<Vec<Option<(usize, usize)>>> {
        let mut groups = Vec::new();
        self.match_into(text, at, &mut groups).then_some(groups)
    }

    /// Whether a match starts at `at` in `text`, with each group's span, the whole match first, written
    /// into `groups`, a buffer the caller keeps from one match to the next.
    pub fn match_into(&self, text: &[char], at: usize, groups: &mut Vec<Option<(usize, usize)>>) -> bool {
        if !self.first.empty && !text.get(at).is_some_and(|&letter| self.first.allows(letter)) {
            return false;
        }
        groups.clear();
        groups.resize(self.groups + 1, None);
        let mut run = Run { text, groups: std::mem::take(groups), steps: 0 };
        let mut end = None;
        let found = self.walk(&self.node, at, &mut run, &mut |to, _| {
            end = Some(to);
            true
        });
        if found {
            run.groups[0] = end.map(|to| (at, to));
        }
        *groups = run.groups;
        found
    }

    fn same(&self, a: char, b: char) -> bool {
        a == b || (self.fold && lower(a) == lower(b))
    }

    fn holds(&self, items: &[Item], negated: bool, letter: char) -> bool {
        let test = |one: char| {
            items.iter().any(|item| match *item {
                Item::Range(low, high) => (low..=high).contains(&one),
                Item::Digit(not) => one.is_ascii_digit() != not,
                Item::Word(not) => is_word(one) != not,
                Item::Space(not) => is_space(one) != not,
                Item::Property(property, not) => property.holds(one) != not,
            })
        };
        let found = test(letter) || (self.fold && (test(lower(letter)) || letter.to_uppercase().next().is_some_and(test)));
        found != negated
    }

    // Whether one character at `at` is taken by a node that takes exactly one, for the quantifier that
    // steps along without going deeper.
    fn one(&self, node: &Node, letter: char) -> Option<bool> {
        Some(match node {
            Node::Char(own) => self.same(*own, letter),
            Node::Any => !ends_line(letter),
            Node::Class(items, negated) => self.holds(items, *negated, letter),
            _ => return None,
        })
    }

    fn walk(&self, node: &Node, at: usize, run: &mut Run, then: &mut dyn FnMut(usize, &mut Run) -> bool) -> bool {
        run.steps += 1;
        if run.steps > STEPS {
            return false;
        }
        let text = run.text;
        match node {
            Node::Empty => then(at, run),
            Node::Char(_) | Node::Any | Node::Class(..) => match text.get(at) {
                Some(&letter) if self.one(node, letter) == Some(true) => then(at + 1, run),
                _ => false,
            },
            Node::Start => at == 0 && then(at, run),
            Node::End => at == text.len() && then(at, run),
            Node::Boundary(not) => {
                let before = at > 0 && is_word(text[at - 1]);
                let after = text.get(at).is_some_and(|&letter| is_word(letter));
                ((before != after) != *not) && then(at, run)
            }
            Node::Group(inner, index) => self.walk(inner, at, run, &mut |to, run| {
                let Some(index) = *index else {
                    return then(to, run);
                };
                let kept = run.groups[index];
                run.groups[index] = Some((at, to));
                if then(to, run) {
                    true
                } else {
                    run.groups[index] = kept;
                    false
                }
            }),
            Node::Look(inner, not) => {
                let kept = run.groups.clone();
                let found = self.walk(inner, at, run, &mut |_, _| true);
                if *not {
                    run.groups = kept;
                    !found && then(at, run)
                } else if found {
                    then(at, run) || {
                        run.groups = kept;
                        false
                    }
                } else {
                    run.groups = kept;
                    false
                }
            }
            Node::Concat(parts) => self.chain(parts, at, run, then),
            Node::Alt(options, firsts) => {
                let next = text.get(at).copied();
                options.iter().zip(firsts).any(|(option, first)| (first.empty || next.is_some_and(|letter| first.allows(letter))) && self.walk(option, at, run, then))
            }
            Node::Repeat(inner, min, max, greedy) => {
                if matches!(**inner, Node::Char(_) | Node::Any | Node::Class(..)) {
                    // One character at a time: count how far the run goes, then hand on from the
                    // longest to the shortest, or the other way where lazy.
                    let most = max.unwrap_or(usize::MAX);
                    let mut reach = 0;
                    while reach < most && text.get(at + reach).is_some_and(|&letter| self.one(inner, letter) == Some(true)) {
                        reach += 1;
                    }
                    if reach < *min {
                        return false;
                    }
                    if *greedy {
                        (*min..=reach).rev().any(|count| then(at + count, run))
                    } else {
                        (*min..=reach).any(|count| then(at + count, run))
                    }
                } else {
                    self.repeat(inner, *min, *max, *greedy, 0, at, run, then)
                }
            }
            Node::Back(index) => {
                let Some(Some((from, to))) = run.groups.get(*index).copied() else {
                    return then(at, run);
                };
                let length = to - from;
                if at + length > text.len() || !(0..length).all(|step| self.same(text[from + step], text[at + step])) {
                    return false;
                }
                then(at + length, run)
            }
        }
    }

    fn chain(&self, parts: &[Node], at: usize, run: &mut Run, then: &mut dyn FnMut(usize, &mut Run) -> bool) -> bool {
        match parts.split_first() {
            None => then(at, run),
            Some((first, rest)) => self.walk(first, at, run, &mut |to, run| self.chain(rest, to, run, then)),
        }
    }

    #[allow(clippy::too_many_arguments)]
    fn repeat(&self, inner: &Node, min: usize, max: Option<usize>, greedy: bool, count: usize, at: usize, run: &mut Run, then: &mut dyn FnMut(usize, &mut Run) -> bool) -> bool {
        if count < min {
            return self.again(inner, min, max, greedy, count, at, run, then);
        }
        // Greedy tries one more iteration before handing on, and lazy hands on first.
        if greedy {
            if self.again(inner, min, max, greedy, count, at, run, then) {
                return true;
            }
            return then(at, run);
        }
        if then(at, run) {
            return true;
        }
        self.again(inner, min, max, greedy, count, at, run, then)
    }

    // One more iteration of a repetition, and the rest of it after.
    #[allow(clippy::too_many_arguments)]
    fn again(&self, inner: &Node, min: usize, max: Option<usize>, greedy: bool, count: usize, at: usize, run: &mut Run, then: &mut dyn FnMut(usize, &mut Run) -> bool) -> bool {
        if max.is_some_and(|most| count >= most) {
            return false;
        }
        self.walk(inner, at, run, &mut |to, run| {
            // An iteration that takes nothing ends the repetition, once the fewest are taken.
            if to == at && count >= min {
                return false;
            }
            self.repeat(inner, min, max, greedy, count + 1, to, run, then)
        })
    }
}

#[cfg(test)]
mod tests {
    use super::Regexp;

    fn at(source: &str, flags: &str, text: &str, from: usize) -> Option<String> {
        let letters: Vec<char> = text.chars().collect();
        let found = Regexp::new(source, flags).unwrap().match_at(&letters, from)?;
        let (start, end) = found[0]?;
        Some(letters[start..end].iter().collect())
    }

    fn groups(source: &str, text: &str) -> Vec<Option<String>> {
        let letters: Vec<char> = text.chars().collect();
        Regexp::new(source, "").unwrap().match_at(&letters, 0).unwrap().iter().map(|span| span.map(|(start, end)| letters[start..end].iter().collect())).collect()
    }

    #[test]
    fn characters_classes_and_quantifiers() {
        assert_eq!(at(r"\d+", "", "123abc", 0).as_deref(), Some("123"));
        assert_eq!(at(r"[a-z_][\w$]*", "", "abc_1$ x", 0).as_deref(), Some("abc_1$"));
        assert_eq!(at(r"[^\]\\]+", "", r"ab\]", 0).as_deref(), Some("ab"));
        assert_eq!(at(r"0[xX][\da-fA-F_']+[uUlLzZ]*", "", "0xFFul;", 0).as_deref(), Some("0xFFul"));
        assert_eq!(at(r"#[\da-fA-F]{3,8}\b", "", "#fff;", 0).as_deref(), Some("#fff"));
        assert_eq!(at(r"#[\da-fA-F]{3,8}\b", "", "#ffg", 0), None);
        assert_eq!(at(r"a{2}", "", "aaa", 0).as_deref(), Some("aa"));
        assert_eq!(at(r"a{2,}", "", "aaaa", 0).as_deref(), Some("aaaa"));
        assert_eq!(at(r"a{,2}", "", "a{,2}", 0).as_deref(), Some("a{,2}"));
        assert_eq!(at(r"\.", "", "x", 0), None);
        assert_eq!(at(r".*", "", "ab\ncd", 0).as_deref(), Some("ab"));
        assert_eq!(at(r"A\x42\u{43}", "", "ABC", 0).as_deref(), Some("ABC"));
    }

    #[test]
    fn sticky_at_a_place_with_anchors_and_boundaries() {
        assert_eq!(at(r"\w+", "", "  word", 2).as_deref(), Some("word"));
        assert_eq!(at(r"\w+", "", "  word", 0), None);
        assert_eq!(at(r"^\s*:\w+", "", "  :label", 0).as_deref(), Some("  :label"));
        assert_eq!(at(r"^:\w+", "", "x:label", 1), None);
        assert_eq!(at(r"\/\/.*$", "", "a // note", 2).as_deref(), Some("// note"));
        assert_eq!(at(r"$", "", "ab", 2).as_deref(), Some(""));
        assert_eq!(at(r"\bif\b", "", "if(", 0).as_deref(), Some("if"));
        assert_eq!(at(r"\bif\b", "", "iff", 0), None);
    }

    #[test]
    fn alternation_groups_lookahead_and_laziness() {
        assert_eq!(at(r"(?:\d[\d_']*)?\.?\d[\d_']*(?:[eE][\-+]?\d+)?", "", "3.25e-4 ", 0).as_deref(), Some("3.25e-4"));
        assert_eq!(at(r"[\w-]+(?=\s*:)", "", "color : red", 0).as_deref(), Some("color"));
        assert_eq!(at(r"[\w-]+(?=\s*:)", "", "color red", 0), None);
        assert_eq!(at(r"\w+(?!\()", "", "call(", 0).as_deref(), Some("cal"));
        assert_eq!(at(r"cat|category", "", "category", 0).as_deref(), Some("cat"));
        assert_eq!(at(r"<.*?>", "", "<a><b>", 0).as_deref(), Some("<a>"));
        assert_eq!(at(r"<.*>", "", "<a><b>", 0).as_deref(), Some("<a><b>"));
        assert_eq!(at(r"(ab)+", "", "ababx", 0).as_deref(), Some("abab"));
        assert_eq!(at(r"(a|ab)(c|bcd)(d*)", "", "abcd", 0).as_deref(), Some("abcd"));
        assert_eq!(at(r"(a*)*b", "", "aaab", 0).as_deref(), Some("aaab"));
        assert_eq!(at(r"(x?)+$", "", "", 0).as_deref(), Some(""));
        assert_eq!(at(r"[]a", "", "a", 0), None);
        assert_eq!(at(r"[^]+", "", "a]b", 0).as_deref(), Some("a]b"));
        assert_eq!(groups(r"(\w+)\s(\w+)", "one two"), [Some("one two".into()), Some("one".into()), Some("two".into())]);
        assert_eq!(groups(r"(a)|(b)", "b"), [Some("b".into()), None, Some("b".into())]);
    }

    #[test]
    fn backreferences_and_case() {
        assert_eq!(at(r#"[rR](['"])(-*)[\[({].*?[\])}]\2\1"#, "", r#"r"-(x)-" tail"#, 0).as_deref(), Some(r#"r"-(x)-""#));
        assert_eq!(at(r#"(['"]).*?\1"#, "", r#"'a"b' c"#, 0).as_deref(), Some(r#"'a"b'"#));
        assert_eq!(at(r"select", "i", "SELECT x", 0).as_deref(), Some("SELECT"));
        assert_eq!(at(r"[a-z]+", "i", "MiXed1", 0).as_deref(), Some("MiXed"));
        assert_eq!(at(r"select", "", "SELECT", 0), None);
    }

    #[test]
    fn unicode_properties_under_the_u_flag() {
        assert_eq!(at(r"[\p{L}_][\p{L}\p{N}_']*", "u", "αβ_1' x", 0).as_deref(), Some("αβ_1'"));
        assert_eq!(at(r"\P{N}+", "u", "ab3", 0).as_deref(), Some("ab"));
        assert_eq!(at(r"\p{Lu}\p{Ll}+", "u", "Être!", 0).as_deref(), Some("Être"));
        assert_eq!(at(r"\p{L}", "", "p{L}", 0).as_deref(), Some("p{L}"));
    }

    #[test]
    fn what_is_not_read_is_refused() {
        assert!(Regexp::new(r"(?<=a)b", "").is_err());
        assert!(Regexp::new(r"(?<name>a)", "").is_err());
        assert!(Regexp::new(r"\p{Script=Greek}", "u").is_err());
        assert!(Regexp::new(r"(a", "").is_err());
        assert!(Regexp::new(r"a)", "").is_err());
        assert!(Regexp::new(r"*a", "").is_err());
    }

    #[test]
    fn a_runaway_pattern_gives_up() {
        let text: String = "a".repeat(40) + "!";
        assert_eq!(at(r"(a|aa)+$", "", &text, 0), None);
    }
}
