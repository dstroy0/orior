// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// faces.c: the window's faces as this system holds them, each face's file found once among its families
#include "faces.h"

#include "../../cli/src/command_line/cli_internal.h"
#include "../../cli/src/print/font_families.h"

// the families the interface face and the reading face are looked for among, in the order they are preferred
static const char *const s_sans[6][4] = {
    {"segoeui.ttf", "segoeuib.ttf", "segoeuii.ttf", "segoeuiz.ttf"},
    {"DejaVuSans.ttf", "DejaVuSans-Bold.ttf", "DejaVuSans-Oblique.ttf", "DejaVuSans-BoldOblique.ttf"},
    {"NotoSans-Regular.ttf", "NotoSans-Bold.ttf", "NotoSans-Italic.ttf", "NotoSans-BoldItalic.ttf"},
    {"LiberationSans-Regular.ttf", "LiberationSans-Bold.ttf", "LiberationSans-Italic.ttf",
     "LiberationSans-BoldItalic.ttf"},
    {"Helvetica.ttc", "", "", ""},
    {"arial.ttf", "arialbd.ttf", "ariali.ttf", "arialbi.ttf"}};
static const char *const s_serif[6][4] = {
    {"georgia.ttf", "georgiab.ttf", "georgiai.ttf", "georgiaz.ttf"},
    {"DejaVuSerif.ttf", "DejaVuSerif-Bold.ttf", "DejaVuSerif-Italic.ttf", "DejaVuSerif-BoldItalic.ttf"},
    {"NotoSerif-Regular.ttf", "NotoSerif-Bold.ttf", "NotoSerif-Italic.ttf", "NotoSerif-BoldItalic.ttf"},
    {"LiberationSerif-Regular.ttf", "LiberationSerif-Bold.ttf", "LiberationSerif-Italic.ttf",
     "LiberationSerif-BoldItalic.ttf"},
    {"Times New Roman.ttf", "", "", ""},
    {"times.ttf", "timesbd.ttf", "timesi.ttf", "timesbi.ttf"}};
// the families the symbol face is looked for among, in the order they are preferred: the one the system's own text
// falls back to for a character its face has none of
static const char *const s_symbol[3][4] = {
    {"seguisym.ttf", "", "", ""}, {"DejaVuSans.ttf", "", "", ""}, {"NotoSansSymbols2-Regular.ttf", "", "", ""}};

// each face's path, looked for once; a face the system has none of retains an empty path
static char s_path[GUI_FACES][CLI_PATH_MAX_LEN];
static int s_looked[GUI_FACES];

// the path of `face` found: the code faces among the monospace fonts, the interface face and its bold among the sans
// families, the reading face among the serif families and the symbol face among its own
static const char *faces_path(unsigned int face)
{
    if (face >= GUI_FACES)
    {
        return "";
    }
    if (!s_looked[face])
    {
        char paths[4][CLI_PATH_MAX_LEN];
        if (face <= GUI_FACE_CODE_BOLD_ITALIC)
        {
            EMBED_CALL(cli_font_families.monospace, CliFontFamiliesCfg, .paths = paths);
            for (unsigned int code = GUI_FACE_CODE; code <= GUI_FACE_CODE_BOLD_ITALIC; code += 1u)
            {
                memor.cpy(&(MemoriaCfg){.dst = s_path[code], .src = paths[code], .bytes = sizeof(paths[code])});
                s_looked[code] = 1;
            }
        }
        else if ((face == GUI_FACE_HEAD) || (face == GUI_FACE_HEAD_BOLD))
        {
            EMBED_CALL(cli_font_families.find, CliFontFamiliesCfg, .families = s_sans,
                       .count = sizeof(s_sans) / sizeof(s_sans[0]), .paths = paths);
            memor.cpy(&(MemoriaCfg){.dst = s_path[GUI_FACE_HEAD], .src = paths[0], .bytes = sizeof(paths[0])});
            memor.cpy(&(MemoriaCfg){.dst = s_path[GUI_FACE_HEAD_BOLD], .src = paths[1], .bytes = sizeof(paths[1])});
            s_looked[GUI_FACE_HEAD] = 1;
            s_looked[GUI_FACE_HEAD_BOLD] = 1;
        }
        else if (face == GUI_FACE_TEXT)
        {
            EMBED_CALL(cli_font_families.find, CliFontFamiliesCfg, .families = s_serif,
                       .count = sizeof(s_serif) / sizeof(s_serif[0]), .paths = paths);
            memor.cpy(&(MemoriaCfg){.dst = s_path[GUI_FACE_TEXT], .src = paths[0], .bytes = sizeof(paths[0])});
            s_looked[GUI_FACE_TEXT] = 1;
        }
        else
        {
            EMBED_CALL(cli_font_families.find, CliFontFamiliesCfg, .families = s_symbol,
                       .count = sizeof(s_symbol) / sizeof(s_symbol[0]), .paths = paths);
            memor.cpy(&(MemoriaCfg){.dst = s_path[GUI_FACE_SYMBOL], .src = paths[0], .bytes = sizeof(paths[0])});
            s_looked[GUI_FACE_SYMBOL] = 1;
        }
    }
    return s_path[face];
}

static int faces_size(unsigned int face, unsigned long long *size)
{
    const char *const path = faces_path(face);
    return (path[0] != '\0') &&
           EMBED_CALL(cli_path_size_internal.file_size, CliPathSizeCtx, .path = path, .size_pointer = size);
}

static int faces_read(unsigned int face, unsigned char *at, unsigned long long size)
{
    unsigned long long got = 0ULL;
    return EMBED_CALL(cli_path_from_internal.file_read_at, CliPathFromCtx, .path = faces_path(face), .from = 0ULL,
                      .bytes_bytes = at, .count_u64 = size, .got = &got) &&
           (got == size);
}

const GuiGlyphsFaces g_gui_faces = {faces_size, faces_read};
