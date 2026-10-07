// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The lines of a text that differ from an older text of it: added, changed, and where lines were
// taken out. The lines the two share at their start and end are set aside first, and Myers' shortest
// edit runs on what is left. It holds one band of its frontier for each edit: what it keeps grows
// with the edits and not with the text. Past MOST_EDITS the texts read as too far apart to mark.

const MOST_EDITS = 3000;

// The shortest edit from `a` to `b`, arrays of numbers, as [kind, index] in order: kind 0 keeps a[i],
// 1 takes a[i] out, 2 puts b[i] in. Null past MOST_EDITS.
function shortest(a, b) {
  const n = a.length;
  const m = b.length;
  const most = Math.min(n + m, MOST_EDITS);
  const middle = most + 1;
  const v = new Int32Array(2 * most + 3);
  const trace = [];
  let found = -1;
  for (let d = 0; d <= most && found < 0; d += 1) {
    trace.push(v.slice(middle - d, middle + d + 1));
    for (let k = -d; k <= d; k += 2) {
      let x = k === -d || (k !== d && v[middle + k - 1] < v[middle + k + 1]) ? v[middle + k + 1] : v[middle + k - 1] + 1;
      let y = x - k;
      while (x < n && y < m && a[x] === b[y]) {
        x += 1;
        y += 1;
      }
      v[middle + k] = x;
      if (x >= n && y >= m) {
        found = d;
        break;
      }
    }
  }
  if (found < 0) {
    return null;
  }
  const steps = [];
  let x = n;
  let y = m;
  for (let d = found; d > 0; d -= 1) {
    const band = trace[d];
    const at = (k) => band[k + d];
    const k = x - y;
    const back = k === -d || (k !== d && at(k - 1) < at(k + 1)) ? k + 1 : k - 1;
    const backX = at(back);
    const backY = backX - back;
    while (x > backX && y > backY) {
      x -= 1;
      y -= 1;
      steps.push([0, x]);
    }
    if (x === backX) {
      y -= 1;
      steps.push([2, y]);
    } else {
      x -= 1;
      steps.push([1, x]);
    }
  }
  while (x > 0 && y > 0) {
    x -= 1;
    y -= 1;
    steps.push([0, x]);
  }
  return steps.reverse();
}

// The marks for `now`, its lines in order, against `then`: { added, changed } sets of lines of `now`,
// `removed`, the lines of `now` above which lines were taken out, `now.length` for below the last,
// and `hunks`, each change in order as { now: [from, to), then: [from, to) }, the lines it spans in
// each text. Null where the two are too far apart to mark.
export function lineChanges(then, now) {
  const added = new Set();
  const changed = new Set();
  const removed = new Set();
  const hunks = [];
  let start = 0;
  while (start < then.length && start < now.length && then[start] === now[start]) {
    start += 1;
  }
  let end = 0;
  while (end < then.length - start && end < now.length - start && then[then.length - 1 - end] === now[now.length - 1 - end]) {
    end += 1;
  }
  const ids = new Map();
  const id = (line) => {
    if (!ids.has(line)) {
      ids.set(line, ids.size);
    }
    return ids.get(line);
  };
  const a = then.slice(start, then.length - end).map(id);
  const b = now.slice(start, now.length - end).map(id);
  const steps = shortest(a, b);
  if (!steps) {
    return null;
  }
  // Each run of steps between two kept lines is one change: lines put in where none came out are
  // added, lines put in where some came out are changed, and lines only taken out mark where.
  let line = start;
  let thenLine = start;
  let out = 0;
  let put = [];
  const close = () => {
    if (put.length) {
      put.forEach((at) => (out ? changed : added).add(at));
    } else if (out) {
      removed.add(line);
    }
    if (put.length || out) {
      hunks.push({ now: [line - put.length, line], then: [thenLine - out, thenLine] });
    }
    out = 0;
    put = [];
  };
  for (const [kind] of steps) {
    if (kind === 0) {
      close();
      line += 1;
      thenLine += 1;
    } else if (kind === 1) {
      out += 1;
      thenLine += 1;
    } else {
      put.push(line);
      line += 1;
    }
  }
  close();
  return { added, changed, removed, hunks };
}
