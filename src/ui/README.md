# The orior app

One window for the engine. **Run** starts every build, ingest, run, render, sim, viewer, stage pipeline
and test suite in the tree, shows their output as it arrives, and opens each page a viewer writes in a
window of its own. **Edit** is the editor for `.g`, `.gsm` and the k-files, with each file type's
definition beside the file. In a file of `src/cu/transpiler/lstar/protocol/table/` the bridge shows
beside it as well: for the key under the cursor, its pairs and their verdicts in `Lstar.klq`, its
name in each `.klm`, and each ruleset's entry of that name.

The app reads all of it from the tree each time:

| what | read from |
| --- | --- |
| the builds | every `build*.sh` under `src/`, `utils/maint/engine/`, `utils/maint/texbuild/` and each example |
| the protocol | every `klq_*.sh` under `utils/maint/engine/`: its modes from its usage lines, and its settings, `KLQ_TRACE` and `KLQ_SEED`, from its opening comment, each set in the script's environment |
| ingest, the run parts, render | `examples/cell_tracking/src/track_driver/track_driver.cu`, its usage text |
| the sims | the `case` in `src/sims/run.sh` |
| the other runs | `examples/navier_stokes/run.sh` and its cfgs, `examples/qasm/run.sh` and its circuits |
| the viewers | every `build_*_view.py` under `examples/` |
| the stages and the pipelines | `examples/<subject>/<n>_<stage>/*.py` |
| the tests | every `run.sh` under `utils/test/` and each example's `test/` |
| the languages and file types | `src/lng/*.tsv` and `src/cu/types/file_defs/*/*.oracle.tsv` |

A job's description is the opening comment or docstring of its own file.

## Running it

It needs Rust 1.77 or later to build. Every line of the page is in `src/`, written for this tree,
and the window loads those files as they are. None of the page comes from outside the tree.

```
cd src/ui/src-tauri
cargo run
```

The installers come from the Tauri command, which cargo installs once:

```
cargo install tauri-cli --version "^2" --locked
cd src/ui/src-tauri
cargo tauri build
```

They land under `src-tauri/target/release/bundle/`, for the platform the build runs on. Each
published release carries them for Windows, macOS and Linux, built by `.github/workflows/app.yml`.
Without a clone, `cargo install --git https://github.com/dstroy0/orior orior-ui --locked` builds and
installs the program `orior`. A plain cargo build puts the page inside the program, as the installers
do; `cargo tauri dev` serves it from `src/` as it changes.

orior files the errors it meets as issues on dstroy0/orior on its own, and asks once whether to,
yes the answer given by default. The Windows installer asks as it installs (`src-tauri/windows/hooks.nsh`);
every other install asks on its first run, in the window or at the terminal. A build for testing,
`cargo tauri dev`'s, answers yes on its own and asks nothing: every error a test meets is filed as an
issue. Help, Automatic Error Reports turns it on or off later, and `ORIOR_NO_REPORTS` turns it off
for a run.

| platform | also needs |
| --- | --- |
| Windows | WebView2, which Windows 10 and 11 carry, and Git for Windows for the bash the scripts run in |
| macOS | the Xcode command line tools |
| Linux | `webkit2gtk-4.1`, `libayatana-appindicator3` and `librsvg2`, by their names in the distribution |

On Windows the window starts with `--disable-direct-composition`, set in `tauri.conf.json`. With
direct composition on, a window whose page moves holds to 60 frames a second whatever the display's
rate, and stalls for up to half a second every few seconds. The first three features that line
turns off are the ones the window turns off when it is given no line of its own.

The app works on the tree it is started in, or the one `ORIOR_ROOT` names. `ORIOR_BASH` and
`ORIOR_PYTHON` name the bash and the Python the jobs run with, where the ones on the path are not the
ones to use.

## orior's own folder

orior keeps what it carries from one run to the next in a folder of its own: `orior` in `%APPDATA%`
on Windows, in `$XDG_CONFIG_HOME` or `~/.config` elsewhere, or the folder `ORIOR_HOME` names.

| what | where in it |
| --- | --- |
| the settings Preferences and the menus set, the macros kept, and whether errors file on their own | `settings.json` |
| each theme of the reader's | `themes/`, a file each, named for the theme |
| the reader's plugins | `plugins/`, a folder each |
| the stylesheet laid over the window's own | `user.css` |
| the toolchains the reader added | `user_toolchains.json` |
| the panes, each tree's tabs and the files opened last, the places left in them, bookmarks, breakpoints, watches and the values each job last ran with | `state.json` |
| the text of each tab with changes not saved | `backups/`, a file each |
| each file's Local History | `history/` |
| the repositories File, Open, Repository clones | `repositories/` |

`settings.json` and `state.json` hold each entry under its own key, as JSON. orior reads the folder
as it starts, which takes in an edit made while it was closed; a key taken out goes back to its
default.

## The command line

The program is `orior`, and it is the command line as well as the window. Given no words it opens
the window; given words it runs them in the terminal, over the same jobs and the same bridge. A
menu's title and one of its commands are the words for it, read from `cli/commands.json`, the file
the menus are drawn from as well. [The app](../../README.md#the-app) in the repository's own page
is the guide to the window and the command line, and `orior help` lists every word.

From inside the tree, `cargo run -- run list` runs it without an install.
