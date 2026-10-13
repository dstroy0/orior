// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's widgets: what the pointer rests on stays open while the pointer is on its name, a step
// off it, on its way to it or in it, and goes once the pointer has left all of them a moment; and the
// completion list, which stands at once on the file's own words while a slow server is awaited.

import { invoke } from "../../../src/bridge.js";
import { editing, openFile } from "../../../src/edit.js";
import { loadPlugins } from "../../../src/plugins.js";
import { togglePane } from "../../../src/sides.js";
import { showView } from "../../../src/views.js";

const { test, assert, ui } = window.__harness;

const wait = (ms) => new Promise((done) => setTimeout(done, ms));

test("what the pointer rests on stays while the pointer is on its name, a step off it, on its way to it or in it, and goes once it has left them a moment", async () => {
  const home = await invoke("home_folder");
  const plugin = { id: "hovering", name: "Hovering", version: "1.0.0", kind: "language", extensions: ["hov"], grammar: null, hovers: { widget: "a thing the test made" } };
  await invoke("file_write_any", { path: `${home}/plugins/hovering/plugin.json`, text: JSON.stringify(plugin) });
  await loadPlugins();
  await invoke("file_write", { path: "rest.hov", text: `the widget here${"\n".repeat(14)}and other words far below\n` });
  showView("edit");
  await openFile("rest.hov");
  await ui.waitFor(() => editing().active === "rest.hov" && editing().editor?.s?.language?.hover, 5000);
  const view = editing().editor;
  const hover = view.hover;
  // The explorer shut, and the editor still where it stands once it has slid there.
  togglePane(false);
  let last = null;
  await ui.waitFor(() => {
    const now = JSON.stringify(view.host.getBoundingClientRect());
    const still = now === last;
    last = now;
    return still;
  }, 3000);
  await wait(300);
  const at = (line, col) => {
    const box = view.rectOf({ line, col });
    return { x: box.left + view.cw / 2, y: (box.top + box.bottom) / 2 };
  };
  // "widget" spans cols 4 to 10 of the first line.
  await ui.hover(at(0, 6));
  await ui.waitFor(() => hover.shown, 3000);
  assert.match(hover.el.textContent, /a thing the test made/);
  await ui.hover(at(0, 10));
  await wait(900);
  assert.ok(hover.shown, "a step off the name, onto the space after it, keeps it");
  const name = view.rectOf({ line: 0, col: 4 });
  const box = hover.el.getBoundingClientRect();
  const between = box.top >= name.bottom ? (name.bottom + box.top) / 2 : (box.bottom + name.top) / 2;
  const gap = { x: name.left + view.cw, y: between };
  await ui.hover(gap);
  await wait(500);
  const atGap = document.elementFromPoint(gap.x, gap.y);
  assert.ok(hover.shown, `on its way between the name and the box, it stays: at ${JSON.stringify(gap)}, the name ${JSON.stringify(name)}, the box ${JSON.stringify(box)}, kept by ${JSON.stringify(hover.keptBy)}, ${atGap?.className || atGap?.tagName} under the pointer`);
  const inside = { x: box.left + box.width / 2, y: box.top + box.height / 2 };
  await ui.hover(inside);
  await wait(900);
  const under = document.elementFromPoint(inside.x, inside.y);
  assert.ok(hover.shown, `in the box, it stays: the box ${JSON.stringify(box)}, kept by ${JSON.stringify(hover.keptBy)}, ${under?.className || under?.tagName} under the pointer`);
  await ui.hover(at(0, 6));
  await wait(500);
  assert.ok(hover.shown, "back on the name, it stays");
  // Left for good, for words that show nothing, it goes a moment after.
  await ui.hover(at(15, 6));
  const left = performance.now();
  await ui.waitFor(() => !hover.shown, 3000);
  assert.ok(performance.now() - left >= 250, "not at once, but a moment after");
});

test("the completion list stands at once on the file's words and the language's, the server's join as they come, and no key typed fast is lost", async () => {
  await invoke("file_write", { path: "words.hov", text: "alphabet alphaWord beta\n\n" });
  showView("edit");
  await openFile("words.hov");
  await ui.waitFor(() => editing().active === "words.hov" && editing().editor?.s?.language, 5000);
  const view = editing().editor;
  const own = view.s.language;
  let answer;
  view.s.language = Object.assign(Object.create(own), {
    complete: () => new Promise((done) => (answer = done)),
    completeNow: () => [{ label: "alphaOwn", kind: "keyword", insert: "alphaOwn" }],
  });
  try {
    const labels = () => [...view.suggest.list.querySelectorAll(".ed-item-label")].map((label) => label.textContent);
    view.goTo(1, 0);
    view.focus();
    view.type("a");
    assert.ok(view.suggest.pending && view.suggest.el.hidden, "the letter is drawn before the list's work begins");
    await ui.type("lpha");
    assert.equal(view.doc.line(1), "alpha", "every key typed fast, in order");
    await ui.waitFor(() => view.suggest.open && view.suggest.typed === "alpha", 2000);
    assert.deepEqual(labels().sort(), ["alphaOwn", "alphaWord", "alphabet"], "the language's word and the file's, before the server answers");
    answer([{ label: "alphaServer", kind: "function", insert: "alphaServer" }]);
    await ui.waitFor(() => labels().includes("alphaServer"), 2000);
    assert.ok(labels().includes("alphaWord"), "the file's words stay beside the server's");
    await ui.type("S");
    await ui.key("Enter");
    assert.equal(view.doc.line(1), "alphaServer", "Enter at once takes the item for the word as typed");
  } finally {
    view.s.language = own;
  }
});
