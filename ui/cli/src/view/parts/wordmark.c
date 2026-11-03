// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// wordmark.c: the name on the raster, the place, the times and the shape the page's, the work the raster's: orior and
// the eye made one mask at the name's em, the bold face read for that alone and given back; TABVLA's cuts made once as
// incised.js cuts them, each widest where the stylus goes in and narrowing as it lifts, into a picture of the cuts with
// the lip below them and the shade over both; and the funnel laid once as the name goes, a table of where each row of
// the window reads the name and how far it bends, so a frame of the flight is integers along the rows the name has ink
// on
#include "wordmark.h"

#include "../../command_line/cli_internal.h"
#include "../claim.h"

#include <math.h>

// the em's clamp(3rem, 7vw, 5.5rem) in the page's pixels and hundredths of the window's width
#define WORDMARK_EM_LEAST 48
#define WORDMARK_EM_MOST 88
#define WORDMARK_EM_SHARE 7

// in ems: the word's letter spacing; its line; the gap between the column's rows; the eye's box; TABVLA's height; how
// far TABVLA's middle stands below the eye's; the column's height, orior, the eye and TABVLA with their gaps
#define WORDMARK_SPACING (-0.04f)
#define WORDMARK_WORD_TALL 1.0f
#define WORDMARK_GAP 0.1f
#define WORDMARK_EYE_WIDE 1.6f
#define WORDMARK_EYE_TALL 1.58f
#define WORDMARK_UNDER_TALL 0.56f
#define WORDMARK_RISE 1.17f
#define WORDMARK_COLUMN 3.34f

// the eye's drawing in its own units, and its line there in the large name and in the bar
#define WORDMARK_EYE_BOX_WIDE 255.2f
#define WORDMARK_EYE_BOX_TALL 252.2f
#define WORDMARK_EYE_LINE 2.2f
#define WORDMARK_BAR_EYE_LINE 6.75f

// the eye's square in the bar in the page's pixels: its side, 1.164em by 1.475 at 17 pixels fitted to its box, and
// where it stands unclaimed: the bar's padding of 16 less the rule's 7 across; half of what the eye leaves of the bar's
// 39 inside, and the rule's 5.2 down, less the fit's share at its top
#define WORDMARK_BAR_EYE_SIDE 29.18f
#define WORDMARK_BAR_EYE_SIDE_WORDS 1912340LL
#define WORDMARK_BAR_EYE_LEFT_WORDS 589824LL
#define WORDMARK_BAR_EYE_TOP_WORDS 662569LL

// incised.js on a cap height of 10: a cut's half width where the stylus goes in and at its tail; the box round the word
// and how far it stands out from the capitals; and the most steps a cut is laid in, and points a stroke passes
#define WORDMARK_CUT_HEAD 1.0f
#define WORDMARK_CUT_TAIL 0.36f
#define WORDMARK_BOX_WIDE 64.2f
#define WORDMARK_BOX_TALL 12.6f
#define WORDMARK_BOX_EDGE 1.3f
#define WORDMARK_CUT_STEPS 40u
#define WORDMARK_CUT_POINTS 15u
#define WORDMARK_ARC_POINTS 13u
// the pixels a cut reaches past TABVLA's box, in the page's pixels: the shade's offset and three of its blur's widths;
// and the lip's alpha, --open-fg at 36%
#define WORDMARK_REACH 4
#define WORDMARK_LIP_ALPHA 0x5cu

// wordmark.js: the height a row saves at the tip and the share of a side that runs straight; and in the frame's words,
// the share of the flight the funnel forms over and the share past which the name starts up, .8 of that
#define WORDMARK_TIP_ROW 0.25f
#define WORDMARK_STRAIGHT 0.35f
#define WORDMARK_FORMS 22938
#define WORDMARK_STARTS 18350

// the word and its letters, and TABVLA's straight cuts as incised.js lays them, each from where the stylus goes in to
// where it lifts, on the word's own units: T, A, B's stem, V, L and A
static const char s_word[] = "orior";
#define WORDMARK_LETTERS 5u
static const float s_cuts[][4] = {
    {0.0f, 0.0f, 8.0f, 0.0f},    {4.0f, 0.0f, 4.0f, 10.0f},    {15.1f, 0.0f, 10.6f, 10.0f}, {15.1f, 0.0f, 19.6f, 10.0f},
    {12.8f, 6.6f, 17.4f, 6.6f},  {22.2f, 0.0f, 22.2f, 10.0f},  {31.9f, 0.0f, 36.4f, 10.0f}, {40.9f, 0.0f, 36.4f, 10.0f},
    {43.5f, 0.0f, 43.5f, 10.0f}, {43.5f, 10.0f, 50.0f, 10.0f}, {57.1f, 0.0f, 52.6f, 10.0f}, {57.1f, 0.0f, 61.6f, 10.0f},
    {54.8f, 6.6f, 59.4f, 6.6f},
};
// B's two bowls: the stem's x and where the bowl leaves it, the arc's center and radius, and where it comes back
static const float s_bowls[][6] = {
    {22.2f, 0.0f, 26.2f, 2.4f, 2.4f, 4.8f},
    {22.2f, 4.8f, 26.7f, 7.4f, 2.6f, 10.0f},
};

// the stages of the flight and of TABVLA's fade in, slide and fade out, one a frame's time, and TABVLA's opacity at
// each and its share of the way up: its curves read once into tables made once, ease-in-out as it fades in, ease-out as
// it slides, ease-in as it fades out
#define WORDMARK_FLIGHT_STAGES ((unsigned int)(GUI_WORDMARK_FLIGHT_MS / GUI_WORDMARK_FRAME_MS) + 1u)
#define WORDMARK_SHOW_STAGES ((unsigned int)(GUI_WORDMARK_FADE_IN_MS / GUI_WORDMARK_FRAME_MS) + 1u)
#define WORDMARK_SLIDE_STAGES ((unsigned int)(GUI_WORDMARK_SLIDE_MS / GUI_WORDMARK_FRAME_MS) + 1u)
#define WORDMARK_FADE_STAGES ((unsigned int)(GUI_WORDMARK_FADE_OUT_MS / GUI_WORDMARK_FRAME_MS) + 1u)
static unsigned char s_shows[WORDMARK_SHOW_STAGES];
static int s_slides[WORDMARK_SLIDE_STAGES];
static unsigned char s_fades[WORDMARK_FADE_STAGES];
static int s_curves_created;

static void wordmark_curves_init(void)
{
    if (s_curves_created)
    {
        return;
    }
    int curve[GUI_EASE_STEPS + 1u];
    EMBED_CALL(gui_autohide_table.ease_table_init, GuiAutohideTableCfg, .table = curve, .x1 = 0.42, .y1 = 0.0,
               .x2 = 0.58, .y2 = 1.0);
    for (unsigned int stage = 0u; stage < WORDMARK_SHOW_STAGES; stage += 1u)
    {
        // a share of a time under a second is a word, and a share of 255, rounded, is a byte
        const int part =
            (int)(((unsigned long long)stage * GUI_WORDMARK_FRAME_MS * GUI_FIXED_ONE) / GUI_WORDMARK_FADE_IN_MS);
        s_shows[stage] = (unsigned char)(((EMBED_CALL(gui_autohide_share.ease_table_read, GuiAutohideShareCfg,
                                                      .table_read = curve, .share = part) *
                                           255) +
                                          (GUI_FIXED_ONE / 2)) >>
                                         16);
    }
    EMBED_CALL(gui_autohide_table.ease_table_init, GuiAutohideTableCfg, .table = curve, .x1 = 0.0, .y1 = 0.0,
               .x2 = 0.58, .y2 = 1.0);
    for (unsigned int stage = 0u; stage < WORDMARK_SLIDE_STAGES; stage += 1u)
    {
        // a share of a time under a second is a word
        s_slides[stage] =
            EMBED_CALL(gui_autohide_share.ease_table_read, GuiAutohideShareCfg, .table_read = curve,
                       .share = (int)(((unsigned long long)stage * GUI_WORDMARK_FRAME_MS * GUI_FIXED_ONE) /
                                      GUI_WORDMARK_SLIDE_MS));
    }
    EMBED_CALL(gui_autohide_table.ease_table_init, GuiAutohideTableCfg, .table = curve, .x1 = 0.42, .y1 = 0.0,
               .x2 = 1.0, .y2 = 1.0);
    for (unsigned int stage = 0u; stage < WORDMARK_FADE_STAGES; stage += 1u)
    {
        // a share of a time under a second is a word, and a share of 255, rounded, is a byte
        const int part =
            (int)(((unsigned long long)stage * GUI_WORDMARK_FRAME_MS * GUI_FIXED_ONE) / GUI_WORDMARK_FADE_OUT_MS);
        s_fades[stage] =
            (unsigned char)((((GUI_FIXED_ONE - EMBED_CALL(gui_autohide_share.ease_table_read, GuiAutohideShareCfg,
                                                          .table_read = curve, .share = part)) *
                              255) +
                             (GUI_FIXED_ONE / 2)) >>
                            16);
    }
    s_curves_created = 1;
}

