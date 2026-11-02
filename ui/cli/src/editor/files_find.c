// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files_find.c: a regular expression searched by git over the tree's files, and the listed files whose paths contain
// each word of a query
#include "../code/git_internal.h"
#include "../code/symbols.h"
#include "../machine/trust.h"
#include "../text/json_tree.h"
#include "../text/regexp.h"
#include "../text/text.h"
#include "files_internal.h"

// the most paths a find returns
#define CLI_FOUND_MOST 500u

void cli_hit_write(const CliFilesPathCtx *args)
{
    mmgr_line_put(args->out, args->first ? "{\"path\":" : ",{\"path\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = args->path,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(args->out, ",\"line\":");
    mmgr_line_decimal(args->out, args->line, 0u);
    mmgr_line_put(args->out, ",\"col\":");
    mmgr_line_decimal(args->out, args->col, 0u);
    mmgr_line_put(args->out, ",\"text\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = args->text, .length = cli_files_path_internal.hit_cut(args));
    mmgr_line_ch(args->out, '}');
}

typedef struct
{
    const char *text;
    unsigned long long length;
    unsigned long long *value;
    const char *root;
    char *prefix;
    const CliRan *ran;
    const char *prefix_read;
    const CliPatterns *patterns;
    const CliPatterns *set;
    VerbaLinea *out;
    const char *path;
    const char *query;
} FilesFindCtx;
// a whole number as Rust reads one, a plus before it or none, from `length` bytes of `text`: 1 with it into `value`,
// or 0 where it is none
EMBED_INLINE int cli_whole(const FilesFindCtx *args)
{
    unsigned long long *value = args->value;
    unsigned long long at = ((args->length > 0ull) && (args->text[0] == '+')) ? 1ull : 0ull;
    if (at == args->length)
    {
        return 0;
    }
    *value = 0ull;
    for (; at < args->length; at += 1ull)
    {
        const unsigned long long digit = (unsigned long long)(args->text[at] - '0');
        if ((args->text[at] < '0') || (args->text[at] > '9') || (*value > ((~0ull - digit) / 10ull)))
        {
            return 0;
        }
        *value = (*value * 10ull) + digit;
    }
    return 1;
}

// the folder of the tree git's paths start from, as `rev-parse --show-prefix` gives it, into `prefix`: empty where git
// gives none
EMBED_INLINE void cli_prefix(const FilesFindCtx *args)
{
    static const char *const s_words[4] = {"git", "rev-parse", "--show-prefix", NULL};
    args->prefix[0] = '\0';
    CliRan *const ran = cli_ran_take();
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ull};
    if (EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = s_words, .folder = args->root, .settings = NULL,
                   .count = 0u, .ran = ran, .err_msg = &unstarted))
    {
        const char *from = ran->out;
        const char *to = ran->out + ran->out_size;
        EMBED_CALL(cli_git_from_internal.trim, CliGitFromCtx, .from = &from, .to = &to);
        // the trimmed output lies in the output, its end at or past its start
        const unsigned long long length = (unsigned long long)(to - from);
        if (length < CLI_PATH_MAX_LEN)
        {
            memor.cpy(&(MemoriaCfg){.dst = args->prefix, .src = from, .bytes = length});
            args->prefix[length] = '\0';
        }
    }
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
}

// the lines git found, each `path:line:column:text`, written as hits, at most CLI_HITS_MOST of them, less those whose
// files the patterns hide
EMBED_INLINE void cli_git_hits(const FilesFindCtx *args)
{
    const CliPatterns *patterns = args->patterns;
    VerbaLinea *out = args->out;
    const unsigned long long prefix_length =
        cellul.len(&(CatenaFinitaCfg){.src = args->prefix_read, .cap = CLI_CELLS_POOL});
    const char *at = args->ran->out;
    const char *const end = args->ran->out + args->ran->out_size;
    unsigned int found = 0u;
    mmgr_line_ch(out, '[');
    while ((at < end) && (found < CLI_HITS_MOST))
    {
        const CliTextLine bounds = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = at, .at = 0ull,
                                              .size = (unsigned long long)(end - at));
        const char *const stop = at + bounds.cut;
        const char *field = at;
        at += bounds.next;
        unsigned long long length = 0ull;
        const char *const path = EMBED_CALL(cli_git_at_internal.field, CliGitAtCtx, .at = &field, .end = stop,
                                            .stop = ':', .length_pointer = &length);
        unsigned long long number_length = 0ull;
        const char *const number = EMBED_CALL(cli_git_at_internal.field, CliGitAtCtx, .at = &field, .end = stop,
                                              .stop = ':', .length_pointer = &number_length);
        unsigned long long col_length = 0ull;
        const char *const col = EMBED_CALL(cli_git_at_internal.field, CliGitAtCtx, .at = &field, .end = stop,
                                           .stop = ':', .length_pointer = &col_length);
        unsigned long long line = 0ull;
        unsigned long long column = 0ull;
        if ((path == NULL) || (number == NULL) || (col == NULL) ||
            !EMBED_CALL(cli_whole, FilesFindCtx, .text = number, .length = number_length, .value = &line) ||
            !EMBED_CALL(cli_whole, FilesFindCtx, .text = col, .length = col_length, .value = &column))
        {
            continue;
        }
        char file[CLI_PATH_MAX_LEN];
        const int prefixed =
            (length >= prefix_length) &&
            (memor.cmp(&(MemoriaCfg){.src = path, .other = args->prefix_read, .bytes = prefix_length}) == 0);
        const unsigned long long from = prefixed ? prefix_length : 0ull;
        if ((length - from) >= CLI_PATH_MAX_LEN)
        {
            continue;
        }
        memor.cpy(&(MemoriaCfg){.dst = file, .src = path + from, .bytes = length - from});
        file[length - from] = '\0';
        if (EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = patterns, .path = file, .dir = 0) ||
            EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = args->set, .path = file, .dir = 0))
        {
            continue;
        }
        // the text is what follows the third colon, or nothing where the line contains no more
        const char *const text = (field <= stop) ? field : stop;
        EMBED_CALL(cli_files_path_internal.hit_write, CliFilesPathCtx, .out = out, .first = found == 0u, .path = file,
                   .line = line, .col = column, .text = (const unsigned char *)text,
                   .size = (unsigned long long)(stop - text));
        found += 1u;
    }
    mmgr_line_ch(out, ']');
}

