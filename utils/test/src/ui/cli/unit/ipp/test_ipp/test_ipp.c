// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// test_ipp.c: IPP: a message reads back as written, an address gives its host, port and path, and a CUPS server lists its printers, accepts a
// job and cancels one. The server tests run where ORIOR_CUPS names a server as host:port
#include "ui/cli/src/print/ipp.h"
#include "ui/cli/src/text/text.h"
#include "utils/test/src/ui/cli/unit/support/cli_unity.h"

static CliIppMessage s_message;
static CliIppMessage s_back;
static char s_bytes[1u << 16u];
static unsigned long long s_size;
static char s_pdf[1u << 20u];
static unsigned long long s_pdf_size;
static char s_error[512];
static VerbaLinea s_err_msg;
static int s_message_allocated;
static char s_uri[CLI_PATH_MAX_LEN];
static char s_host[CLI_NAME_MAX_LEN];
static unsigned int s_port;

// a message of two groups written and encoded with a document after it into s_bytes
static void message_encoded(void)
{
    EMBED_CALL(cli_ipp_message_code.begin, CliIppMessageCodeCfg, .message = &s_message, .code = 0x0002u, .request_id = 7u);
    s_message_allocated = 1;
    EMBED_CALL(cli_ipp_message_tag.group, CliIppMessageTagCfg, .message = &s_message, .tag = CLI_IPP_OPERATION);
    EMBED_CALL(cli_ipp_table.attribute, CliIppCfg, .message = &s_message, .name = "attributes-charset");
    EMBED_CALL(cli_ipp_message_tag.text, CliIppMessageTagCfg, .message = &s_message, .tag = CLI_IPP_CHARSET, .text = "utf-8");
    EMBED_CALL(cli_ipp_table.attribute, CliIppCfg, .message = &s_message, .name = "attributes-natural-language");
    EMBED_CALL(cli_ipp_message_tag.text, CliIppMessageTagCfg, .message = &s_message, .tag = CLI_IPP_LANGUAGE, .text = "en");
    EMBED_CALL(cli_ipp_table.attribute, CliIppCfg, .message = &s_message, .name = "printer-uri");
    EMBED_CALL(cli_ipp_message_tag.text, CliIppMessageTagCfg, .message = &s_message, .tag = CLI_IPP_URI, .text = "ipp://printer.local:631/ipp/print");
    EMBED_CALL(cli_ipp_table.attribute, CliIppCfg, .message = &s_message, .name = "job-name");
    EMBED_CALL(cli_ipp_message_tag.text, CliIppMessageTagCfg, .message = &s_message, .tag = CLI_IPP_NAME, .text = "main.py");
    EMBED_CALL(cli_ipp_message_tag.group, CliIppMessageTagCfg, .message = &s_message, .tag = CLI_IPP_JOB);
    EMBED_CALL(cli_ipp_table.attribute, CliIppCfg, .message = &s_message, .name = "copies");
    EMBED_CALL(cli_ipp_message_type.number, CliIppMessageTypeCfg, .message = &s_message, .type = CLI_IPP_INTEGER, .value = 2);
    EMBED_CALL(cli_ipp_table.attribute, CliIppCfg, .message = &s_message, .name = "page-ranges");
    EMBED_CALL(cli_ipp_message_low.range, CliIppMessageLowCfg, .message = &s_message, .low = 1, .high = 3);
    EMBED_CALL(cli_ipp_message_low.range, CliIppMessageLowCfg, .message = &s_message, .low = 5, .high = 5);
    EMBED_CALL(cli_ipp_table.attribute, CliIppCfg, .message = &s_message, .name = "orientation-requested");
    EMBED_CALL(cli_ipp_message_type.number, CliIppMessageTypeCfg, .message = &s_message, .type = CLI_IPP_ENUM, .value = 4);
    VerbaLinea bytes = {s_bytes, sizeof(s_bytes), 0ull};
    EMBED_CALL(cli_ipp_message_out_verba_linea.encode, CliIppMessageOutVerbaLineaCfg, .message_read = &s_message, .out_verba_linea_pointer = &bytes);
    mmgr_line_put(&bytes, "%PDF-1.4");
    s_size = bytes.at;
}

// the encoded message decoded into s_back
static void message_decoded(void)
{
    message_encoded();
    TEST_ASSERT_TRUE(EMBED_CALL(cli_ipp_message.decode, CliIppMessageCfg, .bytes = (const unsigned char *)s_bytes, .size = s_size, .message = &s_back, .err_msg = &s_err_msg));
}