void gui_wordmark_init(const GuiWordmarkStillCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->wordmark, .val = 0, .bytes = sizeof(*args->wordmark)});
}

typedef struct
{
    GuiWordmark *wordmark;
    float pixels;
    const GuiFrame *frame;
    GuiGlyphs *glyphs;
    unsigned int em;
    float word_width;
    GuiWordmarkPosition *position_out;
    GuiMask *into;
    const GuiMask *from;
    int x;
    int y;
    GuiIcons *icons;
    const GuiWordmarkPosition *place;
    float *cover;
    unsigned int width;
    unsigned int height;
    const float *point;
    unsigned int count;
    float unit;
    float pad;
    unsigned char *cover_bytes;
    unsigned char *other;
    unsigned int passes;
    const GuiPalette *palette;
    int share;
    const GuiWordmark *wordmark_read;
    int footer;
    int *read_at;
    int *side_share;
    unsigned int raster_width;
    const int *read_at_read;
    const int *side_share_read;
    int part;
    GuiWordmarkSpan *span;
    unsigned char *cover_out;
    unsigned int *spans;
    unsigned int *covers;
    unsigned int stage;
    GuiRaster *raster;
    GuiColor color;
    unsigned long long through;
} WordmarkCtx;
// the flight's stages given back
EMBED_INLINE void wordmark_flight_free(const WordmarkCtx *args)
{
    g_cli_cells->persistent_buf_release(args->wordmark->flight_first, &g_gui_cells);
    args->wordmark->flight_first = NULL;
    args->wordmark->flight_span = NULL;
    args->wordmark->flight_cover = NULL;
}

// the name's mask and its funnel's row given back
EMBED_INLINE void wordmark_name_free(const WordmarkCtx *args)
{
    g_cli_cells->persistent_buf_release(args->wordmark->row_sum, &g_gui_cells);
    args->wordmark->row_sum = NULL;
    args->wordmark->text_color = NULL;
    args->wordmark->row = NULL;
    args->wordmark->name.cover = NULL;
}

// the name's and TABVLA's cells given back
EMBED_INLINE void wordmark_pictures_free(const WordmarkCtx *args)
{
    wordmark_name_free(args);
    EMBED_CALL(gui_raster_rect_color.free, GuiRasterRectColorCfg, .raster = &args->wordmark->under);
    args->wordmark->created_em = 0u;
    args->wordmark->laid_width = 0u;
}

void gui_wordmark_free(const GuiWordmarkStillCfg *args)
{
    EMBED_CALL(wordmark_flight_free, WordmarkCtx, .wordmark = args->wordmark);
    EMBED_CALL(wordmark_pictures_free, WordmarkCtx, .wordmark = args->wordmark);
}

void gui_wordmark_still(const GuiWordmarkStillCfg *args)
{
    int still = args->still;
    args->wordmark->still = still;
    if (!still)
    {
        return;
    }
    args->wordmark->fade_in_finished = 1;
    if (args->wordmark->exit_start_ms != 0ULL)
    {
        args->wordmark->exit_start_ms = 0ULL;
        args->wordmark->pours = 0;
        args->wordmark->home = 1;
        EMBED_CALL(wordmark_flight_free, WordmarkCtx, .wordmark = args->wordmark);
        EMBED_CALL(wordmark_pictures_free, WordmarkCtx, .wordmark = args->wordmark);
    }
}

void gui_wordmark_stand(const GuiWordmarkFrameCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    args->wordmark->exists = 1;
    args->wordmark->visible_start_ms = now_ms;
    args->wordmark->fade_in_finished = args->wordmark->still;
}

void gui_wordmark_land(const GuiWordmarkStillCfg *args)
{
    args->wordmark->home = 1;
}

void gui_wordmark_tree_read(const GuiWordmarkStillCfg *args)
{
    args->wordmark->tree_read = 1;
}

// a size in pixels rounded to a whole pixel; every place here is far inside what an int represents
EMBED_INLINE int wordmark_round(const WordmarkCtx *args)
{
    return (int)roundf(args->pixels);
}

// the name's em for the frame
EMBED_INLINE unsigned int wordmark_em(const WordmarkCtx *args)
{
    // the window's width and the scale are far inside what an int represents
    const int required = (WORDMARK_EM_SHARE * (int)args->frame->width) / 100;
    const int min =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = WORDMARK_EM_LEAST, .scale = args->frame->scale);
    const int most =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = WORDMARK_EM_MOST, .scale = args->frame->scale);
    // the em stands within its clamp, which is not negative
    const int bounded = (required > most) ? most : required;
    return (unsigned int)((required < min) ? min : bounded);
}

// the word's width as the page's box takes it, its letter spacing after every letter, in pixels at `em`
EMBED_INLINE float wordmark_word_width(const WordmarkCtx *args)
{
    GuiGlyphs *glyphs = args->glyphs;
    const float spacing = WORDMARK_SPACING * (float)args->em;
    float pixel_width = 0.0f;
    for (unsigned int at = 0u; at < WORDMARK_LETTERS; at += 1u)
    {
        float advance = 0.0f;
        // a letter of the word is a byte
        (void)EMBED_CALL(gui_glyphs_face_letter.glyph, GuiGlyphsFaceLetterCfg, .glyphs = glyphs,
                         .face = GUI_FACE_HEAD_BOLD, .letter = (unsigned int)(unsigned char)s_word[at],
                         .size = args->em, .advance = &advance);
        pixel_width += advance + spacing;
    }
    return pixel_width;
}

