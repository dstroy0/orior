// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// macos_sign.c: the programs a build makes signed on macOS by codesign and notarized by notarytool, as settings.json
// specifies under signing.macos and signing.notarize, and what a signed program's signature needs there
#include "../../../../ui/cli/src/programs/executables_internal.h"
#include "../../../../ui/cli/src/text/text.h"

#if defined(__APPLE__)
typedef struct
{
    const char *path;
    const char *extension;
    char *out;
} PosixSignCtx;

// `path` with its file's extension put as `extension` into `out`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE void sign_with_extension(const PosixSignCtx *args)
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

int cli_executables_signs(void)
{
    char setting[CLI_PATH_MAX_LEN];
    return EMBED_CALL(cli_executables_sign_internal.signing, CliExecutablesSignCtx, .key = "macos", .out = setting);
}

int cli_executables_sign_steps(const CliExecutablesErrMsgCfg *args)
{
    char (*displayed)[CLI_PATH_MAX_LEN * 4u] = args->displayed;
    VerbaLinea *err_msg = args->err_msg;
    char identity[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_executables_sign_internal.signing, CliExecutablesSignCtx, .key = "macos", .out = identity))
    {
        mmgr_line_put(err_msg, "settings.json names no identity under signing.macos");
        return -1;
    }
    if (args->capacity < 3u)
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
    if (!EMBED_CALL(cli_executables_sign_internal.signing, CliExecutablesSignCtx, .key = "notarize", .out = profile))
    {
        return 1;
    }
    char zip[CLI_PATH_MAX_LEN];
    EMBED_CALL(sign_with_extension, PosixSignCtx, .path = args->file, .extension = "zip", .out = zip);
    EMBED_CALL(cli_runner.command_begin, CliRunnerCfg, .command = &args->commands[1], .program = "ditto");
    static const char *const s_ditto[3] = {"-c", "-k", "--retainParent"};
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
    mmgr_line_put(&pack, "ditto -c -k --retainParent ");
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
}

void cli_executables_signature_print(const CliExecutablesCfg *args)
{
    char profile[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_executables_sign_internal.signing, CliExecutablesSignCtx, .key = "notarize", .out = profile))
    {
        EMBED_CALL(cli_executables_sign_internal.print, CliExecutablesSignCtx, .sink = args->sink,
                   .text =
                       "a program outside an app, a disk image or an installer cannot contain its notarization ticket: "
                       "Gatekeeper queries Apple for it as the program opens, online");
    }
}
#endif
