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
 *       there. It does not stream, and it reads no comments. It has no duplicate key policy.
 */
#ifndef JSON_VALUE_H
#define JSON_VALUE_H

#include <cstdint>
#include <map>
#include <string>
#include <vector>

/** @brief What a parsed value turned out to be. */
enum class JsonKind
{
    Null,
    Boolean,
    Number,
    String,
    Array,
    Object
};

/** @brief One parsed JSON value. It owns its children. */
struct JsonValue
{
    JsonKind kind = JsonKind::Null;
    bool boolean = false;
    double number = 0.0;
    std::string text;
    std::vector<JsonValue> elements;
    std::map<std::string, JsonValue> members;

    /**
     * @brief Looks up an object member.
     *
     * @param[in] name Member to find.
     * @return         The member, or a null value where absent. Never fails.
     */
    const JsonValue &member(const std::string &name) const
    {
        static const JsonValue absent;
        if (kind != JsonKind::Object)
        {
            return absent;
        }
        const auto found = members.find(name);
        return (found == members.end()) ? absent : found->second;
    }

    /**
     * @brief Looks up an array element.
     *
     * @param[in] index Position to read.
     * @return          The element, or a null value where out of range. Never fails.
     */
    const JsonValue &at(size_t index) const
    {
        static const JsonValue absent;
        if ((kind != JsonKind::Array) || (index >= elements.size()))
        {
            return absent;
        }
        return elements[index];
    }

    /** @brief Reports whether this value is a string, which stratum fields mostly are. */
    bool is_string() const
    {
        return kind == JsonKind::String;
    }

    /** @brief Reports whether the value exists. */
    bool exists() const
    {
        return kind != JsonKind::Null;
    }
};

namespace json_detail
{

/**
 * @brief Advances past whitespace.
 *
 * @param[in]     source Text to parse.
 * @param[in,out] at     Cursor, moved past any whitespace.
 */
inline void skip_whitespace(const std::string &source, size_t &at)
{
    while ((at < source.size()) && ((source[at] == ' ') || (source[at] == '\t') ||
                                    (source[at] == '\r') || (source[at] == '\n')))
    {
        at += 1u;
    }
}

bool parse_value(const std::string &source, size_t &at, JsonValue &out);

/**
 * @brief Parses a quoted string. It resolves the escapes that stratum can actually contain.
 *
 * @param[in]     source Text to parse.
 * @param[in,out] at     Cursor, left past the closing quote on success.
 * @param[out]    out    Where the decoded characters land.
 * @return               True where the parser read a complete string.
 */
inline bool parse_string(const std::string &source, size_t &at, std::string &out)
{
    if ((at >= source.size()) || (source[at] != '"'))
    {
        return false;
    }
    at += 1u;
    out.clear();

    while (at < source.size())
    {
        const char character = source[at];

        if (character == '"')
        {
            at += 1u;
            return true;
        }
        if (character != '\\')
        {
            out.push_back(character);
            at += 1u;
            continue;
        }

        at += 1u;
        if (at >= source.size())
        {
            return false;
        }
        const char escaped = source[at];
        at += 1u;

        switch (escaped)
        {
        case 'n':
            out.push_back('\n');
            break;
        case 't':
            out.push_back('\t');
            break;
        case 'r':
            out.push_back('\r');
            break;
        case 'b':
            out.push_back('\b');
            break;
        case 'f':
            out.push_back('\f');
            break;
        case 'u':
        {
            if ((at + 4u) > source.size())
            {
                return false;
            }
            const unsigned code = (unsigned)std::stoul(source.substr(at, 4u), nullptr, 16);
            at += 4u;
            // Stratum carries hex and ASCII identifiers. The parser folds anything above the basic
            // range to a placeholder. It has no UTF-8 encoder, because that encoder would never run.
            out.push_back((code < 0x80u) ? (char)code : '?');
            break;
        }
        default:
            out.push_back(escaped);
            break;
        }
    }
    return false;
}

/**
 * @brief Parses one value of any kind.
 *
 * @param[in]     source Text to parse.
 * @param[in,out] at     Cursor, left past the value on success.
 * @param[out]    out    Where the value lands.
 * @return               True where the parser read a complete value.
 */
inline bool parse_value(const std::string &source, size_t &at, JsonValue &out)
{
    skip_whitespace(source, at);
    if (at >= source.size())
    {
        return false;
    }

    const char lead = source[at];

    if (lead == '"')
    {
        out.kind = JsonKind::String;
        return parse_string(source, at, out.text);
    }
    if (lead == '{')
    {
        at += 1u;
        out.kind = JsonKind::Object;
        skip_whitespace(source, at);
        if ((at < source.size()) && (source[at] == '}'))
        {
            at += 1u;
            return true;
        }
        while (at < source.size())
        {
            skip_whitespace(source, at);
            std::string key;
            if (!parse_string(source, at, key))
            {
                return false;
            }
            skip_whitespace(source, at);
            if ((at >= source.size()) || (source[at] != ':'))
            {
                return false;
            }
            at += 1u;

            JsonValue child;
            if (!parse_value(source, at, child))
            {
                return false;
            }
            out.members[key] = child;

            skip_whitespace(source, at);
            if (at >= source.size())
            {
                return false;
            }
            if (source[at] == ',')
            {
                at += 1u;
                continue;
            }
            if (source[at] == '}')
            {
                at += 1u;
                return true;
            }
            return false;
        }
        return false;
    }
    if (lead == '[')
    {
        at += 1u;
        out.kind = JsonKind::Array;
        skip_whitespace(source, at);
        if ((at < source.size()) && (source[at] == ']'))
        {
            at += 1u;
            return true;
        }
        while (at < source.size())
        {
            JsonValue child;
            if (!parse_value(source, at, child))
            {
                return false;
            }
            out.elements.push_back(child);

            skip_whitespace(source, at);
            if (at >= source.size())
            {
                return false;
            }
            if (source[at] == ',')
            {
                at += 1u;
                continue;
            }
            if (source[at] == ']')
            {
                at += 1u;
                return true;
            }
            return false;
        }
        return false;
    }
    if (source.compare(at, 4u, "true") == 0)
    {
        out.kind = JsonKind::Boolean;
        out.boolean = true;
        at += 4u;
        return true;
    }
    if (source.compare(at, 5u, "false") == 0)
    {
        out.kind = JsonKind::Boolean;
        out.boolean = false;
        at += 5u;
        return true;
    }
    if (source.compare(at, 4u, "null") == 0)
    {
        out.kind = JsonKind::Null;
        at += 4u;
        return true;
    }

    const size_t number_start = at;
    while ((at < source.size()) && ((source[at] == '-') || (source[at] == '+') ||
                                    (source[at] == '.') || (source[at] == 'e') ||
                                    (source[at] == 'E') ||
                                    ((source[at] >= '0') && (source[at] <= '9'))))
    {
        at += 1u;
    }
    if (at == number_start)
    {
        return false;
    }
    out.kind = JsonKind::Number;
    out.number = std::stod(source.substr(number_start, at - number_start));
    return true;
}

} // namespace json_detail

/**
 * @brief Parses one complete JSON document.
 *
 * @param[in]  source Text to parse.
 * @param[out] out    Where the value lands.
 * @return            True where the whole document parsed.
 */
inline bool json_parse(const std::string &source, JsonValue &out)
{
    size_t at = 0u;

    if (!json_detail::parse_value(source, at, out))
    {
        return false;
    }
    json_detail::skip_whitespace(source, at);
    return true;
}

#endif
