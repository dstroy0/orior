// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame_forms.c: the frame forms of a part's ruleset. The module reads the table of names, writes a form's line as
// the schema's form, and writes the lines. The ruleset's own line stays where the run passes none.
#include "frame_forms.h"
#include "memoria_operor/memoria_linea.h"
#include "../../../../src/platform/platform.h"
#include "cellularum_laboro/cellularum_linea.h"

#include "../../../parsers/line_read.h"
#include "../../krf/krf.h"
#include "fixed.h"
#include "krs_write_internal.h"
#include "verba_scribo/verba_scribo.h"

// The max rows the table contains, and the max places a row gives a form's parameters.
#define KRS_FRAME_ROWS 32u

#define KRS_FRAME_PLACES 8u

// The max form lines a ruleset gives, and the longest line.
#define KRS_FRAME_GIVEN_MAX 256u

#define KRS_FRAME_LINE (KRS_FRAME_TEXT * 2u)

// One row of the table: the relation and the schema's form the writer writes it as. The row also holds the place of
// the relation's tuple each of the form's parameters takes, in the parameters' order.
typedef struct
{
    char relation[KRS_FRAME_RELATION];
    char form[KRS_NAME];
    unsigned int place[KRS_FRAME_PLACES];
    unsigned int places;
} KrsFrameRow;

static KrsFrameRow s_row[KRS_FRAME_ROWS];
static unsigned int s_rows;

// A form line the ruleset gave before this run: the form's name and the whole line.
typedef struct
{
    char name[KRS_NAME];
    char line[KRS_FRAME_LINE];
} KrsFrameGiven;

// The table's path if the caller passes none: frame_forms.tsv in this source's folder in the tree of the build.
static void frame_table_beside(char *path, size_t room)
{
    const char *const file = __FILE__;
    const char *const forward = mmgr_text_char_last(file, '/', PLATFORM_TEXT_TO_TERMINATOR);
    const char *const back = mmgr_text_char_last(file, '\\', PLATFORM_TEXT_TO_TERMINATOR);
    const char *const slash = (back == NULL) ? forward : ((forward == NULL) || (back > forward)) ? back : forward;
    // The folder is __FILE__ up to and with its last slash. A file name with no folder is in this folder.
    const int folder = (slash != NULL) ? (int)((slash - file) + 1) : 0;
    {
        VerbaLinea buffer_line = mmgr_line_over(path, room);
        mmgr_line_put_max(&buffer_line, file, (size_t)(folder));
        mmgr_line_put(&buffer_line, "frame_forms.tsv");
        (void)mmgr_line_finish(&buffer_line);
    }
}

unsigned int krs_frame_read(const char *path)
{
    char beside[1024];
    if (path == NULL)
    {
        frame_table_beside(beside, sizeof(beside));
        path = beside;
    }
    s_rows = 0u;
    const unsigned char *file_bytes = NULL;
    unsigned long long file_size = 0ULL;
    unsigned long long file_at = 0ULL;
    if (!qry_member_file_blob(path, &file_bytes, &file_size))
    {
        return 0u;
    }
    char line[1024];
    while (line_read(file_bytes, file_size, &file_at, line, sizeof(line)) && (s_rows < KRS_FRAME_ROWS))
    {
        line[mmgr_text_span_not(line, "\r\n", PLATFORM_TEXT_TO_TERMINATOR)] = '\0';
        if ((line[0] == '#') || (line[0] == '\0'))
        {
            continue;
        }
        char *const form = mmgr_text_char(line, '\t', PLATFORM_TEXT_TO_TERMINATOR);
        if (form == NULL)
        {
            continue;
        }
        *form = '\0';
        // The places column can be empty, a form of no parameters, and a line can end before it.
        char *const places = mmgr_text_char(form + 1, '\t', PLATFORM_TEXT_TO_TERMINATOR);
        if (places != NULL)
        {
            *places = '\0';
        }
        // The head line names the columns.
        if (mmgr_text_equal(line, "relation", PLATFORM_TEXT_TO_TERMINATOR))
        {
            continue;
        }
        KrsFrameRow *const row = &s_row[s_rows];
        {
            VerbaLinea buffer_line = mmgr_line_over(row->relation, sizeof(row->relation));
            mmgr_line_put_max(&buffer_line, line, (size_t)((int)(sizeof(row->relation) - 1u)));
            (void)mmgr_line_finish(&buffer_line);
        }
        {
            VerbaLinea buffer_line = mmgr_line_over(row->form, sizeof(row->form));
            mmgr_line_put_max(&buffer_line, form + 1, (size_t)((int)(sizeof(row->form) - 1u)));
            (void)mmgr_line_finish(&buffer_line);
        }
        row->places = 0u;
        const char *rest = (places != NULL) ? (places + 1) : "";
        unsigned long long place = 0ULL;
        while ((row->places < KRS_FRAME_PLACES) && line_number(&rest, 0, &place) && (place <= 0xffffffffULL))
        {
            // A place is a small number that an unsigned int represents.
            row->place[row->places] = (unsigned int)place;
            row->places += 1u;
        }
        s_rows += 1u;
    }
    return s_rows;
}

