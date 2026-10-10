// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! A tree's dev container, as its own devcontainer.json describes it, brought up with docker for a
//! window to open the tree in.
//!
//! The description is read from `.devcontainer/devcontainer.json`, `.devcontainer.json`, or the first
//! `.devcontainer/<name>/devcontainer.json`, as JSON with comments and trailing commas. Its image is
//! `image`, or the one `build.dockerfile` builds from `build.context` with `build.args` and
//! `build.target`; the tree is mounted at `workspaceFolder`, `/workspaces/<the tree's folder>` where
//! it names none, the container runs with `containerEnv`, `mounts`, `runArgs` and `containerUser`,
//! and it is kept running by a command of orior's in place of the image's own. A description with
//! `dockerComposeFile` brings up its `service` with docker compose, which mounts the tree as its files
//! say. `onCreateCommand` and `postCreateCommand` run in the container once it is made, and
//! `postStartCommand` each time it starts. `${localWorkspaceFolder}`,
//! `${localWorkspaceFolderBasename}`, `${containerWorkspaceFolder}` and `${localEnv:NAME}` read as
//! they do elsewhere.
//!
//! The container is named for the tree, and found again by that name: brought up once, it is started
//! again as it is.

use std::path::{Path, PathBuf};
use std::process::Command;

use serde_json::Value;

/// How a container is kept running, in place of the image's own command.
const KEEP_RUNNING: &str = "echo orior's dev container started; trap 'exit 0' TERM; while sleep 1 & wait $!; do :; done";

/// What a tree's dev container is built from.
#[derive(Debug, PartialEq)]
pub enum Source {
    Image(String),
    Build { dockerfile: PathBuf, context: PathBuf, args: Vec<(String, String)>, target: Option<String> },
    Compose { files: Vec<PathBuf>, service: String, services: Vec<String> },
}

/// A tree's dev container, as its description says.
#[derive(Debug)]
pub struct Plan {
    pub file: PathBuf,
    pub root: PathBuf,
    /// The container's name, and the compose project's.
    pub name: String,
    pub source: Source,
    /// The tree's folder in the container.
    pub folder: String,
    pub env: Vec<(String, String)>,
    pub mounts: Vec<String>,
    pub run_args: Vec<String>,
    pub user: Option<String>,
    pub on_create: Vec<Value>,
    pub on_start: Vec<Value>,
}

/// JSON with comments and trailing commas, as VS Code's files hold it, made plain JSON: each comment
/// outside a string taken out, and each comma before a closing bracket.
pub fn plain_json(text: &str) -> String {
    let chars: Vec<char> = text.chars().collect();
    let mut out = String::with_capacity(text.len());
    let mut at = 0;
    let mut in_string = false;
    while at < chars.len() {
        let c = chars[at];
        if in_string {
            out.push(c);
            if c == '\\' && at + 1 < chars.len() {
                out.push(chars[at + 1]);
                at += 2;
                continue;
            }
            if c == '"' {
                in_string = false;
            }
            at += 1;
        } else if c == '"' {
            in_string = true;
            out.push(c);
            at += 1;
        } else if c == '/' && chars.get(at + 1) == Some(&'/') {
            while at < chars.len() && chars[at] != '\n' {
                at += 1;
            }
        } else if c == '/' && chars.get(at + 1) == Some(&'*') {
            at += 2;
            while at + 1 < chars.len() && !(chars[at] == '*' && chars[at + 1] == '/') {
                at += 1;
            }
            at += 2;
        } else if c == ',' {
            let next = chars[at + 1..].iter().find(|one| !one.is_whitespace());
            if !matches!(next, Some(']') | Some('}')) {
                out.push(c);
            }
            at += 1;
        } else {
            out.push(c);
            at += 1;
        }
    }
    out
}

/// The tree's description of its dev container, or None.
pub fn find(root: &Path) -> Option<PathBuf> {
    let folder = root.join(".devcontainer");
    let mut found = [folder.join("devcontainer.json"), root.join(".devcontainer.json")].into_iter().find(|path| path.is_file());
    if found.is_none() {
        let mut inner: Vec<PathBuf> = std::fs::read_dir(&folder).into_iter().flatten().flatten().map(|entry| entry.path().join("devcontainer.json")).filter(|path| path.is_file()).collect();
        inner.sort();
        found = inner.into_iter().next();
    }
    found
}

