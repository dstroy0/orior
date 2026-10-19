// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// deploy_gather.c: whether a file is left out by a deployment's patterns, the mode git's index gives a file, else the
// one it has here, and a release's name by the time it is made
#include "../command_line/cells.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "deploy_internal.h"
#include "memoria_operor/memoria_operor.h"

#include <stdlib.h>

// the modes git's index gives the tree's files, by their paths in the tree, sorted by path, in the pool they were read
// into
static CliDeployPair *s_modes;
static unsigned int s_mode_count;

// the bytes of the letter at `at` of `size` bytes, one at the fewest
static unsigned long long gather_letter(const char *text, unsigned long long size, unsigned long long at)
{
    unsigned int letter = 0u;
    return cli_lossy_letter((const unsigned char *)text, size, at, &letter);
}

int cli_deploy_fits(const char *pattern, const char *name, unsigned long long size)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = pattern, .cap = CLI_CELLS_POOL});
    unsigned long long p = 0ull;
    unsigned long long n = 0ull;
    unsigned long long star = 0ull;
    unsigned long long mark = 0ull;
    int starred = 0;
    while (n < size)
    {
        const unsigned long long name_letter = gather_letter(name, size, n);
        const unsigned long long pattern_letter = (p < length) ? gather_letter(pattern, length, p) : 0ull;
        const int same =
            (p < length) && (pattern_letter == name_letter) &&
            (memor.cmp(&(MemoriaCfg){.src = pattern + p, .other = name + n, .bytes = (size_t)name_letter}) == 0);
        if ((p < length) && ((pattern[p] == '?') || same))
        {
            p += (pattern[p] == '?') ? 1u : pattern_letter;
            n += name_letter;
        }
        else if ((p < length) && (pattern[p] == '*'))
        {
            starred = 1;
            star = p;
            mark = n;
            p += 1u;
        }
        else if (starred)
        {
            p = star + 1u;
            mark += gather_letter(name, size, mark);
            n = mark;
        }
        else
        {
            return 0;
        }
    }
    while ((p < length) && (pattern[p] == '*'))
    {
        p += 1u;
    }
    return p == length;
}

// whether a pattern ending in `/` leaves out a file at `path`: its folder named anywhere among the folders above it,
// or, where the folder has a `/` in it, as a run of the path's parts
static int gather_folder_out(const char *folder, unsigned long long folder_size, const char *path)
{
    char named[CLI_PATH_LONGEST];
    if (folder_size >= sizeof(named))
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = named, .src = folder, .bytes = (size_t)folder_size});
    named[folder_size] = '\0';
    unsigned long long starts[CLI_PATH_LONGEST / 2u];
    unsigned int parts = 0u;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ull; (at <= length) && (parts < CLI_PATH_LONGEST / 2u); at += 1u)
    {
        if ((at == 0u) || (path[at - 1u] == '/'))
        {
            starts[parts] = at;
            parts += 1u;
        }
    }
    if (memor.chr(&(MemoriaCfg){.src = named, .bytes = (size_t)folder_size, .val = (uint8_t)'/'}) == NULL)
    {
        for (unsigned int part = 0u; part + 1u < parts; part += 1u)
        {
            if (cli_deploy_fits(named, path + starts[part], starts[part + 1u] - starts[part] - 1u))
            {
                return 1;
            }
        }
        return 0;
    }
    if (cellul.starts(&(CatenaFinitaCfg){.src = path, .other = named, .cap = (size_t)folder_size + 1u}) &&
        (path[folder_size] == '/'))
    {
        return 1;
    }
    unsigned int run = 1u;
    for (unsigned long long at = 0ull; at < folder_size; at += 1u)
    {
        run += (named[at] == '/') ? 1u : 0u;
    }
    for (unsigned int part = 0u; part + run <= parts; part += 1u)
    {
        const unsigned long long end = (part + run < parts) ? (starts[part + run] - 1u) : length;
        if (cli_deploy_fits(named, path + starts[part], end - starts[part]))
        {
            return 1;
        }
    }
    return 0;
}

const char *cli_deploy_left_out_by(const char *const *patterns, unsigned int count, const char *path)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    for (unsigned int index = 0u; index < count; index += 1u)
    {
        const char *const pattern = patterns[index];
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = pattern, .cap = CLI_CELLS_POOL});
        if ((size > 0u) && (pattern[size - 1u] == '/'))
        {
            if (gather_folder_out(pattern, size - 1u, path))
            {
                return pattern;
            }
            continue;
        }
        if (memor.chr(&(MemoriaCfg){.src = pattern, .bytes = (size_t)size, .val = (uint8_t)'/'}) != NULL)
        {
            const char *trimmed = pattern;
            while (*trimmed == '/')
            {
                trimmed += 1;
            }
            if (cli_deploy_fits(trimmed, path, length))
            {
                return pattern;
            }
            continue;
        }
        for (unsigned long long start = 0ull; start <= length;)
        {
            const char *const slash =
                memor.chr(&(MemoriaCfg){.src = path + start, .bytes = (size_t)(length - start), .val = (uint8_t)'/'});
            const unsigned long long end = (slash != NULL) ? (unsigned long long)(slash - path) : length;
            if (cli_deploy_fits(pattern, path + start, end - start))
            {
                return pattern;
            }
            start = end + 1u;
        }
    }
    return NULL;
}

