// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// What a file is for, before it is opened: the pointer resting on a file in the explorer shows what
// its first comment says and the files of the tree that use it.

import { invoke } from "../../src/bridge.js";
import { editing } from "../../src/edit.js";
import { shownCard } from "../../src/purpose.js";
import { togglePane } from "../../src/sides.js";
import { showView } from "../../src/views.js";

const { test, assert, ui } = window.__harness;

const rowOf = (path) => document.querySelector(`#files [data-key="${CSS.escape(path)}"]`);

test("the pointer resting on a file shows what its first comment says and the files that use it", async () => {
  await invoke("file_write", { path: "card_lib.js", text: "// SPDX-License-Identifier: MIT\n\n// Helpers the cards are drawn with.\nexport const one = 1;\n" });
  await invoke("file_write", { path: "card_main.js", text: 'import { one } from "./card_lib.js";\n' });
  showView("edit");
  togglePane(true, { take: false, pin: true });
  await ui.waitFor(() => rowOf("card_lib.js")?.checkVisibility(), 10000);
  await ui.hover(rowOf("card_lib.js"));
  await ui.waitFor(() => shownCard()?.querySelector(".file-card-user"), 5000);
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
  await ui.hover(rowOf("card_main.js"));
  await ui.waitFor(() => shownCard()?.querySelector(".file-card-head"), 5000);
  assert.equal(shownCard().querySelector(".file-card-said").textContent, "No comment at its head says what it is for.");
  assert.equal(shownCard().querySelector(".file-card-head").textContent, "No file of the tree uses it.");
  await ui.hover(document.getElementById("statusbar"));
  await ui.waitFor(() => !shownCard(), 2000);
});
