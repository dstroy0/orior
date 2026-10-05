// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksh.h: the shard's kind and its head
#ifndef KSH_H
#define KSH_H

#define KREP_KIND_SHARD "KSH\0"

// The flattened file's format word, first in its head. Format 2 records each sample's orders after its name, and a file
// of any other format errors on read.
#define FLATTEN_FORMAT 2u

#define FLATTEN_HEAD_LIMBS 5u

#endif
