// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_effect_value.c: the integer a condition's leaves give, read by the roles their grammar names: numbers in the
// base their leaves name, operators by how tightly they bind, `defined`, and names as the values of the macros they
// name, a macro called with arguments expanded by the roles of its value's leaves, brackets and separators by the
// roles their grammar names, and each operator doing the operation its grammar names. Nothing of a language is read
// here but by its plugin's grammar
#include "../command_line/cells.h"
#include "../command_line/file_writing.h"
#include "../text/text.h"
#include "symbols_internal.h"

// the most arguments of one call read, and the room a call's leaves and joined texts are begun with
#define CLI_EFFECT_ARGUMENTS 64u
#define CLI_EFFECT_CALLED_FIRST 64u
#define CLI_EFFECT_JOINED_FIRST 256u

// A condition being read: the walk, its leaves, how many, the one at hand, and how many macros deep the reading is
typedef struct
{
    CliEffect *effect;
    const CliEffectToken *token;
    unsigned int count;
    unsigned int at;
    unsigned int through;
} CliEffectReading;

static long long cli_effect_condition(CliEffectReading *reading);

// whether the leaf at hand plays every role of `roles`, CLI_LEAF_ bits its grammar names
static int cli_effect_at(const CliEffectReading *reading, unsigned long long roles)
{
    return (reading->at < reading->count) && ((reading->token[reading->at].is & roles) == roles);
}

// the number a leaf's digits write in the base its leaf names, a letter that is no digit of that base passed over
static long long cli_effect_number(const CliEffectToken *token)
{
    unsigned long long value = 0ull;
    for (unsigned long long at = 0ull; at < token->length; at += 1ull)
    {
        const char letter = token->text[at];
        const unsigned long long digit =
            ((letter >= '0') && (letter <= '9'))   ? (unsigned long long)(letter - '0')
            : ((letter >= 'a') && (letter <= 'z')) ? (unsigned long long)(letter - 'a' + 10)
            : ((letter >= 'A') && (letter <= 'Z')) ? (unsigned long long)(letter - 'A' + 10)
                                                   : 64ull;
        if (digit < token->base)
        {
            value = (value * token->base) + digit;
        }
    }
    // a condition's integer is read as the widest signed one, its high bit its sign
    return (long long)value;
}

// a cell of `count` leaves read from the walk's value leaves from `first`, their texts the walk's; NULL where the cells
// cannot meet it
static CliEffectToken *cli_effect_tokens_of(const CliEffect *effect, unsigned int first, unsigned int count)
{
    CliEffectToken *const tokens =
        cli_lane_alloc((unsigned long long)(count + 1u) * sizeof(CliEffectToken), &g_cli_code_cells);
    for (unsigned int at = 0u; (tokens != NULL) && (at < count); at += 1u)
    {
        const CliEffectValueLeaf *const leaf = &effect->value_leaf[first + at];
        tokens[at] = (CliEffectToken){
            cli_effect_text(effect, leaf->text), leaf->length, leaf->is, leaf->binds, leaf->base, leaf->does};
    }
    return tokens;
}

// The leaves a call's value is made of as it is expanded: in a cell whose room doubles as they are put, how many and
// the room; and the texts of leaves joined, a growing line whose cell moves as it grows, and a joined leaf's text is
// read from the line once every leaf is put
typedef struct
{
    CliEffectToken *token;
    unsigned int count;
    unsigned int room;
    VerbaLinea joined;
    unsigned int *joined_at;
    int refused;
} CliEffectCalled;