// the name's place on the frame's inside at `em`, the word `word_width` across
EMBED_INLINE void wordmark_position_get(const WordmarkCtx *args)
{
    unsigned int em = args->em;
    args->position_out->em = em;
    const float size = (float)em;
    const float logo_width = WORDMARK_EYE_WIDE * size;
    const float pixel_width = (args->word_width > logo_width) ? args->word_width : logo_width;
    const float middle = (float)args->frame->inside.x + ((float)args->frame->inside.width / 2.0f);
    const float top =
        (float)args->frame->inside.y + (((float)args->frame->inside.height - (WORDMARK_COLUMN * size)) / 2.0f);
    const float name_height = (WORDMARK_WORD_TALL + WORDMARK_GAP + WORDMARK_EYE_TALL) * size;
    args->position_out->name =
        (GuiRect){EMBED_CALL(wordmark_round, WordmarkCtx, .pixels = middle - (pixel_width / 2.0f)),
                  EMBED_CALL(wordmark_round, WordmarkCtx, .pixels = top),
                  EMBED_CALL(wordmark_round, WordmarkCtx, .pixels = pixel_width),
                  EMBED_CALL(wordmark_round, WordmarkCtx, .pixels = name_height)};
    const float under_height = WORDMARK_UNDER_TALL * size;
    const float under_width = under_height * WORDMARK_BOX_WIDE / WORDMARK_BOX_TALL;
    args->position_out->under =
        (GuiRect){EMBED_CALL(wordmark_round, WordmarkCtx, .pixels = middle - (under_width / 2.0f)),
                  args->position_out->name.y +
                      EMBED_CALL(wordmark_round, WordmarkCtx, .pixels = name_height + (WORDMARK_GAP * size)),
                  EMBED_CALL(wordmark_round, WordmarkCtx, .pixels = under_width),
                  EMBED_CALL(wordmark_round, WordmarkCtx, .pixels = under_height)};
    args->position_out->rise = -EMBED_CALL(wordmark_round, WordmarkCtx, .pixels = WORDMARK_RISE * size);
}

void gui_wordmark_position(const GuiWordmarkGlyphsCfg *args)
{
    GuiGlyphs *glyphs = args->glyphs;
    const int read = EMBED_CALL(cli_glyphs.head_bold_read, GuiGlyphsCfg, .glyphs = glyphs);
    const unsigned int em = EMBED_CALL(wordmark_em, WordmarkCtx, .frame = args->frame);
    EMBED_CALL(wordmark_position_get, WordmarkCtx, .frame = args->frame, .em = em,
               .word_width = EMBED_CALL(wordmark_word_width, WordmarkCtx, .glyphs = glyphs, .em = em),
               .position_out = args->place);
    if (read)
    {
        EMBED_CALL(cli_glyphs.head_bold_free, GuiGlyphsCfg, .glyphs = glyphs);
    }
}

// `from` laid into the name's mask with its top left at x, y, each pixel the more of what it has and what is laid
EMBED_INLINE void wordmark_layout_in(const WordmarkCtx *args)
{
    // the masks' sides are far inside what an int represents
    const GuiRect on = EMBED_CALL(gui_raster_one.rect_intersect, GuiRasterOneCfg,
                                  .one = (GuiRect){args->x, args->y, (int)args->from->width, (int)args->from->height},
                                  .other = (GuiRect){0, 0, (int)args->into->width, (int)args->into->height});
    for (int row = on.y; row < on.y + on.height; row += 1)
    {
        for (int column = on.x; column < on.x + on.width; column += 1)
        {
            // the place lies on both masks: neither place is negative
            const unsigned char share =
                args->from
                    ->cover[((unsigned int)(row - args->y) * args->from->width) + (unsigned int)(column - args->x)];
            unsigned char *const had =
                &args->into->cover[((unsigned int)row * args->into->width) + (unsigned int)column];
            *had = (share > *had) ? share : *had;
        }
    }
}

// orior and the eye made one mask at the place's em, the mask a little past the box where the word's last letter stands
// past its letter spacing, with the funnel's row, its sums and each row's ink in its one cell: 1, or 0 where the cell
// could not be taken
EMBED_INLINE int wordmark_name_init(const WordmarkCtx *args)
{
    GuiGlyphs *glyphs = args->glyphs;
    GuiIcons *icons = args->icons;
    const float size = (float)args->place->em;
    // the box's sides are not negative, and the spacing's pixels past the word's box are few
    const unsigned int end = (unsigned int)ceilf(-WORDMARK_SPACING * size);
    const unsigned int width = (unsigned int)args->place->name.width + end;
    const unsigned int height = (unsigned int)args->place->name.height;
    const unsigned long long pixels = (unsigned long long)width * height;
    const unsigned long long sums = (unsigned long long)width * sizeof(*args->wordmark->row_sum);
    const unsigned long long colors = (unsigned long long)height * sizeof(*args->wordmark->text_color);
    unsigned int *const cell = g_cli_cells->persistent_buf_alloc(sums + colors + width + pixels, &g_gui_cells);
    if (cell == NULL)
    {
        return 0;
    }
    args->wordmark->row_sum = cell;
    args->wordmark->text_color = (unsigned short (*)[2])(cell + width);
    args->wordmark->row = (unsigned char *)cell + sums + colors;
    args->wordmark->name = (GuiMask){width, height, 0, 0, args->wordmark->row + width};
    memor.set(&(MemoriaCfg){.dst = args->wordmark->name.cover, .val = 0, .bytes = pixels});
    // the word on the baseline its line sets it on, centered on the box
    const float ascent = EMBED_CALL(gui_glyphs_face.ascent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                    .face = GUI_FACE_HEAD_BOLD, .size = args->place->em);
    const float descent = EMBED_CALL(gui_glyphs_face.descent, GuiGlyphsFaceCfg, .glyphs_read = glyphs,
                                     .face = GUI_FACE_HEAD_BOLD, .size = args->place->em);
    const int baseline = EMBED_CALL(wordmark_round, WordmarkCtx,
                                    .pixels = ((WORDMARK_WORD_TALL * size - ascent - descent) / 2.0f) + ascent);
    float pen = ((float)args->place->name.width - args->word_width) / 2.0f;
    for (unsigned int at = 0u; at < WORDMARK_LETTERS; at += 1u)
    {
        float advance = 0.0f;
        // a letter of the word is a byte
        const GuiMask *const letter = EMBED_CALL(
            gui_glyphs_face_letter.glyph, GuiGlyphsFaceLetterCfg, .glyphs = glyphs, .face = GUI_FACE_HEAD_BOLD,
            .letter = (unsigned int)(unsigned char)s_word[at], .size = args->place->em, .advance = &advance);
        if ((letter != NULL) && (letter->cover != NULL))
        {
            EMBED_CALL(wordmark_layout_in, WordmarkCtx, .into = &args->wordmark->name, .from = letter,
                       .x = EMBED_CALL(wordmark_round, WordmarkCtx, .pixels = pen) + letter->left,
                       .y = baseline - letter->top);
        }
        pen += advance + (WORDMARK_SPACING * size);
    }
    // the eye fitted to its box and centered in the column, its drawing's own square laid about its box
    const float width_fit = (WORDMARK_EYE_WIDE * size) / WORDMARK_EYE_BOX_WIDE;
    const float height_fit = (WORDMARK_EYE_TALL * size) / WORDMARK_EYE_BOX_TALL;
    const float fit = (width_fit < height_fit) ? width_fit : height_fit;
    const float side = WORDMARK_EYE_BOX_WIDE * fit;
    // the square's side is a positive size in pixels
    const GuiMask *const logo =
        EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = icons, .name = "eye",
                   .size = (unsigned int)EMBED_CALL(wordmark_round, WordmarkCtx, .pixels = side),
                   .stroke = WORDMARK_EYE_LINE * fit);
    if ((logo != NULL) && (logo->cover != NULL))
    {
        const float over = (WORDMARK_EYE_BOX_WIDE - WORDMARK_EYE_BOX_TALL) * fit / 2.0f;
        EMBED_CALL(
            wordmark_layout_in, WordmarkCtx, .into = &args->wordmark->name, .from = logo,
            .x = EMBED_CALL(wordmark_round, WordmarkCtx, .pixels = ((float)args->place->name.width - side) / 2.0f),
            .y = EMBED_CALL(wordmark_round, WordmarkCtx,
                            .pixels = ((WORDMARK_WORD_TALL + WORDMARK_GAP) * size) +
                                      (((WORDMARK_EYE_TALL * size) - (WORDMARK_EYE_BOX_TALL * fit)) / 2.0f) - over));
    }
    // each row's ink, its first inked column and the column past its last: a frame's work is what lies between
    for (unsigned int row = 0u; row < height; row += 1u)
    {
        const unsigned char *const cover = &args->wordmark->name.cover[row * width];
        unsigned int first = 0u;
        unsigned int inner_end = width;
        while ((first < width) && (cover[first] == 0u))
        {
            first += 1u;
        }
        while ((inner_end > first) && (cover[inner_end - 1u] == 0u))
        {
            inner_end -= 1u;
        }
        // a column of the mask stands below 2^16
        args->wordmark->text_color[row][0] = (unsigned short)first;
        args->wordmark->text_color[row][1] = (unsigned short)inner_end;
    }
    return 1;
}

