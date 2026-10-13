// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cubin.c: the container: held safe and run, its answers first and whole and by the toolchain, the environment, the clock, an ask's text and name, and the loop or the time preferred
#include "probe_ask_internal.h"

// A question of preference instead of membership. Every question above asks whether the part CAN do a thing;
// this asks which of two codings of one thing the part prefers. Both must answer the same, or they are not
// two codings of one thing and the reading is void. The part's preference is not what a listing says and not what
// the encoding says: it is the part's own answer, in its own clock.
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
    FILE *const file = fopen(path, "rb");
    const size_t size = (file != NULL) ? fread(s_safe_image, 1u, sizeof(s_safe_image), file) : 0u;
    if (file != NULL)
    {
        fclose(file);
    }
    unsigned long long at = 0ull;
    const unsigned int verdict = ((probe->machine == NULL) || (size == 0u) || (size == sizeof(s_safe_image)))
                                     ? CUBIN_SAFE_SIZE
                                     : cubin_safe_image(probe->machine, s_safe_image, size, 0u, &at);
    if (verdict != CUBIN_SAFE)
    {
        printf("  cubin: %s held off the part: %s at byte %llu\n", path, cubin_safe_name(verdict), at);
        return 0;
    }
    return 1;
}

// the cubin at `path` run on the device through interface_ptx_probe over one case whose first word is `first`, its answer
// into `answered`: 1, or 0 with the reason printed
static int sass_cubin_run(SassProbe *probe, const char *path, unsigned int first, char *answered, size_t room)
{
    char output[1024];
    snprintf(output, sizeof(output), "%s/run.out", probe->folder);
    char first_word[16];
    snprintf(first_word, sizeof(first_word), "%08x", first);
    // one case of eight words, none of the other seven zero, so that a question that divides is not asked for a zero
    // divisor
    char *const command[] = {(char *)probe->prober, (char *)"run",   (char *)path,        first_word,
                             (char *)"00000007",    (char *)"00000003", (char *)"00000005", (char *)"00000002",
                             (char *)"00000009",    (char *)"00000001", (char *)"00000004", NULL};
    const int status = sass_run(command, output);
    const char *const line = (status == 0) ? strstr(sass_output(), "answered ") : NULL;
    if (line == NULL)
    {
        printf("  cubin: %s did not run (exit %d)\n%s", path, status, (status > 0) ? sass_output() : "");
        return 0;
    }
    snprintf(answered, room, "%.*s", (int)strcspn(line, "\r\n"), line);
    return 1;
}

// the cubin at `path`, which our assembler wrote, held to cubin_safe and run as sass_cubin_run runs one
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

// `name` set to `value` in this process's environment, which the runner it starts inherits. MSVC defines putenv with
// an underscore and warns on the other; POSIX has setenv and takes the two apart
void sass_environment(const char *name, const char *value)
{
#if defined(_WIN32)
    char both[64];
    snprintf(both, sizeof(both), "%s=%s", name, value);
    _putenv(both);
#else
    setenv(name, value, 1);
#endif
}

// The nanoseconds a run took, as the runner timed it by the host's clock, or 0 where it printed none. The runner
// times a run only where PROBE_REPEATS is set, and the count is what the caller set before calling
double sass_cubin_nanoseconds(void)
{
    const char *const line = strstr(sass_output(), "timed ");
    const char *const taken = (line != NULL) ? strstr(line, ", ") : NULL;
    return (taken != NULL) ? strtod(taken + 2, NULL) : 0.0;
}

