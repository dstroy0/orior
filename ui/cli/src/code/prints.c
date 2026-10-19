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

// the most prints found and the bytes of their paths and texts
#define CLI_PRINTS_MOST 65536u
#define CLI_PRINTS_TEXT (1u << 24u)

// the starts of a line that prints, by the extension of its file, the list ended by NULL
static const char *const *cli_starts(const char *extension)
{
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

int cli_prints_line(const char *path, const char *text, unsigned long long length)
{
    // the extension after the path's last dot, in lower case
    char extension[CLI_NAME_LONGEST];
    const char *const dot =
        cli_text_last(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), '.');
    unsigned int kept = 0u;
    for (const char *at = (dot != NULL) ? (dot + 1) : ""; (at[0] != '\0') && (kept < (CLI_NAME_LONGEST - 1u)); at += 1)
    {
        extension[kept] = ((at[0] >= 'A') && (at[0] <= 'Z')) ? (char)(at[0] + 32) : at[0];
        kept += 1u;
    }
    extension[kept] = '\0';
    // the line with the whitespace at its start left off
    unsigned long long from = 0ull;
    while (from < length)
    {
        unsigned int letter = 0u;
        const unsigned long long step = cli_lossy_letter((const unsigned char *)text, length, from, &letter);
        if (!cli_fuzzy_white(letter))
        {
            break;
        }
        from += step;
    }
    for (const char *const *start = cli_starts(extension); start[0] != NULL; start += 1)
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = start[0], .cap = CLI_CELLS_POOL});
        if (((length - from) < size) ||
            (memor.cmp(&(MemoriaCfg){.src = text + from, .other = start[0], .bytes = size}) != 0))
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
        if (joined || ((rest < length) && ((text[rest] == ' ') || (text[rest] == '('))))
        {
            return 1;
        }
    }
    return 0;
}

// A print found: its path and text in the texts, its line from 0, and its text's bytes
typedef struct
{
    unsigned int path;
    unsigned int text;
    unsigned long long size;
    unsigned int line;
} CliPrint;

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
static unsigned int cli_kept(const char *from, unsigned long long length)
{
    if ((s_prints->used + length + 1ull) > CLI_PRINTS_TEXT)
    {
        return CLI_PRINTS_TEXT;
    }
    const unsigned int at = s_prints->used;
    memor.cpy(&(MemoriaCfg){.dst = s_prints->text + at, .src = from, .bytes = length});
    s_prints->text[at + length] = '\0';
    // the texts are below CLI_PRINTS_TEXT bytes, which an unsigned int holds
    s_prints->used += (unsigned int)length + 1u;
    return at;
}

// a print put among those found, its path `at` before `path`
static void cli_print_put(const char *at, const char *path, unsigned long long path_length, unsigned int line,
                          const char *text, unsigned long long size)
{
    char whole[CLI_PATH_LONGEST];
    CliLine named = {whole, sizeof(whole), 0ull};
    cli_line_text(&named, at);
    cli_line_counted(&named, path, path_length);
    if ((cli_line_closed(&named) == 0ull) || (s_prints->count >= CLI_PRINTS_MOST))
    {
        return;
    }
    const unsigned int kept_path = cli_kept(whole, cellul.len(&(CatenaFinitaCfg){.src = whole, .cap = CLI_CELLS_POOL}));
    const unsigned int kept_text = cli_kept(text, size);
    if ((kept_path == CLI_PRINTS_TEXT) || (kept_text == CLI_PRINTS_TEXT))
    {
        return;
    }
    s_prints->item[s_prints->count] = (CliPrint){kept_path, kept_text, size, line};
    s_prints->count += 1u;
}

