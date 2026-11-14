// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// themes.c: the reader's color themes read from the state store's entry and written back to it, found, added, renamed
// and taken out, their colors and icons set, an icon matched to a file's name, and one theme shared as text. The
// themes stand as records in one movable cell their holder owns, each read into a record of the call that reads it
#include "themes.h"

#include "../../../includes/window/palette.h"
#include "../../cli/src/command_line/cli_internal.h"
#include "../../cli/src/text/json_tree.h"

// the scheme every theme is drawn over, as the entry names it
static const char s_base[] = "dark";

typedef struct
{
    const CliJson *json;
    unsigned int node;
    GuiTheme *theme;
    const GuiTheme *theme_read;
    VerbaLinea *out;
    int pretty;
    unsigned int indent;
    const char *text;
    char *out_text;
    unsigned long long capacity;
    const char *pattern;
    const char *name;
    unsigned long long length;
    const GuiThemes *themes_read;
    unsigned int at;
    GuiColor color;
    int on;
    const char *mark;
    const char *color_text;
} ThemesCtx;

// the theme at `at` of `themes_read` read into `theme`, which is left zero where the themes have no cell
EMBED_INLINE void theme_at_read(const ThemesCtx *args)
{
    memor.set(&(MemoriaCfg){.dst = args->theme, .val = 0, .bytes = sizeof(*args->theme)});
    if (args->themes_read->cell != NULL)
    {
        (void)g_cli_guard->movable_buf_read(
            &(CarcerMovableCfg){.cell = args->themes_read->cell,
                                .offset = (unsigned long long)args->at * sizeof(GuiTheme),
                                .dst = args->theme,
                                .bytes = sizeof(GuiTheme)});
    }
}

// `theme_read` written as the theme at `at` of `themes_read`
EMBED_INLINE void theme_at_write(const ThemesCtx *args)
{
    if (args->themes_read->cell != NULL)
    {
        (void)g_cli_guard->movable_buf_write(
            &(CarcerMovableCfg){.cell = args->themes_read->cell,
                                .offset = (unsigned long long)args->at * sizeof(GuiTheme),
                                .src = args->theme_read,
                                .bytes = sizeof(GuiTheme)});
    }
}

int gui_themes_init(const GuiThemesOwnerCfg *args)
{
    GuiThemes *const themes = args->themes;
    themes->cell = g_cli_guard->movable_buf_alloc((unsigned long long)GUI_THEMES_MAX * sizeof(GuiTheme), args->owner);
    themes->owner = args->owner;
    themes->count = 0u;
    return themes->cell != NULL;
}

void gui_themes_free(const GuiThemesOwnerCfg *args)
{
    GuiThemes *const themes = args->themes;
    if (themes->cell != NULL)
    {
        (void)g_cli_guard->movable_buf_release(themes->cell, themes->owner);
    }
    themes->cell = NULL;
    themes->count = 0u;
}

int gui_theme_get(const GuiThemesAtCfg *args)
{
    if (args->at >= args->themes->count)
    {
        return 0;
    }
    EMBED_CALL(theme_at_read, ThemesCtx, .themes_read = args->themes, .at = args->at, .theme = args->theme_out);
    return 1;
}

// `text` copied into `out_text`, `capacity` bytes with its end, cut where it is longer
EMBED_INLINE void theme_text_copy(const ThemesCtx *args)
{
    VerbaLinea line = {args->out_text, args->capacity, 0ULL};
    mmgr_line_put(&line, (args->text != NULL) ? args->text : "");
    (void)mmgr_line_finish(&line);
}

// the string member `name` of the object `node`, NULL where none is or it is no string
EMBED_INLINE const char *theme_member_text(const ThemesCtx *args)
{
    const unsigned int member = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                           .json_read = args->json, .object = args->node, .name = args->name);
    return EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = args->json,
                      .node = member, .length_pointer = NULL);
}

