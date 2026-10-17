// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! A query run (`qry_buffer.h`): its folder, its `.qry`, its writer's process, whether it was
//! joined, and the files it read and may leave. Every process a loop starts reaches the run by
//! `QRY`. A run's `.qry` holds everything the run did and every file it ran against, as it read
//! each one and as it left each one it changed: one `.qry` says what one iteration saw and did. As
//! a run ends only the newest `QRY_KEPT` runs of its folder are kept, 64 where it is not set.

use std::path::{Path, PathBuf};
use std::process::Child;

use crate::leaves::{self, Asker, Leave};
use crate::member;
use crate::programs::Programs;
use crate::tools;
use crate::writer::{Priority, Writer};

/// One run: started by this process or joined.
pub struct Run {
    pub folder: PathBuf,
    writer: Writer,
    process: Option<Child>,
    joined: bool,
    read: Vec<PathBuf>,
    left: Vec<PathBuf>,
}

/// The runs of `folder`, newest first: each its number and its `.qry`, or the folder of its crystal.
pub fn each(folder: &Path) -> Vec<(u64, PathBuf)> {
    let mut found: Vec<(u64, PathBuf)> = std::fs::read_dir(folder)
        .into_iter()
        .flatten()
        .flatten()
        .map(|entry| entry.path())
        .filter_map(|path| {
            let name = path.file_name()?.to_str()?;
            let rest = name.strip_prefix("query")?;
            let (number, _) = rest.split_once('-')?;
            let number: u64 = number.parse().ok()?;
            let is_run = if name.ends_with(".qry") { path.is_file() } else { path.is_dir() };
            is_run.then_some((number, path.clone()))
        })
        .collect();
    found.sort_by(|a, b| b.0.cmp(&a.0));
    found
}

/// The next run's name in `folder`: `query<n>-<datetime>.qry`, `n` one past the highest there.
pub fn next_name(folder: &Path) -> String {
    let highest = each(folder).first().map(|(n, _)| *n).unwrap_or(0);
    format!("query{}-{}.qry", highest + 1, stamp())
}

/// The moment as `%Y%m%dT%H%M%S`, read off the system clock as universal time.
fn stamp() -> String {
    let seconds = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_secs()).unwrap_or(0);
    let (date, time) = (seconds / 86_400, seconds % 86_400);
    let (year, month, day) = civil(date);
    format!("{year:04}{month:02}{day:02}T{:02}{:02}{:02}", time / 3600, (time % 3600) / 60, time % 60)
}

/// The civil date of a count of days from 1970-01-01.
fn civil(days: u64) -> (u64, u64, u64) {
    let z = days + 719_468;
    let era = z / 146_097;
    let doe = z - era * 146_097;
    let yoe = (doe - doe / 1460 + doe / 36_524 - doe / 146_096) / 365;
    let y = yoe + era * 400;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = doy - (153 * mp + 2) / 5 + 1;
    let m = if mp < 10 { mp + 3 } else { mp - 9 };
    (if m <= 2 { y + 1 } else { y }, m, d)
}

/// `path` absolute, without the `\\?\` prefix Windows gives a canonical path.
pub fn plain(path: &Path) -> PathBuf {
    let full = if path.is_absolute() { path.to_path_buf() } else { std::env::current_dir().map(|d| d.join(path)).unwrap_or_else(|_| path.to_path_buf()) };
    let text = full.to_string_lossy();
    PathBuf::from(text.strip_prefix("\\\\?\\").unwrap_or(&text))
}

/// The name a file is handed under: its path from the top of the tree with forward slashes, else
/// its base name.
pub fn named(top: &Path, path: &Path) -> String {
    match plain(path).strip_prefix(plain(top)) {
        Ok(under) => under.to_string_lossy().replace('\\', "/"),
        Err(_) => path.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default(),
    }
}