// Return how many places `text` contains, each {<place>} counted once.
static unsigned int frame_places_count(const char *text)
{
    unsigned int seen = 0u;
    for (const char *walk = text; *walk != '\0'; walk += 1)
    {
        unsigned int place = 0u;
        int length = 0;
        if (krs_place_read(walk, &place, &length) && (place < 32u))
        {
            seen |= 1u << place;
        }
    }
    unsigned int count = 0u;
    for (unsigned int bit = 0u; bit < 32u; bit += 1u)
    {
        count += (seen >> bit) & 1u;
    }
    return count;
}

// Return 1 if `row` names `form`: the row is of the form's relation, and its places are as many as the form's text
// contains.
static int frame_row_fits(const KrsFrameRow *row, const KrsFrameForm *form)
{
    return (mmgr_text_equal(row->relation, form->relation, PLATFORM_TEXT_TO_TERMINATOR)) &&
           (row->places == frame_places_count(form->text));
}

// Return the first row that names `form`, or NULL if none does.
static const KrsFrameRow *frame_row_of(const KrsFrameForm *form)
{
    for (unsigned int at = 0u; at < s_rows; at += 1u)
    {
        if (frame_row_fits(&s_row[at], form))
        {
            return &s_row[at];
        }
    }
    return NULL;
}

// Write `form` without its guard into `bare` if its text opens with a placed guard, `@{<place>}`, and that is its
// one place. The vendor writes a guard at every predicate as none. The form read with its guard is then also the
// relation's form with no place, as exit_if's instruction is return's. Return 1 if `bare` holds the form.
static int frame_unguarded(const KrsFrameForm *form, KrsFrameForm *bare)
{
    unsigned int place = 0u;
    int length = 0;
    if ((form->text[0] != '@') || !krs_place_read(form->text + 1, &place, &length) || (form->text[length + 1] != ' ') ||
        (frame_places_count(form->text) != 1u))
    {
        return 0;
    }
    *bare = *form;
    {
        VerbaLinea buffer_line = mmgr_line_over(bare->text, sizeof(bare->text));
        mmgr_line_put(&buffer_line, form->text + length + 2);
        (void)mmgr_line_finish(&buffer_line);
    }
    return 1;
}

