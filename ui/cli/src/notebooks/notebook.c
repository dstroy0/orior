// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// notebook.c: a notebook read from its JSON or its percent script, and written back as it was read
#include "../text/text.h"
#include "notebook_internal.h"

// the extension of `path`, after the last `.` of its last part, in lower case, into `out`, which holds 16: empty
// where it has none or one longer
static void cli_extension(const char *path, char *out)
{
    const char *name = path + cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL});
    while ((name > path) && (name[-1] != '/') && (name[-1] != '\\'))
    {
        name -= 1;
    }
    const char *const dot =
        cli_text_last(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), '.');
    out[0] = '\0';
    if ((dot == NULL) || (cellul.len(&(CatenaFinitaCfg){.src = dot + 1, .cap = CLI_CELLS_POOL}) >= 16u))
    {
        return;
    }
    unsigned int at = 0u;
    for (const char *letter = dot + 1; letter[0] != '\0'; letter += 1)
    {
        out[at] = ((letter[0] >= 'A') && (letter[0] <= 'Z')) ? (char)(letter[0] + 32) : letter[0];
        at += 1u;
    }
    out[at] = '\0';
}

// The comment a script's cells are marked with in the language of `path`, by its extension
const char *cli_notebook_comment(const char *path)
{
    static const char *const s_slashes[] = {"js",   "ts", "mjs",   "cs", "cpp", "c",
                                            "java", "kt", "scala", "go", "rs",  "swift"};
    static const char *const s_dashes[] = {"lua", "sql", "hs"};
    char extension[16];
    cli_extension(path, extension);
    for (unsigned int at = 0u; at < (sizeof(s_slashes) / sizeof(s_slashes[0])); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = s_slashes[at], .cap = CLI_CELLS_POOL}))
        {
            return "//";
        }
    }
    for (unsigned int at = 0u; at < (sizeof(s_dashes) / sizeof(s_dashes[0])); at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = s_dashes[at], .cap = CLI_CELLS_POOL}))
        {
            return "--";
        }
    }
    return (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "m", .cap = CLI_CELLS_POOL})) ? "%" : "#";
}

static int cli_is_space(char letter)
{
    return (letter == ' ') || ((letter >= '\t') && (letter <= '\r'));
}

// the line of `text` that starts at `at` before `end`: where it ends, its `\r` before `\n` left off, into `stop`;
// where the next starts
const char *cli_notebook_line(const char *at, const char *end, const char **stop)
{
    const char *next = at;
    while ((next < end) && (next[0] != '\n'))
    {
        next += 1;
    }
    *stop = ((next > at) && (next[-1] == '\r')) ? (next - 1) : next;
    return (next < end) ? (next + 1) : next;
}

// whether the line from `from` to `to`, its spaces before it left off, starts with `mark`: where after it, else NULL
static const char *cli_marked(const char *from, const char *to, const char *mark)
{
    while ((from < to) && cli_is_space(from[0]))
    {
        from += 1;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = mark, .cap = CLI_CELLS_POOL});
    return (((unsigned long long)(to - from) >= length) &&
            (memor.cmp(&(MemoriaCfg){.src = from, .other = mark, .bytes = length}) == 0))
               ? (from + length)
               : NULL;
}

int cli_notebook_is(const char *path, const char *text, unsigned long long size)
{
    char extension[16];
    cli_extension(path, extension);
    if (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "ipynb", .cap = CLI_CELLS_POOL}))
    {
        return 1;
    }
    char mark[8];
    CliLine line = {mark, sizeof(mark), 0ull};
    cli_line_text(&line, cli_notebook_comment(path));
    cli_line_text(&line, " %%");
    cli_line_closed(&line);
    const char *const end = text + size;
    const char *at = text;
    while (at < end)
    {
        const char *stop = NULL;
        const char *const next = cli_notebook_line(at, end, &stop);
        if (cli_marked(at, stop, mark) != NULL)
        {
            return 1;
        }
        at = next;
    }
    return 0;
}

int cli_notebook_ends(const char *text, unsigned long long length, const char *ending)
{
    const unsigned long long size = cellul.len(&(CatenaFinitaCfg){.src = ending, .cap = CLI_CELLS_POOL});
    return (length >= size) &&
           (memor.cmp(&(MemoriaCfg){.src = text + length - size, .other = ending, .bytes = size}) == 0);
}

