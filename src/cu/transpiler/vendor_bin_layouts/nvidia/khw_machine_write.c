// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// khw_machine_write.c: the vendor's writer, which reads what the query protocol answered and writes the .khw the gate
// reads. The protocol asks and keeps the answers; this names them and lays them out in the vendor's own machine file
// (P14). It is a program the protocol runs through the interface, and nothing of it reaches back into the protocol.
//
//     khw_machine_write <part> <path.khw> <layout> <mnemonics> <answers> <mode>
//
// Four modes. In `krs` the file at `path.khw` is read and each form that answered a relation is written to `answers`
// for the protocol, one a line: the relation, 1 where the answers read a word signed and 0 where not, the encoding the
// form was first seen with, low word first, and the form's text with each operand that stands in the relation's tuple
// written as its place, {<place>}, for the protocol to name; nothing else is read. In `kernel` the container the layout holds is read and the kernel the system accepted is written to
// `answers` for the protocol: a header of its places and the registers a thread holds, then one line a place with the
// place's encoding. In `gate` the file holds those same places, one form a place, so that the gate reads every
// question of a round against what the part has already run (cubin_safe.h). In `final` the file holds the forms the
// part answered for, read from the answers file the protocol wrote, each named from the vendor's table
// (mnemonic_nvidia.tsv) and laid out as the instruction its runs make; a form the table names none of is no form of
// the file. The answers file is one form a line: the relation the protocol read, whether it read a word signed and
// whether it asked, the encoding, how many runs its operands sit in, and each run's first and last bit and its place
// in the relation's tuple, - where it has none. Each form is written with its relation, its signedness and its runs'
// places as the protocol read them.
#include "cubin_write.h"
#include "sass_machine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the most bytes a container takes, the most places its code holds, the bytes of one instruction, and the most
// endings the container records
#define KHW_PATTERN_BYTES 262144u
#define KHW_PLACES 256u
#define KHW_INSTRUCTION 16u
#define KHW_EXITS 64u

// the most pieces the vendor's table holds, and the longest piece and the longest column of reads in it
#define KHW_PIECES 128u
#define KHW_PIECE 32u
#define KHW_READS 256u

// the longest name a form is composed
#define KHW_NAME 64u

// the register number the part answers nothing at, which a form prints by the name the tree writes it under
#define KHW_REGISTER_NONE 255u
#define KHW_REGISTER_NONE_TEXT "RZ"

// what the vendor's table calls the reads that put a piece in a name: a relation of the ladder, and the word of the
// high order language for a word read as an unsigned integer, the kind the ladder's cases hold
#define KHW_READS_WORD "hol.u<n>_t"

// the qualifiers the answers decide between where the two readings of a case differ
#define KHW_PIECE_UNSIGNED "U32"
#define KHW_PIECE_SIGNED "S32"

// one piece of a name as the vendor's table gives it
typedef struct
{
    char kind[KHW_PIECE];
    char piece[KHW_PIECE];
    char reads[KHW_READS];
} KhwPiece;

static KhwPiece s_piece[KHW_PIECES];
static unsigned int s_pieces;

// the container the system accepted, the part it enters at, how many registers a thread of it holds, the kernel's
// code and how many places it holds
static unsigned char s_pattern[KHW_PATTERN_BYTES];
static unsigned long long s_pattern_size;
static char s_kernel[128];
static unsigned int s_registers;
static unsigned char s_kernel_text[KHW_PLACES * KHW_INSTRUCTION];
static unsigned int s_kernel_places;

// the machine the file holds
static SassMachine s_machine;

// What the protocol answered of one form, read from the answers file: the relation every case of it answered, whether
// the answers read a word signed and whether a case of the relation let the part answer so, the encoding, and the
// runs of bits its operands sit in with each run's place in the relation's tuple
typedef struct
{
    char relation[KHW_READS];
    int signed_read;
    int signedness_asked;
    unsigned long long low;
    unsigned long long high;
    unsigned int runs;
    unsigned int first[SASS_MACHINE_RUNS];
    unsigned int last[SASS_MACHINE_RUNS];
    unsigned int place[SASS_MACHINE_RUNS];
} KhwForm;

// one word of an instruction
static unsigned long long khw_word_read(const unsigned char *instruction, unsigned int byte)
{
    unsigned long long value = 0ull;
    for (unsigned int at = 0u; at < 8u; at += 1u)
    {
        value |= (unsigned long long)instruction[byte + at] << (8u * at);
    }
    return value;
}

