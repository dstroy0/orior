// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { CORES_KEY, coresKept, giveCores, KEPT_LEAST, machineCores, setCoresKept } from "../../src/cores.js";
import { runCommand } from "../../src/menubar.js";
import { keepingStorage } from "../helpers.js";

const { test, assert, ui } = window.__harness;

test("one core is kept for the reader where nothing kept says more, and never fewer", () =>
  keepingStorage([CORES_KEY], () => {
    localStorage.removeItem(CORES_KEY);
    assert.equal(coresKept(), KEPT_LEAST);
    assert.equal(KEPT_LEAST, 1);
    for (const wrong of ["0", "-2", "1.5", "many", ""]) {
      localStorage.setItem(CORES_KEY, wrong);
      assert.equal(coresKept(), KEPT_LEAST, `kept ${JSON.stringify(wrong)}`);
    }
  }));

test("orior's side takes the cores kept and works on the rest, one at the fewest", () =>
  keepingStorage([CORES_KEY], async () => {
    try {
      localStorage.removeItem(CORES_KEY);
      const one = await giveCores();
      assert.deepEqual([one.kept, one.work], [1, Math.max(1, one.machine - 1)], JSON.stringify(one));
      const two = await setCoresKept(2);
      assert.deepEqual([two.kept, two.work], [2, Math.max(1, two.machine - 2)], JSON.stringify(two));
      const all = await setCoresKept(two.machine + 3);
      assert.equal(all.work, 1, "keeping more than the machine has leaves one to work on");
      const none = await setCoresKept(0);
      assert.equal(none.kept, 1, "keeping none keeps one");
    } finally {
      localStorage.removeItem(CORES_KEY);
      await giveCores();
    }
  }));

test("in Preferences the reader keeps more cores for themselves, from one to all the machine's but one", () =>
  keepingStorage([CORES_KEY], async () => {
    localStorage.removeItem(CORES_KEY);
    runCommand("preferences");
    try {
      const select = () => document.querySelector("dialog.sheet[open] .prefs-cores");
      await ui.waitFor(select, 5000);
      const values = [...select().options].map((option) => Number(option.value));
      assert.equal(values[0], 1);
      assert.equal(values.at(-1), Math.max(1, machineCores() - 1));
      assert.equal(select().value, "1", "one kept to start");
      await ui.click(select());
      await ui.key("Down");
      await ui.key("Enter");
      await ui.waitFor(() => coresKept() === 2, 2000);
      const taken = await giveCores();
      assert.equal(taken.kept, 2, "and orior's side keeps the two");
    } finally {
      document.querySelector("dialog.sheet")?.close();
      localStorage.removeItem(CORES_KEY);
      await giveCores();
    }
  }));
