// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// disasm_verify_steps.c: the verification's steps, each a program run with the toolchains' PATH within its patience:
// the tree's command line built with link-time optimization, the module's object lifted to IR and its functions listed,
// their symbols listed from the program, the symbols' instructions read by the disassembler and by Ghidra's analyzer
// running the plugin's script, and the program executed. A program that ends with a code other than 0 is reported with
// its errors' first bytes
#include "../command_line/cells.h"
#include "../command_line/command_line_internal.h"
#include "../runs/runner.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "disasm_verify.h"

// how long the build, each reading and Ghidra's analysis may take, in milliseconds
#define CLI_DISASM_BUILD_PATIENCE (30ull * 60ull * 1000ull)

#define CLI_DISASM_READ_PATIENCE (5ull * 60ull * 1000ull)

#define CLI_DISASM_GHIDRA_PATIENCE (30ull * 60ull * 1000ull)

// the longest run PATH
#define CLI_DISASM_PATH_TEXT (1u << 15u)

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
    s_words.word[0] = NULL;
}

typedef struct
{
    const char *folder;
    const CliSetting *settings;
    unsigned int count;
    unsigned long long patience;
    VerbaLinea *err_msg;
} DisasmVerifyStepsCtx;
// a program run in `folder` with the toolchains' PATH and `settings` besides, within `patience` ms: what it gave, or
// NULL with why given; a program that ends with a code other than 0 is reported with its errors' first bytes
EMBED_INLINE CliRan *verify_run(const DisasmVerifyStepsCtx *args)
{
    const char *folder = args->folder;
    const CliSetting *settings = args->settings;
    unsigned int count = args->count;
    VerbaLinea *err_msg = args->err_msg;
    CliRan *const ran = cli_ran_take();
    if (!EMBED_CALL(cli_words_internal.ran_within, CliWordsCtx, .words = s_words.word, .folder = folder,
                    .settings = settings, .count = count, .input = NULL, .length = 0ull,
                    .milliseconds_u64 = args->patience, .ran = ran, .err_msg = err_msg))
    {
        EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
        return NULL;
    }
    if (ran->code != 0)
    {
        mmgr_line_put(err_msg, s_words.word[0]);
        mmgr_line_put(err_msg, ran->stopped ? " ran past its time" : " ended with ");
        if (!ran->stopped)
        {
            // an exit code is small and positive where it is not a signal's -1
            mmgr_line_decimal(err_msg, (ran->code < 0) ? 0ull : (unsigned long long)ran->code, 1u);
        }
        mmgr_line_put(err_msg, ": ");
        mmgr_line_put_n(err_msg, ran->err, (ran->err_size < 600ull) ? ran->err_size : 600ull);
        EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
        return NULL;
    }
    return ran;
}

int cli_disasm_beside(const CliDisasmVerifyPathCfg *args)
{
    const char *const *names = args->names_letters;
    char *path = args->path;
    char folder[CLI_PATH_MAX_LEN];
    unsigned int count = 0u;
    while (names[count] != NULL)
    {
        count += 1u;
    }
    if (EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->compiler, .out = folder) &&
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
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words, .word = bash);
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words, .word = script);
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word = args->target);
    const CliSetting settings[2] = {{"PATH", s_run_path}, {"CC", args->compiler}};
    CliRan *const ran = EMBED_CALL(verify_run, DisasmVerifyStepsCtx, .folder = args->top, .settings = settings,
                                   .count = 2u, .patience = CLI_DISASM_BUILD_PATIENCE, .err_msg = err_msg);
    if (ran == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    return 1;
}

