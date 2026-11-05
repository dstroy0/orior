// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// trust_code.c: the files of a tree that run as code as its tools read it, cached a while, and the words git is
// started with where a tree's hook is disabled
#include "../command_line/cells.h"
#include "../editor/files.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "trust_internal.h"

// the configurations Prettier and ESLint run as code
static const char *const s_prettier[6] = {".prettierrc.js",     ".prettierrc.cjs",     ".prettierrc.mjs",
                                          "prettier.config.js", "prettier.config.cjs", "prettier.config.mjs"};

static const char *const s_eslint[5] = {".eslintrc.js", ".eslintrc.cjs", "eslint.config.js", "eslint.config.cjs",
                                        "eslint.config.mjs"};

// how long a tree's code found is cached before the tree is looked through again, in milliseconds
#define CLI_TRUST_FOUND_FOR 10000ULL

// the most trees whose code found is cached, and the bytes the paths of a tree's code are written in
#define CLI_TRUST_TREES 8u
#define CLI_TRUST_PATHS_CAPACITY (1ULL << 20u)

// The code found in a tree looked through, and when
typedef struct
{
    char tree[CLI_PATH_MAX_LEN];
    unsigned long long when;
    CliTrustCode code;
    int has;
} CliTrustFound;

static CliTrustFound s_found[CLI_TRUST_TREES];

void cli_trust_forget(void)
{
    for (unsigned int at = 0u; at < CLI_TRUST_TREES; at += 1u)
    {
        s_found[at].has = 0;
        g_cli_cells->persistent_buf_release((&s_found[at].code)->paths, &g_cli_trust_cells);
        *(&s_found[at].code) = (CliTrustCode){NULL, 0ULL, 0u};
    }
}

typedef struct
{
    const char *paths;
    unsigned long long size;
    unsigned int count;
    CliTrustCode *code;
    const char *text;
    unsigned long long length;
    const char *word_text;
    const char **from;
    const char **to;
    const char *folder;
    const char *name;
    const char *const *names;
} TrustCodeCtx;
// `count` paths, `size` bytes of them at `paths`, copied into `code`, a cell of their own where there are any: 1, or 0
// with none into `code` where the cellblock could not meet them
EMBED_INLINE int cli_code_copy(const TrustCodeCtx *args)
{
    const char *paths = args->paths;
    unsigned long long size = args->size;
    unsigned int count = args->count;
    CliTrustCode *code = args->code;
    *code = (CliTrustCode){NULL, 0ULL, 0u};
    if (count == 0u)
    {
        return 1;
    }
    code->paths = g_cli_cells->persistent_buf_alloc(size, &g_cli_trust_cells);
    if (code->paths == NULL)
    {
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = code->paths, .src = paths, .bytes = (size_t)size});
    code->size = size;
    code->count = count;
    return 1;
}

// whether `length` bytes at `text` are `word` without regard to ASCII case
EMBED_INLINE int cli_same_word(const TrustCodeCtx *args)
{
    if (args->length != cellul.len(&(CatenaFinitaCfg){.src = args->word_text, .cap = CLI_CELLS_POOL}))
    {
        return 0;
    }
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        const char one = ((args->text[at] >= 'A') && (args->text[at] <= 'Z')) ? (char)(args->text[at] + ('a' - 'A'))
                                                                              : args->text[at];
        if (one != args->word_text[at])
        {
            return 0;
        }
    }
    return 1;
}

// the bytes from `*from` to `*to` with the whitespace at either end left off
EMBED_INLINE void cli_trim(const TrustCodeCtx *args)
{
    const char **from = args->from;
    const char **to = args->to;
    *from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = *from, .at = 0ULL,
                        .size = (unsigned long long)(*to - *from), .set = CLI_TEXT_WHITE);
    *to = *from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = *from,
                             .at = (unsigned long long)(*to - *from), .set = CLI_TEXT_WHITE);
}

