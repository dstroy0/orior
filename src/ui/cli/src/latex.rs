// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! A LaTeX file made into pages for the live preview, from the text in the editor, saved or not.
//!
//! The text is written into a folder orior keeps for the file, under the file's own name where that
//! name is a plain one, and made there: each make finds the last one's .aux, and its references
//! settle. The file's own folder is put on TEXINPUTS after the make's; what the file inputs, the
//! images it takes and a bibliography made beside it are found where the file is, and nothing is
//! written into the tree. The engine a `% !TEX program = ...` line at the top of the file names
//! makes it, else LuaLaTeX where the file takes fontspec or unicode-math, else pdfLaTeX, each told
//! to stop at the first error.
//!
//! The PDF is drawn as SVG by pdftocairo, its pages sharing one set of glyphs, and the SyncTeX file
//! the engine writes is read here into the boxes the text set on each page, each with the lines that
//! set it, in the page's own points: the preview takes the reader from a line to its place on a page
//! by them, and from a place on a page back to its line. A make that fails gives the errors the log
//! names, each with its file and line, and no pages.

use std::collections::HashMap;
use std::ffi::OsString;
use std::path::{Path, PathBuf};
use std::process::{Command, Stdio};
use std::time::{Duration, Instant};

use serde::Serialize;

use crate::{archive, digest, home, root, runner, toolchains};

/// How long an engine or pdftocairo may take before it is stopped.
const PATIENCE: Duration = Duration::from_secs(120);

/// The most errors a make gives.
const MOST_ERRORS: usize = 20;

/// A LaTeX file made: its pages, the glyphs they share, where its lines were set, and what failed.
#[derive(Serialize, Debug, Default)]
pub struct Made {
    pub engine: String,
    /// The glyphs, clips and images the pages draw with, as the inside of an SVG `defs`.
    pub defs: String,
    pub pages: Vec<Page>,
    pub boxes: Vec<Placed>,
    pub errors: Vec<Failure>,
    /// How long the make took, in milliseconds.
    pub ms: u64,
}

/// A page, in points: its size, and what it draws, as the inside of an SVG of that size.
#[derive(Serialize, Debug, PartialEq)]
pub struct Page {
    pub width: f64,
    pub height: f64,
    pub svg: String,
}

/// A box of text set on a page, counted from 1, in the page's points from its top left: the file of
/// the tree that set it, and each line of that file it holds with the point across where that
/// line's part of it starts.
#[derive(Serialize, Debug, PartialEq)]
pub struct Placed {
    pub page: usize,
    pub file: String,
    pub x: f64,
    pub y: f64,
    pub w: f64,
    pub h: f64,
    pub lines: Vec<(f64, usize)>,
}

/// An error the log names, with the file and the line it stands at where the log gives them.
#[derive(Serialize, Debug, PartialEq)]
pub struct Failure {
    pub file: String,
    pub line: Option<usize>,
    pub text: String,
}

/// The text of each line of `text` before a comment.
fn uncommented(text: &str) -> impl Iterator<Item = &str> {
    text.lines().map(|line| {
        let bytes = line.as_bytes();
        let cut = (0..bytes.len()).find(|&at| bytes[at] == b'%' && (at == 0 || bytes[at - 1] != b'\\')).unwrap_or(bytes.len());
        &line[..cut]
    })
}

