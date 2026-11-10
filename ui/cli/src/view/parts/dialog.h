// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// dialog.h: a modal dialog in a system window of its own, owned by the window that asks it. It lies on the glass
// as Preferences does. A head band holds the title and the close mark, the body holds lines of text, and a foot row
// holds the buttons at its right. A press, Enter or Escape gives the answer as a change (dialog.c)
#ifndef GUI_PARTS_DIALOG_H
#define GUI_PARTS_DIALOG_H
#include "../frame.h"
#include "../glyphs.h"
#include "../palette.h"
#include "embed_dispatch_layout.h"
#include "glass.h"

typedef struct GuiDialogCfg GuiDialogCfg;
typedef struct GuiDialogAskCfg GuiDialogAskCfg;
typedef struct GuiDialogWantCfg GuiDialogWantCfg;
typedef struct GuiDialogWidthCfg GuiDialogWidthCfg;
typedef struct GuiDialogReadCfg GuiDialogReadCfg;
typedef struct GuiDialogKeyCfg GuiDialogKeyCfg;
typedef struct GuiDialogPointCfg GuiDialogPointCfg;
typedef struct GuiDialogDrawCfg GuiDialogDrawCfg;

// The most buttons, and the most bytes of the title, of the body and of a button's label, with the end byte
#define GUI_DIALOG_BUTTONS_MOST 4u
#define GUI_DIALOG_TITLE 128u
#define GUI_DIALOG_BODY 1024u
#define GUI_DIALOG_LABEL 32u
// The sizes in unscaled pixels: the mat, the band of glass between the window's border and the sheet; the pad, the
// sheet's inset to its text and buttons; the head band's height, a body line's height, a button's height, the
// padding each side of a label, a button's least width and the space between two buttons. Then the close mark's
// width and its cross's side, a button's rounding, the window's rounding, the text's size and the least width
#define GUI_DIALOG_MAT 6
#define GUI_DIALOG_PAD 12
#define GUI_DIALOG_HEAD 36
#define GUI_DIALOG_LINE 22
#define GUI_DIALOG_BUTTON_HEIGHT 28
#define GUI_DIALOG_BUTTON_PAD 16
#define GUI_DIALOG_BUTTON_LEAST 88
#define GUI_DIALOG_BUTTON_SPACE 8
#define GUI_DIALOG_CLOSE_WIDTH 32
#define GUI_DIALOG_CLOSE_SIDE 10
#define GUI_DIALOG_BUTTON_RADIUS 6u
#define GUI_DIALOG_WINDOW_RADIUS 8u
#define GUI_DIALOG_SIZE 13u
#define GUI_DIALOG_LEAST_WIDTH 420

// What an input does to the dialog: nothing, a change that needs a new frame, a button's answer in `button`, or the
// dialog closed with no answer
#define GUI_DIALOG_NOTHING 0u
#define GUI_DIALOG_DRAWN 1u
#define GUI_DIALOG_ANSWERED 2u
#define GUI_DIALOG_CLOSED 3u
typedef struct
{
    unsigned int type;
    unsigned int button;
} GuiDialogChange;

// The dialog: whether it shows and holds the focus, its client's size and scale, its texts, and each button's width
// at the scale. `preferred` is the button Enter presses. `marked` is the button with the keys. `pointed` is the button
// under the pointer and `pressed` the button a press went down on, each GUI_DIALOG_BUTTONS_MOST for none
typedef struct
{
    int displayed;
    int focused;
    unsigned int width;
    unsigned int height;
    int scale;
    char title[GUI_DIALOG_TITLE];
    char body[GUI_DIALOG_BODY];
    char label[GUI_DIALOG_BUTTONS_MOST][GUI_DIALOG_LABEL];
    int button_width[GUI_DIALOG_BUTTONS_MOST];
    unsigned int buttons;
    unsigned int preferred;
    unsigned int marked;
    unsigned int pointed;
    unsigned int pressed;
    int close_pointed;
    int close_pressed;
    int error;
} GuiDialog;

// a dialog not shown
void gui_dialog_init(const GuiDialogCfg *args);
// The dialog shown with `title`, the lines of `body` and the `count` labels of `labels`, `preferred` the button Enter
// presses. Where `error` is set, the dialog tells of an error and its focus border stands in the error color. It
// returns 1, or 0 where the count is 0 or more than GUI_DIALOG_BUTTONS_MOST, and then nothing changes
int gui_dialog_ask(const GuiDialogAskCfg *args);
// the client's size at `scale` that shows the title, every line and every button
void gui_dialog_want(const GuiDialogWantCfg *args);
// the client sized `width` by `height` at `scale`, each button's width measured with `glyphs`
void gui_dialog_resize(const GuiDialogWidthCfg *args);
// Where each part stands in the client: the head band, the close mark, the body and the button `at`
GuiRect gui_dialog_head_rect(const GuiDialogReadCfg *args);
GuiRect gui_dialog_close_rect(const GuiDialogReadCfg *args);
GuiRect gui_dialog_body_rect(const GuiDialogReadCfg *args);
GuiRect gui_dialog_button_rect(const GuiDialogReadCfg *args);
// A key named `name`: Enter answers the button with the keys, Escape closes, Tab and Right move the keys to the next
// button, Shift+Tab and Left to the one before
void gui_dialog_key(const GuiDialogKeyCfg *args);
// The pointer at x, y: moved, pressed down or let up. A release on the button or the close mark that the press went
// down on answers it
void gui_dialog_move(const GuiDialogPointCfg *args);
void gui_dialog_press(const GuiDialogPointCfg *args);
void gui_dialog_release(const GuiDialogPointCfg *args);
// the dialog drawn into `raster` over its lattice `ground`, or over the deep color where `ground` is NULL
void gui_dialog_draw(const GuiDialogDrawCfg *args);
struct GuiDialogCfg
{
    GuiDialog *dialog;
};

