// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Printing: the printers to print to.

import { calls, given, press, putBackAsFound, sheetShown } from "../menus.js";

const { test, assert } = window.__harness;

test("File, Print shows the printers to print to", async () => {
  calls.answer("printers_list", () => ({ printers: [{ name: "Test Printer", default: true }], silent: true }));
  await given("printed|");
  await press("print");
  const shown = await sheetShown("select");
  assert.ok([...shown.options].some((one) => one.textContent === "Save as PDF"));
});

test("the window is put back as the file found it", putBackAsFound);
