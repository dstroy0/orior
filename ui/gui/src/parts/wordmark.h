// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// wordmark.h: the name, as wordmark.js, incised.js, loading.js and session.js draw and move it: orior over the eye
// with TABVLA cut under it, standing large over the lattice on the pane the first start in the system's session
// shows while the tree is read, the view under it not drawn; orior and the eye poured up a funnel to the bar's head
// as the pane leaves, TABVLA sliding up to the name's middle, staying whole and fading; and the eye alone in the bar
// from then on, free at the window's top left while the claim holds and shrinking out toward its own top left with
// the bar and the strip both away; a press on either opens orior's repository (wordmark.c)
#ifndef GUI_PARTS_WORDMARK_H
#define GUI_PARTS_WORDMARK_H
typedef struct GuiWordmarkFrameCfg GuiWordmarkFrameCfg;
typedef struct GuiWordmarkReadCfg GuiWordmarkReadCfg;
typedef struct GuiWordmarkGlyphsCfg GuiWordmarkGlyphsCfg;
typedef struct GuiWordmarkTextCfg GuiWordmarkTextCfg;
typedef struct GuiWordmarkCfg GuiWordmarkCfg;
typedef struct GuiWordmarkStillCfg GuiWordmarkStillCfg;
#include "../frame.h"
#include "../glyphs.h"
#include "../icons.h"
#include "../palette.h"
#include "embed_dispatch_layout.h"

// the page's times in milliseconds: the least the pane stands, SHORTEST; the flight, FLIES; TABVLA's slide up, SLIDES,
// its time whole there, HOLDS, and its fade, FADES; the wait before it fades in as the name stands and that fade; and
// the time between frames while any of it moves
#define GUI_WORDMARK_MIN_VISIBLE_MS 6000ull
#define GUI_WORDMARK_FLIGHT_MS 900ull
#define GUI_WORDMARK_SLIDE_MS 540ull
#define GUI_WORDMARK_FADE_OUT_DELAY_MS 500ull
#define GUI_WORDMARK_FADE_OUT_MS 360ull
#define GUI_WORDMARK_FADE_IN_DELAY_MS 300ull
#define GUI_WORDMARK_FADE_IN_MS 500ull
#define GUI_WORDMARK_FRAME_MS 16ull
// the repository a press on the name opens, and the entry orior's folder saves the session last started in under
#define GUI_WORDMARK_REPOSITORY "https://github.com/dstroy0/orior"
#define GUI_WORDMARK_SESSION_KEY "orior.wordmark.session"

// A session as the system names it, down to two words: when the computer started and its log in's hash, and whether it
// was read at all
typedef struct
{
    unsigned long long booted;
    unsigned long long logon;
    int known;
} GuiWordmarkSession;

// Where the large name stands in a frame: the pixels of its em; the box round orior and the eye; TABVLA's box; and how
// far TABVLA rises, in pixels, to stand at the eye's middle, negative for up
typedef struct
{
    unsigned int em;
    GuiRect name;
    GuiRect under;
    int rise;
} GuiWordmarkPlace;

// A span of a stage of the flight: the row and the column it starts at, its pixels, and where its coverage starts
typedef struct
{
    int y;
    int x;
    unsigned int count;
    unsigned int cover;
} GuiWordmarkSpan;

// The wordmark: whether the eye stands in the bar; whether the pane stands, the view under it not drawn, when it was
// shown and whether TABVLA has come whole on it; whether the tree has been read; when the name went, 0 where it has
// not, whether it still pours up the funnel, and the funnel's tip on the bar's foot; where orior and the eye stood,
// TABVLA's box, where its picture is laid and how far it rises; the em the pictures were made at and the word's width
// at it; the window's width and inside the places were taken for; orior and the eye as one mask, and in its one cell
// the row the funnel lays, that row's sums and each mask row's ink, its first column and the column past its last; the
// funnel's stages in a cell of their own while it pours, each stage's first span, the spans and their coverage, and
// TABVLA's way up at each stage of its slide; TABVLA's picture, its cuts with their shade and their lip, with its own
// alpha; the session last started in as orior's folder saves it; and whether less motion is asked for
typedef struct
{
    int still;
    int home;
    int stands;
    unsigned long long visible_start_ms;
    int under_whole;
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
    unsigned int made_em;
    float word_wide;
    unsigned int laid_width;
    GuiRect laid_inside;
    GuiMask name;
    unsigned char *row;
    unsigned int *row_sum;
    unsigned short (*ink)[2];
    unsigned int *flight_first;
    GuiWordmarkSpan *flight_span;
    unsigned char *flight_cover;
    short under_down[GUI_WORDMARK_SLIDE_MS / GUI_WORDMARK_FRAME_MS + 1ull];
    GuiRaster under;
    GuiWordmarkSession saved;
} GuiWordmark;

