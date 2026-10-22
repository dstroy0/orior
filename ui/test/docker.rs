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

/// The image the window's tests run in, and the one they run in where Docker runs Windows containers.
pub const IMAGE: &str = "orior-test-window";
pub const IMAGE_WINDOWS: &str = "orior-test-window-windows";

/// Whether Docker on this machine runs Windows containers.
pub fn windows_daemon() -> bool {
    Command::new("docker").args(["info", "--format", "{{.OSType}}"]).stdin(Stdio::null()).output().is_ok_and(|out| String::from_utf8_lossy(&out.stdout).trim() == "windows")
}

/// What the Windows container runs: the harness built on this machine, told where the tree stands at
/// C:\host and handed the window built here, driving it through its debugging port, its window's
/// tests alone, with the words `passed` besides. Nothing is built in the container.
pub fn inside_windows(ui: &Path, passed: &[String]) -> String {
    let words: Vec<String> = passed.iter().map(|word| format!("'{}'", word.replace('\'', "''"))).collect();
    format!(
        "$env:ORIOR_TEST_HERE = 'C:\\host\\ui\\test'; $env:ORIOR_TEST_GUI = 'C:\\host\\build\\cargo\\app\\debug\\{}.exe'; \
         & 'C:\\host\\build\\cargo\\release\\{}.exe' --driver cdp --no-servers --no-rust --no-systems {}",
        crate::bin_named(ui, "gui"),
        crate::bin_named(ui, "harness"),
        words.join(" ")
    )
    .trim_end()
    .to_string()
}

/// The words of `docker run` for the Windows container: the tree's top at `top` read and written
/// at C:\host, where the run's reports go, and the servers' settings `env`.
pub fn run_words_windows(ui: &Path, passed: &[String], env: &[(String, String)]) -> Vec<String> {
    let top = ui.parent().unwrap_or(ui);
    let mut words: Vec<String> = ["run", "--rm"].map(String::from).to_vec();
    for (key, value) in env {
        words.push("-e".into());
        words.push(format!("{key}={value}"));
    }
    words.push("--mount".into());
    words.push(format!("type=bind,source={},target=C:\\host", top.display()));
    words.push(IMAGE_WINDOWS.into());
    words.extend(["powershell".to_string(), "-NoProfile".to_string(), "-Command".to_string(), inside_windows(ui, passed)]);
    words
}

/// Builds the Windows image and runs the window's tests in it, the window and the harness built on
/// this machine first, the servers `needed` names started here for the run.
fn run_windows(here: &Path, ui: &Path, passed: &[String], needed: &[&str]) -> ExitCode {
    let Some(top) = ui.parent() else {
        eprintln!("the tree's top is not above {}", ui.display());
        return ExitCode::FAILURE;
    };
    println!("the window's tests in a Windows container, image {IMAGE_WINDOWS}");
    let built = Command::new("docker").args(["build", "-t", IMAGE_WINDOWS]).arg(here.join("docker").join("windows")).stdin(Stdio::null()).status();
    if !built.is_ok_and(|status| status.success()) {
        eprintln!("the image {IMAGE_WINDOWS} did not build");
        return ExitCode::FAILURE;
    }
    if let Err(said) = crate::window::build(ui, &top.join("build").join("cargo").join("app")) {
        eprintln!("{said}");
        return ExitCode::FAILURE;
    }
    let up = crate::servers::start(here, needed);
    for said in &up.said {
        println!("  {said}");
    }
    let mut env = crate::systems::seen_from_a_container(&up.env);
    let names: Vec<String> = env.iter().map(|(key, _)| key.clone()).collect();
    env.push(("ORIOR_TEST_SERVERS".into(), names.join(",")));
    let ran = Command::new("docker").args(run_words_windows(ui, passed, &env)).stdin(Stdio::null()).status();
    crate::servers::stop(&up.started);
    match ran {
        Ok(status) if status.success() => ExitCode::SUCCESS,
        Ok(_) => ExitCode::FAILURE,
        Err(error) => {
            eprintln!("docker: {error}");
            ExitCode::FAILURE
        }
    }
}

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
        "rsync -a --checksum --no-times --delete --exclude /test/target --exclude /test/runs --exclude /cli/target --exclude /gui/src-tauri/target --exclude __pycache__ /host/ui/ /ui/ \
         && dbus-run-session -- xvfb-run -a -s '-screen 0 1920x1080x24 -dpi 96' orior-watch cargo run --manifest-path /ui/test/Cargo.toml --release -- --driver webdriver --no-servers --no-rust --no-systems {}",
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
    if let Some(cu) = ui.parent().map(|top| top.join("src").join("cu")).filter(|cu| cu.is_dir()) {
        mount(cu.display().to_string(), "/src/cu", true);
    }
    mount(ui.join("test").join("runs").display().to_string(), "/ui/test/runs", false);
    if let Some(keys) = keys {
        mount(keys.display().to_string(), "/host/ssh", true);
    }
    for (volume, target) in [("orior-test-window-tree", "/ui"), ("orior-test-window-cargo", "/root/.cargo/registry")] {
        words.push("-v".into());
        words.push(format!("{volume}:{target}"));
    }
    words.push(IMAGE.into());
    words.extend(["sh".to_string(), "-c".to_string(), inside(passed)]);
    words
}

