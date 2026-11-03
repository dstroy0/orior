// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_internal.h: what the deployment's sources share: the deployments deploy.json names, the places one sends to,
// the files it sends and leaves out, and what each place has against what goes
#ifndef DEPLOY_INTERNAL_H
#define DEPLOY_INTERNAL_H
#include "../comms/link.h"
#include "../text/json_tree.h"
#include "deploy.h"
#include "embed_dispatch_layout.h"

typedef struct CliDeployErrMsgCtx CliDeployErrMsgCtx;
typedef struct CliDeploySizeCtx CliDeploySizeCtx;
typedef struct CliDeployErrMsgIndexCtx CliDeployErrMsgIndexCtx;
typedef struct CliDeployRootCtx CliDeployRootCtx;
typedef struct CliDeployErrMsgPathCtx CliDeployErrMsgPathCtx;
typedef struct CliDeployOldCtx CliDeployOldCtx;
typedef struct CliDeployTextCtx CliDeployTextCtx;
typedef struct CliDeployCountCtx CliDeployCountCtx;
typedef struct CliDeploySizePatternCtx CliDeploySizePatternCtx;
typedef struct CliDeployPatternsCtx CliDeployPatternsCtx;
typedef struct CliDeployOutCtx CliDeployOutCtx;
typedef struct CliDeployPoolCtx CliDeployPoolCtx;
typedef struct CliDeployOutIndexCtx CliDeployOutIndexCtx;
typedef struct CliDeployInTreeCtx CliDeployInTreeCtx;
typedef struct CliDeployLeftCtx CliDeployLeftCtx;
typedef struct CliDeployIndexCtx CliDeployIndexCtx;
typedef struct CliDeployIndexPathCtx CliDeployIndexPathCtx;
typedef struct CliDeployOutSecondsCtx CliDeployOutSecondsCtx;
typedef struct CliDeployCtx CliDeployCtx;

// the bytes of each page of a pool, a text longer taking a page of its own
#define CLI_DEPLOY_PAGE (64u << 10u)

// the most bytes of a shell line a deployment runs and of the listing of a machine's releases, and of the hashes of the
// files a place has
#define CLI_DEPLOY_LINE (1ull << 20u)
#define CLI_DEPLOY_HASHES (32ull << 20u)

// A deployment: its name, the folder whose files go, the patterns of the files that stay, the places, how many
// releases each place retains, and whether its files go into each place's folder itself
typedef struct
{
    const char *name;
    const char *from;
    const char **exclude;
    unsigned int excludes;
    const char **to;
    unsigned int tos;
    unsigned long long retain;
    int in_position;
} CliDeployment;

// A place a deployment sends to: a machine orior_cli reaches, or a folder on this one, by its path
typedef struct
{
    int here;
    CliLinkAddress address;
    char path[CLI_PATH_MAX_LEN];
} CliDeployPosition;

// A file a deployment sends: its path under the folder sent, its mode, and the SHA-256 of its bytes, empty where they
// could not be read
typedef struct
{
    const char *path;
    unsigned int mode;
    char hash[65];
} CliDeploySent;

// A path and a text beside it: a file left out and the pattern that leaves it out, or a file at a place and its hash
typedef struct
{
    const char *path;
    const char *text;
} CliDeployPair;

// A pair stored in a pool before the pairs stored already
typedef struct CliDeployPairNodeStruct
{
    struct CliDeployPairNodeStruct *before;
    CliDeployPair pair;
} CliDeployPairNode;

// A page of a pool, a cell of its own: the page taken before it, its bytes and those used, and the bytes
typedef struct CliDeployPageStruct
{
    struct CliDeployPageStruct *before;
    unsigned long long size;
    unsigned long long used;
    char bytes[];
} CliDeployPage;

// A pool: its pages, taken as they are needed, the newest first, and whether one could not be taken
typedef struct
{
    CliDeployPage *page;
    int refused;
} CliDeployPool;

// A run of a deployment: the deployment; its places; the patterns every file is matched against; the files it sends, by
// their paths, and those it leaves out; for each place, the files changed there since they were last sent with their
// hashes there, and the files there the tree no longer has; each list in the run's pool at its count; and the pool,
// given back as the next run begins and as this one ends
typedef struct
{
    CliDeployment deployment;
    CliDeployPosition *place;
    unsigned int positions;
    const char **pattern;
    unsigned int patterns;
    CliDeploySent *sent;
    unsigned int sents;
    CliDeployPair *out;
    unsigned int outs;
    CliDeployPair **changed_list;
    unsigned int *changed;
    CliDeployPair **removed_list;
    unsigned int *deleted;
    CliDeployPool pool;
    const char *root;
    CliDeployPrintFn print;
    void *context;
} CliDeployRun;