/// The run at `held` as a `.qry` to read: itself, or one read back out of its crystal into
/// `restored/` beside it through `crystal`; none where it cannot be had.
pub fn opened(held: &Path, crystal: Option<&Path>) -> Option<PathBuf> {
    if held.extension().is_some_and(|e| e == "qry") {
        return Some(held.to_path_buf());
    }
    let crystal = crystal?;
    let restored = held.parent()?.join("restored");
    std::fs::create_dir_all(&restored).ok()?;
    let named = restored.join(format!("{}.qry", held.file_name()?.to_string_lossy()));
    if !named.is_file() {
        let status = std::process::Command::new(crystal).arg("read").arg(held).arg(&named).stdin(std::process::Stdio::null()).stdout(std::process::Stdio::null()).status().ok()?;
        if !status.success() {
            return None;
        }
    }
    Some(named)
}

/// The crystal's program where it is built, `build/qry_crystal/engine/qry_crystal`, through which a
/// run kept as its crystal is read back; none where it is not.
pub fn crystal(root: &Path) -> Option<PathBuf> {
    let program = root.join("build").join("qry_crystal").join("engine").join(tools::binary_name("qry_crystal"));
    program.is_file().then_some(program)
}

/// How many runs of a folder are kept: `QRY_KEPT`, 64 where it is not set.
pub fn kept_most() -> usize {
    std::env::var("QRY_KEPT").ok().and_then(|v| v.parse().ok()).unwrap_or(64)
}

/// Every run of `folder` but the newest `most` removed, and the restored crystals with them.
pub fn kept(folder: &Path, most: usize) {
    let _ = std::fs::remove_dir_all(folder.join("restored"));
    for (_, held) in each(folder).into_iter().skip(most) {
        if held.is_dir() {
            let _ = std::fs::remove_dir_all(&held);
        } else {
            let _ = std::fs::remove_file(&held);
        }
    }
}

/// The latest run of `folder` other than `own` whose blobs hold `name`.
pub fn latest(folder: &Path, name: &str, own: Option<&Path>, writer: &Writer, crystal: Option<&Path>) -> Option<PathBuf> {
    for (_, held) in each(folder) {
        let Some(named) = opened(&held, crystal) else { continue };
        if own.is_some_and(|o| plain(o) == plain(&named)) {
            continue;
        }
        let other = Writer { binary: writer.binary.clone(), qry: named.clone(), top: writer.top.clone(), path: writer.path.clone(), member: None };
        if other.latest(name).is_some() {
            return Some(named);
        }
    }
    None
}

impl Run {
    /// A run started in `folder`, or joined where `QRY` names a `.qry` whose writer runs. The writer
    /// is built where stale.
    pub fn started(root: &Path, folder: &Path, programs: &dyn Programs) -> Result<Run, String> {
        let binary = tools::built(root, programs, "qry_writer")?;
        let path = programs.run_path();
        if let Some(qry) = std::env::var_os("QRY").map(PathBuf::from) {
            let mut writer = Writer { binary: binary.clone(), qry: qry.clone(), top: root.to_path_buf(), path: path.clone(), member: None };
            if writer.runs() && writer.joined() {
                member::current_set(writer.member.clone());
                let folder = qry.parent().map(Path::to_path_buf).unwrap_or_else(|| folder.to_path_buf());
                return Ok(Run { folder, writer, process: None, joined: true, read: Vec::new(), left: Vec::new() });
            }
        }
        std::fs::create_dir_all(folder).map_err(|e| format!("{} is no folder: {e}", folder.display()))?;
        let qry = plain(&folder.join(next_name(folder)));
        let mut writer = Writer { binary, qry, top: root.to_path_buf(), path, member: None };
        let process = writer.start()?;
        let Some(made) = writer.made() else {
            return Err("the run's writer did not start".into());
        };
        // the writer's own output stays out of this console; the line naming the run's database is said here, for a
        // reader of the run to find it by
        if !made.is_empty() {
            println!("{made}");
        }
        if !writer.joined() {
            let _ = writer.stop();
            return Err("the run's buffer could not be opened".into());
        }
        member::current_set(writer.member.clone());
        Ok(Run { folder: folder.to_path_buf(), writer, process: Some(process), joined: false, read: Vec::new(), left: Vec::new() })
    }

