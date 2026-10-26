// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

import { invoke } from "../bridge.js";
import { askFor, sheet } from "../menubar.js";
import { say } from "../statusbar.js";
import { chosen, element, KEPT_DEPTH, KEPT_MOST, KEPT_WIDTH, pausedOf, state } from "../debug.js";
import { drawVariables, frameMenu, variableRow } from "./variables.js";

// Values kept to compare.

// A value read whole, as deep as KEPT_DEPTH and as wide as KEPT_WIDTH at each level.
export async function readWhole(variable, depth = 0) {
  const tree = { name: variable.name, value: variable.value, kind: variable.kind, children: [] };
  if (variable.reference && depth < KEPT_DEPTH) {
    const found = await invoke("debug_variables", { session: chosen().info.id, reference: variable.reference }).catch(() => []);
    for (const child of found.slice(0, KEPT_WIDTH)) {
      tree.children.push(await readWhole(child, depth + 1));
    }
  }
  return tree;
}

export async function keepValue(variable) {
  const paused = pausedOf();
  const frame = paused?.frames[paused.at];
  const tree = await readWhole(variable);
  const where = frame?.path ? ` at ${frame.path.split("/").pop()}:${frame.line + 1}` : "";
  state.kept.unshift({ label: `${variable.name}${where}`, tree });
  state.kept.length = Math.min(state.kept.length, KEPT_MOST);
  say(`${variable.name} is kept as it stands${where}.`);
  drawVariables();
}

// Each field of a value, by its path of names, with its value.
function fieldsOf(tree, prefix = "", into = new Map()) {
  into.set(prefix || tree.name, tree.value);
  for (const child of tree.children) {
    fieldsOf(child, `${prefix || tree.name}.${child.name}`, into);
  }
  return into;
}

// The sheet that sets a kept value beside one read now, field by field, a field that differs marked.
export function compareValues(kept, now) {
  const before = fieldsOf(kept.tree, kept.tree.name);
  const after = fieldsOf(now, kept.tree.name);
  const paths = [...new Set([...before.keys(), ...after.keys()])];
  const table = element("table", { className: "debug-compare" }, element("thead", {}, element("tr", {}, element("th", { textContent: "Field" }), element("th", { textContent: kept.label }), element("th", { textContent: "Now" }))));
  const body = element("tbody");
  let differ = 0;
  for (const path of paths) {
    const a = before.get(path);
    const b = after.get(path);
    const row = element("tr", {}, element("td", { textContent: path }), element("td", { textContent: a ?? "—" }), element("td", { textContent: b ?? "—" }));
    if (a !== b) {
      row.classList.add("differs");
      differ += 1;
    }
    body.append(row);
  }
  table.append(body);
  const form = element("div", { className: "sheet-report sheet-wide" }, element("h2", { textContent: `${now.name}: ${differ} of ${paths.length} field${paths.length === 1 ? "" : "s"} differ${differ === 1 ? "s" : ""}` }), table);
  sheet(form);
}

// Data, memory and instructions.