// the cut along `count` of `point`, x and y on the word's units, filled on `cover`: one side from head to tail and the
// other back, its half width falling from the head's to the tail's as the stylus lifts; `unit` the pixels of a unit
// and `pad` the pixels the word's box stands in from the cover's corner
static void wordmark_cut_fill(const WordmarkCtx *args)
{
    if (args->count < 2u)
    {
        return;
    }
    float *cover = args->cover;
    unsigned int width = args->width;
    unsigned int height = args->height;
    float reached[WORDMARK_CUT_POINTS];
    reached[0] = 0.0f;
    for (unsigned int at = 1u; at < args->count; at += 1u)
    {
        const float across = args->point[at * 2u] - args->point[(at - 1u) * 2u];
        const float down = args->point[(at * 2u) + 1u] - args->point[((at - 1u) * 2u) + 1u];
        reached[at] = reached[at - 1u] + sqrtf((across * across) + (down * down));
    }
    const float length = reached[args->count - 1u];
    // a length in units, doubled and rounded up, is a small count
    const unsigned int required = (unsigned int)ceilf(length * 2.0f);
    const unsigned int min = (required < 8u) ? 8u : required;
    const unsigned int steps = (min > WORDMARK_CUT_STEPS) ? WORDMARK_CUT_STEPS : min;
    const unsigned int corners = 2u * (steps + 1u);
    float corner[(WORDMARK_CUT_STEPS + 1u) * 4u];
    unsigned int segment = 0u;
    for (unsigned int step = 0u; step <= steps; step += 1u)
    {
        const float along = (length * (float)step) / (float)steps;
        while ((segment + 2u < args->count) && (reached[segment + 1u] < along))
        {
            segment += 1u;
        }
        const float from_x = args->point[segment * 2u];
        const float from_y = args->point[(segment * 2u) + 1u];
        const float to_x = args->point[(segment + 1u) * 2u];
        const float to_y = args->point[((segment + 1u) * 2u) + 1u];
        const float span = reached[segment + 1u] - reached[segment];
        const float share = (along - reached[segment]) / span;
        const float x = from_x + ((to_x - from_x) * share);
        const float y = from_y + ((to_y - from_y) * share);
        const float lifted = 1.0f - ((float)step / (float)steps);
        const float half = WORDMARK_CUT_TAIL + ((WORDMARK_CUT_HEAD - WORDMARK_CUT_TAIL) * lifted * lifted);
        const float normal_x = (-(to_y - from_y) / span) * half;
        const float normal_y = ((to_x - from_x) / span) * half;
        const unsigned int back = corners - 1u - step;
        corner[step * 2u] = ((x + normal_x + WORDMARK_BOX_EDGE) * args->unit) + args->pad;
        corner[(step * 2u) + 1u] = ((y + normal_y + WORDMARK_BOX_EDGE) * args->unit) + args->pad;
        corner[back * 2u] = ((x - normal_x + WORDMARK_BOX_EDGE) * args->unit) + args->pad;
        corner[(back * 2u) + 1u] = ((y - normal_y + WORDMARK_BOX_EDGE) * args->unit) + args->pad;
    }
    EMBED_CALL(gui_icons_cover.polygon_fill, GuiIconsCoverCfg, .cover = cover, .width = width, .height = height,
               .corner = corner, .corners = corners);
}

// every cut of TABVLA filled on `cover`
EMBED_INLINE void wordmark_word_cut(const WordmarkCtx *args)
{
    for (unsigned int at = 0u; at < sizeof(s_cuts) / sizeof(s_cuts[0]); at += 1u)
    {
        EMBED_CALL(wordmark_cut_fill, WordmarkCtx, .cover = args->cover, .width = args->width, .height = args->height,
                   .point = s_cuts[at], .count = 2u, .unit = args->unit, .pad = args->pad);
    }
    for (unsigned int at = 0u; at < sizeof(s_bowls) / sizeof(s_bowls[0]); at += 1u)
    {
        const float *const bowl = s_bowls[at];
        float point[WORDMARK_CUT_POINTS * 2u];
        point[0] = bowl[0];
        point[1] = bowl[1];
        // the arc from straight up round the right to straight down, on the page's twelve steps
        for (unsigned int step = 0u; step < WORDMARK_ARC_POINTS; step += 1u)
        {
            const float angle = (3.14159265f * (float)step) / (float)(WORDMARK_ARC_POINTS - 1u);
            point[(step + 1u) * 2u] = bowl[2] + (bowl[4] * sinf(angle));
            point[((step + 1u) * 2u) + 1u] = bowl[3] - (bowl[4] * cosf(angle));
        }
        point[(WORDMARK_ARC_POINTS + 1u) * 2u] = bowl[0];
        point[((WORDMARK_ARC_POINTS + 1u) * 2u) + 1u] = bowl[5];
        EMBED_CALL(wordmark_cut_fill, WordmarkCtx, .cover = args->cover, .width = args->width, .height = args->height,
                   .point = point, .count = WORDMARK_ARC_POINTS + 2u, .unit = args->unit, .pad = args->pad);
    }
}

// `cover` blurred by `passes` of a one, two, one kernel across and down, `other` a cover of the same size to work in:
// each pass adds half a pixel's square to the blur's variance
EMBED_INLINE void wordmark_blur(const WordmarkCtx *args)
{
    for (unsigned int pass = 0u; pass < args->passes; pass += 1u)
    {
        for (unsigned int y = 0u; y < args->height; y += 1u)
        {
            const unsigned char *const row = &args->cover_bytes[y * args->width];
            for (unsigned int x = 0u; x < args->width; x += 1u)
            {
                const unsigned int before = row[(x > 0u) ? x - 1u : x];
                const unsigned int after = row[(x + 1u < args->width) ? x + 1u : x];
                // the kernel's weights are four: a mean of bytes, rounded, is a byte
                args->other[(y * args->width) + x] = (unsigned char)((before + (2u * row[x]) + after + 2u) >> 2u);
            }
        }
        for (unsigned int y = 0u; y < args->height; y += 1u)
        {
            const unsigned char *const above = &args->other[((y > 0u) ? y - 1u : y) * args->width];
            const unsigned char *const row = &args->other[y * args->width];
            const unsigned char *const below = &args->other[((y + 1u < args->height) ? y + 1u : y) * args->width];
            for (unsigned int x = 0u; x < args->width; x += 1u)
            {
                args->cover_bytes[(y * args->width) + x] =
                    (unsigned char)((above[x] + (2u * row[x]) + below[x] + 2u) >> 2u);
            }
        }
    }
}