// the wordmark made with no eye in the bar and nothing standing; and every cell of its given back
void gui_wordmark_init(const GuiWordmarkStillCfg *args);
void gui_wordmark_free(const GuiWordmarkStillCfg *args);
// less motion asked for or no longer: asked for, TABVLA stands whole with the name, the pane leaves with no flight, and
// a flight under way ends with the eye home and nothing of the name saved
void gui_wordmark_still(const GuiWordmarkStillCfg *args);
// `size` bytes of a session as the CLI writes it, `{"booted":n,"logon":s}`, read into `session`, its log in as a hash
// and no tree made of it: 1, or 0 with it not known where the text is NULL or not of that shape; and whether this start
// is the first in the system's session, as session.js reads it: 1 where either session is not known, where their log
// ins differ, or where the computer's starts stand a minute or more apart
int gui_wordmark_session_read(const GuiWordmarkTextCfg *args);
int gui_wordmark_first_in(const GuiWordmarkCfg *args);
// the pane shown, the name standing on it from `now`; the eye home at once, every start after the first; and the tree
// read, the pane leaving once it has stood its least
void gui_wordmark_stand(const GuiWordmarkFrameCfg *args);
void gui_wordmark_land(const GuiWordmarkStillCfg *args);
void gui_wordmark_tree_read(const GuiWordmarkStillCfg *args);
// the name's place in `frame`, on the frame's inside, as style.css sets it
void gui_wordmark_place(const GuiWordmarkGlyphsCfg *args);
// TABVLA's way down from its box, in pixels, and its opacity, 0 to 255, at `now`, as its transition and its animation
// run; whether a frame is owed for what moved by `now`, the pane leaving and the funnel ending as the clock passes
// them, the frame being what the funnel's tip is taken from; and when the wordmark next wants a tick, in milliseconds,
// 0 for never
void gui_wordmark_under_at(const GuiWordmarkReadCfg *args);
int gui_wordmark_tick(const GuiWordmarkFrameCfg *args);
unsigned long long gui_wordmark_next_deadline_ms(const GuiWordmarkReadCfg *args);
// the large name drawn as it stands or goes at `now`: orior and the eye on the pane or up the funnel, and TABVLA; and
// the square the eye stands on in the bar, its size given the claim's numbers, empty where it has shrunk out; and the
// eye drawn on it once it is home
void gui_wordmark_draw(const GuiWordmarkFrameCfg *args);
GuiRect gui_wordmark_eye_square(const GuiWordmarkFrameCfg *args);
void gui_wordmark_eye_draw(const GuiWordmarkFrameCfg *args);
// whether a press at x, y lands on the name: the eye once it is home, the large name where it was last drawn while it
// stands
int gui_wordmark_holds(const GuiWordmarkFrameCfg *args);
struct GuiWordmarkFrameCfg
{
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
    const GuiWordmark *wordmark_read;
    int *down;
    unsigned int *opacity;
};

struct GuiWordmarkGlyphsCfg
{
    GuiGlyphs *glyphs;
    const GuiFrame *frame;
    GuiWordmarkPlace *place;
};

struct GuiWordmarkTextCfg
{
    const char *text;
    unsigned long long size;
    GuiWordmarkSession *session;
};

struct GuiWordmarkCfg
{
    const GuiWordmarkSession *saved;
    const GuiWordmarkSession *now_wordmark_session_pointer;
};

struct GuiWordmarkStillCfg
{
    GuiWordmark *wordmark;
    int still;
};

typedef struct
{
    void (*init)(const GuiWordmarkStillCfg *args);
    void (*free)(const GuiWordmarkStillCfg *args);
    void (*still)(const GuiWordmarkStillCfg *args);
    void (*land)(const GuiWordmarkStillCfg *args);
    void (*tree_read)(const GuiWordmarkStillCfg *args);
} CliWordmarkStillNs;
EMBED_TABLE_LAYOUT(CliWordmarkStillNs, init, free, still, land, tree_read);
EMBED_TABLE_STORAGE CliWordmarkStillNs cli_wordmark_still EMBED_UNUSED = {
    .init = gui_wordmark_init,
    .free = gui_wordmark_free,
    .still = gui_wordmark_still,
    .land = gui_wordmark_land,
    .tree_read = gui_wordmark_tree_read,
};

typedef struct
{
    int (*session_read)(const GuiWordmarkTextCfg *args);
} GuiWordmarkTextNs;
EMBED_TABLE_LAYOUT(GuiWordmarkTextNs, session_read);
EMBED_TABLE_STORAGE GuiWordmarkTextNs gui_wordmark_text EMBED_UNUSED = {
    .session_read = gui_wordmark_session_read,
};

typedef struct
{
    int (*first_in)(const GuiWordmarkCfg *args);
} GuiWordmarkNs;
EMBED_TABLE_LAYOUT(GuiWordmarkNs, first_in);
EMBED_TABLE_STORAGE GuiWordmarkNs gui_wordmark EMBED_UNUSED = {
    .first_in = gui_wordmark_first_in,
};

typedef struct
{
    void (*stand)(const GuiWordmarkFrameCfg *args);
    int (*tick)(const GuiWordmarkFrameCfg *args);
    void (*draw)(const GuiWordmarkFrameCfg *args);
    GuiRect (*eye_square)(const GuiWordmarkFrameCfg *args);
    void (*eye_draw)(const GuiWordmarkFrameCfg *args);
    int (*holds)(const GuiWordmarkFrameCfg *args);
} GuiWordmarkFrameNs;
EMBED_TABLE_LAYOUT(GuiWordmarkFrameNs, stand, tick, draw, eye_square, eye_draw, holds);
EMBED_TABLE_STORAGE GuiWordmarkFrameNs gui_wordmark_frame EMBED_UNUSED = {
    .stand = gui_wordmark_stand,
    .tick = gui_wordmark_tick,
    .draw = gui_wordmark_draw,
    .eye_square = gui_wordmark_eye_square,
    .eye_draw = gui_wordmark_eye_draw,
    .holds = gui_wordmark_holds,
};

typedef struct
{
    void (*place)(const GuiWordmarkGlyphsCfg *args);
} GuiWordmarkGlyphsNs;
EMBED_TABLE_LAYOUT(GuiWordmarkGlyphsNs, place);
EMBED_TABLE_STORAGE GuiWordmarkGlyphsNs gui_wordmark_glyphs EMBED_UNUSED = {
    .place = gui_wordmark_place,
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
