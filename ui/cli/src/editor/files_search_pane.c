// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_search_pane.c: the Search pane's rows. The query, its choices and the set chosen stand here; a change marks the
// rest's start, and the step that finds the rest passed searches the tree through files_search.c, which leaves the
// lines it found standing in its own cells with the files they were found in and each file's first line. Every row
// points into them, the lines of one file under one file row, each file's first row written as the search ends and as
// a file closes or opens, and a regular expression's match in each line read once as the search ends. The sets
// Preferences lists are read as the pane begins, as the set moves on and as a search runs. The watch marks the search
// stale from its thread through an atomic word
#include "files.h"

#include "../command_line/cells.h"
#include "../text/regexp.h"
#include "../text/text.h"
#include "embed_atomic.h"
#include "files_search_hits.h"

// the most letters a found line's text decodes to for a regular expression's match, the keys the set chosen and the
// sets stand under in orior's folder, and the set's name where the whole tree is searched
#define SEARCH_LETTERS (CLI_HIT_TEXT + 1u)
#define SEARCH_SET_KEY "search.set"
#define SEARCH_SETS_KEY "search.sets"
#define SEARCH_WHOLE "Whole tree"

// A file with lines found: its first row, the bytes of its path, the place of its name in its path, and whether its
// lines are closed
typedef struct
{
    unsigned int row;
    unsigned int length;
    unsigned int name;
    int closed;
} SearchFile;

// The pane's data: the query and its bytes, how it is read, the set chosen counted from 1 and 0 for the whole tree,
// how many sets Preferences lists, the set's name and its patterns, the files with lines found in the search's order
// and the rows counted, the bytes a regular expression matched in each line, the report, whether rows stand for the
// query, whether a search waits and since when, and the regular expression the query reads as with its work state, the
// letters of a line and the match's spans
typedef struct
{
    char query[CLI_SEARCH_QUERY_MAX];
    unsigned long long query_length;
    CliSearching how;
    unsigned int set;
    unsigned int sets;
    char set_name[CLI_SEARCH_NAME_MAX];
    CliPatterns patterns;
    SearchFile file[CLI_HITS_MAX];
    unsigned int rows;
    unsigned int match[CLI_HITS_MAX];
    char report[CLI_SEARCH_REPORT];
    unsigned long long report_length;
    int searched;
    int waits;
    unsigned long long changed_ms;
    CliRegexp regexp;
    CliRegexpWorkState work_state;
    int regexp_read;
    unsigned int letters[SEARCH_LETTERS];
    CliRegexpSpan groups[CLI_REGEXP_GROUPS + 1u];
} Search;

static Search *s_search;
static void (*s_woken)(void);
static const CliStateEntries *s_entries;
static embed_atomic_u32 s_stale;

typedef struct
{
    const char *name;
    unsigned int which;
    unsigned int at;
    const CliSearchHit *hit;
    const char *root;
    unsigned long long now_ms;
    unsigned int set;
    char *name_out;
    unsigned int *count_out;
} FilesSearchPaneCtx;

// the cell taken where none is: it, or NULL where none can be taken
EMBED_INLINE Search *search_of(const FilesSearchPaneCtx *args)
{
    (void)args;
    if (s_search == NULL)
    {
        s_search = g_cli_cells->persistent_buf_alloc(sizeof(Search), &g_cli_search_pane_cells);
        if (s_search != NULL)
        {
            memor.set(&(MemoriaCfg){.dst = s_search, .val = 0, .bytes = sizeof(Search)});
            EMBED_CALL(cli_regexp_capacity.capacity, CliRegexpWorkStateCfg, .regexp_out = &s_search->regexp,
                       .work_state = &s_search->work_state);
        }
    }
    return s_search;
}

