// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// devcontainer_plan.c: a description's JSON read, its strings, members and lists, its variables read in, and the
// plan's texts it is stored in
#include "../../../../src/cu/includes/formats/cfg_json/cfg_json.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "devcontainer_internal.h"

// the most bytes a description contains and the most tokens it reads into
#define CLI_DESCRIPTION_BYTES (1u << 20u)

#define CLI_DESCRIPTION_TOKENS 65536u

// the description made plain, a cell of the bytes it was read in and one more, and its tokens, a cell of their count;
// guarded by the plan's lock from the read to the plan made
static char *s_plain;
static CfgJsonToken *s_tokens;

// 1 where a text of the description did not fit what a plan stores
static int s_full;

int cli_plan_description_read(const CliDevcontainerCtx *args)
{
    s_full = 0;
    unsigned long long size = 0ULL;
    const unsigned char *const raw = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->file,
                                                .size_pointer = &size, .owner = &g_cli_devcontainer_cells);
    if ((raw == NULL) || (size > CLI_DESCRIPTION_BYTES))
    {
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = raw,
                   .owner = &g_cli_devcontainer_cells);
        mmgr_line_put(args->err_msg, args->file);
        mmgr_line_put(args->err_msg, ": it could not be read");
        return 0;
    }
    // plain JSON is the description with its comments and trailing commas left out, never longer
    s_plain = g_cli_cells->persistent_buf_alloc(size + 1ULL, &g_cli_devcontainer_cells);
    VerbaLinea plain = mmgr_line_over(s_plain, size + 1ULL);
    // the description's bytes read as the text they are
    EMBED_CALL(cli_devcontainer_text.plain_json, CliDevcontainerTextCfg, .text = (const char *)raw, .size = size,
               .plain = &plain);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = raw, .owner = &g_cli_devcontainer_cells);
    const unsigned int counted =
        mmgr_line_ok(&plain)
            ? EMBED_CALL(cli_json_tree_text.json_tokens_count, CliJsonTreeTextCfg, .text = s_plain, .size = plain.at)
            : 0u;
    const unsigned int tokens =
        (counted < CLI_DESCRIPTION_TOKENS) ? ((counted != 0u) ? counted : 1u) : CLI_DESCRIPTION_TOKENS;
    s_tokens = mmgr_line_ok(&plain)
                   ? g_cli_cells->persistent_buf_alloc(tokens * sizeof(*s_tokens), &g_cli_devcontainer_cells)
                   : NULL;
    CfgJsonParse parse = {0u, 0u, 0u, NULL};
    if ((s_tokens != NULL) && cfg_json_parse_metadata(s_plain, plain.at, s_tokens, tokens, &parse))
    {
        return 1;
    }
    if ((s_plain == NULL) || (mmgr_line_ok(&plain) && (s_tokens == NULL)))
    {
        cli_plan_description_free();
        mmgr_line_put(args->err_msg, args->file);
        mmgr_line_put(args->err_msg, ": the cells cannot meet it");
        return 0;
    }
    cli_plan_description_free();
    mmgr_line_put(args->err_msg, args->file);
    mmgr_line_put(args->err_msg, ": ");
    if (!mmgr_line_ok(&plain))
    {
        mmgr_line_put(args->err_msg, "it is longer than a description is read");
        return 0;
    }
    mmgr_line_put(args->err_msg, (parse.reason != NULL) ? parse.reason : "it is not JSON");
    mmgr_line_put(args->err_msg, " at line ");
    mmgr_line_decimal(args->err_msg, parse.line, 1u);
    mmgr_line_put(args->err_msg, ", column ");
    mmgr_line_decimal(args->err_msg, parse.column, 1u);
    return 0;
}

void cli_plan_description_free(void)
{
    g_cli_cells->persistent_buf_release(s_tokens, &g_cli_devcontainer_cells);
    g_cli_cells->persistent_buf_release(s_plain, &g_cli_devcontainer_cells);
    s_tokens = NULL;
    s_plain = NULL;
}

int cli_plan_overflow(void)
{
    return s_full;
}

