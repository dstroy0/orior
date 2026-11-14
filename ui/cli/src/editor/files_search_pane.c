// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_search_pane.c: the Search pane's rows. The query, its choices and the set chosen are here; a change marks the
// rest's start, and the step that finds the rest passed searches the tree through files_search.c, which leaves the
// lines it found located in its own cells with the files they were found in and each file's first line. Every row
// points into them, the lines of one file under one file row. What the search adds, each file's first row with its
// name's place and whether it is closed, and the bytes a regular expression matched in each line, is in one movable
// cell sized to that search, read and written a record at a time, and released as the next begins; the report is
// in the pane's own bytes. The sets Preferences lists are read as the pane begins, as the set moves on and as a search
// runs. The watch marks the search stale from its thread through an atomic word
#include "files.h"

#include "../command_line/pool.h"
#include "../text/regexp.h"
#include "../text/text.h"
#include "../text/typed.h"
#include "embed_atomic.h"
#include "files_search_hits.h"

// the max characters a found line's text decodes to for a regular expression's match, the keys the set chosen and the
// sets are under in orior's folder, and the set's name where the whole tree is searched
#define SEARCH_CHARACTERS (CLI_HIT_TEXT + 1u)
#define SEARCH_SET_KEY "search.set"
#define SEARCH_SETS_KEY "search.sets"
#define SEARCH_WHOLE "Whole tree"
// a part of a cell rounded up to the alignment every part of it has
#define SEARCH_ALIGNED(bytes_) ((((unsigned long long)(bytes_)) + 15ULL) & ~15ULL)

// A file with lines found: its first row, the bytes of its path, the place of its name in its path, and whether its
// lines are closed
typedef struct
{
    unsigned int row;
    unsigned int length;
    unsigned int name;
    int closed;
} SearchFile;

// The pane's data: the query, how it is read, the set chosen counted from 1 and 0 for the whole tree, how many sets
// Preferences lists, the set's name in Preferences' text with its bytes, whether a search waits and since when, whether
// rows are for the query and how many, the report with its bytes, and the search's movable cell: a SearchFile a file
// and, where the query is a regular expression, the bytes it matched in each line after them, how many files it holds
// and whether it holds the matches. The set's patterns are read into the search run's scratch as it runs
typedef struct
{
    CliTyped query;
    CliSearching how;
    unsigned int set;
    unsigned int sets;
    const char *set_name;
    unsigned long long set_name_length;
    char set_name_text[CLI_SEARCH_NAME_MAX];
    int waits;
    unsigned long long changed_ms;
    int searched;
    unsigned int rows;
    char report[CLI_SEARCH_REPORT];
    unsigned long long report_length;
    const CarcerMovableEntry *cell;
    unsigned int files;
    int matched;
} Search;

// the place of the file `file_`'s record in the search's cell, and of the line `at_`'s match after the files' records
#define SEARCH_FILE_AT(file_) ((unsigned long long)(file_) * sizeof(SearchFile))
#define SEARCH_MATCH_AT(at_) (SEARCH_FILE_AT(s_search.files) + ((unsigned long long)(at_) * sizeof(unsigned int)))

static Search s_search;
static void (*s_woken)(void);
static const CliStateEntries *s_entries;
static embed_atomic_u32 s_stale;

typedef struct
{
    const char *name;
    unsigned int at;
    const CliStandingLine *line_read;
    const char *text;
    const char *root;
    unsigned long long now_ms;
    unsigned int set;
    unsigned int *count_out;
    CliRegexp *regexp;
    unsigned int *characters;
    CliRegexpSpan *groups;
    CliPatterns *patterns;
} FilesSearchPaneCtx;

