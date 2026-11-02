// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// palette.c: the colors the window draws in, each the value style.css gives its variable in the scheme, but for the
// bar's resting text, the large name and what the pointer lights, which stand on the luminance ladder under the rung of
// what they serve; a translucent color carries the stylesheet's alpha as a byte of 255
#include "palette.h"

#include "../command_line/cli_internal.h"
#include "../text/text.h"

#include <stddef.h>

// the dark scheme's tokens, --t-<name>
static const GuiTokenColor s_dark_tokens[] = {
    {"attribute", GUI_RGB(0xc9u, 0xb8u, 0xffu)},
    {"comment", GUI_RGB(0x6eu, 0x6bu, 0x99u)},
    {"delimiter", GUI_RGB(0x9eu, 0x9bu, 0xc4u)},
    {"directive", GUI_RGB(0xf0u, 0xa0u, 0x7au)},
    {"emphasis", GUI_RGB(0xc9u, 0xb8u, 0xffu)},
    {"function", GUI_RGB(0x7fu, 0xd8u, 0xe8u)},
    {"identifier", GUI_RGB(0x9eu, 0xa3u, 0xb5u)},
    {"keyword", GUI_RGB(0xa4u, 0x8cu, 0xffu)},
    {"module", GUI_RGB(0xa8u, 0xc8u, 0xffu)},
    {"number", GUI_RGB(0xf2u, 0xd4u, 0x8au)},
    {"number-cost", GUI_RGB(0x6fu, 0xdcu, 0xb4u)},
    {"operator", GUI_RGB(0xffu, 0xffu, 0xffu)},
    {"operator-memory", GUI_RGB(0xf2u, 0x9bu, 0xb0u)},
    {"parameter", GUI_RGB(0xf2u, 0xd4u, 0x8au)},
    {"predefined", GUI_RGB(0x8au, 0xb8u, 0xffu)},
    {"property", GUI_RGB(0xc9u, 0xb8u, 0xffu)},
    {"string", GUI_RGB(0x9fu, 0xe3u, 0xc8u)},
    {"strong", GUI_RGB(0xe4u, 0xe3u, 0xf5u)},
    {"tag", GUI_RGB(0x8au, 0xb8u, 0xffu)},
    {"type", GUI_RGB(0x6fu, 0xdcu, 0xb4u)},
    {"variable", GUI_RGB(0xa8u, 0xc8u, 0xffu)},
};

