// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_search_pane.c: the Search pane's rows. The query, its choices and the set chosen stand here; a change marks the
// rest's start, and the step that finds the rest passed searches the tree through files_search.c, each line found
// handed in the order of the files and copied into the pane's own cell with its match's place and bytes, the lines of
// one file a run of hits under one file entry. The set's patterns are read from Preferences' `search.sets` at each
// search. The watch marks the search stale from its thread through an atomic word
#include "files.h"

#include "../command_line/cells.h"
#include "../text/regexp.h"
#include "../text/text.h"
#include "embed_atomic.h"
#include "files_internal.h"

// the bytes the paths and lines found take, the most files closed, the most lines a set's patterns take and the bytes
// they take, the most letters a found line's text decodes to for a regular expression's match, and the key the set
// chosen is saved under in orior's folder
#define SEARCH_TEXT (4u << 20u)
#define SEARCH_CLOSED 64u
#define SEARCH_SET_LINES 256u
#define SEARCH_SET_TEXT 16384u
#define SEARCH_LETTERS (CLI_HIT_TEXT + 1u)
#define SEARCH_SET_KEY "search.set"
#define SEARCH_SETS_KEY "search.sets"
#define SEARCH_WHOLE "Whole tree"

// A line found: its file's path's and its text's places in the text, its line and column counted from 1, the text's
// bytes, the match's place and bytes in the text, and its file entry
typedef struct
{
    unsigned int path;
    unsigned int text;
    unsigned long long line;
    unsigned long long col;
    unsigned long long size;
    unsigned long long match_at;
    unsigned long long match_length;
    unsigned int file;
} SearchHit;

// A file with lines found: its path's place in the text, its first line found and how many
typedef struct
{
    unsigned int path;
    unsigned int first;
    unsigned int count;
} SearchFile;

// The pane's data: the query and its bytes, how it is read, the set chosen counted from 1 and 0 for the whole tree,
// the set's name, its patterns and the lines they are read from, the lines found and their files, the text their
// paths and texts stand in, the hashes of the paths of the files closed, the report, whether rows stand for the query,
// whether a search waits and since when, and the regular expression the query reads as with its work state, the letters
// of a line and the match's spans
typedef struct
{
    char query[CLI_SEARCH_QUERY_MAX];
    unsigned long long query_length;
    CliSearching how;
    unsigned int set;
    char set_name[CLI_SEARCH_NAME_MAX];
    CliPatterns patterns;
    char set_text[SEARCH_SET_TEXT];
    const char *set_line[SEARCH_SET_LINES];
    SearchHit hit[CLI_HITS_MAX];
    unsigned int hits;
    SearchFile file[CLI_HITS_MAX];
    unsigned int files;
    char text[SEARCH_TEXT];
    unsigned int used;
    unsigned long long closed[SEARCH_CLOSED];
    unsigned int closed_count;
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
    const char *text;
    unsigned long long length;
    const char *name;
    unsigned int which;
    unsigned int at;
    SearchHit *hit;
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

// `length` bytes of `text` copied into the pane's text with a NUL after them: their place, or SEARCH_TEXT where they do
// not fit
EMBED_INLINE unsigned int search_text_append(const FilesSearchPaneCtx *args)
{
    Search *const search = s_search;
    if (args->length + 1ULL > (unsigned long long)(SEARCH_TEXT - search->used))
    {
        return SEARCH_TEXT;
    }
    const unsigned int at = search->used;
    memor.cpy(&(MemoriaCfg){.dst = search->text + at, .src = args->text, .bytes = args->length});
    search->text[at + args->length] = '\0';
    // a text that fits the bytes is counted by an unsigned int
    search->used = at + (unsigned int)args->length + 1u;
    return at;
}

// the hash of `length` bytes of `text`, FNV-1a over 64 bits
EMBED_INLINE unsigned long long search_hash(const FilesSearchPaneCtx *args)
{
    unsigned long long hash = 14695981039346656037ULL;
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        hash ^= (unsigned char)args->text[at];
        hash *= 1099511628211ULL;
    }
    return hash;
}

// the place of the hash of the path `text` among the closed, SEARCH_CLOSED where it is not closed
EMBED_INLINE unsigned int search_closed_at(const FilesSearchPaneCtx *args)
{
    const Search *const search = s_search;
    const unsigned long long hash =
        EMBED_CALL(search_hash, FilesSearchPaneCtx, .text = args->text,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_PATH_MAX_LEN}));
    for (unsigned int at = 0u; at < search->closed_count; at += 1u)
    {
        if (search->closed[at] == hash)
        {
            return at;
        }
    }
    return SEARCH_CLOSED;
}

