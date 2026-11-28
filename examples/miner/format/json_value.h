// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file json_value.h
 * @brief Enough JSON to read stratum, parsed and not pattern matched.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note A client that searches for substrings and counts brackets by hand reads a merkle branch as a
 *       flat token list. It cannot tell a capability "mining.notify" in a subscribe reply from a method
 *       field with the same letters. This file removes that defect.
 * @note Stratum is small and its shapes do not change. Thus this parses the value grammar and stops
 *       there. It does not stream, and it reads no comments. Of two members with one key, the last
 *       is the one a lookup finds.
 * @note A document holds its nodes and its decoded text in arrays of a fixed bound. A message that
 *       passes either bound does not parse.
 */
#ifndef JSON_VALUE_H
#define JSON_VALUE_H

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include <stdint.h>
#include <stdlib.h>

/** @brief The nodes a document holds at max. Node 0 is the absent value. */
#define JSON_NODES_MAX 4096u

/** @brief The bytes of decoded text a document holds at max, each string terminated. */
#define JSON_TEXT_BYTES 65536u

/** @brief The node of no value: what a lookup answers where the value is absent. */
#define JSON_ABSENT 0u

/** @brief What a parsed value turned out to be. */
enum class JsonType
{
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
};

/**
 * @brief One parsed value. Its children are a list from `first_child` through `next`, `children` of
 *        them. A member of an object carries its key at `key`, and a string its text at `text`, both
 *        offsets into the text of the document.
 */
struct JsonNode
{
    JsonType type;
    bool boolean;
    double number;
    unsigned int text;
    unsigned int key;
    unsigned int first_child;
    unsigned int last_child;
    unsigned int next;
    unsigned int children;
};

/** @brief A parsed document: its nodes, `node_count` of them, and its decoded text, `text_used` bytes. */
struct JsonDocument
{
    JsonNode node[JSON_NODES_MAX];
    unsigned int node_count;
    char text[JSON_TEXT_BYTES];
    unsigned int text_used;
};

/** @brief The type of a node. */
inline JsonType json_type(const JsonDocument *document, unsigned int node)
{
    return document->node[node].type;
}

/** @brief Reports whether a node is a string, which stratum fields mostly are. */
inline bool json_is_string(const JsonDocument *document, unsigned int node)
{
    return document->node[node].type == JsonType::String;
}

/** @brief The terminated text of a string node, and an empty text for any other node. */
inline const char *json_text(const JsonDocument *document, unsigned int node)
{
    return (document->node[node].type == JsonType::String) ? &document->text[document->node[node].text] : "";
}

/** @brief The children of an array or an object. */
inline unsigned int json_count(const JsonDocument *document, unsigned int node)
{
    const JsonType type = document->node[node].type;
    return ((type == JsonType::Array) || (type == JsonType::Object)) ? document->node[node].children : 0u;
}

/**
 * @brief Looks up an array element.
 *
 * @param[in] document The document [BORROWS].
 * @param[in] node     The array.
 * @param[in] index    Position to read.
 * @return             The element, or JSON_ABSENT where out of range or not an array. Never fails.
 */
inline unsigned int json_at(const JsonDocument *document, unsigned int node, unsigned int index)
{
    if ((document->node[node].type != JsonType::Array) || (index >= document->node[node].children))
    {
        return JSON_ABSENT;
    }
    unsigned int child = document->node[node].first_child;
    for (unsigned int at = 0u; at < index; at += 1u)
    {
        child = document->node[child].next;
    }
    return child;
}

/**
 * @brief Looks up an object member.
 *
 * @param[in] document The document [BORROWS].
 * @param[in] node     The object.
 * @param[in] name     Member to find.
 * @return             The last member of that key, or JSON_ABSENT where absent. Never fails.
 */
inline unsigned int json_member(const JsonDocument *document, unsigned int node, const char *name)
{
    if (document->node[node].type != JsonType::Object)
    {
        return JSON_ABSENT;
    }
    unsigned int found = JSON_ABSENT;
    for (unsigned int child = document->node[node].first_child, at = 0u; at < document->node[node].children;
         child = document->node[child].next, at += 1u)
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = &document->text[document->node[child].key],
                       .cap = JSON_TEXT_BYTES - document->node[child].key, .other = name, .ci = EMBED_FALSE))
        {
            found = child;
        }
    }
    return found;
}

