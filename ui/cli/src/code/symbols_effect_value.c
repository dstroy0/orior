// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// symbols_effect_value.c: the integer a condition's leaves give, read by the roles their grammar names: numbers in the
// base their leaves name, operators by how tightly they bind, `defined`, and names as the values of the macros they
// name, a macro called with arguments expanded by the roles of its value's leaves, brackets and separators by the
// roles their grammar names, and each operator doing the operation its grammar names. Nothing of a language is read
// here but by its plugin's grammar
#include "../command_line/cells.h"
#include "../text/text.h"
#include "symbols_internal.h"

// A condition being read: the walk, its leaves, how many, the one at hand, and how many macros deep the reading is
typedef struct
{
    CliEffect *effect;
    const CliEffectToken *token;
    unsigned int count;
    unsigned int at;
    unsigned int through;
} CliEffectReadState;
// The leaves a call's value is made of as it is expanded, how many and the capacity; and the texts of leaves joined,
// whose places a joined leaf's text is read from once every leaf is put; all in one cell
typedef struct
{
    CliEffectToken *token;
    unsigned int count;
    unsigned int capacity;
    VerbaLinea joined;
    unsigned int *joined_at;
    int refused;
} CliEffectCalled;
// the most arguments of one call read, the most leaves one call's value is made of, and the bytes of its joined texts
#define CLI_EFFECT_ARGUMENTS 64u
#define CLI_EFFECT_CALLED_MAX 4096u
#define CLI_EFFECT_JOINED_CAPACITY (64ULL << 10u)

typedef struct
{
    CliEffectReadState *read_state;
    const CliEffectReadState *read_state_read;
    unsigned long long roles;
    const CliEffectToken *token;
    const CliEffect *effect;
    unsigned int first;
    unsigned int count;
    CliEffectCalled *called;
    unsigned int joined_at;
    unsigned int *first_pointer;
    unsigned int *end;
    const CliEffectMacro *macro;
    unsigned int macro_at;
    unsigned int min;
} SymbolsEffectValueCtx;
static long long cli_effect_condition(const SymbolsEffectValueCtx *args);

// whether the leaf at hand plays every role of `roles`, CLI_LEAF_ bits its grammar names
EMBED_INLINE int cli_effect_at(const SymbolsEffectValueCtx *args)
{
    unsigned long long roles = args->roles;
    return (args->read_state_read->at < args->read_state_read->count) &&
           ((args->read_state_read->token[args->read_state_read->at].is & roles) == roles);
}

// the number a leaf's digits write in the base its leaf names, a letter that is no digit of that base passed over
EMBED_INLINE long long cli_effect_number(const SymbolsEffectValueCtx *args)
{
    unsigned long long value = 0ULL;
    for (unsigned long long at = 0ULL; at < args->token->length; at += 1ULL)
    {
        const char letter = args->token->text[at];
        const unsigned long long digit =
            ((letter >= '0') && (letter <= '9'))   ? (unsigned long long)(letter - '0')
            : ((letter >= 'a') && (letter <= 'z')) ? (unsigned long long)(letter - 'a' + 10)
            : ((letter >= 'A') && (letter <= 'Z')) ? (unsigned long long)(letter - 'A' + 10)
                                                   : 64ULL;
        if (digit < args->token->base)
        {
            value = (value * args->token->base) + digit;
        }
    }
    // a condition's integer is read as the widest signed one, its high bit its sign
    return (long long)value;
}

// a cell of `count` leaves read from the walk's value leaves from `first`, their texts the walk's; NULL where the cells
// cannot meet it
EMBED_INLINE CliEffectToken *cli_effect_tokens_get(const SymbolsEffectValueCtx *args)
{
    const CliEffect *effect = args->effect;
    CliEffectToken *const tokens = g_cli_cells->persistent_buf_alloc(
        (unsigned long long)(args->count + 1u) * sizeof(CliEffectToken), &g_cli_code_cells);
    for (unsigned int at = 0u; (tokens != NULL) && (at < args->count); at += 1u)
    {
        const CliEffectValueLeaf *const leaf = &effect->value_leaf[args->first + at];
        tokens[at] = (CliEffectToken){
            EMBED_CALL(cli_symbols_internal.effect_text, CliSymbolsCtx, .effect_read = effect, .at_value = leaf->text),
            leaf->length,
            leaf->is,
            leaf->binds,
            leaf->base,
            leaf->does};
    }
    return tokens;
}

