// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// trust_code.c: the files of a tree that run as code as its tools read it, kept a while, and the words git is
// started with where a tree's hook is held back
#include "../editor/files.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "trust_internal.h"

// the configurations Prettier and ESLint run as code
static const char *const s_prettier[6] = {".prettierrc.js",     ".prettierrc.cjs",     ".prettierrc.mjs",
                                          "prettier.config.js", "prettier.config.cjs", "prettier.config.mjs"};

static const char *const s_eslint[5] = {".eslintrc.js", ".eslintrc.cjs", "eslint.config.js", "eslint.config.cjs",
                                        "eslint.config.mjs"};

// how long a tree's code found is kept before the tree is looked through again, in milliseconds
#define CLI_TRUST_FOUND_FOR 10000ull

// the most trees whose code found is kept, and the bytes the paths of a tree's code are begun to be written in,
// doubling as they fill
#define CLI_TRUST_TREES 8u
#define CLI_TRUST_PATHS_FIRST 1024u

CliCellsTally g_cli_trust_cells = CLI_CELLS_TALLY("cli_trust");

// The code found in a tree looked through, and when
typedef struct
{
    char tree[CLI_PATH_LONGEST];
    unsigned long long when;
    CliTrustCode code;
    int has;
} CliTrustFound;

static CliTrustFound s_found[CLI_TRUST_TREES];
static CliLock s_found_lock = CLI_LOCK_INIT;

void cli_trust_code_given(CliTrustCode *code)
{
    cli_cell_given_by(&g_cli_trust_cells, code->paths);
    *code = (CliTrustCode){NULL, 0ull, 0u};
}

void cli_trust_forget(void)
{
    cli_lock_take(&s_found_lock);
    for (unsigned int at = 0u; at < CLI_TRUST_TREES; at += 1u)
    {
        s_found[at].has = 0;
        cli_trust_code_given(&s_found[at].code);
    }
    cli_lock_give(&s_found_lock);
}

// `count` paths, `size` bytes of them at `paths`, copied into `code`, a cell of their own where there are any: 1, or 0
// with none into `code` where the cellblock could not meet them
static int cli_code_copied(const char *paths, unsigned long long size, unsigned int count, CliTrustCode *code)
{
    *code = (CliTrustCode){NULL, 0ull, 0u};
    if (count == 0u)
    {
        return 1;
    }
    code->paths = cli_cell_taken_by(&g_cli_trust_cells, size, "the files a tree runs as code");
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
static int cli_same_word(const char *text, unsigned long long length, const char *word)
{
    if (length != cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL}))
    {
        return 0;
    }
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        const char one = ((text[at] >= 'A') && (text[at] <= 'Z')) ? (char)(text[at] + ('a' - 'A')) : text[at];
        if (one != word[at])
        {
            return 0;
        }
    }
    return 1;
}

// the bytes from `*from` to `*to` with the whitespace at either end left off
static void cli_trimmed(const char **from, const char **to)
{
    while ((*from < *to) && ((**from == ' ') || ((**from >= '\t') && (**from <= '\r'))))
    {
        *from += 1;
    }
    while ((*to > *from) && (((*to)[-1] == ' ') || (((*to)[-1] >= '\t') && ((*to)[-1] <= '\r'))))
    {
        *to -= 1;
    }
}

int cli_trust_names_a_hook(const char *config)
{
    static const char *const s_none[9] = {"", "true", "false", "yes", "no", "on", "off", "1", "0"};
    int in_core = 0;
    const char *line = config;
    while (line[0] != '\0')
    {
        const char *end = line;
        while ((end[0] != '\0') && (end[0] != '\n'))
        {
            end += 1;
        }
        const char *const next = (end[0] != '\0') ? (end + 1) : end;
        const char *from = line;
        const char *to = end;
        cli_trimmed(&from, &to);
        line = next;
        if ((from < to) && (from[0] == '['))
        {
            while ((from < to) && (from[0] == '['))
            {
                from += 1;
            }
            while ((to > from) && (to[-1] == ']'))
            {
                to -= 1;
            }
            cli_trimmed(&from, &to);
            in_core = cli_same_word(from, (unsigned long long)(to - from), "core");
            continue;
        }
        const char *equals = from;
        while ((equals < to) && (equals[0] != '='))
        {
            equals += 1;
        }
        if (!in_core || (equals == to))
        {
            continue;
        }
        const char *name_to = equals;
        const char *name_from = from;
        cli_trimmed(&name_from, &name_to);
        const char *value_from = equals + 1;
        const char *value_to = to;
        cli_trimmed(&value_from, &value_to);
        while ((value_from < value_to) && (value_from[0] == '"'))
        {
            value_from += 1;
        }
        while ((value_to > value_from) && (value_to[-1] == '"'))
        {
            value_to -= 1;
        }
        if (!cli_same_word(name_from, (unsigned long long)(name_to - name_from), "fsmonitor"))
        {
            continue;
        }
        int none = 0;
        for (unsigned int at = 0u; !none && (at < 9u); at += 1u)
        {
            none = cli_same_word(value_from, (unsigned long long)(value_to - value_from), s_none[at]);
        }
        if (!none)
        {
            return 1;
        }
    }
    return 0;
}

