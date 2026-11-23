// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// chaitin_omega_terms.cu: terms: counting, unranking, reduction
#include "cellularum_laboro/cellularum_laboro.h"
#include "chaitin_omega_internal.h"
#include "memoria_operor/memoria_operor.h"

// the closed-under-k count of n-bit codes; k past n - 1 counts as n - 1, since no variable of an n-bit
// code is bound further out than that
unsigned long long omega_count(const OmegaCounts *counts, unsigned int length, unsigned int depth)
{
    if (length < 2u)
    {
        return 0ULL;
    }
    const unsigned int range = (depth < (length - 1u)) ? depth : (length - 1u);
    return counts->count[length][range];
}

void omega_count_all(OmegaCounts *counts)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = counts->count, .bytes = sizeof(counts->count), .val = 0);
    for (unsigned int length = 2u; length <= counts->length; length += 1u)
    {
        for (unsigned int depth = 0u; depth < length; depth += 1u)
        {
            unsigned long long total = ((length - 1u) <= depth) ? 1ULL : 0ULL;
            total += omega_count(counts, length - 2u, depth + 1u);
            for (unsigned int left = 2u; (left + 4u) <= length; left += 1u)
            {
                total += omega_count(counts, left, depth) * omega_count(counts, length - 2u - left, depth);
            }
            counts->count[length][depth] = total;
        }
    }
}

// the term of `length` bits closed under `depth` at `index` in the count's order: the variable, then the
// lambda, then the applications by the left part's length
void omega_unrank(const OmegaCounts *counts, unsigned int length, unsigned int depth, unsigned long long index,
                  OmegaTokens *term)
{
    if ((length - 1u) <= depth)
    {
        if (index == 0ULL)
        {
            omega_put(term, (int)(length - 1u));
            return;
        }
        index -= 1ULL;
    }
    const unsigned long long bodies = omega_count(counts, length - 2u, depth + 1u);
    if (index < bodies)
    {
        omega_put(term, OMEGA_LAMBDA);
        omega_unrank(counts, length - 2u, depth + 1u, index, term);
        return;
    }
    index -= bodies;
    for (unsigned int left = 2u; (left + 4u) <= length; left += 1u)
    {
        const unsigned long long lefts = omega_count(counts, left, depth);
        const unsigned long long rights = omega_count(counts, length - 2u - left, depth);
        if (index < (lefts * rights))
        {
            omega_put(term, OMEGA_APPLY);
            omega_unrank(counts, left, depth, index / rights, term);
            omega_unrank(counts, length - 2u - left, depth, index % rights, term);
            return;
        }
        index -= lefts * rights;
    }
}

// the run ended with `why`, as a table passes its bound or the pool gives no cell
void omega_refused(const char *why)
{
    static const char s_head[] = "chaitin_omega: ";
    platform_terminal_write(1, s_head, (unsigned long long)(sizeof(s_head) - 1u));
    platform_terminal_write(
        1, why,
        (unsigned long long)EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = why, .cap = PLATFORM_TEXT_TO_TERMINATOR));
    platform_terminal_write(1, "\n", 1ULL);
    platform_process_ended(2);
}

// A cell of `count` elements of `size` bytes, on the temporary tier where `ephemeral` is 1 and on the persistent tier
// where it is 0. The run ends where the pool gives no cell [RETURNS OWNERSHIP]
void *omega_taken(unsigned long long count, unsigned long long size, int ephemeral)
{
    void *const cell = platform_cell_taken(count * size, (unsigned int)platform_place_g_program_owner, ephemeral);
    if ((cell == NULL) && ((count * size) != 0ULL))
    {
        omega_refused("the pool gave no cell");
    }
    return cell;
}

// a cell that omega_taken gave, given back [TAKES OWNERSHIP]
void omega_given(void *cell)
{
    platform_cell_released(cell, (unsigned int)platform_place_g_program_owner);
}

void omega_put(OmegaTokens *term, int token)
{
    if (term->count == term->room)
    {
        omega_refused("a term passes its room");
    }
    term->token[term->count] = token;
    term->count += 1u;
}

