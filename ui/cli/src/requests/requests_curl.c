// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// requests_curl.c: the request a curl line sends, written as a request of a file, a line copied for cmd, its lines
// ending in carets, read as cmd reads it; and the curl line that sends a request, for bash and the shells that read
// quotes as it does
#include "../command_line/cells.h"
#include "../text/base64.h"
#include "../text/json_tree.h"
#include "requests_internal.h"

// the most headers and pieces of data a curl line gives that are read
#define CLI_REQUESTS_CURL_MOST 4096u
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
    CurlText name[CLI_REQUESTS_CURL_MOST];
    CurlText value[CLI_REQUESTS_CURL_MOST];
    unsigned int headers;
    CurlText form_name[CLI_REQUESTS_CURL_MOST];
    CurlText form_value[CLI_REQUESTS_CURL_MOST];
    unsigned int forms;
    unsigned int data;
    int get;
} CurlRead;

// A reading's room, a cell taken with the lock and given back with it: the line's words, the line with its carets
// taken off, the texts made and the body, the request written, and what is read
typedef struct
{
    CliRequestsWords words;
    char uncareted[CLI_REQUESTS_CURL_TEXT];
    char made[CLI_REQUESTS_CURL_TEXT];
    VerbaLinea made_line;
    char body[CLI_REQUESTS_CURL_TEXT];
    VerbaLinea body_line;
    char written[CLI_REQUESTS_CURL_TEXT];
    CurlRead read;
} CliCurlRoom;

static CliCurlRoom *s_curl;
static CliLock s_lock = CLI_LOCK_INIT;

typedef struct
{
    const char *text;
    unsigned long long size;
    const char *word;
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
} RequestsCurlCtx;
// whether `size` bytes of `text` are `word`
EMBED_INLINE int curl_is(const RequestsCurlCtx *args)
{
    return (cellul.len(&(CatenaFinitaCfg){.src = args->word, .cap = CLI_CELLS_POOL}) == args->size) &&
           (memor.cmp(&(MemoriaCfg){.src = args->text, .other = args->word, .bytes = (size_t)args->size}) == 0);
}