// the dark scheme: --bg, --surface, --well, --deep; --fg, --fg-2, --fg-3; --line; --signal, --link, --hover, --fail;
// --ink and --glass-tint, the ink at --bar-opacity; --bar-fg, --bar-title, --bar-title-hover, --bar-bright, --bar-path,
// --status-fg; --ed-bg at --editor-opacity, --ed-fg, --ed-num, --ed-current, --ed-sel, --ed-sel-idle, --ed-caret,
// --ed-guide, --ed-same,
// --ed-widget-line, --ed-match, --ed-match-on, --ed-mini-sel, --ed-mini-match, --ed-mini-slider, --ed-mini-slider-on,
// --plasma-dusk, --fuse-fire, --bar-signal at 18%, --ink at 50%, --ed-error, --ed-warning, --ed-note, --open-fg,
// --bar-mark, --lifted, the color .wordmark-under's rule makes, --deep at 80% with black and its OKLCH lightness by
// 1.15, --fuse-ash, --fuse-burn, --fuse-fire, the white the page's plasma lights its core with, --lattice-1 to
// --lattice-3, --t1 to --t8
static const GuiPalette s_dark = {
    GUI_RGB(0x13u, 0x13u, 0x31u),
    GUI_RGB(0x1cu, 0x19u, 0x40u),
    GUI_RGB(0x1bu, 0x1bu, 0x3du),
    GUI_RGB(0x0fu, 0x0du, 0x2eu),
    GUI_RGB(0xe4u, 0xe3u, 0xf5u),
    GUI_RGBA(0xe4u, 0xe3u, 0xf5u, 0xb3u),
    GUI_RGBA(0xe4u, 0xe3u, 0xf5u, 0x66u),
    GUI_RGBA(0xe4u, 0xe3u, 0xf5u, 0x1au),
    GUI_RGB(0x6fu, 0xdcu, 0xb4u),
    GUI_RGB(0x8au, 0xb8u, 0xffu),
    GUI_RGB(0xb4u, 0x9cu, 0xffu),
    GUI_RGB(0xffu, 0xa3u, 0xbdu),
    GUI_RGB(0x1du, 0x18u, 0x50u),
    GUI_RGBA(0x1du, 0x18u, 0x50u, 0x4du),
    GUI_RGB(0xd7u, 0xd6u, 0xdfu),
    GUI_RGBA(0xf3u, 0xf2u, 0xfbu, 0xc7u),
    GUI_RGBA(0xf3u, 0xf2u, 0xfbu, 0x1au),
    GUI_RGB(0xe5u, 0xe4u, 0xedu),
    GUI_RGBA(0xf3u, 0xf2u, 0xfbu, 0x99u),
    GUI_RGBA(0xf3u, 0xf2u, 0xfbu, 0xd1u),
    GUI_RGBA(0x13u, 0x13u, 0x31u, 0xccu),
    GUI_RGB(0xe4u, 0xe3u, 0xf5u),
    GUI_RGB(0x5eu, 0x5bu, 0x8au),
    GUI_RGB(0x1cu, 0x19u, 0x40u),
    GUI_RGB(0x3au, 0x33u, 0x80u),
    GUI_RGB(0x2au, 0x25u, 0x60u),
    GUI_RGB(0x6fu, 0xdcu, 0xb4u),
    GUI_RGB(0x24u, 0x21u, 0x4fu),
    GUI_RGBA(0xa4u, 0x8cu, 0xffu, 0x33u),
    GUI_RGB(0x2fu, 0x2au, 0x66u),
    GUI_RGBA(0x6fu, 0xdcu, 0xb4u, 0x38u),
    GUI_RGBA(0x6fu, 0xdcu, 0xb4u, 0x73u),
    GUI_RGBA(0x5au, 0x50u, 0xbeu, 0x8cu),
    GUI_RGBA(0x6fu, 0xdcu, 0xb4u, 0xb3u),
    GUI_RGBA(0xe4u, 0xe3u, 0xf5u, 0x14u),
    GUI_RGBA(0xe4u, 0xe3u, 0xf5u, 0x29u),
    GUI_RGB(0x32u, 0x12u, 0x62u),
    GUI_RGB(0x94u, 0x5eu, 0xe8u),
    GUI_RGBA(0x6fu, 0xdcu, 0xb4u, 0x2eu),
    GUI_RGBA(0x1du, 0x18u, 0x50u, 0x80u),
    GUI_RGB(0xffu, 0x6bu, 0x81u),
    GUI_RGB(0xe8u, 0xc4u, 0x6au),
    GUI_RGB(0x7au, 0xa2u, 0xf7u),
    GUI_RGB(0xd7u, 0xd6u, 0xdfu),
    GUI_RGB(0xdfu, 0xe0u, 0xf0u),
    GUI_RGBA(0x08u, 0x06u, 0x1eu, 0x73u),
    GUI_RGB(0x11u, 0x10u, 0x2bu),
    GUI_RGB(50u, 18u, 98u),
    GUI_RGB(94u, 40u, 166u),
    GUI_RGB(148u, 94u, 232u),
    GUI_RGB(199u, 163u, 255u),
    {GUI_RGB(0x6fu, 0xdcu, 0xb4u), GUI_RGB(0x8au, 0xb8u, 0xffu), GUI_RGB(0xb4u, 0x9cu, 0xffu)},
    {GUI_RGB(0xffu, 0x8fu, 0xa3u), GUI_RGB(0x6fu, 0xdcu, 0xb4u), GUI_RGB(0xf2u, 0xd4u, 0x8au),
     GUI_RGB(0x8au, 0xb8u, 0xffu), GUI_RGB(0xb4u, 0x9cu, 0xffu), GUI_RGB(0x7fu, 0xd8u, 0xe8u),
     GUI_RGB(0xd6u, 0xd4u, 0xeeu), GUI_RGB(0x5eu, 0x5bu, 0x8au)},
    s_dark_tokens,
    sizeof(s_dark_tokens) / sizeof(s_dark_tokens[0]),
};

