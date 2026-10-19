// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// head.h: the lines of a SASS ruleset's head the part answered for, read from its .ksc (The frame asked, Open 6.3)
#ifndef KRS_HEAD_H
#define KRS_HEAD_H

#include "../../qry/writer/qry_member.h"

#include <stddef.h>

// The head lines the part's .ksc at `ksc_path` answers for, written at `*at` into `out`, which holds `cap`, and `*at`
// moved past them, or to `cap` where they did not fit (verba_scribo.h): `toolchain cubin_write` where it holds
// `run answers <n> container returned`, the container the vendor's writer makes loaded and returned from, and
// `header container` where it holds `run answers <n> opening least`, the least opening a kernel runs with. The facts
// not answered are written into `missing`, `room` bytes, apart by ", ", and empty where both are. The count written
unsigned int krs_head_written(char *out, size_t cap, size_t *at, const char *ksc_path, char *missing, size_t room);

#endif
