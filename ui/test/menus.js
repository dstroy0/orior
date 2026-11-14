// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What the tests of the menus share: the window as each test file found it, put back by its last test; the
// commands a test runs; and the presses, menus and sheets the tests make and read.

import { allCommands, runCommand } from "../gui/src-js/menubar.js";
import { invoke } from "../gui/src-js/bridge.js";
import { closeMenu } from "../gui/src-js/menu.js";
import { editing, openFile, treeRowShown } from "../gui/src-js/edit.js";
import { toggleTerminal } from "../gui/src-js/terminal.js";
import { showView } from "../gui/src-js/views.js";
import { closeDiff } from "../gui/src-js/diffview.js";
import { closeChange } from "../gui/src-js/changeview.js";
import { noticesWaiting, noticesSaid } from "../gui/src-js/statusbar.js";
import { zoom } from "../gui/src-js/zoom.js";
import { closePeek } from "../gui/src-js/peek.js";
import { closeBoard } from "../gui/src-js/gather.js";
import { panesNow, putPanes, showGroup, displayedGroup } from "../gui/src-js/explorer.js";
import { debugging, toggleDebugPanel } from "../gui/src-js/debug.js";

const { assert, ui } = window.__harness;

// The page's storage as the file found it, put back by its last test.
export const STORED = Object.entries(localStorage);

// The explorer's group and panes as the file found them, put back by its last test.
export const FOUND = { group: displayedGroup(), panes: panesNow() };

export const wait = (ms) => new Promise((done) => setTimeout(done, ms));

export const openMenus = () => [...document.querySelectorAll("body > .menu")];

export const itemsIn = (menu) => [...menu.querySelectorAll(":scope > .menu-item")];

export const labelOf = (button) => button.querySelector("span")?.textContent ?? "";

// The window's size, its zoom and what the page sees of it, said where a press misses.
export const viewSaid = () => `the window ${innerWidth}x${innerHeight} at zoom ${zoom()}, the page seen ${Math.round(visualViewport.width)}x${Math.round(visualViewport.height)} at scale ${visualViewport.scale}, ${devicePixelRatio} pixels to a point`;

// Every menu closed, every sheet and dialog closed, a finding of the slow plugin stopped, and the
// keys let go.
export async function putBack() {
  closeMenu(false);
  document.querySelectorAll("dialog[open]").forEach((dialog) => dialog.close());
  [...document.querySelectorAll("#slow-finder:not([hidden]) .slow-finder-button")].find((button) => button.textContent === "Stop")?.click();
  await ui.release("Ctrl+Shift+Alt").catch(() => {});
  await wait(60);
}

// Opens the menu of the bar's title at `at` and, through it, each submenu `path` names, pressing
// each as a person does, and gives the items of the menu at the end.
export async function openTo(at, path) {
  await putBack();
  const title = document.getElementById("menubar").children[at];
  const named = (depth) => [title.textContent.trim(), ...path.slice(0, depth)].join(" / ");
  await ui.click(title);
  let menu = await ui.waitFor(() => openMenus()[0], 3000).catch(() => {
    throw new Error(`${named(0)} did not open; ${viewSaid()}, the title at ${JSON.stringify(title.getBoundingClientRect())}`);
  });
  for (const [depth, label] of path.entries()) {
    const item = itemsIn(menu).find((button) => labelOf(button) === label);
    assert.ok(item, `${named(depth + 1)} is in its menu`);
    const heard = [];
    const hear = (event) => heard.push(`${event.type}:${event.target?.className || event.target?.nodeName || "window"}${event.type === "click" ? `[${event.target?.textContent?.trim()}]` : ""}:${Math.round(performance.now())}`);
    const types = ["pointermove", "pointerdown", "pointerup", "click", "focusout", "blur", "resize", "keydown"];
    types.forEach((type) => window.addEventListener(type, hear, true));
    const aimed = item.getBoundingClientRect();
    await ui.click(item);
    heard.push(`aimed at ${label} at ${Math.round(aimed.top)}, now at ${Math.round(item.getBoundingClientRect().top)}`);
    menu = await ui.waitFor(() => openMenus()[depth + 1], 3000).catch(() => {
      const open = openMenus().map((one) => `${itemsIn(one).length} items at ${Math.round(one.getBoundingClientRect().top)}`);
      throw new Error(`${named(depth + 1)} did not open: ${open.length} menus open (${open.join(", ")}), the window ${innerWidth}x${innerHeight}; heard ${heard.join(" ")}; closed by ${(window.__removedMenus ?? []).slice(-3).join(" ||| ")}`);
    }).finally(() => types.forEach((type) => window.removeEventListener(type, hear, true)));
  }
  return itemsIn(menu);
}