const GuiPalette *gui_palette(const GuiPaletteCfg *args)
{
    (void)args;
    return &s_dark;
}

_Static_assert((sizeof(s_dark_tokens) / sizeof(s_dark_tokens[0])) <= GUI_PALETTE_TOKENS,
               "GUI_PALETTE_TOKENS holds the scheme's tokens");

void gui_palette_own_init(const GuiPaletteCfg *args)
{
    const GuiPalette *const from = cli_palette.palette(args);
    args->own->colors = *from;
    memor.cpy(&(MemoriaCfg){.dst = args->own->token,
                            .src = from->token,
                            .bytes = (unsigned long long)from->token_count * sizeof(GuiTokenColor)});
    args->own->colors.token = args->own->token;
}

void gui_palette_opacity_layout(const GuiPaletteOwnBarPercentCfg *args)
{
    // a percent of 255, rounded, is a byte
    args->own->colors.glass =
        (args->own->colors.text_color & 0x00ffffffu) | ((((args->bar_percent * 255u) + 50u) / 100u) << 24u);
    args->own->colors.editor_background =
        (args->own->colors.editor_background & 0x00ffffffu) | ((((args->editor_percent * 255u) + 50u) / 100u) << 24u);
}

// A color of the palette by name: its name, where it lies in the palette, and where a second field that is the same
// color lies, 0 for none, the ground lying first and never being a second field
typedef struct
{
    const char *name;
    unsigned long long offset;
    unsigned long long also;
} PaletteNamed;

