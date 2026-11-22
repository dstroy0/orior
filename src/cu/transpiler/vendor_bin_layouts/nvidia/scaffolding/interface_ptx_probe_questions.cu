// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_ptx_probe_questions.cu: registers, forms and the questions
#include "../../../../engine/engine_note.h"
#include "interface_ptx_probe_internal.h"

static const unsigned int s_probe_edges[PROBE_EDGES] = {
    0u, 1u, 2u, 3u, 31u, 32u, 33u, 64u, 0x7FFFFFFFu, 0x80000000u, 0x80000001u, 0xFFFFFFFEu, 0xFFFFFFFFu, 0x10000u};

// register `number` of the bank `bank` as the ruleset names it, the writer marked broken where it names none
static ProbeName probe_register(ProbeWriter *writer, const char *bank, unsigned int number)
{
    ProbeName name;
    VerbaLinea line = mmgr_line_over(name.text, sizeof(name.text));
    const int named = ruleset_register_written(writer->rules, bank, number, &line);
    (void)mmgr_line_finish(&line);
    name.text[0] = (named != 0) ? name.text[0] : '\0';
    writer->broken = writer->broken || (name.text[0] == '\0');
    return name;
}

ProbeName probe_temporary(ProbeWriter *writer, unsigned int number)
{
    return probe_register(writer, "temporary", number);
}

ProbeName probe_wide(ProbeWriter *writer, unsigned int number)
{
    return probe_register(writer, "wide", number);
}

static ProbeName probe_predicate(ProbeWriter *writer, unsigned int number)
{
    return probe_register(writer, "predicate", number);
}

// A scratch register a construct takes onto `name`, of the temporaries, the 64-bit temporaries or the predicates. 0
// for another bank or past the registers declared
static int probe_scratch(void *context, const char *bank, VerbaLinea *name)
{
    ProbeWriter *const writer = (ProbeWriter *)context;
    unsigned int *next = NULL;
    if (mmgr_text_equal(bank, "temporary", PLATFORM_TEXT_TO_TERMINATOR))
    {
        next = &writer->scratch_temporary;
    }
    else if (mmgr_text_equal(bank, "wide", PLATFORM_TEXT_TO_TERMINATOR))
    {
        next = &writer->scratch_wide;
    }
    else if (mmgr_text_equal(bank, "predicate", PLATFORM_TEXT_TO_TERMINATOR))
    {
        next = &writer->scratch_predicate;
    }
    if ((next == NULL) || (*next >= PROBE_DECLARED))
    {
        return 0;
    }
    *next += 1u;
    return ruleset_register_written(writer->rules, bank, *next - 1u, name);
}

void probe_form(ProbeWriter *writer, VerbaLinea *text, const char *name, const char *const *arguments,
                unsigned int count)
{
    const RulesetNamedScratch scratch = {probe_scratch, writer};
    if (!ruleset_form_written(writer->rules, name, arguments, count, &scratch, text))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  interface_ptx_probe: the ruleset does not write the form ");
            mmgr_line_put(&engine_note.line, name);
            mmgr_line_put(&engine_note.line, " with ");
            mmgr_line_decimal(&engine_note.line, count, 1u);
            mmgr_line_put(&engine_note.line, " arguments");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        writer->broken = 1;
    }
}

static unsigned long long probe_mix(unsigned long long value)
{
    // splitmix64's finalizer: every case's words from its number alone
    value += 0x9E3779B97F4A7C15ULL;
    value = (value ^ (value >> 30u)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27u)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31u);
}

// a case's input words: each an edge three times in four, else a word drawn whole
void probe_case(unsigned int number, unsigned int *in)
{
    for (unsigned int word = 0u; word < PROBE_IN_WORDS; word += 1u)
    {
        const unsigned long long drawn = probe_mix(((unsigned long long)number * PROBE_IN_WORDS) + word);
        // a drawn word's low 32 bits, and an edge's place below the edges' count
        in[word] = ((drawn & 3ULL) == 0ULL) ? (unsigned int)(drawn >> 32u)
                                            : s_probe_edges[(unsigned int)((drawn >> 2u) % PROBE_EDGES)];
    }
}

