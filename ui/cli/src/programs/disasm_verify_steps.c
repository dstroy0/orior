// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// disasm_verify_steps.c: the verification's steps, each a program run with the toolchains' PATH within its patience:
// the tree's command line built with link-time optimization, the module's object lifted to IR and its functions listed,
// their symbols listed from the program, the symbols' instructions read by the disassembler and by Ghidra's analyzer
// running the plugin's script, and the program executed. A program that ends with a code other than 0 is reported with
// its errors' first bytes
#include "../command_line/command_line_internal.h"
#include "../command_line/pool.h"
#include "../runs/runner.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "disasm_verify.h"

// how long the build, each reading and Ghidra's analysis may take, in milliseconds
#define CLI_DISASM_BUILD_PATIENCE (30ULL * 60ULL * 1000ULL)

#define CLI_DISASM_READ_PATIENCE (5ULL * 60ULL * 1000ULL)

#define CLI_DISASM_GHIDRA_PATIENCE (30ULL * 60ULL * 1000ULL)

// the longest run PATH, and the bytes of the disassembler's flag before its symbols' list
#define CLI_DISASM_PATH_TEXT (1u << 15u)

#define CLI_DISASM_LIST_FLAG 32ULL

// the words of the step's program and the PATH it runs with
static CliWords s_words;
static char s_run_path[CLI_DISASM_PATH_TEXT];

void cli_disasm_steps_begin(void)
{
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = s_run_path,
               .size = sizeof(s_run_path));
}

// the words begun anew
static void verify_words_begin(void)
{
    s_words.count = 0u;
    s_words.used = 0u;
    s_words.full = 0;
    s_words.word_text[0] = NULL;
}

typedef struct
{
    const char *folder;
    const CliSetting *settings;
    unsigned int count;
    unsigned long long patience;
    VerbaLinea *err_msg;
    const char *text;
    unsigned long long size;
    char (*names)[CLI_NAME_MAX_LEN];
    unsigned int capacity;
} DisasmVerifyStepsCtx;
// a program run in `folder` with the toolchains' PATH and `settings` besides, within `patience` ms: what it gave, or
// NULL with why given; a program that ends with a code other than 0 is reported with its errors' first bytes
EMBED_INLINE CliRunResult *verify_run(const DisasmVerifyStepsCtx *args)
{
    const char *folder = args->folder;
    const CliSetting *settings = args->settings;
    unsigned int count = args->count;
    VerbaLinea *err_msg = args->err_msg;
    CliRunResult *const run_result = cli_run_result_alloc();
    if (!EMBED_CALL(cli_words_internal.run_result_within, CliWordsCtx, .words = s_words.word_text, .folder = folder,
                    .settings = settings, .count = count, .input = NULL, .length = 0ULL,
                    .milliseconds_u64 = args->patience, .run_result = run_result, .err_msg = err_msg))
    {
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        return NULL;
    }
    if (run_result->code != 0)
    {
        mmgr_line_put(err_msg, s_words.word_text[0]);
        mmgr_line_put(err_msg, run_result->stopped ? " ran past its time" : " ended with ");
        if (!run_result->stopped)
        {
            // an exit code is small and positive where it is not a signal's -1
            mmgr_line_decimal(err_msg, (run_result->code < 0) ? 0ULL : (unsigned long long)run_result->code, 1u);
        }
        mmgr_line_put(err_msg, ": ");
        mmgr_line_put_n(err_msg, run_result->err, (run_result->err_size < 600ULL) ? run_result->err_size : 600ULL);
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        return NULL;
    }
    return run_result;
}

int cli_disasm_adjacent(const CliDisasmVerifyPathCfg *args)
{
    const char *const *names = args->names_characters;
    char *path = args->path;
    char folder[CLI_PATH_MAX_LEN];
    unsigned int count = 0u;
    while (names[count] != NULL)
    {
        count += 1u;
    }
    if (EMBED_CALL(cli_path_out_type_internal.path_parent, CliPathOutTypeCtx, .path = args->compiler, .out = folder) &&
        EMBED_CALL(cli_toolchains_dir.program_in, CliToolchainsDirCfg, .dir = folder, .names = names, .count = count,
                   .path = path))
    {
        return 1;
    }
    mmgr_line_put(args->err_msg, names[0]);
    mmgr_line_put(args->err_msg, " is not beside ");
    mmgr_line_put(args->err_msg, args->compiler);
    return 0;
}