unsigned int cli_disasm_functions(const CliDisasmVerifyIrCfg *args)
{
    VerbaLinea *err_msg = args->err_msg;
    verify_words_begin();
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word = args->lifter);
    if (args->gnu)
    {
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word = "-list");
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word = args->object);
    }
    else
    {
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word = args->object);
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words, .word = "-o");
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word = args->ir);
    }
    const CliSetting settings[1] = {{"PATH", s_run_path}};
    CliRan *const ran = EMBED_CALL(verify_run, DisasmVerifyStepsCtx, .folder = NULL, .settings = settings, .count = 1u,
                                   .patience = CLI_DISASM_READ_PATIENCE, .err_msg = err_msg);
    if (ran == NULL)
    {
        return 0u;
    }
    unsigned int count = 0u;
    if (args->gnu)
    {
        (void)EMBED_CALL(cli_path_bytes_internal.private_file_write, CliPathBytesCtx, .path = args->ir,
                         .bytes = ran->out, .size = ran->out_size);
        for (unsigned long long at = 0ull; (at < ran->out_size) && (count < CLI_DISASM_FUNCTIONS);)
        {
            unsigned long long end = at;
            end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = ran->out, .at = end,
                             .size = ran->out_size, .set = "\n");
            unsigned long long word = end;
            word = (at + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = ran->out + at,
                                    .at = word - at, .set = " \t"));
            const int function = ((end - word) > 0ull) && ((end - word) < CLI_NAME_MAX_LEN) && (ran->out[at] == 'T');
            if (function)
            {
                memor.cpy(&(MemoriaCfg){.dst = args->names[count], .src = ran->out + word, .bytes = end - word});
                args->names[count][end - word] = '\0';
                count += 1u;
            }
            at = end + 1ull;
        }
    }
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    if (!args->gnu)
    {
        unsigned long long size = 0ull;
        const unsigned char *const text = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = args->ir,
                                                     .size_pointer = &size, .owner = &g_cli_programs_cells);
        if (text == NULL)
        {
            mmgr_line_put(err_msg, args->ir);
            mmgr_line_put(err_msg, " was not written by the lifter");
            return 0u;
        }
        count = EMBED_CALL(cli_disasm_verify_ir_size.disasm_functions_define, CliDisasmVerifyIrSizeCfg,
                           .ir = (const char *)text, .size = size, .names = args->names, .room = CLI_DISASM_FUNCTIONS);
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = text,
                   .owner = &g_cli_programs_cells);
    }
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
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word = args->lister);
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word = "--defined-only");
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word = args->program);
    const CliSetting settings[1] = {{"PATH", s_run_path}};
    CliRan *const ran = EMBED_CALL(verify_run, DisasmVerifyStepsCtx, .folder = NULL, .settings = settings, .count = 1u,
                                   .patience = CLI_DISASM_READ_PATIENCE, .err_msg = err_msg);
    if (ran == NULL)
    {
        return 0u;
    }
    unsigned int found_count = 0u;
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        if (EMBED_CALL(cli_disasm_verify_size_text.disasm_symbol_get, CliDisasmVerifySizeTextCfg, .text = ran->out,
                       .size = ran->out_size, .name = args->names_callback[each],
                       .symbol_out = args->symbols_callback[each]))
        {
            found_count += 1u;
        }
        else
        {
            args->symbols_callback[each][0] = '\0';
        }
    }
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    return found_count;
}

