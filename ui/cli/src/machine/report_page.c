// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// report_page.c: a report's new-issue page with the template's fields filled in, and the issue's body as the
// template's form lays out what is entered in it
#include "report_internal.h"

// the template a report fills, under .github/ISSUE_TEMPLATE
static const char s_template[] = "bug_report.yml";

const char *cli_report_category_of(const CliReport *report)
{
    return cli_report_category_known(report->category) ? report->category : "unknown";
}

// orior's version, and the commit and changes of the tree where it is orior's own, into `out`
static void cli_version(const char *root, CliLine *out)
{
    cli_line_text(out, "orior_cli " CLI_VERSION);
    if (root == NULL)
    {
        return;
    }
    const char *const commit[5] = {"git", "rev-parse", "--short=10", "HEAD", NULL};
    const char *const status[5] = {"git", "status", "--porcelain", "--untracked-files=no", NULL};
    CliRan *const ran = cli_ran_taken();
    char why[CLI_NAME_LONGEST];
    CliLine unstarted = {why, sizeof(why), 0ull};
    if (cli_ran(commit, root, NULL, 0u, ran, &unstarted) && (ran->code == 0ll))
    {
        const char *from = ran->out;
        const char *to = ran->out + ran->out_size;
        while ((from < to) && ((from[0] == ' ') || ((from[0] >= '\t') && (from[0] <= '\r'))))
        {
            from += 1;
        }
        while ((to > from) && ((to[-1] == ' ') || ((to[-1] >= '\t') && (to[-1] <= '\r'))))
        {
            to -= 1;
        }
        char short_commit[64];
        CliLine line = {short_commit, sizeof(short_commit), 0ull};
        cli_line_counted(&line, from, (unsigned long long)(to - from));
        cli_line_closed(&line);
        unstarted.at = 0ull;
        int changed = 0;
        if (cli_ran(status, root, NULL, 0u, ran, &unstarted) && (ran->code == 0ll))
        {
            for (unsigned long long at = 0ull; !changed && (at < ran->out_size); at += 1ull)
            {
                changed = !((ran->out[at] == ' ') || ((ran->out[at] >= '\t') && (ran->out[at] <= '\r')));
            }
        }
        cli_line_text(out, " at ");
        cli_line_text(out, short_commit);
        cli_line_text(out, changed ? " with changes" : "");
    }
    cli_ran_given(ran);
}

// this system and its processor, as Rust names them, into `out`
static void cli_system(CliLine *out)
{
#if defined(_WIN32)
    cli_line_text(out, "windows ");
#elif defined(__APPLE__)
    cli_line_text(out, "macos ");
#else
    cli_line_text(out, "linux ");
#endif
#if defined(__x86_64__) || defined(_M_X64)
    cli_line_text(out, "x86_64");
#elif defined(__aarch64__) || defined(_M_ARM64)
    cli_line_text(out, "aarch64");
#elif defined(__i386__) || defined(_M_IX86)
    cli_line_text(out, "x86");
#else
    cli_line_text(out, "arm");
#endif
}

// a field of the issue's body: its text trimmed, or the template's word for none
static void cli_field(CliLine *out, const char *text)
{
    const char *from = text;
    const char *to = text + cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    while ((from < to) && ((from[0] == ' ') || ((from[0] >= '\t') && (from[0] <= '\r'))))
    {
        from += 1;
    }
    while ((to > from) && ((to[-1] == ' ') || ((to[-1] >= '\t') && (to[-1] <= '\r'))))
    {
        to -= 1;
    }
    if (from == to)
    {
        cli_line_text(out, "_No response_");
    }
    else
    {
        cli_line_counted(out, from, (unsigned long long)(to - from));
    }
}

void cli_report_body(const CliReport *report, const char *root, const char *print, CliLine *out)
{
    cli_line_text(out, "### Category\n\n");
    cli_line_text(out, cli_report_category_of(report));
    cli_line_text(out, "\n\n### What happened\n\n");
    cli_field(out, report->what);
    cli_line_text(out, "\n\n### Steps to reproduce\n\n");
    cli_field(out, report->steps);
    cli_line_text(out, "\n\n### Version\n\n");
    cli_version(root, out);
    cli_line_text(out, "\n\n### System\n\n");
    cli_system(out);
    cli_line_text(out, "\n\n### Logs\n\n");
    const char *from = report->logs;
    const char *to = report->logs + cellul.len(&(CatenaFinitaCfg){.src = report->logs, .cap = CLI_CELLS_POOL});
    while ((from < to) && ((from[0] == ' ') || ((from[0] >= '\t') && (from[0] <= '\r'))))
    {
        from += 1;
    }
    while ((to > from) && ((to[-1] == ' ') || ((to[-1] >= '\t') && (to[-1] <= '\r'))))
    {
        to -= 1;
    }
    if (from == to)
    {
        cli_line_text(out, "_No response_");
    }
    else
    {
        cli_line_text(out, "```text\n");
        cli_line_counted(out, from, (unsigned long long)(to - from));
        cli_line_text(out, "\n```");
    }
    cli_line_text(out, "\n\n<!-- orior-report ");
    cli_line_text(out, print);
    cli_line_text(out, " -->\n");
}

