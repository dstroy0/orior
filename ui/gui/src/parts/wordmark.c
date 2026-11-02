// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// wordmark.c: the name on the raster, the place, the times and the shape the page's, the work the raster's: orior and
// the eye made one mask at the name's em, the bold face read for that alone and given back; TABVLA's cuts made once as
// incised.js cuts them, each widest where the stylus goes in and narrowing as it lifts, into a picture of the cuts with
// the lip below them and the shade over both; and the funnel laid once as the name goes, a table of where each row of
// the window reads the name and how far it bends, so a frame of the flight is integers along the rows the name has ink
// on
#include "wordmark.h"

#include "../../../cli/src/command_line/cli_internal.h"
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

// the eye's square in the bar in the page's pixels: its side, 1.164em by 1.475 at 17 pixels fitted to its box, and where
// it stands unclaimed: the bar's padding of 16 less the rule's 7 across; half of what the eye leaves of the bar's 39
// inside, and the rule's 5.2 down, less the fit's share at its top
#define WORDMARK_BAR_EYE_SIDE 29.18f
#define WORDMARK_BAR_EYE_SIDE_WORDS 1912340ll
#define WORDMARK_BAR_EYE_LEFT_WORDS 589824ll
#define WORDMARK_BAR_EYE_TOP_WORDS 662569ll

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

// wordmark.js: the height a row keeps at the tip and the share of a side that runs straight; and in the frame's words,
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
    {0.0f, 0.0f, 8.0f, 0.0f},      {4.0f, 0.0f, 4.0f, 10.0f},     {15.1f, 0.0f, 10.6f, 10.0f},   {15.1f, 0.0f, 19.6f, 10.0f},
    {12.8f, 6.6f, 17.4f, 6.6f},    {22.2f, 0.0f, 22.2f, 10.0f},   {31.9f, 0.0f, 36.4f, 10.0f},   {40.9f, 0.0f, 36.4f, 10.0f},
    {43.5f, 0.0f, 43.5f, 10.0f},   {43.5f, 10.0f, 50.0f, 10.0f},  {57.1f, 0.0f, 52.6f, 10.0f},   {57.1f, 0.0f, 61.6f, 10.0f},
    {54.8f, 6.6f, 59.4f, 6.6f},
};
// B's two bowls: the stem's x and where the bowl leaves it, the arc's center and radius, and where it comes back
static const float s_bowls[][6] = {
    {22.2f, 0.0f, 26.2f, 2.4f, 2.4f, 4.8f},
    {22.2f, 4.8f, 26.7f, 7.4f, 2.6f, 10.0f},
};

// the stages of the flight and of TABVLA's fade in, slide and fade out, one a frame's time, and TABVLA's opacity at each
// and its share of the way up: its curves read once into tables made once, ease-in-out as it fades in, ease-out as it
// slides, ease-in as it fades out
#define WORDMARK_FLIGHT_STAGES ((unsigned int)(GUI_WORDMARK_FLIES / GUI_WORDMARK_FRAME) + 1u)
#define WORDMARK_SHOW_STAGES ((unsigned int)(GUI_WORDMARK_SHOWS / GUI_WORDMARK_FRAME) + 1u)
#define WORDMARK_SLIDE_STAGES ((unsigned int)(GUI_WORDMARK_SLIDES / GUI_WORDMARK_FRAME) + 1u)
#define WORDMARK_FADE_STAGES ((unsigned int)(GUI_WORDMARK_FADES / GUI_WORDMARK_FRAME) + 1u)
static unsigned char s_shows[WORDMARK_SHOW_STAGES];
static int s_slides[WORDMARK_SLIDE_STAGES];
static unsigned char s_fades[WORDMARK_FADE_STAGES];
static int s_curves_made;

static void wordmark_curves_made(void)
{
    if (s_curves_made)
    {
        return;
    }
    int curve[GUI_EASE_STEPS + 1u];
    gui_ease_table_made(curve, 0.42, 0.0, 0.58, 1.0);
    for (unsigned int stage = 0u; stage < WORDMARK_SHOW_STAGES; stage += 1u)
    {
        // a share of a time under a second is a word, and a share of 255, rounded, is a byte
        const int part = (int)(((unsigned long long)stage * GUI_WORDMARK_FRAME * GUI_WHOLE) / GUI_WORDMARK_SHOWS);
        s_shows[stage] = (unsigned char)(((gui_ease_table_read(curve, part) * 255) + (GUI_WHOLE / 2)) >> 16);
    }
    gui_ease_table_made(curve, 0.0, 0.0, 0.58, 1.0);
    for (unsigned int stage = 0u; stage < WORDMARK_SLIDE_STAGES; stage += 1u)
    {
        // a share of a time under a second is a word
        s_slides[stage] = gui_ease_table_read(curve, (int)(((unsigned long long)stage * GUI_WORDMARK_FRAME * GUI_WHOLE) / GUI_WORDMARK_SLIDES));
    }
    gui_ease_table_made(curve, 0.42, 0.0, 1.0, 1.0);
    for (unsigned int stage = 0u; stage < WORDMARK_FADE_STAGES; stage += 1u)
    {
        // a share of a time under a second is a word, and a share of 255, rounded, is a byte
        const int part = (int)(((unsigned long long)stage * GUI_WORDMARK_FRAME * GUI_WHOLE) / GUI_WORDMARK_FADES);
        s_fades[stage] = (unsigned char)((((GUI_WHOLE - gui_ease_table_read(curve, part)) * 255) + (GUI_WHOLE / 2)) >> 16);
    }
    s_curves_made = 1;
}

void gui_wordmark_made(GuiWordmark *wordmark)
{
    memor.set(&(MemoriaCfg){.dst = wordmark, .val = 0, .bytes = sizeof(*wordmark)});
}

// the flight's stages given back
static void wordmark_flight_given(GuiWordmark *wordmark)
{
    g_cli_cells->persistent_buf_release(wordmark->flight_first, &g_gui_cells);
    wordmark->flight_first = NULL;
    wordmark->flight_span = NULL;
    wordmark->flight_cover = NULL;
}