int cli_trust_names_a_hook(const CliTrustCfg *args)
{
    static const char *const s_none[9] = {"", "true", "false", "yes", "no", "on", "off", "1", "0"};
    int in_core = 0;
    const char *line = args->config;
    while (line[0] != '\0')
    {
        const char *end = line;
        end += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = end, .at = 0ULL,
                          .size = cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_CELLS_POOL}), .set = "\n");
        const char *const next = (end[0] != '\0') ? (end + 1) : end;
        const char *from = line;
        const char *to = end;
        EMBED_CALL(cli_trim, TrustCodeCtx, .from = &from, .to = &to);
        line = next;
        if ((from < to) && (from[0] == '['))
        {
            from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ULL,
                               .size = (unsigned long long)(to - from), .set = "[");
            to = from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = from,
                                   .at = (unsigned long long)(to - from), .set = "]");
            EMBED_CALL(cli_trim, TrustCodeCtx, .from = &from, .to = &to);
            in_core = EMBED_CALL(cli_same_word, TrustCodeCtx, .text = from, .length = (unsigned long long)(to - from),
                                 .word_text = "core");
            continue;
        }
        const char *equals = from;
        equals += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = equals, .at = 0ULL,
                             .size = (unsigned long long)(to - equals), .set = "=");
        if (!in_core || (equals == to))
        {
            continue;
        }
        const char *name_to = equals;
        const char *name_from = from;
        EMBED_CALL(cli_trim, TrustCodeCtx, .from = &name_from, .to = &name_to);
        const char *value_from = equals + 1;
        const char *value_to = to;
        EMBED_CALL(cli_trim, TrustCodeCtx, .from = &value_from, .to = &value_to);
        value_from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = value_from,
                                 .at = 0ULL, .size = (unsigned long long)(value_to - value_from), .set = "\"");
        value_to =
            value_from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = value_from,
                                    .at = (unsigned long long)(value_to - value_from), .set = "\"");
        if (!EMBED_CALL(cli_same_word, TrustCodeCtx, .text = name_from,
                        .length = (unsigned long long)(name_to - name_from), .word_text = "fsmonitor"))
        {
            continue;
        }
        int none = 0;
        {
            unsigned int at = 0u;
            while (!none && (at < 9u))
            {
                none = EMBED_CALL(cli_same_word, TrustCodeCtx, .text = value_from,
                                  .length = (unsigned long long)(value_to - value_from), .word_text = s_none[at]);
                at += 1u;
            }
        }
        if (!none)
        {
            return 1;
        }
    }
    return 0;
}

// the file `name` under `folder` read whole, a zero after it, into a cell of its size given back with
// g_cli_cells->persistent_buf_release: the cell, or NULL where it could not be read or the cellblock could not meet it
EMBED_INLINE char *cli_read_in(const TrustCodeCtx *args)
{
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&line, args->folder);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, args->name);
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = (mmgr_line_finish(&line) != 0ULL)
                                           ? EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                        .size_pointer = &size, .owner = &g_cli_trust_cells)
                                           : NULL;
    char *const text = (bytes != NULL) ? g_cli_cells->persistent_buf_alloc(size + 1ULL, &g_cli_trust_cells) : NULL;
    if (text != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = text, .src = bytes, .bytes = (size_t)size});
        text[size] = '\0';
    }
    g_cli_cells->persistent_buf_release((void *)(uintptr_t)bytes, (&g_cli_trust_cells));
    return text;
}

// whether `text` contains a line that is `proc-macro=true` with its spaces left out
EMBED_INLINE int cli_proc_macro(const TrustCodeCtx *args)
{
    const char *text = args->text;
    const char *at = text;
    while (at[0] != '\0')
    {
        // the line's letters but its spaces, and its `\r` before the `\n` left off
        char name_buf[32];
        unsigned int count = 0u;
        int longer = 0;
        const unsigned long long stop =
            EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = at, .at = 0ULL,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL}), .set = "\n");
        for (unsigned long long piece = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg,
                                                   .text = at, .at = 0ULL, .size = stop, .set = " ");
             piece < stop;)
        {
            const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                      .text = at, .at = piece, .size = stop, .set = " ");
            const unsigned long long fits = sizeof(name_buf) - 1u - count;
            const unsigned long long take = ((end - piece) < fits) ? (end - piece) : fits;
            memor.cpy(&(MemoriaCfg){.dst = name_buf + count, .src = at + piece, .bytes = (size_t)take});
            // no more than fit in the name's capacity, which an unsigned int stores
            count += (unsigned int)take;
            longer = longer || (take < (end - piece));
            piece = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = at, .at = end,
                               .size = stop, .set = " ");
        }
        at += stop;
        if ((at[0] == '\n') && (count > 0u) && (name_buf[count - 1u] == '\r') && !longer)
        {
            count -= 1u;
        }
        name_buf[count] = '\0';
        if (!longer &&
            (cellul.eq(&(CatenaFinitaCfg){.src = name_buf, .other = "proc-macro=true", .cap = CLI_CELLS_POOL})))
        {
            return 1;
        }
        at += (at[0] == '\n') ? 1 : 0;
    }
    return 0;
}

