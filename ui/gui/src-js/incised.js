// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A word incised: its Roman capitals drawn as the cuts a stylus makes in clay. Each stroke is cut the
// way a hand cuts it, down and to the right, and each cut is widest where the stylus went in and
// narrows to its tail as the stylus lifts.

const SVG = "http://www.w3.org/2000/svg";

// A cut's half width where the stylus went in and at its tail, and the space between two characters, on a
// cap height of 10.
const HEAD = 1.0;
const TAIL = 0.36;
const SPACE = 2.6;

// The points round an arc about `centerX`, `centerY` of radius `radius`, from angle `from` to `to`.
const arc = (centerX, centerY, radius, from, to) =>
  Array.from({ length: 13 }, (_, step) => {
    const angle = from + ((to - from) * step) / 12;
    return [centerX + radius * Math.cos(angle), centerY + radius * Math.sin(angle)];
  });

// Each capital's width and its strokes, each stroke the points the stylus passes in the order it cuts
// them: a bowl from the stem round and back to it.
const CAPITALS = {
  T: { wide: 8, strokes: [[[0, 0], [8, 0]], [[4, 0], [4, 10]]] },
  A: { wide: 9, strokes: [[[4.5, 0], [0, 10]], [[4.5, 0], [9, 10]], [[2.2, 6.6], [6.8, 6.6]]] },
  B: {
    wide: 7.1,
    strokes: [
      [[0, 0], [0, 10]],
      [[0, 0], ...arc(4, 2.4, 2.4, -Math.PI / 2, Math.PI / 2), [0, 4.8]],
      [[0, 4.8], ...arc(4.5, 7.4, 2.6, -Math.PI / 2, Math.PI / 2), [0, 10]],
    ],
  },
  V: { wide: 9, strokes: [[[0, 0], [4.5, 10]], [[9, 0], [4.5, 10]]] },
  L: { wide: 6.5, strokes: [[[0, 0], [0, 10]], [[0, 10], [6.5, 10]]] },
};

const apart = ([fromX, fromY], [toX, toY]) => Math.hypot(toX - fromX, toY - fromY);

// The outline of the cut along `points`: one side from head to tail and the other back, its half width
// falling from HEAD to TAIL as the stylus lifts.
function cut(points) {
  const reached = [0];
  points.slice(1).forEach((point, index) => reached.push(reached[index] + apart(points[index], point)));
  const length = reached.at(-1);
  const steps = Math.max(8, Math.ceil(length * 2));
  const left = [];
  const right = [];
  let segment = 0;
  for (let step = 0; step <= steps; step += 1) {
    const along = (length * step) / steps;
    while (segment < points.length - 2 && reached[segment + 1] < along) {
      segment += 1;
    }
    const [fromX, fromY] = points[segment];
    const [toX, toY] = points[segment + 1];
    const span = reached[segment + 1] - reached[segment];
    const share = span ? (along - reached[segment]) / span : 0;
    const x = fromX + (toX - fromX) * share;
    const y = fromY + (toY - fromY) * share;
    const lifted = 1 - step / steps;
    const half = TAIL + (HEAD - TAIL) * lifted * lifted;
    const normalX = (-(toY - fromY) / span) * half;
    const normalY = ((toX - fromX) / span) * half;
    left.push([x + normalX, y + normalY]);
    right.push([x - normalX, y - normalY]);
  }
  const outline = [...left, ...right.reverse()];
  return `M${outline.map(([x, y]) => `${x.toFixed(2)} ${y.toFixed(2)}`).join("L")}Z`;
}

// `word`, its characters among CAPITALS, incised: an svg the height of its capitals and as wide as they
// are, read as nothing of its own.
export function incised(word) {
  const svg = document.createElementNS(SVG, "svg");
  svg.setAttribute("class", "incised");
  svg.setAttribute("aria-hidden", "true");
  svg.dataset.word = word;
  const cuts = document.createElementNS(SVG, "g");
  cuts.setAttribute("class", "cuts");
  let x = 0;
  for (const character of word) {
    const { wide, strokes } = CAPITALS[character];
    const group = document.createElementNS(SVG, "g");
    group.setAttribute("class", "character");
    group.setAttribute("transform", `translate(${x} 0)`);
    for (const stroke of strokes) {
      const path = document.createElementNS(SVG, "path");
      path.setAttribute("class", "cut");
      path.setAttribute("d", cut(stroke));
      group.append(path);
    }
    cuts.append(group);
    x += wide + SPACE;
  }
  svg.append(cuts);
  const edge = HEAD + 0.3;
  svg.setAttribute("viewBox", `${-edge} ${-edge} ${x - SPACE + 2 * edge} ${10 + 2 * edge}`);
  return svg;
}