// TABVLA's picture made at the place's em: under all the shade, --lifted, of the cuts and their lip together, blurred a
// page's pixel and a page's pixel up; over it the lip, --open-fg at 36%, a page's pixel down; and the cuts over both:
// 1, or 0 where a cell could not be taken
EMBED_INLINE int wordmark_under_init(const WordmarkCtx *args)
{
    // the lip's drop, a page's pixel at the scale, at least one
    const int scaled_one =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = 1, .scale = args->frame->scale);
    const int bevel_drop = (scaled_one < 1) ? 1 : scaled_one;
    const int reach =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = WORDMARK_REACH, .scale = args->frame->scale);
    // the box's sides and the reach are not negative
    const unsigned int width = (unsigned int)(args->place->under.width + (2 * reach));
    const unsigned int height = (unsigned int)(args->place->under.height + (2 * reach));
    const unsigned long long pixels = (unsigned long long)width * height;
    float *const floats = g_cli_cells->temporary_buf_alloc((pixels * sizeof(*floats)) + (3ULL * pixels), &g_gui_cells);
    if ((floats == NULL) || !EMBED_CALL(gui_raster_width_x.init, GuiRasterWidthXCfg, .raster = &args->wordmark->under,
                                        .width = width, .height = height))
    {
        g_cli_cells->persistent_buf_release(floats, &g_gui_cells);
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = floats, .val = 0, .bytes = pixels * sizeof(*floats)});
    const float unit = (WORDMARK_UNDER_TALL * (float)args->place->em) / WORDMARK_BOX_TALL;
    EMBED_CALL(wordmark_word_cut, WordmarkCtx, .cover = floats, .width = width, .height = height, .unit = unit,
               .pad = (float)reach);
    unsigned char *const cuts = (unsigned char *)(floats + pixels);
    unsigned char *const shade = cuts + pixels;
    unsigned char *const other = shade + pixels;
    for (unsigned long long at = 0ULL; at < pixels; at += 1ULL)
    {
        // a coverage from 0 to 1 by 255, rounded, is a byte
        cuts[at] = (unsigned char)((floats[at] * 255.0f) + 0.5f);
    }
    // the drop is positive
    const unsigned int drop = (unsigned int)bevel_drop;
    for (unsigned int y = 0u; y < height; y += 1u)
    {
        for (unsigned int x = 0u; x < width; x += 1u)
        {
            const unsigned int cut = cuts[(y * width) + x];
            const unsigned int bevel =
                (y >= drop) ? ((cuts[((y - drop) * width) + x] * WORDMARK_LIP_ALPHA) + 127u) / 255u : 0u;
            // two alphas laid over each other are an alpha
            shade[(y * width) + x] = (unsigned char)(cut + (((bevel * (255u - cut)) + 127u) / 255u));
        }
    }
    EMBED_CALL(wordmark_blur, WordmarkCtx, .cover_bytes = shade, .other = other, .width = width, .height = height,
               .passes = 2u * drop * drop);
    const GuiMask cuts_mask = {width, height, 0, 0, cuts};
    const GuiMask shade_mask = {width, height, 0, 0, shade};
    EMBED_CALL(gui_raster_xy.stenciled_over, GuiRasterXYCfg, .picture = &args->wordmark->under,
               .mask_bits = &shade_mask, .x = 0, .y = -bevel_drop, .color = args->palette->lifted);
    EMBED_CALL(gui_raster_xy.stenciled_over, GuiRasterXYCfg, .picture = &args->wordmark->under, .mask_bits = &cuts_mask,
               .x = 0, .y = bevel_drop,
               .color = (args->palette->open_text & 0x00ffffffu) | ((GuiColor)WORDMARK_LIP_ALPHA << 24u));
    EMBED_CALL(gui_raster_xy.stenciled_over, GuiRasterXYCfg, .picture = &args->wordmark->under, .mask_bits = &cuts_mask,
               .x = 0, .y = 0, .color = args->palette->incised);
    g_cli_cells->persistent_buf_release(floats, &g_gui_cells);
    return 1;
}

// the name's and TABVLA's pictures made where the frame wants another em, the bold face read for that alone, and their
// places taken where the frame has moved: nothing is done for a frame like the last: 1, or 0 where they could not be
// made
static int wordmark_pictures_init(const WordmarkCtx *args)
{
    GuiGlyphs *glyphs = args->glyphs;
    const GuiRect inside = args->frame->inside;
    if ((args->wordmark->laid_width == args->frame->width) && (args->wordmark->laid_inside.x == inside.x) &&
        (args->wordmark->laid_inside.y == inside.y) && (args->wordmark->laid_inside.width == inside.width) &&
        (args->wordmark->laid_inside.height == inside.height))
    {
        return 1;
    }
    const unsigned int em = wordmark_em(args);
    GuiWordmarkPosition place;
    if (args->wordmark->created_em != em)
    {
        wordmark_pictures_free(args);
        const int read = EMBED_CALL(cli_glyphs.head_bold_read, GuiGlyphsCfg, .glyphs = glyphs);
        const float word_width = EMBED_CALL(wordmark_word_width, WordmarkCtx, .glyphs = glyphs, .em = em);
        EMBED_CALL(wordmark_position_get, WordmarkCtx, .frame = args->frame, .em = em, .word_width = word_width,
                   .position_out = &place);
        const int named = EMBED_CALL(wordmark_name_init, WordmarkCtx, .wordmark = args->wordmark, .glyphs = glyphs,
                                     .icons = args->icons, .place = &place, .word_width = word_width);
        if (read)
        {
            EMBED_CALL(cli_glyphs.head_bold_free, GuiGlyphsCfg, .glyphs = glyphs);
        }
        if (!named || !EMBED_CALL(wordmark_under_init, WordmarkCtx, .wordmark = args->wordmark,
                                  .palette = args->palette, .frame = args->frame, .place = &place))
        {
            wordmark_pictures_free(args);
            return 0;
        }
        args->wordmark->created_em = em;
        args->wordmark->word_width = word_width;
    }
    else
    {
        EMBED_CALL(wordmark_position_get, WordmarkCtx, .frame = args->frame, .em = em,
                   .word_width = args->wordmark->word_width, .position_out = &place);
    }
    // the mask's sides are far inside what an int represents
    args->wordmark->from =
        (GuiRect){place.name.x, place.name.y, (int)args->wordmark->name.width, (int)args->wordmark->name.height};
    args->wordmark->under_box = place.under;
    const int reach =
        EMBED_CALL(gui_frame_pixels.scale, GuiFramePixelsCfg, .pixels = WORDMARK_REACH, .scale = args->frame->scale);
    args->wordmark->under_x = place.under.x - reach;
    args->wordmark->under_y = place.under.y - reach;
    args->wordmark->rise = place.rise;
    args->wordmark->laid_width = args->frame->width;
    args->wordmark->laid_inside = inside;
    return 1;
}

// a share of the way, in the frame's words, eased to stand still at both ends, as the page's `smooth` does
EMBED_INLINE int wordmark_smooth(const WordmarkCtx *args)
{
    long long part;
    if (args->share < 0)
    {
        part = 0LL;
    }
    else if (args->share > GUI_FIXED_ONE)
    {
        part = (long long)GUI_FIXED_ONE;
    }
    else
    {
        part = (long long)args->share;
    }
    // the result stands between 0 and a whole
    return (int)((((part * part) >> 16) * ((3LL * GUI_FIXED_ONE) - (2LL * part))) >> 16);
}

// the funnel's tables for a name standing at `wordmark`'s place: where each row of the window reads the name and how
// far it bends, into `read_at` and `side_share`, each sized for the foot and two
EMBED_INLINE void wordmark_funnel_layout(const WordmarkCtx *args)
{
    const float high = (float)args->wordmark_read->from.height;
    const float reach = (float)args->footer;
    const float shortens = (((float)args->footer - high) > 1.0f) ? (float)args->footer - high : 1.0f;
    const float offset = shortens * (1.0f - WORDMARK_TIP_ROW) / 2.0f;
    // the foot is positive
    for (unsigned int row = 0u; row < (unsigned int)args->footer + 2u; row += 1u)
    {
        const float y = (float)row;
        float read = y;
        if (y < shortens - offset)
        {
            const float way =
                sqrtf((WORDMARK_TIP_ROW * WORDMARK_TIP_ROW) + ((2.0f * (1.0f - WORDMARK_TIP_ROW) * y) / shortens));
            read = ((way - WORDMARK_TIP_ROW) * shortens / (1.0f - WORDMARK_TIP_ROW)) - offset;
        }
        const float part = (y + 0.5f) / reach;
        const float clamped = (part > 1.0f) ? 1.0f : part;
        const float sided = (WORDMARK_STRAIGHT * clamped) +
                            ((1.0f - WORDMARK_STRAIGHT) * clamped * clamped * (3.0f - (2.0f * clamped)));
        // a place in rows with sixteen bits of fraction, and a share of a whole, each is a word
        args->read_at[row] = (y < shortens - offset) ? (int)(read * (float)GUI_FIXED_ONE) : (int)row * GUI_FIXED_ONE;
        args->side_share[row] = (int)((1.0f - sided) * (float)GUI_FIXED_ONE);
    }
}

