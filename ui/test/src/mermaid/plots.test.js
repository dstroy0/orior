// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Pies, mind maps, Gantt charts, journeys and git graphs read and drawn.

import { parseGantt, parseGitGraph, parseJourney, parseMindmap, pieSvg } from "../../../gui/src-js/mermaid/plots.js";
import { mermaidSvg } from "../../../gui/src-js/mermaid.js";

const { test, assert } = window.__harness;

test("a pie's slices are drawn with their shares", () => {
  const svg = pieSvg('pie title Pets\n "Dogs" : 3\n "Cats" : 1');
  assert.match(svg, />Pets</);
  assert.match(svg, />75%</);
  assert.match(svg, />25%</);
  assert.equal((svg.match(/class="mm-slice"/g) ?? []).length, 4, "two slices and two marks of the legend");
});

test("a mind map's ideas hang from the one they are indented under", () => {
  const read = parseMindmap("mindmap\n  root((Core))\n    A\n      A1\n    B[Box]");
  assert.deepEqual(read.nodes.map((node) => [node.label, node.shape]), [["Core", "circle"], ["A", "round"], ["A1", "round"], ["Box", "rect"]]);
  assert.deepEqual(read.edges.map((edge) => [edge.from, edge.to]), [["n0", "n1"], ["n1", "n2"], ["n0", "n3"]]);
});

test("a Gantt chart's tasks start, end and follow one another", () => {
  const read = parseGantt("gantt\n dateFormat YYYY-MM-DD\n section S\n A :done, a1, 2026-01-01, 10d\n B :after a1, 5d\n C :2026-02-01, 2026-02-03");
  const day = (at) => new Date(at).toISOString().slice(0, 10);
  assert.deepEqual(read.tasks.map((task) => [task.name, day(task.start), day(task.end), task.tags.join()]), [["A", "2026-01-01", "2026-01-11", "done"], ["B", "2026-01-11", "2026-01-16", ""], ["C", "2026-02-01", "2026-02-03", ""]]);
  assert.match(mermaidSvg("gantt\n title Plan\n A :2026-01-01, 3d"), /^<svg class="mermaid-diagram gantt"/);
});

test("a `journey` diagram's tasks with their scores and actors, and a git graph's commits on their branches", () => {
  const journey = parseJourney("journey\n title Day\n section Morning\n Coffee: 5: Me\n Commute: 2: Me, Cat");
  assert.deepEqual(journey.tasks.map((task) => [task.name, task.score, task.actors.join("+")]), [["Coffee", 5, "Me"], ["Commute", 2, "Me+Cat"]]);
  const git = parseGitGraph('gitGraph\n commit\n branch dev\n checkout dev\n commit id: "x"\n checkout main\n merge dev tag: "v1"');
  assert.deepEqual(git.commits.map((one) => [one.id, one.lane, one.merged?.id ?? null, one.tag]), [["0", 0, null, ""], ["x", 1, null, ""], ["2", 0, "x", "v1"]]);
});
