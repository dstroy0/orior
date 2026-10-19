// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// written.c: what the pass writes: the run's texts, the runs read, the answers and the working set
#include "../../../parsers/who.h"
#include "khw_write.h"
#include "verba_scribo/verba_scribo.h"

// the text a writer here puts together: its bytes, the room they have, and the offset written to, the room once a write
// did not fit
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

// `text` written after what the text holds
static void text_put(const char *text)
{
    s_text_at =
        verba_textus.put(&(VerbaTextusCfg){.out = s_text_out, .cap = s_text_cap, .at = s_text_at, .text = text});
}

// `character` written after what the text holds
static void text_character(char character)
{
    s_text_at =
        verba_littera.ch(&(VerbaLitteraCfg){.out = s_text_out, .cap = s_text_cap, .at = s_text_at, .ch = character});
}

// `value` written in base ten after what the text holds
static void text_decimal(unsigned long long value)
{
    s_text_at =
        verba_numerus.u64(&(VerbaNumerusCfg){.out = s_text_out, .cap = s_text_cap, .at = s_text_at, .val = value});
}

// `value` written in base ten with its sign after what the text holds
static void text_signed(long long value)
{
    s_text_at =
        verba_numerus.i64(&(VerbaNumerusCfg){.out = s_text_out, .cap = s_text_cap, .at = s_text_at, .sval = value});
}

// `value` written in sixteen hex digits after what the text holds
static void text_hex(unsigned long long value)
{
    s_text_at = verba_numerus.hex(
        &(VerbaNumerusCfg){.out = s_text_out, .cap = s_text_cap, .at = s_text_at, .val = value, .min = 16u});
}

// the text handed to the run as the record `name`. 1, or 0 with the reason printed
static int khw_text_handed(const char *name)
{
    const int handed = verba_finis.ok(&(VerbaFinisCfg){.cap = s_text_cap, .at = s_text_at}) &&
                       (qry_member_append(name, s_text_out, s_text_at, QRY_ORDINARY) != 0);
    if (!handed)
    {
        printf("  khw_write: %s was not handed to the run's buffer\n", name);
    }
    return handed;
}

// run `run` of `held` written after what the text holds as its first bit, its last and its place with its kind's
// letter after it, KHW_PLACE_NONE_TEXT where it has none
static void khw_run_printed(const KhwAnswered *held, unsigned int run)
{
    const unsigned int kind = (held->kind[run] <= KHW_KIND_CONSTANT) ? held->kind[run] : KHW_KIND_REGISTER;
    text_character(' ');
    text_decimal(held->first[run]);
    text_character(' ');
    text_decimal(held->last[run]);
    text_character(' ');
    if (held->place[run] == KHW_PLACE_NONE)
    {
        // a run with no place carries its kind's letter after the none mark where it is not a register's
        text_put(KHW_PLACE_NONE_TEXT);
        if (kind != KHW_KIND_REGISTER)
        {
            text_character(KHW_KIND_LETTERS[kind]);
        }
        return;
    }
    text_decimal(held->place[run]);
    text_character(KHW_KIND_LETTERS[kind]);
}

// the encoding `low`, `high` written after what the text holds as two words of sixteen hex digits, each after a space
static void khw_encoding_printed(unsigned long long low, unsigned long long high)
{
    text_character(' ');
    text_hex(low);
    text_character(' ');
    text_hex(high);
}

// the runs of `held` read from `text`, `held->runs` of them: three words a run, its first bit, its last and its place
// with its kind's letter after it, a register where none; or two, its first bit and its last, where the line holds no
// place, every run then holding none. 1, or 0 where the words are neither
int khw_runs_read(KhwAnswered *held, const char *text)
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
        char place[16];
        held->place[run] = KHW_PLACE_NONE;
        if (each == 2u)
        {
            if (!line_unsigned(&rest, &held->first[run]) || !line_unsigned(&rest, &held->last[run]))
            {
                return 0;
            }
        }
        else
        {
            if (!line_unsigned(&rest, &held->first[run]) || !line_unsigned(&rest, &held->last[run]) ||
                !line_word(&rest, place, sizeof(place)))
            {
                return 0;
            }
            // the none mark, with its kind's letter after it where the run is no register's, or the place with its
            // letter after it; the letter's place among the four is its kind, and no letter is a register's
            char *end = NULL;
            const int none = (strncmp(place, KHW_PLACE_NONE_TEXT, strlen(KHW_PLACE_NONE_TEXT)) == 0);
            held->place[run] = none ? KHW_PLACE_NONE : (unsigned int)strtoul(place, &end, 10);
            end = none ? (place + strlen(KHW_PLACE_NONE_TEXT)) : end;
            const char *const letter = ((end != NULL) && (*end != '\0')) ? strchr(KHW_KIND_LETTERS, *end) : NULL;
            held->kind[run] = (letter != NULL) ? (unsigned int)(letter - KHW_KIND_LETTERS) : KHW_KIND_REGISTER;
        }
    }
    return 1;
}

