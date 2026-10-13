// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The runs of the tree kept while the host is idle, in a run of their own. Every folder of the
//! tree that holds a run is taken in turn, but one whose newest run's writer runs, which a loop is
//! still using: each run still a `.qry` has its writer's account read, its bytes, its writes and
//! the slowest of them, and the folder's median rate said beside every run that wrote at under
//! half of it; where the part is free, each run but the newest is kept as its crystal; only the
//! newest `QRY_KEPT` runs of the folder are kept; and the host is swept once more where no sweep
//! of it is newer than `QRY_SWEEP_HOURS`, 12 where it is not set. The sweep: the run's buffer's
//! candidates timed once more (qry_sweep.c), and `qry_host.h` written beside `qry_buffer.h` from
//! the median of every sweep kept, once there are enough of them to say.

use std::path::{Path, PathBuf};

use crate::commands::Context;
use crate::programs::{Programs, Role};
use crate::run;
use crate::tools::text;
use crate::writer::{Priority, Writer};

/// The keepers' own run folder, and the sweeps'.
const KEPT: &str = "build/qry_kept/qry";

const SWEEPS: &str = "build/qry_sweep/qry";

/// The header the sweeps write, beside the buffer's.
const HOST_HEADER: &str = "src/cu/types/file_defs/qry/qry_host.h";

/// A writer's account of its run, as `qry_write_until_stopped` says it.
pub struct Account {
    pub bytes: u64,
    pub writes: u64,
    pub slowest_ms: f64,
    pub slowest_bytes: u64,
    pub ms: f64,
    pub panics: u64,
}

impl Account {
    /// MiB written a second, 0 where no time was spent writing.
    pub fn rate(&self) -> f64 {
        if self.ms > 0.0 { (self.bytes as f64 / 1_048_576.0) / (self.ms / 1000.0) } else { 0.0 }
    }
}

/// The account read off its line: `<n> blobs handed, <n> dumped, <bytes> bytes in <writes> writes,
/// the slowest <ms> ms for <bytes> bytes, <ms> ms writing in all; <n> panics, ...`.
pub fn account_read(line: &str) -> Option<Account> {
    let words: Vec<&str> = line.split_whitespace().collect();
    if words.len() < 24 || words.get(1) != Some(&"blobs") {
        return None;
    }
    let number = |at: usize| words[at].trim_end_matches([',', ';']).parse::<u64>().ok();
    let fraction = |at: usize| words[at].trim_end_matches([',', ';']).parse::<f64>().ok();
    Some(Account { bytes: number(5)?, writes: number(8)?, slowest_ms: fraction(12)?, slowest_bytes: number(15)?, ms: fraction(17)?, panics: number(22)? })
}

/// The folder's median rate over `accounts`, each a run's name and its rate, and every run under
/// half of it, as lines; none where there is no account.
pub fn median_said(folder: &str, accounts: &[(String, f64)]) -> Vec<String> {
    if accounts.is_empty() {
        return Vec::new();
    }
    let mut rates: Vec<f64> = accounts.iter().map(|(_, rate)| *rate).collect();
    rates.sort_by(|a, b| a.partial_cmp(b).unwrap_or(std::cmp::Ordering::Equal));
    let median = rates[(rates.len() + 1) / 2 - 1];
    let mut lines = vec![format!("  {folder}: {} runs read, the median {median:.0} MiB/s", accounts.len())];
    for (name, rate) in accounts {
        if *rate < median / 2.0 {
            lines.push(format!("    {name} wrote at under half the median"));
        }
    }
    lines
}

/// Every folder under `root` holding a run's `.qry`, `.git` and every `restored` pruned, sorted.
pub fn folders_with_runs(root: &Path) -> Vec<PathBuf> {
    let mut found = Vec::new();
    let mut pending = vec![root.to_path_buf()];
    while let Some(folder) = pending.pop() {
        let mut holds = false;
        for entry in std::fs::read_dir(&folder).into_iter().flatten().flatten() {
            let path = entry.path();
            let name = entry.file_name().to_string_lossy().into_owned();
            if path.is_dir() {
                if name != ".git" && name != "restored" {
                    pending.push(path);
                }
            } else if name.starts_with("query") && name.ends_with(".qry") && name[5..].chars().next().is_some_and(|c| c.is_ascii_digit()) && name.contains('-') {
                holds = true;
            }
        }
        if holds {
            found.push(folder);
        }
    }
    found.sort();
    found
}

