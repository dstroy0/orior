// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// sea_build.c: a script built into a single executable of Node's, by `node --build-sea` from Node 25.5 and by Node's
// documented steps before it; node's copy written in pieces from its mapping, so node is never read whole
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/text.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "sea.h"

// the max bytes of a script read, and the max modules outside Node reported
#define CLI_SEA_SCRIPT (1u << 22u)

#define CLI_SEA_REQUESTED 64u

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
} SeaBuildCtx;
// a line printed through `sink`
EMBED_INLINE void cli_print(const SeaBuildCtx *args)
{
    EMBED_CALL(args->sink.print, CliEachLineSizeCfg, .context = args->sink.context, .line = args->text,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES}));
}

// `path` with its file's extension, where it has one, put as `extension` into `out`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE void cli_with_extension(const SeaBuildCtx *args)
{
    const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES});
    const unsigned long long name_at = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                                                  .text = args->path, .at = whole, .set = "/\\");
    // a dot counts past the name's first character, so that a name that starts with a dot has no extension for it
    const unsigned long long past_first = (name_at < whole) ? (name_at + 1ULL) : whole;
    const unsigned long long dot_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path + past_first,
                   .at = whole - past_first, .set = ".");
    VerbaLinea line = {args->out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put_n(&line, args->path, (dot_end != 0ULL) ? (past_first + dot_end - 1ULL) : whole);
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
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1u});
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = slashed,
               .size = cellul.len(&(CatenaFinitaCfg){.src = slashed, .cap = CLI_POOL_BYTES}), .from = '\\', .to = '/');
    char quoted[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea json = {quoted, sizeof(quoted), 0ULL};
    mmgr_line_ch(&json, '"');
    EMBED_CALL(cli_text_size_set.append_mark, CliTextSizeSetCfg, .out = &json, .text = slashed, .set = "\"\\",
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
    char displayed[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea line = {displayed, sizeof(displayed), 0ULL};
    mmgr_line_put(&line, "$ node");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_ch(&line, ' ');
        mmgr_line_put(&line, args->request[at]);
    }
    mmgr_line_finish(&line);
    EMBED_CALL(cli_print, SeaBuildCtx, .sink = args->sink, .text = displayed);
    const char *words[8] = {args->node, NULL, NULL, NULL, NULL, NULL, NULL, NULL};
    for (unsigned int at = 0u; (at < count) && (at < 6u); at += 1u)
    {
        words[at + 1u] = args->request[at];
    }
    CliRunResult *const run_result = cli_run_result_alloc();
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = NULL, .settings = NULL, .count = 0u,
                    .run_result = run_result, .err_msg = &unstarted))
    {
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        mmgr_line_put(err_msg, "node: ");
        mmgr_line_put_n(err_msg, why, unstarted.at);
        return 0;
    }
    for (unsigned int stream = 0u; stream < 2u; stream += 1u)
    {
        const char *text = (stream == 0u) ? run_result->out : run_result->err;
        while (text[0] != '\0')
        {
            const char *stop = text;
            stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                               .size = cellul.len(&(CatenaFinitaCfg){.src = stop, .cap = CLI_POOL_BYTES}), .set = "\n");
            const unsigned long long length =
                (unsigned long long)(stop - text) - (((stop > text) && (stop[-1] == '\r')) ? 1ULL : 0ULL);
            EMBED_CALL(args->sink.print, CliEachLineSizeCfg, .context = args->sink.context, .line = text,
                       .size = length);
            text = (stop[0] != '\0') ? (stop + 1) : stop;
        }
    }
    const long long code = run_result->code;
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    if (code != 0LL)
    {
        mmgr_line_put(err_msg, "node ");
        mmgr_line_put(err_msg, (count != 0u) ? args->request[0] : "");
        mmgr_line_put(err_msg, " ended with exit code: ");
        if (code < 0LL)
        {
            mmgr_line_ch(err_msg, '-');
        }
        // the code's size, its sign written apart
        mmgr_line_decimal(err_msg, (unsigned long long)((code < 0LL) ? -code : code), 1u);
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
    CliRunResult *const run_result = cli_run_result_alloc();
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = NULL, .settings = NULL, .count = 0u,
                    .run_result = run_result, .err_msg = &unstarted))
    {
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        mmgr_line_put(err_msg, args->node);
        mmgr_line_put(err_msg, ": ");
        mmgr_line_put_n(err_msg, why, unstarted.at);
        return 0;
    }
    const char *text = run_result->out;
    const char *end = run_result->out + run_result->out_size;
    text += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = text, .at = 0ULL,
                       .size = (unsigned long long)(end - text), .set = CLI_TEXT_WHITE);
    end = text + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = text,
                            .at = (unsigned long long)(end - text), .set = CLI_TEXT_WHITE);
    const char *const trimmed = text;
    text += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = text, .at = 0ULL,
                       .size = (unsigned long long)(end - text), .set = "v");
    // the parts between the dots that are numbers
    unsigned long long numbers[3] = {0ULL, 0ULL, 0ULL};
    unsigned int read = 0u;
    while ((text < end) && (read < 3u))
    {
        const char *stop = text;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                           .size = (unsigned long long)(end - stop), .set = ".");
        int number = stop > text;
        unsigned long long value = 0ULL;
        const char *digit = text;
        while (number && (digit < stop))
        {
            // Explicit cast reads the byte as its code point
            number = ascii.in(&(AsciiCfg){.type = MMGR_ASCII_NUM, .byte = (uint8_t)digit[0]});
            value = (value * 10ULL) + (unsigned long long)(digit[0] - '0');
            digit += 1;
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
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        return 0;
    }
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
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

