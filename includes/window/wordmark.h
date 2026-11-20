// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// wordmark.h: the name, as wordmark.js, incised.js, loading.js and session.js draw and move it: orior over the eye
// with TABVLA cut under it, located large over the lattice on the pane the first start in the system's session
// shows while the tree is read, the view under it not drawn; orior and the eye poured up a funnel to the bar's head
// as the pane leaves, TABVLA sliding up to the name's middle, staying whole and fading; and the eye alone in the bar
// from then on, free at the window's top left while the claim holds and shrinking out toward its own top left with
// the bar and the side_menu both away; a press on either opens orior's repository (wordmark.c)
#ifndef GUI_WINDOW_WORDMARK_H
#define GUI_WINDOW_WORDMARK_H
#include "frame.h"
#include "glyphs.h"
#include "icons.h"
#include "palette.h"
#include "embed_dispatch_layout.h"

typedef struct GuiWordmarkFrameCfg GuiWordmarkFrameCfg;
typedef struct GuiWordmarkReadCfg GuiWordmarkReadCfg;
typedef struct GuiWordmarkGlyphsCfg GuiWordmarkGlyphsCfg;
typedef struct GuiWordmarkStillCfg GuiWordmarkStillCfg;

// the durations in milliseconds: the pane's minimum visible time; the flight; TABVLA's slide up, the delay before it
// fades out and the fade out. The delay before it fades in as the name appears and that fade in; and the frame time
// while any of it moves
#define GUI_WORDMARK_MIN_VISIBLE_MS 6000ULL
#define GUI_WORDMARK_FLIGHT_MS 900ULL
#define GUI_WORDMARK_SLIDE_MS 540ULL
#define GUI_WORDMARK_FADE_OUT_DELAY_MS 500ULL
#define GUI_WORDMARK_FADE_OUT_MS 360ULL
#define GUI_WORDMARK_FADE_IN_DELAY_MS 300ULL
#define GUI_WORDMARK_FADE_IN_MS 500ULL
#define GUI_WORDMARK_FRAME_MS 16ULL
// the repository a press on the name opens, and the entry orior's folder saves the session last started in under

// Where the large name is in a frame: the pixels of its em; the box round orior and the eye; TABVLA's box. And how
// far TABVLA rises, in pixels, to be at the eye's middle, negative for up
typedef struct
{
    unsigned int em;
    GuiRect name;
    GuiRect under;
    int rise;
} GuiWordmarkPosition;

// A span of a stage of the flight: the row and the column it starts at, its pixels, and where its coverage starts
typedef struct
{
    int y;
    int x;
    unsigned int count;
    unsigned int cover;
} GuiWordmarkSpan;

// The wordmark:
//   - whether the eye is in the bar.
//   - whether the pane exists, with the view under it not drawn. When the pane showed, and whether TABVLA came whole
//     on it.
//   - whether the code read the tree.
//   - when the name went, 0 where it did not. Whether it still pours up the funnel, and the tip of the funnel on the
//     foot of the bar.
//   - where orior and the eye were, the box of TABVLA, where its picture sits and how far it rises.
//   - the em that the code made the pictures at, and the width of the word at it.
//   - the width of the window and the inside that the code took the places for.
//   - orior and the eye as one mask. Its one cell holds the row that the funnel positions and the sums of that row. It
//     also holds the ink of each mask row, its first column and the column past its last.
//   - the stages of the funnel in a cell of their own while it pours: the first span of each stage, the spans and their
//     coverage.
//   - the way up of TABVLA at each stage of its slide.
//   - the picture of TABVLA, its cuts with their shade and their lip, with its own alpha.
//   - the session last started in, as the folder of orior saves it.
//   - whether the user asks for less motion.
typedef struct
{
    int still;
    int home;
    int exists;
    unsigned long long visible_start_ms;
    int fade_in_finished;
    int tree_read;
    unsigned long long exit_start_ms;
    int pours;
    int tip_y;
    int tip_left;
    int tip_right;
    GuiRect from;
    GuiRect under_box;
    int under_x;
    int under_y;
    int rise;
    unsigned int created_em;
    float word_width;
    unsigned int positioned_width;
    GuiRect positioned_inside;
    GuiMask name;
    unsigned char *row;
    unsigned int *row_sum;
    unsigned short (*text_color)[2];
    unsigned int *flight_first;
    GuiWordmarkSpan *flight_span;
    unsigned char *flight_cover;
    short under_down[GUI_WORDMARK_SLIDE_MS / GUI_WORDMARK_FRAME_MS + 1ULL];
    GuiRaster under;
} GuiWordmark;