// `token` put after the call's leaves; `joined_at` its joined text's place, or 0xffffffff for a text of its own
EMBED_INLINE void cli_effect_called_append(const SymbolsEffectValueCtx *args)
{
    if (args->called->count >= args->called->capacity)
    {
        args->called->refused = 1;
        return;
    }
    args->called->token[args->called->count] = *args->token;
    args->called->joined_at[args->called->count] = args->joined_at;
    args->called->count += 1u;
}

// the leaf `token` joined to the call's last leaf: their texts one after the other made a text of its own, with the
// last leaf's role
EMBED_INLINE void cli_effect_called_join(const SymbolsEffectValueCtx *args)
{
    if (args->called->count == 0u)
    {
        EMBED_CALL(cli_effect_called_append, SymbolsEffectValueCtx, .called = args->called, .token = args->token,
                   .joined_at = 0xffffffffu);
        return;
    }
    CliEffectToken *const last = &args->called->token[args->called->count - 1u];
    const unsigned int was = args->called->joined_at[args->called->count - 1u];
    const char *const before = (was == 0xffffffffu) ? last->text : (args->called->joined.out + was);
    // a call's joined texts are far under 4 GiB
    const unsigned int at = (unsigned int)args->called->joined.at;
    const unsigned long long length = last->length;
    mmgr_line_put_n(&args->called->joined, before, length);
    mmgr_line_put_n(&args->called->joined, args->token->text, args->token->length);
    args->called->refused |= !mmgr_line_ok(&args->called->joined);
    last->length = length + args->token->length;
    args->called->joined_at[args->called->count - 1u] = at;
}

// the arguments of a call, its bracket the leaf at hand: each the leaves between separators at its depth, up to the
// bracket that closes it, `first` and `end` each argument's first leaf and the one after its last; how many, the
// reading moved past the closing bracket, or CLI_EFFECT_ARGUMENTS + 1 where the bracket is not closed
EMBED_INLINE unsigned int cli_effect_arguments(const SymbolsEffectValueCtx *args)
{
    args->read_state->at += 1u;
    unsigned int arguments = 0u;
    unsigned int depth = 0u;
    unsigned int from = args->read_state->at;
    for (; args->read_state->at < args->read_state->count; args->read_state->at += 1u)
    {
        const int opens = EMBED_CALL(cli_effect_at, SymbolsEffectValueCtx, .read_state_read = args->read_state,
                                     .roles = CLI_LEAF_OPEN);
        const int closes = EMBED_CALL(cli_effect_at, SymbolsEffectValueCtx, .read_state_read = args->read_state,
                                      .roles = CLI_LEAF_CLOSE);
        if ((closes && (depth == 0u)) ||
            ((depth == 0u) && EMBED_CALL(cli_effect_at, SymbolsEffectValueCtx, .read_state_read = args->read_state,
                                         .roles = CLI_LEAF_SEPARATOR)))
        {
            if (arguments < CLI_EFFECT_ARGUMENTS)
            {
                args->first_pointer[arguments] = from;
                args->end[arguments] = args->read_state->at;
                arguments += 1u;
            }
            from = args->read_state->at + 1u;
            if (closes)
            {
                args->read_state->at += 1u;
                // a call with no leaf between its brackets has no argument
                return ((arguments == 1u) && (args->first_pointer[0] == args->end[0])) ? 0u : arguments;
            }
            continue;
        }
        depth = opens ? (depth + 1u) : (closes ? (depth - 1u) : depth);
    }
    return CLI_EFFECT_ARGUMENTS + 1u;
}

