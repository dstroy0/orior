// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Pie charts, mind maps, Gantt charts, journeys and git graphs.

import { chartSvg, plainNode } from "./flowchart.js";
import { lineOf, svg, textAt, width } from "./svg.js";

// ---- Pie charts ----

// Each slice's color, a lever of the theme's: --mm-slice-1 to --mm-slice-8.
const SLICES = Array.from({ length: 8 }, (_, at) => `var(--mm-slice-${at + 1})`);

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
    parts.push(`${shape} class="mm-slice" style="fill: ${SLICES[at % SLICES.length]}" />`);
    const middle = angle + sweep / 2;
    parts.push(textAt(cx + r * 0.65 * Math.cos(middle), cy + r * 0.65 * Math.sin(middle), `${Math.round((slice.value / sum) * 100)}%`));
    angle += sweep;
  });
  const legend = slices.map((slice, at) => `<rect x="${cx + r + 30}" y="${cy - r + at * 24}" width="14" height="14" style="fill: ${SLICES[at % SLICES.length]}" class="mm-slice" />${textAt(cx + r + 52, cy - r + at * 24 + 7, slice.label, "start")}`).join("");
  const wide = cx + r + 60 + Math.max(0, ...slices.map((slice) => width(slice.label)));
  return svg(wide, cy + r + 20, `${title ? textAt(wide / 2, 20, title, "middle", "mm-text strong") : ""}${parts.join("")}${legend}`, "pie");
}

// ---- Mind maps ----

export function parseMindmap(text) {
  const nodes = [];
  const edges = [];
  const stack = [];
  for (const raw of text.split("\n").slice(1)) {
    if (!raw.trim() || /^\s*%%/.test(raw)) {
      continue;
    }
    const indent = raw.match(/^\s*/)[0].replace(/\t/g, "    ").length;
    const line = raw.trim().replace(/::icon\([^)]*\)/, "").trim();
    if (!line) {
      continue;
    }
    let shape = "round";
    let label = line;
    const forms = [["((", "))", "circle"], ["))", "((", "bang"], [")", "(", "cloud"], ["{{", "}}", "hexagon"], ["(", ")", "round"], ["[", "]", "rect"]];
    for (const [open, close, kind] of forms) {
      const at = line.indexOf(open);
      if (at >= 0 && line.endsWith(close) && at + open.length <= line.length - close.length) {
        label = line.slice(at + open.length, line.length - close.length);
        shape = kind === "bang" ? "hexagon" : kind;
        break;
      }
    }
    const node = { id: `n${nodes.length}`, label: label.replace(/^"(.*)"$/, "$1"), shape, order: nodes.length };
    while (stack.length && stack.at(-1).indent >= indent) {
      stack.pop();
    }
    if (stack.length) {
      edges.push({ from: stack.at(-1).node.id, to: node.id });
    }
    stack.push({ indent, node });
    nodes.push(node);
  }
  return { nodes, edges };
}

export function mindmapSvg(text) {
  const read = parseMindmap(text);
  if (read.nodes[0]) {
    read.nodes[0].shape = read.nodes[0].shape === "round" ? "circle" : read.nodes[0].shape;
  }
  return chartSvg("LR", read.nodes, read.edges, plainNode, "mindmap");
}

// ---- Gantt charts ----

const DAY = 86400000;

function duration(text) {
  const match = text.match(/^(\d+(?:\.\d+)?)(ms|s|m|h|d|w)$/);
  if (!match) {
    return null;
  }
  const unit = { ms: 1, s: 1000, m: 60000, h: 3600000, d: DAY, w: 7 * DAY }[match[2]];
  return Number(match[1]) * unit;
}

export function parseGantt(text) {
  let title = "";
  let section = "";
  const tasks = [];
  const byId = new Map();
  for (const raw of text.split("\n").slice(1)) {
    const line = lineOf(raw);
    let match;
    if (!line || /^(dateFormat|axisFormat|excludes|todayMarker|tickInterval|weekday)\b/.test(line)) {
      continue;
    }
    if ((match = line.match(/^title\s+(.+)$/))) {
      title = match[1];
    } else if ((match = line.match(/^section\s+(.+)$/))) {
      section = match[1];
    } else if ((match = line.match(/^(.+?)\s*:\s*(.+)$/))) {
      const parts = match[2].split(",").map((part) => part.trim());
      const tags = [];
      while (parts.length && ["done", "active", "crit", "milestone"].includes(parts[0])) {
        tags.push(parts.shift());
      }
      let id = null;
      if (parts.length >= 3 || (parts.length === 2 && !/^\d{4}-|^after\s/.test(parts[0]) && !duration(parts[0]))) {
        id = parts.shift();
      }
      const previous = tasks.at(-1);
      let start = previous ? previous.end : Date.UTC(2000, 0, 1);
      let end = null;
      for (const part of parts) {
        const after = part.match(/^after\s+(.+)$/);
        if (after) {
          start = Math.max(...after[1].split(/\s+/).map((one) => byId.get(one)?.end ?? start));
        } else if (/^\d{4}-\d{2}-\d{2}/.test(part)) {
          const at = Date.parse(part.length === 10 ? `${part}T00:00:00Z` : part);
          if (parts.indexOf(part) === parts.length - 1 && parts.length > 1) {
            end = at;
          } else {
            start = at;
          }
        } else if (duration(part) !== null) {
          end = start + duration(part);
        }
      }
      const task = { name: match[1], section, tags, id, start, end: end ?? start + (tags.includes("milestone") ? 0 : DAY) };
      tasks.push(task);
      if (id) {
        byId.set(id, task);
      }
    }
  }
  return { title, tasks };
}

