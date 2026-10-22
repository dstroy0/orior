// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_there.c: the hashes of the files a place holds, and of those last sent there as the record kept in orior's
// own folder has them
#include "../command_line/cells.h"
#include "../text/digest.h"
#include "../text/pieces.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "deploy_internal.h"
#include "memoria_operor/memoria_operor.h"

#include <stdlib.h>

// the files a place holds and their hashes, by path, sorted, and those last sent there and theirs, each list in the
// place's pool at its count; the pool, which holds the texts the walk of a folder here finds and each pair as it is
// found, the newest first; the listing a machine gave of them, in a cell of its size; and the record of the hashes last
// sent there, read into a tree of its size. Each is given back as the next place is read and as the run ends
CliDeployPair *g_cli_deploy_there;
unsigned int g_cli_deploy_there_count;
CliDeployPair *g_cli_deploy_last;
unsigned int g_cli_deploy_last_count;
static CliDeployPool s_pool;
static CliDeployHeld *s_held;
static char *s_listing;
static CliJson s_record;
static const char *s_walk_top;
static int s_walk_full;

void cli_deploy_there_given(void)
{
    g_cli_deploy_there = NULL;
    g_cli_deploy_there_count = 0u;
    g_cli_deploy_last = NULL;
    g_cli_deploy_last_count = 0u;
    s_held = NULL;
    cli_deploy_pool_given(&s_pool);
    cli_cell_given(s_listing);
    s_listing = NULL;
    cli_json_given(&s_record);
}

// a file's path and hash held before the pairs held already
static void there_held(const char *path, const char *hash)
{
    CliDeployHeld *const held = cli_deploy_placed(&s_pool, sizeof(*held));
    if (held != NULL)
    {
        held->pair = (CliDeployPair){path, hash};
        held->before = s_held;
        s_held = held;
        g_cli_deploy_there_count += 1u;
    }
}

// two pairs in the order of their paths
static int there_order(const void *one, const void *other)
{
    return cli_deploy_order(((const CliDeployPair *)one)->path, ((const CliDeployPair *)other)->path);
}

// the text beside `path` among `count` sorted pairs, NULL where it is not among them
const char *cli_deploy_find(const CliDeployPair *pairs, unsigned int count, const char *path)
{
    unsigned int low = 0u;
    unsigned int high = count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        const int order = cli_deploy_order(pairs[middle].path, path);
        if (order == 0)
        {
            return pairs[middle].text;
        }
        low = (order < 0) ? (middle + 1u) : low;
        high = (order < 0) ? high : middle;
    }
    return NULL;
}

// the hash the run sends `path` with, NULL where it does not send it
const char *cli_deploy_ours(const char *path)
{
    const CliDeployRun *const run = &g_cli_deploy;
    unsigned int low = 0u;
    unsigned int high = run->sents;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        const int order = cli_deploy_order(run->sent[middle].path, path);
        if (order == 0)
        {
            return run->sent[middle].hash;
        }
        low = (order < 0) ? (middle + 1u) : low;
        high = (order < 0) ? high : middle;
    }
    return NULL;
}

// the file of the record of the hashes last sent by the run's deployment to the place at `index` into `path`, which
// holds CLI_PATH_LONGEST: 1, or 0 where orior's own folder is not known
int cli_deploy_record_path(unsigned int index, char *path)
{
    const CliDeployRun *const run = &g_cli_deploy;
    char home[CLI_PATH_LONGEST];
    char key[(3u * CLI_PATH_LONGEST) + 8u];
    CliLine key_line = {key, sizeof(key), 0ull};
    cli_line_text(&key_line, run->root);
#if defined(_WIN32)
    for (unsigned long long at = 0ull; at < key_line.at; at += 1u)
    {
        key[at] = (key[at] == '/') ? '\\' : key[at];
    }
#endif
    cli_line_character(&key_line, '\n');
    cli_line_text(&key_line, run->deployment.name);
    cli_line_character(&key_line, '\n');
    cli_deploy_place_name(&run->place[index], &key_line);
    if (!cli_home_folder(home) || !cli_line_fits(&key_line))
    {
        return 0;
    }
    char digest[65];
    CliLine digest_line = {digest, sizeof(digest), 0ull};
    cli_sha256_text(key, key_line.at, &digest_line);
    CliLine line = {path, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, home);
    cli_line_text(&line, "/deployed/");
    cli_line_counted(&line, digest, 16u);
    cli_line_text(&line, ".json");
    return cli_line_closed(&line) != 0ull;
}

// the record of the hashes last sent to the place at `index` read into g_cli_deploy_last, placed in the place's pool at
// its count, none where it is not there or reads as no object of texts
void cli_deploy_last_read(unsigned int index)
{
    g_cli_deploy_last = NULL;
    g_cli_deploy_last_count = 0u;
    cli_json_given(&s_record);
    char path[CLI_PATH_LONGEST];
    char why[CLI_SAID_LONGEST];
    CliLine none = {why, sizeof(why), 0ull};
    const unsigned int top =
        cli_deploy_record_path(index, path) ? cli_json_file_taken(&s_record, path, 0u, 0ull, &none) : CLI_JSON_NONE;
    if (!cli_json_is(&s_record, top, CFG_JSON_OBJECT))
    {
        return;
    }
    unsigned int members = 0u;
    for (unsigned int member = s_record.node[top].first; member != CLI_JSON_NONE; member = s_record.node[member].next)
    {
        if (cli_json_string(&s_record, member, NULL) == NULL)
        {
            return;
        }
        members += 1u;
    }
    CliDeployPair *const last =
        (members > 0u) ? cli_deploy_placed(&s_pool, (unsigned long long)members * sizeof(last[0])) : NULL;
    if (last == NULL)
    {
        return;
    }
    unsigned int at = 0u;
    for (unsigned int member = s_record.node[top].first; member != CLI_JSON_NONE; member = s_record.node[member].next)
    {
        last[at] = (CliDeployPair){s_record.text + s_record.node[member].key, cli_json_string(&s_record, member, NULL)};
        at += 1u;
    }
    qsort(last, members, sizeof(last[0]), there_order);
    g_cli_deploy_last = last;
    g_cli_deploy_last_count = members;
}

