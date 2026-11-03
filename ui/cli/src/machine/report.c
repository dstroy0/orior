// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// report.c: whether errors file on their own, stored in settings.json in orior's own folder with the window's own
// settings retained as they are; what identifies the reporter struck from a report; an error's fingerprint; and the
// run's recent errors
#include "report.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"

// the most recent errors retained for a report that requests them, and the most bytes of each; the nodes and the bytes
// of texts a tree of the settings reserves over for the answer, beyond its copy of the settings; and the bytes the
// settings written and a text struck are taken with
#define CLI_REPORT_RETAIN_MAX 20u
#define CLI_REPORT_ERROR (1u << 16u)
#define CLI_REPORT_ANSWER_NODES 4u
#define CLI_REPORT_ANSWER_TEXT 64ULL
#define CLI_REPORT_STRUCK_MORE 64ULL

// the run's recent errors, a ring of cells each of its error's size, the oldest at `first`, under the lock
static char *s_errors[CLI_REPORT_RETAIN_MAX];
static unsigned int s_first;
static unsigned int s_count;

// settings.json read and written whole under its lock

int cli_report_category_know(const CliReportCategoryTextCfg *args)
{
    static const char *const s_categories[4] = {"library", "cli", "ui", "unknown"};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->category, .other = s_categories[at], .cap = CLI_CELLS_POOL}))
        {
            return 1;
        }
    }
    return 0;
}

typedef struct
{
    char *path;
    CliJson *json;
    int copied;
    int *on;
    char letter;
    const char *text;
    const char *found;
    const char *put;
    VerbaLinea *out;
    char **struck;
} ReportCtx;
// settings.json's path into `path`, sized for CLI_PATH_MAX_LEN: 1, or 0 where orior_cli has no folder of its own
EMBED_INLINE int cli_settings_path(const ReportCtx *args)
{
    char home[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_folder_internal.home_folder, CliFolderCtx, .folder_out = home))
    {
        return 0;
    }
    VerbaLinea line = {args->path, CLI_PATH_MAX_LEN, 0ULL};
    mmgr_line_put(&line, home);
    mmgr_line_put(&line, "/settings.json");
    return mmgr_line_finish(&line) != 0ULL;
}

// settings.json read, the lock acquired, into a tree of its own sized to it, with capacity over for a copy of it and
// the answer where `copied` is 1: its object, or CLI_JSON_NONE where it is not there or does not read, the tree then
// reserving capacity for the answer alone where `copied` is 1; the tree given back with cli_json_free
EMBED_INLINE unsigned int cli_settings(const ReportCtx *args)
{
    CliJson *json = args->json;
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = json, .node_json_node_pointer = NULL,
               .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char path[CLI_PATH_MAX_LEN];
    unsigned long long size = 0ULL;
    const unsigned char *const bytes = EMBED_CALL(cli_settings_path, ReportCtx, .path = path)
                                           ? EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                        .size_pointer = &size, .owner = &g_cli_machine_cells)
                                           : NULL;
    // the file's bytes are read as its text
    const char *const text = (const char *)bytes;
    const unsigned int tokens =
        (args->copied && (bytes != NULL))
            ? EMBED_CALL(cli_json_tree_text.json_tokens_count, CliJsonTreeTextCfg, .text = text, .size = size)
            : 0u;
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea reason = {why, sizeof(why), 0ULL};
    const unsigned int read = (bytes != NULL)
                                  ? EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg,
                                               .json = json, .text = text, .size = size,
                                               .nodes_more = args->copied ? (tokens + CLI_REPORT_ANSWER_NODES) : 0u,
                                               .text_more = args->copied ? (size + CLI_REPORT_ANSWER_TEXT) : 0ULL,
                                               .err_msg = &reason, .owner = &g_cli_machine_cells)
                                  : CLI_JSON_NONE;
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_machine_cells);
    const unsigned int root = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = json,
                                         .node = read, .kind = CFG_JSON_OBJECT)
                                  ? read
                                  : CLI_JSON_NONE;
    if (args->copied && (root == CLI_JSON_NONE))
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = json);
        (void)EMBED_CALL(cli_json_tree_nodes_max.json_reserve, CliJsonTreeNodesMaxCfg, .json = json,
                         .nodes_max = CLI_REPORT_ANSWER_NODES, .text_max = CLI_REPORT_ANSWER_TEXT,
                         .what = "the settings", .owner = &g_cli_machine_cells);
    }
    return root;
}

