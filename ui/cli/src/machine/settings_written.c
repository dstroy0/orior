// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// settings_written.c: the settings in force written as the window reads them, each map in the order of its keys, and
// a setting's text in force read from them
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "settings_internal.h"

typedef struct
{
    const CliLayers *layers;
    unsigned int text;
    VerbaLinea *out;
    const void *items;
    unsigned int (*keys)(const CliItemsAtCfg *args);
    unsigned int count;
    unsigned int *order;
    const CliLayerText *texts;
    const unsigned int *texts_u32s;
} SettingsWrittenCtx;

const char *cli_settings_layer(const CliSettingsKeyCfg *args)
{
    const CliLayerText *const texts = args->over ? args->layers_read->over : args->layers_read->under;
    const unsigned int count = args->over ? args->layers_read->overs : args->layers_read->unders;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){
                .src = args->layers_read->text + texts[at].key, .other = args->key, .cap = CLI_CELLS_POOL}))
        {
            return args->layers_read->text + texts[at].text;
        }
    }
    return NULL;
}