struct GuiDialogAskCfg
{
    GuiDialog *dialog;
    const char *title;
    const char *body;
    const char *const *labels;
    unsigned int count;
    unsigned int preferred;
    int error;
};

struct GuiDialogWantCfg
{
    const GuiDialog *dialog_read;
    GuiGlyphs *glyphs;
    int scale;
    unsigned int *width_pointer;
    unsigned int *height_pointer;
};

struct GuiDialogWidthCfg
{
    GuiDialog *dialog;
    GuiGlyphs *glyphs;
    unsigned int width;
    unsigned int height;
    int scale;
};

struct GuiDialogReadCfg
{
    const GuiDialog *dialog_read;
    unsigned int at;
};

struct GuiDialogKeyCfg
{
    GuiDialog *dialog;
    const char *name;
    GuiDialogChange *change;
};

struct GuiDialogPointCfg
{
    GuiDialog *dialog;
    int x;
    int y;
    GuiDialogChange *change;
};

struct GuiDialogDrawCfg
{
    const GuiDialog *dialog_read;
    GuiRaster *raster;
    GuiGlyphs *glyphs;
    const GuiPalette *palette;
    const GuiRaster *ground;
};

typedef struct
{
    void (*init)(const GuiDialogCfg *args);
} GuiDialogNs;
EMBED_TABLE_LAYOUT(GuiDialogNs, init);
EMBED_TABLE_STORAGE GuiDialogNs gui_dialog EMBED_UNUSED = {
    .init = gui_dialog_init,
};

typedef struct
{
    int (*ask)(const GuiDialogAskCfg *args);
} GuiDialogAskNs;
EMBED_TABLE_LAYOUT(GuiDialogAskNs, ask);
EMBED_TABLE_STORAGE GuiDialogAskNs gui_dialog_asking EMBED_UNUSED = {
    .ask = gui_dialog_ask,
};

typedef struct
{
    void (*want)(const GuiDialogWantCfg *args);
} GuiDialogWantNs;
EMBED_TABLE_LAYOUT(GuiDialogWantNs, want);
EMBED_TABLE_STORAGE GuiDialogWantNs gui_dialog_wanting EMBED_UNUSED = {
    .want = gui_dialog_want,
};

typedef struct
{
    void (*resize)(const GuiDialogWidthCfg *args);
} GuiDialogWidthNs;
EMBED_TABLE_LAYOUT(GuiDialogWidthNs, resize);
EMBED_TABLE_STORAGE GuiDialogWidthNs gui_dialog_width EMBED_UNUSED = {
    .resize = gui_dialog_resize,
};

typedef struct
{
    GuiRect (*head_rect)(const GuiDialogReadCfg *args);
    GuiRect (*close_rect)(const GuiDialogReadCfg *args);
    GuiRect (*body_rect)(const GuiDialogReadCfg *args);
    GuiRect (*button_rect)(const GuiDialogReadCfg *args);
} GuiDialogReadNs;
EMBED_TABLE_LAYOUT(GuiDialogReadNs, head_rect, close_rect, body_rect, button_rect);
EMBED_TABLE_STORAGE GuiDialogReadNs gui_dialog_read EMBED_UNUSED = {
    .head_rect = gui_dialog_head_rect,
    .close_rect = gui_dialog_close_rect,
    .body_rect = gui_dialog_body_rect,
    .button_rect = gui_dialog_button_rect,
};

typedef struct
{
    void (*key)(const GuiDialogKeyCfg *args);
} GuiDialogKeyNs;
EMBED_TABLE_LAYOUT(GuiDialogKeyNs, key);
EMBED_TABLE_STORAGE GuiDialogKeyNs gui_dialog_keys EMBED_UNUSED = {
    .key = gui_dialog_key,
};

typedef struct
{
    void (*move)(const GuiDialogPointCfg *args);
    void (*press)(const GuiDialogPointCfg *args);
    void (*release)(const GuiDialogPointCfg *args);
} GuiDialogPointNs;
EMBED_TABLE_LAYOUT(GuiDialogPointNs, move, press, release);
EMBED_TABLE_STORAGE GuiDialogPointNs gui_dialog_point EMBED_UNUSED = {
    .move = gui_dialog_move,
    .press = gui_dialog_press,
    .release = gui_dialog_release,
};

typedef struct
{
    void (*draw)(const GuiDialogDrawCfg *args);
} GuiDialogDrawNs;
EMBED_TABLE_LAYOUT(GuiDialogDrawNs, draw);
EMBED_TABLE_STORAGE GuiDialogDrawNs gui_dialog_drawing EMBED_UNUSED = {
    .draw = gui_dialog_draw,
};

#endif