// A string too long to decode is recorded as none and the plan refused
int cli_plan_string(const CliDevcontainerValueOutCtx *args)
{
    if ((args->value == 0u) || (s_tokens[args->value].kind != CFG_JSON_STRING))
    {
        return 0;
    }
    if (!cfg_json_string(s_plain, &s_tokens[args->value], args->out, CLI_STRING_MAX_LEN))
    {
        s_full = 1;
        return 0;
    }
    return 1;
}

unsigned int cli_plan_member(const CliDevcontainerNameCtx *args)
{
    return cfg_json_member(s_plain, s_tokens, 0u, args->name);
}

unsigned int cli_plan_member_get(const CliDevcontainerNameCtx *args)
{
    return (args->object == 0u) ? 0u : cfg_json_member(s_plain, s_tokens, args->object, args->name);
}

unsigned int cli_plan_strings(const CliDevcontainerValueCtx *args)
{
    if ((args->value != 0u) && (s_tokens[args->value].kind == CFG_JSON_STRING) && (args->capacity != 0u))
    {
        args->tokens[0] = args->value;
        return 1u;
    }
    unsigned int count = 0u;
    unsigned int at = args->value + 1u;
    for (unsigned int each = 0u; (args->value != 0u) && (s_tokens[args->value].kind == CFG_JSON_ARRAY) &&
                                 (each < s_tokens[args->value].count) && (count < args->capacity);
         each += 1u)
    {
        if (s_tokens[at].kind == CFG_JSON_STRING)
        {
            args->tokens[count] = at;
            count += 1u;
        }
        at = s_tokens[at].next;
    }
    return count;
}

typedef struct
{
    unsigned int one;
    unsigned int other;
    unsigned int object;
    unsigned int *keys;
    unsigned int *values;
    unsigned int capacity;
    const char *path;
    CliPlan *plan;
    unsigned int value;
    CliPlanCommand *commands;
    unsigned int *count;
    const CliPlanPositions *positions;
} DevcontainerPlanCtx;
// whether two keys are written the same
EMBED_INLINE int cli_same_key(const DevcontainerPlanCtx *args)
{
    unsigned int other = args->other;
    const size_t size = s_tokens[args->one].end - s_tokens[args->one].start;
    return ((s_tokens[other].end - s_tokens[other].start) == size) &&
           (memor.cmp(&(MemoriaCfg){.src = s_plain + s_tokens[args->one].start,
                                    .other = s_plain + s_tokens[other].start,
                                    .bytes = size}) == 0);
}

// each member of the object at token `object` once, as a JSON object contains it: where its key first stands, with the
// value of its last; the count, each key and value token into `keys` and `values`
EMBED_INLINE unsigned int cli_members(const DevcontainerPlanCtx *args)
{
    unsigned int count = 0u;
    unsigned int key = args->object + 1u;
    for (unsigned int member = 0u; (args->object != 0u) && (s_tokens[args->object].kind == CFG_JSON_OBJECT) &&
                                   (member < s_tokens[args->object].count);
         member += 1u)
    {
        unsigned int seen = 0u;
        while ((seen < count) && !EMBED_CALL(cli_same_key, DevcontainerPlanCtx, .one = args->keys[seen], .other = key))
        {
            seen += 1u;
        }
        if (seen < count)
        {
            args->values[seen] = key + 1u;
        }
        else if (count < args->capacity)
        {
            args->keys[count] = key;
            args->values[count] = key + 1u;
            count += 1u;
        }
        else
        {
            s_full = 1;
        }
        key = s_tokens[key + 1u].next;
    }
    return count;
}

// The empty text at 0 where `text` does not fit
unsigned int cli_plan_append(const CliDevcontainerPlanTextCtx *args)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL}) + 1ULL;
    if ((args->plan->used + size) > CLI_PLAN_TEXT)
    {
        s_full = 1;
        return 0u;
    }
    const unsigned int at = args->plan->used;
    memor.cpy(&(MemoriaCfg){.dst = args->plan->text + at, .src = args->text, .bytes = size});
    // the size is below CLI_PLAN_TEXT, checked above
    args->plan->used += (unsigned int)size;
    return at;
}

