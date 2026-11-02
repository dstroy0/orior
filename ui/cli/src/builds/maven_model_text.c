// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_model_text.c: what a Maven model contains, stored in one cell taken at its most: its properties' lists, its
// managed dependencies' lists and its texts, and the cell given back
#include "maven_internal.h"

void cli_maven_model_free(const CliMavenTextSizeModelCtx *args)
{
    CliMavenModel *model = args->model;
    g_cli_cells->persistent_buf_release(model->name_at, &g_cli_maven_model_cells);
    *model = (CliMavenModel){NULL, NULL, NULL, NULL, 0u, 0u, NULL, NULL, 0u, 0u, 0, 0, NULL, 0u, 0u};
}

void cli_maven_model_take(const CliMavenTextSizeModelCtx *args)
{
    const unsigned long long lists_bytes =
        ((4ull * CLI_MAVEN_PROPERTIES) + (2ull * CLI_MAVEN_MANAGED)) * sizeof(unsigned int);
    unsigned int *const lists =
        g_cli_cells->persistent_buf_alloc(lists_bytes + CLI_MAVEN_MODEL_TEXT, &g_cli_maven_model_cells);
    if (lists == NULL)
    {
        return;
    }
    args->model->name_at = lists;
    args->model->name_size = lists + CLI_MAVEN_PROPERTIES;
    args->model->value_at = lists + (2u * CLI_MAVEN_PROPERTIES);
    args->model->value_size = lists + (3u * CLI_MAVEN_PROPERTIES);
    args->model->property_room = CLI_MAVEN_PROPERTIES;
    args->model->managed_at = lists + (4u * CLI_MAVEN_PROPERTIES);
    args->model->managed_size = args->model->managed_at + CLI_MAVEN_MANAGED;
    args->model->managed_room = CLI_MAVEN_MANAGED;
    args->model->text = (char *)(void *)(args->model->managed_size + CLI_MAVEN_MANAGED);
    args->model->text_room = CLI_MAVEN_MODEL_TEXT;
}

unsigned int cli_maven_model_text_store(const CliMavenSizeModelCtx *args)
{
    const char *bytes = args->bytes;
    if (args->size > args->model->text_room - args->model->used)
    {
        return CLI_MAVEN_MODEL_TEXT;
    }
    const unsigned int at = args->model->used;
    memor.cpy(&(MemoriaCfg){.dst = args->model->text + at, .src = bytes, .bytes = (size_t)args->size});
    args->model->used += (unsigned int)args->size;
    return at;
}