// `length` bytes of `text` put after the text being made at the end of the tree's texts
static void cli_piece(CliJson *json, const char *text, unsigned long long length)
{
    if ((json->used + length + 1ull) > json->text_most)
    {
        json->full = 1;
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = json->text + json->used, .src = text, .bytes = length});
    json->used += length;
}

// the text made from `start` to the end of the tree's texts made a string node
static unsigned int cli_pieces_made(CliJson *json, unsigned long long start)
{
    const unsigned int made = json->full ? CLI_JSON_NONE : cli_json_made(json, CFG_JSON_STRING);
    if (made == CLI_JSON_NONE)
    {
        return made;
    }
    json->text[json->used] = '\0';
    json->node[made].text = start;
    json->node[made].length = json->used - start;
    json->used += 1ull;
    return made;
}

unsigned int cli_notebook_joined(CliJson *json, unsigned int value)
{
    const unsigned long long start = json->used;
    if (cli_json_is(json, value, CFG_JSON_STRING))
    {
        cli_piece(json, json->text + json->node[value].text, json->node[value].length);
    }
    for (unsigned int line = cli_json_is(json, value, CFG_JSON_ARRAY) ? json->node[value].first : CLI_JSON_NONE;
         line != CLI_JSON_NONE; line = json->node[line].next)
    {
        if (json->node[line].kind == CFG_JSON_STRING)
        {
            cli_piece(json, json->text + json->node[line].text, json->node[line].length);
        }
    }
    return cli_pieces_made(json, start);
}

const char *cli_notebook_source(const CliJson *json, unsigned int cell, unsigned long long *length)
{
    const char *const source = cli_json_string(json, cli_json_member(json, cell, "source"), length);
    if (source == NULL)
    {
        *length = 0ull;
        return "";
    }
    return source;
}

// A text as Jupyter keeps it: its lines, each with its line end but the last
unsigned int cli_notebook_lines_of(CliJson *json, const char *text, unsigned long long length)
{
    const unsigned int lines = cli_json_made(json, CFG_JSON_ARRAY);
    unsigned long long from = 0ull;
    while (from < length)
    {
        unsigned long long to = from;
        while ((to < length) && (text[to] != '\n'))
        {
            to += 1ull;
        }
        to += (to < length) ? 1ull : 0ull;
        cli_json_append(json, lines, cli_json_string_made(json, text + from, to - from));
        from = to;
    }
    return lines;
}

// each output of a cell read: its text one text, and each of its data that is a list and no JSON one text
static void cli_outputs_joined(CliJson *json, unsigned int cell)
{
    const unsigned int outputs = cli_json_member(json, cell, "outputs");
    for (unsigned int output = cli_json_is(json, outputs, CFG_JSON_ARRAY) ? json->node[outputs].first : CLI_JSON_NONE;
         output != CLI_JSON_NONE; output = json->node[output].next)
    {
        const unsigned int text = cli_json_member(json, output, "text");
        if (text != CLI_JSON_NONE)
        {
            cli_json_set(json, output, "text", cli_notebook_joined(json, text));
        }
        const unsigned int data = cli_json_member(json, output, "data");
        for (unsigned int kind = cli_json_is(json, data, CFG_JSON_OBJECT) ? json->node[data].first : CLI_JSON_NONE;
             kind != CLI_JSON_NONE; kind = json->node[kind].next)
        {
            const char *const name = json->text + json->node[kind].key;
            if ((json->node[kind].kind == CFG_JSON_ARRAY) &&
                !cli_notebook_ends(name, cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), "json"))
            {
                cli_json_set(json, data, name, cli_notebook_joined(json, kind));
            }
        }
    }
}

