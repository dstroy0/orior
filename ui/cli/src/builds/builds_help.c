// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// builds_help.c: what the build files' readers and the schemas' share: a text's lines and the place of a byte; a
// file's findings and their fixes, stored and written as the protocol's diagnostics; the completion items of what was
// typed; a folder as a path in the tree; and a snapshot's version
#include "../command_line/callback_cfg.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "builds.h"

unsigned int *cli_build_lines_alloc(const CliBuildsTextSizeCfg *args)
{
    unsigned long long count = 1ULL;
    for (unsigned long long at = 0ULL; at < args->size; at += 1ULL)
    {
        count += (args->text[at] == '\n') ? 1ULL : 0ULL;
    }
    unsigned int *const starts =
        (count <= 0xffffffffULL) ? g_cli_guard->persistent_buf_alloc(count * sizeof(unsigned int), g_cli_build_owner)
                                 : NULL;
    // a count no more than 0xffffffff where the cell is taken, which an unsigned int stores
    EMBED_CALL(cli_builds_text_size.build_lines_get, CliBuildsTextSizeCfg, .text = args->text, .size = args->size,
               .starts = starts, .most = (starts != NULL) ? (unsigned int)count : 0u, .lines_out = args->lines_out);
    return starts;
}

void cli_build_lines_get(const CliBuildsTextSizeCfg *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    args->lines_out->text = text;
    args->lines_out->size = size;
    args->lines_out->start = args->starts;
    args->lines_out->count = 0u;
    if (args->most == 0u)
    {
        return;
    }
    args->starts[0] = 0u;
    args->lines_out->count = 1u;
    for (unsigned long long at = 0ULL; (at < size) && (args->lines_out->count < args->most);)
    {
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = text + at, .val = '\n', .bytes = (size_t)(size - at)});
        if (newline == NULL)
        {
            break;
        }
        at = (unsigned long long)(newline - text) + 1u;
        args->starts[args->lines_out->count] = (unsigned int)at;
        args->lines_out->count += 1u;
    }
}

CliTextPosition cli_build_position(const CliBuildsLinesAtCfg *args)
{
    unsigned int low = 0u;
    unsigned int high = args->lines->count;
    while (low < high)
    {
        const unsigned int middle = low + ((high - low) / 2u);
        if (args->lines->start[middle] <= args->at)
        {
            low = middle + 1u;
        }
        else
        {
            high = middle;
        }
    }
    unsigned int line = (low > 0u) ? low - 1u : 0u;
    unsigned long long start = (args->lines->count > 0u) ? args->lines->start[line] : 0ULL;
    const unsigned long long end = (args->at < args->lines->size) ? args->at : args->lines->size;
    for (unsigned long long walk = start; walk < end; walk += 1u)
    {
        if (args->lines->text[walk] == '\n')
        {
            line += 1u;
            start = walk + 1u;
        }
    }
    const unsigned long long units = (start <= end) ? EMBED_CALL(cli_text_size_byte.units, CliTextSizeByteCfg,
                                                                 .text = args->lines->text + start, .size = end - start)
                                                    : 0ULL;
    return (CliTextPosition){line, (unsigned int)units};
}

unsigned long long cli_build_line_start(const CliBuildsLineCfg *args)
{
    if (args->line < args->lines->count)
    {
        return args->lines->start[args->line];
    }
    unsigned long long at = (args->lines->count > 0u) ? args->lines->start[args->lines->count - 1u] : 0ULL;
    for (unsigned int walk = (args->lines->count > 0u) ? args->lines->count - 1u : 0u; walk < args->line; walk += 1u)
    {
        const char *const newline =
            (at < args->lines->size)
                ? memor.chr(&(MemoriaCfg){
                      .src = args->lines->text + at, .val = '\n', .bytes = (size_t)(args->lines->size - at)})
                : NULL;
        if (newline == NULL)
        {
            return args->lines->size;
        }
        at = (unsigned long long)(newline - args->lines->text) + 1u;
    }
    return at;
}

