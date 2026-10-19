// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// executables_cmake.c: the file a CMake build made, as the reply of CMake's file API names it, and the file a Meson
// build made, as `meson introspect` names it
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "executables_internal.h"

// the most nodes and bytes of a reply read
#define CLI_REPLY_NODES (1u << 17u)

#define CLI_REPLY_BYTES (1u << 23u)

// A reply's file read, its tree, and what is read of it, a cell taken with the lock and given back with it
typedef struct
{
    char file[CLI_REPLY_BYTES];
    char text[CLI_REPLY_BYTES];
    CliJsonNode nodes[CLI_REPLY_NODES];
    CfgJsonToken tokens[CLI_REPLY_NODES];
} CliReplyRoom;

static CliReplyRoom *s_reply;
static CliLock s_lock = CLI_LOCK_INIT;

// the lock taken and the reply's room a cell under it: 1, or 0 with why said and the lock given back
static int reply_held(CliLine *said)
{
    cli_lock_take(&s_lock);
    s_reply = cli_cell_taken(sizeof(*s_reply), "a build's reply");
    if (s_reply == NULL)
    {
        cli_lock_give(&s_lock);
        cli_line_text(said, "a build's reply could not be taken");
    }
    return s_reply != NULL;
}

// the reply's room given back, and the lock with it
static void reply_released(void)
{
    cli_cell_given(s_reply);
    s_reply = NULL;
    cli_lock_give(&s_lock);
}

// the JSON file at `path` read into the tree, begun anew: its root, or CLI_JSON_NONE with why said
static unsigned int cli_json_file(CliJson *json, const char *path, CliLine *said)
{
    cli_json_begun(json, s_reply->nodes, CLI_REPLY_NODES, s_reply->text, sizeof(s_reply->text));
    const unsigned long long size = qry_member_file_read(path, (unsigned char *)s_reply->file, sizeof(s_reply->file));
    if (size == QRY_MEMBER_UNREAD)
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be read");
        return CLI_JSON_NONE;
    }
    char why[CLI_NAME_LONGEST];
    CliLine reason = {why, sizeof(why), 0ull};
    const unsigned int root = cli_json_read(json, s_reply->file, size, s_reply->tokens, CLI_REPLY_NODES, &reason);
    if (root == CLI_JSON_NONE)
    {
        cli_line_text(said, path);
        cli_line_text(said, ": ");
        cli_line_counted(said, why, reason.at);
    }
    return root;
}

// The reply's index files: the last of them by name
typedef struct
{
    char last[CLI_NAME_LONGEST];
    int has;
} CliIndex;

static void cli_index_each(void *context, const char *name, int is_folder)
{
    CliIndex *const index = (CliIndex *)context;
    if (!is_folder && (cellul.starts(&(CatenaFinitaCfg){.src = name, .other = "index-", .cap = CLI_CELLS_POOL})) &&
        (!index->has || (cli_text_order(name, index->last) > 0)))
    {
        memor.cpy(&(MemoriaCfg){.dst = index->last,
                                .src = name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
        index->has = 1;
    }
}

// `name` joined to `folder` into `out`, which holds CLI_PATH_LONGEST
static void cli_in(const char *folder, const char *name, char *out)
{
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    cli_line_text(&line, folder);
    cli_line_character(&line, '/');
    cli_line_text(&line, name);
    cli_line_closed(&line);
}

// the member `key` of the first item of `array` whose member `match` is `wanted`, as text: NULL where none is
static const char *cli_found_in(const CliJson *json, unsigned int array, const char *match, const char *wanted,
                                const char *key)
{
    for (unsigned int item = cli_json_is(json, array, CFG_JSON_ARRAY) ? json->node[array].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        const char *const value = cli_json_string(json, cli_json_member(json, item, match), NULL);
        if ((value != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = value, .other = wanted, .cap = CLI_CELLS_POOL})))
        {
            return cli_json_string(json, cli_json_member(json, item, key), NULL);
        }
    }
    return NULL;
}

// whether two texts are the same, ASCII's case aside
static int cli_same_case_aside(const char *one, const char *other)
{
    for (; (one[0] != '\0') && (other[0] != '\0'); one += 1, other += 1)
    {
        const char left = ((one[0] >= 'A') && (one[0] <= 'Z')) ? (char)(one[0] + 32) : one[0];
        const char right = ((other[0] >= 'A') && (other[0] <= 'Z')) ? (char)(other[0] + 32) : other[0];
        if (left != right)
        {
            return 0;
        }
    }
    return (one[0] == '\0') && (other[0] == '\0');
}