// the lines a unified diff with no lines of context puts in that print, each by its path, its line in the file as it
// stands, from 0, and its text, put among those found with `at` before their paths
static void cli_put_in(const char *at, const char *diff, unsigned long long size)
{
    char path[CLI_PATH_LONGEST] = "";
    unsigned int line = 0u;
    unsigned long long from = 0ull;
    while (from < size)
    {
        const char *const newline = memor.chr(&(MemoriaCfg){.src = diff + from, .val = '\n', .bytes = size - from});
        // a newline found lies in the diff, at or past `from`
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - diff) : size;
        unsigned long long length = end - from;
        if ((newline != NULL) && (length > 0ull) && (diff[end - 1ull] == '\r'))
        {
            length -= 1ull;
        }
        const char *const row = diff + from;
        from = end + 1ull;
        if ((length >= 4ull) && (memor.cmp(&(MemoriaCfg){.src = row, .other = "+++ ", .bytes = 4u}) == 0))
        {
            const unsigned long long skip =
                ((length >= 6ull) && (memor.cmp(&(MemoriaCfg){.src = row + 4, .other = "b/", .bytes = 2u}) == 0))
                    ? 6ull
                    : 4ull;
            const unsigned long long kept = ((length - skip) < (CLI_PATH_LONGEST - 1ull)) ? (length - skip) : 0ull;
            memor.cpy(&(MemoriaCfg){.dst = path, .src = row + skip, .bytes = kept});
            path[kept] = '\0';
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
                while ((digit < length) && (row[digit] >= '0') && (row[digit] <= '9') && (value <= 0xffffffffull))
                {
                    value = (value * 10ull) + (unsigned long long)(row[digit] - '0');
                    digit += 1ull;
                }
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
                cli_prints_line(path, row + 1, length - 1ull))
            {
                cli_print_put(at, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), line,
                              row + 1, length - 1ull);
            }
            line += 1u;
        }
    }
}

// the prints of a file new to git, read whole as UTF-8
static void cli_new_read(const char *root, const char *path)
{
    char full[CLI_PATH_LONGEST];
    cli_full(root, path, full);
    unsigned long long size = 0ull;
    const unsigned char *const bytes = qry_member_file_mapped(full, &size);
    if (bytes == NULL)
    {
        return;
    }
    unsigned long long at = 0ull;
    int text = 1;
    while (text && (at < size))
    {
        unsigned int letter = 0u;
        const unsigned long long step = cli_lossy_letter(bytes, size, at, &letter);
        text = !((step == 1ull) && (bytes[at] >= 0x80u));
        at += step;
    }
    unsigned int line = 0u;
    at = 0ull;
    while (text && (at < size))
    {
        const char *const row = (const char *)bytes + at;
        const unsigned char *const newline =
            memor.chr(&(MemoriaCfg){.src = bytes + at, .val = '\n', .bytes = size - at});
        // a newline found lies in the bytes, at or past `at`
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - bytes) : size;
        unsigned long long length = end - at;
        if ((newline != NULL) && (length > 0ull) && (bytes[end - 1ull] == '\r'))
        {
            length -= 1ull;
        }
        if (cli_prints_line(path, row, length))
        {
            cli_print_put("", path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), line, row,
                          length);
        }
        line += 1u;
        at = end + 1ull;
    }
    qry_member_file_unmapped(bytes, size);
}

// whether a print comes before another: by path, then by line
static int cli_print_before(const CliPrint *one, const CliPrint *other)
{
    const int order = cli_text_order(s_prints->text + one->path, s_prints->text + other->path);
    return (order < 0) || ((order == 0) && (one->line < other->line));
}