void cli_build_findings_begin(const CliBuildsLinesCfg *args)
{
    g_cli_guard->persistent_buf_release(args->found->finding, g_cli_build_owner);
    *args->found = (CliBuildFindings){NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u, 0u, 0};
    const unsigned long long finding_bytes = (unsigned long long)CLI_BUILD_FINDINGS * sizeof(CliBuildFinding);
    const unsigned long long fix_bytes = (unsigned long long)CLI_BUILD_FIXES * sizeof(CliBuildFix);
    char *const cell = g_cli_guard->persistent_buf_alloc(finding_bytes + fix_bytes + CLI_BUILD_TEXT, g_cli_build_owner);
    if (cell == NULL)
    {
        args->found->full = 1;
        return;
    }
    args->found->finding = (CliBuildFinding *)(void *)cell;
    args->found->finding_capacity = CLI_BUILD_FINDINGS;
    args->found->fix = (CliBuildFix *)(void *)(cell + finding_bytes);
    args->found->fix_capacity = CLI_BUILD_FIXES;
    args->found->text = cell + finding_bytes + fix_bytes;
    args->found->text_capacity = CLI_BUILD_TEXT;
}

VerbaLinea cli_build_msg_line(const CliBuildsLinesCfg *args)
{
    return (args->found->text != NULL) ? (VerbaLinea){args->found->text + args->found->used,
                                                      args->found->text_capacity - args->found->used, 0ULL}
                                       : (VerbaLinea){NULL, 0ULL, 0ULL};
}

typedef struct
{
    CliBuildFindings *found;
    const char *bytes;
    unsigned long long size;
    const VerbaLinea *line;
    unsigned int finding;
    const VerbaLinea *title;
    CliTextPosition place;
    VerbaLinea *out;
    const CliBuildFindings *found_read;
    const CliBuildFix *fix;
} BuildsHelpCtx;
// `line` stored in the findings' texts, in place where it was written there: where it starts, or CLI_BUILD_TEXT where
// it does not fit
EMBED_INLINE unsigned int help_text_store(const BuildsHelpCtx *args)
{
    const char *bytes = args->bytes;
    // a line written in place already has its capacity; any other is copied in
    const int in_position = (args->found->text != NULL) && (bytes == args->found->text + args->found->used);
    if ((args->found->text == NULL) || ((args->found->used + args->size + 1ULL) > args->found->text_capacity))
    {
        args->found->full = 1;
        return CLI_BUILD_TEXT;
    }
    const unsigned int at = args->found->used;
    if (!in_position)
    {
        memor.move_down(&(MemoriaCfg){.dst = args->found->text + at, .src = bytes, .bytes = (size_t)args->size});
    }
    args->found->text[at + args->size] = '\0';
    args->found->used += (unsigned int)args->size + 1u;
    return at;
}

// a line written in the findings' texts stored: where it starts, or CLI_BUILD_TEXT where it did not fit
EMBED_INLINE unsigned int help_line_store(const BuildsHelpCtx *args)
{
    if (!mmgr_line_ok(args->line))
    {
        args->found->full = 1;
        return CLI_BUILD_TEXT;
    }
    return EMBED_CALL(help_text_store, BuildsHelpCtx, .found = args->found, .bytes = args->line->out,
                      .size = args->line->at);
}

unsigned int cli_build_finding(const CliBuildsLinesCfg *args)
{
    if (args->found->count >= args->found->finding_capacity)
    {
        args->found->full = 1;
        return CLI_BUILD_NONE;
    }
    const unsigned int at = EMBED_CALL(help_line_store, BuildsHelpCtx, .found = args->found, .line = args->message);
    if (at == CLI_BUILD_TEXT)
    {
        return CLI_BUILD_NONE;
    }
    args->found->finding[args->found->count] = (CliBuildFinding){
        EMBED_CALL(cli_builds_lines_at.build_position, CliBuildsLinesAtCfg, .lines = args->lines, .at = args->from),
        EMBED_CALL(cli_builds_lines_at.build_position, CliBuildsLinesAtCfg, .lines = args->lines, .at = args->to),
        args->severity,
        args->code,
        at,
        (unsigned int)args->message->at};
    args->found->count += 1u;
    return args->found->count - 1u;
}

