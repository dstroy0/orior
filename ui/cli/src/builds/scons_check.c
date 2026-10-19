// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scons_check.c: an SConstruct or SConscript checked: each SConscript it reads that no file is at, `#` naming the
// folder of the SConstruct it belongs to; and each name it imports that no SConstruct or SConscript of the tree
// exports, the first 400 of them read. The names SCons gives a file; and what completes the word before the cursor.
// More imports than CLI_SCONS_IMPORTS are not kept, and the names exported past CLI_SCONS_EXPORTS of them or
// CLI_SCONS_EXPORTS_TEXT of their bytes are not held to them. Each call takes its rooms as cells of the cellblock,
// counted to the checks' own mark, and gives them back before it returns
#include "../editor/files.h"
#include "../text/json_tree.h"
#include "scons_internal.h"

#include "../text/text.h"

// the most names a file imports kept, the most names exported held to them and the bytes they take, the most files
// read for what they export, and the most bytes of one read
#define CLI_SCONS_IMPORTS 4096u
#define CLI_SCONS_EXPORTS 65536u
#define CLI_SCONS_EXPORTS_TEXT (1u << 22u)
#define CLI_SCONS_OTHERS 400u
#define CLI_SCONS_OTHER_TEXT (1u << 22u)

// What a check works with: the root, the file's folder and its SConstruct's, the lines, and the findings
typedef struct
{
    const char *text;
    const CliBuildLines *lines;
    CliBuildFindings *found;
    char folder[CLI_PATH_LONGEST];
    char top[CLI_PATH_LONGEST];
} CliSconsCheck;

// A name the file imports: its span in the file, and whether an SConstruct or SConscript of the tree exports it
typedef struct
{
    unsigned long long from;
    unsigned long long to;
    int exported;
} CliSconsImport;

// the check under way; the names the file imports, a cell for the check holding `s_imports_most` of them, NULL while
// they are only counted, and how many there are; the names exported held to them and the bytes those take; and what
// the checks' rooms hold in the cellblock
static CliSconsCheck s_check;
static CliSconsImport *s_import;
static unsigned int s_imports_most;
static unsigned int s_imports;
static unsigned int s_exports;
static unsigned int s_export_used;
static CliCellsTally s_tally = CLI_CELLS_TALLY("cli_scons_rooms");

void cli_scons_cells_said(CliLine *out)
{
    cli_cells_tally_said(&s_tally, out);
}

// `size` bytes of `text` read as a file of SCons, its class bytes a cell: the cell, for the caller to give back, or
// NULL, nothing held, where it is larger than a file read, the cellblock cannot meet its class bytes or it is not read
static unsigned char *check_read(const char *text, unsigned long long size)
{
    unsigned char *const class_of =
        (size <= CLI_SCONS_TEXT) ? cli_cell_taken_by(&s_tally, size + 1ull, "a file's classes") : NULL;
    if ((class_of != NULL) && !cli_scons_read(text, size, class_of))
    {
        cli_cell_given_by(&s_tally, class_of);
        return NULL;
    }
    return class_of;
}

// `from` copied into `out`, which holds CLI_PATH_LONGEST
static void check_copied(const char *from, char *out)
{
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, from);
    (void)cli_line_closed(&line);
}

// the folder of the SConstruct the file at `path` belongs to: the nearest above it in the tree that holds one, the
// tree's top where none does
static void check_top_folder(const char *root, const char *path, char *out)
{
    static const char *const s_names[3] = {"SConstruct", "Sconstruct", "sconstruct"};
    char folder[CLI_PATH_LONGEST];
    char above[CLI_PATH_LONGEST];
    if (!cli_path_parent(path, folder))
    {
        check_copied(root, folder);
    }
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    for (;;)
    {
        for (unsigned int at = 0u; at < 3u; at += 1u)
        {
            char file[CLI_PATH_LONGEST];
            cli_full(folder, s_names[at], file);
            if (query_is_file(file))
            {
                check_copied(folder, out);
                return;
            }
        }
        const char after = folder[length];
        const int inside =
            cli_text_same(folder, root, length) && ((after == '\0') || (after == '/') || (after == '\\'));
        if ((cellul.eq(&(CatenaFinitaCfg){.src = folder, .other = root, .cap = CLI_CELLS_POOL})) || !inside ||
            !cli_path_parent(folder, above))
        {
            check_copied(root, out);
            return;
        }
        check_copied(above, folder);
    }
}

