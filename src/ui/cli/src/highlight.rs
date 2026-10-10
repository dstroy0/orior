// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The editor's colors, worked out here: a text a line at a time from a grammar, as the page's
//! tokens.js reads one. A grammar is the page's own, handed over as JSON with each pattern as its
//! source and flags, and is kept under a key for the lines asked of it after.
//!
//! A grammar is `{ tokenizer: { root: rules, <state>: rules, ... }, <name>: [words], ... }`. Each rule
//! is tried in order at each place in a line, and the first whose pattern matches there takes the
//! match: `[pattern, token]`; `[pattern, token, next]` or `[pattern, { token, next }]`, after which the
//! state changes; `[pattern, [token, ...]]`, each group of the match a token of its own; or `{ include:
//! "@state" }`, the rules of another state. A token is a name, or `{ cases: { "@<name>": token,
//! "<word>": token, "@default": token } }`, which picks by the matched text, and `@brackets` is a
//! bracket's. A next of `@<state>` enters a state, `@pop` leaves the one entered last, and `@popall`
//! returns to root. A grammar whose `perLine` is set starts every line in root; any other carries its
//! states from the end of one line to the start of the next, written as their names joined by `/`.

use std::collections::{HashMap, HashSet};
use std::sync::{Arc, Mutex, OnceLock};

use serde::Serialize;
use serde_json::Value;

use crate::regexp::Regexp;

/// The longest part of a line that is colored, in the page's units; the rest takes no token.
const LONGEST: usize = 20000;

// A token: a name, by its place in the grammar's names; cases that pick one by the matched text; or one
// token for each group of the match.
#[derive(Debug, Clone)]
enum Token {
    Name(u16),
    Cases(Vec<(String, Token)>),
    Groups(Vec<Token>),
}

#[derive(Debug)]
struct Rule {
    pattern: Regexp,
    token: Option<Token>,
    next: Option<String>,
}

/// A grammar read and ready.
#[derive(Debug)]
pub struct Grammar {
    states: HashMap<String, Vec<Arc<Rule>>>,
    words: HashMap<String, HashSet<String>>,
    per_line: bool,
    ignore_case: bool,
    /// Every name a token can take, the empty name first.
    pub names: Vec<String>,
}

// The place of a token's name among `names`, added where it is new; a bracket's name is
// `delimiter.bracket`.
fn name_of(names: &mut Vec<String>, name: &str) -> u16 {
    let name = if name == "@brackets" { "delimiter.bracket" } else { name };
    match names.iter().position(|one| one == name) {
        Some(at) => at as u16,
        None => {
            names.push(name.to_string());
            (names.len() - 1) as u16
        }
    }
}

fn token_of(value: &Value, names: &mut Vec<String>) -> Result<Option<Token>, String> {
    Ok(match value {
        Value::Null => None,
        Value::String(name) => Some(Token::Name(name_of(names, name))),
        Value::Array(parts) => Some(Token::Groups(parts.iter().map(|part| token_of(part, names).map(|one| one.unwrap_or(Token::Name(0)))).collect::<Result<_, _>>()?)),
        Value::Object(fields) => match fields.get("cases") {
            Some(Value::Object(cases)) => Some(Token::Cases(cases.iter().map(|(key, value)| Ok((key.clone(), token_of(value, names)?.unwrap_or(Token::Name(0))))).collect::<Result<_, String>>()?)),
            _ => return Err("a token that is neither a name, a list nor cases".into()),
        },
        _ => return Err("a token that is neither a name, a list nor cases".into()),
    })
}

fn pattern_of(value: &Value) -> Result<Regexp, String> {
    match value {
        Value::String(source) => Regexp::new(source, ""),
        Value::Object(fields) => {
            let source = fields.get("pattern").and_then(Value::as_str).ok_or("a pattern with no source")?;
            Regexp::new(source, fields.get("flags").and_then(Value::as_str).unwrap_or(""))
        }
        _ => Err("a pattern that is neither text nor a source and flags".into()),
    }
}

