// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// themes_test.c: the themes' check: the entry's list read and written back the same, a theme found and a free name
// made, one added from another, renamed and taken out, its colors set and put back, its icons set, taken off and
// matched to a file's name, and one theme shared as text and read back
#include "../../../../../ui/cli/src/view/themes.h"
#include "../../../../../ui/cli/src/command_line/owners.h"
#include "../cli/cli_check.h"

static GuiThemes s_themes;
static GuiThemes s_again;

// the list `[Night, Day]`: Night sets two colors and gives two icons, Day sets one color, and a light theme and a
// color that does not read are left out
static const char s_list[] =
    "[{\"name\":\"Night\",\"base\":\"dark\",\"colors\":{\"accent\":\"#112233\",\"text.primary\":\"#aabbccdd\","
    "\"divider\":\"blue\"},\"icons\":[{\"pattern\":\"*.c\",\"mark\":\"C\",\"color\":\"accent\"},"
    "{\"pattern\":\"Make*\",\"mark\":\"mk\",\"color\":\"\"},{\"mark\":\"x\"}]},"
    "{\"name\":\"Noon\",\"base\":\"light\",\"colors\":{}},"
    "{\"name\":\"Day\",\"base\":\"dark\",\"colors\":{\"accent\":\"#445566\"}}]";

