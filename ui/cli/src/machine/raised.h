// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raised.h: writing a file the system refuses to write for the reader, once the system has asked the reader for the
// rights the write needs: the text put in a file of orior's own first and copied over the file with the rights. A
// reader who declines the prompt leaves the file as it was (raised.c)
#ifndef RAISED_H
#define RAISED_H

#include "../command_line/cli_internal.h"

// whether `said`, what a write that failed said, is the system refusing it the rights; the file `from` copied over
// `to` with the rights; and `size` bytes of `text` written to the file at `path` with the rights. Each copy and write
// 1, or 0 with why said
int cli_raised_is_refused(const char *said);
int cli_raised_copy_over(const char *from, const char *to, CliLine *said);
int cli_raised_write(const char *path, const void *text, unsigned long long size, CliLine *said);

#endif
