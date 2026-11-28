// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// questions.cu: the values a case is made of, a run's blobs read as text, and the links and questions the stick's
// manifest lists
#include "../../../../../../../includes/platform/platform.h"
#include "identity_internal.h"

// the operations that transfer control or wait, which cubin_safe.h's rule 2 blocks from the part. No question that
// holds one goes to the part as written
static const char *const s_control_or_wait[] = {
    "BRA",      "BRX",   "JMP", "JMX",    "CALL",      "RET", "EXIT", "BSSY", "BSYNC",  "BREAK",  "BMOV",
    "WARPSYNC", "YIELD", "BAR", "DEPBAR", "NANOSLEEP", "BPT", "RTT",  "KILL", "RPCMOV", "RETIRE", "PMTRIG"};

// the `size` bytes at `bytes` copied into a cell of the temporary tier, terminated; NULL where the pool cannot meet it
static char *run_bytes_copied(const unsigned char *bytes, unsigned long long size, size_t *copied)
{
    char *const cell =
        (char *)platform_cell_taken(size + 1ULL, platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if (cell == NULL)
    {
        *copied = 0u;
        return NULL;
    }
    // a blob is at max the buffer's bytes, which a size_t counts
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = cell, .src = bytes, .bytes = (size_t)size);
    cell[size] = '\0';
    *copied = (size_t)size;
    return cell;
}

char *run_blob_read(const char *name, size_t *size)
{
    const unsigned char *bytes = NULL;
    unsigned long long blob_size = 0ULL;
    if (!qry_member_blob(name, &bytes, &blob_size, NULL))
    {
        *size = 0u;
        return NULL;
    }
    return run_bytes_copied(bytes, blob_size, size);
}

char *run_file_read(const char *path, size_t *size)
{
    const unsigned char *bytes = NULL;
    unsigned long long file_size = 0ULL;
    if (!qry_member_file_blob(path, &bytes, &file_size))
    {
        *size = 0u;
        return NULL;
    }
    return run_bytes_copied(bytes, file_size, size);
}

int run_closed(RunWritten *written)
{
    QryShared *const run = qry_member_run();
    const char *const path = written->path.text;
    if (run == NULL)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity: no query writer runs to write ");
        mmgr_line_put(output_line, path);
        mmgr_line_put(output_line, ": ");
        mmgr_line_put(output_line, QRY_ENVIRONMENT);
        mmgr_line_put(output_line, " names no .qry a writer has open");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
        return 0;
    }
    const unsigned long long size = platform_text_size(&written->text);
    const char *const bytes = platform_text_bytes(&written->text);
    if (bytes == NULL)
    {
        return 0;
    }
    if (written->filed)
    {
        return qry_member_filed(path, bytes, size) != 0;
    }
    const char *const slash =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '/');
    const char *const back =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\\');
    const char *const last = ((back != NULL) && ((slash == NULL) || (back > slash))) ? back : slash;
    return qry_member_append((last != NULL) ? (last + 1) : path, bytes, size, QRY_ORDINARY) != 0;
}

int rule_broken(const char *link)
{
    const CarrierPiece text = carrier_piece_of(link);
    size_t guarded = 0u;
    if (link[0] == '@')
    {
        const char *const space =
            EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = link, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ' ');
        // the guard ends at its space, which lies past the link's start. A guard with no space after it leaves the
        // operation at the link's start
        guarded = (space != NULL) ? ((size_t)(space - link) + 1u) : 0u;
    }
    const size_t end = (guarded < text.length)
                           ? guarded + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &link[guarded],
                                                  .cap = text.length - guarded, .other = ". ")
                           : guarded;
    const CarrierPiece operation = {&link[guarded], end - guarded};
    if (identity_piece_is(operation, "EXIT"))
    {
        return 0;
    }
    for (size_t at = 0u; at < (sizeof(s_control_or_wait) / sizeof(s_control_or_wait[0])); at += 1u)
    {
        if (identity_piece_is(operation, s_control_or_wait[at]))
        {
            return 1;
        }
    }
    return 0;
}