int cli_search_by_git(const CliFilesRootCtx *args)
{
    static const char *const s_args[7] = {"grep", "-n", "--column", "-I", "--no-color", "--untracked", "--full-name"};
    static const char *s_words[CLI_WORDS_MOST + 24u];
    static char s_prefix[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_prefix, FilesFindCtx, .root = args->root, .prefix = s_prefix);
    unsigned int count = 0u;
    s_words[count] = "git";
    count += 1u;
    count +=
        EMBED_CALL(cli_trust_tree_words.git_words, CliTrustTreeWordsCfg, .tree = args->root, .words = s_words + count);
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        s_words[count] = s_args[at];
        count += 1u;
    }
    if (!args->how.match_case)
    {
        s_words[count] = "-i";
        count += 1u;
    }
    if (args->how.word)
    {
        s_words[count] = "-w";
        count += 1u;
    }
    s_words[count] = "-E";
    s_words[count + 1u] = "-e";
    s_words[count + 2u] = args->query;
    s_words[count + 3u] = "--";
    count += 4u;
    for (unsigned int at = 0u; at < args->specs->count; at += 1u)
    {
        s_words[count] = args->specs->word[at];
        count += 1u;
    }
    s_words[count] = NULL;
    CliRan *const ran = cli_ran_take();
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ull};
    int came = CLI_GIT_SEARCH_NONE;
    // git returns 1 where nothing matched, and more where it could not search
    if (!EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = s_words, .folder = args->root, .settings = NULL,
                    .count = 0u, .ran = ran, .err_msg = &unstarted))
    {
        came = CLI_GIT_SEARCH_NONE;
    }
    else if (ran->code == 0ll)
    {
        EMBED_CALL(cli_git_hits, FilesFindCtx, .ran = ran, .prefix_read = s_prefix, .patterns = args->patterns,
                   .set = args->set, .out = args->out);
        came = CLI_GIT_SEARCH_FOUND;
    }
    else if (ran->code == 1ll)
    {
        mmgr_line_put(args->out, "[]");
        came = CLI_GIT_SEARCH_FOUND;
    }
    else if (cellul.find(&(CatenaFinitaCfg){
                 .src = ran->err, .cap = CLI_CELLS_POOL, .other = "regular expression", .other_cap = CLI_CELLS_POOL}) !=
             NULL)
    {
        const char *from = ran->err;
        const char *to = ran->err + ran->err_size;
        EMBED_CALL(cli_git_from_internal.trim, CliGitFromCtx, .from = &from, .to = &to);
        // the trimmed text lies in what git printed, its end at or past its start
        mmgr_line_put_n(args->err_msg, from, (unsigned long long)(to - from));
        came = CLI_GIT_SEARCH_FAILED;
    }
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    return came;
}

// A find: the words of the query in lower case, each where it starts and its count of letters, the patterns that hide,
// the path being matched, its letters in lower case, and the paths found
typedef struct
{
    unsigned int word[CLI_FUZZY_LETTERS];
    unsigned int start[CLI_FUZZY_LETTERS];
    unsigned int length[CLI_FUZZY_LETTERS];
    unsigned int words;
    CliPatterns patterns;
    unsigned int shown[CLI_FUZZY_LETTERS];
    unsigned int lower[CLI_FUZZY_LETTERS];
    char (*found)[CLI_PATH_MAX_LEN];
    unsigned int found_count;
} CliFinding;

static CliFinding s_finding;

