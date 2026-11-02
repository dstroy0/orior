// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sea_build.c: a script built into a single executable of Node's, by `node --build-sea` from Node 25.5 and by Node's
// documented steps before it; node's copy written in pieces from its mapping, so node is never read whole
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
#include "../text/text.h"
#include "sea.h"

// the most bytes of a script read, and the most modules outside Node reported
#define CLI_SEA_SCRIPT (1u << 22u)

#define CLI_SEA_WANTED 64u

typedef struct
{
    QuerySink sink;
    const char *text;
    const char *path;
    const char *extension;
    char *out;
    VerbaLinea *line;
    const char *node;
    const char *const *request;
    unsigned int count;
    VerbaLinea *err_msg;
    unsigned long long *major;
    unsigned long long *minor;
    const char *how;
#if !((!defined(_WIN32)))
    const unsigned char *program;
    unsigned long long size;
    const unsigned char *data;
    unsigned long long data_size;
    const char *output;
#endif
#if (!defined(_WIN32))
    const unsigned char *program;
    unsigned long long size;
    const unsigned char *data;
    unsigned long long data_size;
    const char *output;
#endif
} SeaBuildCtx;
// a line printed through `sink`
EMBED_INLINE void cli_print(const SeaBuildCtx *args)
{
    EMBED_CALL(args->sink.print, CliEachLineSizeCfg, .context = args->sink.context, .line = args->text,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL}));
}

// `path` with its file's extension, where it has one, put as `extension` into `out`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE void cli_with_extension(const SeaBuildCtx *args)
{
    const char *name = args->path;
    const unsigned long long name_past =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}), .set = "/\\");
    name = (name_past != 0ull) ? (args->path + name_past) : name;
    const char *dot = NULL;
    const unsigned long long dot_past =
        (name[0] != '\0')
            ? EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = name + 1,
                         .at = cellul.len(&(CatenaFinitaCfg){.src = name + 1, .cap = CLI_CELLS_POOL}), .set = ".")
            : 0ull;
    dot = (dot_past != 0ull) ? (name + dot_past) : NULL;
    VerbaLinea line = {args->out, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put_n(&line, args->path,
                    (dot != NULL) ? (unsigned long long)(dot - args->path)
                                  : cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(&line, '.');
    mmgr_line_put(&line, args->extension);
    mmgr_line_finish(&line);
}

// `path` with `/` between its parts, written as a JSON string after `line`
EMBED_INLINE void cli_slash(const SeaBuildCtx *args)
{
    char slashed[CLI_PATH_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = slashed,
                            .src = args->path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}) + 1u});
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = slashed,
               .size = cellul.len(&(CatenaFinitaCfg){.src = slashed, .cap = CLI_CELLS_POOL}), .from = '\\', .to = '/');
    char quoted[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea json = {quoted, sizeof(quoted), 0ull};
    mmgr_line_ch(&json, '"');
    EMBED_CALL(cli_text_size_set.put_mark, CliTextSizeSetCfg, .out = &json, .text = slashed, .set = "\"\\",
               .mark = '\\');
    mmgr_line_ch(&json, '"');
    mmgr_line_finish(&json);
    mmgr_line_put(args->line, quoted);
}

