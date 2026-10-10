// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cubin_safe_check.c: cubin_safe held to one case a rule, each made from the machine file's own forms, then every cubin
// named read on the host and its verdict printed. Nothing here touches a device.
//
//     cubin_safe_check <machine file> [<cubin>...]
//
// One line a case and a cubin, and a last line with the checks. Exit 0 where every case gives its rule's verdict, 1
// where one does not, 2 where the machine file did not read. A cubin's verdict is reported and is no check: a cubin
// held off the part is the gate doing its work.
#include "../cubin_safe.h"
#include "../sass_assemble.h"

#include <stdio.h>
#include <string.h>

#define CHECK_CUBIN_BYTES 1048576u
#define CHECK_CODE_INSTRUCTIONS 4u

static SassMachine s_machine;
static unsigned char s_image[CHECK_CUBIN_BYTES];
static unsigned char s_code[16u * CHECK_CODE_INSTRUCTIONS];
static unsigned int s_checks;
static unsigned int s_failed;

static void check_put(unsigned char *bytes, unsigned long long value)
{
    for (unsigned int byte = 0u; byte < 8u; byte += 1u)
    {
        bytes[byte] = (unsigned char)((value >> (8u * byte)) & 0xffu);
    }
}

// `high` with its scheduler's bits set to the safe word's stall and wait, its barriers set to none
static unsigned long long check_safe_high(unsigned long long high)
{
    const unsigned long long scheduler = ~0ull << (SASS_STALL_FIRST - 64u);
    high &= ~scheduler;
    high |= (unsigned long long)SASS_STALL_LONGEST << (SASS_STALL_FIRST - 64u);
    high |= (unsigned long long)SASS_BARRIER_NONE << (SASS_WRITE_BARRIER_FIRST - 64u);
    high |= (unsigned long long)SASS_BARRIER_NONE << (SASS_READ_BARRIER_FIRST - 64u);
    high |= (unsigned long long)SASS_WAIT_EVERY << (SASS_WAIT_FIRST - 64u);
    return high;
}

// `high`, already holding the safe word, with its stall set to `stall`
static unsigned long long check_stalled(unsigned long long high, unsigned int stall)
{
    high &= ~(0xfull << (SASS_STALL_FIRST - 64u));
    return high | ((unsigned long long)stall << (SASS_STALL_FIRST - 64u));
}

// the instruction at `place` of s_code set to `low` and `high`
static void check_instruction(unsigned int place, unsigned long long low, unsigned long long high)
{
    check_put(&s_code[16u * place], low);
    check_put(&s_code[(16u * place) + 8u], high);
}

// a case held to cubin_safe for a kernel of `registers` registers, 0 where none is declared (rule 6)
static void check_verdict_allot(const char *name, unsigned int instructions, unsigned int registers,
                                unsigned int wanted)
{
    unsigned long long at = 0ull;
    const unsigned int verdict = cubin_safe(&s_machine, s_code, 16ull * instructions, registers, &at);
    s_checks += 1u;
    if (verdict != wanted)
    {
        s_failed += 1u;
    }
    printf("  %s %s: %s at %llu, where %s was wanted\n", (verdict == wanted) ? "held" : "FAILED", name,
           cubin_safe_name(verdict), at, cubin_safe_name(wanted));
}

static void check_verdict(const char *name, unsigned int instructions, unsigned int wanted)
{
    check_verdict_allot(name, instructions, 0u, wanted);
}

// the first form of the machine whose operation is `operation`, or NULL
static const SassForm *check_form(const char *operation)
{
    for (unsigned int at = 0u; at < s_machine.forms; at += 1u)
    {
        if (strcmp(s_machine.form[at].operation, operation) == 0)
        {
            return &s_machine.form[at];
        }
    }
    return NULL;
}

// the form of the machine seen as the instruction `text`, or NULL
static const SassForm *check_form_seen(const char *text)
{
    for (unsigned int at = 0u; at < s_machine.forms; at += 1u)
    {
        if (strcmp(s_machine.form[at].text, text) == 0)
        {
            return &s_machine.form[at];
        }
    }
    return NULL;
}

// `low` with its guard set to PT, the predicate always true: three bits of its number at 12 and the negation at 15
static unsigned long long check_unguarded(unsigned long long low)
{
    return (low & ~0xf000ull) | 0x7000ull;
}

// 1 where some form of the machine sits under the operation key `key`
static int check_key_known(unsigned long long key)
{
    for (unsigned int at = 0u; at < s_machine.forms; at += 1u)
    {
        if ((s_machine.form[at].low & SASS_OPERATION_MASK) == key)
        {
            return 1;
        }
    }
    return 0;
}

// an encoding of `form` with one operation bit past the key turned that no form holds, through `low`: 1, or 0 where
// every such turn is held by some form
static int check_unheld(const SassForm *form, unsigned long long *low)
{
    char text[256];
    for (unsigned int bit = 12u; bit < 64u; bit += 1u)
    {
        const unsigned long long turned = form->low ^ (1ull << bit);
        if (!sass_encoding_read(&s_machine, turned, check_safe_high(form->high), 0ull, text, sizeof(text)))
        {
            *low = turned;
            return 1;
        }
    }
    return 0;
}

