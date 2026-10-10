// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The Commit window, the Changes pane of the explorer's Commit group: every file that differs from
// the last commit, each with a box that says whether the next commit takes it, all of them taken
// until one is left out. A press on a file shows its changes side by side. Under the files a field
// for the commit's message, Commit, and Commit and Push. Its bar reads the tree again, rolls the
// files taken back to the last commit after a second press, pulls, and pushes, beside how many
// commits the branch is behind and ahead of its remote.
//
// A file's arrow opens its changes under it, each change with a box of its own and each line of a
// change with one, and the next commit takes of the file only the lines whose boxes are checked; the
// rest stay in the file for a later commit.
//
// A commit runs git's hooks and signing as the tree has them, and saves the open files first. What
// git says when it refuses shows under the buttons.
//
// Where the tree holds more than one repository, each one's files stand under a row that names it and
// its branch, a commit goes to each repository that holds a file taken, Commit and Push pushes each of
// them, and the bar's pull and push act on the repository open.

import { invoke } from "./bridge.js";
import { icon } from "./icons.js";
import { iconOf } from "./explorer.js";
import { menuOn, copyText } from "./menu.js";
import { say } from "./statusbar.js";
import { lineChanges } from "./editor/diff.js";

const MARKS = { M: "modified", A: "added", D: "deleted", R: "renamed", U: "new", C: "conflicted" };

// `open` holds the files whose changes show under them, and `parts` each one's two texts, its
// changes, and the lines left out, `-n` a line of the last commit's text kept and `+n` a line of the
// file's left out.
const state = { hooks: null, changes: new Map(), left: new Set(), busy: false, rolling: false, message: "", said: "", sync: null, open: new Set(), parts: new Map() };

// The marks of a file whose changes open under it.
const PARTED = new Set(["M", "A", "U"]);

// Where the reader's choice to leave notebooks' outputs out of a commit is kept.
const OUTPUTS_KEY = "orior.commit.outputs";

const outputsLeft = () => localStorage.getItem(OUTPUTS_KEY) === "out";

// Every line a change holds, as `-n` and `+n`.
const keysOf = (hunk) => [...Array.from({ length: hunk.then[1] - hunk.then[0] }, (_, at) => `-${hunk.then[0] + at}`), ...Array.from({ length: hunk.now[1] - hunk.now[0] }, (_, at) => `+${hunk.now[0] + at}`)];

// Whether a file is taken in part: some of its lines left out and some not.
function inPart(path) {
  const part = state.parts.get(path);
  return Boolean(part?.out.size && part.hunks.some((hunk) => keysOf(hunk).some((key) => !part.out.has(key))));
}

// The text a file taken in part gives the commit: the last commit's, with the lines taken out that
// are taken and the lines added that are taken.
function partText(part) {
  const lines = [];
  let at = 0;
  for (const hunk of part.hunks) {
    lines.push(...part.then.slice(at, hunk.then[0]));
    for (let line = hunk.then[0]; line < hunk.then[1]; line += 1) {
      if (part.out.has(`-${line}`)) {
        lines.push(part.then[line]);
      }
    }
    for (let line = hunk.now[0]; line < hunk.now[1]; line += 1) {
      if (!part.out.has(`+${line}`)) {
        lines.push(part.now[line]);
      }
    }
    at = hunk.then[1];
  }
  lines.push(...part.then.slice(at));
  return lines.join(part.eol);
}

