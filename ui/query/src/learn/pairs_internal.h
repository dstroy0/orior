// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pairs_internal.h: what pairs.c and pairs_modes.c share: what a pairs' words request, the folders the modes use, and
// the calls one reads from the other
#ifndef PAIRS_INTERNAL_H
#define PAIRS_INTERNAL_H
typedef struct QueryPairsPartDryCtx QueryPairsPartDryCtx;
typedef struct QueryPairsContextErrMsgRunCtx QueryPairsContextErrMsgRunCtx;
typedef struct QueryPairsContextErrMsgCtx QueryPairsContextErrMsgCtx;
typedef struct QueryPairsPartContextDryCtx QueryPairsPartContextDryCtx;
typedef struct QueryPairsContextCtx QueryPairsContextCtx;
typedef struct QueryPairsPartContextCtx QueryPairsPartContextCtx;
typedef struct QueryPairsErrMsgCtx QueryPairsErrMsgCtx;
typedef struct QueryPairsPartPathCtx QueryPairsPartPathCtx;
typedef struct QueryPairsPartCtx QueryPairsPartCtx;
typedef struct QueryPairsFolderCtx QueryPairsFolderCtx;
typedef struct QueryPairsPartNameCtx QueryPairsPartNameCtx;
typedef struct QueryPairsWordsCtx QueryPairsWordsCtx;
typedef struct QueryPairsCtx QueryPairsCtx;

#include "../query_loops.h"
#include "embed_dispatch_layout.h"

// the stick's folder, the vendor's reading of it and, under `engine/`, the engine's; the identity's working folder;
// and the run folder where no trace folder is named
#define QUERY_PAIRS_STICK "build/measuring_stick"

#define QUERY_PAIRS_WORK "build/engine/identity"

#define QUERY_PAIRS_FOLDER "build/engine/qry"

// What a pairs' words request: the mode, its own words, the part and the language, dry or not, the seed the links'
// order is drawn from, and the folder the run goes in
typedef struct
{
    const char *mode;
    const char *words[QUERY_WORDS_MAX];
    unsigned int word_count;
    const char *part;
    const char *to;
    int dry;
    const char *seed;
    const char *trace;
} QueryPairsMode;

// the part's name as the vendor's reader takes it: `sm_86` is `SM86`, into `name`, sized for `capacity`
void query_part_read(const QueryPairsPartNameCtx *args);
// the carrier's words with the tools built and the vendor's reader found, added to `words`: the airlock every round
// passes before the part, the pop and the cross, and the carrier stopped before the driver where `dry`
int query_pairs_machine_transfer(const QueryPairsPartContextDryCtx *args);
// the part's files, the bridge, the rulesets and the stick's manifest read into the run
int query_pairs_configs(const QueryPairsPartContextCtx *args);
// the host's questions, the C++ the run contains as `host_questions.cpp`, compiled under `out` with signed arithmetic
// wrapping as the part's does, run, and its answers handed as `host_answers.txt`
int query_host_answer(const QueryPairsContextCtx *args);
// `folder`/`name` into `path`; `part` and `extension` under the bridge's folder into `path`; the identity run with
// `words` teed as `orior_klq_identity.console`, 1 where it ran (pairs_modes.c)
void query_pairs_in(const QueryPairsFolderCtx *args);
void query_part_file(const QueryPairsPartPathCtx *args);
int query_identity_run(const QueryPairsContextErrMsgRunCtx *args);
// the modes (pairs_modes.c, pairs_judged.c): every identity between texts in `Lstar.klq`; the modes over the stick's
// questions and the host's answers; and the stick's questions sliced into identities
int query_text_identity(const QueryPairsContextErrMsgCtx *args);
int query_judged(const QueryPairsContextErrMsgCtx *args);
int query_stick_slice(const QueryPairsContextErrMsgCtx *args);

