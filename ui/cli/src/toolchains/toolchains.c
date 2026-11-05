// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// toolchains.c: the manifest read, orior's own and the reader's, a tool's program found in a folder, and the folders a
// pattern of toolchains.json names
#include "../command_line/callback_cfg.h"
#include "../command_line/cells.h"
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
    char (*found_from)[CLI_PATH_MAX_LEN];
    char (*found_into)[CLI_PATH_MAX_LEN];
    unsigned int found;
    char (*names)[CLI_NAME_MAX_LEN];
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
           (cellul.eq(&(CatenaFinitaCfg){.src = only, .other = cli_toolchains_system(), .cap = CLI_CELLS_POOL}));
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
        VerbaLinea line = {path + cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}),
                           CLI_PATH_MAX_LEN - cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), 0ULL};
        mmgr_line_put(&line, "/user_toolchains.json");
        added_text = (mmgr_line_finish(&line) != 0ULL)
                         ? EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                      .size_pointer = &added_size, .owner = &g_cli_toolchains_cells)
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
    CfgJsonToken *const tokens = g_cli_cells->temporary_buf_alloc((unsigned long long)tokens_max * sizeof(CfgJsonToken),
                                                                  &g_cli_toolchains_cells);
    const int reserved =
        (tokens != NULL) &&
        EMBED_CALL(cli_json_tree_nodes_max.json_reserve, CliJsonTreeNodesMaxCfg, .json = args->json_out,
                   .nodes_max = bundled_tokens + added_tokens + 1u + args->nodes_more,
                   .text_max = g_cli_toolchains_json_size + added_size + 1ULL + args->text_more,
                   .what = "the toolchains' manifest", .owner = &g_cli_toolchains_cells);
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
        (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json_out, .node = tools,
                    .kind = CFG_JSON_ARRAY) &&
         (added_text != NULL))
            ? EMBED_CALL(cli_json_tree_text_size_tokens.json_read, CliJsonTreeTextSizeTokensCfg, .json = args->json_out,
                         .text = (const char *)added_text, .size = added_size, .tokens = tokens,
                         .tokens_max = tokens_max, .err_msg = &err_msg)
            : CLI_JSON_NONE;
    g_cli_cells->persistent_buf_release(tokens, &g_cli_toolchains_cells);
    g_cli_cells->persistent_buf_release((void *)(uintptr_t)added_text, (&g_cli_toolchains_cells));
    if (!EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = args->json_out, .node = tools,
                    .kind = CFG_JSON_ARRAY))
    {
        return CLI_JSON_NONE;
    }
    const unsigned int theirs = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                           .json_read = args->json_out, .object = added, .name = "tools");
    for (unsigned int tool = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                        .json_read = args->json_out, .node = theirs, .kind = CFG_JSON_ARRAY)
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
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = args->json_out, .kind = CFG_JSON_ARRAY);
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
    for (unsigned int tool = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                        .json_read = args->json_out, .node = args->manifest, .kind = CFG_JSON_ARRAY)
                                 ? args->json_out->node[args->manifest].first
                                 : CLI_JSON_NONE;
         tool != CLI_JSON_NONE; tool = args->json_out->node[tool].next)
    {
        const char *const named =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json_out,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = args->json_out, .object = tool, .name = "id"),
                       .length_pointer = NULL);
        if ((named != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = named, .other = args->id, .cap = CLI_CELLS_POOL})))
        {
            return tool;
        }
    }
    mmgr_line_put(args->err_msg, "orior_cli knows no toolchain ");
    mmgr_line_put(args->err_msg, args->id);
    mmgr_line_put(args->err_msg, ": orior_cli file toolchains lists them");
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
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg,
                                        .json_read = args->json, .node = programs, .kind = CFG_JSON_ARRAY)
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
                           .size = cellul.len(&(CatenaFinitaCfg){.src = mark, .cap = CLI_CELLS_POOL}), .set = "%$");
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
                              .size = cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_CELLS_POOL}), .set = "%");
            if (end[0] == '\0')
            {
                return 0;
            }
        }
        else
        {
            end += EMBED_CALL(cli_text_at_size_classes.end_class, CliTextAtSizeClassesCfg, .text = end, .at = 0ULL,
                              .size = cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_CELLS_POOL}),
                              .classes = CLI_TEXT_LETTERS | CLI_TEXT_DIGITS, .set = "_");
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

// whether `name` matches `pattern`, whose * matches any run of letters, case aside on Windows
int cli_toolchains_matches(const CliToolchainsPatternCtx *args)
{
    char left[CLI_NAME_MAX_LEN];
    char right[CLI_NAME_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = left,
                            .src = args->pattern,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->pattern, .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){.dst = right,
                            .src = args->name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) + 1u});
