// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A page of HTML, CSS or SVG for the live preview: its elements tied to their lines, a stylesheet's
// rules and the page made from its selectors, a linked sheet taken from a tab, and a change patched
// where it changed.

import { annotate, cssPage, cssRules, elementAt, LINE, morph, pageOf, rebase, selectorElements } from "../../src/pagepreview.js";

const { test, assert } = window.__harness;

test("each element's start tag is told its line, a comment's and a script's text left as they are", () => {
  const text = '<div>\n<!-- <p> -->\n<p class="a">x</p>\n<script>if (a<b) {}</script>\n<br/>';
  assert.equal(annotate(text), `<div ${LINE}="0">\n<!-- <p> -->\n<p ${LINE}="2" class="a">x</p>\n<script ${LINE}="3">if (a<b) {}</script>\n<br ${LINE}="4"/>`);
});

test("a stylesheet's rules are read with their lines, inside @media too, other at-rules passed over", () => {
  const css = "/* a { } */\nh1 { color: red }\n@media (min-width: 1px) {\n  .a > b,\n  i { x: '}' }\n}\n@font-face { font-family: X }\np::before { content: \"{\" }";
  assert.deepEqual(cssRules(css), [
    { selector: "h1", line: 1 },
    { selector: ".a > b, i", line: 3 },
    { selector: "p::before", line: 7 },
  ]);
});

test("a selector is built as the elements it applies to, the last holding its text and tied to its rule's line", () => {
  const doc = document.implementation.createHTMLDocument("");
  const holder = doc.createElement("div");
  holder.append(selectorElements(doc, "nav ul > li.on + li a[href='#x']:hover", 4));
  assert.equal(holder.innerHTML, `<nav><ul><li class="on"></li><li><a href="#x" ${LINE}="4">nav ul &gt; li.on + li a[href='#x']:hover</a></li></ul></nav>`);
  assert.equal(selectorElements(doc, "head meta", 1), null, "an element not seen");
  holder.replaceChildren(selectorElements(doc, "body > p", 2));
  assert.equal(holder.innerHTML, `<p ${LINE}="2">body &gt; p</p>`, "the page's body stands for itself");
});

test("a CSS file's page holds the elements of each selector of each rule, in rule order", () => {
  const doc = document.implementation.createHTMLDocument("");
  cssPage(doc, "h1, .lead { x: y }\ninput[type=checkbox] {}\n");
  assert.deepEqual(
    [...doc.body.children].map((one) => [one.localName, one.getAttribute(LINE)]),
    [
      ["h1", "0"],
      ["div", "0"],
      ["input", "1"],
    ]
  );
  assert.equal(doc.body.querySelector("input").type, "checkbox");
});

test("an address in a stylesheet is read from the sheet's own folder", () => {
  const css = "a { background: url(img/x.png) } @import 'b.css'; i { background: url(data:x) }";
  assert.equal(rebase(css, "http://view.localhost/css/"), "a { background: url(http://view.localhost/css/img/x.png) } @import 'http://view.localhost/css/b.css'; i { background: url(data:x) }");
});

test("a page takes a linked sheet a tab holds in place of the link, and its addresses from the file's folder", () => {
  const sheet = (href) => (href === "s.css" ? { text: "p { color: red }", base: "http://view.localhost/site/" } : null);
  const doc = pageOf("html", '<html><head><link rel="stylesheet" href="s.css"><link rel="stylesheet" href="other.css"></head><body><p>x</p></body></html>', { base: "http://view.localhost/site/", sheet });
  assert.equal(doc.querySelector("base").href, "http://view.localhost/site/");
  assert.equal(doc.querySelector('style[data-orior-sheet="s.css"]').textContent, "p { color: red }");
  assert.equal(doc.querySelectorAll("link").length, 1, "the sheet no tab holds stays a link");
  assert.equal(pageOf("svg", "<svg><g></svg>", { base: "http://view.localhost/" }), null, "SVG that does not read as XML");
  const svg = pageOf("svg", '<?xml version="1.0"?>\n<svg xmlns="http://www.w3.org/2000/svg"><rect width="5" height="5"/></svg>', { base: "http://view.localhost/" });
  assert.equal(svg.body.querySelector("rect").getAttribute(LINE), "1");
  const css = pageOf("css", "p { color: blue }", { base: "http://view.localhost/" });
  assert.equal(css.querySelector("style[data-orior-sheet]").textContent, "p { color: blue }");
  assert.equal(css.body.querySelector("p").textContent, "p");
});

test("a page is patched where it changed, each node of the same kind in the same place kept", () => {
  const live = document.createElement("div");
  live.innerHTML = "<p>one</p><p>two</p><input value='a'>";
  const input = live.querySelector("input");
  const fresh = document.createElement("div");
  fresh.innerHTML = "<p>one</p><p>2</p><input value='a'><b>new</b>";
  const counts = morph(live, fresh);
  assert.equal(live.innerHTML, fresh.innerHTML);
  assert.equal(live.querySelector("input"), input, "the field kept");
  assert.deepEqual(counts, { kept: 5, changed: 1, added: 1, removed: 0 });
  const less = document.createElement("div");
  less.innerHTML = "<p>one</p>";
  assert.deepEqual(morph(live, less), { kept: 3, changed: 0, added: 0, removed: 3 });
});

test("the element of a line is the innermost drawn tied to it or to the nearest line before it", () => {
  const host = document.createElement("div");
  host.innerHTML = annotate("<div>\n<p>a</p>\n\n<span>b</span><i hidden>c</i></div>");
  document.body.append(host);
  try {
    assert.equal(elementAt(document, 1).textContent, "a");
    assert.equal(elementAt(document, 2).textContent, "a", "a line with no tag of its own");
    assert.equal(elementAt(document, 3).localName, "span", "one not drawn passed over");
    assert.equal(elementAt(document, 0).localName, "div");
  } finally {
    host.remove();
  }
});
