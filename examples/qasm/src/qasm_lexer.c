// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qasm_lexer.c: the lexer and tokens
#include "../../../src/platform/platform.h"
#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "qasm_internal.h"

// ---------------------------------------------------------------------------------------------------------------
// the lexer

// the bytes of a name after its first, and the digits of a number
static const char QASM_NAME_BYTES[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_";
static const char QASM_DIGIT_BYTES[] = "0123456789";

// Explicit casts read the byte as its code point
static int qasm_is_character(char c)
{
    return EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_ALPHA, .byte = (uint8_t)c) || (c == '_');
}

static int qasm_is_digit(char c)
{
    // Explicit cast reads the byte as its code point
    return EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = (uint8_t)c);
}

// the bytes at `from` of `text`, `length` bytes in all, that the set `bytes` holds
static size_t qasm_lex_run(const char *text, size_t length, size_t from, const char *bytes)
{
    return EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &text[from], .cap = length - from, .other = bytes);
}

int qasm_lex(QasmParser *parser)
{
    const char *const text = parser->text;
    const size_t length = parser->length;
    size_t capacity = 1024u;
    parser->tokens = (QasmToken *)platform_cell_taken(capacity * sizeof(QasmToken), platform_place_g_program_owner, 0);
    if (!QASM_CHECK(parser->tokens != NULL, parser, parser->error))
    {
        parser->failed = 1;
        return 0;
    }
    unsigned int line = 1u;
    unsigned int column = 1u;
    size_t at = 0u;
    while (1)
    {
        // spaces and comments
        while (at < length)
        {
            const char c = text[at];
            if (c == '\n')
            {
                line += 1u;
                column = 1u;
                at += 1u;
            }
            else if ((c == ' ') || (c == '\t') || (c == '\r'))
            {
                const size_t blanks = qasm_lex_run(text, length, at, " \t\r");
                // Explicit cast takes a column inside one line, which an unsigned int counts
                column += (unsigned int)blanks;
                at += blanks;
            }
            else if ((c == '/') && ((at + 1u) < length) && (text[at + 1u] == '/'))
            {
                at += EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &text[at], .cap = length - at, .other = "\n");
            }
            else if ((c == '/') && ((at + 1u) < length) && (text[at + 1u] == '*'))
            {
                at += 2u;
                column += 2u;
                // the body ends at the close, or at the end of the text where no close follows
                const char *const close = EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = &text[at], .cap = length - at,
                                                     .other = "*/", .other_len = 2u);
                const size_t body_end = (close != NULL) ? (size_t)(close - text) : length;
                // each line end of the body, reached from the one before it, and the column past the last
                size_t from = at;
                for (const void *ended =
                         EMBED_CALL(memor.chr, MemoriaCfg, .src = &text[from], .bytes = body_end - from, .val = '\n');
                     ended != NULL; ended = EMBED_CALL(memor.chr, MemoriaCfg, .src = &text[from],
                                                       .bytes = body_end - from, .val = '\n'))
                {
                    line += 1u;
                    column = 1u;
                    // Explicit cast reads the found byte as a byte of the text
                    from = (size_t)((const char *)ended - text) + 1u;
                }
                // Explicit casts take a column inside one line, which an unsigned int counts
                column += (unsigned int)(body_end - from);
                at = body_end + 2u;
                column += 2u;
            }
            else
            {
                break;
            }
        }
        if (parser->token_count + 1u >= capacity)
        {
            capacity *= 2u;
            QasmToken *const grown =
                (QasmToken *)platform_cell_grown(parser->tokens, (capacity / 2u) * sizeof(QasmToken),
                                                 capacity * sizeof(QasmToken), platform_place_g_program_owner, 0);
            if (!QASM_CHECK(grown != NULL, parser, parser->error))
            {
                parser->failed = 1;
                return 0;
            }
            parser->tokens = grown;
        }
        QasmToken *const token = &parser->tokens[parser->token_count];
        token->start = (unsigned int)at;
        token->line = line;
        token->column = column;
        if (at >= length)
        {
            token->type = QASM_TOKEN_END;
            token->length = 0u;
            parser->token_count += 1u;
            return 1;
        }
        const char c = text[at];
        size_t end = at + 1u;
        if (qasm_is_character(c))
        {
            end += qasm_lex_run(text, length, end, QASM_NAME_BYTES);
            token->type = QASM_TOKEN_IDENT;
        }
        else if (qasm_is_digit(c) || ((c == '.') && ((at + 1u) < length) && qasm_is_digit(text[at + 1u])))
        {
            end = at + qasm_lex_run(text, length, at, QASM_DIGIT_BYTES);
            if ((end < length) && (text[end] == '.'))
            {
                end += 1u;
                end += qasm_lex_run(text, length, end, QASM_DIGIT_BYTES);
            }
            if ((end < length) && ((text[end] == 'e') || (text[end] == 'E')))
            {
                size_t exponent = end + 1u;
                if ((exponent < length) && ((text[exponent] == '+') || (text[exponent] == '-')))
                {
                    exponent += 1u;
                }
                if ((exponent < length) && qasm_is_digit(text[exponent]))
                {
                    end = exponent + qasm_lex_run(text, length, exponent, QASM_DIGIT_BYTES);
                }
            }
            token->type = QASM_TOKEN_NUMBER;
        }
        else if (c == '"')
        {
            end +=
                EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &text[end], .cap = length - end, .other = "\"\n");
            if ((end >= length) || (text[end] != '"'))
            {
                token->type = QASM_TOKEN_SYMBOL;
                token->length = 1u;
                parser->token_count += 1u;
                qasm_error(parser, token, "a string is not closed on its line");
                return 0;
            }
            end += 1u;
            token->type = QASM_TOKEN_STRING;
        }
        else if ((c == '-') && ((at + 1u) < length) && (text[at + 1u] == '>'))
        {
            end = at + 2u;
            token->type = QASM_TOKEN_SYMBOL;
        }
        else if ((c == '=') && ((at + 1u) < length) && (text[at + 1u] == '='))
        {
            end = at + 2u;
            token->type = QASM_TOKEN_SYMBOL;
        }
        // Explicit cast takes the character as the byte that the search compares against
        else if (EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = ";,()[]{}+-*/^", .cap = PLATFORM_TEXT_TO_TERMINATOR,
                            .byte = (uint8_t)c) != NULL)
        {
            token->type = QASM_TOKEN_SYMBOL;
        }
        else
        {
            token->type = QASM_TOKEN_SYMBOL;
            token->length = 1u;
            parser->token_count += 1u;
            qasm_error(parser, token, "'%c' is not part of OpenQASM 2.0", c);
            return 0;
        }
        token->length = (unsigned int)(end - at);
        column += token->length;
        at = end;
        parser->token_count += 1u;
    }
}