void cli_plan_list(const CliDevcontainerPlanTextCtx *args)
{
    if (args->list->count >= CLI_PLAN_LIST_MAX)
    {
        s_full = 1;
        return;
    }
    args->list->at[args->list->count] = cli_devcontainer_plan_text_internal.plan_append(args);
    args->list->count += 1u;
}

// the last part of `path`, after its last separator
EMBED_INLINE const char *cli_last_part(const DevcontainerPlanCtx *args)
{
    const char *last = args->path + cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL});
    last = args->path + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                                   .at = (unsigned long long)(last - args->path), .set = "/\\");
    return last;
}

// A variable the description does not know, and `${` with no `}` after it, stand as they are
void cli_plan_expand(const CliDevcontainerOutCtx *args)
{
    VerbaLinea line = {args->out, CLI_STRING_MAX_LEN, 0ULL};
    const char *rest = args->text;
    const char *open =
        cellul.find(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL, .other = "${", .other_cap = CLI_CELLS_POOL});
    while (open != NULL)
    {
        mmgr_line_put_n(&line, rest, (unsigned long long)(open - rest));
        const char *const close = cellul.chr(&(CatenaFinitaCfg){.src = open, .cap = CLI_CELLS_POOL, .byte = '}'});
        if (close == NULL)
        {
            rest = open;
            break;
        }
        char named[CLI_STRING_MAX_LEN];
        VerbaLinea name_line = {named, sizeof(named), 0ULL};
        mmgr_line_put_n(&name_line, open + 2, (unsigned long long)(close - open - 2));
        mmgr_line_finish(&name_line);
        if (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = "localWorkspaceFolder", .cap = CLI_CELLS_POOL}))
        {
            mmgr_line_put(&line, args->positions->root);
        }
        else if (cellul.eq(
                     &(CatenaFinitaCfg){.src = named, .other = "localWorkspaceFolderBasename", .cap = CLI_CELLS_POOL}))
        {
            mmgr_line_put(&line, EMBED_CALL(cli_last_part, DevcontainerPlanCtx, .path = args->positions->root));
        }
        else if (cellul.eq(
                     &(CatenaFinitaCfg){.src = named, .other = "containerWorkspaceFolder", .cap = CLI_CELLS_POOL}))
        {
            mmgr_line_put(&line, args->positions->folder);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){
                     .src = named, .other = "containerWorkspaceFolderBasename", .cap = CLI_CELLS_POOL}))
        {
            const char *const slash = EMBED_CALL(
                cli_text_byte.last, CliTextByteCfg, .text = args->positions->folder,
                .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->positions->folder, .cap = CLI_CELLS_POOL}),
                .byte = '/');
            mmgr_line_put(&line, (slash != NULL) ? (slash + 1) : args->positions->folder);
        }
        else if (cellul.starts(&(CatenaFinitaCfg){.src = named, .other = "localEnv:", .cap = CLI_CELLS_POOL}))
        {
            const char *const name = named + 9;
            char *const colon = cellul.chr(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL, .byte = ':'});
            const char *const fallback = (colon != NULL) ? (colon + 1) : "";
            if (colon != NULL)
            {
                colon[0] = '\0';
            }
            if (!EMBED_CALL(query_name_out_internal.environment_write, QueryNameOutCtx, .name = name, .out = &line))
            {
                mmgr_line_put(&line, fallback);
            }
        }
        else
        {
            mmgr_line_put_n(&line, open, (unsigned long long)(close - open) + 1ULL);
        }
        rest = close + 1;
        open = cellul.find(
            &(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL, .other = "${", .other_cap = CLI_CELLS_POOL});
    }
    mmgr_line_put(&line, rest);
    s_full = s_full || !mmgr_line_ok(&line);
    mmgr_line_finish(&line);
}