// node run with `args`, `count` of them, each line it writes printed as it ends: 1, or 0 with why given where it fails
EMBED_INLINE int cli_node_run(const SeaBuildCtx *args)
{
    unsigned int count = args->count;
    VerbaLinea *err_msg = args->err_msg;
    char shown[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea line = {shown, sizeof(shown), 0ull};
    mmgr_line_put(&line, "$ node");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_ch(&line, ' ');
        mmgr_line_put(&line, args->request[at]);
    }
    mmgr_line_finish(&line);
    EMBED_CALL(cli_print, SeaBuildCtx, .sink = args->sink, .text = shown);
    const char *words[8] = {args->node, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    for (unsigned int at = 0u; (at < count) && (at < 6u); at += 1u)
    {
        words[at + 1u] = args->request[at];
    }
    CliRan *const ran = cli_ran_take();
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ull};
    if (!EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = NULL, .settings = NULL, .count = 0u,
                    .ran = ran, .err_msg = &unstarted))
    {
        EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
        mmgr_line_put(err_msg, "node: ");
        mmgr_line_put_n(err_msg, why, unstarted.at);
        return 0;
    }
    for (unsigned int stream = 0u; stream < 2u; stream += 1u)
    {
        const char *text = (stream == 0u) ? ran->out : ran->err;
        while (text[0] != '\0')
        {
            const char *stop = text;
            stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ull,
                               .size = cellul.len(&(CatenaFinitaCfg){.src = stop, .cap = CLI_CELLS_POOL}), .set = "\n");
            const unsigned long long length =
                (unsigned long long)(stop - text) - (((stop > text) && (stop[-1] == '\r')) ? 1ull : 0ull);
            EMBED_CALL(args->sink.print, CliEachLineSizeCfg, .context = args->sink.context, .line = text,
                       .size = length);
            text = (stop[0] != '\0') ? (stop + 1) : stop;
        }
    }
    const long long code = ran->code;
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    if (code != 0ll)
    {
        mmgr_line_put(err_msg, "node ");
        mmgr_line_put(err_msg, (count != 0u) ? args->request[0] : "");
        mmgr_line_put(err_msg, " ended with exit code: ");
        if (code < 0ll)
        {
            mmgr_line_ch(err_msg, '-');
        }
        // the code's size, its sign written apart
        mmgr_line_decimal(err_msg, (unsigned long long)((code < 0ll) ? -code : code), 1u);
        return 0;
    }
    return 1;
}

// the version node reports it is, its three numbers into `major` and `minor`: 1, or 0 with why given
EMBED_INLINE int cli_version(const SeaBuildCtx *args)
{
    unsigned long long *major = args->major;
    unsigned long long *minor = args->minor;
    VerbaLinea *err_msg = args->err_msg;
    const char *const words[3] = {args->node, "--version", NULL};
    CliRan *const ran = cli_ran_take();
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ull};
    if (!EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = NULL, .settings = NULL, .count = 0u,
                    .ran = ran, .err_msg = &unstarted))
    {
        EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
        mmgr_line_put(err_msg, args->node);
        mmgr_line_put(err_msg, ": ");
        mmgr_line_put_n(err_msg, why, unstarted.at);
        return 0;
    }
    const char *text = ran->out;
    const char *end = ran->out + ran->out_size;
    text += EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = text, .at = 0ull,
                       .size = (unsigned long long)(end - text), .set = CLI_TEXT_WHITE);
    end = text + EMBED_CALL(cli_text_at_size_classes.back_past, CliTextAtSizeClassesCfg, .text = text,
                            .at = (unsigned long long)(end - text), .set = CLI_TEXT_WHITE);
    const char *const trimmed = text;
    text += EMBED_CALL(cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = text, .at = 0ull,
                       .size = (unsigned long long)(end - text), .set = "v");
    // the parts between the dots that are numbers
    unsigned long long numbers[3] = {0ull, 0ull, 0ull};
    unsigned int read = 0u;
    while ((text < end) && (read < 3u))
    {
        const char *stop = text;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ull,
                           .size = (unsigned long long)(end - stop), .set = ".");
        int number = stop > text;
        unsigned long long value = 0ull;
        for (const char *digit = text; number && (digit < stop); digit += 1)
        {
            number = (digit[0] >= '0') && (digit[0] <= '9');
            value = (value * 10ull) + (unsigned long long)(digit[0] - '0');
        }
        if (number)
        {
            numbers[read] = value;
            read += 1u;
        }
        text = (stop < end) ? (stop + 1) : stop;
    }
    if (read < 3u)
    {
        mmgr_line_put(err_msg, args->node);
        mmgr_line_put(err_msg, " reports no version: ");
        mmgr_line_put_n(err_msg, trimmed, (unsigned long long)(end - trimmed));
        EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
        return 0;
    }
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    *major = numbers[0];
    *minor = numbers[1];
    return 1;
}

