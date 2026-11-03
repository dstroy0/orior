// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_gather.c: whether a file is left out by a deployment's patterns, the mode git's index gives a file, else the
// one it has here, and a release's name by the time it is made
#include "../command_line/cells.h"
#include "../text/text.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "deploy_internal.h"
#include "memoria_operor/memoria_operor.h"

#include <stdlib.h>

// the modes git's index gives the tree's files, by their paths in the tree, sorted by path, in the pool they were read
// into
static CliDeployPair *s_modes;
static unsigned int s_mode_count;

typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned long long at;
    const char *folder;
    unsigned long long folder_size;
    const char *path;
} DeployGatherCtx;
// the bytes of the letter at `at` of `size` bytes, one at the fewest
EMBED_INLINE unsigned long long gather_letter(const DeployGatherCtx *args)
{
    const char *text = args->text;
    unsigned long long at = args->at;
    unsigned int letter = 0u;
    return EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                      .text_bytes = (const unsigned char *)text, .length = args->size, .at = at, .letter = &letter);
}

int cli_deploy_fits(const CliDeploySizePatternCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->pattern, .cap = CLI_CELLS_POOL});
    unsigned long long p = 0ULL;
    unsigned long long n = 0ULL;
    unsigned long long star = 0ULL;
    unsigned long long mark = 0ULL;
    int starred = 0;
    while (n < args->size)
    {
        const unsigned long long name_letter =
            EMBED_CALL(gather_letter, DeployGatherCtx, .text = args->name, .size = args->size, .at = n);
        const unsigned long long pattern_letter =
            (p < length) ? EMBED_CALL(gather_letter, DeployGatherCtx, .text = args->pattern, .size = length, .at = p)
                         : 0ULL;
        const int same = (p < length) && (pattern_letter == name_letter) &&
                         (memor.cmp(&(MemoriaCfg){
                              .src = args->pattern + p, .other = args->name + n, .bytes = (size_t)name_letter}) == 0);
        if ((p < length) && ((args->pattern[p] == '?') || same))
        {
            p += (args->pattern[p] == '?') ? 1u : pattern_letter;
            n += name_letter;
        }
        else if ((p < length) && (args->pattern[p] == '*'))
        {
            starred = 1;
            star = p;
            mark = n;
            p += 1u;
        }
        else if (starred)
        {
            p = star + 1u;
            mark += EMBED_CALL(gather_letter, DeployGatherCtx, .text = args->name, .size = args->size, .at = mark);
            n = mark;
        }
        else
        {
            return 0;
        }
    }
    p = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = args->pattern, .at = p,
                   .size = length, .set = "*");
    return p == length;
}

// whether a pattern ending in `/` leaves out a file at `path`: its folder named anywhere among the folders above it,
// or, where the folder has a `/` in it, as a run of the path's parts
EMBED_INLINE int gather_folder_out(const DeployGatherCtx *args)
{
    char named[CLI_PATH_MAX_LEN];
    if (args->folder_size >= sizeof(named))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = named, .src = args->folder, .bytes = (size_t)args->folder_size});
    named[args->folder_size] = '\0';
    unsigned long long starts[CLI_PATH_MAX_LEN / 2u];
    unsigned int parts = 0u;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    {
        unsigned long long at = 0ULL;
        while ((at <= length) && (parts < CLI_PATH_MAX_LEN / 2u))
        {
            if ((at == 0u) || (args->path[at - 1u] == '/'))
            {
                starts[parts] = at;
                parts += 1u;
            }
            at += 1u;
        }
    }
    if (memor.chr(&(MemoriaCfg){.src = named, .bytes = (size_t)args->folder_size, .val = (uint8_t)'/'}) == NULL)
    {
        for (unsigned int part = 0u; part + 1u < parts; part += 1u)
        {
            if (EMBED_CALL(cli_deploy_size_pattern_internal.fits, CliDeploySizePatternCtx, .pattern = named,
                           .name = args->path + starts[part], .size = starts[part + 1u] - starts[part] - 1u))
            {
                return 1;
            }
        }
        return 0;
    }
    if (cellul.starts(&(CatenaFinitaCfg){.src = args->path, .other = named, .cap = (size_t)args->folder_size + 1u}) &&
        (args->path[args->folder_size] == '/'))
    {
        return 1;
    }
    unsigned int run = 1u;
    for (unsigned long long at = 0ULL; at < args->folder_size; at += 1u)
    {
        run += (named[at] == '/') ? 1u : 0u;
    }
    for (unsigned int part = 0u; part + run <= parts; part += 1u)
    {
        const unsigned long long end = (part + run < parts) ? (starts[part + run] - 1u) : length;
        if (EMBED_CALL(cli_deploy_size_pattern_internal.fits, CliDeploySizePatternCtx, .pattern = named,
                       .name = args->path + starts[part], .size = end - starts[part]))
        {
            return 1;
        }
    }
    return 0;
}

