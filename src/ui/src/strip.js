// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The tool strip down the window's left edge, and the tools at the top bar's right end.
//
// The strip's icons each open a tool window, and a press on the one open closes it. In the edit view
// the pointer over an icon of the explorer's panes opens the explorer on them, and the explorer stays
// open while the pointer is on the strip or on it. At its top:
// Explorer, the edit view with its files; Structure, the open file's outline; Commit, the files that
// differ from the last commit; and more, a menu of the explorer's other panes. At its foot: Run, the
// run view and its jobs; Debug and Terminal, the panels under the views; Problems, the open files'
// diagnostics, its icon marked while there are errors; and Git, the branch's commits. The icon of
// each window that shows is drawn in signal on a square of it. Under the top icons, a line apart, the
// menus of jobs commands.json puts on the strip, Protocol, Ingest, Render, Sim, Pipeline and Stage,
// each an icon whose press opens its menu beside it, and each there only where the tree has its jobs.
//
// The top bar's tools: Preferences, Notifications, which lists what the status bar has said and is
// marked while some of it is unread, Toolchains, and the definitions beside the editor. Past them the
// window's own controls, minimize, maximize and close, in place of the system's frame: a press on
// the top bar where nothing else takes it moves the window, and a second press at once maximizes it
// or restores it.

import { invoke } from "./bridge.js";
import { showGroup, shownGroup, morePanes, showPane } from "./explorer.js";
import { icon } from "./icons.js";
import { showMenu } from "./menu.js";
import { paneNodeShown, togglePane, togglePaneNode } from "./sides.js";
import { clearNotices, noticesSaid, noticesSeen, noticesUnseen, onNotice, say } from "./statusbar.js";
import { onView, shownView, showView } from "./views.js";

const state = { hooks: null, buttons: new Map(), popover: null, errors: 0 };

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

const explorerShown = (group) => shownView() === "edit" && shownGroup() === group && paneNodeShown(document.getElementById("explorer"));

// A window of the explorer: the edit view with the group's panes, or, where it shows, closed.
function openGroup(group) {
  if (explorerShown(group)) {
    togglePane(false);
    return;
  }
  showView("edit");
  showGroup(group);
  togglePane(true, { take: false });
}

// The strip's icons: each its name, label, keys, what a press does, and whether its window shows.
const STRIP = {
  top: [
    ["explorer", "folder", "Explorer", "Ctrl+Shift+E", () => openGroup("explorer"), () => explorerShown("explorer")],
    ["structure", "structure", "Structure", "", () => openGroup("structure"), () => explorerShown("structure")],
    ["commit", "commit", "Commit", "", () => openGroup("commit"), () => explorerShown("commit")],
    ["more", "more", "More Tool Windows", "", (button) => moreMenu(button), () => false],
  ],
  foot: [
    ["run", "run", "Run", "Ctrl+Shift+D", () => (shownView() === "run" ? togglePane() : showView("run")), () => shownView() === "run"],
    ["debug", "debug", "Debug", "Alt+5", () => state.hooks.run("debug-view"), () => !document.getElementById("debug").hidden],
    ["terminal", "terminal", "Terminal", "Ctrl+`", () => state.hooks.run("terminal-view"), () => !document.getElementById("term").hidden],
    ["problems", "problems", "Problems", "", () => openGroup("problems"), () => explorerShown("problems")],
    ["git", "git", "Git", "", () => openGroup("git"), () => explorerShown("git")],
  ],
};

function moreMenu(button) {
  const box = button.getBoundingClientRect();
  showMenu(
    box.right + 4,
    box.top,
    morePanes().map(({ name, label }) => ({
      label,
      run: () => {
        showView("edit");
        showPane(name);
        togglePane(true, { take: false });
      },
    })),
    { anchor: button },
  );
}

