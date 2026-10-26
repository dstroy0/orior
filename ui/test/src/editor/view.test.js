// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's view: how far ahead of a view at rest its lines are colored, its marks drawn at every speed, text
// that runs right to left placed where the page draws it, and a letter typed drawn in the frame after its key.

import { coloringReach, runsRightToLeft } from "../../../gui/src-js/editor/view.js";
import { invoke } from "../../../gui/src-js/bridge.js";
import { editing, onEdit, openAt, openFile, setVimKeys, vimKeys } from "../../../gui/src-js/edit.js";
import { runCommand } from "../../../gui/src-js/menubar.js";
import { showView } from "../../../gui/src-js/views.js";
import { write } from "../../../gui/src-js/status.js";
import { framesOver } from "../../frames.js";

const { test, assert, ui } = window.__harness;

test("a view that narrows with lines shorter than it has no sideways scroll, in the frame it narrows", async () => {
  showView("edit");
  await openFile("hello.py");
  await ui.waitFor(() => editing().active === "hello.py" && editing().editor?.s, 10000);
  await ui.rest(300);
  const editor = document.querySelector("#mode-edit .ed");
  const scroller = editor.querySelector(".ed-scroll");
  const width = editor.getBoundingClientRect().width;
  try {
    editor.style.width = `${Math.round(width - 240)}px`;
    assert.ok(scroller.scrollWidth <= scroller.clientWidth, `no sideways scroll as it narrows: ${scroller.scrollWidth} in ${scroller.clientWidth}`);
  } finally {
    editor.style.width = "";
  }
});

test("a view that came to rest slowly is colored three screens ahead", () => {
  assert.equal(coloringReach(40, 0, 30), 120);
  assert.equal(coloringReach(40, 0.1, 30), 120);
});

test("a view that came to rest fast is colored as far as its speed carries it over two screens' coloring", () => {
  const fast = coloringReach(40, 100, 30);
  assert.ok(fast > 120, `farther than three screens: ${fast}`);
  const twice = coloringReach(40, 200, 30);
  assert.ok(Math.abs(twice - 2 * fast) <= 2, `twice the speed reaches twice as far: ${fast}, ${twice}`);
  assert.ok(Math.abs(coloringReach(40, 100, 60) - 2 * fast) <= 2, "twice the coloring time reaches twice as far");
});

test("at every speed the gutter's change marks and the diagnostics are drawn, as at rest", async () => {
  showView("edit");
  await openFile("hello.py");
  await ui.waitFor(() => editing().editor?.s, 5000);
  const view = editing().editor;
  const s = view.s;
  const root = view.gutter.closest(".ed") ?? view.gutter.parentElement;
  const drawn = () => ({ change: Boolean(view.gutter.querySelector(".ed-change")), diagnostic: Boolean(root.querySelector(".ed-diag")) });
  view.setChanges(s, { added: new Set([0]), changed: new Set(), removed: new Set() });
  s.diagnostics = [{ from: { line: 0, col: 0 }, to: { line: 0, col: 3 }, severity: 1, message: "a test's" }];
  try {
    view.paint();
    assert.deepEqual(drawn(), { change: true, diagnostic: true }, "at rest everything is drawn");
    write("scroll", { speed: 12 });
    view.paint();
    assert.deepEqual(drawn(), { change: true, diagnostic: true }, "and at speed the same");
    assert.ok(view.gutter.querySelector(".ed-num-text"), "with the numbers");
  } finally {
    write("scroll", { speed: 0 });
    s.diagnostics = [];
    view.setChanges(s, null);
    view.paint();
  }
});

test("each character runs as its script does, and a space between two of a right-to-left script runs with them", () => {
  assert.deepEqual(runsRightToLeft("ab שלום cd"), [false, false, false, true, true, true, true, false, false, false]);
  assert.deepEqual(runsRightToLeft("שלום עולם"), [true, true, true, true, true, true, true, true, true]);
  assert.deepEqual(runsRightToLeft("a 1"), [false, false, false]);
});