// the `length` bytes of `text` with each hexadecimal number written in decimal, onto `out`
static void numbers_decimal(const char *text, size_t length, PlatformText *out)
{
    static TablePatternOnce s_hex;
    TableWalk walk;
    table_walk_begun(&walk, table_once(&s_hex, "0x([0-9a-fA-F]+)", 0), text, length);
    size_t at = 0u;
    while (table_walk_next(&walk))
    {
        const RegexpSpan whole = walk.match.group[0];
        (void)mmgr_line_put_n(&out->line, &text[at], (size_t)whole.from - at);
        size_t digits_length = 0u;
        const char *const digits = table_walk_group(&walk, 1u, &digits_length);
        char number[32] = {0};
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = number, .src = digits,
                   .bytes = (digits_length < 31u) ? digits_length : 31u);
        mmgr_line_decimal(&out->line,
                          EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = number, .end = NULL, .base = 16), 1u);
        at = (size_t)whole.to;
    }
    table_walk_ended(&walk);
    (void)mmgr_line_put_n(&out->line, &text[at], length - at);
}

// an operand with its registers read as their type and its numbers in decimal, onto `out`
static void operand_type(CarrierPiece operand, PlatformText *out)
{
    static TablePatternOnce s_reuse;
    static TablePatternOnce s_uniform;
    static TablePatternOnce s_register;
    static TablePatternOnce s_predicate;
    PlatformText text;
    identity_text_begun(&text);
    (void)mmgr_line_put_n(&text.line, operand.text, operand.length);
    identity_replaced(&text, table_once(&s_reuse, "\\.reuse", 0), "");
    identity_replaced(&text, table_once(&s_uniform, "\\bUR[0-9]+\\b", 0), "UR");
    identity_replaced(&text, table_once(&s_register, "\\bR[0-9]+\\b", 0), "R");
    identity_replaced(&text, table_once(&s_predicate, "(^|[^A-Z_])P[0-6]\\b", 0), "$1P");
    const unsigned long long size = platform_text_size(&text);
    numbers_decimal(platform_text_bytes(&text), (size_t)size, out);
    platform_text_ended(&text);
}

// the pattern of a register an instruction names
static const TablePattern *named_pattern(void)
{
    static TablePatternOnce s_named;
    return table_once(&s_named, "\\b(UR[0-9]+|R[0-9]+|P[0-6])\\b", 0);
}

// 1 where a part of the link the reader reads now passed the room of its record
static thread_local int t_link_full = 0;

// the registers an instruction names, in order, each with a space after it, into `registers`
static void registers_named(CarrierPiece text, IdentityLine *registers)
{
    PlatformText named;
    identity_text_begun(&named);
    TableWalk walk;
    table_walk_begun(&walk, named_pattern(), text.text, text.length);
    while (table_walk_next(&walk))
    {
        size_t length = 0u;
        const char *const each = table_walk_group(&walk, 0u, &length);
        (void)mmgr_line_put_n(&named.line, each, length);
        mmgr_line_ch(&named.line, ' ');
    }
    table_walk_ended(&walk);
    t_link_full |= !IDENTITY_KEPT(&named, *registers);
}

// each register `text` names, put down in the link's placeholders under the terminated `placeholder`
static void placeholders_named(CarrierPiece text, const char *placeholder, Link *link)
{
    TableWalk walk;
    table_walk_begun(&walk, named_pattern(), text.text, text.length);
    while (table_walk_next(&walk))
    {
        size_t length = 0u;
        const char *const each = table_walk_group(&walk, 0u, &length);
        if (link->placeholder_count >= LINK_PARTS_MAX)
        {
            t_link_full = 1;
            continue;
        }
        LinkPlaceholder *const named = &link->placeholders[link->placeholder_count];
        t_link_full |= !identity_name_set(named->placeholder, sizeof(named->placeholder), placeholder,
                                          PLATFORM_TEXT_TO_TERMINATOR);
        t_link_full |= !identity_name_set(named->name, sizeof(named->name), each, length);
        link->placeholder_count += 1u;
    }
    table_walk_ended(&walk);
}

