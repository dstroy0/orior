// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Sequence diagrams: participants in a row over their lifelines, messages and notes in order down them, loops and
// alternatives framed.

import { linesOf, svg, textAt, widest, width } from "./svg.js";

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
      frames.push({ type: step.open, label: step.label, top: y - 14, divides: [] });
      y += 26;
    } else if (step.divide !== undefined && frames.length) {
      frames.at(-1).divides.push({ y: y - 10, label: step.divide });
      y += 26;
    } else if (step.close && frames.length) {
      const frame = frames.pop();
      const left = 8;
      const right = x - GAP + 12;
      parts.push(`<rect class="mm-frame" x="${left}" y="${frame.top}" width="${right - left}" height="${y - frame.top}" />`);
      parts.push(`<rect class="mm-frame-tab" x="${left}" y="${frame.top}" width="${width(frame.type) + 16}" height="20" />${textAt(left + 8, frame.top + 10, frame.type, "start", "mm-text strong")}`);
      if (frame.label) {
        parts.push(textAt(left + width(frame.type) + 24, frame.top + 10, `[${frame.label}]`, "start"));
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