// the bits `first` to `last` of the instruction `low` and `high` make
static unsigned long long khw_run_read(unsigned long long low, unsigned long long high, unsigned int first,
                                       unsigned int last)
{
    unsigned long long value = 0ull;
    for (unsigned int bit = first; bit <= last; bit += 1u)
    {
        const unsigned long long word = (bit < 64u) ? low : high;
        value |= ((word >> (bit % 64u)) & 1ull) << (bit - first);
    }
    return value;
}

// the pieces of the vendor's table at `path`, into s_piece. 1, or 0 with the reason printed
static int khw_pieces_read(const char *path)
{
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        printf("  khw_machine_write: %s could not be read\n", path);
        return 0;
    }
    char line[1024];
    while (fgets(line, sizeof(line), file) != NULL)
    {
        line[strcspn(line, "\r\n")] = '\0';
        if ((line[0] == '#') || (line[0] == '\0') || (s_pieces == KHW_PIECES))
        {
            continue;
        }
        char *const piece = strchr(line, '\t');
        char *const reads = (piece != NULL) ? strchr(piece + 1, '\t') : NULL;
        if (reads == NULL)
        {
            continue;
        }
        *piece = '\0';
        *reads = '\0';
        char *const gloss = strchr(reads + 1, '\t');
        if (gloss != NULL)
        {
            *gloss = '\0';
        }
        snprintf(s_piece[s_pieces].kind, sizeof(s_piece[s_pieces].kind), "%.*s", (int)(KHW_PIECE - 1u), line);
        snprintf(s_piece[s_pieces].piece, sizeof(s_piece[s_pieces].piece), "%.*s", (int)(KHW_PIECE - 1u), piece + 1);
        snprintf(s_piece[s_pieces].reads, sizeof(s_piece[s_pieces].reads), "%.*s", (int)(KHW_READS - 1u), reads + 1);
        s_pieces += 1u;
    }
    fclose(file);
    if (s_pieces == 0u)
    {
        printf("  khw_machine_write: %s holds no piece of a name\n", path);
        return 0;
    }
    return 1;
}

// 1 where the reads of `piece` hold `word` whole, and not as part of a longer one
static int khw_reads_hold(const KhwPiece *piece, const char *word)
{
    const size_t length = strlen(word);
    const char *at = piece->reads;
    while ((at = strstr(at, word)) != NULL)
    {
        const int before = (at == piece->reads) || (*(at - 1) == ' ');
        const int after = (at[length] == '\0') || (at[length] == ' ');
        if (before && after)
        {
            return 1;
        }
        at += length;
    }
    return 0;
}

// the piece of kind `kind` whose reads hold `word`, or NULL where the table gives none
static const char *khw_piece_reading(const char *kind, const char *word)
{
    for (unsigned int at = 0u; at < s_pieces; at += 1u)
    {
        if ((strcmp(s_piece[at].kind, kind) == 0) && khw_reads_hold(&s_piece[at], word))
        {
            return s_piece[at].piece;
        }
    }
    return NULL;
}

// the piece of kind `kind` whose own text is `text`, or NULL where the table gives none
static const char *khw_piece_named(const char *kind, const char *text)
{
    for (unsigned int at = 0u; at < s_pieces; at += 1u)
    {
        if ((strcmp(s_piece[at].kind, kind) == 0) && (strcmp(s_piece[at].piece, text) == 0))
        {
            return s_piece[at].piece;
        }
    }
    return NULL;
}

// The name of a form the part answered `held` of, composed from the pieces of the vendor's table: the kind of value
// the cases hold, the relation answered, how many sources the fields found, and the qualifier the answers read a word
// into where a case of the relation reads two ways. Into `written`, which holds `room`
static void khw_name_composed(const KhwForm *held, char *written, size_t room)
{
    const char *const kind = khw_piece_reading("kind", KHW_READS_WORD);
    const char *const operation = khw_piece_reading("operation", held->relation);
    written[0] = '\0';
    if (operation == NULL)
    {
        return;
    }
    size_t at = 0u;
    at += (size_t)snprintf(&written[at], room - at, "%s%s", (kind != NULL) ? kind : "", operation);
    // the count of sources the part answered fields for, written where the table holds that count for this relation
    char sources[KHW_PIECE];
    snprintf(sources, sizeof(sources), "%u", (held->runs > 0u) ? (held->runs - 1u) : 0u);
    for (unsigned int piece = 0u; piece < s_pieces; piece += 1u)
    {
        if ((strcmp(s_piece[piece].kind, "count") == 0) && (strcmp(s_piece[piece].piece, sources) == 0) &&
            khw_reads_hold(&s_piece[piece], held->relation))
        {
            at += (size_t)snprintf(&written[at], room - at, "%s", sources);
            break;
        }
    }
    if (held->signedness_asked != 0)
    {
        const char *const read_as =
            khw_piece_named("qualifier", (held->signed_read != 0) ? KHW_PIECE_SIGNED : KHW_PIECE_UNSIGNED);
        if (read_as != NULL)
        {
            at += (size_t)snprintf(&written[at], room - at, ".%s", read_as);
        }
    }
    (void)at;
}

