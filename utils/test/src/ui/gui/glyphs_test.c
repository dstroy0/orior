// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// glyphs_test.c: the glyphs' check: the system's code font is read, a character's glyph is drawn once and saved, a space
// has an advance and no pixels, the code face's cell is its glyphs' advance, and a text drawn lands its pixels where
// its characters are and ends where its width says. Then a glyph baked into the compositor's atlas at each place across
// a pixel: at the first place its coverage is the glyph's own mask byte for byte, and at the others it moves right
#include "../../../../../includes/window/block.h"
#include "../../../../../includes/window/glyphs.h"
#include "../cli/cli_check.h"

static unsigned char s_atlas_block[1u << 21u];
static GuiCompositor s_atlas;

// the sum of an atlas glyph's coverage weighted by its column: its weight's center across, by 256
static unsigned long long atlas_center(const GuiCompositor *compositor, embed_u32 entry)
{
    const GuiCompositorGlyphEntry *const glyph = &compositor->entry[entry];
    const embed_u32 side = compositor->limits.atlas_side;
    const unsigned char *const page = compositor->coverage + ((unsigned long long)glyph->page * side * side);
    unsigned long long weighted = 0ULL;
    unsigned long long total = 0ULL;
    for (embed_u32 row = 0u; row < glyph->height; row += 1u)
    {
        for (embed_u32 column = 0u; column < glyph->width; column += 1u)
        {
            const unsigned long long cover = page[((unsigned long long)(glyph->y + row) * side) + glyph->x + column];
            // a column's place on the page is its place in the glyph moved by the glyph's left
            weighted += cover * (unsigned long long)((embed_i32)column + glyph->left + 64) * 256ULL;
            total += cover;
        }
    }
    return (total != 0ULL) ? (weighted / total) : 0ULL;
}

// whether any pixel of `rect` on the raster is not `background_fill`
static int painted(const GuiRaster *raster, GuiRect rect, GuiColor background_fill)
{
    for (int y = rect.y; y < rect.y + rect.height; y += 1)
    {
        for (int x = rect.x; x < rect.x + rect.width; x += 1)
        {
            if (EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = (GuiRect){0, 0, (int)raster->width, (int)raster->height}, .x = x, .y = y) &&
                (raster->pixels[((unsigned int)y * raster->width) + (unsigned int)x] != background_fill))
            {
                return 1;
            }
        }
    }
    return 0;
}

