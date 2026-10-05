// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// krep_head.h: the head every k-file opens with: KREP_MAGIC's eight bytes, the file's four-byte kind, and the version
// as one 32-bit limb
#ifndef KREP_HEAD_H
#define KREP_HEAD_H

#define KREP_HEAD_BYTES 16u

// "KREP" and four zero bytes, the string's own end the last of them
#define KREP_MAGIC "KREP\0\0\0"

#define KREP_VERSION 1u

#endif