// `token` put after the call's leaves; `joined_at` its joined text's place, or 0xffffffff for a text of its own
static void cli_effect_called_put(CliEffectCalled *called, const CliEffectToken *token, unsigned int joined_at)
{
    if (called->count == called->room)
    {
        const unsigned int room = (called->room == 0u) ? CLI_EFFECT_CALLED_FIRST : (2u * called->room);
        CliEffectToken *const grown =
            g_cli_cells->persistent_buf_alloc((unsigned long long)room * sizeof(CliEffectToken), &g_cli_code_cells);
        unsigned int *const places =
            (grown != NULL)
                ? g_cli_cells->persistent_buf_alloc((unsigned long long)room * sizeof(unsigned int), &g_cli_code_cells)
                : NULL;
        if (places == NULL)
        {
            cli_cell_release(grown, &g_cli_code_cells);
            called->refused = 1;
            return;
        }
        memor.cpy(&(MemoriaCfg){.dst = grown, .src = called->token, .bytes = called->count * sizeof(CliEffectToken)});
        memor.cpy(
            &(MemoriaCfg){.dst = places, .src = called->joined_at, .bytes = called->count * sizeof(unsigned int)});
        cli_cell_release(called->token, &g_cli_code_cells);
        cli_cell_release(called->joined_at, &g_cli_code_cells);
        called->token = grown;
        called->joined_at = places;
        called->room = room;
    }
    called->token[called->count] = *token;
    called->joined_at[called->count] = joined_at;
    called->count += 1u;
}

// the leaf `token` joined to the call's last leaf: their texts one after the other made a text of its own, with the
// last leaf's role
static void cli_effect_called_joined(CliEffectCalled *called, const CliEffectToken *token)
{
    if (called->count == 0u)
    {
        cli_effect_called_put(called, token, 0xffffffffu);
        return;
    }
    CliEffectToken *const last = &called->token[called->count - 1u];
    const unsigned int was = called->joined_at[called->count - 1u];
    const char *const before = (was == 0xffffffffu) ? last->text : (called->joined.out + was);
    // a call's joined texts are far under 4 GiB
    const unsigned int at = (unsigned int)called->joined.at;
    // the last leaf's text copied first, as the line it may stand in moves as it grows
    char before_text[CLI_PATH_MAX_LEN];
    const unsigned long long length = (last->length < sizeof(before_text)) ? last->length : (sizeof(before_text) - 1u);
    memor.cpy(&(MemoriaCfg){.dst = before_text, .src = before, .bytes = (size_t)length});
    mmgr_line_put_n(&called->joined, before_text, length);
    mmgr_line_put_n(&called->joined, token->text, token->length);
    called->refused |= !mmgr_line_ok(&called->joined);
    last->length = length + token->length;
    called->joined_at[called->count - 1u] = at;
}

// the arguments of a call, its bracket the leaf at hand: each the leaves between separators at its depth, up to the
// bracket that closes it, `first` and `end` each argument's first leaf and the one after its last; how many, the
// reading moved past the closing bracket, or CLI_EFFECT_ARGUMENTS + 1 where the bracket is not closed
static unsigned int cli_effect_arguments(CliEffectReading *reading, unsigned int *first, unsigned int *end)
{
    reading->at += 1u;
    unsigned int arguments = 0u;
    unsigned int depth = 0u;
    unsigned int from = reading->at;
    for (; reading->at < reading->count; reading->at += 1u)
    {
        const int opens = cli_effect_at(reading, CLI_LEAF_OPEN);
        const int closes = cli_effect_at(reading, CLI_LEAF_CLOSE);
        if ((closes && (depth == 0u)) || ((depth == 0u) && cli_effect_at(reading, CLI_LEAF_SEPARATOR)))
        {
            if (arguments < CLI_EFFECT_ARGUMENTS)
            {
                first[arguments] = from;
                end[arguments] = reading->at;
                arguments += 1u;
            }
            from = reading->at + 1u;
            if (closes)
            {
                reading->at += 1u;
                // a call with no leaf between its brackets has no argument
                return ((arguments == 1u) && (first[0] == end[0])) ? 0u : arguments;
            }
            continue;
        }
        depth = opens ? (depth + 1u) : (closes ? (depth - 1u) : depth);
    }
    return CLI_EFFECT_ARGUMENTS + 1u;
}

