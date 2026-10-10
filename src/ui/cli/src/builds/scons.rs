// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! SCons's SConstruct and SConscript files, which are Python that SCons gives its own names: each
//! SConscript a file reads is found, each name it imports is exported by a file of the tree, and
//! SCons's names, an environment's methods and its construction variables are completed. The names
//! SCons gives a file are passed over where Python's checker reports them undefined.

use std::collections::HashSet;
use std::path::Path;

use super::{items, run_before, word_char, Finding, Lines, ERROR, WARNING};
use crate::servers::Item;

/// The names SCons gives an SConstruct and an SConscript.
const GLOBALS: [&str; 76] = [
    "Environment", "DefaultEnvironment", "Program", "Library", "StaticLibrary", "SharedLibrary", "LoadableModule", "Object", "StaticObject",
    "SharedObject", "Command", "Default", "Alias", "AlwaysBuild", "Depends", "Ignore", "Requires", "Install", "InstallAs", "Glob", "Split",
    "SConscript", "Import", "Export", "Return", "Help", "AddOption", "GetOption", "SetOption", "ARGUMENTS", "ARGLIST", "BUILD_TARGETS",
    "COMMAND_LINE_TARGETS", "DEFAULT_TARGETS", "Dir", "File", "Entry", "Clean", "NoClean", "Precious", "SideEffect", "Variables",
    "BoolVariable", "EnumVariable", "ListVariable", "PathVariable", "PackageVariable", "Builder", "Action", "Scanner", "Configure", "Exit",
    "EnsurePythonVersion", "EnsureSConsVersion", "Decider", "CacheDir", "VariantDir", "Repository", "SConsignFile", "Tool", "WhereIs",
    "Platform", "Execute", "Flatten", "Mkdir", "Copy", "Delete", "Move", "Touch", "Chmod", "Literal", "Value", "GetLaunchDir", "Progress",
    "FindFile", "Local",
];

/// An environment's methods.
const METHODS: [&str; 52] = [
    "Append", "AppendUnique", "AppendENVPath", "Prepend", "PrependUnique", "PrependENVPath", "Replace", "Clone", "Dictionary", "Detect",
    "Dump", "Glob", "MergeFlags", "ParseConfig", "ParseFlags", "SetDefault", "subst", "Tool", "WhereIs", "Alias", "AlwaysBuild", "Command",
    "Default", "Depends", "Ignore", "Install", "InstallAs", "Program", "Library", "StaticLibrary", "SharedLibrary", "LoadableModule",
    "Object", "StaticObject", "SharedObject", "SConscript", "Split", "Dir", "File", "Entry", "Clean", "Precious", "SideEffect", "VariantDir",
    "CacheDir", "Decider", "Requires", "NoClean", "Execute", "Configure", "Help", "get",
];

/// The construction variables an environment holds most often.
const VARIABLES: [&str; 44] = [
    "CC", "CXX", "CFLAGS", "CCFLAGS", "CXXFLAGS", "CPPFLAGS", "CPPPATH", "CPPDEFINES", "LIBS", "LIBPATH", "LINKFLAGS", "LINK", "AR", "AS",
    "ASFLAGS", "RANLIB", "SHCC", "SHCXX", "SHCCFLAGS", "SHCXXFLAGS", "SHLINKFLAGS", "SHLIBPREFIX", "SHLIBSUFFIX", "LIBPREFIX", "LIBSUFFIX",
    "PROGPREFIX", "PROGSUFFIX", "OBJPREFIX", "OBJSUFFIX", "ENV", "TOOLS", "BUILDERS", "PLATFORM", "FRAMEWORKS", "RPATH", "TARGET_ARCH",
    "HOST_ARCH", "MSVC_VERSION", "CCCOMSTR", "CXXCOMSTR", "LINKCOMSTR", "ARCOMSTR", "tools", "toolpath",
];

/// The calls whose keyword arguments are construction variables.
const SETTERS: [&str; 16] = ["Environment", "Append", "AppendUnique", "Prepend", "PrependUnique", "Replace", "Clone", "SetDefault", "Program", "Library", "StaticLibrary", "SharedLibrary", "Object", "StaticObject", "SharedObject", "LoadableModule"];

const CODE: u8 = 0;
const STRING: u8 = 1;
const COMMENT: u8 = 2;

