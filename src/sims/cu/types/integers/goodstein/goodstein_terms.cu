// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// goodstein_terms.cu: forms in place, the stack of forms, terms, towers, cursors and values
#include "goodstein_internal.h"

// The forms that the calls build and give back before they return, as a stack. A call takes its own on top and gives
// them back before it returns, and a call inside it takes its own above them
static GoodsteinNotation s_stack[GOODSTEIN_STACK_MAX];
static unsigned int s_stack_top = 0u;

// the run ended with `why`, as the forms pass the room of the sim
static void goodstein_refused(const char *why)
{
    static const char s_head[] = "goodstein: ";
    platform_terminal_write(1, s_head, (unsigned long long)(sizeof(s_head) - 1u));
    platform_terminal_write(
        1, why,
        (unsigned long long)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = why, .cap = PLATFORM_TEXT_TO_TERMINATOR));
    platform_terminal_write(1, "\n", 1ULL);
    platform_process_ended(2);
}

// a form on top of the stack, empty [BORROWS]
GoodsteinNotation *goodstein_taken(void)
{
    if (s_stack_top == GOODSTEIN_STACK_MAX)
    {
        goodstein_refused("the calls hold more forms at once than the stack holds");
    }
    GoodsteinNotation *const taken = &s_stack[s_stack_top];
    s_stack_top += 1u;
    taken->count = 0u;
    return taken;
}

// the form on top of the stack given back
void goodstein_given(void)
{
    s_stack_top -= 1u;
}

// the whole of `notation`, read in place
GoodsteinForm goodstein_form(const GoodsteinNotation *notation)
{
    const GoodsteinForm form = {notation->blocks, notation->count};
    return form;
}

// the exponent of the term at the block `block` of `form`, read in place
GoodsteinForm goodstein_exponent(GoodsteinForm form, unsigned int block)
{
    const GoodsteinForm exponent = {&form.blocks[block + 1u], form.blocks[block].exponent_blocks};
    return exponent;
}

// the place of the term after the one at the block `block` of `form`: past the term and the blocks of its exponent
static unsigned int goodstein_next_term(GoodsteinForm form, unsigned int block)
{
    return block + 1u + form.blocks[block].exponent_blocks;
}

// the count of the terms of `form`, a run counting as one
unsigned int goodstein_terms(GoodsteinForm form)
{
    unsigned int terms = 0u;
    for (unsigned int block = 0u; block < form.count; block = goodstein_next_term(form, block))
    {
        terms += 1u;
    }
    return terms;
}

// `from` copied whole into `to`
void goodstein_copied(GoodsteinNotation *to, const GoodsteinNotation *from)
{
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = to->blocks, .src = from->blocks,
               .bytes = (size_t)from->count * sizeof(GoodsteinBlock));
    to->count = from->count;
}

// the block `block` put after the blocks of `form`
static void goodstein_put(GoodsteinNotation *form, const GoodsteinBlock *block)
{
    if (form->count == GOODSTEIN_BLOCKS_MAX)
    {
        goodstein_refused("a form holds more blocks than its room");
    }
    form->blocks[form->count] = *block;
    form->count += 1u;
}

// the term c . b^E put after the blocks of `form`, with its exponent's blocks after it
static void goodstein_term_put(GoodsteinNotation *form, GoodsteinForm exponent, unsigned long long coefficient)
{
    const GoodsteinBlock term = {coefficient, 0ULL, 0ULL, 0ULL, exponent.count};
    goodstein_put(form, &term);
    for (unsigned int at = 0u; at < exponent.count; at += 1u)
    {
        goodstein_put(form, &exponent.blocks[at]);
    }
}

// value written hereditarily in base, put after the blocks of `out`, its terms highest first
static void goodstein_written(unsigned long long value, unsigned long long base, GoodsteinNotation *out)
{
    // the digits of value in base, lowest first: 64 at most, as base is 2 at least
    unsigned long long digits[64];
    unsigned int places = 0u;
    for (unsigned long long rest = value; rest != 0ULL; rest /= base)
    {
        digits[places] = rest % base;
        places += 1u;
    }
    for (unsigned int position = places; position > 0u; position -= 1u)
    {
        const unsigned long long digit = digits[position - 1u];
        if (digit == 0ULL)
        {
            continue;
        }
        const unsigned int at = out->count;
        const GoodsteinBlock term = {digit, 0ULL, 0ULL, 0ULL, 0u};
        goodstein_put(out, &term);
        goodstein_written(position - 1u, base, out);
        out->blocks[at].exponent_blocks = out->count - at - 1u;
    }
}