// the sets Preferences lists, `<name>:` on a line and its patterns on the lines after: how many; the set `set`'s name
// in the text and, where `patterns` is not NULL, its patterns read from its lines into it, none where `set` is 0; and
// where `name` is not NULL, the number of the set with that name into `count_out`, 0 where none has it
EMBED_INLINE unsigned int search_sets_read(const FilesSearchPaneCtx *args)
{
    Search *const search = &s_search;
    // the sets' text read out of the entries' cell into a cell of this call
    const unsigned long long sets_size = cli_state_entry_size(s_entries, SEARCH_SETS_KEY);
    char *const sets_text = (sets_size != CLI_STATE_NONE)
                                ? g_cli_guard->temporary_buf_alloc(sets_size + 1ULL, g_cli_search_pane_owner)
                                : NULL;
    const char *const sets =
        (sets_text != NULL) ? cli_state_entry_value(s_entries, SEARCH_SETS_KEY, sets_text, sets_size + 1ULL) : NULL;
    const unsigned long long name_length =
        (args->name != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_SEARCH_NAME_MAX}) : 0ULL;
    unsigned int count = 0u;
    unsigned long long lines_from = 0ULL;
    unsigned long long lines_to = 0ULL;
    search->set_name = SEARCH_WHOLE;
    search->set_name_length = sizeof(SEARCH_WHOLE) - 1ULL;
    if (args->name != NULL)
    {
        *args->count_out = 0u;
    }
    const unsigned long long size =
        (sets != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = sets, .cap = CLI_POOL_BYTES}) : 0ULL;
    for (unsigned long long at = 0ULL; at < size;)
    {
        const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                  .text = sets, .at = at, .size = size, .set = "\n");
        const unsigned long long from = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                                                   .text = sets, .at = at, .size = end, .set = " \t\r");
        unsigned long long to = end;
        while ((to > from) && ((sets[to - 1ULL] == ' ') || (sets[to - 1ULL] == '\t') || (sets[to - 1ULL] == '\r')))
        {
            to -= 1ULL;
        }
        const unsigned long long line_start = at;
        at = end + 1ULL;
        if ((to == from) || (sets[to - 1ULL] != ':') || (to - from < 2ULL))
        {
            continue;
        }
        // the set before this one ends where this one's name starts
        if ((count == args->set) && (args->set != 0u))
        {
            lines_to = line_start;
        }
        count += 1u;
        unsigned long long name_end = to - 1ULL;
        while ((name_end > from) && (sets[name_end - 1ULL] == ' '))
        {
            name_end -= 1ULL;
        }
        const unsigned long long length =
            (name_end - from < CLI_SEARCH_NAME_MAX - 1ULL) ? name_end - from : CLI_SEARCH_NAME_MAX - 1ULL;
        if (count == args->set)
        {
            // the set's name copied into the search's own bytes, which are past this call
            memor.cpy(&(MemoriaCfg){.dst = search->set_name_text, .src = sets + from, .bytes = (embed_word)length});
            search->set_name_text[length] = '\0';
            search->set_name = search->set_name_text;
            search->set_name_length = length;
            lines_from = at;
            lines_to = size;
        }
        if ((args->name != NULL) && (length == name_length) &&
            cellul.starts(&(CatenaFinitaCfg){.src = sets + from, .other = args->name, .cap = CLI_SEARCH_NAME_MAX}))
        {
            *args->count_out = count;
        }
    }
    // the set's lines, as they are in the entry, read as its patterns; none where the whole tree is searched
    if (args->patterns != NULL)
    {
        EMBED_CALL(cli_patterns_text.read_set_text, CliPatternsTextCfg, .text = (sets != NULL) ? sets + lines_from : "",
                   .size = ((args->set != 0u) && (lines_to > lines_from)) ? lines_to - lines_from : 0ULL,
                   .patterns_out = args->patterns);
    }
    g_cli_guard->persistent_buf_release(sets_text, g_cli_search_pane_owner);
    return count;
}

// the bytes a code point takes in UTF-8
EMBED_INLINE unsigned long long search_character_bytes(const FilesSearchPaneCtx *args)
{
    const unsigned int character = args->at;
    if (character < 0x80u)
    {
        return 1ULL;
    }
    if (character < 0x800u)
    {
        return 2ULL;
    }
    return (character < 0x10000u) ? 3ULL : 4ULL;
}

