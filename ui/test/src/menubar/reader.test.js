// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Menus and a toolbar of the reader's, from menus.json: a menu of the reader's located in for one of orior's of
// the same name, the others before Help, orior's hidden by name, an item that names no command drawn but not
// chosen, and the toolbar's buttons displayed and hidden.

import { allCommands, state } from "../../../gui/src-js/menubar.js";
import { loadReaderMenus, setToolbarShown, displayedMenus, toolbarShown } from "../../../gui/src-js/menubar/reader.js";
import { retentionStorage } from "../../helpers.js";
import { putBackAsFound, told } from "../../menus.js";

const { test, assert, calls } = window.__harness;

// Runs `work` with menus.json reading `text`, and the reader's own read again after it.
async function readingMenus(text, work) {
  calls.answer("reader_menus_read", () => text);
  try {
    await loadReaderMenus();
    await work();
  } finally {
    calls.answers.delete("reader_menus_read");
    await loadReaderMenus();
  }
}

test("a menu of the reader's is in for orior's of its name, the others go before Help, and one is hidden by name", async () => {
  const [first, second] = state.menus.filter((menu) => !menu.strip && !menu.popup && menu.items?.length);
  const [one, two] = allCommands().filter((item) => item.label);
  const json = { menus: [{ title: first.title, items: [one.command, "-", { command: two.command, label: "Of the reader's" }, { label: "Under", items: [one.command] }, "no-such-command"] }, { title: "Mine", items: [two.command] }], hide: [second.title] };
  await readingMenus(JSON.stringify(json), () => {
    const titles = displayedMenus().map((menu) => menu.title);
    assert.equal(titles[0], first.title);
    assert.ok(!titles.includes(second.title), "hidden by name");
    const help = titles.indexOf("Help");
    assert.equal(titles.indexOf("Mine"), help < 0 ? titles.length - 1 : help - 1, "a menu of the reader's own before Help");
    const items = displayedMenus()[0].items;
    assert.equal(items[0].command, one.command);
    assert.equal(items[1], "-");
    assert.deepEqual([items[2].command, items[2].label], [two.command, "Of the reader's"]);
    assert.deepEqual([items[3].label, items[3].items[0].command], ["Under", one.command]);
    assert.deepEqual([items[4].label, items[4].needs], ["no-such-command: no such command", "never"]);
  });
});

test("the toolbar has a button for each item and a line for each -, a command's own label, and shows and hides", async () => {
  const [one, two] = allCommands().filter((item) => item.label);
  const json = { toolbar: [one.command, "-", { command: two.command, icon: "Z" }, "no-such-command"] };
  await retentionStorage(["orior.toolbar"], () =>
    readingMenus(JSON.stringify(json), () => {
      const bar = document.getElementById("toolbar");
      setToolbarShown(true);
      assert.ok(!bar.hidden);
      const buttons = [...bar.querySelectorAll(".toolbar-button")];
      assert.equal(buttons.length, 3);
      assert.equal(bar.querySelectorAll(".toolbar-line").length, 1);
      assert.equal(buttons[0].getAttribute("aria-label"), one.label);
      assert.equal(buttons[1].textContent.trim(), "Z", "the icon it names");
      assert.ok(buttons[2].disabled, "an item that names no command cannot act");
      setToolbarShown();
      assert.ok(!toolbarShown() && bar.hidden, "turned over, it hides");
    }),
  );
});

test("a menus.json that is not JSON is said, and orior's menus are", async () => {
  await readingMenus("{ not json", () => {
    assert.ok(told(/^menus\.json is not JSON/));
    assert.ok(displayedMenus().length && displayedMenus().every((menu) => state.menus.includes(menu)), "orior's own menus");
  });
});

test("the window is put back as the file found it", putBackAsFound);