// the sets Preferences lists, `<name>:` on a line and its patterns on the lines after: how many; where `set` is not 0,
// that set's name into `name_out`, sized CLI_SEARCH_NAME_MAX, and its patterns read in place from its lines; and where
// `name` is not NULL, the number of the set with that name into `count_out`, 0 where none has it
EMBED_INLINE unsigned int search_sets_read(const FilesSearchPaneCtx *args)
{
    Search *const search = s_search;
    const char *const sets = (s_entries != NULL) ? cli_state_entry_value(s_entries, SEARCH_SETS_KEY) : NULL;
    const unsigned long long name_length =
        (args->name != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_SEARCH_NAME_MAX}) : 0ULL;
    unsigned int count = 0u;
    unsigned long long lines_from = 0ULL;
    unsigned long long lines_to = 0ULL;
    if (args->name_out != NULL)
    {
        args->name_out[0] = '\0';
    }
    if (args->name != NULL)
    {
        *args->count_out = 0u;
    }
    const unsigned long long size =
        (sets != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = sets, .cap = CLI_CELLS_POOL}) : 0ULL;
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
        // the set before this one ends where its name's line starts
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
        if ((count == args->set) && (args->name_out != NULL))
        {
            memor.cpy(&(MemoriaCfg){.dst = args->name_out, .src = sets + from, .bytes = length});
            args->name_out[length] = '\0';
            lines_from = at;
            lines_to = size;
        }
        if ((args->name != NULL) && (length == name_length) &&
            cellul.starts(&(CatenaFinitaCfg){.src = sets + from, .other = args->name, .cap = CLI_SEARCH_NAME_MAX}))
        {
            *args->count_out = count;
        }
    }
    if (args->set != 0u)
    {
        // the set's lines, as they stand in the entry, read as its patterns
        EMBED_CALL(cli_patterns_text.read_set_text, CliPatternsTextCfg, .text = (sets != NULL) ? sets + lines_from : "",
                   .size = (lines_to > lines_from) ? lines_to - lines_from : 0ULL, .patterns_out = &search->patterns);
    }
    return count;
}

// the bytes a code point takes in UTF-8
EMBED_INLINE unsigned long long search_letter_bytes(const FilesSearchPaneCtx *args)
{
    const unsigned int letter = args->which;
    if (letter < 0x80u)
    {
        return 1ULL;
    }
    if (letter < 0x800u)
    {
        return 2ULL;
    }
    return (letter < 0x10000u) ? 3ULL : 4ULL;
}

// the bytes the regular expression matches at the column of the line `hit`, read in the line's letters: 1 where it
// matches nothing there
EMBED_INLINE unsigned int search_regexp_match(const FilesSearchPaneCtx *args)
{
    Search *const search = s_search;
    const CliSearchHit *const hit = args->hit;
    const unsigned long long size = hit->size;
    const unsigned long long match_at = ((hit->col > 0ULL) && (hit->col - 1ULL < size)) ? hit->col - 1ULL : 0ULL;
    const char *const text = (const char *)hit->text;
    const unsigned long long before =
        EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = text, .size = match_at,
                   .letters = search->letters, .capacity_u64 = SEARCH_LETTERS);
    const unsigned long long letters = EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = text, .size = size,
                                                  .letters = search->letters, .capacity_u64 = SEARCH_LETTERS);
    if ((before >= letters) ||
        !EMBED_CALL(cli_regexp_text_u_32s.match_into, CliRegexpTextU32sCfg, .regexp = &search->regexp,
                    .text_u32s = search->letters, .length = letters, .at = before, .groups = search->groups))
    {
        return 1u;
    }
    unsigned long long bytes = 0ULL;
    for (unsigned long long at = search->groups[0].from; (at < search->groups[0].to) && (at < letters); at += 1ULL)
    {
        bytes += EMBED_CALL(search_letter_bytes, FilesSearchPaneCtx, .which = search->letters[at]);
    }
    // a line's bytes are below CLI_HIT_BYTES, which an unsigned int counts
    return (bytes != 0ULL) ? (unsigned int)bytes : 1u;
}

// how many lines the file `at` has among the lines standing
EMBED_INLINE unsigned int search_file_lines(const FilesSearchPaneCtx *args)
{
    const unsigned int end = (args->at + 1u < cli_search_standing_files())
                                 ? cli_search_standing_file_first(args->at + 1u)
                                 : cli_search_standing_count();
    return end - cli_search_standing_file_first(args->at);
}

