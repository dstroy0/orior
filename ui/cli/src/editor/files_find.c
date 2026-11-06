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
#define CLI_FOUND_MAX 500u

void cli_hit_write(const CliFilesPathCtx *args)
{
    if (args->each != NULL)
    {
        EMBED_CALL(args->each, CliEachHitCfg, .context = args->context, .path = args->path, .line = args->line,
                   .col = args->col, .text = args->text, .size = cli_files_path_internal.hit_cut(args));
        return;
    }
    mmgr_line_put(args->out, args->first ? "{\"path\":" : ",{\"path\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = args->path,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}));
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
    const CliRunResult *run_result;
    const char *prefix_read;
    const CliPatterns *patterns;
    const CliPatterns *set;
    VerbaLinea *out;
    const char *path;
    const char *query;
    void (*each)(const CliEachHitCfg *args);
    void *context;
} FilesFindCtx;
// a whole number as Rust reads one, a plus before it or none, from `length` bytes of `text`: 1 with it into `value`,
// or 0 where it is none
EMBED_INLINE int cli_span_is_whole_word(const FilesFindCtx *args)
{
    unsigned long long *value = args->value;
    unsigned long long at = ((args->length > 0ULL) && (args->text[0] == '+')) ? 1ULL : 0ULL;
    if (at == args->length)
    {
        return 0;
    }
    *value = 0ULL;
    for (; at < args->length; at += 1ULL)
    {
        const unsigned long long digit = (unsigned long long)(args->text[at] - '0');
        if ((args->text[at] < '0') || (args->text[at] > '9') || (*value > ((~0ULL - digit) / 10ULL)))
        {
            return 0;
        }
        *value = (*value * 10ULL) + digit;
    }
    return 1;
}

// the folder of the tree git's paths start from, as `rev-parse --show-prefix` gives it, into `prefix`: empty where git
// gives none
EMBED_INLINE void cli_prefix(const FilesFindCtx *args)
{
    static const char *const s_words[4] = {"git", "rev-parse", "--show-prefix", NULL};
    args->prefix[0] = '\0';
    CliRunResult *const run_result = cli_run_result_alloc();
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ULL};
    if (EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = s_words, .folder = args->root, .settings = NULL,
                   .count = 0u, .run_result = run_result, .err_msg = &unstarted))
    {
        const char *from = run_result->out;
        const char *to = run_result->out + run_result->out_size;
        EMBED_CALL(cli_git_from_internal.trim, CliGitFromCtx, .from = &from, .to = &to);
        // the trimmed output lies in the output, its end at or past its start
        const unsigned long long length = (unsigned long long)(to - from);
        if (length < CLI_PATH_MAX_LEN)
        {
            memor.cpy(&(MemoriaCfg){.dst = args->prefix, .src = from, .bytes = length});
            args->prefix[length] = '\0';
        }
    }
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
}

