// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Mermaid's diagrams drawn as SVG: flowcharts, sequence diagrams and pies.

import { layoutFlowchart, mermaidSvg, parseFlowchart, parseSequence, pieSvg } from "../../src/mermaid.js";

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

test("a diagram of a kind not drawn shows as its text, its kind named, and the same text is drawn once", () => {
  assert.match(mermaidSvg("gantt\n title A"), /A diagram of the kind gantt is shown as its text/);
  const text = "graph TD\n X --> Y";
  assert.equal(mermaidSvg(text), mermaidSvg(text));
});