int cli_disasm_build(const CliDisasmVerifyCompilerCfg *args)
{
    VerbaLinea *err_msg = args->err_msg;
    char bash[CLI_PATH_MAX_LEN];
    char script[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_runner_name.bash, CliRunnerNameCfg, .path = bash, .err_msg = err_msg) ||
        !EMBED_CALL(cli_disasm_verify.disasm_path, CliDisasmVerifyCfg, .path = script,
                    .parts = (const char *const[]){args->top, "/ui/cli/build.sh", NULL}))
    {
        return 0;
    }
    verify_words_begin();
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = bash);
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = script);
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = args->target);
    const CliSetting settings[2] = {{"PATH", s_run_path}, {"CC", args->compiler}};
    CliRunResult *const run_result =
        EMBED_CALL(verify_run, DisasmVerifyStepsCtx, .folder = args->top, .settings = settings, .count = 2u,
                   .patience = CLI_DISASM_BUILD_PATIENCE, .err_msg = err_msg);
    if (run_result == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    return 1;
}

int cli_disasm_lifted(const CliDisasmVerifyIrCfg *args)
{
    VerbaLinea *err_msg = args->err_msg;
    verify_words_begin();
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = args->lifter);
    if (args->gnu)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word_text = "-list");
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word_text = args->object);
    }
    else
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word_text = args->object);
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word_text = "-o");
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word_text = args->ir);
    }
    const CliSetting settings[1] = {{"PATH", s_run_path}};
    CliRunResult *const run_result = EMBED_CALL(verify_run, DisasmVerifyStepsCtx, .folder = NULL, .settings = settings,
                                                .count = 1u, .patience = CLI_DISASM_READ_PATIENCE, .err_msg = err_msg);
    if (run_result == NULL)
    {
        return 0;
    }
    // GNU's lifter prints the list; it is the IR file
    const int written =
        !args->gnu || EMBED_CALL(cli_path_bytes_internal.private_file_write, CliPathBytesCtx, .path = args->ir,
                                 .bytes = run_result->out, .size = run_result->out_size);
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    if (!written)
    {
        mmgr_line_put(err_msg, args->ir);
        mmgr_line_put(err_msg, " could not be written");
    }
    return written;
}

// the functions GNU's list `text`, `size` bytes, names, each line of a `T` symbol, into `names` up to `capacity`, or
// counted alone with `names` NULL: how many
EMBED_INLINE unsigned int verify_listed_functions(const DisasmVerifyStepsCtx *args)
{
    unsigned int count = 0u;
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        const unsigned long long end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg,
                                                  .text = args->text, .at = at, .size = args->size, .set = "\n");
        const unsigned long long word_text =
            at + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->text + at,
                            .at = end - at, .set = " \t");
        const int function =
            ((end - word_text) > 0ULL) && ((end - word_text) < CLI_NAME_MAX_LEN) && (args->text[at] == 'T');
        if (function && (args->names != NULL) && (count < args->capacity))
        {
            memor.cpy(
                &(MemoriaCfg){.dst = args->names[count], .src = args->text + word_text, .bytes = end - word_text});
            args->names[count][end - word_text] = '\0';
        }
        count += (function && ((args->names == NULL) || (count < args->capacity))) ? 1u : 0u;
        at = end + 1ULL;
    }
    return count;
}

unsigned int cli_disasm_functions(const CliDisasmVerifyIrCfg *args)
{
    VerbaLinea *err_msg = args->err_msg;
    unsigned long long size = 0ULL;
    const unsigned char *const text = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->ir,
                                                 .size_pointer = &size, .owner = g_cli_programs_owner);
    if (text == NULL)
    {
        mmgr_line_put(err_msg, args->ir);
        mmgr_line_put(err_msg, " was not written by the lifter");
        return 0u;
    }
    const unsigned int count =
        args->gnu ? EMBED_CALL(verify_listed_functions, DisasmVerifyStepsCtx, .text = (const char *)text, .size = size,
                               .names = args->names, .capacity = args->capacity)
                  : EMBED_CALL(cli_disasm_verify_ir_size.disasm_functions_define, CliDisasmVerifyIrSizeCfg,
                               .ir = (const char *)text, .size = size, .names = args->names, .capacity = args->capacity);
    g_cli_guard->persistent_buf_release((void *)(embed_word)text, (g_cli_programs_owner));
    if (count == 0u)
    {
        mmgr_line_put(err_msg, "the module defines no function");
    }
    return count;
}