// whether the line's headers name `lower`, ASCII case aside
EMBED_INLINE int curl_has(const RequestsCurlCtx *args)
{
    for (unsigned int at = 0u; at < args->read->headers; at += 1u)
    {
        if ((args->read->name[at].size == cellul.len(&(CatenaFinitaCfg){.src = args->lower, .cap = CLI_CELLS_POOL})) &&
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
    if (args->read_out->headers == CLI_REQUESTS_CURL_MOST)
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
    const CurlText made = {s_curl->made + s_curl->made_line.at, args->size};
    mmgr_line_put_n(&s_curl->made_line, args->bytes, args->size);
    return made;
}

// `size` bytes written as a form encodes them, each byte but a letter, a digit and `-_.~` as `%XX`
EMBED_INLINE void curl_encode(const RequestsCurlCtx *args)
{
    static const char s_hex[] = "0123456789ABCDEF";
    for (unsigned long long at = 0ull; at < args->size; at += 1ull)
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
    int takes = 0;
    for (unsigned int index = 0u; !takes && (index < sizeof(valued) / sizeof(valued[0])); index += 1u)
    {
        takes =
            EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = valued[index]);
    }
    if (takes && (value.at == NULL))
    {
        *at += 1u;
        if (*at >= s_curl->words.count)
        {
            mmgr_line_put_n(args->err_msg, args->flag.at, args->flag.size);
            mmgr_line_put(args->err_msg, " wants a value");
            return 0;
        }
        value = (CurlText){s_curl->words.text + s_curl->words.word[*at].at, s_curl->words.word[*at].size};
    }
    const char *const equals =
        (value.at != NULL) ? memor.chr(&(MemoriaCfg){.src = value.at, .val = '=', .bytes = (size_t)value.size}) : NULL;
    const char *const colon =
        (value.at != NULL) ? memor.chr(&(MemoriaCfg){.src = value.at, .val = ':', .bytes = (size_t)value.size}) : NULL;
    int fits = 1;
    if (EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = "-X") ||
        EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = "--request"))
    {
        args->read_out->method = value;
    }
    else if ((EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = "-H") ||
              EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size,
                         .word = "--header")) &&
             (colon != NULL))
    {
        unsigned long long name_start = 0ull;
        unsigned long long name_end = (unsigned long long)(colon - value.at);
        unsigned long long text_start = name_end + 1u;
        unsigned long long text_end = value.size;
        EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = value.at,
                   .start = &name_start, .end = &name_end);
        EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = value.at,
                   .start = &text_start, .end = &text_end);
        fits = EMBED_CALL(curl_header, RequestsCurlCtx, .read_out = args->read_out, .name = value.at + name_start,
                          .name_size = name_end - name_start, .value = value.at + text_start,
                          .value_size = text_end - text_start);
    }
    else if (EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = "-d") ||
             EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = "--data") ||
             EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size,
                        .word = "--data-raw") ||
             EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size,
                        .word = "--data-binary") ||
             EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size,
                        .word = "--data-ascii") ||
             EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = "--json"))
    {
        curl_data_begin(args);
        mmgr_line_put_n(&s_curl->body_line, value.at, value.size);
        static const char *const json[2] = {"Content-Type", "Accept"};
        static const char *const json_lower[2] = {"content-type", "accept"};
        for (unsigned int index = 0u;
             fits &&
             EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = "--json") &&
             (index < 2u);
             index += 1u)
        {
            fits = EMBED_CALL(curl_has, RequestsCurlCtx, .read = args->read_out, .lower = json_lower[index]) ||
                   EMBED_CALL(curl_header, RequestsCurlCtx, .read_out = args->read_out, .name = json[index],
                              .name_size = cellul.len(&(CatenaFinitaCfg){.src = json[index], .cap = CLI_CELLS_POOL}),
                              .value = "application/json", .value_size = 16u);
        }
    }
    else if (EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size,
                        .word = "--data-urlencode"))
    {
        curl_data_begin(args);
        const unsigned long long name = (equals != NULL) ? (unsigned long long)(equals - value.at) : 0ull;
        mmgr_line_put_n(&s_curl->body_line, value.at, (equals != NULL) ? (name + 1u) : 0u);
        EMBED_CALL(curl_encode, RequestsCurlCtx, .text = value.at + ((equals != NULL) ? (name + 1u) : 0u),
                   .size = value.size - ((equals != NULL) ? (name + 1u) : 0u), .out = &s_curl->body_line);
    }
    else if ((EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = "-F") ||
              EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = "--form")) &&
             (equals != NULL))
    {
        fits = (args->read_out->forms < CLI_REQUESTS_CURL_MOST);
        if (fits)
        {
            const unsigned long long name = (unsigned long long)(equals - value.at);
            args->read_out->form_name[args->read_out->forms] = (CurlText){value.at, name};
            args->read_out->form_value[args->read_out->forms] = (CurlText){equals + 1, value.size - name - 1u};
            args->read_out->forms += 1u;
        }
    }
    else if (EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = "-u") ||
             EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = "--user"))
    {
        const unsigned long long start = s_curl->made_line.at;
        mmgr_line_put(&s_curl->made_line, "Basic ");
        EMBED_CALL(cli_base64_table.base64, CliBase64Cfg, .out = &s_curl->made_line,
                   .bytes = (const unsigned char *)value.at, .size = value.size);
        fits = EMBED_CALL(curl_header, RequestsCurlCtx, .read_out = args->read_out, .name = "Authorization",
                          .name_size = 13u, .value = s_curl->made + start, .value_size = s_curl->made_line.at - start);
    }
    else if (EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = "-A") ||
             EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size,
                        .word = "--user-agent"))
    {
        fits = EMBED_CALL(curl_header, RequestsCurlCtx, .read_out = args->read_out, .name = "User-Agent",
                          .name_size = 10u, .value = value.at, .value_size = value.size);
    }
    else if (EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = "-e") ||
             EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = "--referer"))
    {
        fits = EMBED_CALL(curl_header, RequestsCurlCtx, .read_out = args->read_out, .name = "Referer", .name_size = 7u,
                          .value = value.at, .value_size = value.size);
    }
    else if (EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = "-b") ||
             EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = "--cookie"))
    {
        fits = EMBED_CALL(curl_header, RequestsCurlCtx, .read_out = args->read_out, .name = "Cookie", .name_size = 6u,
                          .value = value.at, .value_size = value.size);
    }
    else if (EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = "-G") ||
             EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = "--get"))
    {
        args->read_out->get = 1;
    }
    else if (EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = "-I") ||
             EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = "--head"))
    {
        args->read_out->method = EMBED_CALL(curl_init, RequestsCurlCtx, .bytes = "HEAD", .size = 4u);
    }
    else if (EMBED_CALL(curl_is, RequestsCurlCtx, .text = args->flag.at, .size = args->flag.size, .word = "--url"))
    {
        args->read_out->url = value;
    }
    else if (!takes && ((args->flag.size == 0u) || (args->flag.at[0] != '-')))
    {
        args->read_out->url = args->flag;
    }
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
                                  .src = s_separators, .cap = CLI_CELLS_POOL, .byte = args->path[end - 1u]}) != NULL))
        {
            end -= 1u;
        }
        const int dot =
            (end > 0u) && (args->path[end - 1u] == '.') &&
            ((end == 1u) || (cellul.chr(&(CatenaFinitaCfg){
                                 .src = s_separators, .cap = CLI_CELLS_POOL, .byte = args->path[end - 2u]}) != NULL));
        if (!dot || (end == 1u))
        {
            break;
        }
        end -= 1u;
    }
    unsigned long long begin = end;
    while ((begin > 0u) && (cellul.chr(&(CatenaFinitaCfg){
                                .src = s_separators, .cap = CLI_CELLS_POOL, .byte = args->path[begin - 1u]}) == NULL))
    {
        begin -= 1u;
    }
    const int none = (begin == end) ||
                     ((end - begin == 2u) && (args->path[begin] == '.') && (args->path[begin + 1u] == '.')) ||
                     ((end - begin == 1u) && (args->path[begin] == '.'));
    *start = begin;
    *length = none ? 0ull : (end - begin);
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
            unsigned long long start = 0ull;
            unsigned long long length = 0ull;
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
        (memor.chr(&(MemoriaCfg){.src = s_curl->body, .val = '\n', .bytes = (size_t)s_curl->body_line.at}) == NULL);
    mmgr_line_put(args->out, file ? "< " : "");
    mmgr_line_put_n(args->out, s_curl->body + (file ? 1u : 0u), s_curl->body_line.at - (file ? 1u : 0u));
    mmgr_line_ch(args->out, '\n');
}