#if defined(_WIN32)
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .text_out = left,
               .size = cellul.len(&(CatenaFinitaCfg){.src = left, .cap = CLI_CELLS_POOL}));
    EMBED_CALL(cli_text_size_from.lower, CliTextSizeFromCfg, .text_out = right,
               .size = cellul.len(&(CatenaFinitaCfg){.src = right, .cap = CLI_CELLS_POOL}));
#endif
    if (cellul.chr(&(CatenaFinitaCfg){.src = left, .cap = CLI_CELLS_POOL, .byte = '*'}) == NULL)
    {
        return cellul.eq(&(CatenaFinitaCfg){.src = left, .other = right, .cap = CLI_CELLS_POOL});
    }
    unsigned long long at = 0ULL;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = right, .cap = CLI_CELLS_POOL});
    const char *part = left;
    int first = 1;
    for (;;)
    {
        const char *const star = cellul.chr(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL, .byte = '*'});
        const unsigned long long size = (star != NULL)
                                            ? (unsigned long long)(star - part)
                                            : cellul.len(&(CatenaFinitaCfg){.src = part, .cap = CLI_CELLS_POOL});
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
                   (cellul.eq(&(CatenaFinitaCfg){.src = right + length - size, .other = part, .cap = CLI_CELLS_POOL}));
        }
        else
        {
            char piece[CLI_NAME_MAX_LEN];
            VerbaLinea line = {piece, sizeof(piece), 0ULL};
            mmgr_line_put_n(&line, part, size);
            mmgr_line_finish(&line);
            const char *const found = cellul.find(&(CatenaFinitaCfg){
                .src = right + at, .cap = CLI_CELLS_POOL, .other = piece, .other_cap = CLI_CELLS_POOL});
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

// whether the text's first letter is a digit
EMBED_INLINE int cli_natural_digit(const ToolchainsCtx *args)
{
    return (args->text[0] >= '0') && (args->text[0] <= '9');
}

// the end of the run that starts `text`: of digits where it starts with one, else of what is no digit
EMBED_INLINE const char *cli_natural_run(const ToolchainsCtx *args)
{
    const char *const text = args->text;
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
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

// two runs, `a` to `a_end` and `b` to `b_end`, ordered by their letters without case: below 0 where `a`'s comes first,
// 0 where they are equal
EMBED_INLINE embed_iword cli_natural_letters(const ToolchainsCtx *args)
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
                    : EMBED_CALL(cli_natural_letters, ToolchainsCtx, .a = a, .a_end = a_end, .b = b, .b_end = b_end);
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
    char (*names)[CLI_NAME_MAX_LEN];
    unsigned int count;
    unsigned int capacity;
} CliMatching;

static void cli_matching_each(const CliEachNameIsFolderCfg *args)
{
    const char *name = args->name;
    CliMatching *const matching = (CliMatching *)args->context;
    if (!args->is_folder || (matching->count >= matching->capacity) ||
        !EMBED_CALL(cli_toolchains_pattern_internal.matches, CliToolchainsPatternCtx, .pattern = matching->part,
                    .name = name))
    {
        return;
    }
    unsigned int place = matching->count;
    while ((place > 0u) && (EMBED_CALL(cli_toolchains_a_internal.natural, CliToolchainsACtx,
                                       .a = matching->names[place - 1u], .b = name) < 0))
    {
        memor.cpy(&(MemoriaCfg){
            .dst = matching->names[place],
            .src = matching->names[place - 1u],
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = matching->names[place - 1u], .cap = CLI_CELLS_POOL}) + 1u});
        place -= 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = matching->names[place],
                            .src = name,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1u});
    matching->count += 1u;
}

// a path so far, `path`, followed by `name` and `separator`, written into `out`, CLI_PATH_MAX_LEN bytes: 1, or 0 where
// it does not fit
EMBED_INLINE int cli_folder_joined(const ToolchainsCtx *args)
{
    VerbaLinea joined = {args->out, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&joined, args->path);
    mmgr_line_put(&joined, args->text);
    mmgr_line_ch(&joined, args->separator);
    return mmgr_line_finish(&joined) != 0ULL;
}

