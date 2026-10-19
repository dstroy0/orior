// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// trust.c: the reader's answers kept by folder, in trust.json in orior's home
#include "../text/json_tree.h"
#include "../text/regexp.h"
#include "../text/text.h"
#include "trust_internal.h"

// the most nodes of the file of answers
#define CLI_TRUST_NODES 4096u

// the file of answers read and written under its lock
static char s_file[CLI_TRUST_BYTES];
static char s_written[CLI_TRUST_BYTES];
static CliJsonNode s_nodes[CLI_TRUST_NODES];
static char s_text[CLI_TRUST_BYTES];
static CfgJsonToken s_tokens[CLI_TRUST_NODES];
static CliLock s_kept_lock = CLI_LOCK_INIT;

// the file of answers into `path`, which holds CLI_PATH_LONGEST: 1, or 0 where orior has no home folder
static int cli_trust_file(char *path)
{
    char home[CLI_PATH_LONGEST];
    if (!cli_home_folder(home))
    {
        return 0;
    }
    CliLine line = {path, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, home);
    cli_line_text(&line, "/trust.json");
    return cli_line_closed(&line) != 0ull;
}

// A folder as the answers are kept by into `key`, which holds CLI_PATH_LONGEST: its path with `/` between its parts,
// and in lower case where the system does not tell the cases apart
static void cli_key(const char *folder, char *key)
{
    CliLine line = {key, CLI_PATH_LONGEST, 0ull};
#if defined(_WIN32)
    static unsigned int s_letters[CLI_PATH_LONGEST];
    static CliLock s_letters_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_letters_lock);
    const unsigned long long count = cli_regexp_letters(
        folder, cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}), s_letters, CLI_PATH_LONGEST);
    for (unsigned long long at = 0ull; at < count; at += 1ull)
    {
        cli_utf8_put(&line, (s_letters[at] == '\\') ? '/' : cli_regexp_lower(s_letters[at]));
    }
    cli_lock_give(&s_letters_lock);
#else
    for (const char *at = folder; at[0] != '\0'; at += 1)
    {
        cli_line_character(&line, (at[0] == '\\') ? '/' : at[0]);
    }
#endif
    cli_line_closed(&line);
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL});
    while ((length > 0ull) && (key[length - 1ull] == '/'))
    {
        length -= 1ull;
    }
    key[length] = '\0';
}

int cli_trust_under(const char *inner, const char *folder)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL});
    return cli_text_same(inner, folder, length) && ((inner[length] == '\0') || (inner[length] == '/'));
}

// the file of answers at `path` read into the tree, the lock held: its `trusted` and `refused` lists, each none where
// the file has no such list
static void cli_kept_read(CliJson *json, const char *path, unsigned int *trusted, unsigned int *refused)
{
    cli_json_begun(json, s_nodes, CLI_TRUST_NODES, s_text, CLI_TRUST_BYTES);
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_file, sizeof(s_file));
    char why[CLI_NAME_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    const unsigned int root = (size != QRY_MEMBER_UNREAD)
                                  ? cli_json_read(json, s_file, size, s_tokens, CLI_TRUST_NODES, &said)
                                  : CLI_JSON_NONE;
    const int object = cli_json_is(json, root, CFG_JSON_OBJECT);
    *trusted = object ? cli_json_member(json, root, "trusted") : CLI_JSON_NONE;
    *refused = object ? cli_json_member(json, root, "refused") : CLI_JSON_NONE;
    *trusted = cli_json_is(json, *trusted, CFG_JSON_ARRAY) ? *trusted : CLI_JSON_NONE;
    *refused = cli_json_is(json, *refused, CFG_JSON_ARRAY) ? *refused : CLI_JSON_NONE;
}

