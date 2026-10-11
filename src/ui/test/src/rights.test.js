// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Writing with care: a write the system refuses asks the reader whether to write it with the rights it
// needs, and one the reader leaves fails, said to be refused; any other failure is told as it is.

import { invoke } from "../../src/bridge.js";
import { refused, writeWithCare } from "../../src/rights.js";

const { test, assert, ui } = window.__harness;

test("a refusal is told from any other failure", () => {
  assert.ok(refused("a.txt: Access is denied. (os error 5)"));
  assert.ok(refused("Error: a.txt: Permission denied (os error 13)"));
  assert.ok(!refused("a.txt: The system cannot find the path specified. (os error 3)"));
});

test("a write the system refuses asks the reader first, and one they leave fails as refused", async () => {
  await invoke("folder_create", { path: "locked_for_save.txt" }).catch(() => {});
  const writing = writeWithCare("locked_for_save.txt", "text", { who: "The test" }).then(
    () => null,
    (error) => error,
  );
  await ui.waitFor(() => !document.getElementById("rights-ask").hidden, 5000);
  assert.match(document.querySelector(".rights-said").textContent, /^The test asks to write locked_for_save\.txt, and the system refuses it the rights\. Write it with the rights it needs\?$/);
  assert.ok(document.activeElement === document.body || !document.getElementById("rights-ask").contains(document.activeElement), "the question takes no keys");
  await ui.click([...document.querySelectorAll(".rights-button")].find((one) => one.textContent === "Leave It"));
  const failed = await writing;
  assert.equal(failed?.refused, true, String(failed));
  assert.equal(document.getElementById("rights-ask").hidden, true);
});

test("a write that fails for any other reason is told as it is, and asks nothing", async () => {
  const failed = await writeWithCare("no/such/folder/\u0000name.txt", "text").then(
    () => null,
    (error) => error,
  );
  assert.ok(failed && !failed.refused, String(failed));
  assert.equal(document.getElementById("rights-ask").hidden, true);
});