// the `length` bytes of `text` put after the `*count` words of `words`, which hold `most`: 1, or 0 where they are full
// or the word does not fit
static int link_word_put(LinkWord *words, unsigned int *count, unsigned int most, const char *text, size_t length)
{
    if (*count >= most)
    {
        return 0;
    }
    const int set = IDENTITY_SET(words[*count], text, length);
    *count += 1u;
    return set;
}

CarrierPiece trimmed(CarrierPiece text)
{
    const size_t first =
        EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = text.text, .cap = text.length, .other = CARRIER_SPACES);
    if (first == text.length)
    {
        const CarrierPiece empty = {text.text, 0u};
        return empty;
    }
    size_t last = text.length;
    while ((last > first) && (carrier_space(text.text[last - 1u]) || (text.text[last - 1u] == ';')))
    {
        last -= 1u;
    }
    // where every byte from the first kept is a space or a semicolon, the rest stays whole
    if (last == first)
    {
        last = text.length;
    }
    const CarrierPiece kept = {&text.text[first], last - first};
    return kept;
}

// a number read in hex from the bytes of `piece`
static unsigned long long piece_hex(CarrierPiece piece)
{
    char number[32] = {0};
    const size_t length = (piece.length < 31u) ? piece.length : 31u;
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = number, .src = piece.text, .bytes = length);
    const int prefixed = (number[0] == '0') && ((number[1] == 'x') || (number[1] == 'X'));
    return EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = prefixed ? &number[2] : number, .end = NULL,
                      .base = 16);
}

