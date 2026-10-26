// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// accounts.c: the accounts: a conference named, an account written to the run, a standing joined, a cause read, text trimmed, an expert named, a clause read, and an account and the accounts read back
#include "overseer_internal.h"
#include "verba_scribo/verba_scribo.h"
#include "../table_text.h"
#include "../../../../includes/parsers/who.h"

OverseerLesson s_lessons[OVERSEER_LESSONS_MOST];
unsigned int s_lesson_count;

OverseerTally s_conference_tally;

void overseer_conference_named(const char *identity, char *name)
{
    VerbaLinea line = {name, OVERSEER_NAME_MAX_LEN, 0ull};
    mmgr_line_put(&line, QRY_CONFERENCE_CALL);
    mmgr_line_ch(&line, '_');
    mmgr_line_put_most(&line, identity, OVERSEER_NAME_MAX_LEN - sizeof(QRY_CONFERENCE_CALL) - 1u);
    // the name cut to its room; each space of it an underscore
    const size_t written = (size_t)mmgr_line_finish(&line);
    for (size_t at = 0u; at < written; at += 1u)
    {
        name[at] = (name[at] == ' ') ? '_' : name[at];
    }
}

int overseer_account(QryShared *run, const char *identity, const char *expert, const char *text)
{
    char name[OVERSEER_NAME_MAX_LEN];
    char account[OVERSEER_ACCOUNT_MAX_LEN];
    overseer_conference_named(identity, name);
    VerbaLinea line = {account, sizeof(account), 0ull};
    mmgr_line_put_most(&line, expert, OVERSEER_EXPERT_MAX_LEN);
    mmgr_line_put(&line, ": ");
    // an account longer than its room is cut, its line ended
    mmgr_line_put_most(&line, text, sizeof(account) - line.at - 2u);
    mmgr_line_ch(&line, '\n');
    const size_t line_length = (size_t)mmgr_line_finish(&line);
    if (line_length == 0u)
    {
        return 0;
    }
    return (run != NULL) && (qry_member_append(name, account, (unsigned long long)line_length, QRY_ORDINARY) != 0);
}