namespace json_detail
{

/** @brief A parse: the source, its bytes, the cursor and the document it fills. */
struct JsonParse
{
    const char *source;
    size_t size;
    size_t at;
    JsonDocument *document;
};

/** @brief Advances past whitespace. */
inline void skip_whitespace(JsonParse *parse)
{
    if (parse->at < parse->size)
    {
        parse->at += EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &parse->source[parse->at],
                                .cap = parse->size - parse->at, .other = " \t\r\n");
    }
}

/** @brief A node taken from the document, every field clear; JSON_ABSENT where the nodes are spent. */
inline unsigned int node_taken(JsonParse *parse)
{
    JsonDocument *const document = parse->document;
    if (document->node_count >= JSON_NODES_MAX)
    {
        return JSON_ABSENT;
    }
    const unsigned int node = document->node_count;
    document->node_count += 1u;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &document->node[node], .bytes = sizeof(JsonNode), .val = 0);
    return node;
}

/** @brief A child put at the end of a parent's list. */
inline void child_joined(JsonDocument *document, unsigned int parent, unsigned int child)
{
    JsonNode *const holder = &document->node[parent];
    if (holder->children == 0u)
    {
        holder->first_child = child;
    }
    else
    {
        document->node[holder->last_child].next = child;
    }
    holder->last_child = child;
    holder->children += 1u;
}

/** @brief One byte put at the end of the document's text; false where the text is full. */
inline bool text_put(JsonDocument *document, char character)
{
    if (document->text_used >= JSON_TEXT_BYTES)
    {
        return false;
    }
    document->text[document->text_used] = character;
    document->text_used += 1u;
    return true;
}

bool parse_value(JsonParse *parse, unsigned int out);

/**
 * @brief Parses a quoted string into the document's text, terminated. It resolves the escapes that
 *        stratum can actually contain.
 *
 * @param[in,out] parse The parse, its cursor left past the closing quote on success [BORROWS].
 * @param[out]    text  The offset of the decoded text in the document.
 * @return              True where the parser read a complete string.
 */
inline bool parse_string(JsonParse *parse, unsigned int *text)
{
    const char *const source = parse->source;
    if ((parse->at >= parse->size) || (source[parse->at] != '"'))
    {
        return false;
    }
    parse->at += 1u;
    JsonDocument *const document = parse->document;
    *text = document->text_used;

    // each byte is a step of the decode: a quote ends the string, and a backslash starts an escape
    while (parse->at < parse->size)
    {
        const char character = source[parse->at];

        if (character == '"')
        {
            parse->at += 1u;
            return text_put(document, '\0');
        }
        if (character != '\\')
        {
            parse->at += 1u;
            if (!text_put(document, character))
            {
                return false;
            }
            continue;
        }

        parse->at += 1u;
        if (parse->at >= parse->size)
        {
            return false;
        }
        const char escaped = source[parse->at];
        parse->at += 1u;

        char decoded = escaped;
        switch (escaped)
        {
        case 'n':
            decoded = '\n';
            break;
        case 't':
            decoded = '\t';
            break;
        case 'r':
            decoded = '\r';
            break;
        case 'b':
            decoded = '\b';
            break;
        case 'f':
            decoded = '\f';
            break;
        case 'u':
        {
            if ((parse->at + 4u) > parse->size)
            {
                return false;
            }
            char digits[5];
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = digits, .src = &source[parse->at], .bytes = 4u);
            digits[4] = '\0';
            const unsigned code = (unsigned)strtoul(digits, NULL, 16);
            parse->at += 4u;
            // Stratum carries hex and ASCII identifiers. The parser folds anything above the basic
            // range to a placeholder. It has no UTF-8 encoder, because that encoder would never run.
            decoded = (code < 0x80u) ? (char)code : '?';
            break;
        }
        default:
            break;
        }
        if (!text_put(document, decoded))
        {
            return false;
        }
    }
    return false;
}

