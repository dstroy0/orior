// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { PANIC, ROUND_MOST, ROUNDS_SHOWN, blobsOfNode, nodesOf, panelOf, panelReplanned, panelStart, panelStep, planOf, roundsOf, valuesOfWords } from "../../src/protocol.js";

const { test, assert } = window.__harness;

let sequence = 0;
const blob = (name, size = 100, priority = 2) => ({ sequence: ++sequence, nanoseconds: sequence * 1000, priority, member: "0", name, size });

// A pass's tap, as the writer places it: the files carried in, the first round's question, then two
// rounds of questions with their answers.
function pass() {
  return [
    blob("forms_work.txt", 5000),
    blob("faulted.txt", 300),
    blob("overseer.txt", 0),
    blob("questions.txt", 64),
    blob("answers0.txt", 20),
    blob("questions.txt", 64 * 3),
    blob("answers0.txt", 20),
    blob("answers1.txt", 20),
    blob("answers2.txt", 20),
    blob("questions.txt", 64 * 2),
    blob("answers0.txt", 20),
  ];
}

const plan = { pass: "turns", questions: 2 * ROUND_MOST };

test("the plan is read off its line, from the blob or the console", () => {
  assert.deepEqual(planOf("turns 437304\n"), { pass: "turns", questions: 437304 });
  assert.deepEqual(planOf("  plan: registers 15478"), { pass: "registers", questions: 15478 });
  assert.equal(planOf("turns"), null);
  assert.equal(planOf("turns none"), null);
});

test("the line is whole before the run starts, and the plan divides the rounds", () => {
  assert.deepEqual(nodesOf("costs", null).map((node) => node.label), ["Tools", "Run", "Carried in", "Plan", "costs", "Written", "Leave"]);
  assert.deepEqual(nodesOf("turns", plan).map((node) => node.label), ["Tools", "Run", "Carried in", "Plan", "Round 1", "Round 2", "Written", "Leave"]);
  const costs = nodesOf("costs", { pass: "costs", questions: 24 }).filter((node) => node.round);
  assert.equal(costs.length, 1);
  assert.equal(costs[0].rounds, 24);
  assert.equal(nodesOf("costs", { pass: "costs", questions: ROUNDS_SHOWN }).filter((node) => node.round).length, ROUNDS_SHOWN);
  assert.equal(nodesOf("turns", { pass: "turns", questions: 1 }).filter((node) => node.round).length, 1);
});

test("a plan of many rounds is one stretch of the line, named for the round under way", () => {
  const long = { pass: "turns", questions: 20 * ROUND_MOST };
  assert.equal(roundsOf(long), 20);
  assert.ok(20 > ROUNDS_SHOWN);
  assert.deepEqual(nodesOf("turns", long).map((node) => node.label), ["Tools", "Run", "Carried in", "Plan", "Round 1 of 20", "Written", "Leave"]);
  assert.deepEqual(nodesOf("turns", long, 7).map((node) => node.label), ["Tools", "Run", "Carried in", "Plan", "Round 7 of 20", "Written", "Leave"]);
  assert.equal(nodesOf("turns", long, 99)[4].label, "Round 20 of 20");
  const read = panelOf(pass(), { pass: "turns", plan: long, tools: true, started: true });
  assert.equal(read.nodes[4].label, "Round 3 of 20");
  assert.equal(read.nodes[4].reached, true);
  assert.equal(read.at, 4);
  // two rounds done and the third half answered, over twenty: the fire stands that far along the stretch
  assert.ok(Math.abs(read.place - (4 + 2.5 / 20) / 6) < 1e-9);
  assert.equal(read.nodes[4].blobs, 2);
});

test("the reading folded again once the plan comes reads the earlier rounds as rounds", () => {
  const late = pass().reduce((kept, one) => panelStep(kept, one, { plan: null }), panelStart());
  assert.equal(late.rounds, 0);
  assert.equal(blobsOfNode(late, "plan").count, 3);
  const again = panelReplanned(late, plan);
  assert.equal(again.rounds, 3);
  assert.equal(blobsOfNode(again, "plan").count, 0);
  assert.equal(blobsOfNode(again, "round1").count, 2);
  assert.equal(blobsOfNode(again, "carried").count, 3);
});

