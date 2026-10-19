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
    CliLine made_line;
    char body[CLI_REQUESTS_CURL_TEXT];
    CliLine body_line;
    char written[CLI_REQUESTS_CURL_TEXT];
    CurlRead read;
} CliCurlRoom;

static CliCurlRoom *s_curl;
static CliLock s_lock = CLI_LOCK_INIT;

// whether `size` bytes of `text` are `word`
static int curl_is(const char *text, unsigned long long size, const char *word)
{
    return (cellul.len(&(CatenaFinitaCfg){.src = word, .cap = CLI_CELLS_POOL}) == size) &&
           (memor.cmp(&(MemoriaCfg){.src = text, .other = word, .bytes = (size_t)size}) == 0);
}

// whether the line's headers name `lower`, ASCII case aside
static int curl_has(const CurlRead *read, const char *lower)
{
    for (unsigned int at = 0u; at < read->headers; at += 1u)
    {
        if ((read->name[at].size == cellul.len(&(CatenaFinitaCfg){.src = lower, .cap = CLI_CELLS_POOL})) &&
            cli_requests_holds(read->name[at].at, read->name[at].size, lower))
        {
            return 1;
        }
    }
    return 0;
}

// a header put on the line read: 1, or 0 where it holds the most
static int curl_header(CurlRead *read, const char *name, unsigned long long name_size, const char *value,
                       unsigned long long value_size)
{
    if (read->headers == CLI_REQUESTS_CURL_MOST)
    {
        return 0;
    }
    read->name[read->headers] = (CurlText){name, name_size};
    read->value[read->headers] = (CurlText){value, value_size};
    read->headers += 1u;
    return 1;
}

// `size` bytes made into the pool of texts made: where they stand
static CurlText curl_made(const char *bytes, unsigned long long size)
{
    const CurlText made = {s_curl->made + s_curl->made_line.at, size};
    cli_line_counted(&s_curl->made_line, bytes, size);
    return made;
}

// `size` bytes written as a form encodes them, each byte but a letter, a digit and `-_.~` as `%XX`
static void curl_encoded(const char *text, unsigned long long size, CliLine *out)
{
    static const char s_hex[] = "0123456789ABCDEF";
    for (unsigned long long at = 0ull; at < size; at += 1ull)
    {
        const unsigned char byte = (unsigned char)text[at];
        const int plain = ((byte >= 'a') && (byte <= 'z')) || ((byte >= 'A') && (byte <= 'Z')) ||
                          ((byte >= '0') && (byte <= '9')) || (byte == '-') || (byte == '_') || (byte == '.') ||
                          (byte == '~');
        if (plain)
        {
            cli_line_character(out, (char)byte);
        }
        else
        {
            cli_line_character(out, '%');
            cli_line_character(out, s_hex[byte >> 4u]);
            cli_line_character(out, s_hex[byte & 0x0fu]);
        }
    }
}

// a piece of data put after those before it, joined by `&`
static void curl_data_begun(CurlRead *read)
{
    cli_line_text(&s_curl->body_line, (read->data > 0u) ? "&" : "");
    read->data += 1u;
}

