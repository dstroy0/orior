// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// button.c: a group of buttons as beads: the atlas's pixels the caller's, its colors from the palette, and each
// button's press a slide of the claim's, the press on the ease-out and the release on the bead's spring
#include "button.h"

// the press slide's rest, so the release's wobble below rest stays above 0
#define BUTTONS_PRESS_REST GUI_FIXED_ONE

typedef struct
{
    GuiButtons *buttons;
    unsigned long long time_ms;
    int press;
    unsigned int curve;
    unsigned int tool;
} ButtonsCtx;

void gui_buttons_init(const GuiButtonsCfg *args)
{
    GuiButtons *const buttons = args->buttons;
    EMBED_CALL(gui_bead_bake_table.init, GuiBeadBakeCfg, .atlas = &buttons->atlas);
    buttons->pressed = GUI_BEAD_TOOLS_MAX;
    for (unsigned int tool = 0u; tool < GUI_BEAD_TOOLS_MAX; tool += 1u)
    {
        buttons->press[tool] = (GuiSlide){.from = BUTTONS_PRESS_REST, .to = BUTTONS_PRESS_REST, .curve = GUI_CURVE_BEAD};
    }
}

unsigned long long gui_buttons_bytes(const GuiButtonsBakeCfg *args)
{
    return EMBED_CALL(gui_bead_bake_table.bytes, GuiBeadBakeCfg, .width = args->width, .height = args->height,
                      .pad = args->pad, .reach = args->reach, .tools = args->tools);
}

int gui_buttons_bake(const GuiButtonsBakeCfg *args)
{
    const GuiColor colors[GUI_BEAD_COLORS] = {args->palette->bead_glint, args->palette->bead_caustic,
                                              args->palette->bead_rim, args->palette->bead_shadow};
    return EMBED_CALL(gui_bead_bake_table.bake, GuiBeadBakeCfg, .atlas = &args->buttons->atlas,
                      .pixels = args->pixels, .colors = colors, .width = args->width,
                      .height = args->height, .shape = args->shape, .radius = args->radius, .pad = args->pad,
                      .reach = args->reach, .cells = args->cells, .tools = args->tools, .lights = args->lights,
                      .light_count = args->light_count);
}

// how long a press slide lasts: the press's time toward pressed and the release's back to rest
EMBED_INLINE unsigned long long buttons_press_lasts_ms(const GuiSlide *press)
{
    return (press->to == BUTTONS_PRESS_REST) ? GUI_BUTTONS_RELEASE_MS : GUI_BUTTONS_PRESS_MS;
}

// The button `tool`'s press slide sent to `press` at `time_ms` on `curve` from where it stands. A release while the
// press still runs waits for the press to end, so a quick click shows the whole press and the spring back
EMBED_INLINE void buttons_press_send(const ButtonsCtx *args)
{
    GuiSlide *const press = &args->buttons->press[args->tool];
    if (press->to == args->press)
    {
        return;
    }
    const unsigned long long lasts_ms = buttons_press_lasts_ms(press);
    const unsigned long long end_ms = press->start_ms + lasts_ms;
    const int running = (press->from != press->to) && (args->time_ms < end_ms);
    const int pressing = running && (press->to == BUTTONS_PRESS_REST + GUI_FIXED_ONE);
    const int now = EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg, .slide = press,
                               .time_ms = args->time_ms, .lasts_ms = lasts_ms);
    press->from = pressing ? press->to : now;
    press->to = args->press;
    press->start_ms = pressing ? end_ms : args->time_ms;
    press->velocity = 0;
    press->curve = args->curve;
}

void gui_buttons_press(const GuiButtonsMoveCfg *args)
{
    if (args->tool >= GUI_BEAD_TOOLS_MAX)
    {
        return;
    }
    // a button held while another is pressed is let go first
    gui_buttons_release(args);
    args->buttons->pressed = args->tool;
    EMBED_CALL(buttons_press_send, ButtonsCtx, .buttons = args->buttons, .tool = args->tool,
               .press = BUTTONS_PRESS_REST + GUI_FIXED_ONE, .curve = GUI_CURVE_EASE_OUT, .time_ms = args->time_ms);
}

void gui_buttons_release(const GuiButtonsMoveCfg *args)
{
    GuiButtons *const buttons = args->buttons;
    if (buttons->pressed >= GUI_BEAD_TOOLS_MAX)
    {
        return;
    }
    // the release springs back on the bead's curve, past rest and back
    EMBED_CALL(buttons_press_send, ButtonsCtx, .buttons = buttons, .tool = buttons->pressed,
               .press = BUTTONS_PRESS_REST, .curve = GUI_CURVE_BEAD, .time_ms = args->time_ms);
    buttons->pressed = GUI_BEAD_TOOLS_MAX;
}

unsigned long long gui_buttons_end_ms(const GuiButtonsCfg *args)
{
    const GuiButtons *const buttons = args->buttons_read;
    unsigned long long latest = 0ULL;
    for (unsigned int tool = 0u; tool < buttons->atlas.tools; tool += 1u)
    {
        const unsigned long long end = EMBED_CALL(gui_autohide_slide.slide_end_ms, GuiAutohideSlideCfg,
                                                  .slide = &buttons->press[tool], .time_ms = args->time_ms,
                                                  .lasts_ms = buttons_press_lasts_ms(&buttons->press[tool]));
        latest = (end > latest) ? end : latest;
    }
    return latest;
}

void gui_buttons_draw(const GuiButtonsDrawCfg *args)
{
    GuiButtons *const buttons = args->buttons;
    if (!buttons->atlas.made || (args->tool >= buttons->atlas.tools))
    {
        return;
    }
    const GuiSlide *const press = &buttons->press[args->tool];
    const int pressed = EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg, .slide = press,
                                   .time_ms = args->time_ms, .lasts_ms = buttons_press_lasts_ms(press));
    EMBED_CALL(gui_bead_blit_table.blit, GuiBeadBlitCfg, .atlas = &buttons->atlas, .raster = args->raster,
               .x = args->x, .y = args->y, .tool = args->tool,
               .frame = EMBED_CALL(gui_bead.frame, GuiBeadCfg, .press = pressed - BUTTONS_PRESS_REST,
                                   .one = GUI_FIXED_ONE),
               .opacity = (args->fade >= 255u) ? 0u : 255u - args->fade, .clip = args->clip);
}
