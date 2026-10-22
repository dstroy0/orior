// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// trust.c: the reader's answers kept by folder, in trust.json in orior's home
#include "../text/json_tree.h"
#include "../text/regexp.h"
#include "../text/text.h"
#include "trust_internal.h"

// the nodes and the bytes of their texts the answers kept take past those read, the list, its two lists, the folder
// answered and the names of the lists among them; and the bytes the answers are begun to be written in, doubling as
// they fill
#define CLI_TRUST_NODES_MORE 4u
#define CLI_TRUST_TEXT_MORE 64u
#define CLI_TRUST_WRITTEN_FIRST 4096u

// the file of answers read and written under its lock
static CliLock s_kept_lock = CLI_LOCK_INIT;

// the file of answers into `path`, which holds CLI_PATH_LONGEST: 1, or 0 where orior_cli has no home folder
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

// the file of answers at `path` read into a tree in a cell of its own, given back with cli_json_given, the lock held:
// its `trusted` and `refused` lists, each none where the file has no such list
static void cli_kept_read(CliJson *json, const char *path, unsigned int *trusted, unsigned int *refused)
{
    char why[CLI_NAME_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_file_taken(json, path, 0u, 0ull, &said);
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
    CliJson json = {0};
    unsigned int yes = CLI_JSON_NONE;
    unsigned int no = CLI_JSON_NONE;
    cli_kept_read(&json, path, &yes, &no);
    const unsigned long long by_trusted = cli_nearest(&json, yes, key);
    const unsigned long long by_refused = cli_nearest(&json, no, key);
    cli_json_given(&json);
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

// the folders of a list of `from` not under `folder` put after the items of `into` in `json`
static void cli_kept_without(const CliJson *from, unsigned int list, const char *folder, CliJson *json,
                             unsigned int into)
{
    for (unsigned int item = (list != CLI_JSON_NONE) ? from->node[list].first : CLI_JSON_NONE; item != CLI_JSON_NONE;
         item = from->node[item].next)
    {
        unsigned long long length = 0ull;
        const char *const one = cli_json_string(from, item, &length);
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
    const unsigned long long key_length = cellul.len(&(CatenaFinitaCfg){.src = key, .cap = CLI_CELLS_POOL});
    cli_lock_take(&s_kept_lock);
    CliJson read = {0};
    unsigned int yes = CLI_JSON_NONE;
    unsigned int no = CLI_JSON_NONE;
    cli_kept_read(&read, path, &yes, &no);
    // the answers kept, made in a tree of the room of those read and of the folder answered
    CliJson json = {0};
    int done = cli_json_held(&json, read.nodes + CLI_TRUST_NODES_MORE, read.used + key_length + CLI_TRUST_TEXT_MORE,
                             "the answers kept");
    const unsigned int kept = cli_json_made(&json, CFG_JSON_OBJECT);
    const unsigned int kept_yes = cli_json_made(&json, CFG_JSON_ARRAY);
    const unsigned int kept_no = cli_json_made(&json, CFG_JSON_ARRAY);
    cli_kept_without(&read, yes, key, &json, kept_yes);
    cli_kept_without(&read, no, key, &json, kept_no);
    cli_json_given(&read);
    cli_json_append(&json, trusted ? kept_yes : kept_no, cli_json_string_made(&json, key, key_length));
    cli_json_set(&json, kept, "trusted", kept_yes);
    cli_json_set(&json, kept, "refused", kept_no);
    char parent[CLI_PATH_LONGEST];
    done = done && !json.full;
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
    CliLine out = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
    (void)cli_line_growing(&out, CLI_TRUST_WRITTEN_FIRST, "the answers kept, as they are written");
    cli_json_write(&json, kept, &out, 2u, 0);
    if (done && (!cli_line_fits(&out) || !query_file_written(path, out.out, out.at)))
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be written");
        done = 0;
    }
    cli_cell_given(out.out);
    cli_json_given(&json);
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
        cli_line_text(said, "orior_cli has no home folder");
        return 0;
    }
    return cli_trust_set_in(path, folder, trusted, said);
}
