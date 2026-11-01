// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The claim: the work takes the window's edges while the reader edits, each edge gives its part back
// on a rest in the gap there, the controls' corner drops the three controls alone with a ghost as
// the pointer nears, and the wordmark lives at the top left, whole while the bar or the strip is
// out and shrunk out toward the top left while both are away.

import { claimed, revealed, zoneAt } from "../../gui/src-js/claim.js";
import { togglePane } from "../../gui/src-js/sides.js";
import { editingHello, GAP, mark, markAt, markSaid, middle, paneAt, paneSaid, restedAt, sampled, SLIDE } from "../claims.js";

const { test, assert, ui } = window.__harness;

test("the work takes the bar, the toolbar, the strip and the status bar while the editor holds the keys, a gap kept on every side", async () => {
  await editingHello();
  const work = document.querySelector(".work").getBoundingClientRect();
  assert.ok(work.top >= GAP - 1 && work.top <= GAP + 1, `the work's top at ${work.top}`);
  assert.ok(work.left >= GAP - 1 && work.left <= GAP + 1, `the work's left at ${work.left}`);
  assert.ok(Math.round(window.innerHeight - work.bottom) === GAP, `the work's foot ${window.innerHeight - work.bottom} from the window's`);
  assert.ok(Math.round(window.innerWidth - work.right) === GAP, `the work's right ${window.innerWidth - work.right} from the window's`);
  const bar = document.querySelector("header.bar").getBoundingClientRect();
  assert.ok(bar.bottom <= 0, `the bar above the window, its foot at ${bar.bottom}`);
  assert.ok(markAt(false, GAP), `with the bar and the strip away the wordmark shrinks out toward the top left: ${markSaid()}`);
});

test("a rest in the gap at the left brings the strip in as a pane under the wordmark's corner, the wordmark whole at its head", async () => {
  await editingHello();
  await restedAt(4, middle().y, "left");
  assert.ok(markAt(true, GAP), `the wordmark whole in its corner: ${markSaid()}`);
  const strip = document.querySelector(".strip");
  const corner = GAP + document.querySelector("header.bar").offsetHeight;
  assert.ok(paneAt(strip, { left: GAP, top: corner + GAP, bottom: window.innerHeight - GAP }), `the strip's pane: ${paneSaid(strip)}`);
  assert.ok(strip.getBoundingClientRect().top >= mark().getBoundingClientRect().bottom, "below the wordmark");
  await ui.hover(middle());
  await ui.waitFor(() => revealed() === "", 3000, "the reveal let go");
});

test("a slide's numbers stay on the parts that move, none of them kept by what the parts hold", async () => {
  await editingHello();
  await restedAt(middle().x, 4, "top");
  const inside = document.querySelector("#mode-edit .ed");
  for (const name of ["--claim", "--reveal-top", "--reveal-down"]) {
    assert.equal(getComputedStyle(inside).getPropertyValue(name).trim(), "0", `${name} not kept inside the work`);
  }
  assert.equal(getComputedStyle(document.querySelector("header.bar")).getPropertyValue("--reveal-top").trim(), "1", "the bar holds its own");
  await ui.hover(middle());
  await ui.waitFor(() => revealed() === "", 3000, "the reveal let go");
});

test("as the bar comes back under the controls' tab and goes again, the controls stand still under it and nothing in the bar moves along it", async () => {
  await editingHello();
  await restedAt(window.innerWidth - 40, 4, "controls");
  const controls = document.getElementById("window-controls");
  const tools = document.getElementById("bar-tools");
  const menus = document.getElementById("menubar");
  const coming = sampled(() => [controls.getBoundingClientRect().top, tools.getBoundingClientRect().left, menus.getBoundingClientRect().left]);
  await restedAt(middle().x, 4, "top");
  const came = coming();
  const spread = (column) => Math.max(...came.map((row) => row[column])) - Math.min(...came.map((row) => row[column]));
  assert.ok(spread(0) < 0.5, `the controls stood still as the bar came, moving ${spread(0)}`);
  assert.ok(spread(1) < 0.5 && spread(2) < 0.5, `the bar's tools moved ${spread(1)} and its menus ${spread(2)} along it`);
  const going = sampled(() => [tools.getBoundingClientRect().left, menus.getBoundingClientRect().left]);
  await ui.hover(middle());
  await ui.waitFor(() => revealed() === "", 3000, "the reveal let go");
  await ui.rest(SLIDE);
  const went = going();
  const along = (column) => Math.max(...went.map((row) => row[column])) - Math.min(...went.map((row) => row[column]));
  assert.ok(along(0) < 0.5 && along(1) < 0.5, `as the bar went its tools moved ${along(0)} and its menus ${along(1)} along it`);
});

test("a rest in the gap at the foot brings the status bar back, and it goes once the pointer leaves", async () => {
  await editingHello();
  await restedAt(middle().x, window.innerHeight - 4, "bottom");
  const status = document.getElementById("statusbar");
  assert.ok(paneAt(status, { left: GAP, right: window.innerWidth - GAP, bottom: window.innerHeight - GAP }), `the status bar's pane: ${paneSaid(status)}`);
  await ui.hover(middle());
  await ui.waitFor(() => revealed() === "", 3000, "the reveal let go");
});

