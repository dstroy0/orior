// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_curl.c: the request a curl line sends, written as a request of a file, a line copied for cmd, its lines
// ending in carets, read as cmd reads it; and the curl line that sends a request, for bash and the shells that read
// quotes as it does
#include "../command_line/pool.h"
#include "../text/base64.h"
#include "../text/json_tree.h"
#include "requests_internal.h"

// the most headers and pieces of data a curl line gives that are read
#define CLI_REQUESTS_CURL_MAX 4096u
// A text of a curl line read: where it is and its length
typedef struct
{
    const char *at;
    unsigned long long size;
} CurlText;
// A curl line read: its method and address where it gives them, its headers, its data joined by `&`, its forms, and
// whether its data goes in the address
typedef struct
{
    CurlText method;
    CurlText url;
    CurlText name[CLI_REQUESTS_CURL_MAX];
    CurlText value[CLI_REQUESTS_CURL_MAX];
    unsigned int headers;
    CurlText form_name[CLI_REQUESTS_CURL_MAX];
    CurlText form_value[CLI_REQUESTS_CURL_MAX];
    unsigned int forms;
    unsigned int data;
    int get;
} CurlRead;

// A reading's work state, a cell taken with the lock and given back with it: the line's words, the line with its carets
// taken off, the texts made and the body, the request written, and what is read
typedef struct
{
    CliRequestsWords words;
    char uncareted[CLI_REQUESTS_CURL_TEXT];
    char made[CLI_REQUESTS_CURL_TEXT];
    VerbaLinea created_line;
    char body[CLI_REQUESTS_CURL_TEXT];
    VerbaLinea body_line;
    char written[CLI_REQUESTS_CURL_TEXT];
    CurlRead read;
} CliCurlWorkState;

static CliCurlWorkState *s_curl;

typedef struct
{
    const char *text;
    unsigned long long size;
    const char *word_text;
    const CurlRead *read;
    const char *lower;
    CurlRead *read_out;
    const char *name;
    unsigned long long name_size;
    const char *value;
    unsigned long long value_size;
    const char *bytes;
    VerbaLinea *out;
    CurlText flag;
    CurlText joined;
    unsigned int *at;
    VerbaLinea *err_msg;
    const char *path;
    unsigned long long *start;
    unsigned long long *length;
    const char *const *words;
    unsigned int count;
    int valued;
} RequestsCurlCtx;
// whether `size` bytes of `text` are `word`
EMBED_INLINE int curl_is(const RequestsCurlCtx *args)
{
    return (cellul.len(&(CatenaFinitaCfg){.src = args->word_text, .cap = CLI_POOL_BYTES}) == args->size) &&
           (memor.cmp(&(MemoriaCfg){.src = args->text, .other = args->word_text, .bytes = (size_t)args->size}) == 0);
}

// whether the line's headers name `lower`, ASCII case aside
EMBED_INLINE int curl_has(const RequestsCurlCtx *args)
{
    for (unsigned int at = 0u; at < args->read->headers; at += 1u)
    {
        if ((args->read->name[at].size == cellul.len(&(CatenaFinitaCfg){.src = args->lower, .cap = CLI_POOL_BYTES})) &&
            EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx,
                       .text = args->read->name[at].at, .size = args->read->name[at].size, .needle = args->lower))
        {
            return 1;
        }
    }
    return 0;
}

// a header put on the line read: 1, or 0 where it contains the most
EMBED_INLINE int curl_header(const RequestsCurlCtx *args)
{
    if (args->read_out->headers == CLI_REQUESTS_CURL_MAX)
    {
        return 0;
    }
    args->read_out->name[args->read_out->headers] = (CurlText){args->name, args->name_size};
    args->read_out->value[args->read_out->headers] = (CurlText){args->value, args->value_size};
    args->read_out->headers += 1u;
    return 1;
}

