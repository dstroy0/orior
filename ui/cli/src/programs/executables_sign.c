// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_sign.c: the programs a build makes signed as settings.json specifies, what a signed program can be
// reported of its signature, the program compared with the last build of the same commit, and the command that runs it
#include "../command_line/callback_cfg.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "executables_internal.h"

// the most bytes of a tree's .env read
#define CLI_ENV_BYTES (1u << 16u)

typedef struct
{
    QuerySink sink;
    const char *text;
    const char *key;
    char *out;
#if (defined(_WIN32))
    CliCommand *command;
    VerbaLinea *displayed;
    const char *word;
    const char *displayed_word;
#endif
#if (defined(__APPLE__))
    const char *path;
    const char *extension;
#endif
} ExecutablesSignCtx;
#if defined(_WIN32) || defined(__APPLE__)
// a line printed through `sink`
EMBED_INLINE void cli_print(const ExecutablesSignCtx *args)
{
    EMBED_CALL(args->sink.print, CliEachLineSizeCfg, .context = args->sink.context, .line = args->text,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES}));
}

// a setting of how the programs a build makes are signed, `signing.<key>` in settings.json in orior's own folder,
// trimmed, into `out`, sized for CLI_PATH_MAX_LEN: 1, or 0 where it is not set or empty. `windows` is a
// certificate's thumbprint in the reader's store or the path of a .pfx file, whose password comes from
// ORIOR_SIGN_PASSWORD and from no file; `timestamp` the address of the time stamp server a Windows signature is stamped
// by; `macos` the identity codesign signs with; and `notarize` the keychain profile notarytool sends the program with
EMBED_INLINE int cli_signing(const ExecutablesSignCtx *args)
{
    char *out = args->out;
    char home[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home) ||
        !EMBED_CALL(cli_executables_name_out_folder_internal.join, CliExecutablesNameOutFolderCtx, .folder = home,
                    .name = "settings.json", .out = path))
    {
        return 0;
    }
    // settings.json read into a tree sized to it, given back as the setting is copied out
    CliJson json = {0};
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea reason = {why, sizeof(why), 0ULL};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_err_msg.json_file_alloc, CliJsonTreeErrMsgCfg, .json = &json, .path = path,
                   .nodes_more = 0u, .text_more = 0ULL, .err_msg = &reason, .owner = g_cli_programs_owner);
    char named[CLI_NAME_MAX_LEN];
    VerbaLinea line = {named, sizeof(named), 0ULL};
    mmgr_line_put(&line, "signing.");
    mmgr_line_put(&line, args->key);
    mmgr_line_finish(&line);
    unsigned long long length = 0ULL;
    const char *value = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                      .json_read = &json, .object = root, .name = named),
                                   .length_pointer = &length);
    if (value != NULL)
    {
        EMBED_CALL(cli_executables_text_length_internal.scan_trim, CliExecutablesTextLengthCtx, .text_pointer = &value,
                   .length_pointer = &length);
        EMBED_CALL(cli_executables_length_text_internal.scan_copy, CliExecutablesLengthTextCtx, .text = value,
                   .length = length, .out = out, .capacity = CLI_PATH_MAX_LEN);
    }
    g_cli_guard->persistent_buf_release(json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    return (value != NULL) && (length != 0ULL);
}
#endif

int cli_executables_signs(void)
{
#if defined(_WIN32) || defined(__APPLE__)
    char setting[CLI_PATH_MAX_LEN];
#endif
#if defined(_WIN32)
    return EMBED_CALL(cli_signing, ExecutablesSignCtx, .key = "windows", .out = setting);
#elif defined(__APPLE__)
    return EMBED_CALL(cli_signing, ExecutablesSignCtx, .key = "macos", .out = setting);
#else
    return 0;
#endif
}

#if defined(__APPLE__)
// `path` with its file's extension put as `extension` into `out`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE void cli_with_extension(const ExecutablesSignCtx *args)
{
    const char *name = args->path;
    const unsigned long long name_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}), .set = "/\\");
    name = (name_end != 0ULL) ? (args->path + name_end) : name;
    const char *const dot =
        (name[0] != '\0')
            ? EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = name + 1,
                         .length_value = cellul.len(&(CatenaFinitaCfg){.src = name + 1, .cap = CLI_POOL_BYTES}),
                         .byte = '.')
            : NULL;
    VerbaLinea line = {args->out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put_n(&line, args->path,
                    (dot != NULL) ? (unsigned long long)(dot - args->path)
                                  : cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}));
    mmgr_line_ch(&line, '.');
    mmgr_line_put(&line, args->extension);
    mmgr_line_finish(&line);
}
#endif

#if defined(_WIN32)
// a word put after a command and after the line shown for it
EMBED_INLINE void cli_both(const ExecutablesSignCtx *args)
{
    CliCommand *command = args->command;
    const char *word = args->word;
    EMBED_CALL(cli_runner_command_word.command_word, CliRunnerCommandWordCfg, .command = command, .word_text = word);
    mmgr_line_ch(args->displayed, ' ');
    mmgr_line_put(args->displayed, args->displayed_word);
}
#endif

