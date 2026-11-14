// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// vendor_writer_run.h: the vendor's writer run through the interface by a format's writer, its output printed and
// handed to the run (vendor_writer_run.c)
#ifndef VENDOR_WRITER_RUN_H
#define VENDOR_WRITER_RUN_H

#ifdef __cplusplus
extern "C"
{
#endif

    // The vendor's writer run through the interface as `command`, a list of words ended by NULL, the first the
    // writer's path. Its output comes back over the interface's pipe, is printed, and is handed to the run as the blob
    // `machine_name`. 1 where it ended clean; else a line is printed naming `program` and, where `mode` is not NULL,
    // the mode the writer ran in, and 0 is handed back
    int vendor_writer_run(char *const *command, const char *machine_name, const char *program, const char *mode);

#ifdef __cplusplus
}
#endif

#endif
