// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_find.c: a regular expression searched by git over the tree's files, and the listed files whose paths hold each
// word of a query
#include "../code/git_internal.h"
#include "../machine/trust.h"
#include "../text/json_tree.h"
#include "../text/regexp.h"
#include "../text/text.h"
#include "files_internal.h"

// the most paths a find returns, and the most folders a find's walk holds to look in
#define CLI_FOUND_MOST 500u
#define CLI_FIND_PENDING 4096u

void cli_hit_written(CliLine *out, int first, const char *path, unsigned long long line, unsigned long long col,
                     const unsigned char *text, unsigned long long size)
{
    cli_line_text(out, first ? "{\"path\":" : ",{\"path\":");
    cli_json_quoted(out, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"line\":");
    cli_line_decimal(out, line, 0u);
    cli_line_text(out, ",\"col\":");
    cli_line_decimal(out, col, 0u);
    cli_line_text(out, ",\"text\":");
    cli_json_quoted_lossy(out, text, cli_hit_cut(text, size));
    cli_line_character(out, '}');
}

// a whole number as Rust reads one, a plus before it or none, from `length` bytes of `text`: 1 with it into `value`,
// or 0 where it is none
static int cli_whole(const char *text, unsigned long long length, unsigned long long *value)
{
    unsigned long long at = ((length > 0ull) && (text[0] == '+')) ? 1ull : 0ull;
    if (at == length)
    {
        return 0;
    }
    *value = 0ull;
    for (; at < length; at += 1ull)
    {
        const unsigned long long digit = (unsigned long long)(text[at] - '0');
        if ((text[at] < '0') || (text[at] > '9') || (*value > ((~0ull - digit) / 10ull)))
        {
            return 0;
        }
        *value = (*value * 10ull) + digit;
    }
    return 1;
}

// the folder of the tree git's paths start from, as `rev-parse --show-prefix` gives it, into `prefix`: empty where git
// gives none
static void cli_prefix(const char *root, char *prefix)
{
    static const char *const s_words[4] = {"git", "rev-parse", "--show-prefix", NULL};
    prefix[0] = '\0';
    CliRan *const ran = cli_ran_taken();
    char why[CLI_NAME_LONGEST];
    CliLine unstarted = {why, sizeof(why), 0ull};
    if (cli_ran(s_words, root, NULL, 0u, ran, &unstarted))
    {
        const char *from = ran->out;
        const char *to = ran->out + ran->out_size;
        cli_git_trimmed(&from, &to);
        // the trimmed output lies in the output, its end at or past its start
        const unsigned long long length = (unsigned long long)(to - from);
        if (length < CLI_PATH_LONGEST)
        {
            memor.cpy(&(MemoriaCfg){.dst = prefix, .src = from, .bytes = length});
            prefix[length] = '\0';
        }
    }
    cli_ran_given(ran);
}

// the lines git found, each `path:line:column:text`, written as hits, at most CLI_HITS_MOST of them, less those whose
// files the patterns hide
static void cli_git_hits(const CliRan *ran, const char *prefix, const CliPatterns *patterns, const CliPatterns *set,
                         CliLine *out)
{
    const unsigned long long prefix_length = cellul.len(&(CatenaFinitaCfg){.src = prefix, .cap = CLI_CELLS_POOL});
    const char *at = ran->out;
    const char *const end = ran->out + ran->out_size;
    unsigned int found = 0u;
    cli_line_character(out, '[');
    while ((at < end) && (found < CLI_HITS_MOST))
    {
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = at, .val = '\n', .bytes = (unsigned long long)(end - at)});
        const char *const line_end = (newline != NULL) ? newline : end;
        const char *const stop =
            ((newline != NULL) && (line_end > at) && (line_end[-1] == '\r')) ? (line_end - 1) : line_end;
        const char *field = at;
        at = (newline != NULL) ? (newline + 1) : end;
        unsigned long long length = 0ull;
        const char *const path = cli_git_field(&field, stop, ':', &length);
        unsigned long long number_length = 0ull;
        const char *const number = cli_git_field(&field, stop, ':', &number_length);
        unsigned long long col_length = 0ull;
        const char *const col = cli_git_field(&field, stop, ':', &col_length);
        unsigned long long line = 0ull;
        unsigned long long column = 0ull;
        if ((path == NULL) || (number == NULL) || (col == NULL) || !cli_whole(number, number_length, &line) ||
            !cli_whole(col, col_length, &column))
        {
            continue;
        }
        char file[CLI_PATH_LONGEST];
        const int prefixed = (length >= prefix_length) &&
                             (memor.cmp(&(MemoriaCfg){.src = path, .other = prefix, .bytes = prefix_length}) == 0);
        const unsigned long long from = prefixed ? prefix_length : 0ull;
        if ((length - from) >= CLI_PATH_LONGEST)
        {
            continue;
        }
        memor.cpy(&(MemoriaCfg){.dst = file, .src = path + from, .bytes = length - from});
        file[length - from] = '\0';
        if (cli_patterns_hides(patterns, file, 0) || cli_patterns_hides(set, file, 0))
        {
            continue;
        }
        // the text is what follows the third colon, or nothing where the line holds no more
        const char *const text = (field <= stop) ? field : stop;
        cli_hit_written(out, found == 0u, file, line, column, (const unsigned char *)text,
                        (unsigned long long)(stop - text));
        found += 1u;
    }
    cli_line_character(out, ']');
}

