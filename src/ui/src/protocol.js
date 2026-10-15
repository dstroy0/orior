// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The protocol panel's reading of a run: the course of a pass, known before it starts, as a line of
// labeled nodes with the fuse between them, and the fire walking the line as the run's writer
// places the blobs that say each node is reached. The whole line stands from the first moment;
// nothing on it appears or moves as blobs come in. Nothing here touches the page: the run page
// draws what this reads.
//
// A blob is `{ sequence, nanoseconds, priority, member, name, size }` as the server reads it off the
// tap, the priority 0 a panic, 1 critical, 2 ordinary. The reading is a fold: `panelStart()` is the
// reading of no blobs, `panelStep(reading, blob)` takes one more as it arrives, and `panelRead` gives
// the nodes and where the fire stands.

export const PANIC = 0;

// The most questions one round carries, as the scheduler puts them (scheduler.h).
export const ROUND_MOST = 8192;

// The most rounds the line shows one node each; a plan of more rounds shows them as one stretch of the
// line, its node named for the round under way out of all of them.
export const ROUNDS_SHOWN = 12;

// The nodes of a pass's line, in order. The rounds between Plan and Written are one node a round
// once the plan says how many: a costs pass puts one form a round, every other pass ROUND_MOST
// questions a round. Before the plan is known the rounds are one node named for the pass.
export function nodesOf(pass, plan, underWay = 1) {
  const nodes = [
    { key: "tools", label: "Tools" },
    { key: "run", label: "Run" },
    { key: "carried", label: "Carried in" },
    { key: "plan", label: "Plan" },
  ];
  const rounds = roundsOf(plan);
  if (plan && rounds > ROUNDS_SHOWN) {
    const round = Math.max(1, Math.min(rounds, underWay));
    nodes.push({ key: `round${round}`, label: `Round ${round} of ${rounds}`, round, rounds });
  } else if (plan) {
    for (let round = 1; round <= rounds; round += 1) {
      nodes.push({ key: `round${round}`, label: `Round ${round}`, round });
    }
  } else {
    nodes.push({ key: "rounds", label: pass || "Rounds", round: 1 });
  }
  nodes.push({ key: "written", label: "Written" }, { key: "leave", label: "Leave" });
  return nodes;
}

// How many rounds a plan takes: a costs pass puts one form a round, every other pass ROUND_MOST questions a round.
export function roundsOf(plan) {
  if (!plan) {
    return 1;
  }
  return plan.pass === "costs" ? plan.questions : Math.max(1, Math.ceil(plan.questions / ROUND_MOST));
}

// A question's answer: `answers<n>.txt`, the carrier's answer to question n of the round.
const ANSWER = /^answers(\d+)\.txt$/;

// A line of a round's list is about this many bytes: a round's count of questions is read off the
// list's size until its answers say it exactly.
const LIST_LINE = 64;

// The plan the teacher hands as a pass starts, `<pass> <questions>`, read off the text of `plan.txt`
// or off the teacher's console line `plan: <pass> <questions>`.
export function planOf(text) {
  const words = String(text ?? "").trim().replace(/^plan:\s*/, "").split(/\s+/);
  const questions = Number(words[1]);
  return words.length === 2 && Number.isFinite(questions) && questions > 0 ? { pass: words[0], questions } : null;
}

// The reading of no blobs.
export function panelStart() {
  return { carried: false, rounds: 0, asked: 0, answered: 0, written: false, panic: null, blobs: new Map() };
}

// The most blobs a node keeps for showing; past that the count goes on and the first ones stand.
export const KEPT_BLOBS = 4096;

// The node a blob belongs to, by where the reading stands as it is placed: a file written is Written's,
// a blob of a round is that round's, one before the plan's first round is Plan's where a round has been put
// and Carried in's before.
function nodeOfBlob(reading, blob, plan) {
  if (/\.(ksc|khw|krs|kqr)$/.test(blob.name)) {
    return "written";
  }
  if (plan && reading.rounds) {
    return `round${reading.rounds}`;
  }
  return reading.rounds || blob.name === "questions.txt" ? "plan" : "carried";
}

// The reading with one more blob taken, the one the writer placed next. The rounds counted are the
// rounds put after the plan: a round put before it is the first round's, the kernel's and the slot's.
export function panelStep(reading, blob, { plan = null } = {}) {
  if (blob.priority === PANIC && !reading.panic) {
    reading.panic = blob;
  }
  if (blob.name === "forms_work.txt" || blob.name === "faulted.txt" || blob.name === "overseer.txt") {
    reading.carried = true;
  }
  if (blob.name === "questions.txt" && plan) {
    reading.rounds += 1;
    reading.asked = Math.max(1, Math.round(blob.size / LIST_LINE));
    reading.answered = 0;
  }
  const answer = ANSWER.exec(blob.name);
  if (answer && plan) {
    reading.answered += 1;
    reading.asked = Math.max(reading.asked, Number(answer[1]) + 1);
  }
  if (/\.(ksc|khw|krs|kqr)$/.test(blob.name)) {
    reading.written = true;
  }
  const node = nodeOfBlob(reading, blob, plan);
  const kept = reading.blobs.get(node) ?? { count: 0, list: [] };
  kept.count += 1;
  if (kept.list.length < KEPT_BLOBS) {
    kept.list.push(blob);
  }
  reading.blobs.set(node, kept);
  return reading;
}

