// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The systems orior's tests run on besides this machine's own: Debian, Ubuntu, Fedora, Alpine and
//! Arch Linux, and Debian again on aarch64, each a Docker image of test/systems/, built once and kept.
//! The command line's crate's tests run in each, the tree mounted read-only, each
//! system's downloads and builds kept in volumes of its own, and the test servers reached at the
//! address Docker gives this machine. Docker runs aarch64 through the emulator it carries, slower
//! than the machine's own.
//!
//! A Mac's system runs on Apple's hardware alone by Apple's license, and Windows on Arm on a machine
//! of its own: for them the crate is built for each, and its tests run where those machines are.

use std::path::Path;
use std::process::{Command, Stdio};

use crate::Outcome;

/// Each system: its name, its image's folder under test/systems, and the platform it runs on.
pub const SYSTEMS: &[(&str, &str, &str)] = &[
    ("debian", "debian", "linux/amd64"),
    ("ubuntu", "ubuntu", "linux/amd64"),
    ("fedora", "fedora", "linux/amd64"),
    ("alpine", "alpine", "linux/amd64"),
    ("arch", "arch", "linux/amd64"),
    ("debian-arm64", "debian", "linux/arm64"),
];

/// The longest an image's build is waited for: a build whose download stops answering is stopped and
/// said, and the run goes on to the next system.
const BUILD_LIMIT: std::time::Duration = std::time::Duration::from_secs(30 * 60);

/// The targets the crate is built for where its tests cannot run here.
pub const BUILT_ONLY: &[(&str, &str)] = &[("macos-arm64", "aarch64-apple-darwin"), ("macos-x86-64", "x86_64-apple-darwin"), ("windows-arm64", "aarch64-pc-windows-msvc")];

/// The test servers' settings as a system sees them: an address on this machine's loopback is
/// reached at Docker's name for this machine, and the harness's ssh, whose config names this
/// machine's paths, is left out.
pub fn seen_from_a_container(env: &[(String, String)]) -> Vec<(String, String)> {
    env.iter()
        .filter(|(key, _)| !key.starts_with("ORIOR_TEST_SSH") && key != "ORIOR_CUPS_CONTAINER")
        .map(|(key, value)| (key.clone(), value.replace("127.0.0.1", "host.docker.internal")))
        .collect()
}

/// The command that runs the crate's tests in the system `name`, whose image is built from `folder`
/// for `platform`; the image is built first where it is not there.
pub fn tests_in(name: &str, folder: &str, platform: &str, here: &Path, ui: &Path, env: &[(String, String)]) -> Result<Command, String> {
    let image = format!("orior-test-system-{name}");
    let have = Command::new("docker").args(["image", "inspect", &image]).stdout(Stdio::null()).stderr(Stdio::null()).status().is_ok_and(|status| status.success());
    if !have {
        println!("    building {image}");
        let mut building = Command::new("docker").args(["build", "--platform", platform, "-t", &image]).arg(here.join("systems").join(folder)).stdin(Stdio::null()).spawn().map_err(|error| format!("docker: {error}"))?;
        let until = std::time::Instant::now() + BUILD_LIMIT;
        let built = loop {
            match building.try_wait().map_err(|error| format!("docker: {error}"))? {
                Some(status) => break status.success(),
                None if std::time::Instant::now() >= until => {
                    let _ = building.kill();
                    let _ = building.wait();
                    return Err(format!("the image {image} was still building after {} minutes, and was stopped", BUILD_LIMIT.as_secs() / 60));
                }
                None => std::thread::sleep(std::time::Duration::from_millis(500)),
            }
        };
        if !built {
            return Err(format!("the image {image} did not build"));
        }
    }
    let mut command = Command::new("docker");
    // An init of Docker's own is the container's first process and gathers each process that ends,
    // as a system's own init does.
    command.args(["run", "--rm", "--init", "--platform", platform, "--add-host", "host.docker.internal:host-gateway"]);
    command.arg("--mount").arg(format!("type=bind,source={},target=/ui,readonly", ui.display()));
    // src/cu, two folders up from the crate, where the crate's tests of qry.rs build the query runs'
    // writer from its sources and the headers they include.
    if let Some(cu) = ui.parent().map(|src| src.join("cu")).filter(|cu| cu.is_dir()) {
        command.arg("--mount").arg(format!("type=bind,source={},target=/cu,readonly", cu.display()));
    }
    command.args(["-v", &format!("orior-test-{name}-cargo:/root/.cargo/registry"), "-v", &format!("orior-test-{name}-target:/target")]);
    // Docker's own resolver drops a name now and then; cargo asks again ten times before it gives up.
    command.args(["-e", "CARGO_TARGET_DIR=/target", "-e", "CARGO_NET_RETRY=10"]);
    for (key, value) in seen_from_a_container(env) {
        command.arg("-e").arg(format!("{key}={value}"));
    }
    // The crates are fetched first, three times at most and ten seconds apart, and the tests run
    // offline: a name Docker's resolver drops for a moment fails no test.
    let fetched = "for try in 1 2 3; do cargo fetch --manifest-path /ui/cli/Cargo.toml && break; sleep 10; done";
    command.args([&image, "sh", "-c", &format!("{fetched}; cargo test --offline --no-fail-fast --manifest-path /ui/cli/Cargo.toml")]);
    Ok(command)
}

