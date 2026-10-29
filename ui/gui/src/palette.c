// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// palette.c: the colors the window draws in, each the value style.css gives its variable in the scheme; a translucent
// color carries the stylesheet's alpha as a byte of 255
#include "palette.h"

// the dark scheme's tokens, --t-<name>
static const GuiTokenColor s_dark_tokens[] = {
    {"attribute", GUI_RGB(0xc9u, 0xb8u, 0xffu)},       {"comment", GUI_RGB(0x6eu, 0x6bu, 0x99u)},
    {"delimiter", GUI_RGB(0x9eu, 0x9bu, 0xc4u)},       {"directive", GUI_RGB(0xf0u, 0xa0u, 0x7au)},
    {"emphasis", GUI_RGB(0xc9u, 0xb8u, 0xffu)},        {"function", GUI_RGB(0x7fu, 0xd8u, 0xe8u)},
    {"identifier", GUI_RGB(0x9eu, 0xa3u, 0xb5u)},      {"keyword", GUI_RGB(0xa4u, 0x8cu, 0xffu)},
    {"module", GUI_RGB(0xa8u, 0xc8u, 0xffu)},          {"number", GUI_RGB(0xf2u, 0xd4u, 0x8au)},
    {"number-cost", GUI_RGB(0x6fu, 0xdcu, 0xb4u)},     {"operator", GUI_RGB(0xffu, 0xffu, 0xffu)},
    {"operator-memory", GUI_RGB(0xf2u, 0x9bu, 0xb0u)}, {"parameter", GUI_RGB(0xf2u, 0xd4u, 0x8au)},
    {"predefined", GUI_RGB(0x8au, 0xb8u, 0xffu)},      {"property", GUI_RGB(0xc9u, 0xb8u, 0xffu)},
    {"string", GUI_RGB(0x9fu, 0xe3u, 0xc8u)},          {"strong", GUI_RGB(0xe4u, 0xe3u, 0xf5u)},
    {"tag", GUI_RGB(0x8au, 0xb8u, 0xffu)},             {"type", GUI_RGB(0x6fu, 0xdcu, 0xb4u)},
    {"variable", GUI_RGB(0xa8u, 0xc8u, 0xffu)},
};

// the dark scheme: --bg, --surface, --well, --deep; --fg, --fg-2, --fg-3; --line; --signal, --link, --hover, --fail;
// --ink and --glass-tint, the ink at --bar-opacity; --bar-fg, --bar-title, --bar-title-hover, --bar-bright, --bar-path,
// --status-fg; --ed-bg, --ed-fg, --ed-num, --ed-current, --ed-sel, --ed-sel-idle, --ed-caret, --ed-guide, --ed-same,
// --ed-widget-line, --ed-match, --ed-match-on, --ed-mini-sel, --ed-mini-match, --ed-mini-slider, --ed-mini-slider-on,
// --plasma-dusk, --fuse-fire, --bar-signal at 18%, --ink at 50%, --ed-error, --ed-warning, --ed-note, --t1 to --t8
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
    GUI_RGB(0xf3u, 0xf2u, 0xfbu),
    GUI_RGBA(0xf3u, 0xf2u, 0xfbu, 0xc7u),
    GUI_RGBA(0xf3u, 0xf2u, 0xfbu, 0x1au),
    GUI_RGB(0xffu, 0xffu, 0xffu),
    GUI_RGBA(0xf3u, 0xf2u, 0xfbu, 0x99u),
    GUI_RGBA(0xf3u, 0xf2u, 0xfbu, 0xd1u),
    GUI_RGB(0x13u, 0x13u, 0x31u),
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
    {GUI_RGB(0xffu, 0x8fu, 0xa3u), GUI_RGB(0x6fu, 0xdcu, 0xb4u), GUI_RGB(0xf2u, 0xd4u, 0x8au), GUI_RGB(0x8au, 0xb8u, 0xffu),
     GUI_RGB(0xb4u, 0x9cu, 0xffu), GUI_RGB(0x7fu, 0xd8u, 0xe8u), GUI_RGB(0xd6u, 0xd4u, 0xeeu), GUI_RGB(0x5eu, 0x5bu, 0x8au)},
    s_dark_tokens,
    sizeof(s_dark_tokens) / sizeof(s_dark_tokens[0]),
};

const GuiPalette *gui_palette(unsigned int scheme)
{
    (void)scheme;
    return &s_dark;
}

GuiColor gui_palette_token(const GuiPalette *palette, const char *name, unsigned long long length)
{
    for (unsigned int at = 0u; at < palette->token_count; at += 1u)
    {
        const char *const known = palette->token[at].name;
        unsigned long long same = 0ull;
        while ((same < length) && (known[same] != '\0') && (known[same] == name[same]))
        {
            same += 1ull;
        }
        if ((same == length) && (known[same] == '\0'))
        {
            return palette->token[at].color;
        }
    }
    return palette->editor_text;
}