// the files' first rows from the file `at` on, whose own first row stands, and the rows counted: a row a file, and
// under one whose lines are not closed a row each line
EMBED_INLINE void search_layout(const FilesSearchPaneCtx *args)
{
    Search *const search = s_search;
    const unsigned int files = cli_search_standing_files();
    unsigned int row = (args->at < files) ? search->file[args->at].row : 0u;
    for (unsigned int at = args->at; at < files; at += 1u)
    {
        search->file[at].row = row;
        row += 1u + (search->file[at].closed ? 0u : EMBED_CALL(search_file_lines, FilesSearchPaneCtx, .at = at));
    }
    search->rows = row;
}

// the file whose rows the row `at` stands in: the last whose first row is at or before it
EMBED_INLINE unsigned int search_file_of(const FilesSearchPaneCtx *args)
{
    const Search *const search = s_search;
    unsigned int low = 0u;
    unsigned int high = cli_search_standing_files();
    while (high - low > 1u)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (search->file[middle].row <= args->at)
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

// the tree at `root` searched for the query: the lines found standing in the search's cells, each file's path measured
// and its name found, each line's regular expression match read, the rows laid out, the report written, and the rows
// standing for the query
EMBED_INLINE void search_run(const FilesSearchPaneCtx *args)
{
    Search *const search = s_search;
    search->waits = 0;
    search->rows = 0u;
    search->report_length = 0ULL;
    search->report[0] = '\0';
    search->searched = 0;
    unsigned long long trimmed = 0ULL;
    for (unsigned long long at = 0ULL; at < search->query_length; at += 1ULL)
    {
        trimmed += ((search->query[at] != ' ') && (search->query[at] != '\t')) ? 1ULL : 0ULL;
    }
    if ((trimmed == 0ULL) || (args->root == NULL))
    {
        cli_search_standing_free();
        return;
    }
    search->searched = 1;
    search->sets = EMBED_CALL(search_sets_read, FilesSearchPaneCtx, .set = search->set, .name_out = search->set_name);
    char quiet[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea unread = {quiet, sizeof(quiet), 0ULL};
    search->regexp_read =
        search->how.regex &&
        EMBED_CALL(cli_regexp_source.read, CliRegexpSourceCfg, .regexp_out = &search->regexp, .source = search->query,
                   .flags = search->how.match_case ? "" : "i", .err_msg = &unread);
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    static CliPatterns s_none;
    VerbaLinea report = {search->report, sizeof(search->report), 0ULL};
    const int found =
        EMBED_CALL(cli_files_root_out_err_msg.search, CliFilesRootOutErrMsgCfg, .root = args->root,
                   .query = search->query, .how = search->how, .set = (search->set != 0u) ? &search->patterns : &s_none,
                   .out = NULL, .err_msg = &err_msg, .each = NULL, .context = NULL, .standing = 1);
    const unsigned int files = cli_search_standing_files();
    const unsigned int hits = cli_search_standing_count();
    for (unsigned int at = 0u; at < files; at += 1u)
    {
        const char *const path = cli_search_standing_path(cli_search_standing_hit(cli_search_standing_file_first(at)));
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_PATH_MAX_LEN});
        // a path's bytes are below CLI_PATH_MAX_LEN, which an unsigned int counts
        search->file[at].length = (unsigned int)length;
        search->file[at].name = (unsigned int)EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                                                         .text = path, .at = length, .set = "/");
        search->file[at].closed = 0;
    }
    for (unsigned int at = 0u; search->regexp_read && (at < hits); at += 1u)
    {
        search->match[at] = EMBED_CALL(search_regexp_match, FilesSearchPaneCtx, .hit = cli_search_standing_hit(at));
    }
    EMBED_CALL(search_layout, FilesSearchPaneCtx, .at = 0u);
    if (!found)
    {
        mmgr_line_put_n(&report, why, mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
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
}

// a change to the query or its choices at `now_ms`: the rest begins
EMBED_INLINE void search_changed(const FilesSearchPaneCtx *args)
{
    s_search->waits = 1;
    s_search->changed_ms = args->now_ms;
}

void cli_search_pane_begin(const CliSearchPaneCfg *args)
{
    s_woken = args->woken;
    s_entries = args->entries;
    Search *const search = EMBED_CALL(search_of, FilesSearchPaneCtx, .at = 0u);
    if (search == NULL)
    {
        return;
    }
    // the set saved found by its name among the sets listed
    const char *const saved = (args->entries != NULL) ? cli_state_entry_value(args->entries, SEARCH_SET_KEY) : NULL;
    unsigned int found = 0u;
    search->sets = EMBED_CALL(search_sets_read, FilesSearchPaneCtx, .set = 0u,
                              .name = ((saved != NULL) && (saved[0] != '\0')) ? saved : NULL, .count_out = &found);
    search->set = found;
    if (found != 0u)
    {
        (void)EMBED_CALL(search_sets_read, FilesSearchPaneCtx, .set = found, .name_out = search->set_name);
    }
}

void cli_search_pane_type(const CliSearchPaneCfg *args)
{
    Search *const search = EMBED_CALL(search_of, FilesSearchPaneCtx, .at = 0u);
    if ((search == NULL) || (args->size == 0ULL))
    {
        return;
    }
    unsigned long long size = args->size;
    for (unsigned long long at = 0ULL; at < size; at += 1ULL)
    {
        // a control character typed, a tab or a return, is no part of a query
        size = ((unsigned char)args->text[at] < 0x20u) ? at : size;
    }
    if ((size == 0ULL) || (search->query_length + size + 1ULL > CLI_SEARCH_QUERY_MAX))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = search->query + search->query_length, .src = args->text, .bytes = size});
    search->query_length += size;
    search->query[search->query_length] = '\0';
    EMBED_CALL(search_changed, FilesSearchPaneCtx, .now_ms = args->now_ms);
}

