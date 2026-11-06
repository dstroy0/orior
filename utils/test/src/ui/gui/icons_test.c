// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// icons_test.c: the icons' check: an icon is drawn on a square of pixels as its path says, a line's pixels covered and
// the rest clear, a square's middle clear, the filled dots covered, an arc a ring, a curve of relative steps reaching
// its end, the cog's teeth at its rim; an icon asked again is the one saved; and a name no icon has draws nothing
#include "../../../../../ui/cli/src/view/icons.h"
#include "../cli/cli_check.h"

// the cover of the pixel at x, y of a mask of `size` a side
static unsigned int at(const GuiMask *mask, unsigned int x, unsigned int y)
{
    return (mask != NULL) ? mask->cover[(y * mask->width) + x] : 0u;
}

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    GuiIcons icons;
    EMBED_CALL(cli_icons.init, GuiIconsCfg, .icons = &icons);
    cli_check(EMBED_CALL(cli_icons.icon_by_name, GuiIconsCfg, .name = "folder") && EMBED_CALL(cli_icons.icon_by_name, GuiIconsCfg, .name = "gear") && !EMBED_CALL(cli_icons.icon_by_name, GuiIconsCfg, .name = "nothing"),
              "the icons are known by their names");
    cli_check(EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = &icons, .name = "nothing", .size = 16u, .stroke = 2.0f) == NULL, "a name no icon has draws nothing");

    const GuiMask *const line = EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = &icons, .name = "minimize", .size = 16u, .stroke = 2.0f);
    cli_check((line != NULL) && (line->width == 16u) && (line->height == 16u) && (line->left == 0) && (line->top == 0),
              "an icon is drawn on a square of its size");
    cli_check((at(line, 8u, 7u) == 255u) && (at(line, 8u, 8u) == 255u), "a line two pixels wide covers the two rows it runs on");
    cli_check((at(line, 8u, 5u) == 0u) && (at(line, 8u, 10u) == 0u), "the rows away from it are clear");
    cli_check((at(line, 0u, 7u) == 0u) && (at(line, 15u, 7u) == 0u), "the pixels past its round ends are clear");
    cli_check((at(line, 2u, 7u) > 60u) && (at(line, 2u, 7u) < 200u), "its round end covers the pixel it ends in by a share");
    cli_check(EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = &icons, .name = "minimize", .size = 16u, .stroke = 2.0f) == line, "the icon asked again is the one kept");
    cli_check(EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = &icons, .name = "minimize", .size = 32u, .stroke = 2.0f) != line, "the icon at another size is drawn anew");

    const GuiMask *const square = EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = &icons, .name = "maximize", .size = 16u, .stroke = 2.0f);
    cli_check((at(square, 8u, 8u) == 0u) && (at(square, 4u, 8u) == 255u) && (at(square, 8u, 11u) == 255u),
              "a square's middle is clear and its sides are covered");

    // the eye on its own box: its strokes drawn, its iris filled whole at its middle, and the square's corners clear
    const GuiMask *const logo = EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = &icons, .name = "eye", .size = 64u, .stroke = 1.0f);
    cli_check((logo != NULL) && (logo->width == 64u), "the eye is drawn on its square");
    cli_check(at(logo, 31u, 43u) > 200u, "the iris is filled at its middle");
    cli_check((at(logo, 0u, 0u) == 0u) && (at(logo, 63u, 63u) == 0u), "the square's corners outside the eye are clear");
    unsigned int covered = 0u;
    for (unsigned int y = 0u; y < 64u; y += 1u)
    {
        for (unsigned int x = 0u; x < 64u; x += 1u)
        {
            covered += (at(logo, x, y) != 0u) ? 1u : 0u;
        }
    }
    cli_check((covered > 300u) && (covered < 3000u), "the eye's strokes cover a part of the square and no more");

    const GuiMask *const dots = EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = &icons, .name = "more", .size = 16u, .stroke = 2.0f);
    cli_check((at(dots, 8u, 8u) > 200u) && (at(dots, 3u, 8u) > 200u) && (at(dots, 12u, 8u) > 200u) && (at(dots, 5u, 8u) == 0u),
              "the filled dots are covered and the gaps between them clear");

    const GuiMask *const ring = EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = &icons, .name = "commit", .size = 16u, .stroke = 2.0f);
    cli_check((at(ring, 8u, 8u) == 0u) && (at(ring, 8u, 5u) > 200u) && (at(ring, 10u, 8u) > 200u) && (at(ring, 2u, 7u) > 200u),
              "an arc draws a ring, clear inside, with the lines each side");

    const GuiMask *const wave = EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = &icons, .name = "sim", .size = 16u, .stroke = 2.0f);
    cli_check((at(wave, 1u, 8u) > 200u) && (at(wave, 14u, 6u) > 200u), "a curve of relative steps runs from its start to its end");

    const GuiMask *const cog = EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = &icons, .name = "gear", .size = 16u, .stroke = 2.0f);
    cli_check((at(cog, 14u, 8u) > 200u) && (at(cog, 8u, 8u) < 40u) && (at(cog, 0u, 0u) == 0u),
              "the cog's tooth stands at its rim, its hub about its middle, and its corner is clear");

    const GuiMask *const large = EMBED_CALL(cli_icons.icon, GuiIconsCfg, .icons = &icons, .name = "folder", .size = 29u, .stroke = 2.0f);
    cli_check((large != NULL) && (large->width == 29u) && (at(large, 14u, 23u) > 200u) && (at(large, 14u, 2u) == 0u),
              "an icon at the strip's size covers its folder's foot and not the space above it");
    for (unsigned int at = 0u; at < GUI_ICON_CACHE_ENTRIES; at += 1u)
    {
        if (icons.entry[at].used)
        {
            g_cli_guard->persistent_buf_release(icons.entry[at].mask_bits.cover, g_gui_icons_owner);
            icons.entry[at].mask_bits.cover = NULL;
            icons.entry[at].used = 0;
        }
    }
    icons.count = 0u;

    // a shape of straight sides filled on a cover of its own width and height: covered by each pixel's share inside it,
    // an overlap of the shape's outline with itself inside it as SVG's nonzero fill has it
    float cover[6u * 4u] = {0.0f};
    const float box[8] = {1.0f, 1.0f, 3.5f, 1.0f, 3.5f, 3.0f, 1.0f, 3.0f};
    EMBED_CALL(gui_icons_cover.polygon_fill, GuiIconsCoverCfg, .cover = cover, .width = 6u, .height = 4u, .corner = box, .corners = 4u);
    cli_check((cover[(1u * 6u) + 1u] == 1.0f) && (cover[(2u * 6u) + 2u] == 1.0f), "a pixel inside the shape is covered whole");
    cli_check((cover[(1u * 6u) + 3u] > 0.4f) && (cover[(1u * 6u) + 3u] < 0.6f), "a pixel its side halves is covered by half");
    cli_check((cover[(1u * 6u) + 4u] == 0.0f) && (cover[0] == 0.0f) && (cover[(3u * 6u) + 1u] == 0.0f), "a pixel outside is clear");
    float folded[6u * 4u] = {0.0f};
    const float twice[16] = {0.0f, 0.0f, 4.0f, 0.0f, 4.0f, 4.0f, 0.0f, 4.0f, 0.0f, 0.0f, 4.0f, 0.0f, 4.0f, 4.0f, 0.0f, 4.0f};
    EMBED_CALL(gui_icons_cover.polygon_fill, GuiIconsCoverCfg, .cover = folded, .width = 6u, .height = 4u, .corner = twice, .corners = 8u);
    cli_check(folded[(1u * 6u) + 1u] == 1.0f, "an outline that goes round twice is inside, not out");
    float offset[6u * 4u] = {0.0f};
    const float end[8] = {4.0f, 2.0f, 9.0f, 2.0f, 9.0f, 9.0f, 4.0f, 9.0f};
    EMBED_CALL(gui_icons_cover.polygon_fill, GuiIconsCoverCfg, .cover = offset, .width = 6u, .height = 4u, .corner = end, .corners = 4u);
    cli_check((offset[(3u * 6u) + 5u] == 1.0f) && (offset[(3u * 6u) + 3u] == 0.0f), "a shape past the cover fills what lies on it");
    return cli_on_check();
}