/// A writer for the run at `qry`, in the image of `own`.
fn writer_of(own: &Writer, qry: &Path) -> Writer {
    Writer { binary: own.binary.clone(), qry: qry.to_path_buf(), top: own.top.clone(), path: own.path.clone() }
}

/// Each run of `folder` still a `.qry` with its writer's account read: a line each, then the
/// folder's median and the runs under half of it.
pub fn writers_read(own: &Writer, folder: &Path, shown: &str) -> Vec<String> {
    let mut lines = Vec::new();
    let mut rates = Vec::new();
    for (_, held) in run::each(folder) {
        if held.extension().is_none_or(|e| e != "qry") {
            continue;
        }
        let name = held.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
        let Some(account) = writer_of(own, &held).latest("writer").and_then(|bytes| String::from_utf8_lossy(&bytes).lines().next().and_then(account_read)) else { continue };
        lines.push(format!(
            "    {name}: {:.1} MiB in {} writes, {:.3} ms writing, {:.0} MiB/s, the slowest {:.3} ms for {} bytes; {} panics",
            account.bytes as f64 / 1_048_576.0,
            account.writes,
            account.ms,
            account.rate(),
            account.slowest_ms,
            account.slowest_bytes,
            account.panics
        ));
        rates.push((name, account.rate()));
    }
    lines.extend(median_said(shown, &rates));
    lines
}

/// The size of every file under `folder`.
fn size_under(folder: &Path) -> u64 {
    let mut total = 0;
    let mut pending = vec![folder.to_path_buf()];
    while let Some(folder) = pending.pop() {
        for entry in std::fs::read_dir(&folder).into_iter().flatten().flatten() {
            let path = entry.path();
            if path.is_dir() {
                pending.push(path);
            } else {
                total += std::fs::metadata(&path).map(|m| m.len()).unwrap_or(0);
            }
        }
    }
    total
}

/// Each `.qry` of `folder` but the newest, and any whose writer runs, kept as its crystal through
/// `crystal`; the `.qry` let go only where the crystal held and is smaller. A line each.
pub fn crystals(own: &Writer, folder: &Path, crystal: &Path) -> Vec<String> {
    let mut lines = Vec::new();
    let mut newest = true;
    for (_, held) in run::each(folder) {
        if held.extension().is_none_or(|e| e != "qry") {
            continue;
        }
        if newest {
            newest = false;
            continue;
        }
        if writer_of(own, &held).runs() {
            continue;
        }
        let name = held.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
        let crystal_folder = held.with_extension("");
        let began = std::time::Instant::now();
        let written = std::process::Command::new(crystal).arg("write").arg(&held).arg(&crystal_folder).stdin(std::process::Stdio::null()).output();
        match written {
            Ok(output) if output.status.success() => {
                if size_under(&crystal_folder) >= std::fs::metadata(&held).map(|m| m.len()).unwrap_or(0) {
                    lines.push(format!("  {name} is kept as its .qry: its crystal is no smaller"));
                    let _ = std::fs::remove_dir_all(&crystal_folder);
                } else {
                    let _ = std::fs::remove_file(&held);
                    lines.push(format!("{}, in {} ms", String::from_utf8_lossy(&output.stdout).trim_end(), began.elapsed().as_millis()));
                }
            }
            _ => {
                lines.push(format!("  {name} is kept as its .qry: its crystal did not hold"));
                let _ = std::fs::remove_dir_all(&crystal_folder);
            }
        }
    }
    lines
}

/// One folder of runs kept: left as it is where its newest run's writer runs; else its writers'
/// accounts read, its runs kept as crystals where `crystal` is there, and only the newest `most` kept.
pub fn kept_folder(root: &Path, programs: &dyn Programs, folder: &Path, crystal: Option<&Path>, most: usize, own: &Writer, sink: &mut dyn FnMut(&str)) {
    let _ = programs;
    let shown = run::named(root, folder);
    if let Some((_, newest)) = run::each(folder).first() {
        if newest.extension().is_some_and(|e| e == "qry") && writer_of(own, newest).runs() {
            sink(&format!("  {shown}: left as it is, its newest run's writer runs"));
            return;
        }
    }
    for line in writers_read(own, folder, &shown) {
        sink(&line);
    }
    if let Some(crystal) = crystal {
        for line in crystals(own, folder, crystal) {
            sink(&line);
        }
    }
    let before = run::each(folder).len();
    run::kept(folder, most);
    sink(&format!("  {shown}: {} of {before} runs kept", run::each(folder).len()));
}