// each color named for what it colors, in the form an editor's theme names a color: the fuse's fire also lines the
// shown tab, and a file icon's color goes by its hue, as a terminal's does
#define PALETTE_NAMED(name_, field_) {name_, offsetof(GuiPalette, field_), 0ull}
#define PALETTE_NAMED_AT(name_, field_, at_) {name_, offsetof(GuiPalette, field_) + ((at_) * sizeof(GuiColor)), 0ull}
static const PaletteNamed s_named[] = {
    PALETTE_NAMED("window.background", background_fill),
    PALETTE_NAMED("widget.background", surface),
    PALETTE_NAMED("widget.border", widget_line),
    PALETTE_NAMED("widget.shadow", lifted),
    PALETTE_NAMED("input.background", well),
    PALETTE_NAMED("text.primary", text),
    PALETTE_NAMED("text.secondary", text_2),
    PALETTE_NAMED("text.tertiary", text_3),
    PALETTE_NAMED("divider", line),
    PALETTE_NAMED("accent", signal),
    PALETTE_NAMED("link.foreground", link),
    PALETTE_NAMED("hover.foreground", hover),
    PALETTE_NAMED("failure.foreground", fail),
    PALETTE_NAMED("glass.tint", text_color),
    PALETTE_NAMED("titleBar.foreground", bar_text),
    PALETTE_NAMED("menubar.foreground", bar_title),
    PALETTE_NAMED("toolbar.hoverBackground", bar_title_hover),
    PALETTE_NAMED("toolbar.hoverForeground", bar_bright),
    PALETTE_NAMED("statusBar.foreground", status_text),
    PALETTE_NAMED("breadcrumb.separator", bar_path),
    PALETTE_NAMED("wordmark.foreground", open_text),
    PALETTE_NAMED("wordmark.eye", bar_mark),
    PALETTE_NAMED("tab.activeBackground", tab_background),
    PALETTE_NAMED("editor.background", editor_background),
    PALETTE_NAMED("editor.foreground", editor_text),
    PALETTE_NAMED("editor.lineNumber", number),
    PALETTE_NAMED("editor.lineHighlight", current),
    PALETTE_NAMED("editor.selection", selection),
    PALETTE_NAMED("editor.inactiveSelection", selection_idle),
    PALETTE_NAMED("editor.cursor", caret),
    PALETTE_NAMED("editor.indentGuide", guide),
    PALETTE_NAMED("editor.foldBackground", same),
    PALETTE_NAMED("editor.findMatch", match_on),
    PALETTE_NAMED("editor.findMatchHighlight", match),
    PALETTE_NAMED("minimap.selection", map_selection),
    PALETTE_NAMED("minimap.findMatch", map_match),
    PALETTE_NAMED("minimap.slider", map_slider),
    PALETTE_NAMED("minimap.sliderActive", map_slider_on),
    PALETTE_NAMED("diagnostic.error", error),
    PALETTE_NAMED("diagnostic.warning", warning),
    PALETTE_NAMED("diagnostic.information", note),
    PALETTE_NAMED("fuse.ash", fuse_ash),
    PALETTE_NAMED("fuse.burn", fuse_burn),
    {"fuse.fire", offsetof(GuiPalette, fuse_fire), offsetof(GuiPalette, tab_line)},
    PALETTE_NAMED("lattice.background", deep),
    PALETTE_NAMED_AT("lattice.inPlace", lattice, 0ull),
    PALETTE_NAMED_AT("lattice.moving", lattice, 1ull),
    PALETTE_NAMED_AT("lattice.displaced", lattice, 2ull),
    PALETTE_NAMED_AT("fileIcon.red", mark, 0ull),
    PALETTE_NAMED_AT("fileIcon.green", mark, 1ull),
    PALETTE_NAMED_AT("fileIcon.yellow", mark, 2ull),
    PALETTE_NAMED_AT("fileIcon.blue", mark, 3ull),
    PALETTE_NAMED_AT("fileIcon.violet", mark, 4ull),
    PALETTE_NAMED_AT("fileIcon.cyan", mark, 5ull),
    PALETTE_NAMED_AT("fileIcon.white", mark, 6ull),
    PALETTE_NAMED_AT("fileIcon.gray", mark, 7ull),
};
#define PALETTE_NAMED_COUNT ((unsigned int)(sizeof(s_named) / sizeof(s_named[0])))

unsigned int gui_palette_color_count(const GuiPaletteLengthCfg *args)
{
    return PALETTE_NAMED_COUNT + args->palette->token_count;
}

void gui_palette_color_name(const GuiPaletteAtCfg *args)
{
    VerbaLinea line = {args->name, args->size, 0ull};
    if (args->at < PALETTE_NAMED_COUNT)
    {
        mmgr_line_put(&line, s_named[args->at].name);
    }
    else
    {
        mmgr_line_put(&line, "token.");
        mmgr_line_put(&line, args->palette->token[args->at - PALETTE_NAMED_COUNT].name);
    }
    (void)mmgr_line_finish(&line);
}

typedef struct
{
    const GuiPalette *palette;
    unsigned long long offset;
    unsigned int value;
    GuiColor color;
    GuiColor foreground_color;
    unsigned long long back;
    unsigned int *tenths;
    char *under;
    unsigned long long size;
} PaletteCtx;
// the color lying at `offset` of the palette
EMBED_INLINE GuiColor palette_at(const PaletteCtx *args)
{
    return *(const GuiColor *)(const void *)((const unsigned char *)args->palette + args->offset);
}

GuiColor gui_palette_color_get(const GuiPaletteAtCfg *args)
{
    if (args->at < PALETTE_NAMED_COUNT)
    {
        return EMBED_CALL(palette_at, PaletteCtx, .palette = args->palette, .offset = s_named[args->at].offset);
    }
    return args->palette->token[args->at - PALETTE_NAMED_COUNT].color;
}

