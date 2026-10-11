// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Mermaid's diagrams drawn as SVG: flowcharts, sequence diagrams, pie charts, state diagrams, class
// diagrams, entity-relationship diagrams, mind maps, Gantt charts, `journey` diagrams and git graphs.

import { layoutFlowchart, mermaidSvg, parseClasses, parseEntities, parseFlowchart, parseGantt, parseGitGraph, parseJourney, parseMindmap, parseSequence, parseState, pieSvg } from "../../src/mermaid.js";

const { test, assert } = window.__harness;

test("a flowchart's nodes take their shapes and its edges their kinds, labels and chains", () => {
  const chart = parseFlowchart("graph LR\n  A[Start] --> B{Ready?}\n  B -->|yes| C((Go))\n  B -. no .-> D([Wait]) --> A\n  C & D --- E");
  assert.equal(chart.direction, "LR");
  const shapes = Object.fromEntries(chart.nodes.map((node) => [node.id, [node.label, node.shape]]));
  assert.deepEqual(shapes, { A: ["Start", "rect"], B: ["Ready?", "diamond"], C: ["Go", "circle"], D: ["Wait", "stadium"], E: ["E", "rect"] });
  const edges = chart.edges.map((edge) => `${edge.from}${edge.dotted ? "." : "-"}${edge.arrow ? ">" : ""}${edge.to}${edge.label ? `:${edge.label}` : ""}`);
  assert.deepEqual(edges, ["A->B", "B->C:yes", "B.>D:no", "D->A", "C-E", "D-E"]);
});

test("a flowchart is laid out in ranks the way it runs, a loop left out of the ranks", () => {
  const down = layoutFlowchart(parseFlowchart("graph TD\n A --> B --> C\n C --> A\n A --> D"));
  const y = (id) => down.places.get(id).y;
  assert.ok(y("A") < y("B") && y("B") < y("C"), "top down");
  assert.equal(y("B"), y("D"), "B and D share a rank");
  const across = layoutFlowchart(parseFlowchart("flowchart LR\n A --> B"));
  assert.ok(across.places.get("A").x < across.places.get("B").x, "left to right");
  const up = layoutFlowchart(parseFlowchart("graph BT\n A --> B"));
  assert.ok(up.places.get("A").y > up.places.get("B").y, "bottom up");
});

test("a flowchart is drawn with a shape and a label for each node and a line for each edge", () => {
  const svg = new DOMParser().parseFromString(mermaidSvg("graph TD\n A[One] -->|then| B[Two]"), "image/svg+xml").documentElement;
  assert.equal(svg.querySelectorAll(".mm-node-group").length, 2);
  assert.ok([...svg.querySelectorAll("text")].map((one) => one.textContent).includes("then"));
  assert.equal(svg.querySelectorAll("line.mm-edge").length, 1);
});

test("a sequence diagram's participants, messages, notes and frames", () => {
  const read = parseSequence("sequenceDiagram\n participant A as Alice\n actor B\n A->>B: Hello\n B-->>A: Hi\n Note over A,B: talking\n loop every minute\n A->B: ping\n end");
  assert.deepEqual(read.participants.map((one) => [one.id, one.label, one.actor]), [["A", "Alice", false], ["B", "B", true]]);
  const messages = read.steps.filter((step) => step.from);
  assert.deepEqual(messages.map((step) => [step.from, step.to, step.text, step.dotted, step.head]), [["A", "B", "Hello", false, "arrow"], ["B", "A", "Hi", true, "arrow"], ["A", "B", "ping", false, "none"]]);
  const svg = mermaidSvg("sequenceDiagram\n A->>B: Hello\n loop again\n B->>A: back\n end");
  assert.match(svg, /class="mm-frame"/);
  assert.match(svg, />Hello</);
});

test("a pie's slices are drawn with their shares", () => {
  const svg = pieSvg('pie title Pets\n "Dogs" : 3\n "Cats" : 1');
  assert.match(svg, />Pets</);
  assert.match(svg, />75%</);
  assert.match(svg, />25%</);
  assert.equal((svg.match(/class="mm-slice"/g) ?? []).length, 4, "two slices and two marks of the legend");
});

test("a state diagram's states, its start and end, and its transitions", () => {
  const read = parseState("stateDiagram-v2\n [*] --> Still\n Still --> Moving : push\n Moving --> [*]\n state X <<choice>>\n Still : at rest");
  assert.deepEqual(read.nodes.map((node) => node.shape), ["start", "round", "round", "end", "diamond"]);
  assert.equal(read.nodes[1].label, "Still\\nat rest");
  assert.deepEqual(read.edges.map((edge) => edge.label), ["", "push", ""]);
  assert.match(mermaidSvg("stateDiagram\n [*] --> A"), /^<svg class="mermaid-diagram state"/);
});

test("a class diagram's members and its relations, each kind marked at its ends with its counts", () => {
  const read = parseClasses('classDiagram\n class Animal {\n +String name\n +eat() void\n }\n Animal <|-- Duck\n Duck "1" *-- "many" Egg : lays\n Duck : +swim()');
  const animal = read.classes.find((one) => one.id === "Animal");
  assert.deepEqual([animal.fields, animal.methods], [["+String name"], ["+eat() void"]]);
  assert.deepEqual(read.classes.find((one) => one.id === "Duck").methods, ["+swim()"]);
  assert.deepEqual(read.edges.map((edge) => [edge.from, edge.to, edge.start, edge.end, edge.fromLabel, edge.toLabel, edge.label]), [["Animal", "Duck", "triangle", "", "", "", ""], ["Duck", "Egg", "diamond", "", "1", "many", "lays"]]);
  assert.match(mermaidSvg("classDiagram\n A <|-- B"), /marker-start="url\(#mm-triangle\)"/);
});

test("an entity-relationship diagram's entities, attributes and counts", () => {
  const read = parseEntities("erDiagram\n CUSTOMER ||--o{ ORDER : places\n CUSTOMER {\n string name PK\n }");
  assert.deepEqual(read.edges.map((edge) => [edge.from, edge.to, edge.fromLabel, edge.toLabel, edge.label]), [["CUSTOMER", "ORDER", "1", "0..*", "places"]]);
  assert.deepEqual(read.entities[0].rows, ["string  name  PK"]);
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

test("a diagram of a kind not drawn shows as its text, its kind named, and the same text is drawn once", () => {
  assert.match(mermaidSvg("quadrantChart\n title A"), /A diagram of the kind quadrantChart is shown as its text/);
  const text = "graph TD\n X --> Y";
  assert.equal(mermaidSvg(text), mermaidSvg(text));
});
