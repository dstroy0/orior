// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_ranked.c: the quick open: the tree's files, less what the search's patterns hide, and those that answer a query, the best first. A file named exactly as typed scores over any match, one whose
// name starts with what was typed half that, and one opened lately over the match itself
#include "../code/symbols.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/regexp.h"
#include "files_internal.h"

// what a file named as typed scores over the match itself, and what a file opened lately scores over it
#define CLI_RANKED_EXACT 100000.0
#define CLI_RANKED_LATELY 6.0

static CliLock s_lock = CLI_LOCK_INIT;

// whether the quick open reads a file: the search's patterns do not hide it
static int cli_ranked_includes(void *context, const char *path)
{
    return !cli_patterns_hides((const CliPatterns *)context, path, 0);
}

void cli_files_read_behind(const char *root)
{
    if (!cli_tree_is_listed(root))
    {
        cli_files_warm(root);
    }
}

int cli_files_unread(const char *root)
{
    return !cli_tree_is_listed(root);
}

// A file that answers: its place among the files ranked and its score
typedef struct
{
    unsigned int index;
    double score;
} CliFound;

// The query as the files are matched against it: its letters in lower case and where its last part after a slash
// starts, which a file's name is matched against; those less whitespace, as a match looks for them; the same less
// ASCII's whitespace, read where the query in lower case is all ASCII; and the letters of the file being matched, as
// they are and in lower case, and the places that matched
typedef struct
{
    unsigned int lowered[CLI_FUZZY_LETTERS];
    unsigned int lowered_count;
    unsigned int query_tail;
    unsigned int wanted[CLI_FUZZY_LETTERS];
    unsigned int wanted_count;
    unsigned int wanted_bytes[CLI_FUZZY_LETTERS];
    unsigned int wanted_bytes_count;
    int ascii;
    unsigned int shown[CLI_FUZZY_LETTERS];
    unsigned int lower[CLI_FUZZY_LETTERS];
    unsigned int count;
    unsigned int from;
    unsigned int hits[CLI_FUZZY_LETTERS];
    unsigned int latest[CLI_FUZZY_LETTERS];
    unsigned int hit_count;
    const char *path[CLI_FILES_MOST];
    CliFound found[CLI_FILES_MOST];
    CliFound spare[CLI_FILES_MOST];
} CliRanking;

// the ranking, a cell taken for one ranking under the lock
static CliRanking *s_ranking;

// the ranking taken: 1, or 0 with no file found written, as one still reading, where no room for it is left
static int ranking_reserved(VerbaLinea *out)
{
    s_ranking = g_cli_cells->persistent_buf_alloc(sizeof(*s_ranking), &g_cli_editor_cells);
    if (s_ranking == NULL)
    {
        mmgr_line_put(out, "{\"found\":[],\"reading\":true}");
    }
    return s_ranking != NULL;
}

// the ranking given back
static void ranking_released(void)
{
    g_cli_cells->persistent_buf_release(s_ranking, &g_cli_editor_cells);
    s_ranking = NULL;
}

// the query read into the ranking
static void cli_query_read(const char *query)
{
    // a letter count is below CLI_FUZZY_LETTERS, which an unsigned int stores
    s_ranking->lowered_count =
        (unsigned int)cli_regexp_letters(query, cellul.len(&(CatenaFinitaCfg){.src = query, .cap = CLI_CELLS_POOL}),
                                         s_ranking->lowered, CLI_FUZZY_LETTERS);
    s_ranking->query_tail = 0u;
    s_ranking->wanted_count = 0u;
    s_ranking->wanted_bytes_count = 0u;
    s_ranking->ascii = 1;
    for (unsigned int at = 0u; at < s_ranking->lowered_count; at += 1u)
    {
        const unsigned int letter = cli_regexp_lower(s_ranking->lowered[at]);
        s_ranking->lowered[at] = letter;
        s_ranking->ascii = s_ranking->ascii && (letter < 0x80u);
        s_ranking->query_tail = (letter == '/') ? (at + 1u) : s_ranking->query_tail;
        const int ascii_white =
            (letter == ' ') || (letter == '\t') || (letter == '\n') || (letter == '\f') || (letter == '\r');
        if (!ascii_white)
        {
            s_ranking->wanted_bytes[s_ranking->wanted_bytes_count] = letter;
            s_ranking->wanted_bytes_count += 1u;
        }
        if (!cli_fuzzy_white(letter))
        {
            s_ranking->wanted[s_ranking->wanted_count] = letter;
            s_ranking->wanted_count += 1u;
        }
    }
}