test("a rest in the gap at the head brings the bar in as a pane right of the wordmark's corner, the wordmark whole at its head", async () => {
  await editingHello();
  await restedAt(middle().x, 4, "top");
  const bar = document.querySelector("header.bar");
  assert.ok(paneAt(bar, { left: 44 + 2 * GAP, top: GAP, right: window.innerWidth - GAP }), `the bar's pane: ${paneSaid(bar)}`);
  assert.ok(markAt(true, GAP), `the wordmark whole in its corner: ${markSaid()}`);
  await ui.hover(middle());
  await ui.waitFor(() => revealed() === "", 3000, "the reveal let go");
});

test("a pointer running through the gap at the head without slowing is a miss, and the same pointer come to rest there brings the bar back", async () => {
  await editingHello();
  const seen = new Set();
  for (let x = 120; x < window.innerWidth - 220; x += 20) {
    await ui.hover({ x, y: 4 });
    await ui.rest(15);
    seen.add(revealed());
  }
  for (let x = window.innerWidth - 320; x < window.innerWidth - 10; x += 20) {
    await ui.hover({ x, y: GAP + 8 });
    await ui.rest(15);
    seen.add(revealed() || (document.body.hasAttribute("data-near") ? "near" : ""));
  }
  assert.deepEqual([...seen], [""], "nothing revealed and no ghost while the pointer ran");
  await restedAt(middle().x, 4, "top");
  await ui.hover(middle());
  await ui.waitFor(() => revealed() === "", 3000, "the reveal let go");
});

test("a pointer leaving the window across the gap at the head is a leave, not a rest, and the bar stays away", async () => {
  await editingHello();
  await ui.hover({ x: middle().x, y: 40 });
  await ui.hover({ x: middle().x, y: 4 });
  await ui.hover({ x: middle().x, y: -8 });
  await ui.rest(800);
  assert.equal(revealed(), "", "nothing came out after the pointer left");
  await ui.hover(middle());
});

test("a rest in the controls' corner drops the three controls alone, a ghost of them as the pointer nears", async () => {
  await editingHello();
  const controls = document.getElementById("window-controls");
  await ui.hover({ x: window.innerWidth - 40, y: GAP * 2 + 4 });
  await ui.waitFor(() => document.body.hasAttribute("data-near"), 3000, "the ghost as the pointer nears");
  await restedAt(window.innerWidth - 40, 4, "controls");
  const box = controls.getBoundingClientRect();
  assert.ok(box.top >= 0 && box.bottom > 0 && box.right <= window.innerWidth, `the controls' tab in view at ${box.top},${box.right}`);
  assert.equal(zoneAt(window.innerWidth - 40, 4), "controls");
  assert.ok(document.querySelector("header.bar").getBoundingClientRect().bottom <= 0, "the bar itself stays away");
  await ui.hover(middle());
  await ui.waitFor(() => revealed() === "", 3000, "the reveal let go");
});

test("a rest within three gaps of the controls' corner, on the work's border, brings the controls, and the ghost stays while the pointer wanders there", async () => {
  await editingHello();
  const x = window.innerWidth - 40;
  await ui.hover({ x, y: GAP * 2 + 4 });
  await ui.waitFor(() => document.body.hasAttribute("data-near"), 3000, "the ghost as the pointer nears");
  // the pointer wanders a few pixels at a time, as a hand holding it still does, and the ghost stays through it
  let gone = 0;
  for (let step = 0; step < 8; step += 1) {
    await ui.hover({ x: x + (step % 2 ? 3 : -3), y: GAP * 2 + 4 + (step % 3) });
    await ui.rest(20);
    gone += document.body.hasAttribute("data-near") || revealed() === "controls" ? 0 : 1;
  }
  assert.equal(gone, 0, "the ghost never went while the pointer wandered in the corner");
  await ui.waitFor(() => revealed() === "controls", 3000, "a rest on the border at the corner brings the controls");
  await ui.hover(middle());
  await ui.waitFor(() => revealed() === "", 3000, "the reveal let go");
});

test("a rest within three gaps of the head and the foot, on the work's border, brings their parts as a rest in the gap does", async () => {
  await editingHello();
  await restedAt(middle().x, GAP * 2 + 4, "top");
  await ui.hover(middle());
  await ui.waitFor(() => revealed() === "", 3000, "the bar let go");
  await restedAt(middle().x, window.innerHeight - (GAP * 2 + 4), "bottom");
  await ui.hover(middle());
  await ui.waitFor(() => revealed() === "", 3000, "the status bar let go");
});

