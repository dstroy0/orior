// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_find.c: the adapter a language is debugged by, the first toolchain found of those that have one for it, and a
// program built for debugging: by the CMake or Meson build that makes it, or by the `builds` of the toolchain that
// builds its language, into build/debug/
#include "../command_line/cells.h"
#include "../programs/executables.h"
#include "../toolchains/toolchains.h"
#include "debug_internal.h"

#include "../text/text.h"

// the most toolchains one search names, and the most lines of a failed build reported
#define CLI_DEBUG_NAMED 64u
#define CLI_DEBUG_TAIL 12u

// The PATH's folders, a cell of a search's, a tool found, and the lock they and a search's toolchains are guarded by
static char (*s_folders)[CLI_PATH_MAX_LEN];
static CliFound s_found;

typedef struct
{
    CliJson *manifest;
    const CliJson *json;
    unsigned int array;
    const char *text;
    const unsigned int *named;
    unsigned int count;
    VerbaLinea *out;
    unsigned int *named_out;
    unsigned int *words;
    const char *ahead;
    unsigned long long size;
    const char *path;
    char *out_capacity;
    char *stem;
    unsigned int named_value;
    unsigned int words_node;
    const char *out_text;
    const char *program;
    CliWords *words_words_pointer;
    VerbaLinea *err_msg;
} DebugFindCtx;
// a search's toolchains and the PATH's folders given back, and the lock
EMBED_INLINE void find_release(const DebugFindCtx *args)
{
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = args->manifest);
    g_cli_cells->persistent_buf_release(s_folders, &g_cli_debug_cells);
    s_folders = NULL;
}

// whether the array `array` of `json` contains the string `text`
EMBED_INLINE int find_contains(const DebugFindCtx *args)
{
    const CliJson *json = args->json;
    unsigned int array = args->array;
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = array, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        const char *const one =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = array, .index = at),
                       .length_pointer = NULL);
        if ((one != NULL) && (cellul.eq(&(CatenaFinitaCfg){.src = one, .other = args->text, .cap = CLI_CELLS_POOL})))
        {
            return 1;
        }
    }
    return 0;
}

// the names of the toolchains `named`, `count` of them, joined by " or " into `out`
EMBED_INLINE void find_names_join(const DebugFindCtx *args)
{
    const CliJson *json = args->json;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        mmgr_line_put(args->out, (at > 0u) ? " or " : "");
        mmgr_line_put(args->out,
                      EMBED_CALL(cli_debug_json_node_fallback_internal.text, CliDebugJsonNodeFallbackCtx, .json = json,
                                 .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                    .json_read = json, .object = args->named[at], .name = "name"),
                                 .fallback = ""));
    }
}