    /// The run at `qry` joined, its writer already running: this process a member of it with a
    /// buffer of its own handed to the run's one writer. An error where no writer runs it.
    pub fn joining(root: &Path, qry: &Path, programs: &dyn Programs) -> Result<Run, String> {
        let binary = tools::built(root, programs, "qry_writer")?;
        let mut writer = Writer { binary, qry: qry.to_path_buf(), top: root.to_path_buf(), path: programs.run_path(), member: None };
        if !(writer.runs() && writer.joined()) {
            return Err(format!("no writer runs {}", qry.display()));
        }
        member::current_set(writer.member.clone());
        let folder = qry.parent().map(Path::to_path_buf).unwrap_or_else(|| root.to_path_buf());
        Ok(Run { folder, writer, process: None, joined: true, read: Vec::new(), left: Vec::new() })
    }

    /// 1 where the run was joined, started by another process.
    pub fn joined(&self) -> bool {
        self.joined
    }

    pub fn writer(&self) -> &Writer {
        &self.writer
    }

    /// Each of `files` (paths; their base names are the blobs') carried from the latest run of
    /// `folder` that holds it, newest first; a name no run holds handed from the disk where the file
    /// is there.
    pub fn carried(&self, folder: &Path, files: &[&str], crystal: Option<&Path>) -> Result<(), String> {
        let base = |file: &str| Path::new(file).file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
        // the runs of `folder` and the course's, newest first by when each last changed: a pass run alone
        // after a course carries what the course left
        let course = self.writer.top.join(crate::course::FOLDER);
        let mut runs: Vec<(std::time::SystemTime, PathBuf)> = each(folder)
            .into_iter()
            .chain(if plain(&course) == plain(folder) { Vec::new() } else { each(&course) })
            .map(|(_, path)| (std::fs::metadata(&path).and_then(|m| m.modified()).unwrap_or(std::time::UNIX_EPOCH), path))
            .collect();
        runs.sort_by(|a, b| b.0.cmp(&a.0));
        for (_, held) in runs {
            let missing: Vec<String> = files.iter().map(|f| base(f)).filter(|name| self.writer.latest(name).is_none()).collect();
            if missing.is_empty() {
                break;
            }
            let Some(named) = opened(&held, crystal) else { continue };
            if plain(&named) == plain(&self.writer.qry) {
                continue;
            }
            let _ = self.writer.carry(&named, &missing);
        }
        for file in files {
            let name = base(file);
            if self.writer.latest(&name).is_none() && Path::new(file).is_file() {
                let bytes = std::fs::read(file).map_err(|e| format!("{file}: {e}"))?;
                self.writer.hand(&name, &bytes, Priority::Ordinary)?;
            }
        }
        Ok(())
    }

    /// Each file that is there handed to the run as the run reads it, named by its path from the top
    /// of the tree, and remembered as one the run may leave.
    pub fn read(&mut self, paths: &[PathBuf]) -> Result<(), String> {
        for path in paths {
            if path.is_file() {
                let bytes = std::fs::read(path).map_err(|e| format!("{}: {e}", path.display()))?;
                self.writer.hand(&named(&self.writer.top, path), &bytes, Priority::Ordinary)?;
                self.read.push(path.clone());
            }
        }
        Ok(())
    }

    /// Each path handed to the run as the run leaves it, as the run ends.
    pub fn left(&mut self, paths: &[PathBuf]) {
        self.left.extend(paths.iter().cloned());
    }

    /// Every file the run may leave in its place: each read and each left, once, in the order first named.
    pub fn leaves(&self) -> Vec<PathBuf> {
        let mut all: Vec<PathBuf> = Vec::new();
        for path in self.read.iter().chain(self.left.iter()) {
            if !all.contains(path) {
                all.push(path.clone());
            }
        }
        all
    }