static void check_cases(void)
{
    const SassForm *const exit = check_form("EXIT");
    const SassForm *const branch = check_form("BRA");
    const SassForm *const straight = check_form("IADD3");
    if ((exit == NULL) || (branch == NULL) || (straight == NULL))
    {
        s_checks += 1u;
        s_failed += 1u;
        printf("  FAILED: the machine file holds no EXIT, BRA or IADD3 to make the cases from\n");
        return;
    }
    // the machine file saw EXIT under a guard; every case's EXIT is given PT, which every thread takes
    const unsigned long long exit_low = check_unguarded(exit->low);
    const unsigned long long exit_high = check_safe_high(exit->high);
    check_instruction(0u, exit_low, exit_high);
    check_verdict("an EXIT alone", 1u, CUBIN_SAFE);
    check_verdict("no instruction", 0u, CUBIN_SAFE_SIZE);
    check_instruction(0u, exit->low, exit_high);
    check_verdict("an EXIT under @P0 and nothing after it", 1u, CUBIN_SAFE_EXIT);
    const SassForm *const never = check_form_seen("@P0 EXIT !PT");
    if (never != NULL)
    {
        check_instruction(0u, check_unguarded(never->low), check_safe_high(never->high));
        check_verdict("EXIT !PT, which no thread takes", 1u, CUBIN_SAFE_EXIT);
    }
    check_instruction(0u, exit_low, exit_high & ~(0xfull << (SASS_STALL_FIRST - 64u)));
    check_verdict("an EXIT with no stall", 1u, CUBIN_SAFE);
    check_instruction(0u, exit_low, exit_high & ~(0x3full << (SASS_WAIT_FIRST - 64u)));
    check_verdict("an EXIT waiting on nothing", 1u, CUBIN_SAFE_WAIT);
    check_instruction(0u, branch->low, check_safe_high(branch->high));
    check_instruction(1u, exit_low, exit_high);
    check_verdict("a branch before the EXIT", 2u, CUBIN_SAFE_CONTROL);
    check_instruction(0u, straight->low, check_safe_high(straight->high));
    check_verdict("an IADD3 with no EXIT after it", 1u, CUBIN_SAFE_EXIT);
    check_instruction(1u, exit_low, exit_high);
    check_verdict("an IADD3 then an EXIT", 2u, CUBIN_SAFE);
    // a stall short of a result's soonest read gives a wrong answer and never a kernel that does not return, and the
    // run channel asks below it
    check_instruction(0u, straight->low, check_stalled(check_safe_high(straight->high), 1u));
    check_verdict("an IADD3 stalled one cycle, then an EXIT", 2u, CUBIN_SAFE);
    // a fixed result's barrier is never released
    check_instruction(0u, straight->low,
                      check_safe_high(straight->high) & ~(7ull << (SASS_WRITE_BARRIER_FIRST - 64u)));
    check_verdict("an IADD3 setting write barrier 0", 2u, CUBIN_SAFE_BARRIER);
    // the self branch past the last exit is never reached and is not read
    check_instruction(0u, exit_low, exit_high);
    check_instruction(1u, branch->low, branch->high);
    check_verdict("an EXIT then the self branch", 2u, CUBIN_SAFE);
    unsigned long long unheld = 0ull;
    if (check_unheld(straight, &unheld))
    {
        check_instruction(0u, unheld, check_safe_high(straight->high));
        check_instruction(1u, exit_low, exit_high);
        check_verdict("one instruction no form holds, then an EXIT", 2u, CUBIN_SAFE);
        check_instruction(1u, unheld, check_safe_high(straight->high));
        check_instruction(2u, exit_low, exit_high);
        check_verdict("two instructions no form holds", 3u, CUBIN_SAFE_UNHELD);
    }
    for (unsigned long long key = 0ull; key <= SASS_OPERATION_MASK; key += 1ull)
    {
        if (!check_key_known(key))
        {
            check_instruction(0u, (straight->low & ~SASS_OPERATION_MASK) | key, check_safe_high(straight->high));
            check_instruction(1u, exit_low, exit_high);
            check_verdict("an operation key no form sits under", 2u, CUBIN_SAFE_KEY);
            break;
        }
    }
}

// rule 6 held to what it says of one launch: the driver's status on the stop event and the nanoseconds the host read it
static void check_launch(const char *name, int status, unsigned long long waited, unsigned int wanted)
{
    const unsigned int verdict = cubin_safe_launch(status, waited);
    s_checks += 1u;
    if (verdict != wanted)
    {
        s_failed += 1u;
        printf("  FAILED: %s: %s, where %s\n", name, cubin_safe_name(verdict), cubin_safe_name(wanted));
        return;
    }
    printf("  %s: %s\n", name, cubin_safe_name(verdict));
}

// rule 7 held to what it says of one launch: the part's fault stamp and the driver's status on the stop event
static void check_fault(const char *name, int part_faulted, int status, unsigned int wanted)
{
    const unsigned int verdict = cubin_safe_fault(part_faulted, status);
    s_checks += 1u;
    if (verdict != wanted)
    {
        s_failed += 1u;
        printf("  FAILED: %s: %s, where %s\n", name, cubin_safe_name(verdict), cubin_safe_name(wanted));
        return;
    }
    printf("  %s: %s\n", name, cubin_safe_name(verdict));
}

