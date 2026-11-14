// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A file's icon in the tree, and its mark's ink centered on its name.

import { iconOf } from "../../../gui/src-js/explorer/icons.js";
import { showGroup } from "../../../gui/src-js/explorer.js";
import { invoke } from "../../../gui/src-js/bridge.js";
import { treeRowShown } from "../../../gui/src-js/edit.js";
import { togglePane } from "../../../gui/src-js/sides.js";
import { showView } from "../../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

test("every file has a mark and every folder the folder's shape, a character's width before each name", async () => {
  assert.equal(iconOf("main.go").textContent, "go");
  assert.equal(iconOf("Dockerfile.dev").textContent, "dk", "a name looked up without a type that has no mark");
  assert.equal(iconOf("license.py").textContent, "py", "and a type with a mark is its type");
  const other = iconOf("model.onnx");
  assert.deepEqual([other.textContent, other.classList.contains("unmarked")], ["on", true], "a type with no mark is drawn with its own first characters");
  assert.equal(iconOf("NOTICE").textContent, "no", "and a name with no type with its own");
  for (const name of ["main.c", "view.tsx", "song.wav", "archive.zip", "Makefile", "LICENSE", ".env"]) {
    assert.ok(iconOf(name).textContent.trim(), `${name} has a mark`);
  }
  await invoke("folder_create", { path: "types" }).catch(() => {});
  await invoke("file_write", { path: "types/model.onnx", text: "x" });
  showView("edit");
  togglePane(true, { take: false, pin: true });
  showGroup("explorer");
  // The tree draws the rows in sight alone, so each row looked for is scrolled into sight first.
  const rows = () => [...document.querySelectorAll("#files .node")];
  await ui.waitFor(() => treeRowShown("types"), 10000);
  const types = treeRowShown("types");
  if (types.getAttribute("aria-expanded") !== "true") {
    await ui.click(types);
  }
  await ui.waitFor(() => treeRowShown("types/model.onnx"), 10000);
  for (const row of rows()) {
    const icon = row.querySelector(":scope > .icon");
    assert.ok(icon, `${row.dataset.key} has an icon`);
    if (row.classList.contains("dir") && row.dataset.archive !== "true") {
      assert.ok(icon.querySelector(".glyph-folder"), `${row.dataset.key} has the folder's shape`);
    } else {
      assert.ok(icon.textContent.trim(), `${row.dataset.key} has a mark`);
    }
    const name = row.querySelector(".name").getBoundingClientRect();
    const gap = name.left - icon.getBoundingClientRect().right;
    assert.ok(gap >= 0.5 * parseFloat(getComputedStyle(row).fontSize), `${row.dataset.key}: ${gap.toFixed(1)} px before the name`);
  }
});

test("each file's mark in the tree has its ink centered on the middle of its name's capitals", async () => {
  showView("edit");
  togglePane(true, { take: false, pin: true });
  await document.fonts.ready;
  await ui.waitFor(() => document.querySelectorAll("#files .node:not(.dir) .icon").length >= 3, 10000);
  // A box of no height set in a text is on the text's line, wherever the text is moved to.
  const lineOf = (node) => {
    const probe = Object.assign(document.createElement("span"), { style: "display:inline-block;width:0;height:0;vertical-align:baseline" });
    node.append(probe);
    const at = probe.getBoundingClientRect().bottom;
    probe.remove();
    return at;
  };
  // The ink of `text` set as `node` sets it: how far above its line it reaches, and below, found
  // in its pixels drawn at 100px, the font scaled to set a "0" as wide as the node sets one.
  const canvas = Object.assign(document.createElement("canvas"), { width: 400, height: 240 });
  const pen = canvas.getContext("2d", { willReadFrequently: true });
  const inkOf = (node, text) => {
    const style = getComputedStyle(node);
    const zeros = Object.assign(document.createElement("span"), { textContent: "0000000000", style: "position:absolute;visibility:hidden;white-space:pre" });
    node.append(zeros);
    const wide = zeros.getBoundingClientRect().width / 10;
    zeros.remove();
    pen.font = `${style.fontWeight} 100px ${style.fontFamily}`;
    pen.clearRect(0, 0, canvas.width, canvas.height);
    pen.fillText(text, 10, 160);
    const { data } = pen.getImageData(0, 0, canvas.width, canvas.height);
    const inked = [];
    for (let row = 0; row < canvas.height; row += 1) {
      for (let at = row * canvas.width * 4 + 3; at < (row + 1) * canvas.width * 4; at += 4) {
        if (data[at] > 96) {
          inked.push(row);
          break;
        }
      }
    }
    const scale = wide / pen.measureText("0").width;
    return { above: (160 - inked[0]) * scale, below: (inked.at(-1) + 1 - 160) * scale };
  };
  const off = [...document.querySelectorAll("#files .node:not(.dir)")]
    .filter((row) => row.checkVisibility() && row.querySelector(".icon")?.textContent && row.querySelector(".name"))
    .map((row) => {
      const icon = row.querySelector(".icon");
      const name = row.querySelector(".name");
      const mark = inkOf(icon, icon.textContent);
      const middle = lineOf(icon) - (mark.above - mark.below) / 2;
      const capitals = lineOf(name) - inkOf(name, "H").above / 2;
      return { mark: icon.textContent, name: row.dataset.key, by: Math.round((middle - capitals) * 100) / 100 };
    })
    .filter((one) => Math.abs(one.by) > 0.75);
  assert.deepEqual(off, [], "each mark's ink centered on its name's capitals");
});