impl Grammar {
    /// Reads a grammar, or says what in it is not read here.
    pub fn new(def: &Value) -> Result<Grammar, String> {
        let fields = def.as_object().ok_or("a grammar that is no object")?;
        let ignore_case = fields.get("ignoreCase").and_then(Value::as_bool).unwrap_or(false);
        let mut words = HashMap::new();
        for (name, value) in fields {
            if let Value::Array(list) = value {
                let set = list.iter().filter_map(Value::as_str).map(|word| if ignore_case { word.to_lowercase() } else { word.to_string() }).collect();
                words.insert(name.clone(), set);
            }
        }
        let tokenizer = fields.get("tokenizer").and_then(Value::as_object).ok_or("a grammar with no tokenizer")?;
        let mut names = vec![String::new()];
        let mut own: HashMap<String, Vec<Result<Arc<Rule>, String>>> = HashMap::new();
        for (name, rules) in tokenizer {
            let mut read = Vec::new();
            for rule in rules.as_array().ok_or("a state that is no list")? {
                if let Some(included) = rule.get("include").and_then(Value::as_str) {
                    read.push(Err(included.trim_start_matches('@').to_string()));
                    continue;
                }
                let parts = rule.as_array().ok_or("a rule that is no list")?;
                let pattern = pattern_of(parts.first().ok_or("a rule with no pattern")?)?;
                let action = parts.get(1).unwrap_or(&Value::Null);
                let own_next = action.as_object().filter(|fields| !fields.contains_key("cases"));
                let (token, next) = match own_next {
                    Some(fields) => (token_of(fields.get("token").unwrap_or(&Value::Null), &mut names)?, fields.get("next").and_then(Value::as_str).map(str::to_string)),
                    None => (token_of(action, &mut names)?, parts.get(2).and_then(Value::as_str).map(str::to_string)),
                };
                read.push(Ok(Arc::new(Rule { pattern, token, next })));
            }
            own.insert(name.clone(), read);
        }
        // A state's rules with each include read in its place, a state included within itself once.
        fn expand(name: &str, own: &HashMap<String, Vec<Result<Arc<Rule>, String>>>, seen: &mut Vec<String>) -> Vec<Arc<Rule>> {
            let mut out = Vec::new();
            for rule in own.get(name).map(Vec::as_slice).unwrap_or_default() {
                match rule {
                    Ok(rule) => out.push(rule.clone()),
                    Err(included) if !seen.contains(included) => {
                        seen.push(included.clone());
                        out.extend(expand(included, own, seen));
                        seen.pop();
                    }
                    Err(_) => {}
                }
            }
            out
        }
        let states = own.keys().map(|name| (name.clone(), expand(name, &own, &mut vec![name.clone()]))).collect();
        if names.len() > usize::from(u16::MAX) {
            return Err("a grammar with too many names".into());
        }
        Ok(Grammar { states, words, per_line: fields.get("perLine").and_then(Value::as_bool).unwrap_or(false), ignore_case, names })
    }

    // The name a token takes for the matched text, which is read only where the token picks by it.
    fn resolve(&self, token: Option<&Token>, text: &dyn Fn() -> String) -> u16 {
        match token {
            None | Some(Token::Groups(_)) => 0,
            Some(Token::Name(name)) => *name,
            Some(Token::Cases(cases)) => {
                let text = text();
                let key = if self.ignore_case { text.to_lowercase() } else { text.clone() };
                for (name, value) in cases {
                    if name == "@default" {
                        continue;
                    }
                    let hit = match name.strip_prefix('@') {
                        Some(list) => self.words.get(list).is_some_and(|words| words.contains(&key)),
                        None => *name == text,
                    };
                    if hit {
                        return self.resolve(Some(value), &|| text.clone());
                    }
                }
                self.resolve(cases.iter().find(|(name, _)| name == "@default").map(|(_, value)| value), &|| text.clone())
            }
        }
    }

    fn rules(&self, state: &str) -> &[Arc<Rule>] {
        self.states.get(state).or_else(|| self.states.get("root")).map(Vec::as_slice).unwrap_or_default()
    }