// a fix of the finding at `finding` begun with its title: its place, or CLI_BUILD_FIXES where it does not fit
EMBED_INLINE unsigned int help_fix(const BuildsHelpCtx *args)
{
    unsigned int finding = args->finding;
    if ((finding == CLI_BUILD_NONE) || (args->found->fixes >= args->found->fix_capacity))
    {
        args->found->full = 1;
        return CLI_BUILD_FIXES;
    }
    const unsigned int at = EMBED_CALL(help_line_store, BuildsHelpCtx, .found = args->found, .line = args->title);
    if (at == CLI_BUILD_TEXT)
    {
        return CLI_BUILD_FIXES;
    }
    CliBuildFix *const fix = &args->found->fix[args->found->fixes];
    memor.set(&(MemoriaCfg){.dst = fix, .val = 0, .bytes = sizeof(*fix)});
    fix->finding = finding;
    fix->title_at = at;
    fix->title_size = (unsigned int)args->title->at;
    return args->found->fixes;
}

void cli_build_edit_fix(const CliBuildsTextCfg *args)
{
    unsigned long long from = args->from;
    unsigned long long to = args->to;
    const unsigned int at =
        EMBED_CALL(help_fix, BuildsHelpCtx, .found = args->found, .finding = args->finding, .title = args->title);
    if (at == CLI_BUILD_FIXES)
    {
        return;
    }
    CliBuildFix *const fix = &args->found->fix[at];
    fix->from = EMBED_CALL(cli_builds_lines_at.build_position, CliBuildsLinesAtCfg, .lines = args->lines, .at = from);
    fix->to = EMBED_CALL(cli_builds_lines_at.build_position, CliBuildsLinesAtCfg, .lines = args->lines, .at = to);
    fix->text_at =
        EMBED_CALL(help_text_store, BuildsHelpCtx, .found = args->found, .bytes = args->text, .size = args->size);
    fix->text_size = (unsigned int)args->size;
    if (fix->text_at != CLI_BUILD_TEXT)
    {
        args->found->fixes += 1u;
    }
}

void cli_build_edit_fix_twice(const CliBuildsTextCfg *args)
{
    unsigned long long also_from = args->also_from;
    unsigned long long also_to = args->also_to;
    const unsigned int fixes = args->found->fixes;
    cli_builds_text.build_edit_fix(args);
    if (args->found->fixes > fixes)
    {
        CliBuildFix *const fix = &args->found->fix[fixes];
        fix->also = 1;
        fix->also_from =
            EMBED_CALL(cli_builds_lines_at.build_position, CliBuildsLinesAtCfg, .lines = args->lines, .at = also_from);
        fix->also_to =
            EMBED_CALL(cli_builds_lines_at.build_position, CliBuildsLinesAtCfg, .lines = args->lines, .at = also_to);
    }
}

void cli_build_run_fix(const CliBuildsFoundCfg *args)
{
    unsigned long long folder_size = args->folder_size;
    const unsigned int at =
        EMBED_CALL(help_fix, BuildsHelpCtx, .found = args->found, .finding = args->finding, .title = args->title);
    if (at == CLI_BUILD_FIXES)
    {
        return;
    }
    CliBuildFix *const fix = &args->found->fix[at];
    fix->run = 1;
    fix->folder_at =
        EMBED_CALL(help_text_store, BuildsHelpCtx, .found = args->found, .bytes = args->folder, .size = folder_size);
    fix->folder_size = (unsigned int)folder_size;
    fix->text_at = EMBED_CALL(help_text_store, BuildsHelpCtx, .found = args->found, .bytes = args->line_text,
                              .size = args->line_size);
    fix->text_size = (unsigned int)args->line_size;
    if ((fix->folder_at != CLI_BUILD_TEXT) && (fix->text_at != CLI_BUILD_TEXT))
    {
        args->found->fixes += 1u;
    }
}

// a place written as the protocol's `{"line", "character"}`
EMBED_INLINE void help_position_write(const BuildsHelpCtx *args)
{
    mmgr_line_put(args->out, "{\"line\":");
    mmgr_line_decimal(args->out, args->place.line, 1u);
    mmgr_line_put(args->out, ",\"character\":");
    mmgr_line_decimal(args->out, args->place.col, 1u);
    mmgr_line_ch(args->out, '}');
}