void cli_deploy_release_name(unsigned long long seconds, CliLine *out)
{
    const unsigned long long shifted = (seconds / 86400u) + 719468u;
    const unsigned long long era = shifted / 146097u;
    const unsigned long long of_era = shifted - (era * 146097u);
    const unsigned long long year_of_era = (of_era - (of_era / 1460u) + (of_era / 36524u) - (of_era / 146096u)) / 365u;
    const unsigned long long day_of_year = of_era - ((365u * year_of_era) + (year_of_era / 4u) - (year_of_era / 100u));
    const unsigned long long month_index = ((5u * day_of_year) + 2u) / 153u;
    const unsigned long long day = day_of_year - (((153u * month_index) + 2u) / 5u) + 1u;
    const unsigned long long month = (month_index < 10u) ? (month_index + 3u) : (month_index - 9u);
    const unsigned long long year = year_of_era + (era * 400u) + ((month <= 2u) ? 1u : 0u);
    const unsigned long long rest = seconds % 86400u;
    cli_line_decimal(out, year, 4u);
    cli_line_decimal(out, month, 2u);
    cli_line_decimal(out, day, 2u);
    cli_line_character(out, '-');
    cli_line_decimal(out, rest / 3600u, 2u);
    cli_line_decimal(out, (rest % 3600u) / 60u, 2u);
    cli_line_decimal(out, rest % 60u, 2u);
}

// two pairs in the order of their paths, then of their texts
static int gather_order(const void *one, const void *other)
{
    const CliDeployPair *const left = (const CliDeployPair *)one;
    const CliDeployPair *const right = (const CliDeployPair *)other;
    const int paths = cli_deploy_order(left->path, right->path);
    return (paths != 0) ? paths : cli_deploy_order(left->text, right->text);
}

// the modes git's index gives the tree's files read into s_modes, placed in `pool` at the count of its lines, each
// `0755` where it runs and `0644` where not
void cli_deploy_modes_read(CliDeployPool *pool)
{
    s_modes = NULL;
    s_mode_count = 0u;
    const char *const words[4] = {"git", "ls-files", "-s", NULL};
    char why[CLI_SAID_LONGEST];
    CliLine none = {why, sizeof(why), 0ull};
    CliRan *const ran = cli_ran_taken();
    unsigned int lines = 0u;
    if (cli_ran(words, g_cli_deploy.root, NULL, 0u, ran, &none) && (ran->out_size > 0u))
    {
        lines = 1u;
        for (unsigned long long at = 0ull; at < ran->out_size; at += 1u)
        {
            lines += (ran->out[at] == '\n') ? 1u : 0u;
        }
        s_modes = cli_deploy_placed(pool, (unsigned long long)lines * sizeof(s_modes[0]));
    }
    if (s_modes != NULL)
    {
        for (unsigned long long at = 0ull; (at < ran->out_size) && (s_mode_count < lines);)
        {
            const char *const line = ran->out + at;
            const char *const newline =
                memor.chr(&(MemoriaCfg){.src = line, .bytes = (size_t)(ran->out_size - at), .val = (uint8_t)'\n'});
            const unsigned long long size =
                (newline != NULL) ? (unsigned long long)(newline - line) : (ran->out_size - at);
            at += size + 1u;
            const char *const tab = memor.chr(&(MemoriaCfg){.src = line, .bytes = (size_t)size, .val = (uint8_t)'\t'});
            const char *const space = memor.chr(&(MemoriaCfg){.src = line, .bytes = (size_t)size, .val = (uint8_t)' '});
            unsigned long long mode = 0ull;
            int read = (tab != NULL) && (space != NULL) && (space < tab) && (space > line);
            for (const char *digit = line; read && (digit < space); digit += 1)
            {
                read = (*digit >= '0') && (*digit <= '7') && (mode < 0x20000000ull);
                mode = (mode * 8u) + (unsigned long long)(*digit - '0');
            }
            if (read)
            {
                const unsigned long long path = (unsigned long long)(tab + 1 - line);
                s_modes[s_mode_count].path = cli_deploy_kept(pool, tab + 1, size - path);
                s_modes[s_mode_count].text = ((mode & 0111u) != 0u) ? "0755" : "0644";
                s_mode_count += 1u;
            }
        }
    }
    cli_ran_given(ran);
    qsort(s_modes, s_mode_count, sizeof(s_modes[0]), gather_order);
}

// the mode of the file at `in_tree` in the tree, at `full` here
unsigned int cli_deploy_mode(const char *in_tree, const char *full)
{
    const CliDeployPair key = {in_tree, ""};
    unsigned int low = 0u;
    unsigned int high = s_mode_count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        const int order = cli_deploy_order(s_modes[middle].path, key.path);
        if (order == 0)
        {
            return (s_modes[middle].text[2] == '7') ? 0755u : 0644u;
        }
        low = (order < 0) ? (middle + 1u) : low;
        high = (order < 0) ? high : middle;
    }
#if defined(_WIN32)
    (void)full;
    return 0644u;
#else
    return cli_file_runnable(full) ? 0755u : 0644u;
#endif
}
