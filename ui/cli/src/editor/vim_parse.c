// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// vim_parse.c: the keys typed of a command read as one: "x and its register, a count, the operator or the command, a
// second count after an operator, the motion with the letter f F t T take, the text object after i or a, an operator
// doubled for whole lines, r and its letter, and gg. A command not whole yet waits; keys that are none go
#include "vim_internal.h"

#include "../text/text.h"

// the motions and the commands a command starts with, as their tokens
static const unsigned int s_motions[] = {'h', 'j', 'k', 'l', 'w', 'W', 'b', 'B', 'e', 'E',          '0',       '^',
                                         '$', 'G', ';', ',', '%', '{', '}', 'n', 'N', CLI_VIM_DOWN, CLI_VIM_UP};
static const unsigned int s_commands[] = {'x', 'X', 'D', 'C', 'Y', 's', 'S', 'J', '~', 'p', 'P', 'u', CLI_VIM_REDO,
                                          '.', 'i', 'a', 'I', 'A', 'o', 'O', 'v', 'V', '/', '?', '*', '#'};
// the commands that act on visual mode's selection besides the operators
static const unsigned int s_chosen[] = {'~', 'J', 'p', 'P', 'x', 's', 'o'};

// The tokens read: the code points of the keys typed, how many, and where the reading stands
typedef struct
{
    unsigned int token[CLI_VIM_TYPED_MOST];
    unsigned int count;
    unsigned int at;
} VimTokens;

typedef struct
{
    VimTokens *tokens;
    unsigned int letter;
    const unsigned int *among;
    unsigned int among_count;
    CliVimCommand *command_out;
} VimParseCtx;

// whether `letter` is among the `among_count` tokens of `among`
EMBED_INLINE int parse_among(const VimParseCtx *args)
{
    for (unsigned int at = 0u; at < args->among_count; at += 1u)
    {
        if (args->among[at] == args->letter)
        {
            return 1;
        }
    }
    return 0;
}

// whether `letter` is an operator
EMBED_INLINE int parse_operator(const VimParseCtx *args)
{
    return (args->letter == 'd') || (args->letter == 'c') || (args->letter == 'y') || (args->letter == '>') ||
           (args->letter == '<');
}

// a count read where the tokens stand, its digits taken, a 0 first being no count: the count, 0 where none stands
EMBED_INLINE unsigned long long parse_count(const VimParseCtx *args)
{
    VimTokens *const tokens = args->tokens;
    unsigned long long count = 0ULL;
    while ((tokens->at < tokens->count) && (tokens->token[tokens->at] >= '0') && (tokens->token[tokens->at] <= '9') &&
           ((count != 0ULL) || (tokens->token[tokens->at] != '0')))
    {
        count = (count * 10ULL) + (unsigned long long)(tokens->token[tokens->at] - '0');
        tokens->at += 1u;
    }
    return count;
}

// the motion `letter` read into `command_out`, the letter after it taken: CLI_VIM_PARSE_WHOLE, CLI_VIM_PARSE_WAIT
// where it waits for that letter, or CLI_VIM_PARSE_NONE where `letter` is no motion
EMBED_INLINE unsigned int parse_motion(const VimParseCtx *args)
{
    VimTokens *const tokens = args->tokens;
    CliVimCommand *const command = args->command_out;
    if (args->letter == 'g')
    {
        if (tokens->at >= tokens->count)
        {
            return CLI_VIM_PARSE_WAIT;
        }
        const unsigned int next = tokens->token[tokens->at];
        tokens->at += 1u;
        command->motion = (next == 'g') ? CLI_VIM_TOP : 0u;
        return (next == 'g') ? CLI_VIM_PARSE_WHOLE : CLI_VIM_PARSE_NONE;
    }
    if ((args->letter == 'f') || (args->letter == 'F') || (args->letter == 't') || (args->letter == 'T'))
    {
        if (tokens->at >= tokens->count)
        {
            return CLI_VIM_PARSE_WAIT;
        }
        command->motion = args->letter;
        command->letter = tokens->token[tokens->at];
        tokens->at += 1u;
        return CLI_VIM_PARSE_WHOLE;
    }
    if (EMBED_CALL(parse_among, VimParseCtx, .letter = args->letter, .among = s_motions,
                   .among_count = sizeof(s_motions) / sizeof(s_motions[0])))
    {
        command->motion = args->letter;
        return CLI_VIM_PARSE_WHOLE;
    }
    return CLI_VIM_PARSE_NONE;
}

