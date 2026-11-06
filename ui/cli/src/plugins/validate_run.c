// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// validate_run.c: one check of the copy: the words that run the prover on it, the limits' settings just before the
// file, and the check run within the tool's patience, its errors after its output
#include "../text/text.h"
#include "validate_internal.h"

typedef struct
{
    CliWords *words;
    const char *text;
    const char *file;
} ValidateRunCtx;

// `text` with every `{file}` in it written as `file`, put after the words
EMBED_INLINE void validate_fill(const ValidateRunCtx *args)
{
    CliWords *words = args->words;
    const char *text = args->text;
    char filled[CLI_PATH_TEXT];
    VerbaLinea line = {filled, sizeof(filled), 0ULL};
    EMBED_CALL(cli_text_out_name.append_fill, CliTextOutNameCfg, .out = &line, .text = text, .name = "{file}",
               .value = args->file);
    mmgr_line_finish(&line);
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
               .word_text = filled);
}

int cli_validate_command_for(const CliValidateProgramCtx *args)
{
    unsigned int *from = args->from;
    unsigned int *settings = args->settings;
    char *dir = args->dir;
    const CliJson *const json = &g_cli_validate->json;
    const CliTool *const tool = &g_cli_validate->tool;
    static CliWords s_named;
    s_named.count = 0u;
    s_named.used = 0u;
    s_named.full = 0;
    const int inside = (args->project != NULL) &&
                       (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = json,
                                   .array = tool->in_project, .index = 0u) != CLI_JSON_NONE);
    const unsigned int list_entries = inside ? tool->in_project : tool->args;
    if (!inside)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_named,
                   .word_text = tool->program);
    }
    for (unsigned int index = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                             .json_read = json, .array = list_entries, .index = index) != CLI_JSON_NONE;
         index += 1u)
    {
        EMBED_CALL(validate_fill, ValidateRunCtx, .words = &s_named,
                   .text =
                       EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                                  .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                     .json_read = json, .array = list_entries, .index = index),
                                  .length_pointer = NULL),
                   .file = args->file);
    }
    if (inside)
    {
        (void)cellul.copy(&(CatenaFinitaCfg){.dst = dir, .src = args->project, .cap = CLI_PATH_MAX_LEN});
    }
    else if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->file, .out = dir))
    {
        dir[0] = '\0';
    }
    char runs[CLI_PATH_MAX_LEN];
    char folder[CLI_PATH_MAX_LEN];
    const char *const first = s_named.word_text[0];
    if (cellul.eq(&(CatenaFinitaCfg){.src = first, .other = tool->program, .cap = CLI_POOL_BYTES}))
    {
        memor.cpy(
            &(MemoriaCfg){.dst = runs,
                          .src = args->program,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->program, .cap = CLI_POOL_BYTES}) + 1u});
    }
    else if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->program,
                         .out = folder) ||
             !EMBED_CALL(cli_toolchains_dir.program_in, CliToolchainsDirCfg, .dir = folder, .names = &first,
                         .count = 1u, .path = runs))
    {
        mmgr_line_put(args->err_msg, tool->name);
        mmgr_line_put(args->err_msg, " has no ");
        mmgr_line_put(args->err_msg, first);
        mmgr_line_put(args->err_msg, " beside ");
        mmgr_line_put(args->err_msg, args->program);
        return 0;
    }
    unsigned int at = 1u;
    while ((at < s_named.count) &&
           (!cellul.eq(&(CatenaFinitaCfg){.src = s_named.word_text[at], .other = args->file, .cap = CLI_POOL_BYTES})))
    {
        at += 1u;
    }
    args->words->count = 0u;
    args->words->used = 0u;
    args->words->full = 0;
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
               .word_text = runs);
    for (unsigned int word_text = 1u; word_text < at; word_text += 1u)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = s_named.word_text[word_text]);
    }
    unsigned int keys[CLI_VALIDATE_BARRIERS];
    const unsigned int grown = EMBED_CALL(cli_validate_internal.grown_keys, CliValidateCtx, .keys = keys);
    *from = args->words->count;
    *settings = grown;
    for (unsigned int key = 0u; key < grown; key += 1u)
    {
        char value[24];
        VerbaLinea value_line = {value, sizeof(value), 0ULL};
        mmgr_line_decimal(&value_line, g_cli_validate->grown[keys[key]], 1u);
        mmgr_line_finish(&value_line);
        char setting[CLI_PATH_MAX_LEN];
        VerbaLinea line = {setting, sizeof(setting), 0ULL};
        EMBED_CALL(cli_text_out_name.append_fill, CliTextOutNameCfg, .out = &line,
                   .text = tool->barriers[keys[key]].lift, .name = "{value}", .value = value);
        mmgr_line_finish(&line);
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = setting);
    }
    for (unsigned int word_text = at; word_text < s_named.count; word_text += 1u)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = args->words,
                   .word_text = s_named.word_text[word_text]);
    }
    return 1;
}

