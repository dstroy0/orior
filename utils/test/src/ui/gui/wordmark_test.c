// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// wordmark_test.c: the wordmark's check against the page's numbers: the session's first start read as session.js reads
// it; the name's place and sizes as style.css sets them; TABVLA's transition and animation on the page's times and
// curves; the name standing, its cuts' shade above and lip below; the flight up a funnel that narrows to the eye's
// width at the bar's foot and draws nothing above it; the eye home at the end of the flight, and where it stands and
// how it shrinks as the claim runs; and what a press on the name lands on
#include "../../../../../ui/gui/src/parts/wordmark.h"
#include "../cli/cli_check.h"

#define GROUND GUI_RGB(0x80u, 0x80u, 0x80u)

static GuiColor pixel_at(const GuiRaster *raster, int x, int y)
{
    return raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x];
}

// the raster made all ground
static void cleared(GuiRaster *raster)
{
    // the raster's sides are far inside what an int represents
    gui_raster_filled(raster, (GuiRect){0, 0, (int)raster->width, (int)raster->height}, GROUND);
}

// how many pixels of `rect` are not the ground
static unsigned int painted(const GuiRaster *raster, GuiRect rect)
{
    unsigned int count = 0u;
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            count += (pixel_at(raster, x, y) != GROUND) ? 1u : 0u;
        }
    }
    return count;
}

// how many pixels of `rect` are `color` whole
static unsigned int colored(const GuiRaster *raster, GuiRect rect, GuiColor color)
{
    unsigned int count = 0u;
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            count += (pixel_at(raster, x, y) == color) ? 1u : 0u;
        }
    }
    return count;
}

// the first session read: each way session.js reads one as the first, and the ways it reads the same one
static void session_checked(void)
{
    GuiWordmarkSession now;
    GuiWordmarkSession kept;
    cli_check(gui_wordmark_session_read("{\"booted\":1000,\"logon\":\"alice\"}", 31ull, &now) && now.known && (now.booted == 1000ull),
              "a session as the CLI writes it reads");
    cli_check(!gui_wordmark_session_read(NULL, 0ull, &kept) && gui_wordmark_first_in(&kept, &now), "no session kept is the first start in the session");
    (void)gui_wordmark_session_read("{\"booted\":1000,\"logon\":\"alice\"}", 31ull, &kept);
    GuiWordmarkSession none;
    (void)gui_wordmark_session_read(NULL, 0ull, &none);
    cli_check(gui_wordmark_first_in(&kept, &none), "no session named by the system is taken as the first");
    cli_check(!gui_wordmark_session_read("not json", 8ull, &kept) && !kept.known && gui_wordmark_first_in(&kept, &now), "a session kept that does not read is the first");
    cli_check(!gui_wordmark_session_read("{\"booted\":1000,\"name\":\"alice\"}", 30ull, &kept), "another shape does not read");
    (void)gui_wordmark_session_read("{\"booted\":1000,\"logon\":\"alice\"}", 31ull, &kept);
    cli_check(!gui_wordmark_first_in(&kept, &now), "the same session is not the first");
    (void)gui_wordmark_session_read("{\"booted\":1059,\"logon\":\"alice\"}", 31ull, &kept);
    cli_check(!gui_wordmark_first_in(&kept, &now), "a computer's start read 59 seconds off is the same");
    (void)gui_wordmark_session_read("{\"booted\":1060,\"logon\":\"alice\"}", 31ull, &kept);
    cli_check(gui_wordmark_first_in(&kept, &now), "a computer's start a minute off is another");
    (void)gui_wordmark_session_read("{\"booted\":1000,\"logon\":\"bob\"}", 29ull, &kept);
    cli_check(gui_wordmark_first_in(&kept, &now), "another log in is another session");
}

