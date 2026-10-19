// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_internal.h: what the deployment's sources share: the deployments deploy.json names, the places one sends to,
// the files it sends and leaves out, and what each place holds against what goes
#ifndef DEPLOY_INTERNAL_H
#define DEPLOY_INTERNAL_H

#include "../comms/link.h"
#include "../text/json_tree.h"
#include "deploy.h"

// the bytes of each page of a pool, a text longer taking a page of its own, and of each piece a text is written in
#define CLI_DEPLOY_PAGE (64u << 10u)
#define CLI_DEPLOY_PIECE 4096u

// A deployment: its name, the folder whose files go, the patterns of the files that stay, the places, how many
// releases each place keeps, and whether its files go into each place's folder itself
typedef struct
{
    const char *name;
    const char *from;
    const char **exclude;
    unsigned int excludes;
    const char **to;
    unsigned int tos;
    unsigned long long keep;
    int in_place;
} CliDeployment;

// A place a deployment sends to: a machine orior reaches, or a folder on this one, by its path
typedef struct
{
    int here;
    CliLinkAddress address;
    char path[CLI_PATH_LONGEST];
} CliDeployPlace;

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

// A pair held in a pool before the pairs held already
typedef struct CliDeployHeldStruct
{
    struct CliDeployHeldStruct *before;
    CliDeployPair pair;
} CliDeployHeld;

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

// A run of a deployment: the deployment; its places; the patterns every file is held to; the files it sends, by their
// paths, and those it leaves out; for each place, the files changed there since they were last sent with their hashes
// there, and the files there the tree no longer holds; each list in the run's pool at its count; and the pool, given
// back as the next run begins and as this one ends
typedef struct
{
    CliDeployment deployment;
    CliDeployPlace *place;
    unsigned int places;
    const char **pattern;
    unsigned int patterns;
    CliDeploySent *sent;
    unsigned int sents;
    CliDeployPair *out;
    unsigned int outs;
    CliDeployPair **changed_list;
    unsigned int *changed;
    CliDeployPair **gone_list;
    unsigned int *gone;
    CliDeployPool pool;
    const char *root;
    CliDeploySay say;
    void *context;
} CliDeployRun;

// A tree deploy.json is read into, taken to the file's size, and the pool its deployments are kept in
typedef struct
{
    CliJson json;
    CliDeployPool pool;
} CliDeployTree;

// the one run, the lock it is held under, and a line said through its teller (deploy_run.c)
extern CliDeployRun g_cli_deploy;
extern CliLock g_cli_deploy_lock;
void cli_deploy_say(const CliLine *line);
// the lines of `new_text` that differ from `old`, each said through the run's teller marked `-` for a line taken out
// and
// `+` for a line put in, around the run of lines the two do not share at their heads and tails (deploy_standing.c)
void cli_deploy_difference(const char *old, unsigned long long old_size, const char *new_text,
                           unsigned long long new_size);

// `size` bytes placed in `pool` at a multiple of 8: where they stand, NULL with the pool marked refused where a page
// could not be taken; `size` bytes of `text` kept there with a 0 after, an empty text where they could not be; and
// every page of a pool given back, the pool left empty (deploy_read.c)
void *cli_deploy_placed(CliDeployPool *pool, unsigned long long size);
const char *cli_deploy_kept(CliDeployPool *pool, const char *text, unsigned long long size);
void cli_deploy_pool_given(CliDeployPool *pool);
// two texts in the order of their bytes: below 0 where `left` comes first, 0 where they are the same, above 0 where
// `right` does (deploy_read.c)
int cli_deploy_order(const char *left, const char *right);
// the deployments deploy.json at the tree's top folder names read into `tree`, `*count` of them from `*deployments`:
// 1, or 0 with why said, none where it is not there; the tree given back; a place read from `text`, a folder on this
// machine where it is a path, against the tree at `root`: 1, or 0 with why said; the place's name; the place's folder;
// and a shell line run at a place, `size` bytes of `input` given it, what it wrote into `out`, a machine that asks for
// a password told how a key reaches it (deploy_read.c)
int cli_deploy_read(const char *root, CliDeployTree *tree, const CliDeployment **deployments, unsigned int *count,
                    CliLine *said);
void cli_deploy_tree_given(CliDeployTree *tree);
int cli_deploy_place(const char *text, const char *root, CliDeployPlace *place, CliLine *said);
void cli_deploy_place_name(const CliDeployPlace *place, CliLine *out);
const char *cli_deploy_place_folder(const CliDeployPlace *place);
int cli_deploy_ask(const CliDeployPlace *place, const char *line, const char *input, unsigned long long size,
                   CliLine *out, CliLine *said);

