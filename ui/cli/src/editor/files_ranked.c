// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_ranked.c: the quick open: the tree's files, less what the search's patterns hide, read behind the window and
// held, and those that answer a query, the best first. A file named exactly as typed scores over any match, one whose
// name starts with what was typed half that, and one opened lately over the match itself
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/regexp.h"
#include "files_internal.h"

// what a file named as typed scores over the match itself, and what a file opened lately scores over it
#define CLI_RANKED_EXACT 100000.0
#define CLI_RANKED_LATELY 6.0
// the milliseconds the files read stay before a search reads them again behind it
#define CLI_RANKED_SEEN_FOR 4000ull

// The tree's files as the last quick open read them, in the current room, the other the reader's: the tree, when they
// were read, whether they are being read again, whether they have been read once, and how many reads have begun. Each
// room is the files picked from a held view of the tree; the room the reader leaves behind is let go as it turns
typedef struct
{
    CliPicked room[2];
    unsigned int current;
    char root[CLI_PATH_LONGEST];
    int kept;
    unsigned long long at;
    int reading;
    int read;
    unsigned long long begun;
    CliThread reader;
} CliHeld;

static CliHeld s_held;
static CliLock s_lock = CLI_LOCK_INIT;

// whether the quick open reads a file: the search's patterns do not hide it
static int cli_ranked_keeps(void *context, const char *path)
{
    return !cli_patterns_hides((const CliPatterns *)context, path, 0);
}

// the tree's files picked into the spare room, less what the search's patterns hide, and the rooms turned where no read
// has begun since, the room left behind let go. Readers take turns, so the spare room is one reader's while it reads
static void cli_reader(void *context)
{
    static CliPatterns s_patterns;
    static char s_root[CLI_PATH_LONGEST];
    static CliLock s_turn = CLI_LOCK_INIT;
    (void)context;
    cli_lock_take(&s_turn);
    cli_lock_take(&s_lock);
    memor.cpy(&(MemoriaCfg){.dst = s_root,
                            .src = s_held.root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_held.root, .cap = CLI_CELLS_POOL}) + 1u});
    const unsigned long long begun = s_held.begun;
    const unsigned int spare = 1u - s_held.current;
    cli_lock_give(&s_lock);
    cli_patterns_of(CLI_PART_SEARCH, &s_patterns);
    CliPicked picked;
    (void)cli_files_picked(&picked, s_root, cli_ranked_keeps, &s_patterns);
    cli_lock_take(&s_lock);
    if (s_held.kept && (cellul.eq(&(CatenaFinitaCfg){.src = s_held.root, .other = s_root, .cap = CLI_CELLS_POOL})) &&
        (s_held.begun == begun))
    {
        cli_picked_let(&s_held.room[spare]);
        s_held.room[spare] = picked;
        cli_picked_let(&s_held.room[s_held.current]);
        s_held.current = spare;
        s_held.at = cli_clock_milliseconds();
        s_held.reading = 0;
        s_held.read = 1;
    }
    else
    {
        cli_picked_let(&picked);
    }
    cli_lock_give(&s_lock);
    cli_lock_give(&s_turn);
}

// the tree's files read again on a thread of their own, the lock held
static void cli_read_into(void)
{
    s_held.reading = 1;
    s_held.begun += 1ull;
    s_held.reader = (CliThread){cli_reader, NULL};
    if (!cli_thread_started(&s_held.reader))
    {
        s_held.reading = 0;
    }
}

// whether the files held are the tree's at `root`, the lock held
static int cli_held_for(const char *root)
{
    return s_held.kept && (cellul.eq(&(CatenaFinitaCfg){.src = s_held.root, .other = root, .cap = CLI_CELLS_POOL}));
}

