// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// carry_send.c: what travels written to one file, or laid out in a folder, which, where it is a repository, is
// committed and pushed
#include "../code/git.h"
#include "carry_internal.h"

// the most bytes of the one file a sending writes
#define CLI_SENT_FILE (CLI_CARRIED_TEXT * 2ull)

// The room a sending takes: what travels and the files the folder holds that travel, their cells given back as the
// sending ends
typedef struct
{
    CliCarried packed;
    CliCarried there;
} CliSending;

static CliSending s_send;

// that `count` files went to `to`, said, with how
static void cli_sent(CliLine *out, unsigned int count, const char *how, const char *to, const char *after)
{
    cli_line_decimal(out, count, 0u);
    cli_line_text(out, how);
    cli_line_text(out, to);
    cli_line_text(out, after);
}

// what travels written as one file at `to`: 1, or 0 with why said
static int cli_file_sent(const char *to, CliLine *said)
{
    char *const text = cli_cell_taken(CLI_SENT_FILE, "the settings file sent");
    if (text == NULL)
    {
        cli_line_text(said, to);
        cli_line_text(said, ": no cell could be taken to write it in");
        return 0;
    }
    CliLine file = {text, CLI_SENT_FILE, 0ull};
    cli_line_text(&file, "{\n  \"orior\": \"" CLI_CARRY_MARK "\",\n  \"files\": {");
    for (unsigned int at = 0u; at < s_send.packed.count; at += 1u)
    {
        const char *const place = cli_carried_place(&s_send.packed, at);
        cli_line_text(&file, (at != 0u) ? ",\n    " : "\n    ");
        cli_json_quoted(&file, place, cellul.len(&(CatenaFinitaCfg){.src = place, .cap = CLI_CELLS_POOL}));
        cli_line_text(&file, ": ");
        cli_json_quoted(&file, cli_carried_value(&s_send.packed, at), s_send.packed.size[at]);
    }
    cli_line_text(&file, (s_send.packed.count != 0u) ? "\n  }\n}\n" : "}\n}\n");
    const int written = cli_line_fits(&file) && query_file_written(to, text, file.at);
    cli_cell_given(text);
    if (!written)
    {
        cli_line_text(said, to);
        cli_line_text(said, ": it could not be written");
        return 0;
    }
    return 1;
}

// git run in the folder with `args`: 1 where it succeeded, its words into `out`
static int cli_git_in(const char *folder, const char *const *args, unsigned int count, CliLine *said)
{
    char words[CLI_NAME_LONGEST * 4u];
    CliLine out = {words, sizeof(words), 0ull};
    return cli_git_run(folder, args, count, &out, said);
}

// whether the repository at `folder` has a remote
static int cli_has_remote(const char *folder)
{
    static const char *const s_remote[1] = {"remote"};
    char remotes[CLI_NAME_LONGEST * 4u];
    char why[CLI_NAME_LONGEST];
    CliLine out = {remotes, sizeof(remotes), 0ull};
    CliLine unrun = {why, sizeof(why), 0ull};
    return cli_git_run(folder, s_remote, 1u, &out, &unrun) && (out.at > 0ull);
}

// the folder `to` made to hold what travels and no more: 1, or 0 with why said
static int cli_folder_laid(const char *to, CliLine *said)
{
    cli_carry_files(to, &s_send.there);
    for (unsigned int at = 0u; at < s_send.there.count; at += 1u)
    {
        const char *const path = cli_carried_value(&s_send.there, at);
        if ((cli_carried_find(&s_send.packed, cli_carried_place(&s_send.there, at)) == s_send.packed.count) &&
            !query_removed(path))
        {
            cli_line_text(said, path);
            cli_line_text(said, ": it could not be taken out");
            return 0;
        }
    }
    for (unsigned int at = 0u; at < s_send.packed.count; at += 1u)
    {
        char path[CLI_PATH_LONGEST];
        char parent[CLI_PATH_LONGEST];
        cli_full(to, cli_carried_place(&s_send.packed, at), path);
        const int made = !cli_path_parent(path, parent) || query_folder_made(parent);
        if (!made || !query_file_written(path, cli_carried_value(&s_send.packed, at), s_send.packed.size[at]))
        {
            cli_line_text(said, path);
            cli_line_text(said, ": it could not be written");
            return 0;
        }
    }
    return 1;
}

// the folder `to`, a repository, committed, and pushed where it has a remote: 1 with what it did said, or 0
static int cli_committed(const char *to, CliLine *out, CliLine *said)
{
    static const char *const s_add[4] = {"add", "-A", "--", "."};
    static const char *const s_diff[3] = {"diff", "--cached", "--quiet"};
    if (!cli_git_in(to, s_add, 4u, said))
    {
        return 0;
    }
    char why[CLI_NAME_LONGEST];
    CliLine quiet = {why, sizeof(why), 0ull};
    if (cli_git_in(to, s_diff, 3u, &quiet))
    {
        cli_line_text(out, to);
        cli_line_text(out, " holds these settings already");
        return 1;
    }
    char name[CLI_NAME_LONGEST];
#if defined(_WIN32)
    const int named = query_environment_read("COMPUTERNAME", name, sizeof(name));
#else
    const int named = query_environment_read("HOSTNAME", name, sizeof(name));
#endif
    char message[CLI_NAME_LONGEST * 2u];
    CliLine line = {message, sizeof(message), 0ull};
    cli_line_text(&line, "orior's settings from ");
    cli_line_text(&line, named ? name : "a machine");
    cli_line_closed(&line);
    const char *const commit[3] = {"commit", "-m", message};
    if (!cli_git_in(to, commit, 3u, said))
    {
        return 0;
    }
    if (cli_has_remote(to))
    {
        char words[CLI_NAME_LONGEST * 4u];
        CliLine pushed = {words, sizeof(words), 0ull};
        if (!cli_git_push(to, &pushed, said))
        {
            return 0;
        }
        cli_sent(out, s_send.packed.count, " files committed to ", to, " and pushed");
        return 1;
    }
    cli_sent(out, s_send.packed.count, " files committed to ", to, "");
    return 1;
}

int cli_carry_send(const char *home, const char *to, CliLine *out, CliLine *said)
{
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    int sent = cli_carry_pack(home, &s_send.packed, said);
    if (sent && !query_is_folder(to))
    {
        sent = cli_file_sent(to, said);
        if (sent)
        {
            cli_sent(out, s_send.packed.count, " files written to ", to, "");
        }
        cli_carried_clear(&s_send.packed);
        cli_lock_give(&s_lock);
        return sent;
    }
    char git[CLI_PATH_LONGEST];
    cli_full(to, ".git", git);
    const int repository = query_is_file(git) || query_is_folder(git);
    if (sent && repository && cli_has_remote(to))
    {
        char words[CLI_NAME_LONGEST * 4u];
        CliLine pulled = {words, sizeof(words), 0ull};
        sent = cli_git_pull(to, &pulled, said);
    }
    sent = sent && cli_folder_laid(to, said);
    if (sent && !repository)
    {
        cli_sent(out, s_send.packed.count, " files written to ", to, "");
    }
    else if (sent)
    {
        sent = cli_committed(to, out, said);
    }
    cli_carried_clear(&s_send.there);
    cli_carried_clear(&s_send.packed);
    cli_lock_give(&s_lock);
    return sent;
}
