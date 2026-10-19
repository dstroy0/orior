// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// purpose_comment.c: the first comment of a file's text, read with its language's comment marks, joined into its first
// sentences
#include "../editor/files_internal.h"
#include "../plugins/plugins.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "../toolchains/format.h"
#include "purpose.h"

// the most lines of a text read for its comment, the bytes of the room a comment is made in, and the most nodes of a
// plugin read
#define CLI_COMMENT_LINES 65536u
#define CLI_COMMENT_ROOM (1u << 18u)
#define CLI_COMMENT_NODES 4096u

// A span of a text: where it starts and its bytes
typedef struct
{
    const char *at;
    unsigned long long length;
} CliSpan;

// The room a comment is read in: the text's lines, the comment marks of its language, and the texts made
typedef struct
{
    CliSpan line[CLI_COMMENT_LINES];
    unsigned int lines;
    char id[CLI_NAME_LONGEST];
    char line_mark[CLI_NAME_LONGEST];
    int has_line;
    char open[CLI_NAME_LONGEST];
    char close[CLI_NAME_LONGEST];
    int has_block;
    CliJsonNode nodes[CLI_COMMENT_NODES];
    CfgJsonToken tokens[CLI_COMMENT_NODES];
    char json_text[CLI_COMMENT_ROOM];
    char made[CLI_COMMENT_ROOM];
    char joined[CLI_COMMENT_ROOM];
    char kept[CLI_COMMENT_ROOM];
} CliCommentRoom;

static CliCommentRoom s_room;

// the span with the whitespace at its start, and at either end, left off
static CliSpan cli_trimmed_start(CliSpan span)
{
    while (span.length > 0ull)
    {
        unsigned int letter = 0u;
        const unsigned long long step = cli_lossy_letter((const unsigned char *)span.at, span.length, 0ull, &letter);
        if (!cli_fuzzy_white(letter))
        {
            break;
        }
        span = (CliSpan){span.at + step, span.length - step};
    }
    return span;
}

static CliSpan cli_trimmed(CliSpan span)
{
    span = cli_trimmed_start(span);
    unsigned long long end = 0ull;
    for (unsigned long long at = 0ull; at < span.length;)
    {
        unsigned int letter = 0u;
        const unsigned long long step = cli_lossy_letter((const unsigned char *)span.at, span.length, at, &letter);
        at += step;
        end = cli_fuzzy_white(letter) ? end : at;
    }
    return (CliSpan){span.at, end};
}

// whether a span starts with `text`
static int cli_starts(CliSpan span, const char *text)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL});
    return (span.length >= length) && (memor.cmp(&(MemoriaCfg){.src = span.at, .other = text, .bytes = length}) == 0);
}

// a text's lines, as Rust's lines() splits them, read into the room
static void cli_lines_read(const char *text, unsigned long long size)
{
    s_room.lines = 0u;
    for (unsigned long long at = 0ull; (at < size) && (s_room.lines < CLI_COMMENT_LINES);)
    {
        const char *const newline = memor.chr(&(MemoriaCfg){.src = text + at, .val = '\n', .bytes = size - at});
        // a newline found lies in the text, at or past `at`
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - text) : size;
        unsigned long long length = end - at;
        length -= ((newline != NULL) && (length > 0ull) && (text[end - 1ull] == '\r')) ? 1ull : 0ull;
        s_room.line[s_room.lines] = (CliSpan){text + at, length};
        s_room.lines += 1u;
        at = end + 1ull;
    }
}

static void cli_plugin_each(void *context, const CliPlugin *plugin)
{
    (void)context;
    if (!cellul.eq(&(CatenaFinitaCfg){.src = plugin->id, .other = s_room.id, .cap = CLI_CELLS_POOL}))
    {
        return;
    }
    CliJson json;
    cli_json_begun(&json, s_room.nodes, CLI_COMMENT_NODES, s_room.json_text, CLI_COMMENT_ROOM);
    char why[CLI_NAME_LONGEST];
    CliLine unread = {why, sizeof(why), 0ull};
    const unsigned int root =
        cli_json_read(&json, plugin->text, plugin->size, s_room.tokens, CLI_COMMENT_NODES, &unread);
    const unsigned int comments = cli_json_member(&json, root, "comments");
    const char *const line = cli_json_string(&json, cli_json_member(&json, comments, "line"), NULL);
    const unsigned int block = cli_json_member(&json, comments, "block");
    const char *const open = cli_json_string(&json, cli_json_item(&json, block, 0u), NULL);
    const char *const close = cli_json_string(&json, cli_json_item(&json, block, 1u), NULL);
    s_room.has_line = line != NULL;
    memor.cpy(
        &(MemoriaCfg){.dst = s_room.line_mark, .bytes = sizeof(s_room.line_mark), .src = (line != NULL) ? line : ""});
    s_room.has_block = (open != NULL) && (close != NULL);
    memor.cpy(&(MemoriaCfg){
        .dst = s_room.open,
        .src = (open != NULL) ? open : "",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = (open != NULL) ? open : "", .cap = CLI_CELLS_POOL}) + 1u});
    memor.cpy(&(MemoriaCfg){
        .dst = s_room.close,
        .src = (close != NULL) ? close : "",
        .bytes = cellul.len(&(CatenaFinitaCfg){.src = (close != NULL) ? close : "", .cap = CLI_CELLS_POOL}) + 1u});
    // the first plugin of the id names the marks
    s_room.id[0] = '\0';
}

