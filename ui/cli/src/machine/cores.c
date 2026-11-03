// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cores.c: the cores reserved for the reader, as given or as the reader's settings set them, and those orior's work may
// take
#include "cores.h"
#include "../text/text.h"
#include "settings.h"
#include "state.h"

// the cores reserved, 0 before anything has given them, under the lock
static unsigned int s_reserved;

// What the cores reserved are read from, a cell taken as they are first read and given back as they are reserved: the
// reader's chain, an empty chain for a tree, and the layers of the two
typedef struct
{
    CliChain reader;
    CliChain none;
    CliLayers layers;
} CliCoresRead;

unsigned int cli_cores_machine(void)
{
    return cli_processors();
}

void cli_cores_reserve(const CliCoresCfg *args)
{
    s_reserved = (args->reserved > CLI_CORES_RESERVED_MIN) ? args->reserved : CLI_CORES_RESERVED_MIN;
}

typedef struct
{
    const char *text;
    unsigned int *value;
} CoresCtx;
// a count as written, trimmed: 1 with it into `value`, or 0 where it is no count
EMBED_INLINE int cli_count(const CoresCtx *args)
{
    const char *text = args->text;
    unsigned int *value = args->value;
    text +=
        EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = text, .at = 0ULL,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}), .set = CLI_TEXT_WHITE);
    unsigned long long count = 0ULL;
    const char *at = text;
    unsigned long long digits_end = 0ULL;
    count = EMBED_CALL(cli_text_at_size.number, CliTextAtSizeCfg, .text = at, .at = 0ULL,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = at, .cap = CLI_CELLS_POOL}), .end = &digits_end);
    at += digits_end;
    const char *rest = at;
    rest +=
        EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = rest, .at = 0ULL,
                   .size = cellul.len(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL}), .set = CLI_TEXT_WHITE);
    if ((at == text) || (rest[0] != '\0') || (count > 0xffffffffULL))
    {
        return 0;
    }
    // a count below 2^32, as checked above
    *value = (unsigned int)count;
    return 1;
}

unsigned int cli_cores_on_reserve(void)
{
    if (s_reserved != 0u)
    {
        const unsigned int reserved = s_reserved;
        return reserved;
    }
    CliCoresRead *const read = g_cli_cells->temporary_buf_alloc(sizeof(*read), &g_cli_settings_cells);
    if (read == NULL)
    {
        return CLI_CORES_RESERVED_MIN;
    }
    EMBED_CALL(cli_settings_path_table.reader, CliSettingsPathCfg, .chain = &read->reader);
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = &read->none.json,
               .node_json_node_pointer = NULL, .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    read->none.files = 0u;
    read->none.problems = 0u;
    EMBED_CALL(cli_settings_root.layers, CliSettingsRootCfg, .reader = &read->reader, .tree = &read->none, .root = "",
               .layers = &read->layers);
    // the setting over the reader's own, else their own settings.json's, else under it
    char own[CLI_PATH_MAX_LEN];
    const char *set = EMBED_CALL(cli_settings_key.layer, CliSettingsKeyCfg, .layers_read = &read->layers,
                                 .key = CLI_CORES_KEY, .over = 1);
    if ((set == NULL) && (read->reader.files != 0u))
    {
        const CliJson *const json = &read->reader.json;
        const unsigned int value =
            EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                       .object = read->reader.values[read->reader.files - 1u], .name = CLI_CORES_KEY);
        if (value != CLI_JSON_NONE)
        {
            VerbaLinea line = {own, sizeof(own), 0ULL};
            EMBED_CALL(cli_state_json.read_back, CliStateJsonCfg, .json = json, .value = value, .out = &line);
            mmgr_line_finish(&line);
            set = own;
        }
    }
    set = (set != NULL) ? set
                        : EMBED_CALL(cli_settings_key.layer, CliSettingsKeyCfg, .layers_read = &read->layers,
                                     .key = CLI_CORES_KEY, .over = 0);
    unsigned int reserved = CLI_CORES_RESERVED_MIN;
    if ((set == NULL) || !EMBED_CALL(cli_count, CoresCtx, .text = set, .value = &reserved))
    {
        reserved = CLI_CORES_RESERVED_MIN;
    }
    EMBED_CALL(cli_settings_root.layers_free, CliSettingsRootCfg, .layers = &read->layers);
    EMBED_CALL(cli_settings_path_table.chain_free, CliSettingsPathCfg, .chain = &read->reader);
    g_cli_cells->persistent_buf_release(read, &g_cli_settings_cells);
    s_reserved = (reserved > CLI_CORES_RESERVED_MIN) ? reserved : CLI_CORES_RESERVED_MIN;
    reserved = s_reserved;
    return reserved;
}

unsigned int cli_cores_for_work(void)
{
    const unsigned int machine = cli_cores_machine();
    const unsigned int reserved = cli_cores_on_reserve();
    return (machine > reserved) ? (machine - reserved) : 1u;
}
