// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// toolchains.c: the manifest read, orior's own and the reader's, a tool's program found in a folder, and the folders a
// pattern of toolchains.json names
#include "../command_line/callback_cfg.h"
#include "../command_line/pool.h"
#include "../text/regexp.h"
#include "../text/text.h"
#include "toolchains_internal.h"

#define CLI_PATTERN_LEVELS 16u

const char *cli_toolchains_system(void)
{
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#else
    return "linux";
#endif
}

typedef struct
{
    const CliJson *json;
    unsigned int tool;
    const char *text;
    char *out;
    const char *a;
    const char *a_end;
    const char *b;
    const char *b_end;
    const char *path;
    char separator;
    const CliToolchainsPaths *found_from;
    CliToolchainsPaths *found_into;
    CliToolchainsPaths *names;
} ToolchainsCtx;
// whether a tool is for this system: it names none, or this one
EMBED_INLINE int cli_for_here(const ToolchainsCtx *args)
{
    const CliJson *json = args->json;
    const char *const only =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->tool, .name = "only"),
                   .length_pointer = NULL);
    return (only == NULL) ||
           (cellul.eq(&(CatenaFinitaCfg){.src = only, .other = cli_toolchains_system(), .cap = CLI_POOL_BYTES}));
}

unsigned int cli_toolchains_manifest(const CliToolchainsJsonNodesMoreCfg *args)
{
    // the program's own bytes are toolchains.json's text, and the reader's file is read mapped
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = args->json_out,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    const char *const bundled_text = (const char *)g_cli_toolchains_json;
    char path[CLI_PATH_MAX_LEN];
    unsigned long long added_size = 0ULL;
    const unsigned char *added_text = NULL;
    if (EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = path))
    {
        VerbaLinea line = {path + cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}),
                           CLI_PATH_MAX_LEN - cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES}), 0ULL};
        mmgr_line_put(&line, "/user_toolchains.json");
        added_text = (mmgr_line_finish(&line) != 0ULL)
                         ? EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                      .size_pointer = &added_size, .owner = g_cli_toolchains_owner)
                         : NULL;
    }
    // the tree one cell sized to both texts' tokens and bytes and the array stored, the tokens one cell for the larger
    const unsigned int bundled_tokens = EMBED_CALL(cli_json_tree_text.json_tokens_count, CliJsonTreeTextCfg,
                                                   .text = bundled_text, .size = g_cli_toolchains_json_size) +
                                        1u;
    const unsigned int added_tokens = (added_text != NULL)
                                          ? (EMBED_CALL(cli_json_tree_text.json_tokens_count, CliJsonTreeTextCfg,
                                                        .text = (const char *)added_text, .size = added_size) +
                                             1u)
                                          : 0u;
    const unsigned int tokens_max = (bundled_tokens > added_tokens) ? bundled_tokens : added_tokens;
    CfgJsonToken *const tokens =
        g_cli_guard->temporary_buf_alloc((unsigned long long)tokens_max * sizeof(CfgJsonToken), g_cli_toolchains_owner);
    const int reserved =
        (tokens != NULL) &&
        EMBED_CALL(cli_json_tree_nodes_max.json_reserve, CliJsonTreeNodesMaxCfg, .json = args->json_out,
                   .nodes_max = bundled_tokens + added_tokens + 1u + args->nodes_more,
                   .text_max = g_cli_toolchains_json_size + added_size + 1ULL + args->text_more,
                   .what = "the toolchains' manifest", .owner = g_cli_toolchains_owner);
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    const unsigned int bundled =
        reserved ? EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg,
                              .json = args->json_out, .text = bundled_text, .size = g_cli_toolchains_json_size,
                              .tokens = tokens, .tokens_max = tokens_max, .err_msg = &err_msg)
                 : CLI_JSON_NONE;
    const unsigned int tools = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = args->json_out, .object = bundled, .name = "tools");
    const unsigned int added =
        (EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = args->json_out, .node = tools,
                    .type = CFG_JSON_ARRAY) &&
         (added_text != NULL))
            ? EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg, .json = args->json_out,
                         .text = (const char *)added_text, .size = added_size, .tokens = tokens,
                         .tokens_max = tokens_max, .err_msg = &err_msg)
            : CLI_JSON_NONE;
    g_cli_guard->persistent_buf_release(tokens, g_cli_toolchains_owner);
    g_cli_guard->persistent_buf_release((void *)(embed_word)added_text, (g_cli_toolchains_owner));
    if (!EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = args->json_out, .node = tools,
                    .type = CFG_JSON_ARRAY))
    {
        return CLI_JSON_NONE;
    }
    const unsigned int theirs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                           .json_read = args->json_out, .object = added, .name = "tools");
    for (unsigned int tool = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                        .json_read = args->json_out, .node = theirs, .type = CFG_JSON_ARRAY)
                                 ? args->json_out->node[theirs].first
                                 : CLI_JSON_NONE;
         tool != CLI_JSON_NONE;)
    {
        const unsigned int next = args->json_out->node[tool].next;
        EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = args->json_out, .array = tools,
                   .value = tool);
        tool = next;
    }
    // those for another system left out
    const unsigned int list_node =
        EMBED_CALL(cli_json_tree_type.json_init, CliJsonTreeTypeCfg, .json = args->json_out, .type = CFG_JSON_ARRAY);
    for (unsigned int tool = args->json_out->node[tools].first; tool != CLI_JSON_NONE;)
    {
        const unsigned int next = args->json_out->node[tool].next;
        if (EMBED_CALL(cli_for_here, ToolchainsCtx, .json = args->json_out, .tool = tool))
        {
            EMBED_CALL(cli_json_tree_array.json_append, CliJsonTreeArrayCfg, .json = args->json_out, .array = list_node,
                       .value = tool);
        }
        tool = next;
    }
    return args->json_out->full ? CLI_JSON_NONE : list_node;
}

