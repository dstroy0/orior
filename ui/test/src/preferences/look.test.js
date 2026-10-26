// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Preferences' palette: each color whose contrast with the color under it is too low for text marked.

import { runCommand } from "../../../gui/src-js/menubar.js";
import { scheme } from "../../../gui/src-js/scheme.js";
import { contrastOf, paletteOf } from "../../../gui/src-js/themes.js";
import { inSheet, leave, useTheme } from "../../themes.js";

const { test, assert, ui } = window.__harness;

test("Preferences marks each color whose contrast with the color under it is too low for text", async () => {
  assert.ok(Math.abs(contrastOf("#000000", "#ffffff") - 21) < 0.01);
  assert.ok(Math.abs(contrastOf("#777777", "#777777") - 1) < 0.01);
  assert.ok(contrastOf("rgba(0, 0, 0, 0)", "#ffffff") < 1.01, "a color with alpha laid over the one under it");
  const under = paletteOf(scheme()).get("--ed-bg");
  useTheme("Faint for Themes", { colors: { "--t-comment": under } });
  runCommand("preferences");
  const row = () => inSheet('.prefs-color[data-name="--t-comment"]')[0];
  await ui.waitFor(() => row(), 5000);
  assert.ok(row().classList.contains("low"));
  assert.match(row().querySelector(".prefs-contrast").textContent, /^1\.0:1 on --ed-bg$/);
  assert.match(inSheet(".prefs-contrasts")[0].textContent, /--t-comment on --ed-bg, 1\.0:1/);
  await ui.click(inSheet(".prefs-palette .filter")[0]);
  await ui.type("--t-comment");
  await ui.waitFor(() => inSheet(".prefs-color").every((one) => one.dataset.name.startsWith("--t-comment")), 2000);
  await ui.click(row().querySelector(".prefs-value"));
  const text = row().querySelector(".prefs-value");
  text.select();
  await ui.type(scheme() === "dark" ? "#ffffff" : "#000000");
  await ui.key("Enter");
  await ui.waitFor(() => !row().classList.contains("low"), 2000);
  document.querySelector("dialog.sheet")?.close();
  leave("Faint for Themes");
});