    /// The end: every left file handed, the leaves shown and asked as `leave` says, the writer
    /// stopped where this run started it, and only the newest runs of the folder kept. A joined run
    /// leaves its writer to the run that started it.
    pub fn ended(mut self, leave: Leave, asker: &mut dyn Asker, sink: &mut dyn FnMut(&str)) -> Result<(), String> {
        for path in std::mem::take(&mut self.left) {
            if let Err(said) = self.writer.left(&path) {
                sink(&format!("  {said}"));
            }
        }
        leaves::settled(&self, leave, asker, sink)?;
        self.stopped()
    }

    fn stopped(&mut self) -> Result<(), String> {
        // this process is a member of the run no more, before the writer is asked to stop
        member::current_set(None);
        self.writer.parted();
        let Some(mut process) = self.process.take() else { return Ok(()) };
        let stopped = self.writer.stop();
        let _ = process.wait();
        kept(&self.folder, kept_most());
        stopped
    }
}

impl Drop for Run {
    fn drop(&mut self) {
        let _ = self.stopped();
    }
}

#[cfg(test)]
mod runs {
    use super::*;
    use crate::programs::{OnPath, Role};

    fn root() -> PathBuf {
        crate::tree_root()
    }

    #[test]
    fn runs_of_a_folder_are_numbered_newest_first() {
        let folder = root().join("build/query_test/each");
        let _ = std::fs::remove_dir_all(&folder);
        std::fs::create_dir_all(&folder).unwrap();
        for name in ["query3-20260101T000000.qry", "query12-20260102T000000.qry", "queryx-bad.qry", "other.txt"] {
            std::fs::write(folder.join(name), "").unwrap();
        }
        std::fs::create_dir_all(folder.join("query7-20260101T000001")).unwrap();
        let numbers: Vec<u64> = each(&folder).into_iter().map(|(n, _)| n).collect();
        assert_eq!(numbers, [12, 7, 3]);
        assert_eq!(next_name(&folder).split('-').next().unwrap(), "query13");
        let _ = std::fs::remove_dir_all(&folder);
    }

    #[test]
    fn kept_removes_all_but_the_newest_and_the_restored() {
        let folder = root().join("build/query_test/kept");
        let _ = std::fs::remove_dir_all(&folder);
        std::fs::create_dir_all(folder.join("restored")).unwrap();
        for n in 1..=5 {
            std::fs::write(folder.join(format!("query{n}-20260101T00000{n}.qry")), "").unwrap();
        }
        kept(&folder, 2);
        let left: Vec<u64> = each(&folder).into_iter().map(|(n, _)| n).collect();
        assert_eq!(left, [5, 4]);
        assert!(!folder.join("restored").exists());
        let _ = std::fs::remove_dir_all(&folder);
    }

    #[test]
    fn the_crystal_s_program_is_given_where_it_is_built() {
        let root = root().join("build/query_test/crystal");
        let _ = std::fs::remove_dir_all(&root);
        assert_eq!(crystal(&root), None);
        let engine = root.join("build/qry_crystal/engine");
        std::fs::create_dir_all(&engine).unwrap();
        let program = engine.join(crate::tools::binary_name("qry_crystal"));
        std::fs::write(&program, "").unwrap();
        assert_eq!(crystal(&root), Some(program));
        let _ = std::fs::remove_dir_all(&root);
    }

    #[test]
    fn a_path_is_named_under_the_top_of_the_tree() {
        let top = root();
        assert_eq!(named(&top, &top.join("src/cu/transpiler/lstar/protocol/table/sm_86.khw")), "src/cu/transpiler/lstar/protocol/table/sm_86.khw");
        assert_eq!(named(&top, Path::new("C:/elsewhere/forms.txt")), "forms.txt");
    }

    #[test]
    fn a_failed_loop_still_stops_its_writer() {
        if OnPath.program(Role::C).is_err() {
            return;
        }
        let root = root();
        let folder = root.join("build/query_test/failed/qry");
        let _ = std::fs::remove_dir_all(folder.parent().unwrap());
        let qry = {
            let run = Run::started(&root, &folder, &OnPath).unwrap();
            assert!(!run.joined());
            assert!(run.writer().runs());
            run.writer().qry.clone()
        };
        // the run dropped without `ended`: its writer is stopped in Drop
        let writer = Writer { binary: tools::built(&root, &OnPath, "qry_writer").unwrap(), qry: qry.clone(), top: root.clone(), path: OnPath.run_path(), member: None };
        assert!(!writer.runs());
        assert!(qry.is_file());
        let _ = std::fs::remove_dir_all(folder.parent().unwrap());
    }

