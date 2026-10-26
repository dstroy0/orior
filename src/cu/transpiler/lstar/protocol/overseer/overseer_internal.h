// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// overseer_internal.h: what the modules of the overseer share: its includes, the tables it writes, the bounds of the
// lessons, carriers, experts, votes and clauses, the lessons stored, the causes' words, the votes and what was
// received, and the functions one module reads from another (overseer.h)
#ifndef OVERSEER_INTERNAL_H
#define OVERSEER_INTERNAL_H

#include "overseer.h"
#include "verba_scribo/verba_linea.h"
#include <stdio.h>
#include <string.h>

#include <stdlib.h>
#include <string.h>

// the run's tables of the accounts the overseer receives, the conferences it convenes and the lessons it teaches, each
// row naming its conference by the number of the question's identity in the table of texts (accounts.oracle.tsv,
// conferences.oracle.tsv, lessons.oracle.tsv)
#define OVERSEER_ACCOUNTS_TABLE "accounts"

#define OVERSEER_CONFERENCES_TABLE "conferences"

#define OVERSEER_LESSONS_TABLE "lessons"

// the most lessons a run stores, the longest carrier name, the most experts a conference names, and the most causes it
// receives read
#define OVERSEER_LESSONS_MOST 16384u

#define OVERSEER_CARRIER_MAX_LEN 64u

#define OVERSEER_EXPERTS_MOST 32u

#define OVERSEER_EXPERT_MAX_LEN 32u

#define OVERSEER_VOTES_MOST 64u

#define OVERSEER_CLAUSE_MAX_LEN 512u

// one lesson: the cause it was taught by, the carrier it applies at, `*` at every carrier, and the stand-in as written
typedef struct
{
    unsigned int cause;
    char carrier[OVERSEER_CARRIER_MAX_LEN];
    char standing[OVERSEER_STANDING_MAX_LEN];
} OverseerLesson;

// each cause by the word an account reads it with, in the order of OverseerCause
static const char *const s_cause_words[] = {"none", "instruction", "address", "register",
                                            "time", "exception",   "admitted"};

// one expert's reading of the cause
typedef struct
{
    char expert[OVERSEER_EXPERT_MAX_LEN];
    unsigned int cause;
} OverseerVote;

// what a conference's accounts report, read clause by clause; `admits` and `blocks` the votes of an admission
// conference
typedef struct
{
    char experts[OVERSEER_EXPERTS_MOST][OVERSEER_EXPERT_MAX_LEN];
    unsigned int expert_count;
    OverseerVote votes[OVERSEER_VOTES_MOST];
    unsigned int vote_count;
    int addresses;
    int controls;
    int scratch;
    char carrier[OVERSEER_CARRIER_MAX_LEN];
    char standing[OVERSEER_STANDING_MAX_LEN];
    char learning[OVERSEER_CLAUSE_MAX_LEN];
    char counterexample[OVERSEER_CLAUSE_MAX_LEN];
    unsigned int accounts;
    unsigned int admits;
    unsigned int blocks;
} OverseerTally;

extern OverseerLesson s_lessons[OVERSEER_LESSONS_MOST];
extern unsigned int s_lesson_count;
extern OverseerTally s_conference_tally;

unsigned int overseer_cause_read(const char *word);
void overseer_trimmed(const char *from, const char *to, char *into, size_t room);
void overseer_accounts_read(OverseerTally *tally, const unsigned char *bytes, unsigned long long size,
                            const unsigned char *before, unsigned long long before_size, QryShared *run,
                            unsigned long long conference);

#endif
