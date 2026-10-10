// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The machine's containers, as docker and kubectl say: Docker's containers, each with the Compose
//! project and service it belongs to where it belongs to one, started, stopped and restarted, its logs
//! read and a command run in it; and a Kubernetes cluster's pods, each with its containers, their logs
//! read. A list that cannot be read says why in place of its items, as docker's engine not running
//! or kubectl naming no cluster.
//!
//! ORIOR_DOCKER and ORIOR_KUBECTL name other programs, with words of their own before docker's and
//! kubectl's.

use std::process::{Command, Stdio};

use serde::Serialize;
use serde_json::Value;

/// How many lines of a container's or a pod's log are read, its last.
pub const LOG_LINES: u32 = 500;

#[derive(Serialize, Debug, PartialEq)]
pub struct Container {
    pub id: String,
    pub name: String,
    pub image: String,
    /// running, exited, paused, created, restarting or dead.
    pub state: String,
    /// docker's own words for how long it has been in its state.
    pub status: String,
    pub project: Option<String>,
    pub service: Option<String>,
    pub ports: String,
}

#[derive(Serialize, Debug, PartialEq)]
pub struct Pod {
    pub namespace: String,
    pub name: String,
    /// Pending, Running, Succeeded, Failed or Unknown.
    pub phase: String,
    /// How many of its containers are ready, of how many.
    pub ready: String,
    pub containers: Vec<String>,
    pub node: String,
}

/// A list, or why it could not be read.
#[derive(Serialize, Debug)]
pub struct Listed<T> {
    pub items: Vec<T>,
    pub problem: Option<String>,
}

/// The program `variable` names, with its own words, or `program`.
fn program_of(variable: &str, program: &str) -> (String, Vec<String>) {
    let mut words = crate::runner::program_words(variable).unwrap_or_else(|| vec![program.to_string()]);
    let first = words.remove(0);
    (first, words)
}

/// Runs docker or kubectl with `args`, and gives what it wrote, or what it said where it failed, its
/// first line where it said many.
fn run(variable: &str, program: &str, args: &[&str]) -> Result<String, String> {
    let (first, mut words) = program_of(variable, program);
    words.extend(args.iter().map(|one| one.to_string()));
    let mut command = Command::new(&first);
    command.args(&words).stdin(Stdio::null()).stdout(Stdio::piped()).stderr(Stdio::piped());
    crate::runner::quiet(&mut command);
    let done = command.output().map_err(|error| format!("{program} is not found: {error}"))?;
    if !done.status.success() {
        let said = String::from_utf8_lossy(&done.stderr);
        let line = said.lines().map(str::trim).find(|line| !line.is_empty()).unwrap_or("").to_string();
        return Err(if line.is_empty() { format!("{program} {} failed", args.first().copied().unwrap_or("")) } else { line });
    }
    Ok(String::from_utf8_lossy(&done.stdout).into_owned())
}

fn docker(args: &[&str]) -> Result<String, String> {
    run("ORIOR_DOCKER", "docker", args)
}

fn kubectl(args: &[&str]) -> Result<String, String> {
    run("ORIOR_KUBECTL", "kubectl", args)
}

/// The containers `docker ps -a --format '{{json .}}'` lists, one on each line.
pub fn parse_containers(text: &str) -> Vec<Container> {
    let mut found: Vec<Container> = text
        .lines()
        .filter_map(|line| serde_json::from_str::<Value>(line).ok())
        .map(|one| {
            let labels: Vec<(String, String)> = one["Labels"].as_str().unwrap_or("").split(',').filter_map(|pair| pair.split_once('=')).map(|(key, value)| (key.to_string(), value.to_string())).collect();
            let label = |key: &str| labels.iter().find(|(name, _)| name == key).map(|(_, value)| value.clone());
            let text = |key: &str| one[key].as_str().unwrap_or("").to_string();
            Container { id: text("ID"), name: text("Names"), image: text("Image"), state: text("State"), status: text("Status"), project: label("com.docker.compose.project"), service: label("com.docker.compose.service"), ports: text("Ports") }
        })
        .collect();
    found.sort_by(|a, b| (&a.project, &a.service, &a.name).cmp(&(&b.project, &b.service, &b.name)));
    found
}

/// The pods `kubectl get pods -A -o json` lists.
pub fn parse_pods(text: &str) -> Vec<Pod> {
    let Ok(read) = serde_json::from_str::<Value>(text) else { return Vec::new() };
    let mut found: Vec<Pod> = read["items"]
        .as_array()
        .into_iter()
        .flatten()
        .map(|one| {
            let containers: Vec<String> = one["spec"]["containers"].as_array().into_iter().flatten().filter_map(|one| one["name"].as_str().map(String::from)).collect();
            let statuses = one["status"]["containerStatuses"].as_array().cloned().unwrap_or_default();
            let ready = statuses.iter().filter(|one| one["ready"] == true).count();
            Pod {
                namespace: one["metadata"]["namespace"].as_str().unwrap_or("default").to_string(),
                name: one["metadata"]["name"].as_str().unwrap_or("").to_string(),
                phase: one["status"]["phase"].as_str().unwrap_or("Unknown").to_string(),
                ready: format!("{ready}/{}", containers.len()),
                node: one["spec"]["nodeName"].as_str().unwrap_or("").to_string(),
                containers,
            }
        })
        .collect();
    found.sort_by(|a, b| (&a.namespace, &a.name).cmp(&(&b.namespace, &b.name)));
    found
}