// The room of a rewrite under a token budget. A term of n tokens before a step makes at max n + n^2 / 4 after it.
// Each of the k variables that the redex binds in its body B takes a copy of the argument A, and k |A| <= |B| |A|. A
// run starts under the budget or at a program of OMEGA_LENGTH_MAX bits at max, and stops once a term passes the
// budget. A run with no budget of its own takes OMEGA_TERM_MAX, and a step past it ends the run with the reason
size_t omega_rewrite_room(unsigned int tokens)
{
    const unsigned long long most = (tokens > OMEGA_LENGTH_MAX) ? tokens : OMEGA_LENGTH_MAX;
    const unsigned long long room = most + ((most * most) / 4ULL) + 1ULL;
    return (room < OMEGA_TERM_MAX) ? (size_t)room : (size_t)OMEGA_TERM_MAX;
}

static void omega_tokens_taken(OmegaTokens *term, size_t room, int ephemeral)
{
    term->token = (int *)omega_taken(room, sizeof(int), ephemeral);
    term->count = 0u;
    term->room = room;
}

// the tables of a rewrite at `room` tokens, on the temporary tier where `ephemeral` is 1 [RETURNS OWNERSHIP]
void omega_rewrite_taken(OmegaRewrite *rewrite, size_t room, int ephemeral)
{
    omega_tokens_taken(&rewrite->term, room, ephemeral);
    omega_tokens_taken(&rewrite->next, room, ephemeral);
    omega_tokens_taken(&rewrite->stored, room, ephemeral);
    rewrite->ends = (size_t *)omega_taken(room + 1u, sizeof(size_t), ephemeral);
    rewrite->stored_ends = (size_t *)omega_taken(room + 1u, sizeof(size_t), ephemeral);
}

// the tables of a rewrite given back [TAKES OWNERSHIP]
void omega_rewrite_given(OmegaRewrite *rewrite)
{
    omega_given(rewrite->stored_ends);
    omega_given(rewrite->ends);
    omega_given(rewrite->stored.token);
    omega_given(rewrite->next.token);
    omega_given(rewrite->term.token);
}

// one past the subterm that starts at `at`. Each token is a step of the count of the subterms that the walk needs
size_t omega_end(const int *term, size_t at)
{
    long need = 1;
    while (need > 0)
    {
        const int token = term[at];
        at += 1u;
        need += (token == OMEGA_APPLY) ? 1 : ((token == OMEGA_LAMBDA) ? 0 : -1);
    }
    return at;
}

// the length of a term's code: two bits a lambda or an application, k + 1 a variable k
unsigned long long omega_code_bits(const OmegaTokens *term)
{
    unsigned long long bits = 0ULL;
    for (size_t at = 0u; at < term->count; at += 1u)
    {
        bits += (term->token[at] <= 0) ? 2ULL : ((unsigned long long)term->token[at] + 1ULL);
    }
    return bits;
}

// a term's code written out
void omega_code_text(VerbaLinea *line, const OmegaTokens *term)
{
    for (size_t at = 0u; at < term->count; at += 1u)
    {
        if (term->token[at] == OMEGA_LAMBDA)
        {
            mmgr_line_put(line, "00");
        }
        else if (term->token[at] == OMEGA_APPLY)
        {
            mmgr_line_put(line, "01");
        }
        else
        {
            for (int one = 0; one < term->token[at]; one += 1)
            {
                mmgr_line_ch(line, '1');
            }
            mmgr_line_ch(line, '0');
        }
    }
}

// the argument copied under `lifted` more lambdas: a variable bound outside it moves out by that many
static size_t omega_lift(const int *term, size_t at, int bound, int lifted, OmegaTokens *out)
{
    const int token = term[at];
    if (token == OMEGA_LAMBDA)
    {
        omega_put(out, OMEGA_LAMBDA);
        return omega_lift(term, at + 1u, bound + 1, lifted, out);
    }
    if (token == OMEGA_APPLY)
    {
        omega_put(out, OMEGA_APPLY);
        const size_t right = omega_lift(term, at + 1u, bound, lifted, out);
        return omega_lift(term, right, bound, lifted, out);
    }
    omega_put(out, (token > bound) ? (token + lifted) : token);
    return at + 1u;
}

// the body with the argument put for the variable its lambda binds, and every variable bound past that
// lambda moved in by one
static size_t omega_substitute(const int *term, size_t at, int depth, size_t argument, OmegaTokens *out)
{
    const int token = term[at];
    if (token == OMEGA_LAMBDA)
    {
        omega_put(out, OMEGA_LAMBDA);
        return omega_substitute(term, at + 1u, depth + 1, argument, out);
    }
    if (token == OMEGA_APPLY)
    {
        omega_put(out, OMEGA_APPLY);
        const size_t right = omega_substitute(term, at + 1u, depth, argument, out);
        return omega_substitute(term, right, depth, argument, out);
    }
    if (token == (depth + 1))
    {
        (void)omega_lift(term, argument, 0, depth, out);
    }
    else
    {
        omega_put(out, (token > (depth + 1)) ? (token - 1) : token);
    }
    return at + 1u;
}

