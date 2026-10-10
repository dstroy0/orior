// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Commit window, the Changes pane of the explorer's Commit group: every file that differs from
// the last commit, each with a box that says whether the next commit takes it, all of them taken
// until one is left out. A press on a file shows its changes side by side. Under the files a field
// for the commit's message, Commit, and Commit and Push. Its bar reads the tree again, rolls the
// files taken back to the last commit after a second press, pulls, and pushes, beside how many
// commits the branch is behind and ahead of its remote.
//
// A commit runs git's hooks and signing as the tree has them, and saves the open files first. What
// git says when it refuses shows under the buttons.

import { invoke } from "./bridge.js";
import { icon } from "./icons.js";
import { iconOf } from "./explorer.js";
import { menuOn, copyText } from "./menu.js";
import { say } from "./statusbar.js";

const MARKS = { M: "modified", A: "added", D: "deleted", R: "renamed", U: "new", C: "conflicted" };

const state = { hooks: null, changes: new Map(), left: new Set(), busy: false, rolling: false, message: "", said: "", sync: null };

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

const taken = () => [...state.changes.keys()].filter((path) => !state.left.has(path)).sort();

function toolButton(glyph, label, run, disabled = false) {
  const button = element("button", { className: "commit-tool", type: "button", title: label }, icon(glyph));
  button.setAttribute("aria-label", label);
  button.disabled = disabled || state.busy;
  button.addEventListener("click", run);
  return button;
}

async function act(what, run) {
  state.busy = true;
  state.said = "";
  draw();
  say(`${what}…`);
  try {
    const said = await run();
    if (said) {
      say(said.split("\n")[0]);
    }
  } catch (error) {
    state.said = String(error);
    say(String(error).split("\n")[0], { failed: true });
  } finally {
    state.busy = false;
    await state.hooks.refresh();
    await readSync();
    draw();
  }
}

async function commit(andPush) {
  const paths = taken();
  const message = state.message.trim();
  await act(andPush ? "Committing and pushing" : "Committing", async () => {
    await state.hooks.saveAll();
    const made = await invoke("git_commit", { message, paths });
    state.message = "";
    if (!andPush) {
      return made;
    }
    await invoke("git_push");
    return `${made}, and pushed`;
  });
}

async function rollback() {
  const paths = taken();
  if (!state.rolling) {
    state.rolling = true;
    draw();
    say(`A second press rolls ${paths.length} file${paths.length === 1 ? "" : "s"} back to the last commit.`);
    window.setTimeout(() => {
      state.rolling = false;
      draw();
    }, 4000);
    return;
  }
  state.rolling = false;
  await act("Rolling back", async () => {
    const refused = [];
    for (const path of paths) {
      await invoke("git_rollback", { path }).catch((error) => refused.push(String(error)));
    }
    await state.hooks.reload(paths);
    if (refused.length) {
      throw new Error(refused.join("\n"));
    }
    return `Rolled ${paths.length} file${paths.length === 1 ? "" : "s"} back to the last commit.`;
  });
}

async function readSync() {
  state.sync = await invoke("git_ahead_behind").catch(() => null);
}

function rowOf(path, mark) {
  const cut = path.lastIndexOf("/");
  const box = element("input", { type: "checkbox", className: "commit-take", checked: !state.left.has(path), title: "The next commit takes it" });
  box.setAttribute("aria-label", `Commit ${path}`);
  box.addEventListener("change", () => {
    if (box.checked) {
      state.left.delete(path);
    } else {
      state.left.add(path);
    }
    draw();
  });
  const row = element("div", { className: "node change-row", title: `${path}: ${MARKS[mark] ?? mark}. A press shows its changes.` }, box, iconOf(path.slice(cut + 1)), element("span", { className: "name", textContent: path.slice(cut + 1) }), element("span", { className: "where", textContent: path.slice(0, Math.max(0, cut)) }), element("span", { className: "change", textContent: mark }));
  row.dataset.change = mark;
  row.dataset.key = `change:${path}`;
  row.dataset.path = path;
  row.addEventListener("click", (event) => event.target !== box && state.hooks.diff(path, mark));
  return row;
}