// Marks the icon of each window that shows.
export function refreshStrip() {
  for (const [name, { button, shown }] of state.buttons) {
    const on = shown();
    button.classList.toggle("on", on);
    button.setAttribute("aria-pressed", String(on));
    if (name === "problems") {
      button.classList.toggle("marked", state.errors > 0);
      button.title = state.errors ? `Problems: ${state.errors} error${state.errors === 1 ? "" : "s"}` : "Problems";
    }
  }
  const bell = document.getElementById("tool-notifications");
  bell?.classList.toggle("marked", noticesUnseen() > 0 && noticesSaid().some((notice) => notice.failed));
  bell?.classList.toggle("unread", noticesUnseen() > 0);
}

// The icons whose windows are groups of the explorer's panes, which the pointer over them shows.
const HOVERED = new Set(["explorer", "structure", "commit", "problems", "git"]);

function stripButton(name, glyph, label, keys, run, shown) {
  const button = element("button", { className: `strip-button strip-${name}`, type: "button", title: keys ? `${label} (${keys})` : label }, icon(glyph));
  button.setAttribute("aria-label", label);
  button.addEventListener("click", () => {
    run(button);
    window.requestAnimationFrame(refreshStrip);
  });
  // In the edit view the pointer over one of them opens the explorer on its panes, and it stays open
  // while the pointer is on the strip or on it.
  if (HOVERED.has(name)) {
    button.addEventListener("pointerenter", () => {
      if (shownView() !== "edit" || explorerShown(name)) {
        return;
      }
      showGroup(name);
      togglePane(true, { take: false });
      window.requestAnimationFrame(refreshStrip);
    });
  }
  state.buttons.set(name, { button, shown });
  return button;
}

// Notifications: what the status bar has said, the newest first, in a panel under the bell.

function closeNotices() {
  state.popover?.remove();
  state.popover = null;
}

function time(at) {
  const date = new Date(at);
  return `${String(date.getHours()).padStart(2, "0")}:${String(date.getMinutes()).padStart(2, "0")}`;
}

function showNotices(bell) {
  if (state.popover) {
    closeNotices();
    return;
  }
  const said = noticesSaid();
  const list = element("div", { className: "notices-list" });
  for (const notice of [...said].reverse()) {
    list.append(element("div", { className: `notice${notice.failed ? " failed" : ""}` }, element("span", { className: "notice-text", textContent: notice.text }), element("span", { className: "notice-time", textContent: time(notice.at) })));
  }
  if (!said.length) {
    list.append(element("p", { className: "notices-empty", textContent: "What orior says on the status bar is kept here." }));
  }
  const clear = element("button", { className: "notices-clear", type: "button", textContent: "Clear" });
  clear.disabled = !said.length;
  clear.addEventListener("click", () => {
    clearNotices();
    closeNotices();
  });
  const panel = element("div", { className: "notices" }, element("div", { className: "notices-head" }, element("h3", { textContent: "Notifications" }), clear), list);
  panel.setAttribute("role", "dialog");
  panel.setAttribute("aria-label", "Notifications");
  document.body.append(panel);
  const box = bell.getBoundingClientRect();
  panel.style.top = `${box.bottom + 6}px`;
  panel.style.right = `${Math.max(8, window.innerWidth - box.right - 40)}px`;
  state.popover = panel;
  noticesSeen();
}

function toolButton(id, glyph, label, run) {
  const button = element("button", { className: "bar-tool", id, type: "button", title: label }, icon(glyph));
  button.setAttribute("aria-label", label);
  button.addEventListener("click", () => run(button));
  return button;
}

function toggleDefinitions() {
  const side = document.getElementById("defs-side");
  if (shownView() !== "edit" || side.hidden) {
    say("The open file's type has no definitions to show.");
    return;
  }
  togglePaneNode(side);
}

