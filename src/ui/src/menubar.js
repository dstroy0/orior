// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The menu bar, as commands.json lists it: the list the command line reads too, so that each item
// here is `orior <menu> <command>` there. What each command does in the window is COMMANDS below,
// what it needs before it can act is NEEDS, whether an item that is on or off is on is CHECKS, and
// the keys an item lists are bound here as well, with its `also`, a second key that does the same. A
// key the editor or the terminal takes first stays theirs, and the keys of a command for the editor
// are given to the editor, which acts on them while it holds the keys.
//
// A menu of jobs lists the catalog's groups it names, split by what each job works on where it says
// so, and a group with no job in the tree has no menu. A menu commands.json gives a `strip` icon is
// on the tool strip in place of the bar. A job chosen from a menu shows in the run view with its
// form, and starts only from there or from Run, Start.
//
// A press of the menu bar opens its menu, and while one is open the pointer moving
// to another title opens that one instead. Alt held marks each title's letter, Alt and the letter
// opens the menu, and Alt alone gives the bar the keys: Left and Right step along it, Down, Enter or
// Space opens, and Escape hands the keys back. In an open menu Left and Right step to the menus on
// either side. Past a number of titles set by the window's width, and past what fits, the titles go
// into the last, a menu of menus.

import { invoke, pick } from "./bridge.js";
import { showClone } from "./clone.js";
import { showCreate, showInit, showProject } from "./create.js";
import { checkersNamed, setCheckers } from "./servers.js";
import { lastMemory, memoryBudget, say, setMemoryBudget } from "./statusbar.js";
import { showBookmarks, toggleBookmark } from "./bookmarks.js";
import { askStepping, attachAddress, attachProcess, canStepBack, debugFile, debugging, debugMixed, exportBreakpoints, importBreakpoints, isPaused, recordFile, restartDebug, showMachineCode, showMemory as showDebugMemory, step, stopDebug, toggleBreakpointHere, toggleDebugPanel, watchAddress } from "./debug.js";
import { authorsShown, setAuthorsShown } from "./authors.js";
import { addToWindow, bindEditorKeys, bindReaderKeys, crumbsShown, editing, openAt as openFileAt, openFile, recentFiles, saving, setCrumbs, setSaving, setMarginLine, setVimKeys, marginLine, vimKeys } from "./edit.js";
import { forgetMacro, keepMacro, keptMacros, lastMacro, onMacros, playMacro, recording, setMacroKeys, toggleRecording } from "./macros.js";
import { showPane } from "./explorer.js";
import { openPalette, startPalette } from "./palette.js";
import { fuzzy } from "./fuzzy.js";
import { runAllTests, runFailedTests, setTestsParallel, stopTests, testsParallel } from "./tests.js";
import { showPreferences } from "./preferences.js";
import { showGenerator, showPlugins } from "./pluginsheet.js";
import { loadPlugins } from "./plugins.js";
import { runToolchains } from "./toolchains.js";
import { openUserCss } from "./usercss.js";
import { focusSearch } from "./search.js";
import { zoomBy } from "./zoom.js";
import { printText } from "./print.js";
import { clipText, closeMenu, menuOpen, showMenu } from "./menu.js";
import { chosenJob, chosenLive, listedJobs, showJob, startChosen, stopChosen, subject } from "./run.js";
import { followsSystem, scheme, setFollowSystem, setScheme, toggleScheme } from "./scheme.js";
import { autoCollapse, paneShown, setAutoCollapse, togglePane } from "./sides.js";
import { float, floatItems, solo, toolWindowsItems } from "./docks.js";
import { openWindow } from "./windows.js";
import { setStripEdge, statusItems, stripEdge } from "./statusitems.js";
import { icon } from "./icons.js";
import { focusFollows, setFocusFollows } from "./focusfollow.js";
import { setSystemTitleBar, systemTitleBar } from "./strip.js";
import { profileFile, profileRemote, stopProfile, toggleProfile } from "./profile.js";
import { openRemote } from "./remote.js";
import { toggleContainers } from "./containers.js";
import { clearTerminal, killTerminal, newTerminal, runInTerminal, splitTerminal, terminalAt, terminalSplit, toggleTerminal } from "./terminal.js";
import { applyEdits } from "./intel.js";
import { showView } from "./views.js";
import { askReports, reportForm } from "./reports.js";
import { wordmark } from "./wordmark.js";

// The most titles the bar shows at a window width: [narrowest width in pixels, titles]. The rest go
// into the last title's menu, and so do more where even these do not fit.
const TITLES_AT = [
  [1600, Infinity],
  [1440, 13],
  [1280, 11],
  [1120, 9],
  [0, 7],
];

const state = { bar: null, menus: [], open: -1, titles: [], held: false, before: null, openFolder: () => {} };

// The words on and off as true and false, and anything else as undefined, for the setting to flip.
const onOff = (args) => (args[0] === "on" ? true : args[0] === "off" ? false : undefined);

// The editor's command: the run view gives way to the edit view, and the editor takes the keys.
const inEditor = (act) => () => {
  const { editor } = editing();
  if (!editor) {
    return;
  }
  showView("edit");
  editor.focus();
  act(editor);
};

async function pasted(editor) {
  const text = await clipText();
  editor.focus();
  editor.paste({ preventDefault() {}, clipboardData: { getData: () => text } });
}

// The run view's search, holding `word`.
function searchJobs(word = "") {
  showView("run");
  const filter = document.getElementById("job-filter");
  filter.value = word;
  filter.dispatchEvent(new Event("input"));
  filter.focus();
}

// The file a path names, at the line and column after its colons, or the quick open where none is
// named.
function goToFile(args = []) {
  const found = (args[0] ?? "").match(/^(.*?)(?::(\d+))?(?::(\d+))?$/);
  if (!found[1]) {
    openPalette("");
    return;
  }
  showView("edit");
  openFileAt(found[1], Math.max(0, Number(found[2] || 1) - 1), Math.max(0, Number(found[3] || 1) - 1));
}

// Find in Files: the explorer's Search pane, holding the words given where there are any.
function findInFiles(args = []) {
  showView("edit");
  togglePane(true);
  showPane("search");
  focusSearch(args.length ? args.join(" ") : undefined);
}

// The bridge's key in Lstar.klq, or the file itself.
async function goToBridge(args = []) {
  const read = await invoke("bridge_read").catch(() => null);
  const klq = read?.klq;
  if (!klq) {
    return;
  }
  showView("edit");
  openFileAt(klq.path, klq.keys?.[args[0]]?.line ?? 0);
}

