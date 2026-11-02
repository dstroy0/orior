// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scons_check.c: an SConstruct or SConscript checked: each SConscript it reads that no file is at, `#` naming the
// folder of the SConstruct it belongs to; and each name it imports that no SConstruct or SConscript of the tree
// exports, the first 400 of them read. The names SCons gives a file; and what completes the word before the cursor.
// More imports than CLI_SCONS_IMPORTS are not stored, and the names exported past CLI_SCONS_EXPORTS of them or
// CLI_SCONS_EXPORTS_TEXT of their bytes are not matched to them. Each call takes its capacities as cells of the
// cellblock, counted to the checks' own mark, and gives them back before it returns
#include "../command_line/callback_cfg.h"
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

// the check under way; the names the file imports, a cell for the check containing `s_imports_max` of them, NULL while
// they are only counted, and how many there are; the names exported matched to them and the bytes those take; and what
// the checks' capacities occupy in the cellblock
static CliSconsCheck s_check;
static CliSconsImport *s_import;
static unsigned int s_imports_max;
static unsigned int s_imports;
static unsigned int s_exports;
static unsigned int s_export_used;

void cli_scons_cells_usage_write(const CliSconsCtx *args)
{
    EMBED_CALL(cli_cells_table.owner_usage_write, CliCellsCfg, .owner = &g_cli_scons_work_states_cells,
               .out = args->out);
}

typedef struct
{
    const char *text;
    unsigned long long size;
    const char *from;
    char *out;
    const char *root;
    const char *path;
    const CliBuildLines *lines;
    CliBuildFindings *found;
    unsigned long long end;
    unsigned long long at;
    CliCompletions *items;
} SconsCheckCtx;
// `size` bytes of `text` read as a file of SCons, its class bytes a cell: the cell, for the caller to give back, or
// NULL, nothing allocated, where it is larger than a file read, the cellblock cannot meet its class bytes or it is not
// read
EMBED_INLINE unsigned char *check_read(const SconsCheckCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned char *const class_of = (size <= CLI_SCONS_TEXT)
                                        ? g_cli_cells->persistent_buf_alloc(size + 1ull, &g_cli_scons_work_states_cells)
                                        : NULL;
    if ((class_of != NULL) && !EMBED_CALL(cli_scons_text_size_class_of_internal.read, CliSconsTextSizeClassOfCtx,
                                          .text = text, .size = size, .class_of = class_of))
    {
        g_cli_cells->persistent_buf_release(class_of, &g_cli_scons_work_states_cells);
        return NULL;
    }
    return class_of;
}

// `from` copied into `out`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE void check_copy(const SconsCheckCtx *args)
{
    VerbaLinea line = {args->out, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put(&line, args->from);
    (void)mmgr_line_finish(&line);
}

// the folder of the SConstruct the file at `path` belongs to: the nearest above it in the tree that contains one, the
// tree's top where none does
EMBED_INLINE void check_top_folder(const SconsCheckCtx *args)
{
    const char *root = args->root;
    const char *path = args->path;
    char *out = args->out;
    static const char *const s_names[3] = {"SConstruct", "Sconstruct", "sconstruct"};
    char folder[CLI_PATH_MAX_LEN];
    char above[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = path, .out = folder))
    {
        EMBED_CALL(check_copy, SconsCheckCtx, .from = root, .out = folder);
    }
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = root, .cap = CLI_CELLS_POOL});
    for (;;)
    {
        for (unsigned int at = 0u; at < 3u; at += 1u)
        {
            char file[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = s_names[at], .joined = file);
            if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = file))
            {
                EMBED_CALL(check_copy, SconsCheckCtx, .from = folder, .out = out);
                return;
            }
        }
        const char after = folder[length];
        const int inside = EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = folder, .other_pointer = root,
                                      .size = length) &&
                           ((after == '\0') || (after == '/') || (after == '\\'));
        if ((cellul.eq(&(CatenaFinitaCfg){.src = folder, .other = root, .cap = CLI_CELLS_POOL})) || !inside ||
            !EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = folder, .out = above))
        {
            EMBED_CALL(check_copy, SconsCheckCtx, .from = root, .out = out);
            return;
        }
        EMBED_CALL(check_copy, SconsCheckCtx, .from = above, .out = folder);
    }
}

