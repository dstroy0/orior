// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// prints.c: the lines a change added that print as a print for a task does, read from git's diff of each repository
// of the tree and from the files new to git
#include "prints.h"
#include "../command_line/cells.h"
#include "../editor/files_internal.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "git.h"

// A print found: its path and text in the texts, its line from 0, and its text's bytes
typedef struct
{
    unsigned int path;
    unsigned int text;
    unsigned long long size;
    unsigned int line;
} CliPrint;
// the most prints found and the bytes of their paths and texts
#define CLI_PRINTS_MOST 65536u
#define CLI_PRINTS_TEXT (1u << 24u)

typedef struct
{
    const char *extension;
    const char *from;
    unsigned long long length;
    const char *at;
    const char *path;
    unsigned long long path_length;
    unsigned int line;
    const char *text;
    unsigned long long size;
    const char *diff;
    const char *root;
    const CliPrint *one;
    const CliPrint *other;
    VerbaLinea *out;
    VerbaLinea *err_msg;
} PrintsCtx;
// the starts of a line that prints, by the extension of its file, the list ended by NULL
EMBED_INLINE const char *const *cli_starts(const PrintsCtx *args)
{
    const char *extension = args->extension;
    static const char *const s_py[] = {"print(", "pprint(", "pprint.pprint(", NULL};
    static const char *const s_js[] = {"console.log(",  "console.debug(", "console.info(",
                                       "console.warn(", "console.error(", "console.trace(",
                                       "console.dir(",  "console.table(", NULL};
    static const char *const s_rs[] = {"dbg!(", "println!(", "eprintln!(", "print!(", "eprint!(", NULL};
    static const char *const s_c[] = {"printf(",      "fprintf(stderr", "fprintf(stdout", "puts(", "std::cout <<",
                                      "std::cerr <<", "cout <<",        "cerr <<",        NULL};
    static const char *const s_go[] = {"fmt.Println(", "fmt.Printf(", "fmt.Print(", "log.Println(",
                                       "log.Printf(",  "println(",    NULL};
    static const char *const s_java[] = {
        "System.out.println(", "System.out.print(", "System.err.println(", "println(", "print(", NULL};
    static const char *const s_rb[] = {"puts ", "puts(", "p ", "pp ", "print ", NULL};
    static const char *const s_php[] = {"var_dump(", "print_r(", "echo ", "error_log(", NULL};
    static const char *const s_lua[] = {"print(", NULL};
    static const char *const s_r[] = {"print(", "cat(", NULL};
    static const char *const s_m[] = {"disp(", "fprintf(", "println(", "@show ", NULL};
    static const char *const s_swift[] = {"print(", "debugPrint(", NULL};
    static const char *const s_cs[] = {"Console.WriteLine(", "Console.Write(", "Debug.WriteLine(", NULL};
    static const char *const s_none[] = {NULL};
    static const struct
    {
        const char *extension;
        const char *const *starts;
    } s_by[] = {{"py", s_py},       {"pyi", s_py},     {"js", s_js},      {"mjs", s_js},      {"cjs", s_js},
                {"jsx", s_js},      {"ts", s_js},      {"tsx", s_js},     {"mts", s_js},      {"cts", s_js},
                {"vue", s_js},      {"svelte", s_js},  {"rs", s_rs},      {"c", s_c},         {"h", s_c},
                {"cc", s_c},        {"cpp", s_c},      {"cxx", s_c},      {"hpp", s_c},       {"hh", s_c},
                {"hxx", s_c},       {"cu", s_c},       {"cuh", s_c},      {"go", s_go},       {"java", s_java},
                {"kt", s_java},     {"kts", s_java},   {"scala", s_java}, {"groovy", s_java}, {"rb", s_rb},
                {"php", s_php},     {"lua", s_lua},    {"r", s_r},        {"m", s_m},         {"jl", s_m},
                {"swift", s_swift}, {"dart", s_swift}, {"cs", s_cs}};
    for (unsigned int at = 0u; at < (sizeof(s_by) / sizeof(s_by[0])); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = s_by[at].extension, .cap = CLI_CELLS_POOL}))
        {
            return s_by[at].starts;
        }
    }
    return s_none;
}

