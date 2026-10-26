// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { invoke, pick } from "../bridge.js";
import { motionReduced, setReduceMotion } from "../motion.js";
import { showClone } from "../clone.js";
import { showProject } from "../create.js";
import { say, setFocus } from "../statusbar.js";
import { showBookmarks, toggleBookmark } from "../bookmarks.js";
import { askStepping, attachAddress, debugCommands, attachProcess, debugFile, debugMixed, exportBreakpoints, importBreakpoints, recordFile, restartDebug, showMachineCode, showMemory as showDebugMemory, step, stopDebug, toggleBreakpointHere, toggleDebugPanel, watchAddress } from "../debug.js";
import { authorsShown, setAuthorsShown } from "../authors.js";
import { addToWindow, editing, openFile, popupCommands, saving, setCrumbs, setSaving, setMarginLine, setVimKeys, vimKeys } from "../edit.js";
import { forgetMacro, setMacroKeys } from "../macros.js";
import { explorerCommands, showPane } from "../explorer.js";
import { commitCommands } from "../commit.js";
import { openPalette } from "../palette.js";
import { runAllTests, runFailedTests, setTestsParallel, stopTests, testCommands, testsParallel } from "../tests.js";
import { drawPreferences, prefsCommands, showPreferences } from "../preferences.js";
import { carryPlace, carrySend, carryTake, carryUndo, setCarryPlace } from "../carry.js";
import { offNetwork, setOffNetwork, showNetwork } from "../network.js";
import { showGenerator, showPlugins } from "../pluginsheet.js";
import { loadPlugins } from "../plugins.js";
import { runToolchains } from "../toolchains.js";
import { askTrust } from "../trust.js";
import { loadUserCss, openUserCss } from "../usercss.js";
import { zoomBy } from "../zoom.js";
import { printText } from "../print.js";
import { showMenu } from "../menu.js";
import { chosenJob, runCommands, startChosen, stopChosen } from "../run.js";
import { followsSystem, setFollowSystem, setScheme, toggleScheme } from "../scheme.js";
import { setAutoCollapse, togglePane } from "../sides.js";
import { dockCommands, float, floatItems, solo } from "../docks.js";
import { openWindow } from "../windows.js";
import { setStripEdge, statusCommands } from "../statusitems.js";
import { setFocusFollows } from "../focusfollow.js";
import { setSystemTitleBar, stripCommands } from "../strip.js";
import { profileCommands, profileFile, profileRemote, stopProfile, toggleProfile } from "../profile.js";
import { openRemote, remoteCommands } from "../remote.js";
import { containerCommands, toggleContainers } from "../containers.js";
import { toggleComms } from "../comms.js";
import { reviewChange } from "../changeview.js";
import { gatherLines, toggleBoard } from "../gather.js";
import { peekHere } from "../peek.js";
import { printHere } from "../prints.js";
import { findSlowPlugin } from "../scripts.js";
import { previewMode, setPreview } from "../preview.js";
import { copyCurl, privateEnvironment, sendRequest } from "../requests.js";
import { databaseCommands, runSqlFile, runStatement, toggleDatabase } from "../database.js";
import { clearTerminal, killTerminal, newTerminal, runInTerminal, splitTerminal, terminalCommands, toggleTerminal } from "../terminal.js";
import { intelCommands } from "../intel.js";
import { showView } from "../views.js";
import { reportForm } from "../reports.js";
import { fuzzyFiles, onOff, protocolJob, runCommand, setFuzzyFiles, state } from "../menubar.js";
import { clipOut, create, createRepository, findInFiles, goToBridge, goToFile, openAnyFile, openWorkspace, pasted, searchJobs } from "./files.js";
import { CHECKS, keepLast, playBack, recordMacro } from "./checks.js";
import { branchDo, cherryPick, gitSays, newBranch, removeBranch, renameBranch, resolveConflicts, showBranches, showStashes, stashChanges, stashDo } from "./git.js";
import { askBudget, askCheckers, askContinuation, askDocMargin, askMavenSettings, chooseEnvironment, showTests, updateSnapshots } from "./settings.js";
import { askFor, askKeys, askYes, sheet, showAbout, showChanges, showShortcuts } from "./sheets.js";
import { loadReaderMenus, openReaderMenus, setToolbarShown } from "./reader.js";

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

