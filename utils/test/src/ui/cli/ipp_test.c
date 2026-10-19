// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ipp_test.c: IPP's check: a message reads back as written, an address gives its host, port and path, and a CUPS
// server lists its printers, takes a job and cancels one. The last runs on the CUPS server orior's tests print to,
// where one runs: ORIOR_CUPS names it as host:port
#include "../../../../../ui/cli/src/print/ipp.h"
#include "cli_check.h"

#include "../../../../../ui/cli/src/text/text.h"

static CliIppMessage s_message;
static CliIppMessage s_back;
static char s_bytes[1u << 16u];
static char s_pdf[1u << 20u];

// whether a value of one message is a value of the other: its kind, tag, numbers and bytes
static int ipp_same_value(const CliIppValue *one, const CliIppValue *other)
{
    return (one->kind == other->kind) && (one->tag == other->tag) && (one->low == other->low) &&
           (one->high == other->high) && (one->length == other->length) &&
           (memor.cmp(&(MemoriaCfg){
                .src = s_message.bytes + one->bytes, .other = s_back.bytes + other->bytes, .bytes = one->length}) == 0);
}

// whether the message read back is the message written: its code, id, groups, attributes and values
static int ipp_same(void)
{
    int same = (s_message.code == s_back.code) && (s_message.request_id == s_back.request_id) &&
               (s_message.groups == s_back.groups) && (s_message.attributes == s_back.attributes) &&
               (s_message.values == s_back.values);
    for (unsigned int at = 0u; same && (at < s_message.groups); at += 1u)
    {
        same =
            (s_message.group[at].tag == s_back.group[at].tag) && (s_message.group[at].count == s_back.group[at].count);
    }
    for (unsigned int at = 0u; same && (at < s_message.attributes); at += 1u)
    {
        const CliIppAttribute *const one = &s_message.attribute[at];
        const CliIppAttribute *const other = &s_back.attribute[at];
        same = (one->name_length == other->name_length) && (one->count == other->count) &&
               (memor.cmp(&(MemoriaCfg){.src = s_message.bytes + one->name,
                                        .other = s_back.bytes + other->name,
                                        .bytes = one->name_length}) == 0);
    }
    for (unsigned int at = 0u; same && (at < s_message.values); at += 1u)
    {
        same = ipp_same_value(&s_message.value[at], &s_back.value[at]);
    }
    return same;
}

// a message reads back as written
static void ipp_message(void)
{
    cli_ipp_begun(&s_message, 0x0002u, 7u);
    cli_ipp_group(&s_message, CLI_IPP_OPERATION);
    cli_ipp_attribute(&s_message, "attributes-charset");
    cli_ipp_text(&s_message, CLI_IPP_CHARSET, "utf-8");
    cli_ipp_attribute(&s_message, "attributes-natural-language");
    cli_ipp_text(&s_message, CLI_IPP_LANGUAGE, "en");
    cli_ipp_attribute(&s_message, "printer-uri");
    cli_ipp_text(&s_message, CLI_IPP_URI, "ipp://printer.local:631/ipp/print");
    cli_ipp_attribute(&s_message, "job-name");
    cli_ipp_text(&s_message, CLI_IPP_NAME, "main.py");
    cli_ipp_group(&s_message, CLI_IPP_JOB);
    cli_ipp_attribute(&s_message, "copies");
    cli_ipp_number(&s_message, CLI_IPP_INTEGER, 2);
    cli_ipp_attribute(&s_message, "page-ranges");
    cli_ipp_range(&s_message, 1, 3);
    cli_ipp_range(&s_message, 5, 5);
    cli_ipp_attribute(&s_message, "orientation-requested");
    cli_ipp_number(&s_message, CLI_IPP_ENUM, 4);
    CliLine bytes = {s_bytes, sizeof(s_bytes), 0ull};
    cli_ipp_encode(&s_message, &bytes);
    cli_line_text(&bytes, "%PDF-1.4");
    static const unsigned char s_head[8] = {2u, 0u, 0u, 2u, 0u, 0u, 0u, 7u};
    cli_check((bytes.at > 8ull) && (memor.cmp(&(MemoriaCfg){.src = s_bytes, .other = s_head, .bytes = 8u}) == 0),
              "a message begins with its version, code and id");
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(cli_ipp_decode((const unsigned char *)s_bytes, bytes.at, &s_back, &said), "the message reads");
    cli_check(ipp_same(), "as it was written");
    cli_check((s_back.data_length == 8ull) &&
                  (memor.cmp(&(MemoriaCfg){.src = s_back.data, .other = "%PDF-1.4", .bytes = 8u}) == 0),
              "its document after it");
}