int cli_prints_line(const CliPrintsPathCfg *args)
{
    // the extension after the path's last dot, in lower case
    char extension[CLI_NAME_MAX_LEN];
    const char *const dot = EMBED_CALL(
        cli_text_byte.last, CliTextByteCfg, .text = args->path,
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}), .byte = '.');
    (void)EMBED_CALL(cli_text_out_room.lowered_into, CliTextOutRoomCfg, .out_room = extension,
                     .room = sizeof(extension), .text = (dot != NULL) ? (dot + 1) : "");
    // the line with the whitespace at its start left off
    unsigned long long from = 0ull;
    while (from < args->length)
    {
        unsigned int letter = 0u;
        const unsigned long long step = EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                                   .text_bytes = (const unsigned char *)args->text,
                                                   .length = args->length, .at = from, .letter = &letter);
        if (!EMBED_CALL(cli_files_letter_internal.fuzzy_white, CliFilesLetterCtx, .letter = letter))
        {
            break;
        }
        from += step;
    }
    for (const char *const *start = EMBED_CALL(cli_starts, PrintsCtx, .extension = extension); start[0] != NULL;
         start += 1)
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = start[0], .cap = CLI_CELLS_POOL});
        // a start of no bytes has no last byte to read
        if ((size == 0ull) || ((args->length - from) < size) ||
            (memor.cmp(&(MemoriaCfg){.src = args->text + from, .other = start[0], .bytes = size}) != 0))
        {
            continue;
        }
        const char last = start[0][size - 1ull];
        const int joined =
            (last == '(') || (last == '<') || (last == ' ') ||
            ((size >= 6ull) &&
             ((memor.cmp(&(MemoriaCfg){.src = start[0] + size - 6ull, .other = "stderr", .bytes = 6u}) == 0) ||
              (memor.cmp(&(MemoriaCfg){.src = start[0] + size - 6ull, .other = "stdout", .bytes = 6u}) == 0)));
        const unsigned long long rest = from + size;
        if (joined || ((rest < args->length) && ((args->text[rest] == ' ') || (args->text[rest] == '('))))
        {
            return 1;
        }
    }
    return 0;
}

// The prints found and their texts, the repositories and the files that differ from the last commit
typedef struct
{
    CliPrint item[CLI_PRINTS_MOST];
    unsigned int count;
    char text[CLI_PRINTS_TEXT];
    unsigned int used;
    CliRepositories repositories;
    CliChangedList changed;
} CliPrints;

static CliPrints *s_prints;
static CliLock s_lock = CLI_LOCK_INIT;

// `length` bytes put in the texts, a 0 after them: their place, or the texts' end where they do not fit
EMBED_INLINE unsigned int cli_text_store(const PrintsCtx *args)
{
    if ((s_prints->used + args->length + 1ull) > CLI_PRINTS_TEXT)
    {
        return CLI_PRINTS_TEXT;
    }
    const unsigned int at = s_prints->used;
    memor.cpy(&(MemoriaCfg){.dst = s_prints->text + at, .src = args->from, .bytes = args->length});
    s_prints->text[at + args->length] = '\0';
    // the texts are below CLI_PRINTS_TEXT bytes, which an unsigned int stores
    s_prints->used += (unsigned int)args->length + 1u;
    return at;
}

// a print put among those found, its path `at` before `path`
EMBED_INLINE void cli_print_put(const PrintsCtx *args)
{
    char whole[CLI_PATH_MAX_LEN];
    VerbaLinea named = {whole, sizeof(whole), 0ull};
    mmgr_line_put(&named, args->at);
    mmgr_line_put_n(&named, args->path, args->path_length);
    if ((mmgr_line_finish(&named) == 0ull) || (s_prints->count >= CLI_PRINTS_MOST))
    {
        return;
    }
    const unsigned int stored_path =
        EMBED_CALL(cli_text_store, PrintsCtx, .from = whole,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = whole, .cap = CLI_CELLS_POOL}));
    const unsigned int stored_text = EMBED_CALL(cli_text_store, PrintsCtx, .from = args->text, .length = args->size);
    if ((stored_path == CLI_PRINTS_TEXT) || (stored_text == CLI_PRINTS_TEXT))
    {
        return;
    }
    s_prints->item[s_prints->count] = (CliPrint){stored_path, stored_text, args->size, args->line};
    s_prints->count += 1u;
}