// whether `name` is one of `count` names
EMBED_INLINE int cli_by_name(const TrustCodeCtx *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = args->names[at], .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

int cli_trust_looked_for(const CliTrustTreeCodeCfg *args)
{
    char *const paths_cell = g_cli_cells->temporary_buf_alloc(CLI_TRUST_PATHS_CAPACITY, &g_cli_trust_cells);
    VerbaLinea paths = mmgr_line_over(paths_cell, CLI_TRUST_PATHS_CAPACITY);
    const CliFiles *const files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = args->tree);
    unsigned int found = 0u;
    for (unsigned int index = 0u; (files != NULL) && (index < files->count); index += 1u)
    {
        const char *const file = EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = index);
        const char *const slash =
            EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = file,
                       .length_value = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), .byte = '/');
        const char *const name = (slash != NULL) ? (slash + 1) : file;
        int is_code = 0;
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "build.rs", .cap = CLI_CELLS_POOL}))
        {
            char adjacent[CLI_PATH_MAX_LEN];
            VerbaLinea line = {adjacent, sizeof(adjacent), 0ULL};
            mmgr_line_put_n(&line, file, (unsigned long long)(name - file));
            mmgr_line_put(&line, "Cargo.toml");
            is_code = (mmgr_line_finish(&line) != 0ULL) &&
                      EMBED_CALL(cli_files_path_kind.contains, CliFilesPathKindCfg, .files = files, .path = adjacent);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Cargo.toml", .cap = CLI_CELLS_POOL}))
        {
            char *const text = EMBED_CALL(cli_read_in, TrustCodeCtx, .folder = args->tree, .name = file);
            is_code = (text != NULL) && EMBED_CALL(cli_proc_macro, TrustCodeCtx, .text = text);
            g_cli_cells->persistent_buf_release(text, &g_cli_trust_cells);
        }
        else
        {
            is_code = EMBED_CALL(cli_by_name, TrustCodeCtx, .name = name, .names = s_prettier, .count = 6u) ||
                      EMBED_CALL(cli_by_name, TrustCodeCtx, .name = name, .names = s_eslint, .count = 5u);
        }
        if (is_code)
        {
            mmgr_line_put_n(&paths, file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}) + 1u);
            found += 1u;
        }
    }
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
    char *const config = EMBED_CALL(cli_read_in, TrustCodeCtx, .folder = args->tree, .name = ".git/config");
    if ((config != NULL) && EMBED_CALL(cli_trust_table.names_a_hook, CliTrustCfg, .config = config))
    {
        mmgr_line_put_n(&paths, ".git/config", sizeof(".git/config"));
        found += 1u;
    }
    g_cli_cells->persistent_buf_release(config, &g_cli_trust_cells);
    const int done = mmgr_line_ok(&paths) && EMBED_CALL(cli_code_copy, TrustCodeCtx, .paths = paths.out,
                                                        .size = paths.at, .count = found, .code = args->code);
    g_cli_cells->persistent_buf_release(paths.out, &g_cli_trust_cells);
    return done;
}

int cli_trust_own_code(const CliTrustTreeCodeCfg *args)
{
    CliTrustCode *code = args->code;
    const unsigned long long now = cli_clock_milliseconds();
    for (unsigned int at = 0u; at < CLI_TRUST_TREES; at += 1u)
    {
        const CliTrustFound *const one = &s_found[at];
        if (one->has && (cellul.eq(&(CatenaFinitaCfg){.src = one->tree, .other = args->tree, .cap = CLI_CELLS_POOL})) &&
            ((now - one->when) < CLI_TRUST_FOUND_FOR))
        {
            const int copied = EMBED_CALL(cli_code_copy, TrustCodeCtx, .paths = one->code.paths, .size = one->code.size,
                                          .count = one->code.count, .code = code);
            return copied;
        }
    }
    // a tree whose code the cellblock could not meet is not cached, and it is looked through again when next requested
    CliTrustCode looked = {NULL, 0ULL, 0u};
    if (!EMBED_CALL(cli_trust_tree_code.looked_for, CliTrustTreeCodeCfg, .tree = args->tree, .code = &looked))
    {
        *code = (CliTrustCode){NULL, 0ULL, 0u};
        return 0;
    }
    // the tree's place, else a free one, else the oldest
    unsigned int place = CLI_TRUST_TREES;
    {
        unsigned int at = 0u;
        while ((place == CLI_TRUST_TREES) && (at < CLI_TRUST_TREES))
        {
            place =
                (s_found[at].has &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = s_found[at].tree, .other = args->tree, .cap = CLI_CELLS_POOL})))
                    ? at
                    : place;
            at += 1u;
        }
    }
    {
        unsigned int at = 0u;
        while ((place == CLI_TRUST_TREES) && (at < CLI_TRUST_TREES))
        {
            place = !s_found[at].has ? at : place;
            at += 1u;
        }
    }
    if (place == CLI_TRUST_TREES)
    {
        place = 0u;
        for (unsigned int at = 1u; at < CLI_TRUST_TREES; at += 1u)
        {
            place = (s_found[at].when < s_found[place].when) ? at : place;
        }
    }
    CliTrustFound *const one = &s_found[place];
    memor.cpy(&(MemoriaCfg){.dst = one->tree,
                            .src = args->tree,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->tree, .cap = CLI_CELLS_POOL}) + 1u});
    one->when = cli_clock_milliseconds();
    one->has = 1;
    g_cli_cells->persistent_buf_release(one->code.paths, &g_cli_trust_cells);
    *(&one->code) = (CliTrustCode){NULL, 0ULL, 0u};
    one->code = looked;
    const int copied = EMBED_CALL(cli_code_copy, TrustCodeCtx, .paths = looked.paths, .size = looked.size,
                                  .count = looked.count, .code = code);
    return copied;
}