// the bytes `regexp` matches at the column of the located line `line_read`, whose text is `text`, read in the line's
// characters decoded into `characters` with the match's spans into `groups`: 1 where it matches nothing there
EMBED_INLINE unsigned int search_regexp_match(const FilesSearchPaneCtx *args)
{
    const CliStandingLine *const hit = args->line_read;
    const unsigned long long size = hit->size;
    const unsigned long long match_at = ((hit->col > 0ULL) && (hit->col - 1ULL < size)) ? hit->col - 1ULL : 0ULL;
    const char *const text = args->text;
    const unsigned long long before =
        EMBED_CALL(cli_regexp_text.characters, CliRegexpTextCfg, .text = text, .size = match_at, .characters = args->characters,
                   .capacity_u64 = SEARCH_CHARACTERS);
    const unsigned long long characters = EMBED_CALL(cli_regexp_text.characters, CliRegexpTextCfg, .text = text, .size = size,
                                                  .characters = args->characters, .capacity_u64 = SEARCH_CHARACTERS);
    if ((before >= characters) ||
        !EMBED_CALL(cli_regexp_text_u_32s.match_into, CliRegexpTextU32sCfg, .regexp = args->regexp,
                    .text_u32s = args->characters, .length = characters, .at = before, .groups = args->groups))
    {
        return 1u;
    }
    unsigned long long bytes = 0ULL;
    for (unsigned long long at = args->groups[0].from; (at < args->groups[0].to) && (at < characters); at += 1ULL)
    {
        bytes += EMBED_CALL(search_character_bytes, FilesSearchPaneCtx, .at = args->characters[at]);
    }
    // a line's bytes are below CLI_HIT_BYTES, which an unsigned int counts
    return (bytes != 0ULL) ? (unsigned int)bytes : 1u;
}

// how many lines the file `at` has among the lines located
EMBED_INLINE unsigned int search_file_lines(const FilesSearchPaneCtx *args)
{
    const unsigned int end = (args->at + 1u < cli_search_located_files())
                                 ? cli_search_located_file_first(args->at + 1u)
                                 : cli_search_located_count();
    return end - cli_search_located_file_first(args->at);
}

// the files' first rows from the file `at` on, and the rows counted: a row a file, and under one whose lines are not
// closed a row each line. The first file's first row is 0; a later file's own first row is from the layout before
EMBED_INLINE void search_layout(const FilesSearchPaneCtx *args)
{
    Search *const search = &s_search;
    const unsigned int files = search->files;
    SearchFile file;
    unsigned int row = 0u;
    if ((args->at != 0u) && (args->at < files))
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = search->cell, .offset = SEARCH_FILE_AT(args->at), .dst = &file, .bytes = sizeof(file)});
        row = file.row;
    }
    for (unsigned int at = args->at; at < files; at += 1u)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = search->cell, .offset = SEARCH_FILE_AT(at), .dst = &file, .bytes = sizeof(file)});
        file.row = row;
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = search->cell, .offset = SEARCH_FILE_AT(at), .src = &file, .bytes = sizeof(file)});
        row += 1u + (file.closed ? 0u : EMBED_CALL(search_file_lines, FilesSearchPaneCtx, .at = at));
    }
    search->rows = row;
}

// the file whose rows the row `at` is in: the last whose first row is at or before it
EMBED_INLINE unsigned int search_file_of(const FilesSearchPaneCtx *args)
{
    const Search *const search = &s_search;
    unsigned int low = 0u;
    unsigned int high = search->files;
    while (high - low > 1u)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        SearchFile file;
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = search->cell, .offset = SEARCH_FILE_AT(middle), .dst = &file, .bytes = sizeof(file)});
        if (file.row <= args->at)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

