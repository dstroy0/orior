// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef CONTAINER_LAYOUT_H
#define CONTAINER_LAYOUT_H

// A container's layout as rows in a file. The emitter carries none of it.
//
// Every offset, width, tag and name a format uses is a fact about that format and not about the work of emitting.
// The emitter reads a pattern's tables, puts code and attributes in, and positions the sections out again. Which byte
// stores the section count is the layout's to specify. A second format is a second file and no second emitter.
//
// A layout may be partly known. A field nobody has found yet is absent, and requesting it reports so instead of
// reading a byte that means something else.

#define LAYOUT_NAME_MAX_LEN 48u
#define LAYOUT_TEXT_MAX_LEN 64u
#define LAYOUT_ROWS 96u

typedef enum
{
    LAYOUT_FIELD,
    LAYOUT_SIZE,
    LAYOUT_TAG,
    LAYOUT_NAME
} LayoutRowKind;

// one row: a field has an offset and a width, a size and a tag have a number, and a name has text
typedef struct
{
    char name[LAYOUT_NAME_MAX_LEN];
    unsigned int type;
    unsigned long long offset;
    unsigned int width;
    char text[LAYOUT_TEXT_MAX_LEN];
} LayoutRow;

typedef struct
{
    LayoutRow row[LAYOUT_ROWS];
    unsigned int rows;
    char from[256];
} ContainerLayout;

// `path` read into `layout`. 1, or 0 with the reason printed
int container_layout_read(ContainerLayout *layout, const char *path);

// The place `name` names, through `offset` and `width`. 1, or 0 where the layout defines no such field, which is a
// layout that does not yet know this part of the format
int layout_field(const ContainerLayout *layout, const char *name, unsigned long long *offset, unsigned int *width);

// the number `name` names, or `otherwise` where the layout defines none
unsigned long long layout_number(const ContainerLayout *layout, const char *name, unsigned long long otherwise);

// The text `name` names with {part} replaced by `part`, into `written` which has room for `room`. 1, or 0 where the
// layout defines no such name
int layout_text(const ContainerLayout *layout, const char *name, const char *part, char *written, unsigned int room);

#endif
