// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { invoke, pick } from "../bridge.js";
import { say } from "../statusbar.js";
import { editing } from "../edit.js";
import { showMenu } from "../menu.js";
import { subject } from "../run.js";
import { showView } from "../views.js";
import { runCommand } from "../menubar.js";
import { askFor, askYes } from "./sheets.js";

// Git, and the branch on the status bar: what git is asked to do said on the status bar, git's own
// words where it refuses, and the tree's changes read again after.
export async function gitSays(doing, act) {
  say(`${doing}…`);
  try {
    const said = await act();
    say(String(said ?? "").split("\n").find((line) => line.trim()) ?? `${doing} done.`);
  } catch (error) {
    say(String(error), { failed: true });
    return false;
  }
  await editing().treeChanged();
  return true;
}

export async function newBranch(given) {
  const name = given || (await askFor("New branch, made from the branch open and gone on to", ""));
  if (name) {
    gitSays(`Making ${name}`, () => invoke("git_branch", { act: "create", name, repo: editing().repo() }));
  }
}

// Every local branch, then every remote one, the tree's own checked: each to go on to, to merge into
// the branch open or to rebase it onto, and a local one to rename or delete.
// A branch switched to, merged in, rebased onto, renamed or deleted, as Git, Branches does it.
export const branchDo = (act, name, to) => gitSays(`${act[0].toUpperCase()}${act.slice(1)} ${name}`, () => invoke("git_branch", { act, name, to, repo: editing().repo() }));

export async function removeBranch(name) {
  try {
    say(await invoke("git_branch", { act: "delete", name, repo: editing().repo() }));
    await editing().treeChanged();
  } catch (error) {
    if (/not fully merged/.test(String(error)) && (await askYes(`${name} has commits no other branch holds. Delete it and them?`, "Delete"))) {
      branchDo("delete-unmerged", name);
    } else if (!/not fully merged/.test(String(error))) {
      say(String(error), { failed: true });
    }
  }
}

export async function renameBranch(name, given) {
  const to = given ?? (await askFor(`New name for ${name}`, name));
  if (to && to !== name) {
    await branchDo("rename", name, to);
  }
}

// An item of a list that runs `command` with `args`, as the command line runs it, and says both.
const listed = (command, args, item) => ({ ...item, command, args, run: () => runCommand(command, args) });

export async function showBranches() {
  const list = await invoke("git_branches", { repo: editing().repo() }).catch(() => []);
  const open = list.find((one) => one.current)?.name ?? "HEAD";
  const itemOf = (branch) => {
    const items = [];
    if (!branch.current) {
      items.push(
        listed("branch-switch", [branch.name], { label: "Switch" }),
        listed("branch-merge", [branch.name], { label: `Merge into ${open}` }),
        listed("branch-rebase", [branch.name], { label: `Rebase ${open} onto it` }),
      );
    }
    if (!branch.remote) {
      items.push(listed("branch-rename", [branch.name], { label: "Rename…" }));
      if (!branch.current) {
        items.push(listed("branch-delete", [branch.name], { label: "Delete" }));
      }
    }
    return { label: branch.name, checked: branch.current, items };
  };
  const local = list.filter((one) => !one.remote).map(itemOf);
  const remote = list.filter((one) => one.remote).map(itemOf);
  const items = [listed("new-branch", [], { label: "New Branch…" }), ...(local.length ? ["-", ...local] : []), ...(remote.length ? ["-", ...remote] : [])];
  const anchor = document.getElementById("status-branch");
  const box = anchor.hidden ? { left: window.innerWidth / 3, top: window.innerHeight / 3 } : anchor.getBoundingClientRect();
  showMenu(box.left, (box.bottom ?? box.top) + 2, items, { anchor: anchor.hidden ? null : anchor });
}

// Git, Stash Changes: every change put aside, new files with them, under the message given or asked
// for.
export async function stashChanges(given) {
  const message = given || (await askFor("A message for the changes put aside", ""));
  if (message) {
    gitSays("Stashing", () => invoke("git_stash", { act: "push", message, repo: editing().repo() }));
  }
}

// A stash brought back and kept, brought back and dropped, or thrown away.
export const stashDo = (act, name) => gitSays(`${act[0].toUpperCase()}${act.slice(1)} ${name}`, () => invoke("git_stash", { act, name, repo: editing().repo() }));

// Git, Stashes: each stash, the newest first, to bring back and keep, bring back and drop, or throw
// away.
export async function showStashes() {
  const list = await invoke("git_stashes", { repo: editing().repo() }).catch(() => []);
  if (!list.length) {
    say("No changes are put aside.");
    return;
  }
  const items = list.map((stash) => ({
    label: `${stash.name}  ${stash.subject}`,
    items: [listed("stash-apply", [stash.name], { label: "Apply" }), listed("stash-pop", [stash.name], { label: "Pop" }), listed("stash-drop", [stash.name], { label: "Drop" })],
  }));
  showMenu(window.innerWidth / 3, window.innerHeight / 4, [listed("stash", [], { label: "Stash Changes…" }), "-", ...items]);
}

// Git, Cherry-Pick: the commit named, or one chosen from the commits of other branches the branch open
// does not hold, applied to the branch open as a commit of its own, in `repo`, the repository open
// where none is named.
export async function cherryPick(given, repo = editing().repo()) {
  const pick = (id, subject) => gitSays(`Cherry-picking ${subject ?? id.slice(0, 7)}`, () => invoke("git_cherry_pick", { id, repo }));
  if (given) {
    pick(given);
    return;
  }
  const list = await invoke("git_elsewhere", { repo }).catch(() => []);
  if (!list.length) {
    say("The branch open holds every commit of the other branches.");
    return;
  }
  showMenu(window.innerWidth / 3, window.innerHeight / 4, list.map((commit) => ({ label: `${commit.id.slice(0, 7)}  ${commit.subject}`, command: "cherry-pick", args: [commit.id], run: () => pick(commit.id, commit.subject) })));
}

// Git, Resolve Conflicts: the file named, or a file a merge left in conflict where it is alone in
// that, in the merge window, or a list of them to choose from where there are more.
export function resolveConflicts(given) {
  const files = editing().conflicted();
  if (given || files.length === 1) {
    showView("edit");
    editing().openMerge(given || files[0]);
  } else if (!files.length) {
    say("No file is in conflict.");
  } else {
    showMenu(window.innerWidth / 3, window.innerHeight / 4, files.map((path) => ({ label: path, command: "conflicts", args: [path], run: () => (showView("edit"), editing().openMerge(path)) })));
  }
}

