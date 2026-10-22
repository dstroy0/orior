// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ipp_jobs.c: what printing asks of a printer by IPP: its attributes, a job printed, a job's state, a job canceled, and
// the printers a CUPS server shares
#include "ipp.h"

// the request and the answer, under the lock while one is asked; and the id of the next request
static CliIppMessage s_request;
static CliIppMessage s_answer;
static unsigned int s_next = 1u;
static CliLock s_lock = CLI_LOCK_INIT;

// the request and the answer given back once the answer is read, and the lock given with them
static void cli_jobs_let_go(void)
{
    cli_ipp_given(&s_request);
    cli_ipp_given(&s_answer);
    cli_lock_give(&s_lock);
}

int cli_ipp_status(unsigned int code, CliLine *said)
{
    if (code <= 0x00ffu)
    {
        cli_line_text(said, "done");
        return 1;
    }
    static const unsigned int s_codes[15] = {0x0400u, 0x0401u, 0x0402u, 0x0403u, 0x0404u, 0x0406u, 0x0407u, 0x040au,
                                             0x040bu, 0x0500u, 0x0501u, 0x0502u, 0x0503u, 0x0506u, 0x0507u};
    static const char *const s_said[15] = {"the printer did not understand the request",
                                           "the printer refuses jobs from this machine",
                                           "the printer asks who prints, and takes jobs only from a user it knows",
                                           "the printer asks who prints, and takes jobs only from a user it knows",
                                           "the printer cannot do what was asked now",
                                           "the printer has no such job or queue",
                                           "the job is gone",
                                           "the printer does not take the document's format",
                                           "the printer does not take a setting the job asked for",
                                           "the printer failed inside",
                                           "the printer does not do this",
                                           "the printer does not speak this version of IPP",
                                           "the printer is not taking jobs now",
                                           "the printer is not accepting jobs",
                                           "the printer is busy"};
    for (unsigned int at = 0u; at < 15u; at += 1u)
    {
        if (s_codes[at] == code)
        {
            cli_line_text(said, s_said[at]);
            return 0;
        }
    }
    static const char s_hex[17] = "0123456789ABCDEF";
    cli_line_text(said, "the printer answered status 0x");
    for (unsigned int shift = 16u; shift > 0u; shift -= 4u)
    {
        cli_line_character(said, s_hex[(code >> (shift - 4u)) & 0x0fu]);
    }
    return 0;
}

// the request begun with the operation group every request opens with, the printer's address in it
static void cli_operation(CliIppMessage *request, unsigned int code, const char *uri, int with_uri)
{
    char user[CLI_NAME_LONGEST];
    if (!query_environment_read("USERNAME", user, sizeof(user)) && !query_environment_read("USER", user, sizeof(user)))
    {
        memor.cpy(&(MemoriaCfg){.dst = user,
                                .src = "orior",
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = "orior", .cap = CLI_CELLS_POOL}) + 1u});
    }
    cli_ipp_begun(request, code, s_next);
    s_next += 1u;
    cli_ipp_group(request, CLI_IPP_OPERATION);
    cli_ipp_attribute(request, "attributes-charset");
    cli_ipp_text(request, CLI_IPP_CHARSET, "utf-8");
    cli_ipp_attribute(request, "attributes-natural-language");
    cli_ipp_text(request, CLI_IPP_LANGUAGE, "en");
    if (with_uri)
    {
        cli_ipp_attribute(request, "printer-uri");
        cli_ipp_text(request, CLI_IPP_URI, uri);
    }
    cli_ipp_attribute(request, "requesting-user-name");
    cli_ipp_text(request, CLI_IPP_NAME, user);
}

// the request asked and its answer read, said where it is no success with the printer's own words after: 1, or 0
static int cli_answer(const char *uri, const unsigned char *document, unsigned long long size, CliLine *said)
{
    if (!cli_ipp_exchange(uri, &s_request, document, size, &s_answer, said))
    {
        return 0;
    }
    char words[CLI_PATH_LONGEST];
    CliLine status = {words, sizeof(words), 0ull};
    const int ok = cli_ipp_status(s_answer.code, &status);
    cli_line_closed(&status);
    if (ok)
    {
        return 1;
    }
    cli_line_text(said, words);
    char detail[CLI_PATH_LONGEST];
    if (cli_ipp_first_text(&s_answer, cli_ipp_group_of(&s_answer, CLI_IPP_OPERATION, 0u), "status-message", detail,
                           sizeof(detail)))
    {
        cli_line_text(said, ": ");
        cli_line_text(said, detail);
    }
    return 0;
}