// the parameter of the macro `macro` whose name is `token`'s text: its place among the parameters, or
// CLI_EFFECT_ARGUMENTS where it names none
EMBED_INLINE unsigned int cli_effect_parameter(const SymbolsEffectValueCtx *args)
{
    const CliEffect *effect = args->effect;
    const char *name = EMBED_CALL(cli_symbols_internal.effect_text, CliSymbolsCtx, .effect_read = effect,
                                  .at_value = args->macro->params);
    for (unsigned int at = 0u; at < args->macro->param_count; at += 1u)
    {
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
        if ((length == args->token->length) && EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = name,
                                                          .other_pointer = args->token->text, .size = length))
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
EMBED_INLINE long long cli_effect_call(const SymbolsEffectValueCtx *args)
{
    CliEffect *const effect = args->read_state->effect;
    unsigned int first[CLI_EFFECT_ARGUMENTS];
    unsigned int end[CLI_EFFECT_ARGUMENTS];
    const unsigned int arguments = EMBED_CALL(cli_effect_arguments, SymbolsEffectValueCtx,
                                              .read_state = args->read_state, .first_pointer = first, .end = end);
    if (arguments > CLI_EFFECT_ARGUMENTS)
    {
        return 0LL;
    }
    const CliEffectMacro macro = effect->macro[args->macro_at];
    CliEffectToken *const value = EMBED_CALL(cli_effect_tokens_get, SymbolsEffectValueCtx, .effect = effect,
                                             .first = macro.first, .count = macro.count);
    // each leaf of the macro's value stands for itself or for no more leaves than the arguments span
    const unsigned long long span = (arguments != 0u) ? (unsigned long long)(end[arguments - 1u] - first[0]) : 0ULL;
    const unsigned long long required = ((unsigned long long)macro.count * (span + 1ULL)) + 1ULL;
    const unsigned int capacity = (required < CLI_EFFECT_CALLED_MAX) ? (unsigned int)required : CLI_EFFECT_CALLED_MAX;
    const unsigned long long token_bytes = (unsigned long long)capacity * sizeof(CliEffectToken);
    const unsigned long long position_bytes = (unsigned long long)capacity * sizeof(unsigned int);
    char *const cell = (value != NULL)
                           ? g_cli_cells->temporary_buf_alloc(token_bytes + position_bytes + CLI_EFFECT_JOINED_CAPACITY,
                                                              &g_cli_code_cells)
                           : NULL;
    if (cell == NULL)
    {
        g_cli_cells->persistent_buf_release(value, &g_cli_code_cells);
        return 0LL;
    }
    CliEffectCalled called = {(CliEffectToken *)(void *)cell,
                              0u,
                              capacity,
                              {cell + token_bytes + position_bytes, CLI_EFFECT_JOINED_CAPACITY, 0ULL},
                              (unsigned int *)(void *)(cell + token_bytes),
                              0};
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
        const unsigned int param = ((leaf->is & CLI_LEAF_NAME) != 0u)
                                       ? EMBED_CALL(cli_effect_parameter, SymbolsEffectValueCtx, .effect = effect,
                                                    .macro = &macro, .token = leaf)
                                       : CLI_EFFECT_ARGUMENTS;
        const int rest = macro.rest && ((leaf->is & CLI_LEAF_REMAINDER) != 0u);
        // the leaves this one stands for: the argument at the parameter's place, the arguments past them with the
        // commas between, or itself
        unsigned int from = 0u;
        unsigned int to = 1u;
        const CliEffectToken *source = leaf;
        if ((param < arguments) || (rest && (macro.param_count < arguments)))
        {
            source = args->read_state->token;
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
            const CliEffectToken string = {"", 0ULL, CLI_LEAF_STRING, 0u, 10u, 0u};
            EMBED_CALL(cli_effect_called_append, SymbolsEffectValueCtx, .called = &called, .token = &string,
                       .joined_at = 0xffffffffu);
            quoting = 0;
            joining = 0;
            continue;
        }
        for (unsigned int each = from; each < to; each += 1u)
        {
            if (joining && (each == from))
            {
                EMBED_CALL(cli_effect_called_join, SymbolsEffectValueCtx, .called = &called, .token = &source[each]);
                joining = 0;
                continue;
            }
            EMBED_CALL(cli_effect_called_append, SymbolsEffectValueCtx, .called = &called, .token = &source[each],
                       .joined_at = 0xffffffffu);
        }
    }
    // each joined leaf's text read from the line
    for (unsigned int at = 0u; at < called.count; at += 1u)
    {
        called.token[at].text =
            (called.joined_at[at] == 0xffffffffu) ? called.token[at].text : (called.joined.out + called.joined_at[at]);
    }
    const long long result = called.refused ? 0LL
                                            : EMBED_CALL(cli_symbols_effect_internal.effect_value, CliSymbolsEffectCtx,
                                                         .effect = effect, .token = called.token, .count = called.count,
                                                         .through = args->read_state->through + 1u);
    g_cli_cells->persistent_buf_release(cell, &g_cli_code_cells);
    g_cli_cells->persistent_buf_release(value, &g_cli_code_cells);
    return result;
}

