// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The protocol panel's reading of a run: what the run's writer placed, blob by blob off its tap,
// read into the boxes the fuse burns toward, the round's count of answered against asked, the
// pass's progress against the plan the teacher handed, and the panic that sputters the fuse out.
// Nothing here touches the page: the run page draws what this reads.
//
// A blob is `{ sequence, nanoseconds, priority, member, name, size }` as the server reads it off the
// tap, the priority 0 a panic, 1 critical, 2 ordinary. The reading is a fold: `panelStart()` is the
// reading of no blobs, `panelStep(reading, blob)` takes one more as it arrives, and `panelRead` says
// what the panel shows. The boxes light in the order given, each on the first blob that names it; a
// reader that connects late sees the run from then on, and a box whose blob came before stays dark,
// which says nothing of a failure.

export const PANIC = 0;

// The boxes along the fuse, each with the blobs that light it: a name, or a test of a name. Tools, Run
// and Leave light from the run's own course, not from a blob.
export const BOXES = [
  { key: "tools", label: "Tools", lit: () => false },
  { key: "run", label: "Run", lit: () => false },
  { key: "carried", label: "Carried in", lit: (name) => name === "forms_work.txt" || name === "faulted.txt" || name === "overseer.txt" },
  { key: "first", label: "First round", lit: (name) => name === "kernel.txt" || name === "question.bin" },
  { key: "round", label: "Round", lit: (name) => name === "questions.txt" },
  { key: "panic", label: "Panic", lit: () => false },
  { key: "leave", label: "Leave", lit: () => false },
];

// A question's answer: `answers<n>.txt`, the carrier's answer to question n of the round.
const ANSWER = /^answers(\d+)\.txt$/;

// A line of a round's list is about this many bytes, and a line of the held list this many: a round's
// count of questions is read off the list's size until its answers say it exactly.
const LIST_LINE = 64;
const HELD_LINE = 34;

// The plan the teacher hands as a pass starts, `<pass> <questions>`, read off the text of `plan.txt`
// or off the teacher's console line `plan: <pass> <questions>`.
export function planOf(text) {
  const words = String(text ?? "").trim().replace(/^plan:\s*/, "").split(/\s+/);
  const questions = Number(words[1]);
  return words.length === 2 && Number.isFinite(questions) && questions > 0 ? { pass: words[0], questions } : null;
}

// The reading of no blobs.
export function panelStart() {
  return { lit: new Set(), rounds: 0, asked: 0, answered: 0, answeredAll: 0, held: 0, panic: null };
}

// The reading with one more blob taken, the one the writer placed next.
export function panelStep(reading, blob) {
  if (blob.priority === PANIC && !reading.panic) {
    reading.panic = blob;
    reading.lit.add("panic");
  }
  for (const box of BOXES) {
    if (box.lit(blob.name)) {
      reading.lit.add(box.key);
    }
  }
  if (blob.name === "questions.txt") {
    reading.rounds += 1;
    reading.asked = Math.max(1, Math.round(blob.size / LIST_LINE));
    reading.answered = 0;
  }
  if (blob.name === "held.txt") {
    reading.held += Math.max(0, Math.round(blob.size / HELD_LINE));
  }
  const answer = ANSWER.exec(blob.name);
  if (answer) {
    reading.answered += 1;
    reading.answeredAll += 1;
    reading.asked = Math.max(reading.asked, Number(answer[1]) + 1);
  }
  return reading;
}

// What the panel shows of a reading, with the plan where one is known, whether the tools were built
// and the run started, and whether the run has ended: `boxes` each box's key, label and whether it
// is lit; `round` the rounds written so far and the current round's asked and answered; `answered`
// and `held` over the whole run; `progress` 0 to 1 by the plan, whole once the run ends, or null
// where there is no plan; `panic` the first panic's blob or null.
export function panelRead(reading, { plan = null, tools = false, started = false, ended = false } = {}) {
  const lit = (key) => reading.lit.has(key) || (key === "tools" && tools) || (key === "run" && started) || (key === "leave" && ended);
  const progress = plan ? Math.min(1, (reading.answeredAll + reading.held) / plan.questions) : null;
  return {
    boxes: BOXES.map((box) => ({ key: box.key, label: box.key === "round" && reading.rounds ? `Round ${reading.rounds}` : box.label, lit: lit(box.key) })),
    round: { rounds: reading.rounds, asked: reading.asked, answered: reading.answered },
    answered: reading.answeredAll,
    held: reading.held,
    progress: ended && progress !== null ? 1 : progress,
    panic: reading.panic,
  };
}

// What the panel shows of `blobs` read whole, as the fold of the three above.
export function panelOf(blobs, shown = {}) {
  return panelRead(blobs.reduce(panelStep, panelStart()), shown);
}