// the length of the longest folder of a list that is the tree or holds it, plus one, or 0 where none is
static unsigned long long cli_nearest(const CliJson *json, unsigned int list, const char *tree)
{
    unsigned long long nearest = 0ull;
    for (unsigned int item = (list != CLI_JSON_NONE) ? json->node[list].first : CLI_JSON_NONE; item != CLI_JSON_NONE;
         item = json->node[item].next)
    {
        const char *const folder = cli_json_string(json, item, NULL);
        if ((folder != NULL) && cli_trust_under(tree, folder) &&
            ((cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1ull) > nearest))
        {
            nearest = cellul.len(&(CatenaFinitaCfg){.src = folder, .cap = CLI_CELLS_POOL}) + 1ull;
        }
    }
    return nearest;
}

int cli_trust_answer_in(const char *path, const char *tree, int *trusted)
{
    char key[CLI_PATH_LONGEST];
    cli_key(tree, key);
    cli_lock_take(&s_kept_lock);
    CliJson json;
    unsigned int yes = CLI_JSON_NONE;
    unsigned int no = CLI_JSON_NONE;
    cli_kept_read(&json, path, &yes, &no);
    const unsigned long long by_trusted = cli_nearest(&json, yes, key);
    const unsigned long long by_refused = cli_nearest(&json, no, key);
    cli_lock_give(&s_kept_lock);
    *trusted = by_trusted > by_refused;
    return (by_trusted != 0ull) || (by_refused != 0ull);
}

int cli_trust_answer(const char *tree, int *trusted)
{
    char path[CLI_PATH_LONGEST];
    *trusted = 0;
    return cli_trust_file(path) && cli_trust_answer_in(path, tree, trusted);
}

// the folders of a list not under `folder` put after the items of `into`
static void cli_kept_without(CliJson *json, unsigned int list, const char *folder, unsigned int into)
{
    for (unsigned int item = (list != CLI_JSON_NONE) ? json->node[list].first : CLI_JSON_NONE; item != CLI_JSON_NONE;
         item = json->node[item].next)
    {
        unsigned long long length = 0ull;
        const char *const one = cli_json_string(json, item, &length);
        if ((one != NULL) && !cli_trust_under(one, folder))
        {
            cli_json_append(json, into, cli_json_string_made(json, one, length));
        }
    }
}

int cli_trust_set_in(const char *path, const char *folder, int trusted, CliLine *said)
{
    char key[CLI_PATH_LONGEST];
    cli_key(folder, key);
    cli_lock_take(&s_kept_lock);
    CliJson json;
    unsigned int yes = CLI_JSON_NONE;
    unsigned int no = CLI_JSON_NONE;
    cli_kept_read(&json, path, &yes, &no);
    const unsigned int kept = cli_json_made(&json, CFG_JSON_OBJECT);
    const unsigned int kept_yes = cli_json_made(&json, CFG_JSON_ARRAY);
    const unsigned int kept_no = cli_json_made(&json, CFG_JSON_ARRAY);
    cli_kept_without(&json, yes, key, kept_yes);
    cli_kept_without(&json, no, key, kept_no);
    cli_json_append(
        &json, trusted ? kept_yes : kept_no,
        cli_json_string_made(&json, key, cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL})));
    cli_json_set(&json, kept, "trusted", kept_yes);
    cli_json_set(&json, kept, "refused", kept_no);
    char parent[CLI_PATH_LONGEST];
    int done = !json.full;
    if (!done)
    {
        cli_line_text(said, "the answers kept do not fit");
    }
    if (done && cli_path_parent(path, parent) && !query_folder_made(parent))
    {
        cli_line_text(said, parent);
        cli_line_text(said, ": it could not be made");
        done = 0;
    }
    CliLine out = {s_written, sizeof(s_written), 0ull};
    cli_json_write(&json, kept, &out, 2u, 0);
    if (done && (!cli_line_fits(&out) || !query_file_written(path, s_written, out.at)))
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be written");
        done = 0;
    }
    cli_lock_give(&s_kept_lock);
    if (done)
    {
        cli_trust_forget();
    }
    return done;
}

int cli_trust_set(const char *folder, int trusted, CliLine *said)
{
    char path[CLI_PATH_LONGEST];
    if (!cli_trust_file(path))
    {
        cli_line_text(said, "orior has no home folder");
        return 0;
    }
    return cli_trust_set_in(path, folder, trusted, said);
}
