// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { invoke } from "../bridge.js";
import { drawGit, drawReview, folderIcon, guides, iconOf } from "../explorer.js";
import { drawCommit } from "../commit.js";
import { changed } from "../servers.js";
import { RowList } from "../rowlist.js";
import { drawBranch } from "../statusbar.js";
import { element, ROLL_ORDER, state } from "../edit.js";
import { openFile } from "./open.js";
import { readKept, treeKey } from "./places.js";
import { closeTab, fileOf } from "./tabs.js";

// The tree, a folder at a time as each is opened.

export async function childrenOf(dir) {
  if (!state.children.has(dir)) {
    state.children.set(dir, await invoke("tree_list", { dir }));
  }
  return state.children.get(dir);
}

// The repositories of the tree, read as a tree opens and again as git changes: each one's path in the
// tree, empty for the one the tree is in, and the branch it is on.
const repoPaths = () => (state.repos ?? []).map((repo) => repo.path);

// The repository that holds a path of the tree: the deepest whose folder holds it.
export function repoOf(path) {
  const found = repoPaths().filter((repo) => repo === "" || path === repo || path.startsWith(`${repo}/`));
  return found.length ? found.reduce((deepest, repo) => (repo.length > deepest.length ? repo : deepest)) : null;
}

// The repository open, the one the Git menu, the branch on the status bar and the Commit window's
// pull and push act on: the one that holds the file displayed, or the tree's own, or the first.
export function repoOpen() {
  const file = state.active ? fileOf(state.active) : null;
  const paths = repoPaths();
  return (file && repoOf(file)) ?? (paths.includes("") ? "" : (paths[0] ?? ""));
}

// A repository's name as the server gives it, its `origin` remote's; its folder's where the server has not
// listed it yet.
export function repoName(repo) {
  const listed = (state.repos ?? []).find((one) => one.path === (repo ?? ""))?.name;
  return listed || (repo || document.getElementById("tree-path").textContent.replace(/\\/g, "/")).split("/").filter(Boolean).pop() || "";
}

// The branch on the status bar: the repository open's, named with it where the tree holds more than
// one.
export function drawRepoBranch() {
  const repo = repoOpen();
  const branch = (state.repos ?? []).find((one) => one.path === repo)?.branch ?? state.branch;
  const changed = [...state.changes.keys()].some((path) => repoOf(path) === repo);
  drawBranch(branch && (state.repos?.length ?? 0) > 1 ? `${repoName(repo)}: ${branch}` : branch, changed);
}

// Reads what git says of the tree again: each changed file's state, and each folder's from the files
// under it.
// Another tree opened while the changes are read lets go of the repositories; the changes read are
// those of the tree the read began in, and the repositories are read again on the next.
export async function loadChanges() {
  if (!state.repos) {
    state.repos = await invoke("git_repositories").catch(() => []);
    // the bar names the tree by its own repository's name where the tree is in one; the folder's is
    // where it is in none
    const own = state.repos.find((one) => one.path === "")?.name;
    if (own) {
      document.getElementById("bar-project").textContent = own;
    }
  }
  const [changed, branch] = await Promise.all([invoke("tree_changed", { repos: repoPaths() }).catch(() => []), invoke("tree_branch", { repo: repoOpen() }).catch(() => null)]);
  state.changes = new Map((changed ?? []).map(({ path, state: mark }) => [path, mark]));
  state.branch = branch;
  const open = state.repos?.find((one) => one.path === repoOpen());
  if (open) {
    open.branch = branch;
  }
  drawCommit(state.changes);
  drawGit(branch);
  drawReview();
  state.rolled = new Map();
  for (const [path, mark] of state.changes) {
    let at = path.lastIndexOf("/");
    while (at > 0) {
      const folder = path.slice(0, at);
      const found = state.rolled.get(folder);
      if (!found || ROLL_ORDER.indexOf(mark) < ROLL_ORDER.indexOf(found)) {
        state.rolled.set(folder, mark);
      }
      at = folder.lastIndexOf("/");
    }
  }
  drawRepoBranch();
}