// an SConscript a call reads checked for a file there
static void check_script(void *context, const char *written, unsigned long long size, unsigned long long from,
                         unsigned long long to)
{
    CliSconsCheck *const check = context;
    if (memor.chr(&(MemoriaCfg){.src = written, .val = '$', .bytes = (size_t)size}) != NULL)
    {
        return;
    }
    char name[CLI_PATH_LONGEST];
    CliLine line = {name, sizeof(name), 0ull};
    const char *base = check->folder;
    if ((size > 0u) && (written[0] == '#'))
    {
        unsigned long long skip = 1u;
        while ((skip < size) && (written[skip] == '/'))
        {
            skip += 1u;
        }
        cli_line_counted(&line, written + skip, size - skip);
        base = check->top;
    }
    else
    {
        cli_line_counted(&line, written, size);
    }
    if (!cli_line_fits(&line))
    {
        return;
    }
    (void)cli_line_closed(&line);
    char place[CLI_PATH_LONGEST];
    cli_full(base, name, place);
    if (query_is_file(place) || query_is_folder(place))
    {
        return;
    }
    CliLine said = cli_build_said(check->found);
    cli_line_text(&said, "No file is at `");
    cli_line_counted(&said, written, size);
    cli_line_text(&said, "`, the SConscript this call reads");
    (void)cli_build_finding(check->found, &said, check->lines, from, to, CLI_BUILD_ERROR, "scons-script");
}

// a name the file imports kept, or only counted while the names have no cell
static void check_imported(void *context, unsigned long long from, unsigned long long to)
{
    (void)context;
    if ((s_import == NULL) && (s_imports < CLI_SCONS_IMPORTS))
    {
        s_imports += 1u;
    }
    else if ((s_import != NULL) && (s_imports < s_imports_most))
    {
        s_import[s_imports] = (CliSconsImport){from, to, 0};
        s_imports += 1u;
    }
}

// a name exported held to the names the file imports, each it matches marked exported, while as many are held as a
// check holds
static void check_exported(void *context, const char *name, unsigned long long size)
{
    (void)context;
    if ((s_exports >= CLI_SCONS_EXPORTS) || (size > CLI_SCONS_EXPORTS_TEXT - s_export_used))
    {
        return;
    }
    for (unsigned int at = 0u; at < s_imports; at += 1u)
    {
        CliSconsImport *const import = &s_import[at];
        import->exported =
            import->exported ||
            (((import->to - import->from) == size) &&
             (memor.cmp(&(MemoriaCfg){.src = s_check.text + import->from, .other = name, .bytes = (size_t)size}) == 0));
    }
    s_exports += 1u;
    // a name held is no more than CLI_SCONS_EXPORTS_TEXT bytes, which an unsigned int counts
    s_export_used += (unsigned int)size;
}

// what the other SConstructs and SConscripts of the tree export, the file at `path` left out: the tree's files the
// view held in place for the walk, and each file read into cells given back before the next
static void check_others(const char *root, const char *path)
{
    const CliFiles *const files = cli_files_held(root);
    if (files == NULL)
    {
        return;
    }
    unsigned int read = 0u;
    for (unsigned int at = 0u; (at < files->count) && (read < CLI_SCONS_OTHERS); at += 1u)
    {
        const char *const file = cli_files_at(files, at);
        if (cli_build_kind_of(file) != CLI_BUILD_SCONS)
        {
            continue;
        }
        read += 1u;
        char other[CLI_PATH_LONGEST];
        cli_full(root, file, other);
        unsigned long long bytes = 0ull;
        if ((cellul.eq(&(CatenaFinitaCfg){.src = other, .other = path, .cap = CLI_CELLS_POOL})) ||
            !cli_file_size(other, &bytes) || (bytes > CLI_SCONS_OTHER_TEXT))
        {
            continue;
        }
        unsigned char *const text = cli_cell_taken_by(&s_tally, bytes + 1ull, "a file read for what it exports");
        const unsigned long long size = (text != NULL) ? qry_member_file_read(other, text, bytes) : QRY_MEMBER_UNREAD;
        unsigned char *const class_of = ((size != QRY_MEMBER_UNREAD) && cli_utf8_whole((const char *)text, size))
                                            ? check_read((const char *)text, size)
                                            : NULL;
        if (class_of != NULL)
        {
            cli_scons_exported(check_exported, NULL);
        }
        cli_cell_given_by(&s_tally, class_of);
        cli_cell_given_by(&s_tally, text);
    }
    cli_files_let(files);
}

