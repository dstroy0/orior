// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// written.c: what the pass writes: the run's texts, the runs read, the answers and the working set
#include "../../../../src/platform/platform.h"
#include "../../../parsers/who.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "khw_write.h"
#include "memoria_operor/memoria_operor.h"
#include "verba_scribo/verba_scribo.h"

// the order of the texts `left` and `right`, each read to its terminator or to `cap`: below 0 where `left`
// comes first, 0 where they are the same, above 0 where `right` does
static inline int written_text_order(const char *left, const char *right, size_t cap)
{
    const size_t left_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = left, .cap = cap);
    const size_t right_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = right, .cap = cap);
    const size_t shorter = (left_length < right_length) ? left_length : right_length;
    const size_t reach = (shorter < cap) ? (shorter + 1u) : cap;
    const size_t differs =
        EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = left, .cap = reach, .other = right, .ci = EMBED_FALSE);
    if (differs >= reach)
    {
        return 0;
    }
    // a byte of a text read as unsigned, and its order as a signed count
    return (int)(unsigned char)left[differs] - (int)(unsigned char)right[differs];
}

// the text that a writer here puts together: its bytes, the room that they have, and the offset of the write. The
// offset is the room once a write did not fit
static char *s_text_out;
static size_t s_text_cap;
static size_t s_text_at;

// the `room` bytes at `out` taken as the text, empty
static void text_begun(char *out, size_t room)
{
    s_text_out = out;
    s_text_cap = room;
    s_text_at = 0u;
}

// `text` written after what the text contains
static void text_put(const char *text)
{
    s_text_at =
        verba_textus.put(&(VerbaTextusCfg){.out = s_text_out, .cap = s_text_cap, .at = s_text_at, .text = text});
}

// `character` written after what the text contains
static void text_character(char character)
{
    s_text_at =
        verba_littera.ch(&(VerbaLitteraCfg){.out = s_text_out, .cap = s_text_cap, .at = s_text_at, .ch = character});
}

// `value` written in base ten after what the text contains
static void text_decimal(unsigned long long value)
{
    s_text_at =
        verba_numerus.u64(&(VerbaNumerusCfg){.out = s_text_out, .cap = s_text_cap, .at = s_text_at, .val = value});
}

// `value` written in base ten with its sign after what the text contains
static void text_signed(long long value)
{
    s_text_at =
        verba_numerus.i64(&(VerbaNumerusCfg){.out = s_text_out, .cap = s_text_cap, .at = s_text_at, .sval = value});
}

// `value` written in sixteen hex digits after what the text contains
static void text_hex(unsigned long long value)
{
    s_text_at = verba_numerus.hex(
        &(VerbaNumerusCfg){.out = s_text_out, .cap = s_text_cap, .at = s_text_at, .val = value, .min = 16u});
}

// the text passed to the run as the record `name`. 1, or 0 with the reason printed
static int khw_text_passed(const char *name)
{
    const int passed = verba_finis.ok(&(VerbaFinisCfg){.cap = s_text_cap, .at = s_text_at}) &&
                       (qry_member_append(name, s_text_out, s_text_at, QRY_ORDINARY) != 0);
    if (!passed)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  khw_write: ");
        mmgr_line_put(output_line, name);
        mmgr_line_put(output_line, " was not passed to the run's buffer");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return passed;
}

// run `run` of `form`, written after what the text holds, as its first bit, its last and its place. The character of
// its type comes after the place, or KHW_PLACE_NONE_TEXT where it has none
static void khw_run_printed(const KhwAnswered *answered, unsigned int run)
{
    const unsigned int type = (answered->type[run] <= KHW_TYPE_CONSTANT) ? answered->type[run] : KHW_TYPE_REGISTER;
    text_character(' ');
    text_decimal(answered->first[run]);
    text_character(' ');
    text_decimal(answered->last[run]);
    text_character(' ');
    if (answered->place[run] == KHW_PLACE_NONE)
    {
        // a run with no place carries its type's character after the none mark where it is not a register's
        text_put(KHW_PLACE_NONE_TEXT);
        if (type != KHW_TYPE_REGISTER)
        {
            text_character(KHW_TYPE_CHARACTERS[type]);
        }
        return;
    }
    text_decimal(answered->place[run]);
    text_character(KHW_TYPE_CHARACTERS[type]);
}

