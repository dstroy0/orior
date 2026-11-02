// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// claim_test.c: the claim's check: a slide runs from its number to the next over one slide on the ease curve, the
// claim made runs its number to one, a rest in an edge's band reveals the edge's part and a pass through it does not,
// the part lingers after the pointer leaves and goes, one edge's part waits for another's to go, the controls' ghost
// shows as the pointer slows in their corner, and the slats grow from the point of rest
#include "../../../../../ui/gui/src/claim.h"
#include "../cli/cli_check.h"
#include "test_clock.h"

static int near(int one, int other)
{
    const int apart = one - other;
    return (apart < (GUI_WHOLE / 50)) && (apart > -(GUI_WHOLE / 50));
}

int main(void)
{
    cli_check((EMBED_CALL(gui_claim_share.ease, GuiClaimShareCfg, .share = 0) == 0) && (EMBED_CALL(gui_claim_share.ease, GuiClaimShareCfg, .share = GUI_WHOLE) == GUI_WHOLE), "the ease curve runs from 0 to 1");
    cli_check((EMBED_CALL(gui_claim_share.ease, GuiClaimShareCfg, .share = GUI_WHOLE / 2) > (7 * GUI_WHOLE / 10)) && (EMBED_CALL(gui_claim_share.ease, GuiClaimShareCfg, .share = GUI_WHOLE / 2) < (9 * GUI_WHOLE / 10)), "the ease curve is past the middle halfway along");
    GuiSlide slide = {0, 0, 0ull};
    g_test_clock_ms = 1000ull;
    EMBED_CALL(gui_claim_target.slide_to, GuiClaimTargetCfg, .slide_out = &slide, .to = GUI_WHOLE, .delay_ms = 0ull);
    cli_check(near(EMBED_CALL(gui_claim_slide.slide_at, GuiClaimSlideCfg, .slide = &slide, .time_ms = 1000ull), 0), "a slide begun stands at its start");
    cli_check(EMBED_CALL(gui_claim_slide.slide_running, GuiClaimSlideCfg, .slide = &slide, .time_ms = 1100ull) && (EMBED_CALL(gui_claim_slide.slide_at, GuiClaimSlideCfg, .slide = &slide, .time_ms = 1100ull) > (GUI_WHOLE / 5)) && (EMBED_CALL(gui_claim_slide.slide_at, GuiClaimSlideCfg, .slide = &slide, .time_ms = 1100ull) < (4 * GUI_WHOLE / 5)),
              "a third of the way through, a slide stands between its ends");
    cli_check(near(EMBED_CALL(gui_claim_slide.slide_at, GuiClaimSlideCfg, .slide = &slide, .time_ms = 1300ull), GUI_WHOLE) && !EMBED_CALL(gui_claim_slide.slide_running, GuiClaimSlideCfg, .slide = &slide, .time_ms = 1300ull), "a slide ends at its end");
    g_test_clock_ms = 1150ull;
    EMBED_CALL(gui_claim_target.slide_to, GuiClaimTargetCfg, .slide_out = &slide, .to = 0, .delay_ms = 0ull);
    cli_check(EMBED_CALL(gui_claim_slide.slide_at, GuiClaimSlideCfg, .slide = &slide, .time_ms = 1150ull) > (3 * GUI_WHOLE / 10), "a slide sent back begins where it stands");
    g_test_clock_ms = 2000ull;
    EMBED_CALL(gui_claim_target.slide_to, GuiClaimTargetCfg, .slide_out = &slide, .to = GUI_WHOLE, .delay_ms = GUI_CLAIM_ANIMATION_MS);
    cli_check(near(EMBED_CALL(gui_claim_slide.slide_at, GuiClaimSlideCfg, .slide = &slide, .time_ms = 2200ull), 0) && EMBED_CALL(gui_claim_slide.slide_running, GuiClaimSlideCfg, .slide = &slide, .time_ms = 2200ull), "a slide delayed waits before it runs");
    cli_check(near(EMBED_CALL(gui_claim_slide.slide_at, GuiClaimSlideCfg, .slide = &slide, .time_ms = 2600ull), GUI_WHOLE), "a slide delayed ends a delay and a slide after it begins");

    GuiClaim claim;
    EMBED_CALL(cli_claim_still.init, GuiClaimStillCfg, .claim = &claim);
    GuiClaimNumbers numbers;
    EMBED_CALL(gui_claim_state.numbers, GuiClaimStateCfg, .claim = &claim, .time_ms = 0ull, .numbers = &numbers);
    cli_check(!claim.claimed && near(numbers.claim, 0) && near(numbers.grown_top, GUI_WHOLE) && near(numbers.controls_alpha, GUI_WHOLE),
              "a claim made does not hold, its slats whole and the controls whole");
    g_test_clock_ms = 0ull;
    cli_check(EMBED_CALL(gui_claim.next_deadline_ms, GuiClaimCfg, .claim_read = &claim) == 0ull, "a claim at rest wants no clock");
    g_test_clock_ms = 1000ull;
    EMBED_CALL(gui_claim_on.set, GuiClaimOnCfg, .claim = &claim, .on = 1);
    g_test_clock_ms = 1100ull;
    cli_check(claim.claimed && EMBED_CALL(gui_claim.moving, GuiClaimCfg, .claim_read = &claim, .time_ms = 1100ull) && (EMBED_CALL(gui_claim.next_deadline_ms, GuiClaimCfg, .claim_read = &claim) != 0ull), "the claim made slides and wants the clock");
    EMBED_CALL(gui_claim_state.numbers, GuiClaimStateCfg, .claim = &claim, .time_ms = 1300ull, .numbers = &numbers);
    cli_check(near(numbers.claim, GUI_WHOLE) && !EMBED_CALL(gui_claim.moving, GuiClaimCfg, .claim_read = &claim, .time_ms = 1300ull), "a slide later the claim holds whole");

    // the zones of a 1000 by 800 window at scale 1 with controls 106 wide: the band is three gaps, thirty-six
    cli_check(EMBED_CALL(gui_claim_width.zone, GuiClaimWidthCfg, .width = 1000u, .height = 800u, .scale = 100, .controls_wide = 106, .x = 500, .y = 10) == GUI_CLAIM_TOP, "the head's band is the top");
    cli_check(EMBED_CALL(gui_claim_width.zone, GuiClaimWidthCfg, .width = 1000u, .height = 800u, .scale = 100, .controls_wide = 106, .x = 950, .y = 10) == GUI_CLAIM_CONTROLS, "the corner past the controls' width is theirs");
    cli_check(EMBED_CALL(gui_claim_width.zone, GuiClaimWidthCfg, .width = 1000u, .height = 800u, .scale = 100, .controls_wide = 106, .x = 500, .y = 790) == GUI_CLAIM_BOTTOM, "the foot's band is the bottom");
    cli_check(EMBED_CALL(gui_claim_width.zone, GuiClaimWidthCfg, .width = 1000u, .height = 800u, .scale = 100, .controls_wide = 106, .x = 10, .y = 400) == GUI_CLAIM_LEFT, "the left's band is the left");
    cli_check(EMBED_CALL(gui_claim_width.zone, GuiClaimWidthCfg, .width = 1000u, .height = 800u, .scale = 100, .controls_wide = 106, .x = 990, .y = 400) == GUI_CLAIM_RIGHT, "the right's band is the right");
    cli_check(EMBED_CALL(gui_claim_width.zone, GuiClaimWidthCfg, .width = 1000u, .height = 800u, .scale = 100, .controls_wide = 106, .x = 500, .y = 400) == GUI_CLAIM_NONE, "the middle is no edge's");

    // a rest in the top band reveals the bar; a pass through does not
    g_test_clock_ms = 2000ull;
    EMBED_CALL(gui_claim_pointer.move, GuiClaimPointerCfg, .claim = &claim, .x = 500, .y = 10, .zone = GUI_CLAIM_TOP, .over_parts = 0);
    cli_check((claim.show_deadline_ms == 2000ull + GUI_CLAIM_SHOW_DELAY_MS) && (claim.show_zone == GUI_CLAIM_TOP), "a pointer in the top band begins its rest");
    g_test_clock_ms = 2100ull;
    cli_check(!EMBED_CALL(gui_claim_state.step, GuiClaimStateCfg, .claim = &claim) && (claim.shown == GUI_CLAIM_NONE), "before the rest is up nothing shows");
    g_test_clock_ms = 2300ull;
    cli_check(EMBED_CALL(gui_claim_state.step, GuiClaimStateCfg, .claim = &claim) && (claim.shown == GUI_CLAIM_TOP), "the rest up reveals the top");
    cli_check(claim.pulled_top == 500, "the top's slat is pulled from where the pointer rested");
    EMBED_CALL(gui_claim_state.numbers, GuiClaimStateCfg, .claim = &claim, .time_ms = 2300ull, .numbers = &numbers);
    cli_check(near(numbers.reveal_top, 0) && near(numbers.grown_top, 0), "the reveal begins at 0 and the slat ungrown");
    EMBED_CALL(gui_claim_state.numbers, GuiClaimStateCfg, .claim = &claim, .time_ms = 2450ull, .numbers = &numbers);
    cli_check((numbers.reveal_top > (3 * GUI_WHOLE / 10)) && near(numbers.grown_top, numbers.reveal_top) && near(numbers.room_top, 0),
              "the slat grows with the reveal while the room waits a slide");
    EMBED_CALL(gui_claim_state.numbers, GuiClaimStateCfg, .claim = &claim, .time_ms = 2900ull, .numbers = &numbers);
    cli_check(near(numbers.reveal_top, GUI_WHOLE) && near(numbers.room_top, GUI_WHOLE) && near(numbers.reveal_down, GUI_WHOLE),
              "a slide after the reveal the room is taken and the controls stand whole");

    // the pointer leaves the part: a linger, then the part goes
    g_test_clock_ms = 3000ull;
    EMBED_CALL(gui_claim_pointer.move, GuiClaimPointerCfg, .claim = &claim, .x = 500, .y = 400, .zone = GUI_CLAIM_NONE, .over_parts = 0);
    cli_check(claim.hide_deadline_ms == 3000ull + GUI_CLAIM_HIDE_DELAY_MS, "the pointer off the part begins the linger");
    g_test_clock_ms = 3100ull;
    EMBED_CALL(gui_claim_pointer.move, GuiClaimPointerCfg, .claim = &claim, .x = 500, .y = 30, .zone = GUI_CLAIM_TOP, .over_parts = 1);
    cli_check(claim.hide_deadline_ms == 0ull, "the pointer back over the part ends the linger");
    g_test_clock_ms = 3200ull;
    EMBED_CALL(gui_claim_pointer.move, GuiClaimPointerCfg, .claim = &claim, .x = 500, .y = 400, .zone = GUI_CLAIM_NONE, .over_parts = 0);
    g_test_clock_ms = 3700ull;
    cli_check(EMBED_CALL(gui_claim_state.step, GuiClaimStateCfg, .claim = &claim) && (claim.shown == GUI_CLAIM_NONE), "the linger up takes the part back");
    EMBED_CALL(gui_claim_state.numbers, GuiClaimStateCfg, .claim = &claim, .time_ms = 3700ull, .numbers = &numbers);
    cli_check(near(numbers.grown_top, GUI_WHOLE) && (numbers.reveal_top > (9 * GUI_WHOLE / 10)), "a part going goes as it grew");
    EMBED_CALL(gui_claim_state.numbers, GuiClaimStateCfg, .claim = &claim, .time_ms = 4100ull, .numbers = &numbers);
    cli_check(near(numbers.reveal_top, 0) && near(numbers.grown_top, 0), "a slide later the part is away");

    // a pass through the band: the pointer moves fast across it
    g_test_clock_ms = 5000ull;
    EMBED_CALL(gui_claim_pointer.move, GuiClaimPointerCfg, .claim = &claim, .x = 500, .y = 60, .zone = GUI_CLAIM_NONE, .over_parts = 0);
    g_test_clock_ms = 5010ull;
    EMBED_CALL(gui_claim_pointer.move, GuiClaimPointerCfg, .claim = &claim, .x = 500, .y = 20, .zone = GUI_CLAIM_TOP, .over_parts = 0);
    g_test_clock_ms = 5020ull;
    EMBED_CALL(gui_claim_pointer.move, GuiClaimPointerCfg, .claim = &claim, .x = 500, .y = 2, .zone = GUI_CLAIM_TOP, .over_parts = 0);
    cli_check(claim.show_deadline_ms == 5020ull + GUI_CLAIM_SHOW_DELAY_MS, "a pointer passing fast has its rest begun again at each move");
    g_test_clock_ms = 5030ull;
    EMBED_CALL(gui_claim_pointer.move, GuiClaimPointerCfg, .claim = &claim, .x = 500, .y = 1, .zone = GUI_CLAIM_TOP, .over_parts = 0);
    g_test_clock_ms = 5100ull;
    EMBED_CALL(gui_claim_pointer.move, GuiClaimPointerCfg, .claim = &claim, .x = 500, .y = 1, .zone = GUI_CLAIM_TOP, .over_parts = 0);
    cli_check(claim.show_deadline_ms == 5030ull + GUI_CLAIM_SHOW_DELAY_MS, "a pointer slowed keeps its rest from the move it slowed at");

    // another edge's part waits for the shown one to go
    g_test_clock_ms = 5400ull;
    cli_check(EMBED_CALL(gui_claim_state.step, GuiClaimStateCfg, .claim = &claim) && (claim.shown == GUI_CLAIM_TOP), "the rest up reveals the top again");
    g_test_clock_ms = 6000ull;
    EMBED_CALL(gui_claim_pointer.move, GuiClaimPointerCfg, .claim = &claim, .x = 500, .y = 790, .zone = GUI_CLAIM_BOTTOM, .over_parts = 0);
    g_test_clock_ms = 6300ull;
    cli_check(EMBED_CALL(gui_claim_state.step, GuiClaimStateCfg, .claim = &claim) && (claim.shown == GUI_CLAIM_NONE) && (claim.waiting == GUI_CLAIM_BOTTOM),
              "a rest at the foot sends the top away and waits");
    g_test_clock_ms = 6300ull + GUI_CLAIM_ANIMATION_MS;
    cli_check(EMBED_CALL(gui_claim_state.step, GuiClaimStateCfg, .claim = &claim) && (claim.shown == GUI_CLAIM_BOTTOM), "a slide later the foot's part comes");
    cli_check(claim.pulled_bottom == 500, "the foot's slat is pulled from the rest");

    // the controls' ghost shows as the pointer slows in their corner
    g_test_clock_ms = 7000ull;
    EMBED_CALL(gui_claim_on.set, GuiClaimOnCfg, .claim = &claim, .on = 0);
    cli_check(!claim.claimed && (claim.shown == GUI_CLAIM_NONE), "the claim let go takes every part back");
    g_test_clock_ms = 7000ull;
    EMBED_CALL(gui_claim_on.set, GuiClaimOnCfg, .claim = &claim, .on = 1);
    g_test_clock_ms = 8000ull;
    EMBED_CALL(gui_claim_pointer.move, GuiClaimPointerCfg, .claim = &claim, .x = 950, .y = 10, .zone = GUI_CLAIM_CONTROLS, .over_parts = 0);
    cli_check(!claim.near && (claim.preview_deadline_ms == 8000ull + (2ull * GUI_CLAIM_TRAIL_WINDOW_MS)), "a pointer come fast into the controls' corner waits to settle");
    g_test_clock_ms = 8020ull;
    EMBED_CALL(gui_claim_pointer.move, GuiClaimPointerCfg, .claim = &claim, .x = 950, .y = 10, .zone = GUI_CLAIM_CONTROLS, .over_parts = 0);
    cli_check(claim.near, "a pointer slow in the controls' corner shows their ghost at once");
    EMBED_CALL(gui_claim_state.numbers, GuiClaimStateCfg, .claim = &claim, .time_ms = 8300ull, .numbers = &numbers);
    cli_check(near(numbers.reveal_down, GUI_CLAIM_GHOST_DOWN) && near(numbers.controls_alpha, GUI_CLAIM_GHOST_ALPHA),
              "the ghost stands part of the way down, faint");
    g_test_clock_ms = 8300ull;
    cli_check(EMBED_CALL(gui_claim_state.step, GuiClaimStateCfg, .claim = &claim) && (claim.shown == GUI_CLAIM_CONTROLS), "the rest in the corner reveals the controls");
    EMBED_CALL(gui_claim_state.numbers, GuiClaimStateCfg, .claim = &claim, .time_ms = 8700ull, .numbers = &numbers);
    cli_check(near(numbers.reveal_down, GUI_WHOLE) && near(numbers.controls_alpha, GUI_WHOLE) && near(numbers.reveal_top, 0),
              "the controls come the whole way down alone, the bar staying away");
    g_test_clock_ms = 9000ull;
    EMBED_CALL(gui_claim_pointer.move, GuiClaimPointerCfg, .claim = &claim, .x = 500, .y = 10, .zone = GUI_CLAIM_TOP, .over_parts = 0);
    g_test_clock_ms = 9300ull;
    cli_check(EMBED_CALL(gui_claim_state.step, GuiClaimStateCfg, .claim = &claim) && (claim.shown == GUI_CLAIM_TOP), "the bar comes under the controls without waiting for them to go");
    EMBED_CALL(cli_claim_still.leave, GuiClaimStillCfg, .claim = &claim);
    cli_check((claim.show_deadline_ms == 0ull) && (claim.zone_now == GUI_CLAIM_NONE) && !claim.near, "the pointer gone out of the window leaves nothing waiting on its rest");

    // less motion asked for: a slide stands at once where it is bound, a part's rest still counted before it comes
    EMBED_CALL(cli_claim_still.init, GuiClaimStillCfg, .claim = &claim);
    EMBED_CALL(cli_claim_still.still, GuiClaimStillCfg, .claim = &claim, .still = 1);
    g_test_clock_ms = 10000ull;
    EMBED_CALL(gui_claim_on.set, GuiClaimOnCfg, .claim = &claim, .on = 1);
    EMBED_CALL(gui_claim_state.numbers, GuiClaimStateCfg, .claim = &claim, .time_ms = 10000ull, .numbers = &numbers);
    cli_check((numbers.claim == GUI_WHOLE) && !EMBED_CALL(gui_claim.moving, GuiClaimCfg, .claim_read = &claim, .time_ms = 10000ull), "less motion asked for, the claim stands whole at once");
    g_test_clock_ms = 10100ull;
    EMBED_CALL(gui_claim_pointer.move, GuiClaimPointerCfg, .claim = &claim, .x = 500, .y = 10, .zone = GUI_CLAIM_TOP, .over_parts = 0);
    g_test_clock_ms = 10100ull + GUI_CLAIM_SHOW_DELAY_MS;
    cli_check(EMBED_CALL(gui_claim_state.step, GuiClaimStateCfg, .claim = &claim) && (claim.shown == GUI_CLAIM_TOP), "the pointer's rest still reveals the bar");
    EMBED_CALL(gui_claim_state.numbers, GuiClaimStateCfg, .claim = &claim, .time_ms = 10100ull + GUI_CLAIM_SHOW_DELAY_MS, .numbers = &numbers);
    cli_check((numbers.reveal_top == GUI_WHOLE) && (numbers.room_top == GUI_WHOLE) && !EMBED_CALL(gui_claim.moving, GuiClaimCfg, .claim_read = &claim, .time_ms = 10100ull + GUI_CLAIM_SHOW_DELAY_MS),
              "and the bar and its room stand there at once");
    g_test_clock_ms = 11000ull;
    EMBED_CALL(gui_claim_on.set, GuiClaimOnCfg, .claim = &claim, .on = 0);
    EMBED_CALL(gui_claim_state.numbers, GuiClaimStateCfg, .claim = &claim, .time_ms = 11000ull, .numbers = &numbers);
    cli_check((numbers.claim == 0) && (numbers.reveal_top == 0) && !EMBED_CALL(gui_claim.moving, GuiClaimCfg, .claim_read = &claim, .time_ms = 11000ull), "let go, every part stands back at once");
    EMBED_CALL(cli_claim_still.still, GuiClaimStillCfg, .claim = &claim, .still = 0);
    g_test_clock_ms = 12000ull;
    EMBED_CALL(gui_claim_on.set, GuiClaimOnCfg, .claim = &claim, .on = 1);
    cli_check(EMBED_CALL(gui_claim.moving, GuiClaimCfg, .claim_read = &claim, .time_ms = 12100ull), "no longer asked for, the slides run");
    EMBED_CALL(cli_claim_still.still, GuiClaimStillCfg, .claim = &claim, .still = 1);
    EMBED_CALL(gui_claim_state.numbers, GuiClaimStateCfg, .claim = &claim, .time_ms = 12100ull, .numbers = &numbers);
    cli_check((numbers.claim == GUI_WHOLE) && !EMBED_CALL(gui_claim.moving, GuiClaimCfg, .claim_read = &claim, .time_ms = 12100ull), "asked for while a slide runs, it ends where it was bound");
    return cli_on_check();
}
