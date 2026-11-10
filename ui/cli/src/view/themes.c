// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// themes.c: the reader's color themes read from the state store's entry and written back to it, found, added, renamed
// and taken out, their colors and icons set, an icon matched to a file's name, and one theme shared as text
#include "themes.h"

#include "../command_line/cli_internal.h"
#include "../text/json_tree.h"
#include "palette.h"

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
} ThemesCtx;

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
    return EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc_temporary, CliJsonTreeTextSizeErrMsgCfg, .json = json,
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
    for (unsigned int item = EMBED_CALL(cli_json_tree_node_type.json_is, CliJsonTreeNodeTypeCfg, .json_read = &json,
                                        .node = root, .type = CFG_JSON_ARRAY)
                                 ? json.node[root].first
                                 : CLI_JSON_NONE;
         (item != CLI_JSON_NONE) && (themes->count < GUI_THEMES_MOST); item = json.node[item].next)
    {
        if (EMBED_CALL(theme_object_read, ThemesCtx, .json = &json, .node = item,
                       .theme = &themes->theme[themes->count]))
        {
            themes->count += 1u;
        }
    }
    g_cli_guard->persistent_buf_release(json.node, args->owner);
}

int gui_theme_read(const GuiThemeTextCfg *args)
{
    CliJson json;
    const unsigned int root = theme_tree_alloc(
        &(GuiThemesTextCfg){.text = args->text, .size = args->size, .owner = args->owner}, &json);
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
    mmgr_line_ch(args->out, '[');
    for (unsigned int at = 0u; at < args->themes_read->count; at += 1u)
    {
        mmgr_line_put(args->out, (at != 0u) ? "," : "");
        EMBED_CALL(theme_write, ThemesCtx, .theme_read = &args->themes_read->theme[at], .out = args->out, .pretty = 0,
                   .indent = 0u);
    }
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
    for (unsigned int at = 0u; at < themes->count; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = themes->theme[at].name, .other = args->name, .cap = GUI_THEME_NAME}))
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
    if (themes->count >= GUI_THEMES_MOST)
    {
        return themes->count;
    }
    GuiTheme *const theme = &themes->theme[themes->count];
    if (args->from < themes->count)
    {
        memor.cpy(&(MemoriaCfg){.dst = theme, .src = &themes->theme[args->from], .bytes = sizeof(*theme)});
    }
    else
    {
        memor.set(&(MemoriaCfg){.dst = theme, .val = 0, .bytes = sizeof(*theme)});
    }
    EMBED_CALL(theme_text_copy, ThemesCtx, .text = args->name, .out_text = theme->name, .capacity = sizeof(theme->name));
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
    for (unsigned int at = args->at + 1u; at < themes->count; at += 1u)
    {
        memor.cpy(&(MemoriaCfg){.dst = &themes->theme[at - 1u], .src = &themes->theme[at], .bytes = sizeof(GuiTheme)});
    }
    themes->count -= 1u;
}

void gui_theme_rename(const GuiThemesAtNameCfg *args)
{
    if (args->at < args->themes->count)
    {
        GuiTheme *const theme = &args->themes->theme[args->at];
        EMBED_CALL(theme_text_copy, ThemesCtx, .text = args->name, .out_text = theme->name,
                   .capacity = sizeof(theme->name));
    }
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

void gui_theme_color_set(const GuiThemesAtColorCfg *args)
{
    if (args->at >= args->themes->count)
    {
        return;
    }
    GuiTheme *const theme = &args->themes->theme[args->at];
    const unsigned int place = EMBED_CALL(theme_color_place, ThemesCtx, .theme_read = theme, .name = args->name);
    if (args->on && (place < theme->colors))
    {
        theme->color[place].color = args->color;
    }
    else if (args->on && (theme->colors < GUI_THEME_COLORS))
    {
        GuiThemeColor *const one = &theme->color[theme->colors];
        EMBED_CALL(theme_text_copy, ThemesCtx, .text = args->name, .out_text = one->name, .capacity = sizeof(one->name));
        one->color = args->color;
        theme->colors += 1u;
    }
    else if (!args->on && (place < theme->colors))
    {
        for (unsigned int at = place + 1u; at < theme->colors; at += 1u)
        {
            theme->color[at - 1u] = theme->color[at];
        }
        theme->colors -= 1u;
    }
}

int gui_theme_color_get(const GuiThemesAtColorCfg *args)
{
    if (args->at >= args->themes->count)
    {
        return 0;
    }
    const GuiTheme *const theme = &args->themes->theme[args->at];
    const unsigned int place = EMBED_CALL(theme_color_place, ThemesCtx, .theme_read = theme, .name = args->name);
    if (place < theme->colors)
    {
        *args->color_pointer = theme->color[place].color;
    }
    return place < theme->colors;
}

// the place of the icon of `pattern` among the theme's icons, `icons` where none is
EMBED_INLINE unsigned int theme_icon_place(const ThemesCtx *args)
{
    const GuiTheme *const theme = args->theme_read;
    for (unsigned int at = 0u; at < theme->icons; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = theme->icon[at].pattern, .other = args->pattern, .cap = GUI_THEME_PATTERN}))
        {
            return at;
        }
    }
    return theme->icons;
}

void gui_theme_icon_set(const GuiThemesAtIconCfg *args)
{
    if ((args->at >= args->themes->count) || (args->pattern == NULL) || (args->pattern[0] == '\0'))
    {
        return;
    }
    GuiTheme *const theme = &args->themes->theme[args->at];
    unsigned int place = EMBED_CALL(theme_icon_place, ThemesCtx, .theme_read = theme, .pattern = args->pattern);
    if (place == theme->icons)
    {
        if (theme->icons >= GUI_THEME_ICONS)
        {
            return;
        }
        theme->icons += 1u;
    }
    GuiThemeIcon *const icon = &theme->icon[place];
    EMBED_CALL(theme_text_copy, ThemesCtx, .text = args->pattern, .out_text = icon->pattern,
               .capacity = sizeof(icon->pattern));
    EMBED_CALL(theme_text_copy, ThemesCtx, .text = args->mark, .out_text = icon->mark, .capacity = sizeof(icon->mark));
    EMBED_CALL(theme_text_copy, ThemesCtx, .text = args->color, .out_text = icon->color,
               .capacity = sizeof(icon->color));
}

void gui_theme_icon_remove(const GuiThemesAtIconCfg *args)
{
    if ((args->at >= args->themes->count) || (args->pattern == NULL))
    {
        return;
    }
    GuiTheme *const theme = &args->themes->theme[args->at];
    const unsigned int place = EMBED_CALL(theme_icon_place, ThemesCtx, .theme_read = theme, .pattern = args->pattern);
    if (place == theme->icons)
    {
        return;
    }
    for (unsigned int at = place + 1u; at < theme->icons; at += 1u)
    {
        theme->icon[at - 1u] = theme->icon[at];
    }
    theme->icons -= 1u;
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