int cli_sea_build(const CliSeaNodeCfg *args)
{
    static char s_requested[CLI_SEA_REQUESTED][CLI_NAME_MAX_LEN];
    static char s_config_text[CLI_PATH_MAX_LEN * 4u];
    unsigned long long script_size = 0ULL;
    // the script read as its text, the zero of the slack after it
    const unsigned char *const mapped = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->main,
                                                   .size_pointer = &script_size, .owner = g_cli_sea_owner);
    if ((mapped == NULL) || (script_size >= CLI_SEA_SCRIPT))
    {
        g_platform_guard->persistent_buf_release((void *)(embed_word)mapped, g_cli_sea_owner);
        return EMBED_CALL(cli_fail, SeaBuildCtx, .path = args->main, .how = ": it could not be read",
                          .err_msg = args->err_msg);
    }
    const unsigned int required = EMBED_CALL(cli_sea_table.outside, CliSeaCfg, .text = (const char *)mapped,
                                             .found = s_requested, .capacity = CLI_SEA_REQUESTED);
    g_platform_guard->persistent_buf_release((void *)(embed_word)mapped, g_cli_sea_owner);
    if (required != 0u)
    {
        mmgr_line_put(args->err_msg, args->main);
        mmgr_line_put(args->err_msg, " requires ");
        for (unsigned int at = 0u; at < required; at += 1u)
        {
            mmgr_line_put(args->err_msg, (at != 0u) ? ", " : "");
            mmgr_line_put(args->err_msg, s_requested[at]);
        }
        mmgr_line_put(args->err_msg,
                      ", which a single executable of Node's does not contain: it loads Node's own modules alone; "
                      "Bun or Deno bundles them in");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = args->made,
                            .src = args->output,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->output, .cap = CLI_POOL_BYTES}) + 1u});
    // an output named with no extension takes the one a program's file takes here, where it takes one
    const char *const extension = cli_sea_system.program_extension();
    const char *name = args->output;
    const unsigned long long name_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->output,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->output, .cap = CLI_POOL_BYTES}), .set = "/\\");
    name = (name_end != 0ULL) ? (args->output + name_end) : name;
    if ((extension != NULL) && (name[0] != '\0') &&
        (cellul.chr(&(CatenaFinitaCfg){.src = name + 1, .cap = CLI_POOL_BYTES, .byte = '.'}) == NULL))
    {
        EMBED_CALL(cli_with_extension, SeaBuildCtx, .path = args->output, .extension = extension, .out = args->made);
    }
    char folder[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = args->made, .out = folder) &&
        !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder))
    {
        return EMBED_CALL(cli_fail, SeaBuildCtx, .path = folder, .how = ": it could not be made",
                          .err_msg = args->err_msg);
    }
    unsigned long long major = 0ULL;
    unsigned long long minor = 0ULL;
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
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = config, .cap = CLI_POOL_BYTES}) + 1u});
    EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = config_slashed,
               .size = cellul.len(&(CatenaFinitaCfg){.src = config_slashed, .cap = CLI_POOL_BYTES}), .from = '\\',
               .to = '/');
    const int builds_itself = (major > 25ULL) || ((major == 25ULL) && (minor >= 5ULL));
    if (!builds_itself && (major < 20ULL))
    {
        mmgr_line_put(args->err_msg, "Node ");
        mmgr_line_decimal(args->err_msg, major, 1u);
        mmgr_line_ch(args->err_msg, '.');
        mmgr_line_decimal(args->err_msg, minor, 1u);
        mmgr_line_put(args->err_msg, " makes no single executable: Node 20 is the first that does");
        return 0;
    }
    if (!builds_itself && !EMBED_CALL(cli_sea_system.older_node, CliSeaSystemCfg, .major = major, .minor = minor,
                                      .err_msg = args->err_msg))
    {
        return 0;
    }
    char blob[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_with_extension, SeaBuildCtx, .path = args->made, .extension = "blob", .out = blob);
    VerbaLinea text = {s_config_text, sizeof(s_config_text), 0ULL};
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
    unsigned long long data_size = 0ULL;
    const unsigned char *const data = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = blob,
                                                 .size_pointer = &data_size, .owner = g_cli_sea_owner);
    if (data == NULL)
    {
        return EMBED_CALL(cli_fail, SeaBuildCtx, .path = blob, .how = ": it could not be read",
                          .err_msg = args->err_msg);
    }
    char print_line[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea copy = {print_line, sizeof(print_line), 0ULL};
    mmgr_line_put(&copy, "copy ");
    mmgr_line_put(&copy, args->node);
    mmgr_line_put(&copy, " to ");
    mmgr_line_put(&copy, args->made);
    mmgr_line_finish(&copy);
    EMBED_CALL(cli_print, SeaBuildCtx, .sink = args->sink, .text = print_line);
    unsigned long long size = 0ULL;
    const unsigned char *const program = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->node,
                                                    .size_pointer = &size, .owner = g_cli_sea_owner);
    int done = program != NULL;
    if (!done)
    {
        EMBED_CALL(cli_fail, SeaBuildCtx, .path = args->node, .how = ": it could not be read",
                   .err_msg = args->err_msg);
    }
    done =
        done && EMBED_CALL(cli_sea_system.copy, CliSeaSystemCfg, .program = program, .size = size, .data = data,
                           .data_size = data_size, .output = args->made, .sink = args->sink, .err_msg = args->err_msg);
    g_platform_guard->persistent_buf_release((void *)(embed_word)program, g_cli_sea_owner);
    g_platform_guard->persistent_buf_release((void *)(embed_word)data, g_cli_sea_owner);
    if (done)
    {
        EMBED_CALL(cli_print, SeaBuildCtx, .sink = args->sink, .text = "turned the fuse to 1");
    }
    return done;
}