// A tree deploy.json is read into, taken to the file's size, and the pool its deployments are stored in
typedef struct
{
    CliJson json;
    CliDeployPool pool;
} CliDeployTree;

// the one run, the lock it is guarded by, and a line printed through its teller (deploy_run.c)
extern CliDeployRun g_cli_deploy;
void cli_deploy_print(const CliDeployCtx *args);
// the lines of `new_text` that differ from `old`, each printed through the run's teller marked `-` for a line taken
// out and `+` for a line put in, around the run of lines the two do not share at their heads and tails
// (deploy_existing.c)
void cli_deploy_difference(const CliDeployOldCtx *args);

// `size` bytes placed in `pool` at a multiple of 8: where they stand, NULL with the pool marked refused where a page
// could not be taken; `size` bytes of `text` stored there with a 0 after, an empty text where they could not be; and
// every page of a pool given back, the pool left empty (deploy_read.c)
void *cli_deploy_on_position(const CliDeployPoolCtx *args);
const char *cli_deploy_text_store(const CliDeployPoolCtx *args);
void cli_deploy_pool_free(const CliDeployPoolCtx *args);
// two texts in the order of their bytes: below 0 where `left` comes first, 0 where they are the same, above 0 where
// `right` does (deploy_read.c)
int cli_deploy_order(const CliDeployLeftCtx *args);
// the deployments deploy.json at the tree's top folder names read into `tree`, `*count` of them from `*deployments`:
// 1, or 0 with why given, none where it is not there; the tree given back; a place read from `text`, a folder on this
// machine where it is a path, against the tree at `root`: 1, or 0 with why given; the place's name; the place's folder;
// and a shell line run at a place, `size` bytes of `input` given it, what it wrote into `out`, a machine that prompts
// for a password informed how a key reaches it (deploy_read.c)
int cli_deploy_read(const CliDeployRootCtx *args);
void cli_deploy_tree_free(const CliDeployRootCtx *args);
int cli_deploy_position(const CliDeployTextCtx *args);
void cli_deploy_position_name(const CliDeploySizeCtx *args);
const char *cli_deploy_position_folder(const CliDeploySizeCtx *args);
int cli_deploy_remote_run(const CliDeploySizeCtx *args);

// whether `name` fits the glob `pattern`, `*` taking any run of letters and `?` any one; the pattern of `patterns`,
// `count` of them, that leaves out the file at `path` under the folder sent, NULL where none does; and the name of the
// release made `seconds` after 1970 began, in UTC, which sorts as the releases were made (deploy_gather.c)
int cli_deploy_fits(const CliDeploySizePatternCtx *args);
const char *cli_deploy_left_out_by(const CliDeployPatternsCtx *args);
void cli_deploy_release_name(const CliDeployOutSecondsCtx *args);
// the modes git's index gives the run's tree's files read into `pool`; and the mode of the file at `in_tree` in the
// tree, at `full` here, while that pool has them: `0755` where it runs and `0644` where not, as the index gives it,
// else as it is here (deploy_gather.c)
void cli_deploy_modes_read(const CliDeployPoolCtx *args);
unsigned int cli_deploy_mode(const CliDeployInTreeCtx *args);
// the files the run's deployment sends and those it leaves out, each hashed, sorted by path: 1, or 0 with why given;
// and the full path of the file the run sends at `index` into `out` (deploy_walk.c)
int cli_deploy_gather(const CliDeployTextCtx *args);
void cli_deploy_full(const CliDeployOutIndexCtx *args);
// the tar stream of the files the run sends but the paths of `preserved`, `preserved_count` of them, each with its mode
// and the time `time`, a path longer than a ustar header fits in a PAX header before it, written to the file at `path`:
// 1, or 0 with why given (deploy_tar.c)
int cli_deploy_tar(const CliDeployErrMsgPathCtx *args);

// the files a place has and their hashes, and those last sent there and theirs, each by path and sorted, their
// counts beside them; and each given back (deploy_there.c)
extern CliDeployPair *g_cli_deploy_there;
extern unsigned int g_cli_deploy_there_count;
extern CliDeployPair *g_cli_deploy_last;
extern unsigned int g_cli_deploy_last_count;
void cli_deploy_there_free(void);

