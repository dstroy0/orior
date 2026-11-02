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
#define CLI_COMMENT_ROOM (1u << 18u)

// The room a comment is read in, guarded by the lock: the text's lines and a block comment's, `line_room` of each, the
// comment marks of its language, and the texts made, `room` bytes each; the lists and the texts one cell sized to
// the text read
typedef struct
{
    CliSpan *line;
    unsigned int lines;
    unsigned int line_room;
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
    unsigned long long room;
} CliCommentRoom;

static CliCommentRoom s_room;

typedef struct
{
    const char *text;
    unsigned long long size;
    const char *path;
    VerbaLinea *out;
    unsigned int *at;
} PurposeCommentCtx;
// a text's lines, as Rust's lines() splits them, CLI_COMMENT_LINES at the most, read into the room where it has room
// for a list of them: their count
EMBED_INLINE unsigned int cli_lines_read(const PurposeCommentCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    unsigned int lines = 0u;
    for (unsigned long long at = 0ull; (at < size) && (lines < CLI_COMMENT_LINES);)
    {
        const CliTextLine bounds = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = at, .size = size);
        const unsigned long long end = bounds.stop;
        unsigned long long length = end - at;
        length -= bounds.stop - bounds.cut;
        if (s_room.line != NULL)
        {
            s_room.line[lines] = (CliSpan){text + at, length};
        }
        lines += 1u;
        at = end + 1ull;
    }
    return lines;
}

// the room taken for the lines of `size` bytes of `text`, and the texts made of them, each its bytes and one more at
// the most CLI_COMMENT_ROOM, the lines read into it: 1, or 0 where the cellblock could not meet it
EMBED_INLINE int cli_room_take(const PurposeCommentCtx *args)
{
    s_room.line = NULL;
    const unsigned int lines = cli_lines_read(args);
    const unsigned int line_room = (lines != 0u) ? lines : 1u;
    const unsigned long long room = (args->size < CLI_COMMENT_ROOM) ? (args->size + 1ull) : CLI_COMMENT_ROOM;
    const unsigned long long spans = 2ull * line_room * sizeof(CliSpan);
    char *const cell = g_cli_cells->persistent_buf_alloc(spans + (3ull * room), &g_cli_purpose_cells);
    if (cell == NULL)
    {
        return 0;
    }
    // the spans on the cell's boundary, the texts after them
    s_room.line = (CliSpan *)(void *)cell;
    s_room.body = s_room.line + line_room;
    s_room.line_room = line_room;
    s_room.made = cell + spans;
    s_room.joined = s_room.made + room;
    s_room.paragraph = s_room.joined + room;
    s_room.room = room;
    s_room.lines = cli_lines_read(args);
    return 1;
}

// the room's cell given back
static void cli_room_free(void)
{
    g_cli_cells->persistent_buf_release(s_room.line, &g_cli_purpose_cells);
    s_room.line = NULL;
    s_room.body = NULL;
    s_room.lines = 0u;
}

static void cli_plugin_each(const CliEachPluginCfg *args)
{
    if (!cellul.eq(&(CatenaFinitaCfg){.src = args->plugin->id, .other = s_room.id, .cap = CLI_CELLS_POOL}))
    {
        return;
    }
    CliJson json = {0};
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea unread = {why, sizeof(why), 0ull};
    const unsigned int root =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_take, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = args->plugin->text, .size = args->plugin->size, .nodes_more = 0u, .text_more = 0ull,
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
    s_room.has_line = line != NULL;
    (void)cellul.copy(&(CatenaFinitaCfg){
        .dst = s_room.line_mark, .src = (line != NULL) ? line : "", .cap = sizeof(s_room.line_mark)});
    s_room.has_block = (open != NULL) && (close != NULL);
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_room = s_room.open, .text = open, .otherwise = "");
    EMBED_CALL(cli_text_out.copied_or, CliTextOutCfg, .out_room = s_room.close, .text = close, .otherwise = "");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    // the first plugin of the id names the marks
    s_room.id[0] = '\0';
}

// the comment marks of the language of the file at `path`, as its plugin names them, read into the room
EMBED_INLINE void cli_marks_get(const PurposeCommentCtx *args)
{
    const char *path = args->path;
    s_room.has_line = 0;
    s_room.has_block = 0;
    if (EMBED_CALL(cli_format_table.language_get, CliFormatCfg, .path = path, .id = s_room.id))
    {
        EMBED_CALL(cli_plugins_each_table.each, CliPluginsEachCfg, .each = cli_plugin_each, .context = NULL);
    }
}