unsigned int cli_toolchains_tool(const CliToolchainsJsonManifestCfg *args)
{
    for (unsigned int tool = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                        .json_read = args->json_out, .node = args->manifest, .type = CFG_JSON_ARRAY)
                                 ? args->json_out->node[args->manifest].first
                                 : CLI_JSON_NONE;
         tool != CLI_JSON_NONE; tool = args->json_out->node[tool].next)
    {
        const char *const named =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json_out,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = args->json_out, .object = tool, .name = "id"),
                       .length_pointer = NULL);
        if ((named != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = args->id, .cap = CLI_POOL_BYTES})))
        {
            return tool;
        }
    }
    mmgr_line_put(args->err_msg, "impero knows no toolchain ");
    mmgr_line_put(args->err_msg, args->id);
    mmgr_line_put(args->err_msg, ": imp file toolchains lists them");
    return CLI_JSON_NONE;
}

unsigned int cli_toolchains_for_system(const CliToolchainsJsonMapCfg *args)
{
    const unsigned int here = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                         .json_read = args->json, .object = args->map, .name = cli_toolchains_system());
    return (here != CLI_JSON_NONE) ? here
                                   : EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = args->json, .object = args->map, .name = "any");
}

int cli_toolchains_program_in(const CliToolchainsDirCfg *args)
{
#if defined(_WIN32)
    static const char *const s_extensions[3] = {".exe", ".cmd", ".bat"};
    const unsigned int forms = 3u;
#else
    static const char *const s_extensions[1] = {""};
    const unsigned int forms = 1u;
#endif
    for (unsigned int name = 0u; name < args->count; name += 1u)
    {
        for (unsigned int form = 0u; form < forms; form += 1u)
        {
            VerbaLinea line = {args->path, CLI_PATH_MAX_LEN, 0ULL};
            mmgr_line_put(&line, args->dir);
            mmgr_line_ch(&line, '/');
            mmgr_line_put(&line, args->names[name]);
            mmgr_line_put(&line, s_extensions[form]);
            if ((mmgr_line_finish(&line) != 0ULL) &&
                EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = args->path))
            {
                return 1;
            }
        }
    }
    return 0;
}

