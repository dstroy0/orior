// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Markdown as GitHub reads it.

import { markdownBlocks, markdownHtml } from "../../src/markdown.js";

const { test, assert } = window.__harness;

test("each block is tied to the line it came from", () => {
  const blocks = markdownBlocks("# Title\n\nA paragraph\nof two lines.\n\n- one\n- two\n");
  assert.equal(blocks.length, 3);
  assert.deepEqual(blocks.map((one) => one.match(/data-line="(\d+)"/)[1]), ["0", "2", "5"]);
});

test("task lists, tables and strikethrough", () => {
  const html = markdownHtml("- [x] done\n- [ ] not yet\n\n| a | b |\n|:--|--:|\n| 1 | 2 |\n\n~~gone~~");
  assert.match(html, /<li data-line="0" class="task"><input type="checkbox" disabled checked \/> done<\/li>/);
  assert.match(html, /<th align="left">a<\/th><th align="right">b<\/th>/);
  assert.match(html, /<del>gone<\/del>/);
});

test("an alert is a quote that opens with its kind, as GitHub writes it", () => {
  const html = markdownHtml("> [!WARNING]\n> Mind the step.\n\n> a plain quote");
  assert.match(html, /<div data-line="0" class="markdown-alert markdown-alert-warning"><p class="markdown-alert-title">Warning<\/p><p data-line="1">Mind the step.<\/p><\/div>/);
  assert.match(html, /<blockquote data-line="3">/);
});

test("footnotes are numbered as they are cited and listed at the end", () => {
  const blocks = markdownBlocks("Second[^b] then first[^a].\n\n[^a]: The first.\n[^b]: The second,\n  over two lines.\n");
  assert.match(blocks[0], /Second<sup><a href="#fn-b" id="fnref-b" class="footnote-ref">1<\/a><\/sup> then first<sup><a href="#fn-a" id="fnref-a" class="footnote-ref">2<\/a><\/sup>/);
  const notes = blocks.at(-1);
  assert.match(notes, /^<section class="footnotes" data-line="3"><ol><li id="fn-b" data-line="3">The second,\nover two lines\./);
  assert.match(notes, /<li id="fn-a" data-line="2">The first\./);
  assert.equal(blocks.length, 2, "a note's definition is not a block of its own");
});

test("math between dollars and in math fences is MathML, and a mermaid fence is a diagram", () => {
  const html = markdownHtml("Euler: $e^{i\\pi} + 1 = 0$\n\n$$\n\\frac{a}{b}\n$$\n\n```math\nx^2\n```\n\n```mermaid\ngraph TD\nA-->B\n```");
  assert.match(html, /Euler: <math><semantics>/);
  assert.equal((html.match(/<math display="block">/g) ?? []).length, 2);
  assert.match(html, /<div data-line="\d+" class="mermaid"><svg class="mermaid-diagram flowchart"/);
});

test("HTML in the text keeps only the tags and attributes allowed", () => {
  const html = markdownHtml('<div onclick="x()">hi</div>\n\n<script>alert(1)</script>\n\n[x](javascript:alert(1))');
  assert.ok(!/onclick|<script/.test(html), html);
  assert.match(html, /<a href="#">x<\/a>/);
});

test("an image's and a link's address go through the resolver", () => {
  const html = markdownHtml("![a cat](img/cat.png)", (url) => `resolved:${url}`);
  assert.match(html, /<img src="resolved:img\/cat\.png" alt="a cat" \/>/);
});