const char *cli_deploy_left_out_by(const CliDeployPatternsCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    for (unsigned int index = 0u; index < args->count; index += 1u)
    {
        const char *const pattern = args->patterns[index];
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = pattern, .cap = CLI_CELLS_POOL});
        if ((size > 0u) && (pattern[size - 1u] == '/'))
        {
            if (EMBED_CALL(gather_folder_out, DeployGatherCtx, .folder = pattern, .folder_size = size - 1u,
                           .path = args->path))
            {
                return pattern;
            }
            continue;
        }
        if (memor.chr(&(MemoriaCfg){.src = pattern, .bytes = (size_t)size, .val = (uint8_t)'/'}) != NULL)
        {
            const char *trimmed = pattern;
            trimmed +=
                EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = trimmed, .at = 0ULL,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = trimmed, .cap = CLI_CELLS_POOL}), .set = "/");
            if (EMBED_CALL(cli_deploy_size_pattern_internal.fits, CliDeploySizePatternCtx, .pattern = trimmed,
                           .name = args->path, .size = length))
            {
                return pattern;
            }
            continue;
        }
        for (unsigned long long start = 0ULL; start <= length;)
        {
            const char *const slash = memor.chr(
                &(MemoriaCfg){.src = args->path + start, .bytes = (size_t)(length - start), .val = (uint8_t)'/'});
            const unsigned long long end = (slash != NULL) ? (unsigned long long)(slash - args->path) : length;
            if (EMBED_CALL(cli_deploy_size_pattern_internal.fits, CliDeploySizePatternCtx, .pattern = pattern,
                           .name = args->path + start, .size = end - start))
            {
                return pattern;
            }
            start = end + 1u;
        }
    }
    return NULL;
}

void cli_deploy_release_name(const CliDeployOutSecondsCtx *args)
{
    const unsigned long long shifted = (args->seconds / 86400u) + 719468u;
    const unsigned long long era = shifted / 146097u;
    const unsigned long long of_era = shifted - (era * 146097u);
    const unsigned long long year_of_era = (of_era - (of_era / 1460u) + (of_era / 36524u) - (of_era / 146096u)) / 365u;
    const unsigned long long day_of_year = of_era - ((365u * year_of_era) + (year_of_era / 4u) - (year_of_era / 100u));
    const unsigned long long month_index = ((5u * day_of_year) + 2u) / 153u;
    const unsigned long long day = day_of_year - (((153u * month_index) + 2u) / 5u) + 1u;
    const unsigned long long month = (month_index < 10u) ? (month_index + 3u) : (month_index - 9u);
    const unsigned long long year = year_of_era + (era * 400u) + ((month <= 2u) ? 1u : 0u);
    const unsigned long long rest = args->seconds % 86400u;
    mmgr_line_decimal(args->out, year, 4u);
    mmgr_line_decimal(args->out, month, 2u);
    mmgr_line_decimal(args->out, day, 2u);
    mmgr_line_ch(args->out, '-');
    mmgr_line_decimal(args->out, rest / 3600u, 2u);
    mmgr_line_decimal(args->out, (rest % 3600u) / 60u, 2u);
    mmgr_line_decimal(args->out, rest % 60u, 2u);
}

