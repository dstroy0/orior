// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The minimap beside the text: a press on it brings the line pressed to the middle of the view, a
// press on its strip goes to that part of the file, and the wheel over it scrolls the text.

import { invoke } from "../../../src/bridge.js";
import { editing, openFile } from "../../../src/edit.js";
import { showView } from "../../../src/views.js";

const { test, assert, ui } = window.__harness;

const settled = () => new Promise((rest) => requestAnimationFrame(() => requestAnimationFrame(rest)));

// The line at the middle of the view.
function middleLine(view) {
  const row = Math.floor((view.scrollY() + view.viewSize().height / 2) / view.lineHeight);
  return view.rows().lineOf(row);
}

async function opened() {
  const lines = Array.from({ length: 3000 }, (_, line) => `mapped_${line} = ${line} * 2  # line ${line}`);
  await invoke("file_write", { path: "mapped.py", text: `${lines.join("\n")}\n` });
  showView("edit");
  await openFile("mapped.py");
  await ui.waitFor(() => editing().active === "mapped.py" && editing().editor?.s, 5000);
  const view = editing().editor;
  view.place(0);
  await settled();
  return view;
}

test("a press on the minimap brings the line pressed to the middle of the view", async () => {
  const view = await opened();
  const map = view.minimap;
  const box = map.canvas.getBoundingClientRect();
  const found = map.geometry();
  const y = Math.min(box.height - 20, found.sliderTop + found.sliderHeight + 150);
  const pressed = view.rows().lineOf(Math.floor(map.rowAt(y, found)));
  await ui.click({ x: box.left + box.width / 3, y: box.top + y });
  await settled();
  const middle = middleLine(view);
  assert.ok(Math.abs(middle - pressed) <= 1, `pressed line ${pressed}, line ${middle} at the middle`);
});

test("a press on the minimap's strip goes to that part of the file", async () => {
  const view = await opened();
  const box = view.minimap.canvas.getBoundingClientRect();
  await ui.click({ x: box.right - 3, y: box.top + box.height * 0.75 });
  await settled();
  const middle = middleLine(view);
  assert.ok(Math.abs(middle - 2250) <= 60, `three quarters down the strip, line ${middle} at the middle`);
});

test("the wheel over the minimap scrolls the text", async () => {
  const view = await opened();
  const box = view.minimap.canvas.getBoundingClientRect();
  const before = view.scroller.scrollTop;
  await ui.wheel({ x: box.left + box.width / 3, y: box.top + box.height / 2 }, 600);
  await ui.waitFor(() => view.scroller.scrollTop - before > 200, 3000, "the text to scroll");
});