// Each item that runs something, by the path of labels that reaches it from the title at `at`:
// every item of every submenu, opened as a person opens it.
export async function leavesOf(at, path = []) {
  const found = [];
  for (const button of await openTo(at, path)) {
    const label = labelOf(button);
    if (button.querySelector(".menu-more")) {
      found.push(...(await leavesOf(at, [...path, label])));
    } else if (!button.disabled) {
      found.push([...path, label]);
    }
  }
  return found;
}

// Each command of the menus, run as its menu runs it and as a command line runs it, and what it does
// looked for: COVERED names every command a test below runs, and the first test fails for any command
// of the menus not among them.
export const COVERED = new Set();

// The settings turned on and off by a command that takes on or off, with the words that say each.
export const SETTINGS = {
  "auto-save": ["on", "off"],
  "format-on-save": ["on", "off"],
  "operator-next-line": ["on", "off"],
  "column-mode": ["on", "off"],
  "side-bar": ["show", "hide"],
  "auto-collapse": ["on", "off"],
  "sticky-scroll": ["on", "off"],
  "word-wrap": ["on", "off"],
  "smooth-scroll": ["on", "off"],
  "reduce-motion": ["on", "off"],
  "flick-scroll": ["on", "off"],
  breadcrumbs: ["on", "off"],
  "bracket-pairs": ["on", "off"],
  "type-hints": ["on", "off"],
  "parameter-hints": ["on", "off"],
  whitespace: ["on", "off"],
  "past-ends": ["on", "off"],
  "hide-comments": ["on", "off"],
  toolbar: ["on", "off"],
  "margin-line": ["on", "off"],
  "focus-follows": ["on", "off"],
  "system-title": ["on", "off"],
  "strip-edge": ["on", "off"],
  "side-tabs": ["on", "off"],
  "scheme-system": ["on", "off"],
  "fuzzy-files": ["on", "off"],
  "parallel-tests": ["on", "off"],
  "line-history": ["on", "off"],
  "git-authors": ["on", "off"],
  "network-off": ["on", "off"],
};

Object.keys(SETTINGS).forEach((command) => COVERED.add(command));

COVERED.add("auto-report");

// The editor's commands on text: the text and selections each is given and those it leaves, each
// selection written between [ and ] and each cursor as |.
export const ON_TEXT = {
  "select-all": ["a|b\ncd", "[ab\ncd]"],
  "select-line": ["a|b\ncd", "[ab\n]cd"],
  "copy-line-up": ["a|b\ncd", "a|b\nab\ncd"],
  "copy-line-down": ["a|b\ncd", "ab\na|b\ncd"],
  "move-line-up": ["ab\nc|d", "c|d\nab"],
  "move-line-down": ["a|b\ncd", "cd\na|b"],
  "join-lines": ["ab|\n  cd", "ab cd", "text"],
  "sort-ascending": ["[c\na\nb]", "a\nb\nc", "text"],
  "sort-descending": ["[a\nc\nb]", "c\nb\na", "text"],
  "unique-lines": ["[a\nb\na]", "a\nb", "text"],
  "upper-case": ["[ab] cd", "[AB] cd"],
  "lower-case": ["[AB] cd", "[ab] cd"],
  "title-case": ["[hello world]", "[Hello World]"],
  "cursor-above": ["ab\nc|d", "a|b\nc|d"],
  "cursor-below": ["a|b\ncd", "a|b\nc|d"],
  "next-occurrence": ["x|y xy xy", "[xy] xy xy"],
  "all-occurrences": ["x|y xy z", "[xy] [xy] z"],
  bracket: ["|(ab)", "(ab)|"],
};

