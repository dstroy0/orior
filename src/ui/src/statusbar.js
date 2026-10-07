// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The bar along the bottom of the window. On its left the branch the tree is on, with a star where a
// file differs from the last commit, then the runs going and the files still being read. On its right
// the editor's own line: where the cursor is, what is chosen, the indent, the line ends and the
// language, shown in the edit view only.

const SVG = "http://www.w3.org/2000/svg";

// A branch mark: two commits on one line and a third off it, joined.
function branchMark() {
  const svg = document.createElementNS(SVG, "svg");
  svg.setAttribute("viewBox", "0 0 16 16");
  svg.setAttribute("aria-hidden", "true");
  svg.setAttribute("class", "branch-mark");
  const path = document.createElementNS(SVG, "path");
  path.setAttribute("d", "M5 3.5v9M5 10.5c0-3 6-2.5 6-5.5");
  svg.append(path);
  for (const [cx, cy] of [
    [5, 3],
    [5, 13],
    [11, 4.5],
  ]) {
    const circle = document.createElementNS(SVG, "circle");
    circle.setAttribute("cx", String(cx));
    circle.setAttribute("cy", String(cy));
    circle.setAttribute("r", "1.7");
    svg.append(circle);
  }
  return svg;
}

export function drawBranch(branch, changed) {
  const node = document.getElementById("status-branch");
  node.hidden = !branch;
  if (branch) {
    node.replaceChildren(branchMark(), `${branch}${changed ? "*" : ""}`);
    node.title = branch;
  }
}