// an SConscript a call reads checked for a file there
static void check_script(const CliSconsScriptCfg *args)
{
    unsigned long long size = args->size;
    unsigned long long from = args->from;
    unsigned long long to = args->to;
    CliSconsCheck *const check = args->context;
    if (memor.chr(&(MemoriaCfg){.src = args->written, .val = '$', .bytes = (size_t)size}) != NULL)
    {
        return;
    }
    char name[CLI_PATH_MAX_LEN];
    VerbaLinea line = {name, sizeof(name), 0ull};
    const char *base = check->folder;
    if ((size > 0u) && (args->written[0] == '#'))
    {
        unsigned long long skip = 1u;
        skip = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = args->written, .at = skip,
                          .size = size, .set = "/");
        mmgr_line_put_n(&line, args->written + skip, size - skip);
        base = check->top;
    }
    else
    {
        mmgr_line_put_n(&line, args->written, size);
    }
    if (!mmgr_line_ok(&line))
    {
        return;
    }
    (void)mmgr_line_finish(&line);
    char place[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = base, .relative = name, .joined = place);
    if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = place) ||
        EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = place))
    {
        return;
    }
    VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = check->found);
    mmgr_line_put(&msg, "No file is at `");
    mmgr_line_put_n(&msg, args->written, size);
    mmgr_line_put(&msg, "`, the SConscript this call reads");
    (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = check->found, .message = &msg,
                     .lines = check->lines, .from = from, .to = to, .severity = CLI_BUILD_ERROR,
                     .code = "scons-script");
}

// a name the file imports stored, or only counted while the names have no cell
static void check_import(const CliSconsSpanCfg *args)
{
    if ((s_import == NULL) && (s_imports < CLI_SCONS_IMPORTS))
    {
        s_imports += 1u;
    }
    else if ((s_import != NULL) && (s_imports < s_imports_max))
    {
        s_import[s_imports] = (CliSconsImport){args->from, args->to, 0};
        s_imports += 1u;
    }
}

// a name exported matched against the names the file imports, each it matches marked exported, while as many are stored
// as a check allows
static void check_export(const CliEachNameSizeCfg *args)
{
    if ((s_exports >= CLI_SCONS_EXPORTS) || (args->size > CLI_SCONS_EXPORTS_TEXT - s_export_used))
    {
        return;
    }
    for (unsigned int at = 0u; at < s_imports; at += 1u)
    {
        CliSconsImport *const import = &s_import[at];
        import->exported =
            import->exported ||
            (((import->to - import->from) == args->size) &&
             (memor.cmp(&(MemoriaCfg){
                  .src = s_check.text + import->from, .other = args->name, .bytes = (size_t)args->size}) == 0));
    }
    s_exports += 1u;
    // a name stored is no more than CLI_SCONS_EXPORTS_TEXT bytes, which an unsigned int counts
    s_export_used += (unsigned int)args->size;
}