// what the pairs' checks reach: a pairs' words read into `mode`: 1, or 0 with why given (pairs.c); the carrier's
// words for a pairs' pass, its dry flags first where `dry` as it reads them (pairs.c); the words a mode's program
// is given after its folder; whether a mode opens with the kernel the system accepted; and the vendor's writer's
// words after its program that hand the run that kernel (pairs_judged.c)
int query_pairs_read(const QueryPairsErrMsgCtx *args);
void query_pairs_machine_transfer_words(const QueryPairsPartDryCtx *args);
void query_targets(const QueryPairsWordsCtx *args);
int query_kernel_dispatch(const QueryPairsCtx *args);
void query_kernel_words(const QueryPairsPartCtx *args);

struct QueryPairsPartDryCtx
{
    const char *part;
    int dry;
    QueryWords *words;
    const char *transport;
    const char *cross;
    const char *nvdisasm;
    const char *machine;
    const char *layout;
};

struct QueryPairsContextErrMsgRunCtx
{
    QueryContext *context;
    VerbaLinea *err_msg;
    const QueryRun *run;
    const char *identity;
    const char *const *words_letters;
    unsigned int count;
    long long *code;
};

struct QueryPairsContextErrMsgCtx
{
    QueryContext *context;
    VerbaLinea *err_msg;
    const char *folder;
    const char *identity;
    const QueryPairsMode *mode;
};

struct QueryPairsPartContextDryCtx
{
    const char *part;
    QueryContext *context;
    int dry;
    QueryWords *words;
    VerbaLinea *err_msg;
};

struct QueryPairsContextCtx
{
    QueryContext *context;
    VerbaLinea *err_msg;
    const QueryRun *run;
    const char *out;
};

struct QueryPairsPartContextCtx
{
    const char *part;
    QueryContext *context;
    VerbaLinea *err_msg;
    QueryRun *run_out;
};

struct QueryPairsErrMsgCtx
{
    VerbaLinea *err_msg;
    const char *const *words_letters;
    unsigned int count;
    QueryPairsMode *mode_out;
};

struct QueryPairsPartPathCtx
{
    const char *part;
    char *path;
    const QueryContext *context_read;
    const char *extension;
};

struct QueryPairsPartCtx
{
    const char *part;
    QueryWords *words;
    const QueryFiles *files;
};

struct QueryPairsFolderCtx
{
    const char *folder;
    const char *name_read;
    char *path;
};

struct QueryPairsPartNameCtx
{
    const char *part;
    char *name;
    size_t capacity;
};

struct QueryPairsWordsCtx
{
    QueryWords *words;
    const QueryContext *context_read;
    const QueryPairsMode *mode;
};

struct QueryPairsCtx
{
    const char *mode_text;
};

typedef struct
{
    void (*part_read)(const QueryPairsPartNameCtx *args);
} QueryPairsPartNameNs;
EMBED_TABLE_LAYOUT(QueryPairsPartNameNs, part_read);
EMBED_TABLE_STORAGE QueryPairsPartNameNs query_pairs_part_name_internal EMBED_UNUSED = {
    .part_read = query_part_read,
};

typedef struct
{
    int (*carry)(const QueryPairsPartContextDryCtx *args);
} QueryPairsPartContextDryNs;
EMBED_TABLE_LAYOUT(QueryPairsPartContextDryNs, carry);
EMBED_TABLE_STORAGE QueryPairsPartContextDryNs query_pairs_part_context_dry_internal EMBED_UNUSED = {
    .carry = query_pairs_machine_transfer,
};

typedef struct
{
    int (*configs)(const QueryPairsPartContextCtx *args);
} QueryPairsPartContextNs;
EMBED_TABLE_LAYOUT(QueryPairsPartContextNs, configs);
EMBED_TABLE_STORAGE QueryPairsPartContextNs query_pairs_part_context_internal EMBED_UNUSED = {
    .configs = query_pairs_configs,
};

typedef struct
{
    int (*host_answer)(const QueryPairsContextCtx *args);
} QueryPairsContextNs;
EMBED_TABLE_LAYOUT(QueryPairsContextNs, host_answer);
EMBED_TABLE_STORAGE QueryPairsContextNs query_pairs_context_internal EMBED_UNUSED = {
    .host_answer = query_host_answer,
};

