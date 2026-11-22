// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// fixed.c: the fixed lines of a SASS ruleset's head. Each line is the part's answer in its .ksc, the line the ruleset
// gave before, or absent. The module also puts a fixed register's placeholder into a form's text where the text holds
// the register's numeric name.
#include "fixed.h"
#include "../../../../src/platform/platform.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "../../../parsers/line_read.h"
#include "../../krf/krf.h"
#include "../../ksc/ksc.h"
#include "verba_scribo/verba_scribo.h"

// The four fixed registers the kernel the system accepted leaves at its probe place. The lines come in this order.
static const char *const s_fixed_names[] = {"zero", "lane_number", "record", "index"};
#define KRS_FIXED_NAMES ((unsigned int)(sizeof(s_fixed_names) / sizeof(s_fixed_names[0])))

// The max fixed lines a ruleset gives, and the longest line.
#define KRS_FIXED_GIVEN_MAX 64u
#define KRS_FIXED_LINE 256u

// A fixed line the ruleset gave before this run: the register's name and the whole line.
typedef struct
{
    char name[64];
    char line[KRS_FIXED_LINE];
} KrsFixedGiven;

// A fixed register as the head names it: its name and its register text, as `R4`.
typedef struct
{
    char name[64];
    char text[32];
} KrsFixedRegister;

static KrsFixedRegister s_register[KRS_FIXED_GIVEN_MAX];
static unsigned int s_registers;

// Read every `fixed <name> = ...` line the ruleset at `krs_path` gave into `given`, at max KRS_FIXED_GIVEN_MAX.
// Return the count. Return 0 if there is no path or no file.
static unsigned int fixed_given_read(const char *krs_path, KrsFixedGiven *given)
{
    const unsigned char *file_bytes = NULL;
    unsigned long long file_size = 0ULL;
    unsigned long long file_at = 0ULL;
    unsigned int count = 0u;
    if ((krs_path == NULL) || !qry_member_file_blob(krs_path, &file_bytes, &file_size))
    {
        return 0u;
    }
    char line[KRS_FIXED_LINE];
    while (line_read(file_bytes, file_size, &file_at, line, sizeof(line)) && (count < KRS_FIXED_GIVEN_MAX))
    {
        char type[64];
        char name[64];
        const char *at = line;
        if (!line_word(&at, type, sizeof(type)) || !line_word(&at, name, sizeof(name)) ||
            (!mmgr_text_equal(type, "fixed", PLATFORM_TEXT_TO_TERMINATOR)))
        {
            continue;
        }
        line[mmgr_text_span_not(line, "\r\n", PLATFORM_TEXT_TO_TERMINATOR)] = '\0';
        {
            VerbaLinea buffer_line = mmgr_line_over(given[count].name, sizeof(given[count].name));
            mmgr_line_put(&buffer_line, name);
            (void)mmgr_line_finish(&buffer_line);
        }
        {
            VerbaLinea buffer_line = mmgr_line_over(given[count].line, sizeof(given[count].line));
            mmgr_line_put(&buffer_line, line);
            (void)mmgr_line_finish(&buffer_line);
        }
        count += 1u;
    }
    return count;
}

// Return the line the ruleset gave `name` before this run, of the `count` lines in `given`. Return NULL if it gave
// none.
static const char *fixed_given_of(const KrsFixedGiven *given, unsigned int count, const char *name)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (mmgr_text_equal(given[at].name, name, PLATFORM_TEXT_TO_TERMINATOR))
        {
            return given[at].line;
        }
    }
    return NULL;
}

// Return 1 if `name` is one of the four names the kernel's probe place answers for.
static int fixed_is_placeholder_name(const char *name)
{
    for (unsigned int at = 0u; at < KRS_FIXED_NAMES; at += 1u)
    {
        if (mmgr_text_equal(s_fixed_names[at], name, PLATFORM_TEXT_TO_TERMINATOR))
        {
            return 1;
        }
    }
    return 0;
}

// Read the part's answers for the four fixed registers from the .ksc at `ksc_path`. Set `answered` and `number` for
// each name, and put the top value of a register field into `top`.
static void fixed_answers_read(const char *ksc_path, int *answered, unsigned long long *number, unsigned long long *top)
{
    static KscFile s_file;
    static KscRow s_row;
    *top = 0ULL;
    ksc_read(&s_file, ksc_path);
    for (unsigned int line = 0u; line < s_file.lines; line += 1u)
    {
        unsigned long long first = 0ULL;
        unsigned long long bits = 0ULL;
        const char *read = s_file.line[line];
        if (line_characters(&read, "field") && line_characters(&read, "rd ") && line_number(&read, 0, &first) &&
            line_number(&read, 0, &bits))
        {
            *top = (bits >= 64u) ? 0xffffffffffffffffULL : ((1ULL << bits) - 1ULL);
            continue;
        }
        if (!ksc_row_read(s_file.line[line], "run", &s_row) ||
            (!mmgr_text_equal(s_row.answer, "answers", PLATFORM_TEXT_TO_TERMINATOR)) ||
            (!mmgr_text_starts(s_row.question, "fixed ", PLATFORM_TEXT_TO_TERMINATOR)))
        {
            continue;
        }
        for (unsigned int name = 0u; name < KRS_FIXED_NAMES; name += 1u)
        {
            if (mmgr_text_equal(s_row.question + 6, s_fixed_names[name], PLATFORM_TEXT_TO_TERMINATOR))
            {
                answered[name] = 1;
                number[name] = s_row.value;
            }
        }
    }
    ksc_free(&s_file);
}

