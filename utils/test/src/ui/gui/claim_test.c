// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// claim_test.c: the claim's check: a slide runs from its number to the next over one slide on the ease curve, the
// claim made runs its number to one, a rest in an edge's band reveals the edge's part and a pass through it does not,
// the part lingers after the pointer leaves and goes, one edge's part waits for another's to go, the controls' ghost
// shows as the pointer slows in their corner, and the slats grow from the point of rest
#include "../../../../../ui/gui/src/claim.h"
#include "../cli/cli_check.h"

static int near(int one, int other)
{
    const int apart = one - other;
    return (apart < (GUI_WHOLE / 50)) && (apart > -(GUI_WHOLE / 50));
}

int main(void)
{
    cli_check((gui_ease(0) == 0) && (gui_ease(GUI_WHOLE) == GUI_WHOLE), "the ease curve runs from 0 to 1");
    cli_check((gui_ease(GUI_WHOLE / 2) > (7 * GUI_WHOLE / 10)) && (gui_ease(GUI_WHOLE / 2) < (9 * GUI_WHOLE / 10)), "the ease curve is past the middle halfway along");
    GuiSlide slide = {0, 0, 0ull, 0ull};
    gui_slide_to(&slide, GUI_WHOLE, 1000ull, 0ull);
    cli_check(near(gui_slide_at(&slide, 1000ull), 0), "a slide begun stands at its start");
    cli_check(gui_slide_running(&slide, 1100ull) && (gui_slide_at(&slide, 1100ull) > (GUI_WHOLE / 5)) && (gui_slide_at(&slide, 1100ull) < (4 * GUI_WHOLE / 5)),
              "a third of the way through, a slide stands between its ends");
    cli_check(near(gui_slide_at(&slide, 1300ull), GUI_WHOLE) && !gui_slide_running(&slide, 1300ull), "a slide ends at its end");
    gui_slide_to(&slide, 0, 1150ull, 0ull);
    cli_check(gui_slide_at(&slide, 1150ull) > (3 * GUI_WHOLE / 10), "a slide sent back begins where it stands");
    gui_slide_to(&slide, GUI_WHOLE, 2000ull, GUI_CLAIM_SLIDE);
    cli_check(near(gui_slide_at(&slide, 2200ull), 0) && gui_slide_running(&slide, 2200ull), "a slide delayed waits before it runs");
    cli_check(near(gui_slide_at(&slide, 2600ull), GUI_WHOLE), "a slide delayed ends a delay and a slide after it begins");

    GuiClaim claim;
    gui_claim_made(&claim);
    GuiClaimNumbers numbers;
    gui_claim_numbers(&claim, 0ull, &numbers);
    cli_check(!claim.claimed && near(numbers.claim, 0) && near(numbers.grown_top, GUI_WHOLE) && near(numbers.controls_alpha, GUI_WHOLE),
              "a claim made does not hold, its slats whole and the controls whole");
    cli_check(gui_claim_due(&claim, 0ull) == 0ull, "a claim at rest wants no clock");
    gui_claim_set(&claim, 1, 1000ull);
    cli_check(claim.claimed && gui_claim_moving(&claim, 1100ull) && (gui_claim_due(&claim, 1100ull) != 0ull), "the claim made slides and wants the clock");
    gui_claim_numbers(&claim, 1300ull, &numbers);
    cli_check(near(numbers.claim, GUI_WHOLE) && !gui_claim_moving(&claim, 1300ull), "a slide later the claim holds whole");

    // the zones of a 1000 by 800 window at scale 1 with controls 106 wide: the band is three gaps, thirty-six
    cli_check(gui_claim_zone(1000u, 800u, 1.0f, 106, 500, 10) == GUI_CLAIM_TOP, "the head's band is the top");
    cli_check(gui_claim_zone(1000u, 800u, 1.0f, 106, 950, 10) == GUI_CLAIM_CONTROLS, "the corner past the controls' width is theirs");
    cli_check(gui_claim_zone(1000u, 800u, 1.0f, 106, 500, 790) == GUI_CLAIM_BOTTOM, "the foot's band is the bottom");
    cli_check(gui_claim_zone(1000u, 800u, 1.0f, 106, 10, 400) == GUI_CLAIM_LEFT, "the left's band is the left");
    cli_check(gui_claim_zone(1000u, 800u, 1.0f, 106, 990, 400) == GUI_CLAIM_RIGHT, "the right's band is the right");
    cli_check(gui_claim_zone(1000u, 800u, 1.0f, 106, 500, 400) == GUI_CLAIM_NONE, "the middle is no edge's");

    // a rest in the top band reveals the bar; a pass through does not
    gui_claim_moved(&claim, 500, 10, GUI_CLAIM_TOP, 0, 2000ull);
    cli_check((claim.rest_due == 2000ull + GUI_CLAIM_RESTS) && (claim.rest_zone == GUI_CLAIM_TOP), "a pointer in the top band begins its rest");
    cli_check(!gui_claim_stepped(&claim, 2100ull) && (claim.shown == GUI_CLAIM_NONE), "before the rest is up nothing shows");
    cli_check(gui_claim_stepped(&claim, 2300ull) && (claim.shown == GUI_CLAIM_TOP), "the rest up reveals the top");
    cli_check(claim.pulled_top == 500, "the top's slat is pulled from where the pointer rested");
    gui_claim_numbers(&claim, 2300ull, &numbers);
    cli_check(near(numbers.reveal_top, 0) && near(numbers.grown_top, 0), "the reveal begins at 0 and the slat ungrown");
    gui_claim_numbers(&claim, 2450ull, &numbers);
    cli_check((numbers.reveal_top > (3 * GUI_WHOLE / 10)) && near(numbers.grown_top, numbers.reveal_top) && near(numbers.room_top, 0),
              "the slat grows with the reveal while the room waits a slide");
    gui_claim_numbers(&claim, 2900ull, &numbers);
    cli_check(near(numbers.reveal_top, GUI_WHOLE) && near(numbers.room_top, GUI_WHOLE) && near(numbers.reveal_down, GUI_WHOLE),
              "a slide after the reveal the room is taken and the controls stand whole");

    // the pointer leaves the part: a linger, then the part goes
    gui_claim_moved(&claim, 500, 400, GUI_CLAIM_NONE, 0, 3000ull);
    cli_check(claim.linger_due == 3000ull + GUI_CLAIM_LINGER, "the pointer off the part begins the linger");
    gui_claim_moved(&claim, 500, 30, GUI_CLAIM_TOP, 1, 3100ull);
    cli_check(claim.linger_due == 0ull, "the pointer back over the part ends the linger");
    gui_claim_moved(&claim, 500, 400, GUI_CLAIM_NONE, 0, 3200ull);
    cli_check(gui_claim_stepped(&claim, 3700ull) && (claim.shown == GUI_CLAIM_NONE), "the linger up takes the part back");
    gui_claim_numbers(&claim, 3700ull, &numbers);
    cli_check(near(numbers.grown_top, GUI_WHOLE) && (numbers.reveal_top > (9 * GUI_WHOLE / 10)), "a part going goes as it grew");
    gui_claim_numbers(&claim, 4100ull, &numbers);
    cli_check(near(numbers.reveal_top, 0) && near(numbers.grown_top, 0), "a slide later the part is away");

    // a pass through the band: the pointer moves fast across it
    gui_claim_moved(&claim, 500, 60, GUI_CLAIM_NONE, 0, 5000ull);
    gui_claim_moved(&claim, 500, 20, GUI_CLAIM_TOP, 0, 5010ull);
    gui_claim_moved(&claim, 500, 2, GUI_CLAIM_TOP, 0, 5020ull);
    cli_check(claim.rest_due == 5020ull + GUI_CLAIM_RESTS, "a pointer passing fast has its rest begun again at each move");
    gui_claim_moved(&claim, 500, 1, GUI_CLAIM_TOP, 0, 5030ull);
    gui_claim_moved(&claim, 500, 1, GUI_CLAIM_TOP, 0, 5100ull);
    cli_check(claim.rest_due == 5030ull + GUI_CLAIM_RESTS, "a pointer slowed keeps its rest from the move it slowed at");

    // another edge's part waits for the shown one to go
    cli_check(gui_claim_stepped(&claim, 5400ull) && (claim.shown == GUI_CLAIM_TOP), "the rest up reveals the top again");
    gui_claim_moved(&claim, 500, 790, GUI_CLAIM_BOTTOM, 0, 6000ull);
    cli_check(gui_claim_stepped(&claim, 6300ull) && (claim.shown == GUI_CLAIM_NONE) && (claim.waiting == GUI_CLAIM_BOTTOM),
              "a rest at the foot sends the top away and waits");
    cli_check(gui_claim_stepped(&claim, 6300ull + GUI_CLAIM_SLIDE) && (claim.shown == GUI_CLAIM_BOTTOM), "a slide later the foot's part comes");
    cli_check(claim.pulled_bottom == 500, "the foot's slat is pulled from the rest");

    // the controls' ghost shows as the pointer slows in their corner
    gui_claim_set(&claim, 0, 7000ull);
    cli_check(!claim.claimed && (claim.shown == GUI_CLAIM_NONE), "the claim let go takes every part back");
    gui_claim_set(&claim, 1, 7000ull);
    gui_claim_moved(&claim, 950, 10, GUI_CLAIM_CONTROLS, 0, 8000ull);
    cli_check(!claim.near && (claim.near_due == 8000ull + (2ull * GUI_CLAIM_SPAN)), "a pointer come fast into the controls' corner waits to settle");
    gui_claim_moved(&claim, 950, 10, GUI_CLAIM_CONTROLS, 0, 8020ull);
    cli_check(claim.near, "a pointer slow in the controls' corner shows their ghost at once");
    gui_claim_numbers(&claim, 8300ull, &numbers);
    cli_check(near(numbers.reveal_down, GUI_CLAIM_GHOST_DOWN) && near(numbers.controls_alpha, GUI_CLAIM_GHOST_ALPHA),
              "the ghost stands part of the way down, faint");
    cli_check(gui_claim_stepped(&claim, 8300ull) && (claim.shown == GUI_CLAIM_CONTROLS), "the rest in the corner reveals the controls");
    gui_claim_numbers(&claim, 8700ull, &numbers);
    cli_check(near(numbers.reveal_down, GUI_WHOLE) && near(numbers.controls_alpha, GUI_WHOLE) && near(numbers.reveal_top, 0),
              "the controls come the whole way down alone, the bar staying away");
    gui_claim_moved(&claim, 500, 10, GUI_CLAIM_TOP, 0, 9000ull);
    cli_check(gui_claim_stepped(&claim, 9300ull) && (claim.shown == GUI_CLAIM_TOP), "the bar comes under the controls without waiting for them to go");
    gui_claim_left(&claim);
    cli_check((claim.rest_due == 0ull) && (claim.zone_now == GUI_CLAIM_NONE) && !claim.near, "the pointer gone out of the window leaves nothing waiting on its rest");

    // less motion asked for: a slide stands at once where it is bound, a part's rest still counted before it comes
    gui_claim_made(&claim);
    gui_claim_still(&claim, 1);
    gui_claim_set(&claim, 1, 10000ull);
    gui_claim_numbers(&claim, 10000ull, &numbers);
    cli_check((numbers.claim == GUI_WHOLE) && !gui_claim_moving(&claim, 10000ull), "less motion asked for, the claim stands whole at once");
    gui_claim_moved(&claim, 500, 10, GUI_CLAIM_TOP, 0, 10100ull);
    cli_check(gui_claim_stepped(&claim, 10100ull + GUI_CLAIM_RESTS) && (claim.shown == GUI_CLAIM_TOP), "the pointer's rest still reveals the bar");
    gui_claim_numbers(&claim, 10100ull + GUI_CLAIM_RESTS, &numbers);
    cli_check((numbers.reveal_top == GUI_WHOLE) && (numbers.room_top == GUI_WHOLE) && !gui_claim_moving(&claim, 10100ull + GUI_CLAIM_RESTS),
              "and the bar and its room stand there at once");
    gui_claim_set(&claim, 0, 11000ull);
    gui_claim_numbers(&claim, 11000ull, &numbers);
    cli_check((numbers.claim == 0) && (numbers.reveal_top == 0) && !gui_claim_moving(&claim, 11000ull), "let go, every part stands back at once");
    gui_claim_still(&claim, 0);
    gui_claim_set(&claim, 1, 12000ull);
    cli_check(gui_claim_moving(&claim, 12100ull), "no longer asked for, the slides run");
    gui_claim_still(&claim, 1);
    gui_claim_numbers(&claim, 12100ull, &numbers);
    cli_check((numbers.claim == GUI_WHOLE) && !gui_claim_moving(&claim, 12100ull), "asked for while a slide runs, it ends where it was bound");
    return cli_checked();
}
