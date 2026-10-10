// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Gradle's scripts, in Groovy or Kotlin: their blocks read by their braces, each `dependencies`
//! block's lines by the configuration each names and the dependency it gives, and each name a
//! version catalog's accessor leads, `libs.groovy.core` and the rest, against the catalogs of the
//! build's `gradle` folder.

use std::path::{Path, PathBuf};

use super::catalog::{self, accessor};
use super::{edit_fix, items, nearest, relative, run_before, run_fix, snapshot, word_char, Finding, Lines, ERROR, NOTE, WARNING};
use crate::servers::Item;

/// The configurations Gradle's own plugins and the common ones of others declare.
const CONFIGURATIONS: [&str; 30] = [
    "implementation", "api", "compileOnly", "compileOnlyApi", "runtimeOnly", "testImplementation", "testCompileOnly", "testRuntimeOnly",
    "annotationProcessor", "testAnnotationProcessor", "testFixturesImplementation", "testFixturesApi", "developmentOnly", "kapt", "ksp",
    "classpath", "coreLibraryDesugaring", "lintChecks", "detektPlugins", "androidTestImplementation", "debugImplementation",
    "releaseImplementation", "providedCompile", "providedRuntime", "compileClasspath", "runtimeClasspath", "testCompileClasspath",
    "testRuntimeClasspath", "archives", "default",
];

/// What a `dependencies` block calls beside its configurations.
const HELPERS: [&str; 12] = ["platform", "enforcedPlatform", "project", "files", "fileTree", "gradleApi()", "localGroovy()", "gradleTestKit()", "testFixtures", "constraints", "components", "modules"];

/// The ends of the configurations a source set or a build variant declares, `integrationTestImplementation`
/// and the rest.
const SUFFIXES: [&str; 8] = ["Implementation", "Api", "CompileOnly", "RuntimeOnly", "AnnotationProcessor", "Kapt", "Ksp", "CompileOnlyApi"];

/// The configurations Gradle 7 has no more, and the one that stands in for each.
const REMOVED: [(&str, &str); 5] = [("compile", "implementation"), ("testCompile", "testImplementation"), ("runtime", "runtimeOnly"), ("testRuntime", "testRuntimeOnly"), ("provided", "compileOnly")];

/// What a build script names at its top.
const TOP: [&str; 31] = [
    "plugins", "repositories", "dependencies", "java", "application", "tasks", "test", "jar", "configurations", "sourceSets", "publishing",
    "buildscript", "allprojects", "subprojects", "group", "version", "description", "kotlin", "wrapper", "apply", "ext", "base", "javadoc",
    "compileJava", "compileTestJava", "processResources", "defaultTasks", "dependencyLocking", "artifacts", "idea", "eclipse",
];

/// What a settings script names at its top.
const SETTINGS_TOP: [&str; 9] = ["rootProject", "include", "includeBuild", "pluginManagement", "dependencyResolutionManagement", "enableFeaturePreview", "buildCache", "plugins", "gradle"];

/// The words a block of each name takes.
fn block_words(block: &str) -> &'static [&'static str] {
    match block {
        "repositories" => &["mavenCentral()", "google()", "gradlePluginPortal()", "mavenLocal()", "maven", "ivy", "flatDir", "exclusiveContent"],
        "plugins" => &["id", "alias", "kotlin", "java", "application", "version", "apply"],
        "java" => &["toolchain", "sourceCompatibility", "targetCompatibility", "withSourcesJar()", "withJavadocJar()", "modularity", "registerFeature", "disableAutoTargetJvm()"],
        "toolchain" => &["languageVersion", "vendor", "implementation"],
        "application" => &["mainClass", "applicationName", "applicationDefaultJvmArgs", "mainModule", "executableDir"],
        "test" => &["useJUnitPlatform()", "useJUnit()", "useTestNG()", "maxParallelForks", "testLogging", "systemProperty", "jvmArgs", "filter", "forkEvery", "maxHeapSize", "include", "exclude", "failFast"],
        "tasks" => &["register", "named", "withType", "getByName", "create", "matching", "configureEach"],
        "buildscript" => &["repositories", "dependencies", "ext"],
        "pluginManagement" => &["repositories", "plugins", "includeBuild", "resolutionStrategy"],
        "dependencyResolutionManagement" => &["repositories", "versionCatalogs", "repositoriesMode", "rulesMode", "components"],
        "versionCatalogs" => &["create"],
        "publishing" => &["publications", "repositories"],
        "jar" => &["manifest", "archiveBaseName", "archiveVersion", "archiveClassifier", "from", "exclude", "duplicatesStrategy"],
        "configurations" => &["all", "create", "register", "named", "getByName", "configureEach", "matching"],
        "sourceSets" => &["main", "test", "create", "named", "register"],
        "allprojects" | "subprojects" => &TOP,
        _ => &[],
    }
}

