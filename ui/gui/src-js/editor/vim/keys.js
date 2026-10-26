// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The keys Vim reads: the operators, motions and commands a command starts with, the commands that change the text,
// the arrows' motions, each mode's name, and what a command still waiting for keys answers.

export const OPERATORS = new Set(["d", "c", "y", ">", "<"]);
export const MOTIONS = new Set(["h", "j", "k", "l", "w", "W", "b", "B", "e", "E", "0", "^", "$", "G", ";", ",", "%", "{", "}", "n", "N", "C-d", "C-u"]);
export const COMMANDS = new Set(["x", "X", "D", "C", "Y", "s", "S", "J", "~", "p", "P", "u", "C-r", ".", "i", "a", "I", "A", "o", "O", "v", "V", "/", "?", "*", "#", "Esc"]);
// The commands that change the text, which `.` does again.
export const CHANGES = new Set(["d", "c", ">", "<", "x", "X", "D", "C", "s", "S", "J", "~", "r", "p", "P", "i", "a", "I", "A", "o", "O"]);
export const ARROWS = { ArrowLeft: "h", ArrowRight: "l", ArrowUp: "k", ArrowDown: "j", Home: "0", End: "$", Delete: "x", Enter: "j" };
export const NAMES = { normal: "NORMAL", insert: "INSERT", visual: "VISUAL", line: "VISUAL LINE" };

// What a command still waits for more keys to be whole.
export const WAIT = Symbol("wait");