// the toolchains `named`, `count` of them, with the one of id `ahead` put first, the others in their order
EMBED_INLINE void find_ahead_first(const DebugFindCtx *args)
{
    const CliJson *json = args->json;
    unsigned int first = 0u;
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        if (!cellul.eq(&(CatenaFinitaCfg){
                .src = EMBED_CALL(cli_debug_json_node_fallback_internal.text, CliDebugJsonNodeFallbackCtx, .json = json,
                                  .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                     .json_read = json, .object = args->named_out[at], .name = "id"),
                                  .fallback = ""),
                .other = args->ahead,
                .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        const unsigned int tool = args->named_out[at];
        const unsigned int word_text = args->words[at];
        for (unsigned int moved = at; moved > first; moved -= 1u)
        {
            args->named_out[moved] = args->named_out[moved - 1u];
            args->words[moved] = args->words[moved - 1u];
        }
        args->named_out[first] = tool;
        args->words[first] = word_text;
        first += 1u;
    }
}

int cli_debug_adapter(const CliDebugErrMsgCtx *args)
{
    CliJson *json = args->json_out;
    unsigned int *spec = args->spec;
    unsigned int named[CLI_DEBUG_NAMED];
    unsigned int specs[CLI_DEBUG_NAMED];
    unsigned int count = 0u;
    CliJson manifest_json;
    const unsigned int manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg,
                                             .json_out = &manifest_json, .nodes_more = 0u, .text_more = 0ULL);
    const char *const system = cli_toolchains_system();
    for (unsigned int at = 0u;
         (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &manifest_json,
                     .array = manifest, .index = at) != CLI_JSON_NONE) &&
         (count < CLI_DEBUG_NAMED);
         at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = &manifest_json, .array = manifest, .index = at);
        const unsigned int debugger = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                 .json_read = &manifest_json, .object = one, .name = "debugger");
        const unsigned int systems = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = &manifest_json, .object = debugger, .name = "systems");
        const int on_system =
            (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &manifest_json,
                        .array = systems, .index = 0u) == CLI_JSON_NONE) ||
            EMBED_CALL(find_contains, DebugFindCtx, .json = &manifest_json, .array = systems, .text = system);
        if (EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &manifest_json,
                       .node = debugger, .kind = CFG_JSON_OBJECT) &&
            EMBED_CALL(find_contains, DebugFindCtx, .json = &manifest_json,
                       .array = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                           .json_read = &manifest_json, .object = debugger, .name = "languages"),
                       .text = args->language) &&
            on_system)
        {
            named[count] = one;
            specs[count] = debugger;
            count += 1u;
        }
    }
    if (count == 0u)
    {
        EMBED_CALL(find_release, DebugFindCtx, .manifest = &manifest_json);
        mmgr_line_put(args->err_msg, "orior_cli has no debugger for ");
        mmgr_line_put(args->err_msg, args->language);
        return 0;
    }
    EMBED_CALL(find_ahead_first, DebugFindCtx, .json = &manifest_json, .named_out = named, .words = specs,
               .count = count, .ahead = args->ahead);
    unsigned int folders = 0u;
    s_folders = cli_toolchains_path_allocated(&folders, &g_cli_debug_cells);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(cli_toolchains_json.find, CliToolchainsJsonCfg, .json = &manifest_json, .tool = named[at],
                   .path_callback = (const char (*)[CLI_PATH_MAX_LEN])s_folders, .count = folders, .found = &s_found);
        const char *const names[1] = {
            EMBED_CALL(cli_debug_json_node_fallback_internal.text, CliDebugJsonNodeFallbackCtx, .json = &manifest_json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                          .json_read = &manifest_json, .object = specs[at], .name = "program"),
                       .fallback = "")};
        if ((s_found.folder[0] != '\0') &&
            EMBED_CALL(cli_toolchains_dir.program_in, CliToolchainsDirCfg, .dir = s_found.folder, .names = names,
                       .count = 1u, .path = args->program))
        {
            *spec = EMBED_CALL(cli_json_tree_node.json_copy_from, CliJsonTreeNodeCfg, .to = json,
                               .from = &manifest_json, .node = specs[at]);
            memor.cpy(&(MemoriaCfg){
                .dst = args->tool,
                .src = EMBED_CALL(cli_debug_json_node_fallback_internal.text, CliDebugJsonNodeFallbackCtx,
                                  .json = &manifest_json,
                                  .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                     .json_read = &manifest_json, .object = named[at], .name = "name"),
                                  .fallback = ""),
                .bytes = cellul.len(&(CatenaFinitaCfg){
                             .src = EMBED_CALL(cli_debug_json_node_fallback_internal.text, CliDebugJsonNodeFallbackCtx,
                                               .json = &manifest_json,
                                               .node = EMBED_CALL(cli_json_tree_object.json_member,
                                                                  CliJsonTreeObjectCfg, .json_read = &manifest_json,
                                                                  .object = named[at], .name = "name"),
                                               .fallback = ""),
                             .cap = CLI_CELLS_POOL}) +
                         1u});
            memor.cpy(&(MemoriaCfg){
                .dst = args->tool_id,
                .src = EMBED_CALL(cli_debug_json_node_fallback_internal.text, CliDebugJsonNodeFallbackCtx,
                                  .json = &manifest_json,
                                  .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                     .json_read = &manifest_json, .object = named[at], .name = "id"),
                                  .fallback = ""),
                .bytes = cellul.len(&(CatenaFinitaCfg){
                             .src = EMBED_CALL(cli_debug_json_node_fallback_internal.text, CliDebugJsonNodeFallbackCtx,
                                               .json = &manifest_json,
                                               .node = EMBED_CALL(cli_json_tree_object.json_member,
                                                                  CliJsonTreeObjectCfg, .json_read = &manifest_json,
                                                                  .object = named[at], .name = "id"),
                                               .fallback = ""),
                             .cap = CLI_CELLS_POOL}) +
                         1u});
            EMBED_CALL(find_release, DebugFindCtx, .manifest = &manifest_json);
            return 1;
        }
    }
    EMBED_CALL(find_names_join, DebugFindCtx, .json = &manifest_json, .named = named, .count = count,
               .out = args->err_msg);
    EMBED_CALL(find_release, DebugFindCtx, .manifest = &manifest_json);
    mmgr_line_put(args->err_msg,
                  " debugs it, and is not found: File, Toolchains opens its install page or takes its folder");
    return 0;
}