// the tree at `root` searched for the query: the lines found located in the search's cells; the search's cell taken
// with the report, each file's path measured and its name found, and each line's regular expression match read; the
// rows arranged; and the rows located for the query
EMBED_INLINE void search_run(const FilesSearchPaneCtx *args)
{
    Search *const search = &s_search;
    search->waits = 0;
    search->searched = 0;
    if (search->cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(search->cell, g_cli_search_pane_owner);
    }
    search->cell = NULL;
    search->files = 0u;
    search->matched = 0;
    search->report[0] = '\0';
    search->report_length = 0ULL;
    search->rows = 0u;
    // the query copied out of the typed text's cell for the search
    char query[CLI_SEARCH_QUERY_MAX];
    const unsigned long long query_length =
        EMBED_CALL(cli_typed.copy, CliTypedCfg, .typed_read = &search->query, .dst = query, .capacity = sizeof(query));
    // a query of blanks alone ends at its first byte that is not one
    const unsigned long long blank = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                                                .text = query, .at = 0ULL, .size = query_length, .set = " \t");
    if ((blank == query_length) || (args->root == NULL))
    {
        cli_search_located_free();
        return;
    }
    // the run's scratch, in the temporary tier: the error line, the set's patterns, and for a regular expression the
    // pattern, its work state, the match's spans and a line's characters, each part aligned
    const unsigned long long patterns_at = SEARCH_ALIGNED(CLI_ERR_MSG_MAX_LEN);
    const unsigned long long regexp_at = patterns_at + SEARCH_ALIGNED(sizeof(CliPatterns));
    const unsigned long long work_state_at = regexp_at + SEARCH_ALIGNED(sizeof(CliRegexp));
    const unsigned long long groups_at = work_state_at + SEARCH_ALIGNED(sizeof(CliRegexpWorkState));
    const unsigned long long characters_at =
        groups_at + SEARCH_ALIGNED(sizeof(CliRegexpSpan) * (CLI_REGEXP_GROUPS + 1ULL));
    const unsigned long long scratch_bytes =
        search->how.regex ? characters_at + SEARCH_ALIGNED(sizeof(unsigned int) * SEARCH_CHARACTERS) : regexp_at;
    char *const scratch = g_cli_guard->temporary_buf_alloc(scratch_bytes, g_cli_search_pane_owner);
    if (scratch == NULL)
    {
        cli_search_located_free();
        return;
    }
    search->searched = 1;
    VerbaLinea err_msg = {scratch, CLI_ERR_MSG_MAX_LEN, 0ULL};
    CliPatterns *const patterns = (CliPatterns *)(void *)(scratch + patterns_at);
    search->sets = EMBED_CALL(search_sets_read, FilesSearchPaneCtx, .set = search->set, .patterns = patterns);
    CliRegexp *const regexp = search->how.regex ? (CliRegexp *)(void *)(scratch + regexp_at) : NULL;
    unsigned int *const characters = search->how.regex ? (unsigned int *)(void *)(scratch + characters_at) : NULL;
    CliRegexpSpan *const groups = search->how.regex ? (CliRegexpSpan *)(void *)(scratch + groups_at) : NULL;
    int regexp_read = 0;
    if (search->how.regex)
    {
        EMBED_CALL(cli_regexp_capacity.capacity, CliRegexpWorkStateCfg, .regexp_out = regexp,
                   .work_state = (CliRegexpWorkState *)(void *)(scratch + work_state_at));
        regexp_read = EMBED_CALL(cli_regexp_source.read, CliRegexpSourceCfg, .regexp_out = regexp, .source = query,
                                 .flags = search->how.match_case ? "" : "i", .err_msg = &err_msg);
        err_msg.at = 0ULL;
    }
    const int found = EMBED_CALL(cli_files_root_out_err_msg.search, CliFilesRootOutErrMsgCfg, .root = args->root,
                                 .query = query, .how = search->how, .set = patterns, .out = NULL, .err_msg = &err_msg,
                                 .each = NULL, .context = NULL, .located = 1);
    const unsigned int files = cli_search_located_files();
    const unsigned int hits = cli_search_located_count();
    // the cell holds a record a file and, for a regular expression, a match a line; a search that found no file takes
    // none
    const unsigned long long cell_bytes =
        SEARCH_FILE_AT(files) + (regexp_read ? (unsigned long long)hits * sizeof(unsigned int) : 0ULL);
    search->cell = (cell_bytes != 0ULL) ? g_cli_guard->movable_buf_alloc(cell_bytes, g_cli_search_pane_owner) : NULL;
    if ((cell_bytes != 0ULL) && (search->cell == NULL))
    {
        search->searched = 0;
        cli_search_located_free();
        g_cli_guard->persistent_buf_release(scratch, g_cli_search_pane_owner);
        return;
    }
    search->files = files;
    search->matched = regexp_read;
    for (unsigned int at = 0u; at < files; at += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        (void)cli_search_located_path(at, path);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN});
        // a path's bytes are below CLI_PATH_MAX_LEN, which an unsigned int counts
        const SearchFile file = {0u, (unsigned int)length,
                                 (unsigned int)EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                                                          .text = path, .at = length, .set = "/"),
                                 0};
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = search->cell, .offset = SEARCH_FILE_AT(at), .src = &file, .bytes = sizeof(file)});
    }
    for (unsigned int at = 0u; regexp_read && (at < hits); at += 1u)
    {
        // the line and its text read out of the located cell into this call's bytes
        CliStandingLine line;
        char text[CLI_SEARCH_ROW_TEXT];
        (void)cli_search_located_line(at, &line);
        (void)cli_search_located_text(&line, text, sizeof(text));
        const unsigned int match = EMBED_CALL(search_regexp_match, FilesSearchPaneCtx, .line_read = &line, .text = text,
                                              .regexp = regexp, .characters = characters, .groups = groups);
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
            .cell = search->cell, .offset = SEARCH_MATCH_AT(at), .src = &match, .bytes = sizeof(match)});
    }
    EMBED_CALL(search_layout, FilesSearchPaneCtx, .at = 0u);
    VerbaLinea report = {search->report, sizeof(search->report), 0ULL};
    if (!found)
    {
        mmgr_line_put_n(&report, scratch, mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
    }
    else if (hits == 0u)
    {
        mmgr_line_put(&report, "No results found.");
    }
    else
    {
        mmgr_line_decimal(&report, hits, 1u);
        mmgr_line_put(&report,
                      (hits >= CLI_HITS_MAX) ? "+ results in " : ((hits == 1u) ? " result in " : " results in "));
        mmgr_line_decimal(&report, files, 1u);
        mmgr_line_put(&report, (files == 1u) ? " file" : " files");
    }
    search->report_length = mmgr_line_finish(&report) ? report.at : 0ULL;
    g_cli_guard->persistent_buf_release(scratch, g_cli_search_pane_owner);
}