// the comment marks of the language of the file at `path`, as its plugin names them, read into the room
static void cli_marks_of(const char *path)
{
    s_room.has_line = 0;
    s_room.has_block = 0;
    if (cli_format_language_of(path, s_room.id))
    {
        cli_plugins_each(cli_plugin_each, NULL);
    }
}

// whether a text states a copyright or a license
static int cli_is_license(const char *text, unsigned long long length)
{
    static const char *const s_words[2] = {"copyright", "license"};
    for (unsigned int word = 0u; word < 2u; word += 1u)
    {
        const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = s_words[word], .cap = CLI_CELLS_POOL});
        for (unsigned long long at = 0ull; (at + size) <= length; at += 1ull)
        {
            unsigned long long same = 0ull;
            while ((same < size) &&
                   ((((text[at + same] >= 'A') && (text[at + same] <= 'Z')) ? (text[at + same] + 32)
                                                                            : text[at + same]) == s_words[word][same]))
            {
                same += 1ull;
            }
            if (same == size)
            {
                return 1;
            }
        }
    }
    return 0;
}

// the first sentences of a text, its words joined by spaces, up to CLI_PURPOSE_SAID letters, written into `out`
static void cli_sentences(const char *text, unsigned long long size, CliLine *out)
{
    CliLine joined = {s_room.joined, sizeof(s_room.joined), 0ull};
    unsigned long long letters = 0ull;
    unsigned long long cut = 0ull;
    int in_word = 0;
    for (unsigned long long at = 0ull; at < size;)
    {
        unsigned int letter = 0u;
        const unsigned long long step = cli_lossy_letter((const unsigned char *)text, size, at, &letter);
        if (cli_fuzzy_white(letter))
        {
            in_word = 0;
            at += step;
            continue;
        }
        if (!in_word && (joined.at != 0ull))
        {
            cli_line_character(&joined, ' ');
            letters += 1ull;
            cut = (letters == CLI_PURPOSE_SAID) ? joined.at : cut;
        }
        in_word = 1;
        cli_line_counted(&joined, text + at, step);
        letters += 1ull;
        cut = (letters == CLI_PURPOSE_SAID) ? joined.at : cut;
        at += step;
    }
    if (letters <= CLI_PURPOSE_SAID)
    {
        cli_line_counted(out, s_room.joined, joined.at);
        return;
    }
    // the cut's last sentence end past its first third, else the cut, its trailing whitespace left off, and an ellipsis
    for (unsigned long long end = cut; end >= 2ull; end -= 1ull)
    {
        if ((s_room.joined[end - 2ull] == '.') && (s_room.joined[end - 1ull] == ' '))
        {
            if ((end - 2ull) > (CLI_PURPOSE_SAID / 3u))
            {
                cli_line_counted(out, s_room.joined, end - 1ull);
                return;
            }
            break;
        }
    }
    const CliSpan trimmed = cli_trimmed((CliSpan){s_room.joined, cut});
    cli_line_counted(out, trimmed.at, trimmed.length);
    cli_line_text(out, "\xe2\x80\xa6");
}

// the lines from `from` to `to` of the room, or of `lines`, each with its marks taken off, joined by newlines
static void cli_uncommented(const CliSpan *lines, unsigned int count, const char *mark, CliLine *out)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        CliSpan rest = cli_trimmed_start(lines[at]);
        if (cli_starts(rest, mark))
        {
            const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = mark, .cap = CLI_CELLS_POOL});
            rest = (CliSpan){rest.at + size, rest.length - size};
        }
        while ((rest.length > 0ull) &&
               ((rest.at[0] == '!') || (rest.at[0] == '/') || (rest.at[0] == '*') || (rest.at[0] == '#')))
        {
            rest = (CliSpan){rest.at + 1, rest.length - 1ull};
        }
        rest = cli_trimmed(rest);
        if (at != 0u)
        {
            cli_line_character(out, '\n');
        }
        cli_line_counted(out, rest.at, rest.length);
    }
}