// value written hereditarily in base, into `out`
void goodstein_of(unsigned long long value, unsigned long long base, GoodsteinNotation *out)
{
    out->count = 0u;
    goodstein_written(value, base, out);
}

// omega^^height into `out`: omega^1 at height 1, and omega raised to the tower below at each height above. The tower
// below each term follows it as its exponent
void goodstein_tower(unsigned int height, GoodsteinNotation *out)
{
    out->count = 0u;
    for (unsigned int level = 0u; level <= height; level += 1u)
    {
        const GoodsteinBlock term = {1ULL, 0ULL, 0ULL, 0ULL, height - level};
        goodstein_put(out, &term);
    }
}

static void goodstein_cursor_enter(GoodsteinCursor *cursor)
{
    if (cursor->block < cursor->form.count)
    {
        cursor->high = cursor->form.blocks[cursor->block].high;
    }
}

// the cursor moved to the term after its own block
static void goodstein_cursor_past(GoodsteinCursor *cursor)
{
    cursor->block = goodstein_next_term(cursor->form, cursor->block);
    goodstein_cursor_enter(cursor);
}

static void goodstein_cursor_next(GoodsteinCursor *cursor)
{
    const GoodsteinBlock *const block = &cursor->form.blocks[cursor->block];
    if ((block->origin == 0ULL) || (cursor->high == block->low))
    {
        goodstein_cursor_past(cursor);
    }
    else
    {
        cursor->high -= 1ULL;
    }
}

// the order of two forms as ordinals, which is their order as numbers at any one base: terms compared highest first,
// exponents recursively, then coefficients. Take two runs made at one base, level at their tops with one coefficient.
// They agree term for term down to the higher of their lows, and the comparison passes them in one move.
int goodstein_compare(GoodsteinForm left, GoodsteinForm right, GoodsteinBudget *budget)
{
    GoodsteinCursor one = {left, 0u, 0ULL};
    GoodsteinCursor other = {right, 0u, 0ULL};
    goodstein_cursor_enter(&one);
    goodstein_cursor_enter(&other);
    for (;;)
    {
        const int one_done = one.block >= left.count;
        const int other_done = other.block >= right.count;
        if ((one_done != 0) || (other_done != 0))
        {
            return (one_done == other_done) ? 0 : ((one_done != 0) ? -1 : 1);
        }
        if (budget->terms == 0ULL)
        {
            budget->exhausted = 1;
            return 0;
        }
        budget->terms -= 1ULL;
        const GoodsteinBlock *const one_block = &left.blocks[one.block];
        const GoodsteinBlock *const other_block = &right.blocks[other.block];
        if ((one_block->origin != 0ULL) && (one_block->origin == other_block->origin) && (one.high == other.high) &&
            (one_block->coefficient == other_block->coefficient))
        {
            const unsigned long long floor = (one_block->low > other_block->low) ? one_block->low : other_block->low;
            if (one_block->low == floor)
            {
                goodstein_cursor_past(&one);
            }
            else
            {
                one.high = floor - 1ULL;
            }
            if (other_block->low == floor)
            {
                goodstein_cursor_past(&other);
            }
            else
            {
                other.high = floor - 1ULL;
            }
            continue;
        }
        // a run's term's exponent is the tree of its k, which the stack holds for the comparison below
        GoodsteinNotation *const one_tree = goodstein_taken();
        GoodsteinNotation *const other_tree = goodstein_taken();
        if (one_block->origin != 0ULL)
        {
            goodstein_of(one.high, one_block->origin, one_tree);
        }
        if (other_block->origin != 0ULL)
        {
            goodstein_of(other.high, other_block->origin, other_tree);
        }
        const int top = goodstein_compare(
            (one_block->origin == 0ULL) ? goodstein_exponent(left, one.block) : goodstein_form(one_tree),
            (other_block->origin == 0ULL) ? goodstein_exponent(right, other.block) : goodstein_form(other_tree),
            budget);
        goodstein_given();
        goodstein_given();
        if (budget->exhausted != 0)
        {
            return 0;
        }
        if (top != 0)
        {
            return top;
        }
        if (one_block->coefficient != other_block->coefficient)
        {
            return (one_block->coefficient > other_block->coefficient) ? 1 : -1;
        }
        goodstein_cursor_next(&one);
        goodstein_cursor_next(&other);
    }
}

