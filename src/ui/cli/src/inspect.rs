// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Inspections: checks of orior's own over the tree's Python and JavaScript, each finding given to the
//! editor as a diagnostic beside the language servers', with the edits of its quick fix where one
//! applies.
//!
//! What a file says alone, python.rs and javascript.rs find: a name set and never read, code no path
//! reaches, and a Python docstring out of PEP 257's form. What the tree says, the tree's index finds,
//! from each file's imports and classes:
//!
//! - an import that leads back round to the file that makes it, through the imports the files it
//!   reaches make as they run, named with the files it passes through;
//! - an attribute a method reads of `self` that neither its class, a class it comes from, nor a class
//!   that comes from it sets, as a mixin's methods are found in the classes it is mixed into and a
//!   class's in the mixins it takes in. A class whose family holds a class from outside the tree, or
//!   that sets its attributes by name, is left alone. What each class's family sets is kept, and a
//!   language server's report that a class has no such attribute, as pyright gives of a mixin, is
//!   not passed on where the family sets it.
//!
//! A Python import is found from the importing file's folder, from the folder above its outermost
//! package, and from the tree's top folder, in that order; a relative one from its package. A
//! JavaScript import is a relative path, with `.js`, `.mjs` or `/index.js` after it where the path
//! names no file.

mod javascript;
mod python;

use std::collections::{HashMap, HashSet, VecDeque};
use std::path::Path;

use serde_json::{json, Value};

use crate::servers::{Place, TextEdit};

/// How a finding is shown: a warning, or a note.
#[derive(Clone, Copy, Debug, PartialEq)]
pub enum Severity {
    Warning,
    Note,
}

/// A quick fix: what it says it does, and its edits to the file.
#[derive(Clone, Debug, PartialEq)]
pub struct Fix {
    pub title: String,
    pub edits: Vec<TextEdit>,
}

/// What an inspection found: its span, how it is shown, what it says, its name, and its fixes.
#[derive(Clone, Debug, PartialEq)]
pub struct Finding {
    pub from: Place,
    pub to: Place,
    pub severity: Severity,
    pub message: String,
    pub code: &'static str,
    pub fixes: Vec<Fix>,
}

/// An import a file makes as it runs: the module or path it names, the names a Python `from` import
/// takes from it with their aliases, the alias of a Python `import … as`, and its span.
#[derive(Clone, Debug)]
pub struct Import {
    pub spec: String,
    pub names: Vec<(String, Option<String>)>,
    pub alias: Option<String>,
    pub from: Place,
    pub to: Place,
}

/// A Python class declared at a file's top level: its name and its span, the bases it names, what its
/// body and its methods set, each attribute its methods read of `self` with its span, and whether it
/// sets attributes by name.
#[derive(Clone, Debug)]
pub struct Class {
    pub name: String,
    pub from: Place,
    pub to: Place,
    pub bases: Vec<String>,
    pub has: HashSet<String>,
    pub reads: Vec<(String, Place, Place)>,
    pub dynamic: bool,
}

/// What a file holds for the inspections: its imports, its classes, and what it says alone.
#[derive(Clone, Debug, Default)]
pub struct Facts {
    pub imports: Vec<Import>,
    pub classes: Vec<Class>,
    pub local: Vec<Finding>,
}

/// The languages the inspections read, by the editor's names for them.
pub const LANGUAGES: [&str; 2] = ["python", "javascript"];

/// The facts of a file's text in `language`, where the inspections read the language.
pub fn facts(language: &str, text: &str) -> Option<Facts> {
    match language {
        "python" => Some(python::facts(text)),
        "javascript" => Some(javascript::facts(text)),
        _ => None,
    }
}

/// A finding as a diagnostic of the protocol's form, its fixes kept in its data.
pub fn value_of(finding: &Finding) -> Value {
    let place = |place: &Place| json!({"line": place.line, "character": place.col});
    let fixes: Vec<Value> = finding.fixes.iter().map(|fix| json!({"title": fix.title, "edits": fix.edits})).collect();
    json!({
        "range": {"start": place(&finding.from), "end": place(&finding.to)},
        "severity": match finding.severity { Severity::Warning => 2, Severity::Note => 3 },
        "message": finding.message,
        "source": "orior",
        "code": finding.code,
        "data": {"orior": fixes},
    })
}