/// What each byte of a file is: code, a string's, or a comment's.
fn classes(text: &str) -> Vec<u8> {
    let bytes = text.as_bytes();
    let mut class = vec![CODE; bytes.len()];
    let mut at = 0;
    while at < bytes.len() {
        let rest = &text[at..];
        let (kind, end) = if rest.starts_with('#') {
            (COMMENT, rest.find('\n').map_or(bytes.len(), |end| at + end))
        } else if rest.starts_with("\"\"\"") || rest.starts_with("'''") {
            let quote = &rest[..3];
            (STRING, rest[3..].find(quote).map_or(bytes.len(), |end| at + 3 + end + 3))
        } else if rest.starts_with('"') || rest.starts_with('\'') {
            let quote = bytes[at];
            let mut end = at + 1;
            while end < bytes.len() && bytes[end] != quote && bytes[end] != b'\n' {
                end += if bytes[end] == b'\\' { 2 } else { 1 };
            }
            (STRING, (end + 1).min(bytes.len()))
        } else {
            at += 1;
            continue;
        };
        class[at..end].fill(kind);
        at = end;
    }
    class
}

/// A call of `name` in code: where its arguments start, after its `(`, and where they end, at its
/// `)`.
fn calls(text: &str, class: &[u8], name: &str) -> Vec<(usize, usize)> {
    let mut found = Vec::new();
    for (at, _) in text.match_indices(name) {
        if class[at] != CODE || text[..at].ends_with(|char: char| word_char(char)) {
            continue;
        }
        let after = at + name.len();
        let open = after + text[after..].len() - text[after..].trim_start_matches([' ', '\t']).len();
        if !text[open..].starts_with('(') {
            continue;
        }
        let mut depth = 0;
        let mut end = text.len();
        for (offset, byte) in text.as_bytes()[open..].iter().enumerate() {
            if class[open + offset] != CODE {
                continue;
            }
            match byte {
                b'(' | b'[' | b'{' => depth += 1,
                b')' | b']' | b'}' => {
                    depth -= 1;
                    if depth == 0 {
                        end = open + offset;
                        break;
                    }
                }
                _ => {}
            }
        }
        found.push((open + 1, end));
    }
    found
}

/// The strings of `from` to `to`, each one's text and the span inside its quotes.
fn strings(text: &str, class: &[u8], from: usize, to: usize) -> Vec<(String, usize, usize)> {
    let mut found = Vec::new();
    let mut at = from;
    let to = to.min(class.len());
    while at < to {
        if class[at] == STRING && (at == 0 || class[at - 1] != STRING) {
            let end = (at..class.len()).find(|&end| class[end] != STRING).unwrap_or(class.len());
            let quote = if text[at..].starts_with("\"\"\"") || text[at..].starts_with("'''") { 3 } else { 1 };
            let inner_end = if end - at >= 2 * quote && text.as_bytes()[end - 1] == text.as_bytes()[at] { end - quote } else { end };
            found.push((text[at + quote..inner_end].to_string(), at + quote, inner_end));
            at = end;
            continue;
        }
        at += 1;
    }
    found
}

/// The arguments of a call split at its commas that stand outside brackets: each one's span.
fn arguments(text: &str, class: &[u8], from: usize, to: usize) -> Vec<(usize, usize)> {
    let mut found = Vec::new();
    let mut depth = 0;
    let mut start = from;
    for (at, &kind) in class.iter().enumerate().take(to).skip(from) {
        if kind != CODE {
            continue;
        }
        match text.as_bytes()[at] {
            b'(' | b'[' | b'{' => depth += 1,
            b')' | b']' | b'}' => depth -= 1,
            b',' if depth == 0 => {
                found.push((start, at));
                start = at + 1;
            }
            _ => {}
        }
    }
    if !text[start..to].trim().is_empty() {
        found.push((start, to));
    }
    found
}

/// The keyword an argument gives, where it gives one: its name and where its value starts.
fn keyword(text: &str, from: usize, to: usize) -> Option<(&str, usize)> {
    let part = &text[from..to];
    let eq = part.find('=')?;
    let name = part[..eq].trim();
    (!name.is_empty() && name.chars().all(word_char) && !part[eq + 1..].starts_with('=')).then_some((name, from + eq + 1))
}

/// The names the `Import` calls of a file take.
fn imported(text: &str, class: &[u8]) -> Vec<(String, usize, usize)> {
    let mut found = Vec::new();
    for (from, to) in calls(text, class, "Import") {
        for (value, start, end) in strings(text, class, from, to) {
            let mut offset = 0;
            for word in value.split_whitespace() {
                let at = value[offset..].find(word).map_or(0, |at| offset + at);
                offset = at + word.len();
                found.push((word.to_string(), start + at, (start + at + word.len()).min(end)));
            }
        }
    }
    found
}