// File, Create, File and Folder: the path the command line gave, made at once, or one asked for. A
// file made opens.
async function create(folder, given) {
  const made = async (path) => {
    await editing().treeChanged();
    if (!folder) {
      showView("edit");
      await openFile(path);
    }
  };
  if (!given) {
    showCreate(sheet, { folder, start: editing().folderHere(), made });
    return;
  }
  try {
    await invoke(folder ? "folder_create" : "file_create", { path: given });
    await made(given);
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// File, Create, Repository: the open tree made a repository, or a folder chosen.
function createRepository(given) {
  showInit(sheet, {
    given,
    made: async (folder, here) => {
      if (here) {
        await editing().treeChanged();
      }
      say(`${folder} is a repository now.`);
    },
  });
}

// File, Open, File: a file of the tree, the one the command line named or one the picker gives.
async function openAnyFile(given) {
  const chosen = given ?? (await pick("file"));
  if (typeof chosen !== "string") {
    return;
  }
  let path = (await invoke("tree_relative", { path: chosen }).catch(() => null)) ?? (/^[a-zA-Z]:|^\//.test(chosen) ? null : chosen);
  // A file from outside the tree opens beside it, mounted on its own.
  if (!path) {
    try {
      path = `@${await addToWindow(chosen)}`;
    } catch (error) {
      say(String(error), { failed: true });
      return;
    }
  }
  showView("edit");
  await openFile(path);
}

// File, Open, Workspace: a .code-workspace file opened as a window that holds its folders, the first
// of them as the tree and the rest mounted beside it.
async function openWorkspace(given) {
  const chosen = given ?? (await pick("file"));
  if (typeof chosen !== "string") {
    return;
  }
  let folders;
  try {
    folders = await invoke("workspace_read", { path: chosen });
  } catch (error) {
    say(String(error), { failed: true });
    return;
  }
  const [tree, ...rest] = folders;
  if (!tree) {
    say(`${chosen} names no folder.`, { failed: true });
    return;
  }
  await state.openFolder(tree);
  for (const folder of rest) {
    await addToWindow(folder).catch((error) => say(String(error), { failed: true }));
  }
}

// What each command does in the window, given the words the command line passed it, if any.
const COMMANDS = {
  "create-file": (args) => create(false, args[0]),
  "create-folder": (args) => create(true, args[0]),
  "create-repository": (args) => createRepository(args[0] ?? null),
  "create-project": (args) => showProject(sheet, { given: args[0] ?? null, made: (folder) => state.openFolder(folder) }),
  "keep-template": async (args) => {
    const name = args[0] || (await askFor("A name for the template the tree is kept as", ""));
    if (name) {
      invoke("template_keep", { name })
        .then((folder) => say(`The tree is kept as the template ${name}, in ${folder}.`))
        .catch((error) => say(String(error), { failed: true }));
    }
  },
  "open-file": (args) => openAnyFile(args[0]),
  "open-folder": (args) => state.openFolder(args[0]),
  "add-folder": async (args) => {
    const chosen = args[0] ?? (await pick("dir"));
    if (typeof chosen === "string") {
      await addToWindow(chosen).catch((error) => say(String(error), { failed: true }));
    }
  },
  "add-file": async (args) => {
    const chosen = args[0] ?? (await pick("file"));
    if (typeof chosen === "string") {
      try {
        const name = await addToWindow(chosen);
        showView("edit");
        await openFile(`@${name}`);
      } catch (error) {
        say(String(error), { failed: true });
      }
    }
  },
  "open-workspace": (args) => openWorkspace(args[0]),
  "new-window": (args) => openWindow([], args[0] ?? null).catch((error) => say(String(error), { failed: true })),
  "open-folder-window": async (args) => {
    const chosen = args[0] ?? (await pick("dir"));
    if (typeof chosen === "string") {
      await openWindow([], chosen).catch((error) => say(String(error), { failed: true }));
    }
  },
  "next-display": () => invoke("window_act", { act: "next-display" }),
  "reader-menus": () => openReaderMenus(),
  profile: () => profileFile(),
  "profile-remote": (args) => profileRemote(args[0]),
  "open-remote": (args) => openRemote(args[0]),
  "dev-container": async () => {
    try {
      runInTerminal(await invoke("devcontainer_line"));
    } catch (error) {
      say(String(error), { failed: true });
    }
  },
  "stop-profile": () => stopProfile(),
  "profile-view": () => toggleProfile(),
  "containers-view": () => toggleContainers(),
  "focus-follows": (args) => setFocusFollows(args[0] === "on" ? true : args[0] === "off" ? false : undefined),
  "system-title": (args) => setSystemTitleBar(args[0] === "on" ? true : args[0] === "off" ? false : undefined),
  toolbar: (args) => setToolbarShown(args[0] === "on" ? true : args[0] === "off" ? false : undefined),
  "margin-line": (args) => setMarginLine(args[0] === "on" ? true : args[0] === "off" ? false : undefined),
  "strip-edge": (args) => setStripEdge(args[0] === "on" ? true : args[0] === "off" ? false : undefined),
  float: (args) => (args[0] ? float(args[0]) : showMenu(window.innerWidth / 3, 60, floatItems(float))),
  solo: (args) => (args.length || document.body.dataset.solo ? solo(args[0] ?? document.body.dataset.solo, args[1] ?? null) : showMenu(window.innerWidth / 3, 60, floatItems(solo))),
  "open-repository": (args) => showClone(sheet, state.openFolder, args, { open: true }),
  save: () => editing().save(),
  "save-all": () => editing().saveAll(),
  print: () => {
    const { editor, active } = editing();
    if (editor?.s) {
      printText(editor.s, active ?? "", sheet);
    }
  },
  "close-editor": () => editing().close(),
  "close-all": () => editing().closeAll(),
  exit: () => invoke("app_exit"),
  undo: inEditor((e) => e.undo(true)),
  redo: inEditor((e) => e.undo(false)),
  cut: inEditor(() => document.execCommand("cut")),
  copy: inEditor(() => document.execCommand("copy")),
  paste: inEditor(pasted),
  find: inEditor((e) => e.find.open(false)),
  search: findInFiles,
  palette: (args) => openPalette(`>${args.join(" ")}`),
  "zoom-in": () => zoomBy(1),
  "zoom-out": () => zoomBy(-1),
  "zoom-reset": () => zoomBy(0),
  back: () => editing().back(),
  forward: () => editing().forward(),
  "last-editor": () => editing().lastEditor(),
  symbol: (args) => {
    showView("edit");
    openPalette(`@${args.join(" ")}`);
  },
  "symbol-tree": (args) => {
    showView("edit");
    openPalette(`#${args.join(" ")}`);
  },
  replace: inEditor((e) => e.find.open(true)),
  comment: inEditor((e) => e.toggleComment()),
  format: () => editing().format(),
  "select-all": inEditor((e) => e.selectAll()),
  "select-line": inEditor((e) => e.selectLine()),
  "join-lines": inEditor((e) => e.joinLines()),
  "sort-ascending": inEditor((e) => e.sortLines(false)),
  "sort-descending": inEditor((e) => e.sortLines(true)),
  "unique-lines": inEditor((e) => e.uniqueLines()),
  "upper-case": inEditor((e) => e.transformCase("upper")),
  "lower-case": inEditor((e) => e.transformCase("lower")),
  "title-case": inEditor((e) => e.transformCase("title")),
  "column-mode": (args) => editing().setColumnMode(onOff(args) ?? !editing().columnMode()),
  "next-change": () => {
    showView("edit");
    editing().nextChange();
  },
  "previous-change": () => {
    showView("edit");
    editing().previousChange();
  },
  "copy-line-up": inEditor((e) => e.copyLines(-1)),
  "copy-line-down": inEditor((e) => e.copyLines(1)),
  "move-line-up": inEditor((e) => e.moveLines(-1)),
  "move-line-down": inEditor((e) => e.moveLines(1)),
  "cursor-above": inEditor((e) => e.addCursor(-1)),
  "cursor-below": inEditor((e) => e.addCursor(1)),
  "next-occurrence": inEditor((e) => e.addMatch(false)),
  "all-occurrences": inEditor((e) => e.addMatch(true)),
  "edit-view": () => showView("edit"),
  "side-bar": (args) => togglePane(args[0] === "show" ? true : args[0] === "hide" ? false : undefined),
  "auto-collapse": (args) => setAutoCollapse(args[0] === "on" ? true : args[0] === "off" ? false : undefined),
  "auto-save": (args) => setSaving("auto-save", onOff(args)),
  "format-on-save": (args) => setSaving("format", onOff(args)),
  breadcrumbs: (args) => setCrumbs(onOff(args)),
  "bracket-pairs": (args) => editing().setBrackets(onOff(args) ?? !editing().brackets()),
  whitespace: (args) => editing().setMarks(onOff(args) ?? !editing().marks()),
  "past-ends": (args) => editing().setPastEnds(onOff(args) ?? !editing().pastEnds()),
  "hide-comments": (args) => editing().setCommentsHidden(onOff(args) ?? !editing().commentsHidden()),
  "line-history": (args) => editing().setLineHistory(onOff(args) ?? !editing().lineHistory()),
  "git-authors": (args) => setAuthorsShown(onOff(args) ?? !authorsShown()),
  "smooth-scroll": (args) => editing().setSmoothScroll(onOff(args) ?? !editing().smoothScroll()),
  "type-hints": (args) => editing().setHints("type", onOff(args) ?? !editing().hints("type")),
  checkers: (args) => askCheckers(args.join(" ")),
  environment: (args) => chooseEnvironment(args[0]),
  "maven-settings": () => askMavenSettings(),
  "update-snapshots": () => updateSnapshots(),
  "attach-process": () => attachProcess(),
  "attach-address": (args) => attachAddress(args.length ? args.join(" ") : undefined),
  stepping: () => askStepping(),
  "export-breakpoints": () => exportBreakpoints(),
  "import-breakpoints": () => importBreakpoints(),
  "show-memory": (args) => showDebugMemory(args[0] ?? null),
  "watch-memory": (args) => watchAddress(args.length ? args.join(" ") : undefined),
  "machine-code": () => showMachineCode(),
  tests: () => showTests(),
  "run-tests": () => runAllTests(),
  "run-failed-tests": () => runFailedTests(),
  "cover-tests": () => runAllTests({ cover: true }),
  "stop-tests": () => stopTests(),
  "parallel-tests": (args) => setTestsParallel(args[0] ? args[0] === "on" : !testsParallel()),
  "parameter-hints": (args) => editing().setHints("parameter", onOff(args) ?? !editing().hints("parameter")),
  "flick-scroll": (args) => editing().setFlickScroll(onOff(args) ?? !editing().flickScroll()),
  "memory-budget": (args) => askBudget(args[0]),
  "sticky-scroll": (args) => editing().setSticky(args[0] === "on" ? true : args[0] === "off" ? false : !editing().sticky()),
  preferences: () =>
    showPreferences(sheet, {
      menus: state.menus,
      runCommand,
      checks: CHECKS,
      more: [
        { label: "Trim Trailing Whitespace", on: () => saving("trim"), set: (on) => setSaving("trim", on) },
        { label: "Insert Final Newline", on: () => saving("final-newline"), set: (on) => setSaving("final-newline", on) },
        { label: "Vim's Keys", on: vimKeys, set: setVimKeys },
      ],
    }),
  "user-css": () => openUserCss(),
  plugins: () => showPlugins(sheet),
  "new-plugin": (args) => showGenerator(sheet, args),
  "reload-plugins": () => loadPlugins(),
  toolchains: (args) => runToolchains(sheet, args),
  clone: (args) => showClone(sheet, state.openFolder, args),
  "run-view": () => showView("run"),
  "terminal-view": () => toggleTerminal(),
  scheme: (args) => (args[0] === "light" || args[0] === "dark" ? setScheme(args[0]) : toggleScheme()),
  "scheme-system": (args) => setFollowSystem(onOff(args) ?? !followsSystem()),
  "undo-history": () => showPane("undo"),
  "compare-clipboard": () => editing().compareWithClipboard(),
  "commit-view": () => showPane("changes"),
  push: () => gitSays("Pushing", () => invoke("git_push", { repo: editing().repo() })),
  pull: () => gitSays("Pulling", () => invoke("git_pull", { repo: editing().repo() })),
  branches: () => showBranches(),
  conflicts: (args) => resolveConflicts(args[0]),
  "new-branch": (args) => newBranch(args.join(" ")),
  stash: (args) => stashChanges(args.join(" ")),
  stashes: () => showStashes(),
  "cherry-pick": (args) => cherryPick(args[0]),
  "macro-record": () => recordMacro(),
  "macro-play": (args) => playBack(Number.parseInt(args[0], 10) || 1, args.slice(1).join(" ")),
  "macro-keep": (args) => keepLast(args.join(" ")),
  "split-right": () => (showView("edit"), editing().split("right")),
  "split-down": () => (showView("edit"), editing().split("down")),
  unsplit: () => editing().unsplit(),
  "move-to-split": () => (showView("edit"), editing().moveToNextSplit()),
  "side-tabs": (args) => editing().setSideTabs(args[0] === "on" ? true : args[0] === "off" ? false : undefined),
  "mark-file": () => editing().toggleMark(),
  "marked-files": () => showMenu(window.innerWidth / 3, window.innerHeight / 4, editing().markItems()),
  ...Object.fromEntries([1, 2, 3, 4, 5, 6, 7, 8, 9].map((n) => [`mark-${n}`, () => (showView("edit"), editing().goMark(n))])),
  "open-in-split": (args) => (showView("edit"), args.length ? editing().openInSplit(args.join(" ")) : editing().active && editing().openInSplit(editing().active)),
  "fold-all": inEditor((e) => e.foldAll(true)),
  "unfold-all": inEditor((e) => e.foldAll(false)),
  file: goToFile,
  "search-everywhere": () => openPalette("", { everywhere: true }),
  bookmark: inEditor(() => toggleBookmark()),
  bookmarks: () => showBookmarks(),
  "expand-selection": inEditor((e) => e.expandSelection()),
  "shrink-selection": inEditor((e) => e.shrinkSelection()),
  "recent-files": () => openPalette(""),
  line: (args) => inEditor((e) => (args[0] ? e.goTo(Math.max(0, Number(args[0]) - 1)) : e.goto.open()))(),
  bracket: inEditor((e) => e.jumpBracket()),
  jump: inEditor((e) => (e.startJump(), say("Jump: type the letter or two where the cursor is to go, then the mark there."))),
  bridge: goToBridge,
  definition: inEditor(() => editing().definition()),
  usages: inEditor(() => editing().usages()),
  calls: inEditor(() => editing().calls()),
  rename: inEditor(() => editing().rename()),
  "extract-variable": inEditor(() => editing().extractVariable()),
  "extract-constant": inEditor(() => editing().extractConstant()),
  "extract-function": inEditor(() => editing().extractFunction()),
  "inline-variable": inEditor(() => editing().inlineVariable()),
  "change-signature": inEditor(() => editing().changeSignature(sheet)),
  "move-declaration": inEditor(() => editing().moveDeclaration(askFor)),
  "shape-search": inEditor(() => editing().shapeSearch(sheet)),
  docstring: inEditor(() => editing().writeDocstring()),
  "fill-paragraph": inEditor(() => editing().fillParagraph()),
  continuation: (args) => askContinuation(args),
  "operator-next-line": (args) => editing().setOperatorNext(onOff(args) ?? !editing().operatorNext()),
  "doc-margin": (args) => askDocMargin(args[0]),
  "sort-methods": inEditor(() => editing().sortMethods()),
  "docstring-form": (args) => {
    if (args[0]) {
      editing().setDocstringForm(args[0]);
      return;
    }
    const forms = { google: "Google", numpy: "NumPy", rest: "reStructuredText", plain: "Summary Line Alone" };
    showMenu(window.innerWidth / 3, window.innerHeight / 4, Object.entries(forms).map(([form, label]) => ({ label, checked: editing().docstringForm() === form, run: () => editing().setDocstringForm(form) })));
  },
  "quick-fix": inEditor(() => editing().quickFix()),
  "parameter-info": inEditor(() => editing().parameterInfo()),
  "quick-doc": inEditor(() => editing().quickDoc()),
  debug: () => {
    showView("edit");
    debugFile();
  },
  "restart-debug": () => restartDebug(),
  continue: () => step("continue"),
  "step-over": () => step("next"),
  "step-back": () => step("stepBack"),
  "reverse-continue": () => step("reverseContinue"),
  record: () => recordFile(),
  "debug-mixed": () => debugMixed(),
  "step-into": () => step("stepIn"),
  "step-out": () => step("stepOut"),
  pause: () => step("pause"),
  "stop-debug": () => stopDebug(),
  breakpoint: inEditor(() => toggleBreakpointHere()),
  "debug-view": () => toggleDebugPanel(),
  "next-problem": inEditor((e) => e.stepProblem(1)),
  "previous-problem": inEditor((e) => e.stepProblem(-1)),
  "next-match": inEditor((e) => e.find.step(1)),
  "previous-match": inEditor((e) => e.find.step(-1)),
  start: () => chosenJob() && startChosen(),
  stop: stopChosen,
  "run-file": () => editing().runFile(),
  validate: () => editing().validate(),
  list: (args) => searchJobs(args[0]),
  show: (args) => searchJobs(args[0]),
  new: newTerminal,
  toggle: () => toggleTerminal(),
  split: () => splitTerminal(),
  clear: clearTerminal,
  kill: killTerminal,
  keys: showShortcuts,
  report: (args) => reportForm(sheet, args),
  "auto-report": async (args) => {
    const on = args[0] === "on" ? true : args[0] === "off" ? false : !state.autoReport;
    await invoke("report_auto_set", { on });
    state.autoReport = on;
  },
  about: showAbout,
};

// Whether an item that is on or off is on, by the name commands.json gives it.
const CHECKS = {
  "side-bar": paneShown,
  "auto-collapse": autoCollapse,
  "sticky-scroll": () => editing().sticky(),
  "column-mode": () => editing().columnMode(),
  "auto-save": () => saving("auto-save"),
  "format-on-save": () => saving("format"),
  "parallel-tests": testsParallel,
  breadcrumbs: crumbsShown,
  "bracket-pairs": () => editing().brackets(),
  whitespace: () => editing().marks(),
  "past-ends": () => editing().pastEnds(),
  "hide-comments": () => editing().commentsHidden(),
  "line-history": () => editing().lineHistory(),
  "git-authors": () => authorsShown(),
  "smooth-scroll": () => editing().smoothScroll(),
  "operator-next-line": () => editing().operatorNext(),
  "type-hints": () => editing().hints("type"),
  "parameter-hints": () => editing().hints("parameter"),
  "flick-scroll": () => editing().flickScroll(),
  "auto-report": () => state.autoReport,
  "scheme-system": followsSystem,
  split: () => editing().splitShown(),
  "terminal-split": terminalSplit,
  "side-tabs": () => editing().sideTabs(),
  "strip-edge": stripEdge,
  toolbar: () => toolbarShown(),
  "margin-line": () => marginLine(),
  "focus-follows": () => focusFollows(),
  "system-title": () => systemTitleBar(),
  "macro-recording": recording,
};

// What a command needs before it can act, by the name commands.json gives the need.
const NEEDS = {
  editor: () => Boolean(editing().editor),
  text: () => Boolean(editing().editor),
  changes: () => editing().hasChanges(),
  "changed-active": () => editing().activeChanged,
  changed: () => editing().changed,
  tab: () => Boolean(editing().active),
  tabs: () => editing().open,
  job: () => Boolean(chosenJob()),
  live: () => chosenLive(),
  debugging: () => debugging(),
  debugged: () => debugging() || Boolean(editing().active),
  paused: () => isPaused(),
  back: () => canStepBack(),
  running: () => debugging() && !isPaused(),
  macro: () => Boolean(lastMacro() && editing().editor),
};

// Edit, Macros: recording starts in the editor and stops on a second press, and the last macro
// recorded plays back, or is kept under a name. Each kept macro is listed under them, to play once or
// many times, to bind keys to, or to forget.
function recordMacro() {
  const { editor } = editing();
  if (!editor && !recording()) {
    return;
  }
  const steps = toggleRecording(editor);
  say(steps === null ? "Recording a macro: Record Macro again stops it." : steps.length ? `Recorded a macro of ${steps.length} ${steps.length === 1 ? "step" : "steps"}.` : "Nothing was recorded.");
}

async function playBack(times, name) {
  const steps = name ? keptMacros().find((one) => one.name === name)?.steps : lastMacro();
  if (!steps) {
    say(name ? `No macro is kept as ${name}.` : "No macro is recorded yet.", { failed: true });
    return;
  }
  showView("edit");
  editing().editor?.focus();
  await playMacro(editing().editor, steps, times);
}

async function keepLast(given) {
  const name = given || (await askFor("Keep the last macro as", ""));
  if (name) {
    keepMacro(name);
    say(`Kept the last macro as ${name}.`);
  }
}

// The kept macros as items of Edit, Macros, under a line.
function macroItems() {
  const items = keptMacros().map((one) => ({
    label: one.name,
    items: [
      { label: "Play", keys: one.keys || undefined, run: () => playBack(1, one.name) },
      {
        label: "Play Many Times…",
        run: async () => {
          const times = Number.parseInt(await askFor(`Times to play ${one.name}`, "2"), 10);
          if (times > 0) {
            playBack(times, one.name);
          }
        },
      },
      {
        label: "Keys…",
        run: async () => {
          const keys = await askKeys(`Keys for ${one.name}`);
          if (keys !== null) {
            setMacroKeys(one.name, keys);
          }
        },
      },
      { label: "Forget", run: () => forgetMacro(one.name) },
    ],
  }));
  return items.length ? ["-", ...items] : [];
}

// Binds each kept macro's keys in the editor, to play it where they are pressed.
function bindMacros() {
  bindReaderKeys(keptMacros().filter((one) => one.keys).map((one) => ({ keys: one.keys, run: (editor) => playMacro(editor, one.steps) })));
}

// The items commands.json names to be filled in as their menu opens.
const FILLS = { macros: macroItems, "tool-windows": toolWindowsItems, "status-items": statusItems };

// A sheet that asks for a line of text, with `start` in it. Answers the text, or null where it was
// closed with nothing taken.
export function askFor(label, start) {
  return new Promise((done) => {
    const field = Object.assign(document.createElement("input"), { className: "report-field", type: "text", value: start, spellcheck: false, ariaLabel: label });
    const body = document.createElement("form");
    body.className = "sheet-ask";
    const row = Object.assign(document.createElement("label"), { className: "report-row" });
    row.append(Object.assign(document.createElement("span"), { textContent: label }), field);
    body.append(row);
    let answer = null;
    body.addEventListener("submit", (event) => {
      event.preventDefault();
      answer = field.value.trim() || null;
      dialog.close();
    });
    const dialog = sheet(body);
    dialog.addEventListener("close", () => done(answer));
    field.focus();
    field.select();
  });
}

// A sheet that takes the next keys pressed, as the menus write keys: Ctrl, Shift and Alt, then the
// key. Backspace alone answers no keys, and Escape answers null.
function askKeys(label) {
  return new Promise((done) => {
    const body = document.createElement("div");
    body.className = "sheet-ask";
    const shown = Object.assign(document.createElement("kbd"), { textContent: "…" });
    body.append(Object.assign(document.createElement("p"), { textContent: label }), shown);
    let answer = null;
    const dialog = sheet(body);
    dialog.addEventListener("keydown", (event) => {
      if (["Control", "Shift", "Alt", "Meta"].includes(event.key)) {
        return;
      }
      event.preventDefault();
      event.stopPropagation();
      if (event.key === "Escape") {
        dialog.close();
        return;
      }
      const named = KEY_NAMES[event.code] ?? (/^Key[A-Z]$/.test(event.code) ? event.code.slice(3) : /^Digit\d$/.test(event.code) ? event.code.slice(5) : event.key.length === 1 ? event.key.toUpperCase() : event.key);
      answer = event.key === "Backspace" && !event.ctrlKey && !event.altKey && !event.shiftKey ? "" : [event.ctrlKey && "Ctrl", event.shiftKey && "Shift", event.altKey && "Alt", named].filter(Boolean).join("+");
      shown.textContent = answer || "…";
      window.setTimeout(() => dialog.close(), 300);
    });
    dialog.addEventListener("close", () => done(answer));
    dialog.focus();
  });
}

// Git, and the branch on the status bar: what git is asked to do said on the status bar, git's own
// words where it refuses, and the tree's changes read again after.
async function gitSays(doing, act) {
  say(`${doing}…`);
  try {
    const said = await act();
    say(String(said ?? "").split("\n").find((line) => line.trim()) ?? `${doing} done.`);
  } catch (error) {
    say(String(error), { failed: true });
    return false;
  }
  await editing().treeChanged();
  return true;
}

async function newBranch(given) {
  const name = given || (await askFor("New branch, made from the branch open and gone on to", ""));
  if (name) {
    gitSays(`Making ${name}`, () => invoke("git_branch", { act: "create", name, repo: editing().repo() }));
  }
}

// Every local branch, then every remote one, the tree's own checked: each to go on to, to merge into
// the branch open or to rebase it onto, and a local one to rename or delete.
async function showBranches() {
  const list = await invoke("git_branches", { repo: editing().repo() }).catch(() => []);
  const open = list.find((one) => one.current)?.name ?? "HEAD";
  const act = (act, name, to) => gitSays(`${act[0].toUpperCase()}${act.slice(1)} ${name}`, () => invoke("git_branch", { act, name, to, repo: editing().repo() }));
  const remove = async (name) => {
    try {
      say(await invoke("git_branch", { act: "delete", name, repo: editing().repo() }));
      await editing().treeChanged();
    } catch (error) {
      if (/not fully merged/.test(String(error)) && (await askYes(`${name} has commits no other branch holds. Delete it and them?`, "Delete"))) {
        act("delete-unmerged", name);
      } else if (!/not fully merged/.test(String(error))) {
        say(String(error), { failed: true });
      }
    }
  };
  const itemOf = (branch) => {
    const items = [];
    if (!branch.current) {
      items.push(
        { label: "Switch", run: () => act("switch", branch.name) },
        { label: `Merge into ${open}`, run: () => act("merge", branch.name) },
        { label: `Rebase ${open} onto it`, run: () => act("rebase", branch.name) },
      );
    }
    if (!branch.remote) {
      items.push({
        label: "Rename…",
        run: async () => {
          const to = await askFor(`New name for ${branch.name}`, branch.name);
          if (to && to !== branch.name) {
            act("rename", branch.name, to);
          }
        },
      });
      if (!branch.current) {
        items.push({ label: "Delete", run: () => remove(branch.name) });
      }
    }
    return { label: branch.name, checked: branch.current, items };
  };
  const local = list.filter((one) => !one.remote).map(itemOf);
  const remote = list.filter((one) => one.remote).map(itemOf);
  const items = [{ label: "New Branch…", run: () => newBranch("") }, ...(local.length ? ["-", ...local] : []), ...(remote.length ? ["-", ...remote] : [])];
  const anchor = document.getElementById("status-branch");
  const box = anchor.hidden ? { left: window.innerWidth / 3, top: window.innerHeight / 3 } : anchor.getBoundingClientRect();
  showMenu(box.left, (box.bottom ?? box.top) + 2, items, { anchor: anchor.hidden ? null : anchor });
}

// Edit, Formatting, Continuation Indent: the columns a new line inside a bracket is indented by, for
// a call's arguments, a declaration's parameters and any other bracket, each as given or asked for;
// one left empty is one step of the indent.
function askContinuation(given) {
  const now = editing().continuation();
  const places = [["call", "Calls' arguments"], ["declaration", "Declarations' parameters"], ["expression", "Any other bracket"]];
  const apply = (values) => {
    const set = {};
    places.forEach(([place], index) => {
      const columns = Number(values[index]);
      if (Number.isFinite(columns) && columns > 0) {
        set[place] = Math.round(columns);
      }
    });
    editing().setContinuation(set);
    say(`A new line inside a bracket is indented by ${places.map(([place, label]) => `${set[place] ?? "one step"} for ${label.toLowerCase()}`).join(", ")}.`);
  };
  if (given.length) {
    apply(given);
    return;
  }
  const form = document.createElement("form");
  form.className = "sheet-report sheet-create";
  const fields = places.map(([place, label]) => {
    const field = Object.assign(document.createElement("input"), { className: "report-field", type: "number", min: 1, max: 16, value: now[place] ?? "", placeholder: "One step of the indent" });
    field.setAttribute("aria-label", label);
    const row = Object.assign(document.createElement("label"), { className: "report-row" });
    row.append(Object.assign(document.createElement("span"), { textContent: label }), field);
    return [row, field];
  });
  const go = Object.assign(document.createElement("button"), { className: "primary", type: "submit", textContent: "Set" });
  const foot = Object.assign(document.createElement("div"), { className: "report-foot" });
  foot.append(go);
  form.append(Object.assign(document.createElement("h2"), { textContent: "Continuation Indent" }), ...fields.map(([row]) => row), foot);
  const dialog = sheet(form);
  form.addEventListener("submit", (event) => {
    event.preventDefault();
    apply(fields.map(([, field]) => field.value));
    dialog.close();
  });
  fields[0][1].focus();
}

// File, Maven Settings: the compiler's settings the POM open gives, the tree's top POM where none is
// open, and Maven's own, from settings.xml, each a field; Set writes those changed, the POM's into it
// as edits and Maven's own into the user's settings.xml.
async function askMavenSettings() {
  const ed = editing();
  const file = ed.fileHere();
  const pom = file && /(^|\/)pom\.xml$/i.test(file) ? file : null;
  let rows;
  try {
    rows = await invoke("maven_settings", { path: pom, text: pom ? ed.textHere() : null });
  } catch (error) {
    say(String(error), { failed: true });
    return;
  }
  const form = document.createElement("form");
  form.className = "sheet-report sheet-maven";
  const fields = [];
  const parts = [Object.assign(document.createElement("h2"), { textContent: "Maven Settings" })];
  for (const [group, title] of [["pom", `The compiler, as ${pom ?? "pom.xml"} sets it`], ["settings", "Maven's own"]]) {
    const shown = rows.filter((row) => row.group === group);
    if (shown.length) {
      parts.push(Object.assign(document.createElement("h3"), { textContent: title }));
    }
    for (const row of shown) {
      const field = Object.assign(document.createElement("input"), { className: "report-field", value: row.value, readOnly: !row.settable, placeholder: row.settable ? "Not set" : "None", title: row.file });
      field.setAttribute("aria-label", row.label);
      const line = Object.assign(document.createElement("label"), { className: "report-row" });
      line.append(Object.assign(document.createElement("span"), { textContent: row.label }), field);
      parts.push(line);
      if (row.settable) {
        fields.push([row, field]);
      }
    }
  }
  const go = Object.assign(document.createElement("button"), { className: "primary", type: "submit", textContent: "Set" });
  const foot = Object.assign(document.createElement("div"), { className: "report-foot" });
  foot.append(go);
  form.append(...parts, foot);
  const dialog = sheet(form);
  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const changed = fields.filter(([row, field]) => field.value.trim() !== row.value);
    for (const [row, field] of changed) {
      try {
        const now = pom && editing().fileHere() === pom ? editing().textHere() : null;
        await applyEdits(await invoke("maven_set", { path: pom, text: now, key: row.key, value: field.value }));
      } catch (error) {
        say(String(error), { failed: true });
        return;
      }
    }
    say(changed.length ? `${changed.map(([row]) => row.label).join(", ")} set.` : "No setting was changed.");
    dialog.close();
  });
  fields[0]?.[1].focus();
}

// Run, Find Tests: the tree's Python tests, found as pytest and unittest find them with no setup, in
// the quick open, each by its class and its name, then its file and line; Enter opens it.
async function showTests() {
  let found;
  try {
    found = await invoke("tests_found");
  } catch (error) {
    say(String(error), { failed: true });
    return;
  }
  openPalette("", {
    custom: {
      placeholder: `${found.length} test${found.length === 1 ? "" : "s"}: type to filter, Enter to open one`,
      empty: "No test is found: pytest and unittest take files named test_*.py, *_test.py and test*.py, and in them the functions and methods whose names start with test.",
      rows: (query) => {
        const rows = [];
        for (const test of found) {
          const label = test.class ? `${test.class}.${test.name}` : test.name;
          const hit = fuzzy(query, `${label} ${test.file}`);
          if (hit) {
            rows.push({ label, hits: hit.hits.filter((at) => at < label.length), detail: `${test.file}:${test.line + 1}`, score: query ? hit.score : 0, run: () => openFileAt(test.file, test.line, 0) });
          }
        }
        return query ? rows.sort((a, b) => b.score - a.score) : rows;
      },
    },
  });
}

// Run, Update Snapshots: fetches the newest build of each snapshot dependency of the build the file
// open belongs to, or of the tree's top build, in the terminal.
async function updateSnapshots() {
  try {
    const [folder, line] = await invoke("snapshots_line", { path: editing().fileHere() });
    terminalAt(folder);
    runInTerminal(line);
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// Edit, Formatting, Documentation Margin: the column documentation is filled to, as given or asked
// for; none has documentation filled to the code's margin.
async function askDocMargin(given) {
  const answer = given ?? (await askFor("The column documentation is filled to, or none for the code's", String(editing().docMargin() ?? "")));
  if (answer === null || answer === undefined) {
    return;
  }
  const columns = Number(answer);
  if (answer.trim() === "" || answer.trim() === "none") {
    editing().setDocMargin(null);
    say("Documentation is filled to the code's margin.");
  } else if (Number.isFinite(columns) && columns >= 20) {
    editing().setDocMargin(Math.round(columns));
    say(`Documentation is filled to column ${Math.round(columns)}.`);
  } else {
    say(`${answer} is no margin: a column of 20 or more, or none.`, { failed: true });
  }
}

// File, Tree Environment: the environments the tree holds, each by its folder in the tree and its
// kind, the one in use checked, and the toolchains' own Python; the one chosen kept for the tree by
// its folder in it, which moves with the tree, and taken up again as the tree opens. A tree with none
// chosen takes its own virtual environment, the one nearest its top, with no setup. An environment
// chosen says which packages the files beside it require that it has not installed.
const ENV_KINDS = { venv: "virtual environment", pipenv: "Pipenv", nix: "Nix" };

// The environment the tree open uses, or null for the toolchains' own Python.
let environmentInUse = null;

function environmentKey() {
  return `orior.environment.${document.getElementById("tree-path").textContent}`;
}

// Uses `env` for the tree, or the toolchains' own Python for null, kept as the reader's choice where
// `kept`.
async function useEnvironment(env, { quiet = false, kept = true } = {}) {
  try {
    const used = await invoke("env_use", { kind: env?.kind ?? null, place: env?.place ?? "", requires: env?.requires ?? [] });
    environmentInUse = env ?? null;
    if (kept) {
      localStorage.setItem(environmentKey(), JSON.stringify({ kind: env?.kind ?? null, place: env?.place ?? "" }));
    }
    if (!quiet) {
      const where = env ? `${env.place || "the tree's top folder"}, a ${ENV_KINDS[env.kind]}` : "the toolchains' own";
      const missing = used.missing.length ? `; it has not installed ${used.missing.join(", ")}` : "";
      say(`The tree's environment is ${where}${used.python ? `, its Python ${used.python}` : ""}${missing}.`, { failed: Boolean(used.missing.length) });
    }
  } catch (error) {
    say(String(error), { failed: true });
  }
}

async function chooseEnvironment(given) {
  const found = await invoke("envs_found").catch(() => []);
  if (given) {
    const env = given === "none" ? null : found.find((one) => one.place === given || `${one.kind}:${one.place}` === given);
    if (env === undefined) {
      say(`The tree holds no environment at ${given}.`, { failed: true });
      return;
    }
    await useEnvironment(env);
    return;
  }
  const items = [
    { label: "The Toolchains' Own Python", checked: !environmentInUse, run: () => useEnvironment(null) },
    ...(found.length ? ["-"] : []),
    ...found.map((env) => ({ label: `${env.place || "The tree's top folder"}: ${ENV_KINDS[env.kind]}${env.requires.length ? `, ${env.requires.length} package${env.requires.length === 1 ? "" : "s"} required` : ""}`, checked: environmentInUse?.kind === env.kind && environmentInUse?.place === env.place, run: () => useEnvironment(env) })),
  ];
  showMenu(window.innerWidth / 3, window.innerHeight / 4, items);
}

// Takes up again the environment kept for the tree open, where one is kept and the tree still holds
// it; where none is kept, the tree's own virtual environment nearest its top, not kept, for one the
// tree comes to hold later to be taken in its place.
export async function restoreEnvironment() {
  const kept = JSON.parse(localStorage.getItem(environmentKey()) ?? "null");
  if (kept && kept.kind === null) {
    await useEnvironment(null, { quiet: true, kept: false });
    return;
  }
  const found = await invoke("envs_found").catch(() => []);
  if (!kept) {
    await useEnvironment(found.find((one) => one.kind === "venv") ?? null, { quiet: true, kept: false });
    return;
  }
  const env = found.find((one) => one.kind === kept.kind && one.place === kept.place);
  await useEnvironment(env ?? null, { quiet: true, kept: false });
}

// View, Checkers: the type checkers and linters to run as files change, by name, as given or asked
// for among those the manifest knows; a name none answers to is said.
async function askCheckers(given) {
  const known = await invoke("checkers_known").catch(() => []);
  const answer = given || (await askFor(`The checkers to run as files change, a comma between each: ${known.map(([, name]) => name).join(", ")}`, checkersNamed().join(", ")));
  if (answer === null || answer === undefined) {
    return;
  }
  const names = answer.split(",").map((name) => name.trim()).filter(Boolean);
  const unknown = await setCheckers(names);
  if (unknown.length) {
    say(`No checker is named ${unknown.join(" or ")}.`, { failed: true });
  } else {
    say(names.length ? `${names.join(", ")} check files as they change.` : "No checker runs as files change.");
  }
}

// View, Memory Budget: the megabytes the app is held to in RAM, as given or asked for.
async function askBudget(given) {
  const answer = given ?? (await askFor("The memory budget, in megabytes", String(memoryBudget())));
  const mb = Number(answer);
  if (answer && Number.isFinite(mb) && mb > 0) {
    setMemoryBudget(mb);
    say(`The memory budget is ${Math.round(mb)} MB.`);
  } else if (answer) {
    say(`${answer} is no number of megabytes.`, { failed: true });
  }
}

// The memory reading's menu: what each program the app started holds, the budget, and the servers no
// open tab needs to stop.
function showMemory() {
  const node = document.getElementById("status-memory");
  const read = lastMemory();
  const mb = (bytes) => `${Math.round(bytes / (1024 * 1024))} MB`;
  const parts = (read?.parts ?? []).map((part) => ({ label: `${part.name}: ${mb(part.working)}`, disabled: true }));
  const box = node.getBoundingClientRect();
  showMenu(box.left, box.top, [...parts, ...(parts.length ? ["-"] : []), { label: `Memory Budget: ${memoryBudget()} MB…`, run: () => askBudget() }, { label: "Stop Servers No Open Tab Needs", run: () => editing().holdMemory(Number.MAX_SAFE_INTEGER) }], { anchor: node });
}

// Git, Stash Changes: every change put aside, new files with them, under the message given or asked
// for.
async function stashChanges(given) {
  const message = given || (await askFor("A message for the changes put aside", ""));
  if (message) {
    gitSays("Stashing", () => invoke("git_stash", { act: "push", message, repo: editing().repo() }));
  }
}

// Git, Stashes: each stash, the newest first, to bring back and keep, bring back and drop, or throw
// away.
async function showStashes() {
  const list = await invoke("git_stashes", { repo: editing().repo() }).catch(() => []);
  if (!list.length) {
    say("No changes are put aside.");
    return;
  }
  const act = (act, stash) => gitSays(`${act[0].toUpperCase()}${act.slice(1)} ${stash.name}`, () => invoke("git_stash", { act, name: stash.name, repo: editing().repo() }));
  const items = list.map((stash) => ({
    label: `${stash.name}  ${stash.subject}`,
    items: [
      { label: "Apply", run: () => act("apply", stash) },
      { label: "Pop", run: () => act("pop", stash) },
      { label: "Drop", run: async () => (await askYes(`Throw away ${stash.name}, ${stash.subject}?`, "Drop")) && act("drop", stash) },
    ],
  }));
  showMenu(window.innerWidth / 3, window.innerHeight / 4, [{ label: "Stash Changes…", run: () => stashChanges("") }, "-", ...items]);
}

// Git, Cherry-Pick: the commit named, or one chosen from the commits of other branches the branch open
// does not hold, applied to the branch open as a commit of its own, in `repo`, the repository open
// where none is named.
export async function cherryPick(given, repo = editing().repo()) {
  const pick = (id, subject) => gitSays(`Cherry-picking ${subject ?? id.slice(0, 7)}`, () => invoke("git_cherry_pick", { id, repo }));
  if (given) {
    pick(given);
    return;
  }
  const list = await invoke("git_elsewhere", { repo }).catch(() => []);
  if (!list.length) {
    say("The branch open holds every commit of the other branches.");
    return;
  }
  showMenu(window.innerWidth / 3, window.innerHeight / 4, list.map((commit) => ({ label: `${commit.id.slice(0, 7)}  ${commit.subject}`, run: () => pick(commit.id, commit.subject) })));
}

// Git, Resolve Conflicts: the file named, or a file a merge left in conflict where it is alone in
// that, in the merge window, or a list of them to choose from where there are more.
function resolveConflicts(given) {
  const files = editing().conflicted();
  if (given || files.length === 1) {
    showView("edit");
    editing().openMerge(given || files[0]);
  } else if (!files.length) {
    say("No file is in conflict.");
  } else {
    showMenu(window.innerWidth / 3, window.innerHeight / 4, files.map((path) => ({ label: path, run: () => (showView("edit"), editing().openMerge(path)) })));
  }
}

// A sheet that asks a question with a yes of its own, answering whether that was given.
function askYes(question, yes) {
  return new Promise((done) => {
    const body = document.createElement("div");
    body.className = "sheet-ask";
    const button = Object.assign(document.createElement("button"), { type: "button", className: "primary", textContent: yes });
    body.append(Object.assign(document.createElement("p"), { textContent: question }), button);
    let answer = false;
    const dialog = sheet(body);
    button.addEventListener("click", () => {
      answer = true;
      dialog.close();
    });
    dialog.addEventListener("close", () => done(answer));
    button.focus();
  });
}

// Runs a command of a menu, as its item does, with the words the command line gave it.
export function runCommand(command, args = []) {
  return COMMANDS[command]?.(args);
}

// Whether the window answers `command` itself.
export const hasCommand = (command) => Object.hasOwn(COMMANDS, command);

// Every command the menus list that can act now, for the quick open's >.
function paletteCommands() {
  const found = [];
  for (const menu of state.menus) {
    for (const item of menu.all) {
      if (item.command === "palette" || (item.needs && !NEEDS[item.needs]?.())) {
        continue;
      }
      const label = item.labels?.[scheme()] ?? item.label;
      found.push({ key: `${menu.title}/${item.command}`, menu: menu.title, label: label.replace(/…$/, ""), keys: item.keys, run: () => runCommand(item.command) });
    }
  }
  if (editing().editor) {
    for (const one of keptMacros()) {
      found.push({ key: `Macros/${one.name}`, menu: "Macros", label: one.name, keys: one.keys || undefined, run: () => playBack(1, one.name) });
    }
  }
  return found;
}

function jobItem(job) {
  return {
    label: job.title,
    run: () => {
      showView("run");
      showJob(job.id);
    },
  };
}

const jobsIn = (groups) => listedJobs().filter((job) => groups.includes(job.group));

// A menu's items: its commands, then the jobs of its groups, split by what each works on where the
// menu says so.
function itemsOf(menu) {
  const shown = (item) =>
    item === "-"
      ? "-"
      : item.items
        ? { label: item.label, items: item.items.flatMap(filled) }
        : {
            label: item.labels?.[scheme()] ?? item.label,
            keys: item.keys,
            checked: item.checks ? Boolean(CHECKS[item.checks]?.()) : undefined,
            disabled: Boolean(item.needs && !NEEDS[item.needs]?.()),
            run: () => runCommand(item.command),
          };
  // An entry to be filled in gives its items as the menu opens.
  const filled = (item) => (item.fill ? (FILLS[item.fill]?.() ?? []) : [shown(item)]);
  const items = (menu.items ?? []).flatMap(filled);
  const groups = menu.groups ?? [];
  let jobs;
  if (menu.split) {
    const all = jobsIn(groups);
    jobs = [...new Set(all.map(subject))].map((name) => ({ label: name, items: all.filter((job) => subject(job) === name).map(jobItem) }));
  } else {
    jobs = groups.flatMap((group, at) => {
      const own = jobsIn([group]).map(jobItem);
      return at > 0 && own.length ? ["-", ...own] : own;
    });
  }
  return items.length && jobs.length ? [...items, "-", ...jobs] : [...items, ...jobs];
}

// The keys every menu lists, by menu, in a sheet over the app.
function showShortcuts() {
  const body = document.createElement("div");
  body.className = "sheet-keys";
  for (const menu of state.menus) {
    const rows = menu.all.filter((item) => item.keys);
    if (!rows.length) {
      continue;
    }
    const block = document.createElement("section");
    block.append(Object.assign(document.createElement("h3"), { textContent: menu.title }));
    for (const item of rows) {
      const row = document.createElement("div");
      row.append(Object.assign(document.createElement("span"), { textContent: item.labels?.[scheme()] ?? item.label }), Object.assign(document.createElement("kbd"), { textContent: item.also ? `${item.keys}, ${item.also}` : item.keys }));
      block.append(row);
    }
    body.append(block);
  }
  sheet(body);
}

async function showAbout() {
  const body = document.createElement("div");
  body.className = "sheet-about";
  const version = await invoke("app_version").catch(() => "");
  body.append(
    Object.assign(document.createElement("span"), { className: "mark", textContent: "Σ", ariaHidden: "true" }),
    wordmark("h2"),
    Object.assign(document.createElement("p"), { textContent: version }),
    Object.assign(document.createElement("p"), { className: "tree", textContent: document.getElementById("tree-path").textContent }),
  );
  sheet(body);
}

// A sheet over the app, which Escape, a press outside it or its × closes. Answers the sheet.
export function sheet(body) {
  const dialog = document.createElement("dialog");
  dialog.className = "sheet";
  const close = Object.assign(document.createElement("button"), { type: "button", className: "sheet-close", textContent: "×", ariaLabel: "Close" });
  close.addEventListener("click", () => dialog.close());
  dialog.append(close, body);
  dialog.addEventListener("click", (event) => event.target === dialog && dialog.close());
  dialog.addEventListener("close", () => dialog.remove());
  document.body.append(dialog);
  dialog.showModal();
  return dialog;
}

// The menus the bar has a title for: each of commands, and each of jobs that has a job in the tree.
function shownMenus() {
  return laidOut().filter((menu) => !menu.strip && (menu.items?.length || jobsIn(menu.groups ?? []).length));
}

// Menus and a toolbar of the reader's, from menus.json in orior's own folder. Each item names a
// command of orior's menus, with a label of the reader's where it gives one, or a line, or a list of
// items under a label of its own. A menu of the reader's named as one of orior's stands in its place,
// the others stand before Help, and "hide" takes orior's menus away by name. The keys of every
// command stay as orior's menus bind them, wherever the reader's menus put it. The file is read as
// the window starts and each time it comes back to the front.

// One of the reader's items, made an item of the menus: a command's own, under the reader's label
// where it gives one; one that names no command is drawn but cannot be chosen.
function readerItem(given, commands) {
  if (given === "-") {
    return "-";
  }
  const asked = typeof given === "string" ? { command: given } : given && typeof given === "object" ? given : {};
  if (Array.isArray(asked.items)) {
    return { label: String(asked.label ?? "…"), items: asked.items.map((one) => readerItem(one, commands)) };
  }
  const known = commands.get(asked.command);
  if (!known) {
    return { label: `${asked.label ?? asked.command ?? "?"}: no such command`, command: "", needs: "never" };
  }
  return { ...known, label: typeof asked.label === "string" ? asked.label : known.label };
}

const commandsOf = () => new Map(state.menus.flatMap((menu) => menu.all ?? []).map((item) => [item.command, item]));

// The menus as the bar draws them: orior's, with the reader's laid over them.
function laidOut() {
  const reader = state.reader;
  if (!reader) {
    return state.menus;
  }
  const commands = commandsOf();
  const own = (Array.isArray(reader.menus) ? reader.menus : []).filter((menu) => typeof menu?.title === "string").map((menu) => ({ title: menu.title, items: (Array.isArray(menu.items) ? menu.items : []).map((one) => readerItem(one, commands)) }));
  const hidden = new Set((Array.isArray(reader.hide) ? reader.hide : []).map(String));
  const laid = state.menus.map((menu) => own.find((one) => one.title === menu.title) ?? menu).filter((menu) => !hidden.has(menu.title));
  const added = own.filter((one) => !state.menus.some((menu) => menu.title === one.title));
  const help = laid.findIndex((menu) => menu.title === "Help");
  laid.splice(help < 0 ? laid.length : help, 0, ...added);
  return laid;
}

const TOOLBAR = "orior.toolbar";
export const toolbarShown = () => localStorage.getItem(TOOLBAR) !== "false";

export function setToolbarShown(on = !toolbarShown()) {
  localStorage.setItem(TOOLBAR, String(on));
  drawToolbar();
}

// The toolbar under the menus: a button for each of the reader's toolbar items, its icon the one it
// names or its label's first letter, and a line for each "-".
function drawToolbar() {
  const bar = document.getElementById("toolbar");
  const list = Array.isArray(state.reader?.toolbar) ? state.reader.toolbar : [];
  bar.hidden = !list.length || !toolbarShown();
  const commands = commandsOf();
  bar.replaceChildren(
    ...list.map((given) => {
      if (given === "-") {
        return Object.assign(document.createElement("span"), { className: "toolbar-line" });
      }
      const item = readerItem(given, commands);
      const glyph = typeof given?.icon === "string" ? given.icon : (item.label ?? "?").trim()[0] ?? "?";
      const button = Object.assign(document.createElement("button"), { className: "toolbar-button", type: "button", title: item.keys ? `${item.label} (${item.keys})` : item.label });
      button.setAttribute("aria-label", item.label);
      button.append(icon(glyph.length === 1 ? glyph.toUpperCase() : glyph));
      button.dataset.needs = item.needs ?? "";
      button.addEventListener("click", () => item.command && runCommand(item.command));
      return button;
    }),
  );
  freshenToolbar();
}

// Marks each button of the toolbar that cannot act now.
function freshenToolbar() {
  for (const button of document.querySelectorAll("#toolbar .toolbar-button")) {
    const need = button.dataset.needs;
    button.disabled = need === "never" || Boolean(need && !NEEDS[need]?.());
  }
}

// Reads menus.json again, and lays the menus and the toolbar out again where it changed.
export async function loadReaderMenus() {
  const text = await invoke("reader_menus_read").catch(() => "");
  if (text === state.readerText) {
    return;
  }
  state.readerText = text;
  try {
    state.reader = text.trim() ? JSON.parse(text) : null;
  } catch (error) {
    state.reader = null;
    say(`menus.json is not JSON: ${error.message}`, { failed: true });
  }
  drawMenubar();
  drawToolbar();
}

// File, Menus and Toolbar: opens menus.json, made first where it is not there, in the program the
// system opens it with.
async function openReaderMenus() {
  await invoke("home_reveal", { what: "menus" }).catch((error) => say(String(error), { failed: true }));
  await loadReaderMenus();
}

// The menus of jobs the tool strip holds in place of the bar, each its title, its icon and its
// items, where its groups have a job in the tree.
export function stripMenus() {
  return state.menus.filter((menu) => menu.strip && jobsIn(menu.groups ?? []).length).map((menu) => ({ title: menu.title, icon: menu.strip, items: () => itemsOf(menu) }));
}

// Gives each title the first of its letters no title before it has.
function lettersFor(menus) {
  const taken = new Set();
  return menus.map((menu) => {
    const at = [...menu.title.toLowerCase()].findIndex((letter) => /[a-z]/.test(letter) && !taken.has(letter));
    if (at >= 0) {
      taken.add(menu.title[at].toLowerCase());
    }
    return at;
  });
}

function titleOf(menu, letter) {
  const button = Object.assign(document.createElement("button"), { type: "button", className: "bar-title" });
  button.setAttribute("role", "menuitem");
  button.setAttribute("aria-haspopup", "menu");
  if (letter >= 0) {
    button.append(menu.title.slice(0, letter), Object.assign(document.createElement("u"), { textContent: menu.title[letter] }), menu.title.slice(letter + 1));
    button.dataset.letter = menu.title[letter].toLowerCase();
  } else {
    button.textContent = menu.title;
  }
  return button;
}

// Draws the bar's titles, the ones that fit and the rest in the last.
export function drawMenubar() {
  const bar = state.bar;
  if (!bar) {
    return;
  }
  closeMenu(false);
  state.open = -1;
  const menus = shownMenus();
  const letters = lettersFor(menus);
  bar.replaceChildren(...menus.map((menu, at) => titleOf(menu, letters[at])));
  state.titles = menus.map((menu, at) => ({ items: () => itemsOf(menu), menu, button: bar.children[at] }));
  const more = { title: "…" };
  const moreButton = titleOf(more, -1);
  moreButton.ariaLabel = "More";
  bar.append(moreButton);
  const most = TITLES_AT.find(([width]) => window.innerWidth >= width)[1];
  if (state.titles.length > most || bar.scrollWidth > bar.clientWidth) {
    const folded = [];
    while ((state.titles.length > most || bar.scrollWidth > bar.clientWidth) && state.titles.length > 1) {
      const last = state.titles.pop();
      last.button.remove();
      folded.unshift(last.menu);
    }
    state.titles.push({ items: () => folded.map((menu) => ({ label: menu.title, items: itemsOf(menu) })), menu: more, button: moreButton });
  } else {
    moreButton.remove();
  }
  state.titles.forEach(({ button }, at) => {
    button.addEventListener("click", () => (state.open === at && menuOpen() ? closeMenu() : openAt(at, true)));
    button.addEventListener("pointerenter", () => menuOpen() && state.open >= 0 && state.open !== at && openAt(at, false));
  });
  // The tool strip draws the menus it holds again with the bar's.
  window.dispatchEvent(new Event("menus-drawn"));
}

// The keys a command lists, or undefined where it lists none.
export function keysOf(command) {
  for (const menu of state.menus) {
    const item = menu.all.find((one) => one.command === command);
    if (item) {
      return item.keys;
    }
  }
  return undefined;
}

// Opens the menu under the at'th title. The keys go into it where it was opened from the keyboard or
// by a press, and stay where they are where the pointer only passed onto its title.
function openAt(at, keyed) {
  const count = state.titles.length;
  const index = ((at % count) + count) % count;
  const { menu, button, items: itemsFor } = state.titles[index];
  const box = button.getBoundingClientRect();
  state.bar.querySelectorAll(".bar-title").forEach((one) => one.removeAttribute("aria-expanded"));
  button.setAttribute("aria-expanded", "true");
  const items = itemsFor();
  showMenu(box.left, box.bottom + 2, items.length ? items : [{ label: menu.title, disabled: true, run: () => {} }], {
    side: (by) => openAt(index + by, true),
    keep: state.bar,
    onClose: () => {
      button.removeAttribute("aria-expanded");
      state.open = -1;
    },
    focusFirst: keyed,
  });
  state.open = index;
  leaveBar(false);
}

// The bar holding the keys after Alt alone, its letters marked.
function holdBar() {
  state.before = document.activeElement;
  state.bar.classList.add("held");
  state.titles[0]?.button.focus();
}

function leaveBar(refocus) {
  if (!state.bar.classList.contains("held")) {
    return;
  }
  state.bar.classList.remove("held");
  if (refocus && state.before?.isConnected) {
    state.before.focus();
  }
}

// The key names commands.json writes, as the page names the key: a letter or digit by its place on
// the keyboard, and the rest by name.
// The menus' names for keys a code names otherwise, the other way about from KEY_CODES.
const KEY_NAMES = { Backquote: "`", Backslash: "\\", Slash: "/", ArrowUp: "Up", ArrowDown: "Down", ArrowLeft: "Left", ArrowRight: "Right" };

const KEY_CODES = { "`": "Backquote", "\\": "Backslash", "/": "Slash", Up: "ArrowUp", Down: "ArrowDown", Left: "ArrowLeft", Right: "ArrowRight" };

// Whether an event is the keys an item lists. Two presses, as Ctrl+K Ctrl+0, are the editor's.
function pressed(keys, event) {
  if (!keys || keys.includes(" ")) {
    return false;
  }
  const parts = keys.split("+");
  const key = parts.pop() || "+";
  if (parts.includes("Ctrl") !== event.ctrlKey || parts.includes("Shift") !== event.shiftKey || parts.includes("Alt") !== event.altKey) {
    return false;
  }
  if (/^[A-Z]$/.test(key)) {
    return event.code === `Key${key}`;
  }
  if (/^\d$/.test(key)) {
    return event.code === `Digit${key}`;
  }
  return KEY_CODES[key] ? event.code === KEY_CODES[key] : event.key === key;
}

// Runs the command whose keys an event is, where nothing nearer the focus took the keys first. The
// keys of a command for the editor are the editor's own, and a text field or the run's output keeps
// them for itself. Of two commands with the same keys, the first that can act now runs, as F5
// continues a stopped program and otherwise starts the chosen job. A key bound here never reaches
// the web view, even where no command of it can act.
function onShortcut(event) {
  if (event.defaultPrevented || menuOpen()) {
    return;
  }
  let bound = false;
  for (const menu of state.menus) {
    for (const item of menu.all) {
      if (item.needs !== "editor" && (pressed(item.keys, event) || pressed(item.also, event))) {
        bound = true;
        if (!item.needs || NEEDS[item.needs]?.()) {
          event.preventDefault();
          runCommand(item.command);
          return;
        }
      }
    }
  }
  if (bound) {
    event.preventDefault();
  }
}

// Runs the command the command line opened the window for, once the tree is read.
export async function runLaunch() {
  const launch = await invoke("launch_take").catch(() => null);
  if (launch?.command) {
    runCommand(launch.command, launch.args);
  }
}

// Starts the menu bar and every key its commands are bound to. `commands` is the reading of the
// commands, where it was asked for already. The page marks the moment the keys are bound as
// "keys-bound" in its timeline.
export async function startMenubar({ openFolder, commands = invoke("commands_read") }) {
  state.bar = document.getElementById("menubar");
  state.openFolder = openFolder;
  state.menus = JSON.parse(await commands).menus;
  // Each menu's commands in one list, those of its groups among them.
  const flat = (items) => items.flatMap((item) => (item === "-" || item.fill ? [] : item.items ? flat(item.items) : [item]));
  state.menus.forEach((menu) => (menu.all = flat(menu.items ?? [])));
  bindEditorKeys(
    state.menus.flatMap((menu) =>
      menu.all.filter((item) => item.needs === "editor").flatMap((item) => [item.keys, item.also].filter(Boolean).map((keys) => ({ keys, run: () => runCommand(item.command) }))),
    ),
  );
  startPalette({
    commands: paletteCommands,
    findFiles: (query, recent, most) => invoke("files_find", { query, recent, most }),
    recent: recentFiles,
    symbols: () => editing().symbols(),
    treeSymbols: (query) => invoke("symbols_find", { query }),
    lineCount: () => editing().lineCount(),
    goLine: (line, col) => editing().goLine(line, col),
    openFile: (path, line, col) => {
      showView("edit");
      if (line === null) {
        openFile(path);
      } else {
        openFileAt(path, line, col);
      }
    },
  });
  state.bar.addEventListener("keydown", (event) => {
    const at = state.titles.findIndex(({ button }) => button === document.activeElement);
    if (at < 0) {
      return;
    }
    const step = { ArrowRight: 1, ArrowLeft: -1 }[event.key];
    if (step) {
      state.titles[(at + step + state.titles.length) % state.titles.length].button.focus();
    } else if (event.key === "ArrowDown" || event.key === "Enter" || event.key === " ") {
      openAt(at, true);
    } else if (event.key === "Escape") {
      leaveBar(true);
    } else {
      return;
    }
    event.preventDefault();
  });
  state.bar.addEventListener("focusout", (event) => !state.bar.contains(event.relatedTarget) && state.bar.classList.remove("held"));
  // The reader's menus and toolbar, and again each time the window comes back to the front.
  loadReaderMenus();
  window.addEventListener("focus", () => loadReaderMenus());
  document.getElementById("toolbar").addEventListener("pointerenter", freshenToolbar);
  window.addEventListener("keydown", (event) => {
    if (event.key === "Alt") {
      state.held = true;
      document.body.classList.add("alt-held");
      return;
    }
    state.held = false;
    if (event.altKey && !event.ctrlKey && !event.metaKey && /^Key[A-Z]$/.test(event.code)) {
      const letter = event.code.slice(3).toLowerCase();
      const at = state.titles.findIndex(({ button }) => button.dataset.letter === letter);
      if (at >= 0) {
        event.preventDefault();
        document.body.classList.remove("alt-held");
        openAt(at, true);
      }
      return;
    }
    onShortcut(event);
  });
  window.addEventListener("keyup", (event) => {
    if (event.key !== "Alt") {
      return;
    }
    document.body.classList.remove("alt-held");
    if (state.held && !menuOpen()) {
      event.preventDefault();
      if (state.bar.classList.contains("held")) {
        leaveBar(true);
      } else {
        holdBar();
      }
    }
    state.held = false;
  });
  window.addEventListener("blur", () => document.body.classList.remove("alt-held"));
  let wait = 0;
  window.addEventListener("resize", () => {
    window.clearTimeout(wait);
    wait = window.setTimeout(drawMenubar, 120);
  });
  drawMenubar();
  document.getElementById("status-branch").addEventListener("click", showBranches);
  document.getElementById("status-memory").addEventListener("click", showMemory);
  bindMacros();
  onMacros(bindMacros);
  performance.mark("keys-bound");
  state.autoReport = await invoke("report_auto").catch(() => true);
  const [asked, question] = await invoke("report_asked").catch(() => [true, ""]);
  if (!asked) {
    askReports(sheet, question, (on) => {
      state.autoReport = on;
    });
  }
}
