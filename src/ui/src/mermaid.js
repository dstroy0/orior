// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Mermaid's diagrams as SVG, drawn here: flowcharts, sequence diagrams and pie charts. A flowchart's
// nodes take their shapes and its edges their kinds and labels, laid out in ranks the way the chart
// runs, top down, bottom up, left to right or right to left, each rank ordered to cross as few edges as
// a few sweeps find. A sequence diagram's participants stand in a row over their lifelines, its
// messages and notes in order down them, its loops and alternatives framed. A pie's slices are drawn
// with their shares beside them. A diagram of another kind is shown as its text, its kind named.
//
// The same text gives the same drawing, kept: a diagram is laid out again only when its text changes.

const ESCAPES = { "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" };
const escape = (text) => String(text).replace(/[&<>"]/g, (letter) => ESCAPES[letter]);

const FONT = "14px system-ui, sans-serif";
const drawn = new Map();

let measurer = null;
// The width `text` takes in the diagrams' font, in pixels.
function width(text) {
  measurer ??= document.createElement("canvas").getContext("2d");
  measurer.font = FONT;
  return measurer.measureText(text).width;
}

// A label's lines: <br> and \n each start a new one.
const linesOf = (text) => String(text).split(/<br\s*\/?>|\\n/i);
const widest = (text) => Math.max(...linesOf(text).map(width));

function textAt(x, y, text, anchor = "middle", kind = "mm-text") {
  const lines = linesOf(text);
  const top = y - ((lines.length - 1) * 17) / 2;
  return lines.map((line, at) => `<text class="${kind}" x="${x}" y="${top + at * 17}" text-anchor="${anchor}" dominant-baseline="central">${escape(line)}</text>`).join("");
}

// ---- Flowcharts ----

// The shapes a node's brackets give it, longest first.
const SHAPES = [
  ["(((", ")))", "double"], ["([", "])", "stadium"], ["[[", "]]", "subroutine"], ["[(", ")]", "cylinder"], ["((", "))", "circle"], ["{{", "}}", "hexagon"],
  ["[/", "/]", "parallelogram"], ["[\\", "\\]", "parallelogram"], ["[/", "\\]", "trapezoid"], [">", "]", "flag"], ["[", "]", "rect"], ["(", ")", "round"], ["{", "}", "diamond"],
];

const EDGE = /^\s*(<?)(-->|---|-\.->|-\.-|==>|===|--o|--x|-\.-o|~~~)(?:\|([^|]*)\|)?\s*/;
// An edge with its text between its two halves: `-- text -->`, `== text ==>` or `-. text .->`.
const EDGE_TEXT = /^\s*(<?)(--|==|-\.)\s+([^-=.][^]*?)\s+(-->|---|==>|===|\.->|\.-|-\.->|-\.-)\s*/;

function readNode(text, nodes) {
  const id = text.match(/^\s*([A-Za-z0-9_À-￿][\wÀ-￿.-]*)/);
  if (!id) {
    return null;
  }
  let rest = text.slice(id[0].length);
  const name = id[1];
  let shape = null;
  let label = null;
  for (const [open, close, kind] of SHAPES) {
    if (rest.startsWith(open)) {
      const end = rest.indexOf(close, open.length);
      if (end >= 0) {
        label = rest.slice(open.length, end).trim().replace(/^"(.*)"$/, "$1");
        shape = kind;
        rest = rest.slice(end + close.length);
        break;
      }
    }
  }
  const node = nodes.get(name) ?? { id: name, label: name, shape: "rect", order: nodes.size };
  if (shape) {
    node.label = label;
    node.shape = shape;
  }
  nodes.set(name, node);
  return { ids: [name], rest };
}

// Nodes joined by &, as `A & B`.
function readGroup(text, nodes) {
  const first = readNode(text, nodes);
  if (!first) {
    return null;
  }
  let { ids, rest } = first;
  while (/^\s*&/.test(rest)) {
    const next = readNode(rest.replace(/^\s*&/, ""), nodes);
    if (!next) {
      break;
    }
    ids = ids.concat(next.ids);
    rest = next.rest;
  }
  return { ids, rest };
}

export function parseFlowchart(text) {
  const lines = text.split(/\n|;/).map((line) => line.replace(/%%.*$/, "").trim()).filter(Boolean);
  const direction = lines[0].match(/^(?:graph|flowchart)\s*(TD|TB|BT|LR|RL)?/i)?.[1]?.toUpperCase() ?? "TD";
  const nodes = new Map();
  const edges = [];
  for (const line of lines.slice(1)) {
    if (/^(subgraph|end|classDef|class|style|linkStyle|click|direction)\b/.test(line)) {
      continue;
    }
    let group = readGroup(line, nodes);
    while (group) {
      const rest = group.rest;
      const plain = rest.match(EDGE);
      const worded = plain ? null : rest.match(EDGE_TEXT);
      if (!plain && !worded) {
        break;
      }
      const kind = plain ? plain[2] : worded[4];
      const label = plain ? plain[3] ?? "" : worded[3];
      const both = Boolean((plain ?? worded)[1]);
      const next = readGroup(rest.slice((plain ?? worded)[0].length), nodes);
      if (!next) {
        break;
      }
      for (const from of group.ids) {
        for (const to of next.ids) {
          edges.push({ from, to, label: label.trim(), dotted: kind.includes("."), thick: kind.startsWith("="), arrow: kind.endsWith(">") || kind.endsWith("o") || kind.endsWith("x"), end: kind.at(-1), both });
        }
      }
      group = next;
    }
  }
  return { direction, nodes: [...nodes.values()], edges };
}

function nodeSize(node) {
  const text = widest(node.label);
  const lines = linesOf(node.label).length;
  const tall = 22 + lines * 17;
  switch (node.shape) {
    case "circle":
    case "double": {
      const side = Math.max(text + 24, tall);
      return [side, side];
    }
    case "diamond": {
      const side = Math.max(text + 30, tall) * 1.25;
      return [side, side * 0.8];
    }
    case "hexagon":
      return [text + 50, tall];
    default:
      return [text + 32, tall];
  }
}

// The rank of each node: its longest path from a node with no edge into it, edges that close a loop
// left out.
function ranks(nodes, edges) {
  const into = new Map(nodes.map((node) => [node.id, []]));
  const outOf = new Map(nodes.map((node) => [node.id, []]));
  const state = new Map();
  const forward = [];
  const visit = (id) => {
    state.set(id, 1);
    for (const edge of edges.filter((one) => one.from === id)) {
      if (state.get(edge.to) === 1) {
        continue;
      }
      forward.push(edge);
      if (!state.get(edge.to)) {
        visit(edge.to);
      }
    }
    state.set(id, 2);
  };
  nodes.forEach((node) => !state.get(node.id) && visit(node.id));
  for (const edge of forward) {
    into.get(edge.to).push(edge.from);
    outOf.get(edge.from).push(edge.to);
  }
  const rank = new Map();
  const rankOf = (id, seen = new Set()) => {
    if (rank.has(id)) {
      return rank.get(id);
    }
    seen.add(id);
    const value = Math.max(0, ...into.get(id).filter((one) => !seen.has(one)).map((one) => rankOf(one, seen) + 1));
    rank.set(id, value);
    return value;
  };
  nodes.forEach((node) => rankOf(node.id));
  return { rank, into, outOf };
}

export function layoutFlowchart(chart) {
  const { rank, into, outOf } = ranks(chart.nodes, chart.edges);
  const levels = [];
  for (const node of [...chart.nodes].sort((a, b) => a.order - b.order)) {
    (levels[rank.get(node.id)] ??= []).push(node);
  }
  const position = new Map();
  const reorder = (level, neighbors) => {
    level.forEach((node, at) => position.set(node.id, at));
    const center = (node) => {
      const near = neighbors.get(node.id).map((id) => position.get(id)).filter((at) => at !== undefined);
      return near.length ? near.reduce((sum, at) => sum + at, 0) / near.length : position.get(node.id);
    };
    level.sort((a, b) => center(a) - center(b));
    level.forEach((node, at) => position.set(node.id, at));
  };
  levels.forEach((level) => level.forEach((node, at) => position.set(node.id, at)));
  for (let sweep = 0; sweep < 4; sweep += 1) {
    levels.slice(1).forEach((level) => reorder(level, into));
    levels.slice(0, -1).reverse().forEach((level) => reorder(level, outOf));
  }
  const across = chart.direction === "LR" || chart.direction === "RL";
  const sizes = new Map(chart.nodes.map((node) => [node.id, nodeSize(node)]));
  const GAP = 30;
  const STEP = 60;
  const breadth = (level) => level.reduce((sum, node) => sum + sizes.get(node.id)[across ? 1 : 0], 0) + GAP * (level.length - 1);
  const widestLevel = Math.max(...levels.map(breadth));
  const depths = levels.map((level) => Math.max(...level.map((node) => sizes.get(node.id)[across ? 0 : 1])));
  const places = new Map();
  let deep = 20;
  levels.forEach((level, at) => {
    let along = 20 + (widestLevel - breadth(level)) / 2;
    for (const node of level) {
      const [w, h] = sizes.get(node.id);
      const side = across ? h : w;
      const middleAlong = along + side / 2;
      const middleDeep = deep + depths[at] / 2;
      places.set(node.id, across ? { x: middleDeep, y: middleAlong, w, h } : { x: middleAlong, y: middleDeep, w, h });
      along += side + GAP;
    }
    deep += depths[at] + STEP;
  });
  const total = { w: across ? deep - STEP + 20 : widestLevel + 40, h: across ? widestLevel + 40 : deep - STEP + 20 };
  if (chart.direction === "BT" || chart.direction === "RL") {
    for (const place of places.values()) {
      if (chart.direction === "BT") {
        place.y = total.h - place.y;
      } else {
        place.x = total.w - place.x;
      }
    }
  }
  return { places, total };
}

function shapeSvg(node, { x, y, w, h }) {
  const left = x - w / 2;
  const top = y - h / 2;
  const points = (list) => `<polygon class="mm-node" points="${list.map(([px, py]) => `${px},${py}`).join(" ")}" />`;
  switch (node.shape) {
    case "round":
      return `<rect class="mm-node" x="${left}" y="${top}" width="${w}" height="${h}" rx="8" />`;
    case "stadium":
      return `<rect class="mm-node" x="${left}" y="${top}" width="${w}" height="${h}" rx="${h / 2}" />`;
    case "circle":
      return `<circle class="mm-node" cx="${x}" cy="${y}" r="${w / 2}" />`;
    case "double":
      return `<circle class="mm-node" cx="${x}" cy="${y}" r="${w / 2}" /><circle class="mm-node" cx="${x}" cy="${y}" r="${w / 2 - 4}" />`;
    case "diamond":
      return points([[x, top], [left + w, y], [x, top + h], [left, y]]);
    case "hexagon":
      return points([[left + 15, top], [left + w - 15, top], [left + w, y], [left + w - 15, top + h], [left + 15, top + h], [left, y]]);
    case "parallelogram":
      return points([[left + 12, top], [left + w, top], [left + w - 12, top + h], [left, top + h]]);
    case "trapezoid":
      return points([[left + 12, top], [left + w - 12, top], [left + w, top + h], [left, top + h]]);
    case "flag":
      return points([[left, top], [left + w, top], [left + w, top + h], [left, top + h], [left + 12, y]]);
    case "subroutine":
      return `<rect class="mm-node" x="${left}" y="${top}" width="${w}" height="${h}" /><line class="mm-node-line" x1="${left + 8}" y1="${top}" x2="${left + 8}" y2="${top + h}" /><line class="mm-node-line" x1="${left + w - 8}" y1="${top}" x2="${left + w - 8}" y2="${top + h}" />`;
    case "cylinder":
      return `<path class="mm-node" d="M${left},${top + 6} a${w / 2},6 0 0,0 ${w},0 a${w / 2},6 0 0,0 ${-w},0 v${h - 12} a${w / 2},6 0 0,0 ${w},0 v${-(h - 12)}" />`;
    default:
      return `<rect class="mm-node" x="${left}" y="${top}" width="${w}" height="${h}" />`;
  }
}

// Where the line from the middle of `place` toward (`tx`, `ty`) leaves its box.
function edgeOf(place, tx, ty) {
  const dx = tx - place.x;
  const dy = ty - place.y;
  if (!dx && !dy) {
    return [place.x, place.y];
  }
  const scale = Math.min(Math.abs(place.w / 2 / (dx || 1e-9)), Math.abs(place.h / 2 / (dy || 1e-9)));
  return [place.x + dx * scale, place.y + dy * scale];
}

export function flowchartSvg(text) {
  const chart = parseFlowchart(text);
  const { places, total } = layoutFlowchart(chart);
  const parts = [];
  for (const edge of chart.edges) {
    const from = places.get(edge.from);
    const to = places.get(edge.to);
    const [x1, y1] = edgeOf(from, to.x, to.y);
    const [x2, y2] = edgeOf(to, from.x, from.y);
    const marker = edge.arrow ? ` marker-end="url(#mm-${edge.end === "o" ? "circle" : edge.end === "x" ? "cross" : "arrow"})"` : "";
    const start = edge.both ? ' marker-start="url(#mm-arrow-back)"' : "";
    parts.push(`<line class="mm-edge${edge.dotted ? " dotted" : ""}${edge.thick ? " thick" : ""}" x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}"${marker}${start} />`);
    if (edge.label) {
      const mx = (x1 + x2) / 2;
      const my = (y1 + y2) / 2;
      const w = widest(edge.label) + 10;
      const h = linesOf(edge.label).length * 17 + 4;
      parts.push(`<rect class="mm-label" x="${mx - w / 2}" y="${my - h / 2}" width="${w}" height="${h}" />${textAt(mx, my, edge.label)}`);
    }
  }
  for (const node of chart.nodes) {
    const place = places.get(node.id);
    parts.push(`<g class="mm-node-group" data-id="${escape(node.id)}">${shapeSvg(node, place)}${textAt(place.x, place.y, node.label)}</g>`);
  }
  return svg(total.w, total.h, parts.join(""), "flowchart");
}

// ---- Sequence diagrams ----

export function parseSequence(text) {
  const participants = [];
  const named = new Map();
  const add = (id, label = id, actor = false) => {
    if (!named.has(id)) {
      const one = { id, label, actor };
      named.set(id, one);
      participants.push(one);
    } else if (label !== id) {
      named.get(id).label = label;
    }
    return named.get(id);
  };
  const steps = [];
  let numbered = false;
  for (const raw of text.split("\n").slice(1)) {
    const line = raw.replace(/%%.*$/, "").trim();
    if (!line) {
      continue;
    }
    let match;
    if ((match = line.match(/^(participant|actor)\s+(\S+?)(?:\s+as\s+(.+))?$/))) {
      add(match[2], match[3] ?? match[2], match[1] === "actor");
    } else if (line === "autonumber") {
      numbered = true;
    } else if ((match = line.match(/^note\s+(left of|right of|over)\s+([^:]+):\s*(.*)$/i))) {
      const over = match[2].split(",").map((one) => add(one.trim()).id);
      steps.push({ note: match[3], where: match[1].toLowerCase(), over });
    } else if ((match = line.match(/^(loop|alt|opt|par|critical|break|rect)\b\s*(.*)$/))) {
      steps.push({ open: match[1], label: match[2] });
    } else if ((match = line.match(/^(else|and|option)\b\s*(.*)$/))) {
      steps.push({ divide: match[2] });
    } else if (line === "end") {
      steps.push({ close: true });
    } else if ((match = line.match(/^([^-+>x)]+?)\s*(-->>|->>|-->|->|--x|-x|--\)|-\))\s*([+-]?)([^:]+?)\s*:\s*(.*)$/))) {
      const from = add(match[1].trim()).id;
      const to = add(match[4].trim()).id;
      steps.push({ from, to, text: match[5], dotted: match[2].startsWith("--"), head: match[2].endsWith(">>") ? "arrow" : match[2].endsWith("x") ? "cross" : match[2].endsWith(")") ? "open" : "none" });
    }
  }
  return { participants, steps, numbered };
}