// the sets Preferences lists, `<name>:` on a line and its patterns on the lines after: how many; and where `set` is
// not 0, that set's name into `name_out`, sized CLI_SEARCH_NAME_MAX, and its pattern lines into the set's lines and
// its patterns
EMBED_INLINE unsigned int search_sets_read(const FilesSearchPaneCtx *args)
{
    Search *const search = s_search;
    const char *const sets = (s_entries != NULL) ? cli_state_entry_value(s_entries, SEARCH_SETS_KEY) : NULL;
    unsigned int count = 0u;
    unsigned int lines = 0u;
    unsigned int used = 0u;
    if (args->name_out != NULL)
    {
        args->name_out[0] = '\0';
    }
    const unsigned long long size =
        (sets != NULL) ? cellul.len(&(CatenaFinitaCfg){.src = sets, .cap = CLI_CELLS_POOL}) : 0ULL;
    for (unsigned long long at = 0ULL; at < size;)
    {
        const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                  .text = sets, .at = at, .size = size, .set = "\n");
        unsigned long long from = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = sets,
                                             .at = at, .size = end, .set = " \t\r");
        unsigned long long to = end;
        while ((to > from) && ((sets[to - 1ULL] == ' ') || (sets[to - 1ULL] == '\t') || (sets[to - 1ULL] == '\r')))
        {
            to -= 1ULL;
        }
        at = end + 1ULL;
        if (to == from)
        {
            continue;
        }
        if ((sets[to - 1ULL] == ':') && (to - from > 1ULL))
        {
            count += 1u;
            if ((count == args->set) && (args->name_out != NULL))
            {
                unsigned long long name_end = to - 1ULL;
                while ((name_end > from) && (sets[name_end - 1ULL] == ' '))
                {
                    name_end -= 1ULL;
                }
                const unsigned long long length =
                    (name_end - from < CLI_SEARCH_NAME_MAX - 1ULL) ? name_end - from : CLI_SEARCH_NAME_MAX - 1ULL;
                memor.cpy(&(MemoriaCfg){.dst = args->name_out, .src = sets + from, .bytes = length});
                args->name_out[length] = '\0';
            }
        }
        else if ((count == args->set) && (args->set != 0u) && (lines < SEARCH_SET_LINES) &&
                 (to - from + 1ULL <= (unsigned long long)(SEARCH_SET_TEXT - used)))
        {
            memor.cpy(&(MemoriaCfg){.dst = search->set_text + used, .src = sets + from, .bytes = to - from});
            search->set_text[used + (to - from)] = '\0';
            search->set_line[lines] = search->set_text + used;
            lines += 1u;
            // the set's text is far inside what an unsigned int counts
            used += (unsigned int)(to - from) + 1u;
        }
    }
    if (args->set != 0u)
    {
        memor.set(&(MemoriaCfg){.dst = &search->patterns, .val = 0, .bytes = sizeof(search->patterns)});
        EMBED_CALL(cli_patterns_lines.read_set, CliPatternsLinesCfg, .lines = search->set_line, .count = lines,
                   .patterns_out = &search->patterns);
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

// a line found's match marked: the query's bytes at its column, or for a regular expression what it matches there,
// read in the line's letters: one byte where it matches nothing there
EMBED_INLINE void search_match_mark(const FilesSearchPaneCtx *args)
{
    Search *const search = s_search;
    SearchHit *const hit = args->hit;
    hit->match_at = ((hit->col > 0ULL) && (hit->col - 1ULL < hit->size)) ? hit->col - 1ULL : 0ULL;
    hit->match_length = 1ULL;
    if (!search->how.regex)
    {
        const unsigned long long left = hit->size - hit->match_at;
        hit->match_length = (search->query_length < left) ? search->query_length : left;
        hit->match_length = (hit->match_length != 0ULL) ? hit->match_length : 1ULL;
        return;
    }
    if (!search->regexp_read)
    {
        return;
    }
    const char *const text = search->text + hit->text;
    const unsigned long long before =
        EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = text, .size = hit->match_at,
                   .letters = search->letters, .capacity_u64 = SEARCH_LETTERS);
    const unsigned long long letters =
        EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = text, .size = hit->size,
                   .letters = search->letters, .capacity_u64 = SEARCH_LETTERS);
    if ((before >= letters) ||
        !EMBED_CALL(cli_regexp_text_u_32s.match_into, CliRegexpTextU32sCfg, .regexp = &search->regexp,
                    .text_u32s = search->letters, .length = letters, .at = before, .groups = search->groups))
    {
        return;
    }
    unsigned long long bytes = 0ULL;
    for (unsigned long long at = search->groups[0].from; (at < search->groups[0].to) && (at < letters); at += 1ULL)
    {
        bytes += EMBED_CALL(search_letter_bytes, FilesSearchPaneCtx, .which = search->letters[at]);
    }
    hit->match_length = (bytes != 0ULL) ? bytes : 1ULL;
}