/// Builds the image and runs the window's tests in it, the words `passed` given to the harness
/// there, and gives its exit.
/// Runs the window's tests in the image with the words `passed`, the servers `needed` names started
/// on this machine for the run and taken away after it.
pub fn run(here: &Path, ui: &Path, passed: &[String], needed: &[&str]) -> ExitCode {
    let _ = std::fs::create_dir_all(here.join("runs"));
    if windows_daemon() {
        return run_windows(here, ui, passed, needed);
    }
    println!("the window's tests in Docker, image {IMAGE}; the window shows at {WATCH} as they run");
    let built = Command::new("docker").args(["build", "-t", IMAGE]).arg(here.join("docker")).stdin(Stdio::null()).status();
    if !built.is_ok_and(|status| status.success()) {
        eprintln!("the image {IMAGE} did not build");
        return ExitCode::FAILURE;
    }
    let up = crate::servers::start(here, needed);
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
    let ran = Command::new("docker").args(run_words(ui, passed, &env, ssh.map(|_| here.join("target").join("ssh")).as_deref())).stdin(Stdio::null()).status();
    crate::servers::stop(&up.started);
    match ran {
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
    fn the_windows_container_runs_the_harness_built_here_on_the_window_built_here() {
        let said = inside_windows(Path::new("D:\\repo\\ui"), &["--only".into(), "sides".into()]);
        assert!(said.starts_with("$env:ORIOR_TEST_HERE = 'C:\\host\\ui\\test'; $env:ORIOR_TEST_GUI = 'C:\\host\\build\\cargo\\app\\debug\\orior_gui.exe'; "), "{said}");
        assert!(said.ends_with("orior_harness.exe' --driver cdp --no-servers --no-rust --no-systems '--only' 'sides'"), "{said}");
        let words = run_words_windows(Path::new("D:\\repo\\ui"), &[], &[("ORIOR_TEST_PG".into(), "host.docker.internal:5432".into())]);
        let joined = words.join(" ");
        assert!(joined.contains("type=bind,source=D:\\repo,target=C:\\host"), "{joined}");
        assert!(joined.contains("-e ORIOR_TEST_PG=host.docker.internal:5432"), "{joined}");
        assert!(joined.contains(&format!("{IMAGE_WINDOWS} powershell -NoProfile -Command")), "{joined}");
    }

    #[test]
    fn the_tree_here_is_read_only_and_the_runs_come_back() {
        let ui = Path::new("/repo/ui");
        let words = run_words(ui, &[], &[("ORIOR_TEST_PG".into(), "host.docker.internal:5432".into())], Some(&ui.join("keys")));
        let joined = words.join(" ");
        assert!(joined.contains(&format!("type=bind,source={},target=/host/ui,readonly", ui.display())), "{joined}");
        assert!(joined.contains(&format!("type=bind,source={},target=/ui/test/runs ", ui.join("test").join("runs").display())), "{joined}");
        assert!(joined.contains("-v orior-test-window-tree:/ui"), "{joined}");
        assert!(joined.contains("--add-host host.docker.internal:host-gateway"), "{joined}");
        assert!(joined.contains("-p 127.0.0.1:6080:6080"), "the window is shown on this machine alone: {joined}");
        assert!(joined.contains(&format!("type=bind,source={},target=/host/ssh,readonly", ui.join("keys").display())), "{joined}");
        assert!(joined.contains("-e ORIOR_TEST_PG=host.docker.internal:5432"), "{joined}");
        assert_eq!(words[words.len() - 3..words.len() - 1], ["sh".to_string(), "-c".to_string()]);
        assert!(words.iter().position(|word| word == IMAGE) < words.iter().position(|word| word == "sh"));
    }
}
