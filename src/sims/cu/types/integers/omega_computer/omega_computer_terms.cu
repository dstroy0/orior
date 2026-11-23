// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// omega_computer_terms.cu: cells, terms: counting, unranking, parsing, reduction
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "omega_computer_internal.h"

// the run ended with `why`, as a table passes its bound or the pool gives no cell
void omega_computer_refused(const char *why)
{
    static const char s_head[] = "omega_computer: ";
    platform_terminal_write(1, s_head, (unsigned long long)(sizeof(s_head) - 1u));
    platform_terminal_write(
        1, why,
        (unsigned long long)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = why, .cap = PLATFORM_TEXT_TO_TERMINATOR));
    platform_terminal_write(1, "\n", 1ULL);
    platform_process_ended(2);
}

// A cell of `count` elements of `size` bytes on the persistent tier. The run ends where the pool gives no cell
// [RETURNS OWNERSHIP]
void *omega_computer_taken(unsigned long long count, unsigned long long size)
{
    void *const cell = platform_cell_taken(count * size, (unsigned int)platform_place_g_program_owner, 0);
    if (cell == NULL)
    {
        omega_computer_refused("the pool gave no cell");
    }
    return cell;
}

// a cell that omega_computer_taken gave, given back [TAKES OWNERSHIP]
void omega_computer_given(void *cell)
{
    platform_cell_released(cell, (unsigned int)platform_place_g_program_owner);
}

void omega_computer_put(OmegaComputerTerm *term, int token)
{
    if (term->count == term->room)
    {
        omega_computer_refused("a term passes its room");
    }
    term->token[term->count] = token;
    term->count += 1u;
}

// 1 where the two terms are the same tokens
int omega_computer_same(const OmegaComputerTerm *left, const OmegaComputerTerm *right)
{
    return (left->count == right->count) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = left->token, .other = right->token,
                       .bytes = left->count * sizeof(int)) == 0);
}

// `count` bits of `text` put at the end of the bits
void omega_computer_bits_put(OmegaComputerBits *bits, const char *text, size_t count)
{
    if (count > (bits->room - bits->count))
    {
        omega_computer_refused("a text of bits passes its room");
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &bits->bit[bits->count], .src = text, .bytes = count);
    bits->count += count;
}

// the closed-under-k count of n-bit codes; k past n - 1 counts as n - 1
unsigned long long omega_computer_count(const OmegaComputerCounts *counts, unsigned int length, unsigned int depth)
{
    if (length < 2u)
    {
        return 0ULL;
    }
    const unsigned int range = (depth < (length - 1u)) ? depth : (length - 1u);
    return counts->count[length][range];
}

void omega_computer_count_all(OmegaComputerCounts *counts, unsigned int max)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = counts->count, .bytes = sizeof(counts->count), .val = 0);
    for (unsigned int length = 2u; length <= max; length += 1u)
    {
        for (unsigned int depth = 0u; depth < length; depth += 1u)
        {
            unsigned long long total = ((length - 1u) <= depth) ? 1ULL : 0ULL;
            total += omega_computer_count(counts, length - 2u, depth + 1u);
            for (unsigned int left = 2u; (left + 4u) <= length; left += 1u)
            {
                total +=
                    omega_computer_count(counts, left, depth) * omega_computer_count(counts, length - 2u - left, depth);
            }
            counts->count[length][depth] = total;
        }
    }
}

// the term of `length` bits closed under `depth` at `index` in the count's order: the variable, then the lambda, then
// the applications by the left part's length
void omega_computer_unrank(const OmegaComputerCounts *counts, unsigned int length, unsigned int depth,
                           unsigned long long index, OmegaComputerTerm *term)
{
    if ((length - 1u) <= depth)
    {
        if (index == 0ULL)
        {
            // an index below the length, which is at max 40
            omega_computer_put(term, (int)(length - 1u));
            return;
        }
        index -= 1ULL;
    }
    const unsigned long long bodies = omega_computer_count(counts, length - 2u, depth + 1u);
    if (index < bodies)
    {
        omega_computer_put(term, OMEGA_COMPUTER_LAMBDA);
        omega_computer_unrank(counts, length - 2u, depth + 1u, index, term);
        return;
    }
    index -= bodies;
    for (unsigned int left = 2u; (left + 4u) <= length; left += 1u)
    {
        const unsigned long long lefts = omega_computer_count(counts, left, depth);
        const unsigned long long rights = omega_computer_count(counts, length - 2u - left, depth);
        if (index < (lefts * rights))
        {
            omega_computer_put(term, OMEGA_COMPUTER_APPLY);
            omega_computer_unrank(counts, left, depth, index / rights, term);
            omega_computer_unrank(counts, length - 2u - left, depth, index % rights, term);
            return;
        }
        index -= lefts * rights;
    }
}