void gui_wordmark_under_at(const GuiWordmarkReadCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    int *down = args->down;
    unsigned int *opacity = args->opacity;
    *down = 0;
    *opacity = 0u;
    if (args->wordmark_read->exists)
    {
        const unsigned long long through = now_ms - args->wordmark_read->visible_start_ms;
        if (args->wordmark_read->still || (through >= GUI_WORDMARK_FADE_IN_DELAY_MS + GUI_WORDMARK_FADE_IN_MS))
        {
            *opacity = 255u;
        }
        else if (through > GUI_WORDMARK_FADE_IN_DELAY_MS)
        {
            wordmark_curves_init();
            *opacity = s_shows[(through - GUI_WORDMARK_FADE_IN_DELAY_MS) / GUI_WORDMARK_FRAME_MS];
        }
        return;
    }
    if (args->wordmark_read->exit_start_ms == 0ULL)
    {
        return;
    }
    const unsigned long long through = now_ms - args->wordmark_read->exit_start_ms;
    if (through < GUI_WORDMARK_SLIDE_MS)
    {
        *down = args->wordmark_read->under_down[through / GUI_WORDMARK_FRAME_MS];
        *opacity = 255u;
        return;
    }
    *down = args->wordmark_read->rise;
    const unsigned long long fades = GUI_WORDMARK_SLIDE_MS + GUI_WORDMARK_FADE_OUT_DELAY_MS;
    if (through < fades)
    {
        *opacity = 255u;
    }
    else if (through < fades + GUI_WORDMARK_FADE_OUT_MS)
    {
        wordmark_curves_init();
        *opacity = s_fades[(through - fades) / GUI_WORDMARK_FRAME_MS];
    }
}

// the eye's whole side in the bar in pixels
EMBED_INLINE int wordmark_bar_logo_side(const WordmarkCtx *args)
{
    return EMBED_CALL(wordmark_round, WordmarkCtx,
                      .pixels = (WORDMARK_BAR_EYE_SIDE * (float)args->frame->scale) / (float)GUI_SCALE_ONE);
}

// every stage of the flight made once: 1, or 0 where a cell could not be taken or the funnel has no length
EMBED_INLINE int wordmark_flight_init(const WordmarkCtx *args);

int gui_wordmark_tick(const GuiWordmarkFrameCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    if (args->wordmark->exists)
    {
        const unsigned long long through = now_ms - args->wordmark->visible_start_ms;
        if (args->wordmark->tree_read && (through >= GUI_WORDMARK_MIN_VISIBLE_MS))
        {
            // the pane leaves: the view comes back under the name, and the name goes to the bar's foot under the eye;
            // with less motion asked for it is home at once
            args->wordmark->exists = 0;
            if (args->wordmark->still ||
                !EMBED_CALL(wordmark_pictures_init, WordmarkCtx, .wordmark = args->wordmark, .glyphs = args->glyphs,
                            .icons = args->icons, .palette = args->palette, .frame = args->frame))
            {
                args->wordmark->home = 1;
                EMBED_CALL(wordmark_pictures_free, WordmarkCtx, .wordmark = args->wordmark);
                return 1;
            }
            const GuiRect logo = gui_wordmark_frame.logo_square(args);
            args->wordmark->exit_start_ms = now_ms;
            args->wordmark->tip_y = args->frame->bar.y + args->frame->bar.height;
            args->wordmark->tip_left = logo.x;
            args->wordmark->tip_right = logo.x + EMBED_CALL(wordmark_bar_logo_side, WordmarkCtx, .frame = args->frame);
            wordmark_curves_init();
            for (unsigned int stage = 0u; stage < WORDMARK_SLIDE_STAGES; stage += 1u)
            {
                // a share of the way up, rounded, is far inside what a short represents
                const long long way = (long long)s_slides[stage] * (long long)args->wordmark->rise;
                args->wordmark->under_down[stage] =
                    (short)((way + ((way < 0LL) ? -(GUI_FIXED_ONE / 2) : (GUI_FIXED_ONE / 2))) / GUI_FIXED_ONE);
            }
            // the window's width is the frame's, the pixels the flight is cut to
            args->wordmark->pours = EMBED_CALL(wordmark_flight_init, WordmarkCtx, .wordmark = args->wordmark,
                                               .raster_width = args->frame->width);
            args->wordmark->home = !args->wordmark->pours;
            if (!args->wordmark->pours)
            {
                EMBED_CALL(wordmark_name_free, WordmarkCtx, .wordmark = args->wordmark);
            }
            return 1;
        }
        const int pending = !args->wordmark->fade_in_finished;
        args->wordmark->fade_in_finished = through >= GUI_WORDMARK_FADE_IN_DELAY_MS + GUI_WORDMARK_FADE_IN_MS;
        return pending;
    }
    if (args->wordmark->exit_start_ms == 0ULL)
    {
        return 0;
    }
    const unsigned long long through = now_ms - args->wordmark->exit_start_ms;
    if (args->wordmark->pours && (through >= GUI_WORDMARK_FLIGHT_MS))
    {
        args->wordmark->pours = 0;
        args->wordmark->home = 1;
        EMBED_CALL(wordmark_flight_free, WordmarkCtx, .wordmark = args->wordmark);
    }
    if (through >= GUI_WORDMARK_SLIDE_MS + GUI_WORDMARK_FADE_OUT_DELAY_MS + GUI_WORDMARK_FADE_OUT_MS)
    {
        args->wordmark->exit_start_ms = 0ULL;
        EMBED_CALL(wordmark_pictures_free, WordmarkCtx, .wordmark = args->wordmark);
    }
    return 1;
}

unsigned long long gui_wordmark_next_deadline_ms(const GuiWordmarkReadCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    if (args->wordmark_read->exists)
    {
        if (!args->wordmark_read->fade_in_finished)
        {
            return now_ms + GUI_WORDMARK_FRAME_MS;
        }
        if (!args->wordmark_read->tree_read)
        {
            return 0ULL;
        }
        return args->wordmark_read->visible_start_ms + GUI_WORDMARK_MIN_VISIBLE_MS;
    }
    return (args->wordmark_read->exit_start_ms != 0ULL) ? now_ms + GUI_WORDMARK_FRAME_MS : 0ULL;
}

