// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inputs.c: the writer's inputs: the writer's own binary, a pause, a name, the input read whole, the names of a carry ordered and read, and a run carried into this one
#include "qry_writer_internal.h"

void qry_writer_binary(void)
{
#if defined(_WIN32)
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
}

void qry_writer_pause(void)
{
#if defined(_WIN32)
    Sleep(10u);
#else
    const struct timespec pause = {0, 10000000};
    nanosleep(&pause, NULL);
#endif
}

// the .qry named after the words, or the one QRY names
const char *qry_writer_named(int count, char **words, int at)
{
    return (count > at) ? words[at] : getenv(QRY_ENVIRONMENT);
}

// every byte of the standard input, in memory of the caller's to free
unsigned char *qry_writer_input(unsigned long long *size)
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

static int qry_writer_name_order(const void *left, const void *right)
{
    return strcmp(*(char *const *)left, *(char *const *)right);
}

// the names asked for put in order and each kept once, for qry_writer_carried to find a blob's name among them
void qry_writer_names_ordered(QryWriterCarry *carry)
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
int qry_writer_names_read(unsigned char **bytes, char ***names)
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

void qry_writer_carried(void *context, const char *name, const unsigned char *bytes, unsigned long long size)
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