// the name's mask and its funnel's row given back
static void wordmark_name_given(GuiWordmark *wordmark)
{
    g_cli_cells->persistent_buf_release(wordmark->row_sum, &g_gui_cells);
    wordmark->row_sum = NULL;
    wordmark->ink = NULL;
    wordmark->row = NULL;
    wordmark->name.cover = NULL;
}

// the name's and TABVLA's cells given back
static void wordmark_pictures_given(GuiWordmark *wordmark)
{
    wordmark_name_given(wordmark);
    gui_raster_given(&wordmark->under);
    wordmark->made_em = 0u;
    wordmark->laid_width = 0u;
}

void gui_wordmark_given(GuiWordmark *wordmark)
{
    wordmark_flight_given(wordmark);
    wordmark_pictures_given(wordmark);
}

void gui_wordmark_still(GuiWordmark *wordmark, int still)
{
    wordmark->still = still;
    if (!still)
    {
        return;
    }
    wordmark->under_whole = 1;
    if (wordmark->went_ms != 0ull)
    {
        wordmark->went_ms = 0ull;
        wordmark->pours = 0;
        wordmark->home = 1;
        wordmark_flight_given(wordmark);
        wordmark_pictures_given(wordmark);
    }
}

int gui_wordmark_session_read(const char *text, unsigned long long size, GuiWordmarkSession *session)
{
    static const char s_booted[] = "{\"booted\":";
    static const char s_logon[] = ",\"logon\":";
    session->known = 0;
    if ((text == NULL) || (size < sizeof(s_booted) + sizeof(s_logon)) ||
        (memor.cmp(&(MemoriaCfg){.src = text, .other = s_booted, .bytes = sizeof(s_booted) - 1u}) != 0))
    {
        return 0;
    }
    unsigned long long at = sizeof(s_booted) - 1u;
    unsigned long long booted = 0ull;
    while ((at < size) && (text[at] >= '0') && (text[at] <= '9'))
    {
        // a digit's value is a small count
        booted = (booted * 10ull) + (unsigned long long)(text[at] - '0');
        at += 1ull;
    }
    if ((at + sizeof(s_logon) - 1u > size) || (memor.cmp(&(MemoriaCfg){.src = text + at, .other = s_logon, .bytes = sizeof(s_logon) - 1u}) != 0))
    {
        return 0;
    }
    // the log in as it is written, down to one word: two of them differ where their words do
    unsigned long long logon = 0xcbf29ce484222325ull;
    for (at += sizeof(s_logon) - 1u; at < size; at += 1ull)
    {
        logon = (logon ^ (unsigned char)text[at]) * 0x100000001b3ull;
    }
    session->booted = booted;
    session->logon = logon;
    session->known = 1;
    return 1;
}

int gui_wordmark_first_in(const GuiWordmarkSession *kept, const GuiWordmarkSession *now)
{
    const unsigned long long apart = (kept->booted > now->booted) ? kept->booted - now->booted : now->booted - kept->booted;
    return !kept->known || !now->known || (kept->logon != now->logon) || (apart >= 60ull);
}

void gui_wordmark_stood(GuiWordmark *wordmark, unsigned long long now)
{
    wordmark->stands = 1;
    wordmark->stood_ms = now;
    wordmark->under_whole = wordmark->still;
}

void gui_wordmark_landed(GuiWordmark *wordmark)
{
    wordmark->home = 1;
}

void gui_wordmark_tree_read(GuiWordmark *wordmark)
{
    wordmark->tree_read = 1;
}

// a size in pixels rounded to a whole pixel; every place here is far inside what an int represents
static int wordmark_round(float pixels)
{
    return (int)roundf(pixels);
}

// the name's em for the frame
static unsigned int wordmark_em(const GuiFrame *frame)
{
    // the window's width and the scale are far inside what an int represents
    const int wanted = (WORDMARK_EM_SHARE * (int)frame->width) / 100;
    const int least = WORDMARK_EM_LEAST * frame->scale;
    const int most = WORDMARK_EM_MOST * frame->scale;
    // the em stands within its clamp, which is not negative
    return (unsigned int)((wanted < least) ? least : ((wanted > most) ? most : wanted));
}

// the word's width as the page's box takes it, its letter spacing after every letter, in pixels at `em`
static float wordmark_word_wide(GuiGlyphs *glyphs, unsigned int em)
{
    const float spacing = WORDMARK_SPACING * (float)em;
    float wide = 0.0f;
    for (unsigned int at = 0u; at < WORDMARK_LETTERS; at += 1u)
    {
        float advance = 0.0f;
        // a letter of the word is a byte
        (void)gui_glyph(glyphs, GUI_FACE_HEAD_BOLD, (unsigned int)(unsigned char)s_word[at], em, &advance);
        wide += advance + spacing;
    }
    return wide;
}

// the name's place on the frame's inside at `em`, the word `word_wide` across
static void wordmark_place_of(const GuiFrame *frame, unsigned int em, float word_wide, GuiWordmarkPlace *place)
{
    place->em = em;
    const float size = (float)em;
    const float eye_wide = WORDMARK_EYE_WIDE * size;
    const float wide = (word_wide > eye_wide) ? word_wide : eye_wide;
    const float middle = (float)frame->inside.x + ((float)frame->inside.width / 2.0f);
    const float top = (float)frame->inside.y + (((float)frame->inside.height - (WORDMARK_COLUMN * size)) / 2.0f);
    const float name_tall = (WORDMARK_WORD_TALL + WORDMARK_GAP + WORDMARK_EYE_TALL) * size;
    place->name = (GuiRect){wordmark_round(middle - (wide / 2.0f)), wordmark_round(top), wordmark_round(wide), wordmark_round(name_tall)};
    const float under_tall = WORDMARK_UNDER_TALL * size;
    const float under_wide = under_tall * WORDMARK_BOX_WIDE / WORDMARK_BOX_TALL;
    place->under = (GuiRect){wordmark_round(middle - (under_wide / 2.0f)), place->name.y + wordmark_round(name_tall + (WORDMARK_GAP * size)),
                             wordmark_round(under_wide), wordmark_round(under_tall)};
    place->rise = -wordmark_round(WORDMARK_RISE * size);
}

