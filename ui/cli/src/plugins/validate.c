// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// validate.c: a file validated with a tool plugin: the prover found, the file checked as a copy, and each barrier that
// stops the check lifted a step and the copy checked again, as far as each barrier's ceiling
#include "../command_line/cells.h"
#include "validate_internal.h"

#include "../text/text.h"

CliValidateWorkState *g_cli_validate;

int cli_validate_lock(void)
{
    g_cli_validate = g_cli_cells->persistent_buf_alloc(sizeof(*g_cli_validate), &g_cli_plugins_cells);
    if (g_cli_validate == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &g_cli_validate->json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &g_cli_validate->manifest,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_validate->output = NULL;
    g_cli_validate->output_size = 0ULL;
    return 1;
}

unsigned int cli_validate_key_get(const CliValidateAtCtx *args)
{
    const CliTool *const tool = &g_cli_validate->tool;
    unsigned int key = 0u;
    while ((key < args->at) &&
           (!cellul.eq(&(CatenaFinitaCfg){
               .src = tool->barriers[key].name, .other = tool->barriers[args->at].name, .cap = CLI_CELLS_POOL})))
    {
        key += 1u;
    }
    return key;
}

unsigned int cli_validate_grown_keys(const CliValidateCtx *args)
{
    const CliTool *const tool = &g_cli_validate->tool;
    unsigned int count = 0u;
    for (unsigned int key = 0u; key < tool->barrier_count; key += 1u)
    {
        if (!g_cli_validate->has_grown[key])
        {
            continue;
        }
        unsigned int place = count;
        while ((place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                                           .one = tool->barriers[args->keys[place - 1u]].name,
                                           .other_pointer = tool->barriers[key].name) > 0))
        {
            args->keys[place] = args->keys[place - 1u];
            place -= 1u;
        }
        args->keys[place] = key;
        count += 1u;
    }
    return count;
}

typedef struct
{
    const char *path;
    char *project;
    const CliWords *words;
    unsigned int from;
    unsigned int settings;
    double seconds;
    const int *hit;
    unsigned int hits;
    char (*names)[CLI_NAME_MAX_LEN];
    unsigned long long *values;
    const char *text;
    unsigned long long size;
    const char *program;
    const char *copy;
    const char *project_read;
    VerbaLinea *err_msg;
    VerbaLinea *report;
} ValidateCtx;
// the folder above `path`, nearest first, that contains one of the files the tool's run names as marking a project,
// into `project`, sized for CLI_PATH_MAX_LEN: 1, or 0 where none does
EMBED_INLINE int validate_project_get(const ValidateCtx *args)
{
    const char *path = args->path;
    const CliJson *const json = &g_cli_validate->json;
    const unsigned int marks = g_cli_validate->tool.project;
    char here[CLI_PATH_MAX_LEN];
    char above[CLI_PATH_MAX_LEN];
    int has = EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = path, .out = here);
    while (has)
    {
        for (unsigned int index = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                                 .json_read = json, .array = marks, .index = index) != CLI_JSON_NONE;
             index += 1u)
        {
            char marked[CLI_PATH_MAX_LEN];
            EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = here,
                       .relative = EMBED_CALL(
                           cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                           .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = json, .array = marks, .index = index),
                           .length_pointer = NULL),
                       .joined = marked);
            if (EMBED_CALL(query_path_seconds_internal.is_file, QueryPathSecondsCtx, .path = marked))
            {
                memor.cpy(&(MemoriaCfg){.dst = args->project, .src = here, .bytes = CLI_PATH_MAX_LEN});
                return 1;
            }
        }
        has = EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = here, .out = above);
        memor.cpy(&(MemoriaCfg){.dst = here, .src = above, .bytes = CLI_PATH_MAX_LEN});
    }
    return 0;
}