// each of the `found` paths so far in `found_from` followed by the part `text` and `separator`, written into
// `found_into`: the part as it is, or where it holds a *, each folder in the path whose name matches it, the newest
// first, `names` holding them as they are read; how many paths are written, 64 at the most
EMBED_INLINE unsigned int cli_folders_expanded(const ToolchainsCtx *args)
{
    const int pattern = cellul.chr(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL, .byte = '*'}) != NULL;
    unsigned int next = 0u;
    for (unsigned int at = 0u; at < args->found; at += 1u)
    {
        if (!pattern)
        {
            next += ((next < 64u) &&
                     EMBED_CALL(cli_folder_joined, ToolchainsCtx, .path = args->found_from[at], .text = args->text,
                                .separator = args->separator, .out = args->found_into[next]))
                        ? 1u
                        : 0u;
            continue;
        }
        CliMatching matching = {args->text, args->names, 0u, 64u};
        EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->found_from[at],
                   .each = cli_matching_each, .context_bytess = &matching);
        for (unsigned int each = 0u; (each < matching.count) && (next < 64u); each += 1u)
        {
            next += EMBED_CALL(cli_folder_joined, ToolchainsCtx, .path = args->found_from[at],
                               .text = args->names[each], .separator = args->separator, .out = args->found_into[next])
                        ? 1u
                        : 0u;
        }
    }
    return next;
}

unsigned int cli_toolchains_folders_get(const CliToolchainsCountCfg *args)
{
    unsigned int count = args->count;
    static char s_found[2][64][CLI_PATH_MAX_LEN];
    static char s_names[64][CLI_NAME_MAX_LEN];
    char full[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_expand, ToolchainsCtx, .text = args->pattern, .out = full))
    {
        return count;
    }
    unsigned int now = 0u;
    unsigned int found = 1u;
    s_found[now][0][0] = '\0';
    // each part of the path in turn, a part with a * read from each folder found so far
    const char *part = full;
    while (part[0] != '\0')
    {
        const char *end = part;
        end += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = end, .at = 0ULL,
                          .size = cellul.len(&(CatenaFinitaCfg){.src = end, .cap = CLI_CELLS_POOL}), .set = "/\\");
        char piece[CLI_NAME_MAX_LEN];
        VerbaLinea line = {piece, sizeof(piece), 0ULL};
        mmgr_line_put_n(&line, part, (unsigned long long)(end - part));
        mmgr_line_finish(&line);
        found =
            EMBED_CALL(cli_folders_expanded, ToolchainsCtx, .text = piece, .separator = (end[0] != '\0') ? end[0] : '/',
                       .found_from = s_found[now], .found = found, .found_into = s_found[1u - now], .names = s_names);
        now = 1u - now;
        part = (end[0] != '\0') ? (end + 1) : end;
    }
    for (unsigned int at = 0u; (at < found) && (count < args->capacity); at += 1u)
    {
        // the separator after the last part left off
        const unsigned long long length =
            cellul.len(&(CatenaFinitaCfg){.src = s_found[now][at], .cap = CLI_CELLS_POOL});
        if ((length > 1u) && ((s_found[now][at][length - 1u] == '/') || (s_found[now][at][length - 1u] == '\\')))
        {
            s_found[now][at][length - 1u] = '\0';
        }
        if (EMBED_CALL(query_path_seconds_internal.is_folder, QueryPathSecondsCtx, .path = s_found[now][at]))
        {
            memor.cpy(&(MemoriaCfg){
                .dst = args->folders[count],
                .src = s_found[now][at],
                .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_found[now][at], .cap = CLI_CELLS_POOL}) + 1u});
            count += 1u;
        }
    }
    return count;
}

void cli_toolchains_key_get(const CliToolchainsDirKeyCfg *args)
{
    memor.cpy(&(MemoriaCfg){.dst = args->key,
                            .src = args->dir,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->dir, .cap = CLI_CELLS_POOL}) + 1u});
    unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->key, .cap = CLI_CELLS_POOL});
    length = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->key, .at = length,
                        .set = "\\/");
    args->key[length] = '\0';
#if defined(_WIN32)
    unsigned int letters[CLI_PATH_MAX_LEN];
    const unsigned long long count = EMBED_CALL(cli_regexp_text.letters, CliRegexpTextCfg, .text = args->key,
                                                .size = length, .letters = letters, .capacity_u64 = CLI_PATH_MAX_LEN);
    VerbaLinea line = {args->key, CLI_PATH_MAX_LEN, 0ULL};
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        EMBED_CALL(cli_line_letter_internal.utf8_append, CliLineLetterCtx, .line = &line,
                   .letter = EMBED_CALL(cli_regexp_letter.lower, CliRegexpLetterCfg, .letter = letters[at]));
    }
    mmgr_line_finish(&line);
#endif
}