// the first path of a target's artifacts that is no debug database or import library into `out`: 1, or 0
static int cli_artifact(const CliJson *json, unsigned int target, char *out)
{
    static const char *const s_passed[4] = {".pdb", ".lib", ".exp", ".ilk"};
    const unsigned int artifacts = cli_json_member(json, target, "artifacts");
    for (unsigned int item = cli_json_is(json, artifacts, CFG_JSON_ARRAY) ? json->node[artifacts].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        const char *const path = cli_json_string(json, cli_json_member(json, item, "path"), NULL);
        if (path == NULL)
        {
            continue;
        }
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
        int passed = 0;
        for (unsigned int at = 0u; !passed && (at < 4u); at += 1u)
        {
            passed = (length >= 4ull) && cli_same_case_aside(path + length - 4ull, s_passed[at]);
        }
        if (!passed)
        {
            memor.cpy(&(MemoriaCfg){.dst = out,
                                    .src = path,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
            return 1;
        }
    }
    return 0;
}

int cli_cmake_artifact(const char *build, const char *name, const char *config, char *out, CliLine *said)
{
    char reply[CLI_PATH_LONGEST];
    cli_in(build, ".cmake/api/v1/reply", reply);
    CliIndex index = {"", 0};
    if (!query_folder_each(reply, cli_index_each, &index))
    {
        cli_line_text(said, reply);
        cli_line_text(said, ": it could not be read");
        return 0;
    }
    if (!index.has)
    {
        cli_line_text(said, "CMake wrote no reply in ");
        cli_line_text(said, reply);
        return 0;
    }
    if (!reply_held(said))
    {
        return 0;
    }
    CliJson json;
    char path[CLI_PATH_LONGEST];
    char file[CLI_PATH_LONGEST];
    cli_in(reply, index.last, path);
    unsigned int root = cli_json_file(&json, path, said);
    const char *const model = (root != CLI_JSON_NONE) ? cli_found_in(&json, cli_json_member(&json, root, "objects"),
                                                                     "kind", "codemodel", "jsonFile")
                                                      : NULL;
    if ((root != CLI_JSON_NONE) && (model == NULL))
    {
        cli_line_text(said, "CMake's reply holds no code model");
    }
    if (model != NULL)
    {
        cli_in(reply, model, file);
    }
    root = (model != NULL) ? cli_json_file(&json, file, said) : CLI_JSON_NONE;
    // the configuration named, its case aside, else the first
    const unsigned int configs = cli_json_member(&json, root, "configurations");
    unsigned int chosen = cli_json_item(&json, configs, 0u);
    for (unsigned int item = cli_json_is(&json, configs, CFG_JSON_ARRAY) ? json.node[configs].first : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json.node[item].next)
    {
        const char *const named = cli_json_string(&json, cli_json_member(&json, item, "name"), NULL);
        if ((named != NULL) && cli_same_case_aside(named, config))
        {
            chosen = item;
            break;
        }
    }
    if ((root != CLI_JSON_NONE) && (chosen == CLI_JSON_NONE))
    {
        cli_line_text(said, "CMake's code model holds no configuration");
    }
    const char *const target =
        (chosen != CLI_JSON_NONE)
            ? cli_found_in(&json, cli_json_member(&json, chosen, "targets"), "name", name, "jsonFile")
            : NULL;
    if ((chosen != CLI_JSON_NONE) && (target == NULL))
    {
        cli_line_text(said, "CMake's code model holds no target ");
        cli_line_text(said, name);
    }
    if (target != NULL)
    {
        cli_in(reply, target, file);
    }
    root = (target != NULL) ? cli_json_file(&json, file, said) : CLI_JSON_NONE;
    char artifact[CLI_PATH_LONGEST];
    const int found = (root != CLI_JSON_NONE) && cli_artifact(&json, root, artifact);
    reply_released();
    if ((root != CLI_JSON_NONE) && !found)
    {
        cli_line_text(said, "CMake names no file for ");
        cli_line_text(said, name);
    }
    if (!found)
    {
        return 0;
    }
    const int absolute =
        (artifact[0] == '/') || (artifact[0] == '\\') || ((artifact[0] != '\0') && (artifact[1] == ':'));
    if (absolute)
    {
        memor.cpy(&(MemoriaCfg){.dst = out,
                                .src = artifact,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = artifact, .cap = CLI_CELLS_POOL}) + 1u});
    }
    else
    {
        cli_in(build, artifact, out);
    }
    return 1;
}

int cli_meson_artifact(const char *build, const char *name, char *out, CliLine *said)
{
    const char *const args[3] = {"introspect", "--targets", build};
    CliRan *const ran = cli_ran_taken();
    if (!cli_executables_output("meson", "meson", args, 3u, NULL, ran, said))
    {
        cli_ran_given(ran);
        return 0;
    }
    if (!reply_held(said))
    {
        cli_ran_given(ran);
        return 0;
    }
    CliJson json;
    cli_json_begun(&json, s_reply->nodes, CLI_REPLY_NODES, s_reply->text, sizeof(s_reply->text));
    char why[CLI_NAME_LONGEST];
    CliLine reason = {why, sizeof(why), 0ull};
    const unsigned int targets =
        cli_json_read(&json, ran->out, ran->out_size, s_reply->tokens, CLI_REPLY_NODES, &reason);
    cli_ran_given(ran);
    if (targets == CLI_JSON_NONE)
    {
        reply_released();
        cli_line_text(said, "meson introspect: ");
        cli_line_counted(said, why, reason.at);
        return 0;
    }
    int found = 0;
    for (unsigned int item = cli_json_is(&json, targets, CFG_JSON_ARRAY) ? json.node[targets].first : CLI_JSON_NONE;
         !found && (item != CLI_JSON_NONE); item = json.node[item].next)
    {
        const char *const named = cli_json_string(&json, cli_json_member(&json, item, "name"), NULL);
        const char *const kind = cli_json_string(&json, cli_json_member(&json, item, "type"), NULL);
        if ((named == NULL) || (kind == NULL) ||
            (!cellul.eq(&(CatenaFinitaCfg){.src = named, .other = name, .cap = CLI_CELLS_POOL})) ||
            (!cellul.eq(&(CatenaFinitaCfg){.src = kind, .other = "executable", .cap = CLI_CELLS_POOL})))
        {
            continue;
        }
        const char *const file =
            cli_json_string(&json, cli_json_item(&json, cli_json_member(&json, item, "filename"), 0u), NULL);
        if (file != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = out,
                                    .src = file,
                                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = file, .cap = CLI_CELLS_POOL}) + 1u});
        }
        // the first target named so ends the search, whether or not it names a file
        found = (file != NULL) ? 1 : -1;
    }
    reply_released();
    if (found != 1)
    {
        cli_line_text(said, "Meson names no executable ");
        cli_line_text(said, name);
        return 0;
    }
    return 1;
}
