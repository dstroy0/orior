// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// omega_computer_machine.cu: rewriting, the machine and the run
#include "memoria_operor/memoria_operor.h"
#include "omega_computer_internal.h"

// The rewrite's tables: two terms that take turns as the term and its next step, and the term that Brent's watcher
// stores. Also the ends of the two spines. Each is one cell of the persistent tier at OMEGA_COMPUTER_TERM_MAX, as the
// sim runs thousands of rewrites
static OmegaComputerTerm s_rewrite_terms[2];
static OmegaComputerTerm s_rewrite_stored;
static size_t *s_rewrite_ends;
static size_t *s_rewrite_stored_ends;

static void omega_computer_term_taken(OmegaComputerTerm *term)
{
    term->token = (int *)omega_computer_taken(OMEGA_COMPUTER_TERM_MAX, sizeof(int));
    term->count = 0u;
    term->room = OMEGA_COMPUTER_TERM_MAX;
}

void omega_computer_rewrite_open(void)
{
    omega_computer_term_taken(&s_rewrite_terms[0]);
    omega_computer_term_taken(&s_rewrite_terms[1]);
    omega_computer_term_taken(&s_rewrite_stored);
    s_rewrite_ends = (size_t *)omega_computer_taken(OMEGA_COMPUTER_TERM_MAX + 1u, sizeof(size_t));
    s_rewrite_stored_ends = (size_t *)omega_computer_taken(OMEGA_COMPUTER_TERM_MAX + 1u, sizeof(size_t));
}

void omega_computer_rewrite_close(void)
{
    omega_computer_given(s_rewrite_stored_ends);
    omega_computer_given(s_rewrite_ends);
    omega_computer_given(s_rewrite_stored.token);
    omega_computer_given(s_rewrite_terms[1].token);
    omega_computer_given(s_rewrite_terms[0].token);
}

// the stored term made a copy of the term
static void omega_computer_stored(const OmegaComputerTerm *term)
{
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = s_rewrite_stored.token, .src = term->token,
               .bytes = term->count * sizeof(int));
    s_rewrite_stored.count = term->count;
}

// Normal order from `start` under a step and a size budget, watched by Brent's cycle finder and the growth proof.
// `result` names the last term, in the rewriting's tables until the next rewrite, and on a halt it is the normal form
OmegaComputerFate omega_computer_rewrite(const OmegaComputerTerm *start, unsigned int steps, unsigned int tokens,
                                         unsigned long long *taken, OmegaComputerTerm *result)
{
    OmegaComputerTerm *term = &s_rewrite_terms[0];
    OmegaComputerTerm *next = &s_rewrite_terms[1];
    term->count = 0u;
    if (start->count > term->room)
    {
        omega_computer_refused("a term passes its room");
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = term->token, .src = start->token, .bytes = start->count * sizeof(int));
    term->count = start->count;
    g_omega_computer_peak = term->count;
    omega_computer_stored(term);
    unsigned int stored_range = omega_computer_spine(&s_rewrite_stored, s_rewrite_stored_ends);
    unsigned int least = OMEGA_COMPUTER_NOT_HEAD - 1u;
    unsigned int power = 1u;
    unsigned int since = 0u;
    *taken = steps;
    OmegaComputerFate fate = OMEGA_COMPUTER_OPEN;
    unsigned int step = 0u;
    for (; step < steps; step += 1u)
    {
        const unsigned int redex = omega_computer_head_redex(term);
        if (omega_computer_step(term, next) == 0)
        {
            *taken = step;
            fate = OMEGA_COMPUTER_HALTS;
            break;
        }
        least = ((redex == OMEGA_COMPUTER_NOT_HEAD) || (least == OMEGA_COMPUTER_NOT_HEAD))
                    ? OMEGA_COMPUTER_NOT_HEAD
                    : ((redex < least) ? redex : least);
        OmegaComputerTerm *const swapped = term;
        term = next;
        next = swapped;
        g_omega_computer_peak = (term->count > g_omega_computer_peak) ? term->count : g_omega_computer_peak;
        if (omega_computer_same(term, &s_rewrite_stored) != 0)
        {
            fate = OMEGA_COMPUTER_LOOPS;
            break;
        }
        if ((least != OMEGA_COMPUTER_NOT_HEAD) &&
            (omega_computer_grows_forever(&s_rewrite_stored, s_rewrite_stored_ends, stored_range, term, least,
                                          s_rewrite_ends) != 0))
        {
            fate = OMEGA_COMPUTER_DIVERGES;
            break;
        }
        if (term->count > (size_t)tokens)
        {
            fate = OMEGA_COMPUTER_GREW;
            break;
        }
        since += 1u;
        if (since == power)
        {
            omega_computer_stored(term);
            stored_range = omega_computer_spine(&s_rewrite_stored, s_rewrite_stored_ends);
            least = OMEGA_COMPUTER_NOT_HEAD - 1u;
            power *= 2u;
            since = 0u;
        }
    }
    if (step == steps)
    {
        fate = (omega_computer_step(term, next) == 0) ? OMEGA_COMPUTER_HALTS : OMEGA_COMPUTER_OPEN;
    }
    *result = *term;
    return fate;
}