// the launch cases, which need no machine file
static void check_launches(void)
{
    check_launch("a launch reached inside the bound", 0, 1000ull, CUBIN_SAFE);
    check_launch("a launch the driver timed out", CUBIN_SAFE_STATUS_LAUNCH_TIMEOUT, 0ull, CUBIN_SAFE_LAUNCH_TIMEOUT);
    check_launch("a launch not reached yet, inside the bound", CUBIN_SAFE_STATUS_NOT_READY, CUBIN_SAFE_LAUNCH_BOUND,
                 CUBIN_SAFE);
    check_launch("a launch not reached past the bound", CUBIN_SAFE_STATUS_NOT_READY, CUBIN_SAFE_LAUNCH_BOUND + 1ull,
                 CUBIN_SAFE_LAUNCH_TIMEOUT);
    check_launch("a launch the part refused", 700, 0ull, CUBIN_SAFE);
    check_fault("a launch the part did not fault", 0, 0, CUBIN_SAFE);
    check_fault("a launch the part stamped a critical error", 1, 0, CUBIN_SAFE_LAUNCH_FAULT);
    check_fault("a launch the driver named an illegal address", 0, CUBIN_SAFE_STATUS_ILLEGAL_ADDRESS,
                CUBIN_SAFE_LAUNCH_FAULT);
    check_fault("a launch the driver named an illegal instruction", 0, CUBIN_SAFE_STATUS_ILLEGAL_INSTRUCTION,
                CUBIN_SAFE_LAUNCH_FAULT);
    check_fault("a launch the driver failed outright", 0, CUBIN_SAFE_STATUS_LAUNCH_FAILED, CUBIN_SAFE_LAUNCH_FAULT);
    check_fault("a launch timed out is no fault", 0, CUBIN_SAFE_STATUS_LAUNCH_TIMEOUT, CUBIN_SAFE);
}

// rule 6 against a real form of the machine: a register operand set past what a small kernel allots is held off the
// part. The whole fixed register field (16, 24, 32 in the low word, 64 in the high) is read however much of it the
// operand's run covered, and one instruction is enough, the verdict coming before any EXIT is sought
static void check_registers(void)
{
    static const unsigned int fields[4] = {16u, 24u, 32u, 64u};
    const SassForm *form = NULL;
    unsigned int field = 0u;
    for (unsigned int at = 0u; (form == NULL) && (at < s_machine.forms); at += 1u)
    {
        for (unsigned int run = 0u; (form == NULL) && (run < s_machine.form[at].runs); run += 1u)
        {
            const unsigned int operand = s_machine.form[at].run[run].operand;
            const unsigned int first = s_machine.form[at].run[run].first;
            if ((operand >= s_machine.form[at].operands) ||
                (s_machine.form[at].kind[operand] != SASS_OPERAND_REGISTER))
            {
                continue;
            }
            for (unsigned int which = 0u; which < 4u; which += 1u)
            {
                if ((first >= fields[which]) && (first < (fields[which] + 8u)))
                {
                    form = &s_machine.form[at];
                    field = fields[which];
                }
            }
        }
    }
    if (form == NULL)
    {
        s_checks += 1u;
        printf("  the machine holds no register operand in a known field: rule 6 unread here\n");
        return;
    }
    unsigned long long low = form->low;
    unsigned long long high = check_safe_high(form->high);
    // R40 in that field, a register an eight-register kernel never allotted
    if (field < 64u)
    {
        low = (low & ~(0xffull << field)) | (40ull << field);
    }
    else
    {
        high = (high & ~(0xffull << (field - 64u))) | (40ull << (field - 64u));
    }
    check_instruction(0u, low, high);
    check_verdict_allot("a form naming a register an 8-register kernel lacks", 1u, 8u, CUBIN_SAFE_REGISTER);
}

int main(int count, char **words)
{
    if ((count < 2) || !sass_machine_read(&s_machine, words[1]))
    {
        fprintf(stderr, "cubin_safe_check <machine file> [<cubin>...]\n");
        return 2;
    }
    check_launches();
    check_registers();
    check_cases();
    unsigned int safe = 0u;
    for (int at = 2; at < count; at += 1)
    {
        FILE *const file = fopen(words[at], "rb");
        const size_t size = (file != NULL) ? fread(s_image, 1u, sizeof(s_image), file) : 0u;
        if (file != NULL)
        {
            fclose(file);
        }
        unsigned long long where = 0ull;
        const unsigned int verdict = cubin_safe_image(&s_machine, s_image, size, 0u, &where);
        safe += (verdict == CUBIN_SAFE) ? 1u : 0u;
        printf("  %s: %s at byte %llu\n", words[at], cubin_safe_name(verdict), where);
    }
    if (count > 2)
    {
        printf("  %u of %d cubins safe to put to the part\n", safe, count - 2);
    }
    printf("cubin safe check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
