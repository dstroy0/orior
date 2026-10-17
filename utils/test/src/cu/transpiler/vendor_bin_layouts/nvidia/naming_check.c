// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// naming_check.c: the vendor's writer's texts held on forms written by hand (The ladder's words asked, Open 6.4)
//
//   name     a form is named from the table by its relation, the count of its sources and its kind, none where no kind
//            row reads the relation
//   mark     a take form carries the vendor's mark on the operand at the relation's second word, in whatever bit
//            order the operands stand
//   flag     a run of one bit with no place prints nothing, and is kept past the operand count; a wider run with no
//            place prints as a register
//   guard    a predicate run prints before the operation as the vendor writes a guard
//   kept     the form kept in a machine holds each printed operand's run under its operand and the mark on its operand
//
//     naming_check <mnemonics>
#include "../../../../../../../src/cu/transpiler/vendor_bin_layouts/nvidia/khw_machine_write/khw_machine_write_internal.h"

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

// one run of a hand form: its bits, its place and its kind
typedef struct
{
    unsigned int first;
    unsigned int last;
    unsigned int place;
    unsigned int kind;
} HandRun;

// a form written by hand: `relation` over the encoding `low` and `high` with `count` runs
static void hand_form(KhwForm *form, const char *relation, unsigned long long low, unsigned long long high,
                      const HandRun *runs, unsigned int count)
{
    memset(form, 0, sizeof(*form));
    snprintf(form->relation, sizeof(form->relation), "%s", relation);
    form->low = low;
    form->high = high;
    form->runs = count;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        form->first[at] = runs[at].first;
        form->last[at] = runs[at].last;
        form->place[at] = runs[at].place;
        form->kind[at] = runs[at].kind;
    }
}

// the text the writer composes for `form`, held against `expected`
static void check_text(const KhwForm *form, const char *expected, const char *what)
{
    char text[SASS_MACHINE_TEXT];
    khw_text_composed(form, text, sizeof(text));
    check_that(strcmp(text, expected) == 0, what);
    if (strcmp(text, expected) != 0)
    {
        printf("    read %s\n", text);
    }
}

static SassMachine s_test_machine;