export function sequenceSvg(text) {
  const { participants, steps, numbered } = parseSequence(text);
  const GAP = 50;
  const boxes = [];
  let x = 20;
  for (const one of participants) {
    const w = Math.max(80, widest(one.label) + 30);
    boxes.push({ ...one, x: x + w / 2, w });
    x += w + GAP;
  }
  const at = new Map(boxes.map((box) => [box.id, box]));
  const parts = [];
  let y = 80;
  const frames = [];
  let count = 0;
  for (const step of steps) {
    if (step.from !== undefined) {
      const from = at.get(step.from);
      const to = at.get(step.to);
      count += 1;
      const label = numbered ? `${count}. ${step.text}` : step.text;
      const marker = step.head === "none" ? "" : ` marker-end="url(#mm-${step.head === "cross" ? "cross" : "arrow"})"`;
      if (from === to) {
        parts.push(`<path class="mm-edge${step.dotted ? " dotted" : ""}" fill="none" d="M${from.x},${y} h40 v24 h-38"${marker} />${textAt(from.x + 46, y + 12, label, "start")}`);
        y += 50;
      } else {
        parts.push(`${textAt((from.x + to.x) / 2, y - 10, label)}<line class="mm-edge${step.dotted ? " dotted" : ""}" x1="${from.x}" y1="${y + 6}" x2="${to.x}" y2="${y + 6}"${marker} />`);
        y += 44;
      }
    } else if (step.note !== undefined) {
      const over = step.over.map((id) => at.get(id));
      const w = Math.max(widest(step.note) + 20, step.where === "over" && over.length > 1 ? Math.abs(over.at(-1).x - over[0].x) + 60 : 0);
      const middle = step.where === "over" ? (over[0].x + over.at(-1).x) / 2 : step.where === "left of" ? over[0].x - w / 2 - 10 : over[0].x + w / 2 + 10;
      const h = linesOf(step.note).length * 17 + 14;
      parts.push(`<rect class="mm-note" x="${middle - w / 2}" y="${y - 8}" width="${w}" height="${h}" />${textAt(middle, y - 8 + h / 2, step.note)}`);
      y += h + 14;
    } else if (step.open) {
      frames.push({ kind: step.open, label: step.label, top: y - 14, divides: [] });
      y += 26;
    } else if (step.divide !== undefined && frames.length) {
      frames.at(-1).divides.push({ y: y - 10, label: step.divide });
      y += 26;
    } else if (step.close && frames.length) {
      const frame = frames.pop();
      const left = 8;
      const right = x - GAP + 12;
      parts.push(`<rect class="mm-frame" x="${left}" y="${frame.top}" width="${right - left}" height="${y - frame.top}" />`);
      parts.push(`<rect class="mm-frame-tab" x="${left}" y="${frame.top}" width="${width(frame.kind) + 16}" height="20" />${textAt(left + 8, frame.top + 10, frame.kind, "start", "mm-text strong")}`);
      if (frame.label) {
        parts.push(textAt(left + width(frame.kind) + 24, frame.top + 10, `[${frame.label}]`, "start"));
      }
      for (const divide of frame.divides) {
        parts.push(`<line class="mm-edge dotted" x1="${left}" y1="${divide.y}" x2="${right}" y2="${divide.y}" />${divide.label ? textAt(left + 8, divide.y + 10, `[${divide.label}]`, "start") : ""}`);
      }
      y += 12;
    }
  }
  const bottom = y + 10;
  const heads = boxes
    .map((box) => {
      const head = (top) => (box.actor ? `<circle class="mm-node" cx="${box.x}" cy="${top + 10}" r="8" /><line class="mm-node-line" x1="${box.x}" y1="${top + 18}" x2="${box.x}" y2="${top + 30}" />${textAt(box.x, top + 40, box.label)}` : `<rect class="mm-node" x="${box.x - box.w / 2}" y="${top}" width="${box.w}" height="40" />${textAt(box.x, top + 20, box.label)}`);
      return `<line class="mm-lifeline" x1="${box.x}" y1="60" x2="${box.x}" y2="${bottom}" />${head(20)}${head(bottom)}`;
    })
    .join("");
  return svg(x - GAP + 40, bottom + 60, heads + parts.join(""), "sequence");
}