// The text of a form the part answered `held` of: its name, then each operand in the order its run's first bit rises,
// the order they are printed in, each read from the encoding through its own run. A register the part
// answers nothing at is printed by the name the tree writes it under
static void khw_text_composed(const KhwForm *held, char *written, size_t room)
{
    char name[KHW_NAME];
    khw_name_composed(held, name, sizeof(name));
    written[0] = '\0';
    if (name[0] == '\0')
    {
        return;
    }
    size_t at = (size_t)snprintf(written, room, "%s", name);
    for (unsigned int run = 0u; (run < held->runs) && (at < room); run += 1u)
    {
        const unsigned long long number = khw_run_read(held->low, held->high, held->first[run], held->last[run]);
        if (number == KHW_REGISTER_NONE)
        {
            at += (size_t)snprintf(&written[at], room - at, "%s%s", (run == 0u) ? " " : ", ", KHW_REGISTER_NONE_TEXT);
        }
        else
        {
            at += (size_t)snprintf(&written[at], room - at, "%sR%llu", (run == 0u) ? " " : ", ", number);
        }
    }
}

// `held` kept in `machine` as a form under `text`, with the runs the part answered for its operands. 1, or 0 where
// the machine is full
static int khw_form_kept(SassMachine *machine, const KhwForm *held, const char *text)
{
    SassForm *kept = NULL;
    if (!sass_machine_take(machine, text, held->low, held->high, &kept) || (kept == NULL))
    {
        return 0;
    }
    snprintf(kept->relation, sizeof(kept->relation), "%s", held->relation);
    kept->signed_read = held->signed_read;
    kept->runs = 0u;
    for (unsigned int run = 0u; (run < held->runs) && (kept->runs < SASS_MACHINE_RUNS); run += 1u)
    {
        kept->run[kept->runs].operand = run;
        kept->run[kept->runs].first = held->first[run];
        kept->run[kept->runs].last = held->last[run];
        kept->run[kept->runs].place = held->place[run];
        kept->runs += 1u;
    }
    return 1;
}

// the kernel, read out of the container the layout at `path` holds: the container's bytes, the part it enters at, the
// registers a thread of it holds, and its code. 1, or 0 with the reason printed
static int khw_kernel_read(const char *path)
{
    s_pattern_size = cubin_pattern_read(path, s_pattern, sizeof(s_pattern), s_kernel, sizeof(s_kernel));
    if (s_pattern_size == 0ull)
    {
        return 0;
    }
    unsigned long long offsets[KHW_PLACES];
    unsigned long long sizes[KHW_PLACES];
    const unsigned int sections = cubin_code_sections(s_pattern, s_pattern_size, offsets, sizes, KHW_PLACES);
    if (sections == 0u)
    {
        printf("  khw_machine_write: the container at %s holds no code the kernel is read from\n", path);
        return 0;
    }
    if ((sizes[0] == 0ull) || ((sizes[0] % KHW_INSTRUCTION) != 0ull) ||
        (sizes[0] > (unsigned long long)sizeof(s_kernel_text)))
    {
        printf("  khw_machine_write: the kernel is %llu bytes, which is no whole count of places\n", sizes[0]);
        return 0;
    }
    memcpy(s_kernel_text, &s_pattern[offsets[0]], (size_t)sizes[0]);
    s_kernel_places = (unsigned int)(sizes[0] / KHW_INSTRUCTION);
    s_registers = cubin_registers_read(s_pattern, s_pattern_size, s_kernel);
    if (s_registers == 0u)
    {
        printf("  khw_machine_write: the container says how many registers no thread of %s holds\n", s_kernel);
        return 0;
    }
    return 1;
}