// the theme `node` read into `theme`: 1, or 0 where the object names no theme of the window's scheme. A color whose
// value does not read and an icon without a pattern are left out
EMBED_INLINE int theme_object_read(const ThemesCtx *args)
{
    const CliJson *const json = args->json;
    GuiTheme *const theme = args->theme;
    const char *const name = EMBED_CALL(theme_member_text, ThemesCtx, .json = json, .node = args->node, .name = "name");
    const char *const base = EMBED_CALL(theme_member_text, ThemesCtx, .json = json, .node = args->node, .name = "base");
    const unsigned int colors = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                           .object = args->node, .name = "colors");
    if ((name == NULL) || (name[0] == '\0') || (base == NULL) ||
        !cellul.eq(&(CatenaFinitaCfg){.src = base, .other = s_base, .cap = sizeof(s_base)}) ||
        !EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json, .node = colors,
                    .type = CFG_JSON_OBJECT))
    {
        return 0;
    }
    memor.set(&(MemoriaCfg){.dst = theme, .val = 0, .bytes = sizeof(*theme)});
    EMBED_CALL(theme_text_copy, ThemesCtx, .text = name, .out_text = theme->name, .capacity = sizeof(theme->name));
    for (unsigned int member = json->node[colors].first; member != CLI_JSON_NONE; member = json->node[member].next)
    {
        unsigned long long length = 0ULL;
        const char *const value = EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg,
                                             .json_read = json, .node = member, .length_pointer = &length);
        GuiColor color = 0u;
        const char *const key = json->text + json->node[member].key;
        if ((value != NULL) && (theme->colors < GUI_THEME_COLORS) &&
            (cellul.len(&(CatenaFinitaCfg){.src = key, .cap = GUI_THEME_NAME}) < GUI_THEME_NAME) &&
            EMBED_CALL(gui_palette_text.value_read, GuiPaletteTextCfg, .text_read = value, .length = length,
                       .current = 0xff000000u, .color_pointer = &color))
        {
            GuiThemeColor *const one = &theme->color[theme->colors];
            EMBED_CALL(theme_text_copy, ThemesCtx, .text = key, .out_text = one->name, .capacity = sizeof(one->name));
            one->color = color;
            theme->colors += 1u;
        }
    }
    const unsigned int icons = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                          .object = args->node, .name = "icons");
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = json,
                                        .node = icons, .type = CFG_JSON_ARRAY)
                                 ? json->node[icons].first
                                 : CLI_JSON_NONE;
         item != CLI_JSON_NONE; item = json->node[item].next)
    {
        const char *const pattern =
            EMBED_CALL(theme_member_text, ThemesCtx, .json = json, .node = item, .name = "pattern");
        if ((pattern == NULL) || (pattern[0] == '\0') || (theme->icons >= GUI_THEME_ICONS))
        {
            continue;
        }
        GuiThemeIcon *const icon = &theme->icon[theme->icons];
        EMBED_CALL(theme_text_copy, ThemesCtx, .text = pattern, .out_text = icon->pattern,
                   .capacity = sizeof(icon->pattern));
        EMBED_CALL(theme_text_copy, ThemesCtx,
                   .text = EMBED_CALL(theme_member_text, ThemesCtx, .json = json, .node = item, .name = "mark"),
                   .out_text = icon->mark, .capacity = sizeof(icon->mark));
        EMBED_CALL(theme_text_copy, ThemesCtx,
                   .text = EMBED_CALL(theme_member_text, ThemesCtx, .json = json, .node = item, .name = "color"),
                   .out_text = icon->color, .capacity = sizeof(icon->color));
        theme->icons += 1u;
    }
    return 1;
}

// the tree of `text`, `length` bytes, read in a cell of `owner`: its root, or CLI_JSON_NONE where it is no JSON
EMBED_INLINE unsigned int theme_tree_alloc(const GuiThemesTextCfg *args, CliJson *json)
{
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    EMBED_CALL(cli_json_tree_node_node.json_begin, CliJsonTreeNodeNodeCfg, .json = json, .node_json_node_pointer = NULL,
               .nodes_max = 0u, .text_out = NULL, .text_max = 0ULL);
    if ((args->text == NULL) || (args->size == 0ULL))
    {
        return CLI_JSON_NONE;
    }
    return EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = json,
                      .text = args->text, .size = args->size, .nodes_more = 0u, .text_more = 0ULL, .err_msg = &err_msg,
                      .owner = args->owner);
}

