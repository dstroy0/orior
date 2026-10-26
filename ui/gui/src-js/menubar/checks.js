// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { motionReduced } from "../motion.js";
import { focusOn, say } from "../statusbar.js";
import { canStepBack, debugging, isPaused } from "../debug.js";
import { authorsShown } from "../authors.js";
import { bindReaderKeys, crumbsShown, editing, saving, marginLine } from "../edit.js";
import { forgetMacro, keepMacro, keptMacros, lastMacro, playMacro, recording, toggleRecording } from "../macros.js";
import { testsParallel } from "../tests.js";
import { offNetwork } from "../network.js";
import { chosenJob, chosenLive } from "../run.js";
import { followsSystem } from "../scheme.js";
import { autoCollapse, paneShown } from "../sides.js";
import { toolWindowsItems } from "../docks.js";
import { statusItems, stripEdge } from "../statusitems.js";
import { focusFollows } from "../focusfollow.js";
import { systemTitleBar } from "../strip.js";
import { previewMode } from "../preview.js";
import { isRequests } from "../requests.js";
import { terminalSplit } from "../terminal.js";
import { showView } from "../views.js";
import { fuzzyFiles, state } from "../menubar.js";
import { COMMANDS } from "./commands.js";
import { askFor } from "./sheets.js";
import { toolbarShown } from "./reader.js";

// Whether an item that is on or off is on, by the name commands.json gives it.
export const CHECKS = {
  focus: () => focusOn(),
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
  "reduce-motion": () => motionReduced(),
  "fuzzy-files": () => fuzzyFiles(),
  "word-wrap": () => editing().wordWrap(),
  "operator-next-line": () => editing().operatorNext(),
  "type-hints": () => editing().hints("type"),
  "parameter-hints": () => editing().hints("parameter"),
  "flick-scroll": () => editing().flickScroll(),
  "auto-report": () => state.autoReport,
  "network-off": offNetwork,
  "scheme-system": followsSystem,
  split: () => editing().splitShown(),
  "terminal-split": terminalSplit,
  "side-tabs": () => editing().sideTabs(),
  "strip-edge": stripEdge,
  toolbar: () => toolbarShown(),
  "margin-line": () => marginLine(),
  preview: () => previewMode() === "beside",
  "preview-alone": () => previewMode() === "alone",
  "focus-follows": () => focusFollows(),
  "system-title": () => systemTitleBar(),
  "macro-recording": recording,
};

// Whether the item that is on or off by the name `name` is on, as its menu shows it.
export const isChecked = (name) => Boolean(CHECKS[name]?.());

// What a command needs before it can act, by the name commands.json gives the need.
export const NEEDS = {
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
  sql: () => /\.sql$/i.test(editing().fileHere() ?? ""),
  http: () => isRequests(editing().fileHere() ?? ""),
};

// Edit, Macros: recording starts in the editor and stops on a second press, and the last macro
// recorded plays back, or is kept under a name. Each kept macro is listed under them, to play once or
// many times, to bind keys to, or to forget.
export function recordMacro() {
  const { editor } = editing();
  if (!editor && !recording()) {
    return;
  }
  const steps = toggleRecording(editor);
  say(steps === null ? "Recording a macro: Record Macro again stops it." : steps.length ? `Recorded a macro of ${steps.length} ${steps.length === 1 ? "step" : "steps"}.` : "Nothing was recorded.");
}

export async function playBack(times, name) {
  const steps = name ? keptMacros().find((one) => one.name === name)?.steps : lastMacro();
  if (!steps) {
    say(name ? `No macro is kept as ${name}.` : "No macro is recorded yet.", { failed: true });
    return;
  }
  showView("edit");
  editing().editor?.focus();
  await playMacro(editing().editor, steps, times);
}

export async function keepLast(given) {
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
      { label: "Play", keys: one.keys || undefined, command: "macro-play", args: ["1", one.name], run: () => playBack(1, one.name) },
      {
        label: "Play Many Times…",
        command: "macro-play",
        args: ["2", one.name],
        run: async () => {
          const times = Number.parseInt(await askFor(`Times to play ${one.name}`, "2"), 10);
          if (times > 0) {
            playBack(times, one.name);
          }
        },
      },
      { label: "Keys…", command: "macro-keys", args: [one.name], run: () => COMMANDS["macro-keys"]([one.name]) },
      { label: "Forget", command: "macro-forget", args: [one.name], run: () => forgetMacro(one.name) },
    ],
  }));
  return items.length ? ["-", ...items] : [];
}

// Binds each kept macro's keys in the editor, to play it where they are pressed.
export function bindMacros() {
  bindReaderKeys(keptMacros().filter((one) => one.keys).map((one) => ({ keys: one.keys, run: (editor) => playMacro(editor, one.steps) })));
}

// The items commands.json names to be filled in as their menu opens.
export const FILLS = { macros: macroItems, "tool-windows": toolWindowsItems, "status-items": statusItems };