test("on a line where Hebrew meets text that runs left to right, the caret goes by the order the text is read in and stands where the page draws it", async () => {
  await invoke("file_write", { path: "bidi.txt", text: "abc שלום def\nمرحبًا بك\n" });
  showView("edit");
  await openFile("bidi.txt");
  await ui.waitFor(() => editing().active === "bidi.txt" && editing().editor?.s, 5000);
  const view = editing().editor;
  view.paint();
  const x = (col) => view.xOf({ line: 0, col });
  assert.ok(x(4) > x(5) && x(5) > x(6) && x(6) > x(7), `inside the Hebrew word the caret steps to the left as the reading goes on: ${[4, 5, 6, 7].map(x)}`);
  assert.ok(x(3) < x(7) && x(9) > x(4), "the words that run left to right stand on either side of it");
  view.goTo(0, 4);
  view.focus();
  await ui.key("Right");
  assert.equal(view.primary().head.col, 5, "the arrow goes on to the next character as it is read");
  const node = view.rowNode(0);
  const shin = view.charBox(node, 4);
  const pressed = view.posAt({ clientX: shin.right - 1, clientY: shin.top + shin.height / 2 });
  assert.deepEqual([pressed.line, pressed.col], [0, 4], "a press on the first half of a letter, as it is read, lands before it");
  const later = view.posAt({ clientX: shin.left + 1, clientY: shin.top + shin.height / 2 });
  assert.equal(later.col, 5, "and on its second half, after it");
  view.setSelections([{ anchor: { line: 0, col: 4 }, head: { line: 0, col: 6 }, goal: null }]);
  view.paint();
  const path = view.picked.querySelector("path")?.getAttribute("d") ?? "";
  const xs = [...path.matchAll(/[ML]\s*([\d.]+)/g)].map((one) => Number(one[1]));
  assert.ok(xs.length && Math.min(...xs) >= x(6) - 2 && Math.max(...xs) <= x(4) + 2, `the selection covers the two letters where they are drawn: ${xs} against ${x(6)}..${x(4)}`);
});

test("the word keys stop at the ends of words of either script, a letter's marks part of its word", async () => {
  await openFile("bidi.txt");
  await ui.waitFor(() => editing().active === "bidi.txt", 5000);
  const view = editing().editor;
  view.goTo(1, 0);
  view.focus();
  await ui.key("Ctrl+Right");
  assert.equal(view.primary().head.col, 6, "past the fathatan, to the end of the word");
  await ui.key("Ctrl+Right");
  await ui.key("Ctrl+Left");
  assert.equal(view.primary().head.col, 7, "back to the start of the next word");
});

// Whether the listener the test gives onEdit is listening: it stays given for the page's life.
let hearing = null;

onEdit((s) => hearing?.(s));

test("a letter typed is drawn before anything told of each edit hears of it", async () => {
  await invoke("file_write", { path: "told.py", text: "first = 1\n" });
  showView("edit");
  await openFile("told.py");
  await ui.waitFor(() => editing().active === "told.py" && editing().editor?.s, 5000);
  const view = editing().editor;
  view.goTo(0, 0);
  view.focus();
  const heard = [];
  hearing = () => heard.push(view.rowNode(0)?.textContent ?? "");
  try {
    await ui.type("x");
    await ui.waitFor(() => heard.length > 0, 2000);
  } finally {
    hearing = null;
  }
  assert.equal(view.s.doc.line(0), "xfirst = 1");
  assert.ok(heard[0].startsWith("xfirst"), `the row as the first told of the edit found it: ${JSON.stringify(heard[0])}`);
});

// Holds a letter down in the editor shown for `count` presses, as the system's repeat sends a held
// key, and gives, for each press: whether the letter was in its row when the frame after it began,
// how far the caret stood from where it stood before the key, the page's own milliseconds for it from
// its key to its row drawn, and the milliseconds from the frame after it to the next, the display's
// interval where that frame came on time. Each is kept as it comes and given once the key is let go.
async function keysTimed(view, line, count) {
  const timed = [];
  let key = null;
  let typed = 0;
  const caretAt = () => parseFloat(view.host.querySelector(".ed-caret.main")?.style.left ?? "NaN");
  const began = (event) => {
    if (event.inputType !== "insertText") {
      return;
    }
    const at = performance.now();
    typed += 1;
    const want = event.data.repeat(typed);
    const one = { at, from: caretAt() };
    key = one;
    requestAnimationFrame((first) => {
      const drawn = (view.rowNode(line)?.textContent ?? "").includes(want);
      const caret = view.host.querySelector(".ed-caret.main");
      // A caret hidden by its blink in the frame after a key reads as one that trails it.
      const moved = caret && getComputedStyle(caret).visibility !== "hidden" ? caretAt() - one.from : NaN;
      requestAnimationFrame((second) => timed.push({ drawn, moved, own: one.done - at, frame: second - first }));
    });
  };
  const done = () => {
    if (key && key.done === undefined) {
      key.done = performance.now();
    }
  };
  document.addEventListener("beforeinput", began, true);
  document.addEventListener("input", done);
  try {
    await ui.repeat("q", count, 33);
    await ui.waitFor(() => timed.length >= count, 4000, "each press timed");
  } finally {
    document.removeEventListener("beforeinput", began, true);
    document.removeEventListener("input", done);
  }
  return timed;
}