// What the panel shows of a reading: `nodes` the line, each node with whether it is reached; `at`
// the index of the furthest node reached; `place` how far the fire has burnt, 0 to 1, the node
// reached plus the current round's answered over asked toward the next; `panic` the first panic's
// blob or null, each node with the count of blobs it keeps. `tools` says the tools were built, `started` that the run's tap was named, `plan`
// the plan where it has come, `ended` that the run ended.
export function panelRead(reading, { pass = "", plan = null, tools = false, started = false, ended = false } = {}) {
  const nodes = nodesOf(pass, plan, Math.max(1, reading.rounds));
  const reached = (node) => {
    switch (node.key) {
      case "tools":
        return tools;
      case "run":
        return started;
      case "carried":
        return reading.carried;
      case "plan":
        return Boolean(plan);
      case "written":
        return reading.written || ended;
      case "leave":
        return ended;
      default:
        return Boolean(plan) && reading.rounds >= node.round;
    }
  };
  let at = -1;
  const shown = nodes.map((node, index) => {
    // a stretch of rounds is reached once its first round is, and its node is the round under way
    const is = node.rounds ? Boolean(plan) && reading.rounds >= 1 : reached(node);
    at = is ? index : at;
    const kept = reading.blobs.get(node.key);
    return { key: node.key, label: node.label, reached: is, blobs: kept?.count ?? 0, round: node.round ?? null, rounds: node.rounds ?? null };
  });
  const last = nodes.length - 1;
  const current = nodes[at + 1];
  const answered = reading.asked ? Math.min(1, reading.answered / reading.asked) : 0;
  let fraction = current?.round && reading.rounds === current.round - 1 ? answered : 0;
  const stretch = nodes[at];
  if (stretch?.rounds) {
    // along a stretch the fire stands at the rounds done over all of them, the round under way part way
    const done = reading.rounds - 1 + (reading.rounds <= stretch.rounds ? answered : 1);
    fraction = Math.max(0, Math.min(1, done / stretch.rounds));
  }
  const place = ended ? 1 : Math.max(0, Math.min(1, (at + fraction) / last));
  return { nodes: shown, at, place, round: { rounds: reading.rounds, asked: reading.asked, answered: reading.answered }, panic: reading.panic };
}

// The reading folded again now that `plan` is known: the blobs placed before the plan's line came are
// read as the plan reads them. The blobs are those the nodes kept, in the order placed; a node that kept
// fewer than it saw is read from those it kept.
export function panelReplanned(reading, plan) {
  const blobs = [...reading.blobs.values()].flatMap((kept) => kept.list).sort((one, two) => one.sequence - two.sequence);
  return blobs.reduce((read, blob) => panelStep(read, blob, { plan }), panelStart());
}

// The blobs a node keeps, in the order placed, and how many it saw in all.
export function blobsOfNode(reading, key) {
  const kept = reading.blobs.get(key);
  return kept ? { count: kept.count, list: kept.list } : { count: 0, list: [] };
}

// What the panel shows of `blobs` read whole, as the fold of the three above.
export function panelOf(blobs, shown = {}) {
  return panelRead(blobs.reduce((reading, blob) => panelStep(reading, blob, shown), panelStart()), shown);
}

// A protocol command's words from the command line read into a job's values, as the job's fields would hold them:
// `--<key> <value>` a parameter's value; `--<choice>` alone the choice of the first choice parameter that offers it and
// holds none yet; a bare word the first parameter with no value, a choice where it is one of its choices and a word
// otherwise. `params` is the job's, each `{ key, kind, choices }`. Words no parameter takes are left out.
export function valuesOfWords(params, words) {
  const values = {};
  const open = (param) => !values[param.key]?.length;
  const offers = (param, word) => Array.isArray(param.choices) && param.choices.includes(word);
  for (let at = 0; at < words.length; at += 1) {
    const word = words[at];
    if (word.startsWith("--")) {
      const name = word.slice(2);
      const named = params.find((param) => param.key === name);
      if (named && at + 1 < words.length && !words[at + 1].startsWith("--")) {
        values[name] = [words[at + 1]];
        at += 1;
        continue;
      }
      const chosen = params.find((param) => open(param) && offers(param, name));
      if (chosen) {
        values[chosen.key] = [name];
      }
      continue;
    }
    const taker = params.find((param) => open(param) && (param.kind !== "choice" || offers(param, word)));
    if (taker) {
      values[taker.key] = [word];
    }
  }
  return values;
}
