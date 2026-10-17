// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// blob_said_check.c: the writer says each blob it places, one line a blob, in the order it places them
//
//   lines     six ordinary blobs and a panic handed give seven lines, each naming its blob and its size
//   first     the panic's line comes before the lines of the blobs handed after it
//   rising    the sequences rise in the order the lines are said
//   member    a blob the writer's own process hands goes in past every ring, and its line names no member
//   none      a buffer the writer does not write says nothing
//
//     blob_said_check <folder>
//
// The check makes the buffer itself in <folder>, runs the writer in a thread of its own saying into a file in the
// folder, and hands from another thread as a member
#include "../../../../../../../src/cu/types/file_defs/qry/buffer/qry_buffer_internal.h"

#include <pthread.h>

#if !defined(_WIN32)
#include <unistd.h>
#endif

#define CHECK_LINES 32u

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

static void check_paused(unsigned int milliseconds)
{
#if defined(_WIN32)
    Sleep(milliseconds);
#else
    usleep(milliseconds * 1000u);
#endif
}

static void *check_writer(void *argument)
{
    qry_write_until_stopped((QryBuffer *)argument);
    return NULL;
}

// three ordinary blobs, a panic once they are in, and three ordinary after it, then the stop
static void *check_hands(void *argument)
{
    QryBuffer *const buffer = (QryBuffer *)argument;
    check_paused(50u);
    static const char *const ordinary[] = {"one.txt", "two.txt", "three.txt", "four.txt", "five.txt", "six.txt"};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        qry_hand(buffer, ordinary[at], "abc", 3ull, QRY_ORDINARY);
    }
    qry_moved(buffer);
    qry_hand(buffer, "panic.txt", "faulted", 7ull, QRY_PANIC);
    // the writer woken for the panic writes it ahead of everything waiting; the blobs after it are handed once it has
    check_paused(100u);
    for (unsigned int at = 3u; at < 6u; at += 1u)
    {
        qry_hand(buffer, ordinary[at], "abcde", 5ull, QRY_ORDINARY);
    }
    qry_moved(buffer);
    qry_stop(buffer, 5000u);
    return NULL;
}

int main(int count, char **words)
{
    if (count != 2)
    {
        printf("  blob_said_check <folder>\n");
        return 2;
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s/blob_said.qry", words[1]);
    char said_path[1024];
    snprintf(said_path, sizeof(said_path), "%s/blob_said.txt", words[1]);
    remove(path);
    QryBuffer *const buffer = qry_create(path);
    if (buffer == NULL)
    {
        printf("  the buffer of %s could not be made\n", path);
        return 2;
    }
    check_that(buffer->saying == NULL, "a buffer the writer does not write says nothing");
    FILE *const said = fopen(said_path, "w");
    if (said == NULL)
    {
        printf("  %s could not be opened\n", said_path);
        return 2;
    }
    buffer->saying = said;
    pthread_t writer;
    pthread_create(&writer, NULL, check_writer, buffer);
    pthread_t hander;
    pthread_create(&hander, NULL, check_hands, buffer);
    pthread_join(hander, NULL);
    pthread_join(writer, NULL);
    fclose(said);
    check_that(buffer->saying == NULL, "and the writer says nothing once it has stopped");
    qry_close(buffer);
    FILE *const lines = fopen(said_path, "r");
    if (lines == NULL)
    {
        printf("  %s could not be read\n", said_path);
        return 2;
    }
    unsigned int ordinary = 0u;
    unsigned int panics = 0u;
    unsigned int read = 0u;
    int panic_before_four = 0;
    int seen_four = 0;
    int sized = 1;
    int rising = 1;
    int membered = 1;
    unsigned long long last = 0ull;
    char line[512];
    while ((read < CHECK_LINES) && (fgets(line, sizeof(line), lines) != NULL))
    {
        unsigned long long sequence = 0ull;
        unsigned long long nanoseconds = 0ull;
        unsigned int priority = 0u;
        unsigned int member = 0u;
        unsigned long long size = 0ull;
        char name[256];
        if (sscanf(line, "qry_blob %llu %llu %u %u %llu %255s", &sequence, &nanoseconds, &priority, &member, &size,
                   name) != 6)
        {
            continue;
        }
        read += 1u;
        rising = rising && (sequence > last);
        last = sequence;
        if (priority == QRY_PANIC)
        {
            panics += 1u;
            panic_before_four = !seen_four;
            sized = sized && (size == 7ull) && (strcmp(name, "panic.txt") == 0);
            membered = membered && (member == QRY_MEMBERS);
            continue;
        }
        ordinary += 1u;
        seen_four = seen_four || (strcmp(name, "four.txt") == 0);
        sized = sized && ((size == 3ull) || (size == 5ull));
        membered = membered && (member == QRY_MEMBERS);
    }
    fclose(lines);
    char what[160];
    snprintf(what, sizeof(what), "six ordinary lines and one panic (%u and %u of %u lines)", ordinary, panics, read);
    check_that((ordinary == 6u) && (panics == 1u), what);
    check_that(panic_before_four, "the panic's line comes before the lines of the blobs handed after it");
    check_that(sized, "each line carries its blob's name and size");
    check_that(rising, "the sequences rise in the order the lines are said");
    check_that(membered, "a blob the writer's own process hands names no member");
    printf("blob_said_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