test("a letter typed in the middle of 250,000 lines, and inside a line of 20,000 characters, is drawn in the frame after its key", async () => {
  const rest = await framesOver(500);
  const interval = (rest.at(-1) - rest[0]) / (rest.length - 1);
  const files = [
    ["keys_long.py", Array.from({ length: 250000 }, (_, line) => `x_${line} = ${line} * 2  # line ${line}`).join("\n") + "\n", 125000, 0],
    ["keys_wide.txt", "w".repeat(20000) + "\n", 0, 5000],
  ];
  for (const [path, text, line, col] of files) {
    await invoke("file_write", { path, text });
    showView("edit");
    // a file of 250,000 lines is held as pages: opened at the line, the window stands about it
    await openAt(path, line, col);
    await ui.waitFor(() => editing().active === path && editing().editor?.s, 10000);
    const view = editing().editor;
    await ui.waitFor(() => line >= view.s.base && line < view.s.base + view.s.doc.count, 10000);
    view.goTo(line - view.s.base, col);
    view.focus();
    await framesOver(500);
    const timed = await keysTimed(view, line - view.s.base, 20);
    const own = timed.map((one) => one.own).sort((a, b) => a - b);
    const late = timed.filter((one) => one.frame > 1.5 * interval).length;
    const said = `${path}: own ms ${own.map((ms) => ms.toFixed(1)).join(" ")}; frames after ${timed.map((one) => one.frame.toFixed(1)).join(" ")}`;
    assert.equal(timed.filter((one) => one.drawn).length, timed.length, `each letter in its row as the frame after it began: ${said}`);
    assert.deepEqual(timed.filter((one) => Math.abs(one.moved - view.cw) > 0.5).map((one) => one.moved), [], `the caret a letter on, past the letter typed, as the frame after it began: ${said}`);
    // The page's own work for a key as a share of the display's frame: at 144 frames a second, 5 ms
    // in the middle and 12 at most, and the same shares of a frame on any display.
    assert.ok(own[Math.floor(own.length / 2)] <= 0.72 * interval, `the page's own work for a key, in the middle, within ${(0.72 * interval).toFixed(1)} ms: ${said}`);
    assert.ok(own.at(-1) <= 1.73 * interval, `and at most ${(1.73 * interval).toFixed(1)} ms: ${said}`);
    assert.ok(late <= 2, `frames after a key that came late: ${late} of ${timed.length}: ${said}`);
  }
});

test("a letter typed with the preview beside, Vim's keys on and the tree's check running is drawn in the frame after its key", async () => {
  const rest = await framesOver(500);
  const interval = (rest.at(-1) - rest[0]) / (rest.length - 1);
  const lines = [];
  for (let section = 0; section < 400; section += 1) {
    lines.push(`## Section ${section}`, "", `Some text of section ${section}, with **bold** and \`code\` in it.`, "", "- one", "- two", "");
  }
  await invoke("file_write", { path: "keys_beside.md", text: `${lines.join("\n")}\n` });
  showView("edit");
  await openFile("keys_beside.md");
  await ui.waitFor(() => editing().active === "keys_beside.md" && editing().editor?.s, 10000);
  const view = editing().editor;
  const vim = vimKeys();
  try {
    runCommand("preview", ["beside"]);
    await ui.waitFor(() => document.querySelector(".md-preview")?.checkVisibility(), 10000, "the preview beside the editor");
    setVimKeys(true);
    view.goTo(1400, 0);
    view.focus();
    await ui.key("Escape");
    await ui.key("i");
    // The tree's check reads every file of the tree as the letters are typed.
    invoke("problems_check").catch(() => {});
    await framesOver(300);
    const timed = await keysTimed(view, 1400, 20);
    const own = timed.map((one) => one.own).sort((a, b) => a - b);
    const late = timed.filter((one) => one.frame > 1.5 * interval).length;
    const said = `own ms ${own.map((ms) => ms.toFixed(1)).join(" ")}; frames after ${timed.map((one) => one.frame.toFixed(1)).join(" ")}`;
    assert.equal(timed.filter((one) => one.drawn).length, timed.length, `each letter in its row as the frame after it began: ${said}`);
    assert.deepEqual(timed.filter((one) => Math.abs(one.moved - view.cw) > 0.5).map((one) => one.moved), [], `the caret a letter on, past the letter typed, as the frame after it began: ${said}`);
    assert.ok(own[Math.floor(own.length / 2)] <= 0.72 * interval, `the page's own work for a key, in the middle, within ${(0.72 * interval).toFixed(1)} ms: ${said}`);
    assert.ok(late <= 2, `frames after a key that came late: ${late} of ${timed.length}: ${said}`);
    // The caret's moves answer at once too.
    const before = view.head();
    await ui.key("Escape");
    await ui.key("j");
    assert.equal(view.head().line, before.line + 1, "j moves down a line at once");
  } finally {
    runCommand("preview", ["off"]);
    setVimKeys(vim);
  }
});
