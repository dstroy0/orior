// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fixed.h: the fixed lines of a SASS ruleset's head. Each line is the part's answer in its .ksc or the line the ruleset
// gave before (Open 6.2).
#ifndef KRS_FIXED_H
#define KRS_FIXED_H

#include "../../qry/writer/qry_member.h"

#include <stddef.h>

// Write the fixed lines of a SASS ruleset's head at `*at` into `out`, which has room for `cap` bytes. Move `*at` past
// them, or to `cap` if they did not fit (verba_scribo.h). The lines are the four the kernel's probe place answers for,
// zero, lane_number, record and index in that order. Then come the other fixed registers the ruleset at `krs_path`
// gave a line before this run, in its order.
//
// A register's line is `fixed <name> = R<n>` if the part's .ksc at `ksc_path` contains `run answers <n> fixed <name>`.
// It is `RZ` if n is the top of the register field its `field rd` row bounds, the vendor's name for it. If the .ksc
// contains no row for it, the line the ruleset gave stays whole. A line gives way only to the part's row for its
// register.
//
// A given line a row replaces with another text goes into the part's .krf beside the .ksc (krf.h). `krs_path` may be
// NULL or name no file, and then the ruleset gives nothing. The four with neither go into `missing`, `room` bytes,
// apart by ", ", and `missing` is empty if none is. Return the count written.
unsigned int krs_fixed_written(char *out, size_t cap, size_t *at, const char *ksc_path, const char *krs_path,
                               char *missing, size_t room);

// Read the fixed registers the head names, by the same rules, and keep each name with its register text. Return the
// count kept.
unsigned int krs_fixed_read(const char *ksc_path, const char *krs_path);

// Put the placeholder `{<name>}` into `text`, of `room` bytes, where the text holds the numeric name of a fixed
// register, as `R0`. A numeric name is a whole word of the text, not a part of a longer name and not inside a
// placeholder. The first register kept with that name wins. Return 1, or 0 if the text with the placeholders does not
// fit, and then the text stays as it was.
int krs_fixed_placed(char *text, size_t room);

#endif