export function ganttSvg(text) {
  const { title, tasks } = parseGantt(text);
  const sections = [...new Set(tasks.map((task) => task.section))];
  const label = Math.max(80, ...sections.map((one) => width(one) + 20));
  const from = Math.min(...tasks.map((task) => task.start));
  const to = Math.max(...tasks.map((task) => task.end), from + DAY);
  const span = 640;
  const x = (at) => label + ((at - from) / (to - from)) * span;
  const top = title ? 40 : 14;
  const parts = [];
  if (title) {
    parts.push(textAt(label + span / 2, 18, title, "middle", "mm-text strong"));
  }
  tasks.forEach((task, at) => {
    const y = top + at * 26;
    if (at === 0 || tasks[at - 1].section !== task.section) {
      parts.push(`<rect class="mm-section" x="0" y="${y}" width="${label + span + 20}" height="${tasks.filter((one) => one.section === task.section).length * 26}" />${textAt(10, y + 12, task.section, "start", "mm-text strong")}`);
    }
    const kind = task.tags.includes("crit") ? " crit" : task.tags.includes("done") ? " done" : task.tags.includes("active") ? " active" : "";
    if (task.tags.includes("milestone")) {
      const mx = x(task.start);
      parts.push(`<polygon class="mm-task${kind}" points="${mx},${y + 4} ${mx + 9},${y + 13} ${mx},${y + 22} ${mx - 9},${y + 13}" />${textAt(mx + 14, y + 13, task.name, "start")}`);
    } else {
      const left = x(task.start);
      const w = Math.max(2, x(task.end) - left);
      const inside = width(task.name) + 8 < w;
      parts.push(`<rect class="mm-task${kind}" x="${left}" y="${y + 4}" width="${w}" height="18" rx="3" />${textAt(inside ? left + w / 2 : left + w + 6, y + 13, task.name, inside ? "middle" : "start", inside ? "mm-text on-task" : "mm-text")}`);
    }
  });
  const bottom = top + tasks.length * 26 + 6;
  const days = (to - from) / DAY;
  const step = days > 120 ? 30 : days > 30 ? 7 : 1;
  for (let at = from; at <= to; at += step * DAY) {
    const day = new Date(at).toISOString().slice(0, 10);
    parts.push(`<line class="mm-grid" x1="${x(at)}" y1="${top}" x2="${x(at)}" y2="${bottom}" />${textAt(x(at), bottom + 12, day.slice(5), "middle", "mm-text small")}`);
  }
  return svg(label + span + 60, bottom + 30, parts.join(""), "gantt");
}

// ---- Journeys ----

// Each score's color, a lever of the theme's: --mm-score-1 to --mm-score-5.
const SCORES = ["", ...Array.from({ length: 5 }, (_, at) => `var(--mm-score-${at + 1})`)];

export function parseJourney(text) {
  let title = "";
  let section = "";
  const tasks = [];
  for (const raw of text.split("\n").slice(1)) {
    const line = lineOf(raw);
    let match;
    if ((match = line.match(/^title\s+(.+)$/))) {
      title = match[1];
    } else if ((match = line.match(/^section\s+(.+)$/))) {
      section = match[1];
    } else if ((match = line.match(/^(.+?)\s*:\s*(\d)\s*(?::\s*(.*))?$/))) {
      tasks.push({ name: match[1], score: Math.min(5, Math.max(1, Number(match[2]))), actors: (match[3] ?? "").split(",").map((one) => one.trim()).filter(Boolean), section });
    }
  }
  return { title, tasks };
}