// the text beside `path` among `count` pairs sorted by path, NULL where it is not among them; the hash the run sends
// `path` with, NULL where it does not send it; the file of the record of the hashes last sent by the run's deployment
// to the place at `index` into `path`, sized for CLI_PATH_MAX_LEN: 1, or 0 where orior's own folder is not known;
// that record read into g_cli_deploy_last, none where it is not there or reads as no object of texts; and the hashes of
// the files under `folder` of the place at `index` read into g_cli_deploy_there: 1, or 0 with why given
// (deploy_there.c)
const char *cli_deploy_find(const CliDeployCountCtx *args);
const char *cli_deploy_ours(const CliDeployCountCtx *args);
int cli_deploy_record_path(const CliDeployIndexPathCtx *args);
void cli_deploy_last_read(const CliDeployErrMsgIndexCtx *args);
int cli_deploy_hashes(const CliDeployErrMsgIndexCtx *args);

// what the place at `index` has under `folder` against what was last sent there and what goes now, recorded in the run,
// and printed as it is found: 1, or 0 with why given; and the record of the hashes last sent to a place written after a
// send, the files it retained there recorded as they are there (deploy_existing.c)
int cli_deploy_existing(const CliDeployErrMsgIndexCtx *args);
void cli_deploy_record(const CliDeployErrMsgIndexCtx *args);

// the files sent to the place at `index`, into a new release `release` where `releases`, else into its folder itself,
// those retained there and those gone retained, carried over from the release it runs: 1, or 0 with why given
// (deploy_send.c)
int cli_deploy_send_to(const CliDeployErrMsgCtx *args);
// the shell line that turns a machine's `current` in `folder` to `release`; a place's releases past the newest `retain`
// taken away; and each place's `current` turned back to the release before it: 1, or 0 with why given (deploy_send.c)
void cli_deploy_switch_line(const CliDeployOutCtx *args);
void cli_deploy_prune(const CliDeployIndexCtx *args);
int cli_deploy_roll_back(const CliDeployTextCtx *args);
// the files at the place at `index` that the tree no longer has deleted, in its folder itself, each printed: 1, or 0
// with why given (deploy_send.c)
int cli_deploy_remove(const CliDeployErrMsgIndexCtx *args);

struct CliDeployErrMsgCtx
{
    VerbaLinea *err_msg;
    unsigned int index;
    unsigned long long time;
    int replace;
    int remove;
    int releases;
    const char *release;
};

struct CliDeploySizeCtx
{
    unsigned long long size;
    VerbaLinea *err_msg;
    const CliDeployPosition *place;
    VerbaLinea *out;
    const char *line_text;
    const char *input;
};

struct CliDeployErrMsgIndexCtx
{
    VerbaLinea *err_msg;
    unsigned int index;
    const char *folder;
    int replace;
    int remove;
};

struct CliDeployRootCtx
{
    const char *root;
    CliDeployTree *tree;
    const CliDeployment **deployments;
    unsigned int *count_pointer;
    VerbaLinea *err_msg;
};

struct CliDeployErrMsgPathCtx
{
    VerbaLinea *err_msg;
    const char *path;
    const CliDeployPair *preserved;
    unsigned int preserved_count;
    unsigned long long time;
};

struct CliDeployOldCtx
{
    const char *old;
    unsigned long long old_size;
    const char *new_text;
    unsigned long long new_size;
};

struct CliDeployTextCtx
{
    const char *text;
    const char *root;
    VerbaLinea *err_msg;
    CliDeployPosition *position_out;
};

struct CliDeployCountCtx
{
    unsigned int count;
    const char *path;
    const CliDeployPair *pairs;
};

struct CliDeploySizePatternCtx
{
    unsigned long long size;
    const char *pattern;
    const char *name;
};

struct CliDeployPatternsCtx
{
    const char *const *patterns;
    unsigned int count;
    const char *path;
};

struct CliDeployOutCtx
{
    VerbaLinea *out;
    const char *folder;
    const char *release;
};

struct CliDeployPoolCtx
{
    CliDeployPool *pool;
    unsigned long long size;
    const char *text;
};

struct CliDeployOutIndexCtx
{
    VerbaLinea *out;
    unsigned int index;
};

struct CliDeployInTreeCtx
{
    const char *in_tree;
    const char *full;
};

struct CliDeployLeftCtx
{
    const char *left;
    const char *right;
};

struct CliDeployIndexCtx
{
    unsigned int index;
    unsigned long long retain;
};

struct CliDeployIndexPathCtx
{
    unsigned int index;
    char *path_out;
};

