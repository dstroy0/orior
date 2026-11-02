// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ipp_jobs.c: what printing requests of a printer by IPP: its attributes, a job printed, a job's state, a job canceled,
// and the printers a CUPS server shares
#include "ipp.h"

// the request and the answer, under the lock while one is sent; and the id of the next request
static CliIppMessage s_request;
static CliIppMessage s_answer;
static unsigned int s_next = 1u;

// the request and the answer given back once the answer is read, and the lock given with them
static void cli_jobs_let_go(void)
{
    EMBED_CALL(cli_ipp_message_code.free, CliIppMessageCodeCfg, .message = &s_request);
    EMBED_CALL(cli_ipp_message_code.free, CliIppMessageCodeCfg, .message = &s_answer);
}

int cli_ipp_status(const CliIppCodeCfg *args)
{
    if (args->code <= 0x00ffu)
    {
        mmgr_line_put(args->err_msg, "done");
        return 1;
    }
    static const unsigned int s_codes[15] = {0x0400u, 0x0401u, 0x0402u, 0x0403u, 0x0404u, 0x0406u, 0x0407u, 0x040au,
                                             0x040bu, 0x0500u, 0x0501u, 0x0502u, 0x0503u, 0x0506u, 0x0507u};
    static const char *const s_status_msgs[15] = {
        "the printer did not understand the request",
        "the printer refuses jobs from this machine",
        "the printer requires a user name, and takes jobs only from a user it knows",
        "the printer requires a user name, and takes jobs only from a user it knows",
        "the printer cannot do what was requested now",
        "the printer has no such job or queue",
        "the job is gone",
        "the printer does not take the document's format",
        "the printer does not take a setting the job requested",
        "the printer failed inside",
        "the printer does not do this",
        "the printer does not support this version of IPP",
        "the printer is not taking jobs now",
        "the printer is not accepting jobs",
        "the printer is busy"};
    for (unsigned int at = 0u; at < 15u; at += 1u)
    {
        if (s_codes[at] == args->code)
        {
            mmgr_line_put(args->err_msg, s_status_msgs[at]);
            return 0;
        }
    }
    static const char s_hex[17] = "0123456789ABCDEF";
    mmgr_line_put(args->err_msg, "the printer answered status 0x");
    for (unsigned int shift = 16u; shift > 0u; shift -= 4u)
    {
        mmgr_line_ch(args->err_msg, s_hex[(args->code >> (shift - 4u)) & 0x0fu]);
    }
    return 0;
}

typedef struct
{
    CliIppMessage *request;
    unsigned int code;
    const char *uri;
    int with_uri;
    const unsigned char *document;
    unsigned long long size;
    VerbaLinea *err_msg;
    const CliIppGroup *group;
    const char *name;
    char (*list)[CLI_NAME_MAX_LEN];
    int truth;
} IppJobsCtx;
// the request begun with the operation group every request opens with, the printer's address in it
EMBED_INLINE void cli_operation(const IppJobsCtx *args)
{
    unsigned int code = args->code;
    char user[CLI_NAME_MAX_LEN];
    if (!EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "USERNAME",
                    .value_out = user, .capacity = sizeof(user)) &&
        !EMBED_CALL(query_name_value_internal.environment_read, QueryNameValueCtx, .name = "USER", .value_out = user,
                    .capacity = sizeof(user)))
    {
        memor.cpy(&(MemoriaCfg){.dst = user,
                                .src = "orior",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "orior", .cap = CLI_CELLS_POOL}) + 1u});
    }
    EMBED_CALL(cli_ipp_message_code.begin, CliIppMessageCodeCfg, .message = args->request, .code = code,
               .request_id = s_next);
    s_next += 1u;
    EMBED_CALL(cli_ipp_message_tag.group, CliIppMessageTagCfg, .message = args->request, .tag = CLI_IPP_OPERATION);
    EMBED_CALL(cli_ipp_table.attribute, CliIppCfg, .message = args->request, .name = "attributes-charset");
    EMBED_CALL(cli_ipp_message_tag.text, CliIppMessageTagCfg, .message = args->request, .tag = CLI_IPP_CHARSET,
               .text = "utf-8");
    EMBED_CALL(cli_ipp_table.attribute, CliIppCfg, .message = args->request, .name = "attributes-natural-language");
    EMBED_CALL(cli_ipp_message_tag.text, CliIppMessageTagCfg, .message = args->request, .tag = CLI_IPP_LANGUAGE,
               .text = "en");
    if (args->with_uri)
    {
        EMBED_CALL(cli_ipp_table.attribute, CliIppCfg, .message = args->request, .name = "printer-uri");
        EMBED_CALL(cli_ipp_message_tag.text, CliIppMessageTagCfg, .message = args->request, .tag = CLI_IPP_URI,
                   .text = args->uri);
    }
    EMBED_CALL(cli_ipp_table.attribute, CliIppCfg, .message = args->request, .name = "requesting-user-name");
    EMBED_CALL(cli_ipp_message_tag.text, CliIppMessageTagCfg, .message = args->request, .tag = CLI_IPP_NAME,
               .text = user);
}

