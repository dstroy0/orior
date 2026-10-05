// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kcs.h: the construction set's kind and what it holds
#ifndef KCS_H
#define KCS_H

#define KREP_KIND_CONSTRUCTION_SET "KCS\0"

// a target's construction set for the compiler: each form's cost as the target measured it, then the forms' names,
// each ended by a zero byte and the last word padded with zeros; words holds the costs and then the name words, and
// crc is the CRC-64 of all of them
#define KREP_FORMS_MAX 65536ull

#endif