// the flag `flag` of the word at `*at` read, its value `value` where it is joined to it by `=`, else the word after
// it: 1, or 0 with why said
static int curl_flag(CurlRead *read, CurlText flag, CurlText joined, unsigned int *at, CliLine *said)
{
    CurlText value = joined;
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
        takes = curl_is(flag.at, flag.size, valued[index]);
    }
    if (takes && (value.at == NULL))
    {
        *at += 1u;
        if (*at >= s_curl->words.count)
        {
            cli_line_counted(said, flag.at, flag.size);
            cli_line_text(said, " wants a value");
            return 0;
        }
        value = (CurlText){s_curl->words.text + s_curl->words.word[*at].at, s_curl->words.word[*at].size};
    }
    const char *const equals =
        (value.at != NULL) ? memor.chr(&(MemoriaCfg){.src = value.at, .val = '=', .bytes = (size_t)value.size}) : NULL;
    const char *const colon =
        (value.at != NULL) ? memor.chr(&(MemoriaCfg){.src = value.at, .val = ':', .bytes = (size_t)value.size}) : NULL;
    int fits = 1;
    if (curl_is(flag.at, flag.size, "-X") || curl_is(flag.at, flag.size, "--request"))
    {
        read->method = value;
    }
    else if ((curl_is(flag.at, flag.size, "-H") || curl_is(flag.at, flag.size, "--header")) && (colon != NULL))
    {
        unsigned long long name_start = 0ull;
        unsigned long long name_end = (unsigned long long)(colon - value.at);
        unsigned long long text_start = name_end + 1u;
        unsigned long long text_end = value.size;
        cli_requests_trimmed(value.at, &name_start, &name_end);
        cli_requests_trimmed(value.at, &text_start, &text_end);
        fits = curl_header(read, value.at + name_start, name_end - name_start, value.at + text_start,
                           text_end - text_start);
    }
    else if (curl_is(flag.at, flag.size, "-d") || curl_is(flag.at, flag.size, "--data") ||
             curl_is(flag.at, flag.size, "--data-raw") || curl_is(flag.at, flag.size, "--data-binary") ||
             curl_is(flag.at, flag.size, "--data-ascii") || curl_is(flag.at, flag.size, "--json"))
    {
        curl_data_begun(read);
        cli_line_counted(&s_curl->body_line, value.at, value.size);
        static const char *const json[2] = {"Content-Type", "Accept"};
        static const char *const json_lower[2] = {"content-type", "accept"};
        for (unsigned int index = 0u; fits && curl_is(flag.at, flag.size, "--json") && (index < 2u); index += 1u)
        {
            fits = curl_has(read, json_lower[index]) ||
                   curl_header(read, json[index],
                               cellul.len(&(CatenaFinitaCfg){.src = json[index], .cap = CLI_CELLS_POOL}),
                               "application/json", 16u);
        }
    }
    else if (curl_is(flag.at, flag.size, "--data-urlencode"))
    {
        curl_data_begun(read);
        const unsigned long long name = (equals != NULL) ? (unsigned long long)(equals - value.at) : 0ull;
        cli_line_counted(&s_curl->body_line, value.at, (equals != NULL) ? (name + 1u) : 0u);
        curl_encoded(value.at + ((equals != NULL) ? (name + 1u) : 0u),
                     value.size - ((equals != NULL) ? (name + 1u) : 0u), &s_curl->body_line);
    }
    else if ((curl_is(flag.at, flag.size, "-F") || curl_is(flag.at, flag.size, "--form")) && (equals != NULL))
    {
        fits = (read->forms < CLI_REQUESTS_CURL_MOST);
        if (fits)
        {
            const unsigned long long name = (unsigned long long)(equals - value.at);
            read->form_name[read->forms] = (CurlText){value.at, name};
            read->form_value[read->forms] = (CurlText){equals + 1, value.size - name - 1u};
            read->forms += 1u;
        }
    }
    else if (curl_is(flag.at, flag.size, "-u") || curl_is(flag.at, flag.size, "--user"))
    {
        const unsigned long long start = s_curl->made_line.at;
        cli_line_text(&s_curl->made_line, "Basic ");
        cli_base64(&s_curl->made_line, (const unsigned char *)value.at, value.size);
        fits = curl_header(read, "Authorization", 13u, s_curl->made + start, s_curl->made_line.at - start);
    }
    else if (curl_is(flag.at, flag.size, "-A") || curl_is(flag.at, flag.size, "--user-agent"))
    {
        fits = curl_header(read, "User-Agent", 10u, value.at, value.size);
    }
    else if (curl_is(flag.at, flag.size, "-e") || curl_is(flag.at, flag.size, "--referer"))
    {
        fits = curl_header(read, "Referer", 7u, value.at, value.size);
    }
    else if (curl_is(flag.at, flag.size, "-b") || curl_is(flag.at, flag.size, "--cookie"))
    {
        fits = curl_header(read, "Cookie", 6u, value.at, value.size);
    }
    else if (curl_is(flag.at, flag.size, "-G") || curl_is(flag.at, flag.size, "--get"))
    {
        read->get = 1;
    }
    else if (curl_is(flag.at, flag.size, "-I") || curl_is(flag.at, flag.size, "--head"))
    {
        read->method = curl_made("HEAD", 4u);
    }
    else if (curl_is(flag.at, flag.size, "--url"))
    {
        read->url = value;
    }
    else if (!takes && ((flag.size == 0u) || (flag.at[0] != '-')))
    {
        read->url = flag;
    }
    if (!fits)
    {
        cli_line_text(said, "the curl line is larger than orior holds");
    }
    return fits;
}