// what the other SConstructs and SConscripts of the tree export, the file at `path` left out: the tree's files laid
// flat for the walk, and each file read into cells given back before the next
EMBED_INLINE void check_others(const SconsCheckCtx *args)
{
    const char *root = args->root;
    const char *path = args->path;
    const CliFiles *const files = EMBED_CALL(cli_files_root_paths.view, CliFilesRootPathsCfg, .root = root);
    if (files == NULL)
    {
        return;
    }
    unsigned int read = 0u;
    for (unsigned int at = 0u; (at < files->count) && (read < CLI_SCONS_OTHERS); at += 1u)
    {
        const char *const file = EMBED_CALL(cli_files.at, CliFilesCfg, .files = files, .index = at);
        if (EMBED_CALL(cli_builds_path.build_kind_get, CliBuildsPathCfg, .path = file) != CLI_BUILD_SCONS)
        {
            continue;
        }
        read += 1u;
        char other[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = root, .relative = file, .joined = other);
        unsigned long long bytes = 0ull;
        if ((cellul.eq(&(CatenaFinitaCfg){.src = other, .other = path, .cap = CLI_CELLS_POOL})) ||
            !EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = other, .size_pointer = &bytes) ||
            (bytes > CLI_SCONS_OTHER_TEXT))
        {
            continue;
        }
        unsigned char *const text = g_cli_cells->temporary_buf_alloc(bytes + 1ull, &g_cli_scons_work_states_cells);
        const unsigned long long size = (text != NULL) ? qry_member_file_read(other, text, bytes) : QRY_MEMBER_UNREAD;
        unsigned char *const class_of =
            ((size != QRY_MEMBER_UNREAD) &&
             EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = (const char *)text, .size = size))
                ? EMBED_CALL(check_read, SconsCheckCtx, .text = (const char *)text, .size = size)
                : NULL;
        if (class_of != NULL)
        {
            EMBED_CALL(cli_scons_context_internal.export, CliSconsContextCtx, .named = check_export, .context = NULL);
        }
        g_cli_cells->persistent_buf_release(class_of, &g_cli_scons_work_states_cells);
        g_cli_cells->persistent_buf_release(text, &g_cli_scons_work_states_cells);
    }
    EMBED_CALL(cli_files.let, CliFilesCfg, .files = files);
}

// the file's checks, read already: the SConscripts it reads, and the names it imports matched against those the tree
// exports
EMBED_INLINE void check_read_file(const SconsCheckCtx *args)
{
    const char *path = args->path;
    const char *text = args->text;
    const CliBuildLines *lines = args->lines;
    CliBuildFindings *found = args->found;
    s_check.text = text;
    s_check.lines = lines;
    s_check.found = found;
    if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = path, .out = s_check.folder))
    {
        EMBED_CALL(check_copy, SconsCheckCtx, .from = args->root, .out = s_check.folder);
    }
    EMBED_CALL(check_top_folder, SconsCheckCtx, .root = args->root, .path = path, .out = s_check.top);
    EMBED_CALL(cli_scons_context_each_script_internal.read_scripts, CliSconsContextEachScriptCtx,
               .each_scons_script = check_script, .context = &s_check);
    s_import = NULL;
    s_imports = 0u;
    EMBED_CALL(cli_scons_name_internal.import, CliSconsNameCtx, .each = check_import, .context = NULL);
    s_imports_max = s_imports;
    s_import = (s_imports_max > 0u) ? g_cli_cells->persistent_buf_alloc(s_imports_max * sizeof(CliSconsImport),
                                                                        &g_cli_scons_work_states_cells)
                                    : NULL;
    if (s_import == NULL)
    {
        return;
    }
    s_imports = 0u;
    EMBED_CALL(cli_scons_name_internal.import, CliSconsNameCtx, .each = check_import, .context = NULL);
    s_exports = 0u;
    s_export_used = 0u;
    EMBED_CALL(cli_scons_context_internal.export, CliSconsContextCtx, .named = check_export, .context = NULL);
    EMBED_CALL(check_others, SconsCheckCtx, .root = args->root, .path = path);
    for (unsigned int at = 0u; at < s_imports; at += 1u)
    {
        const char *const name = text + s_import[at].from;
        const unsigned long long name_size = s_import[at].to - s_import[at].from;
        if (((name_size == 1u) && (name[0] == '*')) || s_import[at].exported)
        {
            continue;
        }
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = found);
        mmgr_line_ch(&msg, '`');
        mmgr_line_put_n(&msg, name, name_size);
        mmgr_line_put(&msg, "` is imported here, and no SConstruct or SConscript of the tree exports it");
        (void)EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = found, .message = &msg,
                         .lines = lines, .from = s_import[at].from, .to = s_import[at].to,
                         .severity = CLI_BUILD_WARNING, .code = "scons-import");
    }
    g_cli_cells->persistent_buf_release(s_import, &g_cli_scons_work_states_cells);
    s_import = NULL;
}

