// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// Review comments: a comment left on a line of a change, as a pull request's review leaves one. Each
// is kept with the tree, under orior.review and the tree's folder, by the file's path, the line's
// number and the line's text as it stood. It stands at the line that holds that text nearest the
// number where the file has moved, or at the number, marked as gone from its line, where no line
// holds it. A comment is open until it is resolved.

const listeners = [];

const key = () => `orior.review.${document.getElementById("tree-path").textContent}`;

// Every comment of the tree, in the order they were left.
export function comments() {
  try {
    const list = JSON.parse(localStorage.getItem(key()) ?? "[]");
    return Array.isArray(list) ? list : [];
  } catch {
    return [];
  }
}

function keep(list) {
  localStorage.setItem(key(), JSON.stringify(list));
  listeners.forEach((listener) => listener());
}

// Tells `listener` each time a comment is left, resolved, opened again or deleted.
export function onReview(listener) {
  listeners.push(listener);
}

export function addComment(path, line, text, body) {
  const id = `${Date.now().toString(36)}${Math.random().toString(36).slice(2, 6)}`;
  keep([...comments(), { id, path, line, text, body, made: Date.now(), done: false }]);
}

export function removeComment(id) {
  keep(comments().filter((one) => one.id !== id));
}

export function resolveComment(id, done = true) {
  keep(comments().map((one) => (one.id === id ? { ...one, done } : one)));
}

// Where a comment stands in `lines`, the file's lines as they are now, and whether its line is gone.
export function anchor(comment, lines) {
  if (lines[comment.line] === comment.text) {
    return { line: comment.line, gone: false };
  }
  let best = -1;
  lines.forEach((text, at) => {
    if (text === comment.text && (best < 0 || Math.abs(at - comment.line) < Math.abs(best - comment.line))) {
      best = at;
    }
  });
  return best >= 0 ? { line: best, gone: false } : { line: Math.min(comment.line, Math.max(0, lines.length - 1)), gone: true };
}
