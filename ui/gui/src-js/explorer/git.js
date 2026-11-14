// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { authorsShown } from "../authors.js";
import { invoke } from "../bridge.js";
import { escapeHtml } from "../editor/view.js";
import { copyText } from "../menu.js";
import { act, element, guides, paneOpen, state } from "../explorer.js";
import { iconOf } from "./icons.js";
import { day, time } from "./history.js";

// Git: the branch the tree is on, and every branch's commits as a graph, the newest first, each with
// the branches and tags at it, its subject and its date, and its author where Git, Authors is on. A
// press on a commit opens the files it changed under it, and a press on one of those sets the file as
// the commit left it beside the file as the commit before it did. The field over the graph searches
// the whole history, by subject, id or branch, and by author where authors show, and shows what it
// finds without the graph. Where the tree holds more
// than one repository, a choice over the field says whose commits the graph shows, the repository
// open's until another is chosen.

// A column of the graph is this wide, and its row this high.
const LANE = 12;
const ROW = 22;

const laneX = (lane) => LANE / 2 + lane * LANE;

const tone = (line) => `var(--t${[4, 2, 5, 3, 6, 1][line % 6]})`;

// A row's strokes and its commit's dot, as an SVG the width of the columns it crosses.
function lanesOf(commit) {
  const across = Math.max(commit.lane, ...commit.lines.flatMap(([from, to]) => [from, to])) + 1;
  const mid = ROW / 2;
  let paths = "";
  for (const [from, to, half, line] of commit.lines) {
    const [top, bottom] = half ? [mid, ROW] : [0, mid];
    const [x1, x2] = [laneX(from), laneX(to)];
    const bend = (bottom - top) / 2;
    const d = x1 === x2 ? `M${x1} ${top}V${bottom}` : `M${x1} ${top}C${x1} ${top + bend} ${x2} ${bottom - bend} ${x2} ${bottom}`;
    paths += `<path d="${d}" style="stroke:${tone(line)}"/>`;
  }
  const merge = commit.parents.length > 1;
  const dot = `<circle cx="${laneX(commit.lane)}" cy="${mid}" r="${merge ? 3 : 3.5}" style="${merge ? `fill:var(--surface);stroke:${tone(commit.color)}` : `fill:${tone(commit.color)};stroke:none`}"/>`;
  return `<svg class="git-lanes" width="${across * LANE}" height="${ROW}" aria-hidden="true">${paths}${dot}</svg>`;
}

function commitRow(commit) {
  const open = state.git.open.has(commit.id);
  const refs = commit.refs
    .map((ref) => {
      const head = ref.startsWith("HEAD -> ");
      const name = head ? ref.slice(8) : ref;
      return `<span class="git-ref${head || ref === "HEAD" ? " git-head" : ""}${name.startsWith("tag: ") ? " git-tag" : ""}">${escapeHtml(name.replace(/^tag: /, ""))}</span>`;
    })
    .join("");
  const authors = authorsShown();
  const title = `${commit.id.slice(0, 8)}  ${authors ? `${commit.author}  ` : ""}${day(commit.when)} ${time(commit.when)}${commit.refs.length ? `\n${commit.refs.join(", ")}` : ""}\n${commit.subject}`;
  const author = authors ? `<span class="git-author">${escapeHtml(commit.author)}</span>` : "";
  return `<button class="commit git-commit" type="button" data-key="${commit.id}" data-depth="0" aria-expanded="${open}" title="${escapeHtml(title)}">${lanesOf(commit)}${refs}<span class="name">${escapeHtml(commit.subject)}</span>${author}<span class="where">${day(commit.when)}</span></button>`;
}

// The rows of the files a commit changed, under its row.
function touchedRows(commit) {
  const files = state.git.files.get(commit.id) ?? [];
  if (!files.length) {
    return [element("p", { className: "pane-empty git-none", textContent: "No file under the tree changed." })];
  }
  return files.map((file) => {
    const cut = file.path.lastIndexOf("/");
    const name = file.path.slice(cut + 1);
    const row = element("button", { className: "node git-file", type: "button", title: file.was ? `${file.was} → ${file.path}` : file.path });
    row.dataset.key = `${commit.id}:${file.path}`;
    row.dataset.depth = "1";
    row.dataset.change = file.state;
    row.append(...guides(1), iconOf(name), element("span", { className: "name", textContent: name }), element("span", { className: "where", textContent: file.path.slice(0, Math.max(0, cut)) }), element("span", { className: "change", textContent: file.state }));
    row.addEventListener("click", () => state.hooks.openTouched(commit, file));
    return row;
  });
}