// a stage of the flight at `part`, a share of it in the frame's words, walked: each row of the window from the tip down
// takes what the tables say of the name, one row of it where a row of the window is a row of the name, the rows that
// land on it by their shares where rows shorten, and only what has ink, stretched between the funnel's sides. With
// `span` NULL the spans and the coverage are counted alone; else each is written at `span` and `cover`. The spans and
// the bytes come back through `spans` and `covers`, added to what they hold
static void wordmark_stage_walk(const WordmarkCtx *args)
{
    unsigned int raster_width = args->raster_width;
    unsigned int *spans = args->spans;
    unsigned int *covers = args->covers;
    const long long forming = EMBED_CALL(wordmark_smooth, WordmarkCtx,
                                         .share = (int)(((long long)args->part * GUI_FIXED_ONE) / WORDMARK_FORMS));
    const long long rising =
        (args->part > WORDMARK_STARTS)
            ? (((long long)(args->part - WORDMARK_STARTS) * GUI_FIXED_ONE) / (GUI_FIXED_ONE - WORDMARK_STARTS))
            : 0LL;
    const int left = args->wordmark->from.x;
    const int right = args->wordmark->from.x + args->wordmark->from.width;
    const int high = args->wordmark->from.height;
    const int footer = args->wordmark->from.y + args->wordmark->from.height - args->wordmark->tip_y;
    // the name's top row's place at this moment, in rows with sixteen bits of fraction, is far inside what an int
    // represents
    const int top_row =
        ((footer - high) * GUI_FIXED_ONE) - (int)(((rising * rising) >> 16) * (long long)(footer + high));
    const int high_words = high * GUI_FIXED_ONE;
    const unsigned int width = args->wordmark->name.width;
    const unsigned short (*const text_color)[2] = (const unsigned short (*)[2])args->wordmark->text_color;
    for (int y = 0; y <= footer; y += 1)
    {
        const int first = args->read_at_read[y] - top_row;
        if (first >= high_words)
        {
            break;
        }
        const int last = args->read_at_read[y + 1] - top_row;
        if (last <= 0)
        {
            continue;
        }
        const unsigned char *cover = NULL;
        unsigned int from_x = 0u;
        unsigned int end_x = 0u;
        const int one_row = last - first <= GUI_FIXED_ONE;
        const int nearest = (first + (GUI_FIXED_ONE / 2)) >> 16;
        const int from_row = (first > 0) ? first >> 16 : 0;
        const int rows_end = (last + (GUI_FIXED_ONE - 1)) >> 16;
        const int to_row = (rows_end < high) ? rows_end : high;
        if (one_row)
        {
            // a row of the window is a row of the name: the one nearest, moved by whole rows
            if ((nearest < 0) || (nearest >= high))
            {
                continue;
            }
            from_x = text_color[nearest][0];
            end_x = text_color[nearest][1];
            // the row and column lie inside the mask
            cover = &args->wordmark->name.cover[((unsigned int)nearest * width) + from_x];
        }
        else
        {
            // the rows that land on it: the ink's extent across them
            from_x = width;
            for (int row = from_row; row < to_row; row += 1)
            {
                if (text_color[row][0] < text_color[row][1])
                {
                    from_x = (text_color[row][0] < from_x) ? text_color[row][0] : from_x;
                    end_x = (text_color[row][1] > end_x) ? text_color[row][1] : end_x;
                }
            }
        }
        if (from_x >= end_x)
        {
            continue;
        }
        if (!one_row && (args->span != NULL))
        {
            // each row by its share: their sums cleared across their ink alone
            memor.set(&(MemoriaCfg){.dst = args->wordmark->row_sum + from_x,
                                    .val = 0,
                                    .bytes = (unsigned long long)(end_x - from_x) * sizeof(*args->wordmark->row_sum)});
            const long long full_span = (long long)last - (long long)first;
            for (int row = from_row; row < to_row; row += 1)
            {
                const int row_start = row * GUI_FIXED_ONE;
                const int start = (first > row_start) ? first : row_start;
                const int stop = (last < row_start + GUI_FIXED_ONE) ? last : row_start + GUI_FIXED_ONE;
                // a share of a row by sixteen bits of fraction is a small word
                const unsigned int weight =
                    (unsigned int)((((long long)(stop - start) * GUI_FIXED_ONE) + (full_span / 2LL)) / full_span);
                const unsigned char *const row_cover = &args->wordmark->name.cover[(unsigned int)row * width];
                for (unsigned int x = text_color[row][0]; x < text_color[row][1]; x += 1u)
                {
                    args->wordmark->row_sum[x] += row_cover[x] * weight;
                }
            }
            for (unsigned int x = from_x; x < end_x; x += 1u)
            {
                const unsigned int mean = (args->wordmark->row_sum[x] + (GUI_FIXED_ONE / 2u)) >> 16u;
                // a mean of bytes, 255 at most, is a byte
                args->wordmark->row[x] = (unsigned char)((mean > 255u) ? 255u : mean);
            }
            cover = args->wordmark->row + from_x;
        }
        const long long bend = (forming * (long long)args->side_share_read[y]) >> 16;
        // the sides' places by sixteen bits of fraction are words: a window is far narrower than their range
        const long long start =
            ((long long)left * GUI_FIXED_ONE) + ((long long)(args->wordmark->tip_left - left) * bend);
        const long long end =
            ((long long)right * GUI_FIXED_ONE) + ((long long)(args->wordmark->tip_right - right) * bend);
        const long long across = end - start;
        const int row_left = (int)(start + ((across * from_x) / width));
        const int row_right = (int)(start + ((across * end_x) / width));
        int x = 0;
        // the row's place: the tip's row and how far down the funnel
        unsigned char *const written = (args->span != NULL) ? args->cover_out + *covers : NULL;
        const unsigned int count = EMBED_CALL(gui_raster_width.row_cover, GuiRasterWidthCfg, .cover = cover,
                                              .width = end_x - from_x, .left = row_left, .right = row_right,
                                              .raster_width = raster_width, .out = written, .x_pointer = &x);
        if (count == 0u)
        {
            continue;
        }
        if (args->span != NULL)
        {
            args->span[*spans] = (GuiWordmarkSpan){args->wordmark->tip_y + y, x, count, *covers};
        }
        *spans += 1u;
        *covers += count;
    }
}

// a stage's share of the flight in the frame's words
EMBED_INLINE int wordmark_stage_part(const WordmarkCtx *args)
{
    const unsigned long long through = (unsigned long long)args->stage * GUI_WORDMARK_FRAME_MS;
    // a share of a time under a second is a word
    return (through >= GUI_WORDMARK_FLIGHT_MS) ? GUI_FIXED_ONE
                                               : (int)((through * GUI_FIXED_ONE) / GUI_WORDMARK_FLIGHT_MS);
}

// every stage of the flight made once, sized in one walk and written in a second into one cell, from the funnel's
// tables, which last this call; the name's mask given back after them: 1, or 0 where a cell could not be taken or the
// funnel has no length
EMBED_INLINE int wordmark_flight_init(const WordmarkCtx *args)
{
    const int footer = args->wordmark->from.y + args->wordmark->from.height - args->wordmark->tip_y;
    if (footer < 1)
    {
        return 0;
    }
    // the foot is positive
    const unsigned int entries = (unsigned int)footer + 2u;
    int *const tables =
        g_cli_cells->temporary_buf_alloc((unsigned long long)entries * 2ULL * sizeof(*tables), &g_gui_cells);
    if (tables == NULL)
    {
        return 0;
    }
    int *const read_at = tables;
    int *const side_share = tables + entries;
    EMBED_CALL(wordmark_funnel_layout, WordmarkCtx, .wordmark_read = args->wordmark, .footer = footer,
               .read_at = read_at, .side_share = side_share);
    unsigned int spans = 0u;
    unsigned int covers = 0u;
    for (unsigned int stage = 0u; stage < WORDMARK_FLIGHT_STAGES; stage += 1u)
    {
        EMBED_CALL(wordmark_stage_walk, WordmarkCtx, .wordmark = args->wordmark, .read_at_read = read_at,
                   .side_share_read = side_share, .part = EMBED_CALL(wordmark_stage_part, WordmarkCtx, .stage = stage),
                   .raster_width = args->raster_width, .span = NULL, .cover_out = NULL, .spans = &spans,
                   .covers = &covers);
    }
    const unsigned long long first_bytes = ((unsigned long long)WORDMARK_FLIGHT_STAGES + 1ULL) * sizeof(unsigned int);
    const unsigned long long span_bytes = (unsigned long long)spans * sizeof(GuiWordmarkSpan);
    unsigned int *const cell =
        g_cli_cells->persistent_buf_alloc(first_bytes + span_bytes + covers + 1ULL, &g_gui_cells);
    if (cell == NULL)
    {
        g_cli_cells->persistent_buf_release(tables, &g_gui_cells);
        return 0;
    }
    args->wordmark->flight_first = cell;
    args->wordmark->flight_span = (GuiWordmarkSpan *)(cell + WORDMARK_FLIGHT_STAGES + 1u);
    args->wordmark->flight_cover = (unsigned char *)cell + first_bytes + span_bytes;
    spans = 0u;
    covers = 0u;
    for (unsigned int stage = 0u; stage < WORDMARK_FLIGHT_STAGES; stage += 1u)
    {
        args->wordmark->flight_first[stage] = spans;
        EMBED_CALL(wordmark_stage_walk, WordmarkCtx, .wordmark = args->wordmark, .read_at_read = read_at,
                   .side_share_read = side_share, .part = EMBED_CALL(wordmark_stage_part, WordmarkCtx, .stage = stage),
                   .raster_width = args->raster_width, .span = args->wordmark->flight_span,
                   .cover_out = args->wordmark->flight_cover, .spans = &spans, .covers = &covers);
    }
    args->wordmark->flight_first[WORDMARK_FLIGHT_STAGES] = spans;
    g_cli_cells->persistent_buf_release(tables, &g_gui_cells);
    wordmark_name_free(args);
    return 1;
}