/// A reading of the text a scanner walks, by its byte, its line and its column in UTF-16 units, as
/// the editor and the protocol count columns.
pub(crate) struct Scan<'a> {
    pub src: &'a str,
    pub pos: usize,
    line: u32,
    col: u32,
}

impl<'a> Scan<'a> {
    pub fn new(src: &'a str) -> Scan<'a> {
        Scan { src, pos: 0, line: 0, col: 0 }
    }

    pub fn peek(&self) -> Option<char> {
        self.src[self.pos..].chars().next()
    }

    pub fn peek_at(&self, ahead: usize) -> Option<char> {
        self.src[self.pos..].chars().nth(ahead)
    }

    pub fn bump(&mut self) -> Option<char> {
        let c = self.peek()?;
        self.pos += c.len_utf8();
        if c == '\n' {
            self.line += 1;
            self.col = 0;
        } else {
            self.col += c.len_utf16() as u32;
        }
        Some(c)
    }

    pub fn place(&self) -> Place {
        Place { line: self.line, col: self.col }
    }

    /// Moves to the end of the line, before its line end.
    pub fn skip_line(&mut self) {
        while self.peek().is_some_and(|c| c != '\n') {
            self.bump();
        }
    }
}

/// A file of the tree's index: its language and its facts.
struct File {
    language: String,
    facts: Facts,
}

/// The tree's index: each Python and JavaScript file's facts, by its path in the tree.
#[derive(Default)]
pub struct Tree {
    files: HashMap<String, File>,
}

/// Bases that add no attribute a class reads of `self`.
const BARE_BASES: [&str; 7] = ["object", "ABC", "abc.ABC", "Generic", "typing.Generic", "Protocol", "typing.Protocol"];

/// A class by its file and its place in that file's classes.
type ClassAt = (String, usize);

/// Each class's ancestors and whether all are in the tree, and each class's descendants.
type Families = (HashMap<ClassAt, (Vec<ClassAt>, bool)>, HashMap<ClassAt, Vec<ClassAt>>);

impl Tree {
    /// Sets the facts of the file at `path` from its text, or takes it out where its language is none
    /// the inspections read.
    pub fn set(&mut self, path: &str, language: &str, text: &str) {
        match facts(language, text) {
            Some(facts) => {
                self.files.insert(path.to_string(), File { language: language.to_string(), facts });
            }
            None => {
                self.files.remove(path);
            }
        }
    }

    pub fn remove(&mut self, path: &str) {
        self.files.remove(path);
    }

    pub fn holds(&self, path: &str) -> bool {
        self.files.contains_key(path)
    }

    /// Every file of the index.
    pub fn paths(&self) -> Vec<String> {
        self.files.keys().cloned().collect()
    }

    /// Every finding in every file, by its path: what each says alone, then the tree's.
    pub fn findings(&self) -> HashMap<String, Vec<Finding>> {
        let mut out: HashMap<String, Vec<Finding>> = self.files.iter().map(|(path, file)| (path.clone(), file.facts.local.clone())).collect();
        self.cycles(&mut out);
        self.attributes(&mut out);
        out
    }

    /// The files an import of the file at `path` runs.
    fn targets(&self, path: &str, import: &Import) -> Vec<String> {
        match self.files.get(path).map(|file| file.language.as_str()) {
            Some("python") => self.python_targets(path, import),
            Some("javascript") => self.javascript_target(path, &import.spec).into_iter().collect(),
            _ => Vec::new(),
        }
    }

    /// The file a Python module names from `root`: its own file, or its package's `__init__.py`.
    fn module_in(&self, root: &str, dotted: &str) -> Option<String> {
        let base = if root.is_empty() { dotted.replace('.', "/") } else if dotted.is_empty() { root.to_string() } else { format!("{root}/{}", dotted.replace('.', "/")) };
        [format!("{base}.py"), format!("{base}/__init__.py")].into_iter().find(|one| self.files.contains_key(one))
    }