// the keys after an operator `letter` read: a second count, then the operator again for whole lines, a text object or
// a motion, the counts multiplied
EMBED_INLINE void parse_operated(const VimParseCtx *args)
{
    VimTokens *const tokens = args->tokens;
    CliVimCommand *const command = args->command_out;
    command->name = args->letter;
    const unsigned long long inner = EMBED_CALL(parse_count, VimParseCtx, .tokens = tokens);
    if (tokens->at >= tokens->count)
    {
        command->status = CLI_VIM_PARSE_WAIT;
        return;
    }
    if (command->counted || (inner != 0ULL))
    {
        command->count = (command->counted ? command->count : 1ULL) * ((inner != 0ULL) ? inner : 1ULL);
        command->counted = 1;
    }
    const unsigned int target = tokens->token[tokens->at];
    tokens->at += 1u;
    if (target == args->letter)
    {
        command->lines = 1;
        command->status = CLI_VIM_PARSE_WHOLE;
        return;
    }
    if ((target == 'i') || (target == 'a'))
    {
        if (tokens->at >= tokens->count)
        {
            command->status = CLI_VIM_PARSE_WAIT;
            return;
        }
        command->object_type = target;
        command->object_of = tokens->token[tokens->at];
        tokens->at += 1u;
        command->status = CLI_VIM_PARSE_WHOLE;
        return;
    }
    command->status = EMBED_CALL(parse_motion, VimParseCtx, .tokens = tokens, .letter = target, .command_out = command);
}

void cli_vim_parse(const CliVimCtx *args)
{
    CliVimCommand *const command = args->command_out;
    memor.set(&(MemoriaCfg){.dst = command, .val = 0u, .bytes = sizeof(*command)});
    VimTokens tokens;
    tokens.count = 0u;
    tokens.at = 0u;
    for (unsigned long long read = 0ULL; (read < args->length) && (tokens.count < CLI_VIM_TYPED_MOST);)
    {
        unsigned long long bytes = 0ULL;
        tokens.token[tokens.count] = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = args->text,
                                                .size = args->length, .at = read, .length = &bytes);
        read += (bytes != 0ULL) ? bytes : 1ULL;
        tokens.count += 1u;
    }
    command->status = CLI_VIM_PARSE_WAIT;
    if ((tokens.count != 0u) && (tokens.token[0] == '"'))
    {
        if (tokens.count < 2u)
        {
            return;
        }
        command->register_name = tokens.token[1];
        tokens.at = 2u;
    }
    const unsigned long long first = EMBED_CALL(parse_count, VimParseCtx, .tokens = &tokens);
    command->counted = first != 0ULL;
    command->count = first;
    if (tokens.at >= tokens.count)
    {
        return;
    }
    const unsigned int name = tokens.token[tokens.at];
    tokens.at += 1u;
    CliVimView view;
    (void)g_cli_guard->movable_buf_read(
        &(CarcerMovableCfg){.cell = g_cli_vim_state.view,
                            .offset = (unsigned long long)(args->at) * sizeof(CliVimView),
                            .dst = &view,
                            .bytes = sizeof(CliVimView)});
    const unsigned int mode = view.mode;
    const int visual = (mode == CLI_VIM_VISUAL) || (mode == CLI_VIM_LINE);
    if (visual && (EMBED_CALL(parse_operator, VimParseCtx, .letter = name) ||
                   EMBED_CALL(parse_among, VimParseCtx, .letter = name, .among = s_chosen,
                              .among_count = sizeof(s_chosen) / sizeof(s_chosen[0]))))
    {
        command->name = name;
        command->chosen = 1;
        command->status = CLI_VIM_PARSE_WHOLE;
        return;
    }
    if (visual && ((name == 'i') || (name == 'a')))
    {
        if (tokens.at >= tokens.count)
        {
            return;
        }
        command->object_type = name;
        command->object_of = tokens.token[tokens.at];
        command->status = CLI_VIM_PARSE_WHOLE;
        return;
    }
    if (EMBED_CALL(parse_operator, VimParseCtx, .letter = name))
    {
        EMBED_CALL(parse_operated, VimParseCtx, .tokens = &tokens, .letter = name, .command_out = command);
        return;
    }
    if (name == 'r')
    {
        if (tokens.at >= tokens.count)
        {
            return;
        }
        command->name = name;
        command->letter = tokens.token[tokens.at];
        command->status = CLI_VIM_PARSE_WHOLE;
        return;
    }
    const unsigned int motion =
        EMBED_CALL(parse_motion, VimParseCtx, .tokens = &tokens, .letter = name, .command_out = command);
    if (motion != CLI_VIM_PARSE_NONE)
    {
        command->status = motion;
        return;
    }
    const int known = EMBED_CALL(parse_among, VimParseCtx, .letter = name, .among = s_commands,
                                 .among_count = sizeof(s_commands) / sizeof(s_commands[0]));
    command->name = known ? name : 0u;
    command->status = known ? CLI_VIM_PARSE_WHOLE : CLI_VIM_PARSE_NONE;
}