// Draws the tree from the folders read, and ends once the latest draw has put its rows in. A draw that
// a later one overtakes while it reads puts nothing in. The rows are what each is for, and a row
// list puts on the page only those in sight and those the scroll heads into. A folder of any size
// opens and scrolls at once, and the row that holds the keys retains them.
export function drawTree() {
  state.drawn = drawTreeOnce(state.drawing + 1);
  return state.drawn;
}

async function drawTreeOnce(drawing) {
  const list = document.getElementById("files");
  state.drawing = drawing;
  state.rowList ??= new RowList(list, { height: TREE_ROW, gap: MOUNT_GAP, build: (row) => treeRowOf(row) });
  const put = (rows) => {
    if (drawing !== state.drawing) {
      return state.drawn;
    }
    state.rowList.set(rows, rows.findIndex((row) => row.mountFirst));
    return null;
  };
  const top = document.getElementById("tree-path").textContent.replace(/\\/g, "/");
  document.querySelector('.pane[data-pane="folder"] .pane-head').textContent = top.split("/").filter(Boolean).pop() ?? "";
  const query = document.getElementById("file-filter").value.trim();
  if (query) {
    const found = await invoke("tree_find", { query });
    await put(found.map((path) => treeRow({ name: path, path, dir: false, ignored: false }, 0)));
    return;
  }
  const rows = [];
  const walk = async (dir, depth) => {
    for (const entry of await childrenOf(dir)) {
      rows.push(treeRow(entry, depth));
      if (entry.dir && state.expanded.has(entry.path)) {
        await walk(entry.path, depth + 1);
      }
    }
  };
  await walk("", 0);
  // Each folder or file from another place is after the tree's own, under its own name.
  for (const [at, mounted] of state.mounts.entries()) {
    const path = `@${mounted.name}`;
    rows.push({ ...treeRow({ name: mounted.name, path, dir: mounted.dir, ignored: false }, 0), mount: mounted.path, mountFirst: at === 0 });
    if (mounted.dir && state.expanded.has(path)) {
      await walk(path, 1);
    }
  }
  await put(rows);
}

// The height of a row of the tree, and the room above the first folder or file from another place.
const TREE_ROW = 22;
const MOUNT_GAP = 4;

// A row of the tree as the row list holds it: its key, depth, name and whether it is open, which the
// list's keys read, and the entry it is for.
function treeRow(entry, depth) {
  return { key: entry.path, depth, name: displayedName(entry), open: entry.dir ? state.expanded.has(entry.path) : null, entry };
}

// A row of the tree's element, as it comes into sight.
function treeRowOf(row) {
  const button = node(row.entry, row.depth);
  if (row.mount) {
    button.classList.add("mount");
    button.classList.toggle("mount-first", row.mountFirst);
    button.title = row.mount;
  }
  return button;
}

// The element of the tree's row for `path`, put on the page and scrolled into sight, or null where
// the tree shows no row for it.
export const treeRowShown = (path) => state.rowList?.reveal(path) ?? null;

// More than one tree in a window: folders from other places, and single files from them, mounted
// beside the tree, each with its own place in the explorer and retained with the tree.

const mountsKey = () => `orior.mounts.${treeKey()}`;

async function readMounts() {
  state.mounts = await invoke("tree_mounts").catch(() => []);
  localStorage.setItem(mountsKey(), JSON.stringify(state.mounts.map((mounted) => mounted.path)));
}

// Mounts again the folders and files retained with the tree, as it opens.
export async function loadMounts() {
  const retained = readKept(mountsKey(), []);
  for (const path of Array.isArray(retained) ? retained : []) {
    await invoke("tree_mount", { path }).catch(() => {});
  }
  state.mounts = await invoke("tree_mounts").catch(() => []);
}

// File, Add Folder to Window and Add File to Window: the folder or file at `path` mounted beside the
// tree, and its name given.
export async function addToWindow(path) {
  const name = await invoke("tree_mount", { path });
  await readMounts();
  state.expanded.add(`@${name}`);
  await drawTree();
  return name;
}

