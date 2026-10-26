// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Outline pane.

import { invoke } from "../../../gui/src-js/bridge.js";
import { editing, openFile } from "../../../gui/src-js/edit.js";
import { showGroup, showPane } from "../../../gui/src-js/explorer.js";
import { togglePane } from "../../../gui/src-js/sides.js";
import { showView } from "../../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

test("the Outline of twenty thousand symbols holds the rows in sight, marks the one the cursor is in and goes to the one pressed", async () => {
  const lines = [];
  for (let line = 0; line < 20000; line += 1) {
    lines.push(`def step_${line}(value):`, "    return value");
  }
  await invoke("file_write", { path: "outlined.py", text: `${lines.join("\n")}\n` });
  showView("edit");
  await openFile("outlined.py");
  await ui.waitFor(() => editing().active === "outlined.py" && editing().editor?.s, 10000);
  const view = editing().editor;
  togglePane(true);
  showPane("outline");
  try {
    const body = document.getElementById("outline");
    const rows = () => [...body.querySelectorAll(".sym")];
    const name = (row) => row?.querySelector(".name")?.textContent;
    await ui.waitFor(() => rows().length > 0, 5000);
    assert.ok(rows().length < 200, `rows on the page: ${rows().length} of 20000`);
    assert.equal(name(rows()[0]), "step_0");
    assert.equal(body.scrollHeight >= 20000 * 22 - 22, true, `the space stands for every row: ${body.scrollHeight}`);
    view.goTo(2 * 15000 + 1, 0);
    await ui.waitFor(() => name(body.querySelector('.sym[aria-current="true"]')) === "step_15000", 3000);
    const lit = body.querySelector('.sym[aria-current="true"]');
    const box = lit.getBoundingClientRect();
    const sight = body.getBoundingClientRect();
    assert.ok(box.top >= sight.top - 1 && box.bottom <= sight.bottom + 1, "the marked row is in sight");
    assert.ok(rows().length < 200, `and the rows on the page stay few: ${rows().length}`);
    const next = rows().find((row) => name(row) === "step_15003");
    await ui.click(next);
    await ui.waitFor(() => view.primary().head.line === 2 * 15003, 3000);
    assert.equal(view.primary().head.line, 30006, "the press goes to the symbol's line");
  } finally {
    showGroup("explorer");
  }
});