Object.keys(ON_TEXT).forEach((command) => COVERED.add(command));

["undo", "redo", "comment", "fold-all", "unfold-all", "line", "expand-selection", "shrink-selection"].forEach((command) => COVERED.add(command));

export const GIVEN = "commands_given.py";

// The editor of GIVEN, holding `marked`'s text with its selections.
export async function given(marked) {
  if (editing().active !== GIVEN) {
    await invoke("file_write", { path: GIVEN, text: "" });
    showView("edit");
    await openFile(GIVEN);
    await ui.waitFor(() => editing().active === GIVEN && editing().editor?.s, 5000, "the given file's editor");
  }
  const view = editing().editor;
  const marks = [];
  let text = "";
  let line = 0;
  let col = 0;
  let open = null;
  for (const char of marked) {
    if (char === "|") {
      marks.push({ anchor: { line, col }, head: { line, col }, goal: null });
    } else if (char === "[") {
      open = { line, col };
    } else if (char === "]") {
      marks.push({ anchor: open, head: { line, col }, goal: null });
      open = null;
    } else {
      text += char;
      if (char === "\n") {
        line += 1;
        col = 0;
      } else {
        col += 1;
      }
    }
  }
  view.change([{ from: { line: 0, col: 0 }, to: view.doc.end(), text }], null);
  view.select(marks.length ? marks : [{ anchor: { line: 0, col: 0 }, head: { line: 0, col: 0 }, goal: null }]);
  return view;
}

// The editor's text with its selections written as `given` reads them.
export function marked() {
  const view = editing().editor;
  const lines = [...view.doc.lines];
  const at = [];
  for (const sel of view.s.selections) {
    const forward = sel.anchor.line < sel.head.line || (sel.anchor.line === sel.head.line && sel.anchor.col <= sel.head.col);
    const [from, to] = forward ? [sel.anchor, sel.head] : [sel.head, sel.anchor];
    if (from.line === to.line && from.col === to.col) {
      at.push([from, "|"]);
    } else {
      at.push([from, "["], [to, "]"]);
    }
  }
  at.sort((a, b) => b[0].line - a[0].line || b[0].col - a[0].col);
  for (const [{ line, col }, mark] of at) {
    lines[line] = lines[line].slice(0, col) + mark + lines[line].slice(col);
  }
  return lines.join("\n");
}

export const frame = () => new Promise((done) => requestAnimationFrame(() => done()));

// Runs `command` with `words` through the command line, on this window's tree, as a reader runs it at
// a terminal, and waits for it: the window open there runs it as its menu does, and answers.
export async function press(command, ...words) {
  const item = allCommands().find((one) => one.command === command);
  assert.ok(item, `${command} is a command of the menus`);
  const told = window.__harness.servers ?? {};
  assert.ok(told.ORIOR_TEST_PROGRAM, "the harness names the window's program");
  await ui.run(told.ORIOR_TEST_PROGRAM, [item.menu.toLowerCase(), command, ...words], { env: { ORIOR_HOME: told.ORIOR_TEST_HOME, ORIOR_ROOT: told.ORIOR_TEST_TREE, ORIOR_NO_REPORTS: "1" } });
  await frame();
}

export const calls = window.__harness.calls;

export const closeSheets = () => document.querySelectorAll("dialog[open]").forEach((dialog) => dialog.close());

// The sheet holding `selector`, waited for and then closed.
export async function sheetShown(selector) {
  try {
    return await ui.waitFor(`dialog[open] ${selector}`, 5000, selector);
  } finally {
    closeSheets();
  }
}

// Whether the window has told the reader something `pattern` matches.
export const told = (pattern) => [...noticesSaid(), ...noticesWaiting()].some((one) => pattern.test(one.text));

// The calls named `name` the window has made since the test began.
export const made = (name) => calls.made.filter((one) => one.name === name);

export const PANELS = { "terminal-view": "term", "debug-view": "debug", "profile-view": "profile", "containers-view": "containers", "database-view": "database", "comms-view": "comms" };

