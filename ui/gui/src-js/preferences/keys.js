// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The keys section of Preferences, which takes the sheet it is drawn in.

import { buttonName, keySettings, pressName, setKeySettings, SETS, sharedKeys, wheelName } from "../keys.js";

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

// The keys: the set chosen, how a letter's key is bound, each key more than one command holds with
// those commands, and each command's keys, two at most, each set by pressing it, a button of the
// mouse past its first three or the wheel turned with a key pressed, and each taken off by its cross.
export function keysSection(at) {
  const { menus, state, draw } = at;
  const settings = keySettings();
  const items = menus.flatMap((menu) => (menu.all ?? []).filter((item) => item.command).map((item) => ({ item, label: `${menu.title}: ${item.label.replace(/…$/, "")}` })));
  const set = element("select", { className: "report-field", ariaLabel: "Keys from" }, ...Object.entries(SETS).map(([id, one]) => element("option", { value: id, textContent: one.name, selected: id === settings.set })));
  set.addEventListener("change", () => {
    setKeySettings({ set: set.value });
    draw();
  });
  const by = element(
    "select",
    { className: "report-field", ariaLabel: "A letter's key" },
    element("option", { value: "character", textContent: "the key that types it", selected: settings.by === "character" }),
    element("option", { value: "place", textContent: "the key in its place on the board", selected: settings.by === "place" }),
  );
  by.addEventListener("change", () => {
    setKeySettings({ by: by.value });
    draw();
  });
  const shared = [...sharedKeys(items.map(({ item }) => item))].map(([key, sharing]) => element("li", {}, element("kbd", { textContent: key }), ` ${sharing.map((item) => items.find((one) => one.item === item)?.label ?? item.label).join(", ")}`));
  const filter = element("input", { className: "report-field prefs-keys-filter", type: "search", placeholder: "Filter commands", value: state.keysFilter ?? "", ariaLabel: "Filter commands", spellcheck: false });
  const rows = element("div", { className: "prefs-key-rows" });
  const own = (command) => keySettings().own[command];
  const setOwn = (command, keys) => {
    const now = keySettings().own;
    setKeySettings({ own: { ...now, [command]: keys } });
    draw();
  };
  const fill = () => {
    const wanted = filter.value.trim().toLowerCase();
    rows.replaceChildren(
      ...items
        .filter(({ label, item }) => !wanted || label.toLowerCase().includes(wanted) || [item.keys, item.also].some((key) => key?.toLowerCase().includes(wanted)))
        .map(({ item, label }) => {
          const keys = [item.keys, item.also].filter(Boolean);
          const chips = keys.map((key) => {
            const off = element("button", { type: "button", className: "prefs-key-off", textContent: "×", title: `Take ${key} off` });
            off.addEventListener("click", () => setOwn(item.command, keys.filter((one) => one !== key)));
            return element("span", { className: "prefs-key" }, element("kbd", { textContent: key }), off);
          });
          const record = element("button", { type: "button", className: "prefs-button prefs-key-set", textContent: "Set…" });
          record.addEventListener("click", () => {
            record.textContent = "Press the keys";
            const done = (name) => {
              document.removeEventListener("keydown", onKey, true);
              document.removeEventListener("mousedown", onButton, true);
              document.removeEventListener("wheel", onWheel, true);
              if (name) {
                setOwn(item.command, [name, ...keys.filter((one) => one !== name)].slice(0, 2));
              } else {
                record.textContent = "Set…";
              }
            };
            const onKey = (event) => {
              event.preventDefault();
              event.stopPropagation();
              if (event.key === "Escape") {
                done(null);
              } else if (pressName(event)) {
                done(pressName(event));
              }
            };
            const onButton = (event) => {
              if (buttonName(event)) {
                event.preventDefault();
                done(buttonName(event));
              }
            };
            const onWheel = (event) => {
              if (wheelName(event)) {
                event.preventDefault();
                done(wheelName(event));
              }
            };
            document.addEventListener("keydown", onKey, true);
            document.addEventListener("mousedown", onButton, true);
            document.addEventListener("wheel", onWheel, { capture: true, passive: false });
          });
          const reset = element("button", { type: "button", className: "prefs-button", textContent: "Reset", disabled: own(item.command) === undefined });
          reset.addEventListener("click", () => {
            const now = { ...keySettings().own };
            delete now[item.command];
            setKeySettings({ own: now });
            draw();
          });
          const row = element("div", { className: "prefs-key-row" }, element("span", { className: "prefs-key-label", textContent: label }), element("span", { className: "prefs-key-keys" }, ...chips), record, reset);
          row.dataset.command = item.command;
          return row;
        }),
    );
  };
  filter.addEventListener("input", () => {
    state.keysFilter = filter.value;
    fill();
  });
  fill();
  return element(
    "section",
    { className: "prefs-keys" },
    element("h3", { textContent: "Keys" }),
    element("label", { className: "report-row" }, element("span", { textContent: "Keys from" }), set),
    element("label", { className: "report-row" }, element("span", { textContent: "A letter's key is" }), by),
    shared.length ? element("div", { className: "prefs-shared" }, element("p", { textContent: "Keys more than one command holds, each acting where its command can:" }), element("ul", {}, ...shared)) : null,
    filter,
    rows,
  );
}