// the first paragraph of a Markdown text under its headings, its marks of emphasis and its links' addresses left out
static void cli_first_paragraph(CliLine *out)
{
    const unsigned long long begun = out->at;
    CliLine joined = {s_room.joined, sizeof(s_room.joined), 0ull};
    unsigned int taken = 0u;
    for (unsigned int at = 0u; at < s_room.lines; at += 1u)
    {
        const CliSpan trimmed = cli_trimmed(s_room.line[at]);
        if (trimmed.length == 0ull)
        {
            if (taken != 0u)
            {
                break;
            }
            continue;
        }
        const int lead = cli_starts(trimmed, "#") || cli_starts(trimmed, "![") || cli_starts(trimmed, "[![") ||
                         cli_starts(trimmed, "<") || cli_starts(trimmed, "---") || cli_starts(trimmed, "```");
        if ((taken == 0u) && lead)
        {
            continue;
        }
        if (taken != 0u)
        {
            cli_line_character(&joined, ' ');
        }
        cli_line_counted(&joined, trimmed.at, trimmed.length);
        taken += 1u;
    }
    // each link's address left out, then the marks
    const char *rest = s_room.joined;
    const char *const end = s_room.joined + joined.at;
    for (;;)
    {
        const char *open = NULL;
        for (const char *at = rest; (open == NULL) && ((at + 1) < end); at += 1)
        {
            open = ((at[0] == ']') && (at[1] == '(')) ? at : NULL;
        }
        const char *const close =
            (open != NULL)
                ? memor.chr(&(MemoriaCfg){.src = open, .val = ')', .bytes = (unsigned long long)(end - open)})
                : NULL;
        if (close == NULL)
        {
            break;
        }
        for (const char *at = rest; at < open; at += 1)
        {
            cli_line_character(out, at[0]);
        }
        rest = close + 1;
    }
    for (const char *at = rest; at < end; at += 1)
    {
        cli_line_character(out, at[0]);
    }
    // the marks of emphasis left out of what was written
    char *const written = out->out;
    unsigned long long kept = begun;
    for (unsigned long long at = begun; at < out->at; at += 1ull)
    {
        const char letter = written[at];
        const int double_under = (letter == '_') && ((at + 1ull) < out->at) && (written[at + 1ull] == '_');
        if (double_under)
        {
            at += 1ull;
            continue;
        }
        if ((letter == '[') || (letter == '*') || (letter == '`'))
        {
            continue;
        }
        written[kept] = letter;
        kept += 1ull;
    }
    out->at = kept;
}

// the comment that a block comment's open mark starts at line `at` of the room holds, its lines written into `out`,
// `at` moved past it
static void cli_block_read(unsigned int *at, CliLine *out)
{
    // the room's lines from `at` on, joined by newlines
    CliLine rest = {s_room.joined, sizeof(s_room.joined), 0ull};
    for (unsigned int line = *at; line < s_room.lines; line += 1u)
    {
        if (line != *at)
        {
            cli_line_character(&rest, '\n');
        }
        cli_line_counted(&rest, s_room.line[line].at, s_room.line[line].length);
    }
    cli_line_closed(&rest);
    const char *const joined = s_room.joined;
    const char *const opened = cellul.find(
        &(CatenaFinitaCfg){.src = joined, .cap = CLI_CELLS_POOL, .other = s_room.open, .other_cap = CLI_CELLS_POOL});
    const unsigned long long from = ((opened != NULL) ? (unsigned long long)(opened - joined) : 0ull) +
                                    cellul.len(&(CatenaFinitaCfg){.src = s_room.open, .cap = CLI_CELLS_POOL});
    const char *const closed =
        (from <= rest.at)
            ? cellul.find(&(CatenaFinitaCfg){
                  .src = joined + from, .cap = CLI_CELLS_POOL, .other = s_room.close, .other_cap = CLI_CELLS_POOL})
            : NULL;
    unsigned long long end = rest.at;
    if (closed != NULL)
    {
        // the close mark lies past the open mark's end
        end = (unsigned long long)(closed - joined);
        unsigned int newlines = 0u;
        for (unsigned long long place = 0ull;
             place < (end + cellul.len(&(CatenaFinitaCfg){.src = s_room.close, .cap = CLI_CELLS_POOL})); place += 1ull)
        {
            newlines += (joined[place] == '\n') ? 1u : 0u;
        }
        *at += newlines + 1u;
    }
    else
    {
        *at = s_room.lines;
    }
    // the body's lines, each with its marks taken off
    static CliSpan s_body[CLI_COMMENT_LINES];
    unsigned int count = 0u;
    const unsigned long long length = (end > from) ? (end - from) : 0ull;
    for (unsigned long long place = 0ull; (place < length) && (count < CLI_COMMENT_LINES);)
    {
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = joined + from + place, .val = '\n', .bytes = length - place});
        // a newline found lies in the body, at or past `place`
        const unsigned long long stop = (newline != NULL) ? (unsigned long long)(newline - (joined + from)) : length;
        unsigned long long size = stop - place;
        size -= ((newline != NULL) && (size > 0ull) && (joined[from + stop - 1ull] == '\r')) ? 1ull : 0ull;
        s_body[count] = (CliSpan){joined + from + place, size};
        count += 1u;
        place = stop + 1ull;
    }
    cli_uncommented(s_body, count, "*", out);
}

