// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// names.c: the names a run gives: a file's plain path and the name it is filed by, the tree's top, and the who code every write names its source by
#include "qry_buffer_internal.h"

// `path` made whole and plain into `plain`, which holds `room`: `/` between its folders, a drive the shell writes as
// `/d/` written `D:/`, and a path from the folder the process stands in put after that folder. 1, or 0 where it climbs
// out through `..` or does not fit
static int qry_path_plain(const char *path, char *plain, size_t room)
{
    char given[1024];
    const int length = snprintf(given, sizeof(given), "%s", path);
    if ((length < 0) || ((size_t)length >= sizeof(given)) || (strstr(given, "..") != NULL))
    {
        return 0;
    }
    for (char *at = given; *at != '\0'; at += 1)
    {
        *at = (*at == '\\') ? '/' : *at;
    }
    const char *rest = given;
    while ((rest[0] == '.') && (rest[1] == '/'))
    {
        rest += 2;
    }
    int written = 0;
#if defined(_WIN32)
    const int lettered = ((rest[1] >= 'a') && (rest[1] <= 'z')) || ((rest[1] >= 'A') && (rest[1] <= 'Z'));
    const int drive = ((rest[0] >= 'a') && (rest[0] <= 'z')) || ((rest[0] >= 'A') && (rest[0] <= 'Z'));
    if ((rest[0] == '/') && lettered && (rest[2] == '/'))
    {
        // the shell's `/d/x`, the drive's letter its first folder
        written = snprintf(plain, room, "%c:%s", (char)((rest[1] >= 'a') ? (rest[1] - 'a' + 'A') : rest[1]), rest + 2);
    }
    else if (drive && (rest[1] == ':'))
    {
        written = snprintf(plain, room, "%c%s", (char)((rest[0] >= 'a') ? (rest[0] - 'a' + 'A') : rest[0]), rest + 1);
    }
    else
    {
        char here[1024];
        if (GetCurrentDirectoryA(sizeof(here), here) == 0u)
        {
            return 0;
        }
        for (char *at = here; *at != '\0'; at += 1)
        {
            *at = (*at == '\\') ? '/' : *at;
        }
        written = snprintf(plain, room, "%s/%s", here, rest);
    }
#else
    if (rest[0] == '/')
    {
        written = snprintf(plain, room, "%s", rest);
    }
    else
    {
        char here[1024];
        if (getcwd(here, sizeof(here)) == NULL)
        {
            return 0;
        }
        written = snprintf(plain, room, "%s/%s", here, rest);
    }
#endif
    // a path that does not fit its room names no file
    return (written > 0) && ((size_t)written < room);
}

int qry_file_named(const char *path, char *name, size_t room)
{
    const char *const top_given = getenv(QRY_TOP_ENVIRONMENT);
    char top[1024];
    char plain[1024];
    if ((top_given == NULL) || !qry_path_plain(top_given, top, sizeof(top)) ||
        !qry_path_plain(path, plain, sizeof(plain)))
    {
        return 0;
    }
    size_t top_length = strlen(top);
    while ((top_length != 0u) && (top[top_length - 1u] == '/'))
    {
        top_length -= 1u;
    }
#if defined(_WIN32)
    const int under = (_strnicmp(plain, top, top_length) == 0);
#else
    const int under = (strncmp(plain, top, top_length) == 0);
#endif
    if (!under || (plain[top_length] != '/') || (plain[top_length + 1u] == '\0'))
    {
        return 0;
    }
    const int written = snprintf(name, room, "%s", plain + top_length + 1u);
    // a name that does not fit its room is none
    return (written > 0) && ((size_t)written < room);
}