// the parameter of the macro `macro` whose name is `token`'s text: its place among the parameters, or
// CLI_EFFECT_ARGUMENTS where it names none
static unsigned int cli_effect_parameter(const CliEffect *effect, const CliEffectMacro *macro,
                                         const CliEffectToken *token)
{
    const char *name = cli_effect_text(effect, macro->params);
    for (unsigned int at = 0u; at < macro->param_count; at += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
        if ((length == token->length) && cli_text_same(name, token->text, length))
        {
            return at;
        }
        name += length + 1u;
    }
    return CLI_EFFECT_ARGUMENTS;
}

// the value of a call of the macro `macro`, its bracket the leaf at hand: the macro's value with each name of a
// parameter made the leaves of the argument at its place, a leaf named `rest` made the arguments past the parameters
// with the leaves between them, a leaf named `join` joining the leaves about it into one, and one named `quote` making
// the leaf after it a string; read as a condition one macro deeper
static long long cli_effect_called(CliEffectReading *reading, unsigned int macro_at)
{
    CliEffect *const effect = reading->effect;
    unsigned int first[CLI_EFFECT_ARGUMENTS];
    unsigned int end[CLI_EFFECT_ARGUMENTS];
    const unsigned int arguments = cli_effect_arguments(reading, first, end);
    const CliEffectMacro macro = effect->macro[macro_at];
    CliEffectToken *const value = cli_effect_tokens_of(effect, macro.first, macro.count);
    CliEffectCalled called = {NULL, 0u, 0u, {NULL, 0ull, 0ull, NULL, NULL, 0ull}, NULL, 0};
    if ((arguments > CLI_EFFECT_ARGUMENTS) || (value == NULL) ||
        !cli_growing_line_made(&called.joined, CLI_EFFECT_JOINED_FIRST, &g_cli_code_cells))
    {
        cli_cell_release(value, &g_cli_code_cells);
        cli_cell_release(called.joined.out, &g_cli_code_cells);
        return 0ll;
    }
    int joining = 0;
    int quoting = 0;
    for (unsigned int at = 0u; !called.refused && (at < macro.count); at += 1u)
    {
        const CliEffectToken *const leaf = &value[at];
        if ((leaf->is & CLI_LEAF_JOIN) != 0u)
        {
            joining = 1;
            continue;
        }
        if ((leaf->is & CLI_LEAF_QUOTE) != 0u)
        {
            quoting = 1;
            continue;
        }
        const unsigned int param =
            ((leaf->is & CLI_LEAF_NAME) != 0u) ? cli_effect_parameter(effect, &macro, leaf) : CLI_EFFECT_ARGUMENTS;
        const int rest = macro.rest && ((leaf->is & CLI_LEAF_REST) != 0u);
        // the leaves this one stands for: the argument at the parameter's place, the arguments past them with the
        // commas between, or itself
        unsigned int from = 0u;
        unsigned int to = 1u;
        const CliEffectToken *source = leaf;
        if ((param < arguments) || (rest && (macro.param_count < arguments)))
        {
            source = reading->token;
            from = first[rest ? macro.param_count : param];
            to = end[rest ? (arguments - 1u) : param];
        }
        else if ((param != CLI_EFFECT_ARGUMENTS) || rest)
        {
            // a parameter no argument is given for, or the arguments past the parameters where none is, stands for
            // no leaf
            continue;
        }
        if (quoting)
        {
            // a quoted argument is a string, whose value in a condition is none
            const CliEffectToken string = {"", 0ull, CLI_LEAF_STRING, 0u, 10u, 0u};
            cli_effect_called_put(&called, &string, 0xffffffffu);
            quoting = 0;
            joining = 0;
            continue;
        }
        for (unsigned int each = from; each < to; each += 1u)
        {
            if (joining && (each == from))
            {
                cli_effect_called_joined(&called, &source[each]);
                joining = 0;
                continue;
            }
            cli_effect_called_put(&called, &source[each], 0xffffffffu);
        }
    }
    // each joined leaf's text read from the line, which has stopped moving
    for (unsigned int at = 0u; at < called.count; at += 1u)
    {
        called.token[at].text =
            (called.joined_at[at] == 0xffffffffu) ? called.token[at].text : (called.joined.out + called.joined_at[at]);
    }
    const long long result =
        called.refused ? 0ll : cli_effect_value(effect, called.token, called.count, reading->through + 1u);
    cli_cell_release(called.token, &g_cli_code_cells);
    cli_cell_release(called.joined_at, &g_cli_code_cells);
    cli_cell_release(called.joined.out, &g_cli_code_cells);
    cli_cell_release(value, &g_cli_code_cells);
    return result;
}