// the request sent and its answer read, reported where it is no success with the printer's own words after: 1, or 0
EMBED_INLINE int cli_answer(const IppJobsCtx *args)
{
    const char *uri = args->uri;
    const unsigned char *document = args->document;
    unsigned long long size = args->size;
    VerbaLinea *err_msg = args->err_msg;
    if (!EMBED_CALL(cli_ipp_size.exchange, CliIppSizeCfg, .uri = uri, .request = &s_request, .document = document,
                    .size = size, .answer = &s_answer, .err_msg = err_msg))
    {
        return 0;
    }
    char words[CLI_PATH_MAX_LEN];
    VerbaLinea status = {words, sizeof(words), 0ull};
    const int ok = EMBED_CALL(cli_ipp_code.status, CliIppCodeCfg, .code = s_answer.code, .err_msg = &status);
    mmgr_line_finish(&status);
    if (ok)
    {
        return 1;
    }
    mmgr_line_put(err_msg, words);
    char detail[CLI_PATH_MAX_LEN];
    if (EMBED_CALL(cli_ipp_name.first_text, CliIppNameCfg, .message_read = &s_answer,
                   .group = EMBED_CALL(cli_ipp_tag.group_get, CliIppTagCfg, .message_read = &s_answer,
                                       .tag = CLI_IPP_OPERATION, .after = 0u),
                   .name = "status-message", .out = detail, .capacity = sizeof(detail)))
    {
        mmgr_line_put(err_msg, ": ");
        mmgr_line_put(err_msg, detail);
    }
    return 0;
}

// each text of an attribute into `list`, sized for CLI_IPP_LISTING: their count
EMBED_INLINE unsigned int cli_texts(const IppJobsCtx *args)
{
    const CliIppGroup *group = args->group;
    const char *name = args->name;
    const CliIppAttribute *const attribute =
        EMBED_CALL(cli_ipp_name.get, CliIppNameCfg, .message_read = &s_answer, .group = group, .name = name);
    unsigned int count = 0u;
    for (unsigned int at = 0u; (attribute != NULL) && (at < attribute->count) && (count < CLI_IPP_LISTING); at += 1u)
    {
        count += EMBED_CALL(cli_ipp_message_value.value_text, CliIppMessageValueCfg, .message_read = &s_answer,
                            .value_ipp_value_pointer = &s_answer.value[attribute->first + at], .out = args->list[count],
                            .capacity = CLI_NAME_MAX_LEN)
                     ? 1u
                     : 0u;
    }
    return count;
}