// a line found handed on by the search: copied after the lines so far, under its file's entry, a new one where its
// path is not the last file's
static void search_hit(const CliEachHitCfg *args)
{
    Search *const search = s_search;
    if (search->hits >= CLI_HITS_MAX)
    {
        return;
    }
    const unsigned long long path_length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_PATH_MAX_LEN});
    if ((search->files == 0u) ||
        !cellul.eq(&(CatenaFinitaCfg){
            .src = search->text + search->file[search->files - 1u].path, .other = args->path, .cap = CLI_PATH_MAX_LEN}))
    {
        const unsigned int path =
            EMBED_CALL(search_text_append, FilesSearchPaneCtx, .text = args->path, .length = path_length);
        if (path == SEARCH_TEXT)
        {
            return;
        }
        search->file[search->files] = (SearchFile){path, search->hits, 0u};
        search->files += 1u;
    }
    const unsigned int text =
        EMBED_CALL(search_text_append, FilesSearchPaneCtx, .text = (const char *)args->text, .length = args->size);
    if (text == SEARCH_TEXT)
    {
        return;
    }
    SearchFile *const file = &search->file[search->files - 1u];
    SearchHit *const hit = &search->hit[search->hits];
    *hit = (SearchHit){file->path, text, args->line, args->col, args->size, 0ULL, 0ULL, search->files - 1u};
    EMBED_CALL(search_match_mark, FilesSearchPaneCtx, .hit = hit);
    search->hits += 1u;
    file->count += 1u;
}

// the tree at `root` searched for the query: the lines found, the report written, the rows standing for the query
EMBED_INLINE void search_run(const FilesSearchPaneCtx *args)
{
    Search *const search = s_search;
    search->waits = 0;
    search->hits = 0u;
    search->files = 0u;
    search->used = 0u;
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
        return;
    }
    search->searched = 1;
    (void)EMBED_CALL(search_sets_read, FilesSearchPaneCtx, .set = search->set, .name_out = search->set_name);
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
    if (!EMBED_CALL(cli_files_root_out_err_msg.search, CliFilesRootOutErrMsgCfg, .root = args->root,
                    .query = search->query, .how = search->how,
                    .set = (search->set != 0u) ? &search->patterns : &s_none, .out = NULL, .err_msg = &err_msg,
                    .each = search_hit, .context = NULL))
    {
        mmgr_line_put_n(&report, why, mmgr_line_ok(&err_msg) ? err_msg.at : 0ULL);
    }
    else if (search->hits == 0u)
    {
        mmgr_line_put(&report, "No results found.");
    }
    else
    {
        mmgr_line_decimal(&report, search->hits, 1u);
        mmgr_line_put(&report, (search->hits >= CLI_HITS_MAX)
                                   ? "+ results in "
                                   : ((search->hits == 1u) ? " result in " : " results in "));
        mmgr_line_decimal(&report, search->files, 1u);
        mmgr_line_put(&report, (search->files == 1u) ? " file" : " files");
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
    const char *const saved = (args->entries != NULL) ? cli_state_entry_value(args->entries, SEARCH_SET_KEY) : NULL;
    if ((search == NULL) || (saved == NULL) || (saved[0] == '\0'))
    {
        return;
    }
    // the set saved found by its name among the sets listed
    const unsigned int count = EMBED_CALL(search_sets_read, FilesSearchPaneCtx, .set = 0u, .name_out = NULL);
    for (unsigned int at = 1u; at <= count; at += 1u)
    {
        char name[CLI_SEARCH_NAME_MAX];
        (void)EMBED_CALL(search_sets_read, FilesSearchPaneCtx, .set = at, .name_out = name);
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = saved, .cap = CLI_SEARCH_NAME_MAX}))
        {
            search->set = at;
        }
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
        const unsigned int count = EMBED_CALL(search_sets_read, FilesSearchPaneCtx, .set = 0u, .name_out = NULL);
        search->set = (search->set < count) ? search->set + 1u : 0u;
        (void)EMBED_CALL(search_sets_read, FilesSearchPaneCtx, .set = search->set, .name_out = search->set_name);
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
    Search *const search = EMBED_CALL(search_of, FilesSearchPaneCtx, .at = 0u);
    if (search == NULL)
    {
        *args->count_out = 0u;
        return SEARCH_WHOLE;
    }
    *args->count_out =
        EMBED_CALL(search_sets_read, FilesSearchPaneCtx, .set = search->set, .name_out = search->set_name);
    return ((search->set != 0u) && (search->set_name[0] != '\0')) ? search->set_name : SEARCH_WHOLE;
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

