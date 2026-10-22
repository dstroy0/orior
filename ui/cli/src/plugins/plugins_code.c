// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// plugins_code.c: a script plugin's code: the file its plugin.json names as `main`, or each of the files it names there
// in order, joined by a line's end, plugin.js where it names none, every one in the plugin's own folder. The code of a
// script plugin that comes with orior is read the same way as the program is built, and built into it
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "plugins.h"

// the most files a plugin's `main` names
#define CLI_PLUGINS_MAINS 32u

// The files a plugin's `main` names
typedef struct
{
    char main[CLI_PLUGINS_MAINS][CLI_PATH_LONGEST];
    unsigned int mains;
} CliCodeMains;

// `name` kept as one of the files the plugin's code is read from, where room is left
static void code_main_kept(CliCodeMains *mains, const char *name)
{
    if (mains->mains < CLI_PLUGINS_MAINS)
    {
        CliLine kept = {mains->main[mains->mains], CLI_PATH_LONGEST, 0ull};
        cli_line_text(&kept, name);
        mains->mains += (cli_line_closed(&kept) != 0ull) ? 1u : 0u;
    }
}

// the files the plugin.json `text`, `size` bytes, names as `main` into `mains`: 1, or 0 where it does not read
static int code_mains_read(const char *text, unsigned long long size, CliCodeMains *mains)
{
    CliJson json;
    cli_json_begun(&json, NULL, 0u, NULL, 0ull);
    char why[CLI_PATH_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_taken(&json, text, size, 0u, 0ull, &said);
    mains->mains = 0u;
    const unsigned int main = (root != CLI_JSON_NONE) ? cli_json_member(&json, root, "main") : CLI_JSON_NONE;
    for (unsigned int item = cli_json_is(&json, main, CFG_JSON_ARRAY) ? json.node[main].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json.node[item].next)
    {
        const char *const name = cli_json_string(&json, item, NULL);
        // a name that is no string is kept empty, so it is refused as no file of the folder's
        code_main_kept(mains, (name != NULL) ? name : "");
    }
    const char *const one = cli_json_string(&json, main, NULL);
    if (mains->mains == 0u)
    {
        code_main_kept(mains, (one != NULL) ? one : "plugin.js");
    }
    cli_json_given(&json);
    return root != CLI_JSON_NONE;
}

// whether `name` is a file of the plugin's own folder: each part of it none empty, `.` or `..`, and the name not whole
static int code_own(const char *name)
{
    int own = (name[0] != '/') && (name[0] != '\\') &&
              (cellul.chr(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL, .byte = ':'}) == NULL);
    const char *part = name;
    while (own && (part[0] != '\0'))
    {
        const char *end = part;
        while ((end[0] != '\0') && (end[0] != '/') && (end[0] != '\\'))
        {
            end += 1;
        }
        const unsigned long long length = (unsigned long long)(end - part);
        own = (length != 0ull) && !((length == 1u) && (part[0] == '.')) &&
              !((length == 2u) && (part[0] == '.') && (part[1] == '.'));
        part = (end[0] != '\0') ? (end + 1) : end;
    }
    return own && (name[0] != '\0');
}

// the file `name` of the plugin's folder `folder` read onto the end of `out`: 1, or 0 with why said
static int code_file_read(const char *folder, const char *name, CliLine *out, CliLine *said)
{
    char file[CLI_PATH_LONGEST];
    CliLine line = {file, sizeof(file), 0ull};
    cli_line_text(&line, folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    unsigned long long file_size = 0ull;
    const int found =
        (cli_line_closed(&line) != 0ull) && cli_file_size(file, &file_size) && cli_line_room(out, file_size);
    const unsigned long long room = cli_line_fits(out) ? (out->capacity - out->at) : 0ull;
    const unsigned long long size =
        found ? qry_member_file_read(file, (unsigned char *)(out->out + out->at), room) : QRY_MEMBER_UNREAD;
    if (size == QRY_MEMBER_UNREAD)
    {
        cli_line_text(said, file);
        cli_line_text(said, ": it could not be read");
        return 0;
    }
    out->at += size;
    return 1;
}

int cli_plugins_code_in(const char *id, const char *folder, const char *text, unsigned long long size, CliLine *out,
                        CliLine *said)
{
    static CliCodeMains s_mains;
    static CliLock s_mains_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_mains_lock);
    int done = code_mains_read(text, size, &s_mains);
    if (!done)
    {
        cli_line_text(said, id);
        cli_line_text(said, ": its plugin.json does not read");
    }
    for (unsigned int at = 0u; done && (at < s_mains.mains); at += 1u)
    {
        done = code_own(s_mains.main[at]);
        if (!done)
        {
            cli_line_text(said, id);
            cli_line_text(said, " names ");
            cli_line_text(said, s_mains.main[at]);
            cli_line_text(said, ", which is not a file of its own folder");
        }
    }
    for (unsigned int at = 0u; done && (at < s_mains.mains); at += 1u)
    {
        cli_line_text(out, (at > 0u) ? "\n" : "");
        done = code_file_read(folder, s_mains.main[at], out, said);
    }
    cli_lock_give(&s_mains_lock);
    return done;
}

