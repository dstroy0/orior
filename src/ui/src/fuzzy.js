// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Fuzzy matching, as a quick open reads what is typed: the query's letters in order anywhere in the
// text, case aside, in no more pieces than half its letters or three. A letter scores more at the
// start of a word or of the file's name and right after the letter before it, and a match in a
// path's last part outranks one spread over its folders.

const START = /[\s/\\_\-.:@]/;

function startsWord(text, at) {
  if (at === 0) {
    return true;
  }
  const before = text[at - 1];
  return START.test(before) || (before === before.toLowerCase() && text[at] !== text[at].toLowerCase());
}

// The places in `text` that match `query` and the match's score, or null where it does not match.
// `from` is where the text's name starts; letters past it score more.
export function fuzzy(query, text, from = 0) {
  if (!query) {
    return { score: 0, hits: [] };
  }
  const lower = text.toLowerCase();
  const wanted = query.toLowerCase().replace(/\s+/g, "");
  // The latest place each letter can stand with the letters after it still fitting.
  const latest = new Array(wanted.length);
  let at = lower.length;
  for (let index = wanted.length - 1; index >= 0; index -= 1) {
    at = lower.lastIndexOf(wanted[index], at - 1);
    if (at < 0) {
      return null;
    }
    latest[index] = at;
  }
  const hits = [];
  let score = 0;
  let last = -2;
  for (let index = 0; index < wanted.length; index += 1) {
    const letter = wanted[index];
    // The letter right after the one before it, else the first that starts a word, else the first.
    let found = lower[last + 1] === letter && last >= 0 ? last + 1 : -1;
    let first = -1;
    for (let place = last + 1; found < 0 && place <= latest[index]; place += 1) {
      if (lower[place] !== letter) {
        continue;
      }
      first = first < 0 ? place : first;
      if (startsWord(text, place)) {
        found = place;
      }
    }
    found = found < 0 ? first : found;
    score += 1;
    if (found === last + 1) {
      score += 5;
    }
    if (startsWord(text, found)) {
      score += 8;
    }
    if (found >= from) {
      score += 3;
    }
    if (text[found] === letter) {
      score += 1;
    }
    hits.push(found);
    last = found;
  }
  // A query torn into more pieces than half its letters reads as no match.
  const runs = hits.filter((at, index) => index === 0 || at !== hits[index - 1] + 1).length;
  if (runs > Math.max(3, Math.ceil(wanted.length / 2))) {
    return null;
  }
  score -= (hits.at(-1) - hits[0]) * 0.1 + text.length * 0.02;
  return { score, hits };
}

// `text` as nodes, the places in `hits` marked.
export function marked(text, hits) {
  const nodes = [];
  const held = new Set(hits);
  let run = "";
  let inside = false;
  const flush = () => {
    if (run) {
      nodes.push(inside ? Object.assign(document.createElement("mark"), { textContent: run }) : document.createTextNode(run));
    }
    run = "";
  };
  for (let at = 0; at < text.length; at += 1) {
    if (held.has(at) !== inside) {
      flush();
      inside = held.has(at);
    }
    run += text[at];
  }
  flush();
  return nodes;
}
