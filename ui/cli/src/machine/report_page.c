// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// report_page.c: a report's new-issue page with the template's fields filled in, and the issue's body as the
// template's form lays out what is entered in it
#include "../text/text.h"
#include "report_internal.h"

// the template a report fills, under .github/ISSUE_TEMPLATE
static const char s_template[] = "bug_report.yml";

const char *cli_report_category_get(const CliReportCtx *args)
{
    return EMBED_CALL(cli_report_category_text.category_know, CliReportCategoryTextCfg,
                      .category = args->report->category)
               ? args->report->category
               : "unknown";
}

typedef struct
{
    const char *root;
    VerbaLinea *out;
    const char *text;
    unsigned long long length;
} ReportPageCtx;
// orior's version, and the commit and changes of the tree where it is orior's own, into `out`
static void cli_version(const ReportPageCtx *args)
{
    mmgr_line_put(args->out, "imp " CLI_VERSION);
    if (args->root == NULL)
    {
        return;
    }
    const char *const commit[5] = {"git", "rev-parse", "--short=10", "HEAD", NULL};
    const char *const status[5] = {"git", "status", "--porcelain", "--untracked-files=no", NULL};
    CliRunResult *const run_result = cli_run_result_alloc();
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unstarted = {why, sizeof(why), 0ULL};
    if (EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = commit, .folder = args->root, .settings = NULL,
                   .count = 0u, .run_result = run_result, .err_msg = &unstarted) &&
        (run_result->code == 0LL))
    {
        const char *from = run_result->out;
        const char *to = run_result->out + run_result->out_size;
        from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ULL,
                           .size = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE);
        to = from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = from,
                               .at = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE);
        char short_commit[64];
        VerbaLinea line = {short_commit, sizeof(short_commit), 0ULL};
        mmgr_line_put_n(&line, from, (unsigned long long)(to - from));
        mmgr_line_finish(&line);
        unstarted.at = 0ULL;
        int changed = 0;
        if (EMBED_CALL(cli_words_internal.run, CliWordsCtx, .words = status, .folder = args->root, .settings = NULL,
                       .count = 0u, .run_result = run_result, .err_msg = &unstarted) &&
            (run_result->code == 0LL))
        {
            for (unsigned long long at = 0ULL; !changed && (at < run_result->out_size); at += 1ULL)
            {
                changed =
                    !((run_result->out[at] == ' ') || ((run_result->out[at] >= '\t') && (run_result->out[at] <= '\r')));
            }
        }
        mmgr_line_put(args->out, " at ");
        mmgr_line_put(args->out, short_commit);
        mmgr_line_put(args->out, changed ? " with changes" : "");
    }
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
}

// this system and its processor, as Rust names them, into `out`
EMBED_INLINE void cli_system(const ReportPageCtx *args)
{
#if defined(_WIN32)
    mmgr_line_put(args->out, "windows ");
#elif defined(__APPLE__)
    mmgr_line_put(args->out, "macos ");
#else
    mmgr_line_put(args->out, "linux ");
#endif
#if defined(__x86_64__) || defined(_M_X64)
    mmgr_line_put(args->out, "x86_64");
#elif defined(__aarch64__) || defined(_M_ARM64)
    mmgr_line_put(args->out, "aarch64");
#elif defined(__i386__) || defined(_M_IX86)
    mmgr_line_put(args->out, "x86");
#else
    mmgr_line_put(args->out, "arm");
#endif
}

// a field of the issue's body: its text trimmed, or the template's word for none
EMBED_INLINE void cli_field(const ReportPageCtx *args)
{
    const char *text = args->text;
    const char *from = text;
    const char *to = text + cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_POOL_BYTES});
    from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ULL,
                       .size = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE);
    to = from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = from,
                           .at = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE);
    if (from == to)
    {
        mmgr_line_put(args->out, "_No response_");
    }
    else
    {
        mmgr_line_put_n(args->out, from, (unsigned long long)(to - from));
    }
}

void cli_report_body(const CliReportCtx *args)
{
    mmgr_line_put(args->out, "### Category\n\n");
    mmgr_line_put(args->out, cli_report_internal.category_get(args));
    mmgr_line_put(args->out, "\n\n### What happened\n\n");
    EMBED_CALL(cli_field, ReportPageCtx, .out = args->out, .text = args->report->what);
    mmgr_line_put(args->out, "\n\n### Steps to reproduce\n\n");
    EMBED_CALL(cli_field, ReportPageCtx, .out = args->out, .text = args->report->steps);
    mmgr_line_put(args->out, "\n\n### Version\n\n");
    EMBED_CALL(cli_version, ReportPageCtx, .root = args->root, .out = args->out);
    mmgr_line_put(args->out, "\n\n### System\n\n");
    EMBED_CALL(cli_system, ReportPageCtx, .out = args->out);
    mmgr_line_put(args->out, "\n\n### Logs\n\n");
    const char *from = args->report->logs;
    const char *to =
        args->report->logs + cellul.len(&(CatenaFinitaCfg){.src = args->report->logs, .cap = CLI_POOL_BYTES});
    from += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = from, .at = 0ULL,
                       .size = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE);
    to = from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = from,
                           .at = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE);
    if (from == to)
    {
        mmgr_line_put(args->out, "_No response_");
    }
    else
    {
        mmgr_line_put(args->out, "```text\n");
        mmgr_line_put_n(args->out, from, (unsigned long long)(to - from));
        mmgr_line_put(args->out, "\n```");
    }
    mmgr_line_put(args->out, "\n\n<!-- orior-report ");
    mmgr_line_put(args->out, args->print);
    mmgr_line_put(args->out, " -->\n");
}