// `size` bytes made into the pool of texts made: where they stand
EMBED_INLINE CurlText curl_init(const RequestsCurlCtx *args)
{
    const CurlText made = {s_curl->made + s_curl->created_line.at, args->size};
    mmgr_line_put_n(&s_curl->created_line, args->bytes, args->size);
    return made;
}

// `size` bytes written as a form encodes them, each byte but a letter, a digit and `-_.~` as `%XX`
EMBED_INLINE void curl_encode(const RequestsCurlCtx *args)
{
    static const char s_hex[] = "0123456789ABCDEF";
    for (unsigned long long at = 0ULL; at < args->size; at += 1ULL)
    {
        const unsigned char byte = (unsigned char)args->text[at];
        const int plain = ((byte >= 'a') && (byte <= 'z')) || ((byte >= 'A') && (byte <= 'Z')) ||
                          ((byte >= '0') && (byte <= '9')) || (byte == '-') || (byte == '_') || (byte == '.') ||
                          (byte == '~');
        if (plain)
        {
            mmgr_line_ch(args->out, (char)byte);
        }
        else
        {
            mmgr_line_ch(args->out, '%');
            mmgr_line_ch(args->out, s_hex[byte >> 4u]);
            mmgr_line_ch(args->out, s_hex[byte & 0x0fu]);
        }
    }
}

// a piece of data put after those before it, joined by `&`
EMBED_INLINE void curl_data_begin(const RequestsCurlCtx *args)
{
    mmgr_line_put(&s_curl->body_line, (args->read_out->data > 0u) ? "&" : "");
    args->read_out->data += 1u;
}

// whether the flag `flag` is one of the `count` words of `words`
EMBED_INLINE int curl_among(const RequestsCurlCtx *args)
{
    int among = 0;
    unsigned int index = 0u;
    while (!among && (index < args->count))
    {
        among = EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size,
                           .word_text = args->words[index]);
        index += 1u;
    }
    return among;
}

// the header `name: text` of a -H, `value_size` bytes of `value`, put on the line read with each side trimmed: 1, or 0
// where the line contains the most headers; a value with no colon puts nothing
EMBED_INLINE int curl_header_line(const RequestsCurlCtx *args)
{
    const char *const colon =
        (args->value != NULL)
            ? memor.chr(&(MemoriaCfg){.src = args->value, .val = ':', .bytes = (size_t)args->value_size})
            : NULL;
    if (colon == NULL)
    {
        return 1;
    }
    unsigned long long name_start = 0ULL;
    unsigned long long name_end = (unsigned long long)(colon - args->value);
    unsigned long long text_start = name_end + 1u;
    unsigned long long text_end = args->value_size;
    EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = args->value,
               .start = &name_start, .end = &name_end);
    EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = args->value,
               .start = &text_start, .end = &text_end);
    return EMBED_CALL(curl_header, RequestsCurlCtx, .read_out = args->read_out, .name = args->value + name_start,
                      .name_size = name_end - name_start, .value = args->value + text_start,
                      .value_size = text_end - text_start);
}

// a piece of data of a -d or its kin, `value_size` bytes of `value`, put on the body, and for --json the headers that
// say the body and the answer are JSON where the line names neither: 1, or 0 where the line contains the most headers
EMBED_INLINE int curl_data_put(const RequestsCurlCtx *args)
{
    static const char *const json[2] = {"Content-Type", "Accept"};
    static const char *const json_lower[2] = {"content-type", "accept"};
    curl_data_begin(args);
    mmgr_line_put_n(&s_curl->body_line, args->value, args->value_size);
    const int sent_json =
        EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word_text = "--json");
    int fits = 1;
    unsigned int index = 0u;
    while (fits && sent_json && (index < 2u))
    {
        fits = EMBED_CALL(curl_has, RequestsCurlCtx, .read = args->read_out, .lower = json_lower[index]) ||
               EMBED_CALL(curl_header, RequestsCurlCtx, .read_out = args->read_out, .name = json[index],
                          .name_size = cellul.len(&(CatenaFinitaCfg){.src = json[index], .cap = CLI_POOL_BYTES}),
                          .value = "application/json", .value_size = 16u);
        index += 1u;
    }
    return fits;
}