// whether `uri` reaches `host`, `port` and `path`
static int ipp_reaches(const char *uri, const char *host, unsigned int port, const char *path)
{
    char read_host[CLI_PATH_LONGEST];
    char read_path[CLI_PATH_LONGEST];
    unsigned int read_port = 0u;
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    return cli_ipp_address(uri, read_host, &read_port, read_path, &said) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = read_host, .other = host, .cap = CLI_CELLS_POOL})) &&
           (read_port == port) &&
           (cellul.eq(&(CatenaFinitaCfg){.src = read_path, .other = path, .cap = CLI_CELLS_POOL}));
}

// an address gives its host, port and path
static void ipp_addresses(void)
{
    cli_check(ipp_reaches("ipp://printer.local/ipp/print", "printer.local", 631u, "/ipp/print"),
              "an address with no port is IPP's");
    cli_check(ipp_reaches("ipp://10.0.0.5:8631/printers/office", "10.0.0.5", 8631u, "/printers/office"),
              "an address gives its port");
    cli_check(ipp_reaches("ipp://[fe80::1]:631/x", "fe80::1", 631u, "/x"), "an IPv6 host is read from its brackets");
    char host[CLI_PATH_LONGEST];
    char path[CLI_PATH_LONGEST];
    unsigned int port = 0u;
    char why[256];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(!cli_ipp_address("ipps://printer.local/ipp/print", host, &port, path, &said) &&
                  (cli_line_closed(&said) > 0ull) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = why, .cap = CLI_CELLS_POOL, .other = "encrypted", .other_cap = CLI_CELLS_POOL}) != NULL),
              "an ipps:// address is out of reach");
}

// `with`, a text of the CUPS server's printer's address, its `localhost` the server's host and its first `:631` the
// server's port, into `out`, which holds CLI_PATH_LONGEST
static void ipp_reached(const char *with, const char *host, unsigned int port, char *out)
{
    CliLine line = {out, CLI_PATH_LONGEST, 0ull};
    const char *const local = cellul.find(
        &(CatenaFinitaCfg){.src = with, .cap = CLI_CELLS_POOL, .other = "localhost", .other_cap = CLI_CELLS_POOL});
    const char *const from = (local != NULL) ? local + 9 : with;
    if (local != NULL)
    {
        // `localhost` lies in the address, at or past its start
        cli_line_counted(&line, with, (unsigned long long)(local - with));
        cli_line_text(&line, host);
    }
    const char *const default_port = cellul.find(
        &(CatenaFinitaCfg){.src = from, .cap = CLI_CELLS_POOL, .other = ":631", .other_cap = CLI_CELLS_POOL});
    if (default_port != NULL)
    {
        // `:631` lies in the address, at or past `from`
        cli_line_counted(&line, from, (unsigned long long)(default_port - from));
        cli_line_character(&line, ':');
        cli_line_decimal(&line, port, 1u);
        cli_line_text(&line, default_port + 4);
    }
    else
    {
        cli_line_text(&line, from);
    }
    (void)cli_line_closed(&line);
}

