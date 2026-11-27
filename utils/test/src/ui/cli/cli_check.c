// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cli_check.c: the checks of orior's C side, each printed on the output as it is made, and their count last
#include "cli_check.h"
#include "../../../../../ui/cli/src/command_line/pool.h"

#include "../../../../../ui/cli/src/text/text.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"

static unsigned long long s_checks;
static unsigned long long s_failed;

void cli_check_begun(void)
{
}

// a line written to the output
static void check_line(const VerbaLinea *line)
{
    EMBED_CALL(query_errors_internal.terminal_write, QueryErrorsCtx, .errors = 0, .text = line->out,
               .length = mmgr_line_ok(line) ? line->at : 0ull);
}

// ORIOR_CELLS_OWNERS set: the pool's report and MMgr's owner table written, each owner's bytes now and its peak
static void check_owners_reported(void)
{
    char requested[8];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_CELLS_OWNERS",
                    .value_out = requested, .capacity = sizeof(requested)))
    {
        return;
    }
    static char s_owners[256ull * 1024ull];
    VerbaLinea owners = {s_owners, sizeof(s_owners), 0ull};
    mmgr_line_put(&owners, "cells pool ");
    cli_pool_usage_write(&owners);
    mmgr_line_ch(&owners, '\n');
    // Where the persistent tier ends and where the temporary tier's top is now. The gap between them is what a take
    // from either tier can still get
    const SeriesCellularum *const record = cli_pool_series_cellularum();
    mmgr_line_put(&owners, "cells ends ");
    mmgr_line_decimal(&owners, (unsigned long long)mmgr_series_cellularum_persistent_end(record), 1u);
    mmgr_line_ch(&owners, ' ');
    mmgr_line_decimal(&owners, (unsigned long long)mmgr_series_cellularum_temporary_top(record), 1u);
    mmgr_line_ch(&owners, '\n');
    mmgr_line_put(&owners, "cells owners ");
    cli_pool_owners_write(&owners);
    mmgr_line_ch(&owners, '\n');
    check_line(&owners);
}

// the pool's high-water marks as the last check left them, and the rise, in bytes, that names a check as one that
// raised them
static unsigned long long s_persistent_hw;
static unsigned long long s_temporary_hw;
#define CHECK_RISE_BYTES (4ull * 1024ull * 1024ull)

// The pool's high-water marks written after a check that raised either by CHECK_RISE_BYTES or more. It names the
// check to look at for a run near the pool's ceiling
static void check_rise_reported(void)
{
    unsigned long long pool = 0ull;
    unsigned long long persistent = 0ull;
    unsigned long long temporary = 0ull;
    cli_pool_report(&pool, &persistent, &temporary);
    if ((persistent < s_persistent_hw + CHECK_RISE_BYTES) && (temporary < s_temporary_hw + CHECK_RISE_BYTES))
    {
        return;
    }
    s_persistent_hw = persistent;
    s_temporary_hw = temporary;
    char text[128];
    VerbaLinea line = {text, sizeof(text), 0ull};
    mmgr_line_put(&line, "  cells rose to ");
    cli_pool_usage_write(&line);
    mmgr_line_put(&line, " by this check\n");
    check_line(&line);
}

void cli_check(int passed, const char *what)
{
    char text[4096];
    VerbaLinea line = {text, sizeof(text), 0ull};
    mmgr_line_put(&line, passed ? "  ok   " : "  FAIL ");
    mmgr_line_put(&line, what);
    mmgr_line_ch(&line, '\n');
    check_line(&line);
    check_rise_reported();
    // the table at the first failure, where a pool that filled still is as it filled
    if (!passed && (s_failed == 0ull))
    {
        check_owners_reported();
    }
    s_checks += 1ull;
    s_failed += passed ? 0ull : 1ull;
}