// the reporter's answer: 1 with it into `on`, or 0 where they have not answered
EMBED_INLINE int cli_answer(const ReportCtx *args)
{
    int *on = args->on;
    CliJson json;
    const unsigned int root = EMBED_CALL(cli_settings, ReportCtx, .json = &json, .copied = 0);
    const unsigned int answer = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                           .object = root, .name = "auto_report");
    const int answered = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                    .node = answer, .kind = CFG_JSON_TRUE) ||
                         EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json,
                                    .node = answer, .kind = CFG_JSON_FALSE);
    *on = EMBED_CALL(cli_json_tree_node_kind.json_is, CliJsonTreeNodeKindCfg, .json_read = &json, .node = answer,
                     .kind = CFG_JSON_TRUE);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    return answered;
}

int cli_report_auto(void)
{
    char none[CLI_NAME_MAX_LEN];
    int on = 0;
    return !EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_NO_REPORTS",
                       .value_out = none, .capacity = sizeof(none)) &&
           EMBED_CALL(cli_answer, ReportCtx, .on = &on) && on;
}

int cli_report_prompt(void)
{
    int on = 0;
    return EMBED_CALL(cli_answer, ReportCtx, .on = &on);
}

void cli_report_prompt_once(void)
{
    char none[CLI_NAME_MAX_LEN];
    if (cli_report_prompt() ||
        EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_NO_REPORTS",
                   .value_out = none, .capacity = sizeof(none)) ||
        !query_terminal_is() || !cli_errors_terminal())
    {
        return;
    }
    static const char s_prompt[] = CLI_REPORT_QUESTION "\nFile them? [Y/n] ";
    EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 1, .text = s_prompt,
               .length = sizeof(s_prompt) - 1u);
    char answer[CLI_NAME_MAX_LEN];
    const unsigned long long length = EMBED_CALL(query_capacity_line_internal.terminal_line_read, QueryCapacityLineCtx,
                                                 .line = answer, .capacity = sizeof(answer));
    if (length == QRY_MEMBER_UNREAD)
    {
        return;
    }
    const char *from = answer;
    from +=
        EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ULL,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL}), .set = CLI_TEXT_WHITE);
    const int on = !((from[0] == 'n') || (from[0] == 'N'));
    char why[CLI_PATH_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    if (!EMBED_CALL(cli_report_on.set_auto, CliReportOnCfg, .on = on, .err_msg = &err_msg))
    {
        mmgr_line_ch(&err_msg, '\n');
        EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 1, .text = why,
                   .length = err_msg.at);
    }
}

int cli_report_set_auto(const CliReportOnCfg *args)
{
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_settings_path, ReportCtx, .path = path))
    {
        mmgr_line_put(args->err_msg, "orior_cli has no folder of its own to store settings in");
        return 0;
    }
    char folder[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = path, .out = folder) &&
        !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = folder))
    {
        mmgr_line_put(args->err_msg, folder);
        mmgr_line_put(args->err_msg, ": it could not be made");
        return 0;
    }
    CliJson json;
    const unsigned int settings_node = EMBED_CALL(cli_settings, ReportCtx, .json = &json, .copied = 1);
    // the answer first, then the window's own settings as they are
    const unsigned int made =
        EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = &json, .kind = CFG_JSON_OBJECT);
    EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &json, .object = made,
               .name = "auto_report",
               .value = EMBED_CALL(cli_json_tree_kind.json_init, CliJsonTreeKindCfg, .json = &json,
                                   .kind = args->on ? CFG_JSON_TRUE : CFG_JSON_FALSE));
    for (unsigned int member = (settings_node != CLI_JSON_NONE) ? json.node[settings_node].first : CLI_JSON_NONE;
         member != CLI_JSON_NONE; member = json.node[member].next)
    {
        const char *const key = json.text + json.node[member].key;
        if (!cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "auto_report", .cap = CLI_CELLS_POOL}))
        {
            EMBED_CALL(cli_json_tree_object_name.json_set, CliJsonTreeObjectNameCfg, .json = &json, .object = made,
                       .name = key,
                       .value = EMBED_CALL(cli_json_tree.json_copy, CliJsonTreeCfg, .json = &json, .node = member));
        }
    }
    const unsigned long long out_capacity = EMBED_CALL(cli_json_tree_node_out.json_pretty_capacity,
                                                       CliJsonTreeNodeOutCfg, .json_read = &json, .indent = 2u);
    char *const out_cell = g_cli_cells->temporary_buf_alloc(out_capacity, &g_cli_machine_cells);
    VerbaLinea out = mmgr_line_over(out_cell, out_capacity);
    const int begun = out.out != NULL;
    EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = &json, .node = made, .out = &out,
               .indent = 2u, .sorted = 0);
    const int written = begun && !json.full && mmgr_line_ok(&out) &&
                        EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path,
                                   .bytes = out.out, .size = out.at);
    g_cli_cells->persistent_buf_release(out.out, &g_cli_machine_cells);
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    if (!written)
    {
        mmgr_line_put(args->err_msg, path);
        mmgr_line_put(args->err_msg, ": it could not be written");
    }
    return written;
}