int cli_toolchains_tool_program_in(const CliToolchainsJsonDirCfg *args)
{
    const char *names[64];
    unsigned int count = 0u;
    const unsigned int programs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                             .json_read = args->json, .object = args->tool, .name = "programs");
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg,
                                        .json_read = args->json, .node = programs, .type = CFG_JSON_ARRAY)
                                 ? args->json->node[programs].first
                                 : CLI_JSON_NONE;
         (item != CLI_JSON_NONE) && (count < 64u); item = args->json->node[item].next)
    {
        const char *const name = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                            .json_read = args->json, .node = item, .length_pointer = NULL);
        if (name != NULL)
        {
            names[count] = name;
            count += 1u;
        }
    }
    return EMBED_CALL(cli_toolchains_dir.program_in, CliToolchainsDirCfg, .dir = args->dir, .names = names,
                      .count = count, .path = args->path);
}

// A folder as written in toolchains.json, its variables filled in: %NAME% on Windows, and ~ and $NAME elsewhere, into
// `out`, sized for CLI_PATH_MAX_LEN. A variable that is not set leaves the folder out: 0
EMBED_INLINE int cli_expand(const ToolchainsCtx *args)
{
    const char *text = args->text;
    char *out = args->out;
    VerbaLinea line = {out, CLI_PATH_MAX_LEN, 0ULL};
    const char *rest = text;
    char value[CLI_PATH_MAX_LEN];
    if (rest[0] == '~')
    {
        if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "HOME",
                        .value_out = value, .capacity = sizeof(value)) &&
            !EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "USERPROFILE",
                        .value_out = value, .capacity = sizeof(value)))
        {
            return 0;
        }
        mmgr_line_put(&line, value);
        rest += 1;
    }
    for (;;)
    {
        const char *mark = rest;
        mark += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = mark, .at = 0ULL,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = mark, .cap = CLI_POOL_BYTES}), .set = "%$");
        mmgr_line_put_n(&line, rest, (unsigned long long)(mark - rest));
        if (mark[0] == '\0')
        {
            break;
        }
        const char *const after = mark + 1;
        const char *end = after;
        if (mark[0] == '%')
        {
            end += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = end, .at = 0ULL,
                              .size = cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_POOL_BYTES}), .set = "%");
            if (end[0] == '\0')
            {
                return 0;
            }
        }
        else
        {
            end += EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = end, .at = 0ULL,
                              .size = cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_POOL_BYTES}),
                              .classes = CLI_TEXT_CHARACTERS | CLI_TEXT_DIGITS, .set = "_");
        }
        char name[CLI_NAME_MAX_LEN];
        VerbaLinea named = {name, sizeof(name), 0ULL};
        mmgr_line_put_n(&named, after, (unsigned long long)(end - after));
        if ((mmgr_line_finish(&named) == 0ULL) ||
            !EMBED_CALL(query_name_out_internal.environment_write, QueryNameOutCtx, .name = name, .out = &line))
        {
            return 0;
        }
        rest = (mark[0] == '%') ? (end + 1) : end;
    }
    return mmgr_line_finish(&line) != 0ULL;
}

// whether `name` matches `pattern`, whose * matches any run of characters, case aside on Windows
int cli_toolchains_matches(const CliToolchainsPatternCtx *args)
{
    char left[CLI_NAME_MAX_LEN];
    char right[CLI_NAME_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = left,
                            .src = args->pattern,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->pattern, .cap = CLI_POOL_BYTES}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = right,
                            .src = args->name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES}) + 1u});
#if defined(_WIN32)
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .text_out = left,
               .size = cellul.len(&(CatenaFinitaCfg){.src = left, .cap = CLI_POOL_BYTES}));
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .text_out = right,
               .size = cellul.len(&(CatenaFinitaCfg){.src = right, .cap = CLI_POOL_BYTES}));