    /// One line's tokens as runs `(col, token)`, each running to the next run's col or the line's
    /// end, cols in UTF-16 units, and the state the line ends in.
    pub fn line(&self, text: &str, state: &str) -> (Vec<(u32, u16)>, String) {
        let letters: Vec<char> = text.chars().collect();
        // Where each character starts in UTF-16 units, the line's end last.
        let mut cols = Vec::with_capacity(letters.len() + 1);
        let mut col = 0u32;
        for letter in &letters {
            cols.push(col);
            col += letter.len_utf16() as u32;
        }
        cols.push(col);
        let length = cols.iter().position(|&one| one as usize >= LONGEST).unwrap_or(letters.len()).min(letters.len());
        let mut stack: Vec<String> = state.split('/').map(str::to_string).collect();
        let mut runs: Vec<(u32, u16)> = Vec::new();
        let mut push = |at: usize, name: u16| {
            let col = cols[at];
            if runs.last().is_some_and(|last| last.1 == name) {
                return;
            }
            if runs.last().is_some_and(|last| last.0 == col) {
                runs.pop();
                if runs.last().is_some_and(|last| last.1 == name) {
                    return;
                }
            }
            runs.push((col, name));
        };
        let mut at = 0;
        let mut stalled = 0;
        // The rules of the state on top, read again only as the state changes, and one buffer for every
        // match's groups.
        let mut rules = self.rules(stack.last().map(String::as_str).unwrap_or("root"));
        let mut found = Vec::new();
        while at < length {
            let mut taken = false;
            for rule in rules {
                if !rule.pattern.match_into(&letters, at, &mut found) {
                    continue;
                }
                let (start, end) = found[0].unwrap_or((at, at));
                let size = end - start;
                if size == 0 && (rule.next.is_none() || stalled > 2) {
                    continue;
                }
                match &rule.token {
                    Some(Token::Groups(tokens)) => {
                        let mut place = at;
                        for (index, token) in tokens.iter().enumerate() {
                            let part = found.get(index + 1).copied().flatten().map_or(0, |(from, to)| to - from);
                            if part > 0 {
                                let span = found[index + 1];
                                push(place.min(letters.len()), self.resolve(Some(token), &|| span.map(|(from, to)| letters[from..to].iter().collect()).unwrap_or_default()));
                            }
                            place += part;
                        }
                    }
                    token if size > 0 => {
                        push(at, self.resolve(token.as_ref(), &|| letters[start..end].iter().collect()));
                    }
                    _ => {}
                }
                stalled = if size > 0 { 0 } else { stalled + 1 };
                at += size;
                if rule.next.is_some() {
                    step(&mut stack, rule.next.as_deref());
                    rules = self.rules(stack.last().map(String::as_str).unwrap_or("root"));
                }
                taken = true;
                break;
            }
            if !taken {
                push(at, 0);
                at += 1;
                stalled = 0;
            }
        }
        if length < letters.len() {
            push(length, 0);
        }
        // A rule that matches nothing at the line's end and changes the state still applies there,
        // once: it is how a state that lasts to the end of its line ends.
        let ending = self.states.get(stack.last().map(String::as_str).unwrap_or("root")).and_then(|rules| {
            rules.iter().find(|rule| rule.next.is_some() && rule.pattern.match_at(&letters, letters.len()).and_then(|found| found[0]).is_some_and(|(from, to)| from == to))
        });
        if let Some(rule) = ending {
            step(&mut stack, rule.next.as_deref());
        }
        if runs.is_empty() {
            runs.push((0, 0));
        }
        (runs, if self.per_line { "root".into() } else { stack.join("/") })
    }
}

fn step(stack: &mut Vec<String>, next: Option<&str>) {
    match next {
        None | Some("") => {}
        Some("@pop") => {
            if stack.len() > 1 {
                stack.pop();
            }
        }
        Some("@popall") => stack.truncate(1),
        Some(state) => stack.push(state.trim_start_matches('@').to_string()),
    }
}

/// Lines colored: the names a token can take, each line's runs as a column and the place of its
/// name, and the state each line ends in.
#[derive(Serialize, Debug)]
pub struct Colored {
    pub names: Vec<String>,
    pub runs: Vec<Vec<(u32, u16)>>,
    pub states: Vec<String>,
}

fn kept() -> &'static Mutex<HashMap<String, Arc<Grammar>>> {
    static KEPT: OnceLock<Mutex<HashMap<String, Arc<Grammar>>>> = OnceLock::new();
    KEPT.get_or_init(|| Mutex::new(HashMap::new()))
}

/// Reads a grammar and keeps it under `key`, or says what in it is not read here.
pub fn keep(key: &str, def: &Value) -> Result<(), String> {
    let grammar = Grammar::new(def)?;
    kept().lock().map_err(|error| error.to_string())?.insert(key.to_string(), Arc::new(grammar));
    Ok(())
}

/// Colors `lines` with the grammar kept under `key`, the first starting in `state` and each after in
/// the state the one before it ends in.
pub fn color(key: &str, state: &str, lines: &[String]) -> Result<Colored, String> {
    let grammar = kept().lock().map_err(|error| error.to_string())?.get(key).cloned().ok_or_else(|| format!("no grammar is kept under {key}"))?;
    let mut colored = Colored { names: grammar.names.clone(), runs: Vec::with_capacity(lines.len()), states: Vec::with_capacity(lines.len()) };
    let mut state = state.to_string();
    for line in lines {
        let (runs, after) = grammar.line(line, &state);
        colored.runs.push(runs);
        colored.states.push(after.clone());
        state = after;
    }
    Ok(colored)
}