CliRan *cli_disasm_disassemble(const CliDisasmVerifyCountErrMsgCfg *args)
{
    unsigned int count = args->count;
    VerbaLinea *err_msg = args->err_msg;
    static char s_symbol_list[CLI_DISASM_FUNCTIONS * CLI_NAME_MAX_LEN];
    VerbaLinea listed = {s_symbol_list, sizeof(s_symbol_list), 0ull};
    mmgr_line_put(&listed, "--disassemble-symbols=");
    int any = 0;
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        if (args->symbols[each][0] != '\0')
        {
            mmgr_line_put(&listed, any ? "," : "");
            mmgr_line_put(&listed, args->symbols[each]);
            any = 1;
        }
    }
    if (!mmgr_line_finish(&listed))
    {
        mmgr_line_put(err_msg, "the symbols do not fit one disassembler command");
        return NULL;
    }
    verify_words_begin();
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word = args->disassembler);
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words, .word = "-d");
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words, .word = "-M");
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words, .word = "intel");
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word = s_symbol_list);
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word = args->program);
    const CliSetting settings[1] = {{"PATH", s_run_path}};
    CliRan *const ran = EMBED_CALL(verify_run, DisasmVerifyStepsCtx, .folder = NULL, .settings = settings, .count = 1u,
                                   .patience = CLI_DISASM_READ_PATIENCE, .err_msg = err_msg);
    if (ran != NULL)
    {
        (void)EMBED_CALL(cli_path_bytes_internal.private_file_write, CliPathBytesCtx, .path = args->into,
                         .bytes = ran->out, .size = ran->out_size);
    }
    return ran;
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
    unsigned long long script_size = 0ull;
    const unsigned char *const script_text = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = source,
                                                        .size_pointer = &script_size, .owner = &g_cli_programs_cells);
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
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = script_text,
               .owner = &g_cli_programs_cells);
    static char s_names_text[CLI_DISASM_FUNCTIONS * CLI_NAME_MAX_LEN];
    VerbaLinea listed = {s_names_text, sizeof(s_names_text), 0ull};
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        if (args->symbols[each][0] != '\0')
        {
            mmgr_line_put(&listed, args->symbols[each]);
            mmgr_line_ch(&listed, '\n');
        }
    }
    if (!made || !mmgr_line_ok(&listed) ||
        !EMBED_CALL(cli_path_bytes_internal.private_file_write, CliPathBytesCtx, .path = names, .bytes = s_names_text,
                    .size = listed.at))
    {
        mmgr_line_put(err_msg, "the script and the names could not be written under ");
        mmgr_line_put(err_msg, folder);
        return NULL;
    }
    verify_words_begin();
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word = args->analyzer);
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words, .word = project);
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words, .word = "verify");
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word = "-import");
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word = args->program);
    for (unsigned int each = 0u; args->loader[each] != NULL; each += 1u)
    {
        EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
                   .word = args->loader[each]);
    }
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word = "-overwrite");
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word = "-deleteProject");
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word = "-noanalysis");
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word = "-scriptPath");
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words, .word = scripts);
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word = "-postScript");
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word = "FunctionsDisassembled.java");
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words, .word = names);
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word = args->into);
    // the output emptied first, so that a script the analyzer could not run leaves nothing to read, not an older run's
    (void)EMBED_CALL(cli_path_bytes_internal.private_file_write, CliPathBytesCtx, .path = args->into, .bytes = "",
                     .size = 0ull);
    const CliSetting settings[1] = {{"PATH", s_run_path}};
    CliRan *const ran = EMBED_CALL(verify_run, DisasmVerifyStepsCtx, .folder = folder, .settings = settings,
                                   .count = 1u, .patience = CLI_DISASM_GHIDRA_PATIENCE, .err_msg = err_msg);
    if (ran == NULL)
    {
        return NULL;
    }
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    const unsigned char *const text = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = args->into,
                                                 .size_pointer = size, .owner = &g_cli_programs_cells);
    if ((text == NULL) || (*size == 0ull))
    {
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = text,
                   .owner = &g_cli_programs_cells);
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
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words,
               .word = args->program);
    EMBED_CALL(cli_words_words_internal.words_put, CliWordsWordsCtx, .words_words_pointer = &s_words, .word = "help");
    const CliSetting settings[1] = {{"PATH", s_run_path}};
    CliRan *const ran = EMBED_CALL(verify_run, DisasmVerifyStepsCtx, .folder = NULL, .settings = settings, .count = 1u,
                                   .patience = CLI_DISASM_READ_PATIENCE, .err_msg = args->err_msg);
    if (ran == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_words_internal.ran_free, CliWordsCtx, .ran = ran);
    return 1;
}
