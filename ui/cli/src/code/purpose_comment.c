// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// purpose_comment.c: the first comment of a file's text, read with its language's comment marks, joined into its first
// sentences
#include "../editor/files_internal.h"
#include "../plugins/plugins.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "../toolchains/format.h"
#include "purpose_lines.h"

// the most lines of a text read for its comment, and the most bytes of each text a comment is made in
#define CLI_COMMENT_LINES 65536u
#define CLI_COMMENT_CAPACITY (1u << 18u)

// The work state a comment is read in, guarded by the lock: the text's lines and a block comment's, `line_capacity` of
// each, the comment marks of its language, and the texts made, `capacity` bytes each; the lists and the texts one cell
// sized to the text read
typedef struct
{
    CliSpan *line;
    unsigned int lines;
    unsigned int line_capacity;
    CliSpan *body;
    char id[CLI_NAME_MAX_LEN];
    char line_mark[CLI_NAME_MAX_LEN];
    int has_line;
    char open[CLI_NAME_MAX_LEN];
    char close[CLI_NAME_MAX_LEN];
    int has_block;
    char *made;
    char *joined;
    char *paragraph;
    unsigned long long capacity;
} CliCommentWorkState;

static CliCommentWorkState s_work_state;

typedef struct
{
    const char *text;
    unsigned long long size;
    const char *path;
    VerbaLinea *out;
    unsigned int *at;
} PurposeCommentCtx;
// a text's lines, as Rust's lines() splits them, CLI_COMMENT_LINES at the most, read into the work state where it has
// work state for a list of them: their count
EMBED_INLINE unsigned int cli_lines_read(const PurposeCommentCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned int lines = 0u;
    for (unsigned long long at = 0ULL; (at < size) && (lines < CLI_COMMENT_LINES);)
    {
        const CliTextLine bounds = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = at, .size = size);
        const unsigned long long end = bounds.stop;
        unsigned long long length = end - at;
        length -= bounds.stop - bounds.cut;
        if (s_work_state.line != NULL)
        {
            s_work_state.line[lines] = (CliSpan){text + at, length};
        }
        lines += 1u;
        at = end + 1ULL;
    }
    return lines;
}

// the work state taken for the lines of `size` bytes of `text`, and the texts made of them, each its bytes and one more
// at the most CLI_COMMENT_CAPACITY, the lines read into it: 1, or 0 where the cellblock could not meet it
EMBED_INLINE int cli_byte_buffer_alloc(const PurposeCommentCtx *args)
{
    s_work_state.line = NULL;
    const unsigned int lines = cli_lines_read(args);
    const unsigned int line_capacity = (lines != 0u) ? lines : 1u;
    const unsigned long long capacity =
        (args->size < CLI_COMMENT_CAPACITY) ? (args->size + 1ULL) : CLI_COMMENT_CAPACITY;
    const unsigned long long spans = 2ULL * line_capacity * sizeof(CliSpan);
    char *const cell = g_cli_cells->persistent_buf_alloc(spans + (3ULL * capacity), &g_cli_purpose_cells);
    if (cell == NULL)
    {
        return 0;
    }
    // the spans on the cell's boundary, the texts after them
    s_work_state.line = (CliSpan *)(void *)cell;
    s_work_state.body = s_work_state.line + line_capacity;
    s_work_state.line_capacity = line_capacity;
    s_work_state.made = cell + spans;
    s_work_state.joined = s_work_state.made + capacity;
    s_work_state.paragraph = s_work_state.joined + capacity;
    s_work_state.capacity = capacity;
    s_work_state.lines = cli_lines_read(args);
    return 1;
}

// the work state's cell given back
static void cli_byte_buffer_free(void)
{
    g_cli_cells->persistent_buf_release(s_work_state.line, &g_cli_purpose_cells);
    s_work_state.line = NULL;
    s_work_state.body = NULL;
    s_work_state.lines = 0u;
}

