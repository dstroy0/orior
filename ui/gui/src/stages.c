// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// stages.c: the claim's movement made once: every stage's frame, holes, map and titles laid as a slide is sent, sized
// in one pass and written in a second into one cell, and read by index for the rest of the slide
#include "stages.h"

#include "../../cli/src/command_line/cli_internal.h"

// the slats' shares and their reveals as last read, which the numbers run on from
typedef struct
{
    int grown[3];
    int last[3];
} StagesSlats;

typedef struct
{
    const GuiAutohide *autohide_read;
    GuiAutohide *claim;
    const StagesSlats *panel_bands;
    unsigned int at;
    unsigned int count;
    unsigned long long time_ms;
    unsigned long long end_ms;
    GuiFrame *laid;
    GuiBase *base;
    const GuiPalette *palette;
    GuiMenubar *menubar;
    GuiGlyphs *glyphs;
    unsigned long long moment;
} StagesCtx;
EMBED_INLINE StagesSlats stages_bands_read(const StagesCtx *args)
{
    const StagesSlats panel_bands = {
        {args->autohide_read->grown_top, args->autohide_read->grown_left, args->autohide_read->grown_bottom},
        {args->autohide_read->last_top, args->autohide_read->last_left, args->autohide_read->last_bottom}};
    return panel_bands;
}

EMBED_INLINE void stages_bands_append(const StagesCtx *args)
{
    args->claim->grown_top = args->panel_bands->grown[0];
    args->claim->grown_left = args->panel_bands->grown[1];
    args->claim->grown_bottom = args->panel_bands->grown[2];
    args->claim->last_top = args->panel_bands->last[0];
    args->claim->last_left = args->panel_bands->last[1];
    args->claim->last_bottom = args->panel_bands->last[2];
}

// the moment the stage `at` of `count` stands: `now` and a stage's time on from it, the last when the slides rest
EMBED_INLINE unsigned long long stages_moment(const StagesCtx *args)
{
    return (args->at + 1u == args->count) ? args->end_ms
                                          : args->time_ms + ((unsigned long long)args->at * GUI_STAGE_MS);
}

// `laid` at the claim's numbers at `moment`, and the base and the menu bar laid for it
EMBED_INLINE void stages_one_layout(const StagesCtx *args)
{
    GuiAutohide *claim = args->claim;
    GuiBase *base = args->base;
    const GuiPalette *palette = args->palette;
    GuiMenubar *menubar = args->menubar;
    GuiGlyphs *glyphs = args->glyphs;
    GuiAutohideNumbers numbers;
    EMBED_CALL(gui_autohide_state.numbers, GuiAutohideStateCfg, .claim = claim, .time_ms = args->moment,
               .numbers = &numbers);
    EMBED_CALL(gui_frame.claim, GuiFrameCfg, .frame = args->laid, .numbers = &numbers);
    (void)EMBED_CALL(gui_base_frame.layout, GuiBaseFrameCfg, .base = base, .frame = args->laid, .palette = palette,
                     .controls_steady = args->laid->controls_pane_share >= GUI_FIXED_ONE);
    EMBED_CALL(gui_menubar_bar.titles_layout, GuiMenubarBarCfg, .menubar = menubar, .glyphs = glyphs,
               .frame = args->laid);
}