void cli_scons_check(const CliSconsTextCtx *args)
{
    unsigned char *const class_of = EMBED_CALL(check_read, SconsCheckCtx, .text = args->text, .size = args->size);
    if (class_of != NULL)
    {
        EMBED_CALL(check_read_file, SconsCheckCtx, .root = args->root, .path = args->path, .text = args->text,
                   .lines = args->lines, .found = args->found);
    }
    g_cli_cells->persistent_buf_release(class_of, &g_cli_scons_work_states_cells);
}

// The hand a name SCons gives is handed to
typedef struct
{
    void (*each)(const CliEachNameSizeCfg *args);
    void *context;
} CliSconsInput;

// a name the file imports handed on
static void check_input_import(const CliSconsSpanCfg *args)
{
    const CliSconsInput *const supplied = args->context;
    EMBED_CALL(supplied->each, CliEachNameSizeCfg, .context = supplied->context, .name = g_cli_scons.text + args->from,
               .size = args->to - args->from);
}

void cli_scons_free(const CliSconsTextSizeContextCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    void (*each)(const CliEachNameSizeCfg *args) = args->each_callback;
    void *context = args->context;
    for (unsigned int at = 0u; at < 76u; at += 1u)
    {
        EMBED_CALL(each, CliEachNameSizeCfg, .context = context, .name = g_cli_scons_globals[at],
                   .size = cellul.len(&(CatenaFinitaCfg){.src = g_cli_scons_globals[at], .cap = CLI_CELLS_POOL}));
    }
    unsigned char *const class_of = EMBED_CALL(check_read, SconsCheckCtx, .text = text, .size = size);
    if (class_of != NULL)
    {
        CliSconsInput supplied = {each, context};
        EMBED_CALL(cli_scons_name_internal.import, CliSconsNameCtx, .each = check_input_import, .context = &supplied);
    }
    g_cli_cells->persistent_buf_release(class_of, &g_cli_scons_work_states_cells);
}

// whether a letter goes in an environment's name and its method
static int check_chain_letter(const CliEachLetterCfg *args)
{
    return cli_each_letter.build_word_letter(args) || (args->letter == '.');
}

// the `(` of the call whose arguments end at `end`, the class of each byte read: its byte, or `end` where the cursor
// stands in no call's arguments
EMBED_INLINE unsigned long long check_call_open(const SconsCheckCtx *args)
{
    const char *const text = g_cli_scons.text;
    long long depth = 0;
    for (unsigned long long index = args->end; index > 0u;)
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
            return args->end;
        }
    }
    return args->end;
}