// Write the fixed line of the answered register `name` into `line`. The register is RZ at the top value.
static void fixed_line_of(unsigned int name, unsigned long long value, unsigned long long top, char *line, size_t room)
{
    if ((top != 0ULL) && (value == top))
    {
        VerbaLinea buffer_line = mmgr_line_over(line, room);
        mmgr_line_put(&buffer_line, "fixed ");
        mmgr_line_put(&buffer_line, s_fixed_names[name]);
        mmgr_line_put(&buffer_line, " = RZ");
        (void)mmgr_line_finish(&buffer_line);
    }
    else
    {
        VerbaLinea buffer_line = mmgr_line_over(line, room);
        mmgr_line_put(&buffer_line, "fixed ");
        mmgr_line_put(&buffer_line, s_fixed_names[name]);
        mmgr_line_put(&buffer_line, " = R");
        mmgr_line_decimal(&buffer_line, value, 1u);
        (void)mmgr_line_finish(&buffer_line);
    }
}

// Keep the register of the fixed line `line` under `name` in s_register, if the line reads `fixed <name> = <text>`.
static void fixed_register_kept(const char *name, const char *line)
{
    const char *const equals = mmgr_text_find(line, "= ", PLATFORM_TEXT_TO_TERMINATOR);
    if ((equals == NULL) || (s_registers >= KRS_FIXED_GIVEN_MAX))
    {
        return;
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(s_register[s_registers].name, sizeof(s_register[s_registers].name));
        mmgr_line_put(&buffer_line, name);
        (void)mmgr_line_finish(&buffer_line);
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(s_register[s_registers].text, sizeof(s_register[s_registers].text));
        mmgr_line_put(&buffer_line, equals + 2);
        (void)mmgr_line_finish(&buffer_line);
    }
    s_register[s_registers].text[mmgr_text_span_not(s_register[s_registers].text, " \t", PLATFORM_TEXT_TO_TERMINATOR)] =
        '\0';
    s_registers += 1u;
}

unsigned int krs_fixed_read(const char *ksc_path, const char *krs_path)
{
    static KrsFixedGiven s_given[KRS_FIXED_GIVEN_MAX];
    int answered[KRS_FIXED_NAMES] = {0};
    unsigned long long number[KRS_FIXED_NAMES] = {0ULL};
    unsigned long long top = 0ULL;
    s_registers = 0u;
    fixed_answers_read(ksc_path, answered, number, &top);
    const unsigned int givens = fixed_given_read(krs_path, s_given);
    for (unsigned int name = 0u; name < KRS_FIXED_NAMES; name += 1u)
    {
        if (answered[name] != 0)
        {
            char line[KRS_FIXED_LINE];
            fixed_line_of(name, number[name], top, line, sizeof(line));
            fixed_register_kept(s_fixed_names[name], line);
            continue;
        }
        const char *const given_text = fixed_given_of(s_given, givens, s_fixed_names[name]);
        if (given_text != NULL)
        {
            fixed_register_kept(s_fixed_names[name], given_text);
        }
    }
    for (unsigned int given = 0u; given < givens; given += 1u)
    {
        if (!fixed_is_placeholder_name(s_given[given].name))
        {
            fixed_register_kept(s_given[given].name, s_given[given].line);
        }
    }
    return s_registers;
}

// Return 1 if `text` is a register's numeric name, `R` and digits.
static int fixed_numeric(const char *text)
{
    if ((text[0] != 'R') || (text[1] == '\0'))
    {
        return 0;
    }
    for (const char *walk = text + 1; *walk != '\0'; walk += 1)
    {
        if ((*walk < '0') || (*walk > '9'))
        {
            return 0;
        }
    }
    return 1;
}

// Return 1 if the character `ch` continues a word: a letter, a digit or an underscore.
static int fixed_word_character(char ch)
{
    return ((ch >= 'a') && (ch <= 'z')) || ((ch >= 'A') && (ch <= 'Z')) || ((ch >= '0') && (ch <= '9')) || (ch == '_');
}