// A cell of a script ended: its lines from `from` to `to`, its blank lines at the end left off, its source the lines
// joined, a markdown cell's each with its comment taken off
static void cli_cell_finished(CliJson *json, unsigned int cells, const char *comment, unsigned int kind,
                              unsigned int title, const char *from, const char *to)
{
    const int markdown = cellul.eq(
        &(CatenaFinitaCfg){.src = json->text + json->node[kind].text, .other = "markdown", .cap = CLI_CELLS_POOL});
    const unsigned long long start = json->used;
    const unsigned long long comment_length = cellul.len(&(CatenaFinitaCfg){.src = comment, .cap = CLI_CELLS_POOL});
    int first = 1;
    const char *at = from;
    while (at < to)
    {
        const char *stop = NULL;
        const char *const next = cli_notebook_line(at, to, &stop);
        const char *line = at;
        if (markdown && ((unsigned long long)(stop - line) >= comment_length) &&
            (memor.cmp(&(MemoriaCfg){.src = line, .other = comment, .bytes = comment_length}) == 0))
        {
            line += comment_length;
            line += ((line < stop) && (line[0] == ' ')) ? 1 : 0;
        }
        if (!first)
        {
            cli_piece(json, "\n", 1ull);
        }
        first = 0;
        cli_piece(json, line, (unsigned long long)(stop - line));
        at = next;
    }
    const unsigned int source = cli_pieces_made(json, start);
    const unsigned int cell = cli_json_made(json, CFG_JSON_OBJECT);
    const unsigned int metadata = cli_json_made(json, CFG_JSON_OBJECT);
    cli_json_set(json, cell, "cell_type", kind);
    cli_json_set(json, cell, "source", source);
    cli_json_set(json, metadata, "title", title);
    cli_json_set(json, cell, "metadata", metadata);
    cli_json_set(json, cell, "outputs", cli_json_made(json, CFG_JSON_ARRAY));
    cli_json_set(json, cell, "execution_count", cli_json_made(json, CFG_JSON_NULL));
    cli_json_append(json, cells, cell);
}

// a cell's mark read after `%%`, `rest` to `end`: its kind, and its title, the kind's word taken out, both trimmed
static void cli_mark_read(CliJson *json, const char *rest, const char *end, unsigned int *kind, unsigned int *title)
{
    while ((rest < end) && cli_is_space(rest[0]))
    {
        rest += 1;
    }
    while ((end > rest) && cli_is_space(end[-1]))
    {
        end -= 1;
    }
    const char *word = NULL;
    const char *after = end;
    const char *found = NULL;
    for (const char *at = rest; (found == NULL) && (at < end); at += 1)
    {
        const unsigned long long left = (unsigned long long)(end - at);
        if (((left >= 10u) && (memor.cmp(&(MemoriaCfg){.src = at, .other = "[markdown]", .bytes = 10u}) == 0)) ||
            ((left >= 4u) && (memor.cmp(&(MemoriaCfg){.src = at, .other = "[md]", .bytes = 4u}) == 0)))
        {
            found = at;
            word = "markdown";
            after = at;
            while ((after < end) && (after[0] != ']'))
            {
                after += 1;
            }
            after += (after < end) ? 1 : 0;
        }
    }
    for (const char *at = rest; (found == NULL) && (at + 5 <= end); at += 1)
    {
        if (memor.cmp(&(MemoriaCfg){.src = at, .other = "[raw]", .bytes = 5u}) == 0)
        {
            found = at;
            word = "raw";
            after = at + 5;
        }
    }
    *kind = cli_json_string_made(
        json, (word != NULL) ? word : "code",
        cellul.len(&(CatenaFinitaCfg){.src = (word != NULL) ? word : "code", .cap = CLI_CELLS_POOL}));
    const unsigned long long start = json->used;
    cli_piece(json, rest, (unsigned long long)(((found != NULL) ? found : end) - rest));
    cli_piece(json, after, (unsigned long long)(end - after));
    // the title trimmed where it is made, before anything else is put after it
    unsigned long long from = start;
    unsigned long long to = json->used;
    while ((from < to) && cli_is_space(json->text[from]))
    {
        from += 1ull;
    }
    while ((to > from) && cli_is_space(json->text[to - 1u]))
    {
        to -= 1ull;
    }
    json->used = to;
    *title = cli_pieces_made(json, from);
}