// how many rows the file `at` takes: its own, and its lines where it is not closed
EMBED_INLINE unsigned int search_file_rows(const FilesSearchPaneCtx *args)
{
    const Search *const search = s_search;
    const SearchFile *const file = &search->file[args->at];
    const int closed =
        EMBED_CALL(search_closed_at, FilesSearchPaneCtx, .text = search->text + file->path) != SEARCH_CLOSED;
    return 1u + (closed ? 0u : file->count);
}

unsigned int cli_search_pane_rows(const CliSearchPaneCfg *args)
{
    (void)args;
    const Search *const search = s_search;
    if ((search == NULL) || !search->searched)
    {
        return 0u;
    }
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < search->files; at += 1u)
    {
        count += EMBED_CALL(search_file_rows, FilesSearchPaneCtx, .at = at);
    }
    return count;
}

// the file whose rows the row `at` stands in, into `count_out`, and the row's place under the file's own row into
// `which`: 1, or 0 where the row stands past the files
EMBED_INLINE int search_file_of(const FilesSearchPaneCtx *args)
{
    const Search *const search = s_search;
    unsigned int at = args->at;
    for (unsigned int file = 0u; file < search->files; file += 1u)
    {
        const unsigned int rows = EMBED_CALL(search_file_rows, FilesSearchPaneCtx, .at = file);
        if (at < rows)
        {
            *args->count_out = file;
            *(unsigned int *)args->name_out = at;
            return 1;
        }
        at -= rows;
    }
    return 0;
}

int cli_search_pane_row(const CliSearchPaneCfg *args)
{
    const Search *const search = s_search;
    CliSearchRow *const row = args->row;
    memor.set(&(MemoriaCfg){.dst = row, .val = 0, .bytes = sizeof(*row)});
    unsigned int file_index = 0u;
    unsigned int under = 0u;
    if ((search == NULL) || !search->searched ||
        !EMBED_CALL(search_file_of, FilesSearchPaneCtx, .at = args->at, .count_out = &file_index,
                    .name_out = (char *)&under))
    {
        return 0;
    }
    const SearchFile *const file = &search->file[file_index];
    row->path = search->text + file->path;
    if (under == 0u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = row->path, .cap = CLI_PATH_MAX_LEN});
        const unsigned long long cut = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                                                  .text = row->path, .at = length, .set = "/");
        row->kind = CLI_SEARCH_ROW_FILE;
        row->name = row->path + cut;
        row->name_length = length - cut;
        row->folder_length = (cut > 0ULL) ? cut - 1ULL : 0ULL;
        row->count = file->count;
        row->open = EMBED_CALL(search_closed_at, FilesSearchPaneCtx, .text = row->path) == SEARCH_CLOSED;
        return 1;
    }
    const SearchHit *const hit = &search->hit[file->first + under - 1u];
    row->kind = CLI_SEARCH_ROW_HIT;
    row->line = hit->line;
    row->col = hit->col;
    row->text = search->text + hit->text;
    row->text_length = hit->size;
    row->match_at = hit->match_at;
    row->match_length = hit->match_length;
    return 1;
}

void cli_search_pane_toggle(const CliSearchPaneCfg *args)
{
    Search *const search = s_search;
    CliSearchRow row;
    if ((search == NULL) || !EMBED_CALL(cli_search_pane.row, CliSearchPaneCfg, .at = args->at, .row = &row) ||
        (row.kind != CLI_SEARCH_ROW_FILE))
    {
        return;
    }
    const unsigned int closed = EMBED_CALL(search_closed_at, FilesSearchPaneCtx, .text = row.path);
    if (closed != SEARCH_CLOSED)
    {
        search->closed[closed] = search->closed[search->closed_count - 1u];
        search->closed_count -= 1u;
        return;
    }
    // the table full, its first entry is replaced by the file
    const unsigned int at = (search->closed_count < SEARCH_CLOSED) ? search->closed_count : 0u;
    search->closed_count += (search->closed_count < SEARCH_CLOSED) ? 1u : 0u;
    search->closed[at] = EMBED_CALL(search_hash, FilesSearchPaneCtx, .text = row.path,
                                    .length = cellul.len(&(CatenaFinitaCfg){.src = row.path, .cap = CLI_PATH_MAX_LEN}));
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