// the current file the name `name`, `length` bytes of it, gives, its path from the top of the tree, into `path`, which
// holds `room`: 1, or 0 where no top is named, the name climbs out of it, or the path does not fit
int qry_file_placed(const char *name, size_t length, char *path, size_t room)
{
    const char *const top_given = getenv(QRY_TOP_ENVIRONMENT);
    char top[1024];
    if ((top_given == NULL) || !qry_path_plain(top_given, top, sizeof(top)) || (length == 0u) ||
        (length > QRY_NAME_LONGEST) || (name[0] == '/') || (memchr(name, ':', length) != NULL))
    {
        return 0;
    }
    char named[QRY_NAME_LONGEST + 1u];
    memcpy(named, name, length);
    named[length] = '\0';
    if (strstr(named, "..") != NULL)
    {
        return 0;
    }
    const int written = snprintf(path, room, "%s/%s", top, named);
    // a path that does not fit its room names no file
    return (written > 0) && ((size_t)written < room);
}

unsigned long long qry_filed_by(QryBuffer *buffer, const char *path, const void *bytes, unsigned long long size,
                                QryWho who)
{
    char name[QRY_NAME_LONGEST + 1u];
    char placed[1400];
    if ((buffer == NULL) || ((bytes == NULL) && (size != 0ull)) || !qry_file_named(path, name, sizeof(name)) ||
        !qry_file_placed(name, strlen(name), placed, sizeof(placed)))
    {
        return 0ull;
    }
    // in the .qry on disk once the hand returns; in the file's place once the run ends (qry_files_put)
    const unsigned long long sequence = qry_handed(buffer, name, bytes, size, QRY_CRITICAL, QRY_MARK_FILED);
    if (sequence == 0ull)
    {
        return 0ull;
    }
    // the write a row of the run's table of files after it, critical: its who, its sequence and its path's number in
    // the table of texts
    const unsigned long long file[2] = {sequence, qry_row_text(buffer, name, strlen(name))};
    return qry_row_handed_by(buffer, QRY_FILES_TABLE, file, 2u, QRY_CRITICAL, who) ? sequence : 0ull;
}

static char *s_who_source[QRY_WHO_MOST];
static unsigned int s_who_highest;
static unsigned int s_who_program;
static atomic_int s_who_state;

int qry_top(char *top, size_t room)
{
    const char *const given = getenv(QRY_TOP_ENVIRONMENT);
    if (given != NULL)
    {
        return qry_path_plain(given, top, room);
    }
    static const char s_own[] = "src/cu/types/file_defs/qry/buffer/names.c";
    char plain[1024];
    if (!qry_path_plain(__FILE__, plain, sizeof(plain)))
    {
        return 0;
    }
    const size_t length = strlen(plain);
    const size_t own = strlen(s_own);
    if ((length <= own) || (strcmp(plain + length - own, s_own) != 0))
    {
        return 0;
    }
    // the top stands before the file's own path and the `/` that opens it, which an int counts
    const int written = snprintf(top, room, "%.*s", (int)(length - own - 1u), plain);
    return (written > 0) && ((size_t)written < room);
}

// the path of `file` from the top of the tree into `name`, which holds `room`: 1, or 0 where it stands outside it
static int qry_who_source_named(const char *file, char *name, size_t room)
{
    char top[1024];
    char plain[1024];
    if (!qry_top(top, sizeof(top)) || !qry_path_plain(file, plain, sizeof(plain)))
    {
        return 0;
    }
    const size_t length = strlen(top);
#if defined(_WIN32)
    const int under = (_strnicmp(plain, top, length) == 0);
#else
    const int under = (strncmp(plain, top, length) == 0);
#endif
    if (!under || (plain[length] != '/'))
    {
        return 0;
    }
    const int written = snprintf(name, room, "%s", plain + length + 1u);
    return (written > 0) && ((size_t)written < room);
}

// the number the who table gives the source at `name`, its path from the top of the tree, 0 where it gives none
static unsigned int qry_who_number(const char *name)
{
    for (unsigned int number = 1u; number <= s_who_highest; number += 1u)
    {
        if ((s_who_source[number] != NULL) && (strcmp(s_who_source[number], name) == 0))
        {
            return number;
        }
    }
    return 0u;
}