// the name's place: its em clamped as clamp(3rem, 7vw, 5.5rem) is, its boxes laid as the column's rows are
static void place_checked(GuiGlyphs *glyphs)
{
    GuiFrame frame;
    GuiWordmarkPlace place;
    gui_frame_laid(&frame, 1000u, 700u, 1.0f);
    gui_wordmark_placed(glyphs, &frame, &place);
    cli_check(place.em == 70u, "the em is 7% of the window's width");
    const int middle = frame.inside.x + (frame.inside.width / 2);
    cli_check(place.under.height == 39, "TABVLA is .56em tall");
    cli_check(place.under.width == 200, "TABVLA's width follows its box of 64.2 by 12.6");
    cli_check((place.under.x <= middle - 99) && (place.under.x >= middle - 101), "TABVLA is centered on the pane");
    cli_check((place.name.x + (place.name.width / 2) >= middle - 3) && (place.name.x + (place.name.width / 2) <= middle + 3),
              "orior and the eye are centered on the pane");
    cli_check(place.name.height == 188, "orior and the eye take 1em, .1em and 1.58em");
    cli_check(place.under.y == place.name.y + 195, "TABVLA stands .1em under the eye");
    const int whole_top = frame.inside.y + ((frame.inside.height - 234) / 2);
    cli_check((place.name.y >= whole_top - 1) && (place.name.y <= whole_top + 1), "the column of 3.34em is centered down the pane");
    cli_check(place.rise == -82, "TABVLA's middle rises to the eye's, 1.17em up");
    gui_frame_laid(&frame, 400u, 700u, 1.0f);
    gui_wordmark_placed(glyphs, &frame, &place);
    cli_check(place.em == 48u, "the em is no less than 3rem");
    gui_frame_laid(&frame, 3000u, 700u, 1.0f);
    gui_wordmark_placed(glyphs, &frame, &place);
    cli_check(place.em == 88u, "the em is no more than 5.5rem");
    gui_frame_laid(&frame, 6000u, 1400u, 2.0f);
    gui_wordmark_placed(glyphs, &frame, &place);
    cli_check(place.em == 176u, "the em's bounds go with the scale");
}

// TABVLA's transition as the name stands and its animation as the name goes
static void under_checked(void)
{
    GuiWordmark wordmark;
    gui_wordmark_made(&wordmark);
    int down = 0;
    unsigned int opacity = 0u;
    gui_wordmark_stood(&wordmark, 1000ull);
    gui_wordmark_under_at(&wordmark, 1000ull, &down, &opacity);
    cli_check((down == 0) && (opacity == 0u), "TABVLA is clear as the name stands");
    gui_wordmark_under_at(&wordmark, 1300ull, &down, &opacity);
    cli_check(opacity == 0u, "and stays clear for 300 milliseconds");
    gui_wordmark_under_at(&wordmark, 1550ull, &down, &opacity);
    cli_check((opacity >= 110u) && (opacity <= 145u), "ease-in-out is about half way at the stage half way through its 500 milliseconds");
    gui_wordmark_under_at(&wordmark, 1800ull, &down, &opacity);
    cli_check(opacity == 255u, "TABVLA is whole 800 milliseconds after the name stands");

    wordmark.stands = 0;
    wordmark.went_ms = 10000ull;
    wordmark.rise = -100;
    // the slide's stages as the pane's leaving makes them: the way up at each is ease-out of the time
    for (unsigned int stage = 0u; stage < (unsigned int)(GUI_WORDMARK_SLIDES / GUI_WORDMARK_FRAME) + 1u; stage += 1u)
    {
        wordmark.under_down[stage] = (short)((-100 * (int)stage) / (int)(GUI_WORDMARK_SLIDES / GUI_WORDMARK_FRAME));
    }
    gui_wordmark_under_at(&wordmark, 10000ull, &down, &opacity);
    cli_check((down == 0) && (opacity == 255u), "TABVLA is whole where it stands as the name goes");
    gui_wordmark_under_at(&wordmark, 10270ull, &down, &opacity);
    cli_check(down == wordmark.under_down[270ull / GUI_WORDMARK_FRAME], "part way through the slide TABVLA stands at its stage");
    gui_wordmark_under_at(&wordmark, 10540ull, &down, &opacity);
    cli_check((down == -100) && (opacity == 255u), "TABVLA has risen whole by the end of the slide");
    gui_wordmark_under_at(&wordmark, 11040ull, &down, &opacity);
    cli_check((down == -100) && (opacity == 255u), "and stays whole for 500 milliseconds");
    gui_wordmark_under_at(&wordmark, 11220ull, &down, &opacity);
    cli_check((opacity >= 165u) && (opacity <= 185u), "ease-in has taken 32% of it half way through the fade");
    gui_wordmark_under_at(&wordmark, 11400ull, &down, &opacity);
    cli_check(opacity == 0u, "TABVLA is gone at the end of the fade");
    wordmark.went_ms = 0ull;
    gui_wordmark_given(&wordmark);
}