// ---------------------------------------------------------------------------------------------------------------
// the parser's reading of tokens

const QasmToken *qasm_peek(const QasmParser *parser)
{
    return &parser->tokens[parser->at];
}

int qasm_token_is(const QasmParser *parser, const QasmToken *token, const char *text)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    return (token->length == length) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = parser->text + token->start, .other = text, .bytes = length) == 0);
}

int qasm_tokens_equal(const QasmParser *parser, const QasmToken *left, const QasmToken *right)
{
    return (left->length == right->length) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = parser->text + left->start, .other = parser->text + right->start,
                       .bytes = left->length) == 0);
}

int qasm_accept(QasmParser *parser, const char *text)
{
    const QasmToken *const token = qasm_peek(parser);
    if ((token->type != QASM_TOKEN_END) && (token->type != QASM_TOKEN_STRING) && qasm_token_is(parser, token, text))
    {
        parser->at += 1u;
        return 1;
    }
    return 0;
}

int qasm_expect(QasmParser *parser, const char *text)
{
    if (qasm_accept(parser, text))
    {
        return 1;
    }
    qasm_error(parser, qasm_peek(parser), "'%s' was expected here", text);
    return 0;
}

void qasm_token_name(const QasmParser *parser, const QasmToken *token, char *name)
{
    const unsigned int length = (token->length < (QASM_NAME_CAPACITY - 1u)) ? token->length : (QASM_NAME_CAPACITY - 1u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = name, .src = parser->text + token->start, .bytes = length);
    name[length] = '\0';
}

