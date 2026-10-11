// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_writer.c: a query run's writer, the process that owns the run's buffer and alone writes its .qry (qry_buffer.h),
// and the words a script reaches the buffer with
//
//     qry_writer start <qry>          the buffer made and written to <qry> until the run is stopped; a script starts it
//                                     in the background, and `ready` once it says the buffer is made
//     qry_writer ready [<qry>]        waits for the run's writer to make the buffer: 0 once it has, 1 where it does not
//                                     within 30 seconds
//     qry_writer stop [<qry>]         the writer asked to write the rest and stop, and waited for
//     qry_writer hand <name> [<priority>]
//                                     every byte of the standard input handed as one blob of <name> at <priority>, 0 a
//                                     panic, 1 critical and on disk before it returns, 2 ordinary where none is given
//     qry_writer read <name>          every blob of <name> joined to the standard output, from the buffer while the
//                                     writer runs and from the .qry once it has stopped, a stretch of the file no tag
//                                     opens stepped over and said
//     qry_writer latest <name>        the latest blob of <name> to the standard output, from the buffer while the
//                                     writer runs and from the .qry once it has stopped
//     qry_writer first <name>         the first blob of <name> handed in the run to the standard output, from the
//                                     buffer while the writer runs
//     qry_writer runs [<qry>]         0 where the run's writer runs now, 1 where none does; nothing is waited for
//     qry_writer carry <qry> [<name>... | -]
//                                     the latest blob of each <name> in the finished .qry at <qry> handed to the run,
//                                     ordinary, in the order they were handed there; every name's where none is given,
//                                     and the names the standard input holds, one a line, where the one given is `-`.
//                                     1 where a name given is in none of its blobs
//
// Where no .qry is given, QRY names it, the string every process of the run names it by. Exit 0 where it was done, 1
// where it was not, 2 where the words are none of the above
#if !defined(_WIN32)
// nanosleep is POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
#include "qry_buffer.h"

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

static void qry_writer_binary(void)
{
#if defined(_WIN32)
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
}

static void qry_writer_pause(void)
{
#if defined(_WIN32)
    Sleep(10u);
#else
    const struct timespec pause = {0, 10000000};
    nanosleep(&pause, NULL);
#endif
}

// the .qry named after the words, or the one QRY names
static const char *qry_writer_named(int count, char **words, int at)
{
    return (count > at) ? words[at] : getenv(QRY_ENVIRONMENT);
}

// every byte of the standard input, in memory of the caller's to free
static unsigned char *qry_writer_input(unsigned long long *size)
{
    unsigned long long room = 65536ull;
    unsigned char *bytes = (unsigned char *)malloc((size_t)room);
    *size = 0ull;
    while (bytes != NULL)
    {
        if (*size == room)
        {
            room *= 2ull;
            unsigned char *const larger = (unsigned char *)realloc(bytes, (size_t)room);
            if (larger == NULL)
            {
                free(bytes);
                return NULL;
            }
            bytes = larger;
        }
        const size_t read = fread(bytes + *size, 1u, (size_t)(room - *size), stdin);
        if (read == 0u)
        {
            break;
        }
        *size += read;
    }
    return bytes;
}

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

static int qry_writer_name_order(const void *left, const void *right)
{
    return strcmp(*(char *const *)left, *(char *const *)right);
}

// the names asked for put in order and each kept once, for qry_writer_carried to find a blob's name among them
static void qry_writer_names_ordered(QryWriterCarry *carry)
{
    qsort(carry->names, (size_t)carry->name_count, sizeof(char *), qry_writer_name_order);
    int kept = 0;
    for (int at = 0; at < carry->name_count; at += 1)
    {
        if ((kept == 0) || (strcmp(carry->names[kept - 1], carry->names[at]) != 0))
        {
            carry->names[kept] = carry->names[at];
            kept += 1;
        }
    }
    carry->name_count = kept;
}

// the names the standard input holds, one a line, read into memory of the caller's to free, `*bytes`, and listed in
// `*names`, also the caller's to free: their count, or -1 where there was no memory
static int qry_writer_names_read(unsigned char **bytes, char ***names)
{
    unsigned long long size = 0ull;
    *bytes = qry_writer_input(&size);
    size_t lines = 1u;
    for (unsigned long long at = 0ull; (*bytes != NULL) && (at < size); at += 1ull)
    {
        lines += ((*bytes)[at] == '\n') ? 1u : 0u;
    }
    unsigned char *const larger = (*bytes != NULL) ? (unsigned char *)realloc(*bytes, (size_t)size + 1u) : NULL;
    *names = (larger != NULL) ? (char **)calloc(lines, sizeof(char *)) : NULL;
    if (*names == NULL)
    {
        free((larger != NULL) ? larger : *bytes);
        *bytes = NULL;
        return -1;
    }
    *bytes = larger;
    (*bytes)[size] = '\0';
    int count = 0;
    char *line = (char *)*bytes;
    while (*line != '\0')
    {
        char *const end = strchr(line, '\n');
        char *const next = (end != NULL) ? (end + 1) : (line + strlen(line));
        if (end != NULL)
        {
            *end = '\0';
        }
        line[strcspn(line, "\r")] = '\0';
        if (line[0] != '\0')
        {
            (*names)[count] = line;
            count += 1;
        }
        line = next;
    }
    return count;
}

