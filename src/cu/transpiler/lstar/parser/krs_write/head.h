// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// head.h: the lines of a SASS ruleset's head the part answered for, read from its .ksc (The frame asked, Open 6.3)
#ifndef KRS_HEAD_H
#define KRS_HEAD_H

#include "../../../../types/file_defs/qry/qry_buffer.h"

#include <stddef.h>

// The head lines the part's .ksc at `ksc_path` answers for, appended to `head`: `toolchain cubin_write` where it holds
// `run answers <n> container returned`, the container the vendor's writer makes loaded and returned from, and
// `header container` where it holds `run answers <n> opening least`, the least opening a kernel runs with. The facts
// not answered are written into `missing`, `room` bytes, apart by ", ", and empty where both are. The count written
unsigned int krs_head_written(QryText *head, const char *ksc_path, char *missing, size_t room);

#endif