// form_0's text with the line that begins `IADD3 ` replaced by `instruction`, into `text`: 1, or 0 where the text
// holds no such line
int sass_ask_text(const char *was, const char *instruction, char *text, size_t room)
{
    size_t at = 0u;
    int put = 0;
    const char *walk = was;
    while ((*walk != '\0') && (at < (room - 1u)))
    {
        const size_t length = strcspn(walk, "\n");
        const char *const keep = (strncmp(walk, "IADD3 ", 6u) == 0) ? instruction : walk;
        const size_t kept = (keep == instruction) ? strlen(instruction) : length;
        put = put || (keep == instruction);
        if ((at + kept + 1u) < room)
        {
            memcpy(&text[at], keep, kept);
            at += kept;
            text[at] = '\n';
            at += 1u;
        }
        walk += length + ((walk[length] == '\n') ? 1u : 0u);
    }
    text[at] = '\0';
    return put;
}

// a question of more than one instruction named in one line: its instructions joined by a semicolon, and cut short
// with an ellipsis where the line will not hold them
void sass_ask_named(const char *instruction, char *named, size_t room)
{
    size_t at = 0u;
    for (const char *walk = instruction; (*walk != '\0') && (at < (room - 1u)); walk += 1)
    {
        named[at] = (*walk == '\n') ? ';' : *walk;
        at += 1u;
    }
    named[at] = '\0';
    // 52 is the column the answers line up in, and a question past it is named by its opening
    if (at > 52u)
    {
        memcpy(&named[49], "...", 4u);
    }
}

// `coding` wrapped in a loop of SASS_PREFER_TURNS turns, the count in R9, which the kernel leaves free. The loop's
// own four instructions are under both codings alike and cancel out of the difference
static int sass_prefer_loop(const char *coding, char *text, size_t room)
{
    return snprintf(text, room,
                    "IMAD.MOV.U32 R6, RZ, RZ, %u\n"
                    ".L_turn:\n"
                    "%s\n"
                    "IADD3 R6, P6, R6, -0x1, RZ\n"
                    "ISETP.NE.U32.AND P1, PT, R6, RZ, PT\n"
                    "@P1 BRA `(.L_turn)",
                    SASS_PREFER_TURNS, coding) < (int)room;
}

// one coding written into a cubin, run, and timed; its answer through `answered` and its nanoseconds returned, 0
// where it did not assemble or did not run
static double sass_prefer_time(SassProbe *probe, const SassMachine *machine, const char *was, const char *coding,
                               char *answered, size_t room)
{
    static char s_looped[8192];
    static char s_asking[65536];
    char path[1024];
    // the answer emptied first: a coding the assembler refuses is read as refused and not as the last one's word
    snprintf(answered, room, "refused");
    if (!sass_prefer_loop(coding, s_looped, sizeof(s_looped)) ||
        !sass_ask_text(was, s_looped, s_asking, sizeof(s_asking)) ||
        !sass_cubin_from_text(machine, probe->folder, "form_0", s_asking, "asked"))
    {
        return 0.0;
    }
    snprintf(path, sizeof(path), "%s/asked.cubin", probe->folder);
    char repeats[32];
    snprintf(repeats, sizeof(repeats), "%u", SASS_PREFER_RUNS);
    sass_environment("PROBE_REPEATS", repeats);
    const int ran = sass_cubin_answer(probe, path, answered, room);
    const double nanoseconds = ran ? sass_cubin_nanoseconds() : 0.0;
    sass_environment("PROBE_REPEATS", "0");
    return nanoseconds;
}