#endif
    if (cellul.chr(&(CatenaFinitaCfg){.src = left, .cap = CLI_POOL_BYTES, .byte = '*'}) == NULL)
    {
        return cellul.eq(&(CatenaFinitaCfg){.src = left, .other = right, .cap = CLI_POOL_BYTES});
    }
    unsigned long long at = 0ULL;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = right, .cap = CLI_POOL_BYTES});
    const char *part = left;
    int first = 1;
    for (;;)
    {
        const char *const star = cellul.chr(&(CatenaFinitaCfg){.src = part, .cap = CLI_POOL_BYTES, .byte = '*'});
        const unsigned long long size = (star != NULL)
                                            ? (unsigned long long)(star - part)
                                            : cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_POOL_BYTES});
        if (first)
        {
            if (!EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = right, .other_pointer = part,
                            .size = size))
            {
                return 0;
            }
            at = size;
            first = 0;
        }
        else if (star == NULL)
        {
            return (length >= (at + size)) &&
                   (cellul.eq(&(CatenaFinitaCfg){.src = right + length - size, .other = part, .cap = CLI_POOL_BYTES}));
        }
        else
        {
            char piece[CLI_NAME_MAX_LEN];
            VerbaLinea line = {piece, sizeof(piece), 0ULL};
            mmgr_line_put_n(&line, part, size);
            mmgr_line_finish(&line);
            const char *const found = cellul.find(&(CatenaFinitaCfg){
                .src = right + at, .cap = CLI_POOL_BYTES, .other = piece, .other_cap = CLI_POOL_BYTES});
            if (found == NULL)
            {
                return 0;
            }
            at = (unsigned long long)(found - right) + size;
        }
        if (star == NULL)
        {
            return 1;
        }
        part = star + 1;
    }
}

// whether the text's first character is a digit
EMBED_INLINE int cli_natural_digit(const ToolchainsCtx *args)
{
    return (args->text[0] >= '0') && (args->text[0] <= '9');
}

// the end of the run that starts `text`: of digits where it starts with one, else of what is no digit
EMBED_INLINE const char *cli_natural_run(const ToolchainsCtx *args)
{
    const char *const text = args->text;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES});
    if (EMBED_CALL(cli_natural_digit, ToolchainsCtx, .text = text))
    {
        return text + EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = text, .at = 0ULL,
                                 .size = size, .classes = CLI_TEXT_DIGITS, .set = "");
    }
    return text + EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = 0ULL,
                             .size = size, .set = "0123456789");
}

// two runs of digits, `a` to `a_end` and `b` to `b_end`, ordered by the numbers they write, their leading zeros passed
// over: below 0 where `a`'s comes first, 0 where they are equal
EMBED_INLINE embed_iword cli_natural_numbers(const ToolchainsCtx *args)
{
    const char *const a =
        args->a + EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = args->a, .at = 0ULL,
                             .size = (unsigned long long)(args->a_end - args->a), .set = "0");
    const char *const b =
        args->b + EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = args->b, .at = 0ULL,
                             .size = (unsigned long long)(args->b_end - args->b), .set = "0");
    const long long a_size = args->a_end - a;
    const long long b_size = args->b_end - b;
    if (a_size != b_size)
    {
        return (a_size < b_size) ? -1 : 1;
    }
    return memor.cmp(&(MemoriaCfg){.src = a, .other = b, .bytes = (unsigned long long)a_size});
}

// two runs, `a` to `a_end` and `b` to `b_end`, ordered by their characters without case: below 0 where `a`'s comes
// first, 0 where they are equal
EMBED_INLINE embed_iword cli_natural_characters(const ToolchainsCtx *args)
{
    const char *x = args->a;
    const char *y = args->b;
    while ((x < args->a_end) || (y < args->b_end))
    {
        const char left_read = (x < args->a_end) ? x[0] : '\0';
        const char right_read = (y < args->b_end) ? y[0] : '\0';
        const char left = ((left_read >= 'A') && (left_read <= 'Z')) ? (char)(left_read + 32) : left_read;
        const char right = ((right_read >= 'A') && (right_read <= 'Z')) ? (char)(right_read + 32) : right_read;
        if (left != right)
        {
            return (left < right) ? -1 : 1;
        }
        x += 1;
        y += 1;
    }
    return 0;
}