void gui_themes_read(const GuiThemesTextCfg *args)
{
    GuiThemes *const themes = args->themes;
    themes->count = 0u;
    CliJson json;
    const unsigned int root = theme_tree_alloc(args, &json);
    if (root == CLI_JSON_NONE)
    {
        return;
    }
    // each theme read into a record of this call, then written into the themes' cell
    GuiTheme *const one = g_cli_guard->temporary_buf_alloc(sizeof(*one), args->owner);
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &json,
                                        .node = root, .type = CFG_JSON_ARRAY)
                                 ? json.node[root].first
                                 : CLI_JSON_NONE;
         (one != NULL) && (item != CLI_JSON_NONE) && (themes->count < GUI_THEMES_MAX); item = json.node[item].next)
    {
        if (EMBED_CALL(theme_object_read, ThemesCtx, .json = &json, .node = item, .theme = one))
        {
            EMBED_CALL(theme_at_write, ThemesCtx, .themes_read = themes, .at = themes->count, .theme_read = one);
            themes->count += 1u;
        }
    }
    g_cli_guard->persistent_buf_release(one, args->owner);
    g_cli_guard->persistent_buf_release(json.node, args->owner);
}

int gui_theme_read(const GuiThemeTextCfg *args)
{
    CliJson json;
    const unsigned int root =
        theme_tree_alloc(&(GuiThemesTextCfg){.text = args->text, .size = args->size, .owner = args->owner}, &json);
    if (root == CLI_JSON_NONE)
    {
        return 0;
    }
    const int read = EMBED_CALL(theme_object_read, ThemesCtx, .json = &json, .node = root, .theme = args->theme_out);
    g_cli_guard->persistent_buf_release(json.node, args->owner);
    return read;
}

// a line break and `indent` spaces where the text is pretty, nothing where it is compact
EMBED_INLINE void theme_break(const ThemesCtx *args)
{
    if (!args->pretty)
    {
        return;
    }
    mmgr_line_ch(args->out, '\n');
    for (unsigned int at = 0u; at < args->indent; at += 1u)
    {
        mmgr_line_put(args->out, "  ");
    }
}

// `text` written as a JSON string
EMBED_INLINE void theme_quote(const ThemesCtx *args)
{
    EMBED_CALL(cli_json_tree_text_length.json_quote, CliJsonTreeTextLengthCfg, .out = args->out, .text = args->text,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->text, .cap = CLI_POOL_BYTES}));
}

// the member `name` with the string `text` as its value, after a comma where `node` is not 0
EMBED_INLINE void theme_member_write(const ThemesCtx *args)
{
    mmgr_line_put(args->out, (args->node != 0u) ? "," : "");
    EMBED_CALL(theme_break, ThemesCtx, .out = args->out, .pretty = args->pretty, .indent = args->indent);
    EMBED_CALL(theme_quote, ThemesCtx, .out = args->out, .text = args->name);
    mmgr_line_put(args->out, args->pretty ? ": " : ":");
    EMBED_CALL(theme_quote, ThemesCtx, .out = args->out, .text = args->text);
}