// the `length` bits of a code read into tokens from `at`; 1 where it is one term with every variable bound
int omega_computer_parse(const char *code, size_t length, size_t *at, int depth, OmegaComputerTerm *term)
{
    if ((*at + 2u) > length)
    {
        return 0;
    }
    if (code[*at] == '0')
    {
        const int lambda = code[*at + 1u] == '0';
        *at += 2u;
        if (lambda != 0)
        {
            omega_computer_put(term, OMEGA_COMPUTER_LAMBDA);
            return omega_computer_parse(code, length, at, depth + 1, term);
        }
        omega_computer_put(term, OMEGA_COMPUTER_APPLY);
        return omega_computer_parse(code, length, at, depth, term) &&
               omega_computer_parse(code, length, at, depth, term);
    }
    const size_t ones = EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &code[*at], .cap = length - *at, .other = "1");
    *at += ones;
    if (*at >= length)
    {
        return 0;
    }
    *at += 1u;
    // a run of ones within a code of at max OMEGA_COMPUTER_BITS_MAX bits. An int stores its count
    const int index = (int)ones;
    omega_computer_put(term, index);
    return (index <= depth) ? 1 : 0;
}

// the term's code put at the end of `code`
void omega_computer_code(const OmegaComputerTerm *term, OmegaComputerBits *code)
{
    for (size_t at = 0u; at < term->count; at += 1u)
    {
        const int token = term->token[at];
        if (token == OMEGA_COMPUTER_LAMBDA)
        {
            omega_computer_bits_put(code, "00", 2u);
        }
        else if (token == OMEGA_COMPUTER_APPLY)
        {
            omega_computer_bits_put(code, "01", 2u);
        }
        else
        {
            // an index from 1, at max the lambdas above it
            const size_t ones = (size_t)token;
            if ((ones + 1u) > (code->room - code->count))
            {
                omega_computer_refused("a text of bits passes its room");
            }
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &code->bit[code->count], .bytes = ones, .val = '1');
            code->count += ones;
            omega_computer_bits_put(code, "0", 1u);
        }
    }
}

// The bits as Tromp's list: cons b rest = \z. z b rest. A 0 is the true \x\y. x, a 1 is the false \x\y. y, and nil
// is the false
void omega_computer_list_tokens(const char *bits, size_t count, OmegaComputerTerm *term)
{
    for (size_t at = 0u; at < count; at += 1u)
    {
        omega_computer_put(term, OMEGA_COMPUTER_LAMBDA);
        omega_computer_put(term, OMEGA_COMPUTER_APPLY);
        omega_computer_put(term, OMEGA_COMPUTER_APPLY);
        omega_computer_put(term, 1);
        omega_computer_put(term, OMEGA_COMPUTER_LAMBDA);
        omega_computer_put(term, OMEGA_COMPUTER_LAMBDA);
        omega_computer_put(term, (bits[at] == '0') ? 2 : 1);
    }
    omega_computer_put(term, OMEGA_COMPUTER_LAMBDA);
    omega_computer_put(term, OMEGA_COMPUTER_LAMBDA);
    omega_computer_put(term, 1);
}

// The rewrite, as chaitin_omega's: one past the subterm that starts at `at`. Each token is a step of the count of the
// subterms that the walk still needs
static size_t omega_computer_end(const int *term, size_t at)
{
    long need = 1;
    while (need > 0)
    {
        const int token = term[at];
        at += 1u;
        need += (token == OMEGA_COMPUTER_APPLY) ? 1 : ((token == OMEGA_COMPUTER_LAMBDA) ? 0 : -1);
    }
    return at;
}

// the argument copied under `lifted` more lambdas: a variable bound outside it moves out by that many
static size_t omega_computer_lift(const int *term, size_t at, int bound, int lifted, OmegaComputerTerm *out)
{
    const int token = term[at];
    if (token == OMEGA_COMPUTER_LAMBDA)
    {
        omega_computer_put(out, OMEGA_COMPUTER_LAMBDA);
        return omega_computer_lift(term, at + 1u, bound + 1, lifted, out);
    }
    if (token == OMEGA_COMPUTER_APPLY)
    {
        omega_computer_put(out, OMEGA_COMPUTER_APPLY);
        const size_t right = omega_computer_lift(term, at + 1u, bound, lifted, out);
        return omega_computer_lift(term, right, bound, lifted, out);
    }
    omega_computer_put(out, (token > bound) ? (token + lifted) : token);
    return at + 1u;
}

