// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scons_check.c: an SConstruct or SConscript checked: each SConscript it reads that no file is at, `#` naming the
// folder of the SConstruct it belongs to; and each name it imports that no SConstruct or SConscript of the tree
// exports, the first 400 of them read. The names SCons gives a file; and what completes the word before the cursor.
// More imports than CLI_SCONS_IMPORTS are not stored, and the names exported past CLI_SCONS_EXPORTS of them or
// CLI_SCONS_EXPORTS_TEXT of their bytes are not matched to them. Each call takes its rooms as cells of the cellblock,
// counted to the checks' own mark, and gives them back before it returns
#include "../editor/files.h"
#include "../text/json_tree.h"
#include "scons_internal.h"

#include "../text/text.h"

// the most names a file imports stored, the most names exported matched to them and the bytes they take, the most files
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
    char folder[CLI_PATH_MAX_LEN];
    char top[CLI_PATH_MAX_LEN];
} CliSconsCheck;

// A name the file imports: its span in the file, and whether an SConstruct or SConscript of the tree exports it
typedef struct
{
    unsigned long long from;
    unsigned long long to;
    int exported;
} CliSconsImport;

// the check under way; the names the file imports, a cell for the check containing `s_imports_most` of them, NULL while
// they are only counted, and how many there are; the names exported matched to them and the bytes those take; and what
// the checks' rooms occupy in the cellblock
static CliSconsCheck s_check;
static CliSconsImport *s_import;
static unsigned int s_imports_most;
static unsigned int s_imports;
static unsigned int s_exports;
static unsigned int s_export_used;
static CellOwner s_owner = MMGR_CELL_OWNER("cli_scons_rooms");

void cli_scons_cells_usage_written(VerbaLinea *out)
{
    cli_cells_owner_usage_written(&s_owner, out);
}

// `size` bytes of `text` read as a file of SCons, its class bytes a cell: the cell, for the caller to give back, or
// NULL, nothing allocated, where it is larger than a file read, the cellblock cannot meet its class bytes or it is not
// read
static unsigned char *check_read(const char *text, unsigned long long size)
{
    unsigned char *const class_of =
        (size <= CLI_SCONS_TEXT) ? g_cli_cells->persistent_buf_alloc(size + 1ull, &s_owner) : NULL;
    if ((class_of != NULL) && !cli_scons_read(text, size, class_of))
    {
        g_cli_cells->persistent_buf_release(class_of, &s_owner);
        return NULL;
    }
    return class_of;
}

// `from` copied into `out`, sized for CLI_PATH_MAX_LEN
static void check_copied(const char *from, char *out)
{
    VerbaLinea line = {out, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, from);
    (void)mmgr_line_finish(&line);
}

// the folder of the SConstruct the file at `path` belongs to: the nearest above it in the tree that contains one, the
// tree's top where none does
static void check_top_folder(const char *root, const char *path, char *out)
{
    static const char *const s_names[3] = {"SConstruct", "Sconstruct", "sconstruct"};
    char folder[CLI_PATH_MAX_LEN];
    char above[CLI_PATH_MAX_LEN];
    if (!cli_path_parent(path, folder))
    {
        check_copied(root, folder);
    }
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    for (;;)
    {
        for (unsigned int at = 0u; at < 3u; at += 1u)
        {
            char file[CLI_PATH_MAX_LEN];
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
    char name[CLI_PATH_MAX_LEN];
    VerbaLinea line = {name, sizeof(name), 0ull};
    const char *base = check->folder;
    if ((size > 0u) && (written[0] == '#'))
    {
        unsigned long long skip = 1u;
        while ((skip < size) && (written[skip] == '/'))
        {
            skip += 1u;
        }
        mmgr_line_put_n(&line, written + skip, size - skip);
        base = check->top;
    }
    else
    {
        mmgr_line_put_n(&line, written, size);
    }
    if (!mmgr_line_ok(&line))
    {
        return;
    }
    (void)mmgr_line_finish(&line);
    char place[CLI_PATH_MAX_LEN];
    cli_full(base, name, place);
    if (query_is_file(place) || query_is_folder(place))
    {
        return;
    }
    VerbaLinea msg = cli_build_msg_line(check->found);
    mmgr_line_put(&msg, "No file is at `");
    mmgr_line_put_n(&msg, written, size);
    mmgr_line_put(&msg, "`, the SConscript this call reads");
    (void)cli_build_finding(check->found, &msg, check->lines, from, to, CLI_BUILD_ERROR, "scons-script");
}

// a name the file imports stored, or only counted while the names have no cell
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

// a name exported matched against the names the file imports, each it matches marked exported, while as many are stored
// as a check allows
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
    // a name stored is no more than CLI_SCONS_EXPORTS_TEXT bytes, which an unsigned int counts
    s_export_used += (unsigned int)size;
}

// what the other SConstructs and SConscripts of the tree export, the file at `path` left out: the tree's files the
// view pinned in place for the walk, and each file read into cells given back before the next
static void check_others(const char *root, const char *path)
{
    const CliFiles *const files = cli_files_view(root);
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
        char other[CLI_PATH_MAX_LEN];
        cli_full(root, file, other);
        unsigned long long bytes = 0ull;
        if ((cellul.eq(&(CatenaFinitaCfg){.src = other, .other = path, .cap = CLI_CELLS_POOL})) ||
            !cli_file_size(other, &bytes) || (bytes > CLI_SCONS_OTHER_TEXT))
        {
            continue;
        }
        unsigned char *const text = g_cli_cells->persistent_buf_alloc(bytes + 1ull, &s_owner);
        const unsigned long long size = (text != NULL) ? qry_member_file_read(other, text, bytes) : QRY_MEMBER_UNREAD;
        unsigned char *const class_of = ((size != QRY_MEMBER_UNREAD) && cli_utf8_whole((const char *)text, size))
                                            ? check_read((const char *)text, size)
                                            : NULL;
        if (class_of != NULL)
        {
            cli_scons_exported(check_exported, NULL);
        }
        g_cli_cells->persistent_buf_release(class_of, &s_owner);
        g_cli_cells->persistent_buf_release(text, &s_owner);
    }
    cli_files_let(files);
}

// the file's checks, read already: the SConscripts it reads, and the names it imports matched against those the tree
// exports
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
                   ? g_cli_cells->persistent_buf_alloc(s_imports_most * sizeof(CliSconsImport), &s_owner)
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
        VerbaLinea msg = cli_build_msg_line(found);
        mmgr_line_ch(&msg, '`');
        mmgr_line_put_n(&msg, name, name_size);
        mmgr_line_put(&msg, "` is imported here, and no SConstruct or SConscript of the tree exports it");
        (void)cli_build_finding(found, &msg, lines, s_import[at].from, s_import[at].to, CLI_BUILD_WARNING,
                                "scons-import");
    }
    g_cli_cells->persistent_buf_release(s_import, &s_owner);
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
    g_cli_cells->persistent_buf_release(class_of, &s_owner);
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
    g_cli_cells->persistent_buf_release(class_of, &s_owner);
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
    g_cli_cells->persistent_buf_release(class_of, &s_owner);
    cli_lock_give(&g_cli_scons_lock);
}