static void cli_plugin_each(const CliEachPluginCfg *args)
{
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->plugin->id, .other = s_work_state.id, .cap = CLI_CELLS_POOL}))
    {
        return;
    }
    CliJson json = {0};
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ULL};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = args->plugin->text, .size = args->plugin->size, .nodes_more = 0u, .text_more = 0ULL,
                   .err_msg = &unread, .owner = &g_cli_purpose_cells);
    const unsigned int comments = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                             .object = root, .name = "comments");
    const char *const line =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = comments, .name = "line"),
                   .length_pointer = NULL);
    const unsigned int block = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                          .object = comments, .name = "block");
    const char *const open =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &json,
                                      .array = block, .index = 0u),
                   .length_pointer = NULL);
    const char *const close =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &json,
                                      .array = block, .index = 1u),
                   .length_pointer = NULL);
    s_work_state.has_line = line != NULL;
    (void)cellul.copy(&(CatenaFinitaCfg){
        .dst = s_work_state.line_mark, .src = (line != NULL) ? line : "", .cap = sizeof(s_work_state.line_mark)});
    s_work_state.has_block = (open != NULL) && (close != NULL);
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_capacity = s_work_state.open, .text = open, .otherwise = "");
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_capacity = s_work_state.close, .text = close,
               .otherwise = "");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    // the first plugin of the id names the marks
    s_work_state.id[0] = '\0';
}

// the comment marks of the language of the file at `path`, as its plugin names them, read into the work state
EMBED_INLINE void cli_marks_get(const PurposeCommentCtx *args)
{
    const char *path = args->path;
    s_work_state.has_line = 0;
    s_work_state.has_block = 0;
    if (EMBED_CALL(cli_format_table.language_get, CliFormatCfg, .path = path, .id = s_work_state.id))
    {
        EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = cli_plugin_each, .context = NULL);
    }
}

// the first sentences of a text, its words joined by spaces, up to CLI_PURPOSE_TEXT_MAX_LEN letters, written into `out`
static void cli_sentences(const PurposeCommentCtx *args)
{
    const char *text = args->text;
    VerbaLinea joined = {s_work_state.joined, s_work_state.capacity, 0ULL};
    unsigned long long letters = 0ULL;
    unsigned long long cut = 0ULL;
    int in_word = 0;
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        unsigned int letter = 0u;
        const unsigned long long step =
            EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                       .text_bytes = (const unsigned char *)text, .length = args->size, .at = at, .letter = &letter);
        if (EMBED_CALL(cli_files_letter_internal.fuzzy_white, CliFilesLetterCtx, .letter = letter))
        {
            in_word = 0;
            at += step;
            continue;
        }
        if (!in_word && (joined.at != 0ULL))
        {
            mmgr_line_ch(&joined, ' ');
            letters += 1ULL;
            cut = (letters == CLI_PURPOSE_TEXT_MAX_LEN) ? joined.at : cut;
        }
        in_word = 1;
        mmgr_line_put_n(&joined, text + at, step);
        letters += 1ULL;
        cut = (letters == CLI_PURPOSE_TEXT_MAX_LEN) ? joined.at : cut;
        at += step;
    }
    if (letters <= CLI_PURPOSE_TEXT_MAX_LEN)
    {
        mmgr_line_put_n(args->out, s_work_state.joined, joined.at);
        return;
    }
    // the cut's last sentence end past its first third, else the cut, its trailing whitespace left off, and an ellipsis
    for (unsigned long long end = cut; end >= 2ULL; end -= 1ULL)
    {
        if ((s_work_state.joined[end - 2ULL] == '.') && (s_work_state.joined[end - 1ULL] == ' '))
        {
            if ((end - 2ULL) > (CLI_PURPOSE_TEXT_MAX_LEN / 3u))
            {
                mmgr_line_put_n(args->out, s_work_state.joined, end - 1ULL);
                return;
            }
            break;
        }
    }
    const CliSpan trimmed = EMBED_CALL(cli_purpose_lines_span.purpose_trim, CliPurposeLinesSpanCfg,
                                       .span = (CliSpan){s_work_state.joined, cut});
    mmgr_line_put_n(args->out, trimmed.at, trimmed.length);
    mmgr_line_put(args->out, "\xe2\x80\xa6");
}