// `theme_read` written as a JSON object: its name, its base, its colors by name and its icons where it has any
EMBED_INLINE void theme_write(const ThemesCtx *args)
{
    const GuiTheme *const theme = args->theme_read;
    VerbaLinea *const out = args->out;
    const int pretty = args->pretty;
    const unsigned int inner = args->indent + 1u;
    mmgr_line_ch(out, '{');
    EMBED_CALL(theme_member_write, ThemesCtx, .out = out, .pretty = pretty, .indent = inner, .node = 0u, .name = "name",
               .text = theme->name);
    EMBED_CALL(theme_member_write, ThemesCtx, .out = out, .pretty = pretty, .indent = inner, .node = 1u, .name = "base",
               .text = s_base);
    mmgr_line_ch(out, ',');
    EMBED_CALL(theme_break, ThemesCtx, .out = out, .pretty = pretty, .indent = inner);
    EMBED_CALL(theme_quote, ThemesCtx, .out = out, .text = "colors");
    mmgr_line_put(out, pretty ? ": {" : ":{");
    for (unsigned int at = 0u; at < theme->colors; at += 1u)
    {
        char value[GUI_THEME_COLOR_TEXT];
        EMBED_CALL(gui_palette_size_color.value_text, GuiPaletteSizeColorCfg, .color = theme->color[at].color,
                   .text = value, .size = sizeof(value), .alpha_always = 0);
        EMBED_CALL(theme_member_write, ThemesCtx, .out = out, .pretty = pretty, .indent = inner + 1u, .node = at,
                   .name = theme->color[at].name, .text = value);
    }
    if (theme->colors != 0u)
    {
        EMBED_CALL(theme_break, ThemesCtx, .out = out, .pretty = pretty, .indent = inner);
    }
    mmgr_line_ch(out, '}');
    if (theme->icons != 0u)
    {
        mmgr_line_ch(out, ',');
        EMBED_CALL(theme_break, ThemesCtx, .out = out, .pretty = pretty, .indent = inner);
        EMBED_CALL(theme_quote, ThemesCtx, .out = out, .text = "icons");
        mmgr_line_put(out, pretty ? ": [" : ":[");
        for (unsigned int at = 0u; at < theme->icons; at += 1u)
        {
            const GuiThemeIcon *const icon = &theme->icon[at];
            mmgr_line_put(out, (at != 0u) ? "," : "");
            EMBED_CALL(theme_break, ThemesCtx, .out = out, .pretty = pretty, .indent = inner + 1u);
            mmgr_line_ch(out, '{');
            EMBED_CALL(theme_member_write, ThemesCtx, .out = out, .pretty = 0, .node = 0u, .name = "pattern",
                       .text = icon->pattern);
            mmgr_line_put(out, pretty ? ", " : ",");
            EMBED_CALL(theme_member_write, ThemesCtx, .out = out, .pretty = 0, .node = 0u, .name = "mark",
                       .text = icon->mark);
            mmgr_line_put(out, pretty ? ", " : ",");
            EMBED_CALL(theme_member_write, ThemesCtx, .out = out, .pretty = 0, .node = 0u, .name = "color",
                       .text = icon->color);
            mmgr_line_ch(out, '}');
        }
        EMBED_CALL(theme_break, ThemesCtx, .out = out, .pretty = pretty, .indent = inner);
        mmgr_line_ch(out, ']');
    }
    EMBED_CALL(theme_break, ThemesCtx, .out = out, .pretty = pretty, .indent = args->indent);
    mmgr_line_ch(out, '}');
}

void gui_themes_text(const GuiThemesOutCfg *args)
{
    const GuiThemes *const themes = args->themes_read;
    mmgr_line_ch(args->out, '[');
    // each theme read into a record of this call for the write
    GuiTheme *const one = g_cli_guard->temporary_buf_alloc(sizeof(*one), themes->owner);
    for (unsigned int at = 0u; (one != NULL) && (at < themes->count); at += 1u)
    {
        EMBED_CALL(theme_at_read, ThemesCtx, .themes_read = themes, .at = at, .theme = one);
        mmgr_line_put(args->out, (at != 0u) ? "," : "");
        EMBED_CALL(theme_write, ThemesCtx, .theme_read = one, .out = args->out, .pretty = 0, .indent = 0u);
    }
    g_cli_guard->persistent_buf_release(one, themes->owner);
    mmgr_line_ch(args->out, ']');
}

void gui_theme_text(const GuiThemeOutCfg *args)
{
    EMBED_CALL(theme_write, ThemesCtx, .theme_read = args->theme_read, .out = args->out, .pretty = 1, .indent = 0u);
    mmgr_line_ch(args->out, '\n');
}

unsigned int gui_theme_find(const GuiThemesNameCfg *args)
{
    const GuiThemes *const themes = args->themes_read;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = GUI_THEME_NAME});
    if ((themes->cell == NULL) || (length >= GUI_THEME_NAME))
    {
        return themes->count;
    }
    // a theme's name opens its record, so the name's bytes with their end are ordered against it in place
    for (unsigned int at = 0u; at < themes->count; at += 1u)
    {
        if (g_cli_guard->movable_buf_compare(&(CarcerMovableCfg){.cell = themes->cell,
                                                                 .offset = (unsigned long long)at * sizeof(GuiTheme),
                                                                 .src = args->name,
                                                                 .bytes = length + 1ULL}) == 0)
        {
            return at;
        }
    }
    return themes->count;
}

// whether a theme or the scheme's own name takes `name`
EMBED_INLINE int theme_name_taken(const GuiThemesNameCfg *args)
{
    return cellul.eq(&(CatenaFinitaCfg){.src = args->name, .other = GUI_THEME_BASE_NAME, .cap = GUI_THEME_NAME}) ||
           (gui_theme_find(args) != args->themes_read->count);
}

