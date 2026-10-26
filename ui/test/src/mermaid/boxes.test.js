// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// State, class and entity-relationship diagrams read and drawn.

import { parseClasses, parseEntities, parseState } from "../../../gui/src-js/mermaid/boxes.js";
import { mermaidSvg } from "../../../gui/src-js/mermaid.js";

const { test, assert } = window.__harness;

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