// Orders names as their numbers read, so that Python314 comes after Python39: below 0 where `a` comes first
int cli_toolchains_natural(const CliToolchainsACtx *args)
{
    const char *a = args->a;
    const char *b = args->b;
    while ((a[0] != '\0') && (b[0] != '\0'))
    {
        const char *const a_end = EMBED_CALL(cli_natural_run, ToolchainsCtx, .text = a);
        const char *const b_end = EMBED_CALL(cli_natural_run, ToolchainsCtx, .text = b);
        const int numbers = EMBED_CALL(cli_natural_digit, ToolchainsCtx, .text = a) &&
                            EMBED_CALL(cli_natural_digit, ToolchainsCtx, .text = b);
        const embed_iword order =
            numbers ? EMBED_CALL(cli_natural_numbers, ToolchainsCtx, .a = a, .a_end = a_end, .b = b, .b_end = b_end)
                    : EMBED_CALL(cli_natural_characters, ToolchainsCtx, .a = a, .a_end = a_end, .b = b, .b_end = b_end);
        if (order != 0)
        {
            return (order < 0) ? -1 : 1;
        }
        a = a_end;
        b = b_end;
    }
    if (a[0] == b[0])
    {
        return 0;
    }
    return (a[0] == '\0') ? -1 : 1;
}

// A level's folders read: those whose names match the part, the newest first
typedef struct
{
    const char *part;
    CliToolchainsPaths *names;
} CliMatching;

static void cli_matching_each(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    CliMatching *const matching = (CliMatching *)args->context;
    if (!args->is_folder ||
        !EMBED_CALL(cli_toolchains_pattern_internal.matches, CliToolchainsPatternCtx, .pattern = matching->part,
                    .name = name) ||
        !cli_toolchains_paths_add(&(CliToolchainsPathsCfg){.paths = matching->names, .path = name}))
    {
        return;
    }
    // the name added last moves up past each name that orders before it: the newest stand first
    const unsigned int last = matching->names->count - 1u;
    unsigned int place = last;
    char before[CLI_PATH_MAX_LEN];
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = matching->names->cell,
                            .offset = (embed_word)((place > 0u) ? place - 1u : 0u) * CLI_PATH_MAX_LEN,
                            .dst = before,
                            .bytes = CLI_PATH_MAX_LEN});
    while ((place > 0u) &&
           (EMBED_CALL(cli_toolchains_a_internal.natural, CliToolchainsACtx, .a = before, .b = name) < 0))
    {
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = matching->names->cell,
                                                                 .offset = (embed_word)place * CLI_PATH_MAX_LEN,
                                                                 .src = before,
                                                                 .bytes = CLI_PATH_MAX_LEN});
        place -= 1u;
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = matching->names->cell,
                                .offset = (embed_word)((place > 0u) ? place - 1u : 0u) * CLI_PATH_MAX_LEN,
                                .dst = before,
                                .bytes = CLI_PATH_MAX_LEN});
    }
    if (place != last)
    {
        char slot[CLI_PATH_MAX_LEN];
        VerbaLinea line = {slot, sizeof(slot), 0ULL};
        mmgr_line_put(&line, name);
        (void)mmgr_line_finish(&line);
        (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){.cell = matching->names->cell,
                                                                 .offset = (embed_word)place * CLI_PATH_MAX_LEN,
                                                                 .src = slot,
                                                                 .bytes = CLI_PATH_MAX_LEN});
    }
}

