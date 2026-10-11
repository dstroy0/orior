// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The editor's widgets: what the pointer rests on stays open while the pointer is on its name, a step
// off it, on its way to it or in it, and goes once the pointer has left all of them a moment.

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