// what completes the word before byte `at` of the file read, under the reader's lock
EMBED_INLINE void check_complete(const SconsCheckCtx *args)
{
    const char *text = args->text;
    unsigned long long at = args->at;
    CliCompletions *items = args->items;
    const unsigned long long token = EMBED_CALL(cli_builds_text_at.build_run_before, CliBuildsTextAtCfg, .text = text,
                                                .at = at, .of = check_chain_letter);
    unsigned long long line_start = token;
    line_start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = text, .at = line_start,
                            .set = "\n");
    const unsigned char last = (at > 0u) ? g_cli_scons.class_of[at - 1u] : CLI_SCONS_CODE;
    if (last == CLI_SCONS_STRING)
    {
        unsigned long long opened = token;
        opened = (line_start + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg,
                                          .text = text + line_start, .at = opened - line_start, .set = "\"'"));
        opened = line_start + EMBED_CALL(cli_text_size_byte.trimmed_end, CliTextSizeByteCfg, .text = text + line_start,
                                         .size = opened - line_start);
        if ((opened > line_start) && (text[opened - 1u] == '['))
        {
            EMBED_CALL(cli_builds_items_kind.build_items, CliBuildsItemsKindCfg, .items = items,
                       .words = g_cli_scons_variables, .count = 44u, .kind = "property",
                       .detail = "construction variable");
        }
        return;
    }
    if (last == CLI_SCONS_COMMENT)
    {
        return;
    }
    if (memor.chr(&(MemoriaCfg){.src = text + token, .val = '.', .bytes = (size_t)(at - token)}) != NULL)
    {
        EMBED_CALL(cli_builds_items_kind.build_items, CliBuildsItemsKindCfg, .items = items,
                   .words = g_cli_scons_methods, .count = 52u, .kind = "function", .detail = "environment");
        return;
    }
    const unsigned long long open = EMBED_CALL(check_call_open, SconsCheckCtx, .end = token);
    if (open < token)
    {
        const unsigned long long name_end =
            EMBED_CALL(cli_text_size_byte.trimmed_end, CliTextSizeByteCfg, .text = text, .size = open);
        const unsigned long long name = EMBED_CALL(cli_builds_text_at.build_run_before, CliBuildsTextAtCfg,
                                                   .text = text, .at = name_end, .of = cli_build_word_letter);
        unsigned long long since = token;
        since = (open + 1u +
                 EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = text + open + 1u,
                            .at = since - open + 1u, .set = ","));
        const int blank = (EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg, .text = text + since,
                                      .size = token - since) == token - since);
        for (unsigned int one = 0u; blank && (one < 16u); one += 1u)
        {
            if ((cellul.len(&(CatenaFinitaCfg){.src = g_cli_scons_setters[one], .cap = CLI_CELLS_POOL}) ==
                 name_end - name) &&
                (memor.cmp(&(MemoriaCfg){
                     .src = g_cli_scons_setters[one], .other = text + name, .bytes = (size_t)(name_end - name)}) == 0))
            {
                EMBED_CALL(cli_builds_items_kind.build_items, CliBuildsItemsKindCfg, .items = items,
                           .words = g_cli_scons_variables, .count = 44u, .kind = "property",
                           .detail = "construction variable");
            }
        }
    }
    for (unsigned int one = 0u; one < 76u; one += 1u)
    {
        const char *const global = g_cli_scons_globals[one];
        int upper = 1;
        upper = EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = global, .at = 0ull,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = global, .cap = CLI_CELLS_POOL}),
                           .classes = CLI_TEXT_UPPER,
                           .set = "_") == cellul.len(&(CatenaFinitaCfg){.src = global, .cap = CLI_CELLS_POOL});
        (void)EMBED_CALL(
            cli_edits_items.completion_append, CliEditsItemsCfg, .items = items, .label = global,
            .label_size = cellul.len(&(CatenaFinitaCfg){.src = global, .cap = CLI_CELLS_POOL}),
            .kind = upper ? "constant" : "function", .detail = "SCons", .detail_size = 5u, .insert = global,
            .insert_size = cellul.len(&(CatenaFinitaCfg){.src = global, .cap = CLI_CELLS_POOL}), .snippet = 0);
    }
}

void cli_scons_complete(const CliSconsTextSizeCtx *args)
{
    unsigned char *const class_of = EMBED_CALL(check_read, SconsCheckCtx, .text = args->text, .size = args->at);
    if (class_of != NULL)
    {
        EMBED_CALL(check_complete, SconsCheckCtx, .text = args->text, .at = args->at, .items = args->items);
    }
    g_cli_cells->persistent_buf_release(class_of, &g_cli_scons_work_states_cells);
}
