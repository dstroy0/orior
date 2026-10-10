// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef RECORD_H
#define RECORD_H

// R, the record: every ask with its answer, the part and the size it was read on (Q9, Q10, P13). An ask R holds is not
// put again. It is the part's .kqr (src/cu/types/file_defs/kqr/kqr.oracle.tsv), the same lines the query record of
// klq_identity reads and writes, so that every program that asks the part holds one record of it.
//
// An ask is held by its identity: the hash of its code, the registers, threads, blocks and launches it is put with,
// its count of cases and the hash of its cases, seven words apart by one space. What came back is one of answers,
// illegal, nothing, timed_out or censored: answers with the word of every case, and the others with the refusal. An
// untimed ask is answered from R where R holds it and it did not time out; one that timed out is asked again until an
// answer resolves it, and the answer takes its place. A timed ask is put every time and kept as a sample of its own.
//
// The record is read whole when it is opened and written back whole when it is closed, every line it held as it held
// it, and every ask and sample this run put after them under one line `cycle <n> <mode>`, n after every cycle it held.
// A run that put nothing new adds no line.

// the longest refusal kept, and the most words of an answer
#define RECORD_REFUSAL 128u
#define RECORD_WORDS 4096u

// what R holds of one ask
typedef struct
{
    // answers, illegal, nothing, timed_out or censored
    char answer[16];
    unsigned long long word[RECORD_WORDS];
    unsigned int words;
    char refusal[RECORD_REFUSAL];
} RecordAsk;

// R read from `path`, the part `member`, and every ask this run puts written under a cycle of `mode`. 1, an empty
// record where no file is there, or 0 with the line and the reason printed where a line is none the record holds
int record_open(const char *path, const char *member, const char *mode);

// 1 where a record is open
int record_held(void);

// 1 with what R holds of the ask of `identity` in `held`, where it holds it and it did not time out; 0 otherwise
int record_answer(const char *identity, RecordAsk *held);

// the untimed ask of `identity` kept, where R holds none of it or holds it timed out
void record_keep(const char *identity, const RecordAsk *ask);

// the timed ask of `identity` kept as a sample of its own: `answer`, and the nanoseconds where it answered, with the
// least and the most one launch took where `most` is not 0, or the refusal where it did not
void record_sample(const char *identity, const char *answer, unsigned long long nanoseconds, unsigned long long least,
                   unsigned long long most, const char *refusal);

// R written back to the path it was read from and closed. 1, or 0 with the reason printed
int record_close(void);

#endif