/// What a byte of a script is.
const CODE: u8 = 0;
const STRING: u8 = 1;
const COMMENT: u8 = 2;

/// What each byte of a script is: code, a string's, or a comment's. A string's `${}` is the
/// string's.
fn classes(text: &str) -> Vec<u8> {
    let bytes = text.as_bytes();
    let mut class = vec![CODE; bytes.len()];
    let mut at = 0;
    while at < bytes.len() {
        let rest = &text[at..];
        let (kind, end) = if rest.starts_with("//") {
            (COMMENT, rest.find('\n').map_or(bytes.len(), |end| at + end))
        } else if let Some(inside) = rest.strip_prefix("/*") {
            (COMMENT, inside.find("*/").map_or(bytes.len(), |end| at + 2 + end + 2))
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

/// A block of a script: the name before its brace, and where it opens and closes.
struct Block {
    name: String,
    open: usize,
    close: Option<usize>,
}

/// The blocks of a script, in the order they open, and its braces that close none.
fn blocks(text: &str, class: &[u8]) -> (Vec<Block>, Vec<usize>) {
    let bytes = text.as_bytes();
    let mut found: Vec<Block> = Vec::new();
    let mut open: Vec<usize> = Vec::new();
    let mut stray = Vec::new();
    for (at, byte) in bytes.iter().enumerate() {
        if class[at] != CODE {
            continue;
        }
        match byte {
            b'{' => {
                open.push(found.len());
                found.push(Block { name: name_before(text, class, at), open: at, close: None });
            }
            b'}' => match open.pop() {
                Some(index) => found[index].close = Some(at),
                None => stray.push(at),
            },
            _ => {}
        }
    }
    (found, stray)
}

/// The name a brace at `at` opens the block of: the last word of the call before it, past its
/// arguments and its type arguments.
fn name_before(text: &str, class: &[u8], at: usize) -> String {
    let bytes = text.as_bytes();
    let mut end = at;
    let back = |end: &mut usize| {
        while *end > 0 && (bytes[*end - 1] as char).is_whitespace() {
            *end -= 1;
        }
    };
    back(&mut end);
    for (close, open) in [(b')', b'('), (b'>', b'<')] {
        if end > 0 && bytes[end - 1] == close && class[end - 1] == CODE {
            let mut depth = 0;
            while end > 0 {
                end -= 1;
                if class[end] != CODE {
                    continue;
                }
                if bytes[end] == close {
                    depth += 1;
                } else if bytes[end] == open {
                    depth -= 1;
                    if depth == 0 {
                        break;
                    }
                }
            }
            back(&mut end);
        }
    }
    let start = text[..end].char_indices().rev().take_while(|(_, char)| word_char(*char)).last().map_or(end, |(at, _)| at);
    text[start..end].to_string()
}

/// The names of the blocks that hold byte `at`, the outermost first.
fn blocks_at(blocks: &[Block], at: usize) -> Vec<&Block> {
    blocks.iter().filter(|block| block.open < at && block.close.is_none_or(|close| close >= at)).collect()
}

/// The folder of the Gradle build the file at `path` belongs to: the nearest above it, in the tree,
/// that holds a settings script or a wrapper, a catalog's build being the one above its `gradle`
/// folder; the file's own folder where none does.
pub(crate) fn build_root(root: &Path, path: &Path) -> PathBuf {
    let mut start = path.parent().unwrap_or(root).to_path_buf();
    if super::kind_of(path) == Some(super::Kind::Catalog) {
        start = start.parent().unwrap_or(root).to_path_buf();
    }
    let mut folder = start.clone();
    loop {
        if ["settings.gradle", "settings.gradle.kts", "gradlew"].iter().any(|name| folder.join(name).is_file()) {
            return folder;
        }
        if folder == root || !folder.starts_with(root) {
            return start;
        }
        match folder.parent() {
            Some(parent) => folder = parent.to_path_buf(),
            None => return start,
        }
    }
}

/// The program that runs the Gradle build at `folder`: its wrapper, where it has one.
pub(crate) fn program(folder: &Path) -> String {
    if folder.join("gradlew").is_file() {
        "./gradlew".to_string()
    } else {
        "gradle".to_string()
    }
}

/// The catalogs of the build at `folder`, each by its accessor, the file's name before
/// `.versions.toml`, with its path and its text.
fn catalogs(folder: &Path) -> Vec<(String, PathBuf, String)> {
    let mut found = Vec::new();
    if let Ok(entries) = std::fs::read_dir(folder.join("gradle")) {
        for entry in entries.flatten() {
            let name = entry.file_name().to_string_lossy().to_string();
            if let Some(stem) = name.strip_suffix(".versions.toml") {
                if let Ok(text) = std::fs::read_to_string(entry.path()) {
                    found.push((stem.to_string(), entry.path(), text));
                }
            }
        }
    }
    found.sort();
    found
}

/// The configurations a script declares: by name in a `configurations` block, by `create`,
/// `register` or `maybeCreate`, and by Kotlin's `by configurations.creating`.
fn declared(text: &str, class: &[u8], blocks: &[Block]) -> Vec<String> {
    let mut found = Vec::new();
    for (at, _) in text.match_indices(['c', 'r', 'm']) {
        for call in ["create(", "register(", "maybeCreate("] {
            if text[at..].starts_with(call) && class[at] == CODE && !text[..at].ends_with(|char: char| word_char(char)) {
                let rest = &text[at + call.len()..];
                if let Some(quoted) = rest.strip_prefix('"').or_else(|| rest.strip_prefix('\'')) {
                    found.extend(quoted.split(['"', '\'']).next().map(str::to_string));
                }
            }
        }
    }
    for line in text.lines() {
        let line = line.trim();
        if let Some(rest) = line.strip_prefix("val ") {
            if rest.contains("by configurations.creating") || rest.contains("by configurations.registering") || rest.contains("by creating") {
                found.extend(rest.split_whitespace().next().map(str::to_string));
            }
        }
    }
    for block in blocks.iter().filter(|block| block.name == "configurations") {
        let end = block.close.unwrap_or(text.len());
        for (word, _, _) in statements(text, class, blocks, block, end) {
            if !block_words("configurations").contains(&word) {
                found.push(word.to_string());
            }
        }
    }
    found
}

/// The statements a block holds itself, not those of the blocks inside it: each one's first word,
/// where the word starts, and what follows it on its line.
fn statements<'a>(text: &'a str, class: &[u8], blocks: &[Block], block: &Block, end: usize) -> Vec<(&'a str, usize, &'a str)> {
    let mut found = Vec::new();
    let lines = Lines::new(text);
    let first = lines.place(block.open + 1).line as usize;
    let last = lines.place(end).line as usize;
    for line in first..=last {
        let start = lines.start(line).max(block.open + 1);
        let line_end = text[start..].find('\n').map_or(text.len(), |at| start + at).min(end);
        let part = &text[start..line_end];
        let lead = part.len() - part.trim_start().len();
        let at = start + lead;
        if at >= line_end || class[at] != CODE {
            continue;
        }
        if blocks_at(blocks, at).last().map(|inner| inner.open) != Some(block.open) {
            continue;
        }
        let word_len = text[at..line_end].chars().take_while(|char| word_char(*char)).map(char::len_utf8).sum::<usize>();
        if word_len == 0 {
            continue;
        }
        found.push((&text[at..at + word_len], at, &text[at + word_len..line_end]));
    }
    found
}

/// Whether `word` is a configuration Gradle or the script declares.
fn known_configuration(word: &str, declared: &[String]) -> bool {
    CONFIGURATIONS.contains(&word) || HELPERS.iter().any(|helper| helper.trim_end_matches("()") == word) || declared.iter().any(|one| one == word) || SUFFIXES.iter().any(|end| word.len() > end.len() && word.ends_with(end))
}

/// The findings of a script.
pub(crate) fn check(root: &Path, path: &Path, text: &str) -> Vec<Finding> {
    let lines = Lines::new(text);
    let class = classes(text);
    let (blocks, stray) = blocks(text, &class);
    let mut found = Vec::new();
    for at in stray {
        found.push(Finding::new(&lines, at, at + 1, ERROR, "gradle-brace", "This `}` closes no block".to_string()));
    }
    for block in blocks.iter().filter(|block| block.close.is_none()) {
        found.push(Finding::new(&lines, block.open, block.open + 1, ERROR, "gradle-brace", format!("The block{} this `{{` opens is not closed", if block.name.is_empty() { String::new() } else { format!(" `{}`", block.name) })));
    }
    let folder = build_root(root, path);
    let declared = declared(text, &class, &blocks);
    let update = |coordinates: &str| run_fix(format!("Update the snapshot {coordinates}"), super::inside(root, &folder), format!("{} --refresh-dependencies dependencies", program(&folder)));
    for block in blocks.iter().filter(|block| block.name == "dependencies") {
        let end = block.close.unwrap_or(text.len());
        for (word, at, rest) in statements(text, &class, &blocks, block, end) {
            let next = rest.trim_start().chars().next();
            if matches!(next, Some('=' | '.') | None) && !REMOVED.iter().any(|(old, _)| *old == word) {
                continue;
            }
            if let Some((_, now)) = REMOVED.iter().find(|(old, _)| *old == word) {
                let mut finding = Finding::new(&lines, at, at + word.len(), WARNING, "gradle-configuration", format!("Gradle 7 and later have no configuration `{word}`: `{now}` stands in its place"));
                finding.fixes.push(edit_fix(format!("Change to {now}"), &lines, at, at + word.len(), now));
                found.push(finding);
                continue;
            }
            if next == Some('{') {
                continue;
            }
            if !known_configuration(word, &declared) {
                let mut finding = Finding::new(&lines, at, at + word.len(), WARNING, "gradle-configuration", format!("No configuration `{word}` is declared here, by Gradle or by the script"));
                for near in nearest(word, CONFIGURATIONS.iter().copied().chain(declared.iter().map(String::as_str))) {
                    finding.fixes.push(edit_fix(format!("Change to {near}"), &lines, at, at + word.len(), &near));
                }
                found.push(finding);
                continue;
            }
            notation(&lines, text, &class, at + word.len(), &update, &mut found);
        }
    }
    for (name, catalog_path, catalog_text) in catalogs(&folder) {
        let shown = super::inside(root, &catalog_path);
        references(&lines, text, &class, &name, &shown, &catalog::aliases(&catalog_text), &mut found);
    }
    found
}

/// Checks the dependency a configuration at `after` gives as a string, where it gives one:
/// `group:name:version`, and a note where its version is a snapshot.
fn notation(lines: &Lines, text: &str, class: &[u8], after: usize, update: &dyn Fn(&str) -> serde_json::Value, found: &mut Vec<Finding>) {
    let line_end = text[after..].find('\n').map_or(text.len(), |at| after + at);
    let mut at = after;
    let skip = |at: &mut usize| {
        while *at < line_end && (text.as_bytes()[*at] == b' ' || text.as_bytes()[*at] == b'\t' || text.as_bytes()[*at] == b'(') {
            *at += 1;
        }
    };
    skip(&mut at);
    for wrapper in ["platform", "enforcedPlatform", "testFixtures"] {
        if text[at..line_end].starts_with(wrapper) {
            at += wrapper.len();
            skip(&mut at);
        }
    }
    if at >= line_end || class[at] != STRING || !matches!(text.as_bytes()[at], b'"' | b'\'') {
        return;
    }
    let end = (at + 1..line_end).find(|end| class[*end] != STRING).unwrap_or(line_end);
    let inner = (at + 1, end.saturating_sub(1).max(at + 1));
    let given = &text[inner.0..inner.1];
    if given.contains('$') {
        return;
    }
    let bare = given.split('@').next().unwrap_or(given);
    let parts: Vec<&str> = bare.split(':').collect();
    if parts.len() < 2 || parts.len() > 4 || parts.iter().any(|part| part.trim().is_empty()) {
        found.push(Finding::new(lines, inner.0, inner.1, WARNING, "gradle-notation", format!("`{given}` is no dependency: a dependency is written `group:name:version`")));
    } else if parts.len() >= 3 && snapshot(parts[2]) {
        let mut finding = Finding::new(lines, inner.0, inner.1, NOTE, "snapshot", format!("`{given}` is a snapshot, whose newest build Gradle fetches when it refreshes its dependencies"));
        finding.fixes.push(update(given));
        found.push(finding);
    }
}

/// The words a catalog's accessor leads in code, each one's span: of `libs.groovy.core.get()`,
/// `groovy`, `core`.
fn chains(text: &str, class: &[u8], name: &str) -> Vec<Vec<(usize, usize)>> {
    let mut found = Vec::new();
    let lead = format!("{name}.");
    for (at, _) in text.match_indices(&lead) {
        if class[at] != CODE || text[..at].ends_with(|char: char| word_char(char) || char == '.') {
            continue;
        }
        let mut segments = Vec::new();
        let mut start = at + lead.len();
        loop {
            let len: usize = text[start..].chars().take_while(|char| word_char(*char)).map(char::len_utf8).sum();
            if len == 0 {
                break;
            }
            segments.push((start, start + len));
            if text[start + len..].starts_with('.') {
                start += len + 1;
            } else {
                break;
            }
        }
        if let Some(&(_, end)) = segments.last() {
            if text[end..].trim_start().starts_with('(') {
                segments.pop();
            }
        }
        while segments.last().is_some_and(|&(from, to)| ["get", "asProvider", "orNull", "getOrNull", "orElse", "isPresent", "present", "map", "flatMap"].contains(&&text[from..to])) {
            segments.pop();
        }
        if !segments.is_empty() {
            found.push(segments);
        }
    }
    found
}

/// Checks each name the catalog `name`, at `shown` in the tree, leads in a script against its
/// aliases.
fn references(lines: &Lines, text: &str, class: &[u8], name: &str, shown: &str, aliases: &catalog::Aliases, found: &mut Vec<Finding>) {
    for segments in chains(text, class, name) {
        let words: Vec<&str> = segments.iter().map(|&(from, to)| &text[from..to]).collect();
        let (kind, of, path_from) = match words[0] {
            "versions" => ("version", &aliases.versions, 1),
            "plugins" => ("plugin", &aliases.plugins, 1),
            "bundles" => ("bundle", &aliases.bundles, 1),
            _ => ("library", &aliases.libraries, 0),
        };
        if path_from >= words.len() {
            continue;
        }
        let path = words[path_from..].join(".");
        let known: Vec<String> = of.iter().map(|(alias, _)| accessor(alias)).collect();
        if known.contains(&path) {
            continue;
        }
        let (from, to) = (segments[path_from].0, segments.last().map_or(0, |&(_, to)| to));
        let mut finding = Finding::new(lines, from, to, ERROR, "gradle-catalog", format!("`{name}.{}` names no {kind} of `{shown}`", words.join(".")));
        for near in nearest(&path, known.iter().map(String::as_str)) {
            finding.fixes.push(edit_fix(format!("Change to {name}.{}{near}", if path_from == 1 { format!("{}.", words[0]) } else { String::new() }), lines, from, to, &near));
        }
        found.push(finding);
    }
}

/// What completes the word before byte `at` of a script: a catalog's aliases after its accessor,
/// and at the head of a statement, what the block it stands in takes.
pub(crate) fn complete(root: &Path, path: &Path, text: &str, at: usize) -> Vec<Item> {
    let class = classes(&text[..at]);
    if class.last().is_some_and(|last| *last != CODE) && !text[..at].ends_with('\n') {
        return Vec::new();
    }
    let token = run_before(text, at, |char| word_char(char) || char == '.');
    if let Some((first, rest)) = token.split_once('.') {
        let folder = build_root(root, path);
        let Some((_, _, catalog_text)) = catalogs(&folder).into_iter().find(|(name, _, _)| name == first) else {
            return Vec::new();
        };
        let aliases = catalog::aliases(&catalog_text);
        let mut values: Vec<(String, &'static str, String)> = aliases.libraries.iter().map(|(alias, about)| (accessor(alias), "module", about.clone())).collect();
        for (lead, of, kind) in [("versions", &aliases.versions, "constant"), ("plugins", &aliases.plugins, "module"), ("bundles", &aliases.bundles, "module")] {
            values.extend(of.iter().map(|(alias, about)| (format!("{lead}.{}", accessor(alias)), kind, about.clone())));
        }
        return relative(rest, values);
    }
    let line_start = text[..at].rfind('\n').map_or(0, |at| at + 1);
    let before = text[line_start..at - token.len()].trim();
    if !(before.is_empty() || before.ends_with('{') || before.ends_with(';')) {
        return Vec::new();
    }
    let full = classes(text);
    let (blocks, _) = blocks(text, &full);
    let inner = blocks_at(&blocks, at);
    let settings = path.file_name().is_some_and(|name| name.to_string_lossy().to_lowercase().starts_with("settings.gradle"));
    match inner.last().map(|block| block.name.as_str()) {
        None if settings => items(&SETTINGS_TOP, "keyword", "settings"),
        None => items(&TOP, "keyword", "Gradle"),
        Some("dependencies") => {
            let mut found = items(&CONFIGURATIONS, "function", "configuration");
            found.extend(items(&HELPERS, "function", "dependencies"));
            let declared = declared(text, &full, &blocks);
            found.extend(declared.iter().map(|word| Item { label: word.clone(), kind: "function", detail: "declared configuration".to_string(), insert: word.clone(), snippet: false }));
            found
        }
        Some(name) => items(block_words(name), "keyword", name),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn tree(files: &[(&str, &str)]) -> PathBuf {
        let dir = std::env::temp_dir().join(format!("orior-gradle-{}-{}", std::process::id(), files.len()));
        let _ = std::fs::remove_dir_all(&dir);
        for (name, text) in files {
            let path = dir.join(name);
            std::fs::create_dir_all(path.parent().unwrap()).unwrap();
            std::fs::write(path, text).unwrap();
        }
        dir
    }

    const SCRIPT: &str = "plugins {\n    id(\"java\")\n}\n\ndependencies {\n    implementation(libs.groovy.core)\n    implementation(libs.groovy.cor)\n    compile(\"a:b:1\")\n    implmentation(\"a:b:1\")\n    testImplementation(\"org.junit:junit:4.13\")\n    api(\"onlyone\")\n    runtimeOnly(\"g:a:2.0-SNAPSHOT\")\n    implementation(libs.versions.groovy.get())\n    integrationTestImplementation(\"x:y:1\")\n    constraints {\n        implementation(\"c:d:1\")\n    }\n}\n// } a brace in a comment\nval s = \"{\"\n";

    #[test]
    fn a_script_is_checked() {
        let root = tree(&[("settings.gradle.kts", "rootProject.name = \"x\"\n"), ("gradle/libs.versions.toml", "[versions]\ngroovy = \"3\"\n[libraries]\ngroovy-core = { module = \"g:groovy\", version.ref = \"groovy\" }\n"), ("build.gradle.kts", SCRIPT)]);
        let found = check(&root, &root.join("build.gradle.kts"), SCRIPT);
        let said: Vec<(&str, String)> = found.iter().map(|one| (one.code, one.message.clone())).collect();
        assert!(said.iter().any(|(code, message)| *code == "gradle-catalog" && message.contains("libs.groovy.cor")), "{said:?}");
        assert!(said.iter().any(|(code, message)| *code == "gradle-configuration" && message.contains("`compile`")), "{said:?}");
        assert!(said.iter().any(|(code, message)| *code == "gradle-configuration" && message.contains("`implmentation`")), "{said:?}");
        assert!(said.iter().any(|(code, message)| *code == "gradle-notation" && message.contains("onlyone")), "{said:?}");
        assert!(said.iter().any(|(code, _)| *code == "snapshot"), "{said:?}");
        assert_eq!(found.len(), 5, "{said:?}");
        let typo = found.iter().find(|one| one.message.contains("implmentation")).unwrap();
        assert_eq!(typo.fixes[0]["title"], "Change to implementation");
        let cor = found.iter().find(|one| one.code == "gradle-catalog").unwrap();
        assert_eq!(cor.fixes[0]["edits"][0]["text"], "groovy.core");
        let _ = std::fs::remove_dir_all(root);
    }

    #[test]
    fn braces_left_open_are_found() {
        let found = check(Path::new("/none"), Path::new("/none/build.gradle"), "dependencies {\n  implementation 'a:b:1'\n");
        assert_eq!(found.len(), 1);
        assert!(found[0].message.contains("`dependencies`"));
    }

    #[test]
    fn a_script_completes_by_its_block_and_its_catalog() {
        let root = tree(&[("gradle/libs.versions.toml", "[versions]\ngroovy = \"3\"\n[libraries]\ngroovy-core = \"g:groovy:3\"\ngroovy-json = \"g:json:3\"\n[plugins]\nshadow = \"com.github.johnrengelman.shadow:8\"\n")]);
        let path = root.join("build.gradle");
        let labels = |text: &str| complete(&root, &path, text, text.len()).into_iter().map(|item| item.label).collect::<Vec<_>>();
        assert_eq!(labels("dependencies {\n    implementation libs.groovy."), ["core", "json"]);
        assert!(labels("dependencies {\n    implementation libs.").contains(&"versions.groovy".to_string()));
        assert!(labels("plugins {\n    alias(libs.plugins.sh").contains(&"shadow".to_string()));
        assert!(labels("dependencies {\n    impl").contains(&"implementation".to_string()));
        assert!(labels("repositories {\n    mav").contains(&"mavenCentral".to_string()));
        assert!(labels("dep").contains(&"dependencies".to_string()));
        assert!(labels("x = \"dep").is_empty());
        let _ = std::fs::remove_dir_all(root);
    }
}