// What each command does in the window, given the words the command line passed it, if any.
// The plugins, the stylesheet and the menus of the reader's own read again, as settings taken in
// from another machine change them.
async function reloadOwn() {
  await loadPlugins();
  await loadUserCss();
  await loadReaderMenus();
}

// The settings no menu lists, which Preferences sets on and off, by the names the command line gives.
const MORE_SETTINGS = {
  trim: { label: "Trim Trailing Whitespace", on: () => saving("trim"), set: (on) => setSaving("trim", on) },
  "final-newline": { label: "Insert Final Newline", on: () => saving("final-newline"), set: (on) => setSaving("final-newline", on) },
  "vim-keys": { label: "Vim's Keys", on: vimKeys, set: setVimKeys },
};

export const COMMANDS = {
  "create-file": (args) => create(false, args[0]),
  "create-folder": (args) => create(true, args[0]),
  "create-repository": (args) => createRepository(args[0] ?? null),
  "create-project": (args) => showProject(sheet, { given: args[0] ?? null, made: (folder) => state.openFolder(folder) }),
  "keep-template": async (args) => {
    const name = args[0] || (await askFor("A name for the template the tree is kept as", ""));
    if (name) {
      invoke("template_save", { name })
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
  "database-view": () => toggleDatabase(),
  "comms-view": () => toggleComms(),
  preview: (args) => setPreview(args[0] ?? (previewMode() === "beside" ? "off" : "beside")),
  "preview-alone": () => setPreview(previewMode() === "alone" ? "off" : "alone"),
  "run-statement": () => runStatement(),
  "send-request": () => sendRequest(),
  "copy-as-curl": () => copyCurl(),
  "private-environment": () => privateEnvironment(),
  "run-sql-file": () => runSqlFile(),
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
  cut: inEditor((e) => clipOut(e, true)),
  copy: inEditor((e) => clipOut(e, false)),
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
  "reduce-motion": (args) => setReduceMotion(onOff(args) ?? !motionReduced()),
  "fuzzy-files": (args) => setFuzzyFiles(onOff(args) ?? !fuzzyFiles()),
  "word-wrap": (args) => editing().setWordWrap(onOff(args) ?? !editing().wordWrap()),
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
      openFile: (path) => {
        showView("edit");
        return openFile(path);
      },
      reloadOwn,
      checks: CHECKS,
      more: Object.values(MORE_SETTINGS),
    }),
  // The settings no menu lists, on and off.
  "prefs-setting": ([name, word]) => {
    const setting = MORE_SETTINGS[name];
    if (!setting) {
      say(`The settings are ${Object.keys(MORE_SETTINGS).join(", ")}.`, { failed: true });
      return;
    }
    setting.set(onOff([word]) ?? !setting.on());
    drawPreferences();
  },
  "carry-place": async (words) => {
    const path = words.length ? words.join(" ") : await pick("dir").catch(() => null);
    if (typeof path === "string") {
      setCarryPlace(path);
      drawPreferences();
    }
  },
  "carry-send": async (words) => {
    const to = words.join(" ") || carryPlace();
    try {
      say(to ? await carrySend(to) : "Name a file or a folder first.", { failed: !to });
    } catch (error) {
      say(String(error), { failed: true });
    }
  },
  "carry-take": async ([from = carryPlace(), ...chosen]) => {
    try {
      const taking = from ? await carryTake(from, chosen, reloadOwn) : null;
      say(!from ? "Name a file or a folder first." : taking ? `Took ${taking.files.length} file${taking.files.length === 1 ? "" : "s"} from ${from}.` : `${from} holds the settings this machine has.`, { failed: !from });
    } catch (error) {
      say(String(error), { failed: true });
    }
    drawPreferences();
  },
  "carry-undo": async ([id]) => {
    try {
      const taking = await carryUndo(id, reloadOwn);
      say(`The taking from ${taking.from} is undone.`);
    } catch (error) {
      say(String(error), { failed: true });
    }
    drawPreferences();
  },
  "user-css": () => openUserCss().catch((error) => say(String(error), { failed: true })),
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
  // A kept macro's keys, as given or as pressed, empty taking its keys away.
  "macro-keys": async ([name, keys]) => {
    const pressed = keys ?? (await askKeys(`Keys for ${name}`));
    if (pressed !== null) {
      setMacroKeys(name, pressed);
    }
  },
  "macro-forget": ([name]) => forgetMacro(name),
  "branch-switch": ([name]) => branchDo("switch", name),
  "branch-merge": ([name]) => branchDo("merge", name),
  "branch-rebase": ([name]) => branchDo("rebase", name),
  "branch-rename": ([name, to]) => renameBranch(name, to),
  "branch-delete": ([name]) => removeBranch(name),
  "stash-apply": ([name]) => stashDo("apply", name),
  "stash-pop": ([name]) => stashDo("pop", name),
  // A stash thrown away asks first, at the command line as in the menu.
  "stash-drop": async ([name]) => {
    const stash = (await invoke("git_stashes", { repo: editing().repo() }).catch(() => [])).find((one) => one.name === name);
    if (stash && (await askYes(`Throw away ${stash.name}, ${stash.subject}?`, "Drop"))) {
      await stashDo("drop", name);
    }
  },
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
  "peek-definition": inEditor(() => peekHere("definition")),
  "peek-usages": inEditor(() => peekHere("usages")),
  "peek-callers": inEditor(() => peekHere("callers")),
  "gather-lines": inEditor(() => gatherLines()),
  "gathered-lines": () => toggleBoard(),
  focus: () => setFocus(),
  "find-slow-plugin": () => findSlowPlugin(),
  "review-structure": (args) => reviewChange(args[0] ?? null),
  "print-here": inEditor(() => printHere()),
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
    showMenu(window.innerWidth / 3, window.innerHeight / 4, Object.entries(forms).map(([form, label]) => ({ label, checked: editing().docstringForm() === form, command: "docstring-form", args: [form], run: () => editing().setDocstringForm(form) })));
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
  "disasm-verify": () => editing().verifyDisassembly(),
  list: (args) => searchJobs(args[0]),
  show: (args) => searchJobs(args[0]),
  // The protocol: its one job, the course, shown and with words from the command line started, the run
  // in the protocol panel; and its other commands, which the command line says and the menu does not list.
  course: protocolJob("protocol/course"),
  tables: protocolJob("protocol/tables"),
  decode: protocolJob("protocol/decode"),
  query: protocolJob("protocol/query"),
  learn: protocolJob("protocol/learn"),
  cross: protocolJob("protocol/cross"),
  write: protocolJob("protocol/write"),
  pairs: protocolJob("protocol/pairs"),
  stick: protocolJob("protocol/stick"),
  retention: protocolJob("protocol/retention"),
  sweep: protocolJob("protocol/sweep"),
  new: newTerminal,
  toggle: () => toggleTerminal(),
  split: () => splitTerminal(),
  clear: clearTerminal,
  kill: killTerminal,
  keys: showShortcuts,
  report: (args) => reportForm(sheet, args),
  "auto-report": async (args) => {
    const on = args[0] === "on" ? true : args[0] === "off" ? false : !state.autoReport;
    try {
      await invoke("report_auto_set", { on });
      state.autoReport = on;
    } catch (error) {
      say(String(error), { failed: true });
    }
  },
  network: () => showNetwork(sheet),
  "network-off": (args) => setOffNetwork(onOff(args) ?? !offNetwork()),
  about: showAbout,
  "whats-new": showChanges,
  "trust-tree": () => askTrust(),
};

// A command of the menus or of the popup menus, which take what each item is over, read as it is run.
export const commandNamed = (command) =>
  [COMMANDS, popupCommands, explorerCommands, commitCommands, runCommands, terminalCommands, statusCommands, containerCommands, databaseCommands, remoteCommands, debugCommands, dockCommands, stripCommands, testCommands, profileCommands, intelCommands, prefsCommands].find((table) => Object.hasOwn(table, command))?.[command];

