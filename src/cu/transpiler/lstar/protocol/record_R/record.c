// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// record.c: R read whole, asked of by an ask's identity, kept, and written back whole
#include "record.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the count of words of an ask's identity, and the most letters it takes
#define RECORD_IDENTITY_WORDS 7u
#define RECORD_IDENTITY 160u
// the slots of the table an ask is found by its identity in, a power of two past every ask a record holds
#define RECORD_SLOTS 1048576u

typedef struct
{
    // every line as it was read and as this run added it, each its own allocation
    char **line;
    unsigned int lines;
    unsigned int room;
    // the line each identity's ask stands at, plus one, 0 for a slot that holds none
    unsigned int *slot;
    char path[1024];
    char mode[64];
    unsigned int cycles;
    int marked;
    int changed;
    int open;
} Record;

static Record s_record;

// a 64-bit FNV-1a hash of `text`
static unsigned long long record_text_hash(const char *text)
{
    unsigned long long hash = 0xcbf29ce484222325ull;
    for (const char *at = text; *at != '\0'; at += 1)
    {
        hash = (hash ^ (unsigned char)*at) * 0x100000001b3ull;
    }
    return hash;
}

// the identity of the ask or sample line `line`, the seven words after its kind, into `identity`: 1, or 0 where it
// has no seven
static int record_line_identity(const char *line, char *identity)
{
    const char *at = strchr(line, ' ');
    if (at == NULL)
    {
        return 0;
    }
    at += 1;
    const char *end = at;
    for (unsigned int word = 0u; word < RECORD_IDENTITY_WORDS; word += 1u)
    {
        end = strchr(end, ' ');
        if (end == NULL)
        {
            return 0;
        }
        end += (word + 1u < RECORD_IDENTITY_WORDS) ? 1 : 0;
    }
    const size_t length = (size_t)(end - at);
    if (length >= RECORD_IDENTITY)
    {
        return 0;
    }
    memcpy(identity, at, length);
    identity[length] = '\0';
    return 1;
}

// the slot of `identity`: the one holding it, or the empty one it would be put in
static unsigned int record_slot_of(const char *identity)
{
    unsigned int at = (unsigned int)(record_text_hash(identity) & (RECORD_SLOTS - 1u));
    while (s_record.slot[at] != 0u)
    {
        char held[RECORD_IDENTITY];
        if (record_line_identity(s_record.line[s_record.slot[at] - 1u], held) && (strcmp(held, identity) == 0))
        {
            return at;
        }
        at = (at + 1u) & (RECORD_SLOTS - 1u);
    }
    return at;
}

// `text` taken as a line of its own after every line held: 1, or 0 where it could not be held
static int record_line_add(const char *text)
{
    if (s_record.lines == s_record.room)
    {
        const unsigned int room = (s_record.room == 0u) ? 1024u : (2u * s_record.room);
        char **const grown = (char **)realloc(s_record.line, (size_t)room * sizeof(char *));
        if (grown == NULL)
        {
            return 0;
        }
        s_record.line = grown;
        s_record.room = room;
    }
    const size_t length = strlen(text);
    char *const held = (char *)malloc(length + 1u);
    if (held == NULL)
    {
        return 0;
    }
    memcpy(held, text, length + 1u);
    s_record.line[s_record.lines] = held;
    s_record.lines += 1u;
    return 1;
}

int record_open(const char *path, const char *member, const char *mode)
{
    memset(&s_record, 0, sizeof(s_record));
    s_record.slot = (unsigned int *)calloc(RECORD_SLOTS, sizeof(unsigned int));
    if (s_record.slot == NULL)
    {
        printf("  record: no room for the table of asks\n");
        return 0;
    }
    snprintf(s_record.path, sizeof(s_record.path), "%s", path);
    snprintf(s_record.mode, sizeof(s_record.mode), "%s", mode);
    s_record.open = 1;
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        char first[160];
        snprintf(first, sizeof(first), "kqr %s", member);
        return record_line_add(first);
    }
    static char s_text[1u << 20u];
    unsigned int number = 0u;
    while (fgets(s_text, sizeof(s_text), file) != NULL)
    {
        number += 1u;
        s_text[strcspn(s_text, "\r\n")] = '\0';
        char identity[RECORD_IDENTITY];
        const int asked = (strncmp(s_text, "ask ", 4u) == 0);
        if ((asked || (strncmp(s_text, "sample ", 7u) == 0)) && !record_line_identity(s_text, identity))
        {
            printf("  record: %s:%u: an ask with no identity\n", path, number);
            fclose(file);
            return 0;
        }
        s_record.cycles += (strncmp(s_text, "cycle ", 6u) == 0) ? 1u : 0u;
        if (!record_line_add(s_text))
        {
            printf("  record: %s:%u could not be held\n", path, number);
            fclose(file);
            return 0;
        }
        if (asked)
        {
            const unsigned int at = record_slot_of(identity);
            s_record.slot[at] = (s_record.slot[at] == 0u) ? s_record.lines : s_record.slot[at];
        }
    }
    fclose(file);
    if (s_record.lines == 0u)
    {
        char first[160];
        snprintf(first, sizeof(first), "kqr %s", member);
        return record_line_add(first);
    }
    return 1;
}

int record_held(void)
{
    return s_record.open;
}

