// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Macros: what the editor's keys did and the text typed, recorded as they are pressed and played back
// once or many times. The last one recorded is kept under a name where the reader gives one, and a
// kept one plays from Edit, Macros, or from keys of its own. The kept ones are under orior.macros,
// each with its name, its steps and its keys.

const KEPT = "orior.macros";

const state = { last: null, recorder: null, playing: false, listeners: [] };

export function keptMacros() {
  try {
    const list = JSON.parse(localStorage.getItem(KEPT) ?? "[]");
    return Array.isArray(list) ? list.filter((one) => one && typeof one.name === "string" && Array.isArray(one.steps)) : [];
  } catch {
    return [];
  }
}

function keep(list) {
  localStorage.setItem(KEPT, JSON.stringify(list));
  state.listeners.forEach((listener) => listener());
}

// Tells the listener each time the kept macros change.
export function onMacros(listener) {
  state.listeners.push(listener);
}

export function recording() {
  return Boolean(state.recorder);
}

export function lastMacro() {
  return state.last;
}

// Starts recording in `editor`, and answers null; or stops the recording and answers its steps, which
// are the last macro where there are any.
export function toggleRecording(editor) {
  if (state.recorder) {
    const steps = state.recorder.stopRecording();
    state.recorder = null;
    if (steps.length) {
      state.last = steps;
    }
    return steps;
  }
  editor.record();
  state.recorder = editor;
  return null;
}

// Plays the steps back `count` times in `editor`. A macro playing starts no other, itself among them;
// answers whether it played.
export async function playMacro(editor, steps, count = 1) {
  if (state.playing || !editor || !steps?.length) {
    return false;
  }
  state.playing = true;
  try {
    for (let time = 0; time < count; time += 1) {
      await editor.play(steps);
    }
  } finally {
    state.playing = false;
  }
  return true;
}

// Keeps the steps under the name, in place of a macro kept under it before, its keys kept with it.
export function keepMacro(name, steps = state.last) {
  const list = keptMacros();
  const before = list.find((one) => one.name === name);
  keep([...list.filter((one) => one !== before), { name, steps, keys: before?.keys ?? "" }].sort((a, b) => a.name.localeCompare(b.name)));
}

export function forgetMacro(name) {
  keep(keptMacros().filter((one) => one.name !== name));
}

// Binds the keys to the macro, taking them from any other macro they were bound to.
export function setMacroKeys(name, keys) {
  keep(keptMacros().map((one) => (one.name === name ? { ...one, keys } : one.keys === keys ? { ...one, keys: "" } : one)));
}