void gui_wordmark_placed(GuiGlyphs *glyphs, const GuiFrame *frame, GuiWordmarkPlace *place)
{
    const int read = gui_glyphs_head_bold_read(glyphs);
    const unsigned int em = wordmark_em(frame);
    wordmark_place_of(frame, em, wordmark_word_wide(glyphs, em), place);
    if (read)
    {
        gui_glyphs_head_bold_given(glyphs);
    }
}

// `from` laid into the name's mask with its top left at x, y, each pixel the more of what it has and what is laid
static void wordmark_laid_in(GuiMask *into, const GuiMask *from, int x, int y)
{
    // the masks' sides are far inside what an int represents
    const GuiRect on = gui_rect_met((GuiRect){x, y, (int)from->width, (int)from->height}, (GuiRect){0, 0, (int)into->width, (int)into->height});
    for (int row = on.y; row < on.y + on.height; row += 1)
    {
        for (int column = on.x; column < on.x + on.width; column += 1)
        {
            // the place lies on both masks: neither place is negative
            const unsigned char share = from->cover[((unsigned int)(row - y) * from->width) + (unsigned int)(column - x)];
            unsigned char *const had = &into->cover[((unsigned int)row * into->width) + (unsigned int)column];
            *had = (share > *had) ? share : *had;
        }
    }
}

// orior and the eye made one mask at the place's em, the mask a little past the box where the word's last letter stands
// past its letter spacing, with the funnel's row, its sums and each row's ink in its one cell: 1, or 0 where the cell
// could not be taken
static int wordmark_name_made(GuiWordmark *wordmark, GuiGlyphs *glyphs, GuiIcons *icons, const GuiWordmarkPlace *place, float word_wide)
{
    const float size = (float)place->em;
    // the box's sides are not negative, and the spacing's pixels past the word's box are few
    const unsigned int past = (unsigned int)ceilf(-WORDMARK_SPACING * size);
    const unsigned int width = (unsigned int)place->name.width + past;
    const unsigned int height = (unsigned int)place->name.height;
    const unsigned long long pixels = (unsigned long long)width * height;
    const unsigned long long sums = (unsigned long long)width * sizeof(*wordmark->row_sum);
    const unsigned long long inks = (unsigned long long)height * sizeof(*wordmark->ink);
    unsigned int *const cell = g_cli_cells->persistent_buf_alloc(sums + inks + width + pixels, &g_gui_cells);
    if (cell == NULL)
    {
        return 0;
    }
    wordmark->row_sum = cell;
    wordmark->ink = (unsigned short (*)[2])(cell + width);
    wordmark->row = (unsigned char *)cell + sums + inks;
    wordmark->name = (GuiMask){width, height, 0, 0, wordmark->row + width};
    memor.set(&(MemoriaCfg){.dst = wordmark->name.cover, .val = 0, .bytes = pixels});
    // the word on the baseline its line sets it on, centered on the box
    const float ascent = gui_glyphs_ascent(glyphs, GUI_FACE_HEAD_BOLD, place->em);
    const float descent = gui_glyphs_descent(glyphs, GUI_FACE_HEAD_BOLD, place->em);
    const int baseline = wordmark_round(((WORDMARK_WORD_TALL * size - ascent - descent) / 2.0f) + ascent);
    float pen = ((float)place->name.width - word_wide) / 2.0f;
    for (unsigned int at = 0u; at < WORDMARK_LETTERS; at += 1u)
    {
        float advance = 0.0f;
        // a letter of the word is a byte
        const GuiMask *const letter = gui_glyph(glyphs, GUI_FACE_HEAD_BOLD, (unsigned int)(unsigned char)s_word[at], place->em, &advance);
        if ((letter != NULL) && (letter->cover != NULL))
        {
            wordmark_laid_in(&wordmark->name, letter, wordmark_round(pen) + letter->left, baseline - letter->top);
        }
        pen += advance + (WORDMARK_SPACING * size);
    }
    // the eye fitted to its box and centered in the column, its drawing's own square laid about its box
    const float wide_fit = (WORDMARK_EYE_WIDE * size) / WORDMARK_EYE_BOX_WIDE;
    const float tall_fit = (WORDMARK_EYE_TALL * size) / WORDMARK_EYE_BOX_TALL;
    const float fit = (wide_fit < tall_fit) ? wide_fit : tall_fit;
    const float side = WORDMARK_EYE_BOX_WIDE * fit;
    // the square's side is a positive size in pixels
    const GuiMask *const eye = gui_icon(icons, "eye", (unsigned int)wordmark_round(side), WORDMARK_EYE_LINE * fit);
    if ((eye != NULL) && (eye->cover != NULL))
    {
        const float over = (WORDMARK_EYE_BOX_WIDE - WORDMARK_EYE_BOX_TALL) * fit / 2.0f;
        wordmark_laid_in(&wordmark->name, eye, wordmark_round(((float)place->name.width - side) / 2.0f),
                         wordmark_round(((WORDMARK_WORD_TALL + WORDMARK_GAP) * size) + (((WORDMARK_EYE_TALL * size) - (WORDMARK_EYE_BOX_TALL * fit)) / 2.0f) - over));
    }
    // each row's ink, its first inked column and the column past its last: a frame's work is what lies between
    for (unsigned int row = 0u; row < height; row += 1u)
    {
        const unsigned char *const cover = &wordmark->name.cover[row * width];
        unsigned int first = 0u;
        unsigned int end = width;
        while ((first < width) && (cover[first] == 0u))
        {
            first += 1u;
        }
        while ((end > first) && (cover[end - 1u] == 0u))
        {
            end -= 1u;
        }
        // a column of the mask stands below 2^16
        wordmark->ink[row][0] = (unsigned short)first;
        wordmark->ink[row][1] = (unsigned short)end;
    }
    return 1;
}