test("the nodes are reached in order and the fire stands at the furthest", () => {
  const nothing = panelOf([], { pass: "turns" });
  assert.equal(nothing.at, -1);
  assert.equal(nothing.place, 0);
  const built = panelOf([], { pass: "turns", tools: true, started: true });
  assert.equal(built.at, 1);
  assert.equal(built.place, 1 / 6);
  const carried = panelOf(pass().slice(0, 3), { pass: "turns", tools: true, started: true });
  assert.equal(carried.at, 2);
  assert.equal(carried.nodes[2].reached, true);
});

test("the rounds are counted after the plan, and the round under way burns toward the next node", () => {
  const blobs = pass();
  const read = panelOf(blobs, { pass: "turns", plan, tools: true, started: true });
  assert.equal(read.round.rounds, 3);
  assert.deepEqual(read.nodes.map((node) => node.reached), [true, true, true, true, true, true, false, false]);
  assert.equal(read.at, 5);
  const halfway = panelOf(blobs.slice(0, 7), { pass: "turns", plan, tools: true, started: true });
  assert.equal(halfway.round.rounds, 2);
  assert.equal(halfway.round.asked, 3);
  assert.equal(halfway.round.answered, 1);
  assert.equal(halfway.at, 5);
});

test("each node keeps the blobs placed while it was under way", () => {
  const reading = pass().reduce((kept, one) => panelStep(kept, one, { plan }), panelStart());
  assert.deepEqual(blobsOfNode(reading, "carried").list.map((one) => one.name), ["forms_work.txt", "faulted.txt", "overseer.txt"]);
  assert.deepEqual(blobsOfNode(reading, "round1").list.map((one) => one.name), ["questions.txt", "answers0.txt"]);
  assert.equal(blobsOfNode(reading, "round2").count, 4);
  assert.equal(blobsOfNode(reading, "leave").count, 0);
  assert.equal(blobsOfNode(reading, "round3").count, 2);
  const read = panelOf(pass(), { pass: "turns", plan });
  assert.equal(read.nodes.find((node) => node.key === "round2").blobs, 4);
});

test("the written files and the end reach the last nodes", () => {
  const blobs = [...pass(), blob("sm_86.ksc", 3000)];
  const written = panelOf(blobs, { pass: "turns", plan, tools: true, started: true });
  assert.equal(written.nodes.find((node) => node.key === "written").reached, true);
  assert.equal(written.nodes.find((node) => node.key === "written").blobs, 1);
  assert.equal(written.nodes.find((node) => node.key === "leave").reached, false);
  const ended = panelOf(blobs, { pass: "turns", plan, tools: true, started: true, ended: true });
  assert.equal(ended.place, 1);
  assert.equal(ended.nodes.at(-1).reached, true);
});

test("a panic is kept as the first one", () => {
  const blobs = pass();
  blobs.splice(8, 0, blob("conference_q7", 400, PANIC), blob("conference_q7_again", 400, PANIC));
  assert.equal(panelOf(blobs, { pass: "turns" }).panic.name, "conference_q7");
  assert.equal(panelOf(pass(), { pass: "turns" }).panic, null);
});

test("a protocol command's words read into a job's values", () => {
  const learn = [
    { key: "step", kind: "choice", choices: ["discover", "seed", "turns", "registers", "costs", "rounds", "dry", "enumerate"] },
    { key: "word", kind: "word" },
    { key: "base", kind: "word" },
    { key: "leave", kind: "choice", choices: ["all", "none"] },
  ];
  assert.deepEqual(valuesOfWords(learn, ["costs", "--leave", "all"]), { step: ["costs"], leave: ["all"] });
  assert.deepEqual(valuesOfWords(learn, ["seed", "build/stick.sass", "--leave", "none"]), { step: ["seed"], word: ["build/stick.sass"], leave: ["none"] });
  assert.deepEqual(valuesOfWords(learn, ["dry", "1"]), { step: ["dry"], word: ["1"] });
  const query = [
    { key: "from", kind: "choice", choices: ["cu", "openqasm2_0", "sass"] },
    { key: "to", kind: "choice", choices: ["cu", "openqasm2_0", "sass"] },
    { key: "part", kind: "choice", choices: ["sm_86"] },
    { key: "dry", kind: "choice", choices: ["dry"] },
  ];
  assert.deepEqual(valuesOfWords(query, ["--cu", "--sass", "--sm_86", "--dry"]), { from: ["cu"], to: ["sass"], part: ["sm_86"], dry: ["dry"] });
  assert.deepEqual(valuesOfWords(query, ["--from", "cu", "--to", "sass"]), { from: ["cu"], to: ["sass"] });
  assert.deepEqual(valuesOfWords(learn, ["--fly"]), {});
});