// Write `form` into `line` as the schema's form `row` names. Return 1, or 0 if the row fits no parameters the
// ruleset names or the line does not fit.
static int frame_row_named(const KrsFrameRow *row, const KrsFrameForm *form, const char *named_ruleset, char *line,
                           size_t room)
{
    if (row == NULL)
    {
        return 0;
    }
    char parameter[KRS_PARAMETERS][KRS_NAME];
    const unsigned int parameters = krs_parameters_named(named_ruleset, row->form, parameter);
    if (parameters != row->places)
    {
        return 0;
    }
    // Write the text with each place as the parameter that takes it.
    char text[KRS_FRAME_LINE];
    size_t at = 0u;
    for (const char *walk = form->text; *walk != '\0';)
    {
        unsigned int place = 0u;
        int length = 0;
        if (krs_place_read(walk, &place, &length))
        {
            unsigned int named = row->places;
            for (unsigned int each = 0u; each < row->places; each += 1u)
            {
                named = (row->place[each] == place) ? each : named;
            }
            if ((named == row->places) ||
                ((at + mmgr_text_length(parameter[named], PLATFORM_TEXT_TO_TERMINATOR) + 3u) >= sizeof(text)))
            {
                return 0;
            }
            VerbaLinea formed_line_1 = mmgr_line_over(text + at, sizeof(text) - at);
            mmgr_line_put(&formed_line_1, "{");
            mmgr_line_put(&formed_line_1, parameter[named]);
            mmgr_line_put(&formed_line_1, "}");
            (void)mmgr_line_finish(&formed_line_1);
            at += (size_t)(int)formed_line_1.at;
            walk += length;
            continue;
        }
        if ((at + 2u) >= sizeof(text))
        {
            return 0;
        }
        text[at] = *walk;
        at += 1u;
        text[at] = '\0';
        walk += 1;
    }
    if (!krs_fixed_placed(text, sizeof(text)))
    {
        return 0;
    }
    // The line is the form's name, its parameters, and the text as the ruleset reads a form's, with \n at its end.
    size_t used = 0u;
    VerbaLinea formed_line_2 = mmgr_line_over(line, room);
    mmgr_line_put(&formed_line_2, "form ");
    mmgr_line_put(&formed_line_2, row->form);
    (void)mmgr_line_finish(&formed_line_2);
    int added = (int)formed_line_2.at;
    used += (added > 0) ? (size_t)added : room;
    for (unsigned int each = 0u; (each < parameters) && (used < room); each += 1u)
    {
        VerbaLinea formed_line_3 = mmgr_line_over(line + used, room - used);
        mmgr_line_put(&formed_line_3, " ");
        mmgr_line_put(&formed_line_3, parameter[each]);
        (void)mmgr_line_finish(&formed_line_3);
        added = (int)formed_line_3.at;
        used += (added > 0) ? (size_t)added : room;
    }
    if (used < room)
    {
        VerbaLinea formed_line_4 = mmgr_line_over(line + used, room - used);
        mmgr_line_put(&formed_line_4, " = ");
        mmgr_line_put(&formed_line_4, text);
        mmgr_line_put(&formed_line_4, "\\n\n");
        (void)mmgr_line_finish(&formed_line_4);
        added = (int)formed_line_4.at;
        used += (added > 0) ? (size_t)added : room;
    }
    return (used < room) ? 1 : 0;
}

int krs_frame_named(const KrsFrameForm *form, const char *named_ruleset, char *line, size_t room)
{
    return frame_row_named(frame_row_of(form), form, named_ruleset, line, room);
}

