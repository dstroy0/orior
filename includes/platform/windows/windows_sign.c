// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// windows_sign.c: the programs a build makes signed on Windows by SignTool, as settings.json specifies under
// signing.windows and signing.timestamp, and what Windows reports of a signed program's signature
#include "../../../ui/cli/src/programs/executables_internal.h"
#include "../../../ui/cli/src/text/text.h"

#if defined(_WIN32)

typedef struct
{
    CliCommand *command;
    VerbaLinea *displayed;
    const char *word;
    const char *displayed_word;
} WindowsSignCtx;

// a word put after a command, and `displayed_word` after the line displayed for it
EMBED_INLINE void sign_both(const WindowsSignCtx *args)
{
    EMBED_CALL(cli_runner_command_word.command_word, CliRunnerCommandWordCfg, .command = args->command,
               .word_text = args->word);
    mmgr_line_ch(args->displayed, ' ');
    mmgr_line_put(args->displayed, args->displayed_word);
}

int cli_executables_signs(void)
{
    char setting[CLI_PATH_MAX_LEN];
    return EMBED_CALL(cli_executables_sign_internal.signing, CliExecutablesSignCtx, .key = "windows", .out = setting);
}

int cli_executables_sign_steps(const CliExecutablesErrMsgCfg *args)
{
    char (*displayed)[CLI_PATH_MAX_LEN * 4u] = args->displayed;
    VerbaLinea *err_msg = args->err_msg;
    char certificate[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_executables_sign_internal.signing, CliExecutablesSignCtx, .key = "windows", .out = certificate))
    {
        mmgr_line_put(err_msg, "settings.json names no certificate under signing.windows");
        return -1;
    }
    char signtool[CLI_PATH_MAX_LEN];
    if ((args->capacity < 1u) || !EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = "signtool",
                                             .name = "signtool", .path = signtool, .err_msg = err_msg))
    {
        return -1;
    }
    CliCommand *const command = &args->commands[0];
    VerbaLinea line = {displayed[0], CLI_PATH_MAX_LEN * 4u, 0ULL};
    EMBED_CALL(cli_runner.command_begin, CliRunnerCfg, .command = command, .program = signtool);
    mmgr_line_put(&line, "signtool");
    EMBED_CALL(sign_both, WindowsSignCtx, .command = command, .displayed = &line, .word = "sign",
               .displayed_word = "sign");
    EMBED_CALL(sign_both, WindowsSignCtx, .command = command, .displayed = &line, .word = "/fd",
               .displayed_word = "/fd");
    EMBED_CALL(sign_both, WindowsSignCtx, .command = command, .displayed = &line, .word = "sha256",
               .displayed_word = "sha256");
    // A certificate that ends in .pfx or .p12, in either case, names a file
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = certificate, .cap = CLI_POOL_BYTES});
    const char *const tail = certificate + ((length >= 4ULL) ? (length - 4ULL) : 0ULL);
    const int pfx = (length >= 4ULL) && (EMBED_CALL(cli_text_size_one.same_folded, CliTextSizeOneCfg, .size = 4ULL,
                                                    .one = tail, .other_pointer = ".pfx") ||
                                         EMBED_CALL(cli_text_size_one.same_folded, CliTextSizeOneCfg, .size = 4ULL,
                                                    .one = tail, .other_pointer = ".p12"));
    if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = certificate) || pfx)
    {
        EMBED_CALL(sign_both, WindowsSignCtx, .command = command, .displayed = &line, .word = "/f",
                   .displayed_word = "/f");
        EMBED_CALL(sign_both, WindowsSignCtx, .command = command, .displayed = &line, .word = certificate,
                   .displayed_word = certificate);
        char password[CLI_PATH_MAX_LEN];
        if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_SIGN_PASSWORD",
                       .value_out = password, .capacity = sizeof(password)))
        {
            EMBED_CALL(sign_both, WindowsSignCtx, .command = command, .displayed = &line, .word = "/p",
                       .displayed_word = "/p");
            EMBED_CALL(sign_both, WindowsSignCtx, .command = command, .displayed = &line, .word = password,
                       .displayed_word = "<ORIOR_SIGN_PASSWORD>");
        }
    }
    else
    {
        char thumbprint[CLI_PATH_MAX_LEN];
        VerbaLinea printed = {thumbprint, sizeof(thumbprint), 0ULL};
        const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = certificate, .cap = CLI_POOL_BYTES});
        for (unsigned long long at = 0ULL; at < whole;)
        {
            const unsigned long long stop =
                EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = certificate, .at = at,
                           .size = whole, .classes = CLI_TEXT_DIGITS, .set = "abcdefABCDEF");
            mmgr_line_put_n(&printed, certificate + at, (embed_word)(stop - at));
            at = stop + 1ULL;
        }
        mmgr_line_finish(&printed);
        EMBED_CALL(sign_both, WindowsSignCtx, .command = command, .displayed = &line, .word = "/sha1",
                   .displayed_word = "/sha1");
        EMBED_CALL(sign_both, WindowsSignCtx, .command = command, .displayed = &line, .word = thumbprint,
                   .displayed_word = thumbprint);
    }
    char server[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_executables_sign_internal.signing, CliExecutablesSignCtx, .key = "timestamp", .out = server))
    {
        EMBED_CALL(sign_both, WindowsSignCtx, .command = command, .displayed = &line, .word = "/tr",
                   .displayed_word = "/tr");
        EMBED_CALL(sign_both, WindowsSignCtx, .command = command, .displayed = &line, .word = server,
                   .displayed_word = server);
        EMBED_CALL(sign_both, WindowsSignCtx, .command = command, .displayed = &line, .word = "/td",
                   .displayed_word = "/td");
        EMBED_CALL(sign_both, WindowsSignCtx, .command = command, .displayed = &line, .word = "sha256",
                   .displayed_word = "sha256");
    }
    EMBED_CALL(sign_both, WindowsSignCtx, .command = command, .displayed = &line, .word = args->file,
               .displayed_word = args->file);
    mmgr_line_finish(&line);
    return 1;
}

