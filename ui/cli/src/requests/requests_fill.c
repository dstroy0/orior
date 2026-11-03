// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_fill.c: a text of a request with each `{{name}}` in it given its value: a variable's, a value that names
// others given theirs in turn eight deep, or one of the values JetBrains' HTTP Client makes as it sends
#include "requests_internal.h"

// how deep a value that names others is given theirs
#define CLI_REQUESTS_DEPTH 8u

typedef struct
{
    const char *text;
    unsigned long long size;
    unsigned long long at;
    const char *needle;
    const char *name;
    const char *word;
    VerbaLinea *out;
    VerbaLinea *err_msg;
    const CliRequestsValues *values;
    unsigned int depth;
} RequestsFillCtx;
// where `needle`, two bytes, first stands in `size` bytes of `text` from `at`: its place, or `size` where it does not
EMBED_INLINE unsigned long long fill_find(const RequestsFillCtx *args)
{
    unsigned long long at = args->at;
    while ((at + 1u < args->size) && ((args->text[at] != args->needle[0]) || (args->text[at + 1u] != args->needle[1])))
    {
        at += 1u;
    }
    return (at + 1u < args->size) ? at : args->size;
}

// the date and time `seconds` after 1970 began, as ISO 8601 in UTC
void cli_requests_iso(const CliRequestsOutSecondsCtx *args)
{
    const unsigned long long days = args->seconds / 86400u;
    const unsigned long long rest = args->seconds % 86400u;
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
    mmgr_line_decimal(args->out, year, 4u);
    mmgr_line_ch(args->out, '-');
    mmgr_line_decimal(args->out, month, 2u);
    mmgr_line_ch(args->out, '-');
    mmgr_line_decimal(args->out, day, 2u);
    mmgr_line_ch(args->out, 'T');
    mmgr_line_decimal(args->out, rest / 3600u, 2u);
    mmgr_line_ch(args->out, ':');
    mmgr_line_decimal(args->out, (rest % 3600u) / 60u, 2u);
    mmgr_line_ch(args->out, ':');
    mmgr_line_decimal(args->out, rest % 60u, 2u);
    mmgr_line_ch(args->out, 'Z');
}

// whether `size` bytes of `name` are `word`
EMBED_INLINE int fill_is(const RequestsFillCtx *args)
{
    return (cellul.len(&(CatenaFinitaCfg){.src = args->word, .cap = CLI_CELLS_POOL}) == args->size) &&
           (memor.cmp(&(MemoriaCfg){.src = args->name, .other = args->word, .bytes = (size_t)args->size}) == 0);
}

// the value JetBrains' HTTP Client makes for `name` written into `out`: 1, 0 where it makes none, or -1 with why
// reported where the system gave no randomness
EMBED_INLINE int fill_dynamic(const RequestsFillCtx *args)
{
    unsigned long long size = args->size;
    VerbaLinea *out = args->out;
    const unsigned long long seconds = cli_wall_milliseconds() / 1000u;
    unsigned char bytes[16];
    const int uuid = EMBED_CALL(fill_is, RequestsFillCtx, .name = args->name, .size = size, .word = "$uuid") ||
                     EMBED_CALL(fill_is, RequestsFillCtx, .name = args->name, .size = size, .word = "$random.uuid");
    const int number = EMBED_CALL(fill_is, RequestsFillCtx, .name = args->name, .size = size, .word = "$randomInt");
    if ((uuid || number) &&
        !EMBED_CALL(cli_size_bytes_internal.random_bytes, CliSizeBytesCtx, .bytes_out = bytes, .size = sizeof(bytes)))
    {
        mmgr_line_put(args->err_msg, "the system gave no randomness");
        return -1;
    }
    if (uuid)
    {
        static const char s_hex[] = "0123456789abcdef";
        bytes[6] = (unsigned char)((bytes[6] & 0x0fu) | 0x40u);
        bytes[8] = (unsigned char)((bytes[8] & 0x3fu) | 0x80u);
        for (unsigned int at = 0u; at < 16u; at += 1u)
        {
            mmgr_line_put(out, ((at == 4u) || (at == 6u) || (at == 8u) || (at == 10u)) ? "-" : "");
            mmgr_line_ch(out, s_hex[bytes[at] >> 4u]);
            mmgr_line_ch(out, s_hex[bytes[at] & 0x0fu]);
        }
        return 1;
    }
    if (number)
    {
        unsigned long long value = 0ULL;
        memor.cpy(&(MemoriaCfg){.dst = &value, .src = bytes, .bytes = sizeof(value)});
        mmgr_line_decimal(out, value % 1000u, 1u);
        return 1;
    }
    if (EMBED_CALL(fill_is, RequestsFillCtx, .name = args->name, .size = size, .word = "$timestamp"))
    {
        mmgr_line_decimal(out, seconds, 1u);
        return 1;
    }
    if (EMBED_CALL(fill_is, RequestsFillCtx, .name = args->name, .size = size, .word = "$isoTimestamp"))
    {
        EMBED_CALL(cli_requests_out_seconds_internal.iso, CliRequestsOutSecondsCtx, .seconds = seconds, .out = out);
        return 1;
    }
    return 0;
}

// `size` bytes of `text` filled into `out`, `depth` values deep: 1, or 0 with why given
static int fill_go(const RequestsFillCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    const CliRequestsValues *values = args->values;
    unsigned long long at = 0ULL;
    for (;;)
    {
        const unsigned long long open =
            EMBED_CALL(fill_find, RequestsFillCtx, .text = text, .size = size, .at = at, .needle = "{{");
        const unsigned long long close = (open < size) ? EMBED_CALL(fill_find, RequestsFillCtx, .text = text,
                                                                    .size = size, .at = open + 2u, .needle = "}}")
                                                       : size;
        if (close == size)
        {
            break;
        }
        mmgr_line_put_n(args->out, text + at, open - at);
        unsigned long long start = open + 2u;
        unsigned long long end = close;
        EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = text, .start = &start,
                   .end = &end);
        const CliRequestsText *const value =
            EMBED_CALL(cli_requests_size_name_internal.value, CliRequestsSizeNameCtx, .values_read = values,
                       .name = text + start, .size = end - start);
        if ((value != NULL) && (args->depth < CLI_REQUESTS_DEPTH))
        {
            if (!EMBED_CALL(fill_go, RequestsFillCtx, .text = values->text + value->at, .size = value->size,
                            .values = values, .depth = args->depth + 1u, .out = args->out, .err_msg = args->err_msg))
            {
                return 0;
            }
        }
        else if (value != NULL)
        {
            mmgr_line_put_n(args->out, values->text + value->at, value->size);
        }
        else
        {
            const int made = EMBED_CALL(fill_dynamic, RequestsFillCtx, .name = text + start, .size = end - start,
                                        .out = args->out, .err_msg = args->err_msg);
            if (made == 0)
            {
                mmgr_line_put(args->err_msg, "{{");
                mmgr_line_put_n(args->err_msg, text + start, end - start);
                mmgr_line_put(args->err_msg, "}} is set by no variable of the file or of the environment chosen");
            }
            if (made != 1)
            {
                return 0;
            }
        }
        at = close + 2u;
    }
    mmgr_line_put_n(args->out, text + at, size - at);
    return 1;
}

int cli_requests_fill(const CliRequestsTextSizeCtx *args)
{
    return EMBED_CALL(fill_go, RequestsFillCtx, .text = args->text, .size = args->size, .values = args->values_read,
                      .depth = 0u, .out = args->out, .err_msg = args->err_msg);
}