    /// The folders a Python import is found from: the file's folder, the folder above its outermost
    /// package, and the tree's top folder.
    fn roots(&self, path: &str) -> Vec<String> {
        let folder = parent(path);
        let mut top = folder.clone();
        while self.files.contains_key(&join(&top, "__init__.py")) && !top.is_empty() {
            top = parent(&top);
        }
        let mut roots = vec![folder, top, String::new()];
        roots.dedup();
        roots
    }

    /// The module a Python import names, found from `path`.
    fn python_module(&self, path: &str, spec: &str) -> Option<String> {
        let dots = spec.chars().take_while(|c| *c == '.').count();
        let dotted = &spec[dots..];
        if dots > 0 {
            let mut base = parent(path);
            for _ in 1..dots {
                base = parent(&base);
            }
            return self.module_in(&base, dotted);
        }
        self.roots(path).iter().find_map(|root| self.module_in(root, dotted))
    }

    /// The files a Python import runs: each package on the way to its module, the module, and each
    /// module a `from` import takes from a package.
    fn python_targets(&self, path: &str, import: &Import) -> Vec<String> {
        let mut out = Vec::new();
        let dots = import.spec.chars().take_while(|c| *c == '.').count();
        let parts: Vec<&str> = import.spec[dots..].split('.').filter(|part| !part.is_empty()).collect();
        for end in 1..=parts.len() {
            let spec = format!("{}{}", &import.spec[..dots], parts[..end].join("."));
            if let Some(found) = self.python_module(path, &spec) {
                out.push(found);
            }
        }
        if parts.is_empty() && dots > 0 {
            out.extend(self.python_module(path, &import.spec));
        }
        for (name, _) in &import.names {
            let spec = if import.spec.ends_with('.') { format!("{}{name}", import.spec) } else { format!("{}.{name}", import.spec) };
            if let Some(found) = self.python_module(path, &spec) {
                out.push(found);
            }
        }
        out.retain(|one| one != path);
        out.dedup();
        out
    }

    /// The file a JavaScript import names by a relative path.
    fn javascript_target(&self, path: &str, spec: &str) -> Option<String> {
        let folder = parent(path);
        let mut parts: Vec<&str> = folder.split('/').filter(|part| !part.is_empty()).collect();
        for part in spec.split('/') {
            match part {
                "." | "" => {}
                ".." => {
                    parts.pop()?;
                }
                other => parts.push(other),
            }
        }
        let base = parts.join("/");
        [base.clone(), format!("{base}.js"), format!("{base}.mjs"), format!("{base}/index.js")].into_iter().find(|one| self.files.contains_key(one))
    }

    /// Each import that leads back round to the file that makes it.
    fn cycles(&self, out: &mut HashMap<String, Vec<Finding>>) {
        let mut edges: HashMap<&str, Vec<(usize, String)>> = HashMap::new();
        for (path, file) in &self.files {
            for (at, import) in file.facts.imports.iter().enumerate() {
                for target in self.targets(path, import) {
                    edges.entry(path.as_str()).or_default().push((at, target));
                }
            }
        }
        for (path, file) in &self.files {
            let mut said = HashSet::new();
            for (at, target) in edges.get(path.as_str()).into_iter().flatten() {
                if said.contains(at) {
                    continue;
                }
                if let Some(round) = path_back(&edges, target, path) {
                    said.insert(*at);
                    let import = &file.facts.imports[*at];
                    let mut names = vec![path.clone()];
                    names.extend(round);
                    out.entry(path.clone()).or_default().push(Finding {
                        from: import.from.clone(),
                        to: import.to.clone(),
                        severity: Severity::Warning,
                        message: format!("This import leads back round to the file that makes it: {}.", names.join(" → ")),
                        code: "import-cycle",
                        fixes: Vec::new(),
                    });
                }
            }
        }
    }