// a check written as `{"settings", "seconds", "errors", "barriers"}`, a comma before it where it is not the first
EMBED_INLINE void validate_check_print(const ValidateCtx *args)
{
    VerbaLinea out = {g_cli_validate->checks + g_cli_validate->checks_size,
                      CLI_VALIDATE_CHECKS_TEXT - g_cli_validate->checks_size, 0ULL};
    const CliProverMessages *const messages = &g_cli_validate->last;
    mmgr_line_put(&out, (g_cli_validate->checks_size > 0ULL) ? ",{\"settings\":[" : "{\"settings\":[");
    for (unsigned int at = 0u; at < args->settings; at += 1u)
    {
        mmgr_line_put(&out, (at > 0u) ? "," : "");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &out,
                   .text = args->words->word_text[args->from + at],
                   .length = cellul.len(
                       &(CatenaFinitaCfg){.src = args->words->word_text[args->from + at], .cap = CLI_CELLS_POOL}));
    }
    unsigned long long errors = 0ULL;
    for (unsigned int at = 0u; at < messages->count; at += 1u)
    {
        errors += (messages->message[at].severity == 1u) ? 1ULL : 0ULL;
    }
    mmgr_line_put(&out, "],\"seconds\":");
    EMBED_CALL(cli_json_tree_out_value.json_double, CliJsonTreeOutValueCfg, .out = &out, .value_f64 = args->seconds);
    mmgr_line_put(&out, ",\"errors\":");
    mmgr_line_decimal(&out, errors, 1u);
    mmgr_line_put(&out, ",\"barriers\":[");
    const CliTool *const tool = &g_cli_validate->tool;
    for (unsigned int at = 0u; at < args->hits; at += 1u)
    {
        // a name the one before it wrote is written once
        if ((at > 0u) && (cellul.eq(&(CatenaFinitaCfg){.src = tool->barriers[args->hit[at]].name,
                                                       .other = tool->barriers[args->hit[at - 1u]].name,
                                                       .cap = CLI_CELLS_POOL})))
        {
            continue;
        }
        mmgr_line_put(&out, (at > 0u) ? "," : "");
        EMBED_CALL(
            cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = &out,
            .text = tool->barriers[args->hit[at]].name,
            .length = cellul.len(&(CatenaFinitaCfg){.src = tool->barriers[args->hit[at]].name, .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_put(&out, "]}");
    if (mmgr_line_ok(&out))
    {
        g_cli_validate->checks_size += out.at;
    }
}

// the limits that stop the check grown a step, each as far as its ceiling, its stops at the ceiling retained: 1 where
// any moved
EMBED_INLINE int validate_grow(const ValidateCtx *args)
{
    const CliTool *const tool = &g_cli_validate->tool;
    int moved = 0;
    for (unsigned int at = 0u; at < args->hits; at += 1u)
    {
        const CliBarrier *const barrier = &tool->barriers[args->hit[at]];
        const unsigned int key =
            EMBED_CALL(cli_validate_at_internal.key_get, CliValidateAtCtx, .at = (unsigned int)args->hit[at]);
        const unsigned long long now = g_cli_validate->has_grown[key] ? g_cli_validate->grown[key] : barrier->start;
        if (now >= barrier->ceiling)
        {
            g_cli_validate->stuck[key] = 1;
            g_cli_validate->stuck_at[key] = barrier->ceiling;
            continue;
        }
        const unsigned long long step = (barrier->grow > 2ULL) ? barrier->grow : 2ULL;
        // the step saturates where it would wrap, as Rust's saturating_mul does
        const unsigned long long times = (now > ~0ULL / step) ? ~0ULL : now * step;
        const unsigned long long next = (times < barrier->ceiling) ? times : barrier->ceiling;
        if (!g_cli_validate->has_grown[key] || (g_cli_validate->grown[key] != next))
        {
            g_cli_validate->grown[key] = next;
            g_cli_validate->has_grown[key] = 1;
            moved = 1;
        }
    }
    return moved;
}

// the options the grown limits set, by their names, each limit's barrier lifting by `-D<name>=...`, a later of one
// name standing for an earlier, into `names` and `values`: their count
EMBED_INLINE unsigned int validate_options(const ValidateCtx *args)
{
    const CliTool *const tool = &g_cli_validate->tool;
    unsigned int keys[CLI_VALIDATE_BARRIERS];
    const unsigned int grown = EMBED_CALL(cli_validate_internal.grown_keys, CliValidateCtx, .keys = keys);
    unsigned int count = 0u;
    for (unsigned int key = 0u; key < grown; key += 1u)
    {
        const char *const lift = tool->barriers[keys[key]].lift;
        if (!cellul.starts(&(CatenaFinitaCfg){.src = lift, .other = "-D", .cap = CLI_CELLS_POOL}))
        {
            continue;
        }
        char name[CLI_NAME_MAX_LEN];
        const char *const equal = cellul.chr(&(CatenaFinitaCfg){.src = lift + 2, .cap = CLI_CELLS_POOL, .byte = '='});
        const size_t length = (equal != NULL) ? (size_t)(equal - lift - 2)
                                              : cellul.len(&(CatenaFinitaCfg){.src = lift + 2, .cap = CLI_CELLS_POOL});
        if (length >= sizeof(name))
        {
            continue;
        }
        memor.cpy(&(MemoriaCfg){.dst = name, .src = lift + 2, .bytes = length});
        name[length] = '\0';
        unsigned int place = 0u;
        while ((place < count) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg, .one = args->names[place],
                                              .other_pointer = name) < 0))
        {
            place += 1u;
        }
        if ((place == count) ||
            (!cellul.eq(&(CatenaFinitaCfg){.src = args->names[place], .other = name, .cap = CLI_CELLS_POOL})))
        {
            memor.move_up(&(MemoriaCfg){.dst = args->names[place + 1u],
                                        .src = args->names[place],
                                        .bytes = (count - place) * sizeof(args->names[0])});
            memor.move_up(&(MemoriaCfg){.dst = &args->values[place + 1u],
                                        .src = &args->values[place],
                                        .bytes = (count - place) * sizeof(args->values[0])});
            memor.cpy(&(MemoriaCfg){.dst = args->names[place], .src = name, .bytes = length + 1u});
            count += 1u;
        }
        args->values[place] = g_cli_validate->grown[keys[key]];
    }
    return count;
}

// the checks run on a copy of the file at `copy`, each barrier that stops one grown a step and the copy checked again,
// until none stops it, none moves, one runs out of time, or CLI_VALIDATE_CHECKS have run: 1, or 0 with why given
EMBED_INLINE int validate_checks_run(const ValidateCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    const char *program = args->program;
    const char *project = args->project_read;
    VerbaLinea *err_msg = args->err_msg;
    static CliWords s_words;
    static int s_hit[CLI_VALIDATE_MESSAGES];
    static char s_names[CLI_VALIDATE_BARRIERS][CLI_NAME_MAX_LEN];
    unsigned long long values[CLI_VALIDATE_BARRIERS];
    for (unsigned int check = 0u; check < CLI_VALIDATE_CHECKS; check += 1u)
    {
        const unsigned int options = EMBED_CALL(validate_options, ValidateCtx, .names = s_names, .values = values);
        const char *names[CLI_VALIDATE_BARRIERS];
        for (unsigned int at = 0u; at < options; at += 1u)
        {
            names[at] = s_names[at];
        }
        VerbaLinea own = {g_cli_validate->own, CLI_VALIDATE_OWN, 0ULL};
        const unsigned long long copied =
            EMBED_CALL(cli_validate_text_size_internal.copy_get, CliValidateTextSizeCtx, .text = text, .size = size,
                       .names = names, .values = values, .count = options, .changed = &own);
        g_cli_validate->own_size = mmgr_line_ok(&own) ? own.at : 0ULL;
        if ((copied > CLI_VALIDATE_COPY) ||
            !EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = args->copy,
                        .bytes = g_cli_validate->copy, .size = copied))
        {
            mmgr_line_put(err_msg, args->copy);
            mmgr_line_put(err_msg, ": it could not be written");
            return 0;
        }
        unsigned int from = 0u;
        unsigned int settings = 0u;
        char dir[CLI_PATH_MAX_LEN];
        double seconds = 0.0;
        if (!EMBED_CALL(cli_validate_program_internal.command_for, CliValidateProgramCtx, .program = program,
                        .file = args->copy, .project = project, .words = &s_words, .from = &from, .settings = &settings,
                        .dir = dir, .err_msg = err_msg) ||
            !EMBED_CALL(cli_validate_err_msg_internal.check, CliValidateErrMsgCtx, .words_read = &s_words,
                        .dir_read = dir, .seconds = &seconds, .err_msg = err_msg))
        {
            return 0;
        }
        EMBED_CALL(cli_validate_size_internal.messages_get, CliValidateSizeCtx, .output = g_cli_validate->output,
                   .size = g_cli_validate->output_size, .messages = &g_cli_validate->last);
        if (!g_cli_validate->run_result)
        {
            memor.cpy(&(MemoriaCfg){
                .dst = &g_cli_validate->first, .src = &g_cli_validate->last, .bytes = sizeof(g_cli_validate->first)});
            g_cli_validate->run_result = 1;
        }
        unsigned int hits = 0u;
        for (unsigned int at = 0u; at < g_cli_validate->last.count; at += 1u)
        {
            const CliProverMessage *const message = &g_cli_validate->last.message[at];
            const int barrier = (message->severity == 1u)
                                    ? EMBED_CALL(cli_validate_messages_internal.barrier_get, CliValidateMessagesCtx,
                                                 .messages_read = &g_cli_validate->last, .message = message)
                                    : -1;
            if (barrier >= 0)
            {
                s_hit[hits] = barrier;
                hits += 1u;
            }
        }
        EMBED_CALL(validate_check_print, ValidateCtx, .words = &s_words, .from = from, .settings = settings,
                   .seconds = seconds, .hit = s_hit, .hits = hits);
        if (g_cli_validate->late || (hits == 0u) || !EMBED_CALL(validate_grow, ValidateCtx, .hit = s_hit, .hits = hits))
        {
            break;
        }
    }
    return 1;
}

