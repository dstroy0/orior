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
#include "../frame.h"
#include "../glyphs.h"
#include "../icons.h"
#include "../palette.h"

// the page's times in milliseconds: the least the pane stands, SHORTEST; the flight, FLIES; TABVLA's slide up, SLIDES,
// its time whole there, HOLDS, and its fade, FADES; the wait before it fades in as the name stands and that fade; and
// the time between frames while any of it moves
#define GUI_WORDMARK_SHORTEST 6000ull
#define GUI_WORDMARK_FLIES 900ull
#define GUI_WORDMARK_SLIDES 540ull
#define GUI_WORDMARK_STAYS 500ull
#define GUI_WORDMARK_FADES 360ull
#define GUI_WORDMARK_SHOWS_AFTER 300ull
#define GUI_WORDMARK_SHOWS 500ull
#define GUI_WORDMARK_FRAME 16ull
// the repository a press on the name opens, and the entry orior's folder keeps the session last started in under
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
// TABVLA's box, where its picture is laid and how far it rises; the em the pictures were made at and the word's width at
// it; the window's width and inside the places were taken for; orior and the eye as one mask, and in its one cell the row
// the funnel lays, that row's sums and each mask row's ink, its first column and the column past its last; the funnel's
// stages in a cell of their own while it pours, each stage's first span, the spans and their coverage, and TABVLA's way up
// at each stage of its slide; TABVLA's picture, its cuts with their shade and their lip, with its own
// alpha; the session last started in as orior's folder keeps it; and whether less motion is asked for
typedef struct
{
    int still;
    int home;
    int stands;
    unsigned long long stood_ms;
    int under_whole;
    int tree_read;
    unsigned long long went_ms;
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
    short under_down[GUI_WORDMARK_SLIDES / GUI_WORDMARK_FRAME + 1ull];
    GuiRaster under;
    GuiWordmarkSession kept;
} GuiWordmark;

// the wordmark made with no eye in the bar and nothing standing; and every cell of its given back
void gui_wordmark_made(GuiWordmark *wordmark);
void gui_wordmark_given(GuiWordmark *wordmark);
// less motion asked for or no longer: asked for, TABVLA stands whole with the name, the pane leaves with no flight, and
// a flight under way ends with the eye home and nothing of the name kept
void gui_wordmark_still(GuiWordmark *wordmark, int still);
// `size` bytes of a session as the CLI writes it, `{"booted":n,"logon":s}`, read into `session`, its log in as a hash and
// no tree made of it: 1, or 0 with it not known where the text is NULL or not of that shape; and whether this start is
// the first in the system's session, as session.js reads it: 1 where either session is not known, where their log ins
// differ, or where the computer's starts stand a minute or more apart
int gui_wordmark_session_read(const char *text, unsigned long long size, GuiWordmarkSession *session);
int gui_wordmark_first_in(const GuiWordmarkSession *kept, const GuiWordmarkSession *now);
// the pane shown, the name standing on it from `now`; the eye home at once, every start after the first; and the tree
// read, the pane leaving once it has stood its least
void gui_wordmark_stood(GuiWordmark *wordmark, unsigned long long now);
void gui_wordmark_landed(GuiWordmark *wordmark);
void gui_wordmark_tree_read(GuiWordmark *wordmark);
// the name's place in `frame`, on the frame's inside, as style.css sets it
void gui_wordmark_placed(GuiGlyphs *glyphs, const GuiFrame *frame, GuiWordmarkPlace *place);
// TABVLA's way down from its box, in pixels, and its opacity, 0 to 255, at `now`, as its transition and its animation
// run; whether a frame is owed for what moved by `now`, the pane leaving and the funnel ending as the clock passes
// them, the frame being what the funnel's tip is taken from; and when the wordmark next wants a tick, in milliseconds,
// 0 for never
void gui_wordmark_under_at(const GuiWordmark *wordmark, unsigned long long now, int *down, unsigned int *opacity);
int gui_wordmark_ticked(GuiWordmark *wordmark, GuiGlyphs *glyphs, GuiIcons *icons, const GuiPalette *palette,
                        const GuiFrame *frame, unsigned long long now);
unsigned long long gui_wordmark_due(const GuiWordmark *wordmark, unsigned long long now);
// the large name drawn as it stands or goes at `now`: orior and the eye on the pane or up the funnel, and TABVLA; and
// the square the eye stands on in the bar, its size given the claim's numbers, empty where it has shrunk out; and the
// eye drawn on it once it is home
void gui_wordmark_drawn(GuiWordmark *wordmark, GuiRaster *raster, GuiGlyphs *glyphs, GuiIcons *icons,
                        const GuiPalette *palette, const GuiFrame *frame, unsigned long long now);
GuiRect gui_wordmark_eye_square(const GuiFrame *frame);
void gui_wordmark_eye_drawn(const GuiWordmark *wordmark, GuiRaster *raster, GuiIcons *icons, const GuiPalette *palette,
                            const GuiFrame *frame);
// whether a press at x, y lands on the name: the eye once it is home, the large name where it was last drawn while it
// stands
int gui_wordmark_holds(const GuiWordmark *wordmark, const GuiFrame *frame, int x, int y);
#endif