Object.keys(PANELS).forEach((command) => COVERED.add(command));

["edit-view", "run-view", "palette", "focus", "preview", "preview-alone", "float", "solo", "next-display", "split-right", "split-down", "unsplit", "move-to-split", "open-in-split", "scheme", "zoom-in", "zoom-out", "zoom-reset", "memory-budget", "checkers", "find-slow-plugin"].forEach((command) => COVERED.add(command));

["new", "toggle", "split", "clear", "kill", "keys", "report", "network", "whats-new", "about"].forEach((command) => COVERED.add(command));

["definition", "peek-definition", "peek-usages", "peek-callers", "usages", "calls"].forEach((command) => COVERED.add(command));

["back", "forward", "last-editor", "search-everywhere", "recent-files", "file", "symbol", "symbol-tree", "jump", "mark-file", "marked-files", "bookmark", "bookmarks", "bridge"].forEach((command) => COVERED.add(command));

[1, 2, 3, 4, 5, 6, 7, 8, 9].forEach((n) => COVERED.add(`mark-${n}`));

["gather-lines", "gathered-lines", "next-problem", "previous-problem", "next-change", "previous-change", "find", "replace", "next-match", "previous-match"].forEach((command) => COVERED.add(command));

export const PEEKED = "peek.c";

export const PLUGINS_OFF = "orior.plugins.off";

// A C file clangd serves, open with the cursor on the call of helper on its fourth line.
export async function served() {
  await invoke("file_write", { path: PEEKED, text: "int helper(void) { return 1; }\n\nint main(void) {\n  return helper() + helper();\n}\n" });
  showView("edit");
  await openFile(PEEKED);
  await ui.waitFor(() => editing().active === PEEKED && editing().editor?.s, 5000, "peek.c open");
  editing().editor.goTo(3, 10);
}

["create-file", "create-folder", "create-repository", "create-project", "open-file", "open-folder", "open-repository", "open-workspace", "open-remote", "add-folder", "add-file", "new-window", "open-folder-window"].forEach((command) => COVERED.add(command));

["dev-container", "clone", "retain-template", "trust-tree", "save", "save-all", "print", "close-editor", "close-all", "preferences", "user-css", "reader-menus", "plugins", "new-plugin", "reload-plugins", "environment", "maven-settings", "toolchains", "exit"].forEach((command) => COVERED.add(command));

["undo-history", "cut", "copy", "paste", "compare-clipboard", "macro-record", "macro-play", "macro-retain", "search", "shape-search", "format", "docstring", "docstring-form", "sort-methods", "fill-paragraph", "continuation", "doc-margin"].forEach((command) => COVERED.add(command));

["rename", "extract-variable", "extract-constant", "extract-function", "inline-variable", "change-signature", "move-declaration", "quick-fix", "parameter-info", "quick-doc"].forEach((command) => COVERED.add(command));

export const TREE = () => window.__harness.servers.ORIOR_TEST_TREE.replace(/\\/g, "/");

export const HOME = () => window.__harness.servers.ORIOR_TEST_HOME.replace(/\\/g, "/");

// The command line's own commands, run in the terminal: whether one ran, or how it failed.
export const ran = (command, ...words) => press(command, ...words).then(() => "ran", (error) => String(error));

["start", "stop", "list", "show", "run-file", "validate", "disasm-verify", "update-snapshots", "run-statement", "run-sql-file", "send-request", "copy-as-curl", "private-environment"].forEach((command) => COVERED.add(command));

["tests", "run-tests", "run-failed-tests", "cover-tests", "stop-tests", "profile", "profile-remote", "stop-profile", "print-here", "breakpoint"].forEach((command) => COVERED.add(command));

["debug", "continue", "step-over", "step-into", "step-out", "stop-debug", "pause", "restart-debug", "record", "step-back", "reverse-continue", "debug-mixed"].forEach((command) => COVERED.add(command));

["attach-process", "attach-address", "stepping", "export-breakpoints", "import-breakpoints", "show-memory", "watch-memory", "machine-code"].forEach((command) => COVERED.add(command));