// Every place of the kernel kept in the machine as a form, so that the gate reads a question against what the part has
// already run (cubin_safe.h, its rules 2 and 3). A place the container records as an ending carries the table's name
// for the thread's end, which the gate reads to find where the code stops; every other place carries its own encoding
// whole, so that the gate holds it as the system wrote it until the part names it. The count kept
static unsigned int khw_kernel_forms_kept(void)
{
    unsigned int exits[KHW_EXITS];
    const unsigned int endings = cubin_exits_read(s_pattern, s_pattern_size, s_kernel, exits, KHW_EXITS);
    const char *const ends = khw_piece_reading("operation", "control_coherence");
    unsigned int kept = 0u;
    for (unsigned int place = 0u; place < s_kernel_places; place += 1u)
    {
        const unsigned long long low = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 0u);
        const unsigned long long high = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 8u);
        int ending = 0;
        for (unsigned int at = 0u; at < endings; at += 1u)
        {
            ending = ending || (exits[at] == (place * KHW_INSTRUCTION));
        }
        char text[KHW_NAME];
        if ((ending != 0) && (ends != NULL))
        {
            snprintf(text, sizeof(text), "%s", ends);
        }
        else
        {
            snprintf(text, sizeof(text), "0x%016llx%016llx", low, high);
        }
        SassForm *taken = NULL;
        const int took = sass_machine_take(&s_machine, text, low, high, &taken);
        // an ending whose encoding differs from the one already held under its name is held whole beside it, so that
        // the gate holds every ending as the system wrote it
        if (took && (taken != NULL) && ((taken->low != low) || (taken->high != high)) &&
            (s_machine.forms < SASS_MACHINE_FORMS))
        {
            s_machine.form[s_machine.forms] = *taken;
            s_machine.form[s_machine.forms].low = low;
            s_machine.form[s_machine.forms].high = high;
            s_machine.forms += 1u;
        }
        kept += took ? 1u : 0u;
    }
    return kept;
}

// the runs of `held` read from `text`, `held->runs` of them: three words a run, its first bit, its last and its place,
// or two, its first bit and its last, where the line holds no place, every run then holding none. 1, or 0 where the
// words are neither
static int khw_runs_read(KhwForm *held, const char *text)
{
    unsigned int words = 0u;
    for (const char *at = text; *at != '\0';)
    {
        at += strspn(at, " \t\r\n");
        const size_t length = strcspn(at, " \t\r\n");
        words += (length != 0u) ? 1u : 0u;
        at += length;
    }
    const unsigned int each = (words == (3u * held->runs)) ? 3u : ((words == (2u * held->runs)) ? 2u : 0u);
    if (each == 0u)
    {
        return 0;
    }
    const char *rest = text;
    for (unsigned int run = 0u; run < held->runs; run += 1u)
    {
        int step = 0;
        char place[16];
        held->place[run] = SASS_RUN_NO_PLACE;
        if (each == 2u)
        {
            if (sscanf(rest, "%u %u %n", &held->first[run], &held->last[run], &step) != 2)
            {
                return 0;
            }
        }
        else
        {
            if (sscanf(rest, "%u %u %15s %n", &held->first[run], &held->last[run], place, &step) != 3)
            {
                return 0;
            }
            held->place[run] = (strcmp(place, SASS_RUN_NO_PLACE_TEXT) == 0) ? SASS_RUN_NO_PLACE
                                                                             : (unsigned int)strtoul(place, NULL, 10);
        }
        rest += step;
    }
    return 1;
}

// the forms the protocol answered for, read from the answers file at `path` into `form`, as many as `room` holds. The
// count read
static unsigned int khw_answers_read(const char *path, KhwForm *form, unsigned int room)
{
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        printf("  khw_machine_write: %s could not be read\n", path);
        return 0u;
    }
    char line[2048];
    unsigned int read = 0u;
    while ((fgets(line, sizeof(line), file) != NULL) && (read < room))
    {
        if ((line[0] == '#') || (line[0] == '\0') || (line[0] == '\n'))
        {
            continue;
        }
        KhwForm held;
        memset(&held, 0, sizeof(held));
        int at = 0;
        if (sscanf(line, "%255s %d %d %llx %llx %u %n", held.relation, &held.signed_read, &held.signedness_asked,
                   &held.low, &held.high, &held.runs, &at) != 6)
        {
            continue;
        }
        if ((held.runs > SASS_MACHINE_RUNS) || !khw_runs_read(&held, line + at))
        {
            continue;
        }
        form[read] = held;
        read += 1u;
    }
    fclose(file);
    return read;
}

