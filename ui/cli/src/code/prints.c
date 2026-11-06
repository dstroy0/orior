// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// prints.c: the lines a change added that print as a print for a task does, read from git's diff of each repository
// of the tree and from the files new to git
#include "prints.h"
#include "../command_line/pool.h"
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
#define CLI_PRINTS_MAX 65536u
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
        if (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = s_by[at].extension, .cap = CLI_POOL_BYTES}))
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
        .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}), .byte = '.');
    (void)EMBED_CALL(cli_text_out_capacity.lowered_into, CliTextOutCapacityCfg, .out_capacity = extension,
                     .capacity = sizeof(extension), .text = (dot != NULL) ? (dot + 1) : "");
    // the line with the whitespace at its start left off
    unsigned long long from = 0ULL;
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
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = start[0], .cap = CLI_POOL_BYTES});
        // a start of no bytes has no last byte to read
        if ((size == 0ULL) || ((args->length - from) < size) ||
            (memor.cmp(&(MemoriaCfg){.src = args->text + from, .other = start[0], .bytes = size}) != 0))
        {
            continue;
        }
        const char last = start[0][size - 1ULL];
        const int joined =
            (last == '(') || (last == '<') || (last == ' ') ||
            ((size >= 6ULL) &&
             ((memor.cmp(&(MemoriaCfg){.src = start[0] + size - 6ULL, .other = "stderr", .bytes = 6u}) == 0) ||
              (memor.cmp(&(MemoriaCfg){.src = start[0] + size - 6ULL, .other = "stdout", .bytes = 6u}) == 0)));
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
    CliPrint item[CLI_PRINTS_MAX];
    unsigned int count;
    char text[CLI_PRINTS_TEXT];
    unsigned int used;
    CliRepositories repositories;
    CliChangedList changed;
} CliPrints;