// the last lines of what a build wrote that are not blank, CLI_DEBUG_TAIL of them at most, joined by "\n" into `out`
EMBED_INLINE void find_tail(const DebugFindCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    VerbaLinea *out = args->out;
    unsigned long long starts[CLI_DEBUG_TAIL];
    unsigned long long ends[CLI_DEBUG_TAIL];
    unsigned int count = 0u;
    for (unsigned long long start = 0ULL; start < size;)
    {
        const CliTextLine bounds =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = start, .size = size);
        const unsigned long long next = bounds.next;
        unsigned long long end = bounds.stop;
        end = ((end > start) && (text[end - 1ULL] == '\r')) ? end - 1ULL : end;
        int blank = 1;
        {
            unsigned long long at = start;
            while (blank && (at < end))
            {
                blank = (cellul.chr(&(CatenaFinitaCfg){.src = " \t\f\v\r", .cap = CLI_CELLS_POOL, .byte = text[at]}) !=
                         NULL);
                at += 1ULL;
            }
        }
        if (!blank)
        {
            const unsigned int place = (count < CLI_DEBUG_TAIL) ? count : CLI_DEBUG_TAIL - 1u;
            for (unsigned int moved = 0u; (count >= CLI_DEBUG_TAIL) && (moved + 1u < CLI_DEBUG_TAIL); moved += 1u)
            {
                starts[moved] = starts[moved + 1u];
                ends[moved] = ends[moved + 1u];
            }
            starts[place] = start;
            ends[place] = end;
            count = (count < CLI_DEBUG_TAIL) ? count + 1u : count;
        }
        start = next;
    }
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(out, (at > 0u) ? "\n" : "");
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = out,
                   .text_bytes = (const unsigned char *)text + starts[at], .length = ends[at] - starts[at]);
    }
}

int cli_debug_build_program(const CliDebugErrMsgProgramCtx *args)
{
    char tool[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = args->made->tool,
                    .name = args->made->tool, .path = tool, .err_msg = args->err_msg))
    {
        return 0;
    }
    char path[CLI_PATH_TEXT];
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = path, .size = sizeof(path));
    const CliSetting settings[1] = {{"PATH", path}};
    const int meson = (cellul.eq(&(CatenaFinitaCfg){.src = args->made->tool, .other = "meson", .cap = CLI_CELLS_POOL}));
    const char *const meson_words[6] = {tool, "compile", "-C", args->made->build, args->made->target, NULL};
    const char *const cmake_words[9] = {
        tool, "--build", args->made->build, "--target", args->made->target, "--config", args->made->config, NULL, NULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_buf = {why, sizeof(why), 0ULL};
    CliRunResult *const run_result = cli_run_result_alloc();
    if (!EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = meson ? meson_words : cmake_words, .folder = NULL,
                    .settings = settings, .count = 1u, .run_result = run_result, .err_msg = &err_buf))
    {
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        mmgr_line_put(args->err_msg, tool);
        mmgr_line_put(args->err_msg, ": ");
        mmgr_line_put_n(args->err_msg, err_buf.out, err_buf.at);
        return 0;
    }
    if (run_result->code != 0LL)
    {
        // what the build wrote to its output, then to its errors, as one text, a cell of their size
        char *const both =
            g_cli_cells->temporary_buf_alloc(run_result->out_size + run_result->err_size + 1ULL, &g_cli_debug_cells);
        mmgr_line_put(args->err_msg, "The build of ");
        mmgr_line_put(args->err_msg, args->made->target);
        mmgr_line_put(args->err_msg, " failed");
        if (both != NULL)
        {
            memor.cpy(&(MemoriaCfg){.dst = both, .src = run_result->out, .bytes = run_result->out_size});
            memor.cpy(&(MemoriaCfg){
                .dst = both + run_result->out_size, .src = run_result->err, .bytes = run_result->err_size});
            mmgr_line_put(args->err_msg, ":\n");
            EMBED_CALL(find_tail, DebugFindCtx, .text = both, .size = run_result->out_size + run_result->err_size,
                       .out = args->err_msg);
        }
        g_cli_cells->persistent_buf_release(both, &g_cli_debug_cells);
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        return 0;
    }
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    memor.cpy(
        &(MemoriaCfg){.dst = args->program,
                      .src = args->made->program,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->made->program, .cap = CLI_CELLS_POOL}) + 1u});
    return 1;
}