/// The engine a LaTeX text is made with: the one a `% !TEX program = ...` line before the text's
/// first command names, else lualatex where it takes fontspec or unicode-math, else pdflatex.
pub fn engine_of(text: &str) -> &'static str {
    for line in text.lines().map(str::trim).filter(|line| !line.is_empty()) {
        let Some(comment) = line.strip_prefix('%') else {
            break;
        };
        let said: String = comment.chars().filter(|c| !c.is_whitespace()).collect::<String>().to_ascii_lowercase();
        let named = said.strip_prefix("!texprogram=").or_else(|| said.strip_prefix("!texts-program="));
        match named {
            Some("pdflatex") | Some("latex") => return "pdflatex",
            Some("xelatex") => return "xelatex",
            Some("lualatex") => return "lualatex",
            _ => {}
        }
    }
    let takes = uncommented(text).any(|line| {
        line.match_indices("\\usepackage").any(|(at, word)| {
            let mut rest = &line[at + word.len()..];
            if let Some(options) = rest.trim_start().strip_prefix('[') {
                rest = options.split_once(']').map(|(_, after)| after).unwrap_or("");
            }
            rest.trim_start().strip_prefix('{').and_then(|names| names.split_once('}')).is_some_and(|(names, _)| names.split(',').any(|name| matches!(name.trim(), "fontspec" | "unicode-math")))
        })
    });
    if takes {
        "lualatex"
    } else {
        "pdflatex"
    }
}

/// The name a file is made under: its own where that is a plain one, which keeps \jobname and the
/// bibliography made beside it, and `preview` for any other.
fn job_of(file: &Path) -> String {
    let stem = file.file_stem().map(|stem| stem.to_string_lossy().into_owned()).unwrap_or_default();
    if !stem.is_empty() && stem.chars().all(|c| c.is_ascii_alphanumeric() || c == '-' || c == '_') {
        stem
    } else {
        "preview".into()
    }
}

/// The program `name` as orior finds it: in TeX's folder or Poppler's as File, Toolchains finds
/// them, then on the PATH orior's runs get.
fn program(name: &str) -> Option<PathBuf> {
    let names = [name.to_string()];
    let path = toolchains::path_folders();
    let kept = toolchains::chosen();
    toolchains::manifest()
        .iter()
        .filter(|tool| tool.id == "tex" || tool.id == "poppler")
        .filter_map(|tool| toolchains::find(tool, &path, &kept).folder)
        .find_map(|dir| toolchains::program_in(Path::new(&dir), &names))
        .or_else(|| path.iter().find_map(|dir| toolchains::program_in(dir, &names)))
}

/// Runs `program` in `dir` with `args` and TEXINPUTS `inputs`, and says whether it ended well.
fn run(program: &Path, args: &[String], dir: &Path, inputs: &OsString) -> Result<bool, String> {
    let name = program.file_stem().map(|stem| stem.to_string_lossy().into_owned()).unwrap_or_default();
    let mut command = Command::new(program);
    command.args(args).current_dir(dir).env("PATH", toolchains::run_path()).env("TEXINPUTS", inputs).stdin(Stdio::null()).stdout(Stdio::null()).stderr(Stdio::null());
    runner::quiet(&mut command);
    let mut child = command.spawn().map_err(|error| format!("{}: {error}", program.display()))?;
    let began = Instant::now();
    loop {
        match child.try_wait() {
            Ok(Some(status)) => return Ok(status.success()),
            Ok(None) if began.elapsed() < PATIENCE => std::thread::sleep(Duration::from_millis(15)),
            _ => {
                let _ = child.kill();
                let _ = child.wait();
                return Err(format!("{name} took longer than {} s and was stopped", PATIENCE.as_secs()));
            }
        }
    }
}

/// `path` with forward slashes and no `./` in it, in lower case on Windows, where case does not
/// tell two files apart.
fn plain(path: &str) -> String {
    let slashed = path.trim().replace('\\', "/").replace("/./", "/");
    let slashed = slashed.strip_prefix("./").unwrap_or(&slashed).to_string();
    if cfg!(windows) {
        slashed.to_lowercase()
    } else {
        slashed
    }
}

/// The file of the tree a name the engine writes stands for: the text made for `path`, by its name
/// or its full path in the make's folder, a file under the tree by its path there, and none for any
/// other.
struct Names {
    job: String,
    made: String,
    root: String,
    path: String,
}

impl Names {
    fn new(root: &Path, dir: &Path, job: &str, path: &str) -> Names {
        let job = format!("{job}.tex");
        let made = plain(&dir.join(&job).to_string_lossy());
        let mut root = plain(&root.to_string_lossy());
        if !root.ends_with('/') {
            root.push('/');
        }
        Names { job: plain(&job), made, root, path: path.to_string() }
    }