// the cut along `count` of `point`, x and y on the word's units, filled on `cover`: one side from head to tail and the
// other back, its half width falling from the head's to the tail's as the stylus lifts; `unit` the pixels of a unit
// and `pad` the pixels the word's box stands in from the cover's corner
static void wordmark_cut_filled(float *cover, unsigned int width, unsigned int height, const float *point, unsigned int count,
                                float unit, float pad)
{
    float reached[WORDMARK_CUT_POINTS];
    reached[0] = 0.0f;
    for (unsigned int at = 1u; at < count; at += 1u)
    {
        const float across = point[at * 2u] - point[(at - 1u) * 2u];
        const float down = point[(at * 2u) + 1u] - point[((at - 1u) * 2u) + 1u];
        reached[at] = reached[at - 1u] + sqrtf((across * across) + (down * down));
    }
    const float length = reached[count - 1u];
    // a length in units, doubled and rounded up, is a small count
    const unsigned int wanted = (unsigned int)ceilf(length * 2.0f);
    const unsigned int least = (wanted < 8u) ? 8u : wanted;
    const unsigned int steps = (least > WORDMARK_CUT_STEPS) ? WORDMARK_CUT_STEPS : least;
    const unsigned int corners = 2u * (steps + 1u);
    float corner[(WORDMARK_CUT_STEPS + 1u) * 4u];
    unsigned int segment = 0u;
    for (unsigned int step = 0u; step <= steps; step += 1u)
    {
        const float along = (length * (float)step) / (float)steps;
        while ((segment + 2u < count) && (reached[segment + 1u] < along))
        {
            segment += 1u;
        }
        const float from_x = point[segment * 2u];
        const float from_y = point[(segment * 2u) + 1u];
        const float to_x = point[(segment + 1u) * 2u];
        const float to_y = point[((segment + 1u) * 2u) + 1u];
        const float span = reached[segment + 1u] - reached[segment];
        const float share = (along - reached[segment]) / span;
        const float x = from_x + ((to_x - from_x) * share);
        const float y = from_y + ((to_y - from_y) * share);
        const float lifted = 1.0f - ((float)step / (float)steps);
        const float half = WORDMARK_CUT_TAIL + ((WORDMARK_CUT_HEAD - WORDMARK_CUT_TAIL) * lifted * lifted);
        const float normal_x = (-(to_y - from_y) / span) * half;
        const float normal_y = ((to_x - from_x) / span) * half;
        const unsigned int back = corners - 1u - step;
        corner[step * 2u] = ((x + normal_x + WORDMARK_BOX_EDGE) * unit) + pad;
        corner[(step * 2u) + 1u] = ((y + normal_y + WORDMARK_BOX_EDGE) * unit) + pad;
        corner[back * 2u] = ((x - normal_x + WORDMARK_BOX_EDGE) * unit) + pad;
        corner[(back * 2u) + 1u] = ((y - normal_y + WORDMARK_BOX_EDGE) * unit) + pad;
    }
    gui_icons_polygon_filled(cover, width, height, corner, corners);
}

// every cut of TABVLA filled on `cover`
static void wordmark_word_cut(float *cover, unsigned int width, unsigned int height, float unit, float pad)
{
    for (unsigned int at = 0u; at < sizeof(s_cuts) / sizeof(s_cuts[0]); at += 1u)
    {
        wordmark_cut_filled(cover, width, height, s_cuts[at], 2u, unit, pad);
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
        wordmark_cut_filled(cover, width, height, point, WORDMARK_ARC_POINTS + 2u, unit, pad);
    }
}

// `cover` blurred by `passes` of a one, two, one kernel across and down, `other` a cover of the same size to work in:
// each pass adds half a pixel's square to the blur's variance
static void wordmark_blurred(unsigned char *cover, unsigned char *other, unsigned int width, unsigned int height, unsigned int passes)
{
    for (unsigned int pass = 0u; pass < passes; pass += 1u)
    {
        for (unsigned int y = 0u; y < height; y += 1u)
        {
            const unsigned char *const row = &cover[y * width];
            for (unsigned int x = 0u; x < width; x += 1u)
            {
                const unsigned int before = row[(x > 0u) ? x - 1u : x];
                const unsigned int after = row[(x + 1u < width) ? x + 1u : x];
                // the kernel's weights are four: a mean of bytes, rounded, is a byte
                other[(y * width) + x] = (unsigned char)((before + (2u * row[x]) + after + 2u) >> 2u);
            }
        }
        for (unsigned int y = 0u; y < height; y += 1u)
        {
            const unsigned char *const above = &other[((y > 0u) ? y - 1u : y) * width];
            const unsigned char *const row = &other[y * width];
            const unsigned char *const below = &other[((y + 1u < height) ? y + 1u : y) * width];
            for (unsigned int x = 0u; x < width; x += 1u)
            {
                cover[(y * width) + x] = (unsigned char)((above[x] + (2u * row[x]) + below[x] + 2u) >> 2u);
            }
        }
    }
}

