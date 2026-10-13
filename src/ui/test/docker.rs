// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The window's tests in a Docker container of the harness's own, on this machine: the image of
//! test/docker/ built where Docker has none or its file has changed, the tree brought into a volume
//! of the container's own with every build it makes, and the harness run there on a display that
//! draws in memory, driving the window through WebDriver. The test servers run on this machine, as
//! servers.rs starts them, and the container is told where each listens. Each run's report comes
//! back to test/runs/ here. The tree on this machine is only read.

use std::path::Path;
use std::process::{Command, ExitCode, Stdio};

/// The image the window's tests run in.
pub const IMAGE: &str = "orior-test-window";

/// Where a browser on this machine shows the window as the tests drive it.
pub const WATCH: &str = "http://localhost:6080/vnc.html?autoconnect=true&resize=scale";

/// What the container runs: the tree brought in from /host/ui, less what is built there and the
/// runs kept, then the harness, its window's tests alone, with the words `passed` besides. A file is
/// brought in where its bytes differ, and takes the container's own time as it comes: the host's
/// clock and the container's differ, and a file keeping the host's time can stand older than the
/// last build of it, which cargo then keeps. X's
/// display in memory is given 96 dots an inch, at which WebKitWebDriver puts each press where it
/// is sent; at another it puts each one off by the scale between the two.
pub fn inside(passed: &[String]) -> String {
    let words: Vec<String> = passed.iter().map(|word| format!("'{}'", word.replace('\'', "'\\''"))).collect();
    format!(
        "rsync -a --checksum --no-times --delete --exclude /test/target --exclude /test/runs --exclude /cli/target --exclude /src-tauri/target --exclude __pycache__ /host/ui/ /src/ui/ \
         && dbus-run-session -- xvfb-run -a -s '-screen 0 1920x1080x24 -dpi 96' orior-watch cargo run --manifest-path /src/ui/test/Cargo.toml --release -- --driver webdriver --no-servers --no-rust --no-systems {}",
        words.join(" ")
    )
    .trim_end()
    .to_string()
}

/// The words of `docker run` for the container: the tree at `ui` read at /host/ui, src/cu beside
/// it at /src/cu for the query writer, the volumes that keep its tree, builds and downloads
/// between runs, test/runs/ here where its reports go, the servers' settings `env`, as a
/// container reaches this machine, and the ssh key's folder `keys` read at /host/ssh where the ssh
/// server came up.
pub fn run_words(ui: &Path, passed: &[String], env: &[(String, String)], keys: Option<&Path>) -> Vec<String> {
    let mut words: Vec<String> = ["run", "--rm", "--init", "--shm-size=1g", "--add-host", "host.docker.internal:host-gateway", "-p", "127.0.0.1:6080:6080"].map(String::from).to_vec();
    for (key, value) in env {
        words.push("-e".into());
        words.push(format!("{key}={value}"));
    }
    let mut mount = |source: String, target: &str, read_only: bool| {
        words.push("--mount".into());
        words.push(format!("type=bind,source={source},target={target}{}", if read_only { ",readonly" } else { "" }));
    };
    mount(ui.display().to_string(), "/host/ui", true);
    if let Some(cu) = ui.parent().map(|src| src.join("cu")).filter(|cu| cu.is_dir()) {
        mount(cu.display().to_string(), "/src/cu", true);
    }
    mount(ui.join("test").join("runs").display().to_string(), "/src/ui/test/runs", false);
    if let Some(keys) = keys {
        mount(keys.display().to_string(), "/host/ssh", true);
    }
    for (volume, target) in [("orior-test-window-tree", "/src/ui"), ("orior-test-window-cargo", "/root/.cargo/registry")] {
        words.push("-v".into());
        words.push(format!("{volume}:{target}"));
    }
    words.push(IMAGE.into());
    words.extend(["sh".to_string(), "-c".to_string(), inside(passed)]);
    words
}

