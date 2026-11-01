// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Parameter Info from the tree: the window's side answers the call the cursor is in off the leaves of the text it holds,
// asked by the text's key and the cursor's place, its arguments' types and every declaration of its name in order of
// fit; the page shows the first with the argument the cursor stands in marked and each argument that differs marked,
// lists the others, and says where there is none. A server's answer is laid below the tree's where its parameters
// differ.

import { closeSignature, parameterInfo, SIGNATURE_REST, signatureOpen, typed } from "../../../gui/src-js/intel/signature.js";
import { state } from "../../../gui/src-js/intel/state.js";
import { given, made, putBackAsFound, told } from "../../menus.js";

const { test, assert, ui, calls } = window.__harness;

// Runs `work` with the intel's hooks giving the editor and a tab of its session, which no server serves unless
// `served` is given.
async function inTab(view, work, served = false) {
  const kept = state.hooks;
  state.hooks = { ...kept, editor: () => view, tab: () => ({ session: view.s, file: "given.py", served }) };
  try {
    return await work();
  } finally {
    closeSignature();
    state.hooks = kept;
  }
}

// A declaration of total(first, second) as the window's side answers it, standing at `path` and `line`, the arguments
// at `differs` differing from their parameters.
const declared = (path, line, differs = []) => ({ path, line, label: "def total(first, second)", params: [[10, 15], [17, 23]], types: ["", ""], count: true, variadic: false, differs });

// The window's side's answer for a call of total with the cursor in its argument `active`.
const answered = (active, declarations, args = [{ text: "1", type: "number" }]) => ({ name: "total", active, arguments: args, declarations, reading: false });

test("the declaration the tree gives first is shown with the argument the cursor stands in marked, the others listed, and closed again", async () => {
  calls.answer("branch_signature", () => answered(2, [declared("lib.py", 4), declared("other.py", 0)]));
  const view = await given("x = total(1, f(2, 3), |");
  await inTab(view, async () => {
    await parameterInfo();
    assert.ok(signatureOpen());
    const shown = state.signature.innerHTML;
    assert.ok(shown.includes("def total(first, <b>second</b>)</code>"), "past the last parameter, the last marked");
    assert.ok(shown.includes("lib.py:5") && shown.includes("other.py:1"), "where each declaration stands");
    closeSignature();
    assert.ok(!signatureOpen());
  });
});

test("the call is asked of the window's side by the branch's key, with the file and the cursor's place", async () => {
  calls.answer("branch_signature", () => null);
  const view = await given("def run():\n    x = 1\n    total(x, |");
  await inTab(view, async () => {
    await parameterInfo({ quiet: true });
    const asked = made("branch_signature").at(-1).args;
    assert.deepEqual([asked.key, asked.path, asked.line, asked.col], [view.s.doc.mirrored.key, "given.py", 2, 13]);
  });
});

test("an argument whose type differs from its parameter's is marked with its own text and type", async () => {
  calls.answer("branch_signature", () => answered(1, [declared("lib.py", 4, [0])], [{ text: "name", type: "str" }, { text: "", type: null }]));
  const view = await given("x = total(name, |");
  await inTab(view, async () => {
    await parameterInfo();
    const marked = state.signature.querySelector(".ed-signature-differs");
    assert.equal(marked?.textContent, "first");
    assert.equal(marked?.title, "name: str");
  });
});

test("a call the tree declares nothing of, and a cursor in no call, are said", async () => {
  calls.answer("branch_signature", () => answered(0, []));
  const view = await given("x = total(|");
  await inTab(view, async () => {
    await parameterInfo();
    assert.ok(!signatureOpen());
    assert.ok(told(/The tree declares no total\./));
    calls.answer("branch_signature", () => null);
    await given("x = [total|]");
    await parameterInfo();
    assert.ok(told(/The cursor is not in a call\./), "a bracket opened before the call's");
  });
});

test("the window's side reads the call off the leaves, a bracket in a string counting for nothing", async () => {
  const view = await given('x = zq_unheard("(", |');
  await inTab(view, async () => {
    await parameterInfo();
    assert.ok(told(/The tree declares no zq_unheard\./), "the call found past the string's bracket");
  });
});

test("the server's answer is laid below the tree's where its parameters differ, and adds nothing where they agree", async () => {
  calls.answer("branch_signature", () => answered(1, [declared("lib.py", 4)]));
  calls.answer("lsp_signature", () => ({ label: "total(first: int, second: int)", params: [[6, 16], [18, 29]], active: 1, doc: "", count: 1 }));
  const view = await given("x = total(1, |");
  await inTab(
    view,
    async () => {
      await parameterInfo();
      const checked = state.signature.querySelector(".ed-signature-check");
      assert.ok(checked?.innerHTML.includes("<b>second: int</b>"), "the server's below the tree's");
      assert.ok(state.signature.innerHTML.includes("def total(first, <b>second</b>)</code>"), "the tree's first");
      calls.answer("lsp_signature", () => ({ label: "total(first, second)", params: [[6, 11], [13, 19]], active: 1, doc: "", count: 1 }));
      await parameterInfo();
      assert.ok(!state.signature.querySelector(".ed-signature-check"), "nothing laid below where they agree");
    },
    true,
  );
});

test("typing an opening parenthesis opens Parameter Info after a rest", async () => {
  calls.answer("branch_signature", () => answered(0, [declared("lib.py", 4)]));
  const view = await given("total(|");
  await inTab(view, async () => {
    typed();
    await ui.waitFor(() => signatureOpen(), SIGNATURE_REST + 2000);
    assert.ok(state.signature.innerHTML.includes("<b>first</b>"));
  });
});

test("the window is put back as the file found it", putBackAsFound);