// the name of the file at the end of `size` bytes of a path, as Rust's Path::file_name reads it: its start and length
// into `start` and `length`, none where the path ends in `..` or has no name
static void curl_file_name(const char *path, unsigned long long size, unsigned long long *start,
                           unsigned long long *length)
{
#if defined(_WIN32)
    static const char s_separators[] = "/\\";
#else
    static const char s_separators[] = "/";
#endif
    unsigned long long end = size;
    for (;;)
    {
        while ((end > 0u) && (cellul.chr(&(CatenaFinitaCfg){
                                  .src = s_separators, .cap = CLI_CELLS_POOL, .byte = path[end - 1u]}) != NULL))
        {
            end -= 1u;
        }
        const int dot =
            (end > 0u) && (path[end - 1u] == '.') &&
            ((end == 1u) || (cellul.chr(&(CatenaFinitaCfg){
                                 .src = s_separators, .cap = CLI_CELLS_POOL, .byte = path[end - 2u]}) != NULL));
        if (!dot || (end == 1u))
        {
            break;
        }
        end -= 1u;
    }
    unsigned long long begin = end;
    while ((begin > 0u) && (cellul.chr(&(CatenaFinitaCfg){
                                .src = s_separators, .cap = CLI_CELLS_POOL, .byte = path[begin - 1u]}) == NULL))
    {
        begin -= 1u;
    }
    const int none = (begin == end) || ((end - begin == 2u) && (path[begin] == '.') && (path[begin + 1u] == '.')) ||
                     ((end - begin == 1u) && (path[begin] == '.'));
    *start = begin;
    *length = none ? 0ull : (end - begin);
}

// the forms of the line written as a multipart body into `out`
static void curl_forms(const CurlRead *read, CliLine *out)
{
    for (unsigned int at = 0u; at < read->forms; at += 1u)
    {
        const CurlText name = read->form_name[at];
        const CurlText value = read->form_value[at];
        cli_line_text(out, "--WebAppBoundary\nContent-Disposition: form-data; name=\"");
        cli_line_counted(out, name.at, name.size);
        if ((value.size > 0u) && (value.at[0] == '@'))
        {
            unsigned long long start = 0ull;
            unsigned long long length = 0ull;
            curl_file_name(value.at + 1, value.size - 1u, &start, &length);
            cli_line_text(out, "\"; filename=\"");
            cli_line_counted(out, value.at + 1 + start, length);
            cli_line_text(out, "\"\n\n< ");
            cli_line_counted(out, value.at + 1, value.size - 1u);
        }
        else
        {
            cli_line_text(out, "\"\n\n");
            cli_line_counted(out, value.at, value.size);
        }
        cli_line_character(out, '\n');
    }
    cli_line_text(out, "--WebAppBoundary--");
}

// the line read written as a request of a file into `out`
static void curl_written(const CurlRead *read, CliLine *out)
{
    const int moved = read->get && (s_curl->body_line.at > 0u);
    const int forms = (read->forms > 0u);
    const int body = forms || (!moved && (s_curl->body_line.at > 0u));
    if (read->method.at != NULL)
    {
        cli_line_counted(out, read->method.at, read->method.size);
    }
    else
    {
        cli_line_text(out, body ? "POST" : "GET");
    }
    cli_line_character(out, ' ');
    cli_line_counted(out, read->url.at, read->url.size);
    if (moved)
    {
        cli_line_character(
            out, (memor.chr(&(MemoriaCfg){.src = read->url.at, .val = '?', .bytes = (size_t)read->url.size}) != NULL)
                     ? '&'
                     : '?');
        cli_line_counted(out, s_curl->body, s_curl->body_line.at);
    }
    cli_line_character(out, '\n');
    for (unsigned int at = 0u; at < read->headers; at += 1u)
    {
        const int typed = (read->name[at].size == 12u) && cli_requests_holds(read->name[at].at, 12u, "content-type");
        if (!(forms && typed))
        {
            cli_line_counted(out, read->name[at].at, read->name[at].size);
            cli_line_text(out, ": ");
            cli_line_counted(out, read->value[at].at, read->value[at].size);
            cli_line_character(out, '\n');
        }
    }
    if (forms)
    {
        cli_line_text(out, "Content-Type: multipart/form-data; boundary=WebAppBoundary\n\n");
        curl_forms(read, out);
        cli_line_character(out, '\n');
        return;
    }
    if (!body)
    {
        return;
    }
    if (!curl_has(read, "content-type"))
    {
        cli_line_text(out, "Content-Type: application/x-www-form-urlencoded\n");
    }
    cli_line_character(out, '\n');
    const int file =
        (s_curl->body[0] == '@') &&
        (memor.chr(&(MemoriaCfg){.src = s_curl->body, .val = '\n', .bytes = (size_t)s_curl->body_line.at}) == NULL);
    cli_line_text(out, file ? "< " : "");
    cli_line_counted(out, s_curl->body + (file ? 1u : 0u), s_curl->body_line.at - (file ? 1u : 0u));
    cli_line_character(out, '\n');
}

