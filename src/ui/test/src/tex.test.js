// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// TeX's math as MathML.

import { texToMathml } from "../../src/tex.js";

const { test, assert } = window.__harness;

const inner = (tex, display = false) => texToMathml(tex, display).replace(/<annotation[\s\S]*<\/annotation>/, "");

test("letters, numbers, operators and scripts", () => {
  assert.equal(inner("x^2 + y_1"), "<math><semantics><mrow><msup><mi>x</mi><mn>2</mn></msup><mo>+</mo><msub><mi>y</mi><mn>1</mn></msub></mrow></semantics></math>");
  assert.match(inner("a_i^{2}"), /<msubsup><mi>a<\/mi><mi>i<\/mi><mrow><mn>2<\/mn><\/mrow><\/msubsup>/);
  assert.match(inner("f'"), /<msup><mi>f<\/mi><mo>′<\/mo><\/msup>/);
});

test("fractions, roots and binomials", () => {
  assert.match(inner("\\frac{1}{2}"), /<mfrac><mrow><mn>1<\/mn><\/mrow><mrow><mn>2<\/mn><\/mrow><\/mfrac>/);
  assert.match(inner("\\sqrt[3]{x}"), /<mroot><mrow><mi>x<\/mi><\/mrow><mrow><mn>3<\/mn><\/mrow><\/mroot>/);
  assert.match(inner("\\binom{n}{k}"), /linethickness="0"/);
});

test("a big operator's limits go over and under it in display, and beside it in the line", () => {
  assert.match(inner("\\sum_{i=1}^n i", true), /<munderover><mo largeop="true" movablelimits="false">∑<\/mo>/);
  assert.match(inner("\\sum_{i=1}^n i"), /<msubsup><mo largeop="true" movablelimits="true">∑<\/mo>/);
  assert.match(inner("\\int_0^1 x", true), /<msubsup><mo[^>]*>∫<\/mo>/, "an integral's limits stand beside it");
});

test("Greek letters, symbols, functions and type faces", () => {
  assert.match(inner("\\alpha \\Omega \\infty \\le \\to"), /<mi>α<\/mi><mi mathvariant="normal">Ω<\/mi><mi>∞<\/mi><mo>≤<\/mo><mo>→<\/mo>/);
  assert.match(inner("\\sin x"), /<mi>sin<\/mi><mi>x<\/mi>/);
  assert.match(inner("\\mathbb{R}"), /<mi mathvariant="double-struck">R<\/mi>/);
  assert.match(inner("\\text{if } x"), /<mtext>if <\/mtext>/);
});

test("delimiters sized to what they hold, matrices and cases", () => {
  assert.match(inner("\\left( x \\right)"), /<mo fence="true" stretchy="true">\(<\/mo><mi>x<\/mi><mo fence="true" stretchy="true">\)<\/mo>/);
  const matrix = inner("\\begin{bmatrix} 1 & 2 \\\\ 3 & 4 \\end{bmatrix}", true);
  assert.equal((matrix.match(/<mtr>/g) ?? []).length, 2);
  assert.equal((matrix.match(/<mtd>/g) ?? []).length, 4);
  assert.match(matrix, /<mo fence="true" stretchy="true">\[<\/mo><mtable>/);
  assert.match(inner("\\begin{cases} 1 & x \\\\ 0 & y \\end{cases}", true), /<mo fence="true" stretchy="true">{<\/mo><mtable columnalign="left left">/);
});

test("a command it does not know is shown as written and marked", () => {
  assert.match(inner("\\nosuch x"), /<merror><mtext>\\nosuch<\/mtext><\/merror><mi>x<\/mi>/);
  assert.match(texToMathml("<x>", false), /<mo>&lt;<\/mo>/, "the page's own marks are escaped");
});