struct CliDeployOutSecondsCtx
{
    VerbaLinea *out;
    unsigned long long seconds;
};

struct CliDeployCtx
{
    const VerbaLinea *line;
};

typedef struct
{
    void (*print)(const CliDeployCtx *args);
} CliDeployNsTable;
EMBED_TABLE_LAYOUT(CliDeployNsTable, print);
EMBED_TABLE_STORAGE CliDeployNsTable cli_deploy_internal EMBED_UNUSED = {
    .print = cli_deploy_print,
};

typedef struct
{
    void (*difference)(const CliDeployOldCtx *args);
} CliDeployOldNs;
EMBED_TABLE_LAYOUT(CliDeployOldNs, difference);
EMBED_TABLE_STORAGE CliDeployOldNs cli_deploy_old_internal EMBED_UNUSED = {
    .difference = cli_deploy_difference,
};

typedef struct
{
    void *(*place)(const CliDeployPoolCtx *args);
    const char *(*text_store)(const CliDeployPoolCtx *args);
    void (*pool_free)(const CliDeployPoolCtx *args);
    void (*modes_read)(const CliDeployPoolCtx *args);
} CliDeployPoolNs;
EMBED_TABLE_LAYOUT(CliDeployPoolNs, place, text_store, pool_free, modes_read);
EMBED_TABLE_STORAGE CliDeployPoolNs cli_deploy_pool_internal EMBED_UNUSED = {
    .place = cli_deploy_on_position,
    .text_store = cli_deploy_text_store,
    .pool_free = cli_deploy_pool_free,
    .modes_read = cli_deploy_modes_read,
};

typedef struct
{
    int (*order)(const CliDeployLeftCtx *args);
} CliDeployLeftNs;
EMBED_TABLE_LAYOUT(CliDeployLeftNs, order);
EMBED_TABLE_STORAGE CliDeployLeftNs cli_deploy_left_internal EMBED_UNUSED = {
    .order = cli_deploy_order,
};

typedef struct
{
    int (*read)(const CliDeployRootCtx *args);
    void (*tree_free)(const CliDeployRootCtx *args);
} CliDeployRootNsTable;
EMBED_TABLE_LAYOUT(CliDeployRootNsTable, read, tree_free);
EMBED_TABLE_STORAGE CliDeployRootNsTable cli_deploy_root_internal EMBED_UNUSED = {
    .read = cli_deploy_read,
    .tree_free = cli_deploy_tree_free,
};

typedef struct
{
    int (*place)(const CliDeployTextCtx *args);
    int (*gather)(const CliDeployTextCtx *args);
    int (*roll_back)(const CliDeployTextCtx *args);
} CliDeployTextNs;
EMBED_TABLE_LAYOUT(CliDeployTextNs, place, gather, roll_back);
EMBED_TABLE_STORAGE CliDeployTextNs cli_deploy_text_internal EMBED_UNUSED = {
    .place = cli_deploy_position,
    .gather = cli_deploy_gather,
    .roll_back = cli_deploy_roll_back,
};

typedef struct
{
    void (*position_name)(const CliDeploySizeCtx *args);
    const char *(*position_folder)(const CliDeploySizeCtx *args);
    int (*remote_run)(const CliDeploySizeCtx *args);
} CliDeploySizeNs;
EMBED_TABLE_LAYOUT(CliDeploySizeNs, position_name, position_folder, remote_run);
EMBED_TABLE_STORAGE CliDeploySizeNs cli_deploy_size_internal EMBED_UNUSED = {
    .position_name = cli_deploy_position_name,
    .position_folder = cli_deploy_position_folder,
    .remote_run = cli_deploy_remote_run,
};

typedef struct
{
    int (*fits)(const CliDeploySizePatternCtx *args);
} CliDeploySizePatternNs;
EMBED_TABLE_LAYOUT(CliDeploySizePatternNs, fits);
EMBED_TABLE_STORAGE CliDeploySizePatternNs cli_deploy_size_pattern_internal EMBED_UNUSED = {
    .fits = cli_deploy_fits,
};

typedef struct
{
    const char *(*left_out_by)(const CliDeployPatternsCtx *args);
} CliDeployPatternsNs;
EMBED_TABLE_LAYOUT(CliDeployPatternsNs, left_out_by);
EMBED_TABLE_STORAGE CliDeployPatternsNs cli_deploy_patterns_internal EMBED_UNUSED = {
    .left_out_by = cli_deploy_left_out_by,
};