int cli_search_by_git(const char *root, const char *query, CliSearching how, const CliWords *specs,
                      const CliPatterns *patterns, const CliPatterns *set, CliLine *out, CliLine *said)
{
    static const char *const s_args[7] = {"grep", "-n", "--column", "-I", "--no-color", "--untracked", "--full-name"};
    static const char *s_words[CLI_WORDS_MOST + 24u];
    static char s_prefix[CLI_PATH_LONGEST];
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    cli_prefix(root, s_prefix);
    unsigned int count = 0u;
    s_words[count] = "git";
    count += 1u;
    count += cli_trust_git_words(root, s_words + count);
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        s_words[count] = s_args[at];
        count += 1u;
    }
    if (!how.match_case)
    {
        s_words[count] = "-i";
        count += 1u;
    }
    if (how.word)
    {
        s_words[count] = "-w";
        count += 1u;
    }
    s_words[count] = "-E";
    s_words[count + 1u] = "-e";
    s_words[count + 2u] = query;
    s_words[count + 3u] = "--";
    count += 4u;
    for (unsigned int at = 0u; at < specs->count; at += 1u)
    {
        s_words[count] = specs->word[at];
        count += 1u;
    }
    s_words[count] = NULL;
    CliRan *const ran = cli_ran_taken();
    char why[CLI_NAME_LONGEST];
    CliLine unstarted = {why, sizeof(why), 0ull};
    int came = CLI_GIT_SEARCH_NONE;
    // git says 1 where nothing matched, and more where it could not search
    if (!cli_ran(s_words, root, NULL, 0u, ran, &unstarted))
    {
        came = CLI_GIT_SEARCH_NONE;
    }
    else if (ran->code == 0ll)
    {
        cli_git_hits(ran, s_prefix, patterns, set, out);
        came = CLI_GIT_SEARCH_FOUND;
    }
    else if (ran->code == 1ll)
    {
        cli_line_text(out, "[]");
        came = CLI_GIT_SEARCH_FOUND;
    }
    else if (cellul.find(&(CatenaFinitaCfg){
                 .src = ran->err, .cap = CLI_CELLS_POOL, .other = "regular expression", .other_cap = CLI_CELLS_POOL}) !=
             NULL)
    {
        const char *from = ran->err;
        const char *to = ran->err + ran->err_size;
        cli_git_trimmed(&from, &to);
        // the trimmed text lies in what git said, its end at or past its start
        cli_line_counted(said, from, (unsigned long long)(to - from));
        came = CLI_GIT_SEARCH_FAILED;
    }
    cli_ran_given(ran);
    cli_lock_give(&s_lock);
    return came;
}

