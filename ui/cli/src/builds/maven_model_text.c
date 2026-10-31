// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_model_text.c: what a Maven model contains, stored in cells that grow: its properties' lists and its managed
// dependencies' sharing a cell each, and its texts, each doubling as it is put, and the cells given back
#include "maven_internal.h"

// the properties, the managed dependencies and the bytes of text a model's rooms are begun with, each doubling
#define CLI_MAVEN_FIRST 16u
#define CLI_MAVEN_TEXT_FIRST 1024u

void cli_maven_model_given(CliMavenModel *model)
{
    g_cli_cells->persistent_buf_release(model->name_at, &g_cli_maven_model_cells);
    g_cli_cells->persistent_buf_release(model->managed_at, &g_cli_maven_model_cells);
    g_cli_cells->persistent_buf_release(model->text, &g_cli_maven_model_cells);
    *model = (CliMavenModel){NULL, NULL, NULL, NULL, 0u, 0u, NULL, NULL, 0u, 0u, 0, 0, NULL, 0u, 0u};
}

// `lists` lists of `count` each sharing `cell`, `*room` apiece, made to fit one more each, `most` at the most: the
// cell, laid again in a cell of doubled room where it is full, each list at a multiple of the room from its start; or
// NULL where it contains the most or the cell is refused
static unsigned int *model_lists_grown(unsigned int *cell, unsigned int lists, unsigned int count, unsigned int *room,
                                       unsigned int most, const char *what)
{
    if (count < *room)
    {
        return cell;
    }
    const unsigned int doubled = (*room == 0u) ? CLI_MAVEN_FIRST : (*room * 2u);
    const unsigned int grown = (doubled < most) ? doubled : most;
    unsigned int *const larger =
        (count < most) ? g_cli_cells->persistent_buf_alloc((unsigned long long)grown * lists * sizeof(unsigned int),
                                                           &g_cli_maven_model_cells)
                       : NULL;
    if (larger == NULL)
    {
        return NULL;
    }
    for (unsigned int list = 0u; list < lists; list += 1u)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = larger + (list * grown), .src = cell + (list * *room), .bytes = count * sizeof(unsigned int)});
    }
    g_cli_cells->persistent_buf_release(cell, &g_cli_maven_model_cells);
    *room = grown;
    return larger;
}

int cli_maven_property_room(CliMavenModel *model)
{
    unsigned int *const lists = model_lists_grown(model->name_at, 4u, model->properties, &model->property_room,
                                                  CLI_MAVEN_PROPERTIES, "a Maven model's properties");
    if (lists == NULL)
    {
        return 0;
    }
    const unsigned int room = model->property_room;
    model->name_at = lists;
    model->name_size = lists + room;
    model->value_at = lists + (2u * room);
    model->value_size = lists + (3u * room);
    return 1;
}

int cli_maven_managed_room(CliMavenModel *model)
{
    unsigned int *const lists = model_lists_grown(model->managed_at, 2u, model->managed, &model->managed_room,
                                                  CLI_MAVEN_MANAGED, "a Maven model's managed dependencies");
    if (lists == NULL)
    {
        return 0;
    }
    model->managed_at = lists;
    model->managed_size = lists + model->managed_room;
    return 1;
}

unsigned int cli_maven_model_text_stored(CliMavenModel *model, const char *bytes, unsigned long long size)
{
    if (size > CLI_MAVEN_MODEL_TEXT - model->used)
    {
        return CLI_MAVEN_MODEL_TEXT;
    }
    unsigned long long room = (model->text_room == 0u) ? CLI_MAVEN_TEXT_FIRST : model->text_room;
    while (room < (model->used + size))
    {
        room *= 2ull;
    }
    room = (room < CLI_MAVEN_MODEL_TEXT) ? room : CLI_MAVEN_MODEL_TEXT;
    if (room != model->text_room)
    {
        char *const larger = cli_cell_regrown(model->text, model->used, room, &g_cli_maven_model_cells);
        if (larger == NULL)
        {
            return CLI_MAVEN_MODEL_TEXT;
        }
        model->text = larger;
        // a room no more than CLI_MAVEN_MODEL_TEXT, which an unsigned int stores
        model->text_room = (unsigned int)room;
    }
    const unsigned int at = model->used;
    memor.cpy(&(MemoriaCfg){.dst = model->text + at, .src = bytes, .bytes = (size_t)size});
    model->used += (unsigned int)size;
    return at;
}