// the query's letters as typed in the path in lower case, as a match: the last place the path contains them, in its
// name where it does, each letter's place in it, and a score that puts a match in the name and at the start of a word
// first
static int cli_typed(double *score)
{
    const unsigned int length = s_ranking->wanted_count;
    *score = 0.0;
    s_ranking->hit_count = 0u;
    if (length == 0u)
    {
        return 1;
    }
    if (length > s_ranking->count)
    {
        return 0;
    }
    unsigned int found = 0u;
    int has = 0;
    for (unsigned int at = (s_ranking->count - length) + 1u; !has && (at > 0u); at -= 1u)
    {
        has = memor.cmp(&(MemoriaCfg){.src = s_ranking->lower + (at - 1u),
                                      .other = s_ranking->wanted,
                                      .bytes = length * sizeof(unsigned int)}) == 0;
        found = at - 1u;
    }
    if (!has)
    {
        return 0;
    }
    for (unsigned int at = 0u; at < length; at += 1u)
    {
        s_ranking->hits[at] = found + at;
    }
    s_ranking->hit_count = length;
    *score = 10.0 + (double)length;
    *score += (found >= s_ranking->from) ? 20.0 : 0.0;
    *score += cli_fuzzy_starts_word(s_ranking->lower, found, 0) ? 10.0 : 0.0;
    *score -= (double)s_ranking->count * 0.02;
    return 1;
}

// the file at `path` matched against the query, its letters read into the ranking: 1 with its score and places, or 0
// where it does not answer
static int cli_matched(const char *path, int fuzzy, double *score)
{
    s_ranking->count = cli_files_letters(path, s_ranking->shown, s_ranking->lower, CLI_FUZZY_LETTERS);
    s_ranking->from = 0u;
    int ascii = 1;
    for (unsigned int at = 0u; at < s_ranking->count; at += 1u)
    {
        s_ranking->from = (s_ranking->shown[at] == '/') ? (at + 1u) : s_ranking->from;
        ascii = ascii && (s_ranking->shown[at] < 0x80u);
    }
    if (!fuzzy)
    {
        return cli_typed(score);
    }
    if (ascii && s_ranking->ascii)
    {
        return cli_fuzzy_letters(s_ranking->wanted_bytes, s_ranking->wanted_bytes_count, s_ranking->shown,
                                 s_ranking->lower, s_ranking->count, s_ranking->from, 1, s_ranking->latest, score,
                                 s_ranking->hits, &s_ranking->hit_count);
    }
    return cli_fuzzy_letters(s_ranking->wanted, s_ranking->wanted_count, s_ranking->shown, s_ranking->lower,
                             s_ranking->count, s_ranking->from, 0, s_ranking->latest, score, s_ranking->hits,
                             &s_ranking->hit_count);
}

// whether `count` letters of the file's name in lower case from `at` are the query's last part
static int cli_named_as(unsigned int at, unsigned int count)
{
    const unsigned int tail_count = s_ranking->lowered_count - s_ranking->query_tail;
    return (count == tail_count) && (memor.cmp(&(MemoriaCfg){.src = s_ranking->lower + at,
                                                             .other = s_ranking->lowered + s_ranking->query_tail,
                                                             .bytes = count * sizeof(unsigned int)}) == 0);
}

// what the file matched last scores for its name: EXACT named as typed, with or without its extension, half that
// where its name starts with what was typed, else 0
static double cli_named(void)
{
    const unsigned int name = s_ranking->from;
    const unsigned int length = s_ranking->count - name;
    unsigned int stem = length;
    for (unsigned int at = 0u; at < length; at += 1u)
    {
        stem = (s_ranking->lower[name + at] == '.') ? at : stem;
    }
    if (cli_named_as(name, length) || cli_named_as(name, stem))
    {
        return CLI_RANKED_EXACT;
    }
    const unsigned int tail_count = s_ranking->lowered_count - s_ranking->query_tail;
    const int starts =
        (tail_count <= length) && (memor.cmp(&(MemoriaCfg){.src = s_ranking->lower + name,
                                                           .other = s_ranking->lowered + s_ranking->query_tail,
                                                           .bytes = tail_count * sizeof(unsigned int)}) == 0);
    return starts ? (CLI_RANKED_EXACT / 2.0) : 0.0;
}

// the found sorted by score, the highest first, those that score the same retained in their order
static void cli_found_sorted(unsigned int count)
{
    CliFound *from = s_ranking->found;
    CliFound *into = s_ranking->spare;
    for (unsigned int width = 1u; width < count; width *= 2u)
    {
        for (unsigned int start = 0u; start < count; start += 2u * width)
        {
            const unsigned int middle = ((start + width) < count) ? (start + width) : count;
            const unsigned int end = ((middle + width) < count) ? (middle + width) : count;
            unsigned int left = start;
            unsigned int right = middle;
            for (unsigned int at = start; at < end; at += 1u)
            {
                const int take_left = (left < middle) && ((right >= end) || (from[left].score >= from[right].score));
                into[at] = take_left ? from[left] : from[right];
                left += take_left ? 1u : 0u;
                right += take_left ? 0u : 1u;
            }
        }
        CliFound *const turned = from;
        from = into;
        into = turned;
    }
    if (from != s_ranking->found)
    {
        memor.cpy(&(MemoriaCfg){.dst = s_ranking->found, .src = from, .bytes = count * sizeof(CliFound)});
    }
}

