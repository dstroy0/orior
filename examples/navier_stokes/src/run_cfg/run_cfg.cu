// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// run_cfg.cu: reading a cfg (run_cfg.h)
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "run_cfg.h"

int run_cfg_open(const char *path, RunCfg *cfg, VerbaLinea *line)
{
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        mmgr_line_put(line, "  the cfg does not open\n");
        return 0;
    }
    // the bytes counted first, then the cell taken at their count, then filled
    const long end = ((fseek(file, 0L, SEEK_END) == 0) ? ftell(file) : -1L);
    const int rewound = (end >= 0L) && (fseek(file, 0L, SEEK_SET) == 0);
    // Explicit cast takes the length of a file that ftell measured, which is not negative here
    const size_t size = rewound ? (size_t)end : 0u;
    cfg->text = rewound ? (char *)platform_cell_taken(size + 1u, platform_place_g_program_owner, 0) : NULL;
    const int read = (cfg->text != NULL) && (fread(cfg->text, 1u, size, file) == size);
    fclose(file);
    if (!read)
    {
        mmgr_line_put(line, "  the cfg does not read whole\n");
        return 0;
    }
    cfg->text[size] = '\0';
    cfg->size = size;
    // a token takes at least one byte of the text. Explicit cast takes a count of the bytes of a cfg, far below what an
    // unsigned int counts
    cfg->token_room = (unsigned int)(size + 1u);
    cfg->tokens =
        (CfgJsonToken *)platform_cell_cleared(cfg->token_room, sizeof(CfgJsonToken), platform_place_g_program_owner, 0);
    if (cfg->tokens == NULL)
    {
        mmgr_line_put(line, "  the cfg's tokens have no room\n");
        return 0;
    }
    CfgJsonParse parse;
    if (cfg_json_parse(cfg->text, cfg->size, cfg->tokens, cfg->token_room, &parse) == 0)
    {
        mmgr_line_put(line, "  the cfg does not parse: ");
        mmgr_line_put(line, parse.reason);
        mmgr_line_ch(line, '\n');
        return 0;
    }
    return 1;
}

unsigned int run_cfg_find(const RunCfg *cfg, const char *path)
{
    unsigned int at = 0u;
    char part[256];
    const char *walk = path;
    while (*walk != '\0')
    {
        // the name before the next point, at most one byte short of the part's room
        const size_t length =
            EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = walk, .cap = sizeof(part) - 1u, .other = ".");
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = part, .src = walk, .bytes = length);
        part[length] = '\0';
        if ((cfg->tokens[at].type != CFG_JSON_OBJECT) ||
            ((at = cfg_json_member(cfg->text, cfg->tokens, at, part)) == 0u))
        {
            return 0u;
        }
        walk += length;
        if (*walk == '.')
        {
            walk += 1;
        }
    }
    return at;
}

// the decimal string at token `at` read exactly: 1, or 0 where it does not read
static int run_cfg_decimal(const RunCfg *cfg, unsigned int at, SimRational *value)
{
    const CfgJsonToken *const token = &cfg->tokens[at];
    if (token->type != CFG_JSON_STRING)
    {
        return 0;
    }
    const char *const text = &cfg->text[token->start];
    const size_t length = token->end - token->start;
    const void *const point = EMBED_CALL(memor.chr, MemoriaCfg, .src = text, .bytes = length, .val = '.');
    // the point lies inside the text. The places after it number fewer than the text's bytes
    const unsigned int places = (point != NULL) ? (unsigned int)(length - 1u - (size_t)((const char *)point - text)) : 0u;
    if ((orior_exact_from_decimal(text, length, places, &value->numerator) != ORIOR_EXACT_OK) ||
        (sim_exact_power(10ull, places, &value->denominator) == 0))
    {
        return 0;
    }
    sim_rational_settle(value);
    return 1;
}

int run_cfg_rational(const RunCfg *cfg, const char *path, SimRational *value)
{
    const unsigned int at = run_cfg_find(cfg, path);
    return (at != 0u) && run_cfg_decimal(cfg, at, value);
}

int run_cfg_count(const RunCfg *cfg, const char *path, unsigned long long *value)
{
    const unsigned int at = run_cfg_find(cfg, path);
    return (at != 0u) && cfg_json_unsigned(cfg->text, &cfg->tokens[at], value);
}

int run_cfg_rationals(const RunCfg *cfg, const char *path, SimRational **values, unsigned int *count)
{
    *values = NULL;
    *count = 0u;
    const unsigned int at = run_cfg_find(cfg, path);
    if ((at == 0u) || (cfg->tokens[at].type != CFG_JSON_ARRAY) || (cfg->tokens[at].count == 0u))
    {
        return 0;
    }
    SimRational *const list = (SimRational *)platform_cell_taken(
        (unsigned long long)cfg->tokens[at].count * sizeof(SimRational), platform_place_g_program_owner, 0);
    if (list == NULL)
    {
        return 0;
    }
    unsigned int element = at + 1u;
    for (unsigned int index = 0u; index < cfg->tokens[at].count; index += 1u)
    {
        if (run_cfg_decimal(cfg, element, &list[index]) == 0)
        {
            platform_cell_released(list, platform_place_g_program_owner);
            return 0;
        }
        element = cfg->tokens[element].next;
    }
    *values = list;
    *count = cfg->tokens[at].count;
    return 1;
}

int run_cfg_text(const RunCfg *cfg, const char *path, char *value, size_t room)
{
    const unsigned int at = run_cfg_find(cfg, path);
    if ((at == 0u) || (cfg->tokens[at].type != CFG_JSON_STRING) || (cfg->tokens[at].end == cfg->tokens[at].start))
    {
        return 0;
    }
    const size_t length = cfg->tokens[at].end - cfg->tokens[at].start;
    if (length >= room)
    {
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = value, .src = &cfg->text[cfg->tokens[at].start], .bytes = length);
    value[length] = '\0';
    return 1;
}

int run_cfg_beside(const char *cfg_path, const RunCfg *cfg, const char *member, char *path, size_t room)
{
    char name[RUN_CFG_TEXT_ROOM];
    if (run_cfg_text(cfg, member, name, sizeof(name)) == 0)
    {
        return 0;
    }
    // the cfg's folder: its path through the last separator of either kind. Explicit cast reads the found byte as an
    // offset into the path
    const char *const slash =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = cfg_path, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '/');
    const char *const back =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = cfg_path, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\\');
    const char *const separator = ((back != NULL) && ((slash == NULL) || (back > slash))) ? back : slash;
    const size_t folder = (separator != NULL) ? ((size_t)(separator - cfg_path) + 1u) : 0u;
    VerbaLinea written = mmgr_line_over(path, room);
    (void)mmgr_line_put_n(&written, cfg_path, folder);
    mmgr_line_put(&written, name);
    (void)mmgr_line_finish(&written);
    return mmgr_line_ok(&written) ? 1 : 0;
}

void run_cfg_missing(VerbaLinea *line, const char *members)
{
    mmgr_line_put(line, "  the cfg lacks a value, or one does not read: ");
    mmgr_line_put(line, members);
    mmgr_line_ch(line, '\n');
}

int run_cfg_short(void)
{
    return g_sim_rational_wide != 0;
}