// the `count` tokens from `from` put at the end of the term
static void omega_appended(OmegaTokens *term, const int *from, size_t count)
{
    if (count > (term->room - term->count))
    {
        omega_refused("a term passes its room");
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = &term->token[term->count], .src = from, .bytes = count * sizeof(int));
    term->count += count;
}

// the term made a copy of `from`
static void omega_copied(OmegaTokens *term, const OmegaTokens *from)
{
    term->count = 0u;
    omega_appended(term, from->token, from->count);
}

// One normal order step. The leftmost outermost redex is the first application whose left part is a lambda, in the
// order of the code. The answer is 0 at a normal form
static int omega_step(const OmegaTokens *term, OmegaTokens *next)
{
    for (size_t at = 0u; (at + 1u) < term->count; at += 1u)
    {
        if ((term->token[at] == OMEGA_APPLY) && (term->token[at + 1u] == OMEGA_LAMBDA))
        {
            const size_t argument = omega_end(term->token, at + 1u);
            const size_t after = omega_end(term->token, argument);
            next->count = 0u;
            omega_appended(next, term->token, at);
            (void)omega_substitute(term->token, at + 2u, 0, argument, next);
            omega_appended(next, &term->token[after], term->count - after);
            return 1;
        }
    }
    return 0;
}

// the leading applications of the term
static unsigned int omega_applications(const OmegaTokens *term)
{
    unsigned int range = 0u;
    while ((range < term->count) && (term->token[range] == OMEGA_APPLY))
    {
        range += 1u;
    }
    return range;
}

// A term's spine: k leading applications, then the head, then its k arguments. The spine subterm at position a
// (the term less its last a arguments) runs from token a to ends[a], and `ends` holds k + 1 of them. Returns k.
static unsigned int omega_spine(const OmegaTokens *term, size_t *ends)
{
    const unsigned int range = omega_applications(term);
    size_t end = omega_end(term->token, range);
    ends[range] = end;
    for (unsigned int position = range; position > 0u; position -= 1u)
    {
        end = omega_end(term->token, end);
        ends[position - 1u] = end;
    }
    return range;
}

static unsigned int omega_head_redex(const OmegaTokens *term)
{
    const unsigned int range = omega_applications(term);
    return ((range > 0u) && (range < term->count) && (term->token[range] == OMEGA_LAMBDA)) ? (range - 1u)
                                                                                           : OMEGA_NOT_HEAD;
}

// Proven growth without end. Since the checkpoint every step was a head step whose redex sat at spine position
// `least` or deeper. The steps reduced only the checkpoint's spine subterm S at any position a <= least. S never
// became a lambda that eats an argument outside it.
//
// Where S now is at a deeper spine position of the term, head steps reduced S to S B for some arguments B. The same
// steps then take S B to S B' B and on without end. Here X ->h Y gives X Z ->h Y Z while X is no lambda. S has no head
// normal form. Neither has the term, and normal order, which reduces the head first, never halts.
static int omega_grows_forever(const OmegaTokens *stored, const size_t *stored_ends, unsigned int stored_range,
                               const OmegaTokens *term, unsigned int least, size_t *ends)
{
    const unsigned int range = omega_spine(term, ends);
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

// 1 where the two terms are the same tokens
static int omega_same(const OmegaTokens *left, const OmegaTokens *right)
{
    return (left->count == right->count) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = left->token, .other = right->token,
                       .bytes = left->count * sizeof(int)) == 0);
}

