// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Flowcharts, and the charts of boxes and lines that states, classes, entities and a mind map's ideas are drawn as,
// laid out as a flowchart is.

import { escape, linesOf, svg, textAt, widest } from "./svg.js";

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
  for (const [open, close, type] of SHAPES) {
    if (rest.startsWith(open)) {
      const end = rest.indexOf(close, open.length);
      if (end >= 0) {
        label = rest.slice(open.length, end).trim().replace(/^"(.*)"$/, "$1");
        shape = type;
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
      const type = plain ? plain[2] : worded[4];
      const label = plain ? plain[3] ?? "" : worded[3];
      const both = Boolean((plain ?? worded)[1]);
      const next = readGroup(rest.slice((plain ?? worded)[0].length), nodes);
      if (!next) {
        break;
      }
      for (const from of group.ids) {
        for (const to of next.ids) {
          edges.push({ from, to, label: label.trim(), dotted: type.includes("."), thick: type.startsWith("="), arrow: type.endsWith(">") || type.endsWith("o") || type.endsWith("x"), end: type.at(-1), both });
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
  const sizes = new Map(chart.nodes.map((node) => [node.id, node.size ?? nodeSize(node)]));
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
    case "start":
      return `<circle class="mm-solid" cx="${x}" cy="${y}" r="${w / 2}" />`;
    case "end":
      return `<circle class="mm-node" cx="${x}" cy="${y}" r="${w / 2}" /><circle class="mm-solid" cx="${x}" cy="${y}" r="${w / 2 - 4}" />`;
    case "bar":
      return `<rect class="mm-solid" x="${left}" y="${top}" width="${w}" height="${h}" />`;
    case "cloud":
      return `<rect class="mm-node" x="${left}" y="${top}" width="${w}" height="${h}" rx="${h / 2}" ry="${h / 3}" />`;
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

// ---- Charts of boxes and lines ----

// The lines of a chart's edges, box to box: each with the marks its type sets at its ends, dotted
// where it is, its label at its middle and the words of each end beside that end.
function edgesSvg(edges, places) {
  const parts = [];
  for (const edge of edges) {
    const from = places.get(edge.from);
    const to = places.get(edge.to);
    if (!from || !to) {
      continue;
    }
    const [x1, y1] = edgeOf(from, to.x, to.y);
    const [x2, y2] = edgeOf(to, from.x, from.y);
    const start = edge.start ? ` marker-start="url(#mm-${edge.start})"` : "";
    const end = edge.end ? ` marker-end="url(#mm-${edge.end})"` : "";
    parts.push(`<line class="mm-edge${edge.dotted ? " dotted" : ""}${edge.thick ? " thick" : ""}" x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}"${start}${end} />`);
    if (edge.label) {
      const mx = (x1 + x2) / 2;
      const my = (y1 + y2) / 2;
      const w = widest(edge.label) + 10;
      const h = linesOf(edge.label).length * 17 + 4;
      parts.push(`<rect class="mm-label" x="${mx - w / 2}" y="${my - h / 2}" width="${w}" height="${h}" />${textAt(mx, my, edge.label)}`);
    }
    const beside = (x, y, dx, dy, text) => text && parts.push(textAt(x + dx * 18 - dy * 10, y + dy * 18 + dx * 10, text, "middle", "mm-text small"));
    const length = Math.hypot(x2 - x1, y2 - y1) || 1;
    beside(x1, y1, (x2 - x1) / length, (y2 - y1) / length, edge.fromLabel);
    beside(x2, y2, (x1 - x2) / length, (y1 - y2) / length, edge.toLabel);
  }
  return parts.join("");
}

// A chart of `nodes` and `edges` laid out as a flowchart runs `direction`, each node drawn by `draw`.
export function chartSvg(direction, nodes, edges, draw, type) {
  const chart = { direction, nodes, edges };
  const { places, total } = layoutFlowchart(chart);
  const boxes = nodes.map((node) => draw(node, places.get(node.id))).join("");
  return svg(total.w, total.h, edgesSvg(edges, places) + boxes, type);
}

export const plainNode = (node, place) => `<g class="mm-node-group" data-id="${escape(node.id)}">${shapeSvg(node, place)}${node.shape === "start" || node.shape === "end" || node.shape === "bar" ? "" : textAt(place.x, place.y, node.label)}</g>`;