// a piece of data of a --data-urlencode, `value_size` bytes of `value`, put on the body: the name and its `=` where it
// has one as they are, and the rest encoded as a form encodes it
EMBED_INLINE void curl_urlencoded_put(const RequestsCurlCtx *args)
{
    curl_data_begin(args);
    if (args->value == NULL)
    {
        return;
    }
    const char *const equals =
        memor.chr(&(MemoriaCfg){.src = args->value, .val = '=', .bytes = (size_t)args->value_size});
    const unsigned long long lead = (equals != NULL) ? ((unsigned long long)(equals - args->value) + 1u) : 0ULL;
    mmgr_line_put_n(&s_curl->body_line, args->value, lead);
    EMBED_CALL(curl_encode, RequestsCurlCtx, .text = args->value + lead, .size = args->value_size - lead,
               .out = &s_curl->body_line);
}

// the form `name=text` of a -F, `value_size` bytes of `value`, put on the line read: 1, or 0 where the line contains
// the most forms; a value with no `=` puts nothing
EMBED_INLINE int curl_form_put(const RequestsCurlCtx *args)
{
    const char *const equals =
        (args->value != NULL)
            ? memor.chr(&(MemoriaCfg){.src = args->value, .val = '=', .bytes = (size_t)args->value_size})
            : NULL;
    if (equals == NULL)
    {
        return 1;
    }
    const int fits = (args->read_out->forms < CLI_REQUESTS_CURL_MAX);
    if (fits)
    {
        const unsigned long long name = (unsigned long long)(equals - args->value);
        args->read_out->form_name[args->read_out->forms] = (CurlText){args->value, name};
        args->read_out->form_value[args->read_out->forms] = (CurlText){equals + 1, args->value_size - name - 1u};
        args->read_out->forms += 1u;
    }
    return fits;
}

// the user and password of a -u, `value_size` bytes of `value`, put on the line read as its basic authorization
// header: 1, or 0 where the line contains the most headers
EMBED_INLINE int curl_user_put(const RequestsCurlCtx *args)
{
    const unsigned long long start = s_curl->created_line.at;
    mmgr_line_put(&s_curl->created_line, "Basic ");
    EMBED_CALL(cli_base64_table.base64, CliBase64Cfg, .out = &s_curl->created_line,
               .bytes = (const unsigned char *)args->value, .size = args->value_size);
    return EMBED_CALL(curl_header, RequestsCurlCtx, .read_out = args->read_out, .name = "Authorization",
                      .name_size = 13u, .value = s_curl->made + start, .value_size = s_curl->created_line.at - start);
}

