// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fixed.h: the fixed lines of a SASS ruleset's head, each the part's answer in its .ksc or the line the ruleset gave
// before (The fixed registers asked, Open 6.2)
#ifndef KRS_FIXED_H
#define KRS_FIXED_H

#include "../../../../types/file_defs/qry/qry_buffer.h"

#include <stddef.h>

// The fixed lines of a SASS ruleset's head, appended to `head`: the four the kernel's slot answers for, zero,
// lane_number, record and index in that order, then every other fixed register the ruleset at `krs_path` gave a line
// before this run, in its order. A register is written `fixed <name> = R<n>` where the part's .ksc at `ksc_path` holds
// `run answers <n> fixed <name>`, `RZ` where n is the top of the register field its `field rd` row bounds, the vendor's
// name for it; and where the .ksc holds no row for it, as the line the ruleset gave it, kept whole, since a line is
// let go only where a row replaces it. `krs_path` may be NULL or name no file, and then nothing is given. The four
// with neither are written into `missing`, `room` bytes, apart by ", ", and empty where none is. The count written
unsigned int krs_fixed_written(QryText *head, const char *ksc_path, const char *krs_path, char *missing, size_t room);

#endif
