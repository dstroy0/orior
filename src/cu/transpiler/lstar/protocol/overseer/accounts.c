// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// accounts.c: the accounts: a conference named, an account written to the run, a `located` clause joined, a cause read,
// text trimmed, an expert named, a clause read, and an account and the accounts read back
#include "../../../../../../includes/parsers/who.h"
#include "../../../../../../includes/platform/platform.h"
#include "../table_text.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "overseer_internal.h"
#include "verba_scribo/verba_scribo.h"

OverseerLesson s_lessons[OVERSEER_LESSONS_MAX];
unsigned int s_lesson_count;

OverseerTally s_conference_tally;

void overseer_conference_named(const char *identity, char *name)
{
    VerbaLinea line = {name, OVERSEER_NAME_MAX_LEN, 0ULL};
    mmgr_line_put(&line, QRY_CONFERENCE_CALL);
    mmgr_line_ch(&line, '_');
    mmgr_line_put_max(&line, identity, OVERSEER_NAME_MAX_LEN - sizeof(QRY_CONFERENCE_CALL) - 1u);
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
    VerbaLinea line = {account, sizeof(account), 0ULL};
    mmgr_line_put_max(&line, expert, OVERSEER_EXPERT_MAX_LEN);
    mmgr_line_put(&line, ": ");
    // the call cuts an account longer than its room, and ends its line
    mmgr_line_put_max(&line, text, sizeof(account) - line.at - 2u);
    mmgr_line_ch(&line, '\n');
    const size_t line_length = (size_t)mmgr_line_finish(&line);
    if (line_length == 0u)
    {
        return 0;
    }
    return (run != NULL) && (qry_member_append(name, account, (unsigned long long)line_length, QRY_ORDINARY) != 0);
}

void overseer_located_joined(const char *located, char *joined)
{
    // the text cut to the room, then each line end, reached from the one before it, written as a space
    size_t length =
        EMBED_CALL(cellul.copy, CatenaFinitaCfg, .src = located, .cap = OVERSEER_STANDING_MAX_LEN, .dst = joined);
    for (size_t at = EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = joined, .cap = length, .other = "\r\n");
         at < length;
         at += EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &joined[at], .cap = length - at, .other = "\r\n"))
    {
        joined[at] = ' ';
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
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = word, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = s_cause_words[at], .ci = EMBED_FALSE))
        {
            return at;
        }
    }
    return OVERSEER_CAUSE_NONE;
}

// `text` from `from` to `to` copied into `into`, which has room for `room`, the spaces at both ends left out
void overseer_trimmed(const char *from, const char *to, char *into, size_t room)
{
    // Explicit cast takes the bytes of the stretch as a count, which never runs back
    from += (from < to)
                ? EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = from, .cap = (size_t)(to - from), .other = " ")
                : 0u;
    while ((to > from) && ((to[-1] == ' ') || (to[-1] == '\n') || (to[-1] == '\r')))
    {
        to -= 1;
    }
    // the stretch is not negative, and the call cuts it to the room
    size_t length = (size_t)(to - from);
    length = (length < (room - 1u)) ? length : (room - 1u);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = into, .src = from, .bytes = length);
    into[length] = '\0';
}

// the expert `expert` named among those the conference received, once
static void overseer_expert_named(OverseerTally *tally, const char *expert)
{
    for (unsigned int at = 0u; at < tally->expert_count; at += 1u)
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = tally->experts[at], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = expert, .ci = EMBED_FALSE))
        {
            return;
        }
    }
    if (tally->expert_count < OVERSEER_EXPERTS_MAX)
    {
        (void)verba_finis.finish(
            &(VerbaFinisCfg){.out = tally->experts[tally->expert_count],
                             .cap = OVERSEER_EXPERT_MAX_LEN,
                             .at = verba_textus.put_clip(&(VerbaTextusCfg){.out = tally->experts[tally->expert_count],
                                                                           .cap = OVERSEER_EXPERT_MAX_LEN,
                                                                           .at = 0u,
                                                                           .text = expert})});
        tally->expert_count += 1u;
    }
}