/// The names a file exports: by `Export`, its strings and its keywords, and by an `exports` given an
/// SConscript call, its strings and its dictionary's keys.
fn exported(text: &str) -> HashSet<String> {
    let class = classes(text);
    let mut found = HashSet::new();
    for (from, to) in calls(text, &class, "Export") {
        for (start, end) in arguments(text, &class, from, to) {
            match keyword(text, start, end) {
                Some((name, _)) => {
                    found.insert(name.to_string());
                }
                None => {
                    for (value, _, _) in strings(text, &class, start, end) {
                        found.extend(value.split_whitespace().map(str::to_string));
                    }
                    let bare = text[start..end].trim();
                    if !bare.is_empty() && bare.chars().all(word_char) {
                        found.insert(bare.to_string());
                    }
                }
            }
        }
    }
    for (from, to) in calls(text, &class, "SConscript") {
        for (start, end) in arguments(text, &class, from, to) {
            if let Some(("exports", value)) = keyword(text, start, end) {
                for (one, _, _) in strings(text, &class, value, end) {
                    found.extend(one.split_whitespace().map(str::to_string));
                }
            }
        }
    }
    found
}

/// The SConscripts a file's `SConscript` calls read: each path as written and its span, as the first
/// argument gives it or as `dirs` and `name` do.
fn read_scripts(text: &str, class: &[u8]) -> Vec<(String, usize, usize)> {
    let mut found = Vec::new();
    for (from, to) in calls(text, class, "SConscript") {
        let args = arguments(text, class, from, to);
        let mut name = "SConscript".to_string();
        let mut dirs = Vec::new();
        for (index, &(start, end)) in args.iter().enumerate() {
            match keyword(text, start, end) {
                Some(("dirs", value)) => dirs = strings(text, class, value, end),
                Some(("name", value)) => {
                    if let Some((given, _, _)) = strings(text, class, value, end).into_iter().next() {
                        name = given;
                    }
                }
                Some(_) => {}
                None if index == 0 => {
                    let first = text[start..end].trim_start();
                    if first.starts_with(['"', '\'', '[']) {
                        found.extend(strings(text, class, start, end));
                    }
                }
                None => {}
            }
        }
        for (dir, start, end) in dirs {
            found.push((format!("{}/{name}", dir.trim_end_matches('/')), start, end));
        }
    }
    found
}

/// The names SCons gives a file: its own, and those the file imports.
pub(crate) fn given(text: &str) -> HashSet<String> {
    let class = classes(text);
    let mut found: HashSet<String> = GLOBALS.iter().map(|name| name.to_string()).collect();
    found.extend(imported(text, &class).into_iter().map(|(name, _, _)| name));
    found
}

/// The findings of an SConstruct or an SConscript.
pub(crate) fn check(root: &Path, path: &Path, text: &str) -> Vec<Finding> {
    let lines = Lines::new(text);
    let class = classes(text);
    let mut found = Vec::new();
    let folder = path.parent().unwrap_or(root);
    let top = top_folder(root, path);
    for (written, from, to) in read_scripts(text, &class) {
        if written.contains('$') {
            continue;
        }
        let place = match written.strip_prefix('#') {
            Some(rest) => top.join(rest.trim_start_matches('/')),
            None => folder.join(&written),
        };
        if !place.exists() {
            found.push(Finding::new(&lines, from, to, ERROR, "scons-script", format!("No file is at `{written}`, the SConscript this call reads")));
        }
    }
    let names = imported(text, &class);
    if !names.is_empty() {
        let mut exports = exported(text);
        for file in crate::files::all(root).iter().filter(|file| super::kind_of(Path::new(file)) == Some(super::Kind::Scons)).take(400) {
            let other = root.join(file);
            if other != path {
                if let Ok(other_text) = std::fs::read_to_string(&other) {
                    exports.extend(exported(&other_text));
                }
            }
        }
        for (name, from, to) in names.into_iter().filter(|(name, _, _)| name != "*") {
            if !exports.contains(&name) {
                found.push(Finding::new(&lines, from, to, WARNING, "scons-import", format!("`{name}` is imported here, and no SConstruct or SConscript of the tree exports it")));
            }
        }
    }
    found
}

/// The folder of the SConstruct a file belongs to, which `#` names in a path: the nearest above it
/// in the tree that holds one, the tree's top where none does.
fn top_folder(root: &Path, path: &Path) -> std::path::PathBuf {
    let mut folder = path.parent().unwrap_or(root).to_path_buf();
    loop {
        if ["SConstruct", "Sconstruct", "sconstruct"].iter().any(|name| folder.join(name).is_file()) {
            return folder;
        }
        if folder == root || !folder.starts_with(root) {
            return root.to_path_buf();
        }
        match folder.parent() {
            Some(parent) => folder = parent.to_path_buf(),
            None => return root.to_path_buf(),
        }
    }
}

