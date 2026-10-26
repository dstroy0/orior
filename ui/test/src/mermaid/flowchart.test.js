// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Flowcharts read, laid out in ranks and drawn.

import { layoutFlowchart, parseFlowchart } from "../../../gui/src-js/mermaid/flowchart.js";
import { mermaidSvg } from "../../../gui/src-js/mermaid.js";

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