void cli_executables_signature_print(const CliExecutablesCfg *args)
{
    char signtool[CLI_PATH_MAX_LEN];
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unfound = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = "signtool", .name = "signtool",
                    .path = signtool, .err_msg = &unfound))
    {
        return;
    }
    const char *const words[5] = {signtool, "verify", "/pa", args->file, NULL};
    CliRunResult *const run_result = cli_run_result_alloc();
    char err_buf[CLI_PATH_MAX_LEN];
    VerbaLinea unstarted = {err_buf, sizeof(err_buf), 0ULL};
    if (!EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = words, .folder = NULL, .settings = NULL, .count = 0u,
                    .run_result = run_result, .err_msg = &unstarted))
    {
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        return;
    }
    if (run_result->code == 0LL)
    {
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        EMBED_CALL(cli_executables_sign_internal.print, CliExecutablesSignCtx, .sink = args->sink,
                   .text = "Windows trusts the program's signature");
        return;
    }
    // the first line, errors before output, that is SignTool's error
    const char *reason = "";
    unsigned long long reason_length = 0ULL;
    for (unsigned int stream = 0u; (reason_length == 0ULL) && (stream < 2u); stream += 1u)
    {
        for (const char *line = (stream == 0u) ? run_result->err : run_result->out; line[0] != '\0';)
        {
            const char *stop = line;
            stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                               .size = cellul.len(&(CatenaFinitaCfg){.src = stop, .cap = CLI_POOL_BYTES}), .set = "\n");
            const char *from = line;
            unsigned long long length = (unsigned long long)(stop - line);
            EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx,
                       .text_pointer = &from, .length_pointer = &length);
            if ((length >= 14ULL) &&
                (memor.cmp(&(MemoriaCfg){.src = from, .other = "SignTool Error", .bytes = 14u}) == 0))
            {
                reason = from;
                reason_length = length;
                break;
            }
            line = (stop[0] != '\0') ? (stop + 1) : stop;
        }
    }
    char print_buf[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea print_line = {print_buf, sizeof(print_buf), 0ULL};
    mmgr_line_put(&print_line, "the program is signed, and Windows does not trust the signature: ");
    mmgr_line_put_n(&print_line, reason, reason_length);
    mmgr_line_put(&print_line,
                  "; a certificate from an authority Windows trusts, and a reputation built as the program is "
                  "downloaded, prevent SmartScreen from warning");
    mmgr_line_finish(&print_line);
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    EMBED_CALL(cli_executables_sign_internal.print, CliExecutablesSignCtx, .sink = args->sink, .text = print_buf);
}
#endif