["commit-view", "review-structure", "push", "pull", "branches", "new-branch", "stash", "stashes", "cherry-pick", "conflicts"].forEach((command) => COVERED.add(command));

export const QUICK = "build/src/build_quick.sh";

export const SLOW = "build/src/build_slow.sh";

// The protocol's commands, which the command line says and the menu lists under the protocol: each shows the Run
// view with its job chosen; the run's tree holds no protocol jobs, and the job is chosen by its name alone. The
// readings among them, `decode`, print at the command line and are no job of the window's: with no record named the
// command line refuses one.
export const PROTOCOL = ["course", "tables", "decode", "query", "learn", "cross", "write", "pairs", "stick", "retention", "sweep"];

export const PROTOCOL_READINGS = new Set(["decode"]);

PROTOCOL.forEach((command) => COVERED.add(command));

export const DEBUGGED = "debugged.py";

// Ends every debugging session, after a test of the debugger.
export async function endSessions() {
  for (let left = 5; debugging() && left > 0; left -= 1) {
    await runCommand("stop-debug");
    await ui.rest(200);
  }
}

// What the tests above made in the tree, each taken out by the last test.
export const MADE = [
  GIVEN, "peek.c", "served.c", "src/sliced_*", "src/renamed_*", "src/checked_*", "src/retained_*", "src/hello*.py", "src/made_cut.txt", "made_cut.txt", "notebook_cells.py", "notebook_*.ipynb", "compared_*.txt", "history_file.txt", "vars.py", "grouped_*.txt", "debugged.py", "looped.py", "listened.py", "mem.*", "profiled.py", "profiled_calls.py", "test_menus.py", "asked.http", "http-client.private.env.json", ".gitignore", ".orior/settings.json",
  "made_by_command.txt", "made_by_command", "made.code-workspace", "marked_*.txt", "src/build_quick.sh", "src/build_slow.sh", "__pycache__", ".pytest_cache", "build",
];

// The window's explorer as the file found it, and nothing over the window: no board of gathered
// lines, diff, review of a change, peek, sheet, menu, terminal or debugger.
export function asFound() {
  closeMenu(false);
  closeSheets();
  closeBoard();
  closeDiff();
  closeChange();
  closePeek();
  toggleTerminal(false);
  toggleDebugPanel(false);
  showGroup(FOUND.group);
  putPanes(FOUND.panes);
}

// Every item of a menu displayed, its own items' items among them: each with the command it runs.
export const itemsOfMenu = () => [...document.querySelectorAll("body > .menu .menu-item")].map((button) => button.item).filter(Boolean);

export const itemsUnder = (items) => items.flatMap((item) => (item === "-" ? [] : item.items ? itemsUnder(typeof item.items === "function" ? [] : item.items) : [item]));

// The items of the menu a right click on `target` opens.
export async function menuOver(find) {
  closeMenu(false);
  find().scrollIntoView({ block: "center" });
  await ui.frame();
  // Found again, and its menu asked for at once, as a right click asks: a list draws its rows again
  // as it scrolls or changes.
  const target = find();
  const { left, top, width, height } = target.getBoundingClientRect();
  target.dispatchEvent(new MouseEvent("contextmenu", { bubbles: true, cancelable: true, button: 2, clientX: left + Math.min(20, width / 2), clientY: top + height / 2 }));
  await ui.waitFor(() => document.querySelector("body > .menu"), 3000, "the menu");
  const first = document.querySelector("body > .menu .menu-item")?.item;
  const all = [...document.querySelectorAll("body > .menu > .menu-item")].map((button) => button.item);
  closeMenu(false);
  return first ? all : [];
}

["file-open", "file-remove", "file-names", "file-open-window", "file-cut", "file-copy", "file-paste", "file-copy-path", "file-terminal", "file-select-compare", "file-compare"].forEach((command) => COVERED.add(command));

["tab-close", "tab-close-others", "tab-split-right", "tab-split-down", "tab-new-window", "tab-group", "tab-ungroup", "tab-mark", "tab-open-as"].forEach((command) => COVERED.add(command));