void gui_theme_free_name(const GuiThemesNameCfg *args)
{
    const char *const wanted = ((args->name != NULL) && (args->name[0] != '\0')) ? args->name : GUI_THEME_BASE_NAME;
    if (!theme_name_taken(&(GuiThemesNameCfg){.themes_read = args->themes_read, .name = wanted}))
    {
        EMBED_CALL(theme_text_copy, ThemesCtx, .text = wanted, .out_text = args->name_out, .capacity = GUI_THEME_NAME);
        return;
    }
    // the stem: the name less a space and digits at its end
    unsigned long long stem = cellul.len(&(CatenaFinitaCfg){.src = wanted, .cap = GUI_THEME_NAME});
    unsigned long long digits = stem;
    while ((digits > 0ULL) && (wanted[digits - 1ULL] >= '0') && (wanted[digits - 1ULL] <= '9'))
    {
        digits -= 1ULL;
    }
    if ((digits < stem) && (digits > 0ULL) && (wanted[digits - 1ULL] == ' '))
    {
        stem = digits - 1ULL;
    }
    // the stem leaves room for a space and the number's digits inside the name's bytes
    const unsigned long long room = GUI_THEME_NAME - 12ULL;
    stem = (stem > room) ? room : stem;
    for (unsigned int number = 2u;; number += 1u)
    {
        VerbaLinea line = {args->name_out, GUI_THEME_NAME, 0ULL};
        mmgr_line_put_n(&line, wanted, stem);
        mmgr_line_ch(&line, ' ');
        mmgr_line_decimal(&line, number, 1u);
        (void)mmgr_line_finish(&line);
        if (!theme_name_taken(&(GuiThemesNameCfg){.themes_read = args->themes_read, .name = args->name_out}))
        {
            return;
        }
    }
}

unsigned int gui_theme_add(const GuiThemesAtNameCfg *args)
{
    GuiThemes *const themes = args->themes;
    if ((themes->count >= GUI_THEMES_MAX) || (themes->cell == NULL))
    {
        return themes->count;
    }
    // the theme made in a record of this call: the one at `from` under the new name, or an empty one
    GuiTheme *const theme = g_cli_guard->temporary_buf_alloc(sizeof(*theme), themes->owner);
    if (theme == NULL)
    {
        return themes->count;
    }
    memor.set(&(MemoriaCfg){.dst = theme, .val = 0, .bytes = sizeof(*theme)});
    if (args->from < themes->count)
    {
        EMBED_CALL(theme_at_read, ThemesCtx, .themes_read = themes, .at = args->from, .theme = theme);
    }
    EMBED_CALL(theme_text_copy, ThemesCtx, .text = args->name, .out_text = theme->name,
               .capacity = sizeof(theme->name));
    EMBED_CALL(theme_at_write, ThemesCtx, .themes_read = themes, .at = themes->count, .theme_read = theme);
    g_cli_guard->persistent_buf_release(theme, themes->owner);
    themes->count += 1u;
    return themes->count - 1u;
}

void gui_theme_remove(const GuiThemesAtCfg *args)
{
    GuiThemes *const themes = args->themes;
    if (args->at >= themes->count)
    {
        return;
    }
    // each theme after the one taken out moved down one place through a record of this call
    GuiTheme *const moved = g_cli_guard->temporary_buf_alloc(sizeof(*moved), themes->owner);
    for (unsigned int at = args->at + 1u; (moved != NULL) && (at < themes->count); at += 1u)
    {
        EMBED_CALL(theme_at_read, ThemesCtx, .themes_read = themes, .at = at, .theme = moved);
        EMBED_CALL(theme_at_write, ThemesCtx, .themes_read = themes, .at = at - 1u, .theme_read = moved);
    }
    g_cli_guard->persistent_buf_release(moved, themes->owner);
    themes->count -= (moved != NULL) ? 1u : 0u;
}