// a change to the query or its choices at `now_ms`: the rest begins
EMBED_INLINE void search_changed(const FilesSearchPaneCtx *args)
{
    s_search.waits = 1;
    s_search.changed_ms = args->now_ms;
}

void cli_search_pane_begin(const CliSearchPaneCfg *args)
{
    s_woken = args->woken;
    s_entries = args->entries;
    Search *const search = &s_search;
    // the set saved found by its name among the sets listed
    char saved_name[CLI_SEARCH_NAME_MAX];
    const char *const saved = cli_state_entry_value(args->entries, SEARCH_SET_KEY, saved_name, sizeof(saved_name));
    unsigned int found = 0u;
    search->sets = EMBED_CALL(search_sets_read, FilesSearchPaneCtx, .set = 0u,
                              .name = ((saved != NULL) && (saved[0] != '\0')) ? saved : NULL, .count_out = &found);
    search->set = found;
    search->sets = EMBED_CALL(search_sets_read, FilesSearchPaneCtx, .set = found);
}

void cli_search_pane_type(const CliSearchPaneCfg *args)
{
    const unsigned long long before = s_search.query.length;
    EMBED_CALL(cli_typed.put, CliTypedCfg, .typed = &s_search.query, .text = args->text, .size = args->size,
               .max = CLI_SEARCH_QUERY_MAX, .owner = g_cli_search_pane_owner);
    if (s_search.query.length != before)
    {
        EMBED_CALL(search_changed, FilesSearchPaneCtx, .now_ms = args->now_ms);
    }
}

