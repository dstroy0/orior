// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// vendor_writer_run.h: the vendor's writer run through the interface by a format's writer, its output printed and
// passed to the run (vendor_writer_run.c)
#ifndef VENDOR_WRITER_RUN_H
#define VENDOR_WRITER_RUN_H

#ifdef __cplusplus
extern "C"
{
#endif

    // The writer of the vendor run through the interface as `command`, a list of words with NULL at its end. The first
    // word is the path of the writer. Its output comes back over the pipe of the interface. The call prints it and
    // passes it to the run as the blob `machine_name`. The return is 1 where the writer ended clean. Else the call
    // prints a line that names `program`, and the mode of the writer where `mode` is not NULL, and returns 0
    int vendor_writer_run(char *const *command, const char *machine_name, const char *program, const char *mode);

#ifdef __cplusplus
}
#endif

#endif