export async function watchData(name, container) {
  const session = chosen();
  try {
    const watched = await invoke("debug_watch_data", { session: session.info.id, name, reference: container ?? null, bytes: null });
    say(`The program stops where ${watched} changes.`);
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// Run, Stop When Memory Changes: a data breakpoint on the bytes at an address given.
export async function watchAddress(given) {
  const session = chosen();
  if (!session?.info.data) {
    say("The chosen session's debugger cannot stop where memory changes.", { failed: true });
    return;
  }
  const answer = given ?? (await askFor("The address to watch, and how many bytes, such as 0x7ffe1000 8", ""));
  const [address, bytes] = (answer ?? "").trim().split(/\s+/);
  if (!address) {
    return;
  }
  try {
    const watched = await invoke("debug_watch_data", { session: session.info.id, name: address, reference: null, bytes: Number(bytes) || 4 });
    say(`The program stops where ${watched} changes.`);
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// The sheet of the bytes at `reference`: a row of sixteen a line, as hex and as text, read on as it
// is paged.
export async function showMemory(reference, label = reference) {
  const session = chosen();
  if (!session?.info.memory) {
    say("The chosen session's debugger cannot read memory.", { failed: true });
    return;
  }
  const given = reference ?? (await askFor("The address to read, such as 0x7ffe1000", ""));
  if (!given) {
    return;
  }
  let offset = 0;
  const pre = element("pre", { className: "debug-memory" });
  const head = element("h2", { textContent: `Memory at ${label ?? given}` });
  const read = async () => {
    try {
      const found = await invoke("debug_memory", { session: session.info.id, reference: given, offset, count: 256 });
      const base = BigInt(found.address);
      const lines = [];
      for (let at = 0; at < found.bytes.length; at += 16) {
        const row = found.bytes.slice(at, at + 16);
        const hex = row.map((byte) => byte.toString(16).padStart(2, "0")).join(" ").padEnd(47, " ");
        const text = row.map((byte) => (byte >= 32 && byte < 127 ? String.fromCharCode(byte) : "·")).join("");
        lines.push(`${(base + BigInt(at)).toString(16).padStart(16, "0")}  ${hex}  ${text}`);
      }
      pre.textContent = lines.join("\n") + (found.unreadable ? `\n${found.unreadable} bytes past these could not be read.` : "");
    } catch (error) {
      pre.textContent = String(error);
    }
  };
  const nav = element("div", { className: "report-foot" });
  for (const [label2, by] of [["Before", -256], ["After", 256]]) {
    const button = element("button", { type: "button", textContent: label2 });
    button.addEventListener("click", () => {
      offset += by;
      read();
    });
    nav.append(button);
  }
  sheet(element("div", { className: "sheet-report sheet-wide" }, head, pre, nav));
  await read();
}

// The sheet of the instructions about a native frame's line, the frame's own marked.
export async function showInstructions(frame) {
  const session = chosen();
  try {
    const found = await invoke("debug_instructions", { session: session.info.id, reference: frame.ip, offset: -12, count: 40 });
    const pre = element("pre", { className: "debug-memory" });
    for (const one of found) {
      const line = element("div", { textContent: `${one.address}  ${one.text}${one.symbol ? `   <${one.symbol}>` : ""}${one.line !== null && one.line !== undefined ? `   line ${one.line + 1}` : ""}` });
      if (one.address === frame.ip || BigInt(one.address || 0) === BigInt(frame.ip || 0)) {
        line.className = "here";
      }
      pre.append(line);
    }
    sheet(element("div", { className: "sheet-report sheet-wide" }, element("h2", { textContent: `Instructions about ${frame.name}` }), pre));
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// The sheet of the bytecode of a Python frame's function, the instructions of the frame's line marked.
export async function showBytecode(frame) {
  try {
    const [name, found] = await invoke("debug_bytecode", { path: frame.path, line: frame.line });
    const pre = element("pre", { className: "debug-memory" });
    for (const one of found) {
      const line = element("div", { textContent: `${one.address.padStart(5, " ")}  ${one.text}${one.line !== null && one.line !== undefined ? `   line ${one.line + 1}` : ""}` });
      if (one.line === frame.line) {
        line.className = "here";
      }
      pre.append(line);
    }
    sheet(element("div", { className: "sheet-report sheet-wide" }, element("h2", { textContent: `Bytecode of ${name}` }), pre));
  } catch (error) {
    say(String(error), { failed: true });
  }
}

// Run, Show Bytecode or Instructions: those of the chosen frame, or of the cursor's line of a Python
// file where nothing is stopped.
export async function showMachineCode() {
  const paused = pausedOf();
  const frame = paused?.frames[paused.at];
  if (frame) {
    frameMenu(frame, window.innerWidth / 3, window.innerHeight / 3);
    return;
  }
  const tab = state.hooks.tab();
  const editor = state.hooks.editor();
  if (tab?.file?.endsWith(".py") && editor?.s === tab.session) {
    await showBytecode({ path: tab.file, line: tab.session.base + editor.head().line });
  } else {
    say("Stop a program, or open a Python file, to read its machine code or its bytecode.");
  }
}

// The value under the pointer.

// The expression a place of a line stands in: the name there and the names and dots before it.
function expressionAt(text, col) {
  let from = col;
  let to = col;
  while (to < text.length && /[\w$]/.test(text[to])) {
    to += 1;
  }
  while (from > 0 && /[\w$.]/.test(text[from - 1])) {
    from -= 1;
  }
  const expression = text.slice(from, to).replace(/^\.+/, "");
  return /^[A-Za-z_$][\w$]*(?:\.[A-Za-z_$][\w$]*)*$/.test(expression) ? { expression, from: from + (text.slice(from, to).length - expression.length), to } : null;
}

// The value of the name the pointer rests on in `file`, while the chosen session is stopped: a row
// for it, opened a level at a time where it holds more, or null.
export async function valueAt(file, doc, p) {
  const session = chosen();
  const paused = pausedOf();
  const frame = paused?.frames[paused.at];
  if (!session || !frame || !file) {
    return null;
  }
  const found = expressionAt(doc.line(p.line), p.col);
  if (!found) {
    return null;
  }
  let variable;
  try {
    variable = await invoke("debug_evaluate", { session: session.info.id, expression: found.expression, frame: frame.id, context: "hover" });
  } catch {
    return null;
  }
  const node = element("div", { className: "debug-hover" });
  const add = (one, depth, into) => {
    let open = false;
    let shown = null;
    const holder = variableRow(one, depth, false, () => {}, null);
    const twisty = holder.querySelector(".debug-twisty");
    const fresh = twisty.cloneNode(true);
    twisty.replaceWith(fresh);
    fresh.addEventListener("click", async () => {
      open = !open;
      fresh.textContent = open ? "▾" : "▸";
      if (open) {
        shown = element("div");
        const children = await invoke("debug_variables", { session: session.info.id, reference: one.reference }).catch(() => []);
        children.slice(0, 200).forEach((child) => add(child, depth + 1, shown));
        holder.append(shown);
      } else {
        shown?.remove();
      }
    });
    into.append(holder);
  };
  add({ ...variable, name: found.expression }, 0, node);
  return { from: { line: p.line, col: found.from }, to: { line: p.line, col: found.to }, node };
}