int link_read(CarrierPiece line, Link *link)
{
    t_link_full = 0;
    static TablePatternOnce s_address;
    static TablePatternOnce s_comment;
    static TablePatternOnce s_instruction;
    static TablePatternOnce s_self;
    TableMatch address;
    const int addressed =
        table_search(table_once(&s_address, "/\\*([0-9a-f]{4,})\\*/", 0), line.text, line.length, &address);
    size_t address_length = 0u;
    const char *const address_text = addressed ? table_group(&address, 1u, line.text, &address_length) : "";
    const CarrierPiece address_piece = {address_text, address_length};
    PlatformText uncommented;
    identity_text_begun(&uncommented);
    table_replaced(table_once(&s_comment, "/\\*.*?\\*/", 0), line.text, line.length, "", &uncommented);
    const unsigned long long uncommented_size = platform_text_size(&uncommented);
    const CarrierPiece bare = trimmed(carrier_piece_of(platform_text_bytes(&uncommented)));
    TableMatch matched;
    if ((bare.length == 0u) ||
        !table_matched(table_once(&s_instruction, "^(@!?[A-Z0-9]+\\s+)?[A-Z][A-Z0-9_.]*(\\s.*)?$", 1), bare.text,
                       bare.length, &matched))
    {
        platform_text_ended(&uncommented);
        return 0;
    }
    (void)uncommented_size;
    TableMatch self;
    if (addressed && table_matched(table_once(&s_self, "^BRA\\s+0x([0-9a-f]+)$", 1), bare.text, bare.length, &self))
    {
        size_t target_length = 0u;
        const char *const target = table_group(&self, 1u, bare.text, &target_length);
        const CarrierPiece target_piece = {target, target_length};
        if (piece_hex(target_piece) == piece_hex(address_piece))
        {
            platform_text_ended(&uncommented);
            return -1;
        }
    }
    // the written text kept in the link, which the pieces below read through `written`
    EMBED_CALL(memor.set, MemoriaCfg, .dst = link, .bytes = sizeof(*link), .val = 0);
    t_link_full |= !IDENTITY_SET(link->exact, bare.text, bare.length);
    if (addressed)
    {
        t_link_full |= !IDENTITY_SET(link->position, address_text, address_length);
    }
    const unsigned long long address_value = addressed ? piece_hex(address_piece) : 0ULL;
    platform_text_ended(&uncommented);
    PlatformText type;
    identity_text_begun(&type);
    const CarrierPiece written = carrier_piece_of(link->exact.text);
    CarrierPiece text = written;
    if (text.text[0] == '@')
    {
        const size_t space = carrier_space_at(text);
        const CarrierPiece guard = {text.text, space};
        operand_type(guard, &type);
        mmgr_line_ch(&type.line, ' ');
        placeholders_named(guard, "g", link);
        text = trimmed(carrier_after(text, space));
    }
    const size_t space = carrier_space_at(text);
    (void)mmgr_line_put_n(&type.line, text.text, space);
    if (space != text.length)
    {
        mmgr_line_ch(&type.line, ' ');
        // the operands joined, a comma and a space before each once the joined text holds anything
        const unsigned long long joined_from = platform_text_size(&type);
        size_t cut = 0u;
        CarrierPiece piece;
        unsigned int placeholder = 0u;
        const CarrierPiece operands = carrier_after(text, space);
        while (carrier_piece_next(operands, ',', &cut, &piece))
        {
            if (platform_text_size(&type) > joined_from)
            {
                mmgr_line_put(&type.line, ", ");
            }
            operand_type(trimmed(piece), &type);
            char number[LINK_PLACEHOLDER_BYTES];
            VerbaLinea number_line = mmgr_line_over(number, sizeof(number));
            mmgr_line_decimal(&number_line, placeholder, 1u);
            (void)mmgr_line_finish(&number_line);
            placeholders_named(piece, number, link);
            placeholder += 1u;
        }
    }
    // The link with each register a mark and each literal operand another, the registers and the literals in the order
    // written. Its spaces go as one, and its numbers in decimal. The target of an operation that aims at an address of
    // the code is a position in the chain. It goes as its distance in links from the link itself. A barrier's number, a
    // mask or a count is a literal
    static TablePatternOnce s_reuse;
    static TablePatternOnce s_spaces;
    static TablePatternOnce s_literal;
    static TablePatternOnce s_target;
    PlatformText plain_text;
    identity_text_begun(&plain_text);
    (void)mmgr_line_put_n(&plain_text.line, written.text, written.length);
    identity_replaced(&plain_text, table_once(&s_reuse, "\\.reuse", 0), "");
    identity_replaced(&plain_text, table_once(&s_spaces, "\\s+", 0), " ");
    const CarrierPiece plain = carrier_piece_of(platform_text_bytes(&plain_text));
    size_t operation_first = 0u;
    if (plain.text[0] == '@')
    {
        const char *const at_space =
            EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = plain.text, .cap = plain.length, .byte = ' ');
        // the guard ends at its space, which lies past the text's start
        operation_first = (at_space != NULL) ? ((size_t)(at_space - plain.text) + 1u) : plain.length;
    }
    // the operation up to its space, and its name up to its point or its space
    const size_t left = (operation_first < plain.length) ? (plain.length - operation_first) : 0u;
    const size_t operation_last =
        operation_first + ((left != 0u) ? EMBED_CALL(cellul.span_not, CatenaFinitaCfg,
                                                     .src = &plain.text[operation_first], .cap = left, .other = " ")
                                        : 0u);
    const size_t name_end =
        operation_first + ((left != 0u) ? EMBED_CALL(cellul.span_not, CatenaFinitaCfg,
                                                     .src = &plain.text[operation_first], .cap = left, .other = ". ")
                                        : 0u);
    const CarrierPiece operation_name = {&plain.text[operation_first], name_end - operation_first};
    static const char *const s_aimed[] = {"BRA", "JMP", "CALL", "BSSY"};
    int transfers = 0;
    for (size_t at = 0u; at < (sizeof(s_aimed) / sizeof(s_aimed[0])); at += 1u)
    {
        transfers |= identity_piece_is(operation_name, s_aimed[at]) ? 1 : 0;
    }
    PlatformText shaped;
    identity_text_begun(&shaped);
    (void)mmgr_line_put_n(&shaped.line, plain.text, operation_last);
    if (operation_last < plain.length)
    {
        size_t cut = 0u;
        CarrierPiece piece;
        unsigned int at = 0u;
        const CarrierPiece operands = carrier_after(plain, operation_last + 1u);
        while (carrier_piece_next(operands, ',', &cut, &piece))
        {
            const CarrierPiece each = trimmed(piece);
            mmgr_line_put(&shaped.line, (at == 0u) ? " " : ", ");
            TableMatch shape;
            if (transfers && addressed &&
                table_matched(table_once(&s_target, "^0x[0-9a-f]+$", 1), each.text, each.length, &shape))
            {
                const long long distance = ((long long)piece_hex(each) - (long long)address_value) / 16;
                t_link_full |=
                    !link_word_put(link->targets, &link->target_count, LINK_LITERALS_MAX, each.text, each.length);
                char distance_text[32];
                VerbaLinea distance_line = mmgr_line_over(distance_text, sizeof(distance_text));
                mmgr_line_put(&distance_line, (distance < 0) ? "." : ".+");
                mmgr_line_decimal_signed(&distance_line, distance);
                const size_t distance_length = mmgr_line_finish(&distance_line);
                t_link_full |= !link_word_put(link->distances, &link->distance_count, LINK_LITERALS_MAX, distance_text,
                                              distance_length);
                mmgr_line_ch(&shaped.line, '\x03');
            }
            else if (table_matched(table_once(&s_literal,
                                              "^-?(0x[0-9a-f]+|[0-9]+(\\.[0-9]+)?(e[+-][0-9]+)?|[+-]?INF|[+-]?QNAN)$",
                                              1),
                                   each.text, each.length, &shape))
            {
                PlatformText literal;
                identity_text_begun(&literal);
                numbers_decimal(each.text, each.length, &literal);
                if (link->literal_count < LINK_LITERALS_MAX)
                {
                    t_link_full |= !IDENTITY_KEPT(&literal, link->literals[link->literal_count]);
                    link->literal_count += 1u;
                }
                else
                {
                    platform_text_ended(&literal);
                    t_link_full = 1;
                }
                mmgr_line_ch(&shaped.line, '\x02');
            }
            else
            {
                (void)mmgr_line_put_n(&shaped.line, each.text, each.length);
            }
            at += 1u;
        }
    }
    platform_text_ended(&plain_text);
    const unsigned long long shaped_size = platform_text_size(&shaped);
    const char *const shaped_bytes = platform_text_bytes(&shaped);
    PlatformText marked;
    identity_text_begun(&marked);
    TableWalk walk;
    table_walk_begun(&walk, named_pattern(), shaped_bytes, (size_t)shaped_size);
    size_t at = 0u;
    while (table_walk_next(&walk))
    {
        const RegexpSpan whole = walk.match.group[0];
        (void)mmgr_line_put_n(&marked.line, &shaped_bytes[at], (size_t)whole.from - at);
        mmgr_line_ch(&marked.line, '\x01');
        size_t length = 0u;
        const char *const each = table_walk_group(&walk, 0u, &length);
        if (link->named_count < LINK_PARTS_MAX)
        {
            t_link_full |= !IDENTITY_SET(link->named[link->named_count], each, length);
            link->named_count += 1u;
        }
        else
        {
            t_link_full = 1;
        }
        at = (size_t)whole.to;
    }
    table_walk_ended(&walk);
    (void)mmgr_line_put_n(&marked.line, &shaped_bytes[at], (size_t)shaped_size - at);
    PlatformText marked_decimal;
    identity_text_begun(&marked_decimal);
    const unsigned long long marked_size = platform_text_size(&marked);
    numbers_decimal(platform_text_bytes(&marked), (size_t)marked_size, &marked_decimal);
    platform_text_ended(&marked);
    platform_text_ended(&shaped);
    t_link_full |= !IDENTITY_KEPT(&marked_decimal, link->marked);
    registers_named(written, &link->registers);
    t_link_full |= !IDENTITY_KEPT(&type, link->type);
    if (t_link_full)
    {
        // a link with more parts or longer texts than its record holds reads as no link, and the read says so
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  a link holds more than the bounds of its record in identity_internal.h: ");
        mmgr_line_put(output_line, link->exact.text);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
        return 0;
    }
    return 1;
}