// the first sentences of a text, its words joined by spaces, up to CLI_PURPOSE_TEXT_MAX_LEN letters, written into `out`
static void cli_sentences(const PurposeCommentCtx *args)
{
    const char *text = args->text;
    VerbaLinea joined = {s_room.joined, s_room.room, 0ull};
    unsigned long long letters = 0ull;
    unsigned long long cut = 0ull;
    int in_word = 0;
    for (unsigned long long at = 0ull; at < args->size;)
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
        if (!in_word && (joined.at != 0ull))
        {
            mmgr_line_ch(&joined, ' ');
            letters += 1ull;
            cut = (letters == CLI_PURPOSE_TEXT_MAX_LEN) ? joined.at : cut;
        }
        in_word = 1;
        mmgr_line_put_n(&joined, text + at, step);
        letters += 1ull;
        cut = (letters == CLI_PURPOSE_TEXT_MAX_LEN) ? joined.at : cut;
        at += step;
    }
    if (letters <= CLI_PURPOSE_TEXT_MAX_LEN)
    {
        mmgr_line_put_n(args->out, s_room.joined, joined.at);
        return;
    }
    // the cut's last sentence end past its first third, else the cut, its trailing whitespace left off, and an ellipsis
    for (unsigned long long end = cut; end >= 2ull; end -= 1ull)
    {
        if ((s_room.joined[end - 2ull] == '.') && (s_room.joined[end - 1ull] == ' '))
        {
            if ((end - 2ull) > (CLI_PURPOSE_TEXT_MAX_LEN / 3u))
            {
                mmgr_line_put_n(args->out, s_room.joined, end - 1ull);
                return;
            }
            break;
        }
    }
    const CliSpan trimmed =
        EMBED_CALL(cli_purpose_lines_span.purpose_trim, CliPurposeLinesSpanCfg, .span = (CliSpan){s_room.joined, cut});
    mmgr_line_put_n(args->out, trimmed.at, trimmed.length);
    mmgr_line_put(args->out, "\xe2\x80\xa6");
}