// A find: the words of the query in lower case, each where it starts and its count of letters, the patterns that hide,
// the path being held, its letters in lower case, the paths found, and the folders still to look in
typedef struct
{
    unsigned int word[CLI_FUZZY_LETTERS];
    unsigned int start[CLI_FUZZY_LETTERS];
    unsigned int length[CLI_FUZZY_LETTERS];
    unsigned int words;
    CliPatterns patterns;
    unsigned int shown[CLI_FUZZY_LETTERS];
    unsigned int lower[CLI_FUZZY_LETTERS];
    char (*found)[CLI_PATH_LONGEST];
    unsigned int found_count;
    unsigned int found_room;
    char (*pending)[CLI_PATH_LONGEST];
    unsigned int pending_count;
    unsigned int pending_room;
    const char *root;
    const char *folder;
} CliFinding;

static CliFinding s_finding;

// the paths a list's room is begun with, doubling as they are put
#define CLI_FIND_FIRST 16u

// `*list`, `count` paths of `*room`, made to hold one more, `most` at the most: 1, or 0 where it holds the most or the
// larger cell is refused
static int cli_finding_room(char (**list)[CLI_PATH_LONGEST], unsigned int count, unsigned int *room, unsigned int most,
                            const char *what)
{
    if (count < *room)
    {
        return 1;
    }
    const unsigned int doubled = (*room == 0u) ? CLI_FIND_FIRST : (*room * 2u);
    const unsigned int grown = (doubled < most) ? doubled : most;
    char (*const larger)[CLI_PATH_LONGEST] =
        (count < most) ? cli_cell_taken((unsigned long long)grown * CLI_PATH_LONGEST, what) : NULL;
    if (larger == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = larger, .src = *list, .bytes = (size_t)count * CLI_PATH_LONGEST});
    cli_cell_given(*list);
    *list = larger;
    *room = grown;
    return 1;
}

// the find's lists given back
static void cli_finding_given(void)
{
    cli_cell_given(s_finding.found);
    cli_cell_given(s_finding.pending);
    s_finding.found = NULL;
    s_finding.pending = NULL;
    s_finding.found_room = 0u;
    s_finding.pending_room = 0u;
}

// whether the path holds each word of the query, case aside, and the patterns leave it
static int cli_finding_holds(const char *path)
{
    const unsigned int count = cli_files_letters(path, s_finding.shown, s_finding.lower, CLI_FUZZY_LETTERS);
    for (unsigned int index = 0u; index < s_finding.words; index += 1u)
    {
        const unsigned int *const word = s_finding.word + s_finding.start[index];
        const unsigned int length = s_finding.length[index];
        int holds = 0;
        for (unsigned int at = 0u; !holds && ((at + length) <= count); at += 1u)
        {
            holds = memor.cmp(&(MemoriaCfg){
                        .src = s_finding.lower + at, .other = word, .bytes = length * sizeof(unsigned int)}) == 0;
        }
        if (!holds)
        {
            return 0;
        }
    }
    return !cli_patterns_hides(&s_finding.patterns, path, 0);
}

static void cli_finding_each(void *context, const char *name, int is_folder)
{
    (void)context;
    if (s_finding.found_count >= CLI_FOUND_MOST)
    {
        return;
    }
    char full[CLI_PATH_LONGEST];
    CliLine line = {full, sizeof(full), 0ull};
    cli_line_text(&line, s_finding.folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    if (cli_line_closed(&line) == 0ull)
    {
        return;
    }
    if (is_folder)
    {
        const int skipped = (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = ".git", .cap = CLI_CELLS_POOL})) ||
                            (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "target", .cap = CLI_CELLS_POOL}));
        if (!skipped && cli_finding_room(&s_finding.pending, s_finding.pending_count, &s_finding.pending_room,
                                         CLI_FIND_PENDING, "the folders a find has still to look in"))
        {
            memor.cpy(&(MemoriaCfg){.dst = s_finding.pending[s_finding.pending_count],
                                    .src = full,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = full, .cap = CLI_CELLS_POOL}) + 1u});
            s_finding.pending_count += 1u;
        }
        return;
    }
    char path[CLI_PATH_LONGEST];
    cli_relative(s_finding.root, full, path);
    if (cli_finding_holds(path) && cli_finding_room(&s_finding.found, s_finding.found_count, &s_finding.found_room,
                                                    CLI_FOUND_MOST, "the paths a find found"))
    {
        memor.cpy(&(MemoriaCfg){.dst = s_finding.found[s_finding.found_count],
                                .src = path,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
        s_finding.found_count += 1u;
    }
}

// the query's words read into the find, in lower case
static void cli_finding_words(const char *query)
{
    // a letter count is below CLI_FUZZY_LETTERS, which an unsigned int holds
    const unsigned int count = (unsigned int)cli_regexp_letters(
        query, cellul.len(&(CatenaFinitaCfg){.src = query, .cap = CLI_CELLS_POOL}), s_finding.word, CLI_FUZZY_LETTERS);
    s_finding.words = 0u;
    int within = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        s_finding.word[at] = cli_regexp_lower(s_finding.word[at]);
        const int white = cli_fuzzy_white(s_finding.word[at]);
        if (!white && !within)
        {
            s_finding.start[s_finding.words] = at;
            s_finding.length[s_finding.words] = 0u;
            s_finding.words += 1u;
        }
        s_finding.length[s_finding.words - 1u] += white ? 0u : 1u;
        within = !white;
    }
}

