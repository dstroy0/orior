// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A layer of the editor that holds one node for each row it draws and keeps each node's markup in
// RAM. Drawing the layer again writes only the rows whose markup changed. A key that changes one
// row costs the page that row alone, and every other row stays as the page last drew it. A row
// drawn and out of sight stays where the drawing keeps it, and the layer is drawn outward from what
// is in sight by `extend`. `made` counts the rows made since the layer was made, and `writes` the
// rows written again.
//
// A layer that flows holds its rows in blocks of BLOCK, each block placed where its first row stands and
// its rows one under the next in it, a row after a gap by its margin: the page paints a block as one
// piece, a change repainting the block it is in, and lays out a few blocks and not a piece a row. A
// row already in place stays where it is as the rows round it change.

// The rows a block of a layer that flows holds.
const BLOCK = 4;

export class Layer {
  constructor(host, { flow = false } = {}) {
    this.host = host;
    this.flow = flow;
    this.held = new Map();
    this.writes = 0;
    this.made = 0;
    this.blocks = null;
  }

  // `parts` gives each row on screen its class and markup. `top` places a row's node, where the
  // layer's rows are placed one by one, and the first row's, where they flow. A row `keep` says to
  // keep stays though `parts` does not give it.
  draw(parts, top = null, keep = null) {
    for (const [row, held] of this.held) {
      if (!parts.has(row) && !keep?.(row)) {
        held.node.remove();
        this.held.delete(row);
      }
    }
    this.write(parts, top);
  }

  // Draws the rows `parts` gives and keeps every other row drawn: the layer drawn outward.
  extend(parts, top = null) {
    this.write(parts, top);
  }

  write(parts, top) {
    for (const [row, [name, html]] of parts) {
      let held = this.held.get(row);
      if (!held) {
        const node = document.createElement("div");
        if (top && !this.flow) {
          node.style.top = `${top(row)}px`;
        }
        if (!this.flow) {
          this.host.append(node);
        }
        held = { node, name: null, html: null, gap: 0 };
        this.held.set(row, held);
      }
      if (held.name !== name) {
        held.node.className = name;
        held.name = name;
      }
      if (held.html !== html) {
        held.node.innerHTML = html;
        if (held.html === null) {
          this.made += 1;
        } else {
          this.writes += 1;
        }
        held.html = html;
      }
    }
    if (this.flow) {
      this.place(top);
    }
  }

  // Puts the rows of a layer that flows in blocks of BLOCK rows by their numbers, each block placed
  // where `top` puts its first row and its rows in their order in it, a row after a gap by the rows
  // missing before it.
  place(top) {
    const rows = [...this.held.keys()].sort((a, b) => a - b);
    const blocks = new Map();
    for (const row of rows) {
      const number = Math.floor(row / BLOCK);
      if (!blocks.has(number)) {
        blocks.set(number, []);
      }
      blocks.get(number).push(row);
    }
    this.blocks ??= new Map();
    for (const [number, block] of this.blocks) {
      if (!blocks.has(number)) {
        block.node.remove();
        this.blocks.delete(number);
      }
    }
    for (const [number, inside] of blocks) {
      let block = this.blocks.get(number);
      if (!block) {
        block = { node: document.createElement("div"), top: null };
        block.node.className = "ed-block";
        this.host.append(block.node);
        this.blocks.set(number, block);
      }
      const placed = top ? top(number * BLOCK) : 0;
      if (placed !== block.top) {
        block.node.style.top = `${placed}px`;
        block.top = placed;
      }
      let at = block.node.firstChild;
      let before = number * BLOCK - 1;
      for (const row of inside) {
        const held = this.held.get(row);
        const gap = row - before - 1;
        if (held.gap !== gap) {
          held.node.style.marginTop = gap ? `calc(var(--code-line) * ${gap})` : "";
          held.gap = gap;
        }
        if (held.node === at) {
          at = at.nextSibling;
        } else {
          block.node.insertBefore(held.node, at);
        }
        before = row;
      }
    }
  }

  // The rows' numbers moved on by `by`, as lines are put in above them. Each node keeps its place
  // and its markup under its new number.
  shift(by) {
    if (by) {
      this.held = new Map([...this.held].map(([row, held]) => [row + by, held]));
    }
  }

  clear() {
    for (const held of this.held.values()) {
      held.node.remove();
    }
    this.held.clear();
    for (const block of this.blocks?.values() ?? []) {
      block.node.remove();
    }
    this.blocks?.clear();
  }
}