// the value of the macro the name at `token` names: its value's leaves read as a condition one macro deeper, a macro
// called with arguments expanded where a bracket follows it and 0 where none does; 0 for a name of no macro, or where
// the reading is CLI_EFFECT_THROUGH macros deep
static long long cli_effect_named(CliEffectReading *reading, const CliEffectToken *token)
{
    CliEffect *const effect = reading->effect;
    const unsigned int macro = cli_effect_macro(effect, token->text, token->length);
    if ((macro == CLI_JSON_NONE) || (reading->through >= CLI_EFFECT_THROUGH))
    {
        return 0ll;
    }
    if (effect->macro[macro].called)
    {
        return cli_effect_at(reading, CLI_LEAF_OPEN) ? cli_effect_called(reading, macro) : 0ll;
    }
    CliEffectToken *const value = cli_effect_tokens_of(effect, effect->macro[macro].first, effect->macro[macro].count);
    const long long result =
        (value != NULL) ? cli_effect_value(effect, value, effect->macro[macro].count, reading->through + 1u) : 0ll;
    cli_cell_release(value, &g_cli_code_cells);
    return result;
}

// one value and the operators before it: a bracketed condition, `defined` with a name or a bracketed name, a number,
// or a name; an operator before a value applied to it as its symbol writes
static long long cli_effect_unary(CliEffectReading *reading)
{
    while ((reading->at < reading->count) && ((reading->token[reading->at].is & CLI_LEAF_PREFIX) != 0u))
    {
        reading->at += 1u;
    }
    if (reading->at >= reading->count)
    {
        return 0ll;
    }
    const CliEffectToken *const token = &reading->token[reading->at];
    if (((token->is & CLI_LEAF_UNARY) != 0u) || (token->binds != 0u))
    {
        reading->at += 1u;
        const long long value = cli_effect_unary(reading);
        // before a value, an operator that subtracts negates it
        return (token->does == CLI_DOES_NOT)          ? (long long)(value == 0ll)
               : (token->does == CLI_DOES_COMPLEMENT) ? ~value
               : (token->does == CLI_DOES_SUBTRACT)   ? -value
                                                      : value;
    }
    if ((token->is & CLI_LEAF_OPEN) != 0u)
    {
        reading->at += 1u;
        const long long value = cli_effect_condition(reading);
        reading->at += cli_effect_at(reading, CLI_LEAF_CLOSE) ? 1u : 0u;
        return value;
    }
    reading->at += 1u;
    if ((token->is & CLI_LEAF_DEFINED) != 0u)
    {
        const int bracketed = cli_effect_at(reading, CLI_LEAF_OPEN);
        reading->at += bracketed ? 1u : 0u;
        const CliEffectToken *const name = (reading->at < reading->count) ? &reading->token[reading->at] : NULL;
        reading->at += (name != NULL) ? 1u : 0u;
        reading->at += (bracketed && cli_effect_at(reading, CLI_LEAF_CLOSE)) ? 1u : 0u;
        return (name != NULL) && (cli_effect_macro(reading->effect, name->text, name->length) != CLI_JSON_NONE);
    }
    if ((token->is & CLI_LEAF_NUMBER) != 0u)
    {
        // a number's suffix is a leaf of its own after it
        while ((reading->at < reading->count) && ((reading->token[reading->at].is & CLI_LEAF_NUMBER) != 0u))
        {
            reading->at += 1u;
        }
        return cli_effect_number(token);
    }
    return ((token->is & (CLI_LEAF_NAME | CLI_LEAF_KEYWORD | CLI_LEAF_TYPE)) != 0u) ? cli_effect_named(reading, token)
                                                                                    : 0ll;
}

