// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// work_test.c: the check of the work host: with none open nothing is drawn, hit or taken; a table opened is turned the
// wheel of no rows, draws in the frame's editor rectangle alone, answers the hits on the editor with its index and on
// the map with none, and takes the presses, the wheel, the keys and the text there, a press it answers closing closing
// it; Escape closes it; and a second table opened closes the first
#include "../../../../../ui/cli/src/view/parts/work.h"
#include "../cli/cli_check.h"

// what the check's tables were asked: how many draws, wheels and closes, the last wheel's rows, and the last press's,
// key's and text's
typedef struct
{
    unsigned int draws;
    unsigned int wheels;
    unsigned int closes;
    int rows;
    unsigned int pressed;
    const char *key;
    unsigned long long typed;
} WorkSeen;

static WorkSeen s_seen[2];

// the check's table `at`, from the table's name
static unsigned int seen_at(const GuiWorkCfg *args)
{
    return (args->table->name[0] == 's') ? 1u : 0u;
}

static void check_draw(const GuiWorkCfg *args)
{
    s_seen[seen_at(args)].draws += 1u;
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = args->raster, .rect = args->rect,
               .color = args->palette->text);
}

// the editor's left half is the control 7, its right half none of them
static int check_hit(const GuiWorkCfg *args)
{
    if (!EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->rect, .x = args->x, .y = args->y) ||
        (args->x >= args->rect.x + (args->rect.width / 2)))
    {
        return 0;
    }
    *args->index_out = 7u;
    return 1;
}

// the control 9 closes the view; any other is taken
static unsigned int check_press(const GuiWorkCfg *args)
{
    s_seen[seen_at(args)].pressed = args->index;
    return (args->index == 9u) ? GUI_WORK_CLOSES : GUI_WORK_TAKEN;
}

static unsigned int check_wheel(const GuiWorkCfg *args)
{
    s_seen[seen_at(args)].wheels += 1u;
    s_seen[seen_at(args)].rows = args->rows;
    args->work->scroll = 3u;
    return GUI_WORK_TAKEN;
}

// F7 is taken, any other key passes on
static unsigned int check_key(const GuiWorkCfg *args)
{
    s_seen[seen_at(args)].key = args->name;
    return (args->name[0] == 'F') ? GUI_WORK_TAKEN : GUI_WORK_PASSED;
}

static unsigned int check_text(const GuiWorkCfg *args)
{
    s_seen[seen_at(args)].typed = args->length;
    return GUI_WORK_TAKEN;
}

static void check_close(const GuiWorkCfg *args)
{
    s_seen[seen_at(args)].closes += 1u;
}

static const GuiWorkTable s_first = {
    .name = "first",
    .draw = check_draw,
    .hit = check_hit,
    .press = check_press,
    .wheel = check_wheel,
    .key = check_key,
    .text = check_text,
    .close = check_close,
};

static const GuiWorkTable s_second = {
    .name = "second",
    .draw = check_draw,
    .hit = check_hit,
    .press = check_press,
    .wheel = check_wheel,
    .close = check_close,
};

