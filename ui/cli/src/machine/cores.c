// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cores.c: the cores kept for the reader, as given or as the reader's settings set them, and those orior's work may
// take
#include "cores.h"
#include "kept.h"
#include "settings.h"

// the cores kept, 0 before anything has given them, under the lock
static unsigned int s_kept;
static CliLock s_lock = CLI_LOCK_INIT;

// What the cores kept are read from, a cell taken as they are first read and given back as they are kept: the reader's
// chain, an empty chain for a tree, and the layers of the two
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

void cli_cores_keep(unsigned int kept)
{
    cli_lock_take(&s_lock);
    s_kept = (kept > CLI_CORES_KEPT_LEAST) ? kept : CLI_CORES_KEPT_LEAST;
    cli_lock_give(&s_lock);
}

// a count as written, trimmed: 1 with it into `value`, or 0 where it is no count
static int cli_count(const char *text, unsigned int *value)
{
    while ((text[0] == ' ') || ((text[0] >= '\t') && (text[0] <= '\r')))
    {
        text += 1;
    }
    unsigned long long count = 0ull;
    const char *at = text;
    for (; (at[0] >= '0') && (at[0] <= '9') && (count <= 0xffffffffull); at += 1)
    {
        count = (count * 10ull) + (unsigned long long)(at[0] - '0');
    }
    const char *rest = at;
    while ((rest[0] == ' ') || ((rest[0] >= '\t') && (rest[0] <= '\r')))
    {
        rest += 1;
    }
    if ((at == text) || (rest[0] != '\0') || (count > 0xffffffffull))
    {
        return 0;
    }
    // a count below 2^32, as checked above
    *value = (unsigned int)count;
    return 1;
}

unsigned int cli_cores_kept(void)
{
    cli_lock_take(&s_lock);
    if (s_kept != 0u)
    {
        const unsigned int kept = s_kept;
        cli_lock_give(&s_lock);
        return kept;
    }
    CliCoresRead *const read = cli_cell_taken_by(&g_cli_settings_cells, sizeof(*read), "the cores kept read");
    if (read == NULL)
    {
        cli_lock_give(&s_lock);
        return CLI_CORES_KEPT_LEAST;
    }
    cli_settings_reader(&read->reader);
    cli_json_begun(&read->none.json, NULL, 0u, NULL, 0ull);
    read->none.files = 0u;
    read->none.problems = 0u;
    cli_settings_layers(&read->reader, &read->none, "", &read->layers);
    // the setting over the reader's own, else their own settings.json's, else under it
    char own[CLI_PATH_LONGEST];
    const char *set = cli_settings_layer(&read->layers, CLI_CORES_KEY, 1);
    if ((set == NULL) && (read->reader.files != 0u))
    {
        const CliJson *const json = &read->reader.json;
        const unsigned int value = cli_json_member(json, read->reader.values[read->reader.files - 1u], CLI_CORES_KEY);
        if (value != CLI_JSON_NONE)
        {
            CliLine line = {own, sizeof(own), 0ull};
            cli_kept_read_back(json, value, &line);
            cli_line_closed(&line);
            set = own;
        }
    }
    set = (set != NULL) ? set : cli_settings_layer(&read->layers, CLI_CORES_KEY, 0);
    unsigned int kept = CLI_CORES_KEPT_LEAST;
    if ((set == NULL) || !cli_count(set, &kept))
    {
        kept = CLI_CORES_KEPT_LEAST;
    }
    cli_settings_layers_given(&read->layers);
    cli_settings_chain_given(&read->reader);
    cli_cell_given_by(&g_cli_settings_cells, read);
    s_kept = (kept > CLI_CORES_KEPT_LEAST) ? kept : CLI_CORES_KEPT_LEAST;
    kept = s_kept;
    cli_lock_give(&s_lock);
    return kept;
}

unsigned int cli_cores_for_work(void)
{
    const unsigned int machine = cli_cores_machine();
    const unsigned int kept = cli_cores_kept();
    return (machine > kept) ? (machine - kept) : 1u;
}