// the flag `flag` applied to the line read, `value_size` bytes of `value` its value, `valued` whether it is a flag that
// takes one; a word that is no flag is the address: 1, or 0 where the line contains the most of what the flag puts
EMBED_INLINE int curl_flag_applied(const RequestsCurlCtx *args)
{
    const CurlText flag = args->flag;
    const CurlText value = {args->value, args->value_size};
    int fits = 1;
    if (EMBED_CALL(curl_among, RequestsCurlCtx, .flag = flag, .words = (const char *const[]){"-X", "--request"},
                   .count = 2u))
    {
        args->read_out->method = value;
    }
    else if (EMBED_CALL(curl_among, RequestsCurlCtx, .flag = flag, .words = (const char *const[]){"-H", "--header"},
                        .count = 2u))
    {
        fits = EMBED_CALL(curl_header_line, RequestsCurlCtx, .read_out = args->read_out, .value = value.at,
                          .value_size = value.size);
    }
    else if (EMBED_CALL(curl_among, RequestsCurlCtx, .flag = flag,
                        .words = (const char *const[]){"-d", "--data", "--data-raw", "--data-binary", "--data-ascii",
                                                       "--json"},
                        .count = 6u))
    {
        fits = EMBED_CALL(curl_data_put, RequestsCurlCtx, .read_out = args->read_out, .flag = flag, .value = value.at,
                          .value_size = value.size);
    }
    else if (EMBED_CALL(curl_is, RequestsCurlCtx, .text = flag.at, .size = flag.size, .word_text = "--data-urlencode"))
    {
        EMBED_CALL(curl_urlencoded_put, RequestsCurlCtx, .read_out = args->read_out, .value = value.at,
                   .value_size = value.size);
    }
    else if (EMBED_CALL(curl_among, RequestsCurlCtx, .flag = flag, .words = (const char *const[]){"-F", "--form"},
                        .count = 2u))
    {
        fits = EMBED_CALL(curl_form_put, RequestsCurlCtx, .read_out = args->read_out, .value = value.at,
                          .value_size = value.size);
    }
    else if (EMBED_CALL(curl_among, RequestsCurlCtx, .flag = flag, .words = (const char *const[]){"-u", "--user"},
                        .count = 2u))
    {
        fits = EMBED_CALL(curl_user_put, RequestsCurlCtx, .read_out = args->read_out, .value = value.at,
                          .value_size = value.size);
    }
    else if (EMBED_CALL(curl_among, RequestsCurlCtx, .flag = flag, .words = (const char *const[]){"-A", "--user-agent"},
                        .count = 2u))
    {
        fits = EMBED_CALL(curl_header, RequestsCurlCtx, .read_out = args->read_out, .name = "User-Agent",
                          .name_size = 10u, .value = value.at, .value_size = value.size);
    }
    else if (EMBED_CALL(curl_among, RequestsCurlCtx, .flag = flag, .words = (const char *const[]){"-e", "--referer"},
                        .count = 2u))
    {
        fits = EMBED_CALL(curl_header, RequestsCurlCtx, .read_out = args->read_out, .name = "Referer", .name_size = 7u,
                          .value = value.at, .value_size = value.size);
    }
    else if (EMBED_CALL(curl_among, RequestsCurlCtx, .flag = flag, .words = (const char *const[]){"-b", "--cookie"},
                        .count = 2u))
    {
        fits = EMBED_CALL(curl_header, RequestsCurlCtx, .read_out = args->read_out, .name = "Cookie", .name_size = 6u,
                          .value = value.at, .value_size = value.size);
    }
    else if (EMBED_CALL(curl_among, RequestsCurlCtx, .flag = flag, .words = (const char *const[]){"-G", "--get"},
                        .count = 2u))
    {
        args->read_out->get = 1;
    }
    else if (EMBED_CALL(curl_among, RequestsCurlCtx, .flag = flag, .words = (const char *const[]){"-I", "--head"},
                        .count = 2u))
    {
        args->read_out->method = EMBED_CALL(curl_init, RequestsCurlCtx, .bytes = "HEAD", .size = 4u);
    }
    else if (EMBED_CALL(curl_is, RequestsCurlCtx, .text = flag.at, .size = flag.size, .word_text = "--url"))
    {
        args->read_out->url = value;
    }
    else if (!args->valued && ((flag.size == 0u) || (flag.at[0] != '-')))
    {
        args->read_out->url = flag;
    }
    return fits;
}