// a CUPS server lists its printers, takes a job and cancels one
static void ipp_cups(const char *host, unsigned int port)
{
    static CliIppShared s_shared[16];
    static CliIppPrinter s_printer;
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    const int count = cli_ipp_cups_printers(host, port, s_shared, 16u, &said);
    cli_check(count > 0, "the server shares a printer");
    if (count <= 0)
    {
        return;
    }
    char uri[CLI_PATH_LONGEST];
    ipp_reached(s_shared[0].uri, host, port, uri);
    cli_check(cli_ipp_printer(uri, &s_printer, &said), "the printer says what it is");
    int pdf = 0;
    for (unsigned int at = 0u; at < s_printer.format_count; at += 1u)
    {
        pdf |= cellul.eq(
            &(CatenaFinitaCfg){.src = s_printer.formats[at], .other = "application/pdf", .cap = CLI_CELLS_POOL});
    }
    cli_check(pdf, "and that it takes a PDF");
    static const char s_listing[] = "{\"title\":\"ipp test\",\"lines\":[{\"number\":1,\"runs\":[{\"text\":"
                                    "\"printed by orior\",\"color\":\"#000000\"}]}],\"tab\":4}";
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_check_parsed(s_listing, sizeof(s_listing) - 1u, &read);
    CliPrintListing *listing = NULL;
    CliPrintLayout layout;
    memor.set(&(MemoriaCfg){.dst = &layout, .val = 0, .bytes = sizeof(layout)});
    memor.cpy(&(MemoriaCfg){.dst = layout.paper,
                            .src = "a4",
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = "a4", .cap = CLI_CELLS_POOL}) + 1u});
    layout.copies = 1u;
    cli_print_take();
    (void)cli_print_listing_read(json, read, &listing, &said);
    CliLine document = {s_pdf, sizeof(s_pdf), 0ull};
    unsigned long long pages = 0ull;
    cli_print_pdf(listing, &layout, &document, &pages);
    cli_print_give();
    CliIppJob job = {"orior ipp test", "application/pdf", 1u, 0, 1, "iso_a4_210x297mm"};
    int id = 0;
    cli_check(cli_ipp_print(uri, &job, (const unsigned char *)s_pdf, document.at, &id, &said) && (id > 0),
              "a job is taken");
    int state = 0;
    char reasons_text[512];
    CliLine reasons = {reasons_text, sizeof(reasons_text), 0ull};
    cli_check(cli_ipp_job_state(uri, id, &state, &reasons, &said) && (state >= 3) && (state <= 9),
              "and its state told");
    job.name = "orior cancel test";
    int second = 0;
    (void)cli_ipp_print(uri, &job, (const unsigned char *)s_pdf, document.at, &second, &said);
    (void)cli_ipp_cancel(uri, second, &said);
    reasons.at = 0ull;
    cli_check(cli_ipp_job_state(uri, second, &state, &reasons, &said) && (state >= 7), "a canceled job ended");
    job.format = "application/x-orior-none";
    said.at = 0ull;
    cli_check(!cli_ipp_print(uri, &job, (const unsigned char *)"x", 1ull, &second, &said) &&
                  (cli_line_closed(&said) > 0ull) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = why, .cap = CLI_CELLS_POOL, .other = "format", .other_cap = CLI_CELLS_POOL}) != NULL),
              "a format the printer does not take is refused");
}

int main(void)
{
    ipp_message();
    ipp_addresses();
    char host[CLI_NAME_LONGEST];
    unsigned int port = 0u;
    char named[CLI_NAME_LONGEST];
    if (!query_environment_read("ORIOR_CUPS", named, sizeof(named)))
    {
        cli_check(1, "ORIOR_CUPS names no server, and nothing is printed");
        return cli_checked();
    }
    char variable_named[CLI_NAME_LONGEST + 16u];
    CliLine line = {variable_named, sizeof(variable_named), 0ull};
    cli_line_text(&line, named);
    (void)cli_line_closed(&line);
    char *const colon = cli_text_last(
        variable_named, cellul.len(&(CatenaFinitaCfg){.src = variable_named, .cap = CLI_CELLS_POOL}), ':');
    port = 0u;
    for (const char *digit = (colon != NULL) ? colon + 1 : ""; (digit[0] >= '0') && (digit[0] <= '9'); digit += 1)
    {
        port = (port * 10u) + (unsigned int)(digit[0] - '0');
    }
    if ((colon == NULL) || (port == 0u) || (port > 65535u))
    {
        cli_check(1, "ORIOR_CUPS names no host:port, and nothing is printed");
        return cli_checked();
    }
    colon[0] = '\0';
    memor.cpy(
        &(MemoriaCfg){.dst = host,
                      .src = variable_named,
                      .bytes = cellul.len(&(CatenaFinitaCfg){.src = variable_named, .cap = CLI_CELLS_POOL}) + 1u});
    ipp_cups(host, port);
    return cli_checked();
}