test("a rest at another edge while one edge's part is out sends that part away first, and brings the other once it has gone", async () => {
  await editingHello();
  await restedAt(middle().x, 4, "top");
  const bar = document.querySelector("header.bar");
  await ui.hover({ x: 4, y: Math.round(window.innerHeight * 0.6) });
  await ui.waitFor(() => revealed() === "left", 3000, "the strip revealed");
  assert.ok(bar.getBoundingClientRect().bottom <= 0.5, `the bar gone before the strip comes: its foot at ${bar.getBoundingClientRect().bottom}`);
  await ui.rest(SLIDE);
  await ui.hover(middle());
  await ui.waitFor(() => revealed() === "", 3000, "the reveal let go");
});

// The desk's row of tabs, the editor that holds the keys and its minimap, each as it stands.
const tabsRow = () => document.querySelector('#mode-edit .desk[data-tabs="row"] > .tabs');

const deskBox = () => document.querySelector("#mode-edit .desk").getBoundingClientRect();

const editorBox = () => document.getElementById("editor").getBoundingClientRect();

const map = () => document.querySelector("#editor > .ed-mini");

test("while the claim holds the tabs row and the minimap are away, the editor's text taking their room", async () => {
  await editingHello();
  await ui.rest(SLIDE);
  const row = tabsRow().getBoundingClientRect();
  assert.ok(row.bottom <= deskBox().top + 0.5, `the tabs row above the desk, its foot at ${row.bottom} and the desk's top at ${deskBox().top}`);
  assert.ok(Math.abs(editorBox().top - deskBox().top) < 1, `the editor at the desk's top: ${editorBox().top} against ${deskBox().top}`);
  assert.ok(map().getBoundingClientRect().left >= editorBox().right - 0.5, `the minimap right of the editor: ${map().getBoundingClientRect().left} against ${editorBox().right}`);
  const text = document.querySelector("#editor > .ed-scroll").getBoundingClientRect();
  assert.ok(Math.abs(text.right - editorBox().right) < 1, `the text out to the editor's right: ${text.right} against ${editorBox().right}`);
  const corners = getComputedStyle(document.getElementById("editor"), "::after").backgroundPosition;
  assert.match(corners, /calc\(100% \+ 8px\) 0%/,`the minimap's corners gone past the editor's right edge with it: ${corners}`);
});

test("a rest in the gap at the head brings the tabs row back with the bar, the pointer on it keeps it, and it goes as they go", async () => {
  await editingHello();
  await restedAt(middle().x, 4, "top");
  const row = tabsRow().getBoundingClientRect();
  assert.ok(Math.abs(row.top - deskBox().top) < 1 && row.height > 20, `the tabs row back at the desk's top: ${row.top}, ${row.height} tall, the desk's top at ${deskBox().top}`);
  assert.equal(getComputedStyle(tabsRow()).opacity, "1", "the tabs row whole");
  const tab = tabsRow().querySelector(".tab").getBoundingClientRect();
  await ui.hover({ x: Math.round(tab.left + tab.width / 2), y: Math.round(tab.top + tab.height / 2) });
  await ui.rest(1000);
  assert.equal(revealed(), "top", "the pointer on the tabs holds them");
  await ui.hover(middle());
  await ui.waitFor(() => revealed() === "", 3000, "the reveal let go");
  await ui.rest(SLIDE);
  assert.ok(tabsRow().getBoundingClientRect().bottom <= deskBox().top + 0.5, "the tabs row away again");
});

test("a rest in the gap at the right brings the minimap back at the editor's right edge, the pointer on it keeps it, and it goes after", async () => {
  await editingHello();
  assert.equal(zoneAt(window.innerWidth - 4, middle().y), "right");
  assert.equal(zoneAt(window.innerWidth - 4, 4), "controls", "the controls' corner still the controls'");
  await restedAt(window.innerWidth - 4, middle().y, "right");
  const box = map().getBoundingClientRect();
  assert.ok(Math.abs(box.right - editorBox().right) < 1 && box.width >= 83, `the minimap back: ${box.left} to ${box.right}, the editor's right at ${editorBox().right}`);
  await ui.hover({ x: Math.round(box.left + box.width / 2), y: middle().y });
  await ui.rest(1000);
  assert.equal(revealed(), "right", "the pointer on the minimap holds it");
  await ui.hover(middle());
  await ui.waitFor(() => revealed() === "", 3000, "the reveal let go");
  await ui.rest(SLIDE);
  assert.ok(map().getBoundingClientRect().left >= editorBox().right - 0.5, "the minimap away again");
});

test("the claim goes as the keys leave the editor for the explorer", async () => {
  await editingHello();
  togglePane(true, { take: false });
  await ui.waitFor(() => document.getElementById("file-filter").getBoundingClientRect().left >= 0, 3000, "the explorer back beside the editor");
  await ui.click(document.getElementById("file-filter"));
  await ui.waitFor(() => !claimed(), 3000, "the claim let go");
  const bar = document.querySelector("header.bar").getBoundingClientRect();
  await ui.waitFor(() => document.querySelector("header.bar").getBoundingClientRect().top >= 0, 3000, `the bar back, its top at ${bar.top}`);
  await ui.rest(SLIDE);
  assert.ok(markAt(true), `the wordmark whole: ${markSaid()}`);
});