// the lines a unified diff with no lines of context puts in that print, each by its path, its line in the file as it
// stands, from 0, and its text, put among those found with `at` before their paths
static void cli_put_in(const PrintsCtx *args)
{
    const char *at = args->at;
    unsigned long long size = args->size;
    char path[CLI_PATH_MAX_LEN] = "";
    unsigned int line = 0u;
    unsigned long long from = 0ull;
    while (from < size)
    {
        const CliTextLine bounds =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = args->diff, .at = from, .size = size);
        const unsigned long long end = bounds.stop;
        unsigned long long length = end - from;
        length -= bounds.stop - bounds.cut;
        const char *const row = args->diff + from;
        from = end + 1ull;
        if ((length >= 4ull) && (memor.cmp(&(MemoriaCfg){.src = row, .other = "+++ ", .bytes = 4u}) == 0))
        {
            const unsigned long long skip =
                ((length >= 6ull) && (memor.cmp(&(MemoriaCfg){.src = row + 4, .other = "b/", .bytes = 2u}) == 0))
                    ? 6ull
                    : 4ull;
            const unsigned long long copied_len =
                ((length - skip) < (CLI_PATH_MAX_LEN - 1ull)) ? (length - skip) : 0ull;
            memor.cpy(&(MemoriaCfg){.dst = path, .src = row + skip, .bytes = copied_len});
            path[copied_len] = '\0';
        }
        else if ((length >= 3ull) && (memor.cmp(&(MemoriaCfg){.src = row, .other = "@@ ", .bytes = 3u}) == 0))
        {
            // the start of the `+` range after the head, 1 where it has none
            unsigned long long start = 1ull;
            for (unsigned long long place = 3ull; place < length; place += 1ull)
            {
                const int begins = (row[place] == '+') && ((row[place - 1ull] == ' ') || (row[place - 1ull] == '\t'));
                if (!begins)
                {
                    continue;
                }
                unsigned long long value = 0ull;
                unsigned long long digit = place + 1ull;
                value = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = row, .at = digit, .size = length,
                                   .end = &digit);
                const int whole = (digit > (place + 1ull)) && (value <= 0xffffffffull) &&
                                  ((digit == length) || (row[digit] == ',') || (row[digit] == ' '));
                start = whole ? value : 1ull;
                break;
            }
            // the line before the start, 0 at the fewest, is below 2^32
            line = (start > 0ull) ? (unsigned int)(start - 1ull) : 0u;
        }
        else if ((length >= 1ull) && (row[0] == '+'))
        {
            if ((!cellul.eq(&(CatenaFinitaCfg){.src = path, .other = "/dev/null", .cap = CLI_CELLS_POOL})) &&
                EMBED_CALL(cli_prints_path.line, CliPrintsPathCfg, .path = path, .text = row + 1,
                           .length = length - 1ull))
            {
                EMBED_CALL(cli_print_put, PrintsCtx, .at = at, .path = path,
                           .path_length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}),
                           .line = line, .text = row + 1, .size = length - 1ull);
            }
            line += 1u;
        }
    }
}

// the prints of a file new to git, read whole as UTF-8
EMBED_INLINE void cli_new_read(const PrintsCtx *args)
{
    const char *root = args->root;
    const char *path = args->path;
    char full[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = path, .joined = full);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = full,
                                                  .size_pointer = &size, .owner = &g_cli_code_cells);
    if (bytes == NULL)
    {
        return;
    }
    unsigned long long at = 0ull;
    int text = 1;
    while (text && (at < size))
    {
        unsigned int letter = 0u;
        const unsigned long long step = EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                                                   .text_bytes = bytes, .length = size, .at = at, .letter = &letter);
        text = !((step == 1ull) && (bytes[at] >= 0x80u));
        at += step;
    }
    unsigned int line = 0u;
    at = 0ull;
    while (text && (at < size))
    {
        const char *const row = (const char *)bytes + at;
        const CliTextLine bounds =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = (const char *)bytes, .at = at, .size = size);
        const unsigned long long end = bounds.stop;
        unsigned long long length = end - at;
        length -= bounds.stop - bounds.cut;
        if (EMBED_CALL(cli_prints_path.line, CliPrintsPathCfg, .path = path, .text = row, .length = length))
        {
            EMBED_CALL(cli_print_put, PrintsCtx, .at = "", .path = path,
                       .path_length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), .line = line,
                       .text = row, .size = length);
        }
        line += 1u;
        at = end + 1ull;
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_code_cells);
}

// whether a print comes before another: by path, then by line
EMBED_INLINE int cli_print_before(const PrintsCtx *args)
{
    const CliPrint *one = args->one;
    const CliPrint *other = args->other;
    const int order = EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = s_prints->text + one->path,
                                 .other_pointer = s_prints->text + other->path);
    return (order < 0) || ((order == 0) && (one->line < other->line));
}

