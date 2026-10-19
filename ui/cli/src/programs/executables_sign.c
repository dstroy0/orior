// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_sign.c: the programs a build makes signed as settings.json says, what a signed program can be told of its
// signature, the program compared with the last build of the same commit, and the command that runs it
#include "../text/json_tree.h"
#include "../text/text.h"
#include "executables_internal.h"

// the most nodes and bytes of settings.json read, and the most bytes of a tree's .env read
#define CLI_SIGN_NODES 16384u

#define CLI_SIGN_BYTES (1u << 20u)

#define CLI_ENV_BYTES (1u << 16u)

// a line said through `say`
static void cli_say(QuerySink say, const char *text)
{
    say.said(say.context, text, cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}));
}

// a setting of how the programs a build makes are signed, `signing.<key>` in settings.json in orior's own folder,
// trimmed, into `out`, which holds CLI_PATH_LONGEST: 1, or 0 where it is not set or empty. `windows` is a
// certificate's thumbprint in the reader's store or the path of a .pfx file, whose password comes from
// ORIOR_SIGN_PASSWORD and from no file; `timestamp` the address of the time stamp server a Windows signature is stamped
// by; `macos` the identity codesign signs with; and `notarize` the keychain profile notarytool sends the program with
static int cli_signing(const char *key, char *out)
{
    static CliJsonNode s_nodes[CLI_SIGN_NODES];
    static CfgJsonToken s_tokens[CLI_SIGN_NODES];
    static char s_file[CLI_SIGN_BYTES];
    static char s_text[CLI_SIGN_BYTES];
    static CliLock s_lock = CLI_LOCK_INIT;
    char home[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    if (!cli_home_folder(home) || !cli_executables_joined(home, "settings.json", path))
    {
        return 0;
    }
    cli_lock_take(&s_lock);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_file, sizeof(s_file));
    CliJson json;
    cli_json_begun(&json, s_nodes, CLI_SIGN_NODES, s_text, sizeof(s_text));
    char why[CLI_NAME_LONGEST];
    CliLine reason = {why, sizeof(why), 0ull};
    const unsigned int root = (size != QRY_MEMBER_UNREAD)
                                  ? cli_json_read(&json, s_file, size, s_tokens, CLI_SIGN_NODES, &reason)
                                  : CLI_JSON_NONE;
    char named[CLI_NAME_LONGEST];
    CliLine line = {named, sizeof(named), 0ull};
    cli_line_text(&line, "signing.");
    cli_line_text(&line, key);
    cli_line_closed(&line);
    unsigned long long length = 0ull;
    const char *value = cli_json_string(&json, cli_json_member(&json, root, named), &length);
    if (value != NULL)
    {
        cli_scan_trim(&value, &length);
        cli_scan_copied(value, length, out, CLI_PATH_LONGEST);
    }
    cli_lock_give(&s_lock);
    return (value != NULL) && (length != 0ull);
}

int cli_executables_signs(void)
{
    char setting[CLI_PATH_LONGEST];
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
// `path` with its file's extension put as `extension` into `out`, which holds CLI_PATH_LONGEST
static void cli_with_extension(const char *path, const char *extension, char *out)
{
    const char *name = path;
    for (const char *at = path; at[0] != '\0'; at += 1)
    {
        name = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : name;
    }
    const char *const dot =
        (name[0] != '\0')
            ? cli_text_last(name + 1, cellul.len(&(CatenaFinitaCfg){.src = name + 1, .cap = CLI_CELLS_POOL}), '.')
            : NULL;
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    cli_line_counted(&line, path,
                     (dot != NULL) ? (unsigned long long)(dot - path)
                                   : cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}));
    cli_line_character(&line, '.');
    cli_line_text(&line, extension);
    cli_line_closed(&line);
}
#endif

#if defined(_WIN32)
// a word put after a command and after the line shown for it
static void cli_both(CliCommand *command, CliLine *shown, const char *word, const char *shown_word)
{
    cli_command_word(command, word);
    cli_line_character(shown, ' ');
    cli_line_text(shown, shown_word);
}
#endif

int cli_executables_sign_steps(const char *file, CliCommand *commands, char (*shown)[CLI_PATH_LONGEST * 4u],
                               unsigned int room, CliLine *said)
{
#if defined(_WIN32)
    char certificate[CLI_PATH_LONGEST];
    if (!cli_signing("windows", certificate))
    {
        cli_line_text(said, "settings.json names no certificate under signing.windows");
        return -1;
    }
    char signtool[CLI_PATH_LONGEST];
    if ((room < 1u) || !cli_executables_program("signtool", "signtool", signtool, said))
    {
        return -1;
    }
    CliCommand *const command = &commands[0];
    CliLine line = {shown[0], CLI_PATH_LONGEST * 4u, 0ull};
    cli_command_begun(command, signtool);
    cli_line_text(&line, "signtool");
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
        char password[CLI_PATH_LONGEST];
        if (query_environment_read("ORIOR_SIGN_PASSWORD", password, sizeof(password)))
        {
            cli_both(command, &line, "/p", "/p");
            cli_both(command, &line, password, "<ORIOR_SIGN_PASSWORD>");
        }
    }
    else
    {
        char thumbprint[CLI_PATH_LONGEST];
        CliLine printed = {thumbprint, sizeof(thumbprint), 0ull};
        for (const char *at = certificate; at[0] != '\0'; at += 1)
        {
            const char one = at[0];
            if (((one >= '0') && (one <= '9')) || ((one >= 'a') && (one <= 'f')) || ((one >= 'A') && (one <= 'F')))
            {
                cli_line_character(&printed, one);
            }
        }
        cli_line_closed(&printed);
        cli_both(command, &line, "/sha1", "/sha1");
        cli_both(command, &line, thumbprint, thumbprint);
    }
    char server[CLI_PATH_LONGEST];
    if (cli_signing("timestamp", server))
    {
        cli_both(command, &line, "/tr", "/tr");
        cli_both(command, &line, server, server);
        cli_both(command, &line, "/td", "/td");
        cli_both(command, &line, "sha256", "sha256");
    }
    cli_both(command, &line, file, file);
    cli_line_closed(&line);
    return 1;
