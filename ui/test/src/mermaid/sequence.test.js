// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Sequence diagrams read and drawn.

import { parseSequence } from "../../../gui/src-js/mermaid/sequence.js";
import { mermaidSvg } from "../../../gui/src-js/mermaid.js";

const { test, assert } = window.__harness;

test("a sequence diagram's participants, messages, notes and frames", () => {
  const read = parseSequence("sequenceDiagram\n participant A as Alice\n actor B\n A->>B: Hello\n B-->>A: Hi\n Note over A,B: talking\n loop every minute\n A->B: ping\n end");
  assert.deepEqual(read.participants.map((one) => [one.id, one.label, one.actor]), [["A", "Alice", false], ["B", "B", true]]);
  const messages = read.steps.filter((step) => step.from);
  assert.deepEqual(messages.map((step) => [step.from, step.to, step.text, step.dotted, step.head]), [["A", "B", "Hello", false, "arrow"], ["B", "A", "Hi", true, "arrow"], ["A", "B", "ping", false, "none"]]);
  const svg = mermaidSvg("sequenceDiagram\n A->>B: Hello\n loop again\n B->>A: back\n end");
  assert.match(svg, /class="mm-frame"/);
  assert.match(svg, />Hello</);
});