export function journeySvg(text) {
  const { title, tasks } = parseJourney(text);
  const column = Math.max(110, ...tasks.map((task) => width(task.name) + 20));
  const top = title ? 40 : 14;
  const parts = title ? [textAt((tasks.length * column) / 2 + 20, 18, title, "middle", "mm-text strong")] : [];
  const faces = [];
  tasks.forEach((task, at) => {
    const left = 20 + at * column;
    if (at === 0 || tasks[at - 1].section !== task.section) {
      const count = tasks.filter((one) => one.section === task.section).length;
      parts.push(`<rect class="mm-section" x="${left}" y="${top}" width="${count * column - 6}" height="24" />${textAt(left + (count * column - 6) / 2, top + 12, task.section, "middle", "mm-text strong")}`);
    }
    parts.push(`<rect class="mm-node" x="${left}" y="${top + 32}" width="${column - 6}" height="${30 + task.actors.length * 14}" />${textAt(left + (column - 6) / 2, top + 47, task.name)}${task.actors.map((actor, index) => textAt(left + (column - 6) / 2, top + 66 + index * 14, actor, "middle", "mm-text small")).join("")}`);
    const cx = left + (column - 6) / 2;
    const cy = top + 120 + (5 - task.score) * 18;
    faces.push([cx, cy]);
    parts.push(`<circle class="mm-face" cx="${cx}" cy="${cy}" r="11" style="fill: ${SCORES[task.score]}" />${textAt(cx, cy, String(task.score), "middle", "mm-text small")}`);
  });
  const path = faces.map(([fx, fy], at) => `${at ? "L" : "M"}${fx},${fy}`).join(" ");
  return svg(tasks.length * column + 40, top + 220, `<path class="mm-edge dotted" d="${path}" />${parts.join("")}`, "journey");
}

// ---- Git graphs ----

// Each lane's color, a lever of the theme's: --mm-lane-1 to --mm-lane-6.
const LANES = Array.from({ length: 6 }, (_, at) => `var(--mm-lane-${at + 1})`);

export function parseGitGraph(text) {
  const branches = [{ name: "main", lane: 0, head: null }];
  let current = branches[0];
  const commits = [];
  const commit = (fields, merged = null) => {
    const id = fields.match(/id:\s*"([^"]+)"/)?.[1] ?? `${commits.length}`;
    const tag = fields.match(/tag:\s*"([^"]+)"/)?.[1] ?? "";
    const one = { id, tag, lane: current.lane, at: commits.length, parent: current.head, merged };
    commits.push(one);
    current.head = one;
    return one;
  };
  for (const raw of text.split("\n").slice(1)) {
    const line = lineOf(raw);
    let match;
    if ((match = line.match(/^commit\b(.*)$/))) {
      commit(match[1]);
    } else if ((match = line.match(/^branch\s+(\S+)/))) {
      const branch = { name: match[1], lane: branches.length, head: current.head };
      branches.push(branch);
      current = branch;
    } else if ((match = line.match(/^(?:checkout|switch)\s+(\S+)/))) {
      current = branches.find((one) => one.name === match[1]) ?? current;
    } else if ((match = line.match(/^merge\s+(\S+)(.*)$/))) {
      const from = branches.find((one) => one.name === match[1]);
      commit(match[2], from?.head ?? null);
    } else if ((match = line.match(/^cherry-pick\s+(.*)$/))) {
      commit(match[1]);
    }
  }
  return { branches, commits };
}

export function gitGraphSvg(text) {
  const { branches, commits } = parseGitGraph(text);
  const name = Math.max(60, ...branches.map((one) => width(one.name) + 20));
  const x = (at) => name + 30 + at * 50;
  const y = (lane) => 30 + lane * 40;
  const parts = branches.map((branch) => `${textAt(10, y(branch.lane), branch.name, "start", "mm-text strong")}<line class="mm-lane" style="stroke: ${LANES[branch.lane % LANES.length]}" x1="${name}" y1="${y(branch.lane)}" x2="${x(commits.length)}" y2="${y(branch.lane)}" />`);
  const line = (from, to, lane) => `<path class="mm-git" style="stroke: ${LANES[lane % LANES.length]}" d="M${x(from.at)},${y(from.lane)} C${x(from.at) + 25},${y(from.lane)} ${x(to.at) - 25},${y(to.lane)} ${x(to.at)},${y(to.lane)}" />`;
  for (const one of commits) {
    if (one.parent && one.parent.lane !== one.lane) {
      parts.push(line(one.parent, one, one.lane));
    }
    if (one.merged) {
      parts.push(line(one.merged, one, one.merged.lane));
    }
  }
  for (const one of commits) {
    parts.push(`<circle class="mm-commit" cx="${x(one.at)}" cy="${y(one.lane)}" r="${one.merged ? 7 : 9}" style="fill: ${LANES[one.lane % LANES.length]}" />${textAt(x(one.at), y(one.lane) + 20, one.id, "middle", "mm-text small")}`);
    if (one.tag) {
      parts.push(`<rect class="mm-label" x="${x(one.at) - width(one.tag) / 2 - 5}" y="${y(one.lane) - 30}" width="${width(one.tag) + 10}" height="16" />${textAt(x(one.at), y(one.lane) - 22, one.tag, "middle", "mm-text small")}`);
    }
  }
  return svg(x(commits.length) + 20, y(branches.length - 1) + 40, parts.join(""), "git");
}