// whether `name` fits the glob `pattern`, `*` taking any run of letters and `?` any one; the pattern of `patterns`,
// `count` of them, that leaves out the file at `path` under the folder sent, NULL where none does; and the name of the
// release made `seconds` after 1970 began, in UTC, which sorts as the releases were made (deploy_gather.c)
int cli_deploy_fits(const char *pattern, const char *name, unsigned long long size);
const char *cli_deploy_left_out_by(const char *const *patterns, unsigned int count, const char *path);
void cli_deploy_release_name(unsigned long long seconds, CliLine *out);
// the modes git's index gives the run's tree's files read into `pool`; and the mode of the file at `in_tree` in the
// tree, at `full` here, while that pool holds them: `0755` where it runs and `0644` where not, as the index gives it,
// else as it is here (deploy_gather.c)
void cli_deploy_modes_read(CliDeployPool *pool);
unsigned int cli_deploy_mode(const char *in_tree, const char *full);
// the files the run's deployment sends and those it leaves out, each hashed, sorted by path: 1, or 0 with why said;
// and the full path of the file the run sends at `index` into `out` (deploy_walk.c)
int cli_deploy_gather(CliLine *said);
void cli_deploy_full(unsigned int index, CliLine *out);
// the tar stream of the files the run sends but the paths of `kept`, `kept_count` of them, each with its mode and the
// time `time`, a path longer than a ustar header holds in a PAX header before it, written to the file at `path`: 1, or
// 0 with why said (deploy_tar.c)
int cli_deploy_tar(const char *path, const CliDeployPair *kept, unsigned int kept_count, unsigned long long time,
                   CliLine *said);

// the files a place holds and their hashes, and those last sent there and theirs, each by path and sorted, their
// counts beside them; and each given back (deploy_there.c)
extern CliDeployPair *g_cli_deploy_there;
extern unsigned int g_cli_deploy_there_count;
extern CliDeployPair *g_cli_deploy_last;
extern unsigned int g_cli_deploy_last_count;
void cli_deploy_there_given(void);

// the text beside `path` among `count` pairs sorted by path, NULL where it is not among them; the hash the run sends
// `path` with, NULL where it does not send it; the file of the record of the hashes last sent by the run's deployment
// to the place at `index` into `path`, which holds CLI_PATH_LONGEST: 1, or 0 where orior's own folder is not known;
// that record read into g_cli_deploy_last, none where it is not there or reads as no object of texts; and the hashes of
// the files under `folder` of the place at `index` read into g_cli_deploy_there: 1, or 0 with why said
// (deploy_there.c)
const char *cli_deploy_find(const CliDeployPair *pairs, unsigned int count, const char *path);
const char *cli_deploy_ours(const char *path);
int cli_deploy_record_path(unsigned int index, char *path);
void cli_deploy_last_read(unsigned int index);
int cli_deploy_hashes(unsigned int index, const char *folder, CliLine *said);

// what the place at `index` holds under `folder` against what was last sent there and what goes now, kept in the run,
// and said as it is found: 1, or 0 with why said; and the record of the hashes last sent to a place written after a
// send, the files it kept there kept as they are there (deploy_standing.c)
int cli_deploy_standing(unsigned int index, const char *folder, int replace, int remove, CliLine *said);
void cli_deploy_recorded(unsigned int index, int replace);

// the files sent to the place at `index`, into a new release `release` where `releases`, else into its folder itself,
// those kept there and those gone kept carried over from the release it runs: 1, or 0 with why said (deploy_send.c)
int cli_deploy_send_to(unsigned int index, int releases, const char *release, unsigned long long time, int replace,
                       int remove, CliLine *said);
// the shell line that turns a machine's `current` in `folder` to `release`; a place's releases past the newest `keep`
// taken away; and each place's `current` turned back to the release before it: 1, or 0 with why said (deploy_send.c)
void cli_deploy_switch_line(const char *folder, const char *release, CliLine *out);
void cli_deploy_prune(unsigned int index, unsigned long long keep);
int cli_deploy_roll_back(CliLine *said);
// the files at the place at `index` that the tree no longer holds deleted, in its folder itself, each said: 1, or 0
// with why said (deploy_send.c)
int cli_deploy_removed(unsigned int index, CliLine *said);

#endif