/** @brief True where the source at the cursor starts with `word`, which then the cursor passes. */
inline bool parse_word(JsonParse *parse, const char *word, size_t length)
{
    if (((parse->size - parse->at) < length) ||
        (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &parse->source[parse->at], .other = word, .bytes = length) != 0))
    {
        return false;
    }
    parse->at += length;
    return true;
}

/**
 * @brief Parses one value of any type into node `out`.
 *
 * @param[in,out] parse The parse, its cursor left past the value on success [BORROWS].
 * @param[in]     out   The node the value fills.
 * @return              True where the parser read a complete value.
 */
inline bool parse_value(JsonParse *parse, unsigned int out)
{
    JsonDocument *const document = parse->document;
    const char *const source = parse->source;
    skip_whitespace(parse);
    if (parse->at >= parse->size)
    {
        return false;
    }

    const char lead = source[parse->at];

    if (lead == '"')
    {
        document->node[out].type = JsonType::String;
        return parse_string(parse, &document->node[out].text);
    }
    if ((lead == '{') || (lead == '['))
    {
        const bool object = lead == '{';
        const char close = object ? '}' : ']';
        parse->at += 1u;
        document->node[out].type = object ? JsonType::Object : JsonType::Array;
        skip_whitespace(parse);
        if ((parse->at < parse->size) && (source[parse->at] == close))
        {
            parse->at += 1u;
            return true;
        }
        while (parse->at < parse->size)
        {
            unsigned int key = 0u;
            if (object)
            {
                skip_whitespace(parse);
                if (!parse_string(parse, &key))
                {
                    return false;
                }
                skip_whitespace(parse);
                if ((parse->at >= parse->size) || (source[parse->at] != ':'))
                {
                    return false;
                }
                parse->at += 1u;
            }
            const unsigned int child = node_taken(parse);
            if ((child == JSON_ABSENT) || !parse_value(parse, child))
            {
                return false;
            }
            document->node[child].key = key;
            child_joined(document, out, child);

            skip_whitespace(parse);
            if (parse->at >= parse->size)
            {
                return false;
            }
            if (source[parse->at] == ',')
            {
                parse->at += 1u;
                continue;
            }
            if (source[parse->at] == close)
            {
                parse->at += 1u;
                return true;
            }
            return false;
        }
        return false;
    }
    if (parse_word(parse, "true", 4u))
    {
        document->node[out].type = JsonType::Boolean;
        document->node[out].boolean = true;
        return true;
    }
    if (parse_word(parse, "false", 5u))
    {
        document->node[out].type = JsonType::Boolean;
        document->node[out].boolean = false;
        return true;
    }
    if (parse_word(parse, "null", 4u))
    {
        document->node[out].type = JsonType::Null;
        return true;
    }

    const size_t number_start = parse->at;
    parse->at += EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = &source[parse->at], .cap = parse->size - parse->at,
                            .other = "-+.eE0123456789");
    const size_t length = parse->at - number_start;
    char digits[64];
    if ((length == 0u) || (length >= sizeof(digits)))
    {
        return false;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = digits, .src = &source[number_start], .bytes = length);
    digits[length] = '\0';
    char *end = NULL;
    const double number = strtod(digits, &end);
    if (end == digits)
    {
        return false;
    }
    document->node[out].type = JsonType::Number;
    document->node[out].number = number;
    return true;
}

} // namespace json_detail

/**
 * @brief Parses one complete JSON document.
 *
 * @param[in]  source   Text to parse [BORROWS].
 * @param[in]  size     Bytes of the text.
 * @param[out] document Where the values land [BORROWS].
 * @return              The node of the value, or JSON_ABSENT where the document did not parse.
 */
inline unsigned int json_parse(const char *source, size_t size, JsonDocument *document)
{
    document->node_count = 0u;
    document->text_used = 0u;
    json_detail::JsonParse parse = {source, size, 0u, document};
    // node 0 is the absent value, and the empty text at offset 0 is the key of every element of an array
    const unsigned int absent = json_detail::node_taken(&parse);
    (void)absent;
    (void)json_detail::text_put(document, '\0');
    const unsigned int root = json_detail::node_taken(&parse);
    if (!json_detail::parse_value(&parse, root))
    {
        return JSON_ABSENT;
    }
    json_detail::skip_whitespace(&parse);
    return root;
}

#endif