// a script of `%%` cells read: the lines before the first its header, each cell from its mark to the next
static unsigned int cli_script_read(CliJson *json, const char *path, const char *text, unsigned long long size)
{
    const char *const comment = cli_notebook_comment(path);
    char mark[8];
    CliLine line = {mark, sizeof(mark), 0ull};
    cli_line_text(&line, comment);
    cli_line_text(&line, " %%");
    cli_line_closed(&line);
    const char *const end = text + size;
    const unsigned int cells = cli_json_made(json, CFG_JSON_ARRAY);
    const unsigned int book = cli_json_made(json, CFG_JSON_OBJECT);
    unsigned int kind = CLI_JSON_NONE;
    unsigned int title = CLI_JSON_NONE;
    const char *header_end = end;
    const char *body = NULL;
    const char *body_end = NULL;
    const char *at = text;
    while (at < end)
    {
        const char *stop = NULL;
        const char *const next = cli_notebook_line(at, end, &stop);
        const char *const rest = cli_marked(at, stop, mark);
        if (rest != NULL)
        {
            if (kind != CLI_JSON_NONE)
            {
                cli_cell_finished(json, cells, comment, kind, title, body, body_end);
            }
            header_end = (kind == CLI_JSON_NONE) ? at : header_end;
            cli_mark_read(json, rest, stop, &kind, &title);
            body = next;
            body_end = next;
        }
        else if (kind != CLI_JSON_NONE)
        {
            const char *from = at;
            while ((from < stop) && cli_is_space(from[0]))
            {
                from += 1;
            }
            body_end = (from < stop) ? next : body_end;
        }
        at = next;
    }
    if (kind != CLI_JSON_NONE)
    {
        cli_cell_finished(json, cells, comment, kind, title, body, body_end);
    }
    // the header: each line before the first cell, a line end after each
    const unsigned long long start = json->used;
    at = text;
    while (at < header_end)
    {
        const char *stop = NULL;
        const char *const next = cli_notebook_line(at, header_end, &stop);
        cli_piece(json, at, (unsigned long long)(stop - at));
        cli_piece(json, "\n", 1ull);
        at = next;
    }
    const unsigned int header = cli_pieces_made(json, start);
    const char *const language =
        (cellul.eq(&(CatenaFinitaCfg){.src = comment, .other = "//", .cap = CLI_CELLS_POOL}))   ? "javascript"
        : (cellul.eq(&(CatenaFinitaCfg){.src = comment, .other = "%", .cap = CLI_CELLS_POOL}))  ? "matlab"
        : (cellul.eq(&(CatenaFinitaCfg){.src = comment, .other = "--", .cap = CLI_CELLS_POOL})) ? "lua"
                                                                                                : "python";
    const unsigned int metadata = cli_json_made(json, CFG_JSON_OBJECT);
    cli_json_set(
        json, metadata, "language",
        cli_json_string_made(json, language, cellul.len(&(CatenaFinitaCfg){.src = language, .cap = CLI_CELLS_POOL})));
    cli_json_set(json, book, "format", cli_json_string_made(json, "percent", 7u));
    cli_json_set(json, book, "header", header);
    cli_json_set(json, book, "cells", cells);
    cli_json_set(json, book, "metadata", metadata);
    return json->full ? CLI_JSON_NONE : book;
}

unsigned int cli_notebook_read(CliJson *json, const char *path, const char *text, unsigned long long size,
                               CfgJsonToken *tokens, unsigned int tokens_most, CliLine *said)
{
    char extension[16];
    cli_extension(path, extension);
    if (!cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "ipynb", .cap = CLI_CELLS_POOL}))
    {
        const unsigned int book = cli_script_read(json, path, text, size);
        if (book == CLI_JSON_NONE)
        {
            cli_line_text(said, path);
            cli_line_text(said, ": it holds more than a notebook is read into");
        }
        return book;
    }
    char why[CLI_SAID_LONGEST];
    CliLine reason = {why, sizeof(why), 0ull};
    const unsigned int book = cli_json_read(json, text, size, tokens, tokens_most, &reason);
    cli_line_closed(&reason);
    if (book == CLI_JSON_NONE)
    {
        cli_line_text(said, path);
        cli_line_text(said, ": ");
        cli_line_text(said, why);
        return book;
    }
    const unsigned int cells = cli_json_member(json, book, "cells");
    if (!cli_json_is(json, cells, CFG_JSON_ARRAY))
    {
        cli_line_text(said, path);
        cli_line_text(said, " holds no cells");
        return CLI_JSON_NONE;
    }
    for (unsigned int cell = json->node[cells].first; cell != CLI_JSON_NONE; cell = json->node[cell].next)
    {
        if (json->node[cell].kind == CFG_JSON_OBJECT)
        {
            cli_json_set(json, cell, "source", cli_notebook_joined(json, cli_json_member(json, cell, "source")));
            cli_outputs_joined(json, cell);
        }
    }
    cli_json_set(json, book, "format", cli_json_string_made(json, "ipynb", 5u));
    return book;
}
