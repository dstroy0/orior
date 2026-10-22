// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// recording.h: a Python program's run recorded as it goes, and a debug adapter, run in orior_cli itself, that steps through
// the recording backward as well as forward.
//
// The recorder is a shim the program runs under. It notes each line of the tree's own code the program runs, before
// the line runs, with the names of the line's frame whose values changed since the frame's last line, each value as
// `reprlib` writes it; each call and return of the tree's functions; and each exception raised. A step of the recording
// is one such line, and the values a stop shows are the frame's as that line was about to run.
//
// The adapter answers the requests a debugger's client sends: Continue and Run Backward go to the next or the last step
// at a breakpoint, or at an exception raised where those stop it; Step Over, Into and Out go forward as a live
// debugger's do; Step Back goes to the frame's line before, past the calls it made. Two requests of orior's own read a
// name's values through the run, each where it changed and the line that changed it, and go to any step
// (recording_read.c, recording_replay.c, recording_answer.c, recording_serve.c, recording_request.c,
// recording_shim.c)
#ifndef RECORDING_H
#define RECORDING_H

#include "dap.h"

// the adapter that records a Python program's run in the tree at `root` and steps through it, begun: its context, for
// cli_dap_within to hand `cli_recording_serve`, or NULL where no room for the replay is left; and a request served
// (recording_request.c)
void *cli_recording_begun(const char *root);
int cli_recording_serve(void *context, CliDap *dap, const char *request, unsigned long long size);

#endif
