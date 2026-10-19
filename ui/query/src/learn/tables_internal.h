// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tables_internal.h: what tables.c and tables_pull.c share: the tables' folders, the harness's image, the key kernel,
// and the calls one reads from the other
#ifndef TABLES_INTERNAL_H
#define TABLES_INTERNAL_H

#include "../query_loops.h"

// the tables' folder: the key cubins, the vendor's disassembler for Linux, the captures and the tables
#define QUERY_TABLES_OUT "build/vendor/nvidia"

#define QUERY_TABLES_SOURCE "src/cu/transpiler/vendor_bin_layouts/nvidia/tables"

// the harness's Ubuntu image, which has the C compiler the hook builds with
#define QUERY_TABLES_IMAGE "orior-test-system-ubuntu:latest"

// the key kernel: three lines whose content the disassembler reads the architecture off, and nothing else
#define QUERY_TABLES_KEY "__global__ void kernel(float *out) { out[0] = 0; }\n"

// the most architectures nvcc lists
#define QUERY_ARCHITECTURES_MOST 64u

void query_tables_in(const char *folder, const char *name, char *path);
void query_tables_of(const char *out, const char *arch, char *instructions, char *latencies);
void query_in_container(const char *root, const char *path, char *inside);
int query_program(const QueryContext *context, QueryRole role, char *path, CliLine *said);
unsigned int query_architectures(QueryContext *context, const QueryRun *run, const char *const *words,
                                 unsigned int count, char (*arches)[16], CliLine *said);
int query_disassembler(QueryContext *context, const QueryRun *run, const char *out, char *program, CliLine *said);
int query_key_cubin(QueryContext *context, const QueryRun *run, const char *out, const char *arch, char *cubin,
                    CliLine *said);

// the compiler's version, `V13.3.73`, read off `nvcc --version` into `version`: 1, or 0 where it names none; the
// architectures `nvcc --list-gpu-arch` lists, `compute_86` read as `sm_86`, into `arches`: their count; and the
// vendor's archive of nvdisasm at `version` named into `archive`, which holds `room` (tables.c)
int query_version_of(const unsigned char *said, unsigned long long size, char *version, size_t room);
unsigned int query_architectures_of(const unsigned char *said, unsigned long long size, char (*arches)[16]);
void query_archive_named(const char *version, char *archive, size_t room);

#endif