// the relation `held` answered as the answers and the working set write it, `ladder.<anchor>` or `frame.<relation>`,
// the frame's with its reading after a dot where `with_reading` is not 0, into `written` of `room` bytes. The text
const char *khw_relation_named(const KhwAnswered *held, int with_reading, char *written, size_t room)
{
    if (held->frame == (unsigned int)FRAME_RELATION_COUNT)
    {
        snprintf(written, room, "%s%s", KHW_READS_LADDER, s_anchor_text[held->anchor]);
    }
    else if (with_reading != 0)
    {
        snprintf(written, room, "%s%s.%llx", FRAME_READS, s_frame_text[held->frame], held->reading);
    }
    else
    {
        snprintf(written, room, "%s%s", FRAME_READS, s_frame_text[held->frame]);
    }
    return written;
}

// the forms the part answered for handed to the run as KHW_FORMS_NAME, one a line, for the vendor's writer to
// name and lay out: the relation the gate read, whether the answers read a word signed and whether the part was asked,
// the encoding, and each run's first and last bit and its place in the relation's tuple. A form not yet classified, or
// one with no operand run, names nothing; the vendor lays out none of them. 1, or 0 with the reason printed
int khw_answers_written(void)
{
    static char s_forms_bytes[QRY_WRITER_RING_BYTES / 2ull];
    text_begun(s_forms_bytes, sizeof(s_forms_bytes));
    for (unsigned int at = 0u; at < s_names; at += 1u)
    {
        const KhwAnswered *const held = &s_named[at];
        if ((held->phase != KHW_PHASE_CLASSIFIED) || (held->runs == 0u))
        {
            continue;
        }
        char relation[64];
        text_put(khw_relation_named(held, 0, relation, sizeof(relation)));
        text_character(' ');
        text_signed(held->signed_read);
        text_character(' ');
        text_signed(held->signedness_asked);
        khw_encoding_printed(held->low, held->high);
        text_character(' ');
        text_decimal(held->runs);
        for (unsigned int run = 0u; run < held->runs; run += 1u)
        {
            khw_run_printed(held, run);
        }
        text_character('\n');
    }
    return khw_text_handed(KHW_FORMS_NAME);
}

// The working set handed to the run as KHW_WORK_NAME, every form of it a line read back by khw_forms_read the
// next pass: its phase and the order its words were read, the relation, the signedness, the encoding, and its runs
// with their places. A discovered form carries no run, a turned form the candidates its register-runs will sort, each
// with no place, a classified form the operands kept. It is the state the passes of the split hand each other, kept
// apart from what the vendor lays out. 1, or 0 with the reason printed
int khw_forms_work_written(void)
{
    static char s_work_bytes[QRY_WRITER_RING_BYTES / 2ull];
    text_begun(s_work_bytes, sizeof(s_work_bytes));
    for (unsigned int at = 0u; at < s_names; at += 1u)
    {
        const KhwAnswered *const held = &s_named[at];
        char relation[64];
        text_decimal(held->phase);
        text_character(' ');
        text_signed(held->swapped);
        text_character(' ');
        text_put(khw_relation_named(held, 1, relation, sizeof(relation)));
        text_character(' ');
        text_signed(held->signed_read);
        text_character(' ');
        text_signed(held->signedness_asked);
        khw_encoding_printed(held->low, held->high);
        text_character(' ');
        text_decimal(held->runs);
        for (unsigned int run = 0u; run < held->runs; run += 1u)
        {
            khw_run_printed(held, run);
        }
        text_character('\n');
    }
    return khw_text_handed(KHW_WORK_NAME);
}
