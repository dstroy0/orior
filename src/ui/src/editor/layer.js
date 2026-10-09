// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A layer of the editor that holds one node for each row on screen and keeps each node's markup in
// RAM. Drawing the layer again writes only the rows whose markup changed. A key that changes one
// row costs the page that row alone, and every other row stays as the page last drew it.

export class Layer {
  constructor(host) {
    this.host = host;
    this.held = new Map();
  }

  // `parts` gives each row on screen its class and markup. `top` places a row's node, where the
  // layer's rows are placed one by one.
  draw(parts, top = null) {
    for (const [row, held] of this.held) {
      if (!parts.has(row)) {
        held.node.remove();
        this.held.delete(row);
      }
    }
    for (const [row, [name, html]] of parts) {
      let held = this.held.get(row);
      if (!held) {
        const node = document.createElement("div");
        if (top) {
          node.style.top = `${top(row)}px`;
        }
        this.host.append(node);
        held = { node, name: null, html: null };
        this.held.set(row, held);
      }
      if (held.name !== name) {
        held.node.className = name;
        held.name = name;
      }
      if (held.html !== html) {
        held.node.innerHTML = html;
        held.html = html;
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
  }
}