// TABVLA's picture made at the place's em: under all the shade, --lifted, of the cuts and their lip together, blurred a
// page's pixel and a page's pixel up; over it the lip, --open-fg at 36%, a page's pixel down; and the cuts over both:
// 1, or 0 where a cell could not be taken
static int wordmark_under_made(GuiWordmark *wordmark, const GuiPalette *palette, const GuiFrame *frame, const GuiWordmarkPlace *place)
{
    const int scale = frame->scale;
    const int reach = WORDMARK_REACH * scale;
    // the box's sides and the reach are not negative
    const unsigned int width = (unsigned int)(place->under.width + (2 * reach));
    const unsigned int height = (unsigned int)(place->under.height + (2 * reach));
    const unsigned long long pixels = (unsigned long long)width * height;
    float *const floats = g_cli_cells->temporary_buf_alloc((pixels * sizeof(*floats)) + (3ull * pixels), &g_gui_cells);
    if ((floats == NULL) || !gui_raster_made(&wordmark->under, width, height))
    {
        g_cli_cells->persistent_buf_release(floats, &g_gui_cells);
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = floats, .val = 0, .bytes = pixels * sizeof(*floats)});
    const float unit = (WORDMARK_UNDER_TALL * (float)place->em) / WORDMARK_BOX_TALL;
    wordmark_word_cut(floats, width, height, unit, (float)reach);
    unsigned char *const cuts = (unsigned char *)(floats + pixels);
    unsigned char *const shade = cuts + pixels;
    unsigned char *const other = shade + pixels;
    for (unsigned long long at = 0ull; at < pixels; at += 1ull)
    {
        // a coverage from 0 to 1 by 255, rounded, is a byte
        cuts[at] = (unsigned char)((floats[at] * 255.0f) + 0.5f);
    }
    // the scale is positive
    const unsigned int drop = (unsigned int)scale;
    for (unsigned int y = 0u; y < height; y += 1u)
    {
        for (unsigned int x = 0u; x < width; x += 1u)
        {
            const unsigned int cut = cuts[(y * width) + x];
            const unsigned int lip = (y >= drop) ? ((cuts[((y - drop) * width) + x] * WORDMARK_LIP_ALPHA) + 127u) / 255u : 0u;
            // two alphas laid over each other are an alpha
            shade[(y * width) + x] = (unsigned char)(cut + (((lip * (255u - cut)) + 127u) / 255u));
        }
    }
    wordmark_blurred(shade, other, width, height, 2u * drop * drop);
    const GuiMask cuts_mask = {width, height, 0, 0, cuts};
    const GuiMask shade_mask = {width, height, 0, 0, shade};
    gui_raster_stenciled_over(&wordmark->under, &shade_mask, 0, -scale, palette->lifted);
    gui_raster_stenciled_over(&wordmark->under, &cuts_mask, 0, scale,
                              (palette->open_text & 0x00ffffffu) | ((GuiColor)WORDMARK_LIP_ALPHA << 24u));
    gui_raster_stenciled_over(&wordmark->under, &cuts_mask, 0, 0, palette->incised);
    g_cli_cells->persistent_buf_release(floats, &g_gui_cells);
    return 1;
}

// the name's and TABVLA's pictures made where the frame wants another em, the bold face read for that alone, and their
// places taken where the frame has moved: nothing is done for a frame like the last: 1, or 0 where they could not be made
static int wordmark_pictures_made(GuiWordmark *wordmark, GuiGlyphs *glyphs, GuiIcons *icons, const GuiPalette *palette, const GuiFrame *frame)
{
    const GuiRect inside = frame->inside;
    if ((wordmark->laid_width == frame->width) && (wordmark->laid_inside.x == inside.x) && (wordmark->laid_inside.y == inside.y) &&
        (wordmark->laid_inside.width == inside.width) && (wordmark->laid_inside.height == inside.height))
    {
        return 1;
    }
    const unsigned int em = wordmark_em(frame);
    GuiWordmarkPlace place;
    if (wordmark->made_em != em)
    {
        wordmark_pictures_given(wordmark);
        const int read = gui_glyphs_head_bold_read(glyphs);
        const float word_wide = wordmark_word_wide(glyphs, em);
        wordmark_place_of(frame, em, word_wide, &place);
        const int named = wordmark_name_made(wordmark, glyphs, icons, &place, word_wide);
        if (read)
        {
            gui_glyphs_head_bold_given(glyphs);
        }
        if (!named || !wordmark_under_made(wordmark, palette, frame, &place))
        {
            wordmark_pictures_given(wordmark);
            return 0;
        }
        wordmark->made_em = em;
        wordmark->word_wide = word_wide;
    }
    else
    {
        wordmark_place_of(frame, em, wordmark->word_wide, &place);
    }
    // the mask's sides are far inside what an int represents
    wordmark->from = (GuiRect){place.name.x, place.name.y, (int)wordmark->name.width, (int)wordmark->name.height};
    wordmark->under_box = place.under;
    const int reach = WORDMARK_REACH * frame->scale;
    wordmark->under_x = place.under.x - reach;
    wordmark->under_y = place.under.y - reach;
    wordmark->rise = place.rise;
    wordmark->laid_width = frame->width;
    wordmark->laid_inside = inside;
    return 1;
}

// a share of the way, in the frame's words, eased to stand still at both ends, as the page's `smooth` does
static int wordmark_smooth(int share)
{
    const long long part = (share < 0) ? 0ll : ((share > GUI_WHOLE) ? (long long)GUI_WHOLE : (long long)share);
    // the result stands between 0 and a whole
    return (int)((((part * part) >> 16) * ((3ll * GUI_WHOLE) - (2ll * part))) >> 16);
}

// the funnel's tables for a name standing at `wordmark`'s place: where each row of the window reads the name and how far
// it bends, into `read_at` and `side_share`, each sized for the foot and two
static void wordmark_funnel_laid(const GuiWordmark *wordmark, int foot, int *read_at, int *side_share)
{
    const float high = (float)wordmark->from.height;
    const float reach = (float)foot;
    const float shortens = (((float)foot - high) > 1.0f) ? (float)foot - high : 1.0f;
    const float offset = shortens * (1.0f - WORDMARK_TIP_ROW) / 2.0f;
    // the foot is positive
    for (unsigned int row = 0u; row < (unsigned int)foot + 2u; row += 1u)
    {
        const float y = (float)row;
        float read = y;
        if (y < shortens - offset)
        {
            const float way = sqrtf((WORDMARK_TIP_ROW * WORDMARK_TIP_ROW) + ((2.0f * (1.0f - WORDMARK_TIP_ROW) * y) / shortens));
            read = ((way - WORDMARK_TIP_ROW) * shortens / (1.0f - WORDMARK_TIP_ROW)) - offset;
        }
        const float part = (y + 0.5f) / reach;
        const float clamped = (part > 1.0f) ? 1.0f : part;
        const float sided = (WORDMARK_STRAIGHT * clamped) + ((1.0f - WORDMARK_STRAIGHT) * clamped * clamped * (3.0f - (2.0f * clamped)));
        // a place in rows with sixteen bits of fraction, and a share of a whole, each is a word
        read_at[row] = (y < shortens - offset) ? (int)(read * (float)GUI_WHOLE) : (int)row * GUI_WHOLE;
        side_share[row] = (int)((1.0f - sided) * (float)GUI_WHOLE);
    }
}

