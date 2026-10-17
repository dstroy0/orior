// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// frame.c: the frame forms of a part's ruleset: the table of names read, a form's line as the schema's form, and the
// lines written, the ruleset's own line kept where the run hands none
#include "frame.h"

#include "../../../../types/file_defs/krf/krf.h"
#include "krs_write_internal.h"

// the most rows the table holds, and the most places a row gives a form's parameters
#define KRS_FRAME_ROWS 32u

#define KRS_FRAME_PLACES 8u

// the most form lines a ruleset gives, and the longest
#define KRS_FRAME_GIVEN_MOST 256u

#define KRS_FRAME_LINE (KRS_FRAME_TEXT * 2u)

// one row of the table: the relation, the schema's form it is written as, and the place of the relation's tuple each
// of the form's parameters takes, in the parameters' order
typedef struct
{
    char relation[KRS_FRAME_RELATION];
    char form[KRS_NAME];
    unsigned int place[KRS_FRAME_PLACES];
    unsigned int places;
} KrsFrameRow;

static KrsFrameRow s_row[KRS_FRAME_ROWS];
static unsigned int s_rows;

// a form line the ruleset gave before this run: the form's name and the line whole
typedef struct
{
    char name[KRS_NAME];
    char line[KRS_FRAME_LINE];
} KrsFrameGiven;

// the table's path where none is handed: frame_forms.tsv in the folder of this source in the tree it was built from
static void frame_table_beside(char *path, size_t room)
{
    const char *const file = __FILE__;
    const char *const forward = strrchr(file, '/');
    const char *const back = strrchr(file, '\\');
    const char *const slash = (back == NULL) ? forward : ((forward == NULL) || (back > forward)) ? back : forward;
    // the folder is __FILE__ up to and with its last slash, and a file named with no folder is in this one
    const int folder = (slash != NULL) ? (int)((slash - file) + 1) : 0;
    snprintf(path, room, "%.*sframe_forms.tsv", folder, file);
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
    unsigned char *file_bytes = NULL;
    unsigned long long file_size = 0ull;
    unsigned long long file_at = 0ull;
    if (!qry_file_read(qry_run(), path, &file_bytes, &file_size))
    {
        return 0u;
    }
    char line[1024];
    while (qry_line_next(file_bytes, file_size, &file_at, line, sizeof(line)) && (s_rows < KRS_FRAME_ROWS))
    {
        line[strcspn(line, "\r\n")] = '\0';
        if ((line[0] == '#') || (line[0] == '\0'))
        {
            continue;
        }
        char *const form = strchr(line, '\t');
        if (form == NULL)
        {
            continue;
        }
        *form = '\0';
        // the places column may be empty, a form of no parameters, and a line may end before it
        char *const places = strchr(form + 1, '\t');
        if (places != NULL)
        {
            *places = '\0';
        }
        // the head line names the columns
        if (strcmp(line, "relation") == 0)
        {
            continue;
        }
        KrsFrameRow *const row = &s_row[s_rows];
        snprintf(row->relation, sizeof(row->relation), "%.*s", (int)(sizeof(row->relation) - 1u), line);
        snprintf(row->form, sizeof(row->form), "%.*s", (int)(sizeof(row->form) - 1u), form + 1);
        row->places = 0u;
        const char *rest = (places != NULL) ? (places + 1) : "";
        int step = 0;
        while ((row->places < KRS_FRAME_PLACES) && (sscanf(rest, "%u %n", &row->place[row->places], &step) == 1))
        {
            row->places += 1u;
            rest += step;
        }
        s_rows += 1u;
    }
    free(file_bytes);
    return s_rows;
}

