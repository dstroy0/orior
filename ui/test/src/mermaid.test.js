// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Mermaid's diagrams drawn as SVG: flowcharts, sequence diagrams, pie charts, state diagrams, class
// diagrams, entity-relationship diagrams, mind maps, Gantt charts, `journey` diagrams and git graphs.

import { mermaidSvg } from "../../gui/src-js/mermaid.js";

const { test, assert } = window.__harness;

test("a diagram of a kind not drawn shows as its text, its kind named, and the same text is drawn once", () => {
  assert.match(mermaidSvg("quadrantChart\n title A"), /A diagram of the kind quadrantChart is shown as its text/);
  const text = "graph TD\n X --> Y";
  assert.equal(mermaidSvg(text), mermaidSvg(text));
});
