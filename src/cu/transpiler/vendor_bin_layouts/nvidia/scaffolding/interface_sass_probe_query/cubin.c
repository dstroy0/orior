// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cubin.c: the container: checked safe and run, its answers first and whole and by the toolchain, the environment, the
// clock, a query's text and name, and the loop or the time preferred
#include "../../../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "probe_query_internal.h"

// A question of preference instead of membership. Every question above tests whether the part CAN do a thing;
// this tests which of two codings of one thing the part prefers. Both must answer the same, or they are not
// two codings of one thing and the reading is void. The part's preference is not what a listing shows and not what
// the encoding shows: it is the part's own answer, in its own clock.
typedef struct
{
    const char *what;
    const char *one;
    const char *other;
    unsigned int answer;
} SassPrefer;

static unsigned char s_safe_image[SASS_SAFE_CUBIN];

int sass_cubin_safe(const SassProbe *probe, const char *path)
{
    PlatformFile *const file = platform_file_open(path, "rb");
    const size_t size = (file != NULL) ? platform_file_read_items(s_safe_image, 1u, sizeof(s_safe_image), file) : 0u;
    if (file != NULL)
    {
        platform_file_close(file);
    }
    unsigned long long at = 0ULL;
    const unsigned int verdict = ((probe->machine == NULL) || (size == 0u) || (size == sizeof(s_safe_image)))
                                     ? CUBIN_SAFE_SIZE
                                     : cubin_safe_image(probe->machine, s_safe_image, size, 0u, &at);
    if (verdict != CUBIN_SAFE)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  cubin: ");
            mmgr_line_put(output_line, path);
            mmgr_line_put(output_line, " blocked from the part: ");
            mmgr_line_put(output_line, cubin_safe_name(verdict));
            mmgr_line_put(output_line, " at byte ");
            mmgr_line_decimal(output_line, at, 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    return 1;
}

// the cubin at `path` run on the device through interface_ptx_probe over one case whose first word is `first`, its
// answer into `answered`: 1, or 0 with the reason printed
static int sass_cubin_run(SassProbe *probe, const char *path, unsigned int first, char *answered, size_t room)
{
    char output[1024];
    {
        VerbaLinea buffer_line = mmgr_line_over(output, sizeof(output));
        mmgr_line_put(&buffer_line, probe->folder);
        mmgr_line_put(&buffer_line, "/run.out");
        (void)mmgr_line_finish(&buffer_line);
    }
    char first_word[16];
    {
        VerbaLinea buffer_line = mmgr_line_over(first_word, sizeof(first_word));
        mmgr_line_hex(&buffer_line, first, 8u);
        (void)mmgr_line_finish(&buffer_line);
    }
    // one case of eight words, none of the other seven zero, so that the call never queries a question that divides for
    // a zero divisor
    char *const command[] = {(char *)probe->prober, (char *)"run",      (char *)path,       first_word,
                             (char *)"00000007",    (char *)"00000003", (char *)"00000005", (char *)"00000002",
                             (char *)"00000009",    (char *)"00000001", (char *)"00000004", NULL};
    const int status = sass_run(command, output);
    const char *const line = (status == 0) ? EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = sass_output(),
                                                        .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "answered ",
                                                        .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE)
                                           : NULL;
    if (line == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  cubin: ");
            mmgr_line_put(output_line, path);
            mmgr_line_put(output_line, " did not run (exit ");
            mmgr_line_decimal_signed(output_line, status);
            mmgr_line_put(output_line, ")\n");
            mmgr_line_put(output_line, (status > 0) ? sass_output() : "");
            platform_line_out_ended();
        }
        return 0;
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(answered, room);
        mmgr_line_put_max(&buffer_line, line,
                          (size_t)((int)EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = line,
                                                   .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "\r\n")));
        (void)mmgr_line_finish(&buffer_line);
    }
    return 1;
}

// the cubin at `path`, which our assembler wrote, checked against cubin_safe and run as sass_cubin_run runs one
int sass_cubin_answer_first(SassProbe *probe, const char *path, unsigned int first, char *answered, size_t room)
{
    return sass_cubin_safe(probe, path) && sass_cubin_run(probe, path, first, answered, room);
}