/// FNV-1a of `bytes`: a name the same tree is given again from one run to the next.
fn fnv(bytes: &[u8]) -> u64 {
    bytes.iter().fold(0xcbf2_9ce4_8422_2325, |hash, byte| (hash ^ u64::from(*byte)).wrapping_mul(0x0100_0000_01b3))
}

/// The container's name for the tree at `root`: its folder's name, as docker takes names, and a mark
/// of its whole path.
pub fn name_for(root: &Path) -> String {
    let base: String = root.file_name().map(|name| name.to_string_lossy().to_lowercase()).unwrap_or_default().chars().map(|c| if c.is_ascii_alphanumeric() || c == '-' || c == '_' { c } else { '-' }).collect();
    format!("orior-{}-{:08x}", base.trim_matches('-'), fnv(root.to_string_lossy().as_bytes()) as u32)
}

/// `text` with the description's variables read.
fn expand(text: &str, root: &Path, folder: &str) -> String {
    let mut out = String::with_capacity(text.len());
    let mut rest = text;
    while let Some(open) = rest.find("${") {
        out.push_str(&rest[..open]);
        let Some(close) = rest[open..].find('}') else {
            out.push_str(&rest[open..]);
            return out;
        };
        let inner = &rest[open + 2..open + close];
        let read = match inner {
            "localWorkspaceFolder" => Some(root.to_string_lossy().into_owned()),
            "localWorkspaceFolderBasename" => root.file_name().map(|name| name.to_string_lossy().into_owned()),
            "containerWorkspaceFolder" => Some(folder.to_string()),
            "containerWorkspaceFolderBasename" => folder.rsplit('/').next().map(String::from),
            _ => inner.strip_prefix("localEnv:").map(|named| {
                let (name, default) = named.split_once(':').unwrap_or((named, ""));
                std::env::var(name).unwrap_or_else(|_| default.to_string())
            }),
        };
        match read {
            Some(value) => out.push_str(&value),
            None => out.push_str(&rest[open..open + close + 1]),
        }
        rest = &rest[open + close + 1..];
    }
    out.push_str(rest);
    out
}

fn strings(value: &Value) -> Vec<String> {
    match value {
        Value::String(one) => vec![one.clone()],
        Value::Array(many) => many.iter().filter_map(|one| one.as_str().map(String::from)).collect(),
        _ => Vec::new(),
    }
}

/// A mount as `mounts` names it, a string as docker takes it or an object of its parts.
fn mount_of(value: &Value) -> Option<String> {
    match value {
        Value::String(one) => Some(one.clone()),
        Value::Object(parts) => Some(parts.iter().filter_map(|(key, value)| value.as_str().map(|value| format!("{key}={value}"))).collect::<Vec<_>>().join(",")),
        _ => None,
    }
}

