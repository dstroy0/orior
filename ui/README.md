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
| the languages and file types | `src/lng/*.tsv` and `src/cu/includes/file_defs/*/*.oracle.tsv` |

A job's description is the opening comment or docstring of its own file.

## The protocol panel

The protocol is one job, the course, which runs the passes in order and is the Protocol menu's one entry, the Protocol button of the strip and the one job the Run view lists under protocol; its other commands, a query, a learn pass, a cross, a write or the pairs, are the command line's. A protocol job launched either way shows its run as it goes in the protocol panel, in place of a console tee. The panel reads the run where it is written (the table's run section): the run's buffer in shared memory through `qry_read`, from where its last read stopped, each blob's sequence, time, priority, member, size and name, in the order the writer placed them. The panel is the run's telemetry, for an engineer tuning the protocol.

The panel keeps the job's option fields at the top as the run started with them. Under them the console box holds the run as tabs: the overview and the console. The overview is the run's line: a circle for each step holding its name, the fuse between them burnt behind the step under way and afire at it, part way to the next by how far the step's own pass has come, taken from the run's blobs as each round's questions and answers are placed, and a step the course stopped at in the error's color. The course's steps are read off its console lines, `course: <n> steps: ...` before the first and `course: step <k> of <n>: <name>` at each; a pass run alone shows its own steps, Tools, Run, Carried in, Plan, its rounds, Written and Leave. Under the line one line of words says the step and its round, the answers of the questions, the time gone, and the questions withheld, refused and faulted and the panics where there are any, counted off the console and the run's buffer.

A run whose buffer the window cannot read leaves the panel to the console.

Checks: the app's harness holding that the line's nodes are reached from a recorded run's blobs, that a plan of many rounds is one stretch, and that a plan read late counts the earlier rounds as rounds; and `orior protocol learn dry 1 --leave none` as the end-to-end run with nothing launched.

## Precepts

1. Never cause the user friction: a snappy response, and the user does what they need to while we background our processes.
2. Always give stateful, graceful errors.
3. Do not annoy the user: if they are trying to do something, notify them and leave them alone.

## How a feature is made

1. The feature is built.
2. Its test goes in `test/`, which mirrors `gui/src-js/` file for file: `gui/src-js/x.js` is tested by `test/src/x.test.js`. A Rust module keeps its unit tests in its own `tests` module, as Rust's convention has it, and a test that spans modules goes in its crate's `tests/` folder.
3. A feature of the window is tested through the window's debugging port, every way it can be used: each press, key, menu item, drag, hover and scroll, each state it can be in, and each error it can give, checked for what it shows and what it does.
4. `test/harness.rs` runs every test, the crates' and the window's, and names each source file with no test of its own. The walker of the menus, `test/walk.py`, and the memory watcher, `test/memory.py`, run over every command.
5. The feature is committed and pushed once all of it passes.

## Testing it

```
cargo run --manifest-path ui/test/Cargo.toml -- [--only TEXT] [--monitor N]
```

The harness starts the servers the run's tests reach, read from the tests' text, each a Docker
container of its own on this machine's loopback: PostgreSQL, MySQL, MariaDB, CUPS with three
printers, and an ssh server for tunnels. A server no test of the run reaches is not started, and
the ones started are taken away as the run ends. Without Docker the tests that reach them do not
run, and the report names them. It runs
each crate's tests, then the command line's crate's tests again in Debian, Ubuntu, Fedora, Alpine
and Arch Linux and in Debian on aarch64, each a Docker image of `test/systems/`; `--no-systems`
leaves those out. A Mac's system and Windows on Arm run only on machines of their own, and the
crate is built for each of them in their place. Then it runs the window's tests, in orior built for
the run with a home and a tree of its own: a run leaves the reader's orior as it was. The window
goes on the highest numbered monitor that is not the main one, or the one `--monitor` or
`ORIOR_TEST_MONITOR` names; `--monitors` lists them. The run fails while any test fails or any
source file has no test, and its report is in `build/ui_test/runs/<time>/report.md` at the top of
the tree.

On Windows the window is driven through its debugging port. On Linux its web view is WebKitGTK,
and the window is driven through WebKitWebDriver: every press, key, drag and wheel goes as
WebDriver's actions. `--docker` runs the window's tests in the harness's own Docker image, from
`test/docker/`, on any machine Docker runs on. The tree is brought into a volume of the
container's own, the window is built there and drawn on a display in memory, and the report comes
back to `test/runs/`. Every item of every menu and every option of Preferences is pressed there as
a person presses it. While a run goes, a browser on this machine shows its window at
`http://localhost:6080/vnc.html`, to be watched and not driven. `.github/workflows/ui.yml` runs the same on each pull request that changes the
app, beside CodeQL and SonarQube Cloud.

## Running it

It needs Rust 1.77 or later to build. Every line of the page is in `gui/src-js/`, written for this tree,
and the window loads those files as they are. None of the page comes from outside the tree.

Every cargo build of the tree runs from the top of the tree and lands in `build/cargo`, the
window's with it: nothing is built from inside `ui/gui/src-tauri`, and no `target` folder is made
there.

```
cargo run --release --manifest-path ui/gui/src-tauri/Cargo.toml
```

The program is `build/cargo/release/orior`. The installers come from the Tauri command, which cargo
installs once and which finds `src-tauri` under the folder it is run in:

```
cargo install tauri-cli --version "^2" --locked
cd ui/gui
cargo tauri build
```

They land under `build/cargo/release/bundle/`, for the platform the build runs on. Each
published release carries them for Windows, macOS and Linux, built by `.github/workflows/app.yml`.
Without a clone, `cargo install --git https://github.com/dstroy0/orior orior-ui --locked` builds and
installs the program `orior`. A plain cargo build puts the page inside the program, as the installers
do; `cargo tauri dev` serves it from `gui/src-js/` as it changes.

orior files the errors it meets as issues on dstroy0/orior on its own, and asks once whether to,
yes the answer given by default. The Windows installer asks as it installs (`gui/src-tauri/windows/hooks.nsh`);
every other install asks on its first run, in the window or at the terminal. A build for testing,
`cargo tauri dev`'s, answers yes on its own and asks nothing: every error a test meets is filed as an
issue. Help, Automatic Error Reports turns it on or off later, and `ORIOR_NO_REPORTS` turns it off
for a run.

| platform | also needs |
| --- | --- |
| Windows | WebView2, which Windows 10 and 11 carry, and Git for Windows for the bash the scripts run in |
| macOS | the Xcode command line tools |
| Linux | `webkit2gtk-4.1`, `libayatana-appindicator3` and `librsvg2`, by their names in the distribution |

On Windows the window keeps direct composition, which draws the page on the GPU: a fast scroll
through a long file holds the display's own rate, 142 frames a second on a display of 144. Without
it the page is drawn by the processor, about 15 ms a frame for a window of 2560 by 1440, and the
same scroll holds 48. The features `tauri.conf.json` turns off are the ones the window turns off
when it is given no line of its own.

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
| each plugin's own settings | `plugin_settings/`, a file each, named for the plugin |
| the stylesheet laid over the window's own | `user.css` |
| the toolchains the reader added | `user_toolchains.json` |
| the panes, each tree's tabs and the files opened last, the places left in them, bookmarks, breakpoints, watches and the values each job last ran with | `state.json` |
| the text of each tab with changes not saved | `backups/`, a file each |
| each file's Local History | `history/` |
| the repositories File, Open, Repository clones | `repositories/` |
| the hashes of the files each deployment last sent to each place | `deployed/`, a file each |
| each taking of another machine's settings, the files it wrote over as they were | `carried/`, a folder each |
| each address orior has reached off this machine, when and what for, the last thousand | `network.jsonl` |
| the reader's language files, which give the window's texts in another language | `languages/`, `<tag>.json` each |

`settings.json` and `state.json` hold each entry under its own key, as JSON. orior reads the folder
as it starts, which takes in an edit made while it was closed; a key taken out goes back to its
default.

A tree's own settings are in `.orior/settings.json` at its top folder, and stand over the reader's
as the tree opens and each time the file changes; Preferences, Settings in force, shows each setting
a file sets, its value, and the file it comes from. A settings file, the tree's or `settings.json`,
is JSON with comments and trailing commas, and has keys of its own beside its settings: `takes`, the
path of another settings file, from the file's folder, whose settings it changes; `windows`, `linux`
and `macos`, the settings for that system alone; `machine`, in `settings.json`, the settings for
this machine alone; and `[kind]`, as `[markdown]`, the theme of each
scheme, `theme.dark` and `theme.light`, the editor shows that language's files in. Text in a value
names another setting's value as `${setting:name}`, a variable of the system's environment as
`${env:NAME}`, and the home folder, the tree's folder and the system as `${home}`, `${tree}` and
`${system}`. A tree's file does not set `signing.`, `environment.`, `checkers` or `titlebar`, which
run programs or name the reader's own.

```json
{
  "takes": "../shared/settings.json",
  "whitespace": "true",
  "windows": { "zoom": 1.25 },
  "[markdown]": { "theme.dark": "Light" },
  "fonts": { "--code": "${env:CODE_FONT}" }
}
```

A theme's file in `themes/` holds its name, the scheme it is drawn over, and its colors, among them
one for any kind of token a grammar names, as `--t-string-escape`; and, where it has them, `image`,
a picture laid behind the editor as a data address with the share of it that shows, and `icons`,
each a pattern of files' names, the mark the explorer shows for them and its color. Preferences,
Colors, marks each color whose contrast with the color under it is under 4.5:1.

```json
{
  "name": "Night",
  "base": "dark",
  "colors": { "--t-string-escape": "#ffb86c" },
  "image": { "src": "data:image/png;base64,…", "opacity": 0.15 },
  "icons": [{ "pattern": "*.test.js", "mark": "T", "color": "--t2" }]
}
```

Preferences, Settings on Every Machine, sends `settings.json`, the themes, the plugins with each
one's own settings, `menus.json` and `user.css` to a file or to a folder of the reader's, a
repository's commits made and pushed, and takes them in from one on another machine. What a taking
would change shows first, a file at a time and `settings.json` a setting at a time, and nothing is
written until the reader takes what is chosen; each taking is kept in `carried/` and undone from
the list there. `machine`, `signing.`, `environment.`, `memory-budget`, `cores-kept` and
`carry.place` stay on the machine that set them.

orior reaches an address off this machine only for what the reader asked of it: a request sent, a
database connected, a debug adapter joined, a command run over ssh, a repository cloned, pulled or
pushed, and an error report the reader let file on its own; it sends nothing about the reader. Help,
Off the Network, kept as `network`, holds it off the network entirely, each reach refused with what
it was for, and a tree's settings file does not set it. Help, Network… lists each address reached,
when and what for.

The window shows in the reader's language where a language file gives it: `languages/<tag>.json`,
each of the window's English texts with the reader's for it, as `{ "File": "…" }`, the tag the
setting `language` names, chosen in Preferences, or the system's. A text the file does not give
stays in English, and the editor's text, the terminal's and the files' names are never changed. In
the editor, a line where Hebrew, Arabic or another script that runs right to left meets text that
runs left to right is drawn in the order it is read, the arrows move by that order, and a press or a
selection lands where the page draws each character; the word keys count a letter's marks as part of
its word.

File, Plugins reads a catalog of plugins others publish, at an address the reader names on the web or
on this machine, and installs a plugin the search finds from its package, a JSON file of its files by
name, where its hash and its version are those the catalog gives. A script plugin's plugin.json
says what it reads and writes, as patterns of the tree's files, and the hosts it reaches; it runs
only once the reader lets it, Install letting it as the catalog showed, and each call is held to
what it asked for. A plugin is updated only by Update, and an update that asks for more waits for
Let It Run.

```json
{ "orior": "plugin catalog", "plugins": [{ "id": "word-count", "name": "Word Count", "version": "1.2.0", "about": "Counts a file's words.", "package": "word-count-1.2.0.json", "sha256": "…", "reads": ["**/*.md"], "writes": [], "reaches": [] }] }
```

A tree names its deployments in `deploy.json` at its top folder, each a job of Build: its name,
the folder whose files go, the patterns of the files that stay, the places, and how many releases
each place keeps.

```json
{"deployments": [{"name": "site", "from": "dist", "exclude": ["*.map", "drafts/"], "to": ["me@web1:/srv/site", "docker:web:/srv/site"], "keep": 5}]}
```

A place is an address as File, Open, Folder on Another Machine… takes one, or a folder on this
machine. A machine takes each deployment as a new release under `releases/`, and `current` is
turned to it once every place holds it; `"inPlace": true` sends the files into the folder itself, as
a folder on this machine always takes them. The hashes of the files last sent to each place are
kept in `deployed/` in orior's own folder, to tell a file changed at a place since.

The programs a tree's builds make are signed where `settings.json` names how: `signing.windows`, a
certificate's thumbprint in the reader's store or the path of a `.pfx` file, its password taken from
`ORIOR_SIGN_PASSWORD` and from no file; `signing.timestamp`, the address of a timestamp server;
`signing.macos`, the identity `codesign` signs with; and `signing.notarize`, the keychain profile
`notarytool` sends the program with.

## Packages for Linux

`packaging/linux/` builds orior as a Flatpak and as a snap. The Flatpak's manifest,
`com.dstroy0.orior.yml`, builds the window and the command line offline against GNOME's runtime,
the crates listed in `cargo-sources.json`, which `python -I packaging/linux/cargo_sources.py` writes
again from the two Cargo.locks after a crate changes. `sh packaging/linux/check_flatpak.sh`, run
from this folder, checks the manifest, the desktop file, the metadata and the crates' list in a
container of 172 MB, and fetches no runtime: Flathub builds the Flatpak from the manifest. The snap's
`snapcraft.yaml` builds them on core24 with classic confinement.

Inside the Flatpak's sandbox none of the machine's compilers, SDKs and tools are there: orior gives
each program on the machine's PATH a stand-in that runs it on the machine through `flatpak-spawn
--host`, in the folder it was started in, and puts their folder first on its PATH as it starts:
the tree's jobs and tools, the language servers, git and the terminal's shell run on the machine
itself. A variable a run sets reaches the machine's program where `ORIOR_HOST_ENV` names it.

## The command line

The program is `orior`, and it is the command line as well as the window. Given no words it opens
the window; given words it runs them in the terminal, over the same jobs and the same bridge. A
menu's title and one of its commands are the words for it, read from `cli/commands.json`, the file
the menus are drawn from as well. [The app](../../README.md#the-app) in the repository's own page
is the guide to the window and the command line, and `orior help` lists every word.

From inside the tree, `cargo run -- run list` runs it without an install.