void gui_palette_color_set(const GuiPaletteOwnCfg *args)
{
    GuiColor color = args->color;
    if (args->at >= PALETTE_NAMED_COUNT)
    {
        args->own->token[args->at - PALETTE_NAMED_COUNT].color = color;
        return;
    }
    *(GuiColor *)(void *)((unsigned char *)&args->own->colors + s_named[args->at].offset) = color;
    if (s_named[args->at].also != 0ull)
    {
        *(GuiColor *)(void *)((unsigned char *)&args->own->colors + s_named[args->at].also) = color;
    }
    // the ink's tint is the ink at the bar's opacity
    if (s_named[args->at].offset == offsetof(GuiPalette, text_color))
    {
        args->own->colors.glass = (color & 0x00ffffffu) | (args->own->colors.glass & 0xff000000u);
    }
}

// the hex digit of a value from 0 to 15
EMBED_INLINE char palette_digit(const PaletteCtx *args)
{
    return "0123456789abcdef"[args->value & 0xfu];
}

void gui_palette_value_text(const GuiPaletteSizeColorCfg *args)
{
    VerbaLinea line = {args->text, args->size, 0ull};
    mmgr_line_ch(&line, '#');
    const unsigned int opaque = GUI_ALPHA_OF(args->color) == 0xffu;
    // red, green and blue, and the alpha where it is not opaque, the high digit of each first
    const unsigned int shifts[4] = {16u, 8u, 0u, 24u};
    for (unsigned int part = 0u; part < (opaque ? 3u : 4u); part += 1u)
    {
        const unsigned int byte = (args->color >> shifts[part]) & 0xffu;
        mmgr_line_ch(&line, EMBED_CALL(palette_digit, PaletteCtx, .value = byte >> 4u));
        mmgr_line_ch(&line, EMBED_CALL(palette_digit, PaletteCtx, .value = byte));
    }
    (void)mmgr_line_finish(&line);
}

int gui_palette_value_read(const GuiPaletteTextCfg *args)
{
    GuiColor *color = args->color_pointer;
    const unsigned long long skip = ((args->length != 0ull) && (args->text_read[0] == '#')) ? 1ull : 0ull;
    const unsigned long long digits = args->length - skip;
    if ((digits != 6ull) && (digits != 8ull))
    {
        return 0;
    }
    unsigned int value = 0u;
    for (unsigned long long at = 0ull; at < digits; at += 1ull)
    {
        const char digit = args->text_read[skip + at];
        unsigned int read = 16u;
        if ((digit >= '0') && (digit <= '9'))
        {
            // the difference of two digits is far inside what an unsigned int represents
            read = (unsigned int)(digit - '0');
        }
        else if ((digit >= 'a') && (digit <= 'f'))
        {
            read = 10u + (unsigned int)(digit - 'a');
        }
        else if ((digit >= 'A') && (digit <= 'F'))
        {
            read = 10u + (unsigned int)(digit - 'A');
        }
        if (read == 16u)
        {
            return 0;
        }
        value = (value << 4u) | read;
    }
    // six digits are red, green and blue under the current alpha; eight end in their own alpha
    *color = (digits == 6ull) ? (value | (args->current & 0xff000000u)) : ((value >> 8u) | (value << 24u));
    return 1;
}