// the flag `flag` of the word at `*at` read, its value `value` where it is joined to it by `=`, else the word after
// it: 1, or 0 with why given
EMBED_INLINE int curl_flag(const RequestsCurlCtx *args)
{
    unsigned int *at = args->at;
    CurlText value = args->joined;
    static const char *const valued[] = {"-X",
                                         "--request",
                                         "-H",
                                         "--header",
                                         "-d",
                                         "--data",
                                         "--data-raw",
                                         "--data-binary",
                                         "--data-ascii",
                                         "--data-urlencode",
                                         "--json",
                                         "-F",
                                         "--form",
                                         "-u",
                                         "--user",
                                         "-A",
                                         "--user-agent",
                                         "-e",
                                         "--referer",
                                         "-b",
                                         "--cookie",
                                         "--url",
                                         "-o",
                                         "--output",
                                         "-w",
                                         "--write-out",
                                         "--connect-timeout",
                                         "-m",
                                         "--max-time",
                                         "--retry",
                                         "-x",
                                         "--proxy",
                                         "--cacert",
                                         "--cert",
                                         "--key",
                                         "-c",
                                         "--cookie-jar"};
    const int takes_value = EMBED_CALL(curl_among, RequestsCurlCtx, .flag = args->flag, .words = valued,
                                       .count = sizeof(valued) / sizeof(valued[0]));
    if (takes_value && (value.at == NULL))
    {
        *at += 1u;
        if (*at >= s_curl->words.count)
        {
            mmgr_line_put_n(args->err_msg, args->flag.at, args->flag.size);
            mmgr_line_put(args->err_msg, " wants a value");
            return 0;
        }
        value = (CurlText){s_curl->words.text + s_curl->words.word_text[*at].at, s_curl->words.word_text[*at].size};
    }
    const int fits = EMBED_CALL(curl_flag_applied, RequestsCurlCtx, .read_out = args->read_out, .flag = args->flag,
                                .value = value.at, .value_size = value.size, .valued = takes_value);
    if (!fits)
    {
        mmgr_line_put(args->err_msg, "the curl line is larger than orior_cli supports");
    }
    return fits;
}

// the name of the file at the end of `size` bytes of a path, as Rust's Path::file_name reads it: its start and length
// into `start` and `length`, none where the path ends in `..` or has no name
EMBED_INLINE void curl_file_name(const RequestsCurlCtx *args)
{
    unsigned long long *start = args->start;
    unsigned long long *length = args->length;
#if defined(_WIN32)
    static const char s_separators[] = "/\\";
#else
    static const char s_separators[] = "/";
#endif
    unsigned long long end = args->size;
    for (;;)
    {
        while ((end > 0u) && (cellul.chr(&(CatenaFinitaCfg){
                                  .src = s_separators, .cap = CLI_POOL_BYTES, .byte = args->path[end - 1u]}) != NULL))
        {
            end -= 1u;
        }
        const int dot =
            (end > 0u) && (args->path[end - 1u] == '.') &&
            ((end == 1u) || (cellul.chr(&(CatenaFinitaCfg){
                                 .src = s_separators, .cap = CLI_POOL_BYTES, .byte = args->path[end - 2u]}) != NULL));
        if (!dot || (end == 1u))
        {
            break;
        }
        end -= 1u;
    }
    unsigned long long begin = end;
    while ((begin > 0u) && (cellul.chr(&(CatenaFinitaCfg){
                                .src = s_separators, .cap = CLI_POOL_BYTES, .byte = args->path[begin - 1u]}) == NULL))
    {
        begin -= 1u;
    }
    const int none = (begin == end) ||
                     ((end - begin == 2u) && (args->path[begin] == '.') && (args->path[begin + 1u] == '.')) ||
                     ((end - begin == 1u) && (args->path[begin] == '.'));
    *start = begin;
    *length = none ? 0ULL : (end - begin);
}

// the forms of the line written as a multipart body into `out`
EMBED_INLINE void curl_forms(const RequestsCurlCtx *args)
{
    for (unsigned int at = 0u; at < args->read->forms; at += 1u)
    {
        const CurlText name = args->read->form_name[at];
        const CurlText value = args->read->form_value[at];
        mmgr_line_put(args->out, "--WebAppBoundary\nContent-Disposition: form-data; name=\"");
        mmgr_line_put_n(args->out, name.at, name.size);
        if ((value.size > 0u) && (value.at[0] == '@'))
        {
            unsigned long long start = 0ULL;
            unsigned long long length = 0ULL;
            EMBED_CALL(curl_file_name, RequestsCurlCtx, .path = value.at + 1, .size = value.size - 1u, .start = &start,
                       .length = &length);
            mmgr_line_put(args->out, "\"; filename=\"");
            mmgr_line_put_n(args->out, value.at + 1 + start, length);
            mmgr_line_put(args->out, "\"\n\n< ");
            mmgr_line_put_n(args->out, value.at + 1, value.size - 1u);
        }
        else
        {
            mmgr_line_put(args->out, "\"\n\n");
            mmgr_line_put_n(args->out, value.at, value.size);
        }
        mmgr_line_ch(args->out, '\n');
    }
    mmgr_line_put(args->out, "--WebAppBoundary--");
}