// the number of the program's own source: the first the who table gives, a header after every other, whose name
// without its kind is the program's name without its kind; 0 where none is
static unsigned int qry_who_program_number(void)
{
    char path[1024] = "";
#if defined(_WIN32)
    const DWORD length = GetModuleFileNameA(NULL, path, (DWORD)sizeof(path));
    path[(length < sizeof(path)) ? length : (sizeof(path) - 1u)] = '\0';
#else
    const ssize_t length = readlink("/proc/self/exe", path, sizeof(path) - 1u);
    // a length readlink gives past -1 is the count it read, at most the room it was given
    path[(length > 0) ? (size_t)length : 0u] = '\0';
#endif
    const char *const slash = strrchr(path, '/');
    const char *const back = strrchr(path, '\\');
    const char *base = (back > slash) ? (back + 1) : ((slash != NULL) ? (slash + 1) : path);
    const size_t stem = strcspn(base, ".");
    unsigned int header = 0u;
    for (unsigned int number = 1u; number <= s_who_highest; number += 1u)
    {
        const char *const source = s_who_source[number];
        if (source == NULL)
        {
            continue;
        }
        const char *const own = (strrchr(source, '/') != NULL) ? (strrchr(source, '/') + 1) : source;
        if ((strncmp(own, base, stem) != 0) || (own[stem] != '.'))
        {
            continue;
        }
        const char *const kind = own + stem;
        if ((strcmp(kind, ".h") != 0) && (strcmp(kind, ".cuh") != 0) && (strcmp(kind, ".hpp") != 0))
        {
            return number;
        }
        header = (header == 0u) ? number : header;
    }
    return header;
}

// the who table read, once a process, as the run holds it: every number and its source
static void qry_who_table_read(void)
{
    int unread = 0;
    if (!atomic_compare_exchange_strong(&s_who_state, &unread, 1))
    {
        while (atomic_load(&s_who_state) != 2)
        {
            qry_sleep_millisecond();
        }
        return;
    }
    char top[1024];
    char path[1400];
    unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (qry_top(top, sizeof(top)) && (snprintf(path, sizeof(path), "%s/%s", top, QRY_WHO_TABLE) > 0) &&
        qry_file_read(qry_run(), path, &bytes, &size))
    {
        unsigned long long at = 0ull;
        char line[1200];
        while (qry_line_next(bytes, size, &at, line, sizeof(line)))
        {
            line[strcspn(line, "\r\n")] = '\0';
            char *after = NULL;
            const unsigned long long number = strtoull(line, &after, 16);
            if ((after == line) || (*after != '\t') || (number == 0ull) || (number >= QRY_WHO_MOST))
            {
                continue;
            }
            const size_t length = strlen(after + 1);
            char *const source = (char *)malloc(length + 1u);
            if (source == NULL)
            {
                continue;
            }
            memcpy(source, after + 1, length + 1u);
            // a number below QRY_WHO_MOST is one an unsigned int holds
            free(s_who_source[number]);
            s_who_source[number] = source;
            s_who_highest = ((unsigned int)number > s_who_highest) ? (unsigned int)number : s_who_highest;
        }
        free(bytes);
    }
    s_who_program = qry_who_program_number();
    atomic_store(&s_who_state, 2);
}

static const char *s_who_asked[QRY_WHO_ASKED];
static unsigned int s_who_asked_number[QRY_WHO_ASKED];
static unsigned int s_who_asked_next;
static atomic_flag s_who_asked_lock = ATOMIC_FLAG_INIT;

QryWho qry_who(const char *file, unsigned int line)
{
    qry_who_table_read();
    QryWho who = {0u, line, s_who_program, qry_nanoseconds()};
    while (atomic_flag_test_and_set(&s_who_asked_lock))
    {
    }
    int found = 0;
    for (unsigned int at = 0u; (at < QRY_WHO_ASKED) && !found; at += 1u)
    {
        found = (s_who_asked[at] == file);
        who.source = found ? s_who_asked_number[at] : who.source;
    }
    atomic_flag_clear(&s_who_asked_lock);
    if (found)
    {
        return who;
    }
    char name[1024];
    who.source = qry_who_source_named(file, name, sizeof(name)) ? qry_who_number(name) : 0u;
    while (atomic_flag_test_and_set(&s_who_asked_lock))
    {
    }
    s_who_asked[s_who_asked_next] = file;
    s_who_asked_number[s_who_asked_next] = who.source;
    s_who_asked_next = (s_who_asked_next + 1u) % QRY_WHO_ASKED;
    atomic_flag_clear(&s_who_asked_lock);
    return who;
}

