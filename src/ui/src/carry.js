// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Settings on every machine, as carry.rs in the command line's crate carries them: the reader's
// settings, keys, themes, plugins with each one's own settings, menus and stylesheet, sent to a file
// or a folder of the reader's, a repository among them, and taken in from one. Preferences, Settings
// on Every Machine, names the file or the folder, kept on this machine alone. What a taking would
// change is shown first, a file at a time and settings.json a setting at a time, each chosen or not,
// and nothing is written until the reader takes what is chosen. Each taking is listed, and undone
// from the list.

import { invoke } from "./bridge.js";
import { ownItem, settingsChanged } from "./settings.js";

const PLACE = "orior.carry.place";

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

const HOW = { there: "only there", here: "only here, and stays", changed: "changed" };

// Takes into the page what orior's folder keeps after a taking or its undoing: each setting and the
// themes as the files hold them now, each setting no file keeps taken out, each part of the window
// that read them shown again, and the plugins, the stylesheet and the menus read again.
export async function takeKept(reloadOwn) {
  const now = await invoke("kept_now", { keys: Object.keys(localStorage) });
  const changed = new Set();
  for (const [key, text] of Object.entries(now.set)) {
    if (ownItem(`orior.${key}`) !== text) {
      localStorage.setItem(`orior.${key}`, text);
      changed.add(key);
    }
  }
  for (const key of now.gone) {
    localStorage.removeItem(`orior.${key}`);
    changed.add(key);
  }
  settingsChanged(changed);
  await reloadOwn?.();
  return changed;
}

// The file or folder the settings are carried through, kept on this machine alone.
export const carryPlace = () => ownItem(PLACE) ?? "";

export const setCarryPlace = (path) => localStorage.setItem(PLACE, path.trim());

// The settings sent to `to`, and what that says.
export const carrySend = (to) => invoke("carry_send", { to });

// What `from` carries that differs from this machine's taken in: each file and each setting of
// settings.json named in `chosen`, or every one where `chosen` is empty. Answers what was taken, or
// null where nothing differs or nothing is chosen.
export async function carryTake(from, chosen, reloadOwn) {
  const read = await invoke("carry_read", { from });
  const wanted = (name) => !chosen.length || chosen.includes(name);
  const places = read.differences.filter((one) => one.how !== "here" && one.place !== "settings.json" && wanted(one.place)).map((one) => one.place);
  const keys = read.differences.flatMap((one) => one.settings.map((setting) => setting.key)).filter(wanted);
  if (keys.length) {
    places.push("settings.json");
  }
  if (!places.length) {
    return null;
  }
  const taking = await invoke("carry_take", { from, there: read.there, places, keys });
  await takeKept(reloadOwn);
  return taking;
}

// The taking `id` undone.
export async function carryUndo(id, reloadOwn) {
  const taking = await invoke("carry_undo", { id });
  await takeKept(reloadOwn);
  return taking;
}