// the find's lists given back
static void cli_finding_free(void)
{
    g_cli_cells->persistent_buf_release(s_finding.found, &g_cli_editor_cells);
    s_finding.found = NULL;
}

// whether the path contains each word of the query, case aside, and the patterns leave it
EMBED_INLINE int cli_finding_matches(const FilesFindCtx *args)
{
    const char *path = args->path;
    const unsigned int count =
        EMBED_CALL(cli_files_text.letters, CliFilesTextCfg, .text = path, .shown = s_finding.shown,
                   .lower = s_finding.lower, .room = CLI_FUZZY_LETTERS);
    for (unsigned int index = 0u; index < s_finding.words; index += 1u)
    {
        const unsigned int *const word = s_finding.word + s_finding.start[index];
        const unsigned int length = s_finding.length[index];
        int matches = 0;
        for (unsigned int at = 0u; !matches && ((at + length) <= count); at += 1u)
        {
            matches = memor.cmp(&(MemoriaCfg){
                          .src = s_finding.lower + at, .other = word, .bytes = length * sizeof(unsigned int)}) == 0;
        }
        if (!matches)
        {
            return 0;
        }
    }
    return !EMBED_CALL(cli_patterns_path.hides, CliPatternsPathCfg, .patterns = &s_finding.patterns, .path = path,
                       .dir = 0);
}

// the query's words read into the find, in lower case
EMBED_INLINE void cli_finding_words(const FilesFindCtx *args)
{
    // a letter count is below CLI_FUZZY_LETTERS, which an unsigned int stores
    const unsigned int count =
        (unsigned int)EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = args->query,
                                 .size = cellul.len(&(CatenaFinitaCfg){.src = args->query, .cap = CLI_CELLS_POOL}),
                                 .letters = s_finding.word, .room_u64 = CLI_FUZZY_LETTERS);
    s_finding.words = 0u;
    int within = 0;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        s_finding.word[at] = EMBED_CALL(cli_regexp_letter.lower, CliRegexpLetterCfg, .letter = s_finding.word[at]);
        const int white =
            EMBED_CALL(cli_files_letter_internal.fuzzy_white, CliFilesLetterCtx, .letter = s_finding.word[at]);
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

void cli_files_find(const CliFilesRootOutErrMsgCfg *args)
{
    const char *root = args->root;
    EMBED_CALL(cli_finding_words, FilesFindCtx, .query = args->query);
    EMBED_CALL(cli_patterns_part.get, CliPatternsPartCfg, .part = CLI_PART_EXPLORER,
               .patterns_out = &s_finding.patterns);
    s_finding.found_count = 0u;
    mmgr_line_ch(args->out, '[');
    s_finding.found =
        g_cli_cells->temporary_buf_alloc((unsigned long long)CLI_FOUND_MOST * CLI_PATH_MAX_LEN, &g_cli_editor_cells);
    if (s_finding.found == NULL)
    {
        mmgr_line_ch(args->out, ']');
        return;
    }
    EMBED_CALL(cli_symbols_context.tree_list, CliSymbolsContextCfg, .root = root);
    char path[CLI_PATH_MAX_LEN];
    unsigned int file = 0u;
    while ((s_finding.found_count < CLI_FOUND_MOST) &&
           EMBED_CALL(cli_symbols_at.tree_next_file, CliSymbolsAtCfg, .at = &file, .path_out = path))
    {
        if (EMBED_CALL(cli_finding_matches, FilesFindCtx, .path = path))
        {
            memor.cpy(&(MemoriaCfg){.dst = s_finding.found[s_finding.found_count],
                                    .src = path,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
            s_finding.found_count += 1u;
        }
    }
    // the paths found sorted by their bytes
    for (unsigned int at = 1u; at < s_finding.found_count; at += 1u)
    {
        for (unsigned int place = at;
             (place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = s_finding.found[place - 1u],
                                         .other_pointer = s_finding.found[place]) > 0);
             place -= 1u)
        {
            char saved_path[CLI_PATH_MAX_LEN];
            memor.cpy(&(MemoriaCfg){
                .dst = saved_path,
                .src = s_finding.found[place],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_finding.found[place], .cap = CLI_CELLS_POOL}) + 1u});
            memor.cpy(&(MemoriaCfg){
                .dst = s_finding.found[place],
                .src = s_finding.found[place - 1u],
                .bytes =
                    cellul.len(&(CatenaFinitaCfg){.src = s_finding.found[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
            memor.cpy(
                &(MemoriaCfg){.dst = s_finding.found[place - 1u],
                              .src = saved_path,
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = saved_path, .cap = CLI_CELLS_POOL}) + 1u});
        }
    }
    for (unsigned int at = 0u; at < s_finding.found_count; at += 1u)
    {
        mmgr_line_put(args->out, (at != 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                   .text = s_finding.found[at],
                   .length = cellul.len(&(CatenaFinitaCfg){.src = s_finding.found[at], .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_ch(args->out, ']');
    cli_finding_free();
}