int sass_cubin_answer(SassProbe *probe, const char *path, char *answered, size_t room)
{
    return sass_cubin_answer_first(probe, path, 0x0000000bu, answered, room);
}

int sass_cubin_answer_toolchain(SassProbe *probe, const char *path, char *answered, size_t room)
{
    return sass_cubin_run(probe, path, 0x0000000bu, answered, room);
}

// `name` set to `value` in this process's environment, which the runner it starts inherits
void sass_environment(const char *name, const char *value)
{
    (void)platform_environment_set(name, value);
}

// The nanoseconds that a run took, as the runner timed it by the clock of the host, or 0 where it printed none. The
// runner times a run only where the environment holds PROBE_REPEATS, and the count is what the caller set before the
// call
double sass_cubin_nanoseconds(void)
{
    const char *const line =
        EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = sass_output(), .cap = PLATFORM_TEXT_TO_TERMINATOR,
                   .other = "timed ", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
    const char *const taken =
        (line != NULL) ? EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                    .other = ", ", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE)
                       : NULL;
    return (taken != NULL) ? EMBED_CALL(cellul.to_double, TransfiguroCfg, .src = taken + 2, .end = NULL) : 0.0;
}

// form_0's text with the line that begins `IADD3 ` replaced by `instruction`, into `text`: 1, or 0 where the text
// contains no such line
int sass_query_text(const char *was, const char *instruction, char *text, size_t room)
{
    size_t at = 0u;
    int put = 0;
    const char *walk = was;
    while ((*walk != '\0') && (at < (room - 1u)))
    {
        const size_t length = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = walk,
                                         .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "\n");
        const char *const chosen =
            (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = walk, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "IADD3 ", .ci = EMBED_FALSE))
                ? instruction
                : walk;
        const size_t copied = (chosen == instruction) ? EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = instruction,
                                                                   .cap = PLATFORM_TEXT_TO_TERMINATOR)
                                                      : length;
        put = put || (chosen == instruction);
        if ((at + copied + 1u) < room)
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &text[at], .src = chosen, .bytes = copied);
            at += copied;
            text[at] = '\n';
            at += 1u;
        }
        walk += length + ((walk[length] == '\n') ? 1u : 0u);
    }
    text[at] = '\0';
    return put;
}

// a question of more than one instruction, named in one line. Its instructions go joined by a semicolon, cut short with
// an ellipsis where the line cannot hold them
void sass_query_named(const char *instruction, char *named, size_t room)
{
    size_t at = 0u;
    for (const char *walk = instruction; (*walk != '\0') && (at < (room - 1u)); walk += 1)
    {
        named[at] = (*walk == '\n') ? ';' : *walk;
        at += 1u;
    }
    named[at] = '\0';
    // 52 is the column that the answers line up in, and the call names a question past it by its opening
    if (at > 52u)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &named[49], .src = "...", .bytes = 4u);
    }
}

// `coding` wrapped in a loop of SASS_PREFER_TURNS turns, the count in R9, which the kernel leaves free. The loop's
// own four instructions are under both codings alike and cancel out of the difference
static int sass_prefer_loop(const char *coding, char *text, size_t room)
{
    VerbaLinea formed_line_1 = mmgr_line_over(text, room);
    mmgr_line_put(&formed_line_1, "IMAD.MOV.U32 R6, RZ, RZ, ");
    mmgr_line_decimal(&formed_line_1, SASS_PREFER_TURNS, 1u);
    mmgr_line_put(&formed_line_1, "\n.L_turn:\n");
    mmgr_line_put(&formed_line_1, coding);
    mmgr_line_put(&formed_line_1,
                  "\nIADD3 R6, P6, R6, -0x1, RZ\nISETP.NE.U32.AND P1, PT, R6, RZ, PT\n@P1 BRA `(.L_turn)");
    (void)mmgr_line_finish(&formed_line_1);
    return (int)formed_line_1.at < (int)room;
}