void qry_who_words(QryWho who, unsigned long long *words)
{
    words[0] = (((unsigned long long)who.source & 0xffffull) << 48u) |
               (((unsigned long long)who.line & 0xffffull) << 32u) |
               (((unsigned long long)who.program & 0xffffull) << 16u);
    words[1] = who.nanoseconds;
}

QryWho qry_who_of_words(const unsigned long long *words)
{
    // each field is sixteen bits of the first word, which an unsigned int holds
    const QryWho who = {(unsigned int)((words[0] >> 48u) & 0xffffull), (unsigned int)((words[0] >> 32u) & 0xffffull),
                        (unsigned int)((words[0] >> 16u) & 0xffffull), words[1]};
    return who;
}

void qry_who_code(QryWho who, char *code, size_t room)
{
    unsigned long long words[2];
    qry_who_words(who, words);
    snprintf(code, room, "%016llx%016llx", words[0], words[1]);
}

int qry_who_read(const char *code, QryWho *who)
{
    if (strspn(code, "0123456789abcdefABCDEF") != QRY_WHO_CODE_LETTERS)
    {
        return 0;
    }
    char half[17];
    unsigned long long words[2];
    memcpy(half, code, 16u);
    half[16] = '\0';
    words[0] = strtoull(half, NULL, 16);
    memcpy(half, code + 16, 16u);
    words[1] = strtoull(half, NULL, 16);
    *who = qry_who_of_words(words);
    return 1;
}

// the civil date of `days` since 1970-01-01 into `year`, `month` and `day` (the proleptic Gregorian calendar)
static void qry_civil(long long days, long long *year, unsigned int *month, unsigned int *day)
{
    days += 719468ll;
    const long long era = ((days >= 0ll) ? days : (days - 146096ll)) / 146097ll;
    // the day of the era is at least 0 and below 146097, which an unsigned int holds
    const unsigned int of_era = (unsigned int)(days - (era * 146097ll));
    const unsigned int year_of_era = (of_era - (of_era / 1460u) + (of_era / 36524u) - (of_era / 146096u)) / 365u;
    const unsigned int of_year = of_era - ((365u * year_of_era) + (year_of_era / 4u) - (year_of_era / 100u));
    const unsigned int shifted = ((5u * of_year) + 2u) / 153u;
    *day = of_year - (((153u * shifted) + 2u) / 5u) + 1u;
    *month = (shifted < 10u) ? (shifted + 3u) : (shifted - 9u);
    *year = (long long)year_of_era + (era * 400ll) + ((*month <= 2u) ? 1ll : 0ll);
}

int qry_who_named(QryWho who, char *text, size_t room)
{
    qry_who_table_read();
    const char *const source =
        ((who.source != 0u) && (s_who_source[who.source] != NULL)) ? s_who_source[who.source] : "-";
    const char *const program =
        ((who.program != 0u) && (s_who_source[who.program] != NULL)) ? s_who_source[who.program] : "-";
    const unsigned long long seconds = who.nanoseconds / 1000000000ull;
    long long year = 0ll;
    unsigned int month = 0u;
    unsigned int day = 0u;
    // the days since 1970 are far below what a long long counts
    qry_civil((long long)(seconds / 86400ull), &year, &month, &day);
    const unsigned long long of_day = seconds % 86400ull;
    const int written = snprintf(text, room, "%s:%u %s %04lld-%02u-%02uT%02llu:%02llu:%02llu.%09lluZ", source, who.line,
                                 program, year, month, day, of_day / 3600ull, (of_day / 60ull) % 60ull, of_day % 60ull,
                                 who.nanoseconds % 1000000000ull);
    // a text that does not fit its room is none
    return (written > 0) && ((size_t)written < room);
}