// a channel of sRGB from 0 to 255 as its linear light from 0 to 65535, as WCAG 2 takes it
static const unsigned short s_linear[256] = {
    0u,     20u,    40u,    60u,    80u,    99u,    119u,   139u,   159u,   179u,   199u,   219u,   241u,   264u,
    288u,   313u,   340u,   367u,   396u,   427u,   458u,   491u,   526u,   562u,   599u,   637u,   677u,   718u,
    761u,   805u,   851u,   898u,   947u,   997u,   1048u,  1101u,  1156u,  1212u,  1270u,  1330u,  1391u,  1453u,
    1517u,  1583u,  1651u,  1720u,  1790u,  1863u,  1937u,  2013u,  2090u,  2170u,  2250u,  2333u,  2418u,  2504u,
    2592u,  2681u,  2773u,  2866u,  2961u,  3058u,  3157u,  3258u,  3360u,  3464u,  3570u,  3678u,  3788u,  3900u,
    4014u,  4129u,  4247u,  4366u,  4488u,  4611u,  4736u,  4864u,  4993u,  5124u,  5257u,  5392u,  5530u,  5669u,
    5810u,  5953u,  6099u,  6246u,  6395u,  6547u,  6700u,  6856u,  7014u,  7174u,  7335u,  7500u,  7666u,  7834u,
    8004u,  8177u,  8352u,  8528u,  8708u,  8889u,  9072u,  9258u,  9445u,  9635u,  9828u,  10022u, 10219u, 10417u,
    10619u, 10822u, 11028u, 11235u, 11446u, 11658u, 11873u, 12090u, 12309u, 12530u, 12754u, 12980u, 13209u, 13440u,
    13673u, 13909u, 14146u, 14387u, 14629u, 14874u, 15122u, 15371u, 15623u, 15878u, 16135u, 16394u, 16656u, 16920u,
    17187u, 17456u, 17727u, 18001u, 18277u, 18556u, 18837u, 19121u, 19407u, 19696u, 19987u, 20281u, 20577u, 20876u,
    21177u, 21481u, 21787u, 22096u, 22407u, 22721u, 23038u, 23357u, 23678u, 24002u, 24329u, 24658u, 24990u, 25325u,
    25662u, 26001u, 26344u, 26688u, 27036u, 27386u, 27739u, 28094u, 28452u, 28813u, 29176u, 29542u, 29911u, 30282u,
    30656u, 31033u, 31412u, 31794u, 32179u, 32567u, 32957u, 33350u, 33745u, 34143u, 34544u, 34948u, 35355u, 35764u,
    36176u, 36591u, 37008u, 37429u, 37852u, 38278u, 38706u, 39138u, 39572u, 40009u, 40449u, 40891u, 41337u, 41785u,
    42236u, 42690u, 43147u, 43606u, 44069u, 44534u, 45002u, 45473u, 45947u, 46423u, 46903u, 47385u, 47871u, 48359u,
    48850u, 49344u, 49841u, 50341u, 50844u, 51349u, 51858u, 52369u, 52884u, 53401u, 53921u, 54445u, 54971u, 55500u,
    56032u, 56567u, 57105u, 57646u, 58190u, 58737u, 59287u, 59840u, 60396u, 60955u, 61517u, 62082u, 62650u, 63221u,
    63795u, 64372u, 64952u, 65535u,
};

// a color's light as WCAG 2 weighs it and adds its flare of a twentieth, on a scale of twenty times the linear table's:
// red, green and blue by their weights in ten-thousandths, the flare whole at that scale, the alpha passed over
EMBED_INLINE unsigned int palette_light(const PaletteCtx *args)
{
    const unsigned int weighed = (2126u * s_linear[GUI_RED_OF(args->color)]) +
                                 (7152u * s_linear[GUI_GREEN_OF(args->color)]) +
                                 (722u * s_linear[GUI_BLUE_OF(args->color)]);
    return ((weighed / 10000u) * 20u) + 65535u;
}

unsigned int gui_palette_contrast(const GuiPaletteForegroundCfg *args)
{
    const GuiColor under =
        EMBED_CALL(cli_raster.color_over, GuiRasterCfg, .background_color = args->background_fill, .color = args->back);
    const unsigned int foreground_light =
        EMBED_CALL(palette_light, PaletteCtx,
                   .color = EMBED_CALL(cli_raster.color_over, GuiRasterCfg, .background_color = under,
                                       .color = args->foreground_color));
    const unsigned int under_light = EMBED_CALL(palette_light, PaletteCtx, .color = under);
    return (foreground_light > under_light) ? ((foreground_light * 10u) / under_light)
                                            : ((under_light * 10u) / foreground_light);
}