// one coding written into a cubin, run, and timed; its answer through `answered` and its nanoseconds returned, 0
// where it did not assemble or did not run
static double sass_prefer_time(SassProbe *probe, const SassMachine *machine, const char *was, const char *coding,
                               char *answered, size_t room)
{
    static char s_looped[8192];
    static char s_query_text[65536];
    char path[1024];
    // the answer, emptied first. The call reads a coding that the assembler refuses as refused, and not as the word of
    // the last one
    {
        VerbaLinea buffer_line = mmgr_line_over(answered, room);
        mmgr_line_put(&buffer_line, "refused");
        (void)mmgr_line_finish(&buffer_line);
    }
    if (!sass_prefer_loop(coding, s_looped, sizeof(s_looped)) ||
        !sass_query_text(was, s_looped, s_query_text, sizeof(s_query_text)) ||
        !sass_cubin_from_text(machine, probe->folder, "form_0", s_query_text, "queried"))
    {
        return 0.0;
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, probe->folder);
        mmgr_line_put(&buffer_line, "/queried.cubin");
        (void)mmgr_line_finish(&buffer_line);
    }
    char repeats[32];
    {
        VerbaLinea buffer_line = mmgr_line_over(repeats, sizeof(repeats));
        mmgr_line_decimal(&buffer_line, SASS_PREFER_RUNS, 1u);
        (void)mmgr_line_finish(&buffer_line);
    }
    sass_environment("PROBE_REPEATS", repeats);
    const int ran = sass_cubin_answer(probe, path, answered, room);
    const double nanoseconds = ran ? sass_cubin_nanoseconds() : 0.0;
    sass_environment("PROBE_REPEATS", "0");
    return nanoseconds;
}