// two pairs in the order of their paths, then of their texts
static int gather_order(const void *one, const void *other)
{
    const CliDeployPair *const left = (const CliDeployPair *)one;
    const CliDeployPair *const right = (const CliDeployPair *)other;
    const int paths =
        EMBED_CALL(cli_deploy_left_internal.order, CliDeployLeftCtx, .left = left->path, .right = right->path);
    return (paths != 0)
               ? paths
               : EMBED_CALL(cli_deploy_left_internal.order, CliDeployLeftCtx, .left = left->text, .right = right->text);
}

// the modes git's index gives the tree's files read into s_modes, placed in `pool` at the count of its lines, each
// `0755` where it runs and `0644` where not
void cli_deploy_modes_read(const CliDeployPoolCtx *args)
{
    s_modes = NULL;
    s_mode_count = 0u;
    const char *const words[4] = {"git", "ls-files", "-s", NULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea none = {why, sizeof(why), 0ULL};
    CliRunResult *const run_result = cli_run_result_alloc();
    unsigned int lines = 0u;
    if (EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = g_cli_deploy.root, .settings = NULL,
                   .count = 0u, .run_result = run_result, .err_msg = &none) &&
        (run_result->out_size > 0u))
    {
        lines = 1u;
        for (unsigned long long at = 0ULL; at < run_result->out_size; at += 1u)
        {
            lines += (run_result->out[at] == '\n') ? 1u : 0u;
        }
        s_modes = EMBED_CALL(cli_deploy_pool_internal.place, CliDeployPoolCtx, .pool = args->pool,
                             .size = (unsigned long long)lines * sizeof(s_modes[0]));
    }
    if (s_modes != NULL)
    {
        for (unsigned long long at = 0ULL; (at < run_result->out_size) && (s_mode_count < lines);)
        {
            const char *const line = run_result->out + at;
            const char *const newline = memor.chr(
                &(MemoriaCfg){.src = line, .bytes = (size_t)(run_result->out_size - at), .val = (uint8_t)'\n'});
            const unsigned long long size =
                (newline != NULL) ? (unsigned long long)(newline - line) : (run_result->out_size - at);
            at += size + 1u;
            const char *const tab = memor.chr(&(MemoriaCfg){.src = line, .bytes = (size_t)size, .val = (uint8_t)'\t'});
            const char *const space = memor.chr(&(MemoriaCfg){.src = line, .bytes = (size_t)size, .val = (uint8_t)' '});
            unsigned long long mode = 0ULL;
            int read = (tab != NULL) && (space != NULL) && (space < tab) && (space > line);
            const char *digit = line;
            while (read && (digit < space))
            {
                read = (*digit >= '0') && (*digit <= '7') && (mode < 0x20000000ULL);
                mode = (mode * 8u) + (unsigned long long)(*digit - '0');
                digit += 1;
            }
            if (read)
            {
                const unsigned long long path = (unsigned long long)(tab + 1 - line);
                s_modes[s_mode_count].path = EMBED_CALL(cli_deploy_pool_internal.text_store, CliDeployPoolCtx,
                                                        .pool = args->pool, .text = tab + 1, .size = size - path);
                s_modes[s_mode_count].text = ((mode & 0111u) != 0u) ? "0755" : "0644";
                s_mode_count += 1u;
            }
        }
    }
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    qsort(s_modes, s_mode_count, sizeof(s_modes[0]), gather_order);
}

// the mode of the file at `in_tree` in the tree, at `full` here
unsigned int cli_deploy_mode(const CliDeployInTreeCtx *args)
{
    const CliDeployPair key = {args->in_tree, ""};
    unsigned int low = 0u;
    unsigned int high = s_mode_count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        const int order = EMBED_CALL(cli_deploy_left_internal.order, CliDeployLeftCtx, .left = s_modes[middle].path,
                                     .right = key.path);
        if (order == 0)
        {
            return (s_modes[middle].text[2] == '7') ? 0755u : 0644u;
        }
        low = (order < 0) ? (middle + 1u) : low;
        high = (order < 0) ? high : middle;
    }
    return EMBED_CALL(cli_path_file_internal.file_runs, CliPathFileCtx, .path = args->full) ? 0755u : 0644u;
}