// the words of the line read into the line read: 1, or 0 with why said
static int curl_read(CliLine *said)
{
    memor.set(&(MemoriaCfg){.dst = &s_curl->read, .val = 0, .bytes = sizeof(s_curl->read)});
    s_curl->made_line = (CliLine){s_curl->made, sizeof(s_curl->made), 0ull};
    s_curl->body_line = (CliLine){s_curl->body, sizeof(s_curl->body), 0ull};
    for (unsigned int at = 1u; at < s_curl->words.count; at += 1u)
    {
        const CurlText word = {s_curl->words.text + s_curl->words.word[at].at, s_curl->words.word[at].size};
        const char *const equals = memor.chr(&(MemoriaCfg){.src = word.at, .val = '=', .bytes = (size_t)word.size});
        const int split = (word.size >= 2u) && (word.at[0] == '-') && (word.at[1] == '-') && (equals != NULL);
        const CurlText flag = split ? (CurlText){word.at, (unsigned long long)(equals - word.at)} : word;
        const CurlText joined = split ? (CurlText){equals + 1, word.size - (unsigned long long)(equals - word.at) - 1u}
                                      : (CurlText){NULL, 0ull};
        if (!curl_flag(&s_curl->read, flag, joined, &at, said))
        {
            return 0;
        }
    }
    if (!cli_line_fits(&s_curl->made_line) || !cli_line_fits(&s_curl->body_line))
    {
        cli_line_text(said, "the curl line is larger than orior holds");
        return 0;
    }
    if (s_curl->read.url.at == NULL)
    {
        cli_line_text(said, "the curl line names no address");
        return 0;
    }
    return 1;
}

int cli_requests_from_curl(const char *line, unsigned long long size, CliLine *out, CliLine *said)
{
    unsigned long long start = 0ull;
    unsigned long long end = size;
    cli_requests_trimmed(line, &start, &end);
    const char *text = line + start;
    unsigned long long length = end - start;
    cli_lock_take(&s_lock);
    s_curl = cli_cell_taken(sizeof(*s_curl), "a curl line's reading");
    if (s_curl == NULL)
    {
        cli_lock_give(&s_lock);
        cli_line_text(said, "a curl line's reading could not be taken");
        return 0;
    }
    const int cmd = cli_requests_holds(text, length, "^\n") || cli_requests_holds(text, length, "^\r\n") ||
                    cli_requests_holds(text, length, "^\"");
    CliLine uncareted = {s_curl->uncareted, sizeof(s_curl->uncareted), 0ull};
    if (cmd)
    {
        cli_requests_uncareted(text, length, &uncareted);
        text = s_curl->uncareted;
        length = uncareted.at;
    }
    int read = (!cmd || cli_line_fits(&uncareted)) && cli_requests_words(text, length, &s_curl->words, said);
    if (cmd && !cli_line_fits(&uncareted))
    {
        cli_line_text(said, "the curl line is larger than orior holds");
    }
    if (read && ((s_curl->words.count == 0u) ||
                 !curl_is(s_curl->words.text + s_curl->words.word[0].at, s_curl->words.word[0].size, "curl")))
    {
        cli_line_text(said, "the text is no curl line");
        read = 0;
    }
    read = read && curl_read(said);
    CliLine written = {s_curl->written, sizeof(s_curl->written), 0ull};
    if (read)
    {
        curl_written(&s_curl->read, &written);
        read = cli_line_fits(&written);
        if (!read)
        {
            cli_line_text(said, "the curl line is larger than orior holds");
        }
    }
    if (read)
    {
        cli_json_quoted(out, s_curl->written, written.at);
    }
    cli_cell_given(s_curl);
    s_curl = NULL;
    cli_lock_give(&s_lock);
    return read;
}