/// The tree's dev container, as its description says, or why it cannot be read.
pub fn plan(root: &Path) -> Result<Plan, String> {
    let file = find(root).ok_or_else(|| format!("{} has no .devcontainer/devcontainer.json", root.display()))?;
    let text = std::fs::read_to_string(&file).map_err(|error| format!("{}: {error}", file.display()))?;
    let spec: Value = serde_json::from_str(&plain_json(&text)).map_err(|error| format!("{}: {error}", file.display()))?;
    let base = file.parent().map(Path::to_path_buf).unwrap_or_else(|| root.to_path_buf());
    let tree_name = root.file_name().map(|name| name.to_string_lossy().into_owned()).unwrap_or_default();
    let compose = spec.get("dockerComposeFile").map(strings).filter(|files| !files.is_empty());
    let default_folder = if compose.is_some() { "/".to_string() } else { format!("/workspaces/{tree_name}") };
    let folder = spec["workspaceFolder"].as_str().map(|folder| expand(folder, root, "")).unwrap_or(default_folder);
    let read = |text: &str| expand(text, root, &folder);
    let source = if let Some(files) = compose {
        let service = spec["service"].as_str().ok_or_else(|| format!("{} names dockerComposeFile and no service", file.display()))?.to_string();
        Source::Compose { files: files.iter().map(|one| base.join(read(one))).collect(), services: strings(&spec["runServices"]), service }
    } else if let Some(image) = spec["image"].as_str() {
        Source::Image(read(image))
    } else if let Some(dockerfile) = spec["build"]["dockerfile"].as_str().or_else(|| spec["dockerFile"].as_str()) {
        let context = spec["build"]["context"].as_str().unwrap_or(".");
        let args = spec["build"]["args"].as_object().map(|args| args.iter().filter_map(|(key, value)| value.as_str().map(|value| (key.clone(), read(value)))).collect()).unwrap_or_default();
        Source::Build { dockerfile: base.join(read(dockerfile)), context: base.join(read(context)), args, target: spec["build"]["target"].as_str().map(String::from) }
    } else {
        return Err(format!("{} names no image, build.dockerfile or dockerComposeFile", file.display()));
    };
    let env = spec["containerEnv"].as_object().map(|env| env.iter().filter_map(|(key, value)| value.as_str().map(|value| (key.clone(), read(value)))).collect()).unwrap_or_default();
    let mounts = spec["mounts"].as_array().map(|mounts| mounts.iter().filter_map(mount_of).map(|mount| read(&mount)).collect()).unwrap_or_default();
    let run_args = strings(&spec["runArgs"]).iter().map(|arg| read(arg)).collect();
    let user = spec["containerUser"].as_str().or_else(|| spec["remoteUser"].as_str()).map(String::from);
    let commands = |keys: &[&str]| keys.iter().filter_map(|key| spec.get(*key).cloned()).filter(|value| !value.is_null()).collect::<Vec<_>>();
    Ok(Plan { name: name_for(root), root: root.to_path_buf(), source, folder, env, mounts, run_args, user, on_create: commands(&["onCreateCommand", "postCreateCommand"]), on_start: commands(&["postStartCommand"]), file })
}

impl Plan {
    /// The address a window opens the tree at in the container named `container`.
    pub fn address(&self, container: &str) -> String {
        format!("docker:{container}:{}", self.folder)
    }

    /// The words of `docker build` for an image the description builds, and the image's name.
    pub fn build_words(&self) -> Option<(Vec<String>, String)> {
        let Source::Build { dockerfile, context, args, target } = &self.source else { return None };
        let image = format!("{}-image", self.name);
        let mut words: Vec<String> = vec!["build".into(), "-f".into(), dockerfile.display().to_string(), "-t".into(), image.clone()];
        for (key, value) in args {
            words.push("--build-arg".into());
            words.push(format!("{key}={value}"));
        }
        if let Some(target) = target {
            words.push("--target".into());
            words.push(target.clone());
        }
        words.push(context.display().to_string());
        Some((words, image))
    }

    /// The words of `docker run` that make the container from `image`.
    pub fn run_words(&self, image: &str) -> Vec<String> {
        let mut words: Vec<String> = ["run", "-d", "--name", &self.name, "--label"].map(String::from).to_vec();
        words.push(format!("orior.tree={}", self.root.display()));
        words.push("--mount".into());
        words.push(format!("type=bind,source={},target={}", self.root.display(), self.folder));
        for (key, value) in &self.env {
            words.push("-e".into());
            words.push(format!("{key}={value}"));
        }
        for mount in &self.mounts {
            words.push("--mount".into());
            words.push(mount.clone());
        }
        words.extend(self.run_args.iter().cloned());
        if let Some(user) = &self.user {
            words.push("-u".into());
            words.push(user.clone());
        }
        words.extend(["--entrypoint", "/bin/sh", image, "-c", KEEP_RUNNING].map(String::from));
        words
    }

    /// The words of `docker compose` that name the description's files and its project.
    pub fn compose_words(&self) -> Option<Vec<String>> {
        let Source::Compose { files, .. } = &self.source else { return None };
        let mut words = vec!["compose".to_string()];
        for file in files {
            words.push("-f".into());
            words.push(file.display().to_string());
        }
        words.push("-p".into());
        words.push(self.name.clone());
        Some(words)
    }