// the encoding `low`, `high` written after what the text contains as two words of sixteen hex digits, each after a
// space
static void khw_encoding_printed(unsigned long long low, unsigned long long high)
{
    text_character(' ');
    text_hex(low);
    text_character(' ');
    text_hex(high);
}

// the runs of `form` read from `text`, `form->runs` of them:
//   - three words a run: its first bit, its last, and its place with the character of its type after it, a register
//     where none.
//   - or two words, its first bit and its last, where the line holds no place. Each run then holds none.
//
// The result is 1, or 0 where the words are neither
int khw_runs_read(KhwAnswered *answered, const char *text)
{
    unsigned int words = 0u;
    for (const char *at = text; *at != '\0';)
    {
        at +=
            EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = at, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = " \t\r\n");
        const size_t length = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = at,
                                         .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = " \t\r\n");
        words += (length != 0u) ? 1u : 0u;
        at += length;
    }
    const unsigned int each = (words == (3u * answered->runs)) ? 3u : ((words == (2u * answered->runs)) ? 2u : 0u);
    if (each == 0u)
    {
        return 0;
    }
    const char *rest = text;
    for (unsigned int run = 0u; run < answered->runs; run += 1u)
    {
        char place[16];
        answered->place[run] = KHW_PLACE_NONE;
        if (each == 2u)
        {
            if (!line_unsigned(&rest, &answered->first[run]) || !line_unsigned(&rest, &answered->last[run]))
            {
                return 0;
            }
        }
        else
        {
            if (!line_unsigned(&rest, &answered->first[run]) || !line_unsigned(&rest, &answered->last[run]) ||
                !line_word(&rest, place, sizeof(place)))
            {
                return 0;
            }
            // the none mark, with its type's character after it where the run is no register's, or the place with its
            // character after it. The character's place among the four is its type, and no character is a register's
            const char *end = NULL;
            const int none =
                (written_text_order(place, KHW_PLACE_NONE_TEXT,
                                    (size_t)(EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = KHW_PLACE_NONE_TEXT,
                                                        .cap = PLATFORM_TEXT_TO_TERMINATOR))) == 0);
            answered->place[run] = none ? KHW_PLACE_NONE
                                        : (unsigned int)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = place,
                                                                   .end = &end, .base = 10);
            end = none ? (place + EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = KHW_PLACE_NONE_TEXT,
                                             .cap = PLATFORM_TEXT_TO_TERMINATOR))
                       : end;
            const char *const character = ((end != NULL) && (*end != '\0'))
                                              ? EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = KHW_TYPE_CHARACTERS,
                                                           .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = *end)
                                              : NULL;
            answered->type[run] =
                (character != NULL) ? (unsigned int)(character - KHW_TYPE_CHARACTERS) : KHW_TYPE_REGISTER;
        }
    }
    return 1;
}