// one row of the stick's manifest: its number, its category and its C
struct QuestionNamed
{
    IdentityName number;
    IdentityName category;
    char text[IDENTITY_C_BYTES];
};

// The rows of the manifest whose number is 4 bytes, into one cell of the temporary tier that the caller gives back,
// sorted by number. A number the manifest gives twice keeps each row in the order written. NULL where the file does
// not read, holds no such row, or the pool cannot meet the cell
static QuestionNamed *manifest_read(const char *manifest, unsigned long long *count)
{
    *count = 0ULL;
    size_t manifest_size = 0u;
    char *const table = identity_file_read(manifest, &manifest_size);
    const CarrierPiece rows = {(table != NULL) ? table : "", manifest_size};
    QuestionNamed *named = NULL;
    // two walks: the first counts the rows, and the second fills one cell taken once at that count
    for (unsigned int walk = 0u; walk < 2u; walk += 1u)
    {
        if (walk == 1u)
        {
            named = (*count != 0ULL) ? (QuestionNamed *)platform_cell_cleared(*count, sizeof(QuestionNamed),
                                                                              platform_place_g_program_owner,
                                                                              PLATFORM_TIER_TEMPORARY)
                                     : NULL;
            if (named == NULL)
            {
                break;
            }
            *count = 0ULL;
        }
        size_t cut = 0u;
        CarrierPiece row;
        while (identity_line_next(rows, &cut, &row))
        {
            const char *const first =
                EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = row.text, .cap = row.length, .byte = '\t');
            // the first tab lies at or past the row's start
            const size_t first_at = (first != NULL) ? (size_t)(first - row.text) : row.length;
            const char *const second = (first_at < row.length)
                                           ? EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = &row.text[first_at + 1u],
                                                        .cap = row.length - first_at - 1u, .byte = '\t')
                                           : NULL;
            if ((first_at != 4u) || (second == NULL))
            {
                continue;
            }
            if (walk == 1u)
            {
                // the second tab lies past the first
                const size_t second_at = (size_t)(second - row.text);
                QuestionNamed *const entry = &named[*count];
                const CarrierPiece text = trimmed(carrier_after(row, second_at + 1u));
                (void)IDENTITY_SET(entry->number, row.text, 4u);
                (void)IDENTITY_SET(entry->category, &row.text[5], second_at - 5u);
                (void)identity_name_set(entry->text, sizeof(entry->text), text.text, text.length);
            }
            *count += 1ULL;
        }
    }
    platform_cell_released(table, platform_place_g_program_owner);
    if ((named != NULL) && !identity_sorted(named, *count, [](const QuestionNamed &one, const QuestionNamed &other) {
            return identity_order_text(one.number.text, other.number.text) < 0;
        }))
    {
        platform_cell_released(named, platform_place_g_program_owner);
        named = NULL;
    }
    *count = (named != NULL) ? *count : 0ULL;
    return named;
}