// the file `name` under `folder` read whole, a zero after it, into a cell of its size given back with
// cli_cell_given_by: the cell, or NULL where it could not be read or the cellblock could not meet it
static char *cli_read_in(const char *folder, const char *name)
{
    char path[CLI_PATH_LONGEST];
    CliLine line = {path, sizeof(path), 0ull};
    cli_line_text(&line, folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = (cli_line_closed(&line) != 0ull) ? cli_file_held(path, &size) : NULL;
    char *const text =
        (bytes != NULL) ? cli_cell_taken_by(&g_cli_trust_cells, size + 1ull, "a file a tree's tools read") : NULL;
    if (text != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = text, .src = bytes, .bytes = (size_t)size});
        text[size] = '\0';
    }
    cli_file_let_go(bytes, size);
    return text;
}

// whether `text` holds a line that is `proc-macro=true` with its spaces left out
static int cli_proc_macro(const char *text)
{
    const char *at = text;
    while (at[0] != '\0')
    {
        // the line's letters but its spaces, and its `\r` before the `\n` left off
        char kept[32];
        unsigned int count = 0u;
        int longer = 0;
        for (; (at[0] != '\0') && (at[0] != '\n'); at += 1)
        {
            if (at[0] == ' ')
            {
                continue;
            }
            if (count < (sizeof(kept) - 1u))
            {
                kept[count] = at[0];
                count += 1u;
            }
            else
            {
                longer = 1;
            }
        }
        if ((at[0] == '\n') && (count > 0u) && (kept[count - 1u] == '\r') && !longer)
        {
            count -= 1u;
        }
        kept[count] = '\0';
        if (!longer && (cellul.eq(&(CatenaFinitaCfg){.src = kept, .other = "proc-macro=true", .cap = CLI_CELLS_POOL})))
        {
            return 1;
        }
        at += (at[0] == '\n') ? 1 : 0;
    }
    return 0;
}

// whether `name` is one of `count` names
static int cli_named(const char *name, const char *const *names, unsigned int count)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = names[at], .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

int cli_trust_looked_for(const char *tree, CliTrustCode *code)
{
    CliLine paths = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    (void)cli_line_growing(&paths, CLI_TRUST_PATHS_FIRST, "the files a tree runs as code, as they are found");
    const CliFiles *const files = cli_files_held(tree);
    unsigned int found = 0u;
    for (unsigned int index = 0u; (files != NULL) && (index < files->count); index += 1u)
    {
        const char *const file = cli_files_at(files, index);
        const char *const slash =
            cli_text_last(file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}), '/');
        const char *const name = (slash != NULL) ? (slash + 1) : file;
        int is_code = 0;
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "build.rs", .cap = CLI_CELLS_POOL}))
        {
            char beside[CLI_PATH_LONGEST];
            CliLine line = {beside, sizeof(beside), 0ull};
            cli_line_counted(&line, file, (unsigned long long)(name - file));
            cli_line_text(&line, "Cargo.toml");
            is_code = (cli_line_closed(&line) != 0ull) && cli_files_holds(files, beside);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Cargo.toml", .cap = CLI_CELLS_POOL}))
        {
            char *const text = cli_read_in(tree, file);
            is_code = (text != NULL) && cli_proc_macro(text);
            cli_cell_given_by(&g_cli_trust_cells, text);
        }
        else
        {
            is_code = cli_named(name, s_prettier, 6u) || cli_named(name, s_eslint, 5u);
        }
        if (is_code)
        {
            cli_line_counted(&paths, file, cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}) + 1u);
            found += 1u;
        }
    }
    cli_files_let(files);
    char *const config = cli_read_in(tree, ".git/config");
    if ((config != NULL) && cli_trust_names_a_hook(config))
    {
        cli_line_counted(&paths, ".git/config", sizeof(".git/config"));
        found += 1u;
    }
    cli_cell_given_by(&g_cli_trust_cells, config);
    const int done = cli_line_fits(&paths) && cli_code_copied(paths.out, paths.at, found, code);
    cli_cell_given(paths.out);
    return done;
}

