// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// standing_check.c: one form standing for the encodings that answer as it does, so that the working set holds one
// form for each answer and the widening's turns of a form's bits outside its operands do not fill it
//
//   alike      two forms answer alike where their relation, read order, signedness and a frame's reading are one
//   turned     a form holding its runs stands for an encoding holding none that answers alike and differs from it in
//              bits of the turned range outside its placed runs; one differing inside a placed run, in the key or in
//              the scheduler's word, or answering another relation, stands alone
//   runs       two forms holding the same runs and answering alike are one form; the same runs placed apart are two
//   folded     the set folded keeps the oldest of the forms one stands for, in their order
//
//     standing_check
#include "../../../../../../../src/cu/transpiler/lstar/parser/khw_write/khw_write_internal.h"

static unsigned int s_checks = 0u;
static unsigned int s_failed = 0u;

static void check_that(int held, const char *what)
{
    s_checks += 1u;
    if (held == 0)
    {
        s_failed += 1u;
        printf("  FAILED: %s\n", what);
    }
}

// an add form classified: the destination at bits 16 to 23, the first source at 24 to 31, the second at 32 to 39
static KhwAnswered add_form(unsigned long long low, unsigned long long high)
{
    KhwAnswered held;
    memset(&held, 0, sizeof(held));
    held.low = low;
    held.high = high;
    held.anchor = (unsigned int)LADDER_ADD;
    held.frame = (unsigned int)FRAME_RELATION_COUNT;
    held.phase = KHW_PHASE_CLASSIFIED;
    held.runs = 3u;
    held.first[0] = 16u;
    held.last[0] = 23u;
    held.place[0] = 2u;
    held.kind[0] = KHW_KIND_REGISTER;
    held.first[1] = 24u;
    held.last[1] = 31u;
    held.place[1] = 0u;
    held.kind[1] = KHW_KIND_REGISTER;
    held.first[2] = 32u;
    held.last[2] = 39u;
    held.place[2] = 1u;
    held.kind[2] = KHW_KIND_REGISTER;
    return held;
}

// an encoding discovered and not yet turned, answering add
static KhwAnswered add_found(unsigned long long low, unsigned long long high)
{
    KhwAnswered held;
    memset(&held, 0, sizeof(held));
    held.low = low;
    held.high = high;
    held.anchor = (unsigned int)LADDER_ADD;
    held.frame = (unsigned int)FRAME_RELATION_COUNT;
    held.phase = KHW_PHASE_DISCOVERED;
    return held;
}

int main(void)
{
    const KhwAnswered add = add_form(0x0000000700077210ull, 0x004fca0007ffe0ffull);
    KhwAnswered other = add;
    check_that(khw_answers_alike(&add, &other), "alike: a form answers alike with itself");
    other.swapped = 1;
    check_that(!khw_answers_alike(&add, &other), "alike: read swapped answers apart");
    other = add;
    other.signed_read = 1;
    check_that(!khw_answers_alike(&add, &other), "alike: read signed answers apart");
    other = add;
    other.anchor = (unsigned int)LADDER_TAKE;
    check_that(!khw_answers_alike(&add, &other), "alike: another relation answers apart");
    KhwAnswered number = add_found(0x0000001000057802ull, 0x000fca0000000f00ull);
    number.anchor = (unsigned int)LADDER_ANCHOR_COUNT;
    number.frame = (unsigned int)FRAME_NUMBER;
    number.reading = 0x10ull;
    KhwAnswered number_other = number;
    check_that(khw_answers_alike(&number, &number_other), "alike: a frame form answers alike with its reading");
    number_other.reading = 0x20ull;
    check_that(!khw_answers_alike(&number, &number_other), "alike: a frame form read another constant answers apart");

    // bit 40 lies in the turned range outside the placed runs; bit 20 inside the destination; bit 3 in the key; bit
    // 110 in the scheduler's word
    const KhwAnswered turned_outside = add_found(add.low ^ (1ull << 40u), add.high);
    const KhwAnswered turned_inside = add_found(add.low ^ (1ull << 20u), add.high);
    const KhwAnswered turned_key = add_found(add.low ^ (1ull << 3u), add.high);
    const KhwAnswered turned_scheduler = add_found(add.low, add.high ^ (1ull << (110u - 64u)));
    const KhwAnswered turned_twice = add_found(add.low ^ (1ull << 40u) ^ (1ull << 44u), add.high);
    check_that(khw_form_stands_for(&add, &turned_outside), "turned: a bit outside the placed runs is the form's");
    check_that(khw_form_stands_for(&add, &turned_twice), "turned: two bits outside the placed runs are the form's");
    check_that(!khw_form_stands_for(&add, &turned_inside), "turned: a bit inside a placed run stands alone");
    check_that(!khw_form_stands_for(&add, &turned_key), "turned: a bit of the key stands alone");
    check_that(!khw_form_stands_for(&add, &turned_scheduler), "turned: a bit of the scheduler's word stands alone");
    KhwAnswered turned_take = turned_outside;
    turned_take.anchor = (unsigned int)LADDER_TAKE;
    check_that(!khw_form_stands_for(&add, &turned_take), "turned: an encoding answering another relation stands alone");
    check_that(!khw_form_stands_for(&turned_outside, &turned_twice),
               "turned: a form holding no runs stands for nothing");

    const KhwAnswered add_again = add_form(add.low ^ (1ull << 40u), add.high);
    check_that(khw_form_stands_for(&add, &add_again), "runs: the same runs answering alike are one form");
    KhwAnswered add_placed_apart = add_again;
    add_placed_apart.place[1] = 1u;
    add_placed_apart.place[2] = 0u;
    check_that(!khw_form_stands_for(&add, &add_placed_apart), "runs: the same runs placed apart are two forms");
    KhwAnswered add_wider = add_again;
    add_wider.last[2] = 40u;
    check_that(!khw_form_stands_for(&add, &add_wider), "runs: a run of another width is another form");
    KhwAnswered add_fewer = add_again;
    add_fewer.runs = 2u;
    check_that(!khw_form_stands_for(&add, &add_fewer), "runs: fewer runs are another form");

    KhwAnswered set[6];
    set[0] = add;
    set[1] = turned_outside;
    set[2] = number;
    set[3] = add_again;
    set[4] = turned_twice;
    set[5] = add_placed_apart;
    const unsigned int kept = khw_forms_folded(set, 6u);
    check_that(kept == 3u, "folded: three of six stand");
    check_that((set[0].low == add.low) && (set[0].runs == 3u), "folded: the oldest add stands first");
    check_that(set[1].frame == (unsigned int)FRAME_NUMBER, "folded: the number form stands second");
    check_that((kept == 3u) && (set[2].place[1] == 1u), "folded: the add placed apart stands third");
    check_that(khw_form_stood_for(set, kept, &turned_twice), "folded: the set stands for a turn of its add");
    KhwAnswered take = add_found(add.low ^ (1ull << 40u), add.high);
    take.anchor = (unsigned int)LADDER_TAKE;
    check_that(!khw_form_stood_for(set, kept, &take), "folded: the set stands for no take");

    printf("standing_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