unsigned int cli_disasm_symbols(const CliDisasmVerifyCountErrMsgListerCfg *args)
{
    unsigned int count = args->count;
    VerbaLinea *err_msg = args->err_msg;
    verify_words_begin();
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = args->lister);
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "--defined-only");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = args->program);
    const CliSetting settings[1] = {{"PATH", s_run_path}};
    CliRunResult *const run_result = EMBED_CALL(verify_run, DisasmVerifyStepsCtx, .folder = NULL, .settings = settings,
                                                .count = 1u, .patience = CLI_DISASM_READ_PATIENCE, .err_msg = err_msg);
    if (run_result == NULL)
    {
        return 0u;
    }
    unsigned int found_count = 0u;
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        if (EMBED_CALL(cli_disasm_verify_size_text.disasm_symbol_get, CliDisasmVerifySizeTextCfg,
                       .text = run_result->out, .size = run_result->out_size, .name = args->names_callback[each],
                       .symbol_out = args->symbols_callback[each]))
        {
            found_count += 1u;
        }
        else
        {
            args->symbols_callback[each][0] = '\0';
        }
    }
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    return found_count;
}

CliRunResult *cli_disasm_disassemble(const CliDisasmVerifyCountErrMsgCfg *args)
{
    unsigned int count = args->count;
    VerbaLinea *err_msg = args->err_msg;
    // the symbols' list in a cell of the call sized to them, each with its comma, after the flag
    const unsigned long long list_bytes =
        (((unsigned long long)count + 1ULL) * (CLI_NAME_MAX_LEN + 1ULL)) + CLI_DISASM_LIST_FLAG;
    char *const symbol_list = g_cli_guard->temporary_buf_alloc(list_bytes, g_cli_programs_owner);
    if (symbol_list == NULL)
    {
        mmgr_line_put(err_msg, "the symbols' list could not be taken");
        return NULL;
    }
    VerbaLinea list_entries = {symbol_list, list_bytes, 0ULL};
    mmgr_line_put(&list_entries, "--disassemble-symbols=");
    int any = 0;
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        if (args->symbols[each][0] != '\0')
        {
            mmgr_line_put(&list_entries, any ? "," : "");
            mmgr_line_put(&list_entries, args->symbols[each]);
            any = 1;
        }
    }
    if (!mmgr_line_finish(&list_entries))
    {
        mmgr_line_put(err_msg, "the symbols do not fit one disassembler command");
        g_cli_guard->persistent_buf_release(symbol_list, g_cli_programs_owner);
        return NULL;
    }
    verify_words_begin();
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = args->disassembler);
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "-d");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "-M");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "intel");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = symbol_list);
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = args->program);
    // the words copy the list: its cell goes back before the run
    g_cli_guard->persistent_buf_release(symbol_list, g_cli_programs_owner);
    const CliSetting settings[1] = {{"PATH", s_run_path}};
    CliRunResult *const run_result = EMBED_CALL(verify_run, DisasmVerifyStepsCtx, .folder = NULL, .settings = settings,
                                                .count = 1u, .patience = CLI_DISASM_READ_PATIENCE, .err_msg = err_msg);
    if (run_result != NULL)
    {
        (void)EMBED_CALL(cli_path_bytes_internal.private_file_write, CliPathBytesCtx, .path = args->into,
                         .bytes = run_result->out, .size = run_result->out_size);
    }
    return run_result;
}