// the validation with the file's text read, under the work state's lock: 1, or 0 with why given
EMBED_INLINE int validate_text(const ValidateCtx *args)
{
    const char *path = args->path;
    const char *text = args->text;
    unsigned long long size = args->size;
    VerbaLinea *report = args->report;
    const CliTool *const tool = &g_cli_validate->tool;
    CliJson *const json = &g_cli_validate->manifest;
    g_cli_cells->persistent_buf_release(json->node,
                                        EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = json->node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = json, .node_json_node_pointer = NULL,
               .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    const unsigned int manifest = EMBED_CALL(cli_toolchains_json_nodes_more.manifest, CliToolchainsJsonNodesMoreCfg,
                                             .json_out = json, .nodes_more = 0u, .text_more = 0ULL);
    unsigned int chain = CLI_JSON_NONE;
    for (unsigned int index = 0u; (chain == CLI_JSON_NONE) &&
                                  (EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                              .json_read = json, .array = manifest, .index = index) != CLI_JSON_NONE);
         index += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = json, .array = manifest, .index = index);
        const char *const id =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = one, .name = "id"),
                       .length_pointer = NULL);
        chain = ((id != NULL) &&
                 (cellul.eq(&(CatenaFinitaCfg){.src = id, .other = tool->toolchain, .cap = CLI_CELLS_POOL})))
                    ? one
                    : CLI_JSON_NONE;
    }
    if (chain == CLI_JSON_NONE)
    {
        mmgr_line_put(args->err_msg, "the toolchain manifest has no ");
        mmgr_line_put(args->err_msg, tool->toolchain);
        return 0;
    }
    unsigned int count = 0u;
    char (*const folders)[CLI_PATH_MAX_LEN] = cli_toolchains_path_allocated(&count, &g_cli_plugins_cells);
    EMBED_CALL(cli_toolchains_json.find, CliToolchainsJsonCfg, .json = json, .tool = chain,
               .path_callback = (const char (*)[CLI_PATH_MAX_LEN])folders, .count = count,
               .found = &g_cli_validate->found);
    g_cli_cells->persistent_buf_release(folders, &g_cli_plugins_cells);
    if (!g_cli_validate->found.has_program)
    {
        const char *const name =
            EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                       .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = chain, .name = "name"),
                       .length_pointer = NULL);
        mmgr_line_put(args->err_msg, (name != NULL) ? name : tool->toolchain);
        mmgr_line_put(args->err_msg, " is not found: File, Toolchains opens its install page or takes its folder");
        return 0;
    }
    char project[CLI_PATH_MAX_LEN];
    const int has_project = EMBED_CALL(validate_project_get, ValidateCtx, .path = path, .project = project);
    if (!EMBED_CALL(cli_validate_text_internal.code_get, CliValidateTextCtx, .text = text, .size = size))
    {
        mmgr_line_put(args->err_msg, path);
        mmgr_line_put(args->err_msg, " is larger than orior_cli supports");
        return 0;
    }
    cli_validate_words_get();
    g_cli_validate->theorem_count = 0u;
    if (tool->audited)
    {
        cli_validate_theorems_in();
    }
    unsigned long long lines = 0ULL;
    for (unsigned long long at = 0ULL; at < size; at += 1ULL)
    {
        lines += (text[at] == '\n') ? 1ULL : 0ULL;
    }
    g_cli_validate->lines = lines + (((size > 0ULL) && (text[size - 1ULL] != '\n')) ? 1ULL : 0ULL);
    char folder[CLI_PATH_MAX_LEN];
    char copy_name[CLI_PATH_MAX_LEN];
    char copy[CLI_PATH_MAX_LEN];
    const char *const named =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = path,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), .byte = '/');