int main(int count, char **word)
{
    if (count != 7)
    {
        printf("khw_machine_write <part> <path.khw> <layout> <mnemonics> <answers> <mode>\n");
        return 2;
    }
    const char *const part = word[1];
    const char *const path = word[2];
    const char *const layout = word[3];
    const char *const mnemonics = word[4];
    const char *const answers = word[5];
    const char *const mode = word[6];
    if (strcmp(mode, "kernel") == 0)
    {
        // the kernel read out of the container and written to `answers` for the protocol: its places and the registers
        // a thread holds, then one line a place with the place's encoding
        if (!khw_kernel_read(layout))
        {
            return 1;
        }
        FILE *const out = fopen(answers, "wb");
        if (out == NULL)
        {
            printf("  khw_machine_write: %s could not be written\n", answers);
            return 1;
        }
        fprintf(out, "kernel %u %u\n", s_kernel_places, s_registers);
        for (unsigned int place = 0u; place < s_kernel_places; place += 1u)
        {
            const unsigned long long low = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 0u);
            const unsigned long long high = khw_word_read(&s_kernel_text[place * KHW_INSTRUCTION], 8u);
            fprintf(out, "%016llx %016llx\n", low, high);
        }
        fclose(out);
        printf("  the kernel %s: %u places, %u registers a thread\n", s_kernel, s_kernel_places, s_registers);
        return 0;
    }
    if (strcmp(mode, "krs") == 0)
    {
        if (!sass_machine_read(&s_machine, path))
        {
            return 1;
        }
        FILE *const out = fopen(answers, "wb");
        if (out == NULL)
        {
            printf("  khw_machine_write: %s could not be written\n", answers);
            return 1;
        }
        unsigned int written = 0u;
        for (unsigned int at = 0u; at < s_machine.forms; at += 1u)
        {
            const SassForm *const form = &s_machine.form[at];
            char placed[SASS_MACHINE_TEXT];
            if ((form->relation[0] == '\0') || !sass_text_placed(form, placed, sizeof(placed)))
            {
                continue;
            }
            fprintf(out, "%s %d %016llx %016llx %s\n", form->relation, (form->signed_read != 0) ? 1 : 0, form->low,
                    form->high, placed);
            written += 1u;
        }
        fclose(out);
        printf("  khw_machine_write: %u of the %u forms of %s answered a relation, written to %s\n", written,
               s_machine.forms, path, answers);
        return 0;
    }
    if (!khw_pieces_read(mnemonics))
    {
        return 1;
    }
    memset(&s_machine, 0, sizeof(s_machine));
    snprintf(s_machine.part, sizeof(s_machine.part), "%s", part);
    if (strcmp(mode, "gate") == 0)
    {
        if (!khw_kernel_read(layout))
        {
            return 1;
        }
        const unsigned int kept = khw_kernel_forms_kept();
        if (!sass_machine_write(&s_machine, path))
        {
            printf("  khw_machine_write: %s could not be written\n", path);
            return 1;
        }
        printf("  the kernel's places: %u forms the gate reads a question against\n", kept);
        return 0;
    }
    if (strcmp(mode, "final") != 0)
    {
        printf("  khw_machine_write: %s is no mode this writes\n", mode);
        return 2;
    }
    static KhwForm s_form[SASS_MACHINE_FORMS];
    const unsigned int forms = khw_answers_read(answers, s_form, SASS_MACHINE_FORMS);
    unsigned int named = 0u;
    for (unsigned int at = 0u; at < forms; at += 1u)
    {
        char text[SASS_MACHINE_TEXT];
        khw_text_composed(&s_form[at], text, sizeof(text));
        if (text[0] == '\0')
        {
            continue;
        }
        if (!khw_form_kept(&s_machine, &s_form[at], text))
        {
            printf("  khw_machine_write: %s holds no more forms\n", path);
            return 1;
        }
        named += 1u;
    }
    if (!sass_machine_write(&s_machine, path))
    {
        printf("  khw_machine_write: %s could not be written\n", path);
        return 1;
    }
    printf("  khw_machine_write: %u of the %u forms the part answered for named by the vendor's table and written to "
           "%s\n",
           named, forms, path);
    return 0;
}
