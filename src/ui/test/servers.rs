// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The servers orior's tests run against, each a Docker container of the harness's own, named
//! orior-test-*, reached on this machine's loopback alone at a port Docker chooses: PostgreSQL,
//! MySQL, MariaDB, and the print server servers/cups builds, CUPS with three IPP Everywhere printers.
//! A container is made once and kept running between runs; --stop-servers takes them away. What
//! each server is and where it listens goes to the tests as ORIOR_* variables.

use std::path::Path;
use std::process::{Command, Stdio};
use std::time::{Duration, Instant};

/// The tests' password for each database server, which answers on the loopback alone.
pub const PASSWORD: &str = "orior-test";

/// A container: its name, its image, its ports, its settings, and its own arguments.
struct Container {
    name: &'static str,
    image: &'static str,
    ports: &'static [u16],
    env: &'static [(&'static str, &'static str)],
    args: &'static [&'static str],
}

const CONTAINERS: &[Container] = &[
    Container { name: "orior-test-postgres", image: "postgres:17-alpine", ports: &[5432], env: &[("POSTGRES_PASSWORD", PASSWORD)], args: &[] },
    Container { name: "orior-test-mysql", image: "mysql:8.4", ports: &[3306], env: &[("MYSQL_ROOT_PASSWORD", PASSWORD)], args: &["--mysql-native-password=ON"] },
    Container { name: "orior-test-mariadb", image: "mariadb:11", ports: &[3306], env: &[("MARIADB_ROOT_PASSWORD", PASSWORD)], args: &[] },
    Container { name: "orior-test-cups", image: "orior-test-cups", ports: &[631, 8631, 8632, 8633], env: &[], args: &[] },
];

fn docker(args: &[&str]) -> Result<String, String> {
    let done = Command::new("docker").args(args).stdin(Stdio::null()).output().map_err(|error| format!("docker: {error}"))?;
    if done.status.success() {
        Ok(String::from_utf8_lossy(&done.stdout).trim().to_string())
    } else {
        Err(String::from_utf8_lossy(&done.stderr).trim().to_string())
    }
}

/// Whether Docker answers.
pub fn docker_runs() -> bool {
    docker(&["info", "--format", "{{.ServerVersion}}"]).is_ok_and(|said| !said.is_empty())
}

/// The port on this machine a container's `port` is reached at.
fn published(name: &str, port: u16) -> Option<u16> {
    port_of(&docker(&["port", name, &format!("{port}/tcp")]).ok()?)
}

/// The first port docker port gives, of lines as 127.0.0.1:55001 or [::]:55001.
fn port_of(said: &str) -> Option<u16> {
    said.lines().filter_map(|line| line.rsplit(':').next()?.trim().parse().ok()).next()
}

fn reachable(port: u16) -> bool {
    std::net::TcpStream::connect_timeout(&([127, 0, 0, 1], port).into(), Duration::from_millis(300)).is_ok()
}

/// Makes `container` run, building or pulling its image where this machine lacks it; one running
/// with a port not reached from this machine is made again.
fn bring_up(container: &Container, here: &Path) -> Result<(), String> {
    let running = !docker(&["ps", "-q", "-f", &format!("name=^{}$", container.name)])?.is_empty();
    if !running || container.ports.iter().any(|port| published(container.name, *port).is_none()) {
        let _ = docker(&["rm", "-f", container.name]);
        if docker(&["image", "inspect", container.image]).is_err() {
            if container.image == "orior-test-cups" {
                let folder = here.join("servers").join("cups");
                docker(&["build", "-t", "orior-test-cups", &folder.display().to_string()]).map_err(|error| format!("building the print server: {error}"))?;
            } else {
                docker(&["pull", container.image]).map_err(|error| format!("pulling {}: {error}", container.image))?;
            }
        }
        let mut args: Vec<String> = vec!["run".into(), "-d".into(), "--name".into(), container.name.into()];
        for port in container.ports {
            args.push("-p".into());
            args.push(format!("127.0.0.1::{port}"));
        }
        for (key, value) in container.env {
            args.push("-e".into());
            args.push(format!("{key}={value}"));
        }
        args.push(container.image.into());
        args.extend(container.args.iter().map(|one| one.to_string()));
        docker(&args.iter().map(String::as_str).collect::<Vec<_>>())?;
    }
    Ok(())
}

/// Waits until `ready` says the server answers, within `limit`.
fn wait_for(limit: Duration, ready: impl Fn() -> bool) -> bool {
    let until = Instant::now() + limit;
    while Instant::now() < until {
        if ready() {
            return true;
        }
        std::thread::sleep(Duration::from_millis(500));
    }
    false
}

/// The servers up, and what the tests are told of them; each server that could not be brought up
/// is said, and its tests run without it.
pub struct Up {
    pub env: Vec<(String, String)>,
    pub said: Vec<String>,
}