// the stage of the flight standing `through` milliseconds on, blitted
EMBED_INLINE void wordmark_stage_layout(const WordmarkCtx *args)
{
    GuiRaster *raster = args->raster;
    GuiColor color = args->color;
    const unsigned long long reached = args->through / GUI_WORDMARK_FRAME_MS;
    // the stage is one of the stages, far fewer than an int counts
    const unsigned int stage =
        (reached >= WORDMARK_FLIGHT_STAGES) ? WORDMARK_FLIGHT_STAGES - 1u : (unsigned int)reached;
    for (unsigned int at = args->wordmark_read->flight_first[stage]; at < args->wordmark_read->flight_first[stage + 1u];
         at += 1u)
    {
        const GuiWordmarkSpan *const span = &args->wordmark_read->flight_span[at];
        EMBED_CALL(gui_raster_xy_color_count.span_layout, GuiRasterXYColorCountCfg, .raster = raster, .x = span->x,
                   .y = span->y, .cover = &args->wordmark_read->flight_cover[span->cover], .count = span->count,
                   .color = color);
    }
}

void gui_wordmark_draw(const GuiWordmarkFrameCfg *args)
{
    const unsigned long long now_ms = cli_clock_milliseconds();
    if (!args->wordmark->exists && (args->wordmark->exit_start_ms == 0ULL))
    {
        return;
    }
    if (args->wordmark->exists &&
        !EMBED_CALL(wordmark_pictures_init, WordmarkCtx, .wordmark = args->wordmark, .glyphs = args->glyphs,
                    .icons = args->icons, .palette = args->palette, .frame = args->frame))
    {
        return;
    }
    int down = 0;
    unsigned int opacity = 0u;
    EMBED_CALL(gui_wordmark_read.under_at, GuiWordmarkReadCfg, .wordmark_read = args->wordmark, .down = &down,
               .opacity = &opacity);
    EMBED_CALL(gui_raster_x.picture_layout, GuiRasterXCfg, .raster = args->raster,
               .picture_read = &args->wordmark->under, .x = args->wordmark->under_x,
               .y = args->wordmark->under_y + down, .opacity = opacity);
    if (args->wordmark->exists)
    {
        // the raster's sides are far inside what an int represents
        EMBED_CALL(gui_raster_xy_color.mask_bits, GuiRasterXYColorCfg, .raster = args->raster,
                   .mask_bits = &args->wordmark->name, .x = args->wordmark->from.x, .y = args->wordmark->from.y,
                   .color = args->palette->open_text,
                   .clip = (GuiRect){0, 0, (int)args->raster->width, (int)args->raster->height});
    }
    else if (args->wordmark->pours)
    {
        EMBED_CALL(wordmark_stage_layout, WordmarkCtx, .wordmark_read = args->wordmark, .raster = args->raster,
                   .color = args->palette->open_text, .through = now_ms - args->wordmark->exit_start_ms);
    }
}

GuiRect gui_wordmark_logo_square(const GuiWordmarkFrameCfg *args)
{
    const long long scale = args->frame->scale;
    const long long claim = args->frame->claim.claim;
    const long long inset_sum = (long long)args->frame->claim.inset_top + (long long)args->frame->claim.inset_left;
    // the share of the side saved, a number of GUI_FIXED_ONE: the claim takes it, the head's and the left's insets give
    // it back
    const long long shrunk = GUI_FIXED_ONE - ((claim * (GUI_FIXED_ONE - inset_sum)) / GUI_FIXED_ONE);
    long long saved;
    if (shrunk < 0LL)
    {
        saved = 0LL;
    }
    else if (shrunk > GUI_FIXED_ONE)
    {
        saved = GUI_FIXED_ONE;
    }
    else
    {
        saved = shrunk;
    }
    const long long gap = (long long)GUI_FRAME_GAP * claim;
    // the sizes in the page's pixels by sixteen bits of fraction, each rounded to a whole pixel, far inside what an int
    // represents
    const int side = (int)((((((WORDMARK_BAR_EYE_SIDE_WORDS * scale) / GUI_SCALE_ONE) * saved) / GUI_FIXED_ONE) +
                            (GUI_FIXED_ONE / 2)) /
                           GUI_FIXED_ONE);
    return (GuiRect){
        (int)(((((WORDMARK_BAR_EYE_LEFT_WORDS + gap) * scale) / GUI_SCALE_ONE) + (GUI_FIXED_ONE / 2)) / GUI_FIXED_ONE),
        (int)(((((WORDMARK_BAR_EYE_TOP_WORDS + gap) * scale) / GUI_SCALE_ONE) + (GUI_FIXED_ONE / 2)) / GUI_FIXED_ONE),
        side, side};
}

void gui_wordmark_logo_draw(const GuiWordmarkFrameCfg *args)
{
    GuiIcons *icons = args->icons;
    if (!args->wordmark_read->home)
    {
        return;
    }
    const GuiRect square = gui_wordmark_frame.logo_square(args);
    if (EMBED_CALL(gui_raster_rect.rect_empty, GuiRasterRectCfg, .rect = square))
    {
        return;
    }
    // the square's side is positive where it is not empty
    const unsigned int side = (unsigned int)square.width;
    const GuiMask *const logo = EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = icons, .name = "eye", .size = side,
                                           .stroke = (WORDMARK_BAR_EYE_LINE * (float)side) / WORDMARK_EYE_BOX_WIDE);
    if ((logo != NULL) && (logo->cover != NULL))
    {
        // the raster's sides are far inside what an int represents
        EMBED_CALL(gui_raster_xy_color.mask_bits, GuiRasterXYColorCfg, .raster = args->raster, .mask_bits = logo,
                   .x = square.x, .y = square.y, .color = args->palette->bar_mark,
                   .clip = (GuiRect){0, 0, (int)args->raster->width, (int)args->raster->height});
    }
}

int gui_wordmark_holds(const GuiWordmarkFrameCfg *args)
{
    if (args->wordmark_read->exists)
    {
        return EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->wordmark_read->from, .x = args->x,
                          .y = args->y) ||
               EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = args->wordmark_read->under_box,
                          .x = args->x, .y = args->y);
    }
    return args->wordmark_read->home &&
           EMBED_CALL(gui_raster_rect.rect_holds, GuiRasterRectCfg, .rect = gui_wordmark_frame.logo_square(args),
                      .x = args->x, .y = args->y);
}
