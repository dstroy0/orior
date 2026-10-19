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

// the most trees whose code found is kept, and the most bytes of a Cargo.toml read
#define CLI_TRUST_TREES 8u

#define CLI_TRUST_READ (1u << 20u)

// The code found in a tree looked through, and when
typedef struct
{
    char tree[CLI_PATH_LONGEST];
    unsigned long long when;
    char code[CLI_TRUST_CODE][CLI_PATH_LONGEST];
    unsigned int count;
    int has;
} CliTrustFound;

static CliTrustFound s_found[CLI_TRUST_TREES];
static CliLock s_found_lock = CLI_LOCK_INIT;

// a file of the tree read while it is looked through, under its lock
static char s_read[CLI_TRUST_READ];
static CliLock s_looking_lock = CLI_LOCK_INIT;

void cli_trust_forget(void)
{
    cli_lock_take(&s_found_lock);
    for (unsigned int at = 0u; at < CLI_TRUST_TREES; at += 1u)
    {
        s_found[at].has = 0;
    }
    cli_lock_give(&s_found_lock);
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

// the file `name` under `folder` read whole into `s_read`, the looking lock held: 1, or 0 where it could not be read
static int cli_read_in(const char *folder, const char *name)
{
    char path[CLI_PATH_LONGEST];
    CliLine line = {path, sizeof(path), 0ull};
    cli_line_text(&line, folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    if (cli_line_closed(&line) == 0ull)
    {
        return 0;
    }
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_read, sizeof(s_read) - 1u);
    if (size == QRY_MEMBER_UNREAD)
    {
        return 0;
    }
    s_read[size] = '\0';
    return 1;
}

// whether the text read holds a line that is `proc-macro=true` with its spaces left out
static int cli_proc_macro(void)
{
    const char *at = s_read;
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

unsigned int cli_trust_looked_for(const char *tree, char (*code)[CLI_PATH_LONGEST], unsigned int room)
{
    cli_lock_take(&s_looking_lock);
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
            is_code = cli_read_in(tree, file) && cli_proc_macro();
        }
        else
        {
            is_code = cli_named(name, s_prettier, 6u) || cli_named(name, s_eslint, 5u);
        }
        if (is_code && (found < room))
        {
            memor.cpy(&(MemoriaCfg){.dst = code[found],
                                    .src = file,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}) + 1u});
            found += 1u;
        }
    }
    cli_files_let(files);
    if (cli_read_in(tree, ".git/config") && cli_trust_names_a_hook(s_read) && (found < room))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = code[found],
                          .src = ".git/config",
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = ".git/config", .cap = CLI_CELLS_POOL}) + 1u});
        found += 1u;
    }
    cli_lock_give(&s_looking_lock);
    return found;
}

unsigned int cli_trust_own_code(const char *tree, char (*code)[CLI_PATH_LONGEST], unsigned int room)
{
    const unsigned long long now = cli_clock_milliseconds();
    cli_lock_take(&s_found_lock);
    for (unsigned int at = 0u; at < CLI_TRUST_TREES; at += 1u)
    {
        CliTrustFound *const one = &s_found[at];
        if (one->has && (cellul.eq(&(CatenaFinitaCfg){.src = one->tree, .other = tree, .cap = CLI_CELLS_POOL})) &&
            ((now - one->when) < CLI_TRUST_FOUND_FOR))
        {
            unsigned int count = 0u;
            for (; (count < one->count) && (count < room); count += 1u)
            {
                memor.cpy(&(MemoriaCfg){
                    .dst = code[count],
                    .src = one->code[count],
                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = one->code[count], .cap = CLI_CELLS_POOL}) + 1u});
            }
            cli_lock_give(&s_found_lock);
            return count;
        }
    }
    cli_lock_give(&s_found_lock);
    static char s_code[CLI_TRUST_CODE][CLI_PATH_LONGEST];
    static CliLock s_code_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_code_lock);
    const unsigned int found = cli_trust_looked_for(tree, s_code, CLI_TRUST_CODE);
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
    one->count = found;
    one->has = 1;
    unsigned int count = 0u;
    for (unsigned int at = 0u; at < found; at += 1u)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = one->code[at],
                          .src = s_code[at],
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_code[at], .cap = CLI_CELLS_POOL}) + 1u});
        if (count < room)
        {
            memor.cpy(
                &(MemoriaCfg){.dst = code[count],
                              .src = s_code[at],
                              .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_code[at], .cap = CLI_CELLS_POOL}) + 1u});
            count += 1u;
        }
    }
    cli_lock_give(&s_found_lock);
    cli_lock_give(&s_code_lock);
    return count;
}

int cli_trust_held_back(const char *tree)
{
    static char s_code[CLI_TRUST_CODE][CLI_PATH_LONGEST];
    static CliLock s_lock = CLI_LOCK_INIT;
    int trusted = 0;
    if (cli_trust_answer(tree, &trusted) && trusted)
    {
        return 0;
    }
    cli_lock_take(&s_lock);
    const int held = cli_trust_own_code(tree, s_code, CLI_TRUST_CODE) != 0u;
    cli_lock_give(&s_lock);
    return held;
}

int cli_trust_prettier_held_back(const char *tree)
{
    static char s_code[CLI_TRUST_CODE][CLI_PATH_LONGEST];
    static CliLock s_lock = CLI_LOCK_INIT;
    if (!cli_trust_held_back(tree))
    {
        return 0;
    }
    cli_lock_take(&s_lock);
    const unsigned int count = cli_trust_own_code(tree, s_code, CLI_TRUST_CODE);
    int held = 0;
    for (unsigned int at = 0u; !held && (at < count); at += 1u)
    {
        const char *const slash =
            cli_text_last(s_code[at], cellul.len(&(CatenaFinitaCfg){.src = s_code[at], .cap = CLI_CELLS_POOL}), '/');
        held = cli_named((slash != NULL) ? (slash + 1) : s_code[at], s_prettier, 6u);
    }
    cli_lock_give(&s_lock);
    return held;
}

unsigned int cli_trust_git_words(const char *tree, const char **words)
{
    static char s_config[CLI_TRUST_BYTES];
    static CliLock s_lock = CLI_LOCK_INIT;
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
            cli_line_text(&line, "/config");
            cli_lock_take(&s_lock);
            const unsigned long long size =
                (cli_line_closed(&line) != 0ull)
                    ? qry_member_file_read(git, (unsigned char *)s_config, sizeof(s_config) - 1u)
                    : QRY_MEMBER_UNREAD;
            if (size != QRY_MEMBER_UNREAD)
            {
                s_config[size] = '\0';
                hook = cli_trust_names_a_hook(s_config);
            }
            cli_lock_give(&s_lock);
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
    static char s_code[CLI_TRUST_CODE][CLI_PATH_LONGEST];
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_trust_forget();
    int trusted = 0;
    const int answered = cli_trust_answer(tree, &trusted);
    cli_line_text(out, "{\"answer\":");
    cli_line_text(out, !answered ? "null" : (trusted ? "true" : "false"));
    cli_line_text(out, ",\"code\":[");
    cli_lock_take(&s_lock);
    const unsigned int count = cli_trust_own_code(tree, s_code, CLI_TRUST_CODE);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(out, (at != 0u) ? "," : "");
        cli_json_quoted(out, s_code[at], cellul.len(&(CatenaFinitaCfg){.src = s_code[at], .cap = CLI_CELLS_POOL}));
    }
    cli_lock_give(&s_lock);
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
