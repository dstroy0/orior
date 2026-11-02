// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// busy_test.c: the busy circle's check: asks answered in any order, the wait before it comes and the least it stands, a
// stage read by the clock, the burn's five pixels at the ring's head a quarter turn apart a quarter of the stages apart, ash
// ahead of the fire and the trail hot behind it, the burn pulsing and dithered, a step restoring what lay under the circle
// pixel for pixel, and one stage while it stands still
#include "../../../../../ui/gui/src/parts/busy.h"
#include "../cli/cli_check.h"

#include <math.h>

#define GROUND GUI_RGB(0x13u, 0x13u, 0x31u)
#define PICTURE_SIDE 64u

// the pixel of the circle's picture at x, y from its top left
static GuiColor at(const GuiRaster *raster, GuiRect rect, int x, int y)
{
    // the place lies on the raster in every check here
    return raster->pixels[((unsigned int)(rect.y + y) * raster->width) + (unsigned int)(rect.x + x)];
}

// the raster laid back to the ground, every pixel
static void grounded(GuiRaster *raster)
{
    for (unsigned int pixel = 0u; pixel < raster->width * raster->height; pixel += 1u)
    {
        raster->pixels[pixel] = GROUND;
    }
}

// the pixels of the circle's rectangle that are the fuse's core color or its fire, and where the first of the core lies
static unsigned int pixels_of(const GuiRaster *raster, GuiRect rect, GuiColor color, int *x, int *y)
{
    unsigned int count = 0u;
    for (int row = 0; row < rect.height; row += 1)
    {
        for (int column = 0; column < rect.width; column += 1)
        {
            if (at(raster, rect, column, row) == color)
            {
                if (count == 0u)
                {
                    *x = column;
                    *y = row;
                }
                count += 1u;
            }
        }
    }
    return count;
}

// the sum of a color's channels: how bright it stands
static unsigned int bright(GuiColor color)
{
    return GUI_RED_OF(color) + GUI_GREEN_OF(color) + GUI_BLUE_OF(color);
}

// whether a pixel is the burn's: the fuse's fire or its white
static int burning(const GuiPalette *palette, GuiColor color)
{
    return (color == palette->fuse_fire) || (color == palette->fuse_core);
}

// the burn's middle where its five pixels are two lines of three crossing: a burning pixel with one burning on each side of it
static int burn_middle(const GuiRaster *raster, GuiRect rect, const GuiPalette *palette, int *x, int *y)
{
    for (int row = 1; row + 1 < rect.height; row += 1)
    {
        for (int column = 1; column + 1 < rect.width; column += 1)
        {
            if (burning(palette, at(raster, rect, column, row)) && burning(palette, at(raster, rect, column - 1, row)) &&
                burning(palette, at(raster, rect, column + 1, row)) && burning(palette, at(raster, rect, column, row - 1)) &&
                burning(palette, at(raster, rect, column, row + 1)))
            {
                *x = column;
                *y = row;
                return 1;
            }
        }
    }
    return 0;
}

