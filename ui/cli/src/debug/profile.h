// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// profile.h: a profiler for a Python program, on this machine or on another one reached by `ssh`.
//
// The program runs under a shim that looks at every thread's stack every EVERY seconds, counting each stack it sees. At
// each look it reads the process's own memory, its private bytes on Windows and its resident pages elsewhere, and puts
// its growth since the look before to the frames the program's main thread, or the first of its threads, held at both
// looks, outermost first as far as the two agree. The program runs at its own speed under it, where tracing each
// allocation would slow it many times over; memory taken and kept within one look goes to the frames around it. Every
// SEND seconds, and once the program ends, it writes the counts so far on a line of its own that starts with MARK,
// among the program's own lines. On another machine the shim goes to its Python on its input, and the lines come back
// on `ssh`'s output.
//
// A frame of a stack is the function's name, its file, as a path under the folder the program ran in where it is under
// it, and the line it starts at; a stack is its frames, the outermost first, each on a line of its own (profile.c,
// profile_shim.c)
#ifndef PROFILE_H
#define PROFILE_H

#include "../tests/test_runs.h"

// the shim the program runs under (profile_shim.c)
extern const char g_cli_profile_shim[];

// the Python file at `file`, a path under `root`, profiled on this machine, or where `remote` names a machine, as
// `user@host:folder`, on it, in that folder, by the same path, any profile running stopped first; what it tells as it
// goes comes by `tell`, written as JSON, `{"kind": "output", "text", "error"}` for a line the program wrote, `{"kind":
// "counts", "counts"}` for the counts so far, and `{"kind": "done", "code", "said"}` at its end: 1, or 0 with why said.
// And the profile running stopped, where one is (profile.c)
int cli_profile_start(const char *root, const char *file, const char *remote, CliTell tell, CliLine *said);
void cli_profile_stop(void);

#endif
