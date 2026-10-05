// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_sass_probe_class.c: the system's classification, written as <part>.ksc. Every other piece here learns what
// the system holds; this one records how that was learned - which channel each question went out on, and whether
// what came back was an answer, nothing, or a refusal. The pipes the system writes on are the part's and are written
// into <part>.kdm
#include "interface_sass_probe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the most questions kept whole. The decode channel puts one for every bit of every form, far past this, and is
// counted in place of being kept
#define SASS_CLASSED 512u

static const char *const s_channel_text[SASS_CHANNEL_COUNT] = {
#define SASS_CHANNEL_TEXT(name_, text_, why_) text_,
    SASS_CHANNELS(SASS_CHANNEL_TEXT)
#undef SASS_CHANNEL_TEXT
};

static const char *const s_channel_why[SASS_CHANNEL_COUNT] = {
#define SASS_CHANNEL_WHY(name_, text_, why_) why_,
    SASS_CHANNELS(SASS_CHANNEL_WHY)
#undef SASS_CHANNEL_WHY
};

static const char *const s_class_text[SASS_CLASS_COUNT] = {
#define SASS_CLASS_TEXT(name_, text_, why_) text_,
    SASS_CLASSES(SASS_CLASS_TEXT)
#undef SASS_CLASS_TEXT
};

static const char *const s_class_why[SASS_CLASS_COUNT] = {
#define SASS_CLASS_WHY(name_, text_, why_) why_,
    SASS_CLASSES(SASS_CLASS_WHY)
#undef SASS_CLASS_WHY
};

static SassClassed s_classed[SASS_CLASSED];
static unsigned int s_classed_count;
static unsigned int s_tally[SASS_CHANNEL_COUNT][SASS_CLASS_COUNT];

// the writings the system answers alike, two operations a side
#define SASS_EQUALS 256u
typedef struct
{
    char one[SASS_TEXT];
    char other[SASS_TEXT];
} SassEqual;
static SassEqual s_equal[SASS_EQUALS];
static unsigned int s_equal_count;

// the operations the system writes on the FMA pipe in place of the integer pipe, and the lane each moves in
#define SASS_PIPES 32u
typedef struct
{
    char operation[SASS_TEXT];
    char writing[SASS_TEXT];
    unsigned int least;
    unsigned int over;
} SassPipe;
static SassPipe s_pipe[SASS_PIPES];
static unsigned int s_pipe_count;

// The forms the file holds as a ruleset reads them, each line with its newline: what the system folds into one
// instruction, which the ruleset beside the file reads after its own lines. A rewrite writes them back as they stood,
// since nothing here finds them
#define SASS_FORMS_LONGEST 16384u
static char s_forms[SASS_FORMS_LONGEST];
static size_t s_forms_length;

static void sass_text_copy(char *into, const char *from)
{
    unsigned int at = 0u;
    for (; (from[at] != '\0') && (at < (SASS_TEXT - 1u)); at += 1u)
    {
        into[at] = from[at];
    }
    into[at] = '\0';
}

int sass_equal_held(const char *one, const char *other)
{
    for (unsigned int number = 0u; number < s_equal_count; number += 1u)
    {
        const int straight = (strcmp(s_equal[number].one, one) == 0) && (strcmp(s_equal[number].other, other) == 0);
        const int crossed = (strcmp(s_equal[number].one, other) == 0) && (strcmp(s_equal[number].other, one) == 0);
        if (straight || crossed)
        {
            return 1;
        }
    }
    return 0;
}

void sass_equal_take(const char *one, const char *other)
{
    if ((s_equal_count >= SASS_EQUALS) || sass_equal_held(one, other))
    {
        return;
    }
    sass_text_copy(s_equal[s_equal_count].one, one);
    sass_text_copy(s_equal[s_equal_count].other, other);
    s_equal_count += 1u;
}

int sass_pipe_held(const char *operation, char *writing, unsigned int *least, unsigned int *over)
{
    for (unsigned int number = 0u; number < s_pipe_count; number += 1u)
    {
        if (strcmp(s_pipe[number].operation, operation) == 0)
        {
            sass_text_copy(writing, s_pipe[number].writing);
            *least = s_pipe[number].least;
            *over = s_pipe[number].over;
            return 1;
        }
    }
    return 0;
}

