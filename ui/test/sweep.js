// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The sweep's eyes inside the window, put into the page by sweep.py beside monitor.js. It lists every
// control the window shows: each button, link, field, check, choice, slider, tab, menu item, option,
// tree item and focusable part, by a key that names it the same from one listing to the next, and a
// count where two read the same. A link that would take the page somewhere else is kept from going
// and named instead.

(() => {
  if (window.__sweep) {
    return "kept";
  }
  const SELECTOR = [
    "button", "a[href]", "input", "select", "textarea", "summary", "[contenteditable='']", "[contenteditable='true']",
    "[role=button]", "[role=link]", "[role=menuitem]", "[role=menuitemcheckbox]", "[role=menuitemradio]", "[role=tab]",
    "[role=checkbox]", "[role=switch]", "[role=radio]", "[role=option]", "[role=treeitem]", "[role=slider]",
    "[role=combobox]", "[role=textbox]", "[tabindex]:not([tabindex='-1'])", "[onclick]",
  ].join(",");
  const sweep = { navigations: [], found: [] };
  window.__sweep = sweep;

  document.addEventListener(
    "click",
    (event) => {
      const link = event.target?.closest?.("a[href]");
      const href = link?.getAttribute("href") ?? "";
      if (link && !href.startsWith("#")) {
        event.preventDefault();
        sweep.navigations.push(link.href);
      }
    },
    true,
  );

  const label = (element) =>
    (element.getAttribute("aria-label") || element.title || element.placeholder || element.innerText || element.value || element.name || "")
      .trim()
      .replace(/\s+/g, " ")
      .slice(0, 60);
  // A key names a control the same as its state changes: a control with an id by its id alone, and another by the
  // tags up to the nearest parent with an id and its label, each run of digits in the label one mark, as a line and
  // column or a count reads; classes, which a state adds and takes, are left out
  const step = (element) => `${element.tagName.toLowerCase()}${element.id ? `#${element.id}` : ""}`;
  const path = (element) => {
    const steps = [];
    for (let at = element; at && at !== document.body && steps.length < 6; at = at.parentElement) {
      steps.unshift(step(at));
      if (at.id && at !== element) {
        break;
      }
    }
    return steps.join(" > ");
  };
  const keyOf = (element) => (element.id ? step(element) : `${path(element)} | ${label(element).replace(/\d+/g, "#")}`);
  const kind = (element) => {
    const tag = element.tagName.toLowerCase();
    const role = element.getAttribute("role") || "";
    const type = (element.getAttribute("type") || "").toLowerCase();
    if (tag === "select" || role === "combobox") {
      return tag === "select" ? "choice" : "field";
    }
    if (tag === "input" && type === "file") {
      return "file";
    }
    if ((tag === "input" && type === "range") || role === "slider") {
      return "range";
    }
    if ((tag === "input" && ["checkbox", "radio"].includes(type)) || ["checkbox", "switch", "radio", "menuitemcheckbox", "menuitemradio"].includes(role)) {
      return "check";
    }
    if ((tag === "input" && !["button", "submit", "reset", "image", "color", "hidden"].includes(type)) || tag === "textarea" || element.isContentEditable || role === "textbox") {
      return "field";
    }
    if (role === "menuitem" || role === "option") {
      return "item";
    }
    if (role === "tab") {
      return "tab";
    }
    if (role === "treeitem") {
      return "tree";
    }
    return "button";
  };
  const shown = (element) => {
    const box = element.getBoundingClientRect();
    if (box.width < 1 || box.height < 1 || box.bottom < 0 || box.right < 0 || box.top > innerHeight || box.left > innerWidth) {
      return false;
    }
    const style = getComputedStyle(element);
    return style.visibility !== "hidden" && style.display !== "none" && Number(style.opacity) > 0 && !element.closest("[inert], [aria-hidden='true']");
  };

  // Every control shown, each once: a control inside another, as a button's own label, is the
  // outer one's
  sweep.list = () => {
    const out = [];
    const counts = new Map();
    sweep.found = [];
    for (const element of document.querySelectorAll(SELECTOR)) {
      const outer = element.parentElement?.closest(SELECTOR);
      const native = element.matches("button, a[href], input, select, textarea");
      if (!shown(element) || (outer && !native && kind(element) === "button")) {
        continue;
      }
      let key = keyOf(element);
      const seen = counts.get(key) ?? 0;
      counts.set(key, seen + 1);
      key = seen ? `${key} #${seen + 1}` : key;
      const box = element.getBoundingClientRect();
      const x = box.x + Math.min(box.width / 2, 40);
      const y = box.y + box.height / 2;
      const top = document.elementFromPoint(x, y);
      out.push({
        key,
        index: sweep.found.length,
        kind: kind(element),
        label: label(element),
        disabled: Boolean(element.disabled) || element.getAttribute("aria-disabled") === "true",
        x: Math.round(x),
        y: Math.round(y),
        covered: Boolean(top) && top !== element && !element.contains(top),
        options: element.tagName === "SELECT" ? element.options.length : 0,
      });
      sweep.found.push(element);
    }
    return out;
  };

  // The control of `key` from a listing made now, or null
  sweep.locate = (key) => sweep.list().find((one) => one.key === key) ?? null;

  // What a control holds: a field's text, a check's state, a choice's place, a slider's value
  sweep.state = (index) => {
    const element = sweep.found[index];
    if (!element || !element.isConnected) {
      return null;
    }
    if (element.isContentEditable) {
      return element.textContent;
    }
    if (element.tagName === "SELECT") {
      return element.selectedIndex;
    }
    if ("checked" in element && ["checkbox", "radio"].includes(element.type)) {
      return element.checked;
    }
    const aria = element.getAttribute("aria-checked");
    return aria ?? ("value" in element ? element.value : null);
  };

  // A field, choice or slider set to `value` as a person's edit sets it, its input and change told
  sweep.set = (index, value) => {
    const element = sweep.found[index];
    if (!element || !element.isConnected) {
      return false;
    }
    if (element.isContentEditable) {
      element.textContent = value;
    } else if (element.tagName === "SELECT") {
      element.selectedIndex = value;
    } else {
      element.value = value;
    }
    element.dispatchEvent(new Event("input", { bubbles: true }));
    element.dispatchEvent(new Event("change", { bubbles: true }));
    return true;
  };

  // A slider's least and most, for it to be set to each
  sweep.bounds = (index) => {
    const element = sweep.found[index];
    return element ? [element.min ?? element.getAttribute("aria-valuemin") ?? "0", element.max ?? element.getAttribute("aria-valuemax") ?? "100"] : null;
  };

  return "put";
})();