// the first paragraph of a Markdown text under its headings, its marks of emphasis and its links' addresses left out
EMBED_INLINE void cli_first_paragraph(const PurposeCommentCtx *args)
{
    const unsigned long long begun = args->out->at;
    VerbaLinea joined = {s_work_state.joined, s_work_state.capacity, 0ULL};
    unsigned int reserved = 0u;
    for (unsigned int at = 0u; at < s_work_state.lines; at += 1u)
    {
        const CliSpan trimmed =
            EMBED_CALL(cli_purpose_lines_span.purpose_trim, CliPurposeLinesSpanCfg, .span = s_work_state.line[at]);
        if (trimmed.length == 0ULL)
        {
            if (reserved != 0u)
            {
                break;
            }
            continue;
        }
        const int lead =
            EMBED_CALL(cli_purpose_lines_span.purpose_starts, CliPurposeLinesSpanCfg, .span = trimmed, .text = "#") ||
            EMBED_CALL(cli_purpose_lines_span.purpose_starts, CliPurposeLinesSpanCfg, .span = trimmed, .text = "![") ||
            EMBED_CALL(cli_purpose_lines_span.purpose_starts, CliPurposeLinesSpanCfg, .span = trimmed, .text = "[![") ||
            EMBED_CALL(cli_purpose_lines_span.purpose_starts, CliPurposeLinesSpanCfg, .span = trimmed, .text = "<") ||
            EMBED_CALL(cli_purpose_lines_span.purpose_starts, CliPurposeLinesSpanCfg, .span = trimmed, .text = "---") ||
            EMBED_CALL(cli_purpose_lines_span.purpose_starts, CliPurposeLinesSpanCfg, .span = trimmed, .text = "```");
        if ((reserved == 0u) && lead)
        {
            continue;
        }
        if (reserved != 0u)
        {
            mmgr_line_ch(&joined, ' ');
        }
        mmgr_line_put_n(&joined, trimmed.at, trimmed.length);
        reserved += 1u;
    }
    // each link's address left out, then the marks
    const char *rest = s_work_state.joined;
    const char *const end = s_work_state.joined + joined.at;
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
            mmgr_line_ch(args->out, at[0]);
        }
        rest = close + 1;
    }
    for (const char *at = rest; at < end; at += 1)
    {
        mmgr_line_ch(args->out, at[0]);
    }
    // the marks of emphasis left out of what was written
    char *const written = args->out->out;
    unsigned long long paragraph = begun;
    for (unsigned long long at = begun; at < args->out->at; at += 1ULL)
    {
        const char letter = written[at];
        const int double_under = (letter == '_') && ((at + 1ULL) < args->out->at) && (written[at + 1ULL] == '_');
        if (double_under)
        {
            at += 1ULL;
            continue;
        }
        if ((letter == '[') || (letter == '*') || (letter == '`'))
        {
            continue;
        }
        written[paragraph] = letter;
        paragraph += 1ULL;
    }
    args->out->at = paragraph;
}