// the extension of the file `path` names, lowercase, into `out`, sized for CLI_NAME_MAX_LEN, empty where it has
// none; and its name without its extension into `stem`, sized for CLI_PATH_MAX_LEN
EMBED_INLINE void find_name_parts(const DebugFindCtx *args)
{
    char *out = args->out_capacity;
    const char *name = args->path;
    const unsigned long long name_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}), .set = "/\\");
    name = (name_end != 0ULL) ? (args->path + name_end) : name;
    const char *const dot =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = name,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), .byte = '.');
    const int has = (dot != NULL) && (dot != name);
    VerbaLinea extension = {out, CLI_NAME_MAX_LEN, 0ULL};
    EMBED_CALL(cli_text_out_name.append_lower, CliTextOutNameCfg, .out = &extension, .text = has ? (dot + 1) : "");
    mmgr_line_finish(&extension);
    VerbaLinea stem_line = {args->stem, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put_n(&stem_line, name,
                    has ? (unsigned long long)(dot - name)
                        : cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}));
    mmgr_line_finish(&stem_line);
}

// the words of the build of the toolchain `named` found, at `program`, filled in: the file's path, the program it
// makes and the toolchain's program, and each other "{name}" the toolchain's program of that name in its folder, put
// into `words`: 1, or 0 with why given
EMBED_INLINE int find_build_words(const DebugFindCtx *args)
{
    const CliJson *json = args->json;
    const char *path = args->path;
    const char *out = args->out_text;
    CliWords *words = args->words_words_pointer;
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->program, .out = folder))
    {
        folder[0] = '\0';
    }
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = args->words_node, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        unsigned long long length = 0ULL;
        const char *const word_text =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = json, .array = args->words_node, .index = at),
                       .length_pointer = &length);
        const char *const text = (word_text != NULL) ? word_text : "";
        if (cellul.eq(&(CatenaFinitaCfg){.src = text, .other = "{path}", .cap = CLI_CELLS_POOL}))
        {
            EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                       .word_text = path);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = text, .other = "{out}", .cap = CLI_CELLS_POOL}))
        {
            EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                       .word_text = out);
        }
        else if (cellul.eq(&(CatenaFinitaCfg){.src = text, .other = "{program}", .cap = CLI_CELLS_POOL}))
        {
            EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                       .word_text = args->program);
        }
        else if ((length >= 2ULL) && (text[0] == '{') && (text[length - 1ULL] == '}'))
        {
            char name[CLI_NAME_MAX_LEN];
            char found[CLI_PATH_MAX_LEN];
            VerbaLinea name_line = {name, sizeof(name), 0ULL};
            mmgr_line_put_n(&name_line, text + 1, length - 2ULL);
            mmgr_line_finish(&name_line);
            const char *const names[1] = {name};
            if (!EMBED_CALL(cli_toolchains_dir.program_in, CliToolchainsDirCfg, .dir = folder, .names = names,
                            .count = 1u, .path = found))
            {
                mmgr_line_put(
                    args->err_msg,
                    EMBED_CALL(cli_debug_json_node_fallback_internal.text, CliDebugJsonNodeFallbackCtx, .json = json,
                               .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                  .json_read = json, .object = args->named_value, .name = "name"),
                               .fallback = ""));
                mmgr_line_put(args->err_msg, " has no ");
                mmgr_line_put(args->err_msg, name);
                mmgr_line_put(args->err_msg, " in ");
                mmgr_line_put(args->err_msg, folder);
                return 0;
            }
            EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                       .word_text = found);
        }
        else
        {
            EMBED_CALL(cli_words_words_internal.words_append, CliWordsWordsCtx, .words_words_pointer = words,
                       .word_text = text);
        }
    }
    return 1;
}