/// Every container docker knows, running or not.
pub fn containers() -> Listed<Container> {
    match docker(&["ps", "-a", "--no-trunc", "--format", "{{json .}}"]) {
        Ok(text) => Listed { items: parse_containers(&text), problem: None },
        Err(problem) => Listed { items: Vec::new(), problem: Some(problem) },
    }
}

/// Starts, stops or restarts the container `name`, as `act` says.
pub fn act(name: &str, act: &str) -> Result<(), String> {
    if !["start", "stop", "restart"].contains(&act) {
        return Err(format!("{act} is not something a container is"));
    }
    docker(&[act, name]).map(|_| ())
}

/// The last LOG_LINES lines the container `name` wrote, its output and its errors as they came.
pub fn logs(name: &str) -> Result<String, String> {
    let lines = LOG_LINES.to_string();
    let (first, mut words) = program_of("ORIOR_DOCKER", "docker");
    words.extend(["logs", "--tail", &lines, name].map(String::from));
    let mut command = Command::new(&first);
    command.args(&words).stdin(Stdio::null());
    crate::runner::quiet(&mut command);
    let done = command.output().map_err(|error| format!("docker is not found: {error}"))?;
    let mut text = String::from_utf8_lossy(&done.stdout).into_owned();
    text.push_str(&String::from_utf8_lossy(&done.stderr));
    if !done.status.success() {
        return Err(text.lines().next().unwrap_or("docker logs failed").to_string());
    }
    Ok(text)
}

/// Runs `line` in the container `name` by its shell, and gives what it wrote, its errors after.
pub fn run_in(name: &str, line: &str) -> Result<String, String> {
    let (first, mut words) = program_of("ORIOR_DOCKER", "docker");
    words.extend(["exec", name, "sh", "-c", line].map(String::from));
    let mut command = Command::new(&first);
    command.args(&words).stdin(Stdio::null());
    crate::runner::quiet(&mut command);
    let done = command.output().map_err(|error| format!("docker is not found: {error}"))?;
    let mut text = String::from_utf8_lossy(&done.stdout).into_owned();
    text.push_str(&String::from_utf8_lossy(&done.stderr));
    match done.status.code() {
        Some(0) => Ok(text),
        Some(code) => Ok(format!("{text}\nexit code {code}")),
        None => Err(text),
    }
}

/// Every pod of the cluster kubectl names, in every namespace.
pub fn pods() -> Listed<Pod> {
    match kubectl(&["get", "pods", "-A", "-o", "json", "--request-timeout=5s"]) {
        Ok(text) => Listed { items: parse_pods(&text), problem: None },
        Err(problem) => Listed { items: Vec::new(), problem: Some(problem) },
    }
}

/// The last LOG_LINES lines the container `container` of the pod `name` in `namespace` wrote, or its
/// only container's where none is named.
pub fn pod_logs(namespace: &str, name: &str, container: Option<&str>) -> Result<String, String> {
    let lines = LOG_LINES.to_string();
    let mut args = vec!["logs", "-n", namespace, name, "--tail", lines.as_str()];
    if let Some(container) = container.filter(|one| !one.is_empty()) {
        args.extend(["-c", container]);
    }
    kubectl(&args)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn docker_s_list_gives_each_container_its_compose_project_and_service() {
        let text = concat!(
            r#"{"ID":"b2","Image":"redis:7","Labels":"com.docker.compose.project=shop,com.docker.compose.service=cache","Names":"shop-cache-1","Ports":"6379/tcp","State":"running","Status":"Up 2 hours"}"#,
            "\n",
            r#"{"ID":"a1","Image":"python:3.12-slim","Labels":"","Names":"loose","Ports":"","State":"exited","Status":"Exited (0) 3 minutes ago"}"#,
            "\n"
        );
        let found = parse_containers(text);
        assert_eq!(found.len(), 2);
        assert_eq!(found[0].name, "loose");
        assert_eq!(found[0].project, None);
        assert_eq!(found[1].project.as_deref(), Some("shop"));
        assert_eq!(found[1].service.as_deref(), Some("cache"));
        assert_eq!(found[1].state, "running");
    }

    #[test]
    fn kubectl_s_list_gives_each_pod_its_containers_and_how_many_are_ready() {
        let text = r#"{"items":[{"metadata":{"name":"web-7f","namespace":"shop"},"spec":{"nodeName":"n1","containers":[{"name":"web"},{"name":"proxy"}]},"status":{"phase":"Running","containerStatuses":[{"ready":true},{"ready":false}]}},{"metadata":{"name":"dns","namespace":"kube-system"},"spec":{"containers":[{"name":"coredns"}]},"status":{"phase":"Running","containerStatuses":[{"ready":true}]}}]}"#;
        let found = parse_pods(text);
        assert_eq!(found[0].namespace, "kube-system");
        assert_eq!(found[1], Pod { namespace: "shop".into(), name: "web-7f".into(), phase: "Running".into(), ready: "1/2".into(), containers: vec!["web".into(), "proxy".into()], node: "n1".into() });
    }

    #[test]
    fn a_container_is_not_asked_to_do_what_it_does_not() {
        assert!(act("loose", "remove").is_err());
    }
}