    fn of(&self, name: &str) -> Option<String> {
        let given = plain(name);
        if given == self.job || given == self.made {
            return Some(self.path.clone());
        }
        let start = name.trim().replace('\\', "/").replace("/./", "/");
        given.strip_prefix(&self.root).and_then(|_| start.get(self.root.len()..)).map(str::to_string)
    }
}

/// The errors a log names, as `file:line: text` with -file-line-error, the first ones first.
fn errors_of(log: &str, names: &Names) -> Vec<Failure> {
    let mut out = Vec::new();
    for line in log.lines() {
        let bytes = line.as_bytes();
        let found = (1..bytes.len()).filter(|&at| bytes[at] == b':').find_map(|at| {
            let digits = bytes[at + 1..].iter().take_while(|byte| byte.is_ascii_digit()).count();
            let end = at + 1 + digits;
            (digits > 0 && bytes.get(end) == Some(&b':') && bytes.get(end + 1) == Some(&b' ')).then(|| (at, line[at + 1..end].parse::<usize>().ok(), line[end + 2..].trim()))
        });
        let Some((at, number, text)) = found else {
            continue;
        };
        if text.starts_with("==>") || text.starts_with("Emergency stop") || text.is_empty() {
            continue;
        }
        let name = &line[..at];
        out.push(Failure { file: names.of(name).unwrap_or_else(|| name.replace('\\', "/")), line: number, text: text.to_string() });
        if out.len() == MOST_ERRORS {
            break;
        }
    }
    out
}

fn number_of(tag: &str, name: &str) -> f64 {
    tag.split(&format!(" {name}=\"")).nth(1).and_then(|rest| rest.split('"').next()).map(|value| value.trim_end_matches(|c: char| c.is_ascii_alphabetic())).and_then(|value| value.parse().ok()).unwrap_or(0.0)
}

/// The SVG pdftocairo draws, as the defs its pages share and each page's own drawing, every id in it
/// written with `tex-` before it so that none meets an id of the window's.
pub fn pages_of(svg: &str) -> (String, Vec<Page>) {
    let svg = svg.replace(" id=\"", " id=\"tex-").replace("href=\"#", "href=\"#tex-").replace("url(#", "url(#tex-");
    let Some(start) = svg.find("<svg") else {
        return (String::new(), Vec::new());
    };
    let open_end = svg[start..].find('>').map(|end| start + end + 1).unwrap_or(svg.len());
    let (width, height) = (number_of(&svg[start..open_end], "width"), number_of(&svg[start..open_end], "height"));
    let body = &svg[open_end..svg.rfind("</svg>").unwrap_or(svg.len()).max(open_end)];
    let (defs, rest) = match (body.find("<defs>"), body.find("</defs>")) {
        (Some(from), Some(to)) if from < to => (body[from + "<defs>".len()..to].to_string(), format!("{}{}", &body[..from], &body[to + "</defs>".len()..])),
        _ => (String::new(), body.to_string()),
    };
    let drawings: Vec<String> = if rest.contains("<pageSet>") {
        rest.split("<page>").skip(1).map(|part| part.split("</page>").next().unwrap_or("").to_string()).collect()
    } else {
        vec![rest.trim().to_string()]
    };
    (defs, drawings.into_iter().map(|svg| Page { width, height, svg }).collect())
}

/// One record of a SyncTeX file: its file's tag, its line, its point and, for a box, its width,
/// height and depth, all in the file's own units.
struct Record {
    tag: u32,
    line: usize,
    x: i64,
    y: i64,
    size: [i64; 3],
}

