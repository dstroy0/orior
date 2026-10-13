// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_writer_internal.h: what the modules of the writer process share: its includes, the stop and ready waits, what a carry holds, and the functions one module reads from another
#ifndef QRY_WRITER_INTERNAL_H
#define QRY_WRITER_INTERNAL_H

#if !defined(_WIN32)
// nanosleep is POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
#include "../qry_buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <time.h>
#endif


// the most a stop waits for the writer to write the rest, and a ready for it to make the buffer
#define QRY_WRITER_STOP_MILLISECONDS 600000u

#define QRY_WRITER_READY_MILLISECONDS 30000u

// what a carry or a file's latest is handed each name of a finished .qry with: the run's buffer to hand it to, NULL
// where it is written to the standard output, the names asked for in the order qry_writer_name_order puts them, each
// once, every name where there are none, which of them were found, and whether each went where it was sent
typedef struct
{
    QryBuffer *run;
    char **names;
    int name_count;
    int *found;
    unsigned long long carried;
    int sent;
} QryWriterCarry;


void qry_writer_binary(void);
void qry_writer_pause(void);
const char *qry_writer_named(int count, char **words, int at);
unsigned char *qry_writer_input(unsigned long long *size);
void qry_writer_names_ordered(QryWriterCarry *carry);
int qry_writer_names_read(unsigned char **bytes, char ***names);
void qry_writer_carried(void *context, const char *name, const unsigned char *bytes, unsigned long long size);

#endif