    /// The words of `docker exec` that run one of the description's commands in `container`: a string
    /// by the container's shell, a list as its words; an object of them runs each.
    pub fn exec_words(&self, container: &str, command: &Value) -> Vec<Vec<String>> {
        let mut head: Vec<String> = vec!["exec".into(), "-w".into(), self.folder.clone()];
        if let Some(user) = &self.user {
            head.push("-u".into());
            head.push(user.clone());
        }
        head.push(container.to_string());
        match command {
            Value::String(line) => vec![[head.clone(), vec!["/bin/sh".into(), "-c".into(), line.clone()]].concat()],
            Value::Array(words) => vec![[head.clone(), words.iter().filter_map(|word| word.as_str().map(String::from)).collect()].concat()],
            Value::Object(each) => each.values().flat_map(|one| self.exec_words(container, one)).collect(),
            _ => Vec::new(),
        }
    }
}

/// Runs docker with `words`, its output going where this program's goes, after the line it runs is
/// said to `say`.
fn docker(words: &[String], say: &mut dyn FnMut(&str)) -> Result<(), String> {
    say(&format!("docker {}", words.join(" ")));
    let status = Command::new("docker").args(words).status().map_err(|error| format!("docker: {error}"))?;
    if status.success() { Ok(()) } else { Err(format!("docker {} ended with {}", words.first().map_or("", String::as_str), status.code().map_or("a signal".into(), |code| format!("exit code {code}")))) }
}

/// Runs docker with `words` and gives what it wrote, trimmed, or None where it failed.
fn docker_read(words: &[&str]) -> Option<String> {
    let done = Command::new("docker").args(words).output().ok()?;
    done.status.success().then(|| String::from_utf8_lossy(&done.stdout).trim().to_string())
}