// the prints found, sorted by path, then line, each kept once, written as `[{"path", "line", "text"}]`
static void cli_prints_written(CliLine *out)
{
    for (unsigned int at = 1u; at < s_prints->count; at += 1u)
    {
        const CliPrint held = s_prints->item[at];
        unsigned int place = at;
        while ((place > 0u) && cli_print_before(&held, &s_prints->item[place - 1u]))
        {
            s_prints->item[place] = s_prints->item[place - 1u];
            place -= 1u;
        }
        s_prints->item[place] = held;
    }
    cli_line_character(out, '[');
    unsigned int written = 0u;
    for (unsigned int at = 0u; at < s_prints->count; at += 1u)
    {
        const CliPrint *const one = &s_prints->item[at];
        const CliPrint *const last = (at > 0u) ? &s_prints->item[at - 1u] : NULL;
        const int same =
            (last != NULL) && !cli_print_before(last, one) && (last->size == one->size) &&
            (memor.cmp(&(MemoriaCfg){
                 .src = s_prints->text + last->text, .other = s_prints->text + one->text, .bytes = one->size}) == 0);
        if (same)
        {
            continue;
        }
        const char *const path = s_prints->text + one->path;
        cli_line_text(out, (written != 0u) ? ",{\"path\":" : "{\"path\":");
        cli_json_quoted(out, path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
        cli_line_text(out, ",\"line\":");
        cli_line_decimal(out, one->line, 0u);
        cli_line_text(out, ",\"text\":");
        cli_json_quoted_lossy(out, (const unsigned char *)(s_prints->text + one->text), one->size);
        cli_line_character(out, '}');
        written += 1u;
    }
    cli_line_character(out, ']');
}

// the lock taken and the prints' room a cell under it: 1, or 0 with why said and the lock given back
static int prints_held(CliLine *said)
{
    cli_lock_take(&s_lock);
    s_prints = cli_cell_taken(sizeof(*s_prints), "a search for prints");
    if (s_prints == NULL)
    {
        cli_lock_give(&s_lock);
        cli_line_text(said, "a search for prints could not be taken");
        return 0;
    }
    s_prints->count = 0u;
    s_prints->used = 0u;
    return 1;
}

// the prints written into `out`, and the room and the lock given back
static void prints_released(CliLine *out)
{
    cli_prints_written(out);
    cli_cell_given(s_prints);
    s_prints = NULL;
    cli_lock_give(&s_lock);
}

int cli_prints_added(const char *root, CliLine *out, CliLine *said)
{
    if (!prints_held(said))
    {
        return 0;
    }
    cli_git_repositories(root, &s_prints->repositories);
    for (unsigned int index = 0u; index < s_prints->repositories.count; index += 1u)
    {
        const char *const repo = cli_git_text_of(&s_prints->repositories.text, s_prints->repositories.item[index].path);
        char folder[CLI_PATH_LONGEST];
        char at[CLI_PATH_LONGEST];
        cli_full(root, repo, folder);
        CliLine before = {at, sizeof(at), 0ull};
        if (repo[0] != '\0')
        {
            cli_line_text(&before, repo);
            cli_line_character(&before, '/');
        }
        cli_line_closed(&before);
        static const char *const s_words[8] = {"git",        "diff",          "HEAD",       "--unified=0",
                                               "--no-color", "--no-ext-diff", "--relative", NULL};
        CliRan *const ran = cli_ran_taken();
        char why[CLI_NAME_LONGEST];
        CliLine unstarted = {why, sizeof(why), 0ull};
        if (cli_ran(s_words, folder, NULL, 0u, ran, &unstarted))
        {
            cli_put_in(at, ran->out, ran->out_size);
        }
        cli_ran_given(ran);
    }
    cli_git_changed(root, NULL, 0u, 0, &s_prints->changed);
    for (unsigned int index = 0u; index < s_prints->changed.count; index += 1u)
    {
        const CliChanged *const one = &s_prints->changed.item[index];
        const char *const path = cli_git_text_of(&s_prints->changed.text, one->path);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        if ((one->state == 'U') && (length > 0ull) && (path[length - 1ull] != '/'))
        {
            cli_new_read(root, path);
        }
    }
    prints_released(out);
    return 1;
}

int cli_prints_put_in(const char *diff, unsigned long long size, CliLine *out, CliLine *said)
{
    if (!prints_held(said))
    {
        return 0;
    }
    cli_put_in("", diff, size);
    prints_released(out);
    return 1;
}