/// The part's allowed use in percent, as `QRY_PART_USED` gives it, 10 where it is not set or is no
/// number; a negative bound holds the part used whatever it reads.
pub fn used_most(given: Option<&str>) -> i64 {
    given.and_then(|v| v.trim().parse().ok()).unwrap_or(10)
}

/// Whether the part is free: no process of the tree on it, and its use at most `QRY_PART_USED`
/// percent, 10 where it is not set, over five readings a second apart. Why not, where it is not.
pub fn part_free(context: &Context) -> Result<(), String> {
    let smi = context.program(Role::NvidiaSmi).map_err(|_| "no nvidia-smi: the part is not read".to_string())?;
    let apps = std::process::Command::new(&smi).args(["--query-compute-apps=process_name", "--format=csv,noheader"]).output().map_err(|e| e.to_string())?;
    let top = text(context.root).to_ascii_lowercase();
    if String::from_utf8_lossy(&apps.stdout).lines().any(|line| line.to_ascii_lowercase().contains(&top)) {
        return Err("a process of the tree is on the part".into());
    }
    let most = used_most(std::env::var("QRY_PART_USED").ok().as_deref());
    for reading in 0..5 {
        let used = std::process::Command::new(&smi).args(["--query-gpu=utilization.gpu", "--format=csv,noheader,nounits"]).output().map_err(|e| e.to_string())?;
        let percent: i64 = String::from_utf8_lossy(&used.stdout).lines().next().and_then(|l| l.trim().parse().ok()).ok_or("the part's use was not read")?;
        if percent > most {
            return Err(format!("the part is used {percent} percent"));
        }
        if reading < 4 {
            std::thread::sleep(std::time::Duration::from_secs(1));
        }
    }
    Ok(())
}

/// The crystal's program, built against the engine in `build/qry_crystal/engine` where nvcc is
/// found: the engine's changed sources compiled first through `src/build_engine.sh`, and the
/// program again where its source, the engine's header or, on Windows, an object it takes in is
/// newer. None, said why, where it could not be had.
pub fn crystal_built(context: &mut Context) -> Option<PathBuf> {
    let nvcc = context.program(Role::Nvcc).ok()?;
    let bash = crate::programs::on_path(&["bash"])?;
    let engine = context.at("build/qry_crystal/engine");
    let binary = engine.join(crate::tools::binary_name("qry_crystal"));
    let _ = std::fs::create_dir_all(&engine);
    let log = engine.join("build.log");
    let built = std::process::Command::new(&bash).arg(context.at("src/build_engine.sh")).env("BUILD_OUT", &engine).env("PATH", context.programs.run_path()).stdin(std::process::Stdio::null()).output().ok()?;
    let _ = std::fs::write(&log, [built.stdout.as_slice(), built.stderr.as_slice()].concat());
    if !built.status.success() {
        context.say(&format!("  the crystal's engine did not build ({})", log.display()));
        return binary.is_file().then_some(binary);
    }
    let modified = |path: &Path| std::fs::metadata(path).and_then(|m| m.modified()).ok();
    let source = context.at("src/cu/types/file_defs/qry/qry_crystal.c");
    let header = context.at("src/cu/engine/engine.h");
    let objects: Vec<PathBuf> = if cfg!(windows) {
        std::fs::read_to_string(engine.join("cell_tracking_engine.objects")).unwrap_or_default().lines().filter(|l| !l.trim().is_empty()).map(PathBuf::from).collect()
    } else {
        Vec::new()
    };
    let stale = match modified(&binary) {
        None => true,
        Some(built) => [&source, &header].into_iter().chain(objects.iter()).any(|p| modified(p).is_some_and(|m| m > built)),
    };
    if stale {
        let mut words: Vec<String> = Vec::new();
        if cfg!(windows) {
            let ccbin = context.program(Role::Cl).ok().and_then(|cl| cl.parent().map(Path::to_path_buf))?;
            words.extend(["-ccbin".into(), text(&ccbin), "-Xcompiler".into(), "/std:c11 /O2 /Zc:preprocessor /D_CRT_SECURE_NO_WARNINGS".into()]);
        }
        words.extend(["-I".into(), text(&context.at("src/cu/engine")), "-o".into(), text(&binary), text(&source)]);
        if cfg!(windows) {
            words.extend(objects.iter().map(|o| text(o)));
        } else {
            words.extend(["-L".into(), text(&engine), "-lcell_tracking_engine".into(), "-Xlinker".into(), "-rpath".into(), "-Xlinker".into(), text(&engine)]);
        }
        let linked = std::process::Command::new(&nvcc).args(&words).env("PATH", context.programs.run_path()).stdin(std::process::Stdio::null()).output().ok()?;
        let build_log = engine.join("qry_crystal_build.log");
        let _ = std::fs::write(&build_log, [linked.stdout.as_slice(), linked.stderr.as_slice()].concat());
        if !linked.status.success() {
            context.say(&format!("  qry_crystal did not build ({}); runs are kept as their .qry", build_log.display()));
            return None;
        }
    }
    Some(binary)
}

