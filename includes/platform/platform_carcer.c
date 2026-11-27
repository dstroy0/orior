// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_carcer.c: the cellblock of MMgr, as the platform links it. platform_pool.c expands MMGR_OWNERS and so
// defines the table of owners. This unit leaves out the weak table of no owners that locus_carcerum.c defines for a
// program with none. On a PE/COFF target, gcc binds a reference to a weak symbol of the same unit to that symbol, and
// the cellblock would read the empty table and refuse every take
#define MMGR_OWNERS_IN_THIS_UNIT 1
#include "locus_carcerum/locus_carcerum.c"