void cli_purpose_first_comment(const char *path, const char *text, unsigned long long size, CliLine *out)
{
    static CliLock s_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_lock);
    // the extension of the file's name, in lower case
    const char *name = path;
    for (const char *at = path; at[0] != '\0'; at += 1)
    {
        name = ((at[0] == '/') || (at[0] == '\\')) ? (at + 1) : name;
    }
    const char *const dot =
        cli_text_last(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), '.');
    char extension[16] = "";
    for (unsigned int at = 0u; (dot != NULL) && (dot != name) && (dot[at + 1u] != '\0') && (at < 15u); at += 1u)
    {
        const char letter = dot[at + 1u];
        extension[at] = ((letter >= 'A') && (letter <= 'Z')) ? (char)(letter + 32) : letter;
        extension[at + 1u] = '\0';
    }
    cli_lines_read(text, size);
    if ((cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "md", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "markdown", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "mdx", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "rst", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "txt", .cap = CLI_CELLS_POOL})))
    {
        CliLine paragraph = {s_room.made, sizeof(s_room.made), 0ull};
        cli_first_paragraph(&paragraph);
        memor.cpy(&(MemoriaCfg){.dst = s_room.kept, .src = s_room.made, .bytes = paragraph.at});
        cli_sentences(s_room.kept, paragraph.at, out);
        cli_lock_give(&s_lock);
        return;
    }
    cli_marks_of(path);
    unsigned int at = ((s_room.lines > 0u) && cli_starts(s_room.line[0], "#!")) ? 1u : 0u;
    while (at < s_room.lines)
    {
        const CliSpan trimmed = cli_trimmed(s_room.line[at]);
        if (trimmed.length == 0ull)
        {
            at += 1u;
            continue;
        }
        CliLine said = {s_room.made, sizeof(s_room.made), 0ull};
        if (s_room.has_line && cli_starts(trimmed, s_room.line_mark))
        {
            const unsigned int start = at;
            while ((at < s_room.lines) && cli_starts(cli_trimmed(s_room.line[at]), s_room.line_mark))
            {
                at += 1u;
            }
            cli_uncommented(s_room.line + start, at - start, s_room.line_mark, &said);
        }
        else if (s_room.has_block && cli_starts(trimmed, s_room.open))
        {
            cli_block_read(&at, &said);
        }
        else if ((cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "py", .cap = CLI_CELLS_POOL})) &&
                 (cli_starts(trimmed, "\"\"\"") || cli_starts(trimmed, "'''") || cli_starts(trimmed, "r\"\"\"")))
        {
            const char *const quote =
                (cellul.find(&(CatenaFinitaCfg){
                     .src = trimmed.at, .cap = CLI_CELLS_POOL, .other = "'''", .other_cap = CLI_CELLS_POOL}) != NULL) &&
                        ((unsigned long long)(cellul.find(&(CatenaFinitaCfg){.src = trimmed.at,
                                                                             .cap = CLI_CELLS_POOL,
                                                                             .other = "'''",
                                                                             .other_cap = CLI_CELLS_POOL}) -
                                              trimmed.at) < trimmed.length)
                    ? "'''"
                    : "\"\"\"";
            const char *const first = cellul.find(
                &(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL, .other = quote, .other_cap = CLI_CELLS_POOL});
            const char *const body = (first != NULL) ? (first + 3) : text;
            const char *const ended = cellul.find(
                &(CatenaFinitaCfg){.src = body, .cap = CLI_CELLS_POOL, .other = quote, .other_cap = CLI_CELLS_POOL});
            cli_line_counted(&said, body,
                             (ended != NULL) ? (unsigned long long)(ended - body)
                                             : cellul.len(&(CatenaFinitaCfg){.src = body, .cap = CLI_CELLS_POOL}));
            // a docstring read is passed, so a license's docstring does not stop the reading there
            at += 1u;
        }
        else
        {
            cli_lock_give(&s_lock);
            return;
        }
        const CliSpan comment = cli_trimmed((CliSpan){s_room.made, said.at});
        if ((comment.length != 0ull) && !cli_is_license(s_room.made, said.at))
        {
            memor.cpy(&(MemoriaCfg){.dst = s_room.kept, .src = s_room.made, .bytes = said.at});
            cli_sentences(s_room.kept, said.at, out);
            cli_lock_give(&s_lock);
            return;
        }
    }
    cli_lock_give(&s_lock);
}
