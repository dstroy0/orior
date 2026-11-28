// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// head.h: the lines of a SASS ruleset's head the part answered for, read from its .ksc (Open 6.3)
#ifndef KRS_HEAD_H
#define KRS_HEAD_H

#include "../../qry/writer/qry_member.h"

#include <stddef.h>

// The head lines that the .ksc of the part at `ksc_path` answers for, written at `*at` into `out`, which has room for
// `cap`. The call moves `*at` past them, or to `cap` where they did not fit (verba_scribo.h). The line
// `toolchain cubin_write` goes in where the file holds `run answers <n> container returned`. That container is the one
// that the writer of the vendor makes, loads and returns from. The line `header container` goes in where the file
// holds `run answers <n> opening least`, the least opening that a kernel runs with. The call writes the facts that the
// file does not answer into `missing`, `room` bytes, apart by ", ", and empty where it answers both.
//
// The return is the count written
unsigned int krs_head_written(char *out, size_t cap, size_t *at, const char *ksc_path, char *missing, size_t room);

#endif