    #[test]
    fn a_step_joins_the_run_held_open_and_its_end_leaves_the_writer_running() {
        if OnPath.program(Role::C).is_err() {
            return;
        }
        let root = root();
        let folder = root.join("build/query_test/joining/qry");
        let _ = std::fs::remove_dir_all(folder.parent().unwrap());
        let held = Run::started(&root, &folder, &OnPath).unwrap();
        let qry = held.writer().qry.clone();
        {
            let step = Run::joining(&root, &qry, &OnPath).unwrap();
            assert!(step.joined());
            assert_eq!(plain(&step.writer().qry), plain(&qry));
            step.writer().hand("step.console", b"a step's line\n", Priority::Ordinary).unwrap();
        }
        // the step parted as it was dropped, and the writer it joined still runs and holds its line
        assert!(held.writer().runs());
        assert_eq!(held.writer().read("step.console").as_deref(), Some(&b"a step's line\n"[..]));
        drop(held);
        assert!(Run::joining(&root, &qry, &OnPath).is_err(), "no step joins a run whose writer has stopped");
        let _ = std::fs::remove_dir_all(folder.parent().unwrap());
    }

    #[test]
    fn carried_takes_a_name_from_the_latest_run_that_holds_it_else_the_disk() {
        if OnPath.program(Role::C).is_err() {
            return;
        }
        let root = root();
        let base = root.join("build/query_test/carried");
        let _ = std::fs::remove_dir_all(&base);
        let folder = base.join("qry");
        {
            let run = Run::started(&root, &folder, &OnPath).unwrap();
            run.writer().hand("forms.txt", b"from the run before\n", Priority::Ordinary).unwrap();
            run.ended(Leave::All, &mut crate::leaves::Terminal, &mut |_| {}).unwrap();
        }
        std::fs::write(base.join("faulted.txt"), b"from the disk\n").unwrap();
        let run = Run::started(&root, &folder, &OnPath).unwrap();
        let files: Vec<String> = ["forms.txt", "faulted.txt", "nowhere.txt"].iter().map(|f| base.join(f).to_string_lossy().into_owned()).collect();
        run.carried(&folder, &files.iter().map(String::as_str).collect::<Vec<_>>(), None).unwrap();
        assert_eq!(&*run.writer().latest("forms.txt").unwrap(), b"from the run before\n");
        assert_eq!(&*run.writer().latest("faulted.txt").unwrap(), b"from the disk\n");
        assert!(run.writer().latest("nowhere.txt").is_none());
        run.ended(Leave::All, &mut crate::leaves::Terminal, &mut |_| {}).unwrap();
        let _ = std::fs::remove_dir_all(&base);
    }

    #[test]
    fn a_file_read_is_handed_under_its_path_and_remembered_as_a_leaf() {
        if OnPath.program(Role::C).is_err() {
            return;
        }
        let root = root();
        let base = root.join("build/query_test/read");
        let _ = std::fs::remove_dir_all(&base);
        std::fs::create_dir_all(&base).unwrap();
        let file = base.join("sm_86.khw");
        std::fs::write(&file, b"forms\n").unwrap();
        let mut run = Run::started(&root, &base.join("qry"), &OnPath).unwrap();
        run.read(&[file.clone(), base.join("absent.khw")]).unwrap();
        assert_eq!(&*run.writer().latest("build/query_test/read/sm_86.khw").unwrap(), b"forms\n");
        assert_eq!(run.leaves(), [file.clone()]);
        run.left(&[file.clone(), base.join("left.txt")]);
        assert_eq!(run.leaves(), [file.clone(), base.join("left.txt")]);
        run.ended(Leave::All, &mut crate::leaves::Terminal, &mut |_| {}).unwrap();
        let _ = std::fs::remove_dir_all(&base);
    }
}