// The lazy machine: a node appended to the term arena
static int omega_computer_node(OmegaComputerMachine *machine, int type, int left, int right)
{
    if (machine->node_count == OMEGA_COMPUTER_NODES_MAX)
    {
        omega_computer_refused("the lazy machine's nodes pass their table");
    }
    OmegaComputerNode *const node = &machine->nodes[machine->node_count];
    node->type = type;
    node->left = left;
    node->right = right;
    machine->node_count += 1u;
    // the arena holds at max OMEGA_COMPUTER_NODES_MAX nodes, far below 2^31
    return (int)(machine->node_count - 1u);
}

// the tokens from `at` as nodes; returns the root and leaves `at` one past the term
static int omega_computer_compile(OmegaComputerMachine *machine, const OmegaComputerTerm *term, size_t *at)
{
    const int token = term->token[*at];
    *at += 1u;
    if (token == OMEGA_COMPUTER_LAMBDA)
    {
        const int body = omega_computer_compile(machine, term, at);
        return omega_computer_node(machine, OMEGA_COMPUTER_NODE_LAMBDA, body, 0);
    }
    if (token == OMEGA_COMPUTER_APPLY)
    {
        const int function = omega_computer_compile(machine, term, at);
        const int argument = omega_computer_compile(machine, term, at);
        return omega_computer_node(machine, OMEGA_COMPUTER_NODE_APPLY, function, argument);
    }
    return omega_computer_node(machine, OMEGA_COMPUTER_NODE_VARIABLE, token, 0);
}

// the bits as Tromp's list, built from its end. The list shares the true, the false and the variable nodes
static int omega_computer_list(OmegaComputerMachine *machine, const char *bits, size_t count)
{
    int rest = machine->false_node;
    for (size_t at = count; at > 0u; at -= 1u)
    {
        const int bit = (bits[at - 1u] == '0') ? machine->true_node : machine->false_node;
        const int applied = omega_computer_node(machine, OMEGA_COMPUTER_NODE_APPLY, machine->variable_node, bit);
        const int pair = omega_computer_node(machine, OMEGA_COMPUTER_NODE_APPLY, applied, rest);
        rest = omega_computer_node(machine, OMEGA_COMPUTER_NODE_LAMBDA, pair, 0);
    }
    return rest;
}

void omega_computer_machine_open(OmegaComputerMachine *machine, const OmegaComputerTerm *universal)
{
    const unsigned long long arena = OMEGA_COMPUTER_CELLS_MAX + OMEGA_COMPUTER_CELLS_SPARE;
    machine->thunks = (OmegaComputerThunk *)omega_computer_taken(arena, sizeof(OmegaComputerThunk));
    machine->environments = (OmegaComputerCell *)omega_computer_taken(arena, sizeof(OmegaComputerCell));
    machine->spines = (OmegaComputerCell *)omega_computer_taken(arena, sizeof(OmegaComputerCell));
    machine->stack = (int *)omega_computer_taken(OMEGA_COMPUTER_CELLS_MAX, sizeof(int));
    machine->work = (OmegaComputerWork *)omega_computer_taken(OMEGA_COMPUTER_CELLS_MAX, sizeof(OmegaComputerWork));
    machine->arguments = (int *)omega_computer_taken(OMEGA_COMPUTER_CELLS_MAX, sizeof(int));
    machine->thunk_count = 0u;
    machine->environment_count = 0u;
    machine->spine_count = 0u;
    machine->stack_count = 0u;
    machine->work_count = 0u;
    machine->argument_count = 0u;
    machine->node_count = 0u;
    machine->variable_node = omega_computer_node(machine, OMEGA_COMPUTER_NODE_VARIABLE, 1, 0);
    const int second = omega_computer_node(machine, OMEGA_COMPUTER_NODE_VARIABLE, 2, 0);
    machine->true_node = omega_computer_node(machine, OMEGA_COMPUTER_NODE_LAMBDA,
                                             omega_computer_node(machine, OMEGA_COMPUTER_NODE_LAMBDA, second, 0), 0);
    machine->false_node =
        omega_computer_node(machine, OMEGA_COMPUTER_NODE_LAMBDA,
                            omega_computer_node(machine, OMEGA_COMPUTER_NODE_LAMBDA, machine->variable_node, 0), 0);
    size_t at = 0u;
    machine->universal_node = omega_computer_compile(machine, universal, &at);
    machine->fixed_nodes = machine->node_count;
}