/// The sweep: the buffer's candidates timed once more in a run of their own, and `qry_host.h`
/// written from the median of every sweep kept. 0 where the header stands or was written, 1 where
/// the sweeps do not yet say.
pub fn sweep(context: &mut Context, words: &[String]) -> Result<i32, String> {
    if !words.is_empty() {
        return Err("sweep takes no words".into());
    }
    let sweeper = context.tool("qry_sweep")?;
    unsafe { std::env::remove_var("QRY") };
    let out = context.at("build/qry_sweep");
    let run = context.started(SWEEPS)?;
    if context.handed(&run, "sweep", &sweeper, &["time", &text(&out)])? != 0 {
        return Err("the sweep did not run".into());
    }
    context.ended(run)?;
    let runs: Vec<PathBuf> = run::each(&context.at(SWEEPS)).into_iter().map(|(_, held)| held).filter(|held| held.extension().is_some_and(|e| e == "qry")).collect();
    let mut words: Vec<String> = vec!["pick".into()];
    words.extend(runs.iter().map(|r| text(r)));
    let picked = std::process::Command::new(&sweeper).args(&words).stdin(std::process::Stdio::null()).output().map_err(|e| e.to_string())?;
    let header = context.at(HOST_HEADER);
    match picked.status.code() {
        Some(0) => {}
        Some(3) => return Ok(0),
        _ => return Ok(1),
    }
    if std::fs::read(&header).ok().as_deref() == Some(picked.stdout.as_slice()) {
        context.say(&format!("  qry_host.h holds as it was, from {} sweeps", runs.len()));
    } else {
        std::fs::write(&header, &picked.stdout).map_err(|e| format!("{}: {e}", header.display()))?;
        context.say(&format!("  qry_host.h written from {} sweeps:", runs.len()));
    }
    for line in String::from_utf8_lossy(&picked.stdout).lines().filter(|l| l.starts_with("#define")) {
        context.say(&format!("    {line}"));
    }
    Ok(0)
}

/// 1 where a sweep's run in the sweeps' folder is newer than `hours`.
fn swept_within(folder: &Path, hours: u64) -> bool {
    let since = std::time::SystemTime::now() - std::time::Duration::from_secs(hours * 3600);
    run::each(folder).into_iter().any(|(_, held)| held.extension().is_some_and(|e| e == "qry") && std::fs::metadata(&held).and_then(|m| m.modified()).is_ok_and(|m| m > since))
}

/// Why no crystal is made this time, said before any folder: the part not free, its reading on a
/// line of its own, or the crystal's program not built. Nothing where a crystal is to be made.
fn crystals_refused(free: &Result<(), String>, crystal: Option<&Path>) -> Vec<String> {
    match (free, crystal) {
        (Err(why), _) => vec!["  no run is kept as its crystal this time:".into(), format!("  {why}")],
        (Ok(()), None) => vec!["  no run is kept as its crystal this time: the crystal's program is not built".into()],
        _ => Vec::new(),
    }
}

/// What the keepers say of the folders, sorted into the writers' accounts, the crystals' lines and
/// the runs kept.
pub struct Said {
    pub writers: Vec<String>,
    pub crystals: Vec<String>,
    pub kept: Vec<String>,
}