/// Builds the image and runs the window's tests in it, the words `passed` given to the harness
/// there, and gives its exit.
pub fn run(here: &Path, ui: &Path, passed: &[String]) -> ExitCode {
    let _ = std::fs::create_dir_all(here.join("runs"));
    println!("the window's tests in Docker, image {IMAGE}; the window shows at {WATCH} as they run");
    let built = Command::new("docker").args(["build", "-t", IMAGE]).arg(here.join("docker")).stdin(Stdio::null()).status();
    if !built.is_ok_and(|status| status.success()) {
        eprintln!("the image {IMAGE} did not build");
        return ExitCode::FAILURE;
    }
    let up = crate::servers::start(here);
    for said in &up.said {
        println!("  {said}");
    }
    let mut env = crate::systems::seen_from_a_container(&up.env);
    // The ssh server's port, where it came up: the harness there reaches it with a config of its own.
    let ssh = up.env.iter().any(|(key, _)| key == "ORIOR_TEST_SSH").then(|| std::fs::read_to_string(here.join("target").join("ssh").join("config")).ok()).flatten().and_then(|config| crate::servers::ssh_port(&config));
    if let Some(port) = ssh {
        env.push(("ORIOR_TEST_SSH_PORT".into(), port.to_string()));
    }
    // The names of the settings handed, by which the harness there reads them.
    let names: Vec<String> = env.iter().map(|(key, _)| key.clone()).collect();
    env.push(("ORIOR_TEST_SERVERS".into(), names.join(",")));
    match Command::new("docker").args(run_words(ui, passed, &env, ssh.map(|_| here.join("target").join("ssh")).as_deref())).stdin(Stdio::null()).status() {
        Ok(status) if status.success() => ExitCode::SUCCESS,
        Ok(_) => ExitCode::FAILURE,
        Err(error) => {
            eprintln!("docker: {error}");
            ExitCode::FAILURE
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn the_container_brings_the_tree_in_and_drives_the_window_alone() {
        let said = inside(&["--only".into(), "editor/view".into()]);
        assert!(said.starts_with("rsync -a --checksum --no-times --delete "), "{said}");
        assert!(said.contains("--exclude /test/runs"), "{said}");
        assert!(said.contains("xvfb-run -a -s '-screen 0 1920x1080x24 -dpi 96' orior-watch cargo run"), "{said}");
        assert!(said.ends_with("--driver webdriver --no-servers --no-rust --no-systems '--only' 'editor/view'"), "{said}");
    }

    #[test]
    fn the_tree_here_is_read_only_and_the_runs_come_back() {
        let ui = Path::new("/repo/src/ui");
        let words = run_words(ui, &[], &[("ORIOR_TEST_PG".into(), "host.docker.internal:5432".into())], Some(&ui.join("keys")));
        let joined = words.join(" ");
        assert!(joined.contains(&format!("type=bind,source={},target=/host/ui,readonly", ui.display())), "{joined}");
        assert!(joined.contains(&format!("type=bind,source={},target=/src/ui/test/runs ", ui.join("test").join("runs").display())), "{joined}");
        assert!(joined.contains("-v orior-test-window-tree:/src/ui"), "{joined}");
        assert!(joined.contains("--add-host host.docker.internal:host-gateway"), "{joined}");
        assert!(joined.contains("-p 127.0.0.1:6080:6080"), "the window is shown on this machine alone: {joined}");
        assert!(joined.contains(&format!("type=bind,source={},target=/host/ssh,readonly", ui.join("keys").display())), "{joined}");
        assert!(joined.contains("-e ORIOR_TEST_PG=host.docker.internal:5432"), "{joined}");
        assert_eq!(words[words.len() - 3..words.len() - 1], ["sh".to_string(), "-c".to_string()]);
        assert!(words.iter().position(|word| word == IMAGE) < words.iter().position(|word| word == "sh"));
    }
}