// Normal order under a step and a size budget, watched by Brent's cycle finder and the growth proof above. The call
// writes the steps it took to `taken`, and on a halt the rewrite's term holds the normal form
OmegaFate omega_run(OmegaRewrite *rewrite, unsigned int steps, unsigned int tokens, unsigned int *taken)
{
    OmegaTokens *term = &rewrite->term;
    OmegaTokens *next = &rewrite->next;
    OmegaTokens *const stored = &rewrite->stored;
    omega_copied(stored, term);
    unsigned int stored_range = omega_spine(stored, rewrite->stored_ends);
    // the least spine position of a head redex since the checkpoint; OMEGA_NOT_HEAD once a step was not a head step
    unsigned int least = OMEGA_NOT_HEAD - 1u;
    unsigned int power = 1u;
    unsigned int since = 0u;
    *taken = steps;
    OmegaFate fate = OMEGA_OPEN;
    unsigned int step = 0u;
    for (; step < steps; step += 1u)
    {
        const unsigned int redex = omega_head_redex(term);
        if (omega_step(term, next) == 0)
        {
            *taken = step;
            fate = OMEGA_HALTS;
            break;
        }
        least = ((redex == OMEGA_NOT_HEAD) || (least == OMEGA_NOT_HEAD)) ? OMEGA_NOT_HEAD
                                                                         : ((redex < least) ? redex : least);
        OmegaTokens *const swapped = term;
        term = next;
        next = swapped;
        if (omega_same(term, stored) != 0)
        {
            fate = OMEGA_LOOPS;
            break;
        }
        if ((least != OMEGA_NOT_HEAD) &&
            (omega_grows_forever(stored, rewrite->stored_ends, stored_range, term, least, rewrite->ends) != 0))
        {
            fate = OMEGA_DIVERGES;
            break;
        }
        if (term->count > (size_t)tokens)
        {
            // outgrew its space: no bounded search closes this exit
            fate = OMEGA_GREW;
            break;
        }
        since += 1u;
        if (since == power)
        {
            omega_copied(stored, term);
            stored_range = omega_spine(stored, rewrite->stored_ends);
            least = OMEGA_NOT_HEAD - 1u;
            power *= 2u;
            since = 0u;
        }
    }
    if (step == steps)
    {
        fate = (omega_step(term, next) == 0) ? OMEGA_HALTS : OMEGA_OPEN;
    }
    // the last term goes back to the rewrite's term, where the caller reads it
    if (term != &rewrite->term)
    {
        const OmegaTokens held = rewrite->term;
        rewrite->term = rewrite->next;
        rewrite->next = held;
    }
    return fate;
}

// The text of the file at `path`, read once into one cell of the temporary tier with a terminator past it, and its
// length into `length`. NULL where the system cannot read the file [RETURNS OWNERSHIP]
char *omega_text_read(const char *path, size_t *length)
{
    *length = 0u;
    unsigned long long size = 0ULL;
    if (platform_file_size(path, &size) == 0)
    {
        return NULL;
    }
    char *const text = (char *)omega_taken(size + 1ULL, 1u, 1);
    unsigned long long read = 0ULL;
    if (platform_file_read(path, text, size, &read) == 0)
    {
        omega_given(text);
        return NULL;
    }
    text[read] = '\0';
    *length = (size_t)read;
    return text;
}

// the ends of the lines in the text
size_t omega_text_lines(const char *text, size_t length)
{
    size_t lines = 0u;
    size_t at = 0u;
    const char *newline = EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = text, .cap = length, .byte = '\n');
    while (newline != NULL)
    {
        lines += 1u;
        at = (size_t)(newline - text) + 1u;
        newline = EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = &text[at], .cap = length - at, .byte = '\n');
    }
    return lines;
}

// The next whole line of the text from `at`, its end included and a terminator after it, into the `room` bytes at
// `line`, and `at` past it. The end of the line is one '\n'. A "\r\n" end, as a file in text mode on Windows writes
// it, comes back as one '\n'. A line that passes the room comes back as the empty text. The answer is 0 where no
// whole line remains
int omega_text_line(const char *text, size_t length, size_t *at, char *line, size_t room)
{
    if (*at >= length)
    {
        return 0;
    }
    const char *const newline =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = &text[*at], .cap = length - *at, .byte = '\n');
    if (newline == NULL)
    {
        return 0;
    }
    const size_t taken = (size_t)(newline - &text[*at]) + 1u;
    // the bytes of the line before its end, with no '\r' before the '\n'
    const size_t body = ((taken > 1u) && (newline[-1] == '\r')) ? (taken - 2u) : (taken - 1u);
    if ((body + 1u) < room)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = line, .src = &text[*at], .bytes = body);
        line[body] = '\n';
        line[body + 1u] = '\0';
    }
    else
    {
        line[0] = '\0';
    }
    *at += taken;
    return 1;
}