typedef struct
{
    void (*in)(const QueryPairsFolderCtx *args);
} QueryPairsFolderNs;
EMBED_TABLE_LAYOUT(QueryPairsFolderNs, in);
EMBED_TABLE_STORAGE QueryPairsFolderNs query_pairs_folder_internal EMBED_UNUSED = {
    .in = query_pairs_in,
};

typedef struct
{
    void (*part_file)(const QueryPairsPartPathCtx *args);
} QueryPairsPartPathNs;
EMBED_TABLE_LAYOUT(QueryPairsPartPathNs, part_file);
EMBED_TABLE_STORAGE QueryPairsPartPathNs query_pairs_part_path_internal EMBED_UNUSED = {
    .part_file = query_part_file,
};

typedef struct
{
    int (*identity_run)(const QueryPairsContextErrMsgRunCtx *args);
} QueryPairsContextErrMsgRunNs;
EMBED_TABLE_LAYOUT(QueryPairsContextErrMsgRunNs, identity_run);
EMBED_TABLE_STORAGE QueryPairsContextErrMsgRunNs query_pairs_context_err_msg_run_internal EMBED_UNUSED = {
    .identity_run = query_identity_run,
};

typedef struct
{
    int (*text_identity)(const QueryPairsContextErrMsgCtx *args);
    int (*judged)(const QueryPairsContextErrMsgCtx *args);
    int (*stick_slice)(const QueryPairsContextErrMsgCtx *args);
} QueryPairsContextErrMsgNs;
EMBED_TABLE_LAYOUT(QueryPairsContextErrMsgNs, text_identity, judged, stick_slice);
EMBED_TABLE_STORAGE QueryPairsContextErrMsgNs query_pairs_context_err_msg_internal EMBED_UNUSED = {
    .text_identity = query_text_identity,
    .judged = query_judged,
    .stick_slice = query_stick_slice,
};

typedef struct
{
    int (*read)(const QueryPairsErrMsgCtx *args);
} QueryPairsErrMsgNs;
EMBED_TABLE_LAYOUT(QueryPairsErrMsgNs, read);
EMBED_TABLE_STORAGE QueryPairsErrMsgNs query_pairs_err_msg_internal EMBED_UNUSED = {
    .read = query_pairs_read,
};

typedef struct
{
    void (*machine_transfer_words)(const QueryPairsPartDryCtx *args);
} QueryPairsPartDryNs;
EMBED_TABLE_LAYOUT(QueryPairsPartDryNs, machine_transfer_words);
EMBED_TABLE_STORAGE QueryPairsPartDryNs query_pairs_part_dry_internal EMBED_UNUSED = {
    .machine_transfer_words = query_pairs_machine_transfer_words,
};

typedef struct
{
    void (*targets)(const QueryPairsWordsCtx *args);
} QueryPairsWordsNs;
EMBED_TABLE_LAYOUT(QueryPairsWordsNs, targets);
EMBED_TABLE_STORAGE QueryPairsWordsNs query_pairs_words_internal EMBED_UNUSED = {
    .targets = query_targets,
};

typedef struct
{
    int (*kernel_dispatch)(const QueryPairsCtx *args);
} QueryPairsNs;
EMBED_TABLE_LAYOUT(QueryPairsNs, kernel_dispatch);
EMBED_TABLE_STORAGE QueryPairsNs query_pairs_internal EMBED_UNUSED = {
    .kernel_dispatch = query_kernel_dispatch,
};

typedef struct
{
    void (*kernel_words)(const QueryPairsPartCtx *args);
} QueryPairsPartNs;
EMBED_TABLE_LAYOUT(QueryPairsPartNs, kernel_words);
EMBED_TABLE_STORAGE QueryPairsPartNs query_pairs_part_internal EMBED_UNUSED = {
    .kernel_words = query_kernel_words,
};

#endif