// Which of two codings of one thing the part prefers, asked of the part in its own clock. Both are
// run and both must answer what the question says, or they are not two codings of one thing; then both are timed,
// and the part's preference is the difference. How many were asked, and how many gave a reading
unsigned int sass_cubin_prefers(SassProbe *probe, const SassMachine *machine, unsigned int *asked)
{
    static const SassPrefer s_prefers[] = {
        // A move has two codings on this part, one on each pipe, and the compiler writes the FMA pipe's where the
        // integer pipe holds more (sass.ksc's pipe lines); neither is free. R0 holds the case's first word
        {"a move", "MOV R7, R0", "IMAD.MOV.U32 R7, RZ, RZ, R0", 0x0000000bu},
        // A long operation, which is where this is going: the high word of a 64-bit add. sass.krs writes
        // wide_add as IADD3 then IADD3.X; the other coding takes the carry with IMAD.X and adds the high word after, three
        // instructions against two. Both read R0, which the loop never writes. A turn leaves the next one what
        // it found: a body that carries its own answer forward measures a chain 400000 long and not the coding
        {"a wide add's high word", "IADD3 R8, P6, R0, R0, RZ\nIADD3.X R9, R0, R0, RZ, P6, !PT\nMOV R7, R9",
         "IADD3 R8, P6, R0, R0, RZ\nIMAD.X R9, RZ, RZ, R0, P6\nIADD3 R9, R9, R0, RZ\nMOV R7, R9", 0x00000016u},
        // a word doubled, added to itself against shifted left by one. SHF.L.U32 takes its count in a register on
        // this part and no listing gave it an immediate: the shift pays a move for the 1 it shifts by
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
        *asked += 1u;
        double least[2] = {0.0, 0.0};
        double spread = 0.0;
        int held = 1;
        for (unsigned int side = 0u; held && (side < 2u); side += 1u)
        {
            const char *const coding = (side == 0u) ? prefer->one : prefer->other;
            double most = 0.0;
            for (unsigned int take = 0u; held && (take < SASS_PREFER_TAKES); take += 1u)
            {
                char answer[256];
                const double taken = sass_prefer_time(probe, machine, s_was, coding, answer, sizeof(answer));
                // the answer is "answered <word> ..." where it ran, and "refused" where it did not assemble
                const unsigned int word = (strncmp(answer, "answered ", 9u) == 0)
                                              ? (unsigned int)strtoul(answer + 9, NULL, 16)
                                              : 0u;
                if ((taken == 0.0) || (strncmp(answer, "answered ", 9u) != 0) || (word != prefer->answer))
                {
                    printf("  prefer %-24s no reading: the %s coding %s\n", prefer->what,
                           (side == 0u) ? "first" : "second",
                           (taken == 0.0) ? "did not assemble" : "answers what the question does not say");
                    held = 0;
                    continue;
                }
                least[side] = ((least[side] == 0.0) || (taken < least[side])) ? taken : least[side];
                most = (taken > most) ? taken : most;
            }
            spread = ((most - least[side]) > spread) ? (most - least[side]) : spread;
        }
        if (held == 0)
        {
            // one of the two codings would not run, or answered something the question does not say: there is
            // no pair left to weigh
            sass_class_take(SASS_CHANNEL_CLOCK, SASS_CLASS_ILLEGAL, prefer->what, 0u);
            continue;
        }
        read += 1u;
        const double apart = (least[0] > least[1]) ? (least[0] - least[1]) : (least[1] - least[0]);
        const double each = apart / (double)SASS_PREFER_TURNS;
        // A reading the part gave, or a pair it ran and told nothing apart on: the two stood closer together than
        // the same coding stood to itself across its own runs, a question asked with no answer in it.
        //
        // The word kept is 0 and never the time. How long a coding took is a cost and belongs in the .kdm; what
        // belongs here is whether the question got an answer at all. Writing the time would also put a number that
        // moves a nanosecond between runs into a file meant to be read against the last one, and every run would
        // differ from the tree's copy for no reason the part could name
        sass_class_take(SASS_CHANNEL_CLOCK, (apart <= spread) ? SASS_CLASS_NOTHING : SASS_CLASS_ANSWERS, prefer->what,
                        0u);
        printf("  prefer %-24s first %9.0f ns, second %9.0f ns, %.0f ns apart against a spread of %.0f: %s\n",
               prefer->what, least[0], least[1], apart, spread,
               (apart <= spread) ? "no preference above the floor"
                                 : ((least[0] < least[1]) ? "the first" : "the second"));
        if (apart > spread)
        {
            printf("    %.4f ns a turn, %.2f of the part's ticks\n", each, each / SASS_PREFER_TICK);
        }
    }
    return read;
}
