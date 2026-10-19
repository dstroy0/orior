// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// run_file.h: one file run with the toolchain its language has: Python with python, R with Rscript, MATLAB with
// matlab -batch, Octave with octave-cli, Lean with lean --run, TeX with latexmk, a netlist with ngspice or LTspice in
// batch, VHDL with ghdl, and C, C++, CUDA and Rust compiled to build/run/ and then run. Each toolchain's `runs` in
// toolchains.json gives the line, by language, or by language and extension as c.cu is, and of the toolchains whose
// `runs` name it the first found runs it.
//
// The line is for a POSIX shell, the one orior's terminal runs, and runs at the tree's top folder. In it {file} is the
// file's path quoted, {path} the same unquoted, {dir} its folder quoted, {stem} its name without its extension, {out}
// build/run/<stem> quoted, {program} the toolchain's program found, and any other {name} the program of that name in
// the same folder (run_file.c)
#ifndef RUN_FILE_H
#define RUN_FILE_H

#include "../command_line/cli_internal.h"

// a path as a POSIX shell takes it, with forward slashes, in single quotes, after `out`
void cli_run_file_quoted(CliLine *out, const char *path);
// the line that runs the file at `path`, under the tree at `root`, in `language`, written as `{"line", "tool"}`: 1, or
// 0 with why said
int cli_run_file_line(const char *root, const char *path, const char *language, CliLine *out, CliLine *said);

#endif
