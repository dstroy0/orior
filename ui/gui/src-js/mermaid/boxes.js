// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// State, class and entity-relationship diagrams, each drawn as a chart of boxes and lines laid out as a flowchart is.

import { chartSvg, plainNode } from "./flowchart.js";
import { escape, lineOf, textAt, width } from "./svg.js";

// ---- State diagrams ----

export function parseState(text) {
  const nodes = new Map();
  const edges = [];
  let direction = "TD";
  const state = (id) => {
    if (id === "[*]") {
      return null;
    }
    if (!nodes.has(id)) {
      nodes.set(id, { id, label: id, shape: "round", order: nodes.size });
    }
    return nodes.get(id);
  };
  const ends = { start: 0, end: 0 };
  const pole = (side) => {
    const id = `[*]${side}${ends[side]++}`;
    nodes.set(id, { id, label: "", shape: side, order: nodes.size, size: side === "start" ? [16, 16] : [22, 22] });
    return id;
  };
  for (const raw of text.split("\n").slice(1)) {
    const line = lineOf(raw);
    let match;
    if (!line || line === "}" || /^note\b/.test(line)) {
      continue;
    }
    if ((match = line.match(/^direction\s+(TB|TD|BT|LR|RL)$/))) {
      direction = match[1];
    } else if ((match = line.match(/^state\s+"([^"]+)"\s+as\s+(\S+)/))) {
      state(match[2]).label = match[1];
    } else if ((match = line.match(/^state\s+(\S+)\s+<<(choice|fork|join)>>/))) {
      const node = state(match[1]);
      node.shape = match[2] === "choice" ? "diamond" : "bar";
      node.label = match[2] === "choice" ? "" : node.label;
      node.size = match[2] === "choice" ? [28, 28] : [70, 8];
    } else if ((match = line.match(/^state\s+(\S+)\s*\{?$/))) {
      state(match[1]);
    } else if ((match = line.match(/^(\S+)\s*-->\s*(\S+)(?:\s*:\s*(.*))?$/))) {
      const from = match[1] === "[*]" ? pole("start") : state(match[1]).id;
      const to = match[2] === "[*]" ? pole("end") : state(match[2]).id;
      edges.push({ from, to, label: match[3] ?? "", end: "arrow" });
    } else if ((match = line.match(/^(\S+)\s*:\s*(.+)$/))) {
      const node = state(match[1]);
      node.label = node.label === node.id ? `${node.id}\\n${match[2]}` : `${node.label}\\n${match[2]}`;
    }
  }
  return { direction, nodes: [...nodes.values()], edges };
}

export function stateSvg(text) {
  const read = parseState(text);
  return chartSvg(read.direction, read.nodes, read.edges, plainNode, "state");
}

// ---- Class diagrams ----

const RELATIONS = [
  ["<|--", "triangle", "", false], ["--|>", "", "triangle", false], ["<|..", "triangle", "", true], ["..|>", "", "triangle", true],
  ["*--", "diamond", "", false], ["--*", "", "diamond", false], ["o--", "diamond-open", "", false], ["--o", "", "diamond-open", false],
  ["<--", "arrow", "", false], ["-->", "", "arrow", false], ["<..", "arrow", "", true], ["..>", "", "arrow", true], ["--", "", "", false], ["..", "", "", true],
];

export function parseClasses(text) {
  const classes = new Map();
  const edges = [];
  let direction = "TD";
  const named = (id) => {
    const name = id.replace(/~[^~]*~/, "");
    if (!classes.has(name)) {
      classes.set(name, { id: name, label: id.replace(/~([^~]*)~/, "<$1>"), fields: [], methods: [], kind: "", order: classes.size });
    }
    return classes.get(name);
  };
  const member = (one, line) => {
    const text = line.trim();
    if (/^<<.*>>$/.test(text)) {
      one.kind = text;
    } else if (text) {
      (text.includes("(") ? one.methods : one.fields).push(text);
    }
  };
  let open = null;
  for (const raw of text.split("\n").slice(1)) {
    const line = lineOf(raw);
    if (!line) {
      continue;
    }
    if (open) {
      if (line === "}") {
        open = null;
      } else {
        member(open, line);
      }
      continue;
    }
    let match;
    if ((match = line.match(/^direction\s+(TB|TD|BT|LR|RL)$/))) {
      direction = match[1];
    } else if ((match = line.match(/^class\s+(\S+?)\s*(\{)?$/))) {
      const one = named(match[1]);
      open = match[2] ? one : null;
    } else if ((match = line.match(/^<<(.+)>>\s+(\S+)$/))) {
      named(match[2]).kind = `<<${match[1]}>>`;
    } else {
      const relation = RELATIONS.find(([mark]) => line.includes(` ${mark} `) || new RegExp(`\\S${mark.replace(/[|*.]/g, "\\$&")}\\S`).test(line));
      if (relation) {
        const [mark, start, end, dotted] = relation;
        const at = line.indexOf(mark);
        const left = line.slice(0, at).trim().match(/^(\S+)\s*(?:"([^"]*)")?$/);
        const [right, label] = line.slice(at + mark.length).split(/\s*:\s*/);
        const rightRead = right.trim().match(/^(?:"([^"]*)")?\s*(\S+)$/);
        if (left && rightRead) {
          edges.push({ from: named(left[1]).id, to: named(rightRead[2]).id, start, end, dotted, label: label ?? "", fromLabel: left[2] ?? "", toLabel: rightRead[1] ?? "" });
          continue;
        }
      }
      if ((match = line.match(/^(\S+)\s*:\s*(.+)$/))) {
        member(named(match[1]), match[2]);
      }
    }
  }
  return { direction, classes: [...classes.values()], edges };
}

const ROW = 18;

function sectionsSize(title, sections) {
  const all = [title, ...sections.flat()];
  const w = Math.max(80, ...all.map((line) => width(line) + 20));
  const h = 28 + sections.reduce((sum, rows) => sum + 8 + Math.max(1, rows.length) * ROW, 0);
  return [w, h];
}

function sectionsSvg(node, place, title, sections, small = "") {
  const left = place.x - place.w / 2;
  const top = place.y - place.h / 2;
  const parts = [`<rect class="mm-node" x="${left}" y="${top}" width="${place.w}" height="${place.h}" />`];
  if (small) {
    parts.push(textAt(place.x, top + 10, small, "middle", "mm-text small"));
  }
  parts.push(textAt(place.x, top + (small ? 22 : 14), title, "middle", "mm-text strong"));
  let y = top + 28;
  for (const rows of sections) {
    parts.push(`<line class="mm-node-line" x1="${left}" y1="${y}" x2="${left + place.w}" y2="${y}" />`);
    y += 4;
    for (const row of rows) {
      parts.push(textAt(left + 10, y + ROW / 2, row, "start"));
      y += ROW;
    }
    y += rows.length ? 4 : ROW + 4;
  }
  return `<g class="mm-node-group" data-id="${escape(node.id)}">${parts.join("")}</g>`;
}

export function classSvg(text) {
  const read = parseClasses(text);
  const nodes = read.classes.map((one) => ({ ...one, size: sectionsSize(one.label, [one.fields, one.methods]).map((v, at) => v + (at && one.kind ? 12 : 0)) }));
  return chartSvg(read.direction, nodes, read.edges, (node, place) => sectionsSvg(node, place, node.label, [node.fields, node.methods], node.kind), "class");
}

// ---- Entity-relationship diagrams ----

const CARDINALITY = { "||": "1", "|o": "0..1", "o|": "0..1", "}o": "0..*", "o{": "0..*", "}|": "1..*", "|{": "1..*" };

export function parseEntities(text) {
  const entities = new Map();
  const edges = [];
  const entity = (id) => {
    if (!entities.has(id)) {
      entities.set(id, { id, label: id, rows: [], order: entities.size });
    }
    return entities.get(id);
  };
  let open = null;
  for (const raw of text.split("\n").slice(1)) {
    const line = lineOf(raw);
    if (!line) {
      continue;
    }
    if (open) {
      if (line === "}") {
        open = null;
      } else {
        open.rows.push(line.replace(/"[^"]*"\s*$/, "").trim().split(/\s+/).join("  "));
      }
      continue;
    }
    let match;
    if ((match = line.match(/^(\S+)\s*\{$/))) {
      open = entity(match[1]);
    } else if ((match = line.match(/^(\S+)\s+([|}][o|])(--|\.\.)([o|][|{])\s+(\S+)\s*:\s*"?([^"]*)"?$/))) {
      edges.push({ from: entity(match[1]).id, to: entity(match[5]).id, dotted: match[3] === "..", label: match[6], fromLabel: CARDINALITY[match[2]] ?? "", toLabel: CARDINALITY[match[4]] ?? "" });
    } else if ((match = line.match(/^(\S+)$/))) {
      entity(match[1]);
    }
  }
  return { entities: [...entities.values()], edges };
}

export function erSvg(text) {
  const read = parseEntities(text);
  const nodes = read.entities.map((one) => ({ ...one, size: one.rows.length ? sectionsSize(one.label, [one.rows]) : [Math.max(90, width(one.label) + 30), 40] }));
  return chartSvg("TD", nodes, read.edges, (node, place) => (node.rows.length ? sectionsSvg(node, place, node.label, [node.rows]) : plainNode({ ...node, shape: "rect" }, place)), "er");
}