void overseer_standing_joined(const char *standing, char *joined)
{
    size_t length = 0u;
    for (size_t at = 0u; (standing[at] != '\0') && (length < (OVERSEER_STANDING_MAX_LEN - 1u)); at += 1u)
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

// `text` from `from` to `to` copied into `into`, which has room for `room`, the spaces at both ends left out
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

// the expert `expert` named among those the conference received, once
static void overseer_expert_named(OverseerTally *tally, const char *expert)
{
    for (unsigned int at = 0u; at < tally->expert_count; at += 1u)
    {
        if (strcmp(tally->experts[at], expert) == 0)
        {
            return;
        }
    }
    if (tally->expert_count < OVERSEER_EXPERTS_MOST)
    {
        (void)verba_finis.finish(&(VerbaFinisCfg){.out = tally->experts[tally->expert_count], .cap = OVERSEER_EXPERT_MAX_LEN, .at = verba_textus.put_clip(&(VerbaTextusCfg){.out = tally->experts[tally->expert_count], .cap = OVERSEER_EXPERT_MAX_LEN, .at = 0u, .text = expert})});
        tally->expert_count += 1u;
    }
}

// one clause `clause` of `expert`'s account read into `received`; a vote of an admission conference is one word
static void overseer_clause_read(OverseerTally *tally, const char *expert, const char *clause)
{
    if (strcmp(clause, "admit") == 0)
    {
        tally->admits += 1u;
        return;
    }
    if (strcmp(clause, "block") == 0)
    {
        tally->blocks += 1u;
        return;
    }
    const char *const space = strchr(clause, ' ');
    if (space == NULL)
    {
        return;
    }
    char word[32];
    char rest[OVERSEER_CLAUSE_MAX_LEN];
    overseer_trimmed(clause, space, word, sizeof(word));
    overseer_trimmed(space + 1, clause + strlen(clause), rest, sizeof(rest));
    if (strcmp(word, "cause") == 0)
    {
        const unsigned int cause = overseer_cause_read(rest);
        if ((cause != OVERSEER_CAUSE_NONE) && (tally->vote_count < OVERSEER_VOTES_MOST))
        {
            (void)verba_finis.finish(&(VerbaFinisCfg){.out = tally->votes[tally->vote_count].expert, .cap = OVERSEER_EXPERT_MAX_LEN, .at = verba_textus.put_clip(&(VerbaTextusCfg){.out = tally->votes[tally->vote_count].expert, .cap = OVERSEER_EXPERT_MAX_LEN, .at = 0u, .text = expert})});
            tally->votes[tally->vote_count].cause = cause;
            tally->vote_count += 1u;
        }
    }
    else if (strcmp(word, "stand-in") == 0)
    {
        tally->addresses |= (strcmp(rest, "addresses") == 0) ? 1 : 0;
        tally->controls |= (strcmp(rest, "controls") == 0) ? 1 : 0;
        tally->scratch |= (strcmp(rest, "scratch") == 0) ? 1 : 0;
    }
    else if (strcmp(word, "carrier") == 0)
    {
        // a carrier's name is its chain's number, far inside the room; a longer one is cut to it
        (void)verba_finis.finish(&(VerbaFinisCfg){.out = tally->carrier, .cap = sizeof(tally->carrier), .at = verba_textus.put_clip(&(VerbaTextusCfg){.out = tally->carrier, .cap = sizeof(tally->carrier), .at = 0u, .text = rest})});
    }
    else if (strcmp(word, "learning") == 0)
    {
        (void)verba_finis.finish(&(VerbaFinisCfg){.out = tally->learning, .cap = sizeof(tally->learning), .at = verba_textus.put_clip(&(VerbaTextusCfg){.out = tally->learning, .cap = sizeof(tally->learning), .at = 0u, .text = rest})});
    }
    else if (strcmp(word, "counterexample") == 0)
    {
        (void)verba_finis.finish(&(VerbaFinisCfg){.out = tally->counterexample, .cap = sizeof(tally->counterexample), .at = verba_textus.put_clip(&(VerbaTextusCfg){.out = tally->counterexample, .cap = sizeof(tally->counterexample), .at = 0u, .text = rest})});
    }
}

// One account, the line from `line` to `end`, read into `received`: its expert, then each clause after a `;`, a
// `standing` clause taking the rest of the line. The account a row of the run's table of accounts: the conference,
// the number of the question's identity, `conference`; the expert's text, the account's and the cause it reads, 0
// where it reads none
static void overseer_account_read(OverseerTally *tally, const char *line, const char *end, QryShared *run,
                                  unsigned long long conference)
{
    const char *const colon = (const char *)memchr(line, ':', (size_t)(end - line));
    if ((colon == NULL) || (colon == line) || (memchr(line, ' ', (size_t)(colon - line)) != NULL))
    {
        return;
    }
    char expert[OVERSEER_EXPERT_MAX_LEN];
    overseer_trimmed(line, colon, expert, sizeof(expert));
    overseer_expert_named(tally, expert);
    tally->accounts += 1u;
    char text[OVERSEER_ACCOUNT_MAX_LEN];
    overseer_trimmed(colon + 1, end, text, sizeof(text));
    const unsigned long long account[3] = {conference, table_text(expert, strlen(expert)),
                                           table_text(text, strlen(text))};
    const unsigned int votes_before = tally->vote_count;
    char *const standing = strstr(text, "; standing ");
    if (standing != NULL)
    {
        char joined[OVERSEER_STANDING_MAX_LEN];
        overseer_trimmed(standing + strlen("; standing "), text + strlen(text), joined, sizeof(joined));
        overseer_standing_joined(joined, tally->standing);
        *standing = '\0';
    }
    char *clause = strchr(text, ';');
    while (clause != NULL)
    {
        char *const next = strchr(clause + 1, ';');
        char one[OVERSEER_CLAUSE_MAX_LEN];
        overseer_trimmed(clause + 1, (next != NULL) ? next : (text + strlen(text)), one, sizeof(one));
        overseer_clause_read(tally, expert, one);
        clause = next;
    }
    // the cause the account reads, the last of its cause clauses where it gives one
    unsigned long long row[2u + 4] = {
        0ull,       0ull,
        account[0], account[1],
        account[2], (tally->vote_count != votes_before) ? tally->votes[tally->vote_count - 1u].cause : 0ull};
    who_words(WHO_HERE, row);
    qry_member_append(OVERSEER_ACCOUNTS_TABLE, row, sizeof(row), QRY_ORDINARY);
}

// every line of `bytes`, `size` of them, read as an account once, each a row of the run's table of accounts of the
// conference `conference`: a line read before in this conference is the same account received twice, as the carrier's
// own is in its answer and on the panic line
void overseer_accounts_read(OverseerTally *tally, const unsigned char *bytes, unsigned long long size,
                            const unsigned char *before, unsigned long long before_size, QryShared *run,
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
        int seen_before = 0;
        unsigned long long seen = 0ull;
        while (!seen_before && (seen < before_size))
        {
            const unsigned char *const other = before + seen;
            const unsigned char *const other_found =
                (const unsigned char *)memchr(other, '\n', (size_t)(before_size - seen));
            const size_t other_length =
                (size_t)(((other_found != NULL) ? other_found : (before + before_size)) - other);
            seen_before = (other_length == length) && (memcmp(other, line, length) == 0);
            seen += (unsigned long long)other_length + 1ull;
        }
        if (!seen_before && (length != 0u))
        {
            overseer_account_read(tally, (const char *)line, (const char *)end, run, conference);
        }
    }
}
