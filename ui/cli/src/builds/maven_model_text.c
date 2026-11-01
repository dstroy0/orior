// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_model_text.c: what a Maven model contains, stored in one cell taken at its most: its properties' lists, its
// managed dependencies' lists and its texts, and the cell given back
#include "maven_internal.h"

void cli_maven_model_given(CliMavenModel *model)
{
    g_cli_cells->persistent_buf_release(model->name_at, &g_cli_maven_model_cells);
    *model = (CliMavenModel){NULL, NULL, NULL, NULL, 0u, 0u, NULL, NULL, 0u, 0u, 0, 0, NULL, 0u, 0u};
}

void cli_maven_model_taken(CliMavenModel *model)
{
    const unsigned long long lists_bytes =
        ((4ull * CLI_MAVEN_PROPERTIES) + (2ull * CLI_MAVEN_MANAGED)) * sizeof(unsigned int);
    unsigned int *const lists =
        g_cli_cells->persistent_buf_alloc(lists_bytes + CLI_MAVEN_MODEL_TEXT, &g_cli_maven_model_cells);
    if (lists == NULL)
    {
        return;
    }
    model->name_at = lists;
    model->name_size = lists + CLI_MAVEN_PROPERTIES;
    model->value_at = lists + (2u * CLI_MAVEN_PROPERTIES);
    model->value_size = lists + (3u * CLI_MAVEN_PROPERTIES);
    model->property_room = CLI_MAVEN_PROPERTIES;
    model->managed_at = lists + (4u * CLI_MAVEN_PROPERTIES);
    model->managed_size = model->managed_at + CLI_MAVEN_MANAGED;
    model->managed_room = CLI_MAVEN_MANAGED;
    model->text = (char *)(void *)(model->managed_size + CLI_MAVEN_MANAGED);
    model->text_room = CLI_MAVEN_MODEL_TEXT;
}

unsigned int cli_maven_model_text_stored(CliMavenModel *model, const char *bytes, unsigned long long size)
{
    if (size > model->text_room - model->used)
    {
        return CLI_MAVEN_MODEL_TEXT;
    }
    const unsigned int at = model->used;
    memor.cpy(&(MemoriaCfg){.dst = model->text + at, .src = bytes, .bytes = (size_t)size});
    model->used += (unsigned int)size;
    return at;
}