// the line read written as a request of a file into `out`
EMBED_INLINE void curl_write(const RequestsCurlCtx *args)
{
    const int moved = args->read->get && (s_curl->body_line.at > 0u);
    const int forms = (args->read->forms > 0u);
    const int body = forms || (!moved && (s_curl->body_line.at > 0u));
    if (args->read->method.at != NULL)
    {
        mmgr_line_put_n(args->out, args->read->method.at, args->read->method.size);
    }
    else
    {
        mmgr_line_put(args->out, body ? "POST" : "GET");
    }
    mmgr_line_ch(args->out, ' ');
    mmgr_line_put_n(args->out, args->read->url.at, args->read->url.size);
    if (moved)
    {
        mmgr_line_ch(args->out,
                     (memor.chr(&(MemoriaCfg){
                          .src = args->read->url.at, .val = '?', .bytes = (size_t)args->read->url.size}) != NULL)
                         ? '&'
                         : '?');
        mmgr_line_put_n(args->out, s_curl->body, s_curl->body_line.at);
    }
    mmgr_line_ch(args->out, '\n');
    for (unsigned int at = 0u; at < args->read->headers; at += 1u)
    {
        const int typed = (args->read->name[at].size == 12u) &&
                          EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx,
                                     .text = args->read->name[at].at, .size = 12u, .needle = "content-type");
        if (!(forms && typed))
        {
            mmgr_line_put_n(args->out, args->read->name[at].at, args->read->name[at].size);
            mmgr_line_put(args->out, ": ");
            mmgr_line_put_n(args->out, args->read->value[at].at, args->read->value[at].size);
            mmgr_line_ch(args->out, '\n');
        }
    }
    if (forms)
    {
        mmgr_line_put(args->out, "Content-Type: multipart/form-data; boundary=WebAppBoundary\n\n");
        curl_forms(args);
        mmgr_line_ch(args->out, '\n');
        return;
    }
    if (!body)
    {
        return;
    }
    if (!EMBED_CALL(curl_has, RequestsCurlCtx, .read = args->read, .lower = "content-type"))
    {
        mmgr_line_put(args->out, "Content-Type: application/x-www-form-urlencoded\n");
    }
    mmgr_line_ch(args->out, '\n');
    const int file =
        (s_curl->body[0] == '@') &&
        (memor.chr(&(MemoriaCfg){.src = s_curl->body, .val = '\n', .bytes = s_curl->body_line.at}) == NULL);
    mmgr_line_put(args->out, file ? "< " : "");
    mmgr_line_put_n(args->out, s_curl->body + (file ? 1u : 0u), s_curl->body_line.at - (file ? 1u : 0u));
    mmgr_line_ch(args->out, '\n');
}