// the fix at `at` written
EMBED_INLINE void help_fix_write(const BuildsHelpCtx *args)
{
    VerbaLinea *out = args->out;
    mmgr_line_put(out, "{\"title\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
               .text = args->found_read->text + args->fix->title_at, .length = args->fix->title_size);
    if (args->fix->run)
    {
        mmgr_line_put(out, ",\"run\":{\"folder\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
                   .text = args->found_read->text + args->fix->folder_at, .length = args->fix->folder_size);
        mmgr_line_put(out, ",\"line\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = out,
                   .text = args->found_read->text + args->fix->text_at, .length = args->fix->text_size);
        mmgr_line_put(out, "}}");
        return;
    }
    const CliTextEdit edit = {args->fix->from, args->fix->to, args->found_read->text + args->fix->text_at,
                              args->fix->text_size};
    mmgr_line_put(out, ",\"edits\":[");
    EMBED_CALL(cli_edits.text_edit_write, CliEditsCfg, .edit = &edit, .out = out);
    if (args->fix->also)
    {
        const CliTextEdit also = {args->fix->also_from, args->fix->also_to, args->found_read->text + args->fix->text_at,
                                  args->fix->text_size};
        mmgr_line_ch(out, ',');
        EMBED_CALL(cli_edits.text_edit_write, CliEditsCfg, .edit = &also, .out = out);
    }
    mmgr_line_put(out, "]}");
}

void cli_build_findings_write(const CliBuildsCfg *args)
{
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; at < args->found_read->count; at += 1u)
    {
        const CliBuildFinding *const one = &args->found_read->finding[at];
        mmgr_line_put(args->out, (at > 0u) ? ",{\"range\":{\"start\":" : "{\"range\":{\"start\":");
        EMBED_CALL(help_position_write, BuildsHelpCtx, .place = one->from, .out = args->out);
        mmgr_line_put(args->out, ",\"end\":");
        EMBED_CALL(help_position_write, BuildsHelpCtx, .place = one->to, .out = args->out);
        mmgr_line_put(args->out, "},\"severity\":");
        mmgr_line_decimal(args->out, one->severity, 1u);
        mmgr_line_put(args->out, ",\"message\":{\"kind\":\"markdown\",\"value\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                   .text = args->found_read->text + one->message_at, .length = one->message_size);
        mmgr_line_put(args->out, "},\"source\":\"orior\",\"code\":");
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = one->code,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = one->code, .cap = CLI_POOL_BYTES}));
        mmgr_line_put(args->out, ",\"data\":{\"orior\":[");
        int first = 1;
        for (unsigned int fix = 0u; fix < args->found_read->fixes; fix += 1u)
        {
            if (args->found_read->fix[fix].finding == at)
            {
                mmgr_line_put(args->out, first ? "" : ",");
                EMBED_CALL(help_fix_write, BuildsHelpCtx, .found_read = args->found_read,
                           .fix = &args->found_read->fix[fix], .out = args->out);
                first = 0;
            }
        }
        mmgr_line_put(args->out, "]}}");
    }
    mmgr_line_ch(args->out, ']');
}

unsigned long long cli_build_byte_at(const CliBuildsPathCfg *args)
{
    unsigned long long start = 0ULL;
    for (unsigned int walk = 0u; walk < args->line; walk += 1u)
    {
        const char *const newline =
            (start < args->size) ? memor.chr(&(MemoriaCfg){
                                       .src = args->text + start, .val = '\n', .bytes = (size_t)(args->size - start)})
                                 : NULL;
        if (newline == NULL)
        {
            return args->size;
        }
        start = (unsigned long long)(newline - args->text) + 1u;
    }
    const char *const newline =
        (start < args->size)
            ? memor.chr(&(MemoriaCfg){.src = args->text + start, .val = '\n', .bytes = (size_t)(args->size - start)})
            : NULL;
    const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - args->text) : args->size;
    return start + EMBED_CALL(cli_edits_line.text_byte_get, CliEditsLineCfg, .line = args->text + start,
                              .size = end - start, .col = args->col);
}

int cli_build_word_letter(const CliEachLetterCfg *args)
{
    unsigned int letter = args->letter;
    return EMBED_CALL(cli_text_table.alphanumeric, CliTextCfg, .letter = letter) || (letter == '_') || (letter == '$');
}