static float off(float one, float other)
{
    return (one > other) ? one - other : other - one;
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    cli_check(EMBED_CALL(gui_block_size.init, GuiBlockSizeCfg, .pages = &g_gui_pages, .width = CLI_CHECK_MONITOR_WIDTH,
                         .height = CLI_CHECK_MONITOR_HEIGHT),
              "the block is reserved for the monitor");
    char error_text[1024];
    VerbaLinea why = {error_text, sizeof(error_text), 0ull};
    GuiGlyphs glyphs;
    const int made = EMBED_CALL(gui_glyphs.init, GuiGlyphsCfg, .glyphs = &glyphs, .faces = &g_gui_faces, .err_msg = &why);
    cli_check(made, "the system's fonts are read");
    if (!made)
    {
        mmgr_line_finish(&why);
        cli_check(0, error_text);
        return cli_on_check();
    }
    cli_check(glyphs.has[GUI_FACE_CODE], "the code face is read");
    cli_check(EMBED_CALL(gui_glyphs_face.face_had, GuiGlyphsFaceCfg, .glyphs_read = &glyphs, .face = GUI_FACE_CODE_BOLD) < GUI_FACES, "a face asked for is answered with one read");
    cli_check(EMBED_CALL(gui_glyphs_face.face_had, GuiGlyphsFaceCfg, .glyphs_read = &glyphs, .face = GUI_FACE_HEAD_BOLD) != GUI_FACE_HEAD_BOLD, "the interface's bold is not read until it is asked for");
    float regular_width = 0.0f;
    (void)EMBED_CALL(gui_glyphs_face_character.glyph, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_HEAD, .character = (unsigned int)'o', .size = 88u, .advance = &regular_width);
    const int bold_read = EMBED_CALL(gui_glyphs.head_bold_read, GuiGlyphsCfg, .glyphs = &glyphs);
    cli_check(!bold_read || (EMBED_CALL(gui_glyphs_face.face_had, GuiGlyphsFaceCfg, .glyphs_read = &glyphs, .face = GUI_FACE_HEAD_BOLD) == GUI_FACE_HEAD_BOLD), "asked for, it is read where the system has it");
    float bold_width = 0.0f;
    (void)EMBED_CALL(gui_glyphs_face_character.glyph, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_HEAD_BOLD, .character = (unsigned int)'o', .size = 88u, .advance = &bold_width);
    cli_check(!bold_read || (bold_width > regular_width), "the bold's characters are wider than the regular's");
    if (bold_read)
    {
        EMBED_CALL(gui_glyphs.head_bold_free, GuiGlyphsCfg, .glyphs = &glyphs);
    }
    cli_check((EMBED_CALL(gui_glyphs_face.face_had, GuiGlyphsFaceCfg, .glyphs_read = &glyphs, .face = GUI_FACE_HEAD_BOLD) != GUI_FACE_HEAD_BOLD) && (glyphs.data[GUI_FACE_HEAD_BOLD] == NULL), "given back it holds no file");

    // The interface face has no check mark. The window draws it from the symbol face, which it reads the first time a
    // character needs it.
    cli_check(!glyphs.symbol_looked && !glyphs.has[GUI_FACE_SYMBOL], "the symbol face is not looked for until a character needs it");
    float mark_advance = 0.0f;
    const GuiMask *const mark = EMBED_CALL(gui_glyphs_face_character.glyph, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_HEAD, .character = 0x2713u, .size = 13u, .advance = &mark_advance);
#if defined(_WIN32)
    cli_check(glyphs.has[GUI_FACE_SYMBOL], "Windows' symbol face is read for the check mark");
#endif
    float symbol_advance = 0.0f;
    cli_check(!glyphs.has[GUI_FACE_SYMBOL] || ((mark != NULL) && (mark->cover != NULL) && (EMBED_CALL(gui_glyphs_face_character.glyph, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_SYMBOL, .character = 0x2713u, .size = 13u, .advance = &symbol_advance) == mark)),
              "the check mark asked of the interface face is the symbol face's");

    float advance = 0.0f;
    const GuiMask *const character = EMBED_CALL(gui_glyphs_face_character.glyph, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_CODE, .character = 'M', .size = 13u, .advance = &advance);
    cli_check(character != NULL, "the glyph of M at thirteen pixels is drawn");
    cli_check((character != NULL) && (character->width > 4u) && (character->height > 6u), "the glyph has pixels of an M's size");
    cli_check((advance > 5.0f) && (advance < 12.0f), "the glyph's advance is a code cell at thirteen pixels");
    cli_check((character != NULL) && (character->top > 0) && (character->top <= 13), "the glyph's top is above the baseline");
    float again = 0.0f;
    cli_check(EMBED_CALL(gui_glyphs_face_character.glyph, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_CODE, .character = 'M', .size = 13u, .advance = &again) == character, "the same glyph asked again is the one retained");
    cli_check(off(again, advance) < 1e-6f, "the glyph retained retains its advance");
    cli_check(EMBED_CALL(gui_glyphs_face_character.glyph, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_CODE, .character = 'M', .size = 26u, .advance = &again) != character, "the glyph at another size is drawn anew");
    cli_check(off(again, 2.0f * advance) < 0.01f, "the advance at twice the size is twice");

    float space_advance = 0.0f;
    const GuiMask *const space = EMBED_CALL(gui_glyphs_face_character.glyph, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_CODE, .character = ' ', .size = 13u, .advance = &space_advance);
    cli_check((space != NULL) && (space->width == 0u) && (space->cover == NULL), "a space has no pixels");
    cli_check(off(space_advance, advance) < 1e-6f, "a space advances a cell as a character does");

    cli_check(off(EMBED_CALL(gui_glyphs_face_character.cell, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .size = 13u), advance) < 1e-6f, "the code face's cell is its glyphs' advance");

    // a glyph in a color is a cell of lanes, saved by its key, with no lanes where the glyph has no pixels
    float cell_advance = 0.0f;
    const GuiGlyphCell *const cell = EMBED_CALL(gui_glyphs_face_character.glyph_cell, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_CODE, .character = 'M', .size = 13u, .color = GUI_RGB(0xffu, 0xffu, 0xffu), .advance = &cell_advance);
    cli_check((cell != NULL) && (cell->width == character->width) && (cell->height == character->height) && (cell->left == character->left) &&
                  (cell->top == character->top) && (cell->red_blue != NULL),
              "the cell of M in white is the glyph's box with lanes");
    cli_check(off(cell_advance, advance) < 1e-6f, "the cell retains the glyph's advance");
    cli_check(EMBED_CALL(gui_glyphs_face_character.glyph_cell, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_CODE, .character = 'M', .size = 13u, .color = GUI_RGB(0xffu, 0xffu, 0xffu), .advance = &cell_advance) == cell,
              "the same cell asked again is the one retained");
    cli_check(EMBED_CALL(gui_glyphs_face_character.glyph_cell, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_CODE, .character = 'M', .size = 13u, .color = GUI_RGB(0xffu, 0x00u, 0x00u), .advance = &cell_advance) != cell,
              "the glyph in another color is another cell");
    cli_check((glyphs.ascii[glyphs.ascii_last].cell['M' - GUI_ASCII_FIRST] != NULL) && (glyphs.ascii[glyphs.ascii_last].color == GUI_RGB(0xffu, 0x00u, 0x00u)),
              "an ASCII character's cell is in its face, size and color's table at its index");
    const GuiGlyphCell *const space_cell = EMBED_CALL(gui_glyphs_face_character.glyph_cell, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_CODE, .character = ' ', .size = 13u, .color = GUI_RGB(0xffu, 0xffu, 0xffu), .advance = &cell_advance);
    cli_check((space_cell != NULL) && (space_cell->red_blue == NULL), "a space's cell has no lanes");
    unsigned int whole = 0u;
    unsigned int bare = 0u;
    for (unsigned long long at = 0ull; (cell != NULL) && (at < (unsigned long long)cell->width * cell->height); at += 1ull)
    {
        whole += (cell->rest[at] == 0u) ? 1u : 0u;
        bare += (cell->rest[at] == 255u) ? 1u : 0u;
    }
    cli_check((whole > 0u) && (bare > 0u), "a cell's lanes retain none of the ground where the glyph covers a pixel whole and all of it where it covers none");
    cli_check(EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = &glyphs, .face = GUI_FACE_CODE, .size = 13u) > 8.0f, "the ascent at thirteen pixels is the majority of them");
    cli_check(EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = &glyphs, .face = GUI_FACE_CODE, .size = 13u) > 1.0f, "the descent at thirteen pixels is below the baseline");

    GuiRaster raster;
    const GuiColor black = GUI_RGB(0u, 0u, 0u);
    const GuiColor white = GUI_RGB(0xffu, 0xffu, 0xffu);
    cli_check(EMBED_CALL(gui_block_raster.init, GuiBlockRasterCfg, .raster = &raster, .width = 64u, .height = 24u), "a raster for the text is made");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 64, 24}, .color = black);
    const float after = EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = &raster, .glyphs = &glyphs, .face = GUI_FACE_CODE, .size = 13u, .x_f32 = 2.0f, .y = 18, .text = "Hi", .length = 2ull, .color = white, .clip = (GuiRect){0, 0, 64, 24});
    cli_check(off(after, 2.0f + (2.0f * advance)) < 0.01f, "a text drawn ends two cells on");
    cli_check(painted(&raster, (GuiRect){2, 5, (int)advance, 14}, black), "the first character paints its cell");
    cli_check(painted(&raster, (GuiRect){2 + (int)advance, 5, (int)advance, 14}, black), "the second character paints its cell");
    cli_check(!painted(&raster, (GuiRect){2 + (int)(2.0f * advance) + 1, 0, 40, 24}, black),
              "nothing is painted past the text");
    cli_check(!painted(&raster, (GuiRect){0, 19, 64, 5}, black), "an H and an i paint nothing below the baseline");
    cli_check(off(EMBED_CALL(gui_glyphs_face_size.text_width, GuiGlyphsFaceSizeCfg, .glyphs = &glyphs, .face = GUI_FACE_CODE, .size = 13u, .text = "Hi", .length = 2ull), 2.0f * advance) < 0.01f,
              "a text's width is its characters' advances");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 64, 24}, .color = black);
    EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = &raster, .glyphs = &glyphs, .face = GUI_FACE_CODE, .size = 13u, .x_f32 = 2.0f, .y = 18, .text = "Hi", .length = 2ull, .color = white, .clip = (GuiRect){0, 0, 1, 1});
    cli_check(!painted(&raster, (GuiRect){1, 1, 63, 23}, black), "a text is drawn inside the clip alone");
    // a glyph's cell positioned with the pen on the baseline paints its box, and nothing outside its clip
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 64, 24}, .color = black);
    EMBED_CALL(gui_glyphs_raster.glyph_cell_layout, GuiGlyphsRasterCfg, .raster = &raster, .cell = cell, .x = 2, .y = 18,
               .clip = (GuiRect){0, 0, 64, 24});
    cli_check(painted(&raster, (GuiRect){2, 2, (int)advance + 2, 17}, black), "a glyph's cell positioned paints its box");
    EMBED_CALL(gui_raster_rect_color.fill, GuiRasterRectColorCfg, .raster = &raster, .rect = (GuiRect){0, 0, 64, 24}, .color = black);
    EMBED_CALL(gui_glyphs_raster.glyph_cell_layout, GuiGlyphsRasterCfg, .raster = &raster, .cell = cell, .x = 2, .y = 18,
               .clip = (GuiRect){40, 0, 24, 24});
    cli_check(!painted(&raster, (GuiRect){0, 0, 64, 24}, black), "and nothing outside its clip");
    EMBED_CALL(gui_glyphs_face_size.text_draw, GuiGlyphsFaceSizeCfg, .raster = &raster, .glyphs = &glyphs, .face = GUI_FACE_CODE, .size = 13u, .x_f32 = 2.0f, .y = 18, .text = "\xc3\xa9", .length = 2ull, .color = white, .clip = (GuiRect){0, 0, 64, 24});
    cli_check(painted(&raster, (GuiRect){2, 2, (int)advance, 16}, black), "a character of two bytes draws one glyph");

    const GuiCompositorLimitsCfg limits = {256u, 256u, 16u, 4u, 4u, 256u, 2u, 1024u, 16u};
    cli_check(EMBED_CALL(gui_compositor.init, GuiCompositorInitCfg, .compositor = &s_atlas, .limits = limits,
                         .block = s_atlas_block, .bytes = sizeof(s_atlas_block)) &&
                  EMBED_CALL(gui_compositor_atlas.range, GuiCompositorGlyphRangeCfg, .compositor = &s_atlas,
                             .face = GUI_FACE_CODE, .size = 13u, .first = GUI_ASCII_FIRST, .count = GUI_ASCII_COUNT),
              "an atlas is placed and the printable ASCII of the code face reserved in it");
    embed_u32 placed[GUI_COMPOSITOR_PHASES];
    for (embed_u32 phase = 0u; phase < GUI_COMPOSITOR_PHASES; phase += 1u)
    {
        placed[phase] = EMBED_CALL(gui_glyphs_atlas.bake, GuiGlyphsBakeCfg, .glyphs = &glyphs, .compositor = &s_atlas,
                                   .face = GUI_FACE_CODE, .character = 'M', .size = 13u, .phase = phase);
    }
    const embed_u32 m_place = ('M' - GUI_ASCII_FIRST) * GUI_COMPOSITOR_PHASES;
    cli_check((placed[0] == m_place) && (placed[3] == (m_place + 3u)) &&
                  (EMBED_CALL(gui_glyphs_atlas.bake, GuiGlyphsBakeCfg, .glyphs = &glyphs, .compositor = &s_atlas,
                              .face = GUI_FACE_CODE, .character = 'M', .size = 13u, .phase = 0u) == placed[0]),
              "an ASCII glyph is baked at its place in the range, one entry for each place, and found again");
    float m_advance = 0.0f;
    const GuiMask *const m_mask = EMBED_CALL(gui_glyphs_face_character.glyph, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs,
                                             .face = GUI_FACE_CODE, .character = 'M', .size = 13u, .advance = &m_advance);
    const GuiCompositorGlyphEntry *const first = &s_atlas.entry[placed[0]];
    int same = (m_mask != NULL) && (first->width == m_mask->width) && (first->height == m_mask->height) &&
               (first->left == m_mask->left) && (first->top == m_mask->top);
    const unsigned char *const page = s_atlas.coverage + ((unsigned long long)first->page * 256u * 256u);
    for (embed_u32 row = 0u; same && (row < first->height); row += 1u)
    {
        for (embed_u32 column = 0u; column < first->width; column += 1u)
        {
            same &= page[((unsigned long long)(first->y + row) * 256u) + first->x + column] ==
                    m_mask->cover[(row * m_mask->width) + column];
        }
    }
    cli_check(same && (first->advance == (embed_i32)((m_advance * 65536.0f) + 0.5f)),
              "at the first place the atlas holds the glyph's own mask byte for byte, and its advance in 16.16");
    const unsigned long long centers[4] = {atlas_center(&s_atlas, placed[0]), atlas_center(&s_atlas, placed[1]),
                                           atlas_center(&s_atlas, placed[2]), atlas_center(&s_atlas, placed[3])};
    cli_check((centers[1] > centers[0]) && (centers[2] > centers[1]) && (centers[3] > centers[2]) &&
                  ((centers[3] - centers[0]) > 160ULL) && ((centers[3] - centers[0]) < 224ULL),
              "each later place moves the glyph's ink right, three quarters of a pixel across the four");
    const embed_u32 baked_space = EMBED_CALL(gui_glyphs_atlas.bake, GuiGlyphsBakeCfg, .glyphs = &glyphs,
                                       .compositor = &s_atlas, .face = GUI_FACE_CODE, .character = ' ', .size = 13u,
                                       .phase = 0u);
    cli_check((baked_space == 0u) && (s_atlas.entry[baked_space].width == 0u) &&
                  (s_atlas.entry[baked_space].advance == first->advance),
              "a space is baked with no pixels and the code cell's advance");

    // More glyphs and cells than a run of the keys of a cache holds. Thus each order merges its runs, and the cache
    // keeps each glyph that the check asks for again. Then glyphs past three quarters of the cache, which empty it,
    // and a glyph after that retained
    static const GuiMask *s_drawn[52u * 38u];
    static const GuiGlyphCell *s_celled[128u * 20u];
    const unsigned int cached_before = glyphs.cached;
    unsigned int drawn = 0u;
    for (unsigned int size = 10u; size < 48u; size += 1u)
    {
        for (unsigned int letter = 0u; letter < 52u; letter += 1u)
        {
            const unsigned int asked = (letter < 26u) ? ('A' + letter) : ('a' + (letter - 26u));
            float drawn_advance = 0.0f;
            s_drawn[drawn] =
                EMBED_CALL(gui_glyphs_face_character.glyph, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs,
                           .face = GUI_FACE_CODE, .character = asked, .size = size, .advance = &drawn_advance);
            drawn += 1u;
        }
    }
    // the checks before drew some of these glyphs already
    const unsigned int cached_after = glyphs.cached;
    cli_check((cached_after >= drawn) && (cached_after <= (cached_before + drawn)),
              "each glyph drawn takes an entry of its own");
    int retained = 1;
    drawn = 0u;
    for (unsigned int size = 10u; size < 48u; size += 1u)
    {
        for (unsigned int letter = 0u; letter < 52u; letter += 1u)
        {
            const unsigned int asked = (letter < 26u) ? ('A' + letter) : ('a' + (letter - 26u));
            float drawn_advance = 0.0f;
            retained &=
                (s_drawn[drawn] != NULL) && (EMBED_CALL(gui_glyphs_face_character.glyph, GuiGlyphsFaceLetterCfg,
                                                        .glyphs = &glyphs, .face = GUI_FACE_CODE, .character = asked,
                                                        .size = size, .advance = &drawn_advance) == s_drawn[drawn]);
            drawn += 1u;
        }
    }
    cli_check(retained && (glyphs.cached == cached_after), "each glyph asked again is the one retained");
    const unsigned int celled_before = glyphs.celled;
    unsigned int celled = 0u;
    for (unsigned int color = 0u; color < 20u; color += 1u)
    {
        for (unsigned int letter = 0x100u; letter < 0x180u; letter += 1u)
        {
            float celled_advance = 0.0f;
            s_celled[celled] = EMBED_CALL(gui_glyphs_face_character.glyph_cell, GuiGlyphsFaceLetterCfg,
                                          .glyphs = &glyphs, .face = GUI_FACE_CODE, .character = letter, .size = 13u,
                                          .color = GUI_RGB(color * 12u, 0x40u, 0x80u), .advance = &celled_advance);
            celled += 1u;
        }
    }
    cli_check(glyphs.celled == (celled_before + celled), "each cell made takes a cell of its own");
    retained = 1;
    celled = 0u;
    for (unsigned int color = 0u; color < 20u; color += 1u)
    {
        for (unsigned int letter = 0x100u; letter < 0x180u; letter += 1u)
        {
            float celled_advance = 0.0f;
            retained &=
                (s_celled[celled] != NULL) && (EMBED_CALL(gui_glyphs_face_character.glyph_cell, GuiGlyphsFaceLetterCfg,
                                                          .glyphs = &glyphs, .face = GUI_FACE_CODE, .character = letter,
                                                          .size = 13u, .color = GUI_RGB(color * 12u, 0x40u, 0x80u),
                                                          .advance = &celled_advance) == s_celled[celled]);
            celled += 1u;
        }
    }
    cli_check(retained && (glyphs.celled == (celled_before + celled)), "each cell asked again is the one retained");
    for (unsigned int size = 48u; size < 81u; size += 1u)
    {
        for (unsigned int letter = 0u; letter < 52u; letter += 1u)
        {
            const unsigned int asked = (letter < 26u) ? ('A' + letter) : ('a' + (letter - 26u));
            float drawn_advance = 0.0f;
            (void)EMBED_CALL(gui_glyphs_face_character.glyph, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs,
                             .face = GUI_FACE_CODE, .character = asked, .size = size, .advance = &drawn_advance);
        }
    }
    cli_check((glyphs.cached * 4u) <= (GUI_GLYPH_CACHE_ENTRIES * 3u), "glyphs past three quarters empty the cache");
    float late_advance = 0.0f;
    const GuiMask *const late =
        EMBED_CALL(gui_glyphs_face_character.glyph, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs, .face = GUI_FACE_CODE,
                   .character = 'Q', .size = 20u, .advance = &late_advance);
    cli_check((late != NULL) &&
                  (EMBED_CALL(gui_glyphs_face_character.glyph, GuiGlyphsFaceLetterCfg, .glyphs = &glyphs,
                              .face = GUI_FACE_CODE, .character = 'Q', .size = 20u, .advance = &late_advance) == late),
              "a glyph drawn after the cache empties is retained");

    raster.pixels = NULL;
    raster.width = 0u;
    raster.height = 0u;
    EMBED_CALL(gui_glyphs.free, GuiGlyphsCfg, .glyphs = &glyphs);
    cli_check(glyphs.entry == NULL, "the glyphs given back retain no slots");
    EMBED_CALL(gui_block_size.free, GuiBlockSizeCfg);
    return cli_on_check();
}
