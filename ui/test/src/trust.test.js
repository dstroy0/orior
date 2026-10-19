// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Trust: a tree with no code of its own never asked about; a crate with a build script asked about
// once, its answer kept, said on the status bar while its code is held back, asked again from there
// and from File, and trusted with every tree in the folder above it. The crate is a folder of the
// run's tree opened as the tree, and the run's tree is trusted once the tests end, its code then
// running as it did.

import { invoke } from "../../gui/src-js/bridge.js";
import { runCommand } from "../../gui/src-js/menubar.js";
import { readTrust } from "../../gui/src-js/trust.js";

const { test, assert, ui } = window.__harness;

const ask = () => document.getElementById("trust-ask");
const status = () => document.getElementById("status-trust");
const buttonNamed = (label) => [...ask().querySelectorAll("button")].find((button) => button.textContent === label);

test("a tree with no code of its own is not asked about, and nothing of it is held back", async () => {
  const told = await readTrust();
  assert.deepEqual(told.code, []);
  assert.equal(told.answer, null);
  assert.ok(ask().hidden, "no question");
  assert.ok(status().hidden, "nothing said on the status bar");
  runCommand("trust-tree");
  assert.ok(ask().hidden, "File, Trust This Tree says there is nothing to trust");
});

test("a crate with a build script is asked about once, Hold It Back is kept and said, and the folder above is trusted from the status bar", async () => {
  const root = await invoke("root_get");
  await invoke("folder_create", { path: "trusting/crate" }).catch(() => {});
  await invoke("file_write", { path: "trusting/crate/Cargo.toml", text: '[package]\nname = "crate"\n' });
  await invoke("file_write", { path: "trusting/crate/build.rs", text: "fn main() {}\n" });
  const crate = await invoke("root_set", { path: `${root}/trusting/crate` });
  try {
    const typing = document.activeElement;
    const told = await readTrust();
    assert.deepEqual(told.code, ["build.rs"]);
    assert.equal(told.answer, null);
    assert.ok(!ask().hidden, "the question shows");
    assert.match(ask().textContent, /crate holds code its tools run as they read it: build\.rs/);
    assert.equal(document.activeElement, typing, "the question takes no keys");
    await ui.click(buttonNamed("Hold It Back"));
    await ui.waitFor(() => ask().hidden && !status().hidden, 3000);
    assert.equal((await invoke("trust_read")).answer, false, "the answer is kept");
    await readTrust();
    assert.ok(ask().hidden, "asked once: not asked again as the tree opens again");
    assert.ok(!status().hidden, "its code held back, as the status bar says");
    await ui.click(status());
    await ui.waitFor(() => !ask().hidden, 2000);
    await ui.click(buttonNamed("Trust Every Tree in trusting"));
    await ui.waitFor(() => ask().hidden && status().hidden, 3000);
    const after = await invoke("trust_read");
    assert.equal(after.answer, true, "the folder above's answer holds for the crate");
    runCommand("trust-tree");
    await ui.waitFor(() => !ask().hidden, 2000);
    await ui.click(buttonNamed("Trust It"));
    await ui.waitFor(() => ask().hidden, 2000);
    assert.equal((await invoke("trust_read")).answer, true);
    assert.ok(crate.replace(/\\/g, "/").endsWith("trusting/crate"));
  } finally {
    await invoke("root_set", { path: root });
    await invoke("trust_set", { folder: root, trusted: true });
    await readTrust();
  }
  assert.ok(ask().hidden && status().hidden, "the run's tree is trusted, and nothing is held back");
});
