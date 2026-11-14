// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A job's form: a field for each value it takes, of the type the value is, the values given last remembered, and
// the values the form holds read back.

import { field, remember, remembered, valuesFrom } from "../../../gui/src-js/run/form.js";
import { retentionStorage } from "../../helpers.js";

const { test, assert } = window.__harness;

const JOB = {
  id: "form/test",
  params: [
    { key: "mode", type: "choice", choices: ["fast", "slow"], required: false },
    { key: "parts", type: "many", choices: ["a", "b", "c"] },
    { key: "input", type: "file", default: "in.txt" },
    { key: "name", type: "text", required: true },
  ],
};

// The form's fields on the page while `work` runs.
async function withForm(values, work) {
  const host = document.createElement("div");
  for (const param of JOB.params) {
    host.append(...field(JOB, param, values));
  }
  document.body.append(host);
  try {
    return await work(host);
  } finally {
    host.remove();
  }
}

test("each value takes a field of its type: a choice a list, many a box each, a file a field with Open", async () => {
  await withForm({}, (host) => {
    const mode = host.querySelector("#param-mode");
    assert.equal(mode.tagName, "SELECT");
    assert.deepEqual([...mode.options].map((one) => one.value), ["", "fast", "slow"], "a choice not needed may be left empty");
    assert.equal(host.querySelectorAll("#param-parts input[type=checkbox]").length, 3);
    assert.equal(host.querySelector("#param-input").value, "in.txt", "a value not given takes its default");
    assert.equal(host.querySelector("#param-input").closest(".pick").querySelector("button").textContent, "Open");
    assert.equal(host.querySelector('label[for="param-name"]').className, "needed");
  });
});

test("the values given fill the form, and the form's values are read back as given", async () => {
  await withForm({ mode: ["slow"], parts: ["a", "c"], input: ["data.csv"], name: ["  run one  "] }, (host) => {
    assert.equal(host.querySelector("#param-mode").value, "slow");
    assert.deepEqual(valuesFrom(JOB), { mode: ["slow"], parts: ["a", "c"], input: ["data.csv"], name: ["run one"] });
    host.querySelector("#param-name").value = "  ";
    assert.deepEqual(valuesFrom(JOB).name, [], "an empty field gives no value");
  });
});

test("the values given last are remembered for the job", async () => {
  await retentionStorage(["orior.values.form/test"], () => {
    assert.deepEqual(remembered("form/test"), {});
    remember("form/test", { name: ["retained"] });
    assert.deepEqual(remembered("form/test"), { name: ["retained"] });
  });
});