// the lines git found, each `path:line:column:text`, written as hits, at most CLI_HITS_MAX of them, less those whose
// files the patterns hide
EMBED_INLINE void cli_git_hits(const FilesFindCtx *args)
{
    const CliPatterns *patterns = args->patterns;
    VerbaLinea *out = args->out;
    const unsigned long long prefix_length =
        cellul.len(&(CatenaFinitaCfg){.src = args->prefix_read, .cap = CLI_POOL_BYTES});
    const char *at = args->run_result->out;
    const char *const end = args->run_result->out + args->run_result->out_size;
    unsigned int found = 0u;
    if (out != NULL)
    {
        mmgr_line_ch(out, '[');
    }
    while ((at < end) && (found < CLI_HITS_MAX))
    {
        const CliTextLine bounds = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = at, .at = 0ULL,
                                              .size = (unsigned long long)(end - at));
        const char *const stop = at + bounds.cut;
        const char *field = at;
        at += bounds.next;
        unsigned long long length = 0ULL;
        const char *const path = EMBED_CALL(cli_git_at_internal.field, CliGitAtCtx, .at = &field, .end = stop,
                                            .stop = ':', .length_pointer = &length);
        unsigned long long number_length = 0ULL;
        const char *const number = EMBED_CALL(cli_git_at_internal.field, CliGitAtCtx, .at = &field, .end = stop,
                                              .stop = ':', .length_pointer = &number_length);
        unsigned long long col_length = 0ULL;
        const char *const col = EMBED_CALL(cli_git_at_internal.field, CliGitAtCtx, .at = &field, .end = stop,
                                           .stop = ':', .length_pointer = &col_length);
        unsigned long long line = 0ULL;
        unsigned long long column = 0ULL;
        if ((path == NULL) || (number == NULL) || (col == NULL) ||
            !EMBED_CALL(cli_span_is_whole_word, FilesFindCtx, .text = number, .length = number_length,
                        .value = &line) ||
            !EMBED_CALL(cli_span_is_whole_word, FilesFindCtx, .text = col, .length = col_length, .value = &column))
        {
            continue;
        }
        char file[CLI_PATH_MAX_LEN];
        const int prefixed =
            (length >= prefix_length) &&
            (memor.cmp(&(MemoriaCfg){.src = path, .other = args->prefix_read, .bytes = prefix_length}) == 0);
        const unsigned long long from = prefixed ? prefix_length : 0ULL;
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
                   .size = (unsigned long long)(stop - text), .each = args->each, .context = args->context);
        found += 1u;
    }
    if (out != NULL)
    {
        mmgr_line_ch(out, ']');
    }
}

int cli_search_by_git(const CliFilesRootCtx *args)
{
    static const char *const s_args[7] = {"grep", "-n", "--column", "-I", "--no-color", "--untracked", "--full-name"};
    static const char *s_words[CLI_WORDS_MAX + 24u];
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
    if (args->how.word_text)
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
        s_words[count] = args->specs->word_text[at];
        count += 1u;
    }
    s_words[count] = NULL;
    CliRunResult *const run_result = cli_run_result_alloc();
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ULL};
    int received = CLI_GIT_SEARCH_NONE;
    // git returns 1 where nothing matched, and more where it could not search
    if (!EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = s_words, .folder = args->root, .settings = NULL,
                    .count = 0u, .run_result = run_result, .err_msg = &unstarted))
    {
        received = CLI_GIT_SEARCH_NONE;
    }
    else if (run_result->code == 0LL)
    {
        EMBED_CALL(cli_git_hits, FilesFindCtx, .run_result = run_result, .prefix_read = s_prefix,
                   .patterns = args->patterns, .set = args->set, .out = args->out, .each = args->each,
                   .context = args->context);
        received = CLI_GIT_SEARCH_FOUND;
    }
    else if (run_result->code == 1LL)
    {
        if (args->out != NULL)
        {
            mmgr_line_put(args->out, "[]");
        }
        received = CLI_GIT_SEARCH_FOUND;
    }
    else if (cellul.find(&(CatenaFinitaCfg){.src = run_result->err,
                                            .cap = CLI_POOL_BYTES,
                                            .other = "regular expression",
                                            .other_cap = CLI_POOL_BYTES}) != NULL)
    {
        const char *from = run_result->err;
        const char *to = run_result->err + run_result->err_size;
        EMBED_CALL(cli_git_from_internal.trim, CliGitFromCtx, .from = &from, .to = &to);
        // the trimmed text lies in what git printed, its end at or past its start
        mmgr_line_put_n(args->err_msg, from, (unsigned long long)(to - from));
        received = CLI_GIT_SEARCH_FAILED;
    }
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    return received;
}

// A find: the words of the query in lower case, each where it starts and its count of letters, the patterns that hide,
// the path being matched, its letters in lower case, and the paths found
typedef struct
{
    unsigned int word_text[CLI_FUZZY_LETTERS];
    unsigned int start[CLI_FUZZY_LETTERS];
    unsigned int length[CLI_FUZZY_LETTERS];
    unsigned int words;
    CliPatterns patterns;
    unsigned int displayed[CLI_FUZZY_LETTERS];
    unsigned int lower[CLI_FUZZY_LETTERS];
    char (*found)[CLI_PATH_MAX_LEN];
    unsigned int found_count;
} CliFinding;
