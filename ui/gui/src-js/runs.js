// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Runs: every query run of the tree (src/cu/transpiler/lstar/protocol), each a .qry in a folder of runs,
// found by its file wherever it was started, from the command line or anywhere else, and live while its
// writer runs. The runs are listed every LIST_LIVE ms while one is live or the pane shows, and every
// LIST_IDLE ms while neither. A run opened in the pane is read again every READ_LIVE ms while it is
// live, its blobs coming in from its buffer as they are handed, and read by its table once it has
// ended.
//
// A panic shows the moment it lands: said on the status bar, at the head of its run, and the strip's
// Runs icon marked in the error's color. A conference, the blobs `conference_` and its question's
// identity, each space written as an underscore, shows each line `<expert>: <account>` as it is
// written, and the line that starts `overseer:` as the ruling. The lessons of each folder's
// overseer.txt, one `lesson <cause> <carrier or *> <stand-in>` a line, show under its runs.

import { invoke } from "./bridge.js";
import { paneOpen, shownGroup } from "./explorer.js";
import { say } from "./statusbar.js";

const LIST_LIVE = 1000;
const LIST_IDLE = 5000;
const READ_LIVE = 400;
// The latest blobs an open run lists; its panics and conferences show however many came before.
const SHOWN_BLOBS = 200;
const CONFERENCE = "conference_";

// Whether the pane shows: the side on the Runs group, and the pane open in it.
const shown = () => shownGroup() === "runs" && paneOpen("runs");

const state = {
  listing: { runs: [], folders: [] },
  // Each run opened in the pane, by its path: its blobs so far, where its next read goes on from, and
  // whether it was live as last read.
  open: new Map(),
  // The first panic of each run told on the status bar, by its path.
  told: new Map(),
  listedAt: 0,
  reading: 0,
};

function element(tag, props = {}, ...children) {
  const made = Object.assign(document.createElement(tag), props);
  made.append(...children.filter((child) => child !== null && child !== undefined));
  return made;
}

const nameOf = (path) => path.split("/").pop().replace(/\.qry$/, "");
const questionOf = (name) => name.slice(CONFERENCE.length).replaceAll("_", " ");

function sizeOf(bytes) {
  return bytes < 1024 ? `${bytes} B` : bytes < 1 << 20 ? `${(bytes / 1024).toFixed(1)} KB` : `${(bytes / (1 << 20)).toFixed(1)} MB`;
}

function timeOf(nanoseconds) {
  return new Date(Number(BigInt(nanoseconds) / 1000000n)).toLocaleTimeString();
}

// A conference's lines as its blobs give them, joined in the order handed: each expert's account, and
// the overseer's ruling.
export function conferenceLines(text) {
  return text
    .split(/\r?\n/)
    .filter((line) => line.trim())
    .map((line) => {
      const cut = line.indexOf(":");
      const who = cut > 0 ? line.slice(0, cut).trim() : "";
      const said = cut > 0 ? line.slice(cut + 1).trim() : line.trim();
      return { who, said, ruling: who === "overseer" };
    });
}

// The runs listed again, a panic not yet told said, and the Runs icon's marks given.
async function list() {
  try {
    state.listing = await invoke("qry_runs");
  } catch {
    state.listing = { runs: [], folders: [] };
  }
  for (const run of state.listing.runs) {
    if (run.live && run.panicked && state.told.get(run.path) !== run.panicked) {
      state.told.set(run.path, run.panicked);
      await tellPanic(run);
    }
  }
  const live = state.listing.runs.filter((run) => run.live).length;
  const panicked = state.listing.runs.filter((run) => run.live && run.panicked).length;
  window.dispatchEvent(new CustomEvent("runs-changed", { detail: { live, panicked } }));
  draw();
}

// Says a run's first panic on the status bar by its sequence. Nothing of the run is read: its text is the run's own,
// and opening the run in the pane reads it.
async function tellPanic(run) {
  say(`${nameOf(run.path)} panicked at blob ${run.panicked}`, { failed: true });
}

// A run read and held open: read whole the first time, then on from where the last read stopped while
// it is live, and whole again by its table once it has ended.
async function opening(path) {
  let held = state.open.get(path);
  if (!held) {
    held = { blobs: [], next: 0, live: true, table: false, stepped: 0 };
    state.open.set(path, held);
  }
  let read;
  try {
    read = await invoke("qry_read", { path, from: held.live ? held.next : 0 });
    // A place in a live run's schedule is no byte of its .qry: a run that ended since is read whole.
    if (!read.live && held.live && held.next) {
      read = await invoke("qry_read", { path, from: 0 });
    }
  } catch (error) {
    held.error = String(error);
    return held;
  }
  if (read.live && held.live) {
    held.blobs.push(...read.blobs);
  } else {
    held.blobs = read.blobs;
  }
  Object.assign(held, { next: read.next, live: read.live, table: read.table, stepped: read.stepped, error: null });
  return held;
}

function blobRow(blob) {
  const kind = blob.priority === 0 ? "panic" : blob.priority === 1 ? "critical" : "ordinary";
  return element(
    "div",
    { className: `runs-blob ${kind}`, title: `${blob.name}, handed ${timeOf(blob.nanoseconds)}${blob.whole === false ? ", its bytes do not hold to its row's check" : ""}` },
    element("span", { className: "runs-seq", textContent: `#${blob.sequence}` }),
    element("span", { className: "runs-name", textContent: blob.name }),
    element("span", { className: "runs-size", textContent: sizeOf(blob.size) })
  );
}