// less motion asked for: TABVLA whole from the start, the pane standing its least, and the eye home with no flight
static void still_checked(GuiGlyphs *glyphs, GuiIcons *icons, const GuiPalette *palette)
{
    GuiFrame frame;
    gui_frame_laid(&frame, 1280u, 800u, 1.0f);
    GuiWordmark wordmark;
    gui_wordmark_made(&wordmark);
    gui_wordmark_still(&wordmark, 1);
    gui_wordmark_stood(&wordmark, 1000ull);
    int down = 1;
    unsigned int opacity = 0u;
    gui_wordmark_under_at(&wordmark, 1000ull, &down, &opacity);
    cli_check((down == 0) && (opacity == 255u), "less motion asked for, TABVLA stands whole from the start");
    cli_check(gui_wordmark_due(&wordmark, 1000ull) == 0ull, "no frame is wanted while the tree is read");
    gui_wordmark_tree_read(&wordmark);
    cli_check(gui_wordmark_due(&wordmark, 1000ull) == GUI_WORDMARK_SHORTEST, "and the pane stands its least all the same");
    cli_check(gui_wordmark_ticked(&wordmark, glyphs, icons, palette, &frame, 1000ull + GUI_WORDMARK_SHORTEST), "the pane leaves at its time");
    cli_check(!wordmark.stands && wordmark.home && !wordmark.pours && (wordmark.went_ms == 0ull) && (wordmark.row == NULL) &&
                  (wordmark.under.pixels == NULL),
              "with no flight: the eye home, nothing of the name kept");
    cli_check(gui_wordmark_due(&wordmark, 1000ull + GUI_WORDMARK_SHORTEST) == 0ull, "and no frame is wanted after");
    gui_wordmark_given(&wordmark);

    gui_wordmark_made(&wordmark);
    gui_wordmark_stood(&wordmark, 1000ull);
    gui_wordmark_tree_read(&wordmark);
    (void)gui_wordmark_ticked(&wordmark, glyphs, icons, palette, &frame, 1000ull + GUI_WORDMARK_SHORTEST);
    cli_check(wordmark.pours, "with no wish for less motion the name flies");
    gui_wordmark_still(&wordmark, 1);
    cli_check(!wordmark.pours && wordmark.home && (wordmark.went_ms == 0ull) && (wordmark.row == NULL) && (wordmark.flight_first == NULL),
              "asked for in flight, the eye is home at once and nothing of the name kept");
    gui_wordmark_given(&wordmark);
}