int cli_debug_build(const CliDebugErrMsgLanguageCtx *args)
{
    const char *path = args->path;
    int *built = args->built;
    char extension[CLI_NAME_MAX_LEN];
    char stem[CLI_PATH_MAX_LEN];
    char keys[2][CLI_NAME_MAX_LEN];
    EMBED_CALL(find_name_parts, DebugFindCtx, .path = path, .out_capacity = extension, .stem = stem);
    VerbaLinea key = {keys[0], sizeof(keys[0]), 0ULL};
    mmgr_line_put(&key, args->language);
    mmgr_line_ch(&key, '.');
    mmgr_line_put(&key, extension);
    mmgr_line_finish(&key);
    memor.cpy(
        &(MemoriaCfg){.dst = keys[1],
                      .src = args->language,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->language, .cap = CLI_CELLS_POOL}) + 1u});
    unsigned int named[CLI_DEBUG_NAMED];
    unsigned int words_of[CLI_DEBUG_NAMED];
    unsigned int count = 0u;
    *built = 0;
    CliJson json;
    const unsigned int manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg,
                                             .json_out = &json, .nodes_more = 0u, .text_more = 0ULL);
    for (unsigned int one = 0u; one < 2u; one += 1u)
    {
        for (unsigned int at = 0u; (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                               .json_read = &json, .array = manifest, .index = at) != CLI_JSON_NONE) &&
                                   (count < CLI_DEBUG_NAMED);
             at += 1u)
        {
            const unsigned int tool = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                 .json_read = &json, .array = manifest, .index = at);
            const unsigned int words =
                EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                           .object = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = &json, .object = tool, .name = "builds"),
                           .name = keys[one]);
            if (words != CLI_JSON_NONE)
            {
                named[count] = tool;
                words_of[count] = words;
                count += 1u;
            }
        }
    }
    if (count == 0u)
    {
        EMBED_CALL(find_release, DebugFindCtx, .manifest = &json);
        return 1;
    }
    EMBED_CALL(find_ahead_first, DebugFindCtx, .json = &json, .named_out = named, .words = words_of, .count = count,
               .ahead = args->ahead);
    unsigned int folders = 0u;
    s_folders = cli_toolchains_path_allocated(&folders, &g_cli_debug_cells);
    unsigned int chosen = count;
    char compiler[CLI_PATH_MAX_LEN];
    {
        unsigned int at = 0u;
        while ((at < count) && (chosen == count))
        {
            EMBED_CALL(cli_toolchains_json.find, CliToolchainsJsonCfg, .json = &json, .tool = named[at],
                       .path_callback = (const char (*)[CLI_PATH_MAX_LEN])s_folders, .count = folders,
                       .found = &s_found);
            if (s_found.has_program)
            {
                chosen = at;
                memor.cpy(&(MemoriaCfg){
                    .dst = compiler,
                    .src = s_found.program,
                    .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_found.program, .cap = CLI_CELLS_POOL}) + 1u});
            }
            at += 1u;
        }
    }
    if (chosen == count)
    {
        EMBED_CALL(find_names_join, DebugFindCtx, .json = &json, .named = named, .count = count, .out = args->err_msg);
        EMBED_CALL(find_release, DebugFindCtx, .manifest = &json);
        mmgr_line_put(args->err_msg,
                      " builds it, and is not found: File, Toolchains opens its install page or takes its folder");
        return 0;
    }
    char out_folder[CLI_PATH_MAX_LEN];
    char build_folder[CLI_PATH_MAX_LEN];
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = "build", .joined = build_folder);
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = build_folder, .relative = "debug", .joined = out_folder);
    if (!EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = out_folder))
    {
        EMBED_CALL(find_release, DebugFindCtx, .manifest = &json);
        mmgr_line_put(args->err_msg, "build/debug: the folder could not be made");
        return 0;
    }
    char out[CLI_PATH_MAX_LEN];