void omega_computer_machine_close(OmegaComputerMachine *machine)
{
    omega_computer_given(machine->arguments);
    omega_computer_given(machine->work);
    omega_computer_given(machine->stack);
    omega_computer_given(machine->spines);
    omega_computer_given(machine->environments);
    omega_computer_given(machine->thunks);
}

static int omega_computer_cells_left(const OmegaComputerMachine *machine)
{
    return (machine->thunk_count + machine->environment_count + machine->spine_count) <
           (size_t)OMEGA_COMPUTER_CELLS_MAX;
}

static int omega_computer_thunk(OmegaComputerMachine *machine, int term, int environment)
{
    OmegaComputerThunk *const thunk = &machine->thunks[machine->thunk_count];
    thunk->term = term;
    thunk->environment = environment;
    thunk->state = OMEGA_COMPUTER_UNEVALUATED;
    thunk->neutral = 0;
    thunk->value = 0;
    thunk->value_cells = -1;
    machine->thunk_count += 1u;
    // the cells stay below OMEGA_COMPUTER_CELLS_MAX + OMEGA_COMPUTER_CELLS_SPARE, 2^25 + 4
    return (int)(machine->thunk_count - 1u);
}

static int omega_computer_cell(OmegaComputerCell *cells, size_t *count, int thunk, int next)
{
    cells[*count].thunk = thunk;
    cells[*count].next = next;
    *count += 1u;
    // the cells stay below OMEGA_COMPUTER_CELLS_MAX + OMEGA_COMPUTER_CELLS_SPARE, 2^25 + 4
    return (int)(*count - 1u);
}

// a value pushed on a table of the machine that holds OMEGA_COMPUTER_CELLS_MAX of them
static void omega_computer_pushed(int *table, size_t *count, int value)
{
    if (*count == (size_t)OMEGA_COMPUTER_CELLS_MAX)
    {
        omega_computer_refused("a table of the lazy machine passes its bound");
    }
    table[*count] = value;
    *count += 1u;
}

// the thunk the variable `index` (from 1) names in the environment
static int omega_computer_lookup(const OmegaComputerMachine *machine, int environment, int index)
{
    int cell = environment;
    for (int step = 1; step < index; step += 1)
    {
        cell = machine->environments[(size_t)cell].next;
    }
    return machine->environments[(size_t)cell].thunk;
}