// the file's checks, read already: the SConscripts it reads, and the names it imports held to those the tree exports
static void check_read_file(const char *root, const char *path, const char *text, const CliBuildLines *lines,
                            CliBuildFindings *found)
{
    s_check.text = text;
    s_check.lines = lines;
    s_check.found = found;
    if (!cli_path_parent(path, s_check.folder))
    {
        check_copied(root, s_check.folder);
    }
    check_top_folder(root, path, s_check.top);
    cli_scons_read_scripts(check_script, &s_check);
    s_import = NULL;
    s_imports = 0u;
    cli_scons_imported(check_imported, NULL);
    s_imports_most = s_imports;
    s_import = (s_imports_most > 0u)
                   ? cli_cell_taken_by(&s_tally, s_imports_most * sizeof(CliSconsImport), "the names a file imports")
                   : NULL;
    if (s_import == NULL)
    {
        return;
    }
    s_imports = 0u;
    cli_scons_imported(check_imported, NULL);
    s_exports = 0u;
    s_export_used = 0u;
    cli_scons_exported(check_exported, NULL);
    check_others(root, path);
    for (unsigned int at = 0u; at < s_imports; at += 1u)
    {
        const char *const name = text + s_import[at].from;
        const unsigned long long name_size = s_import[at].to - s_import[at].from;
        if (((name_size == 1u) && (name[0] == '*')) || s_import[at].exported)
        {
            continue;
        }
        CliLine said = cli_build_said(found);
        cli_line_character(&said, '`');
        cli_line_counted(&said, name, name_size);
        cli_line_text(&said, "` is imported here, and no SConstruct or SConscript of the tree exports it");
        (void)cli_build_finding(found, &said, lines, s_import[at].from, s_import[at].to, CLI_BUILD_WARNING,
                                "scons-import");
    }
    cli_cell_given_by(&s_tally, s_import);
    s_import = NULL;
}

void cli_scons_check(const char *root, const char *path, const char *text, unsigned long long size,
                     const CliBuildLines *lines, CliBuildFindings *found)
{
    cli_lock_take(&g_cli_scons_lock);
    unsigned char *const class_of = check_read(text, size);
    if (class_of != NULL)
    {
        check_read_file(root, path, text, lines, found);
    }
    cli_cell_given_by(&s_tally, class_of);
    cli_lock_give(&g_cli_scons_lock);
}

// The hand a name SCons gives is handed to
typedef struct
{
    void (*each)(void *context, const char *name, unsigned long long size);
    void *context;
} CliSconsGiven;

// a name the file imports handed on
static void check_given_import(void *context, unsigned long long from, unsigned long long to)
{
    const CliSconsGiven *const given = context;
    given->each(given->context, g_cli_scons.text + from, to - from);
}

void cli_scons_given(const char *text, unsigned long long size,
                     void (*each)(void *context, const char *name, unsigned long long size), void *context)
{
    for (unsigned int at = 0u; at < 76u; at += 1u)
    {
        each(context, g_cli_scons_globals[at],
             cellul.len(&(CatenaFinitaCfg){.src = g_cli_scons_globals[at], .cap = CLI_CELLS_POOL}));
    }
    cli_lock_take(&g_cli_scons_lock);
    unsigned char *const class_of = check_read(text, size);
    if (class_of != NULL)
    {
        CliSconsGiven given = {each, context};
        cli_scons_imported(check_given_import, &given);
    }
    cli_cell_given_by(&s_tally, class_of);
    cli_lock_give(&g_cli_scons_lock);
}

// whether a letter goes in an environment's name and its method
static int check_chain_letter(unsigned int letter)
{
    return cli_build_word_letter(letter) || (letter == '.');
}

