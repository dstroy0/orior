// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// accounts.c: the accounts: a conference named, an account written to the run, a standing joined, a cause read, text trimmed, an expert named, a clause read, and an account and the accounts read back
#include "overseer_internal.h"

OverseerLesson s_lessons[OVERSEER_LESSONS_MOST];
unsigned int s_lesson_count;

OverseerHeard s_heard;

void overseer_conference_named(const char *identity, char *name)
{
    const int length = snprintf(name, OVERSEER_NAME_LONGEST, "%s_%s", QRY_CONFERENCE_CALL, identity);
    // the name fits its room or is cut to it, and its length is not negative; each space of it an underscore
    const size_t written = (length < 0)                               ? 0u
                           : ((size_t)length < OVERSEER_NAME_LONGEST) ? (size_t)length
                                                                      : (OVERSEER_NAME_LONGEST - 1u);
    for (size_t at = 0u; at < written; at += 1u)
    {
        name[at] = (name[at] == ' ') ? '_' : name[at];
    }
}

int overseer_account(QryBuffer *run, const char *identity, const char *expert, const char *text)
{
    char name[OVERSEER_NAME_LONGEST];
    char account[OVERSEER_ACCOUNT_LONGEST];
    overseer_conference_named(identity, name);
    const int length = snprintf(account, sizeof(account), "%s: %s\n", expert, text);
    // an account longer than its room is cut, its line ended; the length is not negative where it was written
    const size_t kept = (length < 0)                         ? 0u
                        : ((size_t)length < sizeof(account)) ? (size_t)length
                                                             : (sizeof(account) - 1u);
    if (kept == 0u)
    {
        return 0;
    }
    account[kept - 1u] = '\n';
    return (run != NULL) && (qry_hand(run, name, account, (unsigned long long)kept, QRY_ORDINARY) != 0ull);
}

void overseer_standing_joined(const char *standing, char *joined)
{
    size_t length = 0u;
    for (size_t at = 0u; (standing[at] != '\0') && (length < (OVERSEER_STANDING_LONGEST - 1u)); at += 1u)
    {
        const char letter = ((standing[at] == '\n') || (standing[at] == '\r')) ? ' ' : standing[at];
        joined[length] = letter;
        length += 1u;
    }
    while ((length != 0u) && (joined[length - 1u] == ' '))
    {
        length -= 1u;
    }
    joined[length] = '\0';
}

// the cause `word` reads, OVERSEER_CAUSE_NONE where it reads none
unsigned int overseer_cause_read(const char *word)
{
    for (unsigned int at = 1u; at < (sizeof(s_cause_words) / sizeof(s_cause_words[0])); at += 1u)
    {
        if (strcmp(word, s_cause_words[at]) == 0)
        {
            return at;
        }
    }
    return OVERSEER_CAUSE_NONE;
}

// `text` from `from` to `to` copied into `into`, which holds `room`, the spaces at both ends left out
void overseer_trimmed(const char *from, const char *to, char *into, size_t room)
{
    while ((from < to) && (*from == ' '))
    {
        from += 1;
    }
    while ((to > from) && ((to[-1] == ' ') || (to[-1] == '\n') || (to[-1] == '\r')))
    {
        to -= 1;
    }
    // the stretch is not negative, and it is cut to the room
    size_t length = (size_t)(to - from);
    length = (length < (room - 1u)) ? length : (room - 1u);
    memcpy(into, from, length);
    into[length] = '\0';
}

// the expert `expert` named among those the conference heard, once
static void overseer_expert_named(OverseerHeard *heard, const char *expert)
{
    for (unsigned int at = 0u; at < heard->expert_count; at += 1u)
    {
        if (strcmp(heard->experts[at], expert) == 0)
        {
            return;
        }
    }
    if (heard->expert_count < OVERSEER_EXPERTS_MOST)
    {
        snprintf(heard->experts[heard->expert_count], OVERSEER_EXPERT_LONGEST, "%s", expert);
        heard->expert_count += 1u;
    }
}