// the value of the macro the name at `token` names: its value's leaves read as a condition one macro deeper, a macro
// called with arguments expanded where a bracket follows it and 0 where none does; 0 for a name of no macro, or where
// the reading is CLI_EFFECT_THROUGH macros deep
EMBED_INLINE long long cli_effect_by_name(const SymbolsEffectValueCtx *args)
{
    const CliEffectToken *token = args->token;
    CliEffect *const effect = args->read_state->effect;
    const unsigned int macro = EMBED_CALL(cli_symbols_name_internal.effect_macro, CliSymbolsNameCtx,
                                          .effect_read = effect, .name = token->text, .length = token->length);
    if ((macro == CLI_JSON_NONE) || (args->read_state->through >= CLI_EFFECT_THROUGH))
    {
        return 0LL;
    }
    if (effect->macro[macro].called)
    {
        return EMBED_CALL(cli_effect_at, SymbolsEffectValueCtx, .read_state_read = args->read_state,
                          .roles = CLI_LEAF_OPEN)
                   ? EMBED_CALL(cli_effect_call, SymbolsEffectValueCtx, .read_state = args->read_state,
                                .macro_at = macro)
                   : 0LL;
    }
    CliEffectToken *const value = EMBED_CALL(cli_effect_tokens_get, SymbolsEffectValueCtx, .effect = effect,
                                             .first = effect->macro[macro].first, .count = effect->macro[macro].count);
    const long long result =
        (value != NULL)
            ? EMBED_CALL(cli_symbols_effect_internal.effect_value, CliSymbolsEffectCtx, .effect = effect,
                         .token = value, .count = effect->macro[macro].count, .through = args->read_state->through + 1u)
            : 0LL;
    g_cli_cells->persistent_buf_release(value, &g_cli_code_cells);
    return result;
}

// one value and the operators before it: a bracketed condition, `defined` with a name or a bracketed name, a number,
// or a name; an operator before a value applied to it as its symbol writes
static long long cli_effect_unary(const SymbolsEffectValueCtx *args)
{
    while ((args->read_state->at < args->read_state->count) &&
           ((args->read_state->token[args->read_state->at].is & CLI_LEAF_PREFIX) != 0u))
    {
        args->read_state->at += 1u;
    }
    if (args->read_state->at >= args->read_state->count)
    {
        return 0LL;
    }
    const CliEffectToken *const token = &args->read_state->token[args->read_state->at];
    if (((token->is & CLI_LEAF_UNARY) != 0u) || (token->binds != 0u))
    {
        args->read_state->at += 1u;
        const long long value = cli_effect_unary(args);
        // before a value, an operator that subtracts negates it
        return (token->does == CLI_DOES_NOT)          ? (long long)(value == 0LL)
               : (token->does == CLI_DOES_COMPLEMENT) ? ~value
               : (token->does == CLI_DOES_SUBTRACT)   ? -value
                                                      : value;
    }
    if ((token->is & CLI_LEAF_OPEN) != 0u)
    {
        args->read_state->at += 1u;
        const long long value = cli_effect_condition(args);
        args->read_state->at += EMBED_CALL(cli_effect_at, SymbolsEffectValueCtx, .read_state_read = args->read_state,
                                           .roles = CLI_LEAF_CLOSE)
                                    ? 1u
                                    : 0u;
        return value;
    }
    args->read_state->at += 1u;
    if ((token->is & CLI_LEAF_DEFINED) != 0u)
    {
        const int bracketed = EMBED_CALL(cli_effect_at, SymbolsEffectValueCtx, .read_state_read = args->read_state,
                                         .roles = CLI_LEAF_OPEN);
        args->read_state->at += bracketed ? 1u : 0u;
        const CliEffectToken *const name =
            (args->read_state->at < args->read_state->count) ? &args->read_state->token[args->read_state->at] : NULL;
        args->read_state->at += (name != NULL) ? 1u : 0u;
        args->read_state->at += (bracketed && EMBED_CALL(cli_effect_at, SymbolsEffectValueCtx,
                                                         .read_state_read = args->read_state, .roles = CLI_LEAF_CLOSE))
                                    ? 1u
                                    : 0u;
        return (name != NULL) && (EMBED_CALL(cli_symbols_name_internal.effect_macro, CliSymbolsNameCtx,
                                             .effect_read = args->read_state->effect, .name = name->text,
                                             .length = name->length) != CLI_JSON_NONE);
    }
    if ((token->is & CLI_LEAF_NUMBER) != 0u)
    {
        // a number's suffix is a leaf of its own after it
        while ((args->read_state->at < args->read_state->count) &&
               ((args->read_state->token[args->read_state->at].is & CLI_LEAF_NUMBER) != 0u))
        {
            args->read_state->at += 1u;
        }
        return EMBED_CALL(cli_effect_number, SymbolsEffectValueCtx, .token = token);
    }
    return ((token->is & (CLI_LEAF_NAME | CLI_LEAF_KEYWORD | CLI_LEAF_TYPE)) != 0u)
               ? EMBED_CALL(cli_effect_by_name, SymbolsEffectValueCtx, .read_state = args->read_state, .token = token)
               : 0LL;
}

