// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What a file is for, before it is opened: the pointer resting on a file in the explorer shows what
// its first comment says and the files of the tree that use it.

import { invoke } from "../../gui/src-js/bridge.js";
import { editing, treeRowShown } from "../../gui/src-js/edit.js";
import { shownCard } from "../../gui/src-js/purpose.js";
import { togglePane } from "../../gui/src-js/sides.js";
import { showView } from "../../gui/src-js/views.js";

const { test, assert, ui } = window.__harness;

const rowOf = (path) => treeRowShown(path);

// The row of `path` in sight once the tree has stopped moving it: the tree draws its rows again as files come and
// go, and a pointer sent to a row that then moves rests on another file.
async function settledRow(path) {
  let place = "";
  let since = performance.now();
  await ui.waitFor(() => {
    const row = rowOf(path);
    const box = row?.getBoundingClientRect();
    const now = row?.checkVisibility() ? `${box.top},${box.left}` : "";
    if (now !== place) {
      place = now;
      since = performance.now();
    }
    return place !== "" && performance.now() - since >= 400;
  }, 10000).catch((error) => {
    const row = rowOf(path);
    throw new Error(`${error.message}; row ${row?.dataset.key} at ${place || "nowhere"}, shown ${row?.checkVisibility()}, files shown ${document.getElementById("files")?.checkVisibility()}`);
  });
  return rowOf(path);
}

// The card shown for `path`.
const cardOf = (path) => (shownCard()?.querySelector(".file-card-path")?.textContent === path ? shownCard() : null);

test("the pointer resting on a file shows what its first comment says and the files that use it", async () => {
  await invoke("file_write", { path: "card_lib.js", text: "// SPDX-License-Identifier: MIT\n\n// Helpers the cards are drawn with.\nexport const one = 1;\n" });
  await invoke("file_write", { path: "card_main.js", text: 'import { one } from "./card_lib.js";\n' });
  showView("edit");
  togglePane(true, { take: false, pin: true });
  await ui.hover(await settledRow("card_lib.js"));
  await ui.waitFor(() => cardOf("card_lib.js")?.querySelector(".file-card-user"), 5000);
  const card = shownCard();
  assert.equal(card.querySelector(".file-card-path").textContent, "card_lib.js");
  assert.equal(card.querySelector(".file-card-said").textContent, "Helpers the cards are drawn with.");
  assert.equal(card.querySelector(".file-card-head").textContent, "Used by 1 file:");
  assert.deepEqual([...card.querySelectorAll(".file-card-user")].map((one) => one.textContent), ["card_main.js"]);
  assert.equal(rowOf("card_lib.js").hasAttribute("title"), false, "the row's own tip gives way to the card");
  assert.ok(card.getBoundingClientRect().left >= rowOf("card_lib.js").getBoundingClientRect().right, "beside the row");
  await ui.click(card.querySelector(".file-card-user"));
  await ui.waitFor(() => editing().active === "card_main.js", 5000);
  assert.equal(shownCard(), null, "a press opens the file, and the card goes");
  assert.equal(rowOf("card_lib.js").title, "card_lib.js");
});

test("a file with no comment at its head says so, and the card goes when the pointer leaves", async () => {
  // the file the card opened takes the window's edges: the explorer is shown again
  togglePane(true, { take: false, pin: true });
  await ui.hover(await settledRow("card_main.js"));
  await ui.waitFor(() => cardOf("card_main.js")?.querySelector(".file-card-head"), 5000);
  assert.equal(shownCard().querySelector(".file-card-said").textContent, "No comment at its head says what it is for.");
  assert.equal(shownCard().querySelector(".file-card-head").textContent, "No file of the tree uses it.");
  await ui.hover(document.getElementById("statusbar"));
  await ui.waitFor(() => !shownCard(), 2000);
});
