// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! Maven's POM: its elements read as XML and checked against the POM's own, its properties and its
//! parent's, its dependencies' versions and scopes, and its executions' phases; completed by the
//! element it is in, the properties it can name, and the groups, artifacts and versions of the local
//! repository. The compiler's settings a POM gives and Maven's own, from `settings.xml`, are read
//! as settings and written again.

use std::collections::{HashMap, HashSet};
use std::path::{Path, PathBuf};

use serde::Serialize;
use serde_json::json;

use super::{nearest, relative, run_before, run_fix, snapshot, Finding, Lines, ERROR, NOTE, WARNING};
use crate::servers::{FileEdit, Item, TextEdit};

/// The children each element of a POM may hold. An element inside a `configuration` or a
/// `properties` holds what its plugin or its reader makes of it, and is not checked.
const SCHEMA: &[(&str, &[&str])] = &[
    ("project", &["modelVersion", "parent", "groupId", "artifactId", "version", "packaging", "name", "description", "url", "inceptionYear", "organization", "licenses", "developers", "contributors", "mailingLists", "prerequisites", "modules", "scm", "issueManagement", "ciManagement", "distributionManagement", "properties", "dependencyManagement", "dependencies", "repositories", "pluginRepositories", "build", "reporting", "profiles"]),
    ("parent", &["groupId", "artifactId", "version", "relativePath"]),
    ("organization", &["name", "url"]),
    ("licenses", &["license"]),
    ("license", &["name", "url", "distribution", "comments"]),
    ("developers", &["developer"]),
    ("contributors", &["contributor"]),
    ("developer", &["id", "name", "email", "url", "organization", "organizationUrl", "roles", "timezone", "properties"]),
    ("contributor", &["name", "email", "url", "organization", "organizationUrl", "roles", "timezone", "properties"]),
    ("roles", &["role"]),
    ("mailingLists", &["mailingList"]),
    ("mailingList", &["name", "subscribe", "unsubscribe", "post", "archive", "otherArchives"]),
    ("otherArchives", &["otherArchive"]),
    ("prerequisites", &["maven"]),
    ("modules", &["module"]),
    ("scm", &["connection", "developerConnection", "tag", "url"]),
    ("issueManagement", &["system", "url"]),
    ("ciManagement", &["system", "url", "notifiers"]),
    ("notifiers", &["notifier"]),
    ("notifier", &["type", "sendOnError", "sendOnFailure", "sendOnSuccess", "sendOnWarning", "address", "configuration"]),
    ("distributionManagement", &["repository", "snapshotRepository", "site", "downloadUrl", "relocation", "status"]),
    ("repository", &["uniqueVersion", "releases", "snapshots", "id", "name", "url", "layout"]),
    ("snapshotRepository", &["uniqueVersion", "releases", "snapshots", "id", "name", "url", "layout"]),
    ("site", &["id", "name", "url"]),
    ("relocation", &["groupId", "artifactId", "version", "message"]),
    ("dependencyManagement", &["dependencies"]),
    ("dependencies", &["dependency"]),
    ("dependency", &["groupId", "artifactId", "version", "type", "classifier", "scope", "systemPath", "exclusions", "optional"]),
    ("exclusions", &["exclusion"]),
    ("exclusion", &["groupId", "artifactId"]),
    ("repositories", &["repository"]),
    ("pluginRepositories", &["pluginRepository"]),
    ("pluginRepository", &["releases", "snapshots", "id", "name", "url", "layout"]),
    ("releases", &["enabled", "updatePolicy", "checksumPolicy"]),
    ("snapshots", &["enabled", "updatePolicy", "checksumPolicy"]),
    ("build", &["sourceDirectory", "scriptSourceDirectory", "testSourceDirectory", "outputDirectory", "testOutputDirectory", "extensions", "defaultGoal", "resources", "testResources", "directory", "finalName", "filters", "pluginManagement", "plugins"]),
    ("extensions", &["extension"]),
    ("extension", &["groupId", "artifactId", "version"]),
    ("resources", &["resource"]),
    ("testResources", &["testResource"]),
    ("resource", &["targetPath", "filtering", "directory", "includes", "excludes"]),
    ("testResource", &["targetPath", "filtering", "directory", "includes", "excludes"]),
    ("includes", &["include"]),
    ("excludes", &["exclude"]),
    ("filters", &["filter"]),
    ("pluginManagement", &["plugins"]),
    ("plugins", &["plugin"]),
    ("plugin", &["groupId", "artifactId", "version", "extensions", "executions", "dependencies", "inherited", "configuration", "reportSets"]),
    ("executions", &["execution"]),
    ("execution", &["id", "phase", "goals", "inherited", "configuration"]),
    ("goals", &["goal"]),
    ("reporting", &["excludeDefaults", "outputDirectory", "plugins"]),
    ("reportSets", &["reportSet"]),
    ("reportSet", &["id", "reports", "inherited", "configuration"]),
    ("reports", &["report"]),
    ("profiles", &["profile"]),
    ("profile", &["id", "activation", "build", "modules", "distributionManagement", "properties", "dependencyManagement", "dependencies", "repositories", "pluginRepositories", "reporting"]),
    ("activation", &["activeByDefault", "jdk", "os", "property", "file", "packaging"]),
    ("os", &["name", "family", "arch", "version"]),
    ("property", &["name", "value"]),
    ("file", &["missing", "exists"]),
];

const SCOPES: [&str; 6] = ["compile", "provided", "runtime", "test", "system", "import"];

const PHASES: [&str; 29] = [
    "pre-clean", "clean", "post-clean", "validate", "initialize", "generate-sources", "process-sources", "generate-resources", "process-resources",
    "compile", "process-classes", "generate-test-sources", "process-test-sources", "generate-test-resources", "process-test-resources",
    "test-compile", "process-test-classes", "test", "prepare-package", "package", "pre-integration-test", "integration-test",
    "post-integration-test", "verify", "install", "deploy", "pre-site", "site", "post-site",
];

const PACKAGINGS: [&str; 8] = ["jar", "war", "ear", "pom", "maven-plugin", "ejb", "rar", "bundle"];
const TYPES: [&str; 7] = ["jar", "war", "pom", "test-jar", "ejb-client", "zip", "maven-plugin"];
const BOOLEANS: [&str; 2] = ["true", "false"];