#if defined(_WIN32)
    // the stem's own extension, where it has one, gives way to .exe, as a program's name is set on Windows
    char *const dot =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = stem,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = stem, .cap = CLI_CELLS_POOL}), .byte = '.');
    if ((dot != NULL) && (dot != stem))
    {
        *dot = '\0';
    }
    VerbaLinea exe = {stem + cellul.len(&(CatenaFinitaCfg){.src = stem, .cap = CLI_CELLS_POOL}),
                      CLI_PATH_MAX_LEN - cellul.len(&(CatenaFinitaCfg){.src = stem, .cap = CLI_CELLS_POOL}), 0ULL};
    mmgr_line_put(&exe, ".exe");
    mmgr_line_finish(&exe);
#endif
    EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = out_folder, .relative = stem, .joined = out);
    static CliWords s_words;
    s_words.count = 0u;
    s_words.used = 0u;
    s_words.full = 0;
    const int filled = EMBED_CALL(find_build_words, DebugFindCtx, .json = &json, .named_value = named[chosen],
                                  .words_node = words_of[chosen], .path = path, .out_text = out, .program = compiler,
                                  .words_words_pointer = &s_words, .err_msg = args->err_msg);
    EMBED_CALL(find_release, DebugFindCtx, .manifest = &json);
    if (filled && (s_words.count == 0u))
    {
        mmgr_line_put(args->err_msg, "the build names no program");
    }
    if (!filled || (s_words.count == 0u))
    {
        return 0;
    }
    char run_path[CLI_PATH_TEXT];
    EMBED_CALL(cli_toolchains_count_path.run_path, CliToolchainsCountPathCfg, .path = run_path,
               .size = sizeof(run_path));
    const CliSetting settings[1] = {{"PATH", run_path}};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_buf = {why, sizeof(why), 0ULL};
    CliRunResult *const run_result = cli_run_result_alloc();
    const int run_result_well =
        EMBED_CALL(cli_words_internal.run_result_within, CliWordsCtx, .words = s_words.word_text, .folder = args->root,
                   .settings = settings, .count = 1u, .input = NULL, .length = 0ULL,
                   .milliseconds_u64 = CLI_DEBUG_BUILDING, .run_result = run_result, .err_msg = &err_buf);
    if (!run_result_well)
    {
        mmgr_line_put(args->err_msg, s_words.word_text[0]);
        mmgr_line_put(args->err_msg, ": ");
        mmgr_line_put_n(args->err_msg, err_buf.out, err_buf.at);
    }
    if (run_result_well && run_result->stopped)
    {
        mmgr_line_put(args->err_msg, "the build took longer than 300 s");
    }
    else if (run_result_well && (run_result->code != 0LL))
    {
        unsigned long long length = 0ULL;
        mmgr_line_put(args->err_msg, "the build for debugging failed:\n");
        // its errors, then its output, trimmed at both ends
        const unsigned long long start = args->err_msg->at;
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
                   .text_bytes = (const unsigned char *)run_result->err, .length = run_result->err_size);
        EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
                   .text_bytes = (const unsigned char *)run_result->out, .length = run_result->out_size);
        while ((args->err_msg->at > start) &&
               (cellul.chr(&(CatenaFinitaCfg){.src = " \t\r\n\f\v",
                                              .cap = CLI_CELLS_POOL,
                                              .byte = args->err_msg->out[args->err_msg->at - 1ULL]}) != NULL))
        {
            args->err_msg->at -= 1ULL;
        }
        while ((start + length < args->err_msg->at) &&
               (cellul.chr(&(CatenaFinitaCfg){
                    .src = " \t\r\n\f\v", .cap = CLI_CELLS_POOL, .byte = args->err_msg->out[start + length]}) != NULL))
        {
            length += 1ULL;
        }
        memor.move_down(&(MemoriaCfg){.dst = args->err_msg->out + start,
                                      .src = args->err_msg->out + start + length,
                                      .bytes = args->err_msg->at - start - length});
        args->err_msg->at -= length;
    }
    const int done = run_result_well && !run_result->stopped && (run_result->code == 0LL);
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    if (done)
    {
        memor.cpy(&(MemoriaCfg){.dst = args->program,
                                .src = out,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = out, .cap = CLI_CELLS_POOL}) + 1u});
        *built = 1;
    }
    return done;
}