// the `paths` the ranking contains that answer `query`, at most `most` of them, the best first, written into `out`, the
// lock acquired; `read` reports whether the tree's files were read
static void cli_ranked_of(unsigned int paths, const char *query, const char *const *recent, unsigned int recents,
                          unsigned int most, int fuzzy, int read, VerbaLinea *out)
{
    cli_query_read(query);
    unsigned int found = 0u;
    for (unsigned int index = 0u; index < paths; index += 1u)
    {
        double score = 0.0;
        if (!cli_matched(s_ranking->path[index], fuzzy, &score))
        {
            continue;
        }
        int lately = 0;
        for (unsigned int at = 0u; !lately && (at < recents); at += 1u)
        {
            lately = cellul.eq(
                &(CatenaFinitaCfg){.src = recent[at], .other = s_ranking->path[index], .cap = CLI_CELLS_POOL});
        }
        const double named = cli_named();
        s_ranking->found[found] = (CliFound){index, score + (lately ? CLI_RANKED_LATELY : 0.0) + named};
        found += 1u;
    }
    cli_found_sorted(found);
    const unsigned int shown = (found < most) ? found : most;
    mmgr_line_put(out, "{\"found\":[");
    for (unsigned int at = 0u; at < shown; at += 1u)
    {
        const CliFound *const one = &s_ranking->found[at];
        const char *const path = s_ranking->path[one->index];
        double score = 0.0;
        cli_matched(path, fuzzy, &score);
        mmgr_line_put(out, (at != 0u) ? ",{\"path\":" : "{\"path\":");
        cli_json_quoted(out, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"score\":");
        cli_json_double(out, one->score);
        mmgr_line_put(out, ",\"hits\":[");
        for (unsigned int hit = 0u; hit < s_ranking->hit_count; hit += 1u)
        {
            if (hit != 0u)
            {
                mmgr_line_ch(out, ',');
            }
            mmgr_line_decimal(out, s_ranking->hits[hit], 0u);
        }
        mmgr_line_put(out, "]}");
    }
    mmgr_line_put(out, read ? "],\"reading\":false}" : "],\"reading\":true}");
}

void cli_files_ranked(const char *root, const char *query, const char *const *recent, unsigned int recents,
                      unsigned int most, int fuzzy, VerbaLinea *out)
{
    cli_lock_take(&s_lock);
    if (!ranking_reserved(out))
    {
        cli_lock_give(&s_lock);
        return;
    }
    // until the files are listed, those opened lately alone answer
    const int read = cli_tree_is_listed(root);
    if (!read)
    {
        cli_files_warm(root);
    }
    static CliPatterns s_patterns;
    cli_patterns_of(CLI_PART_SEARCH, &s_patterns);
    CliPicked picked = {NULL, 0u};
    if (read)
    {
        (void)cli_files_picked(&picked, root, cli_ranked_includes, &s_patterns);
    }
    unsigned int paths = 0u;
    for (unsigned int index = 0u; index < picked.count; index += 1u)
    {
        s_ranking->path[paths] = cli_picked_at(&picked, index);
        paths += 1u;
    }
    for (unsigned int index = 0u; !read && (index < recents) && (paths < CLI_FILES_MOST); index += 1u)
    {
        s_ranking->path[paths] = recent[index];
        paths += 1u;
    }
    cli_ranked_of(paths, query, recent, recents, most, fuzzy, read, out);
    cli_picked_let(&picked);
    ranking_released();
    cli_lock_give(&s_lock);
}

void cli_files_ranked_in(const char *const *files, unsigned int count, const char *query, const char *const *recent,
                         unsigned int recents, unsigned int most, int fuzzy, VerbaLinea *out)
{
    cli_lock_take(&s_lock);
    if (!ranking_reserved(out))
    {
        cli_lock_give(&s_lock);
        return;
    }
    const unsigned int paths = (count < CLI_FILES_MOST) ? count : CLI_FILES_MOST;
    for (unsigned int index = 0u; index < paths; index += 1u)
    {
        s_ranking->path[index] = files[index];
    }
    cli_ranked_of(paths, query, recent, recents, most, fuzzy, 1, out);
    ranking_released();
    cli_lock_give(&s_lock);
}
