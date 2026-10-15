// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { BOXES, PANIC, panelOf, panelRead, panelStart, panelStep, planOf } from "../../src/protocol.js";

const { test, assert } = window.__harness;

let sequence = 0;
const blob = (name, size = 100, priority = 2) => ({ sequence: ++sequence, nanoseconds: sequence * 1000, priority, member: "0", name, size });

// A dry pass's tap, as the writer places it: the files carried in, the first round, two rounds of
// questions with their answers and the cross's held list, and the teacher's console.
function dryPass() {
  return [
    blob("forms_work.txt", 5000),
    blob("faulted.txt", 300),
    blob("overseer.txt", 0),
    blob("kernel.txt", 1200),
    blob("question.bin", 496),
    blob("questions.txt", 64 * 3),
    blob("held.txt", 34),
    blob("answers0.txt", 20),
    blob("answers1.txt", 20),
    blob("answers2.txt", 20),
    blob("questions.txt", 64 * 2),
    blob("held.txt", 0),
    blob("answers0.txt", 20),
  ];
}

test("the plan is read off its line", () => {
  assert.deepEqual(planOf("turns 437304\n"), { pass: "turns", questions: 437304 });
  assert.equal(planOf("turns"), null);
  assert.equal(planOf("turns none"), null);
  assert.equal(planOf(""), null);
});

test("the boxes light in order as the blobs arrive", () => {
  const panel = panelOf(dryPass(), { tools: true, started: true });
  const lit = Object.fromEntries(panel.boxes.map((box) => [box.key, box.lit]));
  assert.deepEqual(lit, { tools: true, run: true, carried: true, first: true, round: true, panic: false, leave: false });
  assert.equal(panel.boxes.map((box) => box.key).join(","), BOXES.map((box) => box.key).join(","));
});

test("the round box counts the rounds and the current round's answered of asked", () => {
  const panel = panelOf(dryPass());
  assert.equal(panel.round.rounds, 2);
  assert.equal(panel.round.asked, 2);
  assert.equal(panel.round.answered, 1);
  assert.equal(panel.boxes.find((box) => box.key === "round").label, "Round 2");
  assert.equal(panel.answered, 4);
  assert.equal(panel.held, 1);
});

test("the progress is the answered and held over the plan, and whole once the run ends", () => {
  const blobs = dryPass();
  const plan = { pass: "turns", questions: 10 };
  assert.equal(panelOf(blobs, { plan }).progress, 0.5);
  assert.equal(panelOf(blobs, { plan: { pass: "turns", questions: 2 } }).progress, 1);
  assert.equal(panelOf(blobs).progress, null);
  assert.equal(panelOf(blobs, { plan, ended: true }).progress, 1);
  assert.equal(panelOf(blobs, { ended: true }).boxes.find((box) => box.key === "leave").lit, true);
});

test("a panic lights its box and is kept as the first one", () => {
  const blobs = dryPass();
  blobs.splice(8, 0, blob("conference_q7", 400, PANIC), blob("conference_q7_again", 400, PANIC));
  const panel = panelOf(blobs);
  assert.equal(panel.boxes.find((box) => box.key === "panic").lit, true);
  assert.equal(panel.panic.name, "conference_q7");
  assert.equal(panelOf(dryPass()).panic, null);
});

test("a reader that connects late leaves the earlier boxes dark", () => {
  const late = dryPass().slice(5);
  const panel = panelOf(late, { started: true });
  const lit = Object.fromEntries(panel.boxes.map((box) => [box.key, box.lit]));
  assert.equal(lit.carried, false);
  assert.equal(lit.first, false);
  assert.equal(lit.round, true);
});

test("the reading taken a blob at a time reads as the blobs read whole", () => {
  const blobs = dryPass();
  const stepped = panelRead(blobs.reduce(panelStep, panelStart()), { tools: true });
  assert.deepEqual(stepped, panelOf(blobs, { tools: true }));
});