// the values joined by the operators that bind at least as tightly as `least`, each read with those that bind more
// tightly first, each operator doing the operation its grammar names, 0 for one it names none of
static long long cli_effect_binary(CliEffectReading *reading, unsigned int least)
{
    long long left = cli_effect_unary(reading);
    while ((reading->at < reading->count) && (reading->token[reading->at].binds >= least) &&
           (reading->token[reading->at].binds != 0u))
    {
        const CliEffectToken *const joined = &reading->token[reading->at];
        reading->at += 1u;
        const long long right = cli_effect_binary(reading, joined->binds + 1u);
        // a shift past a long long's bits gives 0
        const int shifts = (right >= 0ll) && (right < 63ll);
        const unsigned char does = joined->does;
        left = (does == CLI_DOES_MULTIPLY)     ? (left * right)
               : (does == CLI_DOES_DIVIDE)     ? ((right != 0ll) ? (left / right) : 0ll)
               : (does == CLI_DOES_REMAINDER)  ? ((right != 0ll) ? (left % right) : 0ll)
               : (does == CLI_DOES_ADD)        ? (left + right)
               : (does == CLI_DOES_SUBTRACT)   ? (left - right)
               : (does == CLI_DOES_SHIFTLEFT)  ? (shifts ? (left << right) : 0ll)
               : (does == CLI_DOES_SHIFTRIGHT) ? (shifts ? (left >> right) : 0ll)
               : (does == CLI_DOES_LESS)       ? (long long)(left < right)
               : (does == CLI_DOES_ATMOST)     ? (long long)(left <= right)
               : (does == CLI_DOES_GREATER)    ? (long long)(left > right)
               : (does == CLI_DOES_ATLEAST)    ? (long long)(left >= right)
               : (does == CLI_DOES_EQUAL)      ? (long long)(left == right)
               : (does == CLI_DOES_UNEQUAL)    ? (long long)(left != right)
               : (does == CLI_DOES_AND)        ? (long long)((left != 0ll) && (right != 0ll))
               : (does == CLI_DOES_OR)         ? (long long)((left != 0ll) || (right != 0ll))
               : (does == CLI_DOES_BITAND)     ? (left & right)
               : (does == CLI_DOES_BITXOR)     ? (left ^ right)
               : (does == CLI_DOES_BITOR)      ? (left | right)
                                               : 0ll;
    }
    return left;
}

// a whole condition: the values and operators, and a leaf named `choice` after them choosing between the condition
// after it, to the leaf named `otherwise` that closes it, and the condition after that one
static long long cli_effect_condition(CliEffectReading *reading)
{
    const long long chosen = cli_effect_binary(reading, 1u);
    const unsigned long long is = (reading->at < reading->count) ? reading->token[reading->at].is : 0ull;
    if (((is & CLI_LEAF_CHOICE) == 0u) || ((is & CLI_LEAF_OTHERWISE) != 0u))
    {
        return chosen;
    }
    reading->at += 1u;
    const long long then = cli_effect_condition(reading);
    reading->at +=
        ((reading->at < reading->count) && ((reading->token[reading->at].is & CLI_LEAF_OTHERWISE) != 0u)) ? 1u : 0u;
    const long long otherwise = cli_effect_condition(reading);
    return (chosen != 0ll) ? then : otherwise;
}

long long cli_effect_value(CliEffect *effect, const CliEffectToken *token, unsigned int count, unsigned int through)
{
    CliEffectReading reading = {effect, token, count, 0u, through};
    return cli_effect_condition(&reading);
}
