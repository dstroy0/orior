// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pieces.c: the pieces: a word and a run read, the pieces of the vendor's listing read, what a read holds, a piece's reading and name, a form's name and text composed, and a form kept
#include "khw_machine_write_internal.h"

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
unsigned char s_pattern[KHW_PATTERN_BYTES];

unsigned long long s_pattern_size;
char s_kernel[128];
unsigned int s_registers;
unsigned char s_kernel_text[KHW_PLACES * KHW_INSTRUCTION];
unsigned int s_kernel_places;

// the machine the file holds
SassMachine s_machine;

// one word of an instruction
unsigned long long khw_word_read(const unsigned char *instruction, unsigned int byte)
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
int khw_pieces_read(const char *path)
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
const char *khw_piece_reading(const char *kind, const char *word)
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
// the cases hold, the piece whose reads hold the relation, none where no kind row reads it; the relation answered; how
// many sources the fields found; and the qualifier the answers read a word into where a case of the relation reads
// two ways. Into `written`, which holds `room`
static void khw_name_composed(const KhwForm *held, char *written, size_t room)
{
    const char *const kind = khw_piece_reading("kind", held->relation);
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

// 1 where `run` of `held` is a flag: one bit with no place, which the form's encoding holds and no operand prints
static int khw_flag(const KhwForm *held, unsigned int run)
{
    return (held->place[run] == SASS_RUN_NO_PLACE) && (held->first[run] == held->last[run]);
}

// 1 where `run` of `held` prints no operand of its own: a flag; or a run before it stands at the same place of the
// relation's tuple, two runs of one place being one operand printed once, by the first. A wider run with no place
// prints as its kind reads its bits, a register where the table names nothing for it, so that two forms apart in such
// a run read apart
static int khw_place_printed(const KhwForm *held, unsigned int run)
{
    if (khw_flag(held, run))
    {
        return 1;
    }
    for (unsigned int before = 0u; before < run; before += 1u)
    {
        if ((held->place[run] != SASS_RUN_NO_PLACE) && (held->place[before] == held->place[run]))
        {
            return 1;
        }
    }
    return 0;
}

// The text of a form the part answered `held` of: a predicate run as the vendor writes a guard, `@P<n>` before the
// operation, `@!P<n>` where the bit past the predicate's three is set, and nothing where it is every predicate; its
// name; then each other run in the order its first bit rises, the order they are printed in, the runs of one place
// one operand printed by the first, each read from the encoding through its own run by its kind: a register `R<n>`,
// or the name the tree writes a register the part answers nothing at under; an address `[R<n>.64]`; a number `0x<n>`
void khw_text_composed(const KhwForm *held, char *written, size_t room)
{
    char name[KHW_NAME];
    khw_name_composed(held, name, sizeof(name));
    written[0] = '\0';
    if (name[0] == '\0')
    {
        return;
    }
    size_t at = 0u;
    for (unsigned int run = 0u; (run < held->runs) && (at < room); run += 1u)
    {
        if ((held->kind[run] != KHW_KIND_PREDICATE) || khw_place_printed(held, run))
        {
            continue;
        }
        const unsigned long long guard = khw_run_read(held->low, held->high, held->first[run], held->last[run]);
        const unsigned long long predicate = guard & 7ull;
        if (predicate != 7ull)
        {
            at +=
                (size_t)snprintf(&written[at], room - at, "@%sP%llu ", ((guard & 8ull) != 0ull) ? "!" : "", predicate);
        }
    }
    at += (size_t)snprintf(&written[at], room - at, "%s", name);
    // the mark the vendor writes before the operand at the relation's second word, where the table gives one
    const char *const marked = khw_piece_reading("mark", held->relation);
    unsigned int printed = 0u;
    for (unsigned int run = 0u; (run < held->runs) && (at < room); run += 1u)
    {
        if ((held->kind[run] == KHW_KIND_PREDICATE) || khw_place_printed(held, run))
        {
            continue;
        }
        const unsigned long long number = khw_run_read(held->low, held->high, held->first[run], held->last[run]);
        const char *const mark = ((marked != NULL) && (held->place[run] == 1u)) ? marked : "";
        char between[8];
        snprintf(between, sizeof(between), "%s%s", (printed == 0u) ? " " : ", ", mark);
        if (held->kind[run] == KHW_KIND_IMMEDIATE)
        {
            at += (size_t)snprintf(&written[at], room - at, "%s0x%llx", between, number);
        }
        else if (held->kind[run] == KHW_KIND_ADDRESS)
        {
            at += (size_t)snprintf(&written[at], room - at, "%s[R%llu.64]", between, number);
        }
        else if (number == KHW_REGISTER_NONE)
        {
            at += (size_t)snprintf(&written[at], room - at, "%s%s", between, KHW_REGISTER_NONE_TEXT);
        }
        else
        {
            at += (size_t)snprintf(&written[at], room - at, "%sR%llu", between, number);
        }
        printed += 1u;
    }
}

// `held` kept in `machine` as a form under `text`, with the runs the part answered for its operands. 1, or 0 where
// the machine is full
int khw_form_kept(SassMachine *machine, const KhwForm *held, const char *text)
{
    SassForm *kept = NULL;
    if (!sass_machine_take(machine, text, held->low, held->high, &kept) || (kept == NULL))
    {
        return 0;
    }
    // the relation's name fits the form's room, and a longer one is cut to it
    snprintf(kept->relation, sizeof(kept->relation), "%.*s", (int)(sizeof(kept->relation) - 1u), held->relation);
    kept->signed_read = held->signed_read;
    kept->runs = 0u;
    // each run's operand is its order among the printed ones, the runs of one place sharing the first's; a predicate
    // run is the guard, printed before the operation and no operand, and a flag prints none, each kept under the
    // operand count as its mark
    unsigned int printed = 0u;
    unsigned int operand_of[SASS_MACHINE_RUNS];
    for (unsigned int run = 0u; (run < held->runs) && (kept->runs < SASS_MACHINE_RUNS); run += 1u)
    {
        const int guard = (held->kind[run] == KHW_KIND_PREDICATE) || khw_flag(held, run);
        unsigned int operand = guard ? SASS_MACHINE_OPERANDS : printed;
        for (unsigned int before = 0u; before < run; before += 1u)
        {
            operand = ((held->place[run] != SASS_RUN_NO_PLACE) && (held->place[before] == held->place[run]))
                          ? operand_of[before]
                          : operand;
        }
        operand_of[run] = operand;
        kept->run[kept->runs].operand = operand;
        kept->run[kept->runs].first = held->first[run];
        kept->run[kept->runs].last = held->last[run];
        kept->run[kept->runs].place = held->place[run];
        kept->runs += 1u;
        printed += (guard || khw_place_printed(held, run)) ? 0u : 1u;
    }
    return 1;
}