void sass_pipe_take(const char *operation, const char *writing, unsigned int least, unsigned int over)
{
    unsigned int number = 0u;
    while ((number < s_pipe_count) && (strcmp(s_pipe[number].operation, operation) != 0))
    {
        number += 1u;
    }
    if (number >= SASS_PIPES)
    {
        return;
    }
    sass_text_copy(s_pipe[number].operation, operation);
    sass_text_copy(s_pipe[number].writing, writing);
    s_pipe[number].least = least;
    s_pipe[number].over = over;
    s_pipe_count += (number == s_pipe_count) ? 1u : 0u;
}

void sass_class_count(unsigned int channel, unsigned int answered)
{
    if ((channel < SASS_CHANNEL_COUNT) && (answered < SASS_CLASS_COUNT))
    {
        s_tally[channel][answered] += 1u;
    }
}

void sass_class_take(unsigned int channel, unsigned int answered, const char *question, unsigned int word)
{
    sass_class_count(channel, answered);
    if ((s_classed_count >= SASS_CLASSED) || (channel >= SASS_CHANNEL_COUNT) || (answered >= SASS_CLASS_COUNT))
    {
        return;
    }
    SassClassed *const one = &s_classed[s_classed_count];
    one->channel = (unsigned char)channel;
    one->answered = (unsigned char)answered;
    one->word = word;
    // a question of more than one instruction is kept on one line, its instructions joined by a semicolon
    unsigned int at = 0u;
    for (const char *walk = question; (*walk != '\0') && (at < (SASS_TEXT - 1u)); walk += 1)
    {
        one->question[at] = (*walk == '\n') ? ';' : *walk;
        at += 1u;
    }
    one->question[at] = '\0';
    s_classed_count += 1u;
}

void sass_class_drop(unsigned int channel, unsigned int answered, const char *opening)
{
    const size_t length = strlen(opening);
    unsigned int kept = 0u;
    for (unsigned int number = 0u; number < s_classed_count; number += 1u)
    {
        const SassClassed *const one = &s_classed[number];
        const int dropped = (one->channel == channel) && (one->answered == answered) &&
                            (strncmp(one->question, opening, length) == 0);
        if (dropped != 0)
        {
            s_tally[channel][answered] -= (s_tally[channel][answered] != 0u) ? 1u : 0u;
            continue;
        }
        s_classed[kept] = *one;
        kept += 1u;
    }
    s_classed_count = kept;
}

// the channel or class named `text`, or the count where none names it
static unsigned int sass_channel_of(const char *text)
{
    for (unsigned int channel = 0u; channel < SASS_CHANNEL_COUNT; channel += 1u)
    {
        if (strcmp(s_channel_text[channel], text) == 0)
        {
            return channel;
        }
    }
    return SASS_CHANNEL_COUNT;
}

static unsigned int sass_class_of(const char *text)
{
    for (unsigned int answered = 0u; answered < SASS_CLASS_COUNT; answered += 1u)
    {
        if (strcmp(s_class_text[answered], text) == 0)
        {
            return answered;
        }
    }
    return SASS_CLASS_COUNT;
}