export function drawCommit(changes = state.changes) {
  state.changes = changes;
  const body = document.getElementById("changes");
  if (!body || !state.hooks?.shown()) {
    return;
  }
  const paths = [...changes.keys()].sort();
  for (const path of [...state.left]) {
    if (!changes.has(path)) {
      state.left.delete(path);
    }
  }
  const chosen = taken();
  const [ahead, behind] = state.sync ?? [0, 0];
  const bar = element(
    "div",
    { className: "commit-bar" },
    toolButton("refresh", "Read the tree again", () => act("Reading the tree", async () => "")),
    toolButton("undo", state.rolling ? "Press again to roll back" : "Roll back the files taken", rollback, !chosen.length),
    element("span", { className: "commit-gap" }),
    toolButton("pull", state.sync ? `Pull${behind ? `: ${behind} behind` : ""}` : "Pull", () => act("Pulling", () => invoke("git_pull")), !state.sync),
    state.sync && behind ? element("span", { className: "commit-count", textContent: String(behind) }) : null,
    toolButton("push", state.sync ? `Push${ahead ? `: ${ahead} ahead` : ""}` : "Push: sets the branch to follow origin", () => act("Pushing", () => invoke("git_push"))),
    state.sync && ahead ? element("span", { className: "commit-count", textContent: String(ahead) }) : null,
  );
  bar.children[1].classList.toggle("asking", state.rolling);
  const all = element("input", { type: "checkbox", className: "commit-take", checked: chosen.length === paths.length && paths.length > 0, title: "Take every file" });
  all.indeterminate = chosen.length > 0 && chosen.length < paths.length;
  all.disabled = !paths.length;
  all.addEventListener("change", () => {
    state.left = all.checked ? new Set() : new Set(paths);
    draw();
  });
  const head = element("label", { className: "commit-head" }, all, element("span", { textContent: paths.length ? `${chosen.length} of ${paths.length} file${paths.length === 1 ? "" : "s"} taken` : "No file differs from the last commit." }));
  const list = element("div", { className: "commit-list" }, ...paths.map((path) => rowOf(path, changes.get(path))));
  const message = element("textarea", { className: "commit-message", placeholder: "Commit message", spellcheck: true, value: state.message, rows: 3 });
  message.setAttribute("aria-label", "Commit message");
  const ready = () => Boolean(state.message.trim() && taken().length && !state.busy);
  const commitButton = element("button", { className: "commit-button primary-ish", type: "button", textContent: state.busy ? "Working…" : "Commit" });
  const pushButton = element("button", { className: "commit-button", type: "button", textContent: "Commit and Push" });
  const arm = () => {
    commitButton.disabled = !ready();
    pushButton.disabled = !ready();
  };
  message.addEventListener("input", () => {
    state.message = message.value;
    arm();
  });
  message.addEventListener("keydown", (event) => {
    if (event.key === "Enter" && event.ctrlKey && ready()) {
      event.preventDefault();
      commit(false);
    }
  });
  commitButton.addEventListener("click", () => commit(false));
  pushButton.addEventListener("click", () => commit(true));
  arm();
  const buttons = element("div", { className: "commit-buttons" }, commitButton, pushButton);
  const said = state.said ? element("pre", { className: "commit-said", textContent: state.said }) : null;
  const focused = document.activeElement?.classList.contains("commit-message");
  body.replaceChildren(bar, head, list, message, buttons, ...(said ? [said] : []));
  if (focused) {
    message.focus();
    message.setSelectionRange(message.value.length, message.value.length);
  }
}

const draw = () => drawCommit();

// `hooks` says whether the Changes pane shows, saves every open file, re-reads the open tabs of
// files rolled back, reads the tree's changes again, and shows a file's changes.
export function startCommit(hooks) {
  state.hooks = hooks;
  readSync().then(draw);
  menuOn(document.getElementById("changes"), (event) => {
    const path = event.target.closest(".change-row")?.dataset.path;
    if (!path) {
      return null;
    }
    const mark = state.changes.get(path);
    return [
      { label: "Show Changes", run: () => hooks.diff(path, mark) },
      { label: "Open File", disabled: mark === "D", run: () => hooks.open(path) },
      "-",
      { label: "Copy Path", run: () => copyText(path) },
    ];
  });
}