// the manifest's row of `number`, the last it gives where it gives the number twice, or NULL where it gives none
static const QuestionNamed *manifest_row(const QuestionNamed *named, unsigned long long count, const char *number)
{
    const unsigned long long after = identity_lower(
        named, count, [number](const QuestionNamed &row) { return identity_order_text(row.number.text, number) <= 0; });
    return ((after != 0ULL) && identity_same(named[after - 1ULL].number.text, number)) ? &named[after - 1ULL] : NULL;
}

// the files that did not read, said
static void questions_unread(const char *listing, const char *manifest)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  ");
    mmgr_line_put(output_line, listing);
    mmgr_line_put(output_line, " or ");
    mmgr_line_put(output_line, manifest);
    mmgr_line_put(output_line, " could not be read");
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

// The places of the question's chain ordered by exact text, into its `by_exact`, a run of one text by place. 1, or 0
// where the pool cannot meet the sort
int question_ordered(Question *question)
{
    for (unsigned long long at = 0ULL; at < question->links; at += 1ULL)
    {
        question->by_exact[at] = at;
    }
    const Link *const chain = question->chain;
    return identity_sorted(question->by_exact, question->links,
                           [chain](unsigned long long one, unsigned long long other) {
                               return identity_order_text(chain[one].exact.text, chain[other].exact.text) < 0;
                           });
}