int cli_executables_sign_steps(const CliExecutablesErrMsgCfg *args)
{
#if defined(_WIN32) || defined(__APPLE__)
    char (*displayed)[CLI_PATH_MAX_LEN * 4u] = args->displayed;
    unsigned int capacity = args->capacity;
    VerbaLinea *err_msg = args->err_msg;
#endif
#if defined(_WIN32)
    char certificate[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_signing, ExecutablesSignCtx, .key = "windows", .out = certificate))
    {
        mmgr_line_put(err_msg, "settings.json names no certificate under signing.windows");
        return -1;
    }
    char signtool[CLI_PATH_MAX_LEN];
    if ((capacity < 1u) || !EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = "signtool",
                                       .name = "signtool", .path = signtool, .err_msg = err_msg))
    {
        return -1;
    }
    CliCommand *const command = &args->commands[0];
    VerbaLinea line = {displayed[0], CLI_PATH_MAX_LEN * 4u, 0ULL};
    EMBED_CALL(cli_runner.command_begin, CliRunnerCfg, .command = command, .program = signtool);
    mmgr_line_put(&line, "signtool");
    EMBED_CALL(cli_both, ExecutablesSignCtx, .command = command, .displayed = &line, .word = "sign",
               .displayed_word = "sign");
    EMBED_CALL(cli_both, ExecutablesSignCtx, .command = command, .displayed = &line, .word = "/fd",
               .displayed_word = "/fd");
    EMBED_CALL(cli_both, ExecutablesSignCtx, .command = command, .displayed = &line, .word = "sha256",
               .displayed_word = "sha256");
    // the certificate's last four letters in lower case, which name a .pfx or .p12 file
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = certificate, .cap = CLI_POOL_BYTES});
    char tail[5] = "";
    for (unsigned long long at = 0ULL; (length >= 4ULL) && (at < 4ULL); at += 1ULL)
    {
        const char one = certificate[length - 4ULL + at];
        tail[at] = ((one >= 'A') && (one <= 'Z')) ? (char)(one + ('a' - 'A')) : one;
    }
    const int pfx = (cellul.eq(&(CatenaFinitaCfg){.src = tail, .other = ".pfx", .cap = CLI_POOL_BYTES})) ||
                    (cellul.eq(&(CatenaFinitaCfg){.src = tail, .other = ".p12", .cap = CLI_POOL_BYTES}));
    if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = certificate) || pfx)
    {
        EMBED_CALL(cli_both, ExecutablesSignCtx, .command = command, .displayed = &line, .word = "/f",
                   .displayed_word = "/f");
        EMBED_CALL(cli_both, ExecutablesSignCtx, .command = command, .displayed = &line, .word = certificate,
                   .displayed_word = certificate);
        char password[CLI_PATH_MAX_LEN];
        if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_SIGN_PASSWORD",
                       .value_out = password, .capacity = sizeof(password)))
        {
            EMBED_CALL(cli_both, ExecutablesSignCtx, .command = command, .displayed = &line, .word = "/p",
                       .displayed_word = "/p");
            EMBED_CALL(cli_both, ExecutablesSignCtx, .command = command, .displayed = &line, .word = password,
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
        EMBED_CALL(cli_both, ExecutablesSignCtx, .command = command, .displayed = &line, .word = "/sha1",
                   .displayed_word = "/sha1");
        EMBED_CALL(cli_both, ExecutablesSignCtx, .command = command, .displayed = &line, .word = thumbprint,
                   .displayed_word = thumbprint);
    }
    char server[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_signing, ExecutablesSignCtx, .key = "timestamp", .out = server))
    {
        EMBED_CALL(cli_both, ExecutablesSignCtx, .command = command, .displayed = &line, .word = "/tr",
                   .displayed_word = "/tr");
        EMBED_CALL(cli_both, ExecutablesSignCtx, .command = command, .displayed = &line, .word = server,
                   .displayed_word = server);
        EMBED_CALL(cli_both, ExecutablesSignCtx, .command = command, .displayed = &line, .word = "/td",
                   .displayed_word = "/td");
        EMBED_CALL(cli_both, ExecutablesSignCtx, .command = command, .displayed = &line, .word = "sha256",
                   .displayed_word = "sha256");
    }
    EMBED_CALL(cli_both, ExecutablesSignCtx, .command = command, .displayed = &line, .word = args->file,
               .displayed_word = args->file);
    mmgr_line_finish(&line);
    return 1;