int main(void)
{
    (void)cli_pool_made(CLI_POOL_BYTES);
    EMBED_CALL(gui_themes_text_table.read, GuiThemesTextCfg, .themes = &s_themes, .text = s_list,
               .size = sizeof(s_list) - 1ULL, .owner = g_cli_view_owner);
    cli_check(s_themes.count == 2u, "the two dark themes are read and the light one is left out");
    cli_check((s_themes.theme[0].colors == 2u) && (s_themes.theme[0].color[0].color == GUI_RGB(0x11u, 0x22u, 0x33u)) &&
                  (s_themes.theme[0].color[1].color == GUI_RGBA(0xaau, 0xbbu, 0xccu, 0xddu)),
              "a theme's colors read with their alpha, and a value that is no color is left out");
    cli_check((s_themes.theme[0].icons == 2u) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = s_themes.theme[0].icon[1].mark, .other = "mk", .cap = 8ULL}) &&
                  (s_themes.theme[0].icon[1].color[0] == '\0'),
              "the icons with a pattern are read, the one without left out");
    cli_check(EMBED_CALL(gui_themes_name.find, GuiThemesNameCfg, .themes_read = &s_themes, .name = "Day") == 1u,
              "a theme is found by its name");
    cli_check(EMBED_CALL(gui_themes_name.find, GuiThemesNameCfg, .themes_read = &s_themes, .name = "Dusk") == 2u,
              "a name no theme has finds the count");

    char text[4096];
    VerbaLinea out = {text, sizeof(text), 0ULL};
    EMBED_CALL(gui_themes_out.text, GuiThemesOutCfg, .themes_read = &s_themes, .out = &out);
    const unsigned long long written = mmgr_line_finish(&out);
    EMBED_CALL(gui_themes_text_table.read, GuiThemesTextCfg, .themes = &s_again, .text = text, .size = written,
               .owner = g_cli_view_owner);
    cli_check((s_again.count == 2u) && (s_again.theme[0].colors == 2u) && (s_again.theme[0].icons == 2u) &&
                  (s_again.theme[1].color[0].color == GUI_RGB(0x44u, 0x55u, 0x66u)) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = s_again.theme[0].icon[0].pattern, .other = "*.c", .cap = 8ULL}),
              "the list written reads back the same");

    char name[GUI_THEME_NAME];
    EMBED_CALL(gui_themes_name.free_name, GuiThemesNameCfg, .themes_read = &s_themes, .name = "Dusk", .name_out = name);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Dusk", .cap = sizeof(name)}),
              "a name no theme has is free as it is");
    EMBED_CALL(gui_themes_name.free_name, GuiThemesNameCfg, .themes_read = &s_themes, .name = "Night", .name_out = name);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Night 2", .cap = sizeof(name)}),
              "a taken name takes a number");
    EMBED_CALL(gui_themes_name.free_name, GuiThemesNameCfg, .themes_read = &s_themes, .name = GUI_THEME_BASE_NAME,
               .name_out = name);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Dark 2", .cap = sizeof(name)}),
              "the scheme's own name is taken too");
    const unsigned int added =
        EMBED_CALL(gui_themes_at_name.add, GuiThemesAtNameCfg, .themes = &s_themes, .from = 0u, .name = name);
    cli_check((added == 2u) && (s_themes.count == 3u) && (s_themes.theme[2].colors == 2u) &&
                  (s_themes.theme[2].icons == 2u) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = s_themes.theme[2].name, .other = "Dark 2", .cap = 8ULL}),
              "a theme added from another takes its colors and icons under the new name");
    EMBED_CALL(gui_themes_name.free_name, GuiThemesNameCfg, .themes_read = &s_themes, .name = "Dark 2", .name_out = name);
    cli_check(cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "Dark 3", .cap = sizeof(name)}),
              "a numbered name taken counts on from its stem");
    EMBED_CALL(gui_themes_at_name.rename, GuiThemesAtNameCfg, .themes = &s_themes, .at = 2u, .name = "Dusk");
    cli_check(EMBED_CALL(gui_themes_name.find, GuiThemesNameCfg, .themes_read = &s_themes, .name = "Dusk") == 2u,
              "a theme renamed is found by the new name");

    GuiColor color = 0u;
    EMBED_CALL(gui_themes_at_color.color_set, GuiThemesAtColorCfg, .themes = &s_themes, .at = 2u, .name = "divider",
               .color = GUI_RGB(1u, 2u, 3u), .on = 1);
    cli_check((s_themes.theme[2].colors == 3u) &&
                  EMBED_CALL(gui_themes_at_color.color_get, GuiThemesAtColorCfg, .themes = &s_themes, .at = 2u,
                             .name = "divider", .color_pointer = &color) &&
                  (color == GUI_RGB(1u, 2u, 3u)),
              "a color set is read back");
    EMBED_CALL(gui_themes_at_color.color_set, GuiThemesAtColorCfg, .themes = &s_themes, .at = 2u, .name = "accent",
               .color = GUI_RGB(9u, 9u, 9u), .on = 1);
    cli_check((s_themes.theme[2].colors == 3u) &&
                  EMBED_CALL(gui_themes_at_color.color_get, GuiThemesAtColorCfg, .themes = &s_themes, .at = 2u,
                             .name = "accent", .color_pointer = &color) &&
                  (color == GUI_RGB(9u, 9u, 9u)),
              "a color set again takes the place it had");
    EMBED_CALL(gui_themes_at_color.color_set, GuiThemesAtColorCfg, .themes = &s_themes, .at = 2u, .name = "accent",
               .on = 0);
    cli_check((s_themes.theme[2].colors == 2u) &&
                  !EMBED_CALL(gui_themes_at_color.color_get, GuiThemesAtColorCfg, .themes = &s_themes, .at = 2u,
                              .name = "accent", .color_pointer = &color),
              "a color put back is no longer set");

    EMBED_CALL(gui_themes_at_icon.icon_set, GuiThemesAtIconCfg, .themes = &s_themes, .at = 2u, .pattern = "*.h",
               .mark = "H", .color = "#ff0000");
    cli_check(s_themes.theme[2].icons == 3u, "an icon of a new pattern is added");
    EMBED_CALL(gui_themes_at_icon.icon_set, GuiThemesAtIconCfg, .themes = &s_themes, .at = 2u, .pattern = "*.c",
               .mark = "CC", .color = "");
    cli_check((s_themes.theme[2].icons == 3u) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = s_themes.theme[2].icon[0].mark, .other = "CC", .cap = 8ULL}),
              "an icon of a pattern set again takes its place");
    const GuiThemeIcon *icon = EMBED_CALL(gui_theme_name.icon_match, GuiThemeNameCfg, .theme_read = &s_themes.theme[2],
                                          .name = "window.H", .length = 8ULL);
    cli_check((icon != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = icon->mark, .other = "H", .cap = 8ULL}),
              "a pattern covers a name without case");
    icon = EMBED_CALL(gui_theme_name.icon_match, GuiThemeNameCfg, .theme_read = &s_themes.theme[2],
                      .name = "makefile", .length = 8ULL);
    cli_check((icon != NULL) && cellul.eq(&(CatenaFinitaCfg){.src = icon->mark, .other = "mk", .cap = 8ULL}),
              "a star at the end covers the rest of the name");
    icon = EMBED_CALL(gui_theme_name.icon_match, GuiThemeNameCfg, .theme_read = &s_themes.theme[2], .name = "a.cc",
                      .length = 4ULL);
    cli_check(icon == NULL, "a name no pattern covers has no icon");
    EMBED_CALL(gui_themes_at_icon.icon_remove, GuiThemesAtIconCfg, .themes = &s_themes, .at = 2u, .pattern = "*.c");
    cli_check((s_themes.theme[2].icons == 2u) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = s_themes.theme[2].icon[0].pattern, .other = "Make*", .cap = 8ULL}),
              "an icon taken off closes the gap");

    out = (VerbaLinea){text, sizeof(text), 0ULL};
    EMBED_CALL(gui_theme_out.text, GuiThemeOutCfg, .theme_read = &s_themes.theme[2], .out = &out);
    const unsigned long long shared = mmgr_line_finish(&out);
    cli_check((shared != 0ULL) && (text[0] == '{') && (text[shared - 1ULL] == '\n') &&
                  cellul.has(&(CatenaFinitaCfg){.src = text, .other = "\n  \"name\": \"Dusk\",", .cap = sizeof(text), .other_cap = 19u}),
              "a theme shared is pretty text, one member a line");
    GuiTheme read;
    cli_check(EMBED_CALL(gui_theme_text_table.read, GuiThemeTextCfg, .text = text, .size = shared, .theme_out = &read,
                         .owner = g_cli_view_owner) &&
                  (read.colors == 2u) && (read.icons == 2u) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = read.name, .other = "Dusk", .cap = sizeof(read.name)}),
              "the text shared reads back as the theme");
    static const char s_light[] = "{\"name\":\"Noon\",\"base\":\"light\",\"colors\":{}}";
    cli_check(!EMBED_CALL(gui_theme_text_table.read, GuiThemeTextCfg, .text = s_light, .size = sizeof(s_light) - 1ULL,
                          .theme_out = &read, .owner = g_cli_view_owner),
              "a theme of another scheme does not read");
    static const char s_words[] = "some words";
    cli_check(!EMBED_CALL(gui_theme_text_table.read, GuiThemeTextCfg, .text = s_words, .size = sizeof(s_words) - 1ULL,
                          .theme_out = &read, .owner = g_cli_view_owner),
              "text that is no JSON does not read");

    EMBED_CALL(gui_themes_at.remove, GuiThemesAtCfg, .themes = &s_themes, .at = 0u);
    cli_check((s_themes.count == 2u) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = s_themes.theme[0].name, .other = "Day", .cap = 8ULL}) &&
                  cellul.eq(&(CatenaFinitaCfg){.src = s_themes.theme[1].name, .other = "Dusk", .cap = 8ULL}),
              "a theme taken out closes the gap");
    return cli_on_check();
}