// the prints found, sorted by path, then line, each listed once, written as `[{"path", "line", "text"}]`
EMBED_INLINE void cli_prints_write(const PrintsCtx *args)
{
    VerbaLinea *out = args->out;
    for (unsigned int at = 1u; at < s_prints->count; at += 1u)
    {
        const CliPrint pivot = s_prints->item[at];
        unsigned int place = at;
        while ((place > 0u) &&
               EMBED_CALL(cli_print_before, PrintsCtx, .one = &pivot, .other = &s_prints->item[place - 1u]))
        {
            s_prints->item[place] = s_prints->item[place - 1u];
            place -= 1u;
        }
        s_prints->item[place] = pivot;
    }
    mmgr_line_ch(out, '[');
    unsigned int written = 0u;
    for (unsigned int at = 0u; at < s_prints->count; at += 1u)
    {
        const CliPrint *const one = &s_prints->item[at];
        const CliPrint *const last = (at > 0u) ? &s_prints->item[at - 1u] : NULL;
        const int same =
            (last != NULL) && !EMBED_CALL(cli_print_before, PrintsCtx, .one = last, .other = one) &&
            (last->size == one->size) &&
            (memor.cmp(&(MemoriaCfg){
                 .src = s_prints->text + last->text, .other = s_prints->text + one->text, .bytes = one->size}) == 0);
        if (same)
        {
            continue;
        }
        const char *const path = s_prints->text + one->path;
        mmgr_line_put(out, (written != 0u) ? ",{\"path\":" : "{\"path\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out, .text = path,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
        mmgr_line_put(out, ",\"line\":");
        mmgr_line_decimal(out, one->line, 0u);
        mmgr_line_put(out, ",\"text\":");
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)(s_prints->text + one->text), .length = one->size);
        mmgr_line_ch(out, '}');
        written += 1u;
    }
    mmgr_line_ch(out, ']');
}

// the lock taken and the prints' room a cell under it: 1, or 0 with why given and the lock given back
EMBED_INLINE int prints_lock(const PrintsCtx *args)
{
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    s_prints = g_cli_cells->persistent_buf_alloc(sizeof(*s_prints), &g_cli_code_cells);
    if (s_prints == NULL)
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
        mmgr_line_put(args->err_msg, "a search for prints could not be taken");
        return 0;
    }
    s_prints->count = 0u;
    s_prints->used = 0u;
    s_prints->repositories.text.bytes = NULL;
    return 1;
}

// the prints written into `out`, and the room and the lock given back
EMBED_INLINE void prints_release(const PrintsCtx *args)
{
    cli_prints_write(args);
    EMBED_CALL(cli_git_text.text_free, CliGitTextCfg, .text = &s_prints->repositories.text);
    g_cli_cells->persistent_buf_release(s_prints, &g_cli_code_cells);
    s_prints = NULL;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}

int cli_prints_add(const CliPrintsCfg *args)
{
    const char *root = args->root;
    if (!EMBED_CALL(prints_lock, PrintsCtx, .err_msg = args->err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_git_root_found_repositories.repositories, CliGitRootFoundRepositoriesCfg, .root = root,
               .found_repositories_pointer = &s_prints->repositories);
    for (unsigned int index = 0u; index < s_prints->repositories.count; index += 1u)
    {
        const char *const repo =
            EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg, .text_read = &s_prints->repositories.text,
                       .at = s_prints->repositories.item[index].path);
        char folder[CLI_PATH_MAX_LEN];
        char at[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = repo, .joined = folder);
        VerbaLinea before = {at, sizeof(at), 0ull};
        if (repo[0] != '\0')
        {
            mmgr_line_put(&before, repo);
            mmgr_line_ch(&before, '/');
        }
        mmgr_line_finish(&before);
        static const char *const s_words[8] = {"git",        "diff",          "HEAD",       "--unified=0",
                                               "--no-color", "--no-ext-diff", "--relative", NULL};
        CliRan *const ran = cli_ran_take();
        char why[CLI_NAME_MAX_LEN];
        VerbaLinea unstarted = {why, sizeof(why), 0ull};
        if (EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = s_words, .folder = folder, .settings = NULL,
                       .count = 0u, .ran = ran, .err_msg = &unstarted))
        {
            EMBED_CALL(cli_put_in, PrintsCtx, .at = at, .diff = ran->out, .size = ran->out_size);
        }
        EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    }
    EMBED_CALL(cli_git_root_repos.change, CliGitRootReposCfg, .root = root, .repos = NULL, .count = 0u, .named = 0,
               .found_changed_list_pointer = &s_prints->changed);
    for (unsigned int index = 0u; index < s_prints->changed.count; index += 1u)
    {
        const CliChanged *const one = &s_prints->changed.item[index];
        const char *const path = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                            .text_read = &s_prints->changed.text, .at = one->path);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        if ((one->state == 'U') && (length > 0ull) && (path[length - 1ull] != '/'))
        {
            EMBED_CALL(cli_new_read, PrintsCtx, .root = root, .path = path);
        }
    }
    EMBED_CALL(prints_release, PrintsCtx, .out = args->out);
    return 1;
}

int cli_prints_put_in(const CliPrintsOutCfg *args)
{
    if (!EMBED_CALL(prints_lock, PrintsCtx, .err_msg = args->err_msg))
    {
        return 0;
    }
    EMBED_CALL(cli_put_in, PrintsCtx, .at = "", .diff = args->diff, .size = args->size);
    EMBED_CALL(prints_release, PrintsCtx, .out = args->out);
    return 1;
}
