// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// A cubin written from a kernel's machine code and a cubin the toolchain made for a kernel that takes the same
// parameters
#ifndef CUBIN_WRITE_H
#define CUBIN_WRITE_H

#include "../container_write.h"

// A cubin is an ELF, and almost all of it tells what the kernel takes and not what it does. That is its parameters, the
// constant bank that they lie in, its notes, its symbols and its relocations. The probes of the cell found that two
// cubins of one kernel differ only in four things (engine_plan.md, the SASS findings). They are the code, its size, the
// count of registers that it uses, and the offsets of its exits.
//
// Thus the writer takes a cubin that the toolchain made for a kernel with the same parameters, and puts new code in
// it. It does not arrange an ELF from nothing. The writer carries over all that it does not know, and never invents it.
//
// The writer is the emitter of container_write.h, given a cubin and the layout file of the cubin, elf64_nvidia.tsv,
// beside it.
//
// The own kernel of the template decides what fits: the same name, the same parameters, the same constant bank. Give
// the writer code that takes other parameters, and the cubin loads and reads the wrong parameters.

// what the writer writes a cubin from
typedef struct
{
    // the cubin the toolchain made, and its size
    const unsigned char *pattern;
    unsigned long long pattern_size;
    // the kernel's name, which names its code section (.text.<kernel>) and its own info section
    const char *kernel;
    // the kernel's machine code, sixteen bytes an instruction, and its size
    const unsigned char *code;
    unsigned long long code_size;
    // how many registers a thread of the kernel uses, which the part reads from the ELF and no instruction declares
    unsigned int registers;
    // where each exit lies in the code, in bytes, which the kernel's info section carries
    const unsigned int *exits;
    unsigned int exit_count;
} CubinWrite;

// the cubin written into `written`, which has room for `room` bytes, and its size through `size`. The return is 1, or 0
// with the reason printed and nothing written
int cubin_write(const CubinWrite *args, unsigned char *written, unsigned long long room, unsigned long long *size);

// what the writer writes a cubin of many kernels from: a cubin that the toolchain made that holds every one of them.
// Then the name, code and exits of each kernel (container_write.h, ContainerPart), with its register count 0 where
// the count of the pattern stays
typedef struct
{
    const unsigned char *pattern;
    unsigned long long pattern_size;
    const ContainerPart *kernels;
    unsigned int kernel_count;
} CubinKernels;

// the cubin written with every kernel of `args` put in at one arrangement, into `written`, which has room for `room`
// bytes, and its size through `size`. The return is 1, or 0 with the reason printed and nothing written
int cubin_kernels_write(const CubinKernels *args, unsigned char *written, unsigned long long room,
                        unsigned long long *size);

// where the code of `kernel` lies in `pattern`, which is `pattern_size` bytes: its offset and its length. 1, or 0
// where the pattern contains no such kernel
int cubin_code_of(const unsigned char *pattern, unsigned long long pattern_size, const char *kernel,
                  unsigned long long *offset, unsigned long long *size);

// the container that a system accepted, read from the container rows of its layout at `path`. Its bytes go into
// `pattern`, which has room for `room`. The kernel where it enters goes into `kernel`, which has room for
// `kernel_room`. The return is the bytes read. It is 0, with the reason printed, where the file has no such rows, a row
// is not hex, or the bytes do not fit
unsigned long long cubin_pattern_read(const char *path, unsigned char *pattern, unsigned long long room, char *kernel,
                                      unsigned int kernel_room);

// the offsets of the exits in `code`, into `exits`, which has room for `room` of them. The return is the count found.
// An exit is the instruction EXIT, whose encoding comes from `exit_low` masked to the own bits of the operation
unsigned int cubin_exits_find(const unsigned char *code, unsigned long long code_size, unsigned long long exit_low,
                              unsigned int *exits, unsigned int room);

// where every exit of `kernel` lies in its code, in bytes, as the cubin itself records them in the own info section of
// the kernel. They go into `exits`, which has room for `room` of them, and the return is the count found. It is 0
// where the pattern holds no such kernel, or its info carries no exit. The call reads the own record of the cubin, and
// names no instruction to find one
unsigned int cubin_exits_read(const unsigned char *pattern, unsigned long long pattern_size, const char *kernel,
                              unsigned int *exits, unsigned int room);

// how many registers a thread of `kernel` uses in `pattern`, which is `pattern_size` bytes. Its code section carries
// the count in the top byte of its info. It is 0 where the pattern holds no such kernel, or a table, a section or a
// name lies past its end
unsigned int cubin_registers_read(const unsigned char *pattern, unsigned long long pattern_size, const char *kernel);

// every code section of `cubin`, which is `size` bytes, each a section named .text.<function>. The offset of each goes
// through `offsets` and its length through `sizes`, which have room for `room`. The return is the count found. It is 0
// where there is none or more than `room`, or where a header, a name or a section lies past the cubin end
unsigned int cubin_code_sections(const unsigned char *cubin, unsigned long long size, unsigned long long *offsets,
                                 unsigned long long *sizes, unsigned int room);

// every section of the constant bank of `cubin`, which is `size` bytes, each named .nv.constant0.<function>. It is as
// cubin_code_sections gives the sections of the code: the parameter bank of each kernel, in the order of the kernels
unsigned int cubin_constant_sections(const unsigned char *cubin, unsigned long long size, unsigned long long *offsets,
                                     unsigned long long *sizes, unsigned int room);

#endif