// Read every `form <name> ...` line the ruleset at `krs_path` gave into `given`, at max KRS_FRAME_GIVEN_MAX. Return
// the count, 0 if there is no path or no file.
static unsigned int frame_given_read(const char *krs_path, KrsFrameGiven *given)
{
    const unsigned char *file_bytes = NULL;
    unsigned long long file_size = 0ULL;
    unsigned long long file_at = 0ULL;
    unsigned int count = 0u;
    if ((krs_path == NULL) || !qry_member_file_blob(krs_path, &file_bytes, &file_size))
    {
        return 0u;
    }
    char line[KRS_FRAME_LINE];
    while (line_read(file_bytes, file_size, &file_at, line, sizeof(line)) && (count < KRS_FRAME_GIVEN_MAX))
    {
        char type[KRS_NAME];
        char name[KRS_NAME];
        const char *at = line;
        if (!line_word(&at, type, sizeof(type)) || !line_word(&at, name, sizeof(name)) ||
            (!mmgr_text_equal(type, "form", PLATFORM_TEXT_TO_TERMINATOR)))
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

// Return the line the ruleset gave the form `name` before this run, of the `count` lines in `given`. Return NULL
// if it gave none.
static const char *frame_given_of(const KrsFrameGiven *given, unsigned int count, const char *name)
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

// Return 1 if `before`, a line with no newline, is `line` up to its newline.
static int frame_same_line(const char *before, const char *line)
{
    const size_t length = mmgr_text_span_not(line, "\n", PLATFORM_TEXT_TO_TERMINATOR);
    return (mmgr_text_length(before, PLATFORM_TEXT_TO_TERMINATOR) == length) &&
           (mmgr_text_order(before, line, (size_t)(length)) == 0);
}

// Write `row` into `line` from the first of the `count` forms in `form` it fits. Return 1, or 0 if it names none.
static int frame_row_written(const KrsFrameRow *row, const KrsFrameForm *form, unsigned int count,
                             const char *named_ruleset, char *line, size_t room)
{
    int named = 0;
    for (unsigned int each = 0u; (each < count) && !named; each += 1u)
    {
        named = frame_row_fits(row, &form[each]) && frame_row_named(row, &form[each], named_ruleset, line, room);
        // A form of the row's relation whose one place is a guard also writes a row of no places.
        KrsFrameForm bare;
        named = named || ((row->places == 0u) && frame_unguarded(&form[each], &bare) && frame_row_fits(row, &bare) &&
                          frame_row_named(row, &bare, named_ruleset, line, room));
    }
    return named;
}

// Return the first row of the table that writes the schema's form the row at `at` writes.
static unsigned int frame_form_first(unsigned int at)
{
    unsigned int first = 0u;
    while (!mmgr_text_equal(s_row[first].form, s_row[at].form, PLATFORM_TEXT_TO_TERMINATOR))
    {
        first += 1u;
    }
    return first;
}

unsigned int krs_frame_written(char *out, size_t cap, size_t *at, const KrsFrameForm *form, unsigned int count,
                               const char *named_ruleset, const char *krs_path, const char *ksc_path)
{
    static KrsFrameGiven s_given[KRS_FRAME_GIVEN_MAX];
    const unsigned int givens = frame_given_read(krs_path, s_given);
    // The given lines a written line replaces with another text go into the .krf beside the .ksc.
    const char *removed[KRS_FRAME_ROWS];
    unsigned int removals = 0u;
    unsigned int written = 0u;
    for (unsigned int place = 0u; place < s_rows; place += 1u)
    {
        // A schema form that more than one row writes is one line at its first row. The rows are its relation's
        // own row and another relation's mirror row. The line comes from the first of its rows, in the table's
        // order, that a form names.
        if (frame_form_first(place) != place)
        {
            continue;
        }
        const KrsFrameRow *const row = &s_row[place];
        char line[KRS_FRAME_LINE];
        int named = 0;
        for (unsigned int other = place; (other < s_rows) && !named; other += 1u)
        {
            named = (mmgr_text_equal(s_row[other].form, row->form, PLATFORM_TEXT_TO_TERMINATOR)) &&
                    frame_row_written(&s_row[other], form, count, named_ruleset, line, sizeof(line));
        }
        const char *const before = frame_given_of(s_given, givens, row->form);
        if (named)
        {
            *at = verba_textus.put(&(VerbaTextusCfg){.out = out, .cap = cap, .at = *at, .text = line});
            written += 1u;
            if ((before != NULL) && !frame_same_line(before, line))
            {
                removed[removals] = before;
                removals += 1u;
            }
            continue;
        }
        if (before != NULL)
        {
            // Keep the line as the ruleset gave it. Each fixed register it names by its number gets its placeholder.
            char kept[KRS_FRAME_LINE];
            {
                VerbaLinea buffer_line = mmgr_line_over(kept, sizeof(kept));
                mmgr_line_put(&buffer_line, before);
                (void)mmgr_line_finish(&buffer_line);
            }
            (void)krs_fixed_placed(kept, sizeof(kept));
            *at = verba_textus.put(&(VerbaTextusCfg){.out = out, .cap = cap, .at = *at, .text = kept});
            *at = verba_littera.ch(&(VerbaLitteraCfg){.out = out, .cap = cap, .at = *at, .ch = '\n'});
            written += 1u;
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  orior_krs_write: no form answered for in this run writes ");
                mmgr_line_put(output_line, row->form);
                mmgr_line_put(output_line, "; the ruleset retains its form");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
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
            mmgr_line_put(output_line, " frame lines a written one replaced could not be stored in ");
            mmgr_line_put(output_line, krf);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    return written;
}