    /// The class a Python file's base names: by a class of the file, a `from` import, or an import of
    /// the module it is read from. None where it is outside the tree.
    fn base_of(&self, path: &str, base: &str, depth: usize) -> Option<ClassAt> {
        let file = self.files.get(path)?;
        if depth > 6 {
            return None;
        }
        let (module, name) = match base.rsplit_once('.') {
            Some((module, name)) => (Some(module), name),
            None => (None, base),
        };
        match module {
            None => {
                if let Some(at) = file.facts.classes.iter().position(|class| class.name == name) {
                    return Some((path.to_string(), at));
                }
                for import in &file.facts.imports {
                    for (taken, alias) in &import.names {
                        if alias.as_deref().unwrap_or(taken) == name {
                            let from = self.python_module(path, &import.spec)?;
                            return self.base_of(&from, taken, depth + 1);
                        }
                    }
                }
                None
            }
            Some(module) => {
                for import in &file.facts.imports {
                    if import.names.is_empty() && (import.alias.as_deref() == Some(module) || (import.alias.is_none() && import.spec == module)) {
                        let from = self.python_module(path, &import.spec)?;
                        return self.base_of(&from, name, depth + 1);
                    }
                    for (taken, alias) in &import.names {
                        if alias.as_deref().unwrap_or(taken) == module {
                            let spec = if import.spec.ends_with('.') { format!("{}{taken}", import.spec) } else { format!("{}.{taken}", import.spec) };
                            let from = self.python_module(path, &spec)?;
                            return self.base_of(&from, name, depth + 1);
                        }
                    }
                }
                None
            }
        }
    }

    fn class(&self, at: &ClassAt) -> &Class {
        &self.files[&at.0].facts.classes[at.1]
    }

    /// Each class's ancestors in the tree, itself first, and whether all of them are in it; and each
    /// class's descendants, itself among them.
    fn families(&self) -> Families {
        let mut bases: HashMap<ClassAt, (Vec<ClassAt>, bool)> = HashMap::new();
        for (path, file) in &self.files {
            if file.language != "python" {
                continue;
            }
            for (at, class) in file.facts.classes.iter().enumerate() {
                let named: Vec<Option<ClassAt>> = class.bases.iter().filter(|base| !BARE_BASES.contains(&base.as_str())).map(|base| self.base_of(path, base, 0)).collect();
                let all = named.iter().all(Option::is_some);
                bases.insert((path.clone(), at), (named.into_iter().flatten().collect(), all));
            }
        }
        // Each class's ancestors in the tree, and whether all of them are in it.
        let mut ancestors: HashMap<ClassAt, (Vec<ClassAt>, bool)> = HashMap::new();
        for key in bases.keys() {
            let mut seen = vec![key.clone()];
            let mut waiting = vec![key.clone()];
            let mut known = true;
            while let Some(one) = waiting.pop() {
                let Some(list) = bases.get(&one) else {
                    known = false;
                    continue;
                };
                let (found, all) = list;
                known &= *all;
                for base in found {
                    if !seen.contains(base) {
                        seen.push(base.clone());
                        waiting.push(base.clone());
                    }
                }
            }
            ancestors.insert(key.clone(), (seen, known));
        }
        let mut descendants: HashMap<ClassAt, Vec<ClassAt>> = HashMap::new();
        for (key, (family, _)) in &ancestors {
            for ancestor in family {
                descendants.entry(ancestor.clone()).or_default().push(key.clone());
            }
        }
        (ancestors, descendants)
    }

    /// What each Python class's family sets, by its file and its name: what it, the classes it comes
    /// from, the classes that come from it and the classes those come from set.
    pub fn supplied(&self) -> HashMap<String, HashMap<String, HashSet<String>>> {
        let (ancestors, descendants) = self.families();
        let mut out: HashMap<String, HashMap<String, HashSet<String>>> = HashMap::new();
        for key in ancestors.keys() {
            let mut names = HashSet::new();
            for lower in descendants.get(key).into_iter().flatten() {
                for one in ancestors.get(lower).map(|(family, _)| family.as_slice()).unwrap_or_default() {
                    names.extend(self.class(one).has.iter().cloned());
                }
            }
            out.entry(key.0.clone()).or_default().insert(self.class(key).name.clone(), names);
        }
        out
    }

