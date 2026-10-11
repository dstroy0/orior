// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The rows in sight: a folder of three hundred files in the explorer's tree, its rows out of sight
// neither laid out nor painted, and those its scroll heads into drawn ahead.

import { invoke } from "../../src/bridge.js";
import { togglePane } from "../../src/sides.js";
import { aheadOf } from "../../src/sight.js";
import { showView } from "../../src/views.js";

const { test, assert, ui } = window.__harness;

const files = () => document.getElementById("files");
const rowOf = (path) => files().querySelector(`.node[data-key="${path}"]`);
const drawn = (row) => row.querySelector(".name").checkVisibility({ contentVisibilityAuto: true });
const name = (at) => `sight/f${String(at).padStart(3, "0")}.txt`;

test("rows of the tree out of sight are neither laid out nor painted, and those in sight are", async () => {
  for (let at = 0; at < 300; at += 1) {
    await invoke("file_create", { path: name(at) }).catch(() => {});
  }
  showView("edit");
  togglePane(true, { take: false, pin: true });
  await ui.waitFor(() => rowOf("sight")?.checkVisibility(), 10000);
  if (rowOf("sight").getAttribute("aria-expanded") !== "true") {
    await ui.click(rowOf("sight"));
  }
  await ui.waitFor(() => rowOf(name(299)), 10000);
  files().scrollTop = 0;
  await ui.rest(100);
  assert.ok(drawn(rowOf("sight")), "the folder's own row, in sight, is drawn");
  assert.ok(!drawn(rowOf(name(299))), "a row far below is neither laid out nor painted");
  assert.equal(rowOf(name(299)).getBoundingClientRect().height, rowOf("sight").getBoundingClientRect().height, "it keeps a row's height");
});

test("the rows the tree scrolls toward are drawn ahead of it", async () => {
  const list = files();
  for (let turn = 0; turn < 4; turn += 1) {
    await ui.wheel(list, 400);
    await ui.rest(20);
  }
  await ui.waitFor(() => aheadOf(list).length >= 8, 2000);
  const bottom = list.getBoundingClientRect().bottom;
  const marked = aheadOf(list);
  assert.ok(marked.every((row) => row.getBoundingClientRect().top >= bottom - 22), "each lies past the view's lower edge");
  assert.ok(marked.every(drawn), "each is drawn though out of sight");
});

test("the tree drawn as it was", async () => {
  files().scrollTop = 0;
  await ui.click(rowOf("sight"));
  await ui.waitFor(() => !rowOf(name(0)), 5000);
  togglePane(false);
});