void cli_check_text(const char *text, unsigned long long size, const char *required, const char *what)
{
    const int matches = (cellul.len(&(CatenaFinitaCfg){.src = required, .cap = CLI_POOL_BYTES}) == size) &&
                        (memor.cmp(&(MemoriaCfg){.src = text, .other = required, .bytes = size}) == 0);
    cli_check(matches, what);
    if (!matches)
    {
        static char s_received[1u << 16u];
        VerbaLinea line = {s_received, sizeof(s_received), 0ull};
        mmgr_line_put(&line, "    came: ");
        mmgr_line_put_n(&line, text, size);
        mmgr_line_put(&line, "\n    wanted: ");
        mmgr_line_put(&line, required);
        mmgr_line_ch(&line, '\n');
        check_line(&line);
    }
}

void cli_check_folder(const char *name, char *full)
{
    if (!EMBED_CALL(query_capacity_path_internal.path_full, QueryCapacityPathCtx, .path = name, .full = full,
                    .capacity = CLI_PATH_MAX_LEN))
    {
        memor.cpy(&(MemoriaCfg){.dst = full,
                                .src = name,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_POOL_BYTES}) + 1u});
    }
    (void)EMBED_CALL(query_path_seconds_internal.remove, QueryPathSecondsCtx, .path = full);
    (void)EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = full);
}

// The tree a parse writes a text into, a cell sized to the text at each parse. The one before goes back, and the last
// goes back as the checks end
static CliJson s_json;

const CliJson *cli_check_parsed(const char *text, unsigned long long size, unsigned int *root)
{
    g_cli_guard->persistent_buf_release(s_json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), s_json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char why[512];
    VerbaLinea err_msg = {why, sizeof(why), 0ull};
    *root = EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &s_json,
                       .text = text, .size = size, .nodes_more = 0u, .text_more = 0ull, .err_msg = &err_msg,
                       .owner = g_checks_owner);
    return &s_json;
}

const char *cli_check_member_text(unsigned int node, const char *name)
{
    const char *const text =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &s_json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &s_json,
                                      .object = node, .name = name),
                   .length_pointer = NULL);
    return (text != NULL) ? text : "";
}

unsigned int cli_check_finding(unsigned int found, const char *code, const char *part)
{
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = &s_json, .array = found, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        const unsigned int one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                            .json_read = &s_json, .array = found, .index = at);
        const char *const message =
            cli_check_member_text(EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                             .json_read = &s_json, .object = one, .name = "message"),
                                  "value");
        if (((code == NULL) ||
             (cellul.eq(&(CatenaFinitaCfg){
                 .src = cli_check_member_text(one, "code"), .other = code, .cap = CLI_POOL_BYTES}))) &&
            ((part == NULL) ||
             (cellul.find(&(CatenaFinitaCfg){
                  .src = message, .cap = CLI_POOL_BYTES, .other = part, .other_cap = CLI_POOL_BYTES}) != NULL)))
        {
            return one;
        }
    }
    return CLI_JSON_NONE;
}

int cli_check_completes(const CliCompletions *items, const char *label)
{
    for (unsigned int at = 0u; at < items->count; at += 1u)
    {
        const CliCompletion *const item = &items->item[at];
        if ((item->label_size == cellul.len(&(CatenaFinitaCfg){.src = label, .cap = CLI_POOL_BYTES})) &&
            (memor.cmp(&(MemoriaCfg){.src = items->text + item->label_at, .other = label, .bytes = item->label_size}) ==
             0))
        {
            return 1;
        }
    }
    return 0;
}

const CliCompletions *cli_check_build_completed(const char *root, const char *path, const char *text)
{
    static CliCompletions s_items;
    unsigned int line = 0u;
    unsigned int col = 0u;
    for (const char *at = text; at[0] != '\0'; at += 1)
    {
        line += (at[0] == '\n') ? 1u : 0u;
        col = (at[0] == '\n') ? 0u : col + 1u;
    }
    EMBED_CALL(cli_edits_items.completions_begin, CliEditsItemsCfg, .items = &s_items);
    EMBED_CALL(cli_builds_path.build_complete, CliBuildsPathCfg, .root = root, .path = path, .text = text,
               .size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES}), .line = line, .col = col,
               .items = &s_items);
    return &s_items;
}