#if defined(_WIN32)
    const char *const named_back =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = path,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}), .byte = '\\');
    const char *base;
    if ((named_back != NULL) && ((named == NULL) || (named_back > named)))
    {
        base = named_back + 1;
    }
    else if (named != NULL)
    {
        base = named + 1;
    }
    else
    {
        base = path;
    }
#else
    const char *const base = (named != NULL) ? named + 1 : path;
#endif
    const char *const dot =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = base,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL}), .byte = '.');
    const size_t stem = ((dot != NULL) && (dot > base))
                            ? (size_t)(dot - base)
                            : cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL});
    VerbaLinea copy_line = {copy_name, sizeof(copy_name), 0ULL};
    mmgr_line_ch(&copy_line, '.');
    mmgr_line_put_n(&copy_line, base, stem);
    mmgr_line_put(&copy_line, ".validate.");
    mmgr_line_decimal(&copy_line, cli_process_own(), 1u);
    mmgr_line_put(&copy_line, ".lean");
    mmgr_line_finish(&copy_line);
    if (EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = path, .out = folder))
    {
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = folder, .relative = copy_name, .joined = copy);
    }
    else
    {
        memor.cpy(&(MemoriaCfg){.dst = copy, .src = copy_name, .bytes = sizeof(copy)});
    }
    const int run_result = EMBED_CALL(validate_checks_run, ValidateCtx, .text = text, .size = size,
                                      .program = g_cli_validate->found.program, .copy = copy,
                                      .project_read = has_project ? project : NULL, .err_msg = args->err_msg);
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = copy);
    if (!run_result)
    {
        return 0;
    }
    EMBED_CALL(cli_validate_project_internal.report, CliValidateProjectCtx, .path = path,
               .project = has_project ? project : NULL, .report = report);
    return 1;
}