// the theme at `at` of `themes_read` read into `theme`, changed by `each`, and written back: `each`'s answer, or 0
// where `at` names no theme or the record cannot be taken
EMBED_INLINE int theme_changed(const GuiThemesAtCfg *args, int (*each)(const ThemesCtx *args), const ThemesCtx *with)
{
    const GuiThemes *const themes = args->themes;
    if (args->at >= themes->count)
    {
        return 0;
    }
    GuiTheme *const theme = g_cli_guard->temporary_buf_alloc(sizeof(*theme), themes->owner);
    if (theme == NULL)
    {
        return 0;
    }
    EMBED_CALL(theme_at_read, ThemesCtx, .themes_read = themes, .at = args->at, .theme = theme);
    ThemesCtx held = *with;
    held.theme = theme;
    const int changed = each(&held);
    if (changed)
    {
        EMBED_CALL(theme_at_write, ThemesCtx, .themes_read = themes, .at = args->at, .theme_read = theme);
    }
    g_cli_guard->persistent_buf_release(theme, themes->owner);
    return changed;
}

// the theme's name set to `name`: 1
static int theme_renamed(const ThemesCtx *args)
{
    EMBED_CALL(theme_text_copy, ThemesCtx, .text = args->name, .out_text = args->theme->name,
               .capacity = sizeof(args->theme->name));
    return 1;
}

void gui_theme_rename(const GuiThemesAtNameCfg *args)
{
    (void)theme_changed(&(GuiThemesAtCfg){.themes = args->themes, .at = args->at}, theme_renamed,
                        &(ThemesCtx){.name = args->name});
}

// the place of the color `name` among the theme's colors, `colors` where none is
EMBED_INLINE unsigned int theme_color_place(const ThemesCtx *args)
{
    const GuiTheme *const theme = args->theme_read;
    for (unsigned int at = 0u; at < theme->colors; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = theme->color[at].name, .other = args->name, .cap = GUI_THEME_NAME}))
        {
            return at;
        }
    }
    return theme->colors;
}

// the color `name` set to `color` in the theme where `on` holds, else put back: 1 where the theme changed
static int theme_color_changed(const ThemesCtx *args)
{
    GuiTheme *const theme = args->theme;
    const unsigned int place = EMBED_CALL(theme_color_place, ThemesCtx, .theme_read = theme, .name = args->name);
    if (args->on && (place < theme->colors))
    {
        theme->color[place].color = args->color;
        return 1;
    }
    if (args->on && (theme->colors < GUI_THEME_COLORS))
    {
        GuiThemeColor *const one = &theme->color[theme->colors];
        EMBED_CALL(theme_text_copy, ThemesCtx, .text = args->name, .out_text = one->name,
                   .capacity = sizeof(one->name));
        one->color = args->color;
        theme->colors += 1u;
        return 1;
    }
    if (!args->on && (place < theme->colors))
    {
        for (unsigned int at = place + 1u; at < theme->colors; at += 1u)
        {
            theme->color[at - 1u] = theme->color[at];
        }
        theme->colors -= 1u;
        return 1;
    }
    return 0;
}

void gui_theme_color_set(const GuiThemesAtColorCfg *args)
{
    (void)theme_changed(&(GuiThemesAtCfg){.themes = args->themes, .at = args->at}, theme_color_changed,
                        &(ThemesCtx){.name = args->name, .color = args->color, .on = args->on});
}

int gui_theme_color_get(const GuiThemesAtColorCfg *args)
{
    const GuiThemes *const themes = args->themes;
    if (args->at >= themes->count)
    {
        return 0;
    }
    GuiTheme *const theme = g_cli_guard->temporary_buf_alloc(sizeof(*theme), themes->owner);
    if (theme == NULL)
    {
        return 0;
    }
    EMBED_CALL(theme_at_read, ThemesCtx, .themes_read = themes, .at = args->at, .theme = theme);
    const unsigned int place = EMBED_CALL(theme_color_place, ThemesCtx, .theme_read = theme, .name = args->name);
    const int set = place < theme->colors;
    if (set)
    {
        *args->color_pointer = theme->color[place].color;
    }
    g_cli_guard->persistent_buf_release(theme, themes->owner);
    return set;
}

// the place of the icon of `pattern` among the theme's icons, `icons` where none is
EMBED_INLINE unsigned int theme_icon_place(const ThemesCtx *args)
{
    const GuiTheme *const theme = args->theme_read;
    for (unsigned int at = 0u; at < theme->icons; at += 1u)
    {
        if (cellul.eq(
                &(CatenaFinitaCfg){.src = theme->icon[at].pattern, .other = args->pattern, .cap = GUI_THEME_PATTERN}))
        {
            return at;
        }
    }
    return theme->icons;
}