int krs_fixed_placed(char *text, size_t room)
{
    char placed[KRS_FIXED_LINE * 2u];
    size_t at = 0u;
    for (const char *walk = text; *walk != '\0';)
    {
        const char before = (walk == text) ? ' ' : walk[-1];
        const KrsFixedRegister *found = NULL;
        for (unsigned int each = 0u; (found == NULL) && (each < s_registers); each += 1u)
        {
            const size_t length = mmgr_text_length(s_register[each].text, PLATFORM_TEXT_TO_TERMINATOR);
            const int whole = !fixed_word_character(before) && (before != '{') &&
                              (mmgr_text_order(walk, s_register[each].text, (size_t)(length)) == 0) &&
                              !fixed_word_character(walk[length]);
            found = (whole && fixed_numeric(s_register[each].text)) ? &s_register[each] : NULL;
        }
        if (found == NULL)
        {
            if ((at + 2u) >= sizeof(placed))
            {
                return 0;
            }
            placed[at] = *walk;
            at += 1u;
            walk += 1;
            continue;
        }
        VerbaLinea formed_line_1 = mmgr_line_over(placed + at, sizeof(placed) - at);
        mmgr_line_put(&formed_line_1, "{");
        mmgr_line_put(&formed_line_1, found->name);
        mmgr_line_put(&formed_line_1, "}");
        (void)mmgr_line_finish(&formed_line_1);
        const int added = (int)formed_line_1.at;
        if ((added < 0) || ((at + (size_t)added) >= sizeof(placed)))
        {
            return 0;
        }
        at += (size_t)added;
        walk += mmgr_text_length(found->text, PLATFORM_TEXT_TO_TERMINATOR);
    }
    placed[at] = '\0';
    if ((at + 1u) > room)
    {
        return 0;
    }
    mmgr_bytes_copy(text, placed, at + 1u);
    return 1;
}

unsigned int krs_fixed_written(char *out, size_t cap, size_t *at, const char *ksc_path, const char *krs_path,
                               char *missing, size_t room)
{
    static KrsFixedGiven s_given[KRS_FIXED_GIVEN_MAX];
    int answered[KRS_FIXED_NAMES] = {0};
    unsigned long long number[KRS_FIXED_NAMES] = {0ULL};
    unsigned long long top = 0ULL;
    fixed_answers_read(ksc_path, answered, number, &top);
    const unsigned int givens = fixed_given_read(krs_path, s_given);
    unsigned int written = 0u;
    size_t used = 0u;
    missing[0] = '\0';
    // The given lines a row replaces with another text go into the .krf beside the .ksc.
    const char *removed[KRS_FIXED_NAMES];
    unsigned int removals = 0u;
    for (unsigned int name = 0u; name < KRS_FIXED_NAMES; name += 1u)
    {
        if (answered[name] != 0)
        {
            char line[KRS_FIXED_LINE];
            fixed_line_of(name, number[name], top, line, sizeof(line));
            *at = verba_textus.put(&(VerbaTextusCfg){.out = out, .cap = cap, .at = *at, .text = line});
            *at = verba_littera.ch(&(VerbaLitteraCfg){.out = out, .cap = cap, .at = *at, .ch = '\n'});
            written += 1u;
            const char *const before = fixed_given_of(s_given, givens, s_fixed_names[name]);
            if ((before != NULL) && (!mmgr_text_equal(before, line, PLATFORM_TEXT_TO_TERMINATOR)))
            {
                removed[removals] = before;
                removals += 1u;
            }
            continue;
        }
        const char *const given_text = fixed_given_of(s_given, givens, s_fixed_names[name]);
        if (given_text != NULL)
        {
            *at = verba_textus.put(&(VerbaTextusCfg){.out = out, .cap = cap, .at = *at, .text = given_text});
            *at = verba_littera.ch(&(VerbaLitteraCfg){.out = out, .cap = cap, .at = *at, .ch = '\n'});
            written += 1u;
            continue;
        }
        VerbaLinea formed_line_2 = mmgr_line_over(missing + used, room - used);
        mmgr_line_put(&formed_line_2, (used == 0u) ? "" : ", ");
        mmgr_line_put(&formed_line_2, s_fixed_names[name]);
        (void)mmgr_line_finish(&formed_line_2);
        const int added = (int)formed_line_2.at;
        // snprintf counts what it would write. The count is no wider than the room, and what fits is what it wrote.
        used += (added > 0) ? (size_t)added : 0u;
        used = (used < room) ? used : (room - 1u);
    }
    for (unsigned int given = 0u; given < givens; given += 1u)
    {
        if (fixed_is_placeholder_name(s_given[given].name))
        {
            continue;
        }
        *at = verba_textus.put(&(VerbaTextusCfg){.out = out, .cap = cap, .at = *at, .text = s_given[given].line});
        *at = verba_littera.ch(&(VerbaLitteraCfg){.out = out, .cap = cap, .at = *at, .ch = '\n'});
        written += 1u;
    }
    if (removals != 0u)
    {
        char krf[1200];
        krf_path_beside(ksc_path, krf, sizeof(krf));
        if (!krf_removed(krf, "krs", "replaced", removed, removals))
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  orior_krs_write: ");
            mmgr_line_decimal(output_line, removals, 1u);
            mmgr_line_put(output_line, " fixed lines a row replaced could not be stored in ");
            mmgr_line_put(output_line, krf);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    return written;
}
