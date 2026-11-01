// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_sign.c: the programs a build makes signed as settings.json specifies, what a signed program can be
// reported of its signature, the program compared with the last build of the same commit, and the command that runs it
#include "../text/json_tree.h"
#include "../text/text.h"
#include "executables_internal.h"

// the most bytes of a tree's .env read
#define CLI_ENV_BYTES (1u << 16u)

// a line printed through `sink`
static void cli_print(QuerySink sink, const char *text)
{
    sink.print(sink.context, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// a setting of how the programs a build makes are signed, `signing.<key>` in settings.json in orior's own folder,
// trimmed, into `out`, sized for CLI_PATH_MAX_LEN: 1, or 0 where it is not set or empty. `windows` is a
// certificate's thumbprint in the reader's store or the path of a .pfx file, whose password comes from
// ORIOR_SIGN_PASSWORD and from no file; `timestamp` the address of the time stamp server a Windows signature is stamped
// by; `macos` the identity codesign signs with; and `notarize` the keychain profile notarytool sends the program with
static int cli_signing(const char *key, char *out)
{
    char home[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    if (!cli_home_folder(home) || !cli_executables_joined(home, "settings.json", path))
    {
        return 0;
    }
    // settings.json read into a tree sized to it, given back as the setting is copied out
    CliJson json = {0};
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea reason = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_file_taken(&json, path, 0u, 0ull, &reason, &g_cli_programs_cells);
    char named[CLI_NAME_MAX_LEN];
    VerbaLinea line = {named, sizeof(named), 0ull};
    mmgr_line_put(&line, "signing.");
    mmgr_line_put(&line, key);
    mmgr_line_finish(&line);
    unsigned long long length = 0ull;
    const char *value = cli_json_string(&json, cli_json_member(&json, root, named), &length);
    if (value != NULL)
    {
        cli_scan_trim(&value, &length);
        cli_scan_copied(value, length, out, CLI_PATH_MAX_LEN);
    }
    cli_json_given(&json);
    return (value != NULL) && (length != 0ull);
}

int cli_executables_signs(void)
{
    char setting[CLI_PATH_MAX_LEN];
#if defined(_WIN32)
    return cli_signing("windows", setting);
#elif defined(__APPLE__)
    return cli_signing("macos", setting);
#else
    (void)setting;
    return 0;
#endif
}

#if defined(__APPLE__)
// `path` with its file's extension put as `extension` into `out`, sized for CLI_PATH_MAX_LEN
static void cli_with_extension(const char *path, const char *extension, char *out)
{
    const char *name = path;
    const unsigned long long name_past =
        cli_text_back_any(path, cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), "/\\");
    name = (name_past != 0ull) ? (path + name_past) : name;
    const char *const dot =
        (name[0] != '\0')
            ? cli_text_last(name + 1, cellul.len(&(CatenaFinitaCfg){.src = name + 1, .cap = CLI_CELLS_POOL}), '.')
            : NULL;
    VerbaLinea line = {out, CLI_PATH_MAX_LEN, 0ull};
    mmgr_line_put_n(&line, path,
                    (dot != NULL) ? (unsigned long long)(dot - path)
                                  : cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    mmgr_line_ch(&line, '.');
    mmgr_line_put(&line, extension);
    mmgr_line_finish(&line);
}
#endif

#if defined(_WIN32)
// a word put after a command and after the line shown for it
static void cli_both(CliCommand *command, VerbaLinea *shown, const char *word, const char *shown_word)
{
    cli_command_word(command, word);
    mmgr_line_ch(shown, ' ');
    mmgr_line_put(shown, shown_word);
}
#endif

int cli_executables_sign_steps(const char *file, CliCommand *commands, char (*shown)[CLI_PATH_MAX_LEN * 4u],
                               unsigned int room, VerbaLinea *err_msg)
{
#if defined(_WIN32)
    char certificate[CLI_PATH_MAX_LEN];
    if (!cli_signing("windows", certificate))
    {
        mmgr_line_put(err_msg, "settings.json names no certificate under signing.windows");
        return -1;
    }
    char signtool[CLI_PATH_MAX_LEN];
    if ((room < 1u) || !cli_executables_program("signtool", "signtool", signtool, err_msg))
    {
        return -1;
    }
    CliCommand *const command = &commands[0];
    VerbaLinea line = {shown[0], CLI_PATH_MAX_LEN * 4u, 0ull};
    cli_command_begun(command, signtool);
    mmgr_line_put(&line, "signtool");
    cli_both(command, &line, "sign", "sign");
    cli_both(command, &line, "/fd", "/fd");
    cli_both(command, &line, "sha256", "sha256");
    // the certificate's last four letters in lower case, which name a .pfx or .p12 file
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = certificate, .cap = CLI_CELLS_POOL});
    char tail[5] = "";
    for (unsigned long long at = 0ull; (length >= 4ull) && (at < 4ull); at += 1ull)
    {
        const char one = certificate[length - 4ull + at];
        tail[at] = ((one >= 'A') && (one <= 'Z')) ? (char)(one + ('a' - 'A')) : one;
    }
    const int pfx = (cellul.eq(&(CatenaFinitaCfg){.src = tail, .other = ".pfx", .cap = CLI_CELLS_POOL})) ||
                    (cellul.eq(&(CatenaFinitaCfg){.src = tail, .other = ".p12", .cap = CLI_CELLS_POOL}));
    if (query_is_file(certificate) || pfx)
    {
        cli_both(command, &line, "/f", "/f");
        cli_both(command, &line, certificate, certificate);
        char password[CLI_PATH_MAX_LEN];
        if (query_environment_read("ORIOR_SIGN_PASSWORD", password, sizeof(password)))
        {
            cli_both(command, &line, "/p", "/p");
            cli_both(command, &line, password, "<ORIOR_SIGN_PASSWORD>");
        }
    }
    else
    {
        char thumbprint[CLI_PATH_MAX_LEN];
        VerbaLinea printed = {thumbprint, sizeof(thumbprint), 0ull};
        const unsigned long long whole = cellul.len(&(CatenaFinitaCfg){.src = certificate, .cap = CLI_CELLS_POOL});
        for (unsigned long long at = 0ull; at < whole;)
        {
            const unsigned long long stop =
                cli_text_past_class(certificate, at, whole, CLI_TEXT_DIGITS, "abcdefABCDEF");
            mmgr_line_put_n(&printed, certificate + at, (size_t)(stop - at));
            at = stop + 1ull;
        }
        mmgr_line_finish(&printed);
        cli_both(command, &line, "/sha1", "/sha1");
        cli_both(command, &line, thumbprint, thumbprint);
    }
    char server[CLI_PATH_MAX_LEN];
    if (cli_signing("timestamp", server))
    {
        cli_both(command, &line, "/tr", "/tr");
        cli_both(command, &line, server, server);
        cli_both(command, &line, "/td", "/td");
        cli_both(command, &line, "sha256", "sha256");
    }
    cli_both(command, &line, file, file);
    mmgr_line_finish(&line);
    return 1;
#elif defined(__APPLE__)
    char identity[CLI_PATH_MAX_LEN];
    if (!cli_signing("macos", identity))
    {
        mmgr_line_put(err_msg, "settings.json names no identity under signing.macos");
        return -1;
    }
    if (room < 3u)
    {
        mmgr_line_put(err_msg, "no room for the signing steps");
        return -1;
    }
    static const char *const s_codesign[6] = {"--force", "--options", "runtime", "--timestamp", "--sign", NULL};
    cli_command_begun(&commands[0], "codesign");
    for (unsigned int at = 0u; s_codesign[at] != NULL; at += 1u)
    {
        cli_command_word(&commands[0], s_codesign[at]);
    }
    cli_command_word(&commands[0], identity);
    cli_command_word(&commands[0], file);
    VerbaLinea line = {shown[0], CLI_PATH_MAX_LEN * 4u, 0ull};
    mmgr_line_put(&line, "codesign --force --options runtime --timestamp --sign \"");
    mmgr_line_put(&line, identity);
    mmgr_line_put(&line, "\" ");
    mmgr_line_put(&line, file);
    mmgr_line_finish(&line);
    char profile[CLI_PATH_MAX_LEN];
    if (!cli_signing("notarize", profile))
    {
        return 1;
    }
    char zip[CLI_PATH_MAX_LEN];
    cli_with_extension(file, "zip", zip);
    cli_command_begun(&commands[1], "ditto");
    static const char *const s_ditto[3] = {"-c", "-k", "--keepParent"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        cli_command_word(&commands[1], s_ditto[at]);
    }
    cli_command_word(&commands[1], file);
    cli_command_word(&commands[1], zip);
    VerbaLinea pack = {shown[1], CLI_PATH_MAX_LEN * 4u, 0ull};
    mmgr_line_put(&pack, "ditto -c -k --keepParent ");
    mmgr_line_put(&pack, file);
    mmgr_line_ch(&pack, ' ');
    mmgr_line_put(&pack, zip);
    mmgr_line_finish(&pack);
    cli_command_begun(&commands[2], "xcrun");
    cli_command_word(&commands[2], "notarytool");
    cli_command_word(&commands[2], "submit");
    cli_command_word(&commands[2], zip);
    cli_command_word(&commands[2], "--keychain-profile");
    cli_command_word(&commands[2], profile);
    cli_command_word(&commands[2], "--wait");
    VerbaLinea send = {shown[2], CLI_PATH_MAX_LEN * 4u, 0ull};
    mmgr_line_put(&send, "xcrun notarytool submit ");
    mmgr_line_put(&send, zip);
    mmgr_line_put(&send, " --keychain-profile ");
    mmgr_line_put(&send, profile);
    mmgr_line_put(&send, " --wait");
    mmgr_line_finish(&send);
    return 3;
#else
    (void)file;
    (void)commands;
    (void)shown;
    (void)room;
    (void)err_msg;
    return 0;
#endif
}

void cli_executables_signature_print(const char *file, QuerySink sink)
{
#if defined(_WIN32)
    char signtool[CLI_PATH_MAX_LEN];
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unfound = {why, sizeof(why), 0ull};
    if (!cli_executables_program("signtool", "signtool", signtool, &unfound))
    {
        return;
    }
    const char *const words[5] = {signtool, "verify", "/pa", file, NULL};
    CliRan *const ran = cli_ran_taken();
    char err_buf[CLI_PATH_MAX_LEN];
    VerbaLinea unstarted = {err_buf, sizeof(err_buf), 0ull};
    if (!cli_ran(words, NULL, NULL, 0u, ran, &unstarted))
    {
        cli_ran_given(ran);
        return;
    }
    if (ran->code == 0ll)
    {
        cli_ran_given(ran);
        cli_print(sink, "Windows trusts the program's signature");
        return;
    }
    // the first line, errors before output, that is SignTool's error
    const char *reason = "";
    unsigned long long reason_length = 0ull;
    for (unsigned int stream = 0u; (reason_length == 0ull) && (stream < 2u); stream += 1u)
    {
        for (const char *line = (stream == 0u) ? ran->err : ran->out; line[0] != '\0';)
        {
            const char *stop = line;
            stop +=
                cli_text_to_any(stop, 0ull, cellul.len(&(CatenaFinitaCfg){.src = stop, .cap = CLI_CELLS_POOL}), "\n");
            const char *from = line;
            unsigned long long length = (unsigned long long)(stop - line);
            cli_scan_trim(&from, &length);
            if ((length >= 14ull) &&
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
    VerbaLinea print_line = {print_buf, sizeof(print_buf), 0ull};
    mmgr_line_put(&print_line, "the program is signed, and Windows does not trust the signature: ");
    mmgr_line_put_n(&print_line, reason, reason_length);
    mmgr_line_put(&print_line,
                  "; a certificate from an authority Windows trusts, and a reputation built as the program is "
                  "downloaded, prevent SmartScreen from warning");
    mmgr_line_finish(&print_line);
    cli_ran_given(ran);
    cli_print(sink, print_buf);
#elif defined(__APPLE__)
    (void)file;
    char profile[CLI_PATH_MAX_LEN];
    if (cli_signing("notarize", profile))
    {
        cli_print(sink,
                  "a program outside an app, a disk image or an installer cannot contain its notarization ticket: "
                  "Gatekeeper queries Apple for it as the program opens, online");
    }
#else
    (void)file;
    (void)sink;
#endif
}