// Takes a mount out of the window, closing its files' tabs.
export async function removeFromWindow(name) {
  const prefix = `@${name}`;
  for (const tab of state.tabs.filter((one) => one.path === prefix || one.path.startsWith(`${prefix}/`))) {
    tab.closing = true;
    closeTab(tab);
  }
  await invoke("tree_unmount", { name });
  await readMounts();
  state.children.clear();
  await drawTree();
}

// The encodings an archive's names may be read in, each by its label and how the menu names it, the
// first the archive's own word: UTF-8 where its flag says so, the old DOS page elsewhere.
export const NAME_ENCODINGS = [
  ["", "As the Archive Says"],
  ["utf-8", "UTF-8"],
  ["windows-1252", "Western (Windows-1252)"],
  ["iso-8859-2", "Central European (ISO-8859-2)"],
  ["windows-1251", "Cyrillic (Windows-1251)"],
  ["koi8-r", "Cyrillic (KOI8-R)"],
  ["shift_jis", "Japanese (Shift_JIS)"],
  ["euc-jp", "Japanese (EUC-JP)"],
  ["gbk", "Chinese, Simplified (GBK)"],
  ["big5", "Chinese, Traditional (Big5)"],
  ["euc-kr", "Korean (EUC-KR)"],
];

const NAMES_KEY = "orior.archive.names";

// The encoding chosen for each archive's names, by the archive's path.
export function namesChosen() {
  try {
    return JSON.parse(localStorage.getItem(NAMES_KEY) ?? "{}");
  } catch {
    return {};
  }
}

// Reads the names of the archive at `path` in `label`, or as the archive says where it is empty.
export async function chooseNames(path, label) {
  const chosen = namesChosen();
  if (label) {
    chosen[path] = label;
  } else {
    delete chosen[path];
  }
  localStorage.setItem(NAMES_KEY, JSON.stringify(chosen));
  await drawTree();
}

// The name an entry shows: its name's own bytes read in the encoding chosen for the archive it is
// in, where the archive does not say its names are UTF-8 and the reader chose one.
function displayedName(entry) {
  if (!entry.raw) {
    return entry.name;
  }
  const chosen = namesChosen();
  const archive = Object.keys(chosen).find((path) => entry.path.startsWith(`${path}/`));
  if (!archive) {
    return entry.name;
  }
  try {
    return new TextDecoder(chosen[archive]).decode(Uint8Array.from(entry.raw.match(/../g).map((pair) => parseInt(pair, 16))));
  } catch {
    return entry.name;
  }
}

// A row of the tree: a line down from each folder above it, the folder's arrow, the file's icon or
// the folder's, the name, and git's character for it. An archive is a folder with its file's icon.
function node(entry, depth) {
  const known = !entry.dir && state.known?.typeOf(entry.path);
  const mark = entry.dir ? state.rolled.get(entry.path) : state.changes.get(entry.path);
  const button = element("button", {
    className: `node${entry.dir ? " dir" : ""}${known ? " known" : ""}${entry.ignored ? " ignored" : ""}`,
    type: "button",
    title: entry.path,
  });
  button.append(...guides(depth), element("span", { className: "twisty" }));
  button.append(entry.dir && !entry.archive ? folderIcon() : iconOf(entry.name, Boolean(known)));
  button.append(element("span", { className: "name", textContent: displayedName(entry) }));
  if (entry.archive) {
    button.dataset.archive = "true";
  }
  if (mark) {
    button.dataset.change = mark;
    button.append(element("span", { className: "change", textContent: entry.dir ? "●" : mark }));
  }
  button.dataset.key = entry.path;
  button.dataset.depth = String(depth);
  if (entry.dir) {
    button.setAttribute("aria-expanded", String(state.expanded.has(entry.path)));
  }
  if (state.active === entry.path) {
    button.setAttribute("aria-current", "true");
  }
  button.addEventListener("click", async () => {
    if (entry.dir) {
      if (state.expanded.has(entry.path)) {
        state.expanded.delete(entry.path);
      } else {
        state.expanded.add(entry.path);
      }
      await drawTree();
    } else {
      await openFile(entry.path);
    }
  });
  return button;
}

