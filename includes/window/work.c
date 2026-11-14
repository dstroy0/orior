// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// work.c: the host of the views over the editor: the open table reached for each draw and input in the frame's editor
// rectangle, the map's taken with it so that nothing under the view moves, and the view closed by Escape or by its own
// answer
#include "work.h"
#include "cellularum_laboro/cellularum_laboro.h"


typedef struct
{
    const GuiWorkCfg *work;
    unsigned int answer;
} WorkCtx;

// the hooks' arguments: the call's own, the rectangle the frame's editor
EMBED_INLINE GuiWorkCfg work_laid(const WorkCtx *args)
{
    GuiWorkCfg laid = *args->work;
    laid.rect = args->work->frame->editor;
    laid.table = (args->work->work != NULL) ? args->work->work->open : args->work->work_read->open;
    return laid;
}

// whether the pixel at x, y stands on the frame's editor or its map
EMBED_INLINE int work_over(const WorkCtx *args)
{
    const GuiWorkCfg *const work = args->work;
    return EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = work->frame->editor, .x = work->x,
                      .y = work->y) ||
           EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = work->frame->map, .x = work->x,
                      .y = work->y);
}

// a hook's `answer` followed: the view closed where it asks it, and whether the view took the input
EMBED_INLINE int work_answered(const WorkCtx *args)
{
    if (args->answer == GUI_WORK_CLOSES)
    {
        gui_work_close(args->work);
    }
    return args->answer != GUI_WORK_PASSED;
}

void gui_work_init(const GuiWorkCfg *args)
{
    args->work->open = NULL;
    args->work->scroll = 0u;
}

void gui_work_open(const GuiWorkCfg *args)
{
    // another view open goes; the same view opened again stands on what its table opened anew
    if (args->work->open != args->table)
    {
        gui_work_close(args);
    }
    args->work->open = args->table;
    args->work->scroll = 0u;
    if ((args->table != NULL) && (args->table->wheel != NULL))
    {
        GuiWorkCfg turned = EMBED_CALL(work_laid, WorkCtx, .work = args);
        turned.rows = 0;
        (void)args->table->wheel(&turned);
    }
}

void gui_work_close(const GuiWorkCfg *args)
{
    const GuiWorkTable *const open = args->work->open;
    args->work->open = NULL;
    args->work->scroll = 0u;
    if ((open != NULL) && (open->close != NULL))
    {
        GuiWorkCfg closing = *args;
        closing.table = open;
        open->close(&closing);
    }
}

int gui_work_is_open(const GuiWorkCfg *args)
{
    return args->work_read->open != NULL;
}

void gui_work_draw(const GuiWorkCfg *args)
{
    if ((args->work_read->open == NULL) || (args->work_read->open->draw == NULL))
    {
        return;
    }
    const GuiWorkCfg laid = EMBED_CALL(work_laid, WorkCtx, .work = args);
    args->work_read->open->draw(&laid);
}

unsigned int gui_work_hit(const GuiWorkCfg *args)
{
    if ((args->work_read->open == NULL) || !EMBED_CALL(work_over, WorkCtx, .work = args))
    {
        return GUI_WORK_OUTSIDE;
    }
    unsigned int index = GUI_WORK_NOTHING;
    GuiWorkCfg laid = EMBED_CALL(work_laid, WorkCtx, .work = args);
    laid.index_out = &index;
    const int found = (args->work_read->open->hit != NULL) && args->work_read->open->hit(&laid);
    return found ? index : GUI_WORK_NOTHING;
}

int gui_work_press(const GuiWorkCfg *args)
{
    const GuiWorkTable *const open = args->work->open;
    if (open == NULL)
    {
        return 0;
    }
    if ((args->index == GUI_WORK_NOTHING) || (args->index == GUI_WORK_OUTSIDE) || (open->press == NULL))
    {
        return 1;
    }
    const GuiWorkCfg laid = EMBED_CALL(work_laid, WorkCtx, .work = args);
    return EMBED_CALL(work_answered, WorkCtx, .work = args, .answer = open->press(&laid));
}

int gui_work_wheel(const GuiWorkCfg *args)
{
    const GuiWorkTable *const open = args->work->open;
    if ((open == NULL) || !EMBED_CALL(work_over, WorkCtx, .work = args))
    {
        return 0;
    }
    if (open->wheel == NULL)
    {
        return 1;
    }
    const GuiWorkCfg laid = EMBED_CALL(work_laid, WorkCtx, .work = args);
    return EMBED_CALL(work_answered, WorkCtx, .work = args, .answer = open->wheel(&laid));
}

int gui_work_key(const GuiWorkCfg *args)
{
    const GuiWorkTable *const open = args->work->open;
    if (open == NULL)
    {
        return 0;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = "Escape", .cap = 16ULL}))
    {
        gui_work_close(args);
        return 1;
    }
    if (open->key == NULL)
    {
        return 0;
    }
    const GuiWorkCfg laid = EMBED_CALL(work_laid, WorkCtx, .work = args);
    return EMBED_CALL(work_answered, WorkCtx, .work = args, .answer = open->key(&laid));
}

int gui_work_text(const GuiWorkCfg *args)
{
    const GuiWorkTable *const open = args->work->open;
    if (open == NULL)
    {
        return 0;
    }
    if (open->text == NULL)
    {
        return 1;
    }
    const GuiWorkCfg laid = EMBED_CALL(work_laid, WorkCtx, .work = args);
    return EMBED_CALL(work_answered, WorkCtx, .work = args, .answer = open->text(&laid));
}

int gui_work_menu(const GuiWorkCfg *args)
{
    const GuiWorkTable *const open = args->work_read->open;
    if ((open == NULL) || (open->menu == NULL))
    {
        return 0;
    }
    const GuiWorkCfg laid = EMBED_CALL(work_laid, WorkCtx, .work = args);
    return open->menu(&laid);
}