// one clause `clause` of `expert`'s account read into `received`; a vote of an admission conference is one word
static void overseer_clause_read(OverseerTally *tally, const char *expert, const char *clause)
{
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = clause, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "admit",
                   .ci = EMBED_FALSE))
    {
        tally->admits += 1u;
        return;
    }
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = clause, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "block",
                   .ci = EMBED_FALSE))
    {
        tally->blocks += 1u;
        return;
    }
    const char *const space =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = clause, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ' ');
    if (space == NULL)
    {
        return;
    }
    char word[32];
    char rest[OVERSEER_CLAUSE_MAX_LEN];
    overseer_trimmed(clause, space, word, sizeof(word));
    overseer_trimmed(
        space + 1, clause + EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = clause, .cap = PLATFORM_TEXT_TO_TERMINATOR),
        rest, sizeof(rest));
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = word, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "cause",
                   .ci = EMBED_FALSE))
    {
        const unsigned int cause = overseer_cause_read(rest);
        if ((cause != OVERSEER_CAUSE_NONE) && (tally->vote_count < OVERSEER_VOTES_MAX))
        {
            (void)verba_finis.finish(&(VerbaFinisCfg){
                .out = tally->votes[tally->vote_count].expert,
                .cap = OVERSEER_EXPERT_MAX_LEN,
                .at = verba_textus.put_clip(&(VerbaTextusCfg){.out = tally->votes[tally->vote_count].expert,
                                                              .cap = OVERSEER_EXPERT_MAX_LEN,
                                                              .at = 0u,
                                                              .text = expert})});
            tally->votes[tally->vote_count].cause = cause;
            tally->vote_count += 1u;
        }
    }
    else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = word, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "stand-in", .ci = EMBED_FALSE))
    {
        tally->addresses |= (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = rest, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                        .other = "addresses", .ci = EMBED_FALSE))
                                ? 1
                                : 0;
        tally->controls |= (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = rest, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                       .other = "controls", .ci = EMBED_FALSE))
                               ? 1
                               : 0;
        tally->scratch |= (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = rest, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                      .other = "scratch", .ci = EMBED_FALSE))
                              ? 1
                              : 0;
    }
    else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = word, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "carrier",
                        .ci = EMBED_FALSE))
    {
        // the name of a carrier is the number of its chain, far inside the room. The call cuts a longer one to it
        (void)verba_finis.finish(
            &(VerbaFinisCfg){.out = tally->carrier,
                             .cap = sizeof(tally->carrier),
                             .at = verba_textus.put_clip(&(VerbaTextusCfg){
                                 .out = tally->carrier, .cap = sizeof(tally->carrier), .at = 0u, .text = rest})});
    }
    else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = word, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "learning", .ci = EMBED_FALSE))
    {
        (void)verba_finis.finish(
            &(VerbaFinisCfg){.out = tally->learning,
                             .cap = sizeof(tally->learning),
                             .at = verba_textus.put_clip(&(VerbaTextusCfg){
                                 .out = tally->learning, .cap = sizeof(tally->learning), .at = 0u, .text = rest})});
    }
    else if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = word, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = "counterexample", .ci = EMBED_FALSE))
    {
        (void)verba_finis.finish(&(VerbaFinisCfg){
            .out = tally->counterexample,
            .cap = sizeof(tally->counterexample),
            .at = verba_textus.put_clip(&(VerbaTextusCfg){
                .out = tally->counterexample, .cap = sizeof(tally->counterexample), .at = 0u, .text = rest})});
    }
}