fn record(text: &str) -> Option<Record> {
    let (head, place) = text.split_once(':')?;
    let mut head = head.split(',');
    let tag = head.next()?.trim().parse().ok()?;
    let line = head.next()?.trim().parse().ok()?;
    let mut parts = place.split(':');
    let (x, y) = parts.next()?.split_once(',')?;
    let mut size = [0i64; 3];
    if let Some(dims) = parts.next() {
        for (slot, value) in size.iter_mut().zip(dims.split(',')) {
            *slot = value.trim().parse().unwrap_or(0);
        }
    }
    Some(Record { tag, line, x: x.trim().parse().ok()?, y: y.trim().parse().ok()?, size })
}

/// The boxes of text a SyncTeX file says each page holds, each with the lines of a file of the tree
/// that set it; `names` gives the file of the tree each file the engine read stands for.
fn boxes_of(text: &str, names: &dyn Fn(&str) -> Option<String>) -> Vec<Placed> {
    struct Open {
        x: i64,
        y: i64,
        size: [i64; 3],
        marks: Vec<(u32, i64, usize)>,
    }
    let mut files: HashMap<u32, Option<String>> = HashMap::new();
    let (mut unit, mut magnification, mut x_offset, mut y_offset) = (1.0f64, 1000.0f64, 0.0f64, 0.0f64);
    let mut page = 0usize;
    let mut open: Vec<Open> = Vec::new();
    let mut out = Vec::new();
    let mut content = false;
    for line in text.lines() {
        if let Some(rest) = line.strip_prefix("Input:") {
            if let Some((tag, name)) = rest.split_once(':') {
                if let Ok(tag) = tag.parse() {
                    files.insert(tag, names(name));
                }
            }
            continue;
        }
        if !content {
            let number = |rest: &str| rest.trim().parse::<f64>().ok();
            if let Some(value) = line.strip_prefix("Unit:").and_then(number) {
                unit = value;
            } else if let Some(value) = line.strip_prefix("Magnification:").and_then(number) {
                magnification = value;
            } else if let Some(value) = line.strip_prefix("X Offset:").and_then(number) {
                x_offset = value;
            } else if let Some(value) = line.strip_prefix("Y Offset:").and_then(number) {
                y_offset = value;
            }
            content = line.starts_with("Content:");
            continue;
        }
        let points = |value: i64, offset: f64| ((value as f64 * unit + offset) * magnification / 1000.0 / 65536.0 * 72.0 / 72.27 * 1000.0).round() / 1000.0;
        let Some(kind) = line.chars().next() else {
            continue;
        };
        let rest = &line[kind.len_utf8()..];
        match kind {
            '{' => {
                page = rest.trim().parse().unwrap_or(page + 1);
                open.clear();
            }
            '}' => open.clear(),
            '(' => {
                if let Some(read) = record(rest) {
                    open.push(Open { x: read.x, y: read.y, size: read.size, marks: Vec::new() });
                }
            }
            ')' => {
                let Some(done) = open.pop() else {
                    continue;
                };
                let mut tags: Vec<u32> = Vec::new();
                for &(tag, _, _) in &done.marks {
                    if !tags.contains(&tag) {
                        tags.push(tag);
                    }
                }
                for tag in tags {
                    let Some(Some(file)) = files.get(&tag) else {
                        continue;
                    };
                    let mut lines: Vec<(f64, usize)> = Vec::new();
                    for &(_, x, line) in done.marks.iter().filter(|mark| mark.0 == tag) {
                        if lines.last().map(|last| last.1) != Some(line) {
                            lines.push((points(x, x_offset), line));
                        }
                    }
                    let [width, height, depth] = done.size;
                    out.push(Placed { page, file: file.clone(), x: points(done.x, x_offset), y: points(done.y - height, y_offset), w: points(width, 0.0), h: points(height + depth, 0.0), lines });
                }
            }
            'x' | 'k' | 'g' | '$' | 'h' | 'v' => {
                if let (Some(read), Some(top)) = (record(rest), open.last_mut()) {
                    if read.line > 0 {
                        top.marks.push((read.tag, read.x, read.line));
                    }
                }
            }
            _ => {}
        }
    }
    out
}