// ---- Pie charts ----

const SLICES = ["#ECECFF", "#ffffde", "#d4f1d4", "#ffd8b1", "#d0e3ff", "#f9d0e0", "#e1d5f5", "#fff0b3"];

export function pieSvg(text) {
  const lines = text.split("\n").map((line) => line.trim());
  const title = lines[0].match(/^pie\s+(?:showData\s+)?title\s+(.+)$/)?.[1] ?? lines.find((line) => line.startsWith("title "))?.slice(6) ?? "";
  const slices = lines.map((line) => line.match(/^"([^"]+)"\s*:\s*([\d.]+)$/)).filter(Boolean).map(([, label, value]) => ({ label, value: Number(value) }));
  const sum = slices.reduce((all, one) => all + one.value, 0) || 1;
  const r = 110;
  const cx = 140;
  const cy = (title ? 40 : 10) + r + 10;
  let angle = -Math.PI / 2;
  const parts = [];
  slices.forEach((slice, at) => {
    const sweep = (slice.value / sum) * Math.PI * 2;
    const [x1, y1] = [cx + r * Math.cos(angle), cy + r * Math.sin(angle)];
    const [x2, y2] = [cx + r * Math.cos(angle + sweep), cy + r * Math.sin(angle + sweep)];
    const shape = slices.length === 1 ? `<circle cx="${cx}" cy="${cy}" r="${r}"` : `<path d="M${cx},${cy} L${x1},${y1} A${r},${r} 0 ${sweep > Math.PI ? 1 : 0},1 ${x2},${y2} Z"`;
    parts.push(`${shape} class="mm-slice" fill="${SLICES[at % SLICES.length]}" />`);
    const middle = angle + sweep / 2;
    parts.push(textAt(cx + r * 0.65 * Math.cos(middle), cy + r * 0.65 * Math.sin(middle), `${Math.round((slice.value / sum) * 100)}%`));
    angle += sweep;
  });
  const legend = slices.map((slice, at) => `<rect x="${cx + r + 30}" y="${cy - r + at * 24}" width="14" height="14" fill="${SLICES[at % SLICES.length]}" class="mm-slice" />${textAt(cx + r + 52, cy - r + at * 24 + 7, slice.label, "start")}`).join("");
  const wide = cx + r + 60 + Math.max(0, ...slices.map((slice) => width(slice.label)));
  return svg(wide, cy + r + 20, `${title ? textAt(wide / 2, 20, title, "middle", "mm-text strong") : ""}${parts.join("")}${legend}`, "pie");
}

// ---- All ----

function svg(w, h, inner, kind) {
  const defs = `<defs><marker id="mm-arrow" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse"><path class="mm-head" d="M0,0 L10,5 L0,10 z" /></marker><marker id="mm-arrow-back" viewBox="0 0 10 10" refX="1" refY="5" markerWidth="8" markerHeight="8" orient="auto-start-reverse"><path class="mm-head" d="M0,0 L10,5 L0,10 z" /></marker><marker id="mm-circle" viewBox="0 0 10 10" refX="5" refY="5" markerWidth="7" markerHeight="7"><circle class="mm-head" cx="5" cy="5" r="4" /></marker><marker id="mm-cross" viewBox="0 0 10 10" refX="5" refY="5" markerWidth="8" markerHeight="8"><path class="mm-edge" d="M1,1 L9,9 M9,1 L1,9" /></marker></defs>`;
  return `<svg class="mermaid-diagram ${kind}" xmlns="http://www.w3.org/2000/svg" width="${Math.ceil(w)}" height="${Math.ceil(h)}" viewBox="0 0 ${Math.ceil(w)} ${Math.ceil(h)}" role="img">${defs}${inner}</svg>`;
}

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