/// Brings the tree's dev container up: started where it is made and stopped, made where it is not,
/// its commands run as the description says. Gives the address a window opens the tree at in it.
pub fn up(plan: &Plan, say: &mut dyn FnMut(&str)) -> Result<String, String> {
    if let Some(compose) = plan.compose_words() {
        let Source::Compose { service, services, .. } = &plan.source else { unreachable!() };
        let made = docker_read(&[&compose.iter().map(String::as_str).collect::<Vec<_>>()[..], &["ps", "-a", "-q", service.as_str()]].concat()).is_some_and(|id| !id.is_empty());
        let mut words = [compose.clone(), vec!["up".into(), "-d".into()]].concat();
        if !services.is_empty() {
            words.extend(services.iter().cloned());
            if !services.contains(service) {
                words.push(service.clone());
            }
        }
        docker(&words, say)?;
        let id = docker_read(&[&compose.iter().map(String::as_str).collect::<Vec<_>>()[..], &["ps", "-q", service.as_str()]].concat()).filter(|id| !id.is_empty()).ok_or_else(|| format!("docker compose started no {service}"))?;
        let container = id.lines().next().unwrap_or(&id).to_string();
        for command in if made { &plan.on_start } else { &plan.on_create } {
            for words in plan.exec_words(&container, command) {
                docker(&words, say)?;
            }
        }
        return Ok(plan.address(&container));
    }
    match docker_read(&["inspect", "-f", "{{.State.Running}}", &plan.name]).as_deref() {
        Some("true") => {}
        Some(_) => {
            docker(&["start".into(), plan.name.clone()], say)?;
            for command in &plan.on_start {
                for words in plan.exec_words(&plan.name, command) {
                    docker(&words, say)?;
                }
            }
        }
        None => {
            let image = match (&plan.source, plan.build_words()) {
                (Source::Image(image), _) => image.clone(),
                (_, Some((words, image))) => {
                    docker(&words, say)?;
                    image
                }
                _ => return Err("the description names no image".into()),
            };
            docker(&plan.run_words(&image), say)?;
            for command in plan.on_create.iter().chain(&plan.on_start) {
                for words in plan.exec_words(&plan.name, command) {
                    docker(&words, say)?;
                }
            }
        }
    }
    Ok(plan.address(&plan.name))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn tree(description: &str, at: &str) -> PathBuf {
        let dir = std::env::temp_dir().join(format!("orior-devcontainer-{}-{}", std::process::id(), fnv(description.as_bytes())));
        let _ = std::fs::remove_dir_all(&dir);
        let file = dir.join(at);
        std::fs::create_dir_all(file.parent().unwrap()).unwrap();
        std::fs::write(&file, description).unwrap();
        dir
    }

    #[test]
    fn an_image_s_container_mounts_the_tree_and_runs_its_commands_there() {
        let dir = tree("{\n  // the image\n  \"image\": \"python:3.12\",\n  \"containerEnv\": { \"TREE\": \"${containerWorkspaceFolder}\" },\n  \"mounts\": [{ \"source\": \"cache\", \"target\": \"/cache\", \"type\": \"volume\" }],\n  \"runArgs\": [\"--cap-add=SYS_PTRACE\"],\n  \"postCreateCommand\": \"pip install -e .\",\n  \"postStartCommand\": [\"git\", \"status\"],\n}\n", ".devcontainer/devcontainer.json");
        let plan = plan(&dir).unwrap();
        let name = dir.file_name().unwrap().to_string_lossy().into_owned();
        assert_eq!(plan.source, Source::Image("python:3.12".into()));
        assert_eq!(plan.folder, format!("/workspaces/{name}"));
        assert_eq!(plan.env, vec![("TREE".to_string(), format!("/workspaces/{name}"))]);
        let run = plan.run_words("python:3.12");
        assert!(run.windows(2).any(|pair| pair[0] == "--mount" && pair[1] == format!("type=bind,source={},target=/workspaces/{name}", dir.display())), "{run:?}");
        assert!(run.windows(2).any(|pair| pair[0] == "--mount" && pair[1] == "source=cache,target=/cache,type=volume"), "{run:?}");
        assert!(run.contains(&"--cap-add=SYS_PTRACE".to_string()));
        assert_eq!(plan.exec_words(&plan.name, &plan.on_create[0])[0][4..], ["/bin/sh", "-c", "pip install -e ."]);
        assert_eq!(plan.exec_words(&plan.name, &plan.on_start[0])[0][4..], ["git", "status"]);
        assert_eq!(plan.address("c1"), format!("docker:c1:/workspaces/{name}"));
        assert_eq!(name_for(&dir), plan.name);
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_dockerfile_is_built_from_its_context_with_its_args() {
        let dir = tree("{ \"build\": { \"dockerfile\": \"Dockerfile\", \"context\": \"..\", \"args\": { \"V\": \"${localWorkspaceFolderBasename}\" }, \"target\": \"dev\" }, \"workspaceFolder\": \"/src\" }", ".devcontainer.json");
        let plan = plan(&dir).unwrap();
        let (words, image) = plan.build_words().unwrap();
        let name = dir.file_name().unwrap().to_string_lossy().into_owned();
        assert_eq!(image, format!("{}-image", plan.name));
        assert_eq!(words[..2], ["build", "-f"]);
        assert_eq!(PathBuf::from(&words[2]), dir.join("Dockerfile"));
        assert!(words.windows(2).any(|pair| pair[0] == "--build-arg" && pair[1] == format!("V={name}")));
        assert!(words.windows(2).any(|pair| pair[0] == "--target" && pair[1] == "dev"));
        assert_eq!(PathBuf::from(words.last().unwrap()), dir.join(".."));
        assert_eq!(plan.folder, "/src");
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_compose_description_brings_up_its_service() {
        let dir = tree("{ \"dockerComposeFile\": [\"compose.yml\", \"more.yml\"], \"service\": \"app\", \"workspaceFolder\": \"/work\" }", ".devcontainer/devcontainer.json");
        let plan = plan(&dir).unwrap();
        let words = plan.compose_words().unwrap();
        assert_eq!(words[0], "compose");
        assert_eq!(PathBuf::from(&words[2]), dir.join(".devcontainer").join("compose.yml"));
        assert_eq!(words[words.len() - 2..], ["-p".to_string(), plan.name.clone()]);
        assert!(matches!(plan.source, Source::Compose { ref service, .. } if service == "app"));
        let _ = std::fs::remove_dir_all(&dir);
    }

    #[test]
    fn a_description_with_nothing_to_build_says_so() {
        let dir = tree("{ \"name\": \"empty\" }", ".devcontainer/devcontainer.json");
        assert!(plan(&dir).unwrap_err().contains("names no image"));
        let _ = std::fs::remove_dir_all(&dir);
    }
}