// the comment that a block comment's open mark starts at line `at` of the work state contains, its lines written into
// `out`, `at` moved past it
EMBED_INLINE void cli_block_read(const PurposeCommentCtx *args)
{
    unsigned int *at = args->at;
    VerbaLinea *out = args->out;
    // the work state's lines from `at` on, joined by newlines
    VerbaLinea rest = {s_work_state.joined, s_work_state.capacity, 0ULL};
    for (unsigned int line = *at; line < s_work_state.lines; line += 1u)
    {
        if (line != *at)
        {
            mmgr_line_ch(&rest, '\n');
        }
        mmgr_line_put_n(&rest, s_work_state.line[line].at, s_work_state.line[line].length);
    }
    mmgr_line_finish(&rest);
    const char *const joined = s_work_state.joined;
    const char *const opened = cellul.find(&(CatenaFinitaCfg){
        .src = joined, .cap = CLI_CELLS_POOL, .other = s_work_state.open, .other_cap = CLI_CELLS_POOL});
    const unsigned long long from = ((opened != NULL) ? (unsigned long long)(opened - joined) : 0ULL) +
                                    cellul.len(&(CatenaFinitaCfg){.src = s_work_state.open, .cap = CLI_CELLS_POOL});
    const char *const closed = (from <= rest.at) ? cellul.find(&(CatenaFinitaCfg){.src = joined + from,
                                                                                  .cap = CLI_CELLS_POOL,
                                                                                  .other = s_work_state.close,
                                                                                  .other_cap = CLI_CELLS_POOL})
                                                 : NULL;
    unsigned long long end = rest.at;
    if (closed != NULL)
    {
        // the close mark lies past the open mark's end
        end = (unsigned long long)(closed - joined);
        unsigned int newlines = 0u;
        for (unsigned long long place = 0ULL;
             place < (end + cellul.len(&(CatenaFinitaCfg){.src = s_work_state.close, .cap = CLI_CELLS_POOL}));
             place += 1ULL)
        {
            newlines += (joined[place] == '\n') ? 1u : 0u;
        }
        *at += newlines + 1u;
    }
    else
    {
        *at = s_work_state.lines;
    }
    // the body's lines, each with its marks taken off, no more than the work state's lines they were joined from
    unsigned int count = 0u;
    const unsigned long long length = (end > from) ? (end - from) : 0ULL;
    for (unsigned long long place = 0ULL; (place < length) && (count < s_work_state.line_capacity);)
    {
        const CliTextLine bounds =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = joined + from, .at = place, .size = length);
        s_work_state.body[count] = (CliSpan){joined + from + place, bounds.cut - place};
        count += 1u;
        place = bounds.next;
    }
    EMBED_CALL(cli_purpose_lines_count.purpose_uncommented, CliPurposeLinesCountCfg, .lines = s_work_state.body,
               .count = count, .mark = "*", .out = out);
}

