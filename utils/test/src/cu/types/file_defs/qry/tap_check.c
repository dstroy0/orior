// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tap_check.c: the writer's tap held: a reader on the pipe reads one line a blob as the writer places it
//
//   lines     six ordinary blobs and a panic handed give seven lines, each naming its blob and its size
//   first     the panic's line comes before the lines of the two blobs handed after it
//   rising    the ordinary blobs' sequences rise in the order handed
//   name      the tap's name is the pipe named from the hash of the .qry's path
//
//     tap_check <folder>
//
// The check makes the buffer itself in <folder> and runs the writer in a thread of its own; the reader is this process
#include "../../../../../../../src/cu/types/file_defs/qry/buffer/qry_buffer_internal.h"

#include <pthread.h>

#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#endif

#define CHECK_LINES 16u

static unsigned int s_checks = 0u;
static unsigned int s_failed = 0u;

static void check_that(int held, const char *what)
{
    s_checks += 1u;
    if (held == 0)
    {
        s_failed += 1u;
        printf("  FAILED: %s\n", what);
    }
}

static void *check_writer(void *argument)
{
    qry_write_until_stopped((QryBuffer *)argument);
    return NULL;
}

// the tap's lines read to the pipe's end, up to CHECK_LINES: the count read
static unsigned int check_tap_read(const char *name, char lines[][512])
{
    static char text[CHECK_LINES * 512u];
    size_t held = 0u;
#if defined(_WIN32)
    HANDLE pipe = INVALID_HANDLE_VALUE;
    for (unsigned int tried = 0u; (pipe == INVALID_HANDLE_VALUE) && (tried < 200u); tried += 1u)
    {
        pipe = CreateFileA(name, GENERIC_READ, 0u, NULL, OPEN_EXISTING, 0u, NULL);
        if (pipe == INVALID_HANDLE_VALUE)
        {
            Sleep(5u);
        }
    }
    if (pipe == INVALID_HANDLE_VALUE)
    {
        return 0u;
    }
    DWORD read = 0u;
    while ((held < (sizeof(text) - 1u)) && ReadFile(pipe, text + held, (DWORD)(sizeof(text) - 1u - held), &read, NULL) &&
           (read != 0u))
    {
        held += read;
    }
    CloseHandle(pipe);
#else
    const int pipe = open(name, O_RDONLY);
    if (pipe < 0)
    {
        return 0u;
    }
    ssize_t read_now = 0;
    while ((held < (sizeof(text) - 1u)) && ((read_now = read(pipe, text + held, sizeof(text) - 1u - held)) > 0))
    {
        held += (size_t)read_now;
    }
    close(pipe);
#endif
    text[held] = '\0';
    unsigned int count = 0u;
    for (char *at = text; (*at != '\0') && (count < CHECK_LINES);)
    {
        char *const end = strchr(at, '\n');
        const size_t length = (end != NULL) ? (size_t)(end - at) : strlen(at);
        snprintf(lines[count], 512u, "%.*s", (int)((length < 511u) ? length : 511u), at);
        count += 1u;
        at += length + ((end != NULL) ? 1u : 0u);
    }
    return count;
}

// a tap line read apart: its sequence, priority, name and size
static int check_line_read(const char *line, unsigned long long *sequence, unsigned int *priority, char *name,
                           unsigned long long *size)
{
    unsigned long long nanoseconds = 0ull;
    char member[16];
    return sscanf(line, "%llu %llu %u %15s %255s %llu", sequence, &nanoseconds, priority, member, name, size) == 6;
}

typedef struct
{
    QryBuffer *buffer;
    const char *name;
} CheckHands;

// the blobs handed from another thread once the reader is on the pipe: three ordinary, a panic, two ordinary
static void *check_hands(void *argument)
{
    CheckHands *const hands = (CheckHands *)argument;
#if defined(_WIN32)
    Sleep(50u);
#else
    usleep(50000u);
#endif
    static const char *const ordinary[] = {"one.txt", "two.txt", "three.txt", "four.txt", "five.txt", "six.txt"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        qry_hand(hands->buffer, ordinary[at], "abc", 3ull, QRY_ORDINARY);
    }
    qry_moved(hands->buffer);
    qry_hand(hands->buffer, "panic.txt", "faulted", 7ull, QRY_PANIC);
    // the writer woken for the panic writes it ahead of everything waiting; the blobs after it are handed once it has
#if defined(_WIN32)
    Sleep(100u);
#else
    usleep(100000u);
#endif
    for (unsigned int at = 3u; at < 6u; at += 1u)
    {
        qry_hand(hands->buffer, ordinary[at], "abcde", 5ull, QRY_ORDINARY);
    }
    qry_moved(hands->buffer);
    qry_stop(hands->buffer, 5000u);
    return NULL;
}

int main(int count, char **words)
{
    if (count != 2)
    {
        printf("  tap_check <folder>\n");
        return 2;
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s/tap.qry", words[1]);
    remove(path);
    QryBuffer *const buffer = qry_create(path);
    if (buffer == NULL)
    {
        printf("  the buffer of %s could not be made\n", path);
        return 2;
    }
    char name[128];
    qry_tap_name(path, name, sizeof(name));
    char expected[128];
    const unsigned long long hash = qry_hash(path, strlen(path));
#if defined(_WIN32)
    snprintf(expected, sizeof(expected), "\\\\.\\pipe\\qry-%016llx-tap", hash);
#else
    snprintf(expected, sizeof(expected), "/tmp/qry-%016llx-tap", hash);
#endif
    check_that(strcmp(name, expected) == 0, "the tap is named from the hash of the .qry's path");
    check_that(qry_tap_opened(buffer), "the writer opens its tap");
    pthread_t writer;
    pthread_create(&writer, NULL, check_writer, buffer);
    CheckHands hands = {buffer, name};
    pthread_t hander;
    pthread_create(&hander, NULL, check_hands, &hands);
    static char lines[CHECK_LINES][512];
    const unsigned int read = check_tap_read(name, lines);
    pthread_join(hander, NULL);
    pthread_join(writer, NULL);
    unsigned int ordinary = 0u;
    unsigned int panics = 0u;
    int panic_before_four = 0;
    int seen_four = 0;
    int sized = 1;
    int rising = 1;
    unsigned long long last_ordinary = 0ull;
    for (unsigned int at = 0u; at < read; at += 1u)
    {
        unsigned long long sequence = 0ull;
        unsigned int priority = 0u;
        char blob[256];
        unsigned long long size = 0ull;
        if (!check_line_read(lines[at], &sequence, &priority, blob, &size))
        {
            continue;
        }
        if (priority == QRY_PANIC)
        {
            panics += 1u;
            panic_before_four = !seen_four;
            sized = sized && (size == 7ull) && (strcmp(blob, "panic.txt") == 0);
        }
        else if (strstr(blob, ".txt") != NULL && strcmp(blob, "writer") != 0)
        {
            ordinary += 1u;
            seen_four = seen_four || (strcmp(blob, "four.txt") == 0);
            sized = sized && ((size == 3ull) || (size == 5ull));
            rising = rising && (sequence > last_ordinary);
            last_ordinary = sequence;
        }
    }
    char what[160];
    snprintf(what, sizeof(what), "six ordinary lines and one panic (%u and %u of %u lines)", ordinary, panics, read);
    check_that((ordinary == 6u) && (panics == 1u), what);
    check_that(panic_before_four, "the panic's line comes before the lines of the blobs handed after it");
    check_that(sized, "each line carries its blob's name and size");
    check_that(rising, "the ordinary blobs' sequences rise in the order handed");
    qry_close(buffer);
    printf("tap_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