int cli_on_validate(const CliValidatePathCfg *args)
{
    if (!cli_validate_lock())
    {
        mmgr_line_put(args->err_msg, "the file could not be validated: the cells cannot meet it");
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = g_cli_validate->has_grown, .val = 0, .bytes = sizeof(g_cli_validate->has_grown)});
    memor.set(&(MemoriaCfg){.dst = g_cli_validate->stuck, .val = 0, .bytes = sizeof(g_cli_validate->stuck)});
    memor.set(&(MemoriaCfg){.dst = g_cli_validate->lifted, .val = 0, .bytes = sizeof(g_cli_validate->lifted)});
    g_cli_validate->checks_size = 0ULL;
    g_cli_validate->own_size = 0ULL;
    g_cli_validate->finding_count = 0u;
    g_cli_validate->finding_used = 0ULL;
    g_cli_validate->first.count = 0u;
    g_cli_validate->last.count = 0u;
    g_cli_validate->run_result = 0;
    g_cli_validate->late = 0;
    int validated = 0;
    if (!EMBED_CALL(cli_validate_id_internal.tool_read, CliValidateIdCtx, .id = args->id_read))
    {
        mmgr_line_put(args->err_msg, "no tool plugin is ");
        mmgr_line_put(args->err_msg, args->id_read);
    }
    else
    {
        unsigned long long size = 0ULL;
        const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = args->path,
                                                      .size_pointer = &size, .owner = &g_cli_plugins_cells);
        if ((bytes == NULL) ||
            !EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = (const char *)bytes, .size = size))
        {
            mmgr_line_put(args->err_msg, args->path);
            mmgr_line_put(args->err_msg, ": it could not be read");
        }
        else
        {
            validated = EMBED_CALL(validate_text, ValidateCtx, .path = args->path, .text = (const char *)bytes,
                                   .size = size, .report = args->report, .err_msg = args->err_msg);
        }
        g_cli_cells->persistent_buf_release((void *)(uintptr_t)bytes, (&g_cli_plugins_cells));
    }
    g_cli_cells->persistent_buf_release(g_cli_validate->json.node, EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg,
                                                                              .cell = g_cli_validate->json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&g_cli_validate->json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_cells->persistent_buf_release(
        g_cli_validate->manifest.node,
        EMBED_CALL(cli_cells_table.cell_owner, CliCellsCfg, .cell = g_cli_validate->manifest.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&g_cli_validate->manifest),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    g_cli_cells->persistent_buf_release(g_cli_validate->output, &g_cli_plugins_cells);
    g_cli_cells->persistent_buf_release(g_cli_validate, &g_cli_plugins_cells);
    g_cli_validate = NULL;
    return validated;
}
