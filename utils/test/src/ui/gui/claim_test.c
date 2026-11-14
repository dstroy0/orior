// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// autohide_test.c: the claim's check: a slide runs from its number to the next over one slide on the ease curve, the
// claim made runs its number to one, a rest in an edge's band reveals the edge's part and a pass through it does not,
// the part lingers after the pointer leaves and goes, one edge's part waits for another's to go, the controls' ghost
// shows as the pointer slows in their corner, and the slats grow from the point of rest
#include "../../../../../includes/window/claim.h"
#include "../cli/cli_check.h"
#include "test_clock.h"

static int adjacent(int one, int other)
{
    const int apart = one - other;
    return (apart < (GUI_FIXED_ONE / 50)) && (apart > -(GUI_FIXED_ONE / 50));
}

int main(void)
{
    cli_check((EMBED_CALL(gui_autohide_share.ease, GuiAutohideShareCfg, .share = 0) == 0) && (EMBED_CALL(gui_autohide_share.ease, GuiAutohideShareCfg, .share = GUI_FIXED_ONE) == GUI_FIXED_ONE), "the ease curve runs from 0 to 1");
    cli_check((EMBED_CALL(gui_autohide_share.ease, GuiAutohideShareCfg, .share = GUI_FIXED_ONE / 2) > (7 * GUI_FIXED_ONE / 10)) && (EMBED_CALL(gui_autohide_share.ease, GuiAutohideShareCfg, .share = GUI_FIXED_ONE / 2) < (9 * GUI_FIXED_ONE / 10)), "the ease curve is past the middle halfway along");
    GuiSlide slide = {0, 0, 0ull};
    g_test_clock_ms = 1000ull;
    EMBED_CALL(gui_autohide_target.slide_to, GuiAutohideTargetCfg, .slide_out = &slide, .to = GUI_FIXED_ONE, .delay_ms = 0ull, .time_ms = cli_clock_milliseconds());
    cli_check(adjacent(EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg, .slide = &slide, .time_ms = 1000ull), 0), "a slide begun stands at its start");
    cli_check(EMBED_CALL(gui_autohide_slide.slide_running, GuiAutohideSlideCfg, .slide = &slide, .time_ms = 1100ull) && (EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg, .slide = &slide, .time_ms = 1100ull) > (GUI_FIXED_ONE / 5)) && (EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg, .slide = &slide, .time_ms = 1100ull) < (4 * GUI_FIXED_ONE / 5)),
              "a third of the way through, a slide stands between its ends");
    cli_check(adjacent(EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg, .slide = &slide, .time_ms = 1300ull), GUI_FIXED_ONE) && !EMBED_CALL(gui_autohide_slide.slide_running, GuiAutohideSlideCfg, .slide = &slide, .time_ms = 1300ull), "a slide ends at its end");
    g_test_clock_ms = 1150ull;
    EMBED_CALL(gui_autohide_target.slide_to, GuiAutohideTargetCfg, .slide_out = &slide, .to = 0, .delay_ms = 0ull, .time_ms = cli_clock_milliseconds());
    cli_check(EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg, .slide = &slide, .time_ms = 1150ull) > (3 * GUI_FIXED_ONE / 10), "a slide sent back begins where it stands");
    g_test_clock_ms = 2000ull;
    EMBED_CALL(gui_autohide_target.slide_to, GuiAutohideTargetCfg, .slide_out = &slide, .to = GUI_FIXED_ONE, .delay_ms = GUI_AUTOHIDE_ANIMATION_MS, .time_ms = cli_clock_milliseconds());
    cli_check(adjacent(EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg, .slide = &slide, .time_ms = 2200ull), 0) && EMBED_CALL(gui_autohide_slide.slide_running, GuiAutohideSlideCfg, .slide = &slide, .time_ms = 2200ull), "a slide delayed waits before it runs");
    cli_check(adjacent(EMBED_CALL(gui_autohide_slide.slide_at, GuiAutohideSlideCfg, .slide = &slide, .time_ms = 2600ull), GUI_FIXED_ONE), "a slide delayed ends a delay and a slide after it begins");

    GuiAutohide claim;
    EMBED_CALL(gui_autohide_still_table.init, GuiAutohideStillCfg, .claim = &claim, .now_ms = cli_clock_milliseconds());
    GuiAutohideNumbers numbers;
    EMBED_CALL(gui_autohide_state.numbers, GuiAutohideStateCfg, .claim = &claim, .time_ms = 0ull, .numbers = &numbers);
    cli_check(!claim.autohidden && adjacent(numbers.claim, 0) && adjacent(numbers.grown_top, GUI_FIXED_ONE) && adjacent(numbers.controls_alpha, GUI_FIXED_ONE),
              "a claim made does not hold, its slats whole and the controls whole");
    g_test_clock_ms = 0ull;
    cli_check(EMBED_CALL(gui_autohide.next_deadline_ms, GuiAutohideCfg, .autohide_read = &claim, .time_ms = cli_clock_milliseconds()) == 0ull, "a claim at rest wants no clock");
    g_test_clock_ms = 1000ull;
    EMBED_CALL(gui_autohide_on.set, GuiAutohideOnCfg, .claim = &claim, .on = 1, .now_ms = cli_clock_milliseconds());
    g_test_clock_ms = 1100ull;
    cli_check(claim.autohidden && EMBED_CALL(gui_autohide.moving, GuiAutohideCfg, .autohide_read = &claim, .time_ms = 1100ull) && (EMBED_CALL(gui_autohide.next_deadline_ms, GuiAutohideCfg, .autohide_read = &claim, .time_ms = cli_clock_milliseconds()) != 0ull), "the claim made slides and wants the clock");
    EMBED_CALL(gui_autohide_state.numbers, GuiAutohideStateCfg, .claim = &claim, .time_ms = 1300ull, .numbers = &numbers);
    cli_check(adjacent(numbers.claim, GUI_FIXED_ONE) && !EMBED_CALL(gui_autohide.moving, GuiAutohideCfg, .autohide_read = &claim, .time_ms = 1300ull), "a slide later the claim holds whole");

    // the zones of a 1000 by 800 window at scale 1 with controls 106 wide: the band is three gaps, thirty-six
    cli_check(EMBED_CALL(gui_autohide_width.zone, GuiAutohideWidthCfg, .width = 1000u, .height = 800u, .scale = 100, .controls_width = 106, .x = 500, .y = 10) == GUI_AUTOHIDE_TOP, "the head's band is the top");
    cli_check(EMBED_CALL(gui_autohide_width.zone, GuiAutohideWidthCfg, .width = 1000u, .height = 800u, .scale = 100, .controls_width = 106, .x = 950, .y = 10) == GUI_AUTOHIDE_CONTROLS, "the corner past the controls' width is theirs");
    cli_check(EMBED_CALL(gui_autohide_width.zone, GuiAutohideWidthCfg, .width = 1000u, .height = 800u, .scale = 100, .controls_width = 106, .x = 500, .y = 790) == GUI_AUTOHIDE_BOTTOM, "the foot's band is the bottom");
    cli_check(EMBED_CALL(gui_autohide_width.zone, GuiAutohideWidthCfg, .width = 1000u, .height = 800u, .scale = 100, .controls_width = 106, .x = 10, .y = 400) == GUI_AUTOHIDE_LEFT, "the left's band is the left");
    cli_check(EMBED_CALL(gui_autohide_width.zone, GuiAutohideWidthCfg, .width = 1000u, .height = 800u, .scale = 100, .controls_width = 106, .x = 990, .y = 400) == GUI_AUTOHIDE_RIGHT, "the right's band is the right");
    cli_check(EMBED_CALL(gui_autohide_width.zone, GuiAutohideWidthCfg, .width = 1000u, .height = 800u, .scale = 100, .controls_width = 106, .x = 500, .y = 400) == GUI_AUTOHIDE_NONE, "the middle is no edge's");

    // a rest in the top band reveals the bar; a pass through does not
    g_test_clock_ms = 2000ull;
    EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &claim, .x = 500, .y = 10, .zone = GUI_AUTOHIDE_TOP, .over_parts = 0, .now_ms = cli_clock_milliseconds());
    cli_check((claim.show_deadline_ms == 2000ull + GUI_AUTOHIDE_SHOW_DELAY_MS) && (claim.show_zone == GUI_AUTOHIDE_TOP), "a pointer in the top band begins its rest");
    g_test_clock_ms = 2100ull;
    cli_check(!EMBED_CALL(gui_autohide_state.step, GuiAutohideStateCfg, .claim = &claim, .time_ms = cli_clock_milliseconds()) && (claim.displayed == GUI_AUTOHIDE_NONE), "before the rest is up nothing shows");
    g_test_clock_ms = 2300ull;
    cli_check(EMBED_CALL(gui_autohide_state.step, GuiAutohideStateCfg, .claim = &claim, .time_ms = cli_clock_milliseconds()) && (claim.displayed == GUI_AUTOHIDE_TOP), "the rest up reveals the top");
    cli_check(claim.pulled_top == 500, "the top's slat is pulled from where the pointer rested");
    EMBED_CALL(gui_autohide_state.numbers, GuiAutohideStateCfg, .claim = &claim, .time_ms = 2300ull, .numbers = &numbers);
    cli_check(adjacent(numbers.reveal_top, 0) && adjacent(numbers.grown_top, 0), "the reveal begins at 0 and the slat ungrown");
    EMBED_CALL(gui_autohide_state.numbers, GuiAutohideStateCfg, .claim = &claim, .time_ms = 2450ull, .numbers = &numbers);
    cli_check((numbers.reveal_top > (3 * GUI_FIXED_ONE / 10)) && adjacent(numbers.grown_top, numbers.reveal_top) && (numbers.inset_top == numbers.reveal_top),
              "the slat grows with the reveal and the work gives way with it, both at one place");
    EMBED_CALL(gui_autohide_state.numbers, GuiAutohideStateCfg, .claim = &claim, .time_ms = 2900ull, .numbers = &numbers);
    cli_check(adjacent(numbers.reveal_top, GUI_FIXED_ONE) && adjacent(numbers.inset_top, GUI_FIXED_ONE) && adjacent(numbers.reveal_down, GUI_FIXED_ONE),
              "as the reveal ends the capacity is taken and the controls stand whole");

    // the pointer leaves the part: a linger, then the part goes
    g_test_clock_ms = 3000ull;
    EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &claim, .x = 500, .y = 400, .zone = GUI_AUTOHIDE_NONE, .over_parts = 0, .now_ms = cli_clock_milliseconds());
    cli_check(claim.hide_deadline_ms == 3000ull + GUI_AUTOHIDE_HIDE_DELAY_MS, "the pointer off the part begins the linger");
    g_test_clock_ms = 3100ull;
    EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &claim, .x = 500, .y = 30, .zone = GUI_AUTOHIDE_TOP, .over_parts = 1, .now_ms = cli_clock_milliseconds());
    cli_check(claim.hide_deadline_ms == 0ull, "the pointer back over the part ends the linger");
    g_test_clock_ms = 3200ull;
    EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &claim, .x = 500, .y = 400, .zone = GUI_AUTOHIDE_NONE, .over_parts = 0, .now_ms = cli_clock_milliseconds());
    g_test_clock_ms = 3700ull;
    cli_check(EMBED_CALL(gui_autohide_state.step, GuiAutohideStateCfg, .claim = &claim, .time_ms = cli_clock_milliseconds()) && (claim.displayed == GUI_AUTOHIDE_NONE), "the linger up takes the part back");
    EMBED_CALL(gui_autohide_state.numbers, GuiAutohideStateCfg, .claim = &claim, .time_ms = 3700ull, .numbers = &numbers);
    cli_check(adjacent(numbers.grown_top, GUI_FIXED_ONE) && (numbers.reveal_top > (9 * GUI_FIXED_ONE / 10)), "a part going goes as it grew");
    EMBED_CALL(gui_autohide_state.numbers, GuiAutohideStateCfg, .claim = &claim, .time_ms = 4100ull, .numbers = &numbers);
    cli_check(adjacent(numbers.reveal_top, 0) && adjacent(numbers.grown_top, 0), "a slide later the part is away");

    // a pass through the band: the pointer moves fast across it
    g_test_clock_ms = 5000ull;
    EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &claim, .x = 500, .y = 60, .zone = GUI_AUTOHIDE_NONE, .over_parts = 0, .now_ms = cli_clock_milliseconds());
    g_test_clock_ms = 5010ull;
    EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &claim, .x = 500, .y = 20, .zone = GUI_AUTOHIDE_TOP, .over_parts = 0, .now_ms = cli_clock_milliseconds());
    g_test_clock_ms = 5020ull;
    EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &claim, .x = 500, .y = 2, .zone = GUI_AUTOHIDE_TOP, .over_parts = 0, .now_ms = cli_clock_milliseconds());
    cli_check(claim.show_deadline_ms == 5020ull + GUI_AUTOHIDE_SHOW_DELAY_MS, "a pointer passing fast has its rest begun again at each move");
    g_test_clock_ms = 5030ull;
    EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &claim, .x = 500, .y = 1, .zone = GUI_AUTOHIDE_TOP, .over_parts = 0, .now_ms = cli_clock_milliseconds());
    g_test_clock_ms = 5100ull;
    EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &claim, .x = 500, .y = 1, .zone = GUI_AUTOHIDE_TOP, .over_parts = 0, .now_ms = cli_clock_milliseconds());
    cli_check(claim.show_deadline_ms == 5030ull + GUI_AUTOHIDE_SHOW_DELAY_MS, "a pointer slowed keeps its rest from the move it slowed at");

    // another edge's part waits for the shown one to go
    g_test_clock_ms = 5400ull;
    cli_check(EMBED_CALL(gui_autohide_state.step, GuiAutohideStateCfg, .claim = &claim, .time_ms = cli_clock_milliseconds()) && (claim.displayed == GUI_AUTOHIDE_TOP), "the rest up reveals the top again");
    g_test_clock_ms = 6000ull;
    EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &claim, .x = 500, .y = 790, .zone = GUI_AUTOHIDE_BOTTOM, .over_parts = 0, .now_ms = cli_clock_milliseconds());
    g_test_clock_ms = 6300ull;
    cli_check(EMBED_CALL(gui_autohide_state.step, GuiAutohideStateCfg, .claim = &claim, .time_ms = cli_clock_milliseconds()) && (claim.displayed == GUI_AUTOHIDE_NONE) && (claim.waiting == GUI_AUTOHIDE_BOTTOM),
              "a rest at the foot sends the top away and waits");
    g_test_clock_ms = 6300ull + GUI_AUTOHIDE_ANIMATION_MS;
    cli_check(EMBED_CALL(gui_autohide_state.step, GuiAutohideStateCfg, .claim = &claim, .time_ms = cli_clock_milliseconds()) && (claim.displayed == GUI_AUTOHIDE_BOTTOM), "a slide later the foot's part comes");
    cli_check(claim.pulled_bottom == 500, "the foot's slat is pulled from the rest");

    // the controls come the whole way down as the pointer slows in their corner
    g_test_clock_ms = 7000ull;
    EMBED_CALL(gui_autohide_on.set, GuiAutohideOnCfg, .claim = &claim, .on = 0, .now_ms = cli_clock_milliseconds());
    cli_check(!claim.autohidden && (claim.displayed == GUI_AUTOHIDE_NONE), "the claim let go takes every part back");
    g_test_clock_ms = 7000ull;
    EMBED_CALL(gui_autohide_on.set, GuiAutohideOnCfg, .claim = &claim, .on = 1, .now_ms = cli_clock_milliseconds());
    g_test_clock_ms = 7900ull;
    EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &claim, .x = 600, .y = 300, .zone = GUI_AUTOHIDE_NONE, .over_parts = 0, .now_ms = cli_clock_milliseconds());
    g_test_clock_ms = 8000ull;
    EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &claim, .x = 950, .y = 10, .zone = GUI_AUTOHIDE_CONTROLS, .over_parts = 0, .now_ms = cli_clock_milliseconds());
    cli_check((claim.preview_deadline_ms == 8000ull + (2ull * GUI_AUTOHIDE_TRAIL_WINDOW_MS)), "a pointer come fast into the controls' corner waits to settle");
    g_test_clock_ms = 8020ull;
    EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &claim, .x = 950, .y = 10, .zone = GUI_AUTOHIDE_CONTROLS, .over_parts = 0, .now_ms = cli_clock_milliseconds());
    cli_check((claim.displayed == GUI_AUTOHIDE_CONTROLS), "a pointer slow in the controls' corner brings the controls at once, with no ghost");
    EMBED_CALL(gui_autohide_state.numbers, GuiAutohideStateCfg, .claim = &claim, .time_ms = 8120ull, .numbers = &numbers);
    cli_check((numbers.reveal_down > 0) && (numbers.reveal_down < GUI_FIXED_ONE), "the controls slide down in one motion");
    EMBED_CALL(gui_autohide_state.numbers, GuiAutohideStateCfg, .claim = &claim, .time_ms = 8020ull + GUI_AUTOHIDE_ANIMATION_MS, .numbers = &numbers);
    cli_check(adjacent(numbers.reveal_down, GUI_FIXED_ONE) && adjacent(numbers.controls_alpha, GUI_FIXED_ONE) && adjacent(numbers.reveal_top, 0),
              "the controls come the whole way down alone, the bar staying away");
    g_test_clock_ms = 9000ull;
    EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &claim, .x = 500, .y = 10, .zone = GUI_AUTOHIDE_TOP, .over_parts = 0, .now_ms = cli_clock_milliseconds());
    g_test_clock_ms = 9300ull;
    cli_check(EMBED_CALL(gui_autohide_state.step, GuiAutohideStateCfg, .claim = &claim, .time_ms = cli_clock_milliseconds()) && (claim.displayed == GUI_AUTOHIDE_TOP), "the bar comes under the controls without waiting for them to go");
    EMBED_CALL(gui_autohide_still_table.leave, GuiAutohideStillCfg, .claim = &claim, .now_ms = cli_clock_milliseconds());
    cli_check((claim.show_deadline_ms == 0ull) && (claim.zone_now == GUI_AUTOHIDE_NONE), "the pointer gone out of the window leaves nothing waiting on its rest");
    g_test_clock_ms = 10000ull;
    EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &claim, .x = 2, .y = 300, .zone = GUI_AUTOHIDE_LEFT, .over_parts = 0, .now_ms = cli_clock_milliseconds());
    g_test_clock_ms = 10016ull;
    EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &claim, .x = 4, .y = 301, .zone = GUI_AUTOHIDE_LEFT, .over_parts = 0, .now_ms = cli_clock_milliseconds());
    cli_check(claim.show_deadline_ms == 10000ull + GUI_AUTOHIDE_SHOW_DELAY_MS, "a resting hand's tremor in the band keeps the rest going");
    EMBED_CALL(gui_autohide_still_table.leave, GuiAutohideStillCfg, .claim = &claim, .now_ms = cli_clock_milliseconds());
    cli_check((claim.show_deadline_ms == 10000ull + GUI_AUTOHIDE_SHOW_DELAY_MS) && (claim.show_zone == GUI_AUTOHIDE_LEFT),
              "a pointer gone out of the window through an edge's band keeps that edge's rest");
    g_test_clock_ms = 10300ull;
    cli_check(EMBED_CALL(gui_autohide_state.step, GuiAutohideStateCfg, .claim = &claim, .time_ms = cli_clock_milliseconds()) && (claim.waiting == GUI_AUTOHIDE_LEFT),
              "the rest kept past the edge sends the bar away first");
    g_test_clock_ms = 10300ull + GUI_AUTOHIDE_ANIMATION_MS;
    cli_check(EMBED_CALL(gui_autohide_state.step, GuiAutohideStateCfg, .claim = &claim, .time_ms = cli_clock_milliseconds()) && (claim.displayed == GUI_AUTOHIDE_LEFT), "a slide later the edge's part comes");

    // less motion asked for: a slide stands at once where it is bound, a part's rest still counted before it comes
    EMBED_CALL(gui_autohide_still_table.init, GuiAutohideStillCfg, .claim = &claim, .now_ms = cli_clock_milliseconds());
    EMBED_CALL(gui_autohide_still_table.still, GuiAutohideStillCfg, .claim = &claim, .still = 1, .now_ms = cli_clock_milliseconds());
    g_test_clock_ms = 10000ull;
    EMBED_CALL(gui_autohide_on.set, GuiAutohideOnCfg, .claim = &claim, .on = 1, .now_ms = cli_clock_milliseconds());
    EMBED_CALL(gui_autohide_state.numbers, GuiAutohideStateCfg, .claim = &claim, .time_ms = 10000ull, .numbers = &numbers);
    cli_check((numbers.claim == GUI_FIXED_ONE) && !EMBED_CALL(gui_autohide.moving, GuiAutohideCfg, .autohide_read = &claim, .time_ms = 10000ull), "less motion asked for, the claim stands whole at once");
    g_test_clock_ms = 10100ull;
    EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &claim, .x = 500, .y = 10, .zone = GUI_AUTOHIDE_TOP, .over_parts = 0, .now_ms = cli_clock_milliseconds());
    g_test_clock_ms = 10100ull + GUI_AUTOHIDE_SHOW_DELAY_MS;
    cli_check(EMBED_CALL(gui_autohide_state.step, GuiAutohideStateCfg, .claim = &claim, .time_ms = cli_clock_milliseconds()) && (claim.displayed == GUI_AUTOHIDE_TOP), "the pointer's rest still reveals the bar");
    EMBED_CALL(gui_autohide_state.numbers, GuiAutohideStateCfg, .claim = &claim, .time_ms = 10100ull + GUI_AUTOHIDE_SHOW_DELAY_MS, .numbers = &numbers);
    cli_check((numbers.reveal_top == GUI_FIXED_ONE) && (numbers.inset_top == GUI_FIXED_ONE) && !EMBED_CALL(gui_autohide.moving, GuiAutohideCfg, .autohide_read = &claim, .time_ms = 10100ull + GUI_AUTOHIDE_SHOW_DELAY_MS),
              "and the bar and its capacity stand there at once");
    g_test_clock_ms = 11000ull;
    EMBED_CALL(gui_autohide_on.set, GuiAutohideOnCfg, .claim = &claim, .on = 0, .now_ms = cli_clock_milliseconds());
    EMBED_CALL(gui_autohide_state.numbers, GuiAutohideStateCfg, .claim = &claim, .time_ms = 11000ull, .numbers = &numbers);
    cli_check((numbers.claim == 0) && (numbers.reveal_top == 0) && !EMBED_CALL(gui_autohide.moving, GuiAutohideCfg, .autohide_read = &claim, .time_ms = 11000ull), "let go, every part stands back at once");
    EMBED_CALL(gui_autohide_still_table.still, GuiAutohideStillCfg, .claim = &claim, .still = 0, .now_ms = cli_clock_milliseconds());
    g_test_clock_ms = 12000ull;
    EMBED_CALL(gui_autohide_on.set, GuiAutohideOnCfg, .claim = &claim, .on = 1, .now_ms = cli_clock_milliseconds());
    cli_check(EMBED_CALL(gui_autohide.moving, GuiAutohideCfg, .autohide_read = &claim, .time_ms = 12100ull), "no longer asked for, the slides run");
    EMBED_CALL(gui_autohide_still_table.still, GuiAutohideStillCfg, .claim = &claim, .still = 1, .now_ms = cli_clock_milliseconds());
    EMBED_CALL(gui_autohide_state.numbers, GuiAutohideStateCfg, .claim = &claim, .time_ms = 12100ull, .numbers = &numbers);
    cli_check((numbers.claim == GUI_FIXED_ONE) && !EMBED_CALL(gui_autohide.moving, GuiAutohideCfg, .autohide_read = &claim, .time_ms = 12100ull), "asked for while a slide runs, it ends where it was bound");

    // a menu open holds the claim: the part shown stands while the pointer works in the menu, and no part comes
    g_test_clock_ms = 13000ull;
    EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &claim, .x = 500, .y = 10, .zone = GUI_AUTOHIDE_TOP, .over_parts = 0, .now_ms = cli_clock_milliseconds());
    g_test_clock_ms = 13400ull;
    cli_check(EMBED_CALL(gui_autohide_state.step, GuiAutohideStateCfg, .claim = &claim, .time_ms = cli_clock_milliseconds()) && (claim.displayed == GUI_AUTOHIDE_TOP), "the rest up reveals the top before the menu");
    g_test_clock_ms = 13500ull;
    EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &claim, .x = 500, .y = 400, .zone = GUI_AUTOHIDE_NONE, .over_parts = 0, .menu_open = 1, .now_ms = cli_clock_milliseconds());
    g_test_clock_ms = 14100ull;
    cli_check(!EMBED_CALL(gui_autohide_state.step, GuiAutohideStateCfg, .claim = &claim, .menu_open = 1, .time_ms = cli_clock_milliseconds()) && (claim.displayed == GUI_AUTOHIDE_TOP), "the linger up with a menu open leaves the part standing");
    EMBED_CALL(gui_autohide_pointer.move, GuiAutohidePointerCfg, .claim = &claim, .x = 500, .y = 790, .zone = GUI_AUTOHIDE_BOTTOM, .over_parts = 0, .menu_open = 1, .now_ms = cli_clock_milliseconds());
    cli_check(claim.show_deadline_ms == 0ull, "a rest in a band with a menu open brings no part");
    g_test_clock_ms = 14700ull;
    cli_check(EMBED_CALL(gui_autohide_state.step, GuiAutohideStateCfg, .claim = &claim, .time_ms = cli_clock_milliseconds()) && (claim.displayed == GUI_AUTOHIDE_NONE), "the menu closed, the linger takes the part back");
    return cli_on_check();
}