int cli_search_pane_key(const CliSearchPaneCfg *args)
{
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Backspace", .cap = 16ULL}))
    {
        EMBED_CALL(cli_typed.erase, CliTypedCfg, .typed = &s_search.query, .owner = g_cli_search_pane_owner);
        EMBED_CALL(search_changed, FilesSearchPaneCtx, .now_ms = args->now_ms);
        return 1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Enter", .cap = 16ULL}))
    {
        // the rest is over: the next step searches
        EMBED_CALL(search_changed, FilesSearchPaneCtx, .now_ms = args->now_ms - CLI_SEARCH_REST_MS);
        return 1;
    }
    return 0;
}

void cli_search_pane_option(const CliSearchPaneCfg *args)
{
    Search *const search = &s_search;
    switch (args->which)
    {
    case CLI_SEARCH_CASE:
        search->how.match_case = !search->how.match_case;
        break;
    case CLI_SEARCH_WORD:
        search->how.word_text = !search->how.word_text;
        break;
    case CLI_SEARCH_REGEX:
        search->how.regex = !search->how.regex;
        break;
    case CLI_SEARCH_SET: {
        search->set = (search->set < search->sets) ? search->set + 1u : 0u;
        search->sets = EMBED_CALL(search_sets_read, FilesSearchPaneCtx, .set = search->set);
        // the set's name saved as its own text, with the folder it is saved in, in the temporary tier for the save
        char *const folder = g_cli_guard->temporary_buf_alloc(
            CLI_PATH_MAX_LEN + CLI_ERR_MSG_MAX_LEN + search->set_name_length + 1ULL, g_cli_search_pane_owner);
        if (folder == NULL)
        {
            break;
        }
        VerbaLinea err_msg = {folder + CLI_PATH_MAX_LEN, CLI_ERR_MSG_MAX_LEN, 0ULL};
        char *const value = folder + CLI_PATH_MAX_LEN + CLI_ERR_MSG_MAX_LEN;
        const unsigned long long length = (search->set != 0u) ? search->set_name_length : 0ULL;
        memor.cpy(&(MemoriaCfg){.dst = value, .src = search->set_name, .bytes = length});
        value[length] = '\0';
        if (EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
        {
            (void)cli_state_value_save(
                &(CliStateValueCfg){.folder = folder, .key = SEARCH_SET_KEY, .value = value, .err_msg = &err_msg});
        }
        g_cli_guard->persistent_buf_release(folder, g_cli_search_pane_owner);
        break;
    }
    default:
        return;
    }
    EMBED_CALL(search_changed, FilesSearchPaneCtx, .now_ms = args->now_ms - CLI_SEARCH_REST_MS);
}

int cli_search_pane_on(const CliSearchPaneCfg *args)
{
    switch (args->which)
    {
    case CLI_SEARCH_CASE:
        return s_search.how.match_case;
    case CLI_SEARCH_WORD:
        return s_search.how.word_text;
    case CLI_SEARCH_REGEX:
        return s_search.how.regex;
    default:
        return 0;
    }
}

unsigned long long cli_search_pane_query(const CliSearchPaneCfg *args)
{
    return EMBED_CALL(cli_typed.copy, CliTypedCfg, .typed = &s_search.query, .dst = args->text_out,
                      .capacity = CLI_SEARCH_QUERY_MAX);
}

const char *cli_search_pane_set(const CliSearchPaneCfg *args)
{
    *args->count_out = s_search.sets;
    *args->length_out = (s_search.set_name != NULL) ? s_search.set_name_length : sizeof(SEARCH_WHOLE) - 1ULL;
    return (s_search.set_name != NULL) ? s_search.set_name : SEARCH_WHOLE;
}

int cli_search_pane_step(const CliSearchPaneCfg *args)
{
    const int stale = (EMBED_ATOMIC_EXCHANGE_32(&s_stale, 0u) != 0u) && s_search.searched;
    if (stale || (s_search.waits && (args->now_ms - s_search.changed_ms >= CLI_SEARCH_REST_MS)))
    {
        EMBED_CALL(search_run, FilesSearchPaneCtx, .root = args->root);
        return 1;
    }
    return 0;
}

unsigned long long cli_search_pane_deadline_ms(const CliSearchPaneCfg *args)
{
    (void)args;
    return s_search.waits ? s_search.changed_ms + CLI_SEARCH_REST_MS : 0ULL;
}

void cli_search_pane_stale(const CliSearchPaneCfg *args)
{
    (void)args;
    EMBED_ATOMIC_STORE(&s_stale, 1u);
    if (s_woken != NULL)
    {
        s_woken();
    }
}

unsigned int cli_search_pane_rows(const CliSearchPaneCfg *args)
{
    (void)args;
    return s_search.rows;
}

int cli_search_pane_row(const CliSearchPaneCfg *args)
{
    const Search *const search = &s_search;
    CliSearchRow *const row = args->row;
    memor.set(&(MemoriaCfg){.dst = row, .val = 0, .bytes = sizeof(*row)});
    if (args->at >= search->rows)
    {
        return 0;
    }
    const unsigned int file = EMBED_CALL(search_file_of, FilesSearchPaneCtx, .at = args->at);
    SearchFile found;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = search->cell, .offset = SEARCH_FILE_AT(file), .dst = &found, .bytes = sizeof(found)});
    const unsigned int first = cli_search_located_file_first(file);
    const unsigned int under = args->at - found.row;
    row->path = cli_search_located_path(file, row->path_copied);
    if (under == 0u)
    {
        row->type = CLI_SEARCH_ROW_FILE;
        row->name = row->path + found.name;
        row->name_length = found.length - found.name;
        row->folder_length = (found.name > 0u) ? found.name - 1u : 0u;
        row->count = EMBED_CALL(search_file_lines, FilesSearchPaneCtx, .at = file);
        row->open = !found.closed;
        return 1;
    }
    const unsigned int at = first + under - 1u;
    CliStandingLine hit;
    if (!cli_search_located_line(at, &hit))
    {
        return 0;
    }
    row->type = CLI_SEARCH_ROW_HIT;
    row->line = hit.line;
    row->col = hit.col;
    row->text_length = cli_search_located_text(&hit, row->text_copied, sizeof(row->text_copied));
    row->text = row->text_copied;
    // the match: the query's bytes at its column, or what the regular expression matched there, one byte at the least
    row->match_at = ((hit.col > 0ULL) && (hit.col - 1ULL < row->text_length)) ? hit.col - 1ULL : 0ULL;
    const unsigned long long left = row->text_length - row->match_at;
    unsigned int matched = 0u;
    if (search->matched)
    {
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
            .cell = search->cell, .offset = SEARCH_MATCH_AT(at), .dst = &matched, .bytes = sizeof(matched)});
    }
    const unsigned long long length =
        search->matched ? matched : ((search->query.length < left) ? search->query.length : left);
    row->match_length = (length != 0ULL) ? length : 1ULL;
    return 1;
}

void cli_search_pane_toggle(const CliSearchPaneCfg *args)
{
    Search *const search = &s_search;
    if (args->at >= search->rows)
    {
        return;
    }
    const unsigned int file = EMBED_CALL(search_file_of, FilesSearchPaneCtx, .at = args->at);
    SearchFile found;
    (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){
        .cell = search->cell, .offset = SEARCH_FILE_AT(file), .dst = &found, .bytes = sizeof(found)});
    if (found.row != args->at)
    {
        return;
    }
    found.closed = !found.closed;
    (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
        .cell = search->cell, .offset = SEARCH_FILE_AT(file), .src = &found, .bytes = sizeof(found)});
    EMBED_CALL(search_layout, FilesSearchPaneCtx, .at = file);
}

const char *cli_search_pane_report(const CliSearchPaneCfg *args)
{
    *args->length_out = s_search.report_length;
    return s_search.report;
}