// whether any pixel of `rect` in the raster differs from `ground`
static int rect_touched(const GuiRaster *raster, GuiRect rect, GuiColor ground)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            // the rectangle stands inside the raster
            if (raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] != ground)
            {
                return 1;
            }
        }
    }
    return 0;
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    const GuiPalette *const palette = EMBED_CALL(cli_palette.palette, GuiPaletteCfg, .scheme = GUI_SCHEME_DARK);
    GuiFrame frame = {0};
    EMBED_CALL(gui_frame_scale.layout, GuiFrameScaleCfg, .frame = &frame, .width = 1440u, .height = 920u, .scale = 100);
    EMBED_CALL(gui_frame_displayed.side, GuiFrameDisplayedCfg, .frame = &frame, .displayed = 1);
    GuiRaster raster;
    cli_check(
        EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &raster, .width = 1440u, .height = 920u),
        "the raster is taken");
    const GuiColor ground = 0xFF102030u;
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster,
               .rect = (GuiRect){0, 0, 1440, 920}, .color = ground);
    const int left_x = frame.editor.x + 10;
    const int right_x = frame.editor.x + frame.editor.width - 10;
    const int middle_y = frame.editor.y + (frame.editor.height / 2);

    GuiWork work;
    EMBED_CALL(gui_work.init, GuiWorkCfg, .work = &work);
    cli_check(!EMBED_CALL(gui_work.is_open, GuiWorkCfg, .work_read = &work), "the host begins with none open");
    EMBED_CALL(gui_work.draw, GuiWorkCfg, .work_read = &work, .frame = &frame, .raster = &raster, .palette = palette);
    cli_check(!rect_touched(&raster, frame.editor, ground), "with none open nothing is drawn");
    cli_check(EMBED_CALL(gui_work.hit, GuiWorkCfg, .work_read = &work, .frame = &frame, .x = left_x, .y = middle_y) ==
                  GUI_WORK_OUTSIDE,
              "with none open no pixel is the view's");
    cli_check(!EMBED_CALL(gui_work.key, GuiWorkCfg, .work = &work, .frame = &frame, .name = "Escape") &&
                  !EMBED_CALL(gui_work.text, GuiWorkCfg, .work = &work, .frame = &frame, .length = 1ULL) &&
                  !EMBED_CALL(gui_work.wheel, GuiWorkCfg, .work = &work, .frame = &frame, .x = left_x, .y = middle_y,
                              .rows = 1),
              "with none open no key, text or wheel is taken");

    EMBED_CALL(gui_work.open, GuiWorkCfg, .work = &work, .table = &s_first, .frame = &frame);
    cli_check(EMBED_CALL(gui_work.is_open, GuiWorkCfg, .work_read = &work) && (s_seen[0].wheels == 1u) &&
                  (s_seen[0].rows == 0) && (work.scroll == 3u),
              "a table opened is turned the wheel of no rows");
    EMBED_CALL(gui_work.draw, GuiWorkCfg, .work_read = &work, .frame = &frame, .raster = &raster, .palette = palette);
    cli_check((s_seen[0].draws == 1u) && rect_touched(&raster, frame.editor, ground), "the view draws in the editor");
    cli_check(!rect_touched(&raster, frame.side, ground) && !rect_touched(&raster, frame.tabs, ground),
              "and nowhere in the side or the tabs");
    cli_check(EMBED_CALL(gui_work.hit, GuiWorkCfg, .work_read = &work, .frame = &frame, .x = left_x, .y = middle_y) ==
                  7u,
              "a pixel on a control of the view hits it");
    cli_check(EMBED_CALL(gui_work.hit, GuiWorkCfg, .work_read = &work, .frame = &frame, .x = right_x,
                         .y = middle_y) == GUI_WORK_NOTHING,
              "a pixel of the view on none of its controls hits nothing of it");
    cli_check(EMBED_CALL(gui_work.hit, GuiWorkCfg, .work_read = &work, .frame = &frame,
                         .x = frame.map.x + (frame.map.width / 2), .y = middle_y) == GUI_WORK_NOTHING,
              "a pixel on the map is the view's, on nothing of it");
    cli_check(EMBED_CALL(gui_work.hit, GuiWorkCfg, .work_read = &work, .frame = &frame, .x = frame.side.x + 4,
                         .y = middle_y) == GUI_WORK_OUTSIDE,
              "a pixel in the side is not the view's");
    cli_check(EMBED_CALL(gui_work.press, GuiWorkCfg, .work = &work, .frame = &frame, .index = 7u) &&
                  (s_seen[0].pressed == 7u) && EMBED_CALL(gui_work.is_open, GuiWorkCfg, .work_read = &work),
              "a press on a control reaches the view");
    cli_check(EMBED_CALL(gui_work.press, GuiWorkCfg, .work = &work, .frame = &frame, .index = GUI_WORK_NOTHING) &&
                  (s_seen[0].pressed == 7u),
              "a press on nothing of the view is taken and reaches nothing");
    cli_check(EMBED_CALL(gui_work.wheel, GuiWorkCfg, .work = &work, .frame = &frame, .x = left_x, .y = middle_y,
                         .rows = 2) &&
                  (s_seen[0].rows == 2),
              "the wheel over the editor reaches the view");
    cli_check(!EMBED_CALL(gui_work.wheel, GuiWorkCfg, .work = &work, .frame = &frame, .x = frame.side.x + 4,
                          .y = middle_y, .rows = 5) &&
                  (s_seen[0].rows == 2),
              "the wheel over the side does not");
    cli_check(EMBED_CALL(gui_work.key, GuiWorkCfg, .work = &work, .frame = &frame, .name = "F7") &&
                  !EMBED_CALL(gui_work.key, GuiWorkCfg, .work = &work, .frame = &frame, .name = "Mod+S"),
              "a key the view takes is taken, one it passes on is not");
    cli_check(EMBED_CALL(gui_work.text, GuiWorkCfg, .work = &work, .frame = &frame, .text = "ab", .length = 2ULL) &&
                  (s_seen[0].typed == 2ULL),
              "text reaches the view");
    cli_check(EMBED_CALL(gui_work.menu, GuiWorkCfg, .work_read = &work, .frame = &frame, .index = 7u) == 0,
              "a view with no menu builds none");

    EMBED_CALL(gui_work.open, GuiWorkCfg, .work = &work, .table = &s_second, .frame = &frame);
    cli_check((s_seen[0].closes == 1u) && (s_seen[1].wheels == 1u) && (work.open == &s_second),
              "a second table opened closes the first");
    cli_check(EMBED_CALL(gui_work.press, GuiWorkCfg, .work = &work, .frame = &frame, .index = 9u) &&
                  (s_seen[1].closes == 1u) && !EMBED_CALL(gui_work.is_open, GuiWorkCfg, .work_read = &work),
              "a press the view answers closing closes it");
    EMBED_CALL(gui_work.open, GuiWorkCfg, .work = &work, .table = &s_first, .frame = &frame);
    cli_check(EMBED_CALL(gui_work.key, GuiWorkCfg, .work = &work, .frame = &frame, .name = "Escape") &&
                  (s_seen[0].closes == 2u) && !EMBED_CALL(gui_work.is_open, GuiWorkCfg, .work_read = &work) &&
                  (work.scroll == 0u),
              "Escape closes the view");

    g_cli_guard->persistent_buf_release(raster.pixels, g_gui_raster_owner);
    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    return cli_on_check();
}