// each text of an attribute into `list`, which holds CLI_IPP_LISTED: their count
static unsigned int cli_texts(const CliIppGroup *group, const char *name, char (*list)[CLI_NAME_LONGEST])
{
    const CliIppAttribute *const attribute = cli_ipp_get(&s_answer, group, name);
    unsigned int count = 0u;
    for (unsigned int at = 0u; (attribute != NULL) && (at < attribute->count) && (count < CLI_IPP_LISTED); at += 1u)
    {
        count += cli_ipp_value_text(&s_answer, &s_answer.value[attribute->first + at], list[count], CLI_NAME_LONGEST)
                     ? 1u
                     : 0u;
    }
    return count;
}

// whether an attribute's first value is the truth `truth`
static int cli_truth(const CliIppGroup *group, const char *name, int truth)
{
    const CliIppAttribute *const attribute = cli_ipp_get(&s_answer, group, name);
    return (attribute != NULL) && (attribute->count != 0u) &&
           (s_answer.value[attribute->first].kind == CLI_IPP_BOOLEAN) &&
           (s_answer.value[attribute->first].low == truth);
}

int cli_ipp_printer(const char *uri, CliIppPrinter *printer, CliLine *said)
{
    static const char *const s_wanted[7] = {
        "printer-name",  "printer-make-and-model", "document-format-supported", "color-supported",
        "printer-state", "printer-state-reasons",  "printer-is-accepting-jobs"};
    cli_lock_take(&s_lock);
    cli_operation(&s_request, 0x000bu, uri, 1);
    cli_ipp_attribute(&s_request, "requested-attributes");
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        cli_ipp_text(&s_request, CLI_IPP_KEYWORD, s_wanted[at]);
    }
    if (!cli_answer(uri, NULL, 0ull, said))
    {
        cli_jobs_let_go();
        return 0;
    }
    const CliIppGroup *const group = cli_ipp_group_of(&s_answer, CLI_IPP_PRINTER, 0u);
    memor.set(&(MemoriaCfg){.dst = printer, .val = 0, .bytes = sizeof(*printer)});
    cli_ipp_first_text(&s_answer, group, "printer-name", printer->name, sizeof(printer->name));
    cli_ipp_first_text(&s_answer, group, "printer-make-and-model", printer->make, sizeof(printer->make));
    printer->format_count = cli_texts(group, "document-format-supported", printer->formats);
    printer->color = cli_truth(group, "color-supported", 1);
    printer->state = 3;
    cli_ipp_first_number(&s_answer, group, "printer-state", &printer->state);
    printer->reason_count = cli_texts(group, "printer-state-reasons", printer->reasons);
    printer->accepting = !cli_truth(group, "printer-is-accepting-jobs", 0);
    cli_jobs_let_go();
    return 1;
}

int cli_ipp_print(const char *uri, const CliIppJob *job, const unsigned char *document, unsigned long long size,
                  int *id, CliLine *said)
{
    cli_lock_take(&s_lock);
    cli_operation(&s_request, 0x0002u, uri, 1);
    cli_ipp_attribute(&s_request, "job-name");
    cli_ipp_text(&s_request, CLI_IPP_NAME, job->name);
    cli_ipp_attribute(&s_request, "document-format");
    cli_ipp_text(&s_request, CLI_IPP_MIME, job->format);
    cli_ipp_group(&s_request, CLI_IPP_JOB);
    cli_ipp_attribute(&s_request, "media");
    cli_ipp_text(&s_request, CLI_IPP_KEYWORD, job->media);
    cli_ipp_attribute(&s_request, "print-color-mode");
    cli_ipp_text(&s_request, CLI_IPP_KEYWORD, job->gray ? "monochrome" : "color");
    if (job->copies > 1u)
    {
        cli_ipp_attribute(&s_request, "copies");
        // a count of copies is far below what an int holds
        cli_ipp_number(&s_request, CLI_IPP_INTEGER, (int)job->copies);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = job->format, .other = "application/pdf", .cap = CLI_CELLS_POOL}))
    {
        cli_ipp_attribute(&s_request, "orientation-requested");
        cli_ipp_number(&s_request, CLI_IPP_ENUM, job->landscape ? 4 : 3);
    }
    const int answered = cli_answer(uri, document, size, said);
    const int found =
        answered && cli_ipp_first_number(&s_answer, cli_ipp_group_of(&s_answer, CLI_IPP_JOB, 0u), "job-id", id);
    cli_jobs_let_go();
    if (answered && !found)
    {
        cli_line_text(said, "the printer took the job and gave it no id");
    }
    return found;
}