// the stage's picture laid on a raster of the ground
static void stage_laid(GuiBusy *busy, GuiRaster *raster, const GuiPalette *palette, const GuiFrame *frame, unsigned int stage)
{
    grounded(raster);
    gui_busy_drawn(busy, raster, palette, frame, busy->shown_ms + ((unsigned long long)stage * GUI_BUSY_STAGE_MS));
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    const GuiPalette *const palette = gui_palette(GUI_SCHEME_DARK);
    static GuiBusy busy;
    gui_busy_made(&busy);

    // the asks
    unsigned long long since = 0ull;
    cli_check(!gui_busy_waiting(&busy, &since), "nothing asked, nothing waits");
    gui_busy_asked(&busy, 1ull, 1000ull);
    gui_busy_asked(&busy, 2ull, 1010ull);
    gui_busy_asked(&busy, 3ull, 1020ull);
    cli_check(gui_busy_waiting(&busy, &since) && (since == 1000ull), "the oldest ask is the one that waits longest");
    gui_busy_answered(&busy, 2ull);
    cli_check(gui_busy_waiting(&busy, &since) && (since == 1000ull), "one answered behind it changes nothing");
    gui_busy_answered(&busy, 1ull);
    cli_check(gui_busy_waiting(&busy, &since) && (since == 1020ull), "the oldest answered, the next unanswered is the oldest, the one answered out of turn passed over");
    gui_busy_answered(&busy, 3ull);
    cli_check(!gui_busy_waiting(&busy, &since), "every ask answered, nothing waits");
    gui_busy_answered(&busy, 3ull);
    gui_busy_answered(&busy, 900ull);
    cli_check(!gui_busy_waiting(&busy, &since), "an answer twice over or to no ask counts for nothing");

    // work the CLI reports running waits like an ask, from when it began until it ends, the oldest of them waiting longest
    gui_busy_work(&busy, 1, 1100ull);
    cli_check(gui_busy_waiting(&busy, &since) && (since == 1100ull), "work running waits as an ask does");
    gui_busy_work(&busy, 1, 1500ull);
    cli_check(gui_busy_waiting(&busy, &since) && (since == 1100ull), "still running, it waits from when it began");
    gui_busy_asked(&busy, 4ull, 1050ull);
    cli_check(gui_busy_waiting(&busy, &since) && (since == 1050ull), "an ask older than the work waits longer");
    gui_busy_answered(&busy, 4ull);
    cli_check(gui_busy_waiting(&busy, &since) && (since == 1100ull), "the ask answered, the work still waits");
    gui_busy_work(&busy, 0, 1600ull);
    cli_check(!gui_busy_waiting(&busy, &since), "the work ended, nothing waits");

    // the wait and the least it stands
    cli_check((gui_busy_due(&busy, 2000ull) == 0ull) && !gui_busy_ticked(&busy, 2000ull) && !busy.shown, "nothing asked, no tick is wanted and no circle comes");
    gui_busy_asked(&busy, 5ull, 2000ull);
    cli_check(gui_busy_due(&busy, 2050ull) == GUI_BUSY_WAITS - 50ull, "an ask waiting, a tick is wanted as the wait ends");
    cli_check(!gui_busy_ticked(&busy, 2000ull + GUI_BUSY_WAITS - 1ull) && !busy.shown, "the circle stays away through the wait");
    cli_check(gui_busy_ticked(&busy, 2000ull + GUI_BUSY_WAITS) && busy.shown, "the circle comes as the wait ends");
    const unsigned long long came = 2000ull + GUI_BUSY_WAITS;
    gui_busy_answered(&busy, 5ull);
    cli_check(!gui_busy_ticked(&busy, came + GUI_BUSY_STANDS - 1ull) && busy.shown, "answered at once, it stands its least");
    cli_check(gui_busy_ticked(&busy, came + GUI_BUSY_STANDS) && !busy.shown, "and then goes");
    cli_check(busy.cell == NULL, "its cell is given back");

    // a stage by the clock
    gui_busy_asked(&busy, 6ull, 3000ull);
    (void)gui_busy_ticked(&busy, 3000ull + GUI_BUSY_WAITS);
    const unsigned long long shown = 3000ull + GUI_BUSY_WAITS;
    cli_check(gui_busy_stage_at(&busy, shown) == 0u, "it comes at the first stage");
    cli_check(gui_busy_stage_at(&busy, shown + GUI_BUSY_STAGE_MS - 1ull) == 0u, "a stage stands its time");
    cli_check(gui_busy_stage_at(&busy, shown + GUI_BUSY_STAGE_MS) == 1u, "and the next follows");
    cli_check(gui_busy_stage_at(&busy, shown + (GUI_BUSY_STAGE_MS * GUI_BUSY_STAGES)) == 0u, "the swirl runs round");
    cli_check((gui_busy_due(&busy, shown + 3ull) == GUI_BUSY_STAGE_MS - 3ull), "a tick is wanted as the next stage comes");

    // the picture
    GuiRaster raster;
    cli_check(gui_raster_made(&raster, PICTURE_SIDE, PICTURE_SIDE), "a raster to lay it on");
    GuiFrame frame;
    gui_frame_laid(&frame, 800u, 500u, 1.0f);
    frame.editor = (GuiRect){0, 0, PICTURE_SIDE, PICTURE_SIDE};
    const GuiRect rect = gui_busy_rect(&frame);
    cli_check((rect.width == GUI_BUSY_SIDE) && (rect.height == GUI_BUSY_SIDE), "the circle is its side across at the page's scale");
    cli_check((rect.x + rect.width == (int)PICTURE_SIDE - GUI_BUSY_EDGE) && (rect.y + rect.height == (int)PICTURE_SIDE - GUI_BUSY_EDGE), "it keeps its distance from the editor's corner");
    stage_laid(&busy, &raster, palette, &frame, 0u);
    cli_check(busy.cell != NULL, "the first drawing makes its pictures");
    const int half = GUI_BUSY_SIDE / 2;
    cli_check(at(&raster, rect, half, half) == GROUND, "the ring's middle stands clear");
    cli_check(at(&raster, rect, 0, 0) == GROUND, "its corner stands clear");


    // the burn's middle stands on the ring at each quarter, a quarter turn on for a quarter of the stages on, clockwise from
    // the top
    const int reach = GUI_BUSY_REACH;
    const int quarter_x[4] = {half, half + reach, half, half - 1 - reach};
    const int quarter_y[4] = {half - 1 - reach, half, half + reach, half};
    int quarters_lit = 1;
    for (unsigned int quarter = 0u; quarter < 4u; quarter += 1u)
    {
        stage_laid(&busy, &raster, palette, &frame, quarter * (GUI_BUSY_STAGES / 4u));
        quarters_lit = quarters_lit && burning(palette, at(&raster, rect, quarter_x[quarter], quarter_y[quarter]));
    }
    cli_check(quarters_lit, "the burn's middle stands at each quarter of the ring");

    // across the swirl: five pixels at the most, one of them at the most white hot, and where all five burn they are two lines of
    // three crossing; the burn standing within a pixel of the ring and now and then a pixel off it, its twitch
    unsigned int least_lit = 5u;
    unsigned int most_lit = 0u;
    int never_past_five = 1;
    int shape_kept = 1;
    int within_a_pixel = 1;
    unsigned int twitches = 0u;
    for (unsigned int stage = 0u; stage < GUI_BUSY_STAGES; stage += 1u)
    {
        stage_laid(&busy, &raster, palette, &frame, stage);
        int first_x = 0;
        int first_y = 0;
        const unsigned int core = pixels_of(&raster, rect, palette->fuse_core, &first_x, &first_y);
        const unsigned int lit = core + pixels_of(&raster, rect, palette->fuse_fire, &first_x, &first_y);
        least_lit = (lit < least_lit) ? lit : least_lit;
        most_lit = (lit > most_lit) ? lit : most_lit;
        never_past_five = never_past_five && (lit <= 5u) && (core <= 1u);
        if (lit == 5u)
        {
            int middle_x = 0;
            int middle_y = 0;
            shape_kept = shape_kept && burn_middle(&raster, rect, palette, &middle_x, &middle_y);
        }
        // the white hot pixel, where one stands, is the burn's middle
        if (core == 1u)
        {
            int middle_x = 0;
            int middle_y = 0;
            (void)pixels_of(&raster, rect, palette->fuse_core, &middle_x, &middle_y);
            const double turn = ((double)stage * 6.283185307179586) / (double)GUI_BUSY_STAGES;
            const double off_x = fabs(((double)middle_x + 0.5) - ((double)half + ((double)reach * sin(turn))));
            const double off_y = fabs(((double)middle_y + 0.5) - ((double)half - ((double)reach * cos(turn))));
            within_a_pixel = within_a_pixel && (off_x < 1.55) && (off_y < 1.55);
            twitches += ((off_x > 0.55) || (off_y > 0.55)) ? 1u : 0u;
        }
    }
    cli_check(never_past_five, "across the swirl no more than five pixels burn, no more than one of them white hot");
    cli_check(most_lit == 5u, "at the pulse's height all five burn");
    cli_check(least_lit < most_lit, "at its foot the dither lets some of them go");
    cli_check(shape_kept, "the five are two lines of three crossing");
    cli_check(within_a_pixel && (twitches > 0u), "the burn twitches a pixel off the ring and no further");

    // ash ahead of the fire, the trail hot behind it, cooling back round to the ash; at the first stage the head is at the top and
    // the fire goes clockwise: its right is ahead and its left behind
    stage_laid(&busy, &raster, palette, &frame, 0u);
    const GuiColor ahead = at(&raster, rect, half + 5, half - 5);
    const GuiColor just_behind = at(&raster, rect, half - 6, half - 5);
    const GuiColor quarter_behind = at(&raster, rect, half - 1 - reach, half);
    const GuiColor half_behind = at(&raster, rect, half, half + reach);
    const GuiColor round_behind = at(&raster, rect, half + reach, half);
    cli_check((ahead != GROUND) && (round_behind != GROUND), "the ash shows the ring all the way round");
    cli_check(bright(just_behind) > bright(quarter_behind), "the trail is hottest behind the head");
    cli_check(bright(quarter_behind) > bright(half_behind), "and cools going back");
    cli_check(bright(half_behind) >= bright(round_behind), "as far as the ash");
    cli_check(round_behind == at(&raster, rect, half + reach, half - 1), "the ash is one color past the trail, two pixels the ring covers alike");

    // a step restores what lay under it, pixel for pixel: a frame with a line through it, a step to another stage, the same as
    // the stage laid straight over that frame
    GuiRaster straight;
    cli_check(gui_raster_made(&straight, PICTURE_SIDE, PICTURE_SIDE), "a second raster to lay it on");
    grounded(&raster);
    grounded(&straight);
    gui_raster_line(&raster, 0, rect.y + half, PICTURE_SIDE - 1, rect.y + half, palette->link);
    gui_raster_line(&straight, 0, rect.y + half, PICTURE_SIDE - 1, rect.y + half, palette->link);
    gui_busy_drawn(&busy, &raster, palette, &frame, busy.shown_ms);
    const GuiRect again = gui_busy_redrawn(&busy, &raster, busy.shown_ms + (20ull * GUI_BUSY_STAGE_MS));
    cli_check((again.x == rect.x) && (again.y == rect.y) && (again.width == rect.width) && (again.height == rect.height), "the step answers the circle's rectangle");
    gui_busy_drawn(&busy, &straight, palette, &frame, busy.shown_ms + (20ull * GUI_BUSY_STAGE_MS));
    int same = 1;
    for (unsigned int pixel = 0u; pixel < PICTURE_SIDE * PICTURE_SIDE; pixel += 1u)
    {
        same = same && (raster.pixels[pixel] == straight.pixels[pixel]);
    }
    cli_check(same, "a step lays the same pixels as the stage laid straight over the frame");

    // laid off the raster's edge it is clipped and still restored
    GuiFrame edged = frame;
    edged.editor = (GuiRect){(int)PICTURE_SIDE - 10, (int)PICTURE_SIDE - 10, 30, 30};
    grounded(&raster);
    gui_busy_drawn(&busy, &raster, palette, &edged, busy.shown_ms);
    const GuiRect clipped = gui_busy_redrawn(&busy, &raster, busy.shown_ms + GUI_BUSY_STAGE_MS);
    cli_check(!gui_rect_empty(clipped) && (clipped.x + clipped.width <= (int)PICTURE_SIDE) && (clipped.y + clipped.height <= (int)PICTURE_SIDE), "off the raster's edge it is clipped to the raster");

    // another scale makes the pictures again at its side, even, with the burn's cells grown to fill it
    GuiFrame doubled = frame;
    doubled.scale = 2.0f;
    doubled.editor = (GuiRect){0, 0, PICTURE_SIDE, PICTURE_SIDE};
    cli_check(gui_busy_rect(&doubled).width == 2 * GUI_BUSY_SIDE, "at twice the scale it is twice as wide");
    grounded(&raster);
    gui_busy_drawn(&busy, &raster, palette, &doubled, busy.shown_ms);
    cli_check(busy.side == 2u * GUI_BUSY_SIDE, "and its pictures are made again at that side");
    gui_busy_drawn(&busy, &raster, palette, &frame, busy.shown_ms);
    cli_check(busy.side == GUI_BUSY_SIDE, "and again at the first scale");

    // standing still: the one stage, no tick wanted for it, and made again for the swirl
    const unsigned long long swirling_bytes = busy.cell_bytes;
    gui_busy_still(&busy, 1);
    cli_check((busy.cell == NULL) && busy.shown, "asked to stand still it stays and lets its stages go");
    // the work a check runs brings the circle after the wait and keeps it past the ask's answer
    GuiBusy working;
    gui_busy_made(&working);
    gui_busy_asked(&working, 1ull, 5000ull);
    gui_busy_work(&working, 1, 5010ull);
    gui_busy_answered(&working, 1ull);
    cli_check(!gui_busy_ticked(&working, 5100ull) && !working.shown, "the work stands its wait as an ask does");
    cli_check(gui_busy_ticked(&working, 5010ull + GUI_BUSY_WAITS) && working.shown, "and the circle comes");
    cli_check(!gui_busy_ticked(&working, 9000ull) && working.shown, "it stays as long as the work runs");
    gui_busy_work(&working, 0, 9000ull);
    cli_check(gui_busy_ticked(&working, 9000ull) && !working.shown, "and goes as it ends");
    grounded(&raster);
    gui_busy_drawn(&busy, &raster, palette, &frame, busy.shown_ms);
    cli_check((busy.stages == 1u) && (busy.cell_bytes < swirling_bytes), "its one stage is made");
    cli_check(gui_busy_stage_at(&busy, busy.shown_ms + (9ull * GUI_BUSY_STAGE_MS)) == 0u, "the clock does not move it");
    cli_check(gui_busy_due(&busy, busy.shown_ms + GUI_BUSY_STANDS) == 0ull, "standing with its ask unanswered, no tick is wanted: the answer wakes it");
    gui_busy_answered(&busy, 6ull);
    cli_check(gui_busy_due(&busy, busy.shown_ms + GUI_BUSY_STANDS) == 1ull, "answered, a tick is wanted to take it away");
    cli_check(gui_busy_due(&busy, busy.shown_ms + 1ull) == GUI_BUSY_STANDS - 1ull, "or as its least ends");
    gui_busy_still(&busy, 0);
    grounded(&raster);
    gui_busy_drawn(&busy, &raster, palette, &frame, busy.shown_ms);
    cli_check(busy.stages == GUI_BUSY_STAGES, "swirling again its stages are made");

    gui_raster_given(&raster);
    gui_raster_given(&straight);
    gui_busy_given(&busy);
    cli_check(busy.cell == NULL, "the cell is given back at the end");
    return cli_checked();
}