// one first start from end to end: the name standing, the pane leaving, the flight, the landing, the fade
static void flight_checked(GuiGlyphs *glyphs, GuiIcons *icons, const GuiPalette *palette)
{
    GuiFrame frame;
    gui_frame_laid(&frame, 1000u, 700u, 1.0f);
    GuiRaster raster;
    cli_check(gui_raster_made(&raster, 1000u, 700u), "the frame's raster is made");
    const GuiRect whole = {0, 0, 1000, 700};
    GuiWordmark wordmark;
    gui_wordmark_made(&wordmark);
    cli_check(!wordmark.home && !wordmark.stands && (gui_wordmark_due(&wordmark, 5ull) == 0ull), "a wordmark made wants nothing");
    cleared(&raster);
    gui_wordmark_drawn(&wordmark, &raster, glyphs, icons, palette, &frame, 5ull);
    gui_wordmark_eye_drawn(&wordmark, &raster, icons, palette, &frame);
    cli_check(painted(&raster, whole) == 0u, "and draws nothing, not even the eye");

    GuiWordmarkPlace place;
    gui_wordmark_placed(glyphs, &frame, &place);
    const unsigned long long begun = 1000ull;
    gui_wordmark_stood(&wordmark, begun);
    cli_check(wordmark.stands && !wordmark.home, "the name stands on the pane, the eye not yet in the bar");
    cli_check(gui_wordmark_due(&wordmark, begun) == GUI_WORDMARK_FRAME, "TABVLA fading in wants frames");
    (void)gui_wordmark_ticked(&wordmark, glyphs, icons, palette, &frame, begun);
    cleared(&raster);
    gui_wordmark_drawn(&wordmark, &raster, glyphs, icons, palette, &frame, begun);
    cli_check(colored(&raster, place.name, palette->open_text) > 400u, "orior and the eye stand in --open-fg");
    cli_check(painted(&raster, (GuiRect){0, 0, 1000, place.name.y}) == 0u, "nothing is drawn above the name");
    cli_check(painted(&raster, (GuiRect){0, place.under.y, 1000, 80}) == 0u, "TABVLA is clear as the name stands");

    cli_check(gui_wordmark_ticked(&wordmark, glyphs, icons, palette, &frame, begun + 800ull) == 1,
              "the frame TABVLA comes whole in is owed");
    cli_check(gui_wordmark_due(&wordmark, begun + 801ull) == 0ull, "whole, with the tree not read, nothing moves");
    cli_check(gui_wordmark_ticked(&wordmark, glyphs, icons, palette, &frame, begun + 900ull) == 0, "and no frame is owed");
    cleared(&raster);
    gui_wordmark_drawn(&wordmark, &raster, glyphs, icons, palette, &frame, begun + 900ull);
    cli_check(colored(&raster, place.under, palette->incised) > 300u, "TABVLA's cuts stand in their color");
    // the T's bar, in from its head: the cut's own color at its line, the shade over it and the lip under it
    const int column = place.under.x + 5;
    int first = -1;
    int last = -1;
    for (int y = place.under.y - 4; y < place.under.y + 20; y += 1)
    {
        if (pixel_at(&raster, column, y) == palette->incised)
        {
            first = (first < 0) ? y : first;
            last = y;
        }
    }
    cli_check(first >= 0, "the T's bar is cut through the column");
    cli_check(GUI_RED_OF(pixel_at(&raster, column, first - 2)) < 0x80u, "the cut's upper wall lies in shade");
    cli_check(GUI_RED_OF(pixel_at(&raster, column, last + 2)) > 0x80u, "its lower lip catches the light");
    cli_check(GUI_ALPHA_OF(pixel_at(&raster, column, last + 2)) == 0xffu, "and the frame stays opaque");

    gui_wordmark_tree_read(&wordmark);
    cli_check(gui_wordmark_due(&wordmark, begun + 1000ull) == GUI_WORDMARK_SHORTEST - 1000ull,
              "the tree read, the pane stands out its 6 seconds");
    (void)gui_wordmark_ticked(&wordmark, glyphs, icons, palette, &frame, begun + 5999ull);
    cli_check(wordmark.stands && (wordmark.went_ms == 0ull), "one millisecond short, the pane stands");
    const unsigned long long went = begun + GUI_WORDMARK_SHORTEST;
    cli_check(gui_wordmark_ticked(&wordmark, glyphs, icons, palette, &frame, went) == 1, "the pane leaves at 6 seconds");
    cli_check(!wordmark.stands && (wordmark.went_ms == went) && wordmark.pours, "the name goes as the pane leaves");
    cli_check((wordmark.flight_first != NULL) && (wordmark.name.cover == NULL), "its stages are made once as it goes, the mask they were made from given back");
    const GuiRect eye = gui_wordmark_eye_square(&frame);
    cli_check((wordmark.tip_y == frame.bar.y + frame.bar.height) && (wordmark.tip_left == eye.x) &&
                  (wordmark.tip_right == eye.x + eye.width),
              "the funnel's tip is on the bar's foot, as wide as the eye");
    cli_check(gui_wordmark_due(&wordmark, went) == GUI_WORDMARK_FRAME, "the flight wants frames");
    int slid = 0;
    unsigned int slid_opacity = 0u;
    gui_wordmark_under_at(&wordmark, went + 270ull, &slid, &slid_opacity);
    cli_check((slid * 100 <= place.rise * 60) && (slid * 100 >= place.rise * 72) && (slid_opacity == 255u), "its stages have TABVLA about two thirds of the way up half way through the slide, by ease-out");

    // at its start the name stands where it stood
    cleared(&raster);
    gui_wordmark_drawn(&wordmark, &raster, glyphs, icons, palette, &frame, went);
    const unsigned int standing = colored(&raster, place.name, palette->open_text);
    cli_check((standing > 400u) && (painted(&raster, (GuiRect){0, 0, 1000, place.name.y}) == 0u),
              "the flight's first frame is the name where it stood");

    // half way the funnel has formed: the name's rows stand between the bar's foot and where it stood, the rows near
    // the tip narrower than those below them, and nothing above the bar's foot
    const unsigned long long half = went + (GUI_WORDMARK_FLIES * 3ull / 4ull);
    (void)gui_wordmark_ticked(&wordmark, glyphs, icons, palette, &frame, half);
    cleared(&raster);
    gui_wordmark_drawn(&wordmark, &raster, glyphs, icons, palette, &frame, half);
    cli_check(painted(&raster, (GuiRect){0, 0, 1000, wordmark.tip_y}) == 0u, "no row is drawn above the funnel's tip");
    const GuiRect between = {0, wordmark.tip_y, 1000, place.name.y - wordmark.tip_y};
    cli_check(painted(&raster, between) > 200u, "the name has risen into the funnel");
    // how far out the rows lower down stand, and later in the flight how far the rows at the tip do
    int low_left = 1000;
    for (int x = 0; x < 1000; x += 1)
    {
        for (int y = wordmark.tip_y + 60; y < place.name.y; y += 1)
        {
            low_left = ((x < low_left) && (pixel_at(&raster, x, y) != GROUND)) ? x : low_left;
        }
    }
    cli_check((low_left < 1000) && (low_left > wordmark.tip_left + 24), "the rows lower down stand out toward the name");
    const unsigned long long late = went + (GUI_WORDMARK_FLIES * 7ull / 8ull);
    cleared(&raster);
    gui_wordmark_drawn(&wordmark, &raster, glyphs, icons, palette, &frame, late);
    int near_left = 1000;
    int near_right = 0;
    for (int x = 0; x < 1000; x += 1)
    {
        for (int y = wordmark.tip_y; y < wordmark.tip_y + 12; y += 1)
        {
            if (pixel_at(&raster, x, y) != GROUND)
            {
                near_left = (x < near_left) ? x : near_left;
                near_right = (x > near_right) ? x : near_right;
            }
        }
    }
    cli_check((near_right > near_left) && (near_left >= wordmark.tip_left - 24) && (near_right <= wordmark.tip_right + 24),
              "the rows at the tip are near the eye's width and place");

    cli_check(!wordmark.home, "the eye is not home before the flight ends");
    (void)gui_wordmark_ticked(&wordmark, glyphs, icons, palette, &frame, went + GUI_WORDMARK_FLIES);
    cli_check(wordmark.home && !wordmark.pours, "the flight's end brings the eye home");
    cleared(&raster);
    gui_wordmark_drawn(&wordmark, &raster, glyphs, icons, palette, &frame, went + GUI_WORDMARK_FLIES);
    gui_wordmark_eye_drawn(&wordmark, &raster, icons, palette, &frame);
    cli_check(painted(&raster, eye) > 40u, "the eye stands in the bar");
    cli_check(colored(&raster, place.name, palette->open_text) == 0u, "orior and the eye have gone from the pane");
    cli_check(colored(&raster, whole, palette->incised) > 300u, "TABVLA stays on past the flight");

    const unsigned long long ends = GUI_WORDMARK_SLIDES + GUI_WORDMARK_STAYS + GUI_WORDMARK_FADES;
    cli_check(gui_wordmark_due(&wordmark, went + ends - 1ull) == GUI_WORDMARK_FRAME, "TABVLA's fade wants frames to its end");
    cli_check(gui_wordmark_ticked(&wordmark, glyphs, icons, palette, &frame, went + ends) == 1, "the frame it ends in is owed");
    cli_check((wordmark.went_ms == 0ull) && (wordmark.name.cover == NULL) && (wordmark.under.pixels == NULL) && (wordmark.flight_first == NULL),
              "its end gives the name's, the flight's and TABVLA's cells back");
    cli_check(gui_wordmark_due(&wordmark, went + ends) == 0ull, "and nothing moves");
    cleared(&raster);
    gui_wordmark_drawn(&wordmark, &raster, glyphs, icons, palette, &frame, went + ends);
    cli_check(painted(&raster, whole) == 0u, "the large name is gone for good");

    // the tree read after the least, the pane leaves on its next tick
    gui_wordmark_made(&wordmark);
    gui_wordmark_stood(&wordmark, begun);
    (void)gui_wordmark_ticked(&wordmark, glyphs, icons, palette, &frame, begun + 9000ull);
    cli_check(wordmark.stands, "with the tree not read the pane stands past its least");
    gui_wordmark_tree_read(&wordmark);
    cli_check(gui_wordmark_due(&wordmark, begun + 9000ull) == 1ull, "the tree read, a tick is wanted at once");
    (void)gui_wordmark_ticked(&wordmark, glyphs, icons, palette, &frame, begun + 9001ull);
    cli_check(!wordmark.stands && wordmark.pours, "and the pane leaves");
    gui_wordmark_given(&wordmark);
    cli_check((wordmark.name.cover == NULL) && (wordmark.under.pixels == NULL), "a wordmark given holds no cells");

    gui_wordmark_made(&wordmark);
    gui_wordmark_landed(&wordmark);
    cli_check(wordmark.home && !wordmark.stands && (gui_wordmark_due(&wordmark, 1ull) == 0ull), "a later start has the eye home at once");
    gui_wordmark_given(&wordmark);
    gui_raster_given(&raster);
}