// the first comment of `text`, the text of the file at `path` whose lines the work state contains, written into `out`
EMBED_INLINE void cli_comment_read(const PurposeCommentCtx *args)
{
    const char *text = args->text;
    VerbaLinea *out = args->out;
    // the extension of the file's name, in lower case
    const char *name = args->path;
    const unsigned long long name_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}), .set = "/\\");
    name = (name_end != 0ULL) ? (args->path + name_end) : name;
    const char *const dot =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = name,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), .byte = '.');
    char extension[16] = "";
    if ((dot != NULL) && (dot != name))
    {
        (void)EMBED_CALL(cli_text_out_capacity.lowered_into, CliTextOutCapacityCfg, .out_capacity = extension,
                         .capacity = sizeof(extension), .text = dot + 1);
    }
    if ((cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "md", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "markdown", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "mdx", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "rst", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "txt", .cap = CLI_CELLS_POOL})))
    {
        VerbaLinea paragraph = {s_work_state.made, s_work_state.capacity, 0ULL};
        EMBED_CALL(cli_first_paragraph, PurposeCommentCtx, .out = &paragraph);
        memor.cpy(&(MemoriaCfg){.dst = s_work_state.paragraph, .src = s_work_state.made, .bytes = paragraph.at});
        EMBED_CALL(cli_sentences, PurposeCommentCtx, .text = s_work_state.paragraph, .size = paragraph.at, .out = out);
        return;
    }
    cli_marks_get(args);
    unsigned int at =
        ((s_work_state.lines > 0u) && EMBED_CALL(cli_purpose_lines_span.purpose_starts, CliPurposeLinesSpanCfg,
                                                 .span = s_work_state.line[0], .text = "#!"))
            ? 1u
            : 0u;
    while (at < s_work_state.lines)
    {
        const CliSpan trimmed =
            EMBED_CALL(cli_purpose_lines_span.purpose_trim, CliPurposeLinesSpanCfg, .span = s_work_state.line[at]);
        if (trimmed.length == 0ULL)
        {
            at += 1u;
            continue;
        }
        VerbaLinea text_line = {s_work_state.made, s_work_state.capacity, 0ULL};
        if (s_work_state.has_line && EMBED_CALL(cli_purpose_lines_span.purpose_starts, CliPurposeLinesSpanCfg,
                                                .span = trimmed, .text = s_work_state.line_mark))
        {
            const unsigned int start = at;
            while ((at < s_work_state.lines) &&
                   EMBED_CALL(cli_purpose_lines_span.purpose_starts, CliPurposeLinesSpanCfg,
                              .span = EMBED_CALL(cli_purpose_lines_span.purpose_trim, CliPurposeLinesSpanCfg,
                                                 .span = s_work_state.line[at]),
                              .text = s_work_state.line_mark))
            {
                at += 1u;
            }
            EMBED_CALL(cli_purpose_lines_count.purpose_uncommented, CliPurposeLinesCountCfg,
                       .lines = s_work_state.line + start, .count = at - start, .mark = s_work_state.line_mark,
                       .out = &text_line);
        }
        else if (s_work_state.has_block && EMBED_CALL(cli_purpose_lines_span.purpose_starts, CliPurposeLinesSpanCfg,
                                                      .span = trimmed, .text = s_work_state.open))
        {
            EMBED_CALL(cli_block_read, PurposeCommentCtx, .at = &at, .out = &text_line);
        }
        else if ((cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "py", .cap = CLI_CELLS_POOL})) &&
                 (EMBED_CALL(cli_purpose_lines_span.purpose_starts, CliPurposeLinesSpanCfg, .span = trimmed,
                             .text = "\"\"\"") ||
                  EMBED_CALL(cli_purpose_lines_span.purpose_starts, CliPurposeLinesSpanCfg, .span = trimmed,
                             .text = "'''") ||
                  EMBED_CALL(cli_purpose_lines_span.purpose_starts, CliPurposeLinesSpanCfg, .span = trimmed,
                             .text = "r\"\"\"")))
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
            mmgr_line_put_n(&text_line, body,
                            (ended != NULL) ? (unsigned long long)(ended - body)
                                            : cellul.len(&(CatenaFinitaCfg){.src = body, .cap = CLI_CELLS_POOL}));
            // a docstring read is passed: a license's docstring does not stop the reading there
            at += 1u;
        }
        else
        {
            return;
        }
        const CliSpan comment = EMBED_CALL(cli_purpose_lines_span.purpose_trim, CliPurposeLinesSpanCfg,
                                           .span = (CliSpan){s_work_state.made, text_line.at});
        if ((comment.length != 0ULL) && !EMBED_CALL(cli_purpose_lines.purpose_license, CliPurposeLinesCfg,
                                                    .text = s_work_state.made, .length = text_line.at))
        {
            memor.cpy(&(MemoriaCfg){.dst = s_work_state.paragraph, .src = s_work_state.made, .bytes = text_line.at});
            EMBED_CALL(cli_sentences, PurposeCommentCtx, .text = s_work_state.paragraph, .size = text_line.at,
                       .out = out);
            return;
        }
    }
}

void cli_purpose_first_comment(const CliPurposePathTextCfg *args)
{
    if (EMBED_CALL(cli_byte_buffer_alloc, PurposeCommentCtx, .text = args->text, .size = args->size))
    {
        EMBED_CALL(cli_comment_read, PurposeCommentCtx, .path = args->path, .text = args->text, .out = args->out);
    }
    cli_byte_buffer_free();
}