// `uri` reaches `host`, `port` and `path`
static void assert_reaches(const char *uri, const char *host, unsigned int port, const char *path)
{
    char read_host[CLI_PATH_MAX_LEN];
    char read_path[CLI_PATH_MAX_LEN];
    unsigned int read_port = 0u;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_ipp_err_msg.address, CliIppErrMsgCfg, .uri = uri, .host = read_host, .port = &read_port, .path = read_path, .err_msg = &s_err_msg));
    TEST_ASSERT_EQUAL_STRING(host, read_host);
    TEST_ASSERT_EQUAL_UINT(port, read_port);
    TEST_ASSERT_EQUAL_STRING(path, read_path);
}

// the CUPS server ORIOR_CUPS names in s_host and s_port, the test ignored where it names none
static void server_required(void)
{
    char named[CLI_NAME_MAX_LEN];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "ORIOR_CUPS", .value_out = named, .capacity = sizeof(named)))
    {
        TEST_IGNORE_MESSAGE("ORIOR_CUPS names no server");
    }
    char *const colon = EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = named, .length_value = CLI_UNITY_LENGTH(named), .byte = ':');
    s_port = 0u;
    for (const char *digit = (colon != NULL) ? colon + 1 : ""; (digit[0] >= '0') && (digit[0] <= '9'); digit += 1)
    {
        // a digit's value is below 10
        s_port = (s_port * 10u) + (unsigned int)(digit[0] - '0');
    }
    if ((colon == NULL) || (s_port == 0u) || (s_port > 65535u))
    {
        TEST_IGNORE_MESSAGE("ORIOR_CUPS names no host:port");
    }
    colon[0] = '\0';
    memor.cpy(&(MemoriaCfg){.dst = s_host, .src = named, .bytes = CLI_UNITY_LENGTH(named) + 1ull});
}

// the server's first printer's address, its `localhost` the server's host and its first `:631` the server's port, in s_uri
static void printer_found(void)
{
    static CliIppShared s_shared[16];
    server_required();
    TEST_ASSERT_GREATER_THAN_INT(0, EMBED_CALL(cli_ipp_err_msg_host.cups_printers, CliIppErrMsgHostCfg, .host_read = s_host, .port_value = s_port, .shared = s_shared, .capacity_u32 = 16u, .err_msg = &s_err_msg));
    const char *const with = s_shared[0].uri;
    VerbaLinea line = {s_uri, sizeof(s_uri), 0ull};
    const char *const local = cellul.find(&(CatenaFinitaCfg){.src = with, .cap = CLI_POOL_BYTES, .other = "localhost", .other_cap = CLI_POOL_BYTES});
    const char *const from = (local != NULL) ? local + 9 : with;
    if (local != NULL)
    {
        // `localhost` lies in the address, at or past its start
        mmgr_line_put_n(&line, with, (unsigned long long)(local - with));
        mmgr_line_put(&line, s_host);
    }
    const char *const default_port = cellul.find(&(CatenaFinitaCfg){.src = from, .cap = CLI_POOL_BYTES, .other = ":631", .other_cap = CLI_POOL_BYTES});
    if (default_port != NULL)
    {
        // `:631` lies in the address, at or past `from`
        mmgr_line_put_n(&line, from, (unsigned long long)(default_port - from));
        mmgr_line_ch(&line, ':');
        mmgr_line_decimal(&line, s_port, 1u);
        mmgr_line_put(&line, default_port + 4);
    }
    else
    {
        mmgr_line_put(&line, from);
    }
    (void)mmgr_line_finish(&line);
}

// a one-page PDF in s_pdf
static void document_created(void)
{
    static const char s_listing[] = "{\"title\":\"ipp test\",\"lines\":[{\"number\":1,\"runs\":[{\"text\":\"printed by orior\",\"color\":\"#000000\"}]}],\"tab\":4}";
    unsigned int read = CLI_JSON_NONE;
    const CliJson *const json = cli_unity_parsed(s_listing, sizeof(s_listing) - 1u, &read);
    CliPrintListing *listing = NULL;
    CliPrintLayout layout;
    memor.set(&(MemoriaCfg){.dst = &layout, .val = 0, .bytes = sizeof(layout)});
    memor.cpy(&(MemoriaCfg){.dst = layout.paper, .src = "a4", .bytes = 3u});
    layout.copies = 1u;
    cli_print_alloc();
    (void)EMBED_CALL(cli_print_json.listing_read, CliPrintJsonCfg, .json = json, .node = read, .listing_pointer = &listing, .err_msg = &s_err_msg);
    VerbaLinea document = {s_pdf, sizeof(s_pdf), 0ull};
    unsigned long long pages = 0ull;
    EMBED_CALL(cli_print_layout_listing_out.pdf, CliPrintLayoutListingOutCfg, .listing = listing, .layout = &layout, .out = &document, .pages = &pages);
    cli_print_release();
    s_pdf_size = document.at;
}