// `text` with every `found` in it, ASCII's case aside, put as `put`, into `out`
EMBED_INLINE void cli_on_replace(const ReportCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->found, .cap = CLI_CELLS_POOL});
    const char *at = args->text;
    const char *const end = args->text + cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL});
    while (at < end)
    {
        const char *const hit = (length != 0ULL) ? cellul.find(&(CatenaFinitaCfg){.src = at,
                                                                                  .cap = (size_t)(end - at) + 1u,
                                                                                  .other = args->found,
                                                                                  .other_cap = (size_t)length + 1u,
                                                                                  .ci = 1})
                                                 : NULL;
        const char *const stop = (hit != NULL) ? hit : end;
        mmgr_line_put_n(args->out, at, (size_t)(stop - at));
        if (hit == NULL)
        {
            break;
        }
        mmgr_line_put(args->out, args->put);
        at = hit + length;
    }
    mmgr_line_finish(args->out);
}

// `found`, three letters or more, struck from the text `*struck` contains as `put`, as it is, with its slashes leaning
// forward, and with them leaning back, each pass into a cell of its own sized to the text and the one before given
// back; where a cell is refused the text is let go whole, and nothing it contained goes out unstruck
EMBED_INLINE void cli_strike(const ReportCtx *args)
{
    char **struck = args->struck;
    if (cellul.len(&(CatenaFinitaCfg){.src = args->found, .cap = CLI_CELLS_POOL}) < 3u)
    {
        return;
    }
    char forms[3][CLI_PATH_MAX_LEN];
    for (unsigned int form = 0u; form < 3u; form += 1u)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = forms[form],
                          .src = args->found,
                          .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->found, .cap = CLI_CELLS_POOL}) + 1u});
        if (form != 0u)
        {
            EMBED_CALL(cli_text_size_from.swap, CliTextSizeFromCfg, .text_out = forms[form],
                       .size = cellul.len(&(CatenaFinitaCfg){.src = forms[form], .cap = CLI_CELLS_POOL}),
                       .from = (form == 1u) ? '\\' : '/', .to = (form == 1u) ? '/' : '\\');
        }
        const char *const text = (*struck != NULL) ? *struck : "";
        // every place the form stands puts `put` in its stead, a form of at least 3 bytes standing at most once in
        // each 3 of the text
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
        const unsigned long long capacity =
            length + ((length / 3ULL) * cellul.len(&(CatenaFinitaCfg){.src = args->put, .cap = CLI_CELLS_POOL})) +
            CLI_REPORT_STRUCK_MORE;
        char *const out_cell = g_cli_cells->temporary_buf_alloc(capacity, &g_cli_machine_cells);
        VerbaLinea out = mmgr_line_over(out_cell, capacity);
        EMBED_CALL(cli_on_replace, ReportCtx, .text = text, .found = forms[form], .put = args->put, .out = &out);
        g_cli_cells->persistent_buf_release(*struck, &g_cli_machine_cells);
        *struck = mmgr_line_ok(&out) ? out_cell : NULL;
        if (*struck == NULL)
        {
            g_cli_cells->persistent_buf_release(out_cell, &g_cli_machine_cells);
        }
    }
}

void cli_report_scrub(const CliReportTextCfg *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_CELLS_POOL});
    char *struck = g_cli_cells->temporary_buf_alloc(length + 1ULL, &g_cli_machine_cells);
    if (struck != NULL)
    {
        memor.cpy(&(MemoriaCfg){.dst = struck, .src = args->text, .bytes = length + 1u});
    }
    if (args->root != NULL)
    {
        EMBED_CALL(cli_strike, ReportCtx, .struck = &struck, .found = args->root, .put = "<tree>");
    }
    static const char *const s_homes[2] = {"USERPROFILE", "HOME"};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        char home[CLI_PATH_MAX_LEN];
        if (EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = s_homes[at],
                       .value_out = home, .capacity = sizeof(home)))
        {
            EMBED_CALL(cli_strike, ReportCtx, .struck = &struck, .found = home, .put = "~");
        }
    }
    static const char *const s_users[2] = {"USERNAME", "USER"};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        char name[CLI_NAME_MAX_LEN];
        if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = s_users[at],
                        .value_out = name, .capacity = sizeof(name)))
        {
            continue;
        }
        static const char *const s_slashes[2] = {"\\", "/"};
        for (unsigned int slash = 0u; slash < 2u; slash += 1u)
        {
            char found[CLI_NAME_MAX_LEN + 4u];
            char put[16];
            VerbaLinea line = {found, sizeof(found), 0ULL};
            mmgr_line_put(&line, s_slashes[slash]);
            mmgr_line_put(&line, name);
            mmgr_line_put(&line, s_slashes[slash]);
            mmgr_line_finish(&line);
            VerbaLinea user = {put, sizeof(put), 0ULL};
            mmgr_line_put(&user, s_slashes[slash]);
            mmgr_line_put(&user, "<user>");
            mmgr_line_put(&user, s_slashes[slash]);
            mmgr_line_finish(&user);
            EMBED_CALL(cli_strike, ReportCtx, .struck = &struck, .found = found, .put = put);
        }
    }
    mmgr_line_put(args->out, (struck != NULL) ? struck : "");
    g_cli_cells->persistent_buf_release(struck, &g_cli_machine_cells);
}