/// Every folder given kept and its writers read; the sweeps' folder like the rest but made no
/// crystal.
pub fn folders_said(root: &Path, programs: &dyn Programs, folders: &[PathBuf], sweeps: &Path, crystal: Option<&Path>, most: usize, own: &Writer) -> Said {
    let mut said = Said { writers: Vec::new(), crystals: Vec::new(), kept: Vec::new() };
    for folder in folders {
        let crystal_here = if crate::run::plain(folder) == crate::run::plain(sweeps) { None } else { crystal };
        let mut lines = Vec::new();
        kept_folder(root, programs, folder, crystal_here, most, own, &mut |line| lines.push(line.to_string()));
        for line in lines {
            if line.contains(" runs kept") || line.contains("left as it is") {
                said.kept.push(line);
            } else if line.contains("crystal") || line.contains(" ms") && !line.contains("ms writing") {
                said.crystals.push(line);
            } else {
                said.writers.push(line);
            }
        }
    }
    said
}

pub fn run(context: &mut Context, words: &[String]) -> Result<i32, String> {
    if !words.is_empty() {
        return Err("kept takes no words".into());
    }
    unsafe { std::env::remove_var("QRY") };
    let own = context.started(KEPT)?;
    let free = part_free(context);
    let crystal = match &free {
        Ok(()) => crystal_built(context),
        Err(_) => None,
    };
    let mut crystals_said = crystals_refused(&free, crystal.as_deref());
    let sweeps = context.at(SWEEPS);
    let said = folders_said(context.root, context.programs, &folders_with_runs(context.root), &sweeps, crystal.as_deref(), run::kept_most(), own.writer());
    let writers_said = said.writers;
    crystals_said.extend(said.crystals);
    let kept_said = said.kept;
    for (name, lines) in [("writers", &writers_said), ("crystals", &crystals_said), ("kept", &kept_said)] {
        own.writer().hand(name, format!("{}\n", lines.join("\n")).as_bytes(), Priority::Ordinary)?;
        for line in lines {
            context.say(line);
        }
    }
    let hours: u64 = std::env::var("QRY_SWEEP_HOURS").ok().and_then(|v| v.parse().ok()).unwrap_or(12);
    let swept = if swept_within(&sweeps, hours) {
        vec![format!("  a sweep of the host is newer than {hours} hours: none is run")]
    } else {
        let mut lines = Vec::new();
        let mut inner = Context { root: context.root, programs: context.programs, sink: &mut |line: &str| lines.push(line.to_string()), leave: context.leave, asker: context.asker, env: context.env.clone() };
        if let Err(said) = sweep(&mut inner, &[]) {
            lines.push(format!("  {said}"));
        }
        lines
    };
    own.writer().hand("swept", format!("{}\n", swept.join("\n")).as_bytes(), Priority::Ordinary)?;
    for line in &swept {
        context.say(line);
    }
    context.ended(own)?;
    Ok(0)
}

#[cfg(test)]
mod keeping {
    use super::*;
    use crate::programs::OnPath;
    use crate::run::Run;

    #[test]
    fn a_folder_whose_newest_writer_runs_is_left() {
        if OnPath.program(Role::C).is_err() {
            return;
        }
        let root = crate::tree_root();
        let base = root.join("build/query_test/kept_live");
        let _ = std::fs::remove_dir_all(&base);
        let folder = base.join("qry");
        let live = Run::started(&root, &folder, &OnPath).unwrap();
        std::fs::write(folder.join("query0-20250101T000000.qry"), "").unwrap();
        let mut said = Vec::new();
        kept_folder(&root, &OnPath, &folder, None, 64, live.writer(), &mut |line| said.push(line.to_string()));
        assert!(said.iter().any(|l| l.contains("left as it is, its newest run's writer runs")), "{said:?}");
        assert!(folder.join("query0-20250101T000000.qry").is_file());
        drop(live);
        let _ = std::fs::remove_dir_all(&base);
    }

    #[test]
    fn the_writers_account_is_summed_as_the_script_sums_it() {
        let line = "9 blobs handed, 0 dumped, 1234567 bytes in 3 writes, the slowest 1.500 ms for 100 bytes, 2.250 ms writing in all; 0 panics, the slowest 0.000 ms from its hand to the disk";
        let read = account_read(line).unwrap();
        assert_eq!(read.bytes, 1234567);
        assert_eq!(read.writes, 3);
        assert_eq!(read.slowest_ms, 1.5);
        assert_eq!(read.slowest_bytes, 100);
        assert_eq!(read.ms, 2.25);
        assert_eq!(read.panics, 0);
        assert!((read.rate() - (1234567.0 / 1048576.0) / (2.25 / 1000.0)).abs() < 1e-6);
        assert!(account_read("nothing here").is_none());
        let still = account_read("1 blobs handed, 0 dumped, 10 bytes in 1 writes, the slowest 0.100 ms for 10 bytes, 0.000 ms writing in all; 0 panics, the slowest 0.000 ms from its hand to the disk").unwrap();
        assert_eq!(still.rate(), 0.0, "no time writing is no rate");
    }