/// The properties a POM's `<properties>` most often sets.
const COMMON_PROPERTIES: [&str; 6] = ["maven.compiler.release", "maven.compiler.source", "maven.compiler.target", "project.build.sourceEncoding", "project.reporting.outputEncoding", "maven.compiler.encoding"];

/// The first words of the properties Maven, Java and the system give a POM.
const GIVEN: [&str; 16] = ["project", "pom", "env", "settings", "java", "os", "user", "maven", "file", "line", "path", "sun", "session", "basedir", "revision", "changelist"];

/// The properties a POM can name of its project.
const PROJECT_PROPERTIES: [&str; 11] = ["project.groupId", "project.artifactId", "project.version", "project.name", "project.basedir", "project.baseUri", "project.build.directory", "project.build.outputDirectory", "project.build.finalName", "project.build.sourceDirectory", "project.parent.version"];

/// An element of an XML file: its name and the span of the name in its opening tag, where it starts
/// and ends, the span of what it holds, its parent and its children.
pub(crate) struct Element {
    pub name: String,
    pub from: usize,
    pub to: usize,
    pub inner: (usize, usize),
    pub close: Option<(usize, usize)>,
    pub parent: Option<usize>,
    pub children: Vec<usize>,
}

/// An XML file as read: its elements, its comments' spans, and what kept it from reading.
pub(crate) struct Xml {
    pub elements: Vec<Element>,
    pub comments: Vec<(usize, usize)>,
    pub errors: Vec<(usize, usize, String)>,
    pub open: Vec<usize>,
}

fn name_char(char: char) -> bool {
    char.is_alphanumeric() || "_:.-".contains(char)
}

/// Reads an XML file's text, as far as it goes: an element its text leaves open is in `open`.
pub(crate) fn parse(text: &str) -> Xml {
    let mut xml = Xml { elements: Vec::new(), comments: Vec::new(), errors: Vec::new(), open: Vec::new() };
    let mut at = 0;
    while let Some(off) = text[at..].find('<') {
        let lt = at + off;
        let rest = &text[lt..];
        let skip = [("<!--", "-->"), ("<![CDATA[", "]]>"), ("<?", "?>"), ("<!", ">")].into_iter().find(|(open, _)| rest.starts_with(open));
        if let Some((open, close)) = skip {
            match rest[open.len()..].find(close) {
                Some(end) => {
                    at = lt + open.len() + end + close.len();
                    if open == "<!--" {
                        xml.comments.push((lt, at));
                    }
                }
                None => {
                    xml.errors.push((lt, lt + open.len(), format!("This `{open}` is not closed with `{close}`")));
                    break;
                }
            }
            continue;
        }
        let closing = rest.starts_with("</");
        let name_from = lt + if closing { 2 } else { 1 };
        let name_len: usize = text[name_from..].chars().take_while(|char| name_char(*char)).map(char::len_utf8).sum();
        let name_to = name_from + name_len;
        let name = &text[name_from..name_to];
        let Some(gt) = tag_end(text, name_to) else {
            xml.errors.push((lt, name_to.max(lt + 1), "This tag is not closed with `>`".to_string()));
            break;
        };
        at = gt + 1;
        if name.is_empty() {
            xml.errors.push((lt, lt + 1, "This `<` starts no element".to_string()));
            continue;
        }
        if closing {
            match xml.open.iter().rposition(|&index| xml.elements[index].name == name) {
                Some(depth) => {
                    for &left in &xml.open[depth + 1..] {
                        let element = &xml.elements[left];
                        xml.errors.push((element.from, element.to, format!("`<{}>` is not closed", element.name)));
                    }
                    let index = xml.open[depth];
                    xml.open.truncate(depth);
                    let element = &mut xml.elements[index];
                    element.inner.1 = lt;
                    element.close = Some((name_from, name_to));
                }
                None => xml.errors.push((name_from, name_to, format!("`</{name}>` closes no element"))),
            }
            continue;
        }
        let index = xml.elements.len();
        let parent = xml.open.last().copied();
        let lone = text[..gt].ends_with('/');
        xml.elements.push(Element { name: name.to_string(), from: name_from, to: name_to, inner: (gt + 1, gt + 1), close: if lone { Some((name_from, name_to)) } else { None }, parent, children: Vec::new() });
        if let Some(parent) = parent {
            xml.elements[parent].children.push(index);
        }
        if !lone {
            xml.open.push(index);
        }
    }
    xml
}

/// The `>` that ends a tag whose name ends at `from`, past the quotes of its attributes.
fn tag_end(text: &str, from: usize) -> Option<usize> {
    let mut quote = None;
    for (at, char) in text[from..].char_indices() {
        match (quote, char) {
            (None, '"' | '\'') => quote = Some(char),
            (Some(open), _) if char == open => quote = None,
            (None, '>') => return Some(from + at),
            (None, '<') => return None,
            _ => {}
        }
    }
    None
}

impl Xml {
    pub fn root(&self) -> Option<usize> {
        self.elements.iter().position(|element| element.parent.is_none())
    }

    pub fn child(&self, of: usize, name: &str) -> Option<usize> {
        self.elements[of].children.iter().copied().find(|&child| self.elements[child].name == name)
    }

    pub fn children<'a>(&'a self, of: usize, name: &'a str) -> impl Iterator<Item = usize> + 'a {
        self.elements[of].children.iter().copied().filter(move |&child| self.elements[child].name == name)
    }

    /// The text an element holds, where it holds no elements, its entities read.
    pub fn text(&self, text: &str, of: usize) -> Option<String> {
        let element = &self.elements[of];
        if !element.children.is_empty() || element.close.is_none() {
            return None;
        }
        let raw = text.get(element.inner.0..element.inner.1)?.trim();
        let raw = raw.strip_prefix("<![CDATA[").and_then(|inner| inner.strip_suffix("]]>")).unwrap_or(raw);
        Some(raw.replace("&lt;", "<").replace("&gt;", ">").replace("&quot;", "\"").replace("&apos;", "'").replace("&amp;", "&"))
    }

    /// The text of the child `name` of `of`.
    pub fn text_of(&self, text: &str, of: usize, name: &str) -> Option<String> {
        self.child(of, name).and_then(|child| self.text(text, child))
    }

