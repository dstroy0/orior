// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Macros: what the editor's keys did and the text typed, recorded as they are pressed and played back
// once or many times. The last one recorded is retained under a name where the reader gives one, and a
// retained one plays from Edit, Macros, or from keys of its own. The retained ones are under orior.macros,
// each with its name, its steps and its keys.

const RETAINED = "orior.macros";

const state = { last: null, recorder: null, playing: false, listeners: [] };

export function retainedMacros() {
  try {
    const list = JSON.parse(localStorage.getItem(RETAINED) ?? "[]");
    return Array.isArray(list) ? list.filter((one) => one && typeof one.name === "string" && Array.isArray(one.steps)) : [];
  } catch {
    return [];
  }
}

function retain(list) {
  localStorage.setItem(RETAINED, JSON.stringify(list));
  state.listeners.forEach((listener) => listener());
}

// Tells the listener each time the retained macros change.
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

// Retains the steps under the name, in place of a macro retained under it before, its keys retained with it.
export function retainMacro(name, steps = state.last) {
  const list = retainedMacros();
  const before = list.find((one) => one.name === name);
  retain([...list.filter((one) => one !== before), { name, steps, keys: before?.keys ?? "" }].sort((a, b) => a.name.localeCompare(b.name)));
}

export function forgetMacro(name) {
  retain(retainedMacros().filter((one) => one.name !== name));
}

// Binds the keys to the macro, taking them from any other macro they were bound to.
export function setMacroKeys(name, keys) {
  retain(retainedMacros().map((one) => (one.name === name ? { ...one, keys } : one.keys === keys ? { ...one, keys: "" } : one)));
}