void gui_wordmark_under_at(const GuiWordmark *wordmark, unsigned long long now, int *down, unsigned int *opacity)
{
    *down = 0;
    *opacity = 0u;
    if (wordmark->stands)
    {
        const unsigned long long through = now - wordmark->stood_ms;
        if (wordmark->still || (through >= GUI_WORDMARK_SHOWS_AFTER + GUI_WORDMARK_SHOWS))
        {
            *opacity = 255u;
        }
        else if (through > GUI_WORDMARK_SHOWS_AFTER)
        {
            wordmark_curves_made();
            *opacity = s_shows[(through - GUI_WORDMARK_SHOWS_AFTER) / GUI_WORDMARK_FRAME];
        }
        return;
    }
    if (wordmark->went_ms == 0ull)
    {
        return;
    }
    const unsigned long long through = now - wordmark->went_ms;
    if (through < GUI_WORDMARK_SLIDES)
    {
        *down = wordmark->under_down[through / GUI_WORDMARK_FRAME];
        *opacity = 255u;
        return;
    }
    *down = wordmark->rise;
    const unsigned long long fades = GUI_WORDMARK_SLIDES + GUI_WORDMARK_STAYS;
    if (through < fades)
    {
        *opacity = 255u;
    }
    else if (through < fades + GUI_WORDMARK_FADES)
    {
        wordmark_curves_made();
        *opacity = s_fades[(through - fades) / GUI_WORDMARK_FRAME];
    }
}

// the eye's whole side in the bar in pixels
static int wordmark_bar_eye_side(const GuiFrame *frame)
{
    return wordmark_round(WORDMARK_BAR_EYE_SIDE * (float)frame->scale);
}

// every stage of the flight made once: 1, or 0 where a cell could not be taken or the funnel has no length
static int wordmark_flight_made(GuiWordmark *wordmark, unsigned int raster_width);

int gui_wordmark_ticked(GuiWordmark *wordmark, GuiGlyphs *glyphs, GuiIcons *icons, const GuiPalette *palette, const GuiFrame *frame,
                        unsigned long long now)
{
    if (wordmark->stands)
    {
        const unsigned long long through = now - wordmark->stood_ms;
        if (wordmark->tree_read && (through >= GUI_WORDMARK_SHORTEST))
        {
            // the pane leaves: the view comes back under the name, and the name goes to the bar's foot under the eye; with
            // less motion asked for it is home at once
            wordmark->stands = 0;
            if (wordmark->still || !wordmark_pictures_made(wordmark, glyphs, icons, palette, frame))
            {
                wordmark->home = 1;
                wordmark_pictures_given(wordmark);
                return 1;
            }
            const GuiRect eye = gui_wordmark_eye_square(frame);
            wordmark->went_ms = now;
            wordmark->tip_y = frame->bar.y + frame->bar.height;
            wordmark->tip_left = eye.x;
            wordmark->tip_right = eye.x + wordmark_bar_eye_side(frame);
            wordmark_curves_made();
            for (unsigned int stage = 0u; stage < WORDMARK_SLIDE_STAGES; stage += 1u)
            {
                // a share of the way up, rounded, is far inside what a short represents
                const long long way = (long long)s_slides[stage] * (long long)wordmark->rise;
                wordmark->under_down[stage] = (short)((way + ((way < 0ll) ? -(GUI_WHOLE / 2) : (GUI_WHOLE / 2))) / GUI_WHOLE);
            }
            // the window's width is the frame's, the pixels the flight is cut to
            wordmark->pours = wordmark_flight_made(wordmark, frame->width);
            wordmark->home = !wordmark->pours;
            if (!wordmark->pours)
            {
                wordmark_name_given(wordmark);
            }
            return 1;
        }
        const int owed = !wordmark->under_whole;
        wordmark->under_whole = through >= GUI_WORDMARK_SHOWS_AFTER + GUI_WORDMARK_SHOWS;
        return owed;
    }
    if (wordmark->went_ms == 0ull)
    {
        return 0;
    }
    const unsigned long long through = now - wordmark->went_ms;
    if (wordmark->pours && (through >= GUI_WORDMARK_FLIES))
    {
        wordmark->pours = 0;
        wordmark->home = 1;
        wordmark_flight_given(wordmark);
    }
    if (through >= GUI_WORDMARK_SLIDES + GUI_WORDMARK_STAYS + GUI_WORDMARK_FADES)
    {
        wordmark->went_ms = 0ull;
        wordmark_pictures_given(wordmark);
    }
    return 1;
}

unsigned long long gui_wordmark_due(const GuiWordmark *wordmark, unsigned long long now)
{
    if (wordmark->stands)
    {
        if (!wordmark->under_whole)
        {
            return GUI_WORDMARK_FRAME;
        }
        if (!wordmark->tree_read)
        {
            return 0ull;
        }
        const unsigned long long through = now - wordmark->stood_ms;
        return (through < GUI_WORDMARK_SHORTEST) ? GUI_WORDMARK_SHORTEST - through : 1ull;
    }
    return (wordmark->went_ms != 0ull) ? GUI_WORDMARK_FRAME : 0ull;
}