    /// The element at the end of `path` under `of`, each step the first child of its name.
    pub fn at_path(&self, of: usize, path: &[&str]) -> Option<usize> {
        path.iter().try_fold(of, |at, name| self.child(at, name))
    }

    fn inside_free(&self, mut at: usize) -> bool {
        while let Some(parent) = self.elements[at].parent {
            if matches!(self.elements[parent].name.as_str(), "configuration" | "properties") {
                return true;
            }
            at = parent;
        }
        false
    }
}

fn schema(name: &str) -> Option<&'static [&'static str]> {
    SCHEMA.iter().find(|(parent, _)| *parent == name).map(|(_, children)| *children)
}

/// What a POM gives its children: its properties, the dependencies its management gives a version,
/// and whether it and each parent above it were read.
#[derive(Default)]
struct Model {
    properties: HashMap<String, String>,
    managed: HashSet<String>,
    imports: bool,
    whole: bool,
}

/// The home folder of the reader.
fn user_home() -> Option<PathBuf> {
    std::env::var_os("USERPROFILE").or_else(|| std::env::var_os("HOME")).map(PathBuf::from)
}

/// The user's `settings.xml`.
fn user_settings() -> Option<PathBuf> {
    user_home().map(|home| home.join(".m2").join("settings.xml"))
}

/// The local repository: the one the user's settings name, `~/.m2/repository` where they name none.
pub(crate) fn local_repository() -> Option<PathBuf> {
    let named = user_settings().and_then(|path| std::fs::read_to_string(path).ok()).and_then(|text| {
        let xml = parse(&text);
        let root = xml.root()?;
        xml.text_of(&text, root, "localRepository").filter(|value| !value.is_empty())
    });
    named.map(PathBuf::from).or_else(|| user_home().map(|home| home.join(".m2").join("repository")))
}

/// The POM a POM's `<parent>` names: by its relative path, `../pom.xml` where it gives none, and in
/// the local repository where the path holds no POM of it.
fn parent_pom(path: &Path, xml: &Xml, text: &str, parent: usize) -> Option<(PathBuf, String)> {
    let folder = path.parent()?;
    let relative = xml.child(parent, "relativePath").map(|at| xml.text(text, at).unwrap_or_default()).unwrap_or_else(|| "../pom.xml".to_string());
    if !relative.is_empty() {
        let mut place = folder.join(&relative);
        if place.is_dir() {
            place = place.join("pom.xml");
        }
        if let Ok(found) = std::fs::read_to_string(&place) {
            return Some((place, found));
        }
    }
    let (group, artifact, version) = (xml.text_of(text, parent, "groupId")?, xml.text_of(text, parent, "artifactId")?, xml.text_of(text, parent, "version")?);
    let place = local_repository()?.join(group.replace('.', "/")).join(&artifact).join(&version).join(format!("{artifact}-{version}.pom"));
    std::fs::read_to_string(&place).ok().map(|found| (place, found))
}

/// What the POM at `path`, its text `text`, gives its children, its parents' read up to `depth`
/// above it.
fn model_of(path: &Path, text: &str, depth: usize) -> Model {
    let xml = parse(text);
    let mut model = Model { whole: true, ..Model::default() };
    let Some(root) = xml.root() else {
        return model;
    };
    if let Some(parent) = xml.child(root, "parent") {
        match (depth < 8).then(|| parent_pom(path, &xml, text, parent)).flatten() {
            Some((place, found)) => model = model_of(&place, &found, depth + 1),
            None => model.whole = false,
        }
        if let Some(version) = xml.text_of(text, parent, "version") {
            model.properties.insert("project.parent.version".into(), version.clone());
            model.properties.entry("project.version".into()).or_insert(version);
        }
    }
    for (name, value) in [("project.groupId", "groupId"), ("project.artifactId", "artifactId"), ("project.version", "version")] {
        if let Some(found) = xml.text_of(text, root, value) {
            model.properties.insert(name.to_string(), found);
        }
    }
    let mut holders = vec![root];
    holders.extend(xml.child(root, "profiles").into_iter().flat_map(|profiles| xml.children(profiles, "profile").collect::<Vec<_>>()));
    for holder in holders {
        if let Some(properties) = xml.child(holder, "properties") {
            for &one in &xml.elements[properties].children {
                model.properties.insert(xml.elements[one].name.clone(), xml.text(text, one).unwrap_or_default());
            }
        }
        if let Some(dependencies) = xml.at_path(holder, &["dependencyManagement", "dependencies"]) {
            for dependency in xml.children(dependencies, "dependency") {
                if let (Some(group), Some(artifact)) = (xml.text_of(text, dependency, "groupId"), xml.text_of(text, dependency, "artifactId")) {
                    model.managed.insert(format!("{}:{}", resolve(&group, &model.properties), resolve(&artifact, &model.properties)));
                }
                model.imports |= xml.text_of(text, dependency, "scope").as_deref() == Some("import");
            }
        }
    }
    model
}

/// `value` with each property it names given, as far as `properties` give them.
fn resolve(value: &str, properties: &HashMap<String, String>) -> String {
    let mut out = value.to_string();
    for _ in 0..8 {
        let Some(start) = out.find("${") else {
            break;
        };
        let Some(len) = out[start..].find('}') else {
            break;
        };
        let name = &out[start + 2..start + len];
        let Some(given) = properties.get(name) else {
            break;
        };
        out = format!("{}{}{}", &out[..start], given, &out[start + len + 1..]);
    }
    out
}

/// Whether Maven, Java or the system give a property of this name.
fn given(name: &str) -> bool {
    let first = name.split('.').next().unwrap_or(name);
    GIVEN.contains(&first) || name == "sha1"
}

/// The program that runs the Maven build at `folder` of the tree at `root`: its wrapper, the
/// nearest above it in the tree, where one is.
pub(crate) fn program(root: &Path, folder: &Path) -> String {
    let mut up = String::new();
    let mut at = folder.to_path_buf();
    loop {
        if at.join("mvnw").is_file() {
            return format!("{}mvnw", if up.is_empty() { "./".to_string() } else { up });
        }
        if at == root || !at.starts_with(root) {
            return "mvn".to_string();
        }
        match at.parent() {
            Some(parent) => at = parent.to_path_buf(),
            None => return "mvn".to_string(),
        }
        up.push_str("../");
    }
}

