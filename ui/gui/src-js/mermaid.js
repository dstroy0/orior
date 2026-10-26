// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Mermaid's diagrams as SVG, drawn here: flowcharts, sequence diagrams, pie charts, state diagrams,
// class diagrams, entity-relationship diagrams, mind maps, Gantt charts, `journey` diagrams and git graphs. A
// flowchart's nodes take their shapes and its edges their kinds and labels, laid out in ranks the way
// the chart runs, top down, bottom up, left to right or right to left, each rank ordered to cross as
// few edges as a few sweeps find; states, classes, entities and a mind map's ideas are laid out the
// same way, a class and an entity in a box of sections, each relation's kind marked at its ends with
// its counts beside them. A sequence diagram's participants stand in a row over their lifelines, its
// messages and notes in order down them, its loops and alternatives framed. A pie's slices are drawn
// with their shares beside them; a Gantt chart's tasks as bars over a scale of days, by section; a
// `journey` diagram's tasks with each one's score; a git graph's commits along the lanes of their branches. A
// diagram of another kind is shown as its text, its kind named.
//
// The same text gives the same drawing, kept: a diagram is laid out again only when its text changes.

import { classSvg, erSvg, stateSvg } from "./mermaid/boxes.js";
import { flowchartSvg } from "./mermaid/flowchart.js";
import { ganttSvg, gitGraphSvg, journeySvg, mindmapSvg, pieSvg } from "./mermaid/plots.js";
import { sequenceSvg } from "./mermaid/sequence.js";
import { escape } from "./mermaid/svg.js";

export { classSvg, erSvg, parseClasses, parseEntities, parseState, stateSvg } from "./mermaid/boxes.js";
export { flowchartSvg, layoutFlowchart, parseFlowchart } from "./mermaid/flowchart.js";
export { ganttSvg, gitGraphSvg, journeySvg, mindmapSvg, parseGantt, parseGitGraph, parseJourney, parseMindmap, pieSvg } from "./mermaid/plots.js";
export { parseSequence, sequenceSvg } from "./mermaid/sequence.js";

const drawn = new Map();


// The kind of diagram `text` is, by its first word.
export const kindOf = (text) => text.trim().split(/\s/)[0];

// `text`, a Mermaid diagram, as SVG, or as its text with its kind named where it is a kind not drawn.
export function mermaidSvg(text) {
  const source = text.replace(/\r\n?/g, "\n").trim();
  if (drawn.has(source)) {
    return drawn.get(source);
  }
  const kind = kindOf(source);
  let out;
  try {
    if (kind === "graph" || kind === "flowchart") {
      out = flowchartSvg(source);
    } else if (kind === "stateDiagram" || kind === "stateDiagram-v2") {
      out = stateSvg(source);
    } else if (kind === "classDiagram") {
      out = classSvg(source);
    } else if (kind === "erDiagram") {
      out = erSvg(source);
    } else if (kind === "mindmap") {
      out = mindmapSvg(source);
    } else if (kind === "gantt") {
      out = ganttSvg(source);
    } else if (kind === "journey") {
      out = journeySvg(source);
    } else if (kind === "gitGraph") {
      out = gitGraphSvg(source);
    } else if (kind === "sequenceDiagram") {
      out = sequenceSvg(source);
    } else if (kind === "pie") {
      out = pieSvg(source);
    } else {
      out = `<pre class="mermaid-text"><code>${escape(source)}</code></pre><p class="mermaid-said">A diagram of the kind ${escape(kind)} is shown as its text.</p>`;
    }
  } catch (error) {
    out = `<pre class="mermaid-text"><code>${escape(source)}</code></pre><p class="mermaid-said">The diagram was not drawn: ${escape(error.message)}</p>`;
  }
  drawn.set(source, out);
  if (drawn.size > 200) {
    drawn.delete(drawn.keys().next().value);
  }
  return out;
}