// Reads a file's two texts and its changes, for its changes to open under it.
async function readPart(path) {
  const { then, now } = await state.hooks.texts(path, state.changes.get(path));
  const thenLines = then === "" ? [] : then.split(/\r?\n/);
  const nowLines = now.split(/\r?\n/);
  const found = lineChanges(thenLines, nowLines);
  const hunks = found?.hunks ?? [{ then: [0, thenLines.length], now: [0, nowLines.length] }];
  state.parts.set(path, { then: thenLines, now: nowLines, hunks, out: new Set(), eol: (then || now).includes("\r\n") ? "\r\n" : "\n" });
}

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
    const parted = paths.filter(inPart);
    // A file taken in part is committed only as it was when its changes were opened.
    for (const path of parted) {
      const { now } = await state.hooks.texts(path, state.changes.get(path));
      if (now.split(/\r?\n/).join("\n") !== state.parts.get(path).now.join("\n")) {
        throw new Error(`${path} changed since its changes were opened. Open them again to choose what to take.`);
      }
    }
    const parts = parted.map((path) => ({ path, text: partText(state.parts.get(path)) }));
    // A notebook goes into the commit without its outputs and run counts where the reader says so,
    // and keeps them in its file.
    if (outputsLeft()) {
      for (const path of paths.filter((one) => /\.ipynb$/i.test(one) && !parted.includes(one))) {
        parts.push({ path, text: await invoke("notebook_without_outputs", { path }) });
      }
    }
    const whole = paths.filter((path) => !parts.some((part) => part.path === path));
    const made = parts.length ? await invoke("git_commit_parts", { message, whole, parts }) : await invoke("git_commit", { message, paths });
    state.parts.clear();
    state.open.clear();
    state.message = "";
    if (!andPush) {
      return made;
    }
    // Each repository a commit went to is pushed.
    for (const repo of new Set(paths.map((path) => state.hooks.repoOf(path)))) {
      await invoke("git_push", { repo });
    }
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
  state.sync = await invoke("git_ahead_behind", { repo: state.hooks.repo() }).catch(() => null);
}

function rowOf(path, mark) {
  const cut = path.lastIndexOf("/");
  const box = element("input", { type: "checkbox", className: "commit-take", checked: !state.left.has(path), title: "The next commit takes it" });
  box.indeterminate = !state.left.has(path) && inPart(path);
  box.setAttribute("aria-label", `Commit ${path}`);
  box.addEventListener("change", () => {
    state.parts.get(path)?.out.clear();
    if (box.checked) {
      state.left.delete(path);
    } else {
      state.left.add(path);
    }
    draw();
  });
  const opens = PARTED.has(mark);
  const twisty = element("button", { type: "button", className: "commit-twisty", title: opens ? "Its changes" : "", disabled: !opens });
  twisty.setAttribute("aria-expanded", String(state.open.has(path)));
  twisty.addEventListener("click", async () => {
    if (state.open.has(path)) {
      state.open.delete(path);
    } else {
      await readPart(path);
      state.open.add(path);
    }
    draw();
  });
  const row = element("div", { className: "node change-row", title: `${path}: ${MARKS[mark] ?? mark}. A press shows its changes.` }, twisty, box, iconOf(path.slice(cut + 1)), element("span", { className: "name", textContent: path.slice(cut + 1) }), element("span", { className: "where", textContent: path.slice(0, Math.max(0, cut)) }), element("span", { className: "change", textContent: mark }));
  row.dataset.change = mark;
  row.dataset.key = `change:${path}`;
  row.dataset.path = path;
  row.addEventListener("click", (event) => event.target !== box && event.target !== twisty && state.hooks.diff(path, mark));
  return state.open.has(path) && state.parts.has(path) ? [row, ...partRows(path)] : [row];
}

// A file's changes under it: each change's row with its box and the lines it spans, then each line
// with its box, marked as taken out or added.
function partRows(path) {
  const part = state.parts.get(path);
  const set = (keys, on) => {
    keys.forEach((key) => (on ? part.out.delete(key) : part.out.add(key)));
    if (on) {
      state.left.delete(path);
    } else if (part.hunks.every((hunk) => keysOf(hunk).every((key) => part.out.has(key)))) {
      state.left.add(path);
      part.out.clear();
    }
    draw();
  };
  const taken = (key) => !state.left.has(path) && !part.out.has(key);
  const rows = [];
  for (const hunk of part.hunks) {
    const keys = keysOf(hunk);
    const box = element("input", { type: "checkbox", className: "commit-take", checked: keys.some(taken) });
    box.indeterminate = keys.some(taken) && !keys.every(taken);
    box.setAttribute("aria-label", `Commit the change at line ${hunk.now[0] + 1} of ${path}`);
    box.addEventListener("change", () => set(keys, box.checked));
    const size = hunk.now[1] - hunk.now[0];
    const span = size > 1 ? `lines ${hunk.now[0] + 1}–${hunk.now[1]}` : size === 1 ? `line ${hunk.now[1]}` : `after line ${hunk.now[0]}`;
    rows.push(element("label", { className: "commit-hunk" }, box, element("span", { textContent: span })));
    for (const key of keys) {
      const line = Number(key.slice(1));
      const gone = key[0] === "-";
      const lineBox = element("input", { type: "checkbox", className: "commit-take", checked: taken(key) });
      lineBox.addEventListener("change", () => set([key], lineBox.checked));
      rows.push(element("label", { className: `commit-line ${gone ? "gone" : "come"}` }, lineBox, element("span", { className: "commit-sign", textContent: gone ? "−" : "+" }), element("span", { className: "commit-text", textContent: (gone ? part.then : part.now)[line] || " " })));
    }
  }
  return rows;
}