void cli_files_find(const char *root, const char *query, CliLine *out)
{
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    cli_finding_words(query);
    cli_patterns_of(CLI_PART_EXPLORER, &s_finding.patterns);
    s_finding.found_count = 0u;
    cli_line_character(out, '[');
    const CliSeen *const seen = cli_seen_held(root);
    if ((seen != NULL) && seen->git)
    {
        for (unsigned int index = 0u; (index < seen->files.count) && (s_finding.found_count < CLI_FOUND_MOST);
             index += 1u)
        {
            const char *const path = cli_files_at(&seen->files, index);
            if (cli_finding_holds(path))
            {
                cli_line_text(out, (s_finding.found_count != 0u) ? "," : "");
                cli_json_quoted(out, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
                s_finding.found_count += 1u;
            }
        }
        cli_seen_let(seen);
        cli_line_character(out, ']');
        cli_lock_give(&s_lock);
        return;
    }
    cli_seen_let(seen);
    s_finding.root = root;
    if (!cli_finding_room(&s_finding.pending, 0u, &s_finding.pending_room, CLI_FIND_PENDING,
                          "the folders a find has still to look in"))
    {
        cli_line_character(out, ']');
        cli_lock_give(&s_lock);
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_finding.pending[0],
                            .src = root,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL}) + 1u});
    s_finding.pending_count = 1u;
    while ((s_finding.pending_count > 0u) && (s_finding.found_count < CLI_FOUND_MOST))
    {
        s_finding.pending_count -= 1u;
        char folder[CLI_PATH_LONGEST];
        memor.cpy(
            &(MemoriaCfg){.dst = folder, .bytes = sizeof(folder), .src = s_finding.pending[s_finding.pending_count]});
        s_finding.folder = folder;
        query_folder_each(folder, cli_finding_each, NULL);
    }
    // the paths found sorted by their bytes
    for (unsigned int at = 1u; at < s_finding.found_count; at += 1u)
    {
        for (unsigned int place = at;
             (place > 0u) && (cli_text_order(s_finding.found[place - 1u], s_finding.found[place]) > 0); place -= 1u)
        {
            char held[CLI_PATH_LONGEST];
            memor.cpy(&(MemoriaCfg){
                .dst = held,
                .src = s_finding.found[place],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_finding.found[place], .cap = CLI_CELLS_POOL}) + 1u});
            memor.cpy(&(MemoriaCfg){
                .dst = s_finding.found[place],
                .src = s_finding.found[place - 1u],
                .bytes =
                    cellul.len(&(CatenaFinitaCfg){.src = s_finding.found[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
            memor.cpy(&(MemoriaCfg){.dst = s_finding.found[place - 1u],
                                    .src = held,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = held, .cap = CLI_CELLS_POOL}) + 1u});
        }
    }
    for (unsigned int at = 0u; at < s_finding.found_count; at += 1u)
    {
        cli_line_text(out, (at != 0u) ? "," : "");
        cli_json_quoted(out, s_finding.found[at],
                        cellul.len(&(CatenaFinitaCfg){.src = s_finding.found[at], .cap = CLI_CELLS_POOL}));
    }
    cli_line_character(out, ']');
    cli_finding_given();
    cli_lock_give(&s_lock);
}