#elif defined(__APPLE__)
    char identity[CLI_PATH_LONGEST];
    if (!cli_signing("macos", identity))
    {
        cli_line_text(said, "settings.json names no identity under signing.macos");
        return -1;
    }
    if (room < 3u)
    {
        cli_line_text(said, "no room for the signing steps");
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
    CliLine line = {shown[0], CLI_PATH_LONGEST * 4u, 0ull};
    cli_line_text(&line, "codesign --force --options runtime --timestamp --sign \"");
    cli_line_text(&line, identity);
    cli_line_text(&line, "\" ");
    cli_line_text(&line, file);
    cli_line_closed(&line);
    char profile[CLI_PATH_LONGEST];
    if (!cli_signing("notarize", profile))
    {
        return 1;
    }
    char zip[CLI_PATH_LONGEST];
    cli_with_extension(file, "zip", zip);
    cli_command_begun(&commands[1], "ditto");
    static const char *const s_ditto[3] = {"-c", "-k", "--keepParent"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        cli_command_word(&commands[1], s_ditto[at]);
    }
    cli_command_word(&commands[1], file);
    cli_command_word(&commands[1], zip);
    CliLine pack = {shown[1], CLI_PATH_LONGEST * 4u, 0ull};
    cli_line_text(&pack, "ditto -c -k --keepParent ");
    cli_line_text(&pack, file);
    cli_line_character(&pack, ' ');
    cli_line_text(&pack, zip);
    cli_line_closed(&pack);
    cli_command_begun(&commands[2], "xcrun");
    cli_command_word(&commands[2], "notarytool");
    cli_command_word(&commands[2], "submit");
    cli_command_word(&commands[2], zip);
    cli_command_word(&commands[2], "--keychain-profile");
    cli_command_word(&commands[2], profile);
    cli_command_word(&commands[2], "--wait");
    CliLine send = {shown[2], CLI_PATH_LONGEST * 4u, 0ull};
    cli_line_text(&send, "xcrun notarytool submit ");
    cli_line_text(&send, zip);
    cli_line_text(&send, " --keychain-profile ");
    cli_line_text(&send, profile);
    cli_line_text(&send, " --wait");
    cli_line_closed(&send);
    return 3;
#else
    (void)file;
    (void)commands;
    (void)shown;
    (void)room;
    (void)said;
    return 0;
#endif
}

void cli_executables_signed_said(const char *file, QuerySink say)
{
#if defined(_WIN32)
    char signtool[CLI_PATH_LONGEST];
    char why[CLI_NAME_LONGEST];
    CliLine unfound = {why, sizeof(why), 0ull};
    if (!cli_executables_program("signtool", "signtool", signtool, &unfound))
    {
        return;
    }
    const char *const words[5] = {signtool, "verify", "/pa", file, NULL};
    CliRan *const ran = cli_ran_taken();
    char unsaid[CLI_PATH_LONGEST];
    CliLine unstarted = {unsaid, sizeof(unsaid), 0ull};
    if (!cli_ran(words, NULL, NULL, 0u, ran, &unstarted))
    {
        cli_ran_given(ran);
        return;
    }
    if (ran->code == 0ll)
    {
        cli_ran_given(ran);
        cli_say(say, "Windows trusts the program's signature");
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
            while ((stop[0] != '\0') && (stop[0] != '\n'))
            {
                stop += 1;
            }
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
    char said[CLI_PATH_LONGEST * 2u];
    CliLine told = {said, sizeof(said), 0ull};
    cli_line_text(&told, "the program is signed, and Windows does not trust the signature: ");
    cli_line_counted(&told, reason, reason_length);
    cli_line_text(&told, "; a certificate from an authority Windows trusts, and a reputation built as the program is "
                         "downloaded, keep SmartScreen from warning");
    cli_line_closed(&told);
    cli_ran_given(ran);
    cli_say(say, said);
#elif defined(__APPLE__)
    (void)file;
    char profile[CLI_PATH_LONGEST];
    if (cli_signing("notarize", profile))
    {
        cli_say(say, "a program outside an app, a disk image or an installer cannot hold its notarization ticket: "
                     "Gatekeeper asks Apple for it as the program opens, online");
    }
#else
    (void)file;
    (void)say;
#endif
}
