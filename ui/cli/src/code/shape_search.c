// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// shape_search.c: every match of a pattern in a text: the pattern read as its parts, its statement ends at its close
// left off; each place of the code tried in turn, a match's statement ends at its close left off and the search going
// on after it; each match's span and first line found by one walk of the text forward; and the template written with
// each placeholder as the code it took, its statement ends at its close left off. The code's tokens past
// CLI_SHAPE_TOKENS and the pattern's past CLI_SHAPE_PATTERN are not read. One search runs at a time
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "shape_internal.h"

// A walk of the text forward: the byte it stands at, its line and its column in UTF-16 units, and where its line starts
typedef struct
{
    unsigned long long at;
    unsigned int line;
    unsigned int col;
    unsigned long long line_start;
} CliShapeWalk;
// the bytes a match's template is written in

// A search's room, a cell taken with the search's lock and given back with it: the code's tokens and the pattern's
// parts, each part's hole, the holes and the matcher, the tokens' and the holes' lists after it in the same cell, as
// long as the code and the pattern are in bytes
typedef struct
{
    unsigned int *hole_of;
    CliShapeTokens code;
    CliShapeTokens parts;
    CliShapeHoles holes;
    CliShapeMatcher matcher;
} CliShapeSearchRoom;

static CliLock s_search_lock = CLI_LOCK_INIT;
static CliShapeSearchRoom *s_room;

typedef struct
{
    const char *src;
    unsigned long long size;
    CliShapeWalk *walk;
    unsigned long long byte;
    CliSymbolsLanguages *languages;
    const char *language;
    const char *pattern;
    const char *template_text;
    VerbaLinea *out;
    unsigned int code_room;
    unsigned int part_room;
} ShapeSearchCtx;
// the walk moved forward to `byte`: the place it stands at
EMBED_INLINE CliTextPlace search_walk(const ShapeSearchCtx *args)
{
    unsigned long long size = args->size;
    while (args->walk->at < args->byte)
    {
        unsigned long long length = 0ull;
        const unsigned int letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = args->src, .size = size,
                                               .at = args->walk->at, .length = &length);
        args->walk->at += length;
        if (letter == '\n')
        {
            args->walk->line += 1u;
            args->walk->col = 0u;
            args->walk->line_start = args->walk->at;
        }
        else
        {
            args->walk->col += (letter >= 0x10000u) ? 2u : 1u;
        }
    }
    return (CliTextPlace){args->walk->line, args->walk->col};
}

// the pattern read as its parts into the matcher's storage: their count, 0 where its language is none read here
EMBED_INLINE unsigned int search_pattern(const ShapeSearchCtx *args)
{
    CliSymbolsLanguages *languages = args->languages;
    const char *language = args->language;
    unsigned long long size = args->size;
    s_room->holes.count = 0u;
    if (!EMBED_CALL(cli_shape_languages_internal.tokens, CliShapeLanguagesCtx, .languages = languages,
                    .language = language, .src = args->pattern, .size = size, .tokens = &s_room->parts,
                    .holes = &s_room->holes))
    {
        return 0u;
    }
    for (unsigned int at = 0u; at < s_room->parts.count; at += 1u)
    {
        s_room->hole_of[at] = CLI_SHAPE_HOLES;
    }
    for (unsigned int at = 0u; at < s_room->holes.count; at += 1u)
    {
        if (s_room->holes.hole[at].token < s_room->parts.count)
        {
            s_room->hole_of[s_room->holes.hole[at].token] = at;
        }
    }
    // a statement's end and a block's close the pattern's own end puts are no parts of it
    unsigned int count = s_room->parts.count;
    while ((count > 0u) && (s_room->hole_of[count - 1u] == CLI_SHAPE_HOLES) &&
           ((s_room->parts.kind[count - 1u] == CLI_SHAPE_BREAK) ||
            (s_room->parts.start[count - 1u] == s_room->parts.end[count - 1u])))
    {
        count -= 1u;
    }
    s_room->matcher.parts = &s_room->parts;
    return count;
}