unsigned long long cli_build_run_before(const CliBuildsTextAtCfg *args)
{
    const char *text = args->text;
    unsigned long long at = args->at;
    unsigned long long line_start = at;
    line_start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = text, .at = line_start,
                            .set = "\n");
    unsigned long long start = at;
    while (start > line_start)
    {
        unsigned long long length = 0ULL;
        const unsigned int letter =
            EMBED_CALL(cli_text_length.letter_before, CliTextLengthCfg, .text = text + line_start,
                       .end_value = start - line_start, .length = &length);
        if (!EMBED_CALL(args->of, CliEachLetterCfg, .letter = letter))
        {
            break;
        }
        start -= length;
    }
    return start;
}

void cli_build_relative(const CliBuildsItemsCfg *args)
{
    const unsigned long long base =
        EMBED_CALL(cli_builds_text_at.build_run_before, CliBuildsTextAtCfg, .text = args->token, .at = args->token_size,
                   .of = cli_build_word_letter);
    if ((args->value_size <= base) ||
        (memor.cmp(&(MemoriaCfg){.src = args->value, .other = args->token, .bytes = (size_t)base}) != 0))
    {
        return;
    }
    (void)EMBED_CALL(cli_edits_items.completion_append, CliEditsItemsCfg, .items = args->items,
                     .label = args->value + base, .label_size = args->value_size - base, .type = args->type,
                     .detail = args->detail, .detail_size = args->detail_size, .insert = args->value + base,
                     .insert_size = args->value_size - base, .snippet = 0);
}

void cli_build_items(const CliBuildsItemsTypeCfg *args)
{
    for (unsigned int at = 0u; at < args->count; at += 1u)
    {
        unsigned long long label = cellul.len(&(CatenaFinitaCfg){.src = args->words[at], .cap = CLI_POOL_BYTES});
        while ((label >= 2u) &&
               (memor.cmp(&(MemoriaCfg){.src = args->words[at] + label - 2u, .other = "()", .bytes = 2u}) == 0))
        {
            label -= 2u;
        }
        (void)EMBED_CALL(cli_edits_items.completion_append, CliEditsItemsCfg, .items = args->items,
                         .label = args->words[at], .label_size = label, .type = args->type, .detail = args->detail,
                         .detail_size = cellul.len(&(CatenaFinitaCfg){.src = args->detail, .cap = CLI_POOL_BYTES}),
                         .insert = args->words[at],
                         .insert_size = cellul.len(&(CatenaFinitaCfg){.src = args->words[at], .cap = CLI_POOL_BYTES}),
                         .snippet = 0);
    }
}

void cli_build_inside(const CliBuildsRootCfg *args)
{
    VerbaLinea line = {args->out_capacity, CLI_PATH_MAX_LEN, 0ULL};
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = args->root, .cap = CLI_POOL_BYTES});
    const char after = args->folder[length];
    if (!EMBED_CALL(cli_text_size_one.same, CliTextSizeOneCfg, .one = args->folder, .other_pointer = args->root,
                    .size = length) ||
        ((after != '\0') && (after != '/') && (after != '\\') && (length > 0u) && (args->root[length - 1u] != '/') &&
         (args->root[length - 1u] != '\\')))
    {
        args->out_capacity[0] = '\0';
        return;
    }
    const char *rest = args->folder + length;
    rest += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = rest, .at = 0ULL,
                       .size = cellul.len(&(CatenaFinitaCfg){.src = rest, .cap = CLI_POOL_BYTES}), .set = "/\\");
    (void)EMBED_CALL(cli_text_from.append_swap, CliTextFromCfg, .out = &line, .text = rest, .from = '\\', .to = '/');
    line.at = EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->out_capacity,
                         .at = line.at, .set = "/");
    (void)mmgr_line_finish(&line);
}

int cli_build_snapshot(const CliBuildsSizeVersionCfg *args)
{
    unsigned long long from = 0ULL;
    unsigned long long to = 0ULL;
    EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = args->version, .size = args->size,
               .from_u64s = &from, .to_u64s = &to);
    return EMBED_CALL(cli_text_size_suffix.ends, CliTextSizeSuffixCfg, .text = args->version + from, .size = to - from,
                      .suffix = "-SNAPSHOT");
}