// what the ask line `line` holds, into `held`: 1, or 0 where its answer is none R holds
static int record_line_read(const char *line, RecordAsk *held)
{
    memset(held, 0, sizeof(*held));
    const char *at = line;
    for (unsigned int word = 0u; (word < (RECORD_IDENTITY_WORDS + 1u)) && (at != NULL); word += 1u)
    {
        at = strchr(at, ' ');
        at = (at != NULL) ? (at + 1) : NULL;
    }
    if (at == NULL)
    {
        return 0;
    }
    const size_t length = strcspn(at, " ");
    if ((length == 0u) || (length >= sizeof(held->answer)))
    {
        return 0;
    }
    memcpy(held->answer, at, length);
    held->answer[length] = '\0';
    at += length;
    at += (*at == ' ') ? 1 : 0;
    if (strcmp(held->answer, "answers") == 0)
    {
        char *after = NULL;
        while ((*at != '\0') && (held->words < RECORD_WORDS))
        {
            held->word[held->words] = strtoull(at, &after, 16);
            if (after == at)
            {
                break;
            }
            held->words += 1u;
            at = after;
            at += (*at == ' ') ? 1 : 0;
        }
        return 1;
    }
    snprintf(held->refusal, sizeof(held->refusal), "%s", at);
    return 1;
}

int record_answer(const char *identity, RecordAsk *held)
{
    if (!s_record.open)
    {
        return 0;
    }
    const unsigned int at = record_slot_of(identity);
    if ((s_record.slot[at] == 0u) || !record_line_read(s_record.line[s_record.slot[at] - 1u], held))
    {
        return 0;
    }
    return (strcmp(held->answer, "timed_out") != 0) ? 1 : 0;
}

// the line of an ask or a sample, `kind`, `identity`, `answer`, and then `rest`, into a buffer of its own
static char *record_line_written(const char *kind, const char *identity, const char *answer, const char *rest)
{
    static char s_line[(RECORD_WORDS * 17u) + 512u];
    snprintf(s_line, sizeof(s_line), "%s %s %s%s%s", kind, identity, answer, (rest[0] != '\0') ? " " : "", rest);
    return s_line;
}

// the cycle line, before the first ask or sample this run adds
static void record_marked(void)
{
    if (s_record.marked)
    {
        return;
    }
    char cycle[128];
    snprintf(cycle, sizeof(cycle), "cycle %u %s", s_record.cycles + 1u, s_record.mode);
    record_line_add(cycle);
    s_record.marked = 1;
}

void record_keep(const char *identity, const RecordAsk *ask)
{
    if (!s_record.open)
    {
        return;
    }
    static char s_rest[RECORD_WORDS * 17u];
    size_t at = 0u;
    s_rest[0] = '\0';
    if (strcmp(ask->answer, "answers") == 0)
    {
        for (unsigned int word = 0u; word < ask->words; word += 1u)
        {
            at += (size_t)snprintf(&s_rest[at], sizeof(s_rest) - at, "%s%llx", (word == 0u) ? "" : " ", ask->word[word]);
        }
    }
    else
    {
        snprintf(s_rest, sizeof(s_rest), "%s", ask->refusal);
    }
    const char *const line = record_line_written("ask", identity, ask->answer, s_rest);
    const unsigned int slot = record_slot_of(identity);
    if (s_record.slot[slot] != 0u)
    {
        RecordAsk held;
        // an ask R holds is replaced only where it timed out, and a later answer resolves it
        if (!record_line_read(s_record.line[s_record.slot[slot] - 1u], &held) || (strcmp(held.answer, "timed_out") != 0))
        {
            return;
        }
        char *const replaced = (char *)malloc(strlen(line) + 1u);
        if (replaced != NULL)
        {
            memcpy(replaced, line, strlen(line) + 1u);
            free(s_record.line[s_record.slot[slot] - 1u]);
            s_record.line[s_record.slot[slot] - 1u] = replaced;
            s_record.changed = 1;
        }
        return;
    }
    record_marked();
    if (record_line_add(line))
    {
        s_record.slot[slot] = s_record.lines;
        s_record.changed = 1;
    }
}

void record_sample(const char *identity, const char *answer, unsigned long long nanoseconds, unsigned long long least,
                   unsigned long long most, const char *refusal)
{
    if (!s_record.open)
    {
        return;
    }
    char rest[RECORD_REFUSAL + 64u];
    if ((strcmp(answer, "answers") == 0) && (most != 0ull))
    {
        snprintf(rest, sizeof(rest), "%llu %llu %llu", nanoseconds, least, most);
    }
    else if (strcmp(answer, "answers") == 0)
    {
        snprintf(rest, sizeof(rest), "%llu", nanoseconds);
    }
    else
    {
        snprintf(rest, sizeof(rest), "%s", refusal);
    }
    record_marked();
    if (record_line_add(record_line_written("sample", identity, answer, rest)))
    {
        s_record.changed = 1;
    }
}

int record_close(void)
{
    if (!s_record.open)
    {
        return 1;
    }
    int written = 1;
    if (s_record.changed)
    {
        FILE *const file = fopen(s_record.path, "wb");
        written = (file != NULL);
        for (unsigned int at = 0u; written && (at < s_record.lines); at += 1u)
        {
            written = (fprintf(file, "%s\n", s_record.line[at]) >= 0);
        }
        written = (file != NULL) && (fclose(file) == 0) && written;
        if (!written)
        {
            printf("  record: %s could not be written\n", s_record.path);
        }
    }
    for (unsigned int at = 0u; at < s_record.lines; at += 1u)
    {
        free(s_record.line[at]);
    }
    free(s_record.line);
    free(s_record.slot);
    memset(&s_record, 0, sizeof(s_record));
    return written;
}