int cli_ipp_print(const CliIppSizeErrMsgCfg *args)
{
    EMBED_CALL(cli_operation, IppJobsCtx, .request = &s_request, .code = 0x0002u, .uri = args->uri, .with_uri = 1);
    EMBED_CALL(cli_ipp_table.attribute, CliIppCfg, .message = &s_request, .name = "job-name");
    EMBED_CALL(cli_ipp_message_tag.text, CliIppMessageTagCfg, .message = &s_request, .tag = CLI_IPP_NAME,
               .text = args->job->name);
    EMBED_CALL(cli_ipp_table.attribute, CliIppCfg, .message = &s_request, .name = "document-format");
    EMBED_CALL(cli_ipp_message_tag.text, CliIppMessageTagCfg, .message = &s_request, .tag = CLI_IPP_MIME,
               .text = args->job->format);
    EMBED_CALL(cli_ipp_message_tag.group, CliIppMessageTagCfg, .message = &s_request, .tag = CLI_IPP_JOB);
    EMBED_CALL(cli_ipp_table.attribute, CliIppCfg, .message = &s_request, .name = "media");
    EMBED_CALL(cli_ipp_message_tag.text, CliIppMessageTagCfg, .message = &s_request, .tag = CLI_IPP_KEYWORD,
               .text = args->job->media);
    EMBED_CALL(cli_ipp_table.attribute, CliIppCfg, .message = &s_request, .name = "print-color-mode");
    EMBED_CALL(cli_ipp_message_tag.text, CliIppMessageTagCfg, .message = &s_request, .tag = CLI_IPP_KEYWORD,
               .text = args->job->gray ? "monochrome" : "color");
    if (args->job->copies > 1u)
    {
        EMBED_CALL(cli_ipp_table.attribute, CliIppCfg, .message = &s_request, .name = "copies");
        // a count of copies is far below what an int represents
        EMBED_CALL(cli_ipp_message_kind.number, CliIppMessageKindCfg, .message = &s_request, .kind = CLI_IPP_INTEGER,
                   .value = (int)args->job->copies);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = args->job->format, .other = "application/pdf", .cap = CLI_CELLS_POOL}))
    {
        EMBED_CALL(cli_ipp_table.attribute, CliIppCfg, .message = &s_request, .name = "orientation-requested");
        EMBED_CALL(cli_ipp_message_kind.number, CliIppMessageKindCfg, .message = &s_request, .kind = CLI_IPP_ENUM,
                   .value = args->job->landscape ? 4 : 3);
    }
    const int answered = EMBED_CALL(cli_answer, IppJobsCtx, .uri = args->uri, .document = args->document,
                                    .size = args->size, .err_msg = args->err_msg);
    const int found =
        answered && EMBED_CALL(cli_ipp_name.first_number, CliIppNameCfg, .message_read = &s_answer,
                               .group = EMBED_CALL(cli_ipp_tag.group_get, CliIppTagCfg, .message_read = &s_answer,
                                                   .tag = CLI_IPP_JOB, .after = 0u),
                               .name = "job-id", .number = args->id_pointer);
    cli_jobs_let_go();
    if (answered && !found)
    {
        mmgr_line_put(args->err_msg, "the printer took the job and gave it no id");
    }
    return found;
}

int cli_ipp_job_state(const CliIppErrMsgUriCfg *args)
{
    int *state = args->state;
    EMBED_CALL(cli_operation, IppJobsCtx, .request = &s_request, .code = 0x0009u, .uri = args->uri, .with_uri = 1);
    EMBED_CALL(cli_ipp_table.attribute, CliIppCfg, .message = &s_request, .name = "job-id");
    EMBED_CALL(cli_ipp_message_kind.number, CliIppMessageKindCfg, .message = &s_request, .kind = CLI_IPP_INTEGER,
               .value = args->id);
    EMBED_CALL(cli_ipp_table.attribute, CliIppCfg, .message = &s_request, .name = "requested-attributes");
    EMBED_CALL(cli_ipp_message_tag.text, CliIppMessageTagCfg, .message = &s_request, .tag = CLI_IPP_KEYWORD,
               .text = "job-state");
    EMBED_CALL(cli_ipp_message_tag.text, CliIppMessageTagCfg, .message = &s_request, .tag = CLI_IPP_KEYWORD,
               .text = "job-state-reasons");
    if (!EMBED_CALL(cli_answer, IppJobsCtx, .uri = args->uri, .document = NULL, .size = 0ull, .err_msg = args->err_msg))
    {
        cli_jobs_let_go();
        return 0;
    }
    const CliIppGroup *const group =
        EMBED_CALL(cli_ipp_tag.group_get, CliIppTagCfg, .message_read = &s_answer, .tag = CLI_IPP_JOB, .after = 0u);
    *state = 3;
    EMBED_CALL(cli_ipp_name.first_number, CliIppNameCfg, .message_read = &s_answer, .group = group, .name = "job-state",
               .number = state);
    char list_entries[CLI_IPP_LISTING][CLI_NAME_MAX_LEN];
    const unsigned int count =
        EMBED_CALL(cli_texts, IppJobsCtx, .group = group, .name = "job-state-reasons", .list = list_entries);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(args->reasons, (at != 0u) ? ", " : "");
        mmgr_line_put(args->reasons, list_entries[at]);
    }
    cli_jobs_let_go();
    return 1;
}