// the relation that `form` answered, as the answers and the working set write it, into `written` of `room` bytes. It
// is `ladder.<anchor>` or `frame.<relation>`. Where `with_reading` is not 0, the read of the frame comes after a dot.
// The text
const char *khw_relation_named(const KhwAnswered *answered, int with_reading, char *written, size_t room)
{
    if (answered->frame == (unsigned int)FRAME_RELATION_COUNT)
    {
        VerbaLinea buffer_line = mmgr_line_over(written, room);
        mmgr_line_put(&buffer_line, KHW_READS_LADDER);
        mmgr_line_put(&buffer_line, s_anchor_text[answered->anchor]);
        (void)mmgr_line_finish(&buffer_line);
    }
    else if (with_reading != 0)
    {
        VerbaLinea buffer_line = mmgr_line_over(written, room);
        mmgr_line_put(&buffer_line, FRAME_READS);
        mmgr_line_put(&buffer_line, s_frame_text[answered->frame]);
        mmgr_line_put(&buffer_line, ".");
        mmgr_line_hex(&buffer_line, answered->reading, 1u);
        (void)mmgr_line_finish(&buffer_line);
    }
    else
    {
        VerbaLinea buffer_line = mmgr_line_over(written, room);
        mmgr_line_put(&buffer_line, FRAME_READS);
        mmgr_line_put(&buffer_line, s_frame_text[answered->frame]);
        (void)mmgr_line_finish(&buffer_line);
    }
    return written;
}

// the forms that the part answered for, passed to the run as KHW_FORMS_NAME, one a line. The writer of the vendor
// names and arranges them. Each line holds:
//   - the relation that the gate read.
//   - whether the answers read a word as signed, and whether the part got the query.
//   - the encoding.
//   - the first and last bit of each run, and its place in the tuple of the relation.
//
// A form not yet classified, or one with no operand run, names nothing, and the vendor arranges none of them. The
// result is 1, or 0 with the reason printed
int khw_answers_written(void)
{
    static char s_forms_bytes[QRY_WRITER_RING_BYTES / 2ULL];
    text_begun(s_forms_bytes, sizeof(s_forms_bytes));
    for (unsigned int at = 0u; at < s_names; at += 1u)
    {
        const KhwAnswered *const answered = &s_named[at];
        if ((answered->phase != KHW_PHASE_CLASSIFIED) || (answered->runs == 0u))
        {
            continue;
        }
        char relation[64];
        text_put(khw_relation_named(answered, 0, relation, sizeof(relation)));
        text_character(' ');
        text_signed(answered->signed_read);
        text_character(' ');
        text_signed(answered->signedness_queried);
        khw_encoding_printed(answered->low, answered->high);
        text_character(' ');
        text_decimal(answered->runs);
        for (unsigned int run = 0u; run < answered->runs; run += 1u)
        {
            khw_run_printed(answered, run);
        }
        text_character('\n');
    }
    return khw_text_passed(KHW_FORMS_NAME);
}

// The working set, passed to the run as KHW_WORK_NAME. Each form of it is a line, and khw_forms_read reads it back the
// next pass. The line holds its phase, the read order of its words, the relation, the signedness and the encoding.
// It also holds its runs with their places.
//
// A discovered form carries no run. A turned form carries the candidates that its register-runs sort, each with no
// place. A classified form carries the kept operands.
//
// It is the state that the passes of the split hand each other, kept apart from what the vendor arranges. The result
// is 1, or 0 with the reason printed
int khw_forms_work_written(void)
{
    static char s_work_bytes[QRY_WRITER_RING_BYTES / 2ULL];
    text_begun(s_work_bytes, sizeof(s_work_bytes));
    for (unsigned int at = 0u; at < s_names; at += 1u)
    {
        const KhwAnswered *const answered = &s_named[at];
        char relation[64];
        text_decimal(answered->phase);
        text_character(' ');
        text_signed(answered->swapped);
        text_character(' ');
        text_put(khw_relation_named(answered, 1, relation, sizeof(relation)));
        text_character(' ');
        text_signed(answered->signed_read);
        text_character(' ');
        text_signed(answered->signedness_queried);
        khw_encoding_printed(answered->low, answered->high);
        text_character(' ');
        text_decimal(answered->runs);
        for (unsigned int run = 0u; run < answered->runs; run += 1u)
        {
            khw_run_printed(answered, run);
        }
        text_character('\n');
    }
    return khw_text_passed(KHW_WORK_NAME);
}