void cli_report_fingerprint(const CliReportCategoryTextCfg *args)
{
    unsigned long long hash = 0xcbf29ce484222325ULL;
    for (const char *at = args->category; at[0] != '\0'; at += 1)
    {
        hash = (hash ^ (unsigned char)at[0]) * 0x100000001b3ULL;
    }
    for (const char *at = args->text; at[0] != '\0'; at += 1)
    {
        if ((at[0] < '0') || (at[0] > '9'))
        {
            hash = (hash ^ (unsigned char)at[0]) * 0x100000001b3ULL;
        }
    }
    VerbaLinea line = {args->out_capacity, 17u, 0ULL};
    mmgr_line_hex(&line, hash, 16u);
    mmgr_line_finish(&line);
}

void cli_report_save(const CliReportTextCfg *args)
{
    char *const line_cell = g_cli_cells->persistent_buf_alloc(CLI_REPORT_ERROR, &g_cli_machine_cells);
    if (line_cell == NULL)
    {
        return;
    }
    VerbaLinea line = {line_cell, CLI_REPORT_ERROR, 0ULL};
    EMBED_CALL(cli_report_text.scrub, CliReportTextCfg, .text = args->text, .root = args->root, .out = &line);
    // an error stored is its first CLI_REPORT_ERROR bytes at the most
    line.at = (line.at < CLI_REPORT_ERROR) ? line.at : (CLI_REPORT_ERROR - 1u);
    line.out[(line.at < line.capacity) ? line.at : (line.capacity - 1u)] = '\0';
    const unsigned int place = (s_first + s_count) % CLI_REPORT_RETAIN_MAX;
    g_cli_cells->persistent_buf_release(s_errors[place], &g_cli_machine_cells);
    s_errors[place] = line.out;
    if (s_count < CLI_REPORT_RETAIN_MAX)
    {
        s_count += 1u;
    }
    else
    {
        s_first = (s_first + 1u) % CLI_REPORT_RETAIN_MAX;
    }
}

void cli_report_recent(const CliReportRootOutCfg *args)
{
    for (unsigned int at = 0u; at < s_count; at += 1u)
    {
        mmgr_line_put(args->out, (at != 0u) ? "\n\n" : "");
        mmgr_line_put(args->out, s_errors[(s_first + at) % CLI_REPORT_RETAIN_MAX]);
    }
}

const char *cli_report_panic_category(const CliReportCfg *args)
{
    for (const char *line = args->trace; line[0] != '\0';)
    {
        const char *stop = line;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                           .size = cellul.len(&(CatenaFinitaCfg){.src = stop, .cap = CLI_CELLS_POOL}), .set = "\n");
        char category_buf[CLI_PATH_MAX_LEN];
        VerbaLinea category_line = {category_buf, sizeof(category_buf), 0ULL};
        mmgr_line_put_n(&category_line, line, (unsigned long long)(stop - line));
        mmgr_line_finish(&category_line);
        line = (stop[0] != '\0') ? (stop + 1) : stop;
        if (cellul.find(&(CatenaFinitaCfg){
                .src = category_buf, .cap = CLI_CELLS_POOL, .other = "orior_gui_lib::", .other_cap = CLI_CELLS_POOL}) !=
            NULL)
        {
            return "ui";
        }
        if ((cellul.find(&(CatenaFinitaCfg){.src = category_buf,
                                            .cap = CLI_CELLS_POOL,
                                            .other = "orior_cli::cli::",
                                            .other_cap = CLI_CELLS_POOL}) != NULL) ||
            (cellul.find(&(CatenaFinitaCfg){.src = category_buf,
                                            .cap = CLI_CELLS_POOL,
                                            .other = "orior_cli::main",
                                            .other_cap = CLI_CELLS_POOL}) != NULL))
        {
            return "cli";
        }
        if ((cellul.find(&(CatenaFinitaCfg){
                 .src = category_buf, .cap = CLI_CELLS_POOL, .other = "orior_cli::", .other_cap = CLI_CELLS_POOL}) !=
             NULL) &&
            (cellul.find(&(CatenaFinitaCfg){.src = category_buf,
                                            .cap = CLI_CELLS_POOL,
                                            .other = "orior_cli::report::",
                                            .other_cap = CLI_CELLS_POOL}) == NULL))
        {
            return "library";
        }
    }
    return "unknown";
}