void setUp(void)
{
    cli_unity_begun();
    s_err_msg = (VerbaLinea){s_error, sizeof(s_error), 0ull};
    s_message_allocated = 0;
}

void tearDown(void)
{
    if (s_message_allocated)
    {
        g_cli_guard->persistent_buf_release(s_message.group, g_cli_ipp_owner);
        *(&s_message) = (CliIppMessage){0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0ULL, 0};
        g_cli_guard->persistent_buf_release(s_back.group, g_cli_ipp_owner);
        *(&s_back) = (CliIppMessage){0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0ULL, 0};
    }
}

void test_a_message_begins_with_its_version_code_and_id(void)
{
    static const unsigned char s_head[8] = {2u, 0u, 0u, 2u, 0u, 0u, 0u, 7u};
    message_encoded();
    TEST_ASSERT_GREATER_THAN_UINT64(8ull, s_size);
    TEST_ASSERT_EQUAL_MEMORY(s_head, s_bytes, 8u);
}

void test_a_message_reads_its_code_id_and_counts_back(void)
{
    message_decoded();
    TEST_ASSERT_EQUAL_UINT(s_message.code, s_back.code);
    TEST_ASSERT_EQUAL_UINT(s_message.request_id, s_back.request_id);
    TEST_ASSERT_EQUAL_UINT(s_message.groups, s_back.groups);
    TEST_ASSERT_EQUAL_UINT(s_message.attributes, s_back.attributes);
    TEST_ASSERT_EQUAL_UINT(s_message.values, s_back.values);
}

void test_a_message_reads_its_groups_back(void)
{
    message_decoded();
    for (unsigned int at = 0u; at < s_message.groups; at += 1u)
    {
        TEST_ASSERT_EQUAL_UINT(s_message.group[at].tag, s_back.group[at].tag);
        TEST_ASSERT_EQUAL_UINT(s_message.group[at].count, s_back.group[at].count);
    }
}

void test_a_message_reads_its_attributes_back(void)
{
    message_decoded();
    for (unsigned int at = 0u; at < s_message.attributes; at += 1u)
    {
        const CliIppAttribute *const one = &s_message.attribute[at];
        const CliIppAttribute *const other = &s_back.attribute[at];
        TEST_ASSERT_EQUAL_UINT64(one->name_length, other->name_length);
        TEST_ASSERT_EQUAL_UINT(one->count, other->count);
        TEST_ASSERT_EQUAL_MEMORY(s_message.bytes + one->name, s_back.bytes + other->name, one->name_length);
    }
}

void test_a_message_reads_its_values_back(void)
{
    message_decoded();
    for (unsigned int at = 0u; at < s_message.values; at += 1u)
    {
        const CliIppValue *const one = &s_message.value[at];
        const CliIppValue *const other = &s_back.value[at];
        TEST_ASSERT_EQUAL_INT(one->type, other->type);
        TEST_ASSERT_EQUAL_UINT(one->tag, other->tag);
        TEST_ASSERT_EQUAL_INT(one->low, other->low);
        TEST_ASSERT_EQUAL_INT(one->high, other->high);
        TEST_ASSERT_EQUAL_UINT64(one->length, other->length);
        if (one->length > 0ull)
        {
            TEST_ASSERT_EQUAL_MEMORY(s_message.bytes + one->bytes, s_back.bytes + other->bytes, one->length);
        }
    }
}

void test_a_messages_document_follows_it(void)
{
    message_decoded();
    CLI_TEST_ASSERT_TEXT("%PDF-1.4", s_back.data, s_back.data_length);
}

void test_a_messages_memory_is_released_once_it_is_done(void)
{
    message_decoded();
    const unsigned long long in_use = mmgr_owner_in_use(g_cli_ipp_owner);
    g_cli_guard->persistent_buf_release(s_message.group, g_cli_ipp_owner);
    *(&s_message) = (CliIppMessage){0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0ULL, 0};
    g_cli_guard->persistent_buf_release(s_back.group, g_cli_ipp_owner);
    *(&s_back) = (CliIppMessage){0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0ULL, 0};
    s_message_allocated = 0;
    TEST_ASSERT_NOT_EQUAL(0ull, in_use);
    TEST_ASSERT_EQUAL_UINT64(0ull, mmgr_owner_in_use(g_cli_ipp_owner));
}