/// The LaTeX text `text` of the file `path` of the tree `root` made into pages, in orior's own folder.
pub fn make(root: &Path, path: &str, text: &str) -> Result<Made, String> {
    let kept = home::folder().ok_or("orior's own folder is not known")?.join("previews");
    make_in(root, path, text, &kept)
}

/// `make`, its files kept under `kept`.
pub fn make_in(root: &Path, path: &str, text: &str, kept: &Path) -> Result<Made, String> {
    let began = Instant::now();
    let file = root::inside(root, path)?;
    let folder = file.parent().ok_or_else(|| format!("{path} has no folder"))?.to_path_buf();
    let engine = engine_of(text);
    let tex = program(engine).ok_or_else(|| format!("{engine} is not found: File, Toolchains opens TeX's install page or takes its folder"))?;
    let cairo = program("pdftocairo").ok_or("pdftocairo is not found: File, Toolchains opens Poppler's install page or takes its folder")?;
    let dir = kept.join(&digest::sha256(file.to_string_lossy().as_bytes())[..16]);
    std::fs::create_dir_all(&dir).map_err(|error| format!("{}: {error}", dir.display()))?;
    let job = job_of(&file);
    std::fs::write(dir.join(format!("{job}.tex")), text).map_err(|error| format!("{}: {error}", dir.display()))?;
    let pdf = dir.join(format!("{job}.pdf"));
    let _ = std::fs::remove_file(&pdf);
    let between = if cfg!(windows) { ";" } else { ":" };
    let mut inputs = OsString::from(".");
    inputs.push(between);
    inputs.push(folder.into_os_string());
    inputs.push(between);
    if let Some(more) = std::env::var_os("TEXINPUTS") {
        inputs.push(more);
    }
    let names = Names::new(root, &dir, &job, path);
    // The text is named by its path from the make's folder, which no search on TEXINPUTS takes: the
    // file saved in the tree under the same name is never made in its place.
    let args: Vec<String> = ["-interaction=nonstopmode", "-halt-on-error", "-file-line-error", "-synctex=1"].iter().map(|arg| arg.to_string()).chain([format!("./{job}.tex")]).collect();
    let ended = run(&tex, &args, &dir, &inputs)?;
    let mut made = Made { engine: engine.to_string(), ..Made::default() };
    if !ended || !pdf.is_file() {
        let log = std::fs::read(dir.join(format!("{job}.log"))).map(|bytes| String::from_utf8_lossy(&bytes).into_owned()).unwrap_or_default();
        made.errors = errors_of(&log, &names);
        if made.errors.is_empty() {
            let tail: Vec<&str> = log.lines().rev().filter(|line| !line.trim().is_empty()).take(4).collect();
            let text = if tail.is_empty() { format!("{engine} made no pages and wrote no log") } else { tail.into_iter().rev().collect::<Vec<_>>().join("\n") };
            made.errors.push(Failure { file: path.to_string(), line: None, text });
        }
        made.ms = began.elapsed().as_millis() as u64;
        return Ok(made);
    }
    let drawn = dir.join(format!("{job}.svg"));
    if !run(&cairo, &["-svg".to_string(), format!("{job}.pdf"), format!("{job}.svg")], &dir, &inputs)? {
        return Err(format!("pdftocairo could not draw the pages {engine} made"));
    }
    let svg = std::fs::read_to_string(&drawn).map_err(|error| format!("{}: {error}", drawn.display()))?;
    (made.defs, made.pages) = pages_of(&svg);
    if let Some(sync) = std::fs::read(dir.join(format!("{job}.synctex.gz"))).ok().and_then(|bytes| archive::gunzip(&bytes).ok()) {
        made.boxes = boxes_of(&String::from_utf8_lossy(&sync), &|name| names.of(name));
    }
    made.ms = began.elapsed().as_millis() as u64;
    Ok(made)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn scratch(name: &str) -> PathBuf {
        let root = std::env::temp_dir().join(format!("orior_ui_latex_{name}_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&root);
        std::fs::create_dir_all(root.join("doc/parts")).unwrap();
        dunce::canonicalize(&root).unwrap()
    }

    #[test]
    fn the_engine_is_the_one_the_file_names_else_the_one_its_packages_need() {
        assert_eq!(engine_of("% !TEX program = xelatex\n\\documentclass{article}"), "xelatex");
        assert_eq!(engine_of("%!TeX TS-program = LuaLaTeX\n"), "lualatex");
        assert_eq!(engine_of("\\documentclass{article}\n\\usepackage[no-math]{fontspec}"), "lualatex");
        assert_eq!(engine_of("\\documentclass{article}\n\\usepackage{amsmath, unicode-math}"), "lualatex");
        assert_eq!(engine_of("\\documentclass{article}\n% \\usepackage{fontspec}\n\\usepackage{amsmath}"), "pdflatex");
        assert_eq!(engine_of("\\documentclass{article}\n% !TEX program = xelatex"), "pdflatex", "a line after the first command names nothing");
    }

    #[test]
    fn a_plain_name_is_kept_and_any_other_made_as_preview() {
        assert_eq!(job_of(Path::new("a/paper_2-b.tex")), "paper_2-b");
        assert_eq!(job_of(Path::new("a/my paper.tex")), "preview");
        assert_eq!(job_of(Path::new("a/é.tex")), "preview");
    }

    #[test]
    fn the_pages_share_one_set_of_glyphs_and_each_id_is_the_window_s_own() {
        let svg = "<?xml version=\"1.0\"?>\n<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"595.276\" height=\"841.89pt\" viewBox=\"0 0 595.276 841.89\">\n<defs>\n<g id=\"glyph-0-0\"><path d=\"M 0 0\"/></g>\n</defs>\n<pageSet>\n<page>\n<use xlink:href=\"#glyph-0-0\" x=\"1\" y=\"2\"/>\n</page>\n<page>\n<g clip-path=\"url(#clip-1)\"/>\n</page>\n</pageSet>\n</svg>\n";
        let (defs, pages) = pages_of(svg);
        assert!(defs.contains("id=\"tex-glyph-0-0\""), "{defs}");
        assert_eq!(pages.len(), 2);
        assert_eq!((pages[0].width, pages[0].height), (595.276, 841.89));
        assert!(pages[0].svg.contains("href=\"#tex-glyph-0-0\""), "{}", pages[0].svg);
        assert!(pages[1].svg.contains("url(#tex-clip-1)"), "{}", pages[1].svg);
        let (_, one) = pages_of("<svg width=\"10\" height=\"20\"><defs></defs><g/></svg>");
        assert_eq!(one, vec![Page { width: 10.0, height: 20.0, svg: "<g/>".into() }], "a single page has no page set");
    }

    #[test]
    fn synctex_s_boxes_are_read_in_points_with_the_lines_that_set_them() {
        let sync = "SyncTeX Version:1\nInput:1:C:\\made\\paper.tex\nInput:2:C:\\tex\\article.cls\nInput:3:C:\\tree\\doc\\parts\\one.tex\nOutput:pdf\nMagnification:1000\nUnit:1\nX Offset:0\nY Offset:0\nContent:\n!413\n{1\n[1,7:4736286,46220574:26673152,41484288,0\n(1,5:8799518,10300473:22609920,455111,127431\nx1,5:10179418,10300473\ng1,4:10397871,10300473\nx1,4:13314231,10300473\nx2,9:13532684,10300473\ng1,5:31409438,10300473\n)\n]\n}1\n{2\n(3,2:8799518,8865054:22609920,647495,0\nx3,1:9330359,8865054\n)\n}2\n";
        let names = |name: &str| match name.replace('\\', "/").as_str() {
            "C:/made/paper.tex" => Some("doc/paper.tex".to_string()),
            "C:/tree/doc/parts/one.tex" => Some("doc/parts/one.tex".to_string()),
            _ => None,
        };
        let boxes = boxes_of(sync, &names);
        assert_eq!(boxes.len(), 2, "{boxes:?}");
        let first = &boxes[0];
        assert_eq!((first.page, first.file.as_str()), (1, "doc/paper.tex"));
        assert_eq!(first.lines.iter().map(|(_, line)| *line).collect::<Vec<_>>(), vec![5, 4, 5], "one mark where the line changes, none from the class");
        let point = |sp: f64| sp / 65536.0 * 72.0 / 72.27;
        assert!((first.x - point(8799518.0)).abs() < 0.01);
        assert!((first.y - point(10300473.0 - 455111.0)).abs() < 0.01, "the top, the baseline less the height");
        assert!((first.h - point(455111.0 + 127431.0)).abs() < 0.01, "the height and the depth");
        assert_eq!((boxes[1].page, boxes[1].file.as_str(), boxes[1].lines.len()), (2, "doc/parts/one.tex", 1));
    }

    #[test]
    fn the_log_s_errors_are_read_with_their_files_and_lines() {
        let names = Names::new(Path::new("C:/tree"), Path::new("C:/kept/abc"), "paper", "doc/paper.tex");
        let log = "This is pdfTeX\n./paper.tex:12: Undefined control sequence.\nl.12 \\foo\nC:/tree/doc/parts/one.tex:3: Missing $ inserted.\nC:/tex/x.sty:40: LaTeX Error: Option clash.\npaper.tex:12:  ==> Fatal error occurred, no output PDF file produced!\n";
        let errors = errors_of(log, &names);
        assert_eq!(
            errors,
            vec![
                Failure { file: "doc/paper.tex".into(), line: Some(12), text: "Undefined control sequence.".into() },
                Failure { file: "doc/parts/one.tex".into(), line: Some(3), text: "Missing $ inserted.".into() },
                Failure { file: "C:/tex/x.sty".into(), line: Some(40), text: "LaTeX Error: Option clash.".into() },
            ]
        );
    }

    #[test]
    fn a_file_is_made_into_pages_with_its_lines_placed_and_a_failed_one_gives_its_errors() {
        let root = scratch("make");
        let kept = root.join("kept");
        std::fs::write(root.join("doc/parts/one.tex"), "Text of the part.\n").unwrap();
        let text = "\\documentclass{article}\n\\begin{document}\nThe first page.\n\\newpage\nThe second page.\n\\input{parts/one}\n\\end{document}\n";
        let made = match make_in(&root, "doc/paper.tex", text, &kept) {
            Err(said) if said.contains("is not found: File, Toolchains") => {
                eprintln!("{said}");
                assert!(program("pdflatex").is_none() || program("pdftocairo").is_none(), "{said}");
                return;
            }
            other => other.unwrap(),
        };
        assert!(made.errors.is_empty(), "{:?}", made.errors);
        assert_eq!(made.pages.len(), 2);
        assert!(made.defs.contains("tex-glyph"), "the pages share their glyphs");
        assert!(made.boxes.iter().any(|placed| placed.page == 1 && placed.file == "doc/paper.tex" && placed.lines.iter().any(|(_, line)| *line == 3)), "line 3 on page 1: {:?}", made.boxes);
        assert!(made.boxes.iter().any(|placed| placed.page == 2 && placed.file == "doc/paper.tex" && placed.lines.iter().any(|(_, line)| *line == 5)), "line 5 on page 2");
        assert!(made.boxes.iter().any(|placed| placed.page == 2 && placed.file == "doc/parts/one.tex"), "the part input from beside the file");
        assert!(!root.join("doc/paper.pdf").exists() && !root.join("doc/paper.aux").exists(), "nothing written into the tree");
        std::fs::write(root.join("doc/paper.tex"), text).unwrap();
        let failed = make_in(&root, "doc/paper.tex", "\\documentclass{article}\n\\begin{document}\n\\nosuchcommand\n\\end{document}\n", &kept).unwrap();
        assert!(failed.pages.is_empty());
        assert_eq!(failed.errors.first().map(|error| (error.file.as_str(), error.line)), Some(("doc/paper.tex", Some(3))), "{:?}", failed.errors);
        let _ = std::fs::remove_dir_all(&root);
    }
}