// the body with the argument put for the variable its lambda binds, and every variable bound past that lambda moved
// in by one
static size_t omega_computer_substitute(const int *term, size_t at, int depth, size_t argument, OmegaComputerTerm *out)
{
    const int token = term[at];
    if (token == OMEGA_COMPUTER_LAMBDA)
    {
        omega_computer_put(out, OMEGA_COMPUTER_LAMBDA);
        return omega_computer_substitute(term, at + 1u, depth + 1, argument, out);
    }
    if (token == OMEGA_COMPUTER_APPLY)
    {
        omega_computer_put(out, OMEGA_COMPUTER_APPLY);
        const size_t right = omega_computer_substitute(term, at + 1u, depth, argument, out);
        return omega_computer_substitute(term, right, depth, argument, out);
    }
    if (token == (depth + 1))
    {
        (void)omega_computer_lift(term, argument, 0, depth, out);
    }
    else
    {
        omega_computer_put(out, (token > (depth + 1)) ? (token - 1) : token);
    }
    return at + 1u;
}

// the `count` tokens from `from` put at the end of the term
void omega_computer_appended(OmegaComputerTerm *term, const int *from, size_t count)
{
    if (count > (term->room - term->count))
    {
        omega_computer_refused("a term passes its room");
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &term->token[term->count], .src = from, .bytes = count * sizeof(int));
    term->count += count;
}

// One normal order step. The leftmost outermost redex is the first application whose left part is a lambda, in the
// order of the code. The answer is 0 at a normal form
int omega_computer_step(const OmegaComputerTerm *term, OmegaComputerTerm *next)
{
    for (size_t at = 0u; (at + 1u) < term->count; at += 1u)
    {
        if ((term->token[at] == OMEGA_COMPUTER_APPLY) && (term->token[at + 1u] == OMEGA_COMPUTER_LAMBDA))
        {
            const size_t argument = omega_computer_end(term->token, at + 1u);
            const size_t after = omega_computer_end(term->token, argument);
            next->count = 0u;
            omega_computer_appended(next, term->token, at);
            (void)omega_computer_substitute(term->token, at + 2u, 0, argument, next);
            omega_computer_appended(next, &term->token[after], term->count - after);
            return 1;
        }
    }
    return 0;
}

// the leading applications of the term
static unsigned int omega_computer_applications(const OmegaComputerTerm *term)
{
    unsigned int range = 0u;
    while ((range < term->count) && (term->token[range] == OMEGA_COMPUTER_APPLY))
    {
        range += 1u;
    }
    return range;
}

// A term's spine: k leading applications, then the head, then its k arguments. The spine subterm at position a runs
// from token a to ends[a], and `ends` holds k + 1 of them. The answer is k.
unsigned int omega_computer_spine(const OmegaComputerTerm *term, size_t *ends)
{
    const unsigned int range = omega_computer_applications(term);
    size_t end = omega_computer_end(term->token, range);
    ends[range] = end;
    for (unsigned int position = range; position > 0u; position -= 1u)
    {
        end = omega_computer_end(term->token, end);
        ends[position - 1u] = end;
    }
    return range;
}

// the spine position of the head redex, where the term is an application spine over a lambda
unsigned int omega_computer_head_redex(const OmegaComputerTerm *term)
{
    const unsigned int range = omega_computer_applications(term);
    return ((range > 0u) && (range < term->count) && (term->token[range] == OMEGA_COMPUTER_LAMBDA))
               ? (range - 1u)
               : OMEGA_COMPUTER_NOT_HEAD;
}

// proven growth without end (chaitin_omega's omega_grows_forever): a spine subterm reduced only by head steps comes
// back deeper on the spine. It has no head normal form and neither has the term
int omega_computer_grows_forever(const OmegaComputerTerm *stored, const size_t *stored_ends, unsigned int stored_range,
                                 const OmegaComputerTerm *term, unsigned int least, size_t *ends)
{
    const unsigned int range = omega_computer_spine(term, ends);
    const unsigned int deepest = (least < stored_range) ? least : stored_range;
    for (unsigned int from = 0u; from <= deepest; from += 1u)
    {
        const size_t size = stored_ends[from] - from;
        for (unsigned int to = from + 1u; to <= range; to += 1u)
        {
            if (((ends[to] - to) == size) && (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &stored->token[from],
                                                         .other = &term->token[to], .bytes = size * sizeof(int)) == 0))
            {
                return 1;
            }
        }
    }
    return 0;
}

// normal order under a step and a size budget, watched by Brent's cycle finder and the growth proof
size_t g_omega_computer_peak = 0u;