/// A fix that writes `name` over an element's name, in its opening tag and its closing tag.
fn rename_fix(lines: &Lines, element: &Element, name: &str) -> serde_json::Value {
    let mut edits = vec![TextEdit { from: lines.place(element.from), to: lines.place(element.to), text: name.to_string() }];
    if let Some((from, to)) = element.close.filter(|close| close.0 != element.from) {
        edits.push(TextEdit { from: lines.place(from), to: lines.place(to), text: name.to_string() });
    }
    json!({"title": format!("Change to <{name}>"), "edits": edits})
}

/// The findings of a POM.
pub(crate) fn check(root: &Path, path: &Path, text: &str) -> Vec<Finding> {
    let lines = Lines::new(text);
    let xml = parse(text);
    let mut found: Vec<Finding> = xml.errors.iter().map(|(from, to, message)| Finding::new(&lines, *from, *to, ERROR, "pom-xml", message.clone())).collect();
    for &left in &xml.open {
        let element = &xml.elements[left];
        found.push(Finding::new(&lines, element.from, element.to, ERROR, "pom-xml", format!("`<{}>` is not closed", element.name)));
    }
    let Some(project) = xml.root().filter(|&root| xml.elements[root].name == "project") else {
        return found;
    };
    let model = model_of(path, text, 0);
    if let Some(version) = xml.child(project, "modelVersion") {
        if xml.text(text, version).is_some_and(|value| value != "4.0.0") {
            let element = &xml.elements[version];
            found.push(Finding::new(&lines, element.inner.0, element.inner.1, ERROR, "pom-model", "A POM's `<modelVersion>` is `4.0.0`".to_string()));
        }
    }
    for (index, element) in xml.elements.iter().enumerate() {
        let Some(parent) = element.parent else {
            continue;
        };
        let parent_name = xml.elements[parent].name.as_str();
        let Some(allowed) = schema(parent_name) else {
            continue;
        };
        if xml.inside_free(index) || allowed.contains(&element.name.as_str()) {
            continue;
        }
        let mut finding = Finding::new(&lines, element.from, element.to, WARNING, "pom-element", format!("`<{}>` is no element of `<{parent_name}>`", element.name));
        for near in nearest(&element.name, allowed.iter().copied()) {
            finding.fixes.push(rename_fix(&lines, element, &near));
        }
        found.push(finding);
    }
    if model.whole {
        properties(&lines, text, &xml, &model, &mut found);
    }
    let folder = path.parent().unwrap_or(root);
    let run = program(root, folder);
    let shown = super::inside(root, folder);
    let update = |coordinates: &str| run_fix(format!("Update the snapshot {coordinates}"), shown.clone(), format!("{run} -U dependency:get -Dartifact={coordinates}"));
    for (index, element) in xml.elements.iter().enumerate() {
        match element.name.as_str() {
            "dependencies" if !xml.inside_free(index) => dependencies(&lines, text, &xml, index, &model, &update, &mut found),
            "parent" if element.parent == Some(project) => {
                let (group, artifact, version) = (xml.text_of(text, index, "groupId"), xml.text_of(text, index, "artifactId"), xml.text_of(text, index, "version"));
                if let (Some(group), Some(artifact), Some(version)) = (group, artifact, version) {
                    if snapshot(&version) {
                        let at = xml.child(index, "version").map(|at| &xml.elements[at]).unwrap_or(element);
                        let coordinates = format!("{group}:{artifact}:{version}");
                        let mut finding = Finding::new(&lines, at.inner.0, at.inner.1, NOTE, "snapshot", format!("The parent `{coordinates}` is a snapshot, whose newest build Maven fetches with `-U`"));
                        finding.fixes.push(update(&format!("{coordinates}:pom")));
                        found.push(finding);
                    }
                }
            }
            "phase" if xml.elements[element.parent.unwrap_or(index)].name == "execution" => {
                if let Some(phase) = xml.text(text, index).filter(|phase| !phase.contains("${") && !PHASES.contains(&phase.as_str())) {
                    let mut finding = Finding::new(&lines, element.inner.0, element.inner.1, WARNING, "pom-phase", format!("`{phase}` is no phase of Maven's lifecycles"));
                    for near in nearest(&phase, PHASES.iter().copied()) {
                        finding.fixes.push(super::edit_fix(format!("Change to {near}"), &lines, element.inner.0, element.inner.1, &near));
                    }
                    found.push(finding);
                }
            }
            _ => {}
        }
    }
    found
}

/// Checks each property a POM names, outside its comments, against those it and its parents set and
/// those Maven gives.
fn properties(lines: &Lines, text: &str, xml: &Xml, model: &Model, found: &mut Vec<Finding>) {
    let mut at = 0;
    while let Some(off) = text[at..].find("${") {
        let start = at + off;
        at = start + 2;
        if xml.comments.iter().any(|&(from, to)| from <= start && start < to) {
            continue;
        }
        let Some(len) = text[at..].find(['}', '\n', '<']).filter(|&len| text[at + len..].starts_with('}')) else {
            continue;
        };
        let name = &text[at..at + len];
        if name.is_empty() || model.properties.contains_key(name) || given(name) {
            continue;
        }
        let mut finding = Finding::new(lines, at, at + len, WARNING, "pom-property", format!("No property `{name}` is set, by the POM, its parents or Maven"));
        for near in nearest(name, model.properties.keys().map(String::as_str)) {
            finding.fixes.push(super::edit_fix(format!("Change to ${{{near}}}"), lines, at, at + len, &near));
        }
        found.push(finding);
    }
}

