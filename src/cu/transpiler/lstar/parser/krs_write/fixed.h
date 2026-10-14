// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fixed.h: the fixed lines of a SASS ruleset's head the part answered for, read from its .ksc (The fixed registers
// asked, Open 6.2)
#ifndef KRS_FIXED_H
#define KRS_FIXED_H

#include "../../../../types/file_defs/qry/qry_buffer.h"

#include <stddef.h>

// The fixed lines the part's .ksc at `ksc_path` answers for, appended to `head` in the order zero, lane_number, record,
// index: `fixed <name> = R<n>` where it holds `run answers <n> fixed <name>`, and `RZ` where n is the top of the
// register field its `field rd` row bounds, the vendor's name for it. The names not answered are written into
// `missing`, `room` bytes, apart by ", ", and empty where every one is. The count written
unsigned int krs_fixed_written(QryText *head, const char *ksc_path, char *missing, size_t room);

#endif