int cli_trust_own_code(const char *tree, CliTrustCode *code)
{
    const unsigned long long now = cli_clock_milliseconds();
    cli_lock_take(&s_found_lock);
    for (unsigned int at = 0u; at < CLI_TRUST_TREES; at += 1u)
    {
        CliTrustFound *const one = &s_found[at];
        if (one->has && (cellul.eq(&(CatenaFinitaCfg){.src = one->tree, .other = tree, .cap = CLI_CELLS_POOL})) &&
            ((now - one->when) < CLI_TRUST_FOUND_FOR))
        {
            const int copied = cli_code_copied(one->code.paths, one->code.size, one->code.count, code);
            cli_lock_give(&s_found_lock);
            return copied;
        }
    }
    cli_lock_give(&s_found_lock);
    // a tree whose code the cellblock could not meet is not kept, so it is looked through again when next asked of
    CliTrustCode looked = {NULL, 0ull, 0u};
    if (!cli_trust_looked_for(tree, &looked))
    {
        *code = (CliTrustCode){NULL, 0ull, 0u};
        return 0;
    }
    cli_lock_take(&s_found_lock);
    // the tree's place, else a free one, else the oldest
    unsigned int place = CLI_TRUST_TREES;
    for (unsigned int at = 0u; (place == CLI_TRUST_TREES) && (at < CLI_TRUST_TREES); at += 1u)
    {
        place = (s_found[at].has &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = s_found[at].tree, .other = tree, .cap = CLI_CELLS_POOL})))
                    ? at
                    : place;
    }
    for (unsigned int at = 0u; (place == CLI_TRUST_TREES) && (at < CLI_TRUST_TREES); at += 1u)
    {
        place = !s_found[at].has ? at : place;
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
                            .src = tree,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = tree, .cap = CLI_CELLS_POOL}) + 1u});
    one->when = cli_clock_milliseconds();
    one->has = 1;
    cli_trust_code_given(&one->code);
    one->code = looked;
    const int copied = cli_code_copied(looked.paths, looked.size, looked.count, code);
    cli_lock_give(&s_found_lock);
    return copied;
}

int cli_trust_held_back(const char *tree)
{
    int trusted = 0;
    if (cli_trust_answer(tree, &trusted) && trusted)
    {
        return 0;
    }
    // a tree whose code the cellblock could not meet is held back, as one that holds code is
    CliTrustCode code = {NULL, 0ull, 0u};
    const int held = !cli_trust_own_code(tree, &code) || (code.count != 0u);
    cli_trust_code_given(&code);
    return held;
}

int cli_trust_prettier_held_back(const char *tree)
{
    if (!cli_trust_held_back(tree))
    {
        return 0;
    }
    CliTrustCode code = {NULL, 0ull, 0u};
    int held = !cli_trust_own_code(tree, &code);
    const char *path = code.paths;
    for (unsigned int at = 0u; !held && (at < code.count); at += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        const char *const slash = cli_text_last(path, length, '/');
        held = cli_named((slash != NULL) ? (slash + 1) : path, s_prettier, 6u);
        path += length + 1u;
    }
    cli_trust_code_given(&code);
    return held;
}

unsigned int cli_trust_git_words(const char *tree, const char **words)
{
    char folder[CLI_PATH_LONGEST];
    memor.cpy(&(MemoriaCfg){
        .dst = folder, .src = tree, .bytes = cellul.len(&(CatenaFinitaCfg){.src = tree, .cap = CLI_CELLS_POOL}) + 1u});
    int hook = 0;
    for (;;)
    {
        char git[CLI_PATH_LONGEST];
        CliLine line = {git, sizeof(git), 0ull};
        cli_line_text(&line, folder);
        cli_line_text(&line, "/.git");
        if ((cli_line_closed(&line) != 0ull) && (query_is_file(git) || query_is_folder(git)))
        {
            char *const config = cli_read_in(folder, ".git/config");
            hook = (config != NULL) && cli_trust_names_a_hook(config);
            cli_cell_given_by(&g_cli_trust_cells, config);
            break;
        }
        char above[CLI_PATH_LONGEST];
        if (!cli_path_parent(folder, above))
        {
            break;
        }
        memor.cpy(&(MemoriaCfg){.dst = folder,
                                .src = above,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = above, .cap = CLI_CELLS_POOL}) + 1u});
    }
    int trusted = 0;
    if (!hook || (cli_trust_answer(tree, &trusted) && trusted))
    {
        return 0u;
    }
    words[0] = "-c";
    words[1] = "core.fsmonitor=false";
    return 2u;
}

void cli_trust_told(const char *tree, CliLine *out)
{
    cli_trust_forget();
    int trusted = 0;
    const int answered = cli_trust_answer(tree, &trusted);
    cli_line_text(out, "{\"answer\":");
    cli_line_text(out, !answered ? "null" : (trusted ? "true" : "false"));
    cli_line_text(out, ",\"code\":[");
    CliTrustCode code = {NULL, 0ull, 0u};
    (void)cli_trust_own_code(tree, &code);
    const char *path = code.paths;
    for (unsigned int at = 0u; at < code.count; at += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        cli_line_text(out, (at != 0u) ? "," : "");
        cli_json_quoted(out, path, length);
        path += length + 1u;
    }
    cli_trust_code_given(&code);
    cli_line_text(out, "],\"tree\":");
    cli_json_quoted(out, tree, cellul.len(&(CatenaFinitaCfg){.src = tree, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"above\":");
    char above[CLI_PATH_LONGEST];
    char beyond[CLI_PATH_LONGEST];
    if (cli_path_parent(tree, above) && cli_path_parent(above, beyond))
    {
        cli_json_quoted(out, above, cellul.len(&(CatenaFinitaCfg){.src = above, .cap = CLI_CELLS_POOL}));
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_character(out, '}');
}