// the values joined by the operators that bind at least as tightly as `min`, each read with those that bind more
// tightly first, each operator doing the operation its grammar names, 0 for one it names none of
static long long cli_effect_binary(const SymbolsEffectValueCtx *args)
{
    long long left = cli_effect_unary(args);
    while ((args->read_state->at < args->read_state->count) &&
           (args->read_state->token[args->read_state->at].binds >= args->min) &&
           (args->read_state->token[args->read_state->at].binds != 0u))
    {
        const CliEffectToken *const joined = &args->read_state->token[args->read_state->at];
        args->read_state->at += 1u;
        const long long right = EMBED_CALL(cli_effect_binary, SymbolsEffectValueCtx, .read_state = args->read_state,
                                           .min = joined->binds + 1u);
        // a shift past a long long's bits gives 0
        const int shifts = (right >= 0LL) && (right < 63LL);
        const unsigned char does = joined->does;
        left = (does == CLI_DOES_MULTIPLY)     ? (left * right)
               : (does == CLI_DOES_DIVIDE)     ? ((right != 0LL) ? (left / right) : 0LL)
               : (does == CLI_DOES_REMAINDER)  ? ((right != 0LL) ? (left % right) : 0LL)
               : (does == CLI_DOES_ADD)        ? (left + right)
               : (does == CLI_DOES_SUBTRACT)   ? (left - right)
               : (does == CLI_DOES_SHIFTLEFT)  ? (shifts ? (left << right) : 0LL)
               : (does == CLI_DOES_SHIFTRIGHT) ? (shifts ? (left >> right) : 0LL)
               : (does == CLI_DOES_LESS)       ? (long long)(left < right)
               : (does == CLI_DOES_ATMOST)     ? (long long)(left <= right)
               : (does == CLI_DOES_GREATER)    ? (long long)(left > right)
               : (does == CLI_DOES_ATLEAST)    ? (long long)(left >= right)
               : (does == CLI_DOES_EQUAL)      ? (long long)(left == right)
               : (does == CLI_DOES_UNEQUAL)    ? (long long)(left != right)
               : (does == CLI_DOES_AND)        ? (long long)((left != 0LL) && (right != 0LL))
               : (does == CLI_DOES_OR)         ? (long long)((left != 0LL) || (right != 0LL))
               : (does == CLI_DOES_BITAND)     ? (left & right)
               : (does == CLI_DOES_BITXOR)     ? (left ^ right)
               : (does == CLI_DOES_BITOR)      ? (left | right)
                                               : 0LL;
    }
    return left;
}

// a whole condition: the values and operators, and a leaf named `choice` after them choosing between the condition
// after it, to the leaf named `otherwise` that closes it, and the condition after that one
static long long cli_effect_condition(const SymbolsEffectValueCtx *args)
{
    const long long chosen =
        EMBED_CALL(cli_effect_binary, SymbolsEffectValueCtx, .read_state = args->read_state, .min = 1u);
    const unsigned long long is =
        (args->read_state->at < args->read_state->count) ? args->read_state->token[args->read_state->at].is : 0ULL;
    if (((is & CLI_LEAF_CHOICE) == 0u) || ((is & CLI_LEAF_OTHERWISE) != 0u))
    {
        return chosen;
    }
    args->read_state->at += 1u;
    const long long then = cli_effect_condition(args);
    args->read_state->at += ((args->read_state->at < args->read_state->count) &&
                             ((args->read_state->token[args->read_state->at].is & CLI_LEAF_OTHERWISE) != 0u))
                                ? 1u
                                : 0u;
    const long long otherwise = cli_effect_condition(args);
    return (chosen != 0LL) ? then : otherwise;
}

long long cli_effect_value(const CliSymbolsEffectCtx *args)
{
    CliEffectReadState read_state = {args->effect, args->token, args->count, 0u, args->through};
    return EMBED_CALL(cli_effect_condition, SymbolsEffectValueCtx, .read_state = &read_state);
}
