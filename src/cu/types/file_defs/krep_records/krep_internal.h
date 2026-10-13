// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// krep_internal.h: what the modules of the k-file of records share: its includes, the bounds of a number and a row, the rows gathered from a run, and the functions one module reads from another (krep_records.h)
#ifndef KREP_INTERNAL_H
#define KREP_INTERNAL_H

#include "../krep_records.h"

#include "../krep_head.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the most bytes a number takes, seven bits a byte
#define KREP_NUMBER_MOST 10u

// the words of a row of a file's table before the change's own numbers: the file's key, the who's two words and the
// change
#define KREP_ROW_OPENING 4u

// the words of a head row before its name: the opening, then the kind, the version and the name's length
#define KREP_HEAD_WORDS (KREP_ROW_OPENING + 3u)


unsigned int krep_half_of(const unsigned char *bytes);
void krep_half_put(unsigned char *bytes, unsigned int half);
int krep_number_read(const unsigned char *bytes, unsigned long long size, unsigned long long *at, unsigned long long *value);
void krep_number_added(QryText *text, unsigned long long value);
unsigned long long krep_row_handed(KrepFile *file, unsigned long long key, const unsigned long long *who, unsigned int change, const unsigned long long *numbers, unsigned int count, const void *bytes, unsigned long long size, unsigned int priority);
void krep_who_words_at(const KrepFile *file, unsigned int at, unsigned long long *words);
void krep_record_row(KrepFile *file, unsigned int at, const unsigned long long *who);
void krep_text_row(KrepFile *file, unsigned int number);
unsigned int krep_text_slot(const KrepFile *file, const char *bytes, unsigned long long length);
unsigned long long krep_text_taken(KrepFile *file, const char *bytes, unsigned long long length);

#endif