int cli_ipp_job_state(const char *uri, int id, int *state, CliLine *reasons, CliLine *said)
{
    cli_lock_take(&s_lock);
    cli_operation(&s_request, 0x0009u, uri, 1);
    cli_ipp_attribute(&s_request, "job-id");
    cli_ipp_number(&s_request, CLI_IPP_INTEGER, id);
    cli_ipp_attribute(&s_request, "requested-attributes");
    cli_ipp_text(&s_request, CLI_IPP_KEYWORD, "job-state");
    cli_ipp_text(&s_request, CLI_IPP_KEYWORD, "job-state-reasons");
    if (!cli_answer(uri, NULL, 0ull, said))
    {
        cli_jobs_let_go();
        return 0;
    }
    const CliIppGroup *const group = cli_ipp_group_of(&s_answer, CLI_IPP_JOB, 0u);
    *state = 3;
    cli_ipp_first_number(&s_answer, group, "job-state", state);
    char listed[CLI_IPP_LISTED][CLI_NAME_LONGEST];
    const unsigned int count = cli_texts(group, "job-state-reasons", listed);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(reasons, (at != 0u) ? ", " : "");
        cli_line_text(reasons, listed[at]);
    }
    cli_jobs_let_go();
    return 1;
}

int cli_ipp_cancel(const char *uri, int id, CliLine *said)
{
    cli_lock_take(&s_lock);
    cli_operation(&s_request, 0x0008u, uri, 1);
    cli_ipp_attribute(&s_request, "job-id");
    cli_ipp_number(&s_request, CLI_IPP_INTEGER, id);
    const int done = cli_answer(uri, NULL, 0ull, said);
    cli_jobs_let_go();
    return done;
}

int cli_ipp_cups_printers(const char *host, unsigned int port, CliIppShared *shared, unsigned int room, CliLine *said)
{
    static const char *const s_wanted[4] = {"printer-name", "printer-uri-supported", "printer-info",
                                            "printer-make-and-model"};
    char uri[CLI_PATH_LONGEST];
    CliLine line = {uri, sizeof(uri), 0ull};
    cli_line_text(&line, "ipp://");
    cli_line_text(&line, host);
    cli_line_character(&line, ':');
    cli_line_decimal(&line, port, 1u);
    cli_line_character(&line, '/');
    cli_line_closed(&line);
    cli_lock_take(&s_lock);
    cli_operation(&s_request, 0x4002u, uri, 0);
    cli_ipp_attribute(&s_request, "requested-attributes");
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        cli_ipp_text(&s_request, CLI_IPP_KEYWORD, s_wanted[at]);
    }
    if (!cli_answer(uri, NULL, 0ull, said))
    {
        cli_jobs_let_go();
        return -1;
    }
    int count = 0;
    for (unsigned int at = 0u; at < s_answer.groups; at += 1u)
    {
        const CliIppGroup *const group = &s_answer.group[at];
        if ((group->tag != CLI_IPP_PRINTER) || ((unsigned int)count >= room))
        {
            continue;
        }
        CliIppShared *const one = &shared[count];
        if (!cli_ipp_first_text(&s_answer, group, "printer-name", one->name, sizeof(one->name)))
        {
            continue;
        }
        const CliIppAttribute *const uris = cli_ipp_get(&s_answer, group, "printer-uri-supported");
        int found = 0;
        for (unsigned int each = 0u; !found && (uris != NULL) && (each < uris->count); each += 1u)
        {
            found = cli_ipp_value_text(&s_answer, &s_answer.value[uris->first + each], one->uri, sizeof(one->uri)) &&
                    (cellul.starts(&(CatenaFinitaCfg){.src = one->uri, .other = "ipp://", .cap = CLI_CELLS_POOL}));
        }
        if (!found && !cli_ipp_first_text(&s_answer, group, "printer-uri-supported", one->uri, sizeof(one->uri)))
        {
            continue;
        }
        if (!cli_ipp_first_text(&s_answer, group, "printer-info", one->about, sizeof(one->about)) ||
            (one->about[0] == '\0'))
        {
            one->about[0] = '\0';
            cli_ipp_first_text(&s_answer, group, "printer-make-and-model", one->about, sizeof(one->about));
        }
        count += 1;
    }
    cli_jobs_let_go();
    return count;
}