// the words of the line read into the line read: 1, or 0 with why given
EMBED_INLINE int curl_read(const RequestsCurlCtx *args)
{
    memor.set(&(MemoriaCfg){.dst = &s_curl->read, .val = 0, .bytes = sizeof(s_curl->read)});
    s_curl->created_line = (VerbaLinea){s_curl->made, sizeof(s_curl->made), 0ULL};
    s_curl->body_line = (VerbaLinea){s_curl->body, sizeof(s_curl->body), 0ULL};
    for (unsigned int at = 1u; at < s_curl->words.count; at += 1u)
    {
        const CurlText word_text = {s_curl->words.text + s_curl->words.word_text[at].at,
                                    s_curl->words.word_text[at].size};
        const char *const equals =
            memor.chr(&(MemoriaCfg){.src = word_text.at, .val = '=', .bytes = (size_t)word_text.size});
        const int split =
            (word_text.size >= 2u) && (word_text.at[0] == '-') && (word_text.at[1] == '-') && (equals != NULL);
        const CurlText flag = split ? (CurlText){word_text.at, (unsigned long long)(equals - word_text.at)} : word_text;
        const CurlText joined =
            split ? (CurlText){equals + 1, word_text.size - (unsigned long long)(equals - word_text.at) - 1u}
                  : (CurlText){NULL, 0ULL};
        if (!EMBED_CALL(curl_flag, RequestsCurlCtx, .read_out = &s_curl->read, .flag = flag, .joined = joined,
                        .at = &at, .err_msg = args->err_msg))
        {
            return 0;
        }
    }
    if (!mmgr_line_ok(&s_curl->created_line) || !mmgr_line_ok(&s_curl->body_line))
    {
        mmgr_line_put(args->err_msg, "the curl line is larger than orior_cli supports");
        return 0;
    }
    if (s_curl->read.url.at == NULL)
    {
        mmgr_line_put(args->err_msg, "the curl line names no address");
        return 0;
    }
    return 1;
}

int cli_requests_from_curl(const CliRequestsCfg *args)
{
    unsigned long long start = 0ULL;
    unsigned long long end = args->size;
    EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = args->line_text, .start = &start,
               .end = &end);
    const char *text = args->line_text + start;
    unsigned long long length = end - start;
    s_curl = g_cli_guard->persistent_buf_alloc(sizeof(*s_curl), g_cli_requests_owner);
    if (s_curl == NULL)
    {
        mmgr_line_put(args->err_msg, "a curl line's reading could not be taken");
        return 0;
    }
    const int cmd = EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx,
                               .text = text, .size = length, .needle = "^\n") ||
                    EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx,
                               .text = text, .size = length, .needle = "^\r\n") ||
                    EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx,
                               .text = text, .size = length, .needle = "^\"");
    VerbaLinea uncareted = {s_curl->uncareted, sizeof(s_curl->uncareted), 0ULL};
    if (cmd)
    {
        EMBED_CALL(cli_requests_size_out_internal.uncareted, CliRequestsSizeOutCtx, .line = text, .size = length,
                   .out = &uncareted);
        text = s_curl->uncareted;
        length = uncareted.at;
    }
    int read = (!cmd || mmgr_line_ok(&uncareted)) &&
               EMBED_CALL(cli_requests_size_internal.words, CliRequestsSizeCtx, .line = text, .size = length,
                          .words = &s_curl->words, .err_msg = args->err_msg);
    if (cmd && !mmgr_line_ok(&uncareted))
    {
        mmgr_line_put(args->err_msg, "the curl line is larger than orior_cli supports");
    }
    if (read && ((s_curl->words.count == 0u) ||
                 !EMBED_CALL(curl_is, RequestsCurlCtx, .text = s_curl->words.text + s_curl->words.word_text[0].at,
                             .size = s_curl->words.word_text[0].size, .word_text = "curl")))
    {
        mmgr_line_put(args->err_msg, "the text is no curl line");
        read = 0;
    }
    read = read && EMBED_CALL(curl_read, RequestsCurlCtx, .err_msg = args->err_msg);
    VerbaLinea written = {s_curl->written, sizeof(s_curl->written), 0ULL};
    if (read)
    {
        EMBED_CALL(curl_write, RequestsCurlCtx, .read = &s_curl->read, .out = &written);
        read = mmgr_line_ok(&written);
        if (!read)
        {
            mmgr_line_put(args->err_msg, "the curl line is larger than orior_cli supports");
        }
    }
    if (read)
    {
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                   .text = s_curl->written, .length = written.at);
    }
    // a request's headers may carry a secret, and the pool leaves a cell as it is
    mmgr_zero_buf(s_curl, sizeof(*s_curl));
    g_cli_guard->persistent_buf_release(s_curl, g_cli_requests_owner);
    s_curl = NULL;
    return read;
}