// a path so far, `path`, followed by `text` and `separator`, added to `found_into`: 1, or 0 where it does not fit
// CLI_PATH_MAX_LEN bytes or the cells refuse it
EMBED_INLINE int cli_folder_joined(const ToolchainsCtx *args)
{
    char out[CLI_PATH_MAX_LEN];
    VerbaLinea joined = {out, sizeof(out), 0ULL};
    mmgr_line_put(&joined, args->path);
    mmgr_line_put(&joined, args->text);
    mmgr_line_ch(&joined, args->separator);
    return (mmgr_line_finish(&joined) != 0ULL) &&
           cli_toolchains_paths_add(&(CliToolchainsPathsCfg){.paths = args->found_into, .path = out});
}

// each path so far in `found_from` followed by the part `text` and `separator`, added to `found_into`, emptied
// first: the part as it is, or where it holds a *, each folder in the path whose name matches it, the newest
// first, `names` holding them as they are read
EMBED_INLINE void cli_folders_expanded(const ToolchainsCtx *args)
{
    const int pattern = cellul.chr(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES, .byte = '*'}) != NULL;
    args->found_into->count = 0u;
    for (unsigned int at = 0u; at < args->found_from->count; at += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = args->found_from->cell,
                                                                .offset = (embed_word)at * CLI_PATH_MAX_LEN,
                                                                .dst = path,
                                                                .bytes = CLI_PATH_MAX_LEN});
        if (!pattern)
        {
            (void)EMBED_CALL(cli_folder_joined, ToolchainsCtx, .path = path, .text = args->text,
                             .separator = args->separator, .found_into = args->found_into);
            continue;
        }
        args->names->count = 0u;
        CliMatching matching = {args->text, args->names};
        EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = path,
                   .each = cli_matching_each, .context_bytess = &matching);
        for (unsigned int each = 0u; each < args->names->count; each += 1u)
        {
            char named[CLI_PATH_MAX_LEN];
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = args->names->cell,
                                                                    .offset = (embed_word)each * CLI_PATH_MAX_LEN,
                                                                    .dst = named,
                                                                    .bytes = CLI_PATH_MAX_LEN});
            (void)EMBED_CALL(cli_folder_joined, ToolchainsCtx, .path = path, .text = named,
                             .separator = args->separator, .found_into = args->found_into);
        }
    }
}

int cli_toolchains_paths_add(const CliToolchainsPathsCfg *args)
{
    char slot[CLI_PATH_MAX_LEN];
    VerbaLinea line = {slot, sizeof(slot), 0ULL};
    mmgr_line_put(&line, args->path);
    (void)mmgr_line_finish(&line);
    if (!mmgr_line_ok(&line))
    {
        return 0;
    }
    if (args->paths->count == args->paths->capacity)
    {
        const unsigned int capacity = (args->paths->capacity != 0u) ? args->paths->capacity * 2u : 8u;
        const CarcerMovableEntry *const grown =
            g_cli_guard->movable_buf_alloc((unsigned long long)capacity * CLI_PATH_MAX_LEN, g_cli_toolchains_owner);
        if (grown == NULL)
        {
            return 0;
        }
        for (unsigned int at = 0u; at < args->paths->count; at += 1u)
        {
            char held[CLI_PATH_MAX_LEN];
            (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = args->paths->cell,
                                                                    .offset = (embed_word)at * CLI_PATH_MAX_LEN,
                                                                    .dst = held,
                                                                    .bytes = CLI_PATH_MAX_LEN});
            (void)g_cli_guard->movable_buf_write(&(CarcerMovableCfg){
                .cell = grown, .offset = (embed_word)at * CLI_PATH_MAX_LEN, .src = held, .bytes = sizeof(held)});
        }
        if (args->paths->cell != NULL)
        {
            (void)g_cli_guard->movable_buf_release(args->paths->cell, g_cli_toolchains_owner);
        }
        args->paths->cell = grown;
        args->paths->capacity = capacity;
    }
    (void)g_cli_guard->movable_buf_write(
        &(CarcerMovableCfg){.cell = args->paths->cell,
                            .offset = (embed_word)args->paths->count * CLI_PATH_MAX_LEN,
                            .src = slot,
                            .bytes = CLI_PATH_MAX_LEN});
    args->paths->count += 1u;
    return 1;
}