// One account, the line from `line` to `end`, read into `received`. It is its expert, then each clause after a `;`,
// with a `located` clause that takes the rest of the line. The account goes as a row of the table of accounts of the
// run. The row holds the conference, the number of the identity of the question, `conference`. Then the text of the
// expert, that of the account, and the cause that it reads, 0 where it reads none
static void overseer_account_read(OverseerTally *tally, const char *line, const char *end, QryShared *run,
                                  unsigned long long conference)
{
    const char *const colon =
        (const char *)EMBED_CALL(memor.chr, MemoriaCfg, .src = line, .bytes = (size_t)(end - line), .val = ':');
    if ((colon == NULL) || (colon == line) ||
        (EMBED_CALL(memor.chr, MemoriaCfg, .src = line, .bytes = (size_t)(colon - line), .val = ' ') != NULL))
    {
        return;
    }
    char expert[OVERSEER_EXPERT_MAX_LEN];
    overseer_trimmed(line, colon, expert, sizeof(expert));
    overseer_expert_named(tally, expert);
    tally->accounts += 1u;
    char text[OVERSEER_ACCOUNT_MAX_LEN];
    overseer_trimmed(colon + 1, end, text, sizeof(text));
    const unsigned long long account[3] = {
        conference,
        table_text(expert, EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = expert, .cap = PLATFORM_TEXT_TO_TERMINATOR)),
        table_text(text, EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR))};
    const unsigned int votes_before = tally->vote_count;
    // each mark reached through the text itself, which the call writes
    const char *const located_found =
        EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "; located ",
                   .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
    char *const located = (located_found != NULL) ? &text[located_found - text] : NULL;
    if (located != NULL)
    {
        char joined[OVERSEER_STANDING_MAX_LEN];
        overseer_trimmed(
            located + EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = "; located ", .cap = PLATFORM_TEXT_TO_TERMINATOR),
            text + EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR), joined,
            sizeof(joined));
        overseer_located_joined(joined, tally->located);
        *located = '\0';
    }
    const char *clause =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ';');
    while (clause != NULL)
    {
        const char *const next =
            EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = clause + 1, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ';');
        char one[OVERSEER_CLAUSE_MAX_LEN];
        overseer_trimmed(clause + 1,
                         (next != NULL) ? next
                                        : (text + EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text,
                                                             .cap = PLATFORM_TEXT_TO_TERMINATOR)),
                         one, sizeof(one));
        overseer_clause_read(tally, expert, one);
        clause = next;
    }
    // the cause the account reads, the last of its cause clauses where it gives one
    unsigned long long row[2u + 4] = {
        0ULL,       0ULL,
        account[0], account[1],
        account[2], (tally->vote_count != votes_before) ? tally->votes[tally->vote_count - 1u].cause : 0ULL};
    who_words(WHO_HERE, row);
    qry_member_append(OVERSEER_ACCOUNTS_TABLE, row, sizeof(row), QRY_ORDINARY);
}

// every line of `bytes`, `size` of them, read as an account once, each a row of the table of accounts of the run for
// the conference `conference`. A line read before in this conference is the same account received twice. The account of
// the carrier comes twice in that way, in its answer and on the panic line
void overseer_accounts_read(OverseerTally *tally, const unsigned char *bytes, unsigned long long size,
                            const unsigned char *before, unsigned long long before_size, QryShared *run,
                            unsigned long long conference)
{
    unsigned long long at = 0ULL;
    while (at < size)
    {
        const unsigned char *const line = bytes + at;
        const unsigned char *const found = (const unsigned char *)EMBED_CALL(memor.chr, MemoriaCfg, .src = line,
                                                                             .bytes = (size_t)(size - at), .val = '\n');
        const unsigned char *const end = (found != NULL) ? found : (bytes + size);
        // the line's length, and where it ends, are within the bytes
        const size_t length = (size_t)(end - line);
        at += (unsigned long long)length + 1ULL;
        int seen_before = 0;
        unsigned long long seen = 0ULL;
        while (!seen_before && (seen < before_size))
        {
            const unsigned char *const other = before + seen;
            const unsigned char *const other_found = (const unsigned char *)EMBED_CALL(
                memor.chr, MemoriaCfg, .src = other, .bytes = (size_t)(before_size - seen), .val = '\n');
            const size_t other_length =
                (size_t)(((other_found != NULL) ? other_found : (before + before_size)) - other);
            seen_before = (other_length == length) &&
                          (EMBED_CALL(memor.cmp, MemoriaCfg, .src = other, .other = line, .bytes = length) == 0);
            seen += (unsigned long long)other_length + 1ULL;
        }
        if (!seen_before && (length != 0u))
        {
            overseer_account_read(tally, (const char *)line, (const char *)end, run, conference);
        }
    }
}