typedef struct
{
    void (*release_name)(const CliDeployOutSecondsCtx *args);
} CliDeployOutSecondsNs;
EMBED_TABLE_LAYOUT(CliDeployOutSecondsNs, release_name);
EMBED_TABLE_STORAGE CliDeployOutSecondsNs cli_deploy_out_seconds_internal EMBED_UNUSED = {
    .release_name = cli_deploy_release_name,
};

typedef struct
{
    unsigned int (*mode)(const CliDeployInTreeCtx *args);
} CliDeployInTreeNs;
EMBED_TABLE_LAYOUT(CliDeployInTreeNs, mode);
EMBED_TABLE_STORAGE CliDeployInTreeNs cli_deploy_in_tree_internal EMBED_UNUSED = {
    .mode = cli_deploy_mode,
};

typedef struct
{
    void (*full)(const CliDeployOutIndexCtx *args);
} CliDeployOutIndexNs;
EMBED_TABLE_LAYOUT(CliDeployOutIndexNs, full);
EMBED_TABLE_STORAGE CliDeployOutIndexNs cli_deploy_out_index_internal EMBED_UNUSED = {
    .full = cli_deploy_full,
};

typedef struct
{
    int (*tar)(const CliDeployErrMsgPathCtx *args);
} CliDeployErrMsgPathNs;
EMBED_TABLE_LAYOUT(CliDeployErrMsgPathNs, tar);
EMBED_TABLE_STORAGE CliDeployErrMsgPathNs cli_deploy_err_msg_path_internal EMBED_UNUSED = {
    .tar = cli_deploy_tar,
};

typedef struct
{
    const char *(*find)(const CliDeployCountCtx *args);
    const char *(*ours)(const CliDeployCountCtx *args);
} CliDeployCountNs;
EMBED_TABLE_LAYOUT(CliDeployCountNs, find, ours);
EMBED_TABLE_STORAGE CliDeployCountNs cli_deploy_count_internal EMBED_UNUSED = {
    .find = cli_deploy_find,
    .ours = cli_deploy_ours,
};

typedef struct
{
    int (*record_path)(const CliDeployIndexPathCtx *args);
} CliDeployIndexPathNs;
EMBED_TABLE_LAYOUT(CliDeployIndexPathNs, record_path);
EMBED_TABLE_STORAGE CliDeployIndexPathNs cli_deploy_index_path_internal EMBED_UNUSED = {
    .record_path = cli_deploy_record_path,
};

typedef struct
{
    void (*last_read)(const CliDeployErrMsgIndexCtx *args);
    int (*hashes)(const CliDeployErrMsgIndexCtx *args);
    int (*existing)(const CliDeployErrMsgIndexCtx *args);
    void (*record)(const CliDeployErrMsgIndexCtx *args);
    int (*remove)(const CliDeployErrMsgIndexCtx *args);
} CliDeployErrMsgIndexNs;
EMBED_TABLE_LAYOUT(CliDeployErrMsgIndexNs, last_read, hashes, existing, record, remove);
EMBED_TABLE_STORAGE CliDeployErrMsgIndexNs cli_deploy_err_msg_index_internal EMBED_UNUSED = {
    .last_read = cli_deploy_last_read,
    .hashes = cli_deploy_hashes,
    .existing = cli_deploy_existing,
    .record = cli_deploy_record,
    .remove = cli_deploy_remove,
};

typedef struct
{
    int (*send_to)(const CliDeployErrMsgCtx *args);
} CliDeployErrMsgNs;
EMBED_TABLE_LAYOUT(CliDeployErrMsgNs, send_to);
EMBED_TABLE_STORAGE CliDeployErrMsgNs cli_deploy_err_msg_internal EMBED_UNUSED = {
    .send_to = cli_deploy_send_to,
};

typedef struct
{
    void (*switch_line)(const CliDeployOutCtx *args);
} CliDeployOutNs;
EMBED_TABLE_LAYOUT(CliDeployOutNs, switch_line);
EMBED_TABLE_STORAGE CliDeployOutNs cli_deploy_out_internal EMBED_UNUSED = {
    .switch_line = cli_deploy_switch_line,
};

typedef struct
{
    void (*prune)(const CliDeployIndexCtx *args);
} CliDeployIndexNs;
EMBED_TABLE_LAYOUT(CliDeployIndexNs, prune);
EMBED_TABLE_STORAGE CliDeployIndexNs cli_deploy_index_internal EMBED_UNUSED = {
    .prune = cli_deploy_prune,
};

#endif