// the template written with each placeholder as the code it took into `out`
EMBED_INLINE void search_fill(const ShapeSearchCtx *args)
{
    const CliShapeTokens *const code = s_room->matcher.tokens;
    const char *rest = args->template_text;
    for (const char *open = cellul.chr(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL, .byte = '$'});
         open != NULL; open = cellul.chr(&(CatenaFinitaCfg){.src = rest, .cap = CLI_CELLS_POOL, .byte = '$'}))
    {
        mmgr_line_put_n(args->out, rest, (unsigned long long)(open - rest));
        const char *const after = open + 1;
        const char *const close = cellul.chr(&(CatenaFinitaCfg){.src = after, .cap = CLI_CELLS_POOL, .byte = '$'});
        if (close == NULL)
        {
            mmgr_line_ch(args->out, '$');
            rest = after;
            continue;
        }
        const char *const colon = memor.chr(&(MemoriaCfg){.src = after, .val = ':', .bytes = (size_t)(close - after)});
        const unsigned long long name_size = (unsigned long long)(((colon != NULL) ? colon : close) - after);
        const unsigned int bound = EMBED_CALL(cli_shape_internal.bind, CliShapeCtx, .matcher_read = &s_room->matcher,
                                              .name = after, .size = name_size);
        if (bound == CLI_SHAPE_HOLES)
        {
            mmgr_line_ch(args->out, '$');
            rest = after;
            continue;
        }
        const unsigned int from = s_room->matcher.bound_from[bound];
        unsigned int to = s_room->matcher.bound_to[bound];
        while ((to > from) && (code->kind[to - 1u] == CLI_SHAPE_BREAK))
        {
            to -= 1u;
        }
        if (to > from)
        {
            mmgr_line_put_n(args->out, s_room->matcher.src + code->start[from], code->end[to - 1u] - code->start[from]);
        }
        rest = close + 1;
    }
    mmgr_line_put(args->out, rest);
}

// the search's room and its ends given back, and the lock with it
static void search_release(void)
{
    EMBED_CALL(cli_shape_matcher_internal.ends_free, CliShapeMatcherCtx, .matcher = &s_room->matcher);
    g_cli_cells->persistent_buf_release(s_room, &g_cli_shape_cells);
    s_room = NULL;
    EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_search_lock);
}

// the search's room taken for `code_room` tokens of the code and `part_room` of the pattern, the lock taken with it:
// 1, or 0 with the lock given back where the cellblock could not meet it
EMBED_INLINE int search_room_take(const ShapeSearchCtx *args)
{
    // the starts and ends of both and each part's hole, then the kinds of both, then their roles
    const unsigned long long words = (2ull * args->code_room) + (3ull * args->part_room);
    const unsigned long long bytes = 2ull * ((unsigned long long)args->code_room + args->part_room);
    EMBED_CALL(cli_lock_internal.lock_take, CliLockCtx, .lock = &s_search_lock);
    s_room =
        g_cli_cells->persistent_buf_alloc(sizeof(*s_room) + (words * sizeof(unsigned int)) + bytes, &g_cli_shape_cells);
    if (s_room == NULL)
    {
        EMBED_CALL(cli_lock_internal.lock_give, CliLockCtx, .lock = &s_search_lock);
        return 0;
    }
    // the lists start past the room, on a boundary of the room's pointers, the kinds past the words and the roles past
    // the kinds
    unsigned int *const listed = (unsigned int *)(void *)(s_room + 1);
    unsigned char *const kinds = (unsigned char *)(void *)(listed + words);
    unsigned char *const roles = kinds + args->code_room + args->part_room;
    s_room->code = (CliShapeTokens){kinds, roles, listed, listed + args->code_room, 0u, args->code_room, 0};
    s_room->parts = (CliShapeTokens){kinds + args->code_room,
                                     roles + args->code_room,
                                     listed + (2ull * args->code_room),
                                     listed + (2ull * args->code_room) + args->part_room,
                                     0u,
                                     args->part_room,
                                     0};
    s_room->hole_of = listed + (2ull * args->code_room) + (2ull * args->part_room);
    s_room->matcher.ends = NULL;
    s_room->matcher.ends_top = 0u;
    s_room->matcher.ends_room = 0u;
    return 1;
}