int questions_read(const char *listing, const char *manifest, Questions *questions)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = questions, .bytes = sizeof(*questions), .val = 0);
    unsigned long long named_count = 0ULL;
    QuestionNamed *const named = manifest_read(manifest, &named_count);
    size_t listing_size = 0u;
    char *const file = identity_file_read(listing, &listing_size);
    if ((file == NULL) || (named == NULL))
    {
        platform_cell_released(file, platform_place_g_program_owner);
        platform_cell_released(named, platform_place_g_program_owner);
        questions_unread(listing, manifest);
        return 0;
    }
    static TablePatternOnce s_function;
    const TablePattern *const function_pattern = table_once(&s_function, "Function : measuring_stick_([0-9]+)", 0);
    const CarrierPiece listing_text = {file, listing_size};
    // Two walks of the listing. The first counts the questions and the links. The second fills the three cells of the
    // questions, each taken once at its count and never grown
    unsigned long long counts[2] = {0ULL, 0ULL};
    int whole = 1;
    for (unsigned int walk = 0u; whole && (walk < 2u); walk += 1u)
    {
        if (walk == 1u)
        {
            questions->question =
                (Question *)platform_cell_cleared((counts[0] != 0ULL) ? counts[0] : 1ULL, sizeof(Question),
                                                  platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
            questions->links = (Link *)platform_cell_cleared((counts[1] != 0ULL) ? counts[1] : 1ULL, sizeof(Link),
                                                             platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
            questions->places = (unsigned long long *)platform_cell_cleared(
                (counts[1] != 0ULL) ? counts[1] : 1ULL, sizeof(unsigned long long), platform_place_g_program_owner,
                PLATFORM_TIER_PERSISTENT);
            whole = (questions->question != NULL) && (questions->links != NULL) && (questions->places != NULL);
            if (!whole)
            {
                break;
            }
        }
        int ended = 1;
        unsigned long long question_at = 0ULL;
        unsigned long long link_at = 0ULL;
        Question *question = NULL;
        size_t cut = 0u;
        CarrierPiece line;
        while (identity_line_next(listing_text, &cut, &line))
        {
            TableMatch function;
            if (table_search(function_pattern, line.text, line.length, &function))
            {
                if (walk == 1u)
                {
                    size_t number_length = 0u;
                    const char *const number = table_group(&function, 1u, line.text, &number_length);
                    question = &questions->question[question_at];
                    (void)IDENTITY_SET(question->number, number, number_length);
                    const QuestionNamed *const row = manifest_row(named, named_count, question->number.text);
                    if (row != NULL)
                    {
                        (void)IDENTITY_SET_TEXT(question->category, row->category.text);
                        (void)identity_name_set(question->text, sizeof(question->text), row->text,
                                                PLATFORM_TEXT_TO_TERMINATOR);
                    }
                    question->chain = &questions->links[link_at];
                    question->by_exact = &questions->places[link_at];
                }
                question_at += 1ULL;
                ended = 0;
                continue;
            }
            Link link;
            const int read = ended ? 0 : link_read(line, &link);
            ended = ended || (read < 0);
            if (read <= 0)
            {
                continue;
            }
            if (walk == 1u)
            {
                questions->links[link_at] = link;
                question->links += 1ULL;
            }
            link_at += 1ULL;
        }
        counts[0] = question_at;
        counts[1] = link_at;
    }
    questions->count = whole ? counts[0] : 0ULL;
    platform_cell_released(file, platform_place_g_program_owner);
    platform_cell_released(named, platform_place_g_program_owner);
    for (unsigned long long at = 0ULL; whole && (at < questions->count); at += 1ULL)
    {
        whole = question_ordered(&questions->question[at]);
    }
    // the chain runs question by question in the order of their numbers
    whole =
        whole && identity_sorted(questions->question, questions->count, [](const Question &one, const Question &other) {
            return identity_order_text(one.number.text, other.number.text) < 0;
        });
    if (!whole)
    {
        questions_ended(questions);
        identity_path_said(listing, ": the pool cannot meet its questions");
    }
    return whole;
}

// the bytes of one entry of a link's context, its terminator among them: two placeholders and the other link's type
#define CONTEXT_ENTRY_BYTES (IDENTITY_LINE_BYTES + 32u)

// the entries of one context at max: each placeholder of the link, each side, each placeholder of the other link
#define CONTEXT_ENTRIES_MAX (LINK_PARTS_MAX * 2u * LINK_PARTS_MAX)

struct ContextEntry
{
    char text[CONTEXT_ENTRY_BYTES];
};

void member_context(const Question *question, const char *position, PlatformText *out)
{
    unsigned long long at = 0ULL;
    const unsigned long long chain = question->links;
    while ((at < chain) && !identity_same(question->chain[at].position.text, position))
    {
        at += 1ULL;
    }
    if (at == chain)
    {
        return;
    }
    ContextEntry *const entries = (ContextEntry *)platform_cell_taken(
        CONTEXT_ENTRIES_MAX * sizeof(ContextEntry), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if (entries == NULL)
    {
        return;
    }
    unsigned int entry_count = 0u;
    const Link *const link = &question->chain[at];
    for (unsigned int each = 0u; each < link->placeholder_count; each += 1u)
    {
        const LinkPlaceholder *const placeholder = &link->placeholders[each];
        for (int side = 0; side < 2; side += 1)
        {
            const long step = (side == 0) ? -1 : 1;
            // a chain's links are far below 2^63
            for (long walk = (long)at + step; (walk >= 0) && (walk < (long)chain); walk += step)
            {
                const Link *const other = &question->chain[walk];
                int named = 0;
                for (unsigned int other_at = 0u; other_at < other->placeholder_count; other_at += 1u)
                {
                    const LinkPlaceholder *const other_placeholder = &other->placeholders[other_at];
                    if (identity_same(other_placeholder->name, placeholder->name) &&
                        (entry_count < CONTEXT_ENTRIES_MAX))
                    {
                        VerbaLinea entry = mmgr_line_over(entries[entry_count].text, CONTEXT_ENTRY_BYTES);
                        mmgr_line_put(&entry, (side == 0) ? "before " : "after ");
                        mmgr_line_put(&entry, placeholder->placeholder);
                        mmgr_line_ch(&entry, ' ');
                        mmgr_line_put(&entry, other_placeholder->placeholder);
                        mmgr_line_ch(&entry, ' ');
                        mmgr_line_put(&entry, other->type.text);
                        (void)mmgr_line_finish(&entry);
                        entry_count += 1u;
                        named = 1;
                    }
                }
                if (named)
                {
                    break;
                }
            }
        }
    }
    // the entries in the order of their bytes, each once
    identity_sorted_in_place(entries, entry_count, [](const ContextEntry &one, const ContextEntry &other) {
        return identity_order_text(one.text, other.text) < 0;
    });
    unsigned int written = 0u;
    for (unsigned int each = 0u; each < entry_count; each += 1u)
    {
        if ((each != 0u) && identity_same(entries[each].text, entries[each - 1u].text))
        {
            continue;
        }
        mmgr_line_put(&out->line, (written == 0u) ? "" : ";");
        mmgr_line_put(&out->line, entries[each].text);
        written += 1u;
    }
    platform_cell_released(entries, platform_place_g_program_owner);
}

void links_past(const Question *one, const Question *other, PlatformArray *past)
{
    for (unsigned long long at = 0ULL; at < one->links;)
    {
        const unsigned long long place = one->by_exact[at];
        const char *const exact = one->chain[place].exact.text;
        unsigned long long end = at;
        while ((end < one->links) && identity_same(one->chain[one->by_exact[end]].exact.text, exact))
        {
            end += 1ULL;
        }
        unsigned long long other_first = 0ULL;
        const unsigned long long there = question_exact_run(other, exact, &other_first);
        for (unsigned long long extra = there; extra < (end - at); extra += 1ULL)
        {
            (void)platform_array_appended(past, &place);
        }
        at = end;
    }
}

int question_integer(const Question *question)
{
    static const char *const s_categories[] = {"operator",  "compound", "unary",    "conversion",   "conditional",
                                               "statement", "test",     "pressure", "text_identity"};
    int integer = 0;
    for (size_t at = 0u; at < (sizeof(s_categories) / sizeof(s_categories[0])); at += 1u)
    {
        integer |= identity_same(question->category.text, s_categories[at]) ? 1 : 0;
    }
    const char *const text = question->text;
    return integer && !identity_has(text, "float") && !identity_has(text, "double") && !identity_has(text, "__");
}