int cli_validate_check(const CliValidateErrMsgCtx *args)
{
    double *seconds = args->seconds;
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = g_cli_validate->path,
               .size = CLI_PATH_TEXT);
    const CliSetting settings[1] = {{"PATH", g_cli_validate->path}};
    CliRunResult *const run_result = cli_run_result_alloc();
    const unsigned long long began = cli_clock_milliseconds();
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ULL};
    const int started =
        EMBED_CALL(cli_words_internal.run_result_within, CliWordsCtx, .words = args->words_read->word_text,
                   .folder = (args->dir_read[0] != '\0') ? args->dir_read : NULL, .settings = settings, .count = 1u,
                   .input = NULL, .length = 0ULL, .milliseconds_u64 = g_cli_validate->tool.patience * 1000ULL,
                   .run_result = run_result, .err_msg = &unstarted);
    *seconds = (double)(cli_clock_milliseconds() - began) / 1000.0;
    if (!started)
    {
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        mmgr_line_put(args->err_msg, "the prover did not start: ");
        mmgr_line_put_n(args->err_msg, why, unstarted.at);
        return 0;
    }
    // an output that is not UTF-8 reads as nothing, as Rust's read_to_string leaves it
    const unsigned long long out = EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg,
                                              .bytes = run_result->out, .size = run_result->out_size)
                                       ? run_result->out_size
                                       : 0ULL;
    const unsigned long long err = EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg,
                                              .bytes = run_result->err, .size = run_result->err_size)
                                       ? run_result->err_size
                                       : 0ULL;
    // the output stored in a cell of its size, the one stored before given back
    g_cli_guard->persistent_buf_release(g_cli_validate->output, g_cli_plugins_owner);
    g_cli_validate->output_size = 0ULL;
    g_cli_validate->output = g_cli_guard->persistent_buf_alloc(out + err + 2ULL, g_cli_plugins_owner);
    if (g_cli_validate->output == NULL)
    {
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        mmgr_line_put(args->err_msg, "a check's output could not be stored");
        return 0;
    }
    memor.cpy(&(MemoriaCfg){.dst = g_cli_validate->output, .src = run_result->out, .bytes = out});
    unsigned long long used = out;
    unsigned long long blank = 0ULL;
    while ((blank < err) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\r\n\f\v", .cap = CLI_POOL_BYTES, .byte = run_result->err[blank]}) != NULL))
    {
        blank += 1ULL;
    }
    if (blank < err)
    {
        g_cli_validate->output[used] = '\n';
        memor.cpy(&(MemoriaCfg){.dst = g_cli_validate->output + used + 1ULL, .src = run_result->err, .bytes = err});
        used += 1ULL + err;
    }
    g_cli_validate->output_size = used;
    g_cli_validate->late = run_result->stopped;
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    return 1;
}