export async function toggleCommit(row) {
  const commit = state.git.commits.find((one) => one.id === row.dataset.key);
  if (!commit) {
    return;
  }
  if (state.git.open.has(commit.id)) {
    state.git.open.delete(commit.id);
    row.setAttribute("aria-expanded", "false");
    while (row.nextElementSibling && !row.nextElementSibling.classList.contains("git-commit")) {
      row.nextElementSibling.remove();
    }
    return;
  }
  state.git.open.add(commit.id);
  row.setAttribute("aria-expanded", "true");
  if (!state.git.files.has(commit.id)) {
    state.git.files.set(commit.id, await invoke("git_touched", { id: commit.id, repo: gitRepo() }).catch(() => []));
  }
  if (state.git.open.has(commit.id) && row.isConnected) {
    row.after(...touchedRows(commit));
  }
}

// Forgets the graph's search, its choice of repository and the commits whose files were read, for a
// tree opened in place of this one.
export function forgetGraph() {
  Object.assign(state.git, { query: "", commits: [], open: new Set(), files: new Map(), drawn: "", chosen: null });
  document.getElementById("git-query").value = "";
}

// The repository the graph shows: the one chosen over it, or the repository open where none is
// chosen or the one chosen is gone.
export function gitRepo() {
  const repos = state.hooks?.repos() ?? [];
  return repos.some((repo) => repo.path === state.git.chosen) ? state.git.chosen : (state.hooks?.repo() ?? "");
}

// The choice of repository over the graph, there while the tree holds more than one.
function drawRepoChoice(repo) {
  const choice = document.getElementById("git-repo");
  const repos = state.hooks?.repos() ?? [];
  choice.hidden = repos.length < 2;
  const key = repos.map((one) => `${one.path}:${one.branch}`).join();
  if (choice.dataset.key !== key) {
    choice.dataset.key = key;
    choice.replaceChildren(...repos.map((one) => element("option", { value: one.path, textContent: `${state.hooks.repoName(one.path)}${one.branch ? `: ${one.branch}` : ""}` })));
  }
  choice.value = repo;
}

export async function drawGit(branch) {
  const body = document.getElementById("git-log");
  state.git.branch = branch;
  if (!paneOpen("git") || state.group !== "git") {
    return;
  }
  const query = state.git.query;
  const repo = gitRepo();
  drawRepoChoice(repo);
  const label = (state.hooks?.repos() ?? []).find((one) => one.path === repo)?.branch ?? branch;
  const authors = authorsShown();
  const commits = await invoke("git_graph", { query: query || null, repo, authors }).catch(() => []);
  if (query !== state.git.query || repo !== gitRepo() || authors !== authorsShown()) {
    return;
  }
  // A graph that is drawn as it is, is left as it is, the focus and the scroll with it.
  const drawn = `${repo}\n${label}\n${query}\n${authors}\n${commits.map((one) => `${one.id}${one.refs.join()}`).join()}`;
  if (drawn === state.git.drawn && body.childElementCount) {
    return;
  }
  state.git.drawn = drawn;
  state.git.commits = commits;
  const count = `${commits.length} commit${commits.length === 1 ? "" : "s"} ${query ? "found" : "displayed"}`;
  const focused = body.contains(document.activeElement) ? document.activeElement.dataset.key : null;
  body.innerHTML = `<div class="node git-branch"><span class="name">${escapeHtml(label ?? "no branch")}</span><span class="where">${count}</span></div>${commits.map(commitRow).join("")}`;
  for (const id of state.git.open) {
    const row = body.querySelector(`.git-commit[data-key="${id}"]`);
    const commit = commits.find((one) => one.id === id);
    if (row && state.git.files.has(id)) {
      row.after(...touchedRows(commit));
    }
  }
  if (focused) {
    body.querySelector(`[data-key="${CSS.escape(focused)}"]`)?.focus();
  }
}

export function gitItems(event) {
  const row = event.target.closest(".git-commit, .git-file");
  const commit = state.git.commits.find((one) => one.id === (row?.closest(".git-commit") ? row.dataset.key : row?.dataset.key.split(":")[0]));
  if (!commit) {
    return null;
  }
  if (row.classList.contains("git-file")) {
    const file = state.git.files.get(commit.id)?.find((one) => `${commit.id}:${one.path}` === row.dataset.key);
    return [
      act("commit-file-changes", [commit.id, file.path], { label: "Show Changes" }),
      act("commit-open", [file.path, commit.id], { label: "Open as It Was", disabled: file?.state === "D" }),
      "-",
      { label: "Copy Path", command: "file-copy-path", args: [file.path], run: () => copyText(file.path) },
    ];
  }
  return [
    { label: state.git.open.has(commit.id) ? "Close Files" : "Open Files", command: "commit-files", args: [commit.id], run: () => toggleCommit(row) },
    "-",
    { label: "Cherry-Pick", command: "cherry-pick", args: [commit.id], run: () => import("../menubar.js").then((menus) => menus.cherryPick(commit.id, gitRepo())) },
    "-",
    act("commit-copy-id", [commit.id], { label: "Copy Commit ID" }),
    act("commit-copy-message", [commit.id], { label: "Copy Commit Message" }),
  ];
}