#elif defined(__APPLE__)
    char identity[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_signing, ExecutablesSignCtx, .key = "macos", .out = identity))
    {
        mmgr_line_put(err_msg, "settings.json names no identity under signing.macos");
        return -1;
    }
    if (capacity < 3u)
    {
        mmgr_line_put(err_msg, "no capacity for the signing steps");
        return -1;
    }
    static const char *const s_codesign[6] = {"--force", "--options", "runtime", "--timestamp", "--sign", NULL};
    EMBED_CALL(cli_runner.command_begin, CliRunnerCfg, .command = &args->commands[0], .program = "codesign");
    for (unsigned int at = 0u; s_codesign[at] != NULL; at += 1u)
    {
        EMBED_CALL(cli_runner_command_word.command_word, CliRunnerCommandWordCfg, .command = &args->commands[0],
                   .word_text = s_codesign[at]);
    }
    EMBED_CALL(cli_runner_command_word.command_word, CliRunnerCommandWordCfg, .command = &args->commands[0],
               .word_text = identity);
    EMBED_CALL(cli_runner_command_word.command_word, CliRunnerCommandWordCfg, .command = &args->commands[0],
               .word_text = args->file);
    VerbaLinea line = {displayed[0], CLI_PATH_MAX_LEN * 4u, 0ULL};
    mmgr_line_put(&line, "codesign --force --options runtime --timestamp --sign \"");
    mmgr_line_put(&line, identity);
    mmgr_line_put(&line, "\" ");
    mmgr_line_put(&line, args->file);
    mmgr_line_finish(&line);
    char profile[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_signing, ExecutablesSignCtx, .key = "notarize", .out = profile))
    {
        return 1;
    }
    char zip[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_with_extension, ExecutablesSignCtx, .path = args->file, .extension = "zip", .out = zip);
    EMBED_CALL(cli_runner.command_begin, CliRunnerCfg, .command = &args->commands[1], .program = "ditto");
    static const char *const s_ditto[3] = {"-c", "-k", "--keepParent"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        EMBED_CALL(cli_runner_command_word.command_word, CliRunnerCommandWordCfg, .command = &args->commands[1],
                   .word_text = s_ditto[at]);
    }
    EMBED_CALL(cli_runner_command_word.command_word, CliRunnerCommandWordCfg, .command = &args->commands[1],
               .word_text = args->file);
    EMBED_CALL(cli_runner_command_word.command_word, CliRunnerCommandWordCfg, .command = &args->commands[1],
               .word_text = zip);
    VerbaLinea pack = {displayed[1], CLI_PATH_MAX_LEN * 4u, 0ULL};
    mmgr_line_put(&pack, "ditto -c -k --keepParent ");
    mmgr_line_put(&pack, args->file);
    mmgr_line_ch(&pack, ' ');
    mmgr_line_put(&pack, zip);
    mmgr_line_finish(&pack);
    EMBED_CALL(cli_runner.command_begin, CliRunnerCfg, .command = &args->commands[2], .program = "xcrun");
    EMBED_CALL(cli_runner_command_word.command_word, CliRunnerCommandWordCfg, .command = &args->commands[2],
               .word_text = "notarytool");
    EMBED_CALL(cli_runner_command_word.command_word, CliRunnerCommandWordCfg, .command = &args->commands[2],
               .word_text = "submit");
    EMBED_CALL(cli_runner_command_word.command_word, CliRunnerCommandWordCfg, .command = &args->commands[2],
               .word_text = zip);
    EMBED_CALL(cli_runner_command_word.command_word, CliRunnerCommandWordCfg, .command = &args->commands[2],
               .word_text = "--keychain-profile");
    EMBED_CALL(cli_runner_command_word.command_word, CliRunnerCommandWordCfg, .command = &args->commands[2],
               .word_text = profile);
    EMBED_CALL(cli_runner_command_word.command_word, CliRunnerCommandWordCfg, .command = &args->commands[2],
               .word_text = "--wait");
    VerbaLinea send = {displayed[2], CLI_PATH_MAX_LEN * 4u, 0ULL};
    mmgr_line_put(&send, "xcrun notarytool submit ");
    mmgr_line_put(&send, zip);
    mmgr_line_put(&send, " --keychain-profile ");
    mmgr_line_put(&send, profile);
    mmgr_line_put(&send, " --wait");
    mmgr_line_finish(&send);
    return 3;
#else
    (void)args;
    return 0;
#endif
}

void cli_executables_signature_print(const CliExecutablesCfg *args)
{
#if defined(_WIN32)
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
        EMBED_CALL(cli_print, ExecutablesSignCtx, .sink = args->sink, .text = "Windows trusts the program's signature");
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
    EMBED_CALL(cli_print, ExecutablesSignCtx, .sink = args->sink, .text = print_buf);
#elif defined(__APPLE__)
    char profile[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_signing, ExecutablesSignCtx, .key = "notarize", .out = profile))
    {
        EMBED_CALL(cli_print, ExecutablesSignCtx, .sink = args->sink,
                   .text =
                       "a program outside an app, a disk image or an installer cannot contain its notarization ticket: "
                       "Gatekeeper queries Apple for it as the program opens, online");
    }
#else
    (void)args;
#endif
}