#[cfg(test)]
mod tests {
    use super::Grammar;
    use serde_json::json;

    fn grammar() -> Grammar {
        Grammar::new(&json!({
            "keywords": ["if", "return"],
            "tokenizer": {
                "root": [
                    [{ "pattern": "(\\w+)(:)", "flags": "" }, ["label", "delimiter"]],
                    [{ "pattern": "[a-z_]\\w*", "flags": "" }, { "cases": { "@keywords": "keyword", "@default": "identifier" } }],
                    [{ "pattern": "\\d+", "flags": "" }, "number"],
                    [{ "pattern": "\"", "flags": "" }, "string", "@string"],
                    [{ "pattern": "\\/\\*", "flags": "" }, "comment", "@comment"],
                    [{ "pattern": "[()]", "flags": "" }, "@brackets"],
                    { "include": "@space" }
                ],
                "space": [[{ "pattern": "\\s+", "flags": "" }, ""]],
                "string": [
                    [{ "pattern": "[^\"]+", "flags": "" }, "string"],
                    [{ "pattern": "\"", "flags": "" }, "string", "@pop"]
                ],
                "comment": [
                    [{ "pattern": "[^*]+", "flags": "" }, "comment"],
                    [{ "pattern": "\\*\\/", "flags": "" }, "comment", "@pop"],
                    [{ "pattern": "\\*", "flags": "" }, "comment"]
                ]
            }
        }))
        .unwrap()
    }

    fn names<'a>(grammar: &'a Grammar, runs: &[(u32, u16)]) -> Vec<(u32, &'a str)> {
        runs.iter().map(|(col, name)| (*col, grammar.names[usize::from(*name)].as_str())).collect()
    }

    #[test]
    fn a_line_is_colored_by_its_rules() {
        let grammar = grammar();
        let (runs, state) = grammar.line("if (x) return 12", "root");
        assert_eq!(names(&grammar, &runs), [(0, "keyword"), (2, ""), (3, "delimiter.bracket"), (4, "identifier"), (5, "delimiter.bracket"), (6, ""), (7, "keyword"), (13, ""), (14, "number")]);
        assert_eq!(state, "root");
    }

    #[test]
    fn a_state_carries_to_the_next_line() {
        let grammar = grammar();
        let (runs, state) = grammar.line("a /* open", "root");
        assert_eq!(names(&grammar, &runs), [(0, "identifier"), (1, ""), (2, "comment")]);
        assert_eq!(state, "root/comment");
        let (runs, state) = grammar.line("still */ b", &state);
        assert_eq!(names(&grammar, &runs), [(0, "comment"), (8, ""), (9, "identifier")]);
        assert_eq!(state, "root");
    }

    #[test]
    fn columns_count_as_the_page_counts_them() {
        let grammar = grammar();
        let (runs, _) = grammar.line("\"😀\" 7", "root");
        assert_eq!(names(&grammar, &runs), [(0, "string"), (4, ""), (5, "number")]);
    }

    #[test]
    fn a_rule_reads_its_groups_and_a_grammar_its_includes() {
        let grammar = grammar();
        let (runs, _) = grammar.line("key: 1", "root");
        assert_eq!(names(&grammar, &runs), [(0, "label"), (3, "delimiter"), (4, ""), (5, "number")]);
    }

    #[test]
    #[ignore = "times a grammar named by ORIOR_GRAMMAR over fifty thousand lines"]
    fn speed() {
        let path = std::env::var("ORIOR_GRAMMAR").unwrap();
        let def: serde_json::Value = serde_json::from_str(&std::fs::read_to_string(path).unwrap()).unwrap();
        let grammar = Grammar::new(&def).unwrap();
        let lines: Vec<String> = (0..12500).flat_map(|n| [format!("function f{n}(a, b) {{"), format!("  /* {n} */ const total = a + b * {n};"), format!("  return [total, \"{n}\"];"), "}".to_string()]).collect();
        for _ in 0..3 {
            let began = std::time::Instant::now();
            let mut state = "root".to_string();
            for line in &lines {
                state = grammar.line(line, &state).1;
            }
            println!("{} lines in {:?}", lines.len(), began.elapsed());
        }
    }

    #[test]
    fn what_matches_no_rule_has_no_token() {
        let grammar = grammar();
        let (runs, _) = grammar.line("#", "root");
        assert_eq!(names(&grammar, &runs), [(0, "")]);
        assert!(Grammar::new(&json!({ "tokenizer": { "root": [[{ "pattern": "(?<=a)b" }, "x"]] } })).is_err());
    }
}