pub fn start(here: &Path) -> Up {
    let mut up = Up { env: Vec::new(), said: Vec::new() };
    if !docker_runs() {
        up.said.push("Docker does not answer: the database and printing tests run without their servers. Docker is in File, Toolchains.".into());
        return up;
    }
    for container in CONTAINERS {
        if let Err(error) = bring_up(container, here) {
            up.said.push(format!("{} did not come up: {error}", container.name));
            continue;
        }
        let first = published(container.name, container.ports[0]);
        let Some(port) = first.filter(|port| wait_for(Duration::from_secs(60), || reachable(*port))) else {
            up.said.push(format!("{} does not answer on its port", container.name));
            continue;
        };
        let address = format!("127.0.0.1:{port}");
        match container.name {
            "orior-test-postgres" => {
                let ready = wait_for(Duration::from_secs(90), || docker(&["exec", container.name, "pg_isready", "-U", "postgres"]).is_ok());
                // The roles the tests sign in as by md5 and in the clear, ahead of the image's lines.
                let lines = "host all orior_md5 all md5\\nhost all orior_plain all password";
                let hba = format!("f=\"$(psql -U postgres -Atc 'show hba_file')\"; grep -q orior_md5 \"$f\" || sed -i '1i {lines}' \"$f\"; psql -U postgres -c 'select pg_reload_conf()' >/dev/null");
                if !ready || docker(&["exec", container.name, "sh", "-c", &hba]).is_err() {
                    up.said.push("PostgreSQL did not take the test's sign-in lines".into());
                    continue;
                }
                up.env.push(("ORIOR_PG_ADDRESS".into(), address));
                up.env.push(("ORIOR_PG_PASSWORD".into(), PASSWORD.into()));
            }
            "orior-test-mysql" | "orior-test-mariadb" => {
                let client = if container.name == "orior-test-mysql" { "mysql" } else { "mariadb" };
                let ready = wait_for(Duration::from_secs(120), || docker(&["exec", container.name, client, "-uroot", &format!("-p{PASSWORD}"), "-e", "SELECT 1"]).is_ok());
                if !ready {
                    up.said.push(format!("{} did not take sign-ins", container.name));
                    continue;
                }
                let prefix = if container.name == "orior-test-mysql" { "ORIOR_MYSQL" } else { "ORIOR_MARIADB" };
                up.env.push((format!("{prefix}_ADDRESS"), address));
                up.env.push((format!("{prefix}_PASSWORD"), PASSWORD.into()));
            }
            "orior-test-cups" => {
                let ready = wait_for(Duration::from_secs(60), || docker(&["logs", container.name]).is_ok_and(|said| said.contains("printers ready")) || docker(&["exec", container.name, "lpstat", "-p", "office"]).is_ok());
                let ports: Vec<Option<u16>> = [8631, 8632, 8633].iter().map(|port| published(container.name, *port)).collect();
                if !ready || ports.iter().any(Option::is_none) {
                    up.said.push("the print server's printers did not come up".into());
                    continue;
                }
                up.env.push(("ORIOR_CUPS".into(), address));
                for (name, port) in ["PDF", "PWG", "URF"].iter().zip(ports.into_iter().flatten()) {
                    up.env.push((format!("ORIOR_IPP_{name}"), format!("ipp://127.0.0.1:{port}/ipp/print")));
                }
                up.env.push(("ORIOR_CUPS_CONTAINER".into(), container.name.into()));
            }
            _ => {}
        }
    }
    up
}

/// Takes each test server away.
pub fn stop() {
    for container in CONTAINERS {
        let _ = docker(&["rm", "-f", container.name]);
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_port_docker_gives_is_read_from_each_form_it_takes() {
        assert_eq!(port_of("127.0.0.1:55001\n"), Some(55001));
        assert_eq!(port_of("0.0.0.0:49153\n[::]:49153"), Some(49153));
        assert_eq!(port_of("[::1]:50000"), Some(50000));
        assert_eq!(port_of(""), None);
    }

    #[test]
    fn each_server_is_the_harness_s_own_and_has_a_port() {
        for container in CONTAINERS {
            assert!(container.name.starts_with("orior-test-"), "{}", container.name);
            assert!(!container.ports.is_empty(), "{}", container.name);
        }
    }

    #[test]
    fn the_print_server_s_files_are_there_to_build_it() {
        let here = Path::new(env!("CARGO_MANIFEST_DIR")).join("servers").join("cups");
        assert!(here.join("Dockerfile").is_file());
        let start = std::fs::read_to_string(here.join("start.sh")).unwrap();
        for port in ["8631", "8632", "8633"] {
            assert!(start.contains(port), "start.sh starts no printer on {port}");
        }
        assert!(start.contains("printers ready"));
    }
}