int sass_class_read(const char *machines, const char *part)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.ksc", machines, part);
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        return 0;
    }
    // the file is the classification: what was held before it is read is its own, and the equal writings a pass has
    // already taken are kept beside those the file holds
    s_classed_count = 0u;
    s_forms_length = 0u;
    memset(s_tally, 0, sizeof(s_tally));
    char line[SASS_TEXT * 8u];
    while (fgets(line, sizeof(line), file) != NULL)
    {
        const size_t length = strlen(line);
        const int form = (strncmp(line, "form ", 5u) == 0) || (strncmp(line, "err ", 4u) == 0) ||
                         (strncmp(line, "nop ", 4u) == 0);
        if (form && ((s_forms_length + length) < sizeof(s_forms)))
        {
            memcpy(&s_forms[s_forms_length], line, length);
            s_forms_length += length;
        }
        if (form)
        {
            continue;
        }
        char channel_text[16];
        char class_text[16];
        char question[SASS_TEXT];
        char one[SASS_TEXT];
        char other[SASS_TEXT];
        unsigned int word = 0u;
        unsigned int tally = 0u;
        if (sscanf(line, "equal %191s %191s", one, other) == 2)
        {
            sass_equal_take(one, other);
            continue;
        }
        if (sscanf(line, "count %15s %15s %u", channel_text, class_text, &tally) == 3)
        {
            const unsigned int channel = sass_channel_of(channel_text);
            const unsigned int answered = sass_class_of(class_text);
            // a fold is counted as its line is read back, each line one
            if ((channel < SASS_CHANNEL_COUNT) && (answered < SASS_CLASS_COUNT) && (answered != SASS_CLASS_FOLDS))
            {
                s_tally[channel][answered] = tally;
            }
            continue;
        }
        if (sscanf(line, "%15s %15s %8x %191[^\n]", channel_text, class_text, &word, question) == 4)
        {
            const unsigned int channel = sass_channel_of(channel_text);
            const unsigned int answered = sass_class_of(class_text);
            if ((channel < SASS_CHANNEL_COUNT) && (answered == SASS_CLASS_FOLDS))
            {
                s_tally[channel][answered] += 1u;
            }
            if ((channel < SASS_CHANNEL_COUNT) && (answered < SASS_CLASS_COUNT) && (s_classed_count < SASS_CLASSED))
            {
                SassClassed *const one = &s_classed[s_classed_count];
                one->channel = (unsigned char)channel;
                one->answered = (unsigned char)answered;
                one->word = word;
                sass_text_copy(one->question, question);
                s_classed_count += 1u;
            }
        }
    }
    fclose(file);
    return 1;
}

int sass_class_write(const char *machines, const char *part)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.ksc", machines, part);
    FILE *const file = fopen(path, "wb");
    if (file == NULL)
    {
        printf("interface sass class: %s could not be written\n", path);
        return 0;
    }
    fprintf(file, "ksc %s\n", part);
    fprintf(file, "# How this system is asked, and what came back. A probe is not a thing of its own: it is a\n");
    fprintf(file, "# question put on one of the channels below and the answer read back. Written by the interface's\n");
    fprintf(file, "# SASS probe; every line here is something the system said, and nothing here was assumed.\n\n");
    fprintf(file, "# the channels this system answers on\n");
    for (unsigned int channel = 0u; channel < SASS_CHANNEL_COUNT; channel += 1u)
    {
        fprintf(file, "channel %-8s %s\n", s_channel_text[channel], s_channel_why[channel]);
    }
    fprintf(file, "\n# what an answer can be\n");
    for (unsigned int answered = 0u; answered < SASS_CLASS_COUNT; answered += 1u)
    {
        fprintf(file, "class %-8s %s\n", s_class_text[answered], s_class_why[answered]);
    }
    fprintf(file, "\n# every question put, counted by the channel it went out on and what came back\n");
    for (unsigned int channel = 0u; channel < SASS_CHANNEL_COUNT; channel += 1u)
    {
        for (unsigned int answered = 0u; answered < SASS_CLASS_COUNT; answered += 1u)
        {
            fprintf(file, "count %-8s %-8s %u\n", s_channel_text[channel], s_class_text[answered],
                    s_tally[channel][answered]);
        }
    }
    fprintf(file, "\n# every question the system was asked in its own code, one a line: the channel, what came\n");
    fprintf(file, "# back, the word it answered, and the question. These are the ones the system answered for\n");
    fprintf(file, "# itself, with no compiler and no disassembler standing between.\n");
    for (unsigned int number = 0u; number < s_classed_count; number += 1u)
    {
        const SassClassed *const one = &s_classed[number];
        fprintf(file, "%s %s %08x %s\n", s_channel_text[one->channel], s_class_text[one->answered], one->word,
                one->question);
    }
    fprintf(file, "\n# writings the system answers alike: an operation written where the ruleset holds the other, each\n");
    fprintf(file, "# giving the same machine code. A reading in one is the ruleset's form in the other.\n");
    for (unsigned int number = 0u; number < s_equal_count; number += 1u)
    {
        fprintf(file, "equal %s %s\n", s_equal[number].one, s_equal[number].other);
    }
    if (s_forms_length != 0u)
    {
        fprintf(file, "\n# forms the system folds into one instruction of its own, as the ruleset beside this file reads\n");
        fprintf(file, "# them after its own lines and its part's.\n");
        fwrite(s_forms, 1u, s_forms_length, file);
    }
    const int closed = (fclose(file) == 0);
    printf("interface sass class: %s written, %u questions kept whole\n", path, s_classed_count);
    return closed;
}