void cli_check_server(const char *name, unsigned int port, char *host, unsigned int *at)
{
    char variable[64];
    VerbaLinea line = {variable, sizeof(variable), 0ull};
    mmgr_line_put(&line, "ORIOR_");
    mmgr_line_put(&line, name);
    mmgr_line_put(&line, "_ADDRESS");
    (void)mmgr_line_finish(&line);
    char named[CLI_NAME_MAX_LEN];
    memor.cpy(&(MemoriaCfg){.dst = host,
                            .src = "127.0.0.1",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "127.0.0.1", .cap = CLI_POOL_BYTES}) + 1u});
    *at = port;
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = variable, .value_out = named,
                    .capacity = sizeof(named)))
    {
        return;
    }
    const char *const colon =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = named,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_POOL_BYTES}), .byte = ':');
    unsigned long long number = 0ull;
    int digits = (colon != NULL) && (colon[1] != '\0');
    for (const char *character = (colon != NULL) ? colon + 1 : ""; digits && (character[0] != '\0'); character += 1)
    {
        // Explicit cast reads the byte as its code point
        digits = ascii.in(&(AsciiCfg){.type = MMGR_ASCII_NUM, .byte = (uint8_t)character[0]}) && (number < 65536ull);
        number = (number * 10ull) + (unsigned long long)(character[0] - '0');
    }
    if (digits && (number < 65536ull))
    {
        named[colon - named] = '\0';
        memor.cpy(&(MemoriaCfg){.dst = host,
                                .src = named,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = named, .cap = CLI_POOL_BYTES}) + 1u});
        // a port is below 65536, which an unsigned int represents
        *at = (unsigned int)number;
    }
}

int cli_on_check(void)
{
    g_cli_guard->persistent_buf_release(s_json.node, mmgr_owner_of_buf(cli_pool_series_cellularum(), s_json.node));
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = (&s_json),
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    char text[128];
    VerbaLinea line = {text, sizeof(text), 0ull};
    mmgr_line_decimal(&line, s_checks, 1u);
    mmgr_line_put(&line, " checks, ");
    mmgr_line_decimal(&line, s_failed, 1u);
    mmgr_line_put(&line, " failed, cells ");
    cli_pool_usage_write(&line);
    mmgr_line_ch(&line, '\n');
    check_line(&line);
    check_owners_reported();
    return (s_failed > 0ull) ? 1 : 0;
}

// the buffer a check reads a view's frame into, larger than any frame a check builds
static _Alignas(16) unsigned char s_check_frame[1u << 22u];

const CliViewFrame *cli_check_frame_kept(const char *key)
{
    const unsigned long long bytes = cli_view_frame_bytes(&(CliViewAtCfg){.key = key});
    cli_check(bytes <= sizeof(s_check_frame), "the kept frame fits the checks' buffer");
    return ((bytes != 0ULL) && (bytes <= sizeof(s_check_frame)))
               ? cli_view_frame_read(
                     &(CliViewAtCfg){.key = key, .frame_out = s_check_frame, .capacity = sizeof(s_check_frame)})
               : NULL;
}

const CliViewFrame *cli_check_frame(const char *key)
{
    const unsigned long long bytes = cli_view_frame_build(&(CliViewAtCfg){.key = key});
    cli_check(bytes <= sizeof(s_check_frame), "the frame fits the checks' buffer");
    return ((bytes != 0ULL) && (bytes <= sizeof(s_check_frame)))
               ? cli_view_frame_read(
                     &(CliViewAtCfg){.key = key, .frame_out = s_check_frame, .capacity = sizeof(s_check_frame)})
               : NULL;
}
