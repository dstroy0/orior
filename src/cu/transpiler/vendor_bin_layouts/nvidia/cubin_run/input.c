// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// input.c: the input: a blob or a file read, the ask key, the verdict and launch rows put to the run, the input's lines, the cases of a launch, and what the carrier says of an answer, a dry run or a call
#include "cubin_run_internal.h"

SassMachine s_machine;
unsigned char s_pattern[CUBIN_RUN_BYTES];
unsigned char s_code[CUBIN_RUN_BYTES];
unsigned char s_container[CUBIN_RUN_BYTES];
unsigned int s_exits[CUBIN_RUN_EXITS];
unsigned int s_cases[CUBIN_RUN_THREADS_MOST][CUBIN_RUN_IN_WORDS];
unsigned int s_case_count;
unsigned int s_answers[CUBIN_RUN_THREADS_MOST][CUBIN_RUN_OUT_WORDS];

// The run's buffer (qry_buffer.h): every channel input the carrier is named is a blob of it, the latest of that name,
// and every answer, dry line and container it gives is handed to it: no file of the channel is written but by the
// run's writer. The carrier carries nothing where it is NULL (main)
QryBuffer *s_run = NULL;

// the channel input `name` read whole into `bytes`, which holds `room`: the latest blob of it in the run's buffer. The
// bytes read, or 0 where it was not read or does not fit
unsigned long long cubin_run_input(const char *name, unsigned char *bytes, unsigned long long room)
{
    const unsigned char *held = NULL;
    unsigned long long size = 0ull;
    if (!qry_latest(s_run, name, &held, &size, NULL) || (size >= room))
    {
        return 0ull;
    }
    // the blob fits `bytes`, whose count a size_t holds
    memcpy(bytes, held, (size_t)size);
    return size;
}

// the key of the ask whose code is the channel input `code_path`: the sequence of its latest blob in the run's buffer,
// or 0 where the run holds none
unsigned long long cubin_run_ask_key(const char *code_path)
{
    const unsigned char *held = NULL;
    unsigned long long size = 0ull;
    unsigned long long sequence = 0ull;
    return qry_latest(s_run, code_path, &held, &size, &sequence) ? sequence : 0ull;
}

// the gate's verdict on the ask `ask`, `verdict` and the byte `at` it held at, a row of the run's table of verdicts:
// put or held, and carried where the part is there to carry it
void cubin_run_verdict_row(unsigned long long ask, unsigned int verdict, unsigned long long at, int dry)
{
    const char *const reason = cubin_safe_name(verdict);
    const int held = (verdict != CUBIN_SAFE);
    const unsigned long long row[6] = {ask,
                                       verdict,
                                       held ? 1ull : 0ull,
                                       held ? at : 0ull,
                                       qry_row_text(s_run, reason, strlen(reason)),
                                       dry ? 0ull : 1ull};
    qry_row_handed(s_run, CUBIN_RUN_VERDICTS, row, 6u);
}

// a launch of the ask `ask`, its outcome `outcome` and what the carrier said of it, `said`, empty where it answered, a
// row of the run's table of launches with its timed launches' count and times
void cubin_run_launch_row(unsigned long long ask, unsigned int outcome, unsigned int launches,
                                 unsigned long long nanoseconds, unsigned long long least, unsigned long long most,
                                 const char *said)
{
    const unsigned long long row[7] = {
        ask, outcome, launches, nanoseconds, least, most, qry_row_text(s_run, said, strlen(said))};
    qry_row_handed(s_run, CUBIN_RUN_LAUNCHES, row, 7u);
}

// the lines of the input `name` opened: 1, or 0 where it is not there
int cubin_run_lines_open(CubinRunLines *lines, const char *name)
{
    memset(lines, 0, sizeof(*lines));
    return qry_latest(s_run, name, &lines->bytes, &lines->size, NULL);
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
        const int read = sscanf(line, "%x %x %x %x %x %x %x %x", &words[0], &words[1], &words[2], &words[3], &words[4],
                                &words[5], &words[6], &words[7]);
        if (read <= 0)
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

CubinRunSaying s_answer_said;
CubinRunSaying s_dry_said;
CubinRunSaying s_call_said;

// `format` and what follows it added to `saying`: 1, or 0 where there is no memory for it
int cubin_run_say(CubinRunSaying *saying, const char *format, ...)
{
    va_list values;
    va_start(values, format);
    va_list measured;
    va_copy(measured, values);
    const int length = vsnprintf(NULL, 0u, format, measured);
    va_end(measured);
    int kept = (length >= 0);
    // the length counted is not negative here, and the text and its end fit what it grows to
    const size_t needed = kept ? (saying->size + (size_t)length + 1u) : 0u;
    if (kept && (needed > saying->room))
    {
        size_t grown = (saying->room != 0u) ? saying->room : 4096u;
        while (grown < needed)
        {
            grown *= 2u;
        }
        char *const larger = (char *)realloc(saying->text, grown);
        kept = (larger != NULL);
        saying->text = kept ? larger : saying->text;
        saying->room = kept ? grown : saying->room;
    }
    if (kept)
    {
        vsnprintf(saying->text + saying->size, saying->room - saying->size, format, values);
        saying->size += (size_t)length;
    }
    va_end(values);
    return kept;
}

// What `saying` holds given whole as `name`: handed to the run's buffer at `priority`, a blob of its own after any of
// the name before it. `carried` where it was given, 2 where it was not, and what was said let go either way
int cubin_run_said(CubinRunSaying *saying, const char *name, unsigned int priority, int carried)
{
    const int given = qry_hand(s_run, name, saying->text, saying->size, priority) != 0ull;
    saying->size = 0u;
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