// the wordmark made with no eye in the bar and nothing located; and every cell of its given back
void gui_wordmark_init(const GuiWordmarkStillCfg *args);
// a request for less motion, or its end. With the request, TABVLA is whole with the name, and the pane leaves with no
// flight. A flight under way then ends with the eye home, and the code keeps nothing of the name
void gui_wordmark_still(const GuiWordmarkStillCfg *args);
// the pane displayed, with the name on it from `now`. The eye goes home at once, at each start after the first. Then
// the tree read: the pane leaves once its least time is past
void gui_wordmark_are(const GuiWordmarkFrameCfg *args);
void gui_wordmark_land(const GuiWordmarkStillCfg *args);
void gui_wordmark_tree_read(const GuiWordmarkStillCfg *args);
// the name's place in `frame`, on the frame's inside, as style.css sets it
void gui_wordmark_position(const GuiWordmarkGlyphsCfg *args);
// the way down of TABVLA from its box, in pixels, and its opacity, 0 to 255, as its transition and its animation run.
// Then whether the window owes a frame for what moved by `now`. The pane leaves and the funnel ends as the clock passes
// them. The tip of the funnel comes from that frame. Then the next deadline of the wordmark, 0 for none
void gui_wordmark_under_at(const GuiWordmarkReadCfg *args);
int gui_wordmark_tick(const GuiWordmarkFrameCfg *args);
unsigned long long gui_wordmark_next_deadline_ms(const GuiWordmarkReadCfg *args);
// the large name, drawn as it is or as it goes: orior and the eye on the pane or up the funnel, and TABVLA. Then the
// square of the eye in the bar, its size from the numbers of the claim, empty where it shrank out. Then the eye, drawn
// on it once it is home
void gui_wordmark_draw(const GuiWordmarkFrameCfg *args);
GuiRect gui_wordmark_logo_square(const GuiWordmarkFrameCfg *args);
void gui_wordmark_logo_draw(const GuiWordmarkFrameCfg *args);
// whether a press at x, y lands on the name. That is the eye once it is home. While the large name shows, it is the
// large name where the code last drew it
int gui_wordmark_holds(const GuiWordmarkFrameCfg *args);
struct GuiWordmarkFrameCfg
{
    unsigned long long now_ms;
    GuiWordmark *wordmark;
    GuiGlyphs *glyphs;
    const GuiFrame *frame;
    const GuiWordmark *wordmark_read;
    GuiIcons *icons;
    const GuiPalette *palette;
    GuiRaster *raster;
    int x;
    int y;
};

struct GuiWordmarkReadCfg
{
    unsigned long long now_ms;
    const GuiWordmark *wordmark_read;
    int *down;
    unsigned int *opacity;
};

struct GuiWordmarkGlyphsCfg
{
    GuiGlyphs *glyphs;
    const GuiFrame *frame;
    GuiWordmarkPosition *place;
};

struct GuiWordmarkStillCfg
{
    GuiWordmark *wordmark;
    int still;
};

typedef struct
{
    void (*init)(const GuiWordmarkStillCfg *args);
    void (*still)(const GuiWordmarkStillCfg *args);
    void (*land)(const GuiWordmarkStillCfg *args);
    void (*tree_read)(const GuiWordmarkStillCfg *args);
} GuiWordmarkStillNs;
EMBED_TABLE_LAYOUT(GuiWordmarkStillNs, init, still, land, tree_read);
EMBED_TABLE_STORAGE GuiWordmarkStillNs gui_wordmark_still_table EMBED_UNUSED = {
    .init = gui_wordmark_init,
    .still = gui_wordmark_still,
    .land = gui_wordmark_land,
    .tree_read = gui_wordmark_tree_read,
};

typedef struct
{
    void (*are)(const GuiWordmarkFrameCfg *args);
    int (*tick)(const GuiWordmarkFrameCfg *args);
    void (*draw)(const GuiWordmarkFrameCfg *args);
    GuiRect (*logo_square)(const GuiWordmarkFrameCfg *args);
    void (*logo_draw)(const GuiWordmarkFrameCfg *args);
    int (*holds)(const GuiWordmarkFrameCfg *args);
} GuiWordmarkFrameNs;
EMBED_TABLE_LAYOUT(GuiWordmarkFrameNs, are, tick, draw, logo_square, logo_draw, holds);
EMBED_TABLE_STORAGE GuiWordmarkFrameNs gui_wordmark_frame EMBED_UNUSED = {
    .are = gui_wordmark_are,
    .tick = gui_wordmark_tick,
    .draw = gui_wordmark_draw,
    .logo_square = gui_wordmark_logo_square,
    .logo_draw = gui_wordmark_logo_draw,
    .holds = gui_wordmark_holds,
};

typedef struct
{
    void (*place)(const GuiWordmarkGlyphsCfg *args);
} GuiWordmarkGlyphsNs;
EMBED_TABLE_LAYOUT(GuiWordmarkGlyphsNs, place);
EMBED_TABLE_STORAGE GuiWordmarkGlyphsNs gui_wordmark_glyphs EMBED_UNUSED = {
    .place = gui_wordmark_position,
};

typedef struct
{
    void (*under_at)(const GuiWordmarkReadCfg *args);
    unsigned long long (*next_deadline_ms)(const GuiWordmarkReadCfg *args);
} GuiWordmarkReadNs;
EMBED_TABLE_LAYOUT(GuiWordmarkReadNs, under_at, next_deadline_ms);
EMBED_TABLE_STORAGE GuiWordmarkReadNs gui_wordmark_read EMBED_UNUSED = {
    .under_at = gui_wordmark_under_at,
    .next_deadline_ms = gui_wordmark_next_deadline_ms,
};

#endif