// The menus of jobs on the strip, drawn again as the menu bar is.
function drawJobs() {
  const group = document.getElementById("strip-jobs");
  const menus = state.hooks.jobMenus();
  group.hidden = !menus.length;
  group.replaceChildren(
    ...menus.map((menu) => {
      const button = element("button", { className: "strip-button strip-job", type: "button", title: menu.title }, icon(menu.icon));
      button.setAttribute("aria-label", menu.title);
      button.setAttribute("aria-haspopup", "menu");
      button.addEventListener("click", () => {
        const box = button.getBoundingClientRect();
        showMenu(box.right + 4, box.top, menu.items(), { anchor: button });
      });
      return button;
    }),
  );
}

// The window's controls, and the top bar as the handle the window moves by.
function startWindow() {
  const act = (what) => invoke("window_act", { act: what }).catch(() => false);
  const maximize = element("button", { className: "window-button", type: "button" });
  const drawMaximized = (maximized) => {
    maximize.replaceChildren(icon(maximized ? "restore" : "maximize"));
    maximize.title = maximized ? "Restore" : "Maximize";
    maximize.setAttribute("aria-label", maximize.title);
  };
  const control = (glyph, label, what, className = "") => {
    const button = element("button", { className: `window-button ${className}`.trim(), type: "button", title: label }, icon(glyph));
    button.setAttribute("aria-label", label);
    button.addEventListener("click", () => act(what).then(drawMaximized));
    return button;
  };
  maximize.addEventListener("click", () => act("maximize").then(drawMaximized));
  document.getElementById("window-controls").replaceChildren(control("minimize", "Minimize", "minimize"), maximize, control("close", "Close", "close", "window-close"));
  act("state").then(drawMaximized);
  window.addEventListener("resize", () => act("state").then(drawMaximized));
  // A press on the bar itself, its tree name or its empty middle moves the window; a press on a menu,
  // a tool or a control does what it does.
  document.querySelector("header.bar").addEventListener("mousedown", (event) => {
    if (event.button !== 0 || event.target.closest("button, nav, .bar-tools, .window-controls, input")) {
      return;
    }
    event.preventDefault();
    act(event.detail === 2 ? "maximize" : "drag").then(drawMaximized);
  });
}

// `hooks` runs a command of the menus by its name, and gives the menus of jobs the strip holds.
export function startStrip(hooks) {
  state.hooks = hooks;
  const strip = document.getElementById("strip");
  const top = element("div", { className: "strip-group" }, ...STRIP.top.map((one) => stripButton(...one)));
  const jobs = element("div", { className: "strip-group strip-jobs", id: "strip-jobs" });
  const foot = element("div", { className: "strip-group strip-foot" }, ...STRIP.foot.map((one) => stripButton(...one)));
  strip.replaceChildren(top, jobs, foot);
  drawJobs();
  window.addEventListener("menus-drawn", drawJobs);
  startWindow();
  document
    .getElementById("bar-tools")
    .replaceChildren(
      toolButton("tool-preferences", "gear", "Preferences (Ctrl+,)", () => hooks.run("preferences")),
      toolButton("tool-notifications", "bell", "Notifications", (button) => showNotices(button)),
      toolButton("tool-toolchains", "database", "Toolchains", () => hooks.run("toolchains")),
      toolButton("tool-definitions", "m", "Definitions", () => toggleDefinitions()),
    );
  document.addEventListener("mousedown", (event) => state.popover && !state.popover.contains(event.target) && !event.target.closest?.("#tool-notifications") && closeNotices());
  window.addEventListener("keydown", (event) => event.key === "Escape" && state.popover && closeNotices());
  onNotice(refreshStrip);
  onView(() => window.requestAnimationFrame(refreshStrip));
  window.addEventListener("panes-changed", refreshStrip);
  window.addEventListener("problems-changed", (event) => {
    state.errors = event.detail.errors;
    refreshStrip();
  });
  // The panes and panels show and collapse from many places; the strip follows what they do.
  const watched = new MutationObserver(() => refreshStrip());
  for (const id of ["explorer", "job-side", "debug", "term", "defs-side"]) {
    watched.observe(document.getElementById(id), { attributes: true, attributeFilter: ["hidden", "class"] });
  }
  refreshStrip();
}
