// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { closeMenu, menuOpen } from "../../src/menu.js";
import { cssVariable } from "../helpers.js";

const { test, assert, ui } = window.__harness;

const more = () => document.querySelector(".strip-button.strip-more");
const openMenu = () => document.querySelector(".menu");

// The color an item's highlight is drawn in, as the page computes it.
function highlight() {
  const probe = document.body.appendChild(document.createElement("i"));
  probe.style.background = cssVariable("--ed-sel");
  const color = getComputedStyle(probe).backgroundColor;
  probe.remove();
  return color;
}

// Moves the pointer off the strip and its menu, and waits for the menu to go.
async function leave() {
  await ui.hover({ x: innerWidth / 2, y: innerHeight / 2 });
  await ui.waitFor(() => !menuOpen(), 2000).catch(() => closeMenu(false));
}

test("the pointer resting on More opens its menu, leaving the keys where they are", async () => {
  const before = document.activeElement;
  await ui.hover(more());
  await ui.waitFor(() => openMenu());
  assert.equal(more().dataset.menu, "open");
  assert.equal(document.activeElement, before, "the keys stay where they were");
  await leave();
});

// The item under the pointer is lit with the keys or without them: a window behind another gives
// no item :focus, and the keys taken from the item stand in for that here.
test("an item of the menu More opened by resting is lit as the pointer moves onto it, with the keys or without", async () => {
  await ui.hover(more());
  const menu = await ui.waitFor(() => openMenu());
  const items = [...menu.querySelectorAll(".menu-item:not([disabled])")];
  assert.ok(items.length > 1, "the menu lists the other panes");
  const lit = () => items.filter((one) => getComputedStyle(one).backgroundColor === highlight()).map((one) => one.textContent);
  for (const item of items.slice(0, 2)) {
    await ui.hover(item);
    await ui.waitFor(() => lit().includes(item.textContent), 1000).catch(() => {});
    assert.deepEqual(lit(), [item.textContent], "the item under the pointer alone is lit");
    item.blur();
    assert.notEqual(document.activeElement, item);
    assert.deepEqual(lit(), [item.textContent], "it stays lit without the keys");
  }
  await leave();
});

test("a key that steps the menu moves the light from the item the pointer lit", async () => {
  await ui.click(more());
  const menu = await ui.waitFor(() => openMenu());
  const items = [...menu.querySelectorAll(".menu-item:not([disabled])")];
  const lit = () => items.filter((one) => getComputedStyle(one).backgroundColor === highlight()).map((one) => one.textContent);
  await ui.hover(items[0]);
  await ui.waitFor(() => lit().includes(items[0].textContent), 1000);
  await ui.key("Down");
  await ui.waitFor(() => document.activeElement === items[1], 1000);
  assert.deepEqual(lit(), [items[1].textContent]);
  await ui.key("Escape");
  await ui.waitFor(() => !menuOpen());
});

test("the menu More opened by resting goes once the pointer leaves it and the icon", async () => {
  await ui.hover(more());
  await ui.waitFor(() => openMenu());
  await leave();
  assert.equal(menuOpen(), false);
  assert.notEqual(more().dataset.menu, "open");
});

test("a press on More opens its menu with the keys on its first item, and it stays", async () => {
  await ui.click(more());
  const menu = await ui.waitFor(() => openMenu());
  const first = menu.querySelector(".menu-item:not([disabled])");
  assert.equal(document.activeElement, first);
  await ui.hover({ x: innerWidth / 2, y: innerHeight / 2 });
  await ui.rest(700);
  assert.equal(menuOpen(), true, "a pressed menu stays as the pointer leaves");
  await ui.key("Escape");
  await ui.waitFor(() => !menuOpen());
});