// A color of text whose contrast is checked and a color it is drawn on, each where it lies in the palette: a token is
// drawn on the editor's background
typedef struct
{
    unsigned long long foreground_color;
    unsigned long long back;
} PaletteContrast;

static const PaletteContrast s_contrasts[] = {
    {offsetof(GuiPalette, editor_text), offsetof(GuiPalette, editor_background)},
    {offsetof(GuiPalette, number), offsetof(GuiPalette, editor_background)},
    {offsetof(GuiPalette, text), offsetof(GuiPalette, background_fill)},
    {offsetof(GuiPalette, text_2), offsetof(GuiPalette, background_fill)},
    {offsetof(GuiPalette, text), offsetof(GuiPalette, surface)},
    {offsetof(GuiPalette, link), offsetof(GuiPalette, background_fill)},
};

// whether `foreground_color` is under the least over the color lying at `back`: 1, with its contrast and that color's
// name
EMBED_INLINE int palette_low(const PaletteCtx *args)
{
    GuiColor foreground_color = args->foreground_color;
    unsigned long long back = args->back;
    unsigned int *tenths = args->tenths;
    const unsigned int contrast =
        EMBED_CALL(gui_palette_foreground.contrast, GuiPaletteForegroundCfg, .foreground_color = foreground_color,
                   .back = EMBED_CALL(palette_at, PaletteCtx, .palette = args->palette, .offset = back),
                   .background_fill = args->palette->background_fill);
    if (contrast >= GUI_PALETTE_CONTRAST_MIN)
    {
        return 0;
    }
    *tenths = contrast;
    for (unsigned int at = 0u; at < PALETTE_NAMED_COUNT; at += 1u)
    {
        if (s_named[at].offset == back)
        {
            VerbaLinea line = {args->under, args->size, 0ull};
            mmgr_line_put(&line, s_named[at].name);
            (void)mmgr_line_finish(&line);
        }
    }
    return 1;
}

int gui_palette_color_low(const GuiPaletteAtSizeCfg *args)
{
    if (args->at >= PALETTE_NAMED_COUNT)
    {
        return EMBED_CALL(palette_low, PaletteCtx, .palette = args->palette,
                          .foreground_color = args->palette->token[args->at - PALETTE_NAMED_COUNT].color,
                          .back = offsetof(GuiPalette, editor_background), .tenths = args->tenths, .under = args->under,
                          .size = args->size);
    }
    for (unsigned int pair = 0u; pair < (unsigned int)(sizeof(s_contrasts) / sizeof(s_contrasts[0])); pair += 1u)
    {
        if ((s_contrasts[pair].foreground_color == s_named[args->at].offset) &&
            EMBED_CALL(palette_low, PaletteCtx, .palette = args->palette,
                       .foreground_color = EMBED_CALL(palette_at, PaletteCtx, .palette = args->palette,
                                                      .offset = s_named[args->at].offset),
                       .back = s_contrasts[pair].back, .tenths = args->tenths, .under = args->under,
                       .size = args->size))
        {
            return 1;
        }
    }
    return 0;
}

void gui_palette_contrast_text(const GuiPaletteSizeCfg *args)
{
    VerbaLinea line = {args->text, args->size, 0ull};
    mmgr_line_decimal(&line, args->tenths_value / 10u, 1u);
    mmgr_line_ch(&line, '.');
    mmgr_line_decimal(&line, args->tenths_value % 10u, 1u);
    mmgr_line_put(&line, ":1");
    (void)mmgr_line_finish(&line);
}

GuiColor gui_palette_token(const GuiPaletteLengthCfg *args)
{
    for (unsigned int at = 0u; at < args->palette->token_count; at += 1u)
    {
        const char *const known = args->palette->token[at].name;
        if ((cellul.len(&(CatenaFinitaCfg){.src = known, .cap = CLI_CELLS_POOL}) == args->length) &&
            (memor.cmp(&(MemoriaCfg){.src = known, .other = args->name_read, .bytes = args->length}) == 0))
        {
            return args->palette->token[at].color;
        }
    }
    return args->palette->editor_text;
}
