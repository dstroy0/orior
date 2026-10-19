// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kqr_internal.h: what the modules of R, the .kqr's reader and writer, share: its includes, the bounds of the record,
// the answers' names, and the functions one module reads from another (kqr.h)
#ifndef KQR_INTERNAL_H
#define KQR_INTERNAL_H

#include "../kqr_record.h"

#include "../../../parsers/who.h"
#include "verba_scribo/verba_scribo.h"
#include <stdlib.h>
#include <string.h>

// the slots of the table an ask is found by its identity in, a power of two past every ask a record holds
#define KQR_SLOTS 1048576u

// the words of an ask's identity and answer in its record, before its answer's words or its refusal: the identity's
// seven numbers, then the answer
#define KQR_ASKED_WORDS 8u

// the most letters of a line of the text form read
#define KQR_LINE (1u << 20u)

int kqr_number_read(const char *text, size_t length, int base, unsigned long long *value);
unsigned int kqr_slot_of(const KqrRecord *record, const KqrIdentity *identity);
void kqr_text_copied(const KqrRecord *record, unsigned long long number, char *text, size_t room);
unsigned long long kqr_text_number(KqrRecord *record, const char *text);
int kqr_ask_of(const KqrRecord *record, const KrepRecord *held, KqrAsk *ask);
unsigned int kqr_ask_words(KqrRecord *record, const KqrAsk *ask, unsigned long long *words);
int kqr_sample_of(const KqrRecord *record, const KrepRecord *held, KqrSample *sample);
unsigned int kqr_sample_words(KqrRecord *record, const KqrSample *sample, unsigned long long *words);
void kqr_shifted(KqrRecord *record, unsigned int from, unsigned int by);
int kqr_who_put(KqrRecord *record, unsigned int at, int held, Who who);
unsigned int kqr_put(KqrRecord *record, unsigned int code, const unsigned long long *words, unsigned int count,
                     Who who);
void kqr_marks_written(KqrRecord *record, Who who);
int kqr_opened(KqrRecord *record, const char *member);
int kqr_sample_held(KqrRecord *record, unsigned int at);
void kqr_indexed(KqrRecord *record);

#endif