const unsigned char *cli_disasm_ghidra(const CliDisasmVerifyCountCfg *args)
{
    const char *folder = args->folder;
    unsigned int count = args->count;
    unsigned long long *size = args->size_pointer;
    VerbaLinea *err_msg = args->err_msg;
    char source[CLI_PATH_MAX_LEN];
    char scripts[CLI_PATH_MAX_LEN];
    char script[CLI_PATH_MAX_LEN];
    char names[CLI_PATH_MAX_LEN];
    char project[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_disasm_verify.disasm_path, CliDisasmVerifyCfg, .path = source,
                    .parts = (const char *const[]){args->top, "/ui/plugins/ghidra/FunctionsDisassembled.java", NULL}) ||
        !EMBED_CALL(cli_disasm_verify.disasm_path, CliDisasmVerifyCfg, .path = scripts,
                    .parts = (const char *const[]){folder, "/ghidra_scripts", NULL}) ||
        !EMBED_CALL(cli_disasm_verify.disasm_path, CliDisasmVerifyCfg, .path = script,
                    .parts = (const char *const[]){folder, "/ghidra_scripts/FunctionsDisassembled.java", NULL}) ||
        !EMBED_CALL(cli_disasm_verify.disasm_path, CliDisasmVerifyCfg, .path = names,
                    .parts = (const char *const[]){folder, "/functions.txt", NULL}) ||
        !EMBED_CALL(cli_disasm_verify.disasm_path, CliDisasmVerifyCfg, .path = project,
                    .parts = (const char *const[]){folder, "/ghidra_project", NULL}))
    {
        mmgr_line_put(err_msg, "the loop's paths do not fit");
        return NULL;
    }
    unsigned long long script_size = 0ULL;
    const unsigned char *const script_text = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = source,
                                                        .size_pointer = &script_size, .owner = g_cli_programs_owner);
    if (script_text == NULL)
    {
        mmgr_line_put(err_msg, source);
        mmgr_line_put(err_msg, " could not be read");
        return NULL;
    }
    const int made = EMBED_CALL(cli_words_err_msg_internal.private_folder_init, CliWordsErrMsgCtx, .folder = scripts) &&
                     EMBED_CALL(cli_words_err_msg_internal.private_folder_init, CliWordsErrMsgCtx, .folder = project) &&
                     EMBED_CALL(cli_path_bytes_internal.private_file_write, CliPathBytesCtx, .path = script,
                                .bytes = script_text, .size = script_size);
    g_cli_guard->persistent_buf_release((void *)(embed_word)script_text, (g_cli_programs_owner));
    // the symbols' names a line each, in a cell of the call sized to them
    const unsigned long long names_bytes = ((unsigned long long)count + 1ULL) * (CLI_NAME_MAX_LEN + 1ULL);
    char *const names_text = g_cli_guard->temporary_buf_alloc(names_bytes, g_cli_programs_owner);
    if (names_text == NULL)
    {
        mmgr_line_put(err_msg, "the names' list could not be taken");
        return NULL;
    }
    VerbaLinea list_entries = {names_text, names_bytes, 0ULL};
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        if (args->symbols[each][0] != '\0')
        {
            mmgr_line_put(&list_entries, args->symbols[each]);
            mmgr_line_ch(&list_entries, '\n');
        }
    }
    const int listed = made && mmgr_line_ok(&list_entries) &&
                       EMBED_CALL(cli_path_bytes_internal.private_file_write, CliPathBytesCtx, .path = names,
                                  .bytes = names_text, .size = list_entries.at);
    g_cli_guard->persistent_buf_release(names_text, g_cli_programs_owner);
    if (!listed)
    {
        mmgr_line_put(err_msg, "the script and the names could not be written under ");
        mmgr_line_put(err_msg, folder);
        return NULL;
    }
    verify_words_begin();
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = args->analyzer);
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = project);
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "verify");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "-import");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = args->program);
    for (unsigned int each = 0u; args->loader[each] != NULL; each += 1u)
    {
        EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word_text = args->loader[each]);
    }
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "-overwrite");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "-deleteProject");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "-noanalysis");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "-scriptPath");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = scripts);
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "-postScript");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "FunctionsDisassembled.java");
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = names);
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = args->into);
    // the output emptied first, so that a script the analyzer could not run leaves nothing to read, not an older run's
    (void)EMBED_CALL(cli_path_bytes_internal.private_file_write, CliPathBytesCtx, .path = args->into, .bytes = "",
                     .size = 0ULL);
    const CliSetting settings[1] = {{"PATH", s_run_path}};
    CliRunResult *const run_result =
        EMBED_CALL(verify_run, DisasmVerifyStepsCtx, .folder = folder, .settings = settings, .count = 1u,
                   .patience = CLI_DISASM_GHIDRA_PATIENCE, .err_msg = err_msg);
    if (run_result == NULL)
    {
        return NULL;
    }
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    const unsigned char *const text = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->into,
                                                 .size_pointer = size, .owner = g_cli_programs_owner);
    if ((text == NULL) || (*size == 0ULL))
    {
        g_cli_guard->persistent_buf_release((void *)(embed_word)text, (g_cli_programs_owner));
        mmgr_line_put(err_msg, "Ghidra's script wrote nothing to ");
        mmgr_line_put(err_msg, args->into);
        mmgr_line_put(err_msg, ": the analyzer's log in the run's errors explains why");
        return NULL;
    }
    return text;
}

int cli_disasm_execute(const CliDisasmVerifyCountErrMsgCfg *args)
{
    verify_words_begin();
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = args->program);
    EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word_text = "help");
    const CliSetting settings[1] = {{"PATH", s_run_path}};
    CliRunResult *const run_result =
        EMBED_CALL(verify_run, DisasmVerifyStepsCtx, .folder = NULL, .settings = settings, .count = 1u,
                   .patience = CLI_DISASM_READ_PATIENCE, .err_msg = args->err_msg);
    if (run_result == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    return 1;
}