    /// Each attribute a method reads of `self` that no class of its family sets.
    fn attributes(&self, out: &mut HashMap<String, Vec<Finding>>) {
        let (ancestors, descendants) = self.families();
        let sets = |key: &ClassAt, name: &str| -> Option<bool> {
            let (family, known) = ancestors.get(key)?;
            if !known || family.iter().any(|one| self.class(one).dynamic) {
                return None;
            }
            Some(family.iter().any(|one| self.class(one).has.contains(name)))
        };
        for (key, (_, known)) in &ancestors {
            if !known {
                continue;
            }
            let class = self.class(key);
            let lower = descendants.get(key).cloned().unwrap_or_default();
            let mut reported = HashSet::new();
            for (name, from, to) in &class.reads {
                if name.starts_with("__") && name.ends_with("__") || reported.contains(name) {
                    continue;
                }
                let found = lower.iter().map(|one| sets(one, name)).try_fold(false, |any, one| one.map(|one| any || one));
                if found == Some(false) {
                    reported.insert(name.clone());
                    out.entry(key.0.clone()).or_default().push(Finding {
                        from: from.clone(),
                        to: to.clone(),
                        severity: Severity::Warning,
                        message: format!("{} has no {name}: neither it, a class it comes from, nor a class that comes from it sets {name}.", class.name),
                        code: "unknown-attribute",
                        fixes: Vec::new(),
                    });
                }
            }
        }
    }
}

/// The files an import from `from` passes through back to `to`, the shortest way, ending with `to`.
fn path_back(edges: &HashMap<&str, Vec<(usize, String)>>, from: &str, to: &str) -> Option<Vec<String>> {
    let mut came: HashMap<String, String> = HashMap::new();
    let mut waiting = VecDeque::from([from.to_string()]);
    let mut seen = HashSet::from([from.to_string()]);
    while let Some(one) = waiting.pop_front() {
        if one == to {
            let mut way = vec![one.clone()];
            let mut at = one;
            while let Some(before) = came.get(&at) {
                way.push(before.clone());
                at = before.clone();
            }
            way.reverse();
            return Some(way);
        }
        for (_, next) in edges.get(one.as_str()).into_iter().flatten() {
            if seen.insert(next.clone()) {
                came.insert(next.clone(), one.clone());
                waiting.push_back(next.clone());
            }
        }
    }
    None
}

fn parent(path: &str) -> String {
    path.rsplit_once('/').map_or_else(String::new, |(folder, _)| folder.to_string())
}

fn join(folder: &str, name: &str) -> String {
    if folder.is_empty() { name.to_string() } else { format!("{folder}/{name}") }
}

/// The language of a path by its extension, where the inspections read it.
pub fn language_of(path: &str) -> Option<&'static str> {
    match Path::new(path).extension()?.to_str()? {
        "py" => Some("python"),
        "js" | "mjs" => Some("javascript"),
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn messages(language: &str, text: &str) -> Vec<String> {
        facts(language, text).unwrap().local.iter().map(|one| format!("{}:{} {}", one.from.line, one.from.col, one.message)).collect()
    }

    fn apply(text: &str, fix: &Fix) -> String {
        crate::servers::apply(text, &fix.edits)
    }

    #[test]
    fn a_python_name_set_and_never_read_is_found_and_its_fix_keeps_a_call() {
        let text = "def f(a):\n    total = 0\n    seen = g(a)\n    for x in a:\n        total += x\n    with open(a) as handle:\n        pass\n    try:\n        pass\n    except ValueError as error:\n        pass\n    return total\n";
        let found = facts("python", text).unwrap().local;
        let names: Vec<&str> = found.iter().map(|one| one.message.split(' ').next().unwrap()).collect();
        assert_eq!(names, vec!["seen", "handle", "error"], "{found:?}");
        assert_eq!(apply(text, &found[0].fixes[0]), text.replace("    seen = g(a)\n", "    g(a)\n"));
        assert!(apply(text, &found[1].fixes[0]).contains("with open(a):"));
        assert!(apply(text, &found[2].fixes[0]).contains("except ValueError:"));
    }

    #[test]
    fn a_python_name_read_in_a_closure_an_f_string_or_with_locals_is_not_found() {
        let text = "def f():\n    a = 1\n    b = 2\n    c = 3\n    _d = 4\n    def inner():\n        return a\n    print(f\"{b:>{c}}\")\n    return inner\n\ndef g():\n    e = 5\n    return locals()\n";
        assert!(messages("python", text).is_empty(), "{:?}", messages("python", text));
    }

    #[test]
    fn python_code_after_a_return_is_found_and_taken_out() {
        let text = "def f(x):\n    if x:\n        return 1\n        print(x)\n    return 2\n    x += 1\n    if x:\n        print(x)\n\nprint(f(1))\n";
        let found = facts("python", text).unwrap().local;
        let unreachable: Vec<&Finding> = found.iter().filter(|one| one.code == "unreachable").collect();
        assert_eq!(unreachable.iter().map(|one| (one.from.line, one.to.line)).collect::<Vec<_>>(), vec![(3, 3), (5, 7)]);
        assert_eq!(apply(text, &unreachable[1].fixes[0]), "def f(x):\n    if x:\n        return 1\n        print(x)\n    return 2\n\nprint(f(1))\n");
    }

    #[test]
    fn a_docstring_out_of_pep_257_form_is_found_and_fixed() {
        let text = "def f():\n    '''Return one'''\n    return 1\n\ndef g():\n    \"\"\" Return two. \"\"\"\n\ndef h():\n    \"\"\"Return three.\n    More.\"\"\"\n\ndef k():\n    \"\"\"Return four.\n\n    More.\n    \"\"\"\n";
        let found = facts("python", text).unwrap().local;
        let codes: Vec<&str> = found.iter().map(|one| one.code).collect();
        assert_eq!(codes, vec!["docstring-quotes", "docstring-period", "docstring-space", "docstring-blank", "docstring-close"], "{found:?}");
        assert_eq!(apply(text, &found[0].fixes[0]), text.replace("'''Return one'''", "\"\"\"Return one\"\"\""));
        assert_eq!(apply(text, &found[1].fixes[0]), text.replace("'''Return one'''", "'''Return one.'''"));
        assert_eq!(apply(text, &found[2].fixes[0]), text.replace("\"\"\" Return two. \"\"\"", "\"\"\"Return two.\"\"\""));
        assert_eq!(apply(text, &found[4].fixes[0]), text.replace("    More.\"\"\"", "    More.\n    \"\"\""));
    }

    #[test]
    fn a_javascript_declaration_never_read_and_code_after_a_return_are_found() {
        let text = "export function f(a) {\n  const unused = 1;\n  const kept = g(a);\n  let used = a + 1;\n  const obj = { used: 1, other };\n  const after = `${obj.used}`;\n  if (a) return after;\n  return used;\n  console.log(a);\n}\n\nfunction h(x) {\n  switch (x) {\n    case 1:\n      return 1;\n      x += 1;\n    case 2:\n      break;\n  }\n  return /a\\/b/.test(x) ? x / 2 : x;\n}\n";
        let found = facts("javascript", text).unwrap().local;
        let said: Vec<String> = found.iter().map(|one| format!("{}:{} {}", one.from.line, one.code, one.message.split(' ').next().unwrap())).collect();
        assert_eq!(said, vec!["1:unread unused", "2:unread kept", "8:unreachable No", "15:unreachable No"], "{found:?}");
        assert_eq!(apply(text, &found[0].fixes[0]), text.replace("  const unused = 1;\n", ""));
        assert_eq!(apply(text, &found[1].fixes[0]), text.replace("const kept = g(a);", "g(a);"));
        assert_eq!(apply(text, &found[2].fixes[0]), text.replace("  console.log(a);\n", ""));
    }

    #[test]
    #[ignore = "reads the tree ORIOR_TREE names"]
    fn the_tree_is_read_whole() {
        let root = std::path::PathBuf::from(std::env::var("ORIOR_TREE").expect("ORIOR_TREE"));
        let files = crate::files::all(&root);
        let started = std::time::Instant::now();
        let mut tree = Tree::default();
        for file in &files {
            if let (Some(language), Ok(text)) = (language_of(file), std::fs::read_to_string(root.join(file))) {
                tree.set(file, language, &text);
            }
        }
        let read = started.elapsed();
        let found = tree.findings();
        let all = started.elapsed();
        let mut by_code: HashMap<&str, usize> = HashMap::new();
        let mut shown: HashMap<&str, usize> = HashMap::new();
        let mut paths: Vec<&String> = found.keys().collect();
        paths.sort();
        for path in paths {
            for one in &found[path] {
                *by_code.entry(one.code).or_default() += 1;
                let count = shown.entry(one.code).or_default();
                if *count < 12 {
                    *count += 1;
                    println!("{path}:{}:{} [{}] {}", one.from.line + 1, one.from.col + 1, one.code, one.message);
                }
            }
        }
        println!("{} files read in {read:?}, found in {all:?}: {by_code:?}", tree.files.len());
    }

    fn tree(files: &[(&str, &str)]) -> Tree {
        let mut tree = Tree::default();
        for (path, text) in files {
            tree.set(path, language_of(path).unwrap(), text);
        }
        tree
    }

    #[test]
    fn an_import_that_leads_back_round_is_found_in_python_and_javascript() {
        let found = tree(&[
            ("pkg/__init__.py", ""),
            ("pkg/a.py", "from pkg import b\n\ndef f():\n    from pkg import c\n"),
            ("pkg/b.py", "from . import a\n"),
            ("pkg/c.py", "import pkg.a\n"),
            ("pkg/d.py", "import os\nfrom typing import TYPE_CHECKING\nif TYPE_CHECKING:\n    from pkg import a\n"),
            ("web/x.js", "import { y } from \"./y.js\";\n"),
            ("web/y.js", "export { x } from \"./x\";\n"),
            ("web/z.js", "import { x } from \"./x.js\";\n"),
        ])
        .findings();
        let said = |path: &str| found.get(path).map(|all| all.iter().map(|one| one.message.clone()).collect::<Vec<_>>()).unwrap_or_default();
        assert_eq!(said("pkg/a.py"), vec!["This import leads back round to the file that makes it: pkg/a.py → pkg/b.py → pkg/a.py."]);
        assert_eq!(said("pkg/b.py").len(), 1);
        assert!(said("pkg/c.py").is_empty() && said("pkg/d.py").is_empty());
        assert_eq!(said("web/x.js"), vec!["This import leads back round to the file that makes it: web/x.js → web/y.js → web/x.js."]);
        assert!(said("web/z.js").is_empty());
    }

    #[test]
    fn a_method_a_mixin_supplies_is_found_and_one_no_class_sets_is_not() {
        let read = tree(&[
            ("shapes/mixins.py", "class Saving:\n    def save(self):\n        return self.render() + self.missing()\n\nclass Drawing:\n    def draw(self):\n        return self.render()\n"),
            ("shapes/square.py", "from shapes.mixins import Saving\nimport shapes.mixins as mixins\n\nclass Square(Saving, mixins.Drawing):\n    def __init__(self):\n        self.side = self.edge = 2\n\n    def render(self):\n        hook = self.hook() if getattr(self, \"hook\", None) else 0\n        return self.side * self.edge * self.save() + self.draw() + self.colour + hook\n"),
            ("shapes/outside.py", "from somewhere import Base\n\nclass Loose(Base):\n    def go(self):\n        return self.anything()\n"),
        ]);
        let found = read.findings();
        let supplied = read.supplied();
        assert!(supplied["shapes/mixins.py"]["Saving"].contains("render"), "the class a mixin is mixed into supplies its method");
        assert!(!supplied["shapes/mixins.py"]["Saving"].contains("missing"));
        let said = |path: &str| found.get(path).map(|all| all.iter().map(|one| one.message.clone()).collect::<Vec<_>>()).unwrap_or_default();
        assert_eq!(said("shapes/mixins.py"), vec!["Saving has no missing: neither it, a class it comes from, nor a class that comes from it sets missing."]);
        assert_eq!(said("shapes/square.py"), vec!["Square has no colour: neither it, a class it comes from, nor a class that comes from it sets colour."]);
        assert!(said("shapes/outside.py").is_empty());
    }
}