// the files held for the tree at `root`, none yet, the rooms of another tree let go, and their first read begun, the
// lock held
static void cli_held_begun(const char *root)
{
    memor.cpy(&(MemoriaCfg){.dst = s_held.root,
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    s_held.kept = 1;
    s_held.at = cli_clock_milliseconds();
    cli_picked_let(&s_held.room[0]);
    cli_picked_let(&s_held.room[1]);
    s_held.read = 0;
    s_held.begun = 0ull;
    cli_read_into();
}

void cli_files_read_behind(const char *root)
{
    cli_lock_take(&s_lock);
    if (!cli_held_for(root))
    {
        cli_held_begun(root);
    }
    cli_lock_give(&s_lock);
}

int cli_files_unread(const char *root)
{
    cli_lock_take(&s_lock);
    const int unread = !(cli_held_for(root) && s_held.read);
    cli_lock_give(&s_lock);
    return unread;
}

void cli_files_forget_held(void)
{
    cli_lock_take(&s_lock);
    if (s_held.kept)
    {
        cli_read_into();
    }
    cli_lock_give(&s_lock);
}

// A file that answers: its place among the files ranked and its score
typedef struct
{
    unsigned int index;
    double score;
} CliFound;

// The query as the files are held against it: its letters in lower case and where its last part after a slash starts,
// which a file's name is held against; those less whitespace, as a match looks for them; the same less ASCII's
// whitespace, read where the query in lower case is all ASCII; and the letters of the file being matched, as they are
// and in lower case, and the places that matched
typedef struct
{
    unsigned int lowered[CLI_FUZZY_LETTERS];
    unsigned int lowered_count;
    unsigned int asked;
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
static int ranking_held(CliLine *out)
{
    s_ranking = cli_cell_taken(sizeof(*s_ranking), "a ranking of the tree's files");
    if (s_ranking == NULL)
    {
        cli_line_text(out, "{\"found\":[],\"reading\":true}");
    }
    return s_ranking != NULL;
}

// the ranking given back
static void ranking_released(void)
{
    cli_cell_given(s_ranking);
    s_ranking = NULL;
}

// the query read into the ranking
static void cli_query_read(const char *query)
{
    // a letter count is below CLI_FUZZY_LETTERS, which an unsigned int holds
    s_ranking->lowered_count =
        (unsigned int)cli_regexp_letters(query, cellul.len(&(CatenaFinitaCfg){.src = query, .cap = CLI_CELLS_POOL}),
                                         s_ranking->lowered, CLI_FUZZY_LETTERS);
    s_ranking->asked = 0u;
    s_ranking->wanted_count = 0u;
    s_ranking->wanted_bytes_count = 0u;
    s_ranking->ascii = 1;
    for (unsigned int at = 0u; at < s_ranking->lowered_count; at += 1u)
    {
        const unsigned int letter = cli_regexp_lower(s_ranking->lowered[at]);
        s_ranking->lowered[at] = letter;
        s_ranking->ascii = s_ranking->ascii && (letter < 0x80u);
        s_ranking->asked = (letter == '/') ? (at + 1u) : s_ranking->asked;
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

// the query's letters as typed in the path in lower case, as a match: the last place the path holds them, in its
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

// the file at `path` held against the query, its letters read into the ranking: 1 with its score and places, or 0
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
    const unsigned int asked = s_ranking->lowered_count - s_ranking->asked;
    return (count == asked) && (memor.cmp(&(MemoriaCfg){.src = s_ranking->lower + at,
                                                        .other = s_ranking->lowered + s_ranking->asked,
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
    const unsigned int asked = s_ranking->lowered_count - s_ranking->asked;
    const int starts = (asked <= length) && (memor.cmp(&(MemoriaCfg){.src = s_ranking->lower + name,
                                                                     .other = s_ranking->lowered + s_ranking->asked,
                                                                     .bytes = asked * sizeof(unsigned int)}) == 0);
    return starts ? (CLI_RANKED_EXACT / 2.0) : 0.0;
}

// the found sorted by score, the highest first, those that score the same kept in their order
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

// the `paths` the ranking holds that answer `query`, at most `most` of them, the best first, written into `out`, the
// lock held; `read` says whether the tree's files were read
static void cli_ranked_of(unsigned int paths, const char *query, const char *const *recent, unsigned int recents,
                          unsigned int most, int fuzzy, int read, CliLine *out)
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
    cli_line_text(out, "{\"found\":[");
    for (unsigned int at = 0u; at < shown; at += 1u)
    {
        const CliFound *const one = &s_ranking->found[at];
        const char *const path = s_ranking->path[one->index];
        double score = 0.0;
        cli_matched(path, fuzzy, &score);
        cli_line_text(out, (at != 0u) ? ",{\"path\":" : "{\"path\":");
        cli_json_quoted(out, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"score\":");
        cli_json_double(out, one->score);
        cli_line_text(out, ",\"hits\":[");
        for (unsigned int hit = 0u; hit < s_ranking->hit_count; hit += 1u)
        {
            if (hit != 0u)
            {
                cli_line_character(out, ',');
            }
            cli_line_decimal(out, s_ranking->hits[hit], 0u);
        }
        cli_line_text(out, "]}");
    }
    cli_line_text(out, read ? "],\"reading\":false}" : "],\"reading\":true}");
}

void cli_files_ranked(const char *root, const char *query, const char *const *recent, unsigned int recents,
                      unsigned int most, int fuzzy, CliLine *out)
{
    cli_lock_take(&s_lock);
    if (!ranking_held(out))
    {
        cli_lock_give(&s_lock);
        return;
    }
    int read = 0;
    if (cli_held_for(root))
    {
        const int old = (cli_clock_milliseconds() - s_held.at) >= CLI_RANKED_SEEN_FOR;
        if (s_held.read && old && !s_held.reading)
        {
            cli_read_into();
        }
        read = s_held.read;
    }
    else
    {
        cli_held_begun(root);
    }
    // until the files are read, those opened lately alone answer
    const CliPicked *const files = read ? &s_held.room[s_held.current] : NULL;
    unsigned int paths = 0u;
    for (unsigned int index = 0u; read && (index < files->count); index += 1u)
    {
        s_ranking->path[paths] = cli_picked_at(files, index);
        paths += 1u;
    }
    for (unsigned int index = 0u; !read && (index < recents) && (paths < CLI_FILES_MOST); index += 1u)
    {
        s_ranking->path[paths] = recent[index];
        paths += 1u;
    }
    cli_ranked_of(paths, query, recent, recents, most, fuzzy, read, out);
    ranking_released();
    cli_lock_give(&s_lock);
}

void cli_files_ranked_in(const char *const *files, unsigned int count, const char *query, const char *const *recent,
                         unsigned int recents, unsigned int most, int fuzzy, CliLine *out)
{
    cli_lock_take(&s_lock);
    if (!ranking_held(out))
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