// Draws Settings on Every Machine into `section`: the file or folder, the buttons that choose one,
// send there and take from there, what a taking would change, and the takings kept.
export function carryInto(section, { reloadOwn = null, pick }) {
  const said = element("p", { className: "prefs-said carry-said", ariaLive: "polite" });
  const place = element("input", { className: "report-field carry-place", type: "text", value: ownItem(PLACE) ?? "", placeholder: "A file, or a folder of yours such as a repository", ariaLabel: "File or folder", spellcheck: false });
  place.addEventListener("change", () => localStorage.setItem(PLACE, place.value.trim()));
  const shown = element("div", { className: "carry-differences" });
  const kept = element("div", { className: "carry-takings" });
  const button = (text, run) => {
    const made = element("button", { type: "button", className: "prefs-button", textContent: text });
    made.addEventListener("click", async () => {
      made.disabled = true;
      try {
        await run();
      } catch (error) {
        said.textContent = String(error);
      } finally {
        made.disabled = false;
      }
    });
    return made;
  };
  const chosen = (kind) => async () => {
    const path = await pick(kind).catch(() => null);
    if (typeof path === "string") {
      place.value = path;
      localStorage.setItem(PLACE, path);
    }
  };
  const where = () => {
    const path = place.value.trim();
    if (!path) {
      throw new Error("Name a file or a folder first.");
    }
    localStorage.setItem(PLACE, path);
    return path;
  };

  const drawTakings = async () => {
    const takings = await invoke("carry_takings").catch(() => []);
    kept.replaceChildren(
      ...(takings.length ? [element("h4", { textContent: "Takings" })] : []),
      ...takings.map((taking) => {
        const undo = button("Undo", async () => {
          await carryUndo(taking.id, reloadOwn);
          said.textContent = `The taking from ${taking.from} is undone.`;
          await drawTakings();
        });
        const row = element("div", { className: "carry-taking" }, element("span", { textContent: `${new Date(taking.at).toLocaleString()} from ${taking.from}, ${taking.files.length} file${taking.files.length === 1 ? "" : "s"}` }), undo);
        row.dataset.id = taking.id;
        return row;
      }),
    );
  };

  // What `from` carries that differs from this machine's, each to choose, and Take the Chosen.
  const drawDifferences = async (from) => {
    const read = await invoke("carry_read", { from });
    if (!read.differences.length) {
      shown.replaceChildren();
      said.textContent = `${from} holds the settings this machine has.`;
      return;
    }
    said.textContent = "";
    const boxes = [];
    const rows = read.differences.map((difference) => {
      const takes = difference.how !== "here" && difference.place !== "settings.json";
      const box = takes ? element("input", { type: "checkbox", checked: true, ariaLabel: difference.place }) : null;
      if (box) {
        box.dataset.place = difference.place;
        boxes.push(box);
      }
      const head = element("label", { className: "carry-file" }, box, element("code", { textContent: difference.place }), element("span", { className: "carry-how", textContent: HOW[difference.how] }));
      const settings = difference.settings.map((setting) => {
        const one = element("input", { type: "checkbox", checked: true, ariaLabel: setting.key });
        one.dataset.key = setting.key;
        boxes.push(one);
        return element(
          "label",
          { className: "carry-setting" },
          one,
          element("code", { textContent: setting.key }),
          element("span", { className: "carry-here", textContent: setting.here ?? "not set", title: "here" }),
          element("span", { className: "carry-arrow", textContent: "→" }),
          element("span", { className: "carry-there", textContent: setting.there ?? "not set", title: "there" }),
        );
      });
      const row = element("div", { className: "carry-difference" }, head, ...settings);
      row.dataset.place = difference.place;
      return row;
    });
    const take = button("Take the Chosen", async () => {
      const keys = boxes.filter((box) => box.dataset.key && box.checked).map((box) => box.dataset.key);
      const places = boxes.filter((box) => box.dataset.place && box.checked && box.dataset.place !== "settings.json").map((box) => box.dataset.place);
      if (keys.length) {
        places.push("settings.json");
      }
      if (!places.length) {
        said.textContent = "Nothing is chosen.";
        return;
      }
      const taking = await invoke("carry_take", { from, there: read.there, places, keys });
      await takeKept(reloadOwn);
      shown.replaceChildren();
      said.textContent = `Took ${taking.files.length} file${taking.files.length === 1 ? "" : "s"} from ${from}; Undo under Takings writes them back.`;
      await drawTakings();
    });
    const leave = button("Leave Them", () => {
      shown.replaceChildren();
      said.textContent = "";
    });
    shown.replaceChildren(element("p", { className: "carry-heading", textContent: `What ${from} would change here:` }), ...rows, element("div", { className: "prefs-actions" }, take, leave));
  };

  section.replaceChildren(
    element("h3", { textContent: "Settings on Every Machine" }),
    element("p", { className: "carry-about", textContent: "Your settings, keys, themes, plugins with their own settings, menus and stylesheet, sent to a file or a folder and taken in from one on another machine. The settings of this machine alone stay here." }),
    element("div", { className: "prefs-line carry-line" }, place, button("Choose File…", chosen("save")), button("Choose Folder…", chosen("dir"))),
    element(
      "div",
      { className: "prefs-actions" },
      button("Send", async () => {
        said.textContent = await carrySend(where());
      }),
      button("Take…", () => drawDifferences(where())),
    ),
    said,
    shown,
    kept,
  );
  drawTakings();
}