// A repository's files, under a row that names it and the branch it is on, where the tree holds more
// than one.
function repoRows(repo, paths) {
  if (!paths.length) {
    return [];
  }
  const head = element("div", { className: "node commit-repo", title: repo.path || state.hooks.repoName(repo.path) }, element("span", { className: "name", textContent: state.hooks.repoName(repo.path) }), element("span", { className: "where", textContent: repo.branch ?? "" }), element("span", { className: "count", textContent: String(paths.length) }));
  return [head, ...paths.flatMap((path) => rowOf(path, state.changes.get(path)))];
}

export function drawCommit(changes = state.changes) {
  state.changes = changes;
  const body = document.getElementById("changes");
  if (!body || !state.hooks?.shown()) {
    return;
  }
  const paths = [...changes.keys()].sort();
  for (const path of [...state.left, ...state.parts.keys(), ...state.open]) {
    if (!changes.has(path)) {
      state.left.delete(path);
      state.parts.delete(path);
      state.open.delete(path);
    }
  }
  const chosen = taken();
  const [ahead, behind] = state.sync ?? [0, 0];
  // Pull and push act on the repository open, named where the tree holds more than one.
  const repo = state.hooks.repo();
  const repos = state.hooks.repos();
  const named = repos.length > 1 ? ` ${state.hooks.repoName(repo)}` : "";
  const bar = element(
    "div",
    { className: "commit-bar" },
    toolButton("refresh", "Read the tree again", () => act("Reading the tree", async () => "")),
    toolButton("undo", state.rolling ? "Press again to roll back" : "Roll back the files taken", rollback, !chosen.length),
    element("span", { className: "commit-gap" }),
    toolButton("pull", state.sync ? `Pull${named}${behind ? `: ${behind} behind` : ""}` : `Pull${named}`, () => act("Pulling", () => invoke("git_pull", { repo })), !state.sync),
    state.sync && behind ? element("span", { className: "commit-count", textContent: String(behind) }) : null,
    toolButton("push", state.sync ? `Push${named}${ahead ? `: ${ahead} ahead` : ""}` : `Push${named}: sets the branch to follow origin`, () => act("Pushing", () => invoke("git_push", { repo }))),
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
  const list = element("div", { className: "commit-list" }, ...(repos.length > 1 ? repos.flatMap((one) => repoRows(one, paths.filter((path) => state.hooks.repoOf(path) === one.path))) : paths.flatMap((path) => rowOf(path, changes.get(path)))));
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
  const notebooks = chosen.some((path) => /\.ipynb$/i.test(path));
  const outputs = element("input", { type: "checkbox", checked: outputsLeft() });
  outputs.addEventListener("change", () => localStorage.setItem(OUTPUTS_KEY, outputs.checked ? "out" : "in"));
  const outputsRow = notebooks ? element("label", { className: "commit-option" }, outputs, element("span", { textContent: "Leave notebooks' outputs out of the commit" })) : null;
  commitButton.addEventListener("click", () => commit(false));
  pushButton.addEventListener("click", () => commit(true));
  arm();
  const buttons = element("div", { className: "commit-buttons" }, commitButton, pushButton);
  const said = state.said ? element("pre", { className: "commit-said", textContent: state.said }) : null;
  const focused = document.activeElement?.classList.contains("commit-message");
  body.replaceChildren(bar, head, list, message, ...(outputsRow ? [outputsRow] : []), buttons, ...(said ? [said] : []));
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