// `path` could not be read or written, reported
EMBED_INLINE int cli_fail(const SeaBuildCtx *args)
{
    mmgr_line_put(args->err_msg, args->path);
    mmgr_line_put(args->err_msg, args->how);
    return 0;
}

#if !defined(_WIN32)
// the copy of node with the blob added, as Linux takes it: a note in a segment of its own, the fuse turned
EMBED_INLINE int cli_linux_copy(const SeaBuildCtx *args)
{
    const unsigned char *program = args->program;
    unsigned long long size = args->size;
    unsigned long long data_size = args->data_size;
    VerbaLinea *err_msg = args->err_msg;
    static CliSeaNote s_note;
    unsigned long long fuse = 0ull;
    if (!EMBED_CALL(cli_sea_err_msg.note, CliSeaErrMsgCfg, .program = program, .size = size, .data_size = data_size,
                    .note = &s_note, .err_msg = err_msg) ||
        !EMBED_CALL(cli_sea_err_msg_program_size.fuse_at, CliSeaErrMsgProgramSizeCfg, .program = program, .size = size,
                    .at = &fuse, .err_msg = err_msg))
    {
        return 0;
    }
    char print_line[CLI_NAME_MAX_LEN];
    VerbaLinea line = {print_line, sizeof(print_line), 0ull};
    mmgr_line_put(&line, "added ");
    mmgr_line_decimal(&line, data_size, 1u);
    mmgr_line_put(&line, " bytes as the note NODE_SEA_BLOB");
    mmgr_line_finish(&line);
    EMBED_CALL(cli_print, SeaBuildCtx, .sink = args->sink, .text = print_line);
    // the program's head with its header table's place and count set to the new ones
    unsigned char head[64];
    memor.cpy(&(MemoriaCfg){.dst = head, .src = program, .bytes = 64u});
    for (unsigned int byte = 0u; byte < 8u; byte += 1u)
    {
        // each byte of the table's place, its low eight bits
        head[0x20u + byte] = (unsigned char)(s_note.region >> (8u * byte));
    }
    const unsigned long long count = s_note.table_size / 56ull;
    // the count of headers, below 65536 as CLI_SEA_HEADERS bounds it, its two bytes
    head[0x38] = (unsigned char)count;
    head[0x39] = (unsigned char)(count >> 8u);
    static const unsigned char s_turned = '1';
    const CliFilePiece pieces[9] = {{head, 64ull},
                                    {program + 64, fuse - 64ull},
                                    {&s_turned, 1ull},
                                    {program + fuse + 1, size - fuse - 1ull},
                                    {NULL, s_note.region - size},
                                    {s_note.table, s_note.table_size},
                                    {NULL, s_note.note_at - s_note.region - s_note.table_size},
                                    {s_note.head, s_note.head_size},
                                    {args->data, data_size}};
    static const unsigned char s_pad[4] = {0u, 0u, 0u, 0u};
    const CliFilePiece padding = {s_pad, (4ull - (data_size % 4ull)) % 4ull};
    CliFilePiece all[10];
    memor.cpy(&(MemoriaCfg){.dst = all, .src = pieces, .bytes = sizeof(pieces)});
    all[9] = padding;
    if ((fuse < 64ull) || !EMBED_CALL(cli_path_count_internal.file_pieces_write, CliPathCountCtx, .path = args->output,
                                      .pieces = all, .count = 10u))
    {
        return EMBED_CALL(cli_fail, SeaBuildCtx, .path = args->output, .how = ": it could not be written",
                          .err_msg = err_msg);
    }
    EMBED_CALL(cli_path_file_internal.file_runnable, CliPathFileCtx, .path = args->output);
    return 1;
}
#else
// the copy of node with the blob added, as Windows takes it: node's signature taken off, the blob added as a resource,
// and the fuse turned, the copy written beside the output and moved over it
EMBED_INLINE int cli_windows_copy(const SeaBuildCtx *args)
{
    const unsigned char *program = args->program;
    unsigned long long size = args->size;
    const unsigned char *data = args->data;
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long entry = 0ull;
    unsigned long long length = size;
    const int cut = EMBED_CALL(cli_sea_err_msg_program.signature, CliSeaErrMsgProgramCfg, .program = program,
                               .size = size, .entry = &entry, .length = &length, .err_msg = err_msg);
    if (cut < 0)
    {
        return 0;
    }
    if (cut > 0)
    {
        EMBED_CALL(cli_print, SeaBuildCtx, .sink = args->sink, .text = "took off node's signature");
    }
    const CliFilePiece unsigned_pieces[3] = {{program, (cut > 0) ? entry : length},
                                             {NULL, (cut > 0) ? 8ull : 0ull},
                                             {program + entry + 8ull, (cut > 0) ? (length - entry - 8ull) : 0ull}};
    if (!EMBED_CALL(cli_path_count_internal.file_pieces_write, CliPathCountCtx, .path = args->output,
                    .pieces = unsigned_pieces, .count = 3u))
    {
        return EMBED_CALL(cli_fail, SeaBuildCtx, .path = args->output, .how = ": it could not be written",
                          .err_msg = err_msg);
    }
    if (!EMBED_CALL(cli_path_err_msg_internal.resource_add, CliPathErrMsgCtx, .path = args->output,
                    .name = "NODE_SEA_BLOB", .data = data, .size = args->data_size, .err_msg = err_msg))
    {
        return 0;
    }
    char print_line[CLI_NAME_MAX_LEN];
    VerbaLinea line = {print_line, sizeof(print_line), 0ull};
    mmgr_line_put(&line, "added ");
    mmgr_line_decimal(&line, args->data_size, 1u);
    mmgr_line_put(&line, " bytes as the resource NODE_SEA_BLOB");
    mmgr_line_finish(&line);
    EMBED_CALL(cli_print, SeaBuildCtx, .sink = args->sink, .text = print_line);
    unsigned long long made_size = 0ull;
    const unsigned char *const made = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = args->output,
                                                 .size_pointer = &made_size, .owner = &g_cli_sea_cells);
    if (made == NULL)
    {
        return EMBED_CALL(cli_fail, SeaBuildCtx, .path = args->output, .how = ": it could not be read",
                          .err_msg = err_msg);
    }
    unsigned long long fuse = 0ull;
    char fused[CLI_PATH_MAX_LEN];
    VerbaLinea fused_line = {fused, sizeof(fused), 0ull};
    mmgr_line_put(&fused_line, args->output);
    mmgr_line_put(&fused_line, ".fused");
    static const unsigned char s_turned = '1';
    int done = (mmgr_line_finish(&fused_line) != 0ull) &&
               EMBED_CALL(cli_sea_err_msg_program_size.fuse_at, CliSeaErrMsgProgramSizeCfg, .program = made,
                          .size = made_size, .at = &fuse, .err_msg = err_msg);
    if (done)
    {
        const CliFilePiece pieces[3] = {{made, fuse}, {&s_turned, 1ull}, {made + fuse + 1, made_size - fuse - 1ull}};
        done = EMBED_CALL(cli_path_count_internal.file_pieces_write, CliPathCountCtx, .path = fused, .pieces = pieces,
                          .count = 3u);
        if (!done)
        {
            EMBED_CALL(cli_fail, SeaBuildCtx, .path = fused, .how = ": it could not be written", .err_msg = err_msg);
        }
    }
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = made, .owner = &g_cli_sea_cells);
    if (done && !EMBED_CALL(query_from_internal.file_rename, QueryFromCtx, .from = fused, .to = args->output))
    {
        return EMBED_CALL(cli_fail, SeaBuildCtx, .path = args->output, .how = ": it could not be replaced",
                          .err_msg = err_msg);
    }
    return done;
}
#endif