/// Checks the dependencies of a `<dependencies>`: each names its group and artifact, has a version
/// or a management that gives it one, a scope Maven knows, and is listed once; a snapshot is noted.
fn dependencies(lines: &Lines, text: &str, xml: &Xml, list: usize, model: &Model, update: &dyn Fn(&str) -> serde_json::Value, found: &mut Vec<Finding>) {
    let managing = xml.elements[list].parent.is_some_and(|parent| xml.elements[parent].name == "dependencyManagement");
    let mut seen: HashSet<String> = HashSet::new();
    for dependency in xml.children(list, "dependency") {
        let element = &xml.elements[dependency];
        let get = |name: &str| xml.text_of(text, dependency, name).map(|value| resolve(&value, &model.properties));
        let (Some(group), Some(artifact)) = (get("groupId"), get("artifactId")) else {
            found.push(Finding::new(lines, element.from, element.to, ERROR, "pom-dependency", "A dependency names its `<groupId>` and its `<artifactId>`".to_string()));
            continue;
        };
        let key = format!("{group}:{artifact}");
        let whole = format!("{key}:{}:{}", get("type").unwrap_or_else(|| "jar".into()), get("classifier").unwrap_or_default());
        if !seen.insert(whole) {
            found.push(Finding::new(lines, element.from, element.to, WARNING, "pom-duplicate", format!("`{key}` is listed above it in these dependencies")));
        }
        if let Some(scope) = xml.child(dependency, "scope") {
            let at = &xml.elements[scope];
            if let Some(value) = xml.text(text, scope).filter(|value| !value.contains("${") && !SCOPES.contains(&value.as_str())) {
                let mut finding = Finding::new(lines, at.inner.0, at.inner.1, ERROR, "pom-scope", format!("`{value}` is no scope: a scope is {}", SCOPES.map(|one| format!("`{one}`")).join(", ")));
                for near in nearest(&value, SCOPES.iter().copied()) {
                    finding.fixes.push(super::edit_fix(format!("Change to {near}"), lines, at.inner.0, at.inner.1, &near));
                }
                found.push(finding);
            }
        }
        match get("version") {
            None if !managing && model.whole && !model.imports && !model.managed.contains(&key) => {
                found.push(Finding::new(lines, element.from, element.to, WARNING, "pom-version", format!("`{key}` has no version, and no `<dependencyManagement>` gives it one")));
            }
            Some(version) if snapshot(&version) => {
                let at = xml.child(dependency, "version").map(|at| &xml.elements[at]).unwrap_or(element);
                let coordinates = format!("{key}:{version}");
                let mut finding = Finding::new(lines, at.inner.0, at.inner.1, NOTE, "snapshot", format!("`{coordinates}` is a snapshot, whose newest build Maven fetches with `-U`"));
                finding.fixes.push(update(&coordinates));
                found.push(finding);
            }
            _ => {}
        }
    }
}