// `path` alone where it is whole, else after `base` and the separator `base` writes
void cli_plan_join(const CliDevcontainerOutBaseCtx *args)
{
    const int whole =
        (args->path[0] == '/') || (args->path[0] == '\\') || ((args->path[0] != '\0') && (args->path[1] == ':'));
    VerbaLinea line = {args->out, CLI_PATH_MAX_LEN, 0ULL};
    if (!whole)
    {
        const char *const forward = EMBED_CALL(
            cli_text_byte.last, CliTextByteCfg, .text = args->base,
            .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->base, .cap = CLI_CELLS_POOL}), .byte = '/');
        const char *const backward = EMBED_CALL(
            cli_text_byte.last, CliTextByteCfg, .text = args->base,
            .length_value = cellul.len(&(CatenaFinitaCfg){.src = args->base, .cap = CLI_CELLS_POOL}), .byte = '\\');
        mmgr_line_put(&line, args->base);
        mmgr_line_ch(&line, ((backward != NULL) && ((forward == NULL) || (backward > forward))) ? '\\' : '/');
    }
    mmgr_line_put(&line, args->path);
    s_full = s_full || !mmgr_line_ok(&line);
    mmgr_line_finish(&line);
}

// a command the description runs, as the value at token `value` writes it: a string a shell's line, an array a
// program's words, an object each of its values in turn
static void cli_commands_get(const DevcontainerPlanCtx *args)
{
    CliPlan *plan = args->plan;
    unsigned int value = args->value;
    unsigned int *count = args->count;
    static char s_word[CLI_STRING_MAX_LEN];
    const CfgJsonKind kind = s_tokens[value].kind;
    if ((kind == CFG_JSON_STRING) || (kind == CFG_JSON_ARRAY))
    {
        if (*count >= CLI_PLAN_COMMANDS_MAX)
        {
            s_full = 1;
            return;
        }
        unsigned int tokens[CLI_PLAN_LIST_MAX];
        const unsigned int words = EMBED_CALL(cli_devcontainer_value_internal.plan_strings, CliDevcontainerValueCtx,
                                              .value = value, .tokens = tokens, .capacity = CLI_PLAN_LIST_MAX);
        const CliPlanCommand command = {kind == CFG_JSON_STRING, plan->words.count, words};
        for (unsigned int at = 0u; at < words; at += 1u)
        {
            if (EMBED_CALL(cli_devcontainer_value_out_internal.plan_string, CliDevcontainerValueOutCtx,
                           .value = tokens[at], .out = s_word))
            {
                EMBED_CALL(cli_devcontainer_plan_text_internal.plan_list, CliDevcontainerPlanTextCtx, .plan = plan,
                           .list = &plan->words, .text = s_word);
            }
        }
        args->commands[*count] = command;
        *count += 1u;
    }
    else if (kind == CFG_JSON_OBJECT)
    {
        unsigned int keys[CLI_PLAN_LIST_MAX];
        unsigned int values[CLI_PLAN_LIST_MAX];
        const unsigned int members = EMBED_CALL(cli_members, DevcontainerPlanCtx, .object = value, .keys = keys,
                                                .values = values, .capacity = CLI_PLAN_LIST_MAX);
        for (unsigned int at = 0u; at < members; at += 1u)
        {
            EMBED_CALL(cli_commands_get, DevcontainerPlanCtx, .plan = plan, .value = values[at],
                       .commands = args->commands, .count = count);
        }
    }
}

void cli_plan_commands(const CliDevcontainerPlanCtx *args)
{
    const unsigned int value =
        EMBED_CALL(cli_devcontainer_name_internal.plan_member, CliDevcontainerNameCtx, .name = args->key);
    if ((value != 0u) && (s_tokens[value].kind != CFG_JSON_NULL))
    {
        EMBED_CALL(cli_commands_get, DevcontainerPlanCtx, .plan = args->plan, .value = value,
                   .commands = args->commands, .count = args->count);
    }
}

void cli_plan_pairs(const CliDevcontainerObjectCtx *args)
{
    static char s_key[CLI_STRING_MAX_LEN];
    static char s_value[CLI_STRING_MAX_LEN];
    static char s_read[CLI_STRING_MAX_LEN];
    unsigned int keys[CLI_PLAN_LIST_MAX];
    unsigned int values[CLI_PLAN_LIST_MAX];
    const unsigned int members = EMBED_CALL(cli_members, DevcontainerPlanCtx, .object = args->object, .keys = keys,
                                            .values = values, .capacity = CLI_PLAN_LIST_MAX);
    for (unsigned int at = 0u; at < members; at += 1u)
    {
        if (cfg_json_string(s_plain, &s_tokens[keys[at]], s_key, sizeof(s_key)) &&
            EMBED_CALL(cli_devcontainer_value_out_internal.plan_string, CliDevcontainerValueOutCtx, .value = values[at],
                       .out = s_value))
        {
            EMBED_CALL(cli_devcontainer_out_internal.plan_expand, CliDevcontainerOutCtx, .text = s_value,
                       .positions = args->positions, .out = s_read);
            EMBED_CALL(cli_devcontainer_plan_text_internal.plan_list, CliDevcontainerPlanTextCtx, .plan = args->plan,
                       .list = args->list, .text = s_key);
            EMBED_CALL(cli_devcontainer_plan_text_internal.plan_list, CliDevcontainerPlanTextCtx, .plan = args->plan,
                       .list = args->list, .text = s_read);
        }
    }
}