// one entry of a folder of this machine hashed: a folder read in turn, a file's bytes hashed
static void there_walk(void *context, const char *name, int is_folder)
{
    const char *const folder = (const char *)context;
    char path[CLI_PATH_LONGEST];
    CliLine path_line = {path, sizeof(path), 0ull};
    cli_line_text(&path_line, folder);
    cli_line_text(&path_line, (folder[0] != '\0') ? "/" : "");
    cli_line_text(&path_line, name);
    // the path ended before the whole path is written from it
    const unsigned long long ended = cli_line_closed(&path_line);
    char full[(2u * CLI_PATH_LONGEST) + 2u];
    CliLine full_line = {full, sizeof(full), 0ull};
    cli_line_text(&full_line, s_walk_top);
    cli_line_character(&full_line, '/');
    cli_line_text(&full_line, (ended != 0ull) ? path : "");
    if ((ended == 0ull) || (cli_line_closed(&full_line) == 0ull))
    {
        s_walk_full = 1;
        return;
    }
    if (is_folder)
    {
        (void)query_folder_each(full, there_walk, (void *)cli_deploy_kept(&s_pool, path, path_line.at));
        return;
    }
    unsigned long long size = 0ull;
    const unsigned char *const bytes = cli_file_held(full, &size);
    if (bytes == NULL)
    {
        return;
    }
    char digest[65];
    CliLine digest_line = {digest, sizeof(digest), 0ull};
    cli_sha256_text(bytes, size, &digest_line);
    cli_file_let_go(bytes, size);
    there_held(cli_deploy_kept(&s_pool, path, path_line.at), cli_deploy_kept(&s_pool, digest, 64u));
}

// the hashes of the files under `folder` of the place at `index`, by their paths under it, into g_cli_deploy_there,
// placed in the place's pool at their count and sorted: 1, or 0 with why said
int cli_deploy_hashes(unsigned int index, const char *folder, CliLine *said)
{
    const CliDeployPlace *const place = &g_cli_deploy.place[index];
    cli_deploy_there_given();
    s_walk_full = 0;
    if (place->here)
    {
        s_walk_top = folder;
        (void)query_folder_each(folder, there_walk, (void *)"");
    }
    else
    {
        char line[(2u * CLI_PATH_LONGEST) + 128u];
        CliLine command = {line, sizeof(line), 0ull};
        cli_line_text(&command, "cd ");
        cli_link_quote(folder, cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}), &command);
        cli_line_text(&command, " 2>/dev/null || exit 0; find . -type f -exec sha256sum {} + 2>/dev/null; true");
        cli_line_closed(&command);
        char piece[CLI_DEPLOY_PIECE];
        CliPieces pieces;
        CliLine listing;
        cli_pieces_begun(&pieces, &listing, piece, sizeof(piece));
        const int asked = cli_deploy_ask(place, line, NULL, 0ull, &listing, said);
        unsigned long long size = 0ull;
        s_listing = cli_pieces_joined(&pieces, &listing, "a place's hashes", &size);
        if (!asked)
        {
            return 0;
        }
        s_walk_full = (s_listing == NULL);
        static const char s_split[] = "  ";
        for (char *at = s_listing; (at != NULL) && (*at != '\0');)
        {
            char *const newline = (char *)memor.chr(
                &(MemoriaCfg){.src = at, .bytes = (size_t)(size - (unsigned long long)(at - s_listing)), .val = '\n'});
            char *const end = (newline != NULL) ? newline : (s_listing + size);
            const int more = (newline != NULL);
            *end = '\0';
            char *const split = (char *)cellul.find(&(CatenaFinitaCfg){
                .src = at, .cap = (size_t)(end - at) + 1u, .other = s_split, .other_cap = sizeof(s_split)});
            if (split != NULL)
            {
                *split = '\0';
                const char *path = split + 2;
                while ((path[0] == '.') && (path[1] == '/'))
                {
                    path += 2;
                }
                const char *hash = at;
                while (*hash == '\\')
                {
                    hash += 1;
                }
                there_held(path, hash);
            }
            at = more ? (end + 1) : end;
        }
    }
    CliDeployPair *const there =
        (g_cli_deploy_there_count > 0u)
            ? cli_deploy_placed(&s_pool, (unsigned long long)g_cli_deploy_there_count * sizeof(there[0]))
            : NULL;
    if (s_walk_full || s_pool.refused)
    {
        cli_deploy_there_given();
        cli_line_text(said, "it holds more files than orior_cli holds");
        return 0;
    }
    for (unsigned int at = g_cli_deploy_there_count; s_held != NULL; s_held = s_held->before)
    {
        at -= 1u;
        there[at] = s_held->pair;
    }
    // no pairs is no array to order, and qsort takes no null
    if (there != NULL)
    {
        qsort(there, g_cli_deploy_there_count, sizeof(there[0]), there_order);
    }
    g_cli_deploy_there = there;
    return 1;
}
