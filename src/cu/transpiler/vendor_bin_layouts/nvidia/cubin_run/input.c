// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// input.c: the input: a blob or a file read, the ask key, the verdict and launch rows put to the run, the input's lines, the cases of a launch, and what the carrier says of an answer, a dry run or a call
#include "cubin_run_internal.h"
#include "../../../lstar/protocol/table_text.h"
#include "../../../../includes/parsers/who.h"
#include "../../../../includes/parsers/line_read.h"

SassMachine s_machine;
unsigned char s_pattern[CUBIN_RUN_BYTES];
unsigned char s_code[CUBIN_RUN_BYTES];
unsigned char s_container[CUBIN_RUN_BYTES];
unsigned int s_exits[CUBIN_RUN_EXITS];
unsigned int s_cases[CUBIN_RUN_THREADS_MOST][CUBIN_RUN_IN_WORDS];
unsigned int s_case_count;
unsigned int s_answers[CUBIN_RUN_THREADS_MOST][CUBIN_RUN_OUT_WORDS];

// The run's buffer (qry_member.h): every channel input the carrier is named is a blob of it, the latest of that name,
// and every answer, dry line and container it gives is handed to it: no file of the channel is written but by the
// run's writer. The carrier carries nothing where it is NULL (main)
QryShared *s_run = NULL;

// the channel input `name` read whole into `bytes`, which holds `room`: the latest blob of it in the run. The
// bytes read, or 0 where it was not read or does not fit
unsigned long long cubin_run_input(const char *name, unsigned char *bytes, unsigned long long room)
{
    const unsigned char *held = NULL;
    unsigned long long size = 0ull;
    if (!qry_member_blob(name, &held, &size, NULL) || (size >= room))
    {
        return 0ull;
    }
    // the blob fits `bytes`, whose count a size_t holds
    memcpy(bytes, held, (size_t)size);
    return size;
}

// the key of the ask whose code is the channel input `code_path`: the sequence of its latest blob in the run,
// or 0 where the run holds none
unsigned long long cubin_run_ask_key(const char *code_path)
{
    const unsigned char *held = NULL;
    unsigned long long size = 0ull;
    unsigned long long sequence = 0ull;
    return qry_member_blob(code_path, &held, &size, &sequence) ? sequence : 0ull;
}

// the gate's verdict on the ask `ask`, `verdict` and the byte `at` it held at, a row of the run's table of verdicts:
// put or held, and carried where the part is there to carry it
void cubin_run_verdict_row(unsigned long long ask, unsigned int verdict, unsigned long long at, int dry)
{
    const char *const reason = cubin_safe_name(verdict);
    const int held = (verdict != CUBIN_SAFE);
    unsigned long long row[2u + 6] = {0ull, 0ull, ask,
                                       verdict,
                                       held ? 1ull : 0ull,
                                       held ? at : 0ull,
                                       table_text(reason, strlen(reason)),
                                       dry ? 0ull : 1ull};
    who_words(WHO_HERE, row);
    qry_member_append(CUBIN_RUN_VERDICTS, row, sizeof(row), QRY_ORDINARY);
}

// a launch of the ask `ask`, its outcome `outcome` and what the carrier said of it, `said`, empty where it answered, a
// row of the run's table of launches with its timed launches' count and times
void cubin_run_launch_row(unsigned long long ask, unsigned int outcome, unsigned int launches,
                                 unsigned long long nanoseconds, unsigned long long least, unsigned long long most,
                                 const char *said)
{
    unsigned long long row[2u + 7] = {0ull, 0ull, 
        ask, outcome, launches, nanoseconds, least, most, table_text(said, strlen(said))};
    who_words(WHO_HERE, row);
    qry_member_append(CUBIN_RUN_LAUNCHES, row, sizeof(row), QRY_ORDINARY);
}

// the lines of the input `name` opened: 1, or 0 where it is not there
int cubin_run_lines_open(CubinRunLines *lines, const char *name)
{
    memset(lines, 0, sizeof(*lines));
    return qry_member_blob(name, &lines->bytes, &lines->size, NULL);
}

// the next line into `line`, which holds `room`, as much of it as fits and its end kept: 1, or 0 past the last
int cubin_run_line(CubinRunLines *lines, char *line, size_t room)
{
    if (lines->at >= lines->size)
    {
        return 0;
    }
    const unsigned char *const start = lines->bytes + lines->at;
    const unsigned char *const end = (const unsigned char *)memchr(start, '\n', (size_t)(lines->size - lines->at));
    const size_t length = (end != NULL) ? ((size_t)(end - start) + 1u) : (size_t)(lines->size - lines->at);
    const size_t kept = (length < (room - 1u)) ? length : (room - 1u);
    memcpy(line, start, kept);
    line[kept] = '\0';
    lines->at += length;
    return 1;
}

void cubin_run_lines_close(CubinRunLines *lines)
{
    memset(lines, 0, sizeof(*lines));
}

// the cases input `name` read into s_cases: 1, or 0 where it does not read, holds no case or holds more than a
// launch's threads
int cubin_run_cases(const char *name)
{
    CubinRunLines lines;
    if (!cubin_run_lines_open(&lines, name))
    {
        return 0;
    }
    char line[CUBIN_RUN_LINE];
    unsigned int count = 0u;
    int fits = 1;
    while (fits && cubin_run_line(&lines, line, sizeof(line)))
    {
        unsigned int words[CUBIN_RUN_IN_WORDS] = {0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u};
        const char *at = line;
        unsigned int read = 0u;
        unsigned long long word = 0ull;
        while ((read < CUBIN_RUN_IN_WORDS) && line_number(&at, 1, &word) && (word <= 0xffffffffull))
        {
            // the test above holds the word inside 32 bits
            words[read] = (unsigned int)word;
            read += 1u;
        }
        if (read == 0u)
        {
            continue;
        }
        fits = (count < CUBIN_RUN_THREADS_MOST);
        if (fits)
        {
            memcpy(s_cases[count], words, sizeof(words));
            count += 1u;
        }
    }
    cubin_run_lines_close(&lines);
    s_case_count = count;
    return fits && (count != 0u);
}

// the rooms the carrier's answer, its dry record and its call are gathered in, each at most what a record holds
static char s_answer_bytes[QRY_WRITER_RING_BYTES / 2ull];
static char s_dry_bytes[QRY_WRITER_RING_BYTES / 2ull];
static char s_call_bytes[QRY_WRITER_RING_BYTES / 2ull];
ScripturaLine s_answer_said = {s_answer_bytes, sizeof(s_answer_bytes), 0ull};
ScripturaLine s_dry_said = {s_dry_bytes, sizeof(s_dry_bytes), 0ull};
ScripturaLine s_call_said = {s_call_bytes, sizeof(s_call_bytes), 0ull};

// What `saying` holds given whole as `name`: handed to the run at `priority`, a blob of its own after any of
// the name before it. `carried` where it was given, 2 where it was not, and what was said let go either way
int cubin_run_said(ScripturaLine *saying, const char *name, unsigned int priority, int carried)
{
    const int given = scriptura_fits(saying) && (qry_member_append(name, saying->out, saying->at, priority) != 0);
    saying->at = 0ull;
    return given ? carried : 2;
}

// the address of the entry point `name` in `library`, or NULL where it exports none
void *cubin_run_entry(void *library, const char *name)
{
#if defined(_WIN32)
    return (void *)GetProcAddress((HMODULE)library, name);
#else
    return dlsym(library, name);
#endif
}