// a mount as `mounts` names it, read: a string as docker takes it, or an object of its parts joined `key=value,...`
EMBED_INLINE void cli_mount_get(const DevcontainerPlanCtx *args)
{
    CliPlan *plan = args->plan;
    unsigned int value = args->value;
    const CliPlanPositions *positions = args->positions;
    static char s_mount[CLI_STRING_MAX_LEN];
    static char s_read[CLI_STRING_MAX_LEN];
    static char s_part[CLI_STRING_MAX_LEN];
    static char s_key[CLI_STRING_MAX_LEN];
    if (EMBED_CALL(cli_devcontainer_value_out_internal.plan_string, CliDevcontainerValueOutCtx, .value = value,
                   .out = s_mount))
    {
        EMBED_CALL(cli_devcontainer_out_internal.plan_expand, CliDevcontainerOutCtx, .text = s_mount,
                   .positions = positions, .out = s_read);
        EMBED_CALL(cli_devcontainer_plan_text_internal.plan_list, CliDevcontainerPlanTextCtx, .plan = plan,
                   .list = &plan->mounts, .text = s_read);
        return;
    }
    if (s_tokens[value].kind != CFG_JSON_OBJECT)
    {
        return;
    }
    VerbaLinea line = {s_mount, sizeof(s_mount), 0ULL};
    unsigned int keys[CLI_PLAN_LIST_MAX];
    unsigned int values[CLI_PLAN_LIST_MAX];
    const unsigned int members = EMBED_CALL(cli_members, DevcontainerPlanCtx, .object = value, .keys = keys,
                                            .values = values, .capacity = CLI_PLAN_LIST_MAX);
    unsigned int written = 0u;
    for (unsigned int at = 0u; at < members; at += 1u)
    {
        if (!EMBED_CALL(cli_devcontainer_value_out_internal.plan_string, CliDevcontainerValueOutCtx,
                        .value = values[at], .out = s_part))
        {
            continue;
        }
        if (written != 0u)
        {
            mmgr_line_ch(&line, ',');
        }
        s_full = s_full || !cfg_json_string(s_plain, &s_tokens[keys[at]], s_key, sizeof(s_key));
        mmgr_line_put(&line, s_key);
        mmgr_line_ch(&line, '=');
        mmgr_line_put(&line, s_part);
        written += 1u;
    }
    s_full = s_full || !mmgr_line_ok(&line);
    mmgr_line_finish(&line);
    EMBED_CALL(cli_devcontainer_out_internal.plan_expand, CliDevcontainerOutCtx, .text = s_mount,
               .positions = positions, .out = s_read);
    EMBED_CALL(cli_devcontainer_plan_text_internal.plan_list, CliDevcontainerPlanTextCtx, .plan = plan,
               .list = &plan->mounts, .text = s_read);
}

void cli_plan_mounts(const CliDevcontainerObjectCtx *args)
{
    const unsigned int mounts =
        EMBED_CALL(cli_devcontainer_name_internal.plan_member, CliDevcontainerNameCtx, .name = "mounts");
    unsigned int at = mounts + 1u;
    for (unsigned int each = 0u;
         (mounts != 0u) && (s_tokens[mounts].kind == CFG_JSON_ARRAY) && (each < s_tokens[mounts].count); each += 1u)
    {
        EMBED_CALL(cli_mount_get, DevcontainerPlanCtx, .plan = args->plan, .value = at, .positions = args->positions);
        at = s_tokens[at].next;
    }
}