    #[test]
    fn the_part_s_allowed_use_is_read_as_the_script_reads_it() {
        assert_eq!(used_most(None), 10);
        assert_eq!(used_most(Some("25")), 25);
        // a negative bound, which every reading is past, holds the part used whatever it reads
        assert_eq!(used_most(Some("-1")), -1);
        assert_eq!(used_most(Some("many")), 10);
    }

    #[test]
    fn folders_with_runs_skip_git_and_restored() {
        let base = crate::tree_root().join("build/query_test/folders");
        let _ = std::fs::remove_dir_all(&base);
        std::fs::create_dir_all(base.join("a/qry/restored")).unwrap();
        std::fs::create_dir_all(base.join(".git/qry")).unwrap();
        std::fs::create_dir_all(base.join("b")).unwrap();
        std::fs::write(base.join("a/qry/query1-x.qry"), "").unwrap();
        std::fs::write(base.join("a/qry/restored/query1-x.qry"), "").unwrap();
        std::fs::write(base.join(".git/qry/query1-x.qry"), "").unwrap();
        std::fs::write(base.join("b/other.qry"), "").unwrap();
        assert_eq!(folders_with_runs(&base), [base.join("a").join("qry")]);
        let _ = std::fs::remove_dir_all(&base);
    }

    #[test]
    fn the_sweeps_folder_is_kept_and_read_but_made_no_crystal() {
        if OnPath.program(Role::C).is_err() {
            return;
        }
        let root = crate::tree_root();
        let base = root.join("build/query_test/kept_sweeps");
        let _ = std::fs::remove_dir_all(&base);
        let live = Run::started(&root, &base.join("own"), &OnPath).unwrap();
        let sweeps = base.join("sweep").join("qry");
        let other = base.join("other").join("qry");
        std::fs::create_dir_all(&sweeps).unwrap();
        std::fs::create_dir_all(&other).unwrap();
        std::fs::write(sweeps.join("query1-20250101T000001.qry"), "").unwrap();
        std::fs::write(sweeps.join("query2-20250101T000002.qry"), "").unwrap();
        std::fs::write(other.join("query1-20250101T000003.qry"), "").unwrap();
        std::fs::write(other.join("query2-20250101T000004.qry"), "").unwrap();
        let said = folders_said(&root, &OnPath, &[sweeps.clone(), other.clone()], &sweeps, Some(&base.join("no_crystal_here")), 64, live.writer());
        assert!(said.kept.iter().any(|l| l.contains("sweep") && l.contains("2 of 2 runs kept")), "{:?}", said.kept);
        assert!(said.crystals.iter().any(|l| l.contains("000003")), "{:?}", said.crystals);
        assert!(!said.crystals.iter().any(|l| l.contains("000001") || l.contains("000002")), "{:?}", said.crystals);
        drop(live);
        let _ = std::fs::remove_dir_all(&base);
    }

    #[test]
    fn the_part_held_used_is_said_on_a_line_of_its_own_as_the_script_says_it() {
        let said = crystals_refused(&Err("the part is used 3 percent".into()), None);
        assert_eq!(said, ["  no run is kept as its crystal this time:", "  the part is used 3 percent"]);
        let unbuilt = crystals_refused(&Ok(()), None);
        assert_eq!(unbuilt, ["  no run is kept as its crystal this time: the crystal's program is not built"]);
        assert!(crystals_refused(&Ok(()), Some(Path::new("x"))).is_empty());
    }

    #[test]
    fn the_median_rate_names_the_runs_under_half_of_it() {
        let accounts = vec![("query1-a.qry".to_string(), 10.0), ("query2-b.qry".to_string(), 4.0), ("query3-c.qry".to_string(), 12.0)];
        let lines = median_said("x/qry", &accounts);
        assert!(lines[0].contains("3 runs read, the median 10 MiB/s"), "{lines:?}");
        assert!(lines.iter().any(|l| l.contains("query2-b.qry wrote at under half the median")), "{lines:?}");
        assert!(median_said("x/qry", &[]).is_empty());
    }
}