export const rowOf = (path) => treeRowShown(path);

export const tabOfName = (path) => [...document.querySelectorAll("#tabs .tab")].find((tab) => tab.title === path);

["pane-show", "explorer-refresh", "explorer-collapse", "outline-pin"].forEach((command) => COVERED.add(command));

["commit-open", "commit-compare", "commit-copy-id", "commit-copy-message", "commit-files", "commit-file-changes", "history-difference", "history-revert"].forEach((command) => COVERED.add(command));

["comment-open", "comment-changes", "comment-resolve", "comment-delete", "comment-copy"].forEach((command) => COVERED.add(command));

["change-show", "job-start", "job-stop", "job-edit", "job-copy-path", "output-copy", "output-copy-all", "output-select-all"].forEach((command) => COVERED.add(command));

["terminal-copy", "terminal-paste", "terminal-close", "status-show", "status-move", "status-reset"].forEach((command) => COVERED.add(command));

["containers-type", "containers-read", "container-logs", "container-run", "container-terminal", "container-open", "container-start", "container-stop", "container-restart"].forEach((command) => COVERED.add(command));

["db-first-rows", "db-copy-name", "db-read-again", "remote-rejoin", "remote-copy-address"].forEach((command) => COVERED.add(command));

["breakpoint-edit", "breakpoint-message", "breakpoint-remove", "debug-exception", "frame-code", "variable-retain", "variable-compare", "variable-watch", "variable-memory", "variable-history"].forEach((command) => COVERED.add(command));

export const VARS = "vars.py";

["tool-dock", "strip-icon", "pane-open", "group-fold", "group-rename", "group-ungroup", "group-close", "mark-up", "mark-let-go", "servers-stop-idle", "macro-keys", "macro-forget"].forEach((command) => COVERED.add(command));

["branch-switch", "branch-merge", "branch-rebase", "branch-rename", "branch-delete", "stash-apply", "stash-pop", "stash-drop"].forEach((command) => COVERED.add(command));

["test-run", "test-cover", "test-debug", "profile-open", "profile-zoom", "profile-zoom-out", "profile-whole", "quick-fix-apply"].forEach((command) => COVERED.add(command));

["prefs-zoom", "prefs-language", "prefs-cores", "prefs-opacity", "prefs-setting", "prefs-font", "prefs-patterns", "prefs-search-sets"].forEach((command) => COVERED.add(command));

["theme-choose", "theme-new", "theme-delete", "theme-rename", "theme-export", "theme-import", "theme-color", "theme-image", "theme-image-share", "theme-image-remove", "theme-icon", "theme-icon-remove"].forEach((command) => COVERED.add(command));

["keys-from", "keys-character", "key-set", "key-off", "key-reset", "notice-show-again", "settings-own", "settings-tree", "carry-place", "carry-send", "carry-take", "carry-undo"].forEach((command) => COVERED.add(command));

export const prefsField = (label) => document.querySelector(`dialog[open] .sheet-prefs [aria-label="${label}"]`);

// The window put back as the test file found it, by the file's last test.
export async function putBackAsFound() {
  const tree = TREE();
  const git = (...words) => ui.run("git", ["-C", tree, ...words]).catch(() => {});
  await press("close-all").catch(() => {});
  await git("clean", "-fdqx", "--", ...MADE);
  await git("checkout", "--", "hello.py", "README.md");
  for (const mounted of await invoke("tree_mounts").catch(() => [])) {
    await invoke("tree_unmount", { name: mounted.name }).catch(() => {});
  }
  asFound();
  // Key by key, as tabvla retains each change in its own folder through the page's own setItem and
  // removeItem.
  const stored = new Map(STORED);
  for (const key of Object.keys(localStorage).filter((one) => !stored.has(one))) {
    localStorage.removeItem(key);
  }
  for (const [key, value] of STORED) {
    localStorage.setItem(key, value);
  }
  assert.deepEqual(Object.entries(localStorage).sort(), [...STORED].sort());
  // tabvla retains the settings in its own folder once they rest, which the next page reads.
  await ui.rest(1500);
}