int qasm_expect_ident(QasmParser *parser, char *name, unsigned int *token_at)
{
    const QasmToken *const token = qasm_peek(parser);
    if (token->type != QASM_TOKEN_IDENT)
    {
        qasm_error(parser, token, "a name was expected here");
        return 0;
    }
    if (token->length >= QASM_NAME_CAPACITY)
    {
        qasm_error(parser, token, "a name past %u characters is not read", QASM_NAME_CAPACITY - 1u);
        return 0;
    }
    if (name != NULL)
    {
        qasm_token_name(parser, token, name);
    }
    if (token_at != NULL)
    {
        *token_at = parser->at;
    }
    parser->at += 1u;
    return 1;
}

int qasm_expect_count(QasmParser *parser, unsigned int *count)
{
    const QasmToken *const token = qasm_peek(parser);
    unsigned long long value = 0ull;
    int digits = (token->type == QASM_TOKEN_NUMBER);
    for (unsigned int at = 0u; digits && (at < token->length); at += 1u)
    {
        const char c = parser->text[token->start + at];
        digits = qasm_is_digit(c) && (value < 100000000ull);
        value = (value * 10ull) + (unsigned long long)(c - '0');
    }
    if (!digits)
    {
        qasm_error(parser, token, "a whole number was expected here");
        return 0;
    }
    *count = (unsigned int)value;
    parser->at += 1u;
    return 1;
}

// ---------------------------------------------------------------------------------------------------------------
// expressions: + - * / unary minus, parentheses, pi, numbers and the gate's parameters

int qasm_number(QasmParser *parser, const QasmToken *token, QasmAngle *value)
{
    const char *const text = parser->text + token->start;
    // Explicit casts take the length of a token, which an unsigned int counts. The lexer puts one point at most in a
    // number, and the places are the digits of the mantissa after it
    const unsigned int mantissa_end =
        (unsigned int)EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = text, .cap = token->length, .other = "eE");
    const void *const point = EMBED_CALL(memor.chr, MemoriaCfg, .src = text, .bytes = mantissa_end, .val = '.');
    const unsigned int places = (point != NULL) ? (mantissa_end - (unsigned int)((const char *)point - text) - 1u) : 0u;
    if (places > 200u)
    {
        qasm_error(parser, token, "a number past 200 places is not read");
        return 0;
    }
    long long exponent = 0;
    if (mantissa_end < token->length)
    {
        exponent = strtoll(text + mantissa_end + 1u, NULL, 10);
        if ((exponent > 300) || (exponent < -300))
        {
            qasm_error(parser, token, "an exponent past 300 is not read");
            return 0;
        }
    }
    QasmFraction number;
    if (!qasm_exact_ok(parser, orior_exact_from_decimal(text, mantissa_end, places, &number.num)))
    {
        return 0;
    }
    qasm_exact_set(&number.den, 1ull, 0);
    if (!qasm_exact_ok(parser, orior_exact_scale_by_ten(&number.den, places)))
    {
        return 0;
    }
    if ((exponent > 0) && !qasm_exact_ok(parser, orior_exact_scale_by_ten(&number.num, (uint32_t)exponent)))
    {
        return 0;
    }
    if ((exponent < 0) && !qasm_exact_ok(parser, orior_exact_scale_by_ten(&number.den, (uint32_t)(-exponent))))
    {
        return 0;
    }
    if (!qasm_fraction_reduce(parser, &number))
    {
        return 0;
    }
    value->a = number;
    qasm_fraction_integer(&value->b, 0);
    return 1;
}
