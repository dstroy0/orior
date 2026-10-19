// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_fill.c: a text of a request with each `{{name}}` in it given its value: a variable's, a value that names
// others given theirs in turn eight deep, or one of the values JetBrains' HTTP Client makes as it sends
#include "requests_internal.h"

// how deep a value that names others is given theirs
#define CLI_REQUESTS_DEPTH 8u

// where `needle`, two bytes, first stands in `size` bytes of `text` from `at`: its place, or `size` where it does not
static unsigned long long fill_found(const char *text, unsigned long long size, unsigned long long at,
                                     const char *needle)
{
    while ((at + 1u < size) && ((text[at] != needle[0]) || (text[at + 1u] != needle[1])))
    {
        at += 1u;
    }
    return (at + 1u < size) ? at : size;
}

// the date and time `seconds` after 1970 began, as ISO 8601 in UTC
void cli_requests_iso(unsigned long long seconds, CliLine *out)
{
    const unsigned long long days = seconds / 86400u;
    const unsigned long long rest = seconds % 86400u;
    const unsigned long long shifted = days + 719468u;
    const unsigned long long era = shifted / 146097u;
    const unsigned long long day_of_era = shifted - (era * 146097u);
    const unsigned long long year_of_era =
        (day_of_era - (day_of_era / 1460u) + (day_of_era / 36524u) - (day_of_era / 146096u)) / 365u;
    const unsigned long long day_of_year =
        day_of_era - ((365u * year_of_era) + (year_of_era / 4u) - (year_of_era / 100u));
    const unsigned long long month_index = ((5u * day_of_year) + 2u) / 153u;
    const unsigned long long day = day_of_year - (((153u * month_index) + 2u) / 5u) + 1u;
    const unsigned long long month = (month_index < 10u) ? (month_index + 3u) : (month_index - 9u);
    const unsigned long long year = year_of_era + (era * 400u) + ((month <= 2u) ? 1u : 0u);
    cli_line_decimal(out, year, 4u);
    cli_line_character(out, '-');
    cli_line_decimal(out, month, 2u);
    cli_line_character(out, '-');
    cli_line_decimal(out, day, 2u);
    cli_line_character(out, 'T');
    cli_line_decimal(out, rest / 3600u, 2u);
    cli_line_character(out, ':');
    cli_line_decimal(out, (rest % 3600u) / 60u, 2u);
    cli_line_character(out, ':');
    cli_line_decimal(out, rest % 60u, 2u);
    cli_line_character(out, 'Z');
}

// whether `size` bytes of `name` are `word`
static int fill_is(const char *name, unsigned long long size, const char *word)
{
    return (cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL}) == size) &&
           (memor.cmp(&(MemoriaCfg){.src = name, .other = word, .bytes = (size_t)size}) == 0);
}

// the value JetBrains' HTTP Client makes for `name` written into `out`: 1, 0 where it makes none, or -1 with why
// said where the system gave no randomness
static int fill_dynamic(const char *name, unsigned long long size, CliLine *out, CliLine *said)
{
    const unsigned long long seconds = cli_wall_milliseconds() / 1000u;
    unsigned char bytes[16];
    const int uuid = fill_is(name, size, "$uuid") || fill_is(name, size, "$random.uuid");
    const int number = fill_is(name, size, "$randomInt");
    if ((uuid || number) && !cli_random_bytes(bytes, sizeof(bytes)))
    {
        cli_line_text(said, "the system gave no randomness");
        return -1;
    }
    if (uuid)
    {
        static const char s_hex[] = "0123456789abcdef";
        bytes[6] = (unsigned char)((bytes[6] & 0x0fu) | 0x40u);
        bytes[8] = (unsigned char)((bytes[8] & 0x3fu) | 0x80u);
        for (unsigned int at = 0u; at < 16u; at += 1u)
        {
            cli_line_text(out, ((at == 4u) || (at == 6u) || (at == 8u) || (at == 10u)) ? "-" : "");
            cli_line_character(out, s_hex[bytes[at] >> 4u]);
            cli_line_character(out, s_hex[bytes[at] & 0x0fu]);
        }
        return 1;
    }
    if (number)
    {
        unsigned long long value = 0ull;
        memor.cpy(&(MemoriaCfg){.dst = &value, .src = bytes, .bytes = sizeof(value)});
        cli_line_decimal(out, value % 1000u, 1u);
        return 1;
    }
    if (fill_is(name, size, "$timestamp"))
    {
        cli_line_decimal(out, seconds, 1u);
        return 1;
    }
    if (fill_is(name, size, "$isoTimestamp"))
    {
        cli_requests_iso(seconds, out);
        return 1;
    }
    return 0;
}

// `size` bytes of `text` filled into `out`, `depth` values deep: 1, or 0 with why said
static int fill_go(const char *text, unsigned long long size, const CliRequestsValues *values, unsigned int depth,
                   CliLine *out, CliLine *said)
{
    unsigned long long at = 0ull;
    for (;;)
    {
        const unsigned long long open = fill_found(text, size, at, "{{");
        const unsigned long long close = (open < size) ? fill_found(text, size, open + 2u, "}}") : size;
        if (close == size)
        {
            break;
        }
        cli_line_counted(out, text + at, open - at);
        unsigned long long start = open + 2u;
        unsigned long long end = close;
        cli_requests_trimmed(text, &start, &end);
        const CliRequestsText *const value = cli_requests_value(values, text + start, end - start);
        if ((value != NULL) && (depth < CLI_REQUESTS_DEPTH))
        {
            if (!fill_go(values->text + value->at, value->size, values, depth + 1u, out, said))
            {
                return 0;
            }
        }
        else if (value != NULL)
        {
            cli_line_counted(out, values->text + value->at, value->size);
        }
        else
        {
            const int made = fill_dynamic(text + start, end - start, out, said);
            if (made == 0)
            {
                cli_line_text(said, "{{");
                cli_line_counted(said, text + start, end - start);
                cli_line_text(said, "}} is set by no variable of the file or of the environment chosen");
            }
            if (made != 1)
            {
                return 0;
            }
        }
        at = close + 2u;
    }
    cli_line_counted(out, text + at, size - at);
    return 1;
}

int cli_requests_filled(const char *text, unsigned long long size, const CliRequestsValues *values, CliLine *out,
                        CliLine *said)
{
    return fill_go(text, size, values, 0u, out, said);
}