// `length` bytes of `text` written into `out` as an address's query writes them, letters, digits and `-_.~` as they
// are and every other byte as `%XX`
static void cli_encode(CliLine *out, const char *text, unsigned long long length)
{
    static const char s_hex[17] = "0123456789ABCDEF";
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        const unsigned char byte = (unsigned char)text[at];
        const int kept = ((byte >= 'a') && (byte <= 'z')) || ((byte >= 'A') && (byte <= 'Z')) ||
                         ((byte >= '0') && (byte <= '9')) || (byte == '-') || (byte == '_') || (byte == '.') ||
                         (byte == '~');
        if (kept)
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

// the length `length` bytes of `text` take encoded
static unsigned long long cli_encoded_length(const char *text, unsigned long long length)
{
    unsigned long long size = 0ull;
    for (unsigned long long at = 0ull; at < length; at += 1ull)
    {
        const unsigned char byte = (unsigned char)text[at];
        const int kept = ((byte >= 'a') && (byte <= 'z')) || ((byte >= 'A') && (byte <= 'Z')) ||
                         ((byte >= '0') && (byte <= '9')) || (byte == '-') || (byte == '_') || (byte == '.') ||
                         (byte == '~');
        size += kept ? 1ull : 3ull;
    }
    return size;
}

void cli_report_page(const CliReport *report, const char *root, CliLine *out)
{
    char version[CLI_PATH_LONGEST];
    char system[CLI_NAME_LONGEST];
    CliLine versioned = {version, sizeof(version), 0ull};
    CliLine systems = {system, sizeof(system), 0ull};
    cli_version(root, &versioned);
    cli_line_closed(&versioned);
    cli_system(&systems);
    cli_line_closed(&systems);
    const char *const category = cli_report_category_of(report);
    const char *const names[6] = {"title", "category", "what", "steps", "version", "system"};
    const char *const texts[6] = {report->title, category, report->what, report->steps, version, system};
    const unsigned long long start = out->at;
    cli_line_text(out, "https://github.com/" CLI_REPORT_REPOSITORY "/issues/new?template=");
    cli_line_text(out, s_template);
    cli_line_text(out, "&labels=bug,");
    cli_line_text(out, category);
    unsigned long long without = out->at - start;
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        without +=
            cellul.len(&(CatenaFinitaCfg){.src = names[at], .cap = CLI_CELLS_POOL}) + 2ull +
            cli_encoded_length(texts[at], cellul.len(&(CatenaFinitaCfg){.src = texts[at], .cap = CLI_CELLS_POOL}));
    }
    // the logs cut to three quarters of their letters at a time until the address keeps within its limit
    unsigned long long logs = cellul.len(&(CatenaFinitaCfg){.src = report->logs, .cap = CLI_CELLS_POOL});
    while ((logs != 0ull) && ((without + 6ull + cli_encoded_length(report->logs, logs)) > CLI_REPORT_ADDRESS))
    {
        unsigned long long letters = 0ull;
        for (unsigned long long at = 0ull; at < logs; at += 1ull)
        {
            letters += (((unsigned char)report->logs[at] & 0xc0u) != 0x80u) ? 1ull : 0ull;
        }
        const unsigned long long keep = (letters * 3ull) / 4ull;
        unsigned long long kept = 0ull;
        unsigned long long at = 0ull;
        for (; at < logs; at += 1ull)
        {
            const int starts = ((unsigned char)report->logs[at] & 0xc0u) != 0x80u;
            if (starts && (kept == keep))
            {
                break;
            }
            kept += starts ? 1ull : 0ull;
        }
        logs = at;
    }
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        if (texts[at][0] != '\0')
        {
            cli_line_character(out, '&');
            cli_line_text(out, names[at]);
            cli_line_character(out, '=');
            cli_encode(out, texts[at], cellul.len(&(CatenaFinitaCfg){.src = texts[at], .cap = CLI_CELLS_POOL}));
        }
    }
    if (logs != 0ull)
    {
        cli_line_text(out, "&logs=");
        cli_encode(out, report->logs, logs);
    }
}