int cli_ipp_cancel(const CliIppErrMsgUriCfg *args)
{
    EMBED_CALL(cli_operation, IppJobsCtx, .request = &s_request, .code = 0x0008u, .uri = args->uri, .with_uri = 1);
    EMBED_CALL(cli_ipp_table.attribute, CliIppCfg, .message = &s_request, .name = "job-id");
    EMBED_CALL(cli_ipp_message_kind.number, CliIppMessageKindCfg, .message = &s_request, .kind = CLI_IPP_INTEGER,
               .value = args->id);
    const int done =
        EMBED_CALL(cli_answer, IppJobsCtx, .uri = args->uri, .document = NULL, .size = 0ull, .err_msg = args->err_msg);
    cli_jobs_let_go();
    return done;
}

int cli_ipp_cups_printers(const CliIppErrMsgHostCfg *args)
{
    static const char *const s_requested[4] = {"printer-name", "printer-uri-supported", "printer-info",
                                               "printer-make-and-model"};
    char uri[CLI_PATH_MAX_LEN];
    VerbaLinea line = {uri, sizeof(uri), 0ull};
    mmgr_line_put(&line, "ipp://");
    mmgr_line_put(&line, args->host_read);
    mmgr_line_ch(&line, ':');
    mmgr_line_decimal(&line, args->port_value, 1u);
    mmgr_line_ch(&line, '/');
    mmgr_line_finish(&line);
    EMBED_CALL(cli_operation, IppJobsCtx, .request = &s_request, .code = 0x4002u, .uri = uri, .with_uri = 0);
    EMBED_CALL(cli_ipp_table.attribute, CliIppCfg, .message = &s_request, .name = "requested-attributes");
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        EMBED_CALL(cli_ipp_message_tag.text, CliIppMessageTagCfg, .message = &s_request, .tag = CLI_IPP_KEYWORD,
                   .text = s_requested[at]);
    }
    if (!EMBED_CALL(cli_answer, IppJobsCtx, .uri = uri, .document = NULL, .size = 0ull, .err_msg = args->err_msg))
    {
        cli_jobs_let_go();
        return -1;
    }
    int count = 0;
    for (unsigned int at = 0u; at < s_answer.groups; at += 1u)
    {
        const CliIppGroup *const group = &s_answer.group[at];
        if ((group->tag != CLI_IPP_PRINTER) || ((unsigned int)count >= args->capacity_u32))
        {
            continue;
        }
        CliIppShared *const one = &args->shared[count];
        if (!EMBED_CALL(cli_ipp_name.first_text, CliIppNameCfg, .message_read = &s_answer, .group = group,
                        .name = "printer-name", .out = one->name, .capacity = sizeof(one->name)))
        {
            continue;
        }
        const CliIppAttribute *const uris = EMBED_CALL(cli_ipp_name.get, CliIppNameCfg, .message_read = &s_answer,
                                                       .group = group, .name = "printer-uri-supported");
        int found = 0;
        for (unsigned int each = 0u; !found && (uris != NULL) && (each < uris->count); each += 1u)
        {
            found = EMBED_CALL(cli_ipp_message_value.value_text, CliIppMessageValueCfg, .message_read = &s_answer,
                               .value_ipp_value_pointer = &s_answer.value[uris->first + each], .out = one->uri,
                               .capacity = sizeof(one->uri)) &&
                    (cellul.starts(&(CatenaFinitaCfg){.src = one->uri, .other = "ipp://", .cap = CLI_CELLS_POOL}));
        }
        if (!found && !EMBED_CALL(cli_ipp_name.first_text, CliIppNameCfg, .message_read = &s_answer, .group = group,
                                  .name = "printer-uri-supported", .out = one->uri, .capacity = sizeof(one->uri)))
        {
            continue;
        }
        if (!EMBED_CALL(cli_ipp_name.first_text, CliIppNameCfg, .message_read = &s_answer, .group = group,
                        .name = "printer-info", .out = one->about, .capacity = sizeof(one->about)) ||
            (one->about[0] == '\0'))
        {
            one->about[0] = '\0';
            EMBED_CALL(cli_ipp_name.first_text, CliIppNameCfg, .message_read = &s_answer, .group = group,
                       .name = "printer-make-and-model", .out = one->about, .capacity = sizeof(one->about));
        }
        count += 1;
    }
    cli_jobs_let_go();
    return count;
}