// the `(` of the call whose arguments end at `end`, the class of each byte read: its byte, or `end` where the cursor
// stands in no call's arguments
static unsigned long long check_call_open(unsigned long long end)
{
    const char *const text = g_cli_scons.text;
    long long depth = 0;
    for (unsigned long long index = end; index > 0u;)
    {
        index -= 1u;
        if (g_cli_scons.class_of[index] != CLI_SCONS_CODE)
        {
            continue;
        }
        const char byte = text[index];
        if ((byte == ')') || (byte == ']') || (byte == '}'))
        {
            depth += 1;
        }
        else if (((byte == '(') || (byte == '[') || (byte == '{')) && (depth > 0))
        {
            depth -= 1;
        }
        else if (byte == '(')
        {
            return index;
        }
        else if ((byte == '[') || (byte == '{'))
        {
            return end;
        }
    }
    return end;
}

// what completes the word before byte `at` of the file read, under the reader's lock
static void check_completed(const char *text, unsigned long long at, CliCompletions *items)
{
    const unsigned long long token = cli_build_run_before(text, at, check_chain_letter);
    unsigned long long line_start = token;
    while ((line_start > 0u) && (text[line_start - 1u] != '\n'))
    {
        line_start -= 1u;
    }
    const unsigned char last = (at > 0u) ? g_cli_scons.class_of[at - 1u] : CLI_SCONS_CODE;
    if (last == CLI_SCONS_STRING)
    {
        unsigned long long opened = token;
        while ((opened > line_start) && ((text[opened - 1u] == '"') || (text[opened - 1u] == '\'')))
        {
            opened -= 1u;
        }
        opened = line_start + cli_text_trimmed_end(text + line_start, opened - line_start);
        if ((opened > line_start) && (text[opened - 1u] == '['))
        {
            cli_build_items(items, g_cli_scons_variables, 44u, "property", "construction variable");
        }
        return;
    }
    if (last == CLI_SCONS_COMMENT)
    {
        return;
    }
    if (memor.chr(&(MemoriaCfg){.src = text + token, .val = '.', .bytes = (size_t)(at - token)}) != NULL)
    {
        cli_build_items(items, g_cli_scons_methods, 52u, "function", "environment");
        return;
    }
    const unsigned long long open = check_call_open(token);
    if (open < token)
    {
        const unsigned long long name_end = cli_text_trimmed_end(text, open);
        const unsigned long long name = cli_build_run_before(text, name_end, cli_build_word_letter);
        unsigned long long since = token;
        while ((since > open + 1u) && (text[since - 1u] != ','))
        {
            since -= 1u;
        }
        const int blank = (cli_text_lead(text + since, token - since) == token - since);
        for (unsigned int one = 0u; blank && (one < 16u); one += 1u)
        {
            if ((cellul.len(&(CatenaFinitaCfg){.src = g_cli_scons_setters[one], .cap = CLI_CELLS_POOL}) ==
                 name_end - name) &&
                (memor.cmp(&(MemoriaCfg){
                     .src = g_cli_scons_setters[one], .other = text + name, .bytes = (size_t)(name_end - name)}) == 0))
            {
                cli_build_items(items, g_cli_scons_variables, 44u, "property", "construction variable");
            }
        }
    }
    for (unsigned int one = 0u; one < 76u; one += 1u)
    {
        const char *const global = g_cli_scons_globals[one];
        int upper = 1;
        for (const char *letter = global; *letter != '\0'; letter += 1)
        {
            upper = upper && (((*letter >= 'A') && (*letter <= 'Z')) || (*letter == '_'));
        }
        (void)cli_completion_put(items, global, cellul.len(&(CatenaFinitaCfg){.src = global, .cap = CLI_CELLS_POOL}),
                                 upper ? "constant" : "function", "SCons", 5u, global,
                                 cellul.len(&(CatenaFinitaCfg){.src = global, .cap = CLI_CELLS_POOL}), 0);
    }
}

void cli_scons_complete(const char *text, unsigned long long size, unsigned long long at, CliCompletions *items)
{
    (void)size;
    cli_lock_take(&g_cli_scons_lock);
    unsigned char *const class_of = check_read(text, at);
    if (class_of != NULL)
    {
        check_completed(text, at, items);
    }
    cli_cell_given_by(&s_tally, class_of);
    cli_lock_give(&g_cli_scons_lock);
}