// The thunk to its weak head normal form, by Krivine's machine with Sestoft's update frames. The stack stores
// arguments (a thunk, shifted up one) and updates (a thunk, shifted up one, with the low bit set).
//
// A lambda that meets an argument is a beta step. A lambda that meets an update becomes the value of that thunk. A
// variable whose thunk has its value continues as that value. The machine enters a thunk that it did not evaluate
// under an update. A thunk that the machine already entered needs itself to make its own value. It never has one, and
// nor has the term.
static OmegaComputerFate omega_computer_whnf(OmegaComputerMachine *machine, int root)
{
    if (machine->thunks[(size_t)root].state == OMEGA_COMPUTER_EVALUATED)
    {
        return OMEGA_COMPUTER_HALTS;
    }
    if (machine->thunks[(size_t)root].state == OMEGA_COMPUTER_ENTERED)
    {
        return OMEGA_COMPUTER_LOOPS;
    }
    int *const stack = machine->stack;
    machine->stack_count = 0u;
    omega_computer_pushed(stack, &machine->stack_count, (root << 1) | 1);
    machine->thunks[(size_t)root].state = OMEGA_COMPUTER_ENTERED;
    int term = machine->thunks[(size_t)root].term;
    int environment = machine->thunks[(size_t)root].environment;
    for (;;)
    {
        const OmegaComputerNode node = machine->nodes[(size_t)term];
        if (node.type == OMEGA_COMPUTER_NODE_APPLY)
        {
            if (omega_computer_cells_left(machine) == 0)
            {
                return OMEGA_COMPUTER_GREW;
            }
            const OmegaComputerNode argument = machine->nodes[(size_t)node.right];
            // a variable argument shares the thunk it names. No chain of thunks forms
            const int thunk = (argument.type == OMEGA_COMPUTER_NODE_VARIABLE)
                                  ? omega_computer_lookup(machine, environment, argument.left)
                                  : omega_computer_thunk(machine, node.right, environment);
            omega_computer_pushed(stack, &machine->stack_count, thunk << 1);
            term = node.left;
            continue;
        }
        if (node.type == OMEGA_COMPUTER_NODE_LAMBDA)
        {
            const int top = stack[machine->stack_count - 1u];
            if ((top & 1) == 0)
            {
                if (machine->steps >= machine->step_budget)
                {
                    return OMEGA_COMPUTER_OPEN;
                }
                if (omega_computer_cells_left(machine) == 0)
                {
                    return OMEGA_COMPUTER_GREW;
                }
                machine->stack_count -= 1u;
                environment =
                    omega_computer_cell(machine->environments, &machine->environment_count, top >> 1, environment);
                term = node.left;
                machine->steps += 1ULL;
                continue;
            }
            OmegaComputerThunk *const updated = &machine->thunks[(size_t)(top >> 1)];
            updated->state = OMEGA_COMPUTER_EVALUATED;
            updated->neutral = 0;
            updated->value = term;
            updated->value_cells = environment;
            machine->stack_count -= 1u;
            if (machine->stack_count == 0u)
            {
                return OMEGA_COMPUTER_HALTS;
            }
            continue;
        }
        const int named = omega_computer_lookup(machine, environment, node.left);
        const OmegaComputerThunk thunk = machine->thunks[(size_t)named];
        if (thunk.state == OMEGA_COMPUTER_UNEVALUATED)
        {
            machine->thunks[(size_t)named].state = OMEGA_COMPUTER_ENTERED;
            omega_computer_pushed(stack, &machine->stack_count, (named << 1) | 1);
            term = thunk.term;
            environment = thunk.environment;
            continue;
        }
        if (thunk.state == OMEGA_COMPUTER_ENTERED)
        {
            return OMEGA_COMPUTER_LOOPS;
        }
        if (thunk.neutral == 0)
        {
            term = thunk.value;
            environment = thunk.value_cells;
            continue;
        }
        // a variable applied: every argument on the stack joins its spine, and every update takes the spine so far
        int spine = thunk.value_cells;
        while (machine->stack_count != 0u)
        {
            const int entry = stack[machine->stack_count - 1u];
            machine->stack_count -= 1u;
            if ((entry & 1) == 0)
            {
                if (omega_computer_cells_left(machine) == 0)
                {
                    return OMEGA_COMPUTER_GREW;
                }
                spine = omega_computer_cell(machine->spines, &machine->spine_count, entry >> 1, spine);
                continue;
            }
            OmegaComputerThunk *const updated = &machine->thunks[(size_t)(entry >> 1)];
            updated->state = OMEGA_COMPUTER_EVALUATED;
            updated->neutral = 1;
            updated->value = thunk.value;
            updated->value_cells = spine;
        }
        return OMEGA_COMPUTER_HALTS;
    }
}

// an item pushed on the machine's work, which holds OMEGA_COMPUTER_CELLS_MAX of them
static void omega_computer_work_pushed(OmegaComputerMachine *machine, int thunk, int depth)
{
    if (machine->work_count == (size_t)OMEGA_COMPUTER_CELLS_MAX)
    {
        omega_computer_refused("the lazy machine's work passes its table");
    }
    machine->work[machine->work_count].thunk = thunk;
    machine->work[machine->work_count].depth = depth;
    machine->work_count += 1u;
}