static void qry_writer_carried(void *context, const char *name, const unsigned char *bytes, unsigned long long size)
{
    QryWriterCarry *const carry = (QryWriterCarry *)context;
    if (carry->name_count != 0)
    {
        char *const *const named = (char *const *)bsearch(&name, carry->names, (size_t)carry->name_count,
                                                          sizeof(char *), qry_writer_name_order);
        if (named == NULL)
        {
            return;
        }
        carry->found[named - carry->names] = 1;
    }
    const int sent = (carry->run != NULL) ? (qry_hand(carry->run, name, bytes, size, QRY_ORDINARY) != 0ull)
                                          : (fwrite(bytes, 1u, (size_t)size, stdout) == size);
    carry->sent &= sent;
    carry->carried += sent ? 1ull : 0ull;
}

int main(int count, char **words)
{
    if (count < 2)
    {
        fprintf(stderr,
                "qry_writer start <qry> | ready [<qry>] | stop [<qry>] | hand <name> [<priority>] | read <name> | "
                "latest <name> | first <name> | runs [<qry>] | carry <qry> [<name>... | -]\n");
        return 2;
    }
    const char *const word = words[1];
    if ((strcmp(word, "start") == 0) && (count == 3))
    {
        QryBuffer *const buffer = qry_create(words[2]);
        if (buffer == NULL)
        {
            fprintf(stderr, "qry_writer: the buffer of %s could not be made, or its writer runs already\n", words[2]);
            return 1;
        }
        printf("  qry_writer: the buffer of %s is made\n", words[2]);
        fflush(stdout);
        const int written = qry_write_until_stopped(buffer);
        qry_close(buffer);
        return written ? 0 : 1;
    }
    const char *const path =
        ((strcmp(word, "ready") == 0) || (strcmp(word, "stop") == 0) || (strcmp(word, "runs") == 0))
            ? qry_writer_named(count, words, 2)
            : getenv(QRY_ENVIRONMENT);
    if ((path == NULL) || (path[0] == '\0'))
    {
        fprintf(stderr, "qry_writer: no .qry is given and %s names none\n", QRY_ENVIRONMENT);
        return 2;
    }
    if (strcmp(word, "runs") == 0)
    {
        QryBuffer *const buffer = qry_open(path);
        qry_close(buffer);
        return (buffer != NULL) ? 0 : 1;
    }
    if ((strcmp(word, "carry") == 0) && (count >= 3))
    {
        qry_writer_binary();
        unsigned char *listed = NULL;
        char **names = NULL;
        const int from_input = (count == 4) && (strcmp(words[3], "-") == 0);
        const int name_count = from_input ? qry_writer_names_read(&listed, &names) : (count - 3);
        // a list of no names carries none, where no list carries every name
        if (from_input && (name_count == 0))
        {
            free(names);
            free(listed);
            printf("  qry_writer: no name was given to carry from %s\n", words[2]);
            return 0;
        }
        int *const found = (name_count >= 0) ? (int *)calloc((size_t)name_count + 1u, sizeof(int)) : NULL;
        QryWriterCarry carry = {qry_open(path), from_input ? names : (words + 3), name_count, found, 0ull, 1};
        char error[512] = "";
        if (carry.found != NULL)
        {
            qry_writer_names_ordered(&carry);
        }
        const int read = (carry.run != NULL) && (carry.found != NULL) &&
                         qry_file_latest_each(words[2], qry_writer_carried, &carry, error, sizeof(error));
        int whole = read && carry.sent;
        for (int at = 0; read && (at < carry.name_count); at += 1)
        {
            if (!carry.found[at])
            {
                fprintf(stderr, "qry_writer: %s holds no blob %s\n", words[2], carry.names[at]);
                whole = 0;
            }
        }
        if ((carry.run == NULL) || !carry.sent || (error[0] != '\0'))
        {
            fprintf(stderr, "qry_writer: %s\n",
                    (carry.run == NULL) ? "no writer runs for the run to carry into"
                                        : ((error[0] != '\0') ? error : "a blob was not handed to the run"));
        }
        printf("  qry_writer: %llu blob(s) of %s carried into %s\n", carry.carried, words[2], path);
        free(found);
        free(names);
        free(listed);
        qry_close(carry.run);
        return whole ? 0 : 1;
    }
    if (strcmp(word, "ready") == 0)
    {
        for (unsigned int waited = 0u; waited < QRY_WRITER_READY_MILLISECONDS; waited += 10u)
        {
            QryBuffer *const buffer = qry_open(path);
            if (buffer != NULL)
            {
                qry_close(buffer);
                return 0;
            }
            qry_writer_pause();
        }
        fprintf(stderr, "qry_writer: no writer made the buffer of %s\n", path);
        return 1;
    }
    if (strcmp(word, "stop") == 0)
    {
        QryBuffer *const buffer = qry_open(path);
        if (buffer == NULL)
        {
            fprintf(stderr, "qry_writer: no writer runs for %s\n", path);
            return 1;
        }
        const int stopped = qry_stop(buffer, QRY_WRITER_STOP_MILLISECONDS);
        qry_close(buffer);
        if (!stopped)
        {
            fprintf(stderr, "qry_writer: the writer of %s did not stop\n", path);
        }
        return stopped ? 0 : 1;
    }
    const int prioritized = (count == 4) && (strlen(words[count - 1]) == 1u) && (words[count - 1][0] >= '0') &&
                            (words[count - 1][0] <= ('0' + (int)QRY_ORDINARY));
    if ((strcmp(word, "hand") == 0) && ((count == 3) || prioritized))
    {
        qry_writer_binary();
        const unsigned int priority = prioritized ? (unsigned int)(words[3][0] - '0') : QRY_ORDINARY;
        QryBuffer *const buffer = qry_open(path);
        unsigned long long size = 0ull;
        unsigned char *const bytes = (buffer != NULL) ? qry_writer_input(&size) : NULL;
        const unsigned long long sequence = (bytes != NULL) ? qry_hand(buffer, words[2], bytes, size, priority) : 0ull;
        free(bytes);
        qry_close(buffer);
        if (sequence == 0ull)
        {
            fprintf(stderr, "qry_writer: %s was not handed to %s\n", words[2], path);
        }
        return (sequence != 0ull) ? 0 : 1;
    }
    if (((strcmp(word, "read") == 0) || (strcmp(word, "latest") == 0)) && (count == 3))
    {
        qry_writer_binary();
        QryBuffer *const buffer = qry_open(path);
        if ((strcmp(word, "latest") == 0) && (buffer == NULL))
        {
            QryWriterCarry carry = {NULL, words + 2, 1, (int[1]){0}, 0ull, 1};
            char error[512] = "";
            const int read = qry_file_latest_each(path, qry_writer_carried, &carry, error, sizeof(error));
            if (!read || (error[0] != '\0') || !carry.found[0])
            {
                fprintf(stderr, "qry_writer: %s\n", (error[0] != '\0') ? error : "the .qry holds no blob of the name");
            }
            return (read && carry.found[0] && carry.sent) ? 0 : 1;
        }
        if (strcmp(word, "latest") == 0)
        {
            const unsigned char *bytes = NULL;
            unsigned long long size = 0ull;
            const int found = qry_latest(buffer, words[2], &bytes, &size, NULL);
            const int out = found && (fwrite(bytes, 1u, (size_t)size, stdout) == size);
            qry_close(buffer);
            if (!found)
            {
                fprintf(stderr, "qry_writer: %s holds no blob %s\n", path, words[2]);
            }
            return out ? 0 : 1;
        }
        unsigned char *bytes = NULL;
        unsigned long long size = 0ull;
        char error[512] = "";
        const int joined = (buffer != NULL) ? qry_joined(buffer, words[2], &bytes, &size)
                                            : qry_file_joined(path, words[2], &bytes, &size, error, sizeof(error));
        const int out = joined && ((size == 0ull) || (fwrite(bytes, 1u, (size_t)size, stdout) == size));
        free(bytes);
        qry_close(buffer);
        if (!joined || (error[0] != '\0'))
        {
            fprintf(stderr, "qry_writer: %s\n", (error[0] != '\0') ? error : "the blobs could not be joined");
        }
        return out ? 0 : 1;
    }
    if ((strcmp(word, "first") == 0) && (count == 3))
    {
        qry_writer_binary();
        QryBuffer *const buffer = qry_open(path);
        const unsigned char *bytes = NULL;
        unsigned long long size = 0ull;
        const int found = qry_first(buffer, words[2], &bytes, &size);
        const int out = found && ((size == 0ull) || (fwrite(bytes, 1u, (size_t)size, stdout) == size));
        qry_close(buffer);
        if (!found)
        {
            fprintf(stderr, "qry_writer: %s\n",
                    (buffer == NULL) ? "no writer runs for the run to read from" : "the run holds no blob of the name");
        }
        return out ? 0 : 1;
    }
    fprintf(stderr,
            "qry_writer: %s is none of start, ready, stop, hand, read, latest, first, runs and carry as given\n", word);
    return 2;
}