// `length` bytes of `text` written into `out` as an address's query writes them, letters, digits and `-_.~` as they
// are and every other byte as `%XX`
EMBED_INLINE void cli_encode(const ReportPageCtx *args)
{
    static const char s_hex[17] = "0123456789ABCDEF";
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        const unsigned char byte = (unsigned char)args->text[at];
        const int unreserved = ((byte >= 'a') && (byte <= 'z')) || ((byte >= 'A') && (byte <= 'Z')) ||
                               ((byte >= '0') && (byte <= '9')) || (byte == '-') || (byte == '_') || (byte == '.') ||
                               (byte == '~');
        if (unreserved)
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

// the length `length` bytes of `text` take encoded
EMBED_INLINE unsigned long long cli_encoded_length(const ReportPageCtx *args)
{
    unsigned long long size = 0ULL;
    for (unsigned long long at = 0ULL; at < args->length; at += 1ULL)
    {
        const unsigned char byte = (unsigned char)args->text[at];
        const int unreserved = ((byte >= 'a') && (byte <= 'z')) || ((byte >= 'A') && (byte <= 'Z')) ||
                               ((byte >= '0') && (byte <= '9')) || (byte == '-') || (byte == '_') || (byte == '.') ||
                               (byte == '~');
        size += unreserved ? 1ULL : 3ULL;
    }
    return size;
}

void cli_report_page(const CliReportRootOutCfg *args)
{
    char version[CLI_PATH_MAX_LEN];
    char system[CLI_NAME_MAX_LEN];
    VerbaLinea versioned = {version, sizeof(version), 0ULL};
    VerbaLinea systems = {system, sizeof(system), 0ULL};
    EMBED_CALL(cli_version, ReportPageCtx, .root = args->root, .out = &versioned);
    mmgr_line_finish(&versioned);
    EMBED_CALL(cli_system, ReportPageCtx, .out = &systems);
    mmgr_line_finish(&systems);
    const char *const category = EMBED_CALL(cli_report_internal.category_get, CliReportCtx, .report = args->report);
    const char *const names[6] = {"title", "category", "what", "steps", "version", "system"};
    const char *const texts[6] = {args->report->title, category, args->report->what,
                                  args->report->steps, version,  system};
    const unsigned long long start = args->out->at;
    mmgr_line_put(args->out, "https://github.com/" CLI_REPORT_REPOSITORY "/issues/new?template=");
    mmgr_line_put(args->out, s_template);
    mmgr_line_put(args->out, "&labels=bug,");
    mmgr_line_put(args->out, category);
    unsigned long long without = args->out->at - start;
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        without += cellul.len(&(CatenaFinitaCfg){.src = names[at], .cap = CLI_POOL_BYTES}) + 2ULL +
                   EMBED_CALL(cli_encoded_length, ReportPageCtx, .text = texts[at],
                              .length = cellul.len(&(CatenaFinitaCfg){.src = texts[at], .cap = CLI_POOL_BYTES}));
    }
    // the logs cut to three quarters of their letters at a time until the address fits within its limit
    unsigned long long logs = cellul.len(&(CatenaFinitaCfg){.src = args->report->logs, .cap = CLI_POOL_BYTES});
    while (
        (logs != 0ULL) &&
        ((without + 6ULL + EMBED_CALL(cli_encoded_length, ReportPageCtx, .text = args->report->logs, .length = logs)) >
         CLI_REPORT_ADDRESS))
    {
        unsigned long long letters = 0ULL;
        for (unsigned long long at = 0ULL; at < logs; at += 1ULL)
        {
            letters += (((unsigned char)args->report->logs[at] & 0xc0u) != 0x80u) ? 1ULL : 0ULL;
        }
        const unsigned long long budget = (letters * 3ULL) / 4ULL;
        unsigned long long count = 0ULL;
        unsigned long long at = 0ULL;
        for (; at < logs; at += 1ULL)
        {
            const int starts = ((unsigned char)args->report->logs[at] & 0xc0u) != 0x80u;
            if (starts && (count == budget))
            {
                break;
            }
            count += starts ? 1ULL : 0ULL;
        }
        logs = at;
    }
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        if (texts[at][0] != '\0')
        {
            mmgr_line_ch(args->out, '&');
            mmgr_line_put(args->out, names[at]);
            mmgr_line_ch(args->out, '=');
            EMBED_CALL(cli_encode, ReportPageCtx, .out = args->out, .text = texts[at],
                       .length = cellul.len(&(CatenaFinitaCfg){.src = texts[at], .cap = CLI_POOL_BYTES}));
        }
    }
    if (logs != 0ULL)
    {
        mmgr_line_put(args->out, "&logs=");
        EMBED_CALL(cli_encode, ReportPageCtx, .out = args->out, .text = args->report->logs, .length = logs);
    }
}