// base^exponent, 0 where it leaves 64 bits
int goodstein_power(unsigned long long base, unsigned long long exponent, unsigned long long *out)
{
    unsigned long long result = 1ULL;
    for (unsigned long long done = 0ULL; done < exponent; done += 1ULL)
    {
        if (result > (~0ULL / base))
        {
            return 0;
        }
        result *= base;
    }
    *out = result;
    return 1;
}

// total += coefficient . base^exponent, 0 where it leaves 64 bits
int goodstein_add_term(unsigned long long *total, unsigned long long coefficient, unsigned long long base,
                       unsigned long long exponent)
{
    unsigned long long power = 0ULL;
    if ((goodstein_power(base, exponent, &power) == 0) || (power > (~0ULL / coefficient)))
    {
        return 0;
    }
    const unsigned long long term = power * coefficient;
    if (term > (~0ULL - *total))
    {
        return 0;
    }
    *total += term;
    return 1;
}

// the form's value at base, 0 where it leaves 64 bits. A run's k-th exponent is at least k at any base at or above its
// origin. A run that reaches 64 overflows on its first term.
int goodstein_value(GoodsteinForm form, unsigned long long base, unsigned long long *out)
{
    unsigned long long total = 0ULL;
    for (unsigned int at = 0u; at < form.count; at = goodstein_next_term(form, at))
    {
        const GoodsteinBlock *const block = &form.blocks[at];
        if (block->origin == 0ULL)
        {
            unsigned long long exponent = 0ULL;
            if ((goodstein_value(goodstein_exponent(form, at), base, &exponent) == 0) ||
                (goodstein_add_term(&total, block->coefficient, base, exponent) == 0))
            {
                return 0;
            }
            continue;
        }
        for (unsigned long long k = block->high;; k -= 1ULL)
        {
            unsigned long long exponent = 0ULL;
            GoodsteinNotation *const tree = goodstein_taken();
            goodstein_of(k, block->origin, tree);
            const int fits = (goodstein_value(goodstein_form(tree), base, &exponent) != 0) &&
                             (goodstein_add_term(&total, block->coefficient, base, exponent) != 0);
            goodstein_given();
            if (!fits)
            {
                return 0;
            }
            if (k == block->low)
            {
                break;
            }
        }
    }
    *out = total;
    return 1;
}

// subtract one at base: the lowest term c . b^L leaves (c - 1) . b^L and, where L > 0, the run of b - 1 over the
// exponents 0 .. L - 1 of this base. 0 where L's value at base leaves 64 bits, as 64 bits then hold no range of the
// run.
// The lowest term is the last of the form, and the blocks of its exponent end the form
int goodstein_decrement(GoodsteinNotation *form, unsigned long long base)
{
    const GoodsteinForm whole = goodstein_form(form);
    unsigned int last = 0u;
    for (unsigned int at = 0u; at < whole.count; at = goodstein_next_term(whole, at))
    {
        last = at;
    }
    const GoodsteinBlock lowest = form->blocks[last];
    GoodsteinNotation *const exponent = goodstein_taken();
    if (lowest.origin == 0ULL)
    {
        const GoodsteinForm lowest_exponent = goodstein_exponent(whole, last);
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = exponent->blocks, .src = lowest_exponent.blocks,
                   .bytes = (size_t)lowest_exponent.count * sizeof(GoodsteinBlock));
        exponent->count = lowest_exponent.count;
        form->count = last;
    }
    else
    {
        goodstein_of(lowest.low, lowest.origin, exponent);
        if (lowest.low == lowest.high)
        {
            form->count = last;
        }
        else
        {
            form->blocks[last].low += 1ULL;
        }
    }
    if (lowest.coefficient > 1ULL)
    {
        goodstein_term_put(form, goodstein_form(exponent), lowest.coefficient - 1ULL);
    }
    int fits = 1;
    if (exponent->count != 0u)
    {
        unsigned long long range = 0ULL;
        fits = goodstein_value(goodstein_form(exponent), base, &range);
        if (fits != 0)
        {
            const GoodsteinBlock run = {base - 1ULL, base, 0ULL, range - 1ULL, 0u};
            goodstein_put(form, &run);
        }
    }
    goodstein_given();
    return fits;
}