// The normal form of the thunk, read back as tokens. To read back a lambda, the machine puts a fresh variable for its
// own and reads its body a lambda deeper. A variable applied to its spine reads back as the variable, then each
// argument in turn. The index of the variable comes from the depth and the level of its lambda. The machine first
// takes every thunk that it reads to its weak head normal form, which normal order needs too. This reaches the normal
// form wherever one exists.
static OmegaComputerFate omega_computer_normal(OmegaComputerMachine *machine, int root, OmegaComputerTerm *out)
{
    out->count = 0u;
    machine->work_count = 0u;
    omega_computer_work_pushed(machine, root, 0);
    while (machine->work_count != 0u)
    {
        const OmegaComputerWork item = machine->work[machine->work_count - 1u];
        machine->work_count -= 1u;
        const OmegaComputerFate fate = omega_computer_whnf(machine, item.thunk);
        if (fate != OMEGA_COMPUTER_HALTS)
        {
            return fate;
        }
        if (out->count > (size_t)OMEGA_COMPUTER_TOKENS_MAX)
        {
            return OMEGA_COMPUTER_GREW;
        }
        const OmegaComputerThunk thunk = machine->thunks[(size_t)item.thunk];
        if (omega_computer_cells_left(machine) == 0)
        {
            return OMEGA_COMPUTER_GREW;
        }
        if (thunk.neutral == 0)
        {
            omega_computer_put(out, OMEGA_COMPUTER_LAMBDA);
            const int fresh = omega_computer_thunk(machine, 0, -1);
            machine->thunks[(size_t)fresh].state = OMEGA_COMPUTER_EVALUATED;
            machine->thunks[(size_t)fresh].neutral = 1;
            machine->thunks[(size_t)fresh].value = item.depth;
            machine->thunks[(size_t)fresh].value_cells = -1;
            const int environment =
                omega_computer_cell(machine->environments, &machine->environment_count, fresh, thunk.value_cells);
            omega_computer_work_pushed(
                machine, omega_computer_thunk(machine, machine->nodes[(size_t)thunk.value].left, environment),
                item.depth + 1);
            continue;
        }
        machine->argument_count = 0u;
        for (int cell = thunk.value_cells; cell >= 0; cell = machine->spines[(size_t)cell].next)
        {
            omega_computer_pushed(machine->arguments, &machine->argument_count, machine->spines[(size_t)cell].thunk);
        }
        // the spine stores its last argument first
        for (size_t at = 0u; at < machine->argument_count; at += 1u)
        {
            omega_computer_put(out, OMEGA_COMPUTER_APPLY);
        }
        omega_computer_put(out, item.depth - thunk.value);
        for (size_t at = 0u; at < machine->argument_count; at += 1u)
        {
            omega_computer_work_pushed(machine, machine->arguments[at], item.depth);
        }
    }
    return OMEGA_COMPUTER_HALTS;
}

// The bits that one run of the lazy machine reads: U for each depth past the first, the program's code and its input
static char s_run_bits[OMEGA_COMPUTER_BITS_MAX];

// One run of the lazy machine: the program on its input bits `depth` machines deep, the direct run at depth 0
OmegaComputerFate omega_computer_run(OmegaComputerMachine *machine, const OmegaComputerTerm *program, const char *input,
                                     size_t input_count, unsigned int depth, const char *universal_code,
                                     size_t universal_count, OmegaComputerTerm *normal)
{
    machine->node_count = machine->fixed_nodes;
    machine->thunk_count = 0u;
    machine->environment_count = 0u;
    machine->spine_count = 0u;
    machine->steps = 0ULL;
    int root = 0;
    if (depth == 0u)
    {
        size_t at = 0u;
        const int function = omega_computer_compile(machine, program, &at);
        root = omega_computer_node(machine, OMEGA_COMPUTER_NODE_APPLY, function,
                                   omega_computer_list(machine, input, input_count));
    }
    else
    {
        OmegaComputerBits bits = {s_run_bits, 0u, sizeof(s_run_bits)};
        for (unsigned int copy = 1u; copy < depth; copy += 1u)
        {
            omega_computer_bits_put(&bits, universal_code, universal_count);
        }
        omega_computer_code(program, &bits);
        omega_computer_bits_put(&bits, input, input_count);
        root = omega_computer_node(machine, OMEGA_COMPUTER_NODE_APPLY, machine->universal_node,
                                   omega_computer_list(machine, bits.bit, bits.count));
    }
    return omega_computer_normal(machine, omega_computer_thunk(machine, root, -1), normal);
}
