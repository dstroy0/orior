// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A layer's rows: written once and again only where their markup changes, kept where the drawing
// keeps them, drawn outward, and, where the layer flows, held in blocks in their order.

import { Layer } from "../../../gui/src-js/editor/layer.js";

const { test, assert } = window.__harness;

const parts = (rows) => new Map(rows.map((row) => [row, ["ed-row", `row ${row}`]]));

// The rows of a layer that flows, block by block, each block's rows by their text.
function blocksOf(host) {
  return [...host.children].map((block) => [...block.children].map((node) => node.textContent));
}

test("a row is made once and written again only where its markup changes", () => {
  const layer = new Layer(document.createElement("div"));
  layer.draw(parts([0, 1, 2]));
  assert.deepEqual([layer.made, layer.writes], [3, 0]);
  layer.draw(parts([0, 1, 2]));
  assert.deepEqual([layer.made, layer.writes], [3, 0], "the same markup writes nothing");
  layer.draw(new Map([...parts([0, 2]), [1, ["ed-row", "changed"]]]));
  assert.deepEqual([layer.made, layer.writes], [3, 1], "one row written again");
});

test("a row written again is put in place by its own writer where it has one that takes it, else written whole", () => {
  const layer = new Layer(document.createElement("div"));
  layer.draw(parts([0, 1]));
  const node = layer.held.get(0).node;
  const placed = [];
  const inPlace = (text) => (held) => {
    placed.push(text);
    held.textContent = text;
    return true;
  };
  layer.draw(new Map([[0, ["ed-row", "row 0 again", inPlace("row 0 again")]], [1, ["ed-row", "row 1", inPlace("never")]]]));
  assert.equal(layer.held.get(0).node, node, "the row keeps its node");
  assert.deepEqual([node.textContent, placed, layer.writes], ["row 0 again", ["row 0 again"], 1], "written in place, and a row unchanged not at all");
  layer.draw(new Map([[0, ["ed-row", "<b>whole</b>", () => false]]]));
  assert.equal(node.innerHTML, "<b>whole</b>", "a writer that does not take it leaves the row written whole");
});

test("a row the drawing lets go gives its node to a row that comes in, which its writer puts in place, and a node no row takes goes", () => {
  const host = document.createElement("div");
  const layer = new Layer(host, { flow: true });
  layer.draw(parts([0, 1, 2]), (row) => row * 20);
  const nodes = new Set([0, 1, 2].map((row) => layer.held.get(row).node));
  const placed = [];
  const inPlace = (row) => (node) => {
    placed.push(row);
    node.textContent = `row ${row}`;
    return true;
  };
  layer.draw(new Map([9, 10].map((row) => [row, ["ed-row", `row ${row}`, inPlace(row)]])), (row) => row * 20);
  assert.ok([9, 10].every((row) => nodes.has(layer.held.get(row).node)), "the rows that came in hold nodes the rows let go held");
  assert.deepEqual(placed.sort((a, b) => a - b), [9, 10], "each put in place by its writer");
  assert.deepEqual(blocksOf(host), [["row 9", "row 10"]], "the node no row took gone, the rows in their block");
  assert.deepEqual([layer.made, layer.writes], [3, 2], "no node made");
});

test("a row kept where its top moves is placed again where it stands", () => {
  const layer = new Layer(document.createElement("div"));
  layer.draw(parts([0, 1]), (row) => row * 20);
  const node = layer.held.get(1).node;
  layer.draw(parts([0, 1]), (row) => (row - 4) * 20);
  assert.equal(layer.held.get(1).node, node, "the row keeps its node");
  assert.equal(node.style.top, "-60px");
});

test("a row the drawing keeps stays, and drawing outward adds rows and lets none go", () => {
  const host = document.createElement("div");
  const layer = new Layer(host, { flow: true });
  layer.draw(parts([10, 11, 12]), (row) => row * 20);
  layer.extend(parts([13, 14, 15]), (row) => row * 20);
  assert.deepEqual([...layer.held.keys()].sort((a, b) => a - b), [10, 11, 12, 13, 14, 15]);
  layer.draw(parts([11, 12]), (row) => row * 20, (row) => row >= 11 && row <= 14);
  assert.deepEqual([...layer.held.keys()].sort((a, b) => a - b), [11, 12, 13, 14], "the rows kept, and the rest let go");
  layer.draw(parts([12]), (row) => row * 20);
  assert.deepEqual([...layer.held.keys()], [12], "kept by nothing, a row out of the drawing goes");
});

test("a layer that flows holds its rows in blocks of four in their order, each block where its first row stands", () => {
  const host = document.createElement("div");
  const layer = new Layer(host, { flow: true });
  layer.draw(parts([9, 5, 6, 4]), (row) => row * 20);
  layer.extend(parts([3, 1]), (row) => row * 20);
  assert.deepEqual(blocksOf(host).sort(), [["row 1", "row 3"], ["row 4", "row 5", "row 6"], ["row 9"]].sort());
  const block = [...host.children].find((node) => node.textContent.startsWith("row 1"));
  assert.equal(block.style.top, "0px", "the block of rows 0 to 3 stands where row 0 would");
  const one = [...block.children][0];
  const three = [...block.children][1];
  assert.equal(one.style.marginTop, "calc(var(--code-line) * 1)", "row 1 after the missing row 0");
  assert.equal(three.style.marginTop, "calc(var(--code-line) * 1)", "row 3 after the missing row 2");
  layer.clear();
  assert.equal(host.children.length, 0, "cleared, the layer holds nothing");
});