// Which of two codings of one thing the part prefers, queried of the part in its own clock. The call runs both, and
// both must answer what the question specifies, or they are not two codings of one thing. Then the call times both,
// and the preference of the part is the difference. The return is how many the call queried, and how many gave a
// reading
unsigned int sass_cubin_prefers(SassProbe *probe, const SassMachine *machine, unsigned int *queried)
{
    static const SassPrefer s_prefers[] = {
        // A move has two codings on this part, one on each pipe. The compiler writes that of the FMA pipe where the
        // integer pipe carries more (the pipe lines of sass.ksc), and neither is free. R0 holds the first word of the
        // case
        {"a move", "MOV R7, R0", "IMAD.MOV.U32 R7, RZ, RZ, R0", 0x0000000bu},
        // A long operation, which is where this goes: the high word of a 64-bit add. The file sass.krs writes
        // wide_add as IADD3 then IADD3.X. The other coding takes the carry with IMAD.X and adds the high word after,
        // three instructions against two. Both read R0, which the loop never writes. A turn leaves the next one what
        // it found. A body that carries its own answer forward measures a chain 400000 long, and not the coding
        {"a wide add's high word", "IADD3 R8, P6, R0, R0, RZ\nIADD3.X R9, R0, R0, RZ, P6, !PT\nMOV R7, R9",
         "IADD3 R8, P6, R0, R0, RZ\nIMAD.X R9, RZ, RZ, R0, P6\nIADD3 R9, R9, R0, RZ\nMOV R7, R9", 0x00000016u},
        // a word doubled: added to itself, against a shift left by one. SHF.L.U32 takes its count in a register on
        // this part, and no listing gave it an immediate. Thus the shift pays a move for the 1 that it shifts by
        {"doubled", "IADD3 R7, R0, R0, RZ", "IMAD.MOV.U32 R8, RZ, RZ, 0x1\nSHF.L.U32 R7, R0, R8, RZ", 0x00000016u},
    };
    static char s_was[65536];
    if (sass_cubin_text(probe->folder, "form_0", s_was, sizeof(s_was)) == 0u)
    {
        return 0u;
    }
    unsigned int read = 0u;
    for (unsigned int number = 0u; number < (sizeof(s_prefers) / sizeof(s_prefers[0])); number += 1u)
    {
        const SassPrefer *const prefer = &s_prefers[number];
        *queried += 1u;
        double least[2] = {0.0, 0.0};
        double spread = 0.0;
        int written_ok = 1;
        for (unsigned int side = 0u; written_ok && (side < 2u); side += 1u)
        {
            const char *const coding = (side == 0u) ? prefer->one : prefer->other;
            double max = 0.0;
            for (unsigned int take = 0u; written_ok && (take < SASS_PREFER_TAKES); take += 1u)
            {
                char answer[256];
                const double taken = sass_prefer_time(probe, machine, s_was, coding, answer, sizeof(answer));
                // the answer reads "answered <word> ..." where it ran, and "refused" where it did not assemble
                const unsigned int word =
                    (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = answer, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                .other = "answered ", .ci = EMBED_FALSE))
                        ? (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = answer + 9, .end = NULL,
                                                   .base = 16)
                        : 0u;
                if ((taken == 0.0) ||
                    (!EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = answer, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                 .other = "answered ", .ci = EMBED_FALSE)) ||
                    (word != prefer->answer))
                {
                    {
                        VerbaLinea *const output_line = platform_line_out();
                        mmgr_line_put(output_line, "  prefer ");
                        mmgr_line_put_columns(output_line, prefer->what, 24u);
                        mmgr_line_put(output_line, " no reading: the ");
                        mmgr_line_put(output_line, (side == 0u) ? "first" : "second");
                        mmgr_line_put(output_line, " coding ");
                        mmgr_line_put(output_line, (taken == 0.0) ? "did not assemble"
                                                                  : "answers what the question does not specify");
                        mmgr_line_ch(output_line, '\n');
                        platform_line_out_ended();
                    }
                    written_ok = 0;
                    continue;
                }
                least[side] = ((least[side] == 0.0) || (taken < least[side])) ? taken : least[side];
                max = (taken > max) ? taken : max;
            }
            spread = ((max - least[side]) > spread) ? (max - least[side]) : spread;
        }
        if (written_ok == 0)
        {
            // one of the two codings would not run, or answered something the question does not specify: there is
            // no pair left to weigh
            sass_class_take(SASS_CHANNEL_CLOCK, SASS_CLASS_ILLEGAL, prefer->what, 0u);
            continue;
        }
        read += 1u;
        const double apart = (least[0] > least[1]) ? (least[0] - least[1]) : (least[1] - least[0]);
        const double each = apart / (double)SASS_PREFER_TURNS;
        // A reading that the part gave, or a pair that it ran and told nothing apart on. The two were closer together
        // than the same coding was to itself across its own runs: a question queried with no answer in it.
        //
        // The word stored is 0, and never the time. How long a coding took is a cost, and belongs in the .kdm. What
        // belongs here is whether the question got an answer at all. A write of the time would also put a number that
        // moves a nanosecond between runs into the file. A reader compares that file against the last one. Every run
        // would then differ from the copy of the tree for no reason that the part could name
        sass_class_take(SASS_CHANNEL_CLOCK, (apart <= spread) ? SASS_CLASS_NOTHING : SASS_CLASS_ANSWERS, prefer->what,
                        0u);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  prefer ");
            mmgr_line_put_columns(output_line, prefer->what, 24u);
            mmgr_line_put(output_line, " first ");
            mmgr_line_fixed(output_line, (double)(least[0]), 0u, 9u, EMBED_FALSE);
            mmgr_line_put(output_line, " ns, second ");
            mmgr_line_fixed(output_line, (double)(least[1]), 0u, 9u, EMBED_FALSE);
            mmgr_line_put(output_line, " ns, ");
            mmgr_line_fixed(output_line, (double)(apart), 0u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, " ns apart against a spread of ");
            mmgr_line_fixed(output_line, (double)(spread), 0u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, ": ");
            mmgr_line_put(output_line, (apart <= spread) ? "no preference above the floor"
                                                         : ((least[0] < least[1]) ? "the first" : "the second"));
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        if (apart > spread)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "    ");
            mmgr_line_fixed(output_line, (double)(each), 4u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, " ns a turn, ");
            mmgr_line_fixed(output_line, (double)(each / SASS_PREFER_TICK), 2u, 0u, EMBED_FALSE);
            mmgr_line_put(output_line, " of the part's ticks");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    return read;
}