void cli_toolchains_paths_release(const CliToolchainsPathsCfg *args)
{
    if (args->paths->cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(args->paths->cell, g_cli_toolchains_owner);
    }
    *args->paths = (CliToolchainsPaths){NULL, 0u, 0u};
}

unsigned int cli_toolchains_folders_get(const CliToolchainsCountCfg *args)
{
    CliToolchainsPaths *const paths = args->paths;
    char full[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_expand, ToolchainsCtx, .text = args->pattern, .out = full))
    {
        return paths->count;
    }
    // the paths found so far and the paths the next part makes of them, in turn, and a level's names as they are
    // read, each in a cell of this call grown as the parts expand
    CliToolchainsPaths found[2] = {{NULL, 0u, 0u}, {NULL, 0u, 0u}};
    CliToolchainsPaths names = {NULL, 0u, 0u};
    unsigned int now = 0u;
    (void)cli_toolchains_paths_add(&(CliToolchainsPathsCfg){.paths = &found[0], .path = ""});
    // each part of the path in turn, a part with a * read from each folder found so far
    const char *part = full;
    while (part[0] != '\0')
    {
        const char *end = part;
        end += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = end, .at = 0ULL,
                          .size = cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_POOL_BYTES}), .set = "/\\");
        char piece[CLI_NAME_MAX_LEN];
        VerbaLinea line = {piece, sizeof(piece), 0ULL};
        mmgr_line_put_n(&line, part, (unsigned long long)(end - part));
        mmgr_line_finish(&line);
        EMBED_CALL(cli_folders_expanded, ToolchainsCtx, .text = piece, .separator = (end[0] != '\0') ? end[0] : '/',
                   .found_from = &found[now], .found_into = &found[1u - now], .names = &names);
        now = 1u - now;
        part = (end[0] != '\0') ? (end + 1) : end;
    }
    for (unsigned int at = 0u; at < found[now].count; at += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        (void)g_cli_guard->movable_buf_read(&(CarcerMovableCfg){.cell = found[now].cell,
                                                                .offset = (embed_word)at * CLI_PATH_MAX_LEN,
                                                                .dst = path,
                                                                .bytes = CLI_PATH_MAX_LEN});
        // the separator after the last part left off
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_POOL_BYTES});
        if ((length > 1u) && ((path[length - 1u] == '/') || (path[length - 1u] == '\\')))
        {
            path[length - 1u] = '\0';
        }
        if (EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = path))
        {
            (void)cli_toolchains_paths_add(&(CliToolchainsPathsCfg){.paths = paths, .path = path});
        }
    }
    cli_toolchains_paths_release(&(CliToolchainsPathsCfg){.paths = &found[0]});
    cli_toolchains_paths_release(&(CliToolchainsPathsCfg){.paths = &found[1]});
    cli_toolchains_paths_release(&(CliToolchainsPathsCfg){.paths = &names});
    return paths->count;
}

void cli_toolchains_key_get(const CliToolchainsDirKeyCfg *args)
{
    memor.cpy(&(MemoriaCfg){.dst = args->key,
                            .src = args->dir,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->dir, .cap = CLI_POOL_BYTES}) + 1u});
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_POOL_BYTES});
    length = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->key, .at = length,
                        .set = "\\/");
    args->key[length] = '\0';
#if defined(_WIN32)
    unsigned int characters[CLI_PATH_MAX_LEN];
    const unsigned long long count =
        EMBED_CALL(cli_regexp_text.characters, CliRegexpTextCfg, .text = args->key, .size = length,
                   .characters = characters, .capacity_u64 = CLI_PATH_MAX_LEN);
    VerbaLinea line = {args->key, CLI_PATH_MAX_LEN, 0ULL};
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        EMBED_CALL(cli_line_character_internal.utf8_append, CliLineLetterCtx, .line = &line,
                   .character =
                       EMBED_CALL(cli_regexp_character.lower, CliRegexpLetterCfg, .character = characters[at]));
    }
    mmgr_line_finish(&line);
#endif
}