// a stage of the flight at `part`, a share of it in the frame's words, walked: each row of the window from the tip down
// takes what the tables say of the name, one row of it where a row of the window is a row of the name, the rows that land
// on it by their shares where rows shorten, and only what has ink, stretched between the funnel's sides. With `span` NULL
// the spans and the coverage are counted alone; else each is written at `span` and `cover`. The spans and the bytes come
// back through `spans` and `covers`, added to what they hold
static void wordmark_stage_walked(GuiWordmark *wordmark, const int *read_at, const int *side_share, int part, unsigned int raster_width,
                                  GuiWordmarkSpan *span, unsigned char *cover_out, unsigned int *spans, unsigned int *covers)
{
    const long long forming = wordmark_smooth((int)(((long long)part * GUI_WHOLE) / WORDMARK_FORMS));
    const long long rising = (part > WORDMARK_STARTS) ? (((long long)(part - WORDMARK_STARTS) * GUI_WHOLE) / (GUI_WHOLE - WORDMARK_STARTS)) : 0ll;
    const int left = wordmark->from.x;
    const int right = wordmark->from.x + wordmark->from.width;
    const int high = wordmark->from.height;
    const int foot = wordmark->from.y + wordmark->from.height - wordmark->tip_y;
    // the name's top row's place at this moment, in rows with sixteen bits of fraction, is far inside what an int represents
    const int top_row = ((foot - high) * GUI_WHOLE) - (int)(((rising * rising) >> 16) * (long long)(foot + high));
    const int high_words = high * GUI_WHOLE;
    const unsigned int width = wordmark->name.width;
    const unsigned short (*const ink)[2] = (const unsigned short (*)[2])wordmark->ink;
    for (int y = 0; y <= foot; y += 1)
    {
        const int first = read_at[y] - top_row;
        if (first >= high_words)
        {
            break;
        }
        const int last = read_at[y + 1] - top_row;
        if (last <= 0)
        {
            continue;
        }
        const unsigned char *cover = NULL;
        unsigned int from_x = 0u;
        unsigned int past_x = 0u;
        const int one_row = last - first <= GUI_WHOLE;
        const int nearest = (first + (GUI_WHOLE / 2)) >> 16;
        const int from_row = (first > 0) ? first >> 16 : 0;
        const int rows_end = (last + (GUI_WHOLE - 1)) >> 16;
        const int to_row = (rows_end < high) ? rows_end : high;
        if (one_row)
        {
            // a row of the window is a row of the name: the one nearest, moved by whole rows
            if ((nearest < 0) || (nearest >= high))
            {
                continue;
            }
            from_x = ink[nearest][0];
            past_x = ink[nearest][1];
            // the row and column lie inside the mask
            cover = &wordmark->name.cover[((unsigned int)nearest * width) + from_x];
        }
        else
        {
            // the rows that land on it: the ink's extent across them
            from_x = width;
            for (int row = from_row; row < to_row; row += 1)
            {
                if (ink[row][0] < ink[row][1])
                {
                    from_x = (ink[row][0] < from_x) ? ink[row][0] : from_x;
                    past_x = (ink[row][1] > past_x) ? ink[row][1] : past_x;
                }
            }
        }
        if (from_x >= past_x)
        {
            continue;
        }
        if (!one_row && (span != NULL))
        {
            // each row by its share: their sums cleared across their ink alone
            memor.set(&(MemoriaCfg){.dst = wordmark->row_sum + from_x, .val = 0, .bytes = (unsigned long long)(past_x - from_x) * sizeof(*wordmark->row_sum)});
            const long long whole_span = (long long)last - (long long)first;
            for (int row = from_row; row < to_row; row += 1)
            {
                const int row_start = row * GUI_WHOLE;
                const int start = (first > row_start) ? first : row_start;
                const int stop = (last < row_start + GUI_WHOLE) ? last : row_start + GUI_WHOLE;
                // a share of a row by sixteen bits of fraction is a small word
                const unsigned int weight = (unsigned int)((((long long)(stop - start) * GUI_WHOLE) + (whole_span / 2ll)) / whole_span);
                const unsigned char *const row_cover = &wordmark->name.cover[(unsigned int)row * width];
                for (unsigned int x = ink[row][0]; x < ink[row][1]; x += 1u)
                {
                    wordmark->row_sum[x] += row_cover[x] * weight;
                }
            }
            for (unsigned int x = from_x; x < past_x; x += 1u)
            {
                const unsigned int mean = (wordmark->row_sum[x] + (GUI_WHOLE / 2u)) >> 16u;
                // a mean of bytes, 255 at most, is a byte
                wordmark->row[x] = (unsigned char)((mean > 255u) ? 255u : mean);
            }
            cover = wordmark->row + from_x;
        }
        const long long bend = (forming * (long long)side_share[y]) >> 16;
        // the sides' places by sixteen bits of fraction are words: a window is far narrower than their range
        const long long start = ((long long)left * GUI_WHOLE) + ((long long)(wordmark->tip_left - left) * bend);
        const long long end = ((long long)right * GUI_WHOLE) + ((long long)(wordmark->tip_right - right) * bend);
        const long long across = end - start;
        const int row_left = (int)(start + ((across * from_x) / width));
        const int row_right = (int)(start + ((across * past_x) / width));
        int x = 0;
        // the row's place: the tip's row and how far down the funnel
        unsigned char *const written = (span != NULL) ? cover_out + *covers : NULL;
        const unsigned int count = gui_raster_row_covered(cover, past_x - from_x, row_left, row_right, raster_width, written, &x);
        if (count == 0u)
        {
            continue;
        }
        if (span != NULL)
        {
            span[*spans] = (GuiWordmarkSpan){wordmark->tip_y + y, x, count, *covers};
        }
        *spans += 1u;
        *covers += count;
    }
}

// a stage's share of the flight in the frame's words
static int wordmark_stage_part(unsigned int stage)
{
    const unsigned long long through = (unsigned long long)stage * GUI_WORDMARK_FRAME;
    // a share of a time under a second is a word
    return (through >= GUI_WORDMARK_FLIES) ? GUI_WHOLE : (int)((through * GUI_WHOLE) / GUI_WORDMARK_FLIES);
}