// the first paragraph of a Markdown text under its headings, its marks of emphasis and its links' addresses left out
EMBED_INLINE void cli_first_paragraph(const PurposeCommentCtx *args)
{
    const unsigned long long begun = args->out->at;
    VerbaLinea joined = {s_room.joined, s_room.room, 0ull};
    unsigned int taken = 0u;
    for (unsigned int at = 0u; at < s_room.lines; at += 1u)
    {
        const CliSpan trimmed =
            EMBED_CALL(cli_purpose_lines_span.purpose_trim, CliPurposeLinesSpanCfg, .span = s_room.line[at]);
        if (trimmed.length == 0ull)
        {
            if (taken != 0u)
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
        if ((taken == 0u) && lead)
        {
            continue;
        }
        if (taken != 0u)
        {
            mmgr_line_ch(&joined, ' ');
        }
        mmgr_line_put_n(&joined, trimmed.at, trimmed.length);
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
    for (unsigned long long at = begun; at < args->out->at; at += 1ull)
    {
        const char letter = written[at];
        const int double_under = (letter == '_') && ((at + 1ull) < args->out->at) && (written[at + 1ull] == '_');
        if (double_under)
        {
            at += 1ull;
            continue;
        }
        if ((letter == '[') || (letter == '*') || (letter == '`'))
        {
            continue;
        }
        written[paragraph] = letter;
        paragraph += 1ull;
    }
    args->out->at = paragraph;
}

// the comment that a block comment's open mark starts at line `at` of the room contains, its lines written into `out`,
// `at` moved past it
EMBED_INLINE void cli_block_read(const PurposeCommentCtx *args)
{
    unsigned int *at = args->at;
    VerbaLinea *out = args->out;
    // the room's lines from `at` on, joined by newlines
    VerbaLinea rest = {s_room.joined, s_room.room, 0ull};
    for (unsigned int line = *at; line < s_room.lines; line += 1u)
    {
        if (line != *at)
        {
            mmgr_line_ch(&rest, '\n');
        }
        mmgr_line_put_n(&rest, s_room.line[line].at, s_room.line[line].length);
    }
    mmgr_line_finish(&rest);
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
    // the body's lines, each with its marks taken off, no more than the room's lines they were joined from
    unsigned int count = 0u;
    const unsigned long long length = (end > from) ? (end - from) : 0ull;
    for (unsigned long long place = 0ull; (place < length) && (count < s_room.line_room);)
    {
        const CliTextLine bounds =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = joined + from, .at = place, .size = length);
        s_room.body[count] = (CliSpan){joined + from + place, bounds.cut - place};
        count += 1u;
        place = bounds.next;
    }
    EMBED_CALL(cli_purpose_lines_count.purpose_uncommented, CliPurposeLinesCountCfg, .lines = s_room.body,
               .count = count, .mark = "*", .out = out);
}

// the first comment of `text`, the text of the file at `path` whose lines the room contains, written into `out`
EMBED_INLINE void cli_comment_read(const PurposeCommentCtx *args)
{
    const char *text = args->text;
    VerbaLinea *out = args->out;
    // the extension of the file's name, in lower case
    const char *name = args->path;
    const unsigned long long name_past =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}), .set = "/\\");
    name = (name_past != 0ull) ? (args->path + name_past) : name;
    const char *const dot =
        EMBED_CALL(cli_text_byte.last, CliTextByteCfg, .text = name,
                   .length_value = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}), .byte = '.');
    char extension[16] = "";
    if ((dot != NULL) && (dot != name))
    {
        (void)EMBED_CALL(cli_text_out_room.lowered_into, CliTextOutRoomCfg, .out_room = extension,
                         .room = sizeof(extension), .text = dot + 1);
    }
    if ((cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "md", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "markdown", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "mdx", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "rst", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = extension, .other = "txt", .cap = CLI_CELLS_POOL})))
    {
        VerbaLinea paragraph = {s_room.made, s_room.room, 0ull};
        EMBED_CALL(cli_first_paragraph, PurposeCommentCtx, .out = &paragraph);
        memor.cpy(&(MemoriaCfg){.dst = s_room.paragraph, .src = s_room.made, .bytes = paragraph.at});
        EMBED_CALL(cli_sentences, PurposeCommentCtx, .text = s_room.paragraph, .size = paragraph.at, .out = out);
        return;
    }
    cli_marks_get(args);
    unsigned int at = ((s_room.lines > 0u) && EMBED_CALL(cli_purpose_lines_span.purpose_starts, CliPurposeLinesSpanCfg,
                                                         .span = s_room.line[0], .text = "#!"))
                          ? 1u
                          : 0u;
    while (at < s_room.lines)
    {
        const CliSpan trimmed =
            EMBED_CALL(cli_purpose_lines_span.purpose_trim, CliPurposeLinesSpanCfg, .span = s_room.line[at]);
        if (trimmed.length == 0ull)
        {
            at += 1u;
            continue;
        }
        VerbaLinea text_line = {s_room.made, s_room.room, 0ull};
        if (s_room.has_line && EMBED_CALL(cli_purpose_lines_span.purpose_starts, CliPurposeLinesSpanCfg,
                                          .span = trimmed, .text = s_room.line_mark))
        {
            const unsigned int start = at;
            while ((at < s_room.lines) &&
                   EMBED_CALL(cli_purpose_lines_span.purpose_starts, CliPurposeLinesSpanCfg,
                              .span = EMBED_CALL(cli_purpose_lines_span.purpose_trim, CliPurposeLinesSpanCfg,
                                                 .span = s_room.line[at]),
                              .text = s_room.line_mark))
            {
                at += 1u;
            }
            EMBED_CALL(cli_purpose_lines_count.purpose_uncommented, CliPurposeLinesCountCfg,
                       .lines = s_room.line + start, .count = at - start, .mark = s_room.line_mark, .out = &text_line);
        }
        else if (s_room.has_block && EMBED_CALL(cli_purpose_lines_span.purpose_starts, CliPurposeLinesSpanCfg,
                                                .span = trimmed, .text = s_room.open))
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
                                           .span = (CliSpan){s_room.made, text_line.at});
        if ((comment.length != 0ull) && !EMBED_CALL(cli_purpose_lines.purpose_license, CliPurposeLinesCfg,
                                                    .text = s_room.made, .length = text_line.at))
        {
            memor.cpy(&(MemoriaCfg){.dst = s_room.paragraph, .src = s_room.made, .bytes = text_line.at});
            EMBED_CALL(cli_sentences, PurposeCommentCtx, .text = s_room.paragraph, .size = text_line.at, .out = out);
            return;
        }
    }
}

void cli_purpose_first_comment(const CliPurposePathTextCfg *args)
{
    static CliLock s_lock = CLI_LOCK_INIT;
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_lock);
    if (EMBED_CALL(cli_room_take, PurposeCommentCtx, .text = args->text, .size = args->size))
    {
        EMBED_CALL(cli_comment_read, PurposeCommentCtx, .path = args->path, .text = args->text, .out = args->out);
    }
    cli_room_free();
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_lock);
}