/// Builds the crate for `target`, adding the target to the toolchain where it lacks it, and gives
/// whether it built and what the build said where it did not.
pub fn built_for(target: &str, ui: &Path, here: &Path) -> Outcome {
    let _ = Command::new("rustup").args(["target", "add", target]).stdin(Stdio::null()).stdout(Stdio::null()).stderr(Stdio::null()).status();
    let done = Command::new("cargo").args(["check", "--manifest-path"]).arg(ui.join("cli").join("Cargo.toml")).args(["--target", target, "--target-dir"]).arg(here.join("target").join("cross")).stdin(Stdio::null()).output();
    match done {
        Ok(done) if done.status.success() => Outcome { name: format!("it builds for {target}"), ok: true, ms: 0, error: String::new() },
        Ok(done) => Outcome { name: format!("it builds for {target}"), ok: false, ms: 0, error: String::from_utf8_lossy(&done.stderr).lines().filter(|line| line.starts_with("error")).take(10).collect::<Vec<_>>().join("\n") },
        Err(error) => Outcome { name: format!("it builds for {target}"), ok: false, ms: 0, error: error.to_string() },
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_container_reaches_the_test_servers_at_docker_s_name_for_this_machine() {
        let env = vec![
            ("ORIOR_PG_ADDRESS".to_string(), "127.0.0.1:5000".to_string()),
            ("ORIOR_IPP_PDF".to_string(), "ipp://127.0.0.1:6000/ipp/print".to_string()),
            ("ORIOR_TEST_SSH".to_string(), "ssh -F \"D:/x/config\"".to_string()),
            ("ORIOR_PG_PASSWORD".to_string(), "secret".to_string()),
        ];
        assert_eq!(
            seen_from_a_container(&env),
            vec![
                ("ORIOR_PG_ADDRESS".to_string(), "host.docker.internal:5000".to_string()),
                ("ORIOR_IPP_PDF".to_string(), "ipp://host.docker.internal:6000/ipp/print".to_string()),
                ("ORIOR_PG_PASSWORD".to_string(), "secret".to_string()),
            ]
        );
    }

    #[test]
    fn each_system_s_image_is_there_to_build() {
        let here = Path::new(env!("CARGO_MANIFEST_DIR"));
        for (name, folder, platform) in SYSTEMS {
            assert!(here.join("systems").join(folder).join("Dockerfile").is_file(), "{name}");
            assert!(platform.starts_with("linux/"), "{name}");
        }
    }
}
