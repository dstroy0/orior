// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// input_event.c: the one dispatch of an input by its type: each window has a table, and this reads it
#include "input_event.h"

#include <stddef.h>

void gui_input_event_handle(const GuiInputEventCfg *args)
{
    if ((args->input->type < GUI_INPUT_TYPES) && (args->handlers->by_type[args->input->type] != NULL))
    {
        EMBED_CALL(args->handlers->by_type[args->input->type], GuiInputHandlerCfg, .window = args->window,
                   .input = args->input);
    }
}