// one clause `clause` of `expert`'s account read into `heard`; a vote of an admission conference is one word
static void overseer_clause_read(OverseerHeard *heard, const char *expert, const char *clause)
{
    if (strcmp(clause, "admit") == 0)
    {
        heard->admits += 1u;
        return;
    }
    if (strcmp(clause, "hold") == 0)
    {
        heard->holds += 1u;
        return;
    }
    const char *const space = strchr(clause, ' ');
    if (space == NULL)
    {
        return;
    }
    char word[32];
    char rest[OVERSEER_CLAUSE_LONGEST];
    overseer_trimmed(clause, space, word, sizeof(word));
    overseer_trimmed(space + 1, clause + strlen(clause), rest, sizeof(rest));
    if (strcmp(word, "cause") == 0)
    {
        const unsigned int cause = overseer_cause_read(rest);
        if ((cause != OVERSEER_CAUSE_NONE) && (heard->vote_count < OVERSEER_VOTES_MOST))
        {
            snprintf(heard->votes[heard->vote_count].expert, OVERSEER_EXPERT_LONGEST, "%s", expert);
            heard->votes[heard->vote_count].cause = cause;
            heard->vote_count += 1u;
        }
    }
    else if (strcmp(word, "stand-in") == 0)
    {
        heard->addresses |= (strcmp(rest, "addresses") == 0) ? 1 : 0;
        heard->controls |= (strcmp(rest, "controls") == 0) ? 1 : 0;
        heard->scratch |= (strcmp(rest, "scratch") == 0) ? 1 : 0;
    }
    else if (strcmp(word, "carrier") == 0)
    {
        // a carrier's name is its chain's number, far inside the room; a longer one is cut to it
        snprintf(heard->carrier, sizeof(heard->carrier), "%.*s", (int)(sizeof(heard->carrier) - 1u), rest);
    }
    else if (strcmp(word, "learning") == 0)
    {
        snprintf(heard->learning, sizeof(heard->learning), "%s", rest);
    }
    else if (strcmp(word, "counterexample") == 0)
    {
        snprintf(heard->counterexample, sizeof(heard->counterexample), "%s", rest);
    }
}

// One account, the line from `line` to `end`, read into `heard`: its expert, then each clause after a `;`, a
// `standing` clause taking the rest of the line. The account a row of the run's table of accounts: the conference,
// the number of the question's identity, `conference`; the expert's text, the account's and the cause it reads, 0
// where it reads none
static void overseer_account_read(OverseerHeard *heard, const char *line, const char *end, QryBuffer *run,
                                  unsigned long long conference)
{
    const char *const colon = (const char *)memchr(line, ':', (size_t)(end - line));
    if ((colon == NULL) || (colon == line) || (memchr(line, ' ', (size_t)(colon - line)) != NULL))
    {
        return;
    }
    char expert[OVERSEER_EXPERT_LONGEST];
    overseer_trimmed(line, colon, expert, sizeof(expert));
    overseer_expert_named(heard, expert);
    heard->accounts += 1u;
    char text[OVERSEER_ACCOUNT_LONGEST];
    overseer_trimmed(colon + 1, end, text, sizeof(text));
    const unsigned long long account[3] = {conference, qry_row_text(run, expert, strlen(expert)),
                                           qry_row_text(run, text, strlen(text))};
    const unsigned int votes_before = heard->vote_count;
    char *const standing = strstr(text, "; standing ");
    if (standing != NULL)
    {
        char joined[OVERSEER_STANDING_LONGEST];
        overseer_trimmed(standing + strlen("; standing "), text + strlen(text), joined, sizeof(joined));
        overseer_standing_joined(joined, heard->standing);
        *standing = '\0';
    }
    char *clause = strchr(text, ';');
    while (clause != NULL)
    {
        char *const next = strchr(clause + 1, ';');
        char one[OVERSEER_CLAUSE_LONGEST];
        overseer_trimmed(clause + 1, (next != NULL) ? next : (text + strlen(text)), one, sizeof(one));
        overseer_clause_read(heard, expert, one);
        clause = next;
    }
    // the cause the account reads, the last of its cause clauses where it gives one
    const unsigned long long row[4] = {account[0], account[1], account[2],
                                       (heard->vote_count != votes_before) ? heard->votes[heard->vote_count - 1u].cause
                                                                           : 0ull};
    qry_row_handed(run, OVERSEER_ACCOUNTS_TABLE, row, 4u);
}

// every line of `bytes`, `size` of them, read as an account once, each a row of the run's table of accounts of the
// conference `conference`: a line read before in this conference is the same account heard twice, as the carrier's own
// is in its answer and on the panic line
void overseer_accounts_read(OverseerHeard *heard, const unsigned char *bytes, unsigned long long size,
                                   const unsigned char *before, unsigned long long before_size, QryBuffer *run,
                                   unsigned long long conference)
{
    unsigned long long at = 0ull;
    while (at < size)
    {
        const unsigned char *const line = bytes + at;
        const unsigned char *const found = (const unsigned char *)memchr(line, '\n', (size_t)(size - at));
        const unsigned char *const end = (found != NULL) ? found : (bytes + size);
        // the line's length, and where it ends, are within the bytes
        const size_t length = (size_t)(end - line);
        at += (unsigned long long)length + 1ull;
        int heard_before = 0;
        unsigned long long seen = 0ull;
        while (!heard_before && (seen < before_size))
        {
            const unsigned char *const other = before + seen;
            const unsigned char *const other_found =
                (const unsigned char *)memchr(other, '\n', (size_t)(before_size - seen));
            const size_t other_length =
                (size_t)(((other_found != NULL) ? other_found : (before + before_size)) - other);
            heard_before = (other_length == length) && (memcmp(other, line, length) == 0);
            seen += (unsigned long long)other_length + 1ull;
        }
        if (!heard_before && (length != 0u))
        {
            overseer_account_read(heard, (const char *)line, (const char *)end, run, conference);
        }
    }
}