static unsigned long long probe_pair(unsigned int low, unsigned int high)
{
    return ((unsigned long long)high << 32u) | low;
}

// The questions of the process and the bytes of their bodies, each body terminated after the one before. The calls of
// this module share them, and the module writes the questions once
static ProbeQuestion s_probe_questions[PROBE_QUESTIONS_MAX];
static unsigned int s_probe_question_count;
static char s_probe_bodies[PROBE_BODIES_BYTES];

// the questions as the module writes them: the writer, the registers they name, and the body it writes now. That body
// lies over the rest of the bodies' bytes, after the bytes that earlier bodies take
struct ProbeAsking
{
    ProbeWriter *writer;
    ProbeName t[12];
    ProbeName w[4];
    ProbeName p[4];
    VerbaLinea body;
    size_t used;
};

// the next body begun over the bodies' bytes after those used
static void ask_body_begun(ProbeAsking *asking)
{
    asking->body = mmgr_line_over(&s_probe_bodies[asking->used], sizeof(s_probe_bodies) - asking->used);
}

// The question of the body written so far, `outputs` words of it under `rule`, kept and the next body begun. The
// writer is broken where the bodies' bytes or the questions are full
static void ask_query(ProbeAsking *asking, const char *name, unsigned int outputs, ProbeRule rule)
{
    const size_t length = mmgr_line_finish(&asking->body);
    const int kept = mmgr_line_ok(&asking->body) && (s_probe_question_count < PROBE_QUESTIONS_MAX);
    if (kept != 0)
    {
        s_probe_questions[s_probe_question_count] = {name, &s_probe_bodies[asking->used], outputs, rule};
        s_probe_question_count += 1u;
        asking->used += length + 1u;
    }
    asking->writer->broken = asking->writer->broken || (kept == 0);
    ask_body_begun(asking);
}

// form `name` written onto the body with the arguments after it
#define ASK_FORM(asking_, name_, ...) PROBE_FORM((asking_)->writer, &(asking_)->body, name_, __VA_ARGS__)

// a predicate read out as 1 or 0
static void ask_read_out(ProbeAsking *asking, unsigned int output, unsigned int predicate)
{
    ASK_FORM(asking, "word_select", asking->t[8u + output].text, "1", "0", asking->p[predicate].text);
}

