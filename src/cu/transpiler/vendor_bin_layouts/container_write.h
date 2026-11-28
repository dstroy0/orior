// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef CONTAINER_WRITE_H
#define CONTAINER_WRITE_H

// One emitter, every container. It reads a layout file and writes what that layout describes.
//
// There is no emitter for one format and another for the next. The work is the same wherever code must go to a system.
// Take a container that the system already accepts, and put the code in. Put in the few attributes that the code
// decides, and arrange the parts again. Rows in a file (container_layout.h) tell which byte stores the section count,
// the tag of an attribute, and the name of the code part. There is one folder beside this file for each format.
//
// A pattern is a container that the target already accepted. The emitter carries over all that it does not know in
// it, and never invents it. Thus a layout that the emitter only partly knows is enough to work with. The known rows
// tell what to change, and the rest of the bytes travel with no change.

#include "container_layout.h"

// what the emitter writes a container from
typedef struct
{
    // the layout of this container, read from its file
    const ContainerLayout *layout;
    // a container the target already accepted, and its size
    const unsigned char *pattern;
    unsigned long long pattern_size;
    // the name of the part that the emitter writes, which the names of the layout go around
    const char *part;
    // the code, and its size
    const unsigned char *code;
    unsigned long long code_size;
    // how many registers the code uses, which the target reads from the container and no instruction declares
    unsigned int registers;
    // where each exit lies in the code, in bytes, which the container carries for it
    const unsigned int *exits;
    unsigned int exit_count;
} ContainerWrite;

// the container written into `written`, which has room for `room` bytes, and its size through `size`. The return is
// 1, or 0 with the reason printed and nothing written
int container_write(const ContainerWrite *args, unsigned char *written, unsigned long long room,
                    unsigned long long *size);

// one part of many put into one container. It has its name, its code and the size of the code. It has the registers
// that it uses, or 0 where the count of the pattern for it stays. It also has where each of its exits lies in its code
typedef struct
{
    const char *part;
    const unsigned char *code;
    unsigned long long code_size;
    unsigned int registers;
    const unsigned int *exits;
    unsigned int exit_count;
} ContainerPart;

// what the emitter writes a container of many parts from: the layout, the pattern and the parts. Each is a part that
// the pattern already holds, and the emitter puts all of them in at one arrangement of the container
typedef struct
{
    const ContainerLayout *layout;
    const unsigned char *pattern;
    unsigned long long pattern_size;
    const ContainerPart *parts;
    unsigned int part_count;
} ContainerParts;

// the container written with every part of `args` put in, into `written`, which has room for `room` bytes, and its
// size through `size`. The return is 1, or 0 with the reason printed and nothing written
int container_parts_write(const ContainerParts *args, unsigned char *written, unsigned long long room,
                          unsigned long long *size);

// the offsets in `code` of every instruction whose low word matches `ending`, into `exits`, which has room for `room`
// of them. The return is the count found. The target tells what ends a part, and the caller passes it in
unsigned int container_endings_find(const ContainerLayout *layout, const unsigned char *code,
                                    unsigned long long code_size, unsigned long long ending, unsigned int *exits,
                                    unsigned int room);

// how many registers `part` uses in `pattern`, which is `pattern_size` bytes, read where the layout tells. It is 0
// where the pattern holds no such part, or a table, a section or a name lies past its end
unsigned int container_registers_read(const ContainerLayout *layout, const unsigned char *pattern,
                                      unsigned long long pattern_size, const char *part);

// where every ending of `part` lies in its code, in bytes, as the container itself records them in the own attributes
// of the part. They go into `exits`, which has room for `room` of them, and the return is the count found. It is 0
// where the pattern holds no such part, or its attributes carry no ending. The call reads the own record of the
// container, and names no instruction to find one
unsigned int container_exits_read(const ContainerLayout *layout, const unsigned char *pattern,
                                  unsigned long long pattern_size, const char *part, unsigned int *exits,
                                  unsigned int room);

// where the code of `part` lies in `pattern`, which is `pattern_size` bytes: its offset through `offset` and its length
// through `size`. 1, or 0 where the pattern contains no such part or its code lies past the pattern's end
int container_part_code(const ContainerLayout *layout, const unsigned char *pattern, unsigned long long pattern_size,
                        const char *part, unsigned long long *offset, unsigned long long *size);

// every code section of `container`, which is `size` bytes, each a section whose name is the code name of the layout
// for some part. The offset of each goes through `offsets` and its length through `sizes`, which have room for `room`.
// The return is the count found. It is 0 where there is none or more than `room`, or where a header, a name or a
// section lies past the container end
unsigned int container_code_sections(const ContainerLayout *layout, const unsigned char *container,
                                     unsigned long long size, unsigned long long *offsets, unsigned long long *sizes,
                                     unsigned int room);

// every section of `container` whose name starts as the text `key` of the layout does with no part. It is as
// container_code_sections finds the sections of the code by `section.code`. The offset of each goes through `offsets`
// and its length through `sizes`, which have room for `room`, and the return is the count found
unsigned int container_named_sections(const ContainerLayout *layout, const unsigned char *container,
                                      unsigned long long size, const char *key, unsigned long long *offsets,
                                      unsigned long long *sizes, unsigned int room);

#endif
