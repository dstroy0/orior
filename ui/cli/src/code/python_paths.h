// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// python_paths.h: the folders a tree's Python scripts put on `sys.path` as they run, read from their text with no
// setup, for Python's server to search for the modules the scripts import from them. A script that writes
// `sys.path.insert(0, os.path.join(ROOT, "src", "python"))` names a folder its server knows nothing of until it is
// told.
//
// What is read: `sys.path.insert`, `sys.path.append`, `sys.path.extend` and `sys.path[:0] = [...]`, of an expression
// made of `__file__`, strings, the names the script set before it, `os.path`'s `dirname`, `abspath`, `realpath`,
// `normpath` and `join`, `pathlib.Path` with `parent`, `parents`, `resolve`, `/` and `joinpath`, `str`, `os.fspath`,
// and a string's `rsplit(sep, 1)[0]`. A name set again from itself, as a loop walks up a folder at a time until a
// folder is there, stands for each folder above its first value; a name a `for` takes from a tuple or a list stands
// for each of them, and one it takes from `os.scandir`, `os.listdir` or `iterdir` for each folder in that folder. Of
// what an expression may be, the folders that are there in the tree are kept (python_paths.c, python_paths_parse.c,
// python_paths_eval.c)
#ifndef PYTHON_PATHS_H
#define PYTHON_PATHS_H

#include "../command_line/cli_internal.h"

// the folders the Python scripts of the tree at `root` put on `sys.path`, each a folder there in the tree other than
// its top, the most used first, the first `most` of them each handed to `each`: their count. A tree's reading stands
// thirty seconds before it is read again
unsigned int cli_python_paths(const char *root, unsigned int most, void (*each)(void *context, const char *path),
                              void *context);

#endif