int cli_sea_build(const CliSeaNodeCfg *args)
{
    static char s_wanted[CLI_SEA_WANTED][CLI_NAME_MAX_LEN];
    static char s_config_text[CLI_PATH_MAX_LEN * 4u];
    unsigned long long script_size = 0ull;
    // the script read as its text, the zero of the slack after it
    const unsigned char *const mapped = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = args->main,
                                                   .size_pointer = &script_size, .owner = &g_cli_sea_cells);
    if ((mapped == NULL) || (script_size >= CLI_SEA_SCRIPT))
    {
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped, .owner = &g_cli_sea_cells);
        return EMBED_CALL(cli_fail, SeaBuildCtx, .path = args->main, .how = ": it could not be read",
                          .err_msg = args->err_msg);
    }
    const unsigned int wanted = EMBED_CALL(cli_sea_table.outside, CliSeaCfg, .text = (const char *)mapped,
                                           .found = s_wanted, .room = CLI_SEA_WANTED);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped, .owner = &g_cli_sea_cells);
    if (wanted != 0u)
    {
        mmgr_line_put(args->err_msg, args->main);
        mmgr_line_put(args->err_msg, " requires ");
        for (unsigned int at = 0u; at < wanted; at += 1u)
        {
            mmgr_line_put(args->err_msg, (at != 0u) ? ", " : "");
            mmgr_line_put(args->err_msg, s_wanted[at]);
        }
        mmgr_line_put(args->err_msg,
                      ", which a single executable of Node's does not contain: it loads Node's own modules alone; "
                      "Bun or Deno bundles them in");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->made,
                            .src = args->output,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->output, .cap = CLI_CELLS_POOL}) + 1u});
