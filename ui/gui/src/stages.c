// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// stages.c: the claim's movement made once: every stage's frame, holes, map and titles laid as a slide is sent, sized in
// one pass and written in a second into one cell, and read by index for the rest of the slide
#include "stages.h"

#include "../../cli/src/command_line/cli_internal.h"

// the slats' shares and their reveals as last read, which the numbers run on from
typedef struct
{
    int grown[3];
    int last[3];
} StagesSlats;

static StagesSlats stages_slats_read(const GuiClaim *claim)
{
    const StagesSlats slats = {{claim->grown_top, claim->grown_left, claim->grown_bottom}, {claim->last_top, claim->last_left, claim->last_bottom}};
    return slats;
}

static void stages_slats_put(GuiClaim *claim, const StagesSlats *slats)
{
    claim->grown_top = slats->grown[0];
    claim->grown_left = slats->grown[1];
    claim->grown_bottom = slats->grown[2];
    claim->last_top = slats->last[0];
    claim->last_left = slats->last[1];
    claim->last_bottom = slats->last[2];
}

// the moment the stage `at` of `count` stands: `now` and a stage's time on from it, the last when the slides rest
static unsigned long long stages_moment(unsigned int at, unsigned int count, unsigned long long now, unsigned long long rests)
{
    return (at + 1u == count) ? rests : now + ((unsigned long long)at * GUI_STAGE_MS);
}

// `laid` at the claim's numbers at `moment`, and the base and the menu bar laid for it
static void stages_one_laid(GuiFrame *laid, GuiClaim *claim, GuiBase *base, const GuiPalette *palette, GuiMenubar *menubar, GuiGlyphs *glyphs,
                            unsigned long long moment)
{
    GuiClaimNumbers numbers;
    gui_claim_numbers(claim, moment, &numbers);
    gui_frame_claimed(laid, &numbers);
    (void)gui_base_laid(base, laid, palette, laid->controls_pane_share >= GUI_WHOLE);
    gui_menubar_titles_laid(menubar, glyphs, laid);
}

int gui_stages_made(GuiStages *stages, GuiClaim *claim, const GuiFrame *frame, GuiBase *base, const GuiPalette *palette,
                    GuiMenubar *menubar, GuiGlyphs *glyphs, unsigned long long now)
{
    gui_stages_given(stages);
    const unsigned long long rests = gui_claim_rests_at(claim, now);
    // the stages are the slide's time over a stage's, and one for where it rests: far fewer than an int counts
    const unsigned int count = (unsigned int)((rests - now + GUI_STAGE_MS - 1ull) / GUI_STAGE_MS) + 1u;
    const StagesSlats from = stages_slats_read(claim);
    GuiFrame laid = *frame;
    unsigned long long words = 0ull;
    unsigned long long runs = 0ull;
    unsigned long long titles = 0ull;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        stages_one_laid(&laid, claim, base, palette, menubar, glyphs, stages_moment(at, count, now, rests));
        words += (2ull * (unsigned long long)base->bands) + 2ull;
        runs += base->run_first[base->bands];
        titles += menubar->titles;
    }
    const unsigned long long stage_bytes = (unsigned long long)count * sizeof(GuiStage);
    const unsigned long long word_bytes = words * sizeof(unsigned int);
    const unsigned long long run_bytes = runs * sizeof(GuiBaseRun);
    const unsigned long long title_bytes = titles * sizeof(GuiTitle);
    // the runs and the titles lie on the stages' alignment, the words being padded up to it
    const unsigned long long word_room = (word_bytes + 7ull) & ~7ull;
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(stage_bytes + word_room + run_bytes + title_bytes, &g_gui_cells);
    stages_slats_put(claim, &from);
    if (cell == NULL)
    {
        return 0;
    }
    GuiStage *const stage = (GuiStage *)cell;
    unsigned int *word = (unsigned int *)(cell + stage_bytes);
    GuiBaseRun *run = (GuiBaseRun *)(cell + stage_bytes + word_room);
    GuiTitle *title = (GuiTitle *)(cell + stage_bytes + word_room + run_bytes);
    laid = *frame;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        stages_one_laid(&laid, claim, base, palette, menubar, glyphs, stages_moment(at, count, now, rests));
        GuiStage *const one = &stage[at];
        one->frame = laid;
        one->hole_count = gui_frame_holes(&laid, one->holes);
        const unsigned int bands = base->bands;
        const unsigned int band_runs = base->run_first[bands];
        memor.cpy(&(MemoriaCfg){.dst = word, .src = base->band_from, .bytes = ((unsigned long long)bands + 1ull) * sizeof(unsigned int)});
        memor.cpy(&(MemoriaCfg){.dst = word + bands + 1u, .src = base->run_first, .bytes = ((unsigned long long)bands + 1ull) * sizeof(unsigned int)});
        memor.cpy(&(MemoriaCfg){.dst = run, .src = base->run, .bytes = (unsigned long long)band_runs * sizeof(GuiBaseRun)});
        memor.cpy(&(MemoriaCfg){.dst = title, .src = menubar->title, .bytes = (unsigned long long)menubar->titles * sizeof(GuiTitle)});
        one->map = (GuiBaseMap){bands, word, word + bands + 1u, run};
        one->title = title;
        one->titles = menubar->titles;
        one->folded_from = menubar->folded_from;
        word += (2u * bands) + 2u;
        run += band_runs;
        title += menubar->titles;
    }
    stages->stage = stage;
    stages->count = count;
    stages->began = now;
    stages->rests = rests;
    stages->made_for = claim->sent;
    return 1;
}

const GuiStage *gui_stages_at(const GuiStages *stages, unsigned long long now)
{
    if (stages->stage == NULL)
    {
        return NULL;
    }
    if (now >= stages->rests)
    {
        return &stages->stage[stages->count - 1u];
    }
    // a moment before the first stage is the first's, and a stage's index is far fewer than an int counts
    return &stages->stage[(now > stages->began) ? (unsigned int)((now - stages->began) / GUI_STAGE_MS) : 0u];
}

void gui_stages_given(GuiStages *stages)
{
    g_cli_cells->persistent_buf_release(stages->stage, &g_gui_cells);
    stages->stage = NULL;
    stages->count = 0u;
}

void gui_stage_claim_taken(const GuiStage *stage, GuiClaim *claim)
{
    const GuiClaimNumbers *const numbers = &stage->frame.claim;
    const StagesSlats slats = {{numbers->grown_top, numbers->grown_left, numbers->grown_bottom}, {numbers->reveal_top, numbers->reveal_left, numbers->reveal_bottom}};
    stages_slats_put(claim, &slats);
}