// the words of the line read into the line read: 1, or 0 with why given
EMBED_INLINE int curl_read(const RequestsCurlCtx *args)
{
    memor.set(&(MemoriaCfg){.dst = &s_curl->read, .val = 0, .bytes = sizeof(s_curl->read)});
    s_curl->made_line = (VerbaLinea){s_curl->made, sizeof(s_curl->made), 0ull};
    s_curl->body_line = (VerbaLinea){s_curl->body, sizeof(s_curl->body), 0ull};
    for (unsigned int at = 1u; at < s_curl->words.count; at += 1u)
    {
        const CurlText word = {s_curl->words.text + s_curl->words.word[at].at, s_curl->words.word[at].size};
        const char *const equals = memor.chr(&(MemoriaCfg){.src = word.at, .val = '=', .bytes = (size_t)word.size});
        const int split = (word.size >= 2u) && (word.at[0] == '-') && (word.at[1] == '-') && (equals != NULL);
        const CurlText flag = split ? (CurlText){word.at, (unsigned long long)(equals - word.at)} : word;
        const CurlText joined = split ? (CurlText){equals + 1, word.size - (unsigned long long)(equals - word.at) - 1u}
                                      : (CurlText){NULL, 0ull};
        if (!EMBED_CALL(curl_flag, RequestsCurlCtx, .read_out = &s_curl->read, .flag = flag, .joined = joined,
                        .at = &at, .err_msg = args->err_msg))
        {
            return 0;
        }
    }
    if (!mmgr_line_ok(&s_curl->made_line) || !mmgr_line_ok(&s_curl->body_line))
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
    unsigned long long start = 0ull;
    unsigned long long end = args->size;
    EMBED_CALL(cli_requests_text_start_internal.trim, CliRequestsTextStartCtx, .text = args->line_text, .start = &start,
               .end = &end);
    const char *text = args->line_text + start;
    unsigned long long length = end - start;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    s_curl = g_cli_cells->persistent_buf_alloc(sizeof(*s_curl), &g_cli_requests_cells);
    if (s_curl == NULL)
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
        mmgr_line_put(args->err_msg, "a curl line's reading could not be taken");
        return 0;
    }
    const int cmd = EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx,
                               .text = text, .size = length, .needle = "^\n") ||
                    EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx,
                               .text = text, .size = length, .needle = "^\r\n") ||
                    EMBED_CALL(cli_requests_text_size_needle_internal.contains, CliRequestsTextSizeNeedleCtx,
                               .text = text, .size = length, .needle = "^\"");
    VerbaLinea uncareted = {s_curl->uncareted, sizeof(s_curl->uncareted), 0ull};
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
                 !EMBED_CALL(curl_is, RequestsCurlCtx, .text = s_curl->words.text + s_curl->words.word[0].at,
                             .size = s_curl->words.word[0].size, .word = "curl")))
    {
        mmgr_line_put(args->err_msg, "the text is no curl line");
        read = 0;
    }
    read = read && EMBED_CALL(curl_read, RequestsCurlCtx, .err_msg = args->err_msg);
    VerbaLinea written = {s_curl->written, sizeof(s_curl->written), 0ull};
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
    g_cli_cells->persistent_buf_release(s_curl, &g_cli_requests_cells);
    s_curl = NULL;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
    return read;
}