// the icon of `pattern` set in the theme with `mark` and `color_text`, in place of one of the pattern: 1 where the
// theme changed
static int theme_icon_changed(const ThemesCtx *args)
{
    GuiTheme *const theme = args->theme;
    unsigned int place = EMBED_CALL(theme_icon_place, ThemesCtx, .theme_read = theme, .pattern = args->pattern);
    if (place == theme->icons)
    {
        if (theme->icons >= GUI_THEME_ICONS)
        {
            return 0;
        }
        theme->icons += 1u;
    }
    GuiThemeIcon *const icon = &theme->icon[place];
    EMBED_CALL(theme_text_copy, ThemesCtx, .text = args->pattern, .out_text = icon->pattern,
               .capacity = sizeof(icon->pattern));
    EMBED_CALL(theme_text_copy, ThemesCtx, .text = args->mark, .out_text = icon->mark, .capacity = sizeof(icon->mark));
    EMBED_CALL(theme_text_copy, ThemesCtx, .text = args->color_text, .out_text = icon->color,
               .capacity = sizeof(icon->color));
    return 1;
}

void gui_theme_icon_set(const GuiThemesAtIconCfg *args)
{
    if ((args->pattern == NULL) || (args->pattern[0] == '\0'))
    {
        return;
    }
    (void)theme_changed(&(GuiThemesAtCfg){.themes = args->themes, .at = args->at}, theme_icon_changed,
                        &(ThemesCtx){.pattern = args->pattern, .mark = args->mark, .color_text = args->color});
}

// the icon of `pattern` taken off the theme, those after it closing the gap: 1 where the theme changed
static int theme_icon_removed(const ThemesCtx *args)
{
    GuiTheme *const theme = args->theme;
    const unsigned int place = EMBED_CALL(theme_icon_place, ThemesCtx, .theme_read = theme, .pattern = args->pattern);
    if (place == theme->icons)
    {
        return 0;
    }
    for (unsigned int at = place + 1u; at < theme->icons; at += 1u)
    {
        theme->icon[at - 1u] = theme->icon[at];
    }
    theme->icons -= 1u;
    return 1;
}

void gui_theme_icon_remove(const GuiThemesAtIconCfg *args)
{
    if (args->pattern == NULL)
    {
        return;
    }
    (void)theme_changed(&(GuiThemesAtCfg){.themes = args->themes, .at = args->at}, theme_icon_removed,
                        &(ThemesCtx){.pattern = args->pattern});
}

// a byte in lower case where it is an ASCII capital
EMBED_INLINE char theme_lower(char letter)
{
    return ((letter >= 'A') && (letter <= 'Z')) ? (char)(letter + ('a' - 'A')) : letter;
}

// whether `pattern` covers `name`, `length` bytes: `*` stands for any letters and `?` for one byte, the rest compared
// without case. A `*` that fails later is tried again one byte further on
EMBED_INLINE int theme_pattern_covers(const ThemesCtx *args)
{
    const char *const pattern = args->pattern;
    const char *const name = args->name;
    unsigned long long at = 0ULL;
    unsigned long long reading = 0ULL;
    unsigned long long star = 0ULL;
    unsigned long long star_at = 0ULL;
    int starred = 0;
    while (at < args->length)
    {
        if (pattern[reading] == '*')
        {
            starred = 1;
            star = reading;
            star_at = at;
            reading += 1ULL;
        }
        else if ((pattern[reading] == '?') ||
                 ((pattern[reading] != '\0') && (theme_lower(pattern[reading]) == theme_lower(name[at]))))
        {
            reading += 1ULL;
            at += 1ULL;
        }
        else if (starred)
        {
            reading = star + 1ULL;
            star_at += 1ULL;
            at = star_at;
        }
        else
        {
            return 0;
        }
    }
    while (pattern[reading] == '*')
    {
        reading += 1ULL;
    }
    return pattern[reading] == '\0';
}

const GuiThemeIcon *gui_theme_icon_match(const GuiThemeNameCfg *args)
{
    const GuiTheme *const theme = args->theme_read;
    for (unsigned int at = 0u; (theme != NULL) && (at < theme->icons); at += 1u)
    {
        if (EMBED_CALL(theme_pattern_covers, ThemesCtx, .pattern = theme->icon[at].pattern, .name = args->name,
                       .length = args->length))
        {
            return &theme->icon[at];
        }
    }
    return NULL;
}