void cli_shape_search(const CliShapeLanguagesCfg *args)
{
    const char *src = args->src;
    const char *pattern = args->pattern;
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = pattern,
               .size = cellul.len(&(CatenaFinitaCfg){.src = pattern, .cap = CLI_CELLS_POOL}), .from_u64s = &from,
               .to_u64s = &to);
    // a token takes a byte at the least: the code contains no more tokens than bytes, nor the pattern; each below
    // its most, which an unsigned int stores
    const unsigned int code_room = (args->size < CLI_SHAPE_TOKENS) ? ((unsigned int)args->size + 1u) : CLI_SHAPE_TOKENS;
    const unsigned int part_room =
        ((to - from) < CLI_SHAPE_PATTERN) ? ((unsigned int)(to - from) + 1u) : CLI_SHAPE_PATTERN;
    if (!EMBED_CALL(search_room_take, ShapeSearchCtx, .code_room = code_room, .part_room = part_room))
    {
        return;
    }
    s_room->matcher.pattern = pattern + from;
    s_room->matcher.part_count = EMBED_CALL(search_pattern, ShapeSearchCtx, .languages = args->languages,
                                            .language = args->language, .pattern = pattern + from, .size = to - from);
    s_room->matcher.hole_of = s_room->hole_of;
    s_room->matcher.holes = &s_room->holes;
    if ((s_room->matcher.part_count == 0u) ||
        !EMBED_CALL(cli_shape_languages_internal.tokens, CliShapeLanguagesCtx, .languages = args->languages,
                    .language = args->language, .src = src, .size = args->size, .tokens = &s_room->code, .holes = NULL))
    {
        search_release();
        return;
    }
    s_room->matcher.src = src;
    s_room->matcher.tokens = &s_room->code;
    CliShapeWalk walk = {0ull, 0u, 0u, 0ull};
    for (unsigned int at = 0u; at < s_room->code.count;)
    {
        unsigned int end = 0u;
        s_room->matcher.steps = 0u;
        s_room->matcher.bound_count = 0u;
        if ((s_room->code.kind[at] == CLI_SHAPE_BREAK) ||
            !EMBED_CALL(cli_shape_matcher_internal.from, CliShapeMatcherCtx, .matcher = &s_room->matcher, .part = 0u,
                        .at = at, .end = &end))
        {
            at += 1u;
            continue;
        }
        while ((end > at) && (s_room->code.kind[end - 1u] == CLI_SHAPE_BREAK))
        {
            end -= 1u;
        }
        if (end == at)
        {
            at += 1u;
            continue;
        }
        CliShapeFound found = {{0u, 0u}, {0u, 0u}, NULL, 0ull, NULL, 0ull, 0};
        found.from = EMBED_CALL(search_walk, ShapeSearchCtx, .src = src, .size = args->size, .walk = &walk,
                                .byte = s_room->code.start[at]);
        found.line = src + walk.line_start;
        const unsigned long long line_size = EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = found.line,
                                                        .at = 0ull, .size = args->size - walk.line_start)
                                                 .stop;
        found.line_size =
            EMBED_CALL(cli_text_size_byte.trimmed_end, CliTextSizeByteCfg, .text = found.line, .size = line_size);
        found.to = EMBED_CALL(search_walk, ShapeSearchCtx, .src = src, .size = args->size, .walk = &walk,
                              .byte = s_room->code.end[end - 1u]);
        VerbaLinea with = {NULL, 0ull, 0ull, NULL, NULL, 0ull};
        if (args->template_text != NULL)
        {
            // the template's bytes, and for each placeholder, 3 bytes of it at the least, what the match spans at most
            const unsigned long long template_length =
                cellul.len(&(CatenaFinitaCfg){.src = args->template_text, .cap = CLI_CELLS_POOL});
            const unsigned long long span = (unsigned long long)(s_room->code.end[end - 1u] - s_room->code.start[at]);
            const unsigned long long with_room = template_length + (((template_length / 3ull) + 1ull) * span) + 1ull;
            char *const with_cell = g_cli_cells->temporary_buf_alloc(with_room, &g_cli_shape_cells);
            with = mmgr_line_over(with_cell, with_room);
            EMBED_CALL(search_fill, ShapeSearchCtx, .template_text = args->template_text, .out = &with);
            found.has_with = mmgr_line_ok(&with);
            found.with = with.out;
            found.with_size = with.at;
        }
        EMBED_CALL(args->each, CliShapeEachCfg, .context = args->context, .found = &found);
        g_cli_cells->persistent_buf_release(with.out, &g_cli_shape_cells);
        at = end;
    }
    search_release();
}

void cli_shape_found_write(const CliShapeCfg *args)
{
    mmgr_line_put(args->out, "{\"path\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = args->path,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(args->out, ",\"from\":");
    EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg, .place = args->found->from, .out = args->out);
    mmgr_line_put(args->out, ",\"to\":");
    EMBED_CALL(cli_edits_place.text_place_write, CliEditsPlaceCfg, .place = args->found->to, .out = args->out);
    mmgr_line_put(args->out, ",\"line\":");
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
               .text = args->found->line, .length = args->found->line_size);
    mmgr_line_put(args->out, ",\"with\":");
    if (args->found->has_with)
    {
        EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out,
                   .text = args->found->with, .length = args->found->with_size);
    }
    else
    {
        mmgr_line_put(args->out, "null");
    }
    mmgr_line_ch(args->out, '}');
}