int cli_search_pane_key(const CliSearchPaneCfg *args)
{
    Search *const search = EMBED_CALL(search_of, FilesSearchPaneCtx, .at = 0u);
    if (search == NULL)
    {
        return 0;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Backspace", .cap = 16ULL}))
    {
        // the last letter erased: its lead byte and the continuation bytes after it
        while (search->query_length > 0ULL)
        {
            search->query_length -= 1ULL;
            if (((unsigned char)search->query[search->query_length] & 0xC0u) != 0x80u)
            {
                break;
            }
        }
        search->query[search->query_length] = '\0';
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
    Search *const search = EMBED_CALL(search_of, FilesSearchPaneCtx, .at = 0u);
    if (search == NULL)
    {
        return;
    }
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
        search->sets =
            EMBED_CALL(search_sets_read, FilesSearchPaneCtx, .set = search->set, .name_out = search->set_name);
        char folder[CLI_PATH_MAX_LEN];
        char why[CLI_ERR_MSG_MAX_LEN];
        VerbaLinea err_msg = {why, sizeof(why), 0ULL};
        if (EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = folder))
        {
            (void)cli_state_value_save(&(CliStateValueCfg){.folder = folder,
                                                           .key = SEARCH_SET_KEY,
                                                           .value = (search->set != 0u) ? search->set_name : "",
                                                           .err_msg = &err_msg});
        }
        break;
    }
    default:
        return;
    }
    EMBED_CALL(search_changed, FilesSearchPaneCtx, .now_ms = args->now_ms - CLI_SEARCH_REST_MS);
}

int cli_search_pane_on(const CliSearchPaneCfg *args)
{
    const Search *const search = s_search;
    if (search == NULL)
    {
        return 0;
    }
    switch (args->which)
    {
    case CLI_SEARCH_CASE:
        return search->how.match_case;
    case CLI_SEARCH_WORD:
        return search->how.word_text;
    case CLI_SEARCH_REGEX:
        return search->how.regex;
    default:
        return 0;
    }
}

const char *cli_search_pane_query(const CliSearchPaneCfg *args)
{
    const Search *const search = s_search;
    *args->length_out = (search != NULL) ? search->query_length : 0ULL;
    return (search != NULL) ? search->query : "";
}