#if defined(_WIN32)
    const char *name = args->output;
    const unsigned long long name_past =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->output,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->output, .cap = CLI_CELLS_POOL}), .set = "/\\");
    name = (name_past != 0ull) ? (args->output + name_past) : name;
    if ((name[0] != '\0') &&
        (cellul.chr(&(CatenaFinitaCfg){.src = name + 1, .cap = CLI_CELLS_POOL, .byte = '.'}) == NULL))
    {
        EMBED_CALL(cli_with_extension, SeaBuildCtx, .path = args->output, .extension = "exe", .out = args->made);
    }
#endif
    char folder[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->made, .out = folder) &&
        !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder))
    {
        return EMBED_CALL(cli_fail, SeaBuildCtx, .path = folder, .how = ": it could not be made",
                          .err_msg = args->err_msg);
    }
    unsigned long long major = 0ull;
    unsigned long long minor = 0ull;
    if (!EMBED_CALL(cli_version, SeaBuildCtx, .node = args->node, .major = &major, .minor = &minor,
                    .err_msg = args->err_msg))
    {
        return 0;
    }
    const char *const format = args->module ? "module" : "commonjs";
    char config[CLI_PATH_MAX_LEN];
    char config_slashed[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_with_extension, SeaBuildCtx, .path = args->made, .extension = "sea.json", .out = config);
    memor.cpy(&(MemoriaCfg){.dst = config_slashed,
                            .src = config,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = config, .cap = CLI_CELLS_POOL}) + 1u});
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = config_slashed,
               .size = cellul.len(&(CatenaFinitaCfg){.src = config_slashed, .cap = CLI_CELLS_POOL}), .from = '\\',
               .to = '/');
    const int builds_itself = (major > 25ull) || ((major == 25ull) && (minor >= 5ull));
    if (!builds_itself && (major < 20ull))
    {
        mmgr_line_put(args->err_msg, "Node ");
        mmgr_line_decimal(args->err_msg, major, 1u);
        mmgr_line_ch(args->err_msg, '.');
        mmgr_line_decimal(args->err_msg, minor, 1u);
        mmgr_line_put(args->err_msg, " makes no single executable: Node 20 is the first that does");
        return 0;
    }