// how many places `text` holds, each {<place>} counted once
static unsigned int frame_places_held(const char *text)
{
    unsigned int seen = 0u;
    for (const char *walk = text; *walk != '\0'; walk += 1)
    {
        unsigned int place = 0u;
        int length = 0;
        if ((*walk == '{') && (sscanf(walk, "{%u}%n", &place, &length) == 1) && (length != 0) && (place < 32u))
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

// 1 where `row` names `form`: the row is of the form's relation and its places are as many as the form's text holds
static int frame_row_fits(const KrsFrameRow *row, const KrsFrameForm *form)
{
    return (strcmp(row->relation, form->relation) == 0) && (row->places == frame_places_held(form->text));
}

// the first row that names `form`, or NULL where none does
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

// `form` with its guard struck, into `bare`, where its text opens with a placed guard, `@{<place>}`, and that is its
// one place: the vendor writes a guard at every predicate as none, and the form read with its guard is then the
// relation's form with no place as well, exit_if's instruction return's. 1 where `bare` is written
static int frame_unguarded(const KrsFrameForm *form, KrsFrameForm *bare)
{
    unsigned int place = 0u;
    int length = 0;
    if ((sscanf(form->text, "@{%u}%n", &place, &length) != 1) || (length == 0) || (form->text[length] != ' ') ||
        (frame_places_held(form->text) != 1u))
    {
        return 0;
    }
    *bare = *form;
    snprintf(bare->text, sizeof(bare->text), "%s", form->text + length + 1);
    return 1;
}

// `form` written as the schema's form `row` names, into `line`: 1, or 0 where the row fits no parameters the ruleset
// names or the line does not fit
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
    // the text with each place written as the parameter that takes it
    char text[KRS_FRAME_LINE];
    size_t at = 0u;
    for (const char *walk = form->text; *walk != '\0';)
    {
        unsigned int place = 0u;
        int length = 0;
        if ((*walk == '{') && (sscanf(walk, "{%u}%n", &place, &length) == 1) && (length != 0))
        {
            unsigned int named = row->places;
            for (unsigned int each = 0u; each < row->places; each += 1u)
            {
                named = (row->place[each] == place) ? each : named;
            }
            if ((named == row->places) || ((at + strlen(parameter[named]) + 3u) >= sizeof(text)))
            {
                return 0;
            }
            at += (size_t)snprintf(text + at, sizeof(text) - at, "{%s}", parameter[named]);
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
    // the line: the form's name, its parameters, and the text as the ruleset reads a form's, ending in \n
    size_t used = 0u;
    int added = snprintf(line, room, "form %s", row->form);
    used += (added > 0) ? (size_t)added : room;
    for (unsigned int each = 0u; (each < parameters) && (used < room); each += 1u)
    {
        added = snprintf(line + used, room - used, " %s", parameter[each]);
        used += (added > 0) ? (size_t)added : room;
    }
    if (used < room)
    {
        added = snprintf(line + used, room - used, " = %s\\n\n", text);
        used += (added > 0) ? (size_t)added : room;
    }
    return (used < room) ? 1 : 0;
}

int krs_frame_named(const KrsFrameForm *form, const char *named_ruleset, char *line, size_t room)
{
    return frame_row_named(frame_row_of(form), form, named_ruleset, line, room);
}

// every `form <name> ...` line the ruleset at `krs_path` gave, into `given`, at most KRS_FRAME_GIVEN_MOST; the count,
// 0 where there is no path or no file
static unsigned int frame_given_read(const char *krs_path, KrsFrameGiven *given)
{
    unsigned char *file_bytes = NULL;
    unsigned long long file_size = 0ull;
    unsigned long long file_at = 0ull;
    unsigned int count = 0u;
    if ((krs_path == NULL) || !qry_file_read(qry_run(), krs_path, &file_bytes, &file_size))
    {
        return 0u;
    }
    char line[KRS_FRAME_LINE];
    while (qry_line_next(file_bytes, file_size, &file_at, line, sizeof(line)) && (count < KRS_FRAME_GIVEN_MOST))
    {
        char kind[KRS_NAME];
        char name[KRS_NAME];
        if ((sscanf(line, "%63s %63s", kind, name) != 2) || (strcmp(kind, "form") != 0))
        {
            continue;
        }
        line[strcspn(line, "\r\n")] = '\0';
        snprintf(given[count].name, sizeof(given[count].name), "%s", name);
        snprintf(given[count].line, sizeof(given[count].line), "%s", line);
        count += 1u;
    }
    free(file_bytes);
    return count;
}

// the line the ruleset gave the form `name` before this run, of the `count` in `given`, or NULL where it gave none
static const char *frame_given_of(const KrsFrameGiven *given, unsigned int count, const char *name)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (strcmp(given[at].name, name) == 0)
        {
            return given[at].line;
        }
    }
    return NULL;
}

// 1 where `before`, a line with no newline, is `line` up to its newline
static int frame_same_line(const char *before, const char *line)
{
    const size_t length = strcspn(line, "\n");
    return (strlen(before) == length) && (strncmp(before, line, length) == 0);
}

// `row` written into `line` from the first of the `count` forms in `form` it fits: 1, or 0 where none is named by it
static int frame_row_written(const KrsFrameRow *row, const KrsFrameForm *form, unsigned int count,
                             const char *named_ruleset, char *line, size_t room)
{
    int named = 0;
    for (unsigned int each = 0u; (each < count) && !named; each += 1u)
    {
        named = frame_row_fits(row, &form[each]) && frame_row_named(row, &form[each], named_ruleset, line, room);
        // a row of no places is also written from a form of its relation whose one place is a guard
        KrsFrameForm bare;
        named = named || ((row->places == 0u) && frame_unguarded(&form[each], &bare) && frame_row_fits(row, &bare) &&
                          frame_row_named(row, &bare, named_ruleset, line, room));
    }
    return named;
}

// the first row of the table that writes the schema's form the row at `at` writes
static unsigned int frame_form_first(unsigned int at)
{
    unsigned int first = 0u;
    while (strcmp(s_row[first].form, s_row[at].form) != 0)
    {
        first += 1u;
    }
    return first;
}

unsigned int krs_frame_written(QryText *forms, const KrsFrameForm *form, unsigned int count, const char *named_ruleset,
                               const char *krs_path, const char *ksc_path)
{
    static KrsFrameGiven s_given[KRS_FRAME_GIVEN_MOST];
    const unsigned int givens = frame_given_read(krs_path, s_given);
    // the given lines a written one replaces with another text, kept in the .krf beside the .ksc
    const char *removed[KRS_FRAME_ROWS];
    unsigned int removals = 0u;
    unsigned int written = 0u;
    for (unsigned int at = 0u; at < s_rows; at += 1u)
    {
        // a schema form more than one row writes, its relation's own row and another relation's mirror row, is one
        // line, at its first row, from the first of its rows in the table's order a form is named by
        if (frame_form_first(at) != at)
        {
            continue;
        }
        const KrsFrameRow *const row = &s_row[at];
        char line[KRS_FRAME_LINE];
        int named = 0;
        for (unsigned int other = at; (other < s_rows) && !named; other += 1u)
        {
            named = (strcmp(s_row[other].form, row->form) == 0) &&
                    frame_row_written(&s_row[other], form, count, named_ruleset, line, sizeof(line));
        }
        const char *const before = frame_given_of(s_given, givens, row->form);
        if (named)
        {
            qry_text_printed(forms, "%s", line);
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
            qry_text_printed(forms, "%s\n", before);
            written += 1u;
            printf("  krs_write: no form answered for in this run writes %s; the ruleset keeps its form\n", row->form);
        }
    }
    if (removals != 0u)
    {
        char krf[1200];
        krf_path_beside(ksc_path, krf, sizeof(krf));
        if (!krf_removed(krf, "krs", "replaced", removed, removals))
        {
            printf("  krs_write: %u frame lines a written one replaced could not be kept in %s\n", removals, krf);
        }
    }
    return written;
}