void test_an_address_with_no_port_uses_ipps(void)
{
    assert_reaches("ipp://printer.local/ipp/print", "printer.local", 631u, "/ipp/print");
}

void test_an_address_releases_its_port(void)
{
    assert_reaches("ipp://10.0.0.5:8631/printers/office", "10.0.0.5", 8631u, "/printers/office");
}

void test_an_ipv6_host_is_read_from_its_brackets(void)
{
    assert_reaches("ipp://[fe80::1]:631/x", "fe80::1", 631u, "/x");
}

void test_an_encrypted_address_is_refused(void)
{
    char host[CLI_PATH_MAX_LEN];
    char path[CLI_PATH_MAX_LEN];
    unsigned int port = 0u;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_ipp_err_msg.address, CliIppErrMsgCfg, .uri = "ipps://printer.local/ipp/print", .host = host, .port = &port, .path = path, .err_msg = &s_err_msg));
    TEST_ASSERT_GREATER_THAN_UINT64(0ull, mmgr_line_finish(&s_err_msg));
    CLI_TEST_ASSERT_CONTAINS("encrypted", s_error);
}

void test_a_job_is_accepted_and_its_state_reported(void)
{
    printer_found();
    document_created();
    CliIppJob job = {"impero ipp test", "application/pdf", 1u, 0, 1, "iso_a4_210x297mm"};
    int id = 0;
    TEST_ASSERT_TRUE(EMBED_CALL(cli_ipp_size_err_msg.print, CliIppSizeErrMsgCfg, .uri = s_uri, .job = &job, .document = (const unsigned char *)s_pdf, .size = s_pdf_size, .id_pointer = &id, .err_msg = &s_err_msg));
    TEST_ASSERT_GREATER_THAN_INT(0, id);
    int state = 0;
    char reasons_text[512];
    VerbaLinea reasons = {reasons_text, sizeof(reasons_text), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_ipp_err_msg_uri.job_state, CliIppErrMsgUriCfg, .uri = s_uri, .id = id, .state = &state, .reasons = &reasons, .err_msg = &s_err_msg));
    TEST_ASSERT_GREATER_OR_EQUAL_INT(3, state);
    TEST_ASSERT_LESS_OR_EQUAL_INT(9, state);
}

void test_a_canceled_job_ended(void)
{
    printer_found();
    document_created();
    CliIppJob job = {"impero cancel test", "application/pdf", 1u, 0, 1, "iso_a4_210x297mm"};
    int id = 0;
    (void)EMBED_CALL(cli_ipp_size_err_msg.print, CliIppSizeErrMsgCfg, .uri = s_uri, .job = &job, .document = (const unsigned char *)s_pdf, .size = s_pdf_size, .id_pointer = &id, .err_msg = &s_err_msg);
    (void)EMBED_CALL(cli_ipp_err_msg_uri.cancel, CliIppErrMsgUriCfg, .uri = s_uri, .id = id, .err_msg = &s_err_msg);
    int state = 0;
    char reasons_text[512];
    VerbaLinea reasons = {reasons_text, sizeof(reasons_text), 0ull};
    TEST_ASSERT_TRUE(EMBED_CALL(cli_ipp_err_msg_uri.job_state, CliIppErrMsgUriCfg, .uri = s_uri, .id = id, .state = &state, .reasons = &reasons, .err_msg = &s_err_msg));
    TEST_ASSERT_GREATER_OR_EQUAL_INT(7, state);
}

void test_a_format_the_printer_does_not_accept_is_refused(void)
{
    printer_found();
    CliIppJob job = {"impero format test", "application/x-orior-none", 1u, 0, 1, "iso_a4_210x297mm"};
    int id = 0;
    TEST_ASSERT_FALSE(EMBED_CALL(cli_ipp_size_err_msg.print, CliIppSizeErrMsgCfg, .uri = s_uri, .job = &job, .document = (const unsigned char *)"x", .size = 1ull, .id_pointer = &id, .err_msg = &s_err_msg));
    TEST_ASSERT_GREATER_THAN_UINT64(0ull, mmgr_line_finish(&s_err_msg));
    CLI_TEST_ASSERT_CONTAINS("format", s_error);
}