#if defined(__APPLE__)
    if (!builds_itself)
    {
        mmgr_line_put(args->err_msg, "on macOS, Node ");
        mmgr_line_decimal(args->err_msg, major, 1u);
        mmgr_line_ch(args->err_msg, '.');
        mmgr_line_decimal(args->err_msg, minor, 1u);
        mmgr_line_put(args->err_msg, " needs postject to add the blob; Node 25.5 builds it itself, with --build-sea");
        return 0;
    }
#endif
    char blob[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_with_extension, SeaBuildCtx, .path = args->made, .extension = "blob", .out = blob);
    VerbaLinea text = {s_config_text, sizeof(s_config_text), 0ull};
    mmgr_line_put(&text, "{\n  \"main\": ");
    EMBED_CALL(cli_slash, SeaBuildCtx, .line = &text, .path = args->main);
    mmgr_line_put(&text, ",\n  \"output\": ");
    EMBED_CALL(cli_slash, SeaBuildCtx, .line = &text, .path = builds_itself ? args->made : blob);
    if (builds_itself || args->module)
    {
        mmgr_line_put(&text, ",\n  \"mainFormat\": \"");
        mmgr_line_put(&text, format);
        mmgr_line_ch(&text, '"');
    }
    mmgr_line_put(&text, ",\n  \"disableExperimentalSEAWarning\": true\n}\n");
    if (!mmgr_line_ok(&text) || !EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx,
                                            .path = config, .bytes = s_config_text, .size = text.at))
    {
        return EMBED_CALL(cli_fail, SeaBuildCtx, .path = config, .how = ": it could not be written",
                          .err_msg = args->err_msg);
    }
    const char *const arguments[2] = {builds_itself ? "--build-sea" : "--experimental-sea-config", config_slashed};
    if (!EMBED_CALL(cli_node_run, SeaBuildCtx, .node = args->node, .request = arguments, .count = 2u,
                    .sink = args->sink, .err_msg = args->err_msg))
    {
        return 0;
    }
    if (builds_itself)
    {
        return 1;
    }
    unsigned long long data_size = 0ull;
    const unsigned char *const data = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = blob,
                                                 .size_pointer = &data_size, .owner = &g_cli_sea_cells);
    if (data == NULL)
    {
        return EMBED_CALL(cli_fail, SeaBuildCtx, .path = blob, .how = ": it could not be read",
                          .err_msg = args->err_msg);
    }
    char print_line[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea copy = {print_line, sizeof(print_line), 0ull};
    mmgr_line_put(&copy, "copy ");
    mmgr_line_put(&copy, args->node);
    mmgr_line_put(&copy, " to ");
    mmgr_line_put(&copy, args->made);
    mmgr_line_finish(&copy);
    EMBED_CALL(cli_print, SeaBuildCtx, .sink = args->sink, .text = print_line);
    unsigned long long size = 0ull;
    const unsigned char *const program = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = args->node,
                                                    .size_pointer = &size, .owner = &g_cli_sea_cells);
    int done = program != NULL;
    if (!done)
    {
        EMBED_CALL(cli_fail, SeaBuildCtx, .path = args->node, .how = ": it could not be read",
                   .err_msg = args->err_msg);
    }
#if defined(_WIN32)
    done =
        done && EMBED_CALL(cli_windows_copy, SeaBuildCtx, .program = program, .size = size, .data = data,
                           .data_size = data_size, .output = args->made, .sink = args->sink, .err_msg = args->err_msg);
#else
    done =
        done && EMBED_CALL(cli_linux_copy, SeaBuildCtx, .program = program, .size = size, .data = data,
                           .data_size = data_size, .output = args->made, .sink = args->sink, .err_msg = args->err_msg);
#endif
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = program, .owner = &g_cli_sea_cells);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = data, .owner = &g_cli_sea_cells);
    if (done)
    {
        EMBED_CALL(cli_print, SeaBuildCtx, .sink = args->sink, .text = "turned the fuse to 1");
    }
    return done;
}