// every stage of the flight made once, sized in one walk and written in a second into one cell, from the funnel's tables,
// which last this call; the name's mask given back after them: 1, or 0 where a cell could not be taken or the funnel has
// no length
static int wordmark_flight_made(GuiWordmark *wordmark, unsigned int raster_width)
{
    const int foot = wordmark->from.y + wordmark->from.height - wordmark->tip_y;
    if (foot < 1)
    {
        return 0;
    }
    // the foot is positive
    const unsigned int entries = (unsigned int)foot + 2u;
    int *const tables = g_cli_cells->temporary_buf_alloc((unsigned long long)entries * 2ull * sizeof(*tables), &g_gui_cells);
    if (tables == NULL)
    {
        return 0;
    }
    int *const read_at = tables;
    int *const side_share = tables + entries;
    wordmark_funnel_laid(wordmark, foot, read_at, side_share);
    unsigned int spans = 0u;
    unsigned int covers = 0u;
    for (unsigned int stage = 0u; stage < WORDMARK_FLIGHT_STAGES; stage += 1u)
    {
        wordmark_stage_walked(wordmark, read_at, side_share, wordmark_stage_part(stage), raster_width, NULL, NULL, &spans, &covers);
    }
    const unsigned long long first_bytes = ((unsigned long long)WORDMARK_FLIGHT_STAGES + 1ull) * sizeof(unsigned int);
    const unsigned long long span_bytes = (unsigned long long)spans * sizeof(GuiWordmarkSpan);
    unsigned int *const cell = g_cli_cells->persistent_buf_alloc(first_bytes + span_bytes + covers + 1ull, &g_gui_cells);
    if (cell == NULL)
    {
        g_cli_cells->persistent_buf_release(tables, &g_gui_cells);
        return 0;
    }
    wordmark->flight_first = cell;
    wordmark->flight_span = (GuiWordmarkSpan *)(cell + WORDMARK_FLIGHT_STAGES + 1u);
    wordmark->flight_cover = (unsigned char *)cell + first_bytes + span_bytes;
    spans = 0u;
    covers = 0u;
    for (unsigned int stage = 0u; stage < WORDMARK_FLIGHT_STAGES; stage += 1u)
    {
        wordmark->flight_first[stage] = spans;
        wordmark_stage_walked(wordmark, read_at, side_share, wordmark_stage_part(stage), raster_width, wordmark->flight_span, wordmark->flight_cover, &spans, &covers);
    }
    wordmark->flight_first[WORDMARK_FLIGHT_STAGES] = spans;
    g_cli_cells->persistent_buf_release(tables, &g_gui_cells);
    wordmark_name_given(wordmark);
    return 1;
}

// the stage of the flight standing `through` milliseconds on, blitted
static void wordmark_stage_laid(const GuiWordmark *wordmark, GuiRaster *raster, GuiColor color, unsigned long long through)
{
    const unsigned long long reached = through / GUI_WORDMARK_FRAME;
    // the stage is one of the stages, far fewer than an int counts
    const unsigned int stage = (reached >= WORDMARK_FLIGHT_STAGES) ? WORDMARK_FLIGHT_STAGES - 1u : (unsigned int)reached;
    for (unsigned int at = wordmark->flight_first[stage]; at < wordmark->flight_first[stage + 1u]; at += 1u)
    {
        const GuiWordmarkSpan *const span = &wordmark->flight_span[at];
        gui_raster_span_laid(raster, span->x, span->y, &wordmark->flight_cover[span->cover], span->count, color);
    }
}

void gui_wordmark_drawn(GuiWordmark *wordmark, GuiRaster *raster, GuiGlyphs *glyphs, GuiIcons *icons, const GuiPalette *palette,
                        const GuiFrame *frame, unsigned long long now)
{
    if (!wordmark->stands && (wordmark->went_ms == 0ull))
    {
        return;
    }
    if (wordmark->stands && !wordmark_pictures_made(wordmark, glyphs, icons, palette, frame))
    {
        return;
    }
    int down = 0;
    unsigned int opacity = 0u;
    gui_wordmark_under_at(wordmark, now, &down, &opacity);
    gui_raster_picture_laid(raster, &wordmark->under, wordmark->under_x, wordmark->under_y + down, opacity);
    if (wordmark->stands)
    {
        // the raster's sides are far inside what an int represents
        gui_raster_masked(raster, &wordmark->name, wordmark->from.x, wordmark->from.y, palette->open_text,
                          (GuiRect){0, 0, (int)raster->width, (int)raster->height});
    }
    else if (wordmark->pours)
    {
        wordmark_stage_laid(wordmark, raster, palette->open_text, now - wordmark->went_ms);
    }
}

GuiRect gui_wordmark_eye_square(const GuiFrame *frame)
{
    const long long scale = frame->scale;
    const long long claim = frame->claim.claim;
    const long long room = (long long)frame->claim.room_top + (long long)frame->claim.room_left;
    // the share of the side kept, a number of GUI_WHOLE: the claim takes it, the head's and the left's rooms give it back
    const long long shrunk = GUI_WHOLE - ((claim * (GUI_WHOLE - room)) / GUI_WHOLE);
    const long long kept = (shrunk < 0ll) ? 0ll : ((shrunk > GUI_WHOLE) ? GUI_WHOLE : shrunk);
    const long long gap = (long long)GUI_FRAME_GAP * claim;
    // the sizes in the page's pixels by sixteen bits of fraction, each rounded to a whole pixel, far inside what an int represents
    const int side = (int)((((WORDMARK_BAR_EYE_SIDE_WORDS * scale * kept) / GUI_WHOLE) + (GUI_WHOLE / 2)) / GUI_WHOLE);
    return (GuiRect){(int)((((WORDMARK_BAR_EYE_LEFT_WORDS + gap) * scale) + (GUI_WHOLE / 2)) / GUI_WHOLE),
                     (int)((((WORDMARK_BAR_EYE_TOP_WORDS + gap) * scale) + (GUI_WHOLE / 2)) / GUI_WHOLE), side, side};
}

void gui_wordmark_eye_drawn(const GuiWordmark *wordmark, GuiRaster *raster, GuiIcons *icons, const GuiPalette *palette, const GuiFrame *frame)
{
    if (!wordmark->home)
    {
        return;
    }
    const GuiRect square = gui_wordmark_eye_square(frame);
    if (gui_rect_empty(square))
    {
        return;
    }
    // the square's side is positive where it is not empty
    const unsigned int side = (unsigned int)square.width;
    const GuiMask *const eye = gui_icon(icons, "eye", side, (WORDMARK_BAR_EYE_LINE * (float)side) / WORDMARK_EYE_BOX_WIDE);
    if ((eye != NULL) && (eye->cover != NULL))
    {
        // the raster's sides are far inside what an int represents
        gui_raster_masked(raster, eye, square.x, square.y, palette->bar_mark, (GuiRect){0, 0, (int)raster->width, (int)raster->height});
    }
}

int gui_wordmark_holds(const GuiWordmark *wordmark, const GuiFrame *frame, int x, int y)
{
    if (wordmark->stands)
    {
        return gui_rect_holds(wordmark->from, x, y) || gui_rect_holds(wordmark->under_box, x, y);
    }
    return wordmark->home && gui_rect_holds(gui_wordmark_eye_square(frame), x, y);
}