int cli_trust_block(const CliTrustTreeWordsCfg *args)
{
    int trusted = 0;
    if (EMBED_CALL(cli_trust_tree.answer, CliTrustTreeCfg, .tree = args->tree, .trusted = &trusted) && trusted)
    {
        return 0;
    }
    // a tree whose code the cellblock could not meet is disabled, as one that contains code is
    CliTrustCode code = {NULL, 0ULL, 0u};
    const int blocked =
        !EMBED_CALL(cli_trust_tree_code.own_code, CliTrustTreeCodeCfg, .tree = args->tree, .code = &code) ||
        (code.count != 0u);
    g_cli_cells->persistent_buf_release(code.paths, &g_cli_trust_cells);
    *(&code) = (CliTrustCode){NULL, 0ULL, 0u};
    return blocked;
}

int cli_trust_prettier_block(const CliTrustTreeWordsCfg *args)
{
    if (!cli_trust_tree_words.block(args))
    {
        return 0;
    }
    CliTrustCode code = {NULL, 0ULL, 0u};
    int blocked = !EMBED_CALL(cli_trust_tree_code.own_code, CliTrustTreeCodeCfg, .tree = args->tree, .code = &code);
    const char *path = code.paths;
    for (unsigned int at = 0u; !blocked && (at < code.count); at += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        const char *const slash =
            EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = path, .length_value = length, .byte = '/');
        blocked = EMBED_CALL(cli_by_name, TrustCodeCtx, .name = (slash != NULL) ? (slash + 1) : path,
                             .names = s_prettier, .count = 6u);
        path += length + 1u;
    }
    g_cli_cells->persistent_buf_release(code.paths, &g_cli_trust_cells);
    *(&code) = (CliTrustCode){NULL, 0ULL, 0u};
    return blocked;
}

unsigned int cli_trust_git_words(const CliTrustTreeWordsCfg *args)
{
    char folder[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = folder,
                            .src = args->tree,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->tree, .cap = CLI_CELLS_POOL}) + 1u});
    int hook = 0;
    int going = 1;
    while (going)
    {
        char git[CLI_PATH_MAX_LEN];
        VerbaLinea line = {git, sizeof(git), 0ULL};
        mmgr_line_put(&line, folder);
        mmgr_line_put(&line, "/.git");
        if ((mmgr_line_finish(&line) != 0ULL) &&
            (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = git) ||
             EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = git)))
        {
            char *const config = EMBED_CALL(cli_read_in, TrustCodeCtx, .folder = folder, .name = ".git/config");
            hook = (config != NULL) && EMBED_CALL(cli_trust_table.names_a_hook, CliTrustCfg, .config = config);
            g_cli_cells->persistent_buf_release(config, &g_cli_trust_cells);
            going = 0;
            continue;
        }
        char above[CLI_PATH_MAX_LEN];
        if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = folder, .out = above))
        {
            going = 0;
            continue;
        }
        memor.cpy(&(MemoriaCfg){.dst = folder,
                                .src = above,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = above, .cap = CLI_CELLS_POOL}) + 1u});
    }
    int trusted = 0;
    if (!hook ||
        (EMBED_CALL(cli_trust_tree.answer, CliTrustTreeCfg, .tree = args->tree, .trusted = &trusted) && trusted))
    {
        return 0u;
    }
    args->words[0] = "-c";
    args->words[1] = "core.fsmonitor=false";
    return 2u;
}
