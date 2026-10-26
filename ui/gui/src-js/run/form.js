// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// A job's form: a field for each value it takes, the values given last remembered, and the values the form holds.

import { pick } from "../bridge.js";
import { element } from "../run.js";

export const remembered = (id) => JSON.parse(localStorage.getItem(`orior.values.${id}`) || "{}");
export const remember = (id, values) => localStorage.setItem(`orior.values.${id}`, JSON.stringify(values));

async function pickInto(param, input) {
  const given = await pick(param.kind);
  if (typeof given === "string") {
    input.value = given;
    input.dispatchEvent(new Event("input"));
  }
}

export function field(job, param, values) {
  const id = `param-${param.key}`;
  const label = element("label", { htmlFor: id, textContent: param.key, className: param.required ? "needed" : "" });
  const given = values[param.key] ?? (param.default ? [param.default] : []);
  let control;
  if (param.kind === "choice") {
    control = element("select", { id });
    if (!param.required) {
      control.append(element("option", { value: "", textContent: "" }));
    }
    param.choices.forEach((value) => control.append(element("option", { value, textContent: value })));
    control.value = given[0] ?? "";
  } else if (param.kind === "many") {
    control = element("div", { className: "choices", id });
    for (const value of param.choices) {
      const box = element("input", { type: "checkbox", value, checked: given.includes(value) });
      control.append(element("label", {}, box, value));
    }
  } else {
    const input = element("input", { id, type: "text", value: given[0] ?? "", spellcheck: false });
    if (param.kind === "choice-or-file") {
      const list = element("datalist", { id: `${id}-list` });
      param.choices.forEach((value) => list.append(element("option", { value })));
      input.setAttribute("list", list.id);
      control = element("div", { className: "pick" }, input, list, pickButton(param, input));
    } else if (["file", "dir", "save", "save-dir"].includes(param.kind)) {
      control = element("div", { className: "pick" }, input, pickButton(param, input));
    } else {
      control = input;
    }
  }
  return [label, control];
}

function pickButton(param, input) {
  const button = element("button", { className: "secondary", type: "button", textContent: "Open" });
  button.addEventListener("click", () => pickInto(param, input));
  return button;
}

export function valuesFrom(job) {
  const values = {};
  for (const param of job.params) {
    const node = document.getElementById(`param-${param.key}`);
    if (!node) {
      continue;
    }
    if (param.kind === "many") {
      values[param.key] = [...node.querySelectorAll("input:checked")].map((box) => box.value);
    } else {
      const input = node.matches("input, select") ? node : node.querySelector("input");
      values[param.key] = input.value.trim() ? [input.value.trim()] : [];
    }
  }
  return values;
}