/// What completes the word before byte `at`: an environment's methods after its name and a dot, its
/// construction variables in its brackets and as the keywords of the calls that set them, and
/// SCons's names.
pub(crate) fn complete(text: &str, at: usize) -> Vec<Item> {
    let class = classes(&text[..at]);
    let token = run_before(text, at, |char| word_char(char) || char == '.');
    let line_start = text[..at].rfind('\n').map_or(0, |at| at + 1);
    let lead = &text[line_start..at - token.len()];
    if class.last() == Some(&STRING) {
        let opened = lead.trim_end_matches(['"', '\'']);
        return if opened.trim_end().ends_with('[') { items(&VARIABLES, "property", "construction variable") } else { Vec::new() };
    }
    if class.last() == Some(&COMMENT) {
        return Vec::new();
    }
    if token.contains('.') {
        return items(&METHODS, "function", "environment");
    }
    let mut depth = 0;
    let mut call = None;
    for (index, byte) in text.as_bytes()[..at - token.len()].iter().enumerate().rev() {
        if class[index] != CODE {
            continue;
        }
        match byte {
            b')' | b']' | b'}' => depth += 1,
            b'(' | b'[' | b'{' if depth > 0 => depth -= 1,
            b'(' => {
                call = Some(index);
                break;
            }
            b'[' | b'{' => break,
            _ => {}
        }
    }
    let mut found = Vec::new();
    if let Some(open) = call {
        let name_end = text[..open].trim_end().len();
        let name: String = text[..name_end].chars().rev().take_while(|char| word_char(*char)).collect::<Vec<_>>().into_iter().rev().collect();
        let since = text[open + 1..at - token.len()].rsplit(',').next().unwrap_or_default();
        if SETTERS.contains(&name.as_str()) && since.trim().is_empty() {
            found.extend(items(&VARIABLES, "property", "construction variable"));
        }
    }
    found.extend(GLOBALS.iter().map(|name| Item { label: name.to_string(), kind: if name.chars().all(|char| char.is_ascii_uppercase() || char == '_') { "constant" } else { "function" }, detail: "SCons".to_string(), insert: name.to_string(), snippet: false }));
    found
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn scripts_read_and_names_imported_are_checked() {
        let dir = std::env::temp_dir().join(format!("orior-scons-{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&dir);
        std::fs::create_dir_all(dir.join("src")).unwrap();
        let top = "env = Environment(CPPPATH=['include'])\nExport('env')\nSConscript('src/SConscript')\nSConscript(['lib/SConscript'], exports='libs')\nSConscript(dirs=['gone'])\n# SConscript('commented')\n";
        std::fs::write(dir.join("SConstruct"), top).unwrap();
        let sub = "Import('env libs tools')\nenv.Program('hello', ['hello.c'])\n";
        std::fs::write(dir.join("src").join("SConscript"), sub).unwrap();
        let found = check(&dir, &dir.join("SConstruct"), top);
        let said: Vec<String> = found.iter().map(|one| one.message.clone()).collect();
        assert_eq!(said.len(), 2, "{said:?}");
        assert!(said[0].contains("`lib/SConscript`") && said[1].contains("`gone/SConscript`"), "{said:?}");
        let found = check(&dir, &dir.join("src").join("SConscript"), sub);
        let said: Vec<String> = found.iter().map(|one| one.message.clone()).collect();
        assert_eq!(said, ["`tools` is imported here, and no SConstruct or SConscript of the tree exports it"]);
        let tools = &found[0];
        assert_eq!((tools.from.line, tools.from.col, tools.to.col), (0, 17, 22));
        assert!(given(sub).contains("libs") && given(sub).contains("Program"));
        let _ = std::fs::remove_dir_all(dir);
    }

    #[test]
    fn scons_names_complete() {
        let labels = |text: &str| complete(text, text.len()).into_iter().map(|item| item.label).collect::<Vec<_>>();
        assert!(labels("env = Envi").contains(&"Environment".to_string()));
        assert!(labels("env.App").contains(&"AppendUnique".to_string()));
        assert!(labels("env['CPP").contains(&"CPPPATH".to_string()));
        assert!(labels("env = Environment(CPP").contains(&"CPPPATH".to_string()));
        assert!(!labels("x = Environment(tools=foo(CPP").contains(&"CPPPATH".to_string()));
        assert!(labels("x = 'Envi").is_empty());
    }
}
