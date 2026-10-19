// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pairs_internal.h: what pairs.c and pairs_modes.c share: what a pairs' words ask for, the folders the modes use, and
// the calls one reads from the other
#ifndef PAIRS_INTERNAL_H
#define PAIRS_INTERNAL_H

#include "../query_loops.h"

// the stick's folder, the vendor's reading of it and, under `engine/`, the engine's; the identity's working folder;
// and the run folder where no trace folder is named
#define QUERY_PAIRS_STICK "build/measuring_stick"

#define QUERY_PAIRS_WORK "build/engine/identity"

#define QUERY_PAIRS_FOLDER "build/engine/qry"

// What a pairs' words ask for: the mode, its own words, the part and the language, dry or not, the seed the links'
// order is drawn from, and the folder the run goes in
typedef struct
{
    const char *mode;
    const char *words[QUERY_WORDS_MOST];
    unsigned int word_count;
    const char *part;
    const char *to;
    int dry;
    const char *seed;
    const char *trace;
} QueryPairsMode;

// the part's name as the vendor's reader takes it: `sm_86` is `SM86`, into `name`, which holds `room`
void query_part_read(const char *part, char *name, size_t room);
// the carrier's words with the tools built and the vendor's reader found, added to `words`: the airlock every round
// passes before the part, the pop and the cross, and the carrier stopped before the driver where `dry`
int query_pairs_carry(QueryContext *context, const char *part, int dry, QueryWords *words, CliLine *said);
// the part's files, the bridge, the rulesets and the stick's manifest read into the run
int query_pairs_configs(QueryContext *context, QueryRun *run, const char *part, CliLine *said);
// the host's questions, the C++ the run holds as `host_questions.cpp`, compiled under `out` with signed arithmetic
// wrapping as the part's does, run, and its answers handed as `host_answers.txt`
int query_host_answered(QueryContext *context, const QueryRun *run, const char *out, CliLine *said);
// `folder`/`name` into `path`; `part` and `extension` under the bridge's folder into `path`; the identity run with
// `words` teed as `klq_identity.console`, 1 where it ran (pairs_modes.c)
void query_pairs_in(const char *folder, const char *name, char *path);
void query_part_file(const QueryContext *context, const char *part, const char *extension, char *path);
int query_identity_ran(QueryContext *context, const QueryRun *run, const char *identity, const char *const *words,
                       unsigned int count, long long *code, CliLine *said);
// the modes (pairs_modes.c, pairs_judged.c): every identity between texts in `Lstar.klq`; the modes over the stick's
// questions and the host's answers; and the stick's questions sliced into identities
int query_text_identity(QueryContext *context, const QueryPairsMode *mode, const char *identity, const char *folder,
                        CliLine *said);
int query_judged(QueryContext *context, const QueryPairsMode *mode, const char *identity, const char *folder,
                 CliLine *said);
int query_stick_sliced(QueryContext *context, const QueryPairsMode *mode, const char *identity, const char *folder,
                       CliLine *said);

// what the pairs' checks reach: a pairs' words read into `mode`: 1, or 0 with why said (pairs.c); the carrier's
// words for a pairs' pass, its dry flags first where `dry` as it reads them (pairs.c); the words a mode's program
// is given after its folder; whether a mode opens with the kernel the system accepted; and the vendor's writer's
// words after its program that hand the run that kernel (pairs_judged.c)
int query_pairs_read(const char *const *words, unsigned int count, QueryPairsMode *mode, CliLine *said);
void query_pairs_carry_words(QueryWords *words, const char *carrier, const char *cross, const char *nvdisasm,
                             const char *machine, const char *layout, const char *part, int dry);
void query_targets(const QueryContext *context, const QueryPairsMode *mode, QueryWords *words);
int query_kernel_handed(const char *mode);
void query_kernel_words(QueryWords *words, const char *part, const QueryFiles *files);

#endif