int main(int count, char **words)
{
    if (count < 2)
    {
        printf("naming_check <mnemonics>\n");
        return 2;
    }
    check_that(khw_pieces_read(words[1]), "the vendor's table is read");

    // the kernel's IADD3 at the slot, its registers R7, R0 and R7 in the fields 16-23, 24-31 and 32-39, and bit 72 a
    // negate bit, clear
    const unsigned long long add_low = 0x0000000700077210ull;
    const unsigned long long add_high = 0x004fca0007ffe0ffull;
    const HandRun add_runs[4] = {{16u, 23u, 2u, KHW_KIND_REGISTER},
                                 {24u, 31u, 0u, KHW_KIND_REGISTER},
                                 {32u, 39u, 1u, KHW_KIND_REGISTER},
                                 {72u, 72u, SASS_RUN_NO_PLACE, KHW_KIND_REGISTER}};
    KhwForm add;
    hand_form(&add, "ladder.add", add_low, add_high, add_runs, 4u);
    check_text(&add, "IADD3 R7, R0, R7", "an add form reads its three registers, the flag bit printing nothing");

    // the same bits read as take: the mark on the operand at the relation's second word
    KhwForm take;
    hand_form(&take, "ladder.take", add_low, add_high | (1ull << 8u), add_runs, 4u);
    check_text(&take, "IADD3 R7, R0, -R7", "a take form carries the mark on its second word's operand");
    const HandRun swapped_runs[4] = {{16u, 23u, 2u, KHW_KIND_REGISTER},
                                     {24u, 31u, 1u, KHW_KIND_REGISTER},
                                     {32u, 39u, 0u, KHW_KIND_REGISTER},
                                     {72u, 72u, SASS_RUN_NO_PLACE, KHW_KIND_REGISTER}};
    KhwForm swapped;
    hand_form(&swapped, "ladder.take", add_low, add_high | (1ull << 8u), swapped_runs, 4u);
    check_text(&swapped, "IADD3 R7, -R0, R7", "and on the second word where it stands first in the bits");

    // the lane form with its special register, a run of eight bits with no place, printed as a register
    const HandRun lane_runs[2] = {{16u, 23u, 0u, KHW_KIND_REGISTER}, {72u, 79u, SASS_RUN_NO_PLACE, KHW_KIND_REGISTER}};
    KhwForm lane;
    hand_form(&lane, "frame.lane", 0x0000000000077919ull, 0x000e240000002100ull, lane_runs, 2u);
    check_text(&lane, "S2R R7, R33", "a wide run with no place prints as a register");

    // the block size read from the constant bank: the offset run and the bank run, both constant runs with no
    // place, printed once as the bank's operand by the offset run
    const HandRun dim_runs[3] = {{16u, 23u, 0u, KHW_KIND_REGISTER},
                                 {38u, 53u, SASS_RUN_NO_PLACE, KHW_KIND_CONSTANT},
                                 {54u, 58u, SASS_RUN_NO_PLACE, KHW_KIND_CONSTANT}};
    KhwForm dim;
    hand_form(&dim, "frame.dim", 0x0000000000017a02ull, 0x000fe40000000f00ull, dim_runs, 3u);
    check_text(&dim, "MOV R1, c[0x0][0x0]",
               "a dim form prints the constant bank's operand from its offset and bank runs");
    KhwForm banked;
    hand_form(&banked, "frame.dim", 0x0040000000017a02ull | (0x28ull << 38u), 0x000fe40000000f00ull, dim_runs, 3u);
    check_text(&banked, "MOV R1, c[0x1][0x28]", "the bank and the offset read from the encoding's bits");

    // a parameter's word read from the constant bank: its offset run a place, printed from the parameters' base and
    // the place among them its reading names
    const HandRun parameter_runs[2] = {{16u, 23u, 1u, KHW_KIND_REGISTER}, {38u, 53u, 0u, KHW_KIND_CONSTANT}};
    KhwForm parameter;
    hand_form(&parameter, "frame.parameter", 0x0000000000017a02ull | (0x170ull << 38u), 0x000fe40000000f00ull,
              parameter_runs, 2u);
    parameter.reading = 0x10ull;
    check_text(&parameter, "MOV R1, c[0x0][0x160+0x10]",
               "a parameter form prints its offset as the parameters' base and its place among them");

    // a test: the predicate it writes in the field at 81 is an operand, the answer's place, and the predicates it
    // reads at 84 and 87, PT, are operands with no place; the relation read unsigned carries U32 and signed nothing
    khw_guard_field_set(12u, 3u);
    const HandRun test_runs[5] = {{24u, 31u, 0u, KHW_KIND_REGISTER},
                                  {32u, 39u, 1u, KHW_KIND_REGISTER},
                                  {81u, 83u, 2u, KHW_KIND_PREDICATE},
                                  {84u, 86u, SASS_RUN_NO_PLACE, KHW_KIND_PREDICATE},
                                  {87u, 89u, SASS_RUN_NO_PLACE, KHW_KIND_PREDICATE}};
    KhwForm test;
    hand_form(&test, "ladder.at_least", 0x000000070000720cull, 0x0000000003f00000ull, test_runs, 5u);
    test.signedness_asked = 1;
    check_text(&test, "ISETP.GE.U32.AND P0, PT, R0, R7, PT",
               "a test form names its test, its unsigned reading and its join, the predicates it writes and reads "
               "operands in their order");
    test.signed_read = 1;
    check_text(&test, "ISETP.GE.AND P0, PT, R0, R7, PT", "and read signed carries no qualifier");

    // a guarded exit
    const HandRun exit_runs[1] = {{12u, 15u, 0u, KHW_KIND_PREDICATE}};
    KhwForm guarded;
    hand_form(&guarded, "frame.exit", 0x000000000000094dull, 0x000fea0003800000ull, exit_runs, 1u);
    check_text(&guarded, "@P0 EXIT", "a predicate run prints before the operation");

    // the forms kept in a machine: the flag past the operand count, the mark on its operand
    memset(&s_test_machine, 0, sizeof(s_test_machine));
    char text[SASS_MACHINE_TEXT];
    khw_text_composed(&add, text, sizeof(text));
    check_that(khw_form_kept(&s_test_machine, &add, text), "the add form is kept");
    const SassForm *const kept_add = &s_test_machine.form[0];
    check_that((kept_add->runs == 4u) && (kept_add->run[3].operand == SASS_MACHINE_OPERANDS) &&
                   (kept_add->run[0].operand == 0u) && (kept_add->run[1].operand == 1u) &&
                   (kept_add->run[2].operand == 2u),
               "its three registers are the operands 0 to 2 and the flag stands past the operand count");
    khw_text_composed(&take, text, sizeof(text));
    check_that(khw_form_kept(&s_test_machine, &take, text), "the take form is kept");
    const SassForm *const kept_take = &s_test_machine.form[1];
    check_that((kept_take->operands == 3u) && (kept_take->mark[2] == SASS_MARK_NEGATE) &&
                   (kept_take->mark[1] == SASS_MARK_NONE),
               "its third operand carries the negate mark and its second none");
    khw_text_composed(&parameter, text, sizeof(text));
    check_that(khw_form_kept(&s_test_machine, &parameter, text), "the parameter form is kept");
    char placed[SASS_MACHINE_TEXT];
    check_that(sass_text_placed(&s_test_machine.form[2], placed, sizeof(placed)) &&
                   (strcmp(placed, "MOV {1}, c[0x0][0x160+{0}]") == 0),
               "and its placed text fills the place past the parameters' base");
    printf("naming_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