/// What completes the word before byte `at` of a POM: an element's name after `<`, the name of the
/// element left open after `</`, a property's after `${`, and in an element's text, the values it
/// takes, the local repository's groups, artifacts and versions among them.
pub(crate) fn complete(_root: &Path, path: &Path, text: &str, at: usize) -> Vec<Item> {
    let line_start = text[..at].rfind('\n').map_or(0, |at| at + 1);
    let token = run_before(text, at, name_char);
    let lead = &text[line_start..at - token.len()];
    if lead.ends_with("${") {
        let model = model_of(path, text, 0);
        let mut values: Vec<(String, &'static str, String)> = model.properties.iter().map(|(name, value)| (name.clone(), "constant", value.clone())).collect();
        values.extend(PROJECT_PROPERTIES.iter().map(|name| (name.to_string(), "constant", "Maven".to_string())));
        values.extend(["basedir", "maven.build.timestamp", "env.", "settings.localRepository", "java.version", "user.home"].map(|name| (name.to_string(), "constant", "Maven".to_string())));
        return relative(token, values);
    }
    if lead.ends_with("</") {
        let xml = parse(&text[..at - token.len() - 2]);
        return xml.open.last().map(|&open| {
            let name = xml.elements[open].name.clone();
            vec![Item { label: name.clone(), kind: "keyword", detail: "close".to_string(), insert: format!("{name}>"), snippet: false }]
        }).unwrap_or_default();
    }
    if lead.ends_with('<') {
        let xml = parse(&text[..at - token.len() - 1]);
        let Some(&open) = xml.open.last() else {
            return relative(token, vec![("project".to_string(), "keyword", "POM".to_string())]);
        };
        let parent = xml.elements[open].name.as_str();
        let (names, detail): (Vec<&str>, String) = if parent == "properties" {
            (COMMON_PROPERTIES.to_vec(), "property".to_string())
        } else if xml.inside_free(open) || parent == "configuration" {
            return Vec::new();
        } else {
            (schema(parent).map(<[&str]>::to_vec).unwrap_or_default(), format!("in <{parent}>"))
        };
        let follows = text[at..].starts_with('>');
        return relative(token, names.iter().map(|name| (name.to_string(), "keyword", detail.clone())).collect())
            .into_iter()
            .map(|mut item| {
                let full = format!("{}{}", &token[..token.len() - token.chars().rev().take_while(|char| super::word_char(*char)).map(char::len_utf8).sum::<usize>()], item.insert);
                if !follows {
                    item.insert = format!("{}>$0</{full}>", item.insert);
                    item.snippet = true;
                }
                item
            })
            .collect();
    }
    let Some(tag) = lead.rfind('<').filter(|&tag| !lead[tag..].starts_with("</") && lead[tag..].contains('>')) else {
        return Vec::new();
    };
    let name: String = lead[tag + 1..].chars().take_while(|char| name_char(*char)).collect();
    let words = |list: &[&str]| relative(token, list.iter().map(|one| (one.to_string(), "value", format!("<{name}>"))).collect());
    match name.as_str() {
        "scope" => words(&SCOPES),
        "phase" => words(&PHASES),
        "packaging" => words(&PACKAGINGS),
        "type" => words(&TYPES),
        "modelVersion" => words(&["4.0.0"]),
        "optional" | "activeByDefault" | "filtering" | "inherited" | "enabled" | "uniqueVersion" | "excludeDefaults" | "offline" => words(&BOOLEANS),
        "updatePolicy" => words(&["always", "daily", "never", "interval:"]),
        "checksumPolicy" => words(&["fail", "warn", "ignore"]),
        "layout" => words(&["default", "legacy"]),
        "groupId" | "artifactId" | "version" => {
            let Some(repository) = local_repository() else {
                return Vec::new();
            };
            let xml = parse(&text[..line_start + tag]);
            let sibling = |name: &str| xml.open.last().and_then(|&open| xml.text_of(text, open, name));
            let (folder, base) = match name.as_str() {
                "groupId" => {
                    let typed = &token[..token.rfind('.').map_or(0, |at| at + 1)];
                    (repository.join(typed.replace('.', "/")), typed.to_string())
                }
                "artifactId" => match sibling("groupId") {
                    Some(group) => (repository.join(group.replace('.', "/")), String::new()),
                    None => return Vec::new(),
                },
                _ => match (sibling("groupId"), sibling("artifactId")) {
                    (Some(group), Some(artifact)) => (repository.join(group.replace('.', "/")).join(artifact), String::new()),
                    _ => return Vec::new(),
                },
            };
            let mut values = Vec::new();
            if let Ok(entries) = std::fs::read_dir(&folder) {
                for entry in entries.flatten().filter(|entry| entry.path().is_dir()) {
                    values.push((format!("{base}{}", entry.file_name().to_string_lossy()), "module", "local repository".to_string()));
                }
            }
            values.sort();
            relative(token, values)
        }
        _ => Vec::new(),
    }
}

/// A setting a POM or Maven's own settings give: where it is kept, its name and its value as written,
/// the file it is read from, and whether it can be set here.
#[derive(Serialize, Clone, Debug)]
pub struct Setting {
    pub key: &'static str,
    pub group: &'static str,
    pub label: &'static str,
    pub value: String,
    pub file: String,
    pub settable: bool,
}

/// The properties that set each of the compiler's settings, and the element of the compiler plugin's
/// configuration that sets it there.
const COMPILER: [(&str, &str, &str, &str); 4] = [
    ("release", "Java release", "maven.compiler.release", "release"),
    ("source", "Java source", "maven.compiler.source", "source"),
    ("target", "Java target", "maven.compiler.target", "target"),
    ("encoding", "Source encoding", "project.build.sourceEncoding", "encoding"),
];

/// The `maven-compiler-plugin` of a POM's build or its plugin management.
fn compiler_plugin(xml: &Xml, text: &str, project: usize) -> Option<usize> {
    [&["build", "plugins"][..], &["build", "pluginManagement", "plugins"][..]].iter().filter_map(|path| xml.at_path(project, path)).flat_map(|plugins| xml.children(plugins, "plugin").collect::<Vec<_>>()).find(|&plugin| xml.text_of(text, plugin, "artifactId").as_deref() == Some("maven-compiler-plugin"))
}

/// Maven's home folder: the one `MAVEN_HOME` or `M2_HOME` names, or the one above the `bin` that
/// holds `mvn`.
fn maven_home() -> Option<PathBuf> {
    if let Some(home) = std::env::var_os("MAVEN_HOME").or_else(|| std::env::var_os("M2_HOME")) {
        return Some(PathBuf::from(home));
    }
    let names = ["mvn".to_string()];
    crate::toolchains::chosen_program("maven").or_else(|| crate::toolchains::path_folders().iter().find_map(|folder| crate::toolchains::program_in(folder, &names))).and_then(|program| program.parent()?.parent().map(Path::to_path_buf))
}

/// The compiler's settings the POM `pom` gives, its text `text`, and Maven's own.
pub fn settings(root: &Path, pom: &Path, text: &str) -> Vec<Setting> {
    let xml = parse(text);
    let shown = super::inside(root, pom);
    let mut rows = Vec::new();
    if let Some(project) = xml.root().filter(|&root| xml.elements[root].name == "project") {
        let model = model_of(pom, text, 0);
        let plugin = compiler_plugin(&xml, text, project);
        let configuration = plugin.and_then(|plugin| xml.child(plugin, "configuration"));
        for (key, label, property, element) in COMPILER {
            let value = model.properties.get(property).cloned().or_else(|| configuration.and_then(|at| xml.text_of(text, at, element))).unwrap_or_default();
            rows.push(Setting { key, group: "pom", label, value, file: shown.clone(), settable: true });
        }
        let version = plugin.and_then(|plugin| xml.text_of(text, plugin, "version"));
        rows.push(Setting { key: "compiler-plugin", group: "pom", label: "Compiler plugin version", settable: version.is_some(), value: version.unwrap_or_default(), file: shown.clone() });
        let arguments: Vec<String> = configuration.and_then(|at| xml.child(at, "compilerArgs")).map(|args| xml.elements[args].children.iter().filter_map(|&arg| xml.text(text, arg)).collect()).unwrap_or_default();
        rows.push(Setting { key: "compiler-args", group: "pom", label: "Compiler arguments", value: arguments.join(" "), file: shown, settable: false });
    }
    let home = maven_home();
    let user = user_settings();
    let global = home.as_ref().map(|home| home.join("conf").join("settings.xml")).filter(|path| path.is_file());
    let read = |path: &Option<PathBuf>| path.as_ref().and_then(|path| std::fs::read_to_string(path).ok()).unwrap_or_default();
    let (user_text, global_text) = (read(&user), read(&global));
    let (user_xml, global_xml) = (parse(&user_text), parse(&global_text));
    let value = |name: &str| [(&user_xml, &user_text), (&global_xml, &global_text)].iter().find_map(|(xml, text)| xml.root().and_then(|root| xml.text_of(text, root, name)));
    let listed = |list: &str, each: &str, show: &dyn Fn(&Xml, &str, usize) -> String| -> String {
        let mut found = Vec::new();
        for (xml, text) in [(&user_xml, &user_text), (&global_xml, &global_text)] {
            if let Some(holder) = xml.root().and_then(|root| xml.child(root, list)) {
                found.extend(xml.children(holder, each).map(|one| show(xml, text, one)));
            }
        }
        found.join("; ")
    };
    let path_of = |path: &Option<PathBuf>| path.as_ref().map(|path| path.display().to_string()).unwrap_or_default();
    let user_file = path_of(&user);
    rows.push(Setting { key: "maven-home", group: "settings", label: "Maven home", value: path_of(&home), file: String::new(), settable: false });
    rows.push(Setting { key: "user-settings", group: "settings", label: "User settings", value: user.as_ref().filter(|path| path.is_file()).map(|path| path.display().to_string()).unwrap_or_default(), file: user_file.clone(), settable: false });
    rows.push(Setting { key: "global-settings", group: "settings", label: "Global settings", value: path_of(&global), file: path_of(&global), settable: false });
    rows.push(Setting { key: "local-repository", group: "settings", label: "Local repository", value: value("localRepository").unwrap_or_else(|| local_repository().map(|path| path.display().to_string()).unwrap_or_default()), file: user_file.clone(), settable: true });
    rows.push(Setting { key: "offline", group: "settings", label: "Work offline", value: value("offline").unwrap_or_else(|| "false".into()), file: user_file.clone(), settable: true });
    rows.push(Setting { key: "mirrors", group: "settings", label: "Mirrors", value: listed("mirrors", "mirror", &|xml, text, one| format!("{} {} for {}", xml.text_of(text, one, "id").unwrap_or_default(), xml.text_of(text, one, "url").unwrap_or_default(), xml.text_of(text, one, "mirrorOf").unwrap_or_default())), file: user_file.clone(), settable: false });
    rows.push(Setting { key: "proxies", group: "settings", label: "Proxies", value: listed("proxies", "proxy", &|xml, text, one| format!("{}:{}", xml.text_of(text, one, "host").unwrap_or_default(), xml.text_of(text, one, "port").unwrap_or_default())), file: user_file.clone(), settable: false });
    rows.push(Setting { key: "active-profiles", group: "settings", label: "Active profiles", value: listed("activeProfiles", "activeProfile", &|xml, text, one| xml.text(text, one).unwrap_or_default()), file: user_file, settable: false });
    rows
}

/// The indent a line of `text` that starts at `at` has.
fn indent_at(text: &str, at: usize) -> String {
    let start = text[..at].rfind('\n').map_or(0, |at| at + 1);
    text[start..].chars().take_while(|char| *char == ' ' || *char == '\t').collect()
}

/// The indent one step deeper takes in `text`: the indent of the first child of its root.
fn step(text: &str, xml: &Xml) -> String {
    xml.root().and_then(|root| xml.elements[root].children.first().copied()).map(|first| indent_at(text, xml.elements[first].from)).filter(|indent| !indent.is_empty()).unwrap_or_else(|| "  ".to_string())
}

/// The edit that sets the element `name` under `holder` to `value`: its text written over where it
/// is, and where it is not, a line of it added at the end of `holder`.
fn set_element(text: &str, xml: &Xml, holder: usize, name: &str, value: &str, lines: &Lines) -> TextEdit {
    if let Some(at) = xml.child(holder, name) {
        let element = &xml.elements[at];
        return TextEdit { from: lines.place(element.inner.0), to: lines.place(element.inner.1), text: value.to_string() };
    }
    let end = xml.elements[holder].inner.1;
    let indent = format!("{}{}", indent_at(text, end), step(text, xml));
    let line_start = text[..end].rfind('\n').map_or(0, |at| at + 1);
    let at = if text[line_start..end].trim().is_empty() { line_start } else { end };
    let lead = if at == line_start { "" } else { "\n" };
    TextEdit { from: lines.place(at), to: lines.place(at), text: format!("{lead}{indent}<{name}>{value}</{name}>\n") }
}

/// Sets the setting `key` to `value`: in the POM `pom`, its text `text`, by the edits returned; in
/// the user's `settings.xml`, written to it here.
pub fn set(pom: &Path, text: &str, key: &str, value: &str) -> Result<Vec<FileEdit>, String> {
    let value = value.trim();
    if let Some((_, _, property, element)) = COMPILER.iter().find(|(name, ..)| *name == key) {
        let xml = parse(text);
        let lines = Lines::new(text);
        let project = xml.root().filter(|&root| xml.elements[root].name == "project").ok_or("This is no POM")?;
        let configuration = compiler_plugin(&xml, text, project).and_then(|plugin| xml.child(plugin, "configuration"));
        let properties = xml.child(project, "properties");
        let edit = match (properties.and_then(|at| xml.child(at, property)), configuration.and_then(|at| xml.child(at, element))) {
            (None, Some(_)) => set_element(text, &xml, configuration.unwrap_or(project), element, value, &lines),
            _ => match properties {
                Some(properties) => set_element(text, &xml, properties, property, value, &lines),
                None => {
                    let after = ["packaging", "version", "artifactId", "groupId", "parent", "modelVersion"].iter().find_map(|name| xml.child(project, name)).map(|at| xml.elements[at].close.map_or(xml.elements[at].to, |(_, to)| to + 1));
                    let at = after.unwrap_or(xml.elements[project].inner.0);
                    let unit = step(text, &xml);
                    TextEdit { from: lines.place(at), to: lines.place(at), text: format!("\n\n{unit}<properties>\n{unit}{unit}<{property}>{value}</{property}>\n{unit}</properties>") }
                }
            },
        };
        return Ok(vec![FileEdit { path: pom.display().to_string(), edits: vec![edit] }]);
    }
    if key == "compiler-plugin" {
        let xml = parse(text);
        let lines = Lines::new(text);
        let project = xml.root().ok_or("This is no POM")?;
        let plugin = compiler_plugin(&xml, text, project).ok_or("The POM names no maven-compiler-plugin")?;
        let version = xml.child(plugin, "version").ok_or("The POM's maven-compiler-plugin names no version to set")?;
        let element = &xml.elements[version];
        return Ok(vec![FileEdit { path: pom.display().to_string(), edits: vec![TextEdit { from: lines.place(element.inner.0), to: lines.place(element.inner.1), text: value.to_string() }] }]);
    }
    let name = match key {
        "local-repository" => "localRepository",
        "offline" if value == "true" || value == "false" => "offline",
        "offline" => return Err(format!("{value} is no answer: true or false")),
        _ => return Err(format!("{key} is no setting that can be set")),
    };
    let path = user_settings().ok_or("No home folder holds the user's settings")?;
    let now = std::fs::read_to_string(&path).unwrap_or_default();
    let written = if now.trim().is_empty() {
        format!("<settings xmlns=\"http://maven.apache.org/SETTINGS/1.0.0\">\n  <{name}>{value}</{name}>\n</settings>\n")
    } else {
        let xml = parse(&now);
        let lines = Lines::new(&now);
        let root = xml.root().filter(|&root| xml.elements[root].name == "settings").ok_or_else(|| format!("{} holds no <settings>", path.display()))?;
        crate::servers::apply(&now, &[set_element(&now, &xml, root, name, value, &lines)])
    };
    if let Some(folder) = path.parent() {
        std::fs::create_dir_all(folder).map_err(|error| error.to_string())?;
    }
    std::fs::write(&path, written).map_err(|error| format!("{}: {error}", path.display()))?;
    Ok(Vec::new())
}

#[cfg(test)]
mod tests {
    use super::*;

    const POM: &str = "<?xml version=\"1.0\"?>\n<project>\n  <modelVersion>4.0.0</modelVersion>\n  <groupId>g</groupId>\n  <artifactId>a</artifactId>\n  <version>1.0</version>\n  <properties>\n    <junit.version>4.13</junit.version>\n  </properties>\n  <dependencyManagement>\n    <dependencies>\n      <dependency><groupId>m</groupId><artifactId>managed</artifactId><version>2</version></dependency>\n    </dependencies>\n  </dependencyManagement>\n  <dependencies>\n    <dependency>\n      <groupId>junit</groupId>\n      <artifactId>junit</artifactId>\n      <version>${junit.verison}</version>\n      <scope>tset</scope>\n    </dependency>\n    <dependency><groupId>m</groupId><artifactId>managed</artifactId></dependency>\n    <dependency><groupId>x</groupId><artifactId>loose</artifactId></dependency>\n    <dependency><groupId>s</groupId><artifactId>snap</artifactId><version>1.1-SNAPSHOT</version></dependency>\n    <dependency><groupId>s</groupId><artifactId>snap</artifactId><version>1.1-SNAPSHOT</version></dependency>\n    <dependency><artifactId>nogroup</artifactId></dependency>\n  </dependencies>\n  <build>\n    <plugins>\n      <plugin>\n        <artifactId>maven-compiler-plugin</artifactId>\n        <version>3.11.0</version>\n        <configuration><release>17</release><anything>x</anything></configuration>\n        <executions><execution><phase>compil</phase></execution></executions>\n      </plugin>\n    </plugins>\n    <finalname>x</finalname>\n  </build>\n  <!-- ${not.checked} -->\n</project>\n";

    #[test]
    fn a_pom_is_checked() {
        let root = Path::new("/tree");
        let found = check(root, &root.join("pom.xml"), POM);
        let said: Vec<(&str, String)> = found.iter().map(|one| (one.code, one.message.clone())).collect();
        let has = |code: &str, part: &str| said.iter().any(|(one, message)| *one == code && message.contains(part));
        assert!(has("pom-property", "junit.verison"), "{said:?}");
        assert!(has("pom-scope", "tset"), "{said:?}");
        assert!(has("pom-version", "x:loose"), "{said:?}");
        assert!(has("snapshot", "s:snap:1.1-SNAPSHOT"), "{said:?}");
        assert!(has("pom-duplicate", "s:snap"), "{said:?}");
        assert!(has("pom-dependency", "groupId"), "{said:?}");
        assert!(has("pom-phase", "compil"), "{said:?}");
        assert!(has("pom-element", "finalname"), "{said:?}");
        assert_eq!(found.len(), 9, "{said:?}");
        let fix = &found.iter().find(|one| one.code == "pom-element").unwrap().fixes[0];
        assert_eq!(fix["edits"].as_array().unwrap().len(), 2, "the opening and the closing tag are both named again");
        let property = found.iter().find(|one| one.code == "pom-property").unwrap();
        assert_eq!(property.fixes[0]["edits"][0]["text"], "junit.version");
        let snap = found.iter().find(|one| one.code == "snapshot").unwrap();
        assert_eq!(snap.fixes[0]["run"]["line"], "mvn -U dependency:get -Dartifact=s:snap:1.1-SNAPSHOT");
    }

    #[test]
    fn broken_xml_is_found() {
        let found = check(Path::new("/t"), Path::new("/t/pom.xml"), "<project>\n  <dependencies>\n  </project>\n");
        assert!(found.iter().any(|one| one.message.contains("`<dependencies>` is not closed")), "{:?}", found.iter().map(|one| &one.message).collect::<Vec<_>>());
    }

    #[test]
    fn a_pom_completes_elements_properties_and_values() {
        let path = Path::new("/t/pom.xml");
        let complete_at = |text: &str| complete(Path::new("/t"), path, text, text.len());
        let items = complete_at("<project>\n  <dependencies>\n    <dependency>\n      <gr");
        let group = items.iter().find(|item| item.label == "groupId").expect("groupId is offered");
        assert_eq!(group.insert, "groupId>$0</groupId>");
        assert!(group.snippet);
        let closing = complete_at("<project>\n  <build>\n  </");
        assert_eq!(closing[0].insert, "build>");
        let props = complete_at("<project><properties><a.b>1</a.b></properties><version>${a.");
        assert_eq!(props.iter().map(|item| item.label.as_str()).collect::<Vec<_>>(), ["b"]);
        let scopes = complete_at("<project><dependencies><dependency><scope>te");
        assert!(scopes.iter().any(|item| item.label == "test"));
        let inside = complete_at("<project><properties><maven.");
        assert!(inside.iter().any(|item| item.label == "compiler.release" && item.insert == "compiler.release>$0</maven.compiler.release>"), "{:?}", inside.iter().map(|item| (&item.label, &item.insert)).collect::<Vec<_>>());
    }

    #[test]
    fn compiler_settings_are_read_and_set() {
        let rows = settings(Path::new("/t"), Path::new("/t/pom.xml"), POM);
        let row = |key: &str| rows.iter().find(|row| row.key == key).unwrap().value.clone();
        assert_eq!(row("release"), "17");
        assert_eq!(row("compiler-plugin"), "3.11.0");
        let edits = set(Path::new("/t/pom.xml"), POM, "release", "21").unwrap();
        let after = crate::servers::apply(POM, &edits[0].edits);
        assert!(after.contains("<release>21</release>"));
        let edits = set(Path::new("/t/pom.xml"), POM, "source", "21").unwrap();
        let after = crate::servers::apply(POM, &edits[0].edits);
        assert!(after.contains("    <junit.version>4.13</junit.version>\n    <maven.compiler.source>21</maven.compiler.source>\n  </properties>"), "{after}");
        let bare = "<project>\n  <modelVersion>4.0.0</modelVersion>\n  <artifactId>a</artifactId>\n</project>\n";
        let edits = set(Path::new("/t/pom.xml"), bare, "encoding", "UTF-8").unwrap();
        let after = crate::servers::apply(bare, &edits[0].edits);
        assert!(after.contains("<artifactId>a</artifactId>\n\n  <properties>\n    <project.build.sourceEncoding>UTF-8</project.build.sourceEncoding>\n  </properties>\n</project>"), "{after}");
    }
}