const char *cli_search_pane_set(const CliSearchPaneCfg *args)
{
    const Search *const search = s_search;
    *args->count_out = (search != NULL) ? search->sets : 0u;
    return ((search != NULL) && (search->set != 0u) && (search->set_name[0] != '\0')) ? search->set_name : SEARCH_WHOLE;
}

int cli_search_pane_step(const CliSearchPaneCfg *args)
{
    Search *const search = EMBED_CALL(search_of, FilesSearchPaneCtx, .at = 0u);
    if (search == NULL)
    {
        return 0;
    }
    const int stale = (EMBED_ATOMIC_EXCHANGE_32(&s_stale, 0u) != 0u) && search->searched;
    if (stale || (search->waits && (args->now_ms - search->changed_ms >= CLI_SEARCH_REST_MS)))
    {
        EMBED_CALL(search_run, FilesSearchPaneCtx, .root = args->root);
        return 1;
    }
    return 0;
}

unsigned long long cli_search_pane_deadline_ms(const CliSearchPaneCfg *args)
{
    (void)args;
    const Search *const search = s_search;
    return ((search != NULL) && search->waits) ? search->changed_ms + CLI_SEARCH_REST_MS : 0ULL;
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
    const Search *const search = s_search;
    return ((search != NULL) && search->searched) ? search->rows : 0u;
}

int cli_search_pane_row(const CliSearchPaneCfg *args)
{
    const Search *const search = s_search;
    CliSearchRow *const row = args->row;
    memor.set(&(MemoriaCfg){.dst = row, .val = 0, .bytes = sizeof(*row)});
    if ((search == NULL) || !search->searched || (args->at >= search->rows))
    {
        return 0;
    }
    const unsigned int file = EMBED_CALL(search_file_of, FilesSearchPaneCtx, .at = args->at);
    const SearchFile *const found = &search->file[file];
    const unsigned int first = cli_search_standing_file_first(file);
    const unsigned int under = args->at - found->row;
    row->path = cli_search_standing_path(cli_search_standing_hit(first));
    if (under == 0u)
    {
        row->kind = CLI_SEARCH_ROW_FILE;
        row->name = row->path + found->name;
        row->name_length = found->length - found->name;
        row->folder_length = (found->name > 0u) ? found->name - 1u : 0u;
        row->count = EMBED_CALL(search_file_lines, FilesSearchPaneCtx, .at = file);
        row->open = !found->closed;
        return 1;
    }
    const unsigned int at = first + under - 1u;
    const CliSearchHit *const hit = cli_search_standing_hit(at);
    row->kind = CLI_SEARCH_ROW_HIT;
    row->line = hit->line;
    row->col = hit->col;
    row->text = (const char *)hit->text;
    row->text_length = hit->size;
    // the match: the query's bytes at its column, or what the regular expression matched there, one byte at the least
    row->match_at = ((hit->col > 0ULL) && (hit->col - 1ULL < hit->size)) ? hit->col - 1ULL : 0ULL;
    const unsigned long long left = hit->size - row->match_at;
    const unsigned long long length =
        search->regexp_read ? search->match[at] : ((search->query_length < left) ? search->query_length : left);
    row->match_length = (length != 0ULL) ? length : 1ULL;
    return 1;
}

void cli_search_pane_toggle(const CliSearchPaneCfg *args)
{
    Search *const search = s_search;
    if ((search == NULL) || !search->searched || (args->at >= search->rows))
    {
        return;
    }
    const unsigned int file = EMBED_CALL(search_file_of, FilesSearchPaneCtx, .at = args->at);
    if (search->file[file].row != args->at)
    {
        return;
    }
    search->file[file].closed = !search->file[file].closed;
    EMBED_CALL(search_layout, FilesSearchPaneCtx, .at = file);
}

unsigned long long cli_search_pane_report(const CliSearchPaneCfg *args)
{
    const Search *const search = s_search;
    if ((search == NULL) || !search->searched)
    {
        args->report[0] = '\0';
        return 0ULL;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->report, .src = search->report, .bytes = search->report_length + 1ULL});
    return search->report_length;
}
