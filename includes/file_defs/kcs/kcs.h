// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kcs.h: the construction set's type and what it contains
#ifndef KCS_H
#define KCS_H

#define KREP_TYPE_CONSTRUCTION_SET "KCS\0"

// the construction set of a target for the compiler: the cost of each form as the target measured it, then the names
// of the forms. Each name has a zero byte at its end, and zeros fill the last word. The field words holds the costs
// and then the name words, and crc is the CRC-64 of all of them
#define KREP_FORMS_MAX 65536ULL

#endif