function conferenceOf(name, blobs) {
  const text = blobs.map((blob) => blob.text ?? "").join("");
  return element(
    "div",
    { className: "runs-conference" },
    element("div", { className: "runs-question", textContent: questionOf(name) }),
    ...conferenceLines(text).map((line) =>
      element("div", { className: line.ruling ? "runs-account runs-ruling" : "runs-account" }, element("span", { className: "runs-who", textContent: line.who }), element("span", { className: "runs-said", textContent: line.said }))
    )
  );
}

function detailOf(held) {
  if (held.error) {
    return element("div", { className: "runs-detail" }, element("p", { className: "runs-said-error", textContent: held.error }));
  }
  const panics = held.blobs.filter((blob) => blob.priority === 0);
  const conferences = new Map();
  for (const blob of held.blobs.filter((one) => one.name.startsWith(CONFERENCE))) {
    conferences.set(blob.name, [...(conferences.get(blob.name) ?? []), blob]);
  }
  const shown = held.blobs.slice(-SHOWN_BLOBS);
  return element(
    "div",
    { className: "runs-detail" },
    ...panics.map((blob) => element("div", { className: "runs-panic" }, element("span", { className: "runs-seq", textContent: `panic #${blob.sequence}` }), element("pre", { className: "runs-panic-text", textContent: blob.text ?? "" }))),
    ...[...conferences].map(([name, blobs]) => conferenceOf(name, blobs)),
    element("p", { className: "runs-count", textContent: `${held.blobs.length} blob${held.blobs.length === 1 ? "" : "s"}${held.live ? " so far" : held.table ? ", read by its table" : ""}${held.stepped ? `, ${held.stepped} bytes no tag opened stepped over` : ""}` }),
    element("div", { className: "runs-blobs" }, ...shown.map(blobRow))
  );
}

function runRow(run) {
  const held = state.open.get(run.path);
  const row = element(
    "button",
    { type: "button", className: `runs-run${run.live ? " live" : ""}${run.panicked ? " panicked" : ""}`, title: run.path },
    element("span", { className: "runs-run-name", textContent: nameOf(run.path) }),
    run.live ? element("span", { className: "runs-live", textContent: run.panicked ? "panicked" : "live" }) : element("span", { className: "runs-size", textContent: sizeOf(run.size) })
  );
  row.dataset.path = run.path;
  row.setAttribute("aria-expanded", String(Boolean(held)));
  row.addEventListener("click", async () => {
    if (state.open.has(run.path)) {
      state.open.delete(run.path);
      draw();
      return;
    }
    await opening(run.path);
    draw();
  });
  return element("div", { className: "runs-item" }, row, held ? detailOf(held) : null);
}

function lessonsOf(folder) {
  if (!folder.lessons.length) {
    return null;
  }
  return element(
    "div",
    { className: "runs-lessons" },
    element("div", { className: "runs-lessons-head", textContent: "Lessons" }),
    ...folder.lessons.map((lesson) =>
      element("div", { className: "runs-lesson" }, element("span", { className: "runs-cause", textContent: lesson.cause }), element("span", { className: "runs-carrier", textContent: lesson.carrier }), element("span", { className: "runs-stand-in", textContent: lesson.stand_in }))
    )
  );
}

export function draw() {
  const host = document.getElementById("runs");
  if (!host || !shown()) {
    return;
  }
  const { runs, folders } = state.listing;
  if (!runs.length) {
    host.replaceChildren(element("p", { className: "runs-empty", textContent: "No query runs in this tree. A run's .qry shows here from the moment its writer makes it, wherever its script was started." }));
    return;
  }
  host.replaceChildren(
    ...folders.map((folder) =>
      element(
        "section",
        { className: "runs-folder" },
        element("div", { className: "runs-folder-head", textContent: folder.folder || "." }),
        ...runs.filter((run) => run.folder === folder.folder).map(runRow),
        lessonsOf(folder)
      )
    )
  );
}

// Lists the tree's runs on a beat, more often while one is live or the pane shows, and while the pane shows reads
// each run open in it that was live as last read on from where that read stopped, or whole by its table once it
// has ended.
export function startRuns() {
  const beat = async () => {
    const often = shown() || state.listing.runs.some((run) => run.live);
    if (performance.now() - state.listedAt >= (often ? LIST_LIVE : LIST_IDLE)) {
      state.listedAt = performance.now();
      await list();
    }
    const live = shown() ? [...state.open].filter(([, held]) => held.live).map(([path]) => path) : [];
    for (const path of live) {
      await opening(path);
    }
    if (live.length) {
      draw();
    }
    window.setTimeout(beat, READ_LIVE);
  };
  window.setTimeout(beat, 0);
  window.addEventListener("panes-changed", () => {
    if (shown()) {
      state.listedAt = 0;
    }
    draw();
  });
}

// The runs of a tree no longer shown are let go.
export function forgetRuns() {
  state.open.clear();
  state.told.clear();
  state.listing = { runs: [], folders: [] };
  state.listedAt = 0;
}