int gui_stages_init(const GuiStagesAutohideCfg *args)
{
    GuiAutohide *claim = args->claim;
    const GuiFrame *frame = args->frame;
    gui_stages_autohide.free(args);
    const unsigned long long end_ms =
        EMBED_CALL(gui_autohide.animation_end_ms, GuiAutohideCfg, .autohide_read = claim, .time_ms = args->time_ms);
    // the stages are the slide's time over a stage's, and one for the end: far fewer than an int counts
    const unsigned int count = (unsigned int)((end_ms - args->time_ms + GUI_STAGE_MS - 1ull) / GUI_STAGE_MS) + 1u;
    const StagesSlats from = EMBED_CALL(stages_bands_read, StagesCtx, .autohide_read = claim);
    GuiFrame laid = *frame;
    unsigned long long words = 0ull;
    unsigned long long runs = 0ull;
    unsigned long long titles = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(stages_one_layout, StagesCtx, .laid = &laid, .claim = claim, .base = args->base,
                   .palette = args->palette, .menubar = args->menubar, .glyphs = args->glyphs,
                   .moment = EMBED_CALL(stages_moment, StagesCtx, .at = at, .count = count, .time_ms = args->time_ms,
                                        .end_ms = end_ms));
        words += (2ull * (unsigned long long)args->base->bands) + 2ull;
        runs += args->base->run_first[args->base->bands];
        titles += args->menubar->titles;
    }
    const unsigned long long stage_bytes = (unsigned long long)count * sizeof(GuiStage);
    const unsigned long long word_bytes = words * sizeof(unsigned int);
    const unsigned long long run_bytes = runs * sizeof(GuiBaseRun);
    const unsigned long long title_bytes = titles * sizeof(GuiTitle);
    // the runs and the titles lie on the stages' alignment, the words being padded up to it
    const unsigned long long word_capacity = (word_bytes + 7ull) & ~7ull;
    unsigned char *const cell =
        g_cli_cells->persistent_buf_alloc(stage_bytes + word_capacity + run_bytes + title_bytes, &g_gui_cells);
    EMBED_CALL(stages_bands_append, StagesCtx, .claim = claim, .panel_bands = &from);
    if (cell == NULL)
    {
        return 0;
    }
    GuiStage *const stage = (GuiStage *)cell;
    unsigned int *word = (unsigned int *)(cell + stage_bytes);
    GuiBaseRun *run = (GuiBaseRun *)(cell + stage_bytes + word_capacity);
    GuiTitle *title = (GuiTitle *)(cell + stage_bytes + word_capacity + run_bytes);
    laid = *frame;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        EMBED_CALL(stages_one_layout, StagesCtx, .laid = &laid, .claim = claim, .base = args->base,
                   .palette = args->palette, .menubar = args->menubar, .glyphs = args->glyphs,
                   .moment = EMBED_CALL(stages_moment, StagesCtx, .at = at, .count = count, .time_ms = args->time_ms,
                                        .end_ms = end_ms));
        GuiStage *const one = &stage[at];
        one->frame = laid;
        one->hole_count = EMBED_CALL(cli_frame_holes.holes, GuiFrameHolesCfg, .frame_read = &laid, .holes = one->holes);
        const unsigned int bands = args->base->bands;
        const unsigned int band_runs = args->base->run_first[bands];
        memor.cpy(&(MemoriaCfg){.dst = word,
                                .src = args->base->band_from,
                                .bytes = ((unsigned long long)bands + 1ull) * sizeof(unsigned int)});
        memor.cpy(&(MemoriaCfg){.dst = word + bands + 1u,
                                .src = args->base->run_first,
                                .bytes = ((unsigned long long)bands + 1ull) * sizeof(unsigned int)});
        memor.cpy(&(MemoriaCfg){
            .dst = run, .src = args->base->run, .bytes = (unsigned long long)band_runs * sizeof(GuiBaseRun)});
        memor.cpy(&(MemoriaCfg){.dst = title,
                                .src = args->menubar->title,
                                .bytes = (unsigned long long)args->menubar->titles * sizeof(GuiTitle)});
        one->map = (GuiBaseMap){bands, word, word + bands + 1u, run};
        one->title = title;
        one->titles = args->menubar->titles;
        one->folded_from = args->menubar->folded_from;
        word += (2u * bands) + 2u;
        run += band_runs;
        title += args->menubar->titles;
    }
    args->stages->stage = stage;
    args->stages->count = count;
    args->stages->start_ms = args->time_ms;
    args->stages->end_ms = end_ms;
    args->stages->created_for = claim->sent;
    return 1;
}

const GuiStage *gui_stages_at(const GuiStagesReadCfg *args)
{
    if (args->stages_read->stage == NULL)
    {
        return NULL;
    }
    if (args->time_ms >= args->stages_read->end_ms)
    {
        return &args->stages_read->stage[args->stages_read->count - 1u];
    }
    // a moment before the first stage is the first's, and a stage's index is far fewer than an int counts
    return &args->stages_read->stage[(args->time_ms > args->stages_read->start_ms)
                                         ? (unsigned int)((args->time_ms - args->stages_read->start_ms) / GUI_STAGE_MS)
                                         : 0u];
}

void gui_stages_free(const GuiStagesAutohideCfg *args)
{
    g_cli_cells->persistent_buf_release(args->stages->stage, &g_gui_cells);
    args->stages->stage = NULL;
    args->stages->count = 0u;
}

void gui_stage_autohide_alloc(const GuiStagesCfg *args)
{
    const GuiAutohideNumbers *const numbers = &args->stage->frame.claim;
    const StagesSlats panel_bands = {{numbers->grown_top, numbers->grown_left, numbers->grown_bottom},
                                     {numbers->reveal_top, numbers->reveal_left, numbers->reveal_bottom}};
    EMBED_CALL(stages_bands_append, StagesCtx, .claim = args->claim, .panel_bands = &panel_bands);
}
