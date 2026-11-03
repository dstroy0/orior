// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// xsettings.c: the X settings list as its manager lays it out, read for the setting a desktop asks for less motion by:
// a byte order, a serial and a count, then each setting its type, its name padded to four bytes, a serial and its value
#include "xsettings.h"

#include "../../cli/src/command_line/cli_internal.h"

// a setting's types; the head's bytes, and the setting asked for
#define XSETTINGS_INTEGER 0u
#define XSETTINGS_STRING 1u
#define XSETTINGS_COLOR 2u
#define XSETTINGS_HEAD 12ULL
#define XSETTINGS_NAME_SIZE 20ULL
static const char s_animations[] = "Gtk/EnableAnimations";

typedef struct
{
    const unsigned char *at;
    int high_first;
} XsettingsCtx;
// the four bytes at `at` as a word, in the list's byte order
EMBED_INLINE unsigned int xsettings_word(const XsettingsCtx *args)
{
    return args->high_first ? (((unsigned int)args->at[0] << 24u) | ((unsigned int)args->at[1] << 16u) |
                               ((unsigned int)args->at[2] << 8u) | args->at[3])
                            : (((unsigned int)args->at[3] << 24u) | ((unsigned int)args->at[2] << 16u) |
                               ((unsigned int)args->at[1] << 8u) | args->at[0]);
}

int gui_xsettings_animations(const GuiXsettingsCfg *args)
{
    int *enabled = args->enabled;
    if ((args->data == NULL) || (args->size < XSETTINGS_HEAD))
    {
        return 0;
    }
    const int high_first = args->data[0] != 0u;
    const unsigned int settings =
        EMBED_CALL(xsettings_word, XsettingsCtx, .at = args->data + 8u, .high_first = high_first);
    unsigned long long at = XSETTINGS_HEAD;
    for (unsigned int setting = 0u; setting < settings; setting += 1u)
    {
        if (at + 4ULL > args->size)
        {
            return 0;
        }
        const unsigned int type = args->data[at];
        const unsigned long long name_size =
            high_first ? (((unsigned long long)args->data[at + 2ULL] << 8u) | args->data[at + 3ULL])
                       : (((unsigned long long)args->data[at + 3ULL] << 8u) | args->data[at + 2ULL]);
        const unsigned long long name_at = at + 4ULL;
        // past the name, padded to four, and the setting's serial
        const unsigned long long value_at = name_at + ((name_size + 3ULL) & ~3ULL) + 4ULL;
        if (value_at > args->size)
        {
            return 0;
        }
        if (type == XSETTINGS_INTEGER)
        {
            if (value_at + 4ULL > args->size)
            {
                return 0;
            }
            if ((name_size == XSETTINGS_NAME_SIZE) &&
                (memor.cmp(&(MemoriaCfg){
                     .src = args->data + name_at, .other = s_animations, .bytes = XSETTINGS_NAME_SIZE}) == 0))
            {
                *enabled = EMBED_CALL(xsettings_word, XsettingsCtx, .at = args->data + value_at,
                                      .high_first = high_first) != 0u;
                return 1;
            }
            at = value_at + 4ULL;
        }
        else if (type == XSETTINGS_STRING)
        {
            if (value_at + 4ULL > args->size)
            {
                return 0;
            }
            const unsigned long long value_size =
                EMBED_CALL(xsettings_word, XsettingsCtx, .at = args->data + value_at, .high_first = high_first);
            at = value_at + 4ULL + ((value_size + 3ULL) & ~3ULL);
        }
        else if (type == XSETTINGS_COLOR)
        {
            at = value_at + 8ULL;
        }
        else
        {
            return 0;
        }
    }
    return 0;
}