// the questions, each written in the ruleset's forms. Inputs lie in temporaries 0 to 7 and outputs in 8 to 11, each
// output set to 0 before the body
const ProbeQuestion *probe_questions(ProbeWriter *writer, unsigned int *count)
{
    if (s_probe_question_count != 0u)
    {
        *count = s_probe_question_count;
        return s_probe_questions;
    }
    static ProbeAsking s_asking;
    ProbeAsking *const asking = &s_asking;
    asking->writer = writer;
    asking->used = 0u;
    for (unsigned int number = 0u; number < 12u; number += 1u)
    {
        asking->t[number] = probe_temporary(writer, number);
    }
    for (unsigned int number = 0u; number < 4u; number += 1u)
    {
        asking->w[number] = probe_wide(writer, number);
        asking->p[number] = probe_predicate(writer, number);
    }
    ask_body_begun(asking);

    ASK_FORM(asking, "word_add", asking->t[8].text, asking->t[0].text, asking->t[1].text);
    ask_query(asking, "word_add", 1u, [](const unsigned int *in, unsigned int *out) {
        out[0] = in[0] + in[1];
        return 1;
    });
    ASK_FORM(asking, "word_add_first", asking->t[8].text, asking->t[0].text, asking->t[3].text);
    ASK_FORM(asking, "word_add_middle", asking->t[9].text, asking->t[1].text, asking->t[4].text);
    ASK_FORM(asking, "word_add_last", asking->t[10].text, asking->t[2].text, asking->t[5].text);
    ask_query(asking, "word_add_first, word_add_middle, word_add_last: 96 bits", 3u,
              [](const unsigned int *in, unsigned int *out) {
                  unsigned long long carry = 0ULL;
                  for (unsigned int limb = 0u; limb < 3u; limb += 1u)
                  {
                      const unsigned long long sum = (unsigned long long)in[limb] + in[3u + limb] + carry;
                      out[limb] = (unsigned int)sum;
                      carry = sum >> 32u;
                  }
                  return 1;
              });
    ASK_FORM(asking, "word_sub", asking->t[8].text, asking->t[0].text, asking->t[1].text);
    ask_query(asking, "word_sub", 1u, [](const unsigned int *in, unsigned int *out) {
        out[0] = in[0] - in[1];
        return 1;
    });
    ASK_FORM(asking, "word_sub_first", asking->t[8].text, asking->t[0].text, asking->t[3].text);
    ASK_FORM(asking, "word_sub_middle", asking->t[9].text, asking->t[1].text, asking->t[4].text);
    ASK_FORM(asking, "word_sub_last", asking->t[10].text, asking->t[2].text, asking->t[5].text);
    ask_query(asking, "word_sub_first, word_sub_middle, word_sub_last: 96 bits", 3u,
              [](const unsigned int *in, unsigned int *out) {
                  unsigned long long borrow = 0ULL;
                  for (unsigned int limb = 0u; limb < 3u; limb += 1u)
                  {
                      const unsigned long long difference = (unsigned long long)in[limb] - in[3u + limb] - borrow;
                      out[limb] = (unsigned int)difference;
                      borrow = (difference >> 32u) & 1ULL;
                  }
                  return 1;
              });
    ASK_FORM(asking, "word_sub_first", asking->t[8].text, asking->t[0].text, asking->t[1].text);
    ASK_FORM(asking, "word_borrow_read", asking->t[9].text, asking->p[0].text);
    ask_read_out(asking, 2u, 0u);
    ask_query(asking, "word_sub_first, word_borrow_read", 3u, [](const unsigned int *in, unsigned int *out) {
        out[0] = in[0] - in[1];
        out[1] = (in[0] < in[1]) ? 0xFFFFFFFFu : 0u;
        out[2] = (in[0] < in[1]) ? 1u : 0u;
        return 1;
    });
    ASK_FORM(asking, "word_sub_first", asking->t[8].text, asking->t[0].text, asking->t[3].text);
    ASK_FORM(asking, "word_sub_middle", asking->t[9].text, asking->t[1].text, asking->t[4].text);
    ASK_FORM(asking, "word_sub_middle", asking->t[10].text, asking->t[2].text, asking->t[5].text);
    ASK_FORM(asking, "word_borrow_read", asking->t[11].text, asking->p[0].text);
    ask_query(asking, "word_sub_first, word_sub_middle, word_sub_middle, word_borrow_read: 96 bits", 4u,
              [](const unsigned int *in, unsigned int *out) {
                  unsigned long long borrow = 0ULL;
                  for (unsigned int limb = 0u; limb < 3u; limb += 1u)
                  {
                      const unsigned long long difference = (unsigned long long)in[limb] - in[3u + limb] - borrow;
                      out[limb] = (unsigned int)difference;
                      borrow = (difference >> 32u) & 1ULL;
                  }
                  out[3] = (borrow != 0ULL) ? 0xFFFFFFFFu : 0u;
                  return 1;
              });
    ASK_FORM(asking, "word_copy", asking->t[8].text, asking->t[0].text);
    ASK_FORM(asking, "word_set", asking->t[9].text, "4294967295");
    ASK_FORM(asking, "word_set", asking->t[10].text, "2147483648");
    ask_query(asking, "word_copy, word_set", 3u, [](const unsigned int *in, unsigned int *out) {
        out[0] = in[0];
        out[1] = 0xFFFFFFFFu;
        out[2] = 0x80000000u;
        return 1;
    });
    ASK_FORM(asking, "word_bitand", asking->t[8].text, asking->t[0].text, asking->t[1].text);
    ASK_FORM(asking, "word_bitor", asking->t[9].text, asking->t[0].text, asking->t[1].text);
    ASK_FORM(asking, "word_bitxor", asking->t[10].text, asking->t[0].text, asking->t[1].text);
    ask_query(asking, "word_bitand, word_bitor, word_bitxor", 3u, [](const unsigned int *in, unsigned int *out) {
        out[0] = in[0] & in[1];
        out[1] = in[0] | in[1];
        out[2] = in[0] ^ in[1];
        return 1;
    });
    // a shift of the register's width or more leaves 0 (PTX clamps the amount to the width)
    ASK_FORM(asking, "word_shl", asking->t[8].text, asking->t[0].text, asking->t[1].text);
    ASK_FORM(asking, "word_shr", asking->t[9].text, asking->t[0].text, asking->t[1].text);
    ask_query(asking, "word_shl, word_shr", 2u, [](const unsigned int *in, unsigned int *out) {
        out[0] = (in[1] < 32u) ? (in[0] << in[1]) : 0u;
        out[1] = (in[1] < 32u) ? (in[0] >> in[1]) : 0u;
        return 1;
    });
    // the pair's low word after a right shift by the amount clamped to 32
    ASK_FORM(asking, "word_funnel_right", asking->t[8].text, asking->t[0].text, asking->t[1].text, asking->t[2].text);
    ask_query(asking, "word_funnel_right", 1u, [](const unsigned int *in, unsigned int *out) {
        const unsigned int bits = (in[2] < 32u) ? in[2] : 32u;
        out[0] = (unsigned int)(probe_pair(in[0], in[1]) >> bits);
        return 1;
    });
    ASK_FORM(asking, "word_mul", asking->t[8].text, asking->t[0].text, asking->t[1].text);
    ASK_FORM(asking, "word_mul_add", asking->t[9].text, asking->t[0].text, asking->t[1].text, asking->t[2].text);
    ask_query(asking, "word_mul, word_mul_add", 2u, [](const unsigned int *in, unsigned int *out) {
        out[0] = in[0] * in[1];
        out[1] = (in[0] * in[1]) + in[2];
        return 1;
    });
    ASK_FORM(asking, "word_div", asking->t[8].text, asking->t[0].text, asking->t[1].text);
    ask_query(asking, "word_div", 1u, [](const unsigned int *in, unsigned int *out) {
        out[0] = (in[1] != 0u) ? (in[0] / in[1]) : 0u;
        return (in[1] != 0u) ? 1 : 0;
    });
    ASK_FORM(asking, "test_word_nonzero", asking->p[0].text, asking->t[2].text);
    ASK_FORM(asking, "word_select", asking->t[8].text, asking->t[0].text, asking->t[1].text, asking->p[0].text);
    ask_query(asking, "word_select, test_word_nonzero", 1u, [](const unsigned int *in, unsigned int *out) {
        out[0] = (in[2] != 0u) ? in[0] : in[1];
        return 1;
    });
    // a product and its carry in: a b + c never passes 64 bits
    ASK_FORM(asking, "word_mul_low", asking->t[8].text, asking->t[0].text, asking->t[1].text, asking->t[2].text);
    ASK_FORM(asking, "word_mul_high", asking->t[9].text, asking->t[0].text, asking->t[1].text);
    ask_query(asking, "word_mul_low, word_mul_high", 2u, [](const unsigned int *in, unsigned int *out) {
        const unsigned long long product = ((unsigned long long)in[0] * in[1]) + in[2];
        out[0] = (unsigned int)product;
        out[1] = (unsigned int)(product >> 32u);
        return 1;
    });
    ASK_FORM(asking, "sign_set", asking->t[8].text, "-1");
    ASK_FORM(asking, "sign_set", asking->t[9].text, "1");
    ASK_FORM(asking, "test_word_nonzero", asking->p[0].text, asking->t[2].text);
    ASK_FORM(asking, "sign_select", asking->t[10].text, asking->t[0].text, asking->t[1].text, asking->p[0].text);
    ask_query(asking, "sign_set, sign_select", 3u, [](const unsigned int *in, unsigned int *out) {
        out[0] = 0xFFFFFFFFu;
        out[1] = 1u;
        out[2] = (in[2] != 0u) ? in[0] : in[1];
        return 1;
    });
    // the signed product's low word, and the absolute value and negation modulo 2^32: |-2^31| is -2^31
    ASK_FORM(asking, "sign_mul", asking->t[8].text, asking->t[0].text, asking->t[1].text);
    ASK_FORM(asking, "sign_absolute", asking->t[9].text, asking->t[0].text);
    ASK_FORM(asking, "sign_neg", asking->t[10].text, asking->t[0].text);
    ask_query(asking, "sign_mul, sign_absolute, sign_neg", 3u, [](const unsigned int *in, unsigned int *out) {
        out[0] = in[0] * in[1];
        out[1] = ((in[0] & 0x80000000u) != 0u) ? (0u - in[0]) : in[0];
        out[2] = 0u - in[0];
        return 1;
    });
    ASK_FORM(asking, "test_word_nonzero", asking->p[0].text, asking->t[0].text);
    ASK_FORM(asking, "test_word_zero", asking->p[1].text, asking->t[0].text);
    ASK_FORM(asking, "test_signed_word_negative", asking->p[2].text, asking->t[0].text);
    ask_read_out(asking, 0u, 0u);
    ask_read_out(asking, 1u, 1u);
    ask_read_out(asking, 2u, 2u);
    ask_query(asking, "test_word_nonzero, test_word_zero, test_signed_word_negative", 3u,
              [](const unsigned int *in, unsigned int *out) {
                  out[0] = (in[0] != 0u) ? 1u : 0u;
                  out[1] = (in[0] == 0u) ? 1u : 0u;
                  out[2] = ((in[0] & 0x80000000u) != 0u) ? 1u : 0u;
                  return 1;
              });
    // a signed word is zero where its bits are, which test_word_zero above tests
    ASK_FORM(asking, "test_sign_ne", asking->p[0].text, asking->t[0].text, asking->t[1].text);
    ASK_FORM(asking, "test_sign_gt", asking->p[1].text, asking->t[0].text, asking->t[1].text);
    ask_read_out(asking, 0u, 0u);
    ask_read_out(asking, 1u, 1u);
    ask_query(asking, "test_sign_ne, test_sign_gt", 2u, [](const unsigned int *in, unsigned int *out) {
        // a word read as its two's complement value
        const long long left = (long long)(int)in[0];
        // a word read as its two's complement value
        const long long right = (long long)(int)in[1];
        out[0] = (left != right) ? 1u : 0u;
        out[1] = (left > right) ? 1u : 0u;
        return 1;
    });
    ASK_FORM(asking, "wide_pack", asking->w[0].text, asking->t[0].text, asking->t[1].text);
    ASK_FORM(asking, "wide_pack", asking->w[1].text, asking->t[2].text, asking->t[3].text);
    ASK_FORM(asking, "test_word_nonzero", asking->p[3].text, asking->t[4].text);
    ASK_FORM(asking, "test_wide_nonzero", asking->p[0].text, asking->w[0].text);
    ASK_FORM(asking, "test_wide_eq", asking->p[1].text, asking->w[0].text, asking->w[1].text);
    ASK_FORM(asking, "test_wide_lt", asking->p[2].text, asking->w[0].text, asking->w[1].text);
    ASK_FORM(asking, "test_wide_lt_and", asking->p[3].text, asking->w[0].text, asking->w[1].text, asking->p[3].text);
    ask_read_out(asking, 0u, 0u);
    ask_read_out(asking, 1u, 1u);
    ask_read_out(asking, 2u, 2u);
    ask_read_out(asking, 3u, 3u);
    ask_query(asking, "test_wide_nonzero, test_wide_eq, test_wide_lt, test_wide_lt_and", 4u,
              [](const unsigned int *in, unsigned int *out) {
                  const unsigned long long left = probe_pair(in[0], in[1]);
                  const unsigned long long right = probe_pair(in[2], in[3]);
                  out[0] = (left != 0ULL) ? 1u : 0u;
                  out[1] = (left == right) ? 1u : 0u;
                  out[2] = (left < right) ? 1u : 0u;
                  out[3] = ((left < right) && (in[4] != 0u)) ? 1u : 0u;
                  return 1;
              });
    ASK_FORM(asking, "test_word_nonzero", asking->p[0].text, asking->t[0].text);
    ASK_FORM(asking, "test_word_nonzero", asking->p[1].text, asking->t[1].text);
    ASK_FORM(asking, "predicate_bitxor", asking->p[2].text, asking->p[0].text, asking->p[1].text);
    ASK_FORM(asking, "predicate_bitand", asking->p[3].text, asking->p[0].text, asking->p[1].text);
    ask_read_out(asking, 0u, 2u);
    ask_read_out(asking, 1u, 3u);
    ask_query(asking, "predicate_bitxor, predicate_bitand", 2u, [](const unsigned int *in, unsigned int *out) {
        out[0] = ((in[0] != 0u) != (in[1] != 0u)) ? 1u : 0u;
        out[1] = ((in[0] != 0u) && (in[1] != 0u)) ? 1u : 0u;
        return 1;
    });
    ASK_FORM(asking, "wide_from_word", asking->w[0].text, asking->t[0].text);
    ASK_FORM(asking, "wide_unpack", asking->t[8].text, asking->t[9].text, asking->w[0].text);
    ASK_FORM(asking, "wide_pack", asking->w[1].text, asking->t[1].text, asking->t[2].text);
    ASK_FORM(asking, "word_from_wide", asking->t[10].text, asking->w[1].text);
    ASK_FORM(asking, "wide_unpack", asking->t[11].text, asking->t[7].text, asking->w[1].text);
    ask_query(asking, "wide_from_word, word_from_wide, wide_pack, wide_unpack", 4u,
              [](const unsigned int *in, unsigned int *out) {
                  out[0] = in[0];
                  out[1] = 0u;
                  out[2] = in[1];
                  out[3] = in[1];
                  return 1;
              });
    ASK_FORM(asking, "wide_pack", asking->w[0].text, asking->t[0].text, asking->t[1].text);
    ASK_FORM(asking, "wide_pack", asking->w[1].text, asking->t[2].text, asking->t[3].text);
    ASK_FORM(asking, "wide_mul", asking->w[2].text, asking->w[0].text, asking->w[1].text);
    ASK_FORM(asking, "wide_unpack", asking->t[8].text, asking->t[9].text, asking->w[2].text);
    ASK_FORM(asking, "wide_mul_word", asking->w[3].text, asking->t[4].text, asking->t[5].text);
    ASK_FORM(asking, "wide_unpack", asking->t[10].text, asking->t[11].text, asking->w[3].text);
    ask_query(asking, "wide_mul, wide_mul_word", 4u, [](const unsigned int *in, unsigned int *out) {
        const unsigned long long product = probe_pair(in[0], in[1]) * probe_pair(in[2], in[3]);
        const unsigned long long word_product = (unsigned long long)in[4] * in[5];
        out[0] = (unsigned int)product;
        out[1] = (unsigned int)(product >> 32u);
        out[2] = (unsigned int)word_product;
        out[3] = (unsigned int)(word_product >> 32u);
        return 1;
    });
    ASK_FORM(asking, "wide_pack", asking->w[0].text, asking->t[0].text, asking->t[1].text);
    ASK_FORM(asking, "wide_pack", asking->w[1].text, asking->t[2].text, asking->t[3].text);
    ASK_FORM(asking, "wide_add", asking->w[2].text, asking->w[0].text, asking->w[1].text);
    ASK_FORM(asking, "wide_unpack", asking->t[8].text, asking->t[9].text, asking->w[2].text);
    ask_query(asking, "wide_add", 2u, [](const unsigned int *in, unsigned int *out) {
        const unsigned long long sum = probe_pair(in[0], in[1]) + probe_pair(in[2], in[3]);
        out[0] = (unsigned int)sum;
        out[1] = (unsigned int)(sum >> 32u);
        return 1;
    });
    // a shift of 64 bits or more leaves 0
    ASK_FORM(asking, "wide_pack", asking->w[0].text, asking->t[0].text, asking->t[1].text);
    ASK_FORM(asking, "wide_shl", asking->w[2].text, asking->w[0].text, asking->t[2].text);
    ASK_FORM(asking, "wide_unpack", asking->t[8].text, asking->t[9].text, asking->w[2].text);
    ASK_FORM(asking, "wide_pack", asking->w[1].text, asking->t[3].text, asking->t[4].text);
    ASK_FORM(asking, "test_word_nonzero", asking->p[0].text, asking->t[5].text);
    ASK_FORM(asking, "wide_select", asking->w[3].text, asking->w[0].text, asking->w[1].text, asking->p[0].text);
    ASK_FORM(asking, "wide_unpack", asking->t[10].text, asking->t[11].text, asking->w[3].text);
    ask_query(asking, "wide_shl, wide_select", 4u, [](const unsigned int *in, unsigned int *out) {
        const unsigned long long shifted = (in[2] < 64u) ? (probe_pair(in[0], in[1]) << in[2]) : 0ULL;
        const unsigned long long chosen = (in[5] != 0u) ? probe_pair(in[0], in[1]) : probe_pair(in[3], in[4]);
        out[0] = (unsigned int)shifted;
        out[1] = (unsigned int)(shifted >> 32u);
        out[2] = (unsigned int)chosen;
        out[3] = (unsigned int)(chosen >> 32u);
        return 1;
    });
    ASK_FORM(asking, "wide_pack", asking->w[0].text, asking->t[0].text, asking->t[1].text);
    ASK_FORM(asking, "wide_pack", asking->w[1].text, asking->t[2].text, asking->t[3].text);
    ASK_FORM(asking, "wide_div", asking->w[2].text, asking->w[0].text, asking->w[1].text);
    ASK_FORM(asking, "wide_unpack", asking->t[8].text, asking->t[9].text, asking->w[2].text);
    ask_query(asking, "wide_div", 2u, [](const unsigned int *in, unsigned int *out) {
        const unsigned long long divisor = probe_pair(in[2], in[3]);
        const unsigned long long quotient = (divisor != 0ULL) ? (probe_pair(in[0], in[1]) / divisor) : 0ULL;
        out[0] = (unsigned int)quotient;
        out[1] = (unsigned int)(quotient >> 32u);
        return (divisor != 0ULL) ? 1 : 0;
    });
    // global_add_atomic_word, the one form the resident counts with and no other question reaches. What it leaves
    // cannot be read here: the only load the ruleset gives is ld.global.nc, which promises the location is not written
    // while the kernel runs, and a read-back of a word this just added to breaks that promise (ptxas takes it at its
    // word and folds the two loads into one, the .CONSTANT in the listing). So the case queried is one a question here
    // can test - that the part assembles it, runs it, and goes on past it - and the word it adds to is this thread's
    // own case in the input buffer, which the host never reads back. The add's result is read on the part instead, in
    // the SASS probe's own code, where the instructions are ours and the read-back is a plain load
    ASK_FORM(asking, "global_add_atomic_word", "%interface_wide2");
    ASK_FORM(asking, "word_copy", asking->t[8].text, asking->t[0].text);
    ask_query(asking, "global_add_atomic_word", 1u, [](const unsigned int *in, unsigned int *out) {
        out[0] = in[0];
        return 1;
    });
    *count = s_probe_question_count;
    return s_probe_questions;
}