// the eye's square as .bar-mark's rules set it, through the claim
static void eye_checked(GuiGlyphs *glyphs, GuiIcons *icons, const GuiPalette *palette)
{
    GuiFrame frame;
    gui_frame_laid(&frame, 1000u, 700u, 1.0f);
    GuiRect eye = gui_wordmark_eye_square(&frame);
    cli_check((eye.x == 9) && (eye.y == 10) && (eye.width == 29) && (eye.height == 29),
              "unclaimed the eye stands 9 in and 10 down, 1.164em by 1.475 at 17 pixels");
    GuiClaimNumbers numbers = frame.claim;
    numbers.claim = GUI_WHOLE;
    gui_frame_claimed(&frame, &numbers);
    eye = gui_wordmark_eye_square(&frame);
    cli_check(gui_rect_empty(eye), "the claim with the bar and the strip both away has shrunk the eye out");
    numbers.room_top = GUI_WHOLE;
    gui_frame_claimed(&frame, &numbers);
    eye = gui_wordmark_eye_square(&frame);
    cli_check((eye.x == 21) && (eye.y == 22) && (eye.width == 29), "the head's room back has it whole, a gap further in and down");
    numbers.room_top = 0;
    numbers.room_left = GUI_WHOLE;
    numbers.reveal_left = GUI_WHOLE;
    gui_frame_claimed(&frame, &numbers);
    eye = gui_wordmark_eye_square(&frame);
    cli_check((eye.x == 21) && (eye.y == 22) && (eye.width == 29), "the left's room has it whole in the same place");
    numbers.room_left = GUI_WHOLE / 2;
    gui_frame_claimed(&frame, &numbers);
    eye = gui_wordmark_eye_square(&frame);
    cli_check((eye.x == 21) && (eye.y == 22) && (eye.width == 15), "half the room has it half the size toward its own top left");

    gui_frame_laid(&frame, 2000u, 1400u, 2.0f);
    eye = gui_wordmark_eye_square(&frame);
    cli_check((eye.x == 18) && (eye.y == 20) && (eye.width == 58), "the scale takes the eye's place and size with it");

    gui_frame_laid(&frame, 1000u, 700u, 1.0f);
    GuiRaster raster;
    cli_check(gui_raster_made(&raster, 100u, 60u), "a raster for the eye is made");
    cleared(&raster);
    GuiWordmark wordmark;
    gui_wordmark_made(&wordmark);
    gui_wordmark_eye_drawn(&wordmark, &raster, icons, palette, &frame);
    cli_check(painted(&raster, (GuiRect){0, 0, 100, 60}) == 0u, "the eye is not drawn before it is home");
    gui_wordmark_landed(&wordmark);
    gui_wordmark_eye_drawn(&wordmark, &raster, icons, palette, &frame);
    eye = gui_wordmark_eye_square(&frame);
    const unsigned int inked = painted(&raster, eye);
    cli_check((inked > 40u) && (inked == painted(&raster, (GuiRect){0, 0, 100, 60})), "the eye home is drawn on its square and nowhere else");
    cli_check(colored(&raster, eye, palette->bar_mark) > 0u, "its iris is filled whole in --bar-mark");
    cli_check(gui_wordmark_holds(&wordmark, &frame, eye.x + 5, eye.y + 5), "a press on the eye lands on the name");
    cli_check(!gui_wordmark_holds(&wordmark, &frame, eye.x + eye.width + 5, eye.y + 5), "a press past it does not");
    gui_wordmark_given(&wordmark);

    gui_wordmark_made(&wordmark);
    cli_check(!gui_wordmark_holds(&wordmark, &frame, eye.x + 5, eye.y + 5), "before it is home the eye's square is no name");
    gui_wordmark_stood(&wordmark, 1ull);
    gui_wordmark_drawn(&wordmark, &raster, glyphs, icons, palette, &frame, 1ull);
    GuiWordmarkPlace place;
    gui_wordmark_placed(glyphs, &frame, &place);
    cli_check(gui_wordmark_holds(&wordmark, &frame, place.name.x + 5, place.name.y + 5) &&
                  gui_wordmark_holds(&wordmark, &frame, place.under.x + 5, place.under.y + 5),
              "a press on the large name lands on it while it stands");
    cli_check(!gui_wordmark_holds(&wordmark, &frame, 1, place.name.y), "and a press beside it does not");
    gui_wordmark_given(&wordmark);
    gui_raster_given(&raster);
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    char err_text[256];
    VerbaLinea err_msg = {err_text, sizeof(err_text), 0ull};
    GuiGlyphs glyphs;
    if (!gui_glyphs_made(&glyphs, &err_msg))
    {
        cli_check(0, "the glyphs are read");
        return cli_checked();
    }
    GuiIcons icons;
    gui_icons_made(&icons);
    const GuiPalette *const palette = gui_palette(GUI_SCHEME_DARK);
    session_checked();
    place_checked(&glyphs);
    under_checked();
    still_checked(&glyphs, &icons, palette);
    flight_checked(&glyphs, &icons, palette);
    eye_checked(&glyphs, &icons, palette);
    gui_icons_given(&icons);
    gui_glyphs_given(&glyphs);
    return cli_checked();
}