// the comment over the pipes in a .kdm, a line an entry, each written with its newline
static const char *const s_pipe_comment[] = {
    "# operations the system writes on the FMA pipe in place of the integer pipe: the operation, its\n",
    "# writing there, and the lane it moves in, one whose integer pipe holds the first number of\n",
    "# operations or more and the second more than its FMA pipe.\n"};

int sass_pipe_read(const char *machines, const char *part)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.kdm", machines, part);
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        return 0;
    }
    char line[SASS_TEXT * 2u];
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char one[SASS_TEXT];
        char other[SASS_TEXT];
        unsigned int least = 0u;
        unsigned int over = 0u;
        if (sscanf(line, "pipe %191s %191s %u %u", one, other, &least, &over) == 4)
        {
            sass_pipe_take(one, other, least, over);
        }
    }
    fclose(file);
    return 1;
}

// 1 where `line` is a pipe of a .kdm or a line of the comment over them
static int sass_pipe_line(const char *line)
{
    int found = (strncmp(line, "pipe ", 5u) == 0);
    for (unsigned int at = 0u; at < (sizeof(s_pipe_comment) / sizeof(s_pipe_comment[0])); at += 1u)
    {
        found = found || (strcmp(line, s_pipe_comment[at]) == 0);
    }
    return found;
}

int sass_pipe_write(const char *machines, const char *part)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s.kdm", machines, part);
    FILE *const in = fopen(path, "rb");
    if (in == NULL)
    {
        printf("interface sass class: %s could not be read\n", path);
        return 0;
    }
    fseek(in, 0L, SEEK_END);
    const long size = ftell(in);
    fseek(in, 0L, SEEK_SET);
    // ftell answers -1 only where the file cannot be measured, and a .kdm is a few hundred kilobytes
    char *const kept = (size >= 0L) ? (char *)malloc((size_t)size + 1u) : NULL;
    if (kept == NULL)
    {
        fclose(in);
        printf("interface sass class: %s could not be held\n", path);
        return 0;
    }
    size_t length = 0u;
    char line[SASS_TEXT * 4u];
    while (fgets(line, sizeof(line), in) != NULL)
    {
        const size_t taken = strlen(line);
        if ((sass_pipe_line(line) == 0) && ((length + taken) <= (size_t)size))
        {
            memcpy(&kept[length], line, taken);
            length += taken;
        }
    }
    fclose(in);
    // the blank lines the pipes stood after, taken down to the one newline that ends the map's last line
    while ((length >= 2u) && (kept[length - 1u] == '\n') && (kept[length - 2u] == '\n'))
    {
        length -= 1u;
    }
    FILE *const out = fopen(path, "wb");
    if (out == NULL)
    {
        free(kept);
        printf("interface sass class: %s could not be written\n", path);
        return 0;
    }
    fwrite(kept, 1u, length, out);
    free(kept);
    if (s_pipe_count != 0u)
    {
        fprintf(out, "\n");
    }
    for (unsigned int at = 0u; (s_pipe_count != 0u) && (at < (sizeof(s_pipe_comment) / sizeof(s_pipe_comment[0])));
         at += 1u)
    {
        fprintf(out, "%s", s_pipe_comment[at]);
    }
    for (unsigned int number = 0u; number < s_pipe_count; number += 1u)
    {
        fprintf(out, "pipe %s %s %u %u\n", s_pipe[number].operation, s_pipe[number].writing, s_pipe[number].least,
                s_pipe[number].over);
    }
    const int closed = (fclose(out) == 0);
    printf("interface sass class: %s written, %u pipes\n", path, s_pipe_count);
    return closed;
}
