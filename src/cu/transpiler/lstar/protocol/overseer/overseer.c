// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// overseer.c: a question's conference read out of the run, its ruling by consensus, and the lessons it keeps
// (overseer.h)
#include "overseer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the run's tables of the accounts the overseer hears, the conferences it holds and the lessons it teaches, each row
// naming its conference by the number of the question's identity in the table of texts (accounts.oracle.tsv,
// conferences.oracle.tsv, lessons.oracle.tsv)
#define OVERSEER_ACCOUNTS_TABLE "accounts"
#define OVERSEER_CONFERENCES_TABLE "conferences"
#define OVERSEER_LESSONS_TABLE "lessons"

// the most lessons a run holds, the longest carrier name, the most experts a conference names, and the most causes it
// hears read
#define OVERSEER_LESSONS_MOST 4096u
#define OVERSEER_CARRIER_LONGEST 64u
#define OVERSEER_EXPERTS_MOST 32u
#define OVERSEER_EXPERT_LONGEST 32u
#define OVERSEER_VOTES_MOST 64u
#define OVERSEER_CLAUSE_LONGEST 512u

// one lesson: the cause it was taught by, the carrier it holds at, `*` at every carrier, and the stand-in as written
typedef struct
{
    unsigned int cause;
    char carrier[OVERSEER_CARRIER_LONGEST];
    char standing[OVERSEER_STANDING_LONGEST];
} OverseerLesson;

static OverseerLesson s_lessons[OVERSEER_LESSONS_MOST];
static unsigned int s_lesson_count;

// each cause by the word an account reads it with, in the order of OverseerCause
static const char *const s_cause_words[] = {"none", "instruction", "address", "register",
                                            "time", "exception",   "admitted"};

// one expert's reading of the cause
typedef struct
{
    char expert[OVERSEER_EXPERT_LONGEST];
    unsigned int cause;
} OverseerVote;

// what a conference's accounts say, read clause by clause; `admits` and `holds` the votes of an admission conference
typedef struct
{
    char experts[OVERSEER_EXPERTS_MOST][OVERSEER_EXPERT_LONGEST];
    unsigned int expert_count;
    OverseerVote votes[OVERSEER_VOTES_MOST];
    unsigned int vote_count;
    int addresses;
    int controls;
    int scratch;
    char carrier[OVERSEER_CARRIER_LONGEST];
    char standing[OVERSEER_STANDING_LONGEST];
    char learning[OVERSEER_CLAUSE_LONGEST];
    char counterexample[OVERSEER_CLAUSE_LONGEST];
    unsigned int accounts;
    unsigned int admits;
    unsigned int holds;
} OverseerHeard;

static OverseerHeard s_heard;

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
static unsigned int overseer_cause_read(const char *word)
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
static void overseer_trimmed(const char *from, const char *to, char *into, size_t room)
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
static void overseer_accounts_read(OverseerHeard *heard, const unsigned char *bytes, unsigned long long size,
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

// the lesson of `cause` at `carrier` holding `standing` kept, once: 1 where it is new
static int overseer_lesson_kept(unsigned int cause, const char *carrier, const char *standing)
{
    for (unsigned int at = 0u; at < s_lesson_count; at += 1u)
    {
        if ((strcmp(s_lessons[at].standing, standing) == 0) && (strcmp(s_lessons[at].carrier, carrier) == 0))
        {
            return 0;
        }
    }
    if (s_lesson_count >= OVERSEER_LESSONS_MOST)
    {
        return 0;
    }
    s_lessons[s_lesson_count].cause = cause;
    snprintf(s_lessons[s_lesson_count].carrier, OVERSEER_CARRIER_LONGEST, "%s", carrier);
    snprintf(s_lessons[s_lesson_count].standing, OVERSEER_STANDING_LONGEST, "%s", standing);
    s_lesson_count += 1u;
    return 1;
}

// `format` and what follows it added to the end of the ruling's words
static void overseer_said(OverseerRuling *ruling, const char *format, const char *text)
{
    const size_t length = strlen(ruling->said);
    if (length < (sizeof(ruling->said) - 1u))
    {
        snprintf(ruling->said + length, sizeof(ruling->said) - length, format, text);
    }
}

int overseer_convened(QryBuffer *run, const char *identity, int other_way_put, OverseerRuling *ruling)
{
    memset(ruling, 0, sizeof(*ruling));
    memset(&s_heard, 0, sizeof(s_heard));
    OverseerHeard *const heard = &s_heard;
    char name[OVERSEER_NAME_LONGEST];
    overseer_conference_named(identity, name);
    unsigned char *conference = NULL;
    unsigned long long conference_size = 0ull;
    // a question no party called a conference of, one answered as held or halted by the line, is heard by no one
    if ((run == NULL) || !qry_joined(run, name, &conference, &conference_size) || (conference_size == 0ull))
    {
        free(conference);
        return 0;
    }
    // the conference named in each row of it by the number of the question's identity
    const unsigned long long question = qry_row_text(run, identity, strlen(identity));
    overseer_accounts_read(heard, conference, conference_size, NULL, 0ull, run, question);
    // a panic on the line: every party's account of it heard too, those the conference already holds heard once
    if (qry_panicked(run) != 0ull)
    {
        unsigned char *call = NULL;
        unsigned long long call_size = 0ull;
        if (qry_joined(run, QRY_PANIC_CALL, &call, &call_size))
        {
            overseer_accounts_read(heard, call, call_size, conference, conference_size, run, question);
        }
        free(call);
    }
    free(conference);
    ruling->accounts = heard->accounts;
    if (heard->accounts == 0u)
    {
        return 0;
    }
    // the cause every expert that reads one agrees with: an exception agrees with each of the others
    unsigned int specific = OVERSEER_CAUSE_NONE;
    int agreed = (heard->vote_count != 0u);
    for (unsigned int at = 0u; at < heard->vote_count; at += 1u)
    {
        const unsigned int cause = heard->votes[at].cause;
        if (cause == OVERSEER_CAUSE_EXCEPTION)
        {
            continue;
        }
        agreed &= ((specific == OVERSEER_CAUSE_NONE) || (specific == cause)) ? 1 : 0;
        specific = (specific == OVERSEER_CAUSE_NONE) ? cause : specific;
    }
    ruling->agreed = agreed;
    ruling->cause = !agreed                             ? (unsigned int)OVERSEER_CAUSE_NONE
                    : (specific != OVERSEER_CAUSE_NONE) ? specific
                                                        : (unsigned int)OVERSEER_CAUSE_EXCEPTION;
    char count[96];
    snprintf(count, sizeof(count), "%u accounts heard, from", heard->accounts);
    overseer_said(ruling, "%s", count);
    for (unsigned int at = 0u; at < heard->expert_count; at += 1u)
    {
        overseer_said(ruling, (at == 0u) ? " %s" : ", %s", heard->experts[at]);
    }
    if (agreed)
    {
        overseer_said(ruling, "; the cause agreed: %s", s_cause_words[ruling->cause]);
    }
    else if (heard->vote_count == 0u)
    {
        overseer_said(ruling, "%s", "; no expert reads a cause");
    }
    else
    {
        overseer_said(ruling, "%s", "; no consensus:");
        for (unsigned int at = 0u; at < heard->vote_count; at += 1u)
        {
            char vote[96];
            snprintf(vote, sizeof(vote), "%s%s reads %s", (at == 0u) ? " " : ", ", heard->votes[at].expert,
                     s_cause_words[heard->votes[at].cause]);
            overseer_said(ruling, "%s", vote);
        }
    }
    // the side of the statement the agreed cause names: the stand-in where its own encoding or scratch is the cause or
    // it does what the cause names, and the link's context otherwise, a statement no account gives naming neither
    const int stated = (heard->standing[0] != '\0') && (heard->carrier[0] != '\0');
    const unsigned int cause = ruling->cause;
    const int stand_in = (cause == OVERSEER_CAUSE_INSTRUCTION) ||
                         ((cause == OVERSEER_CAUSE_REGISTER) && heard->scratch) ||
                         ((cause == OVERSEER_CAUSE_ADDRESS) && heard->addresses) ||
                         ((cause == OVERSEER_CAUSE_TIME) && heard->controls) ||
                         ((cause == OVERSEER_CAUSE_EXCEPTION) && (heard->addresses || heard->controls));
    ruling->side = (!agreed || !stated) ? (unsigned int)OVERSEER_SIDE_UNKNOWN
                   : stand_in           ? (unsigned int)OVERSEER_SIDE_STAND_IN
                                        : (unsigned int)OVERSEER_SIDE_CONTEXT;
    if (ruling->side == OVERSEER_SIDE_STAND_IN)
    {
        overseer_said(ruling, "%s", "; the stand-in brought it on");
    }
    else if (ruling->side == OVERSEER_SIDE_CONTEXT)
    {
        overseer_said(ruling, "%s", "; the link's context brought it on");
    }
    overseer_said(ruling, "; the learner was after: %s",
                  (heard->learning[0] != '\0') ? heard->learning : "nothing an account reads");
    if (heard->counterexample[0] != '\0')
    {
        overseer_said(ruling, "; the pair holds the counterexample %s", heard->counterexample);
    }
    ruling->guidance = (ruling->side == OVERSEER_SIDE_STAND_IN)
                           ? (other_way_put ? (unsigned int)OVERSEER_WAITS : (unsigned int)OVERSEER_OTHER_WAY)
                       : (ruling->side == OVERSEER_SIDE_CONTEXT) ? (unsigned int)OVERSEER_ANOTHER_LINK
                                                                 : (unsigned int)OVERSEER_HANDS_OFF;
    static const char *const s_guidance_said[] = {
        "; guidance: hands off, the learner goes on as it would", "; guidance: put the stand-in at its next link",
        "; guidance: put the pair the other way round", "; guidance: the relation waits for a later pass"};
    overseer_said(ruling, "%s", s_guidance_said[ruling->guidance]);
    // the lesson, as narrow as the conference: at every carrier where the stand-in's encoding or scratch is the
    // cause, and at the statement's carrier otherwise
    if (ruling->side != OVERSEER_SIDE_UNKNOWN)
    {
        const int everywhere = (ruling->side == OVERSEER_SIDE_STAND_IN) &&
                               ((cause == OVERSEER_CAUSE_INSTRUCTION) || (cause == OVERSEER_CAUSE_REGISTER));
        const char *const carrier = everywhere ? "*" : heard->carrier;
        ruling->taught = overseer_lesson_kept(cause, carrier, heard->standing);
        if (ruling->taught)
        {
            // the lesson a row of the run's table of lessons: its conference, its cause, its carrier, `*` at every
            // carrier, and the stand-in as written
            const unsigned long long lesson[4] = {question, cause, qry_row_text(run, carrier, strlen(carrier)),
                                                  qry_row_text(run, heard->standing, strlen(heard->standing))};
            qry_row_handed(run, OVERSEER_LESSONS_TABLE, lesson, 4u);
        }
        overseer_said(ruling, "; lesson: the stand-in %s", heard->standing);
        overseer_said(ruling, " held off the part %s", everywhere ? "at every carrier" : "at the carrier");
        if (!everywhere)
        {
            overseer_said(ruling, " %s", heard->carrier);
        }
        if (ruling->taught)
        {
            overseer_lessons_handed(run);
        }
    }
    overseer_account(run, identity, "overseer", ruling->said);
    // the conference a row of the run's table of conferences: the ruling, its accounts heard, the lesson where one was
    // taught, and its words
    const unsigned long long ruled[8] = {question,
                                         ruling->agreed ? 1ull : 0ull,
                                         ruling->cause,
                                         ruling->side,
                                         ruling->guidance,
                                         ruling->accounts,
                                         ruling->taught ? 1ull : 0ull,
                                         qry_row_text(run, ruling->said, strlen(ruling->said))};
    qry_row_handed(run, OVERSEER_CONFERENCES_TABLE, ruled, 8u);
    return 1;
}

int overseer_admission(QryBuffer *run, const char *identity, const char *encoding, OverseerRuling *ruling)
{
    memset(ruling, 0, sizeof(*ruling));
    memset(&s_heard, 0, sizeof(s_heard));
    OverseerHeard *const heard = &s_heard;
    char name[OVERSEER_NAME_LONGEST];
    overseer_conference_named(identity, name);
    unsigned char *conference = NULL;
    unsigned long long conference_size = 0ull;
    if ((run == NULL) || !qry_joined(run, name, &conference, &conference_size) || (conference_size == 0ull))
    {
        free(conference);
        return 0;
    }
    const unsigned long long question = qry_row_text(run, identity, strlen(identity));
    overseer_accounts_read(heard, conference, conference_size, NULL, 0ull, run, question);
    free(conference);
    ruling->accounts = heard->accounts;
    if (heard->accounts == 0u)
    {
        return 0;
    }
    // admitted by consensus the other way round: one party at least admits and none holds
    const int admitted = (heard->admits != 0u) && (heard->holds == 0u);
    ruling->agreed = admitted;
    ruling->cause = admitted ? (unsigned int)OVERSEER_CAUSE_ADMITTED : (unsigned int)OVERSEER_CAUSE_NONE;
    ruling->side = (unsigned int)OVERSEER_SIDE_UNKNOWN;
    ruling->guidance = admitted ? (unsigned int)OVERSEER_PUT : (unsigned int)OVERSEER_HANDS_OFF;
    char count[160];
    snprintf(count, sizeof(count), "%u accounts heard, %u admit, %u hold, from", heard->accounts, heard->admits,
             heard->holds);
    overseer_said(ruling, "%s", count);
    for (unsigned int at = 0u; at < heard->expert_count; at += 1u)
    {
        overseer_said(ruling, (at == 0u) ? " %s" : ", %s", heard->experts[at]);
    }
    if (admitted)
    {
        // the lesson, as narrow as the conference: the slot's encoding, at every carrier
        ruling->taught = overseer_lesson_kept((unsigned int)OVERSEER_CAUSE_ADMITTED, "*", encoding);
        if (ruling->taught)
        {
            const unsigned long long lesson[4] = {question, (unsigned int)OVERSEER_CAUSE_ADMITTED,
                                                  qry_row_text(run, "*", 1u),
                                                  qry_row_text(run, encoding, strlen(encoding))};
            qry_row_handed(run, OVERSEER_LESSONS_TABLE, lesson, 4u);
            overseer_lessons_handed(run);
        }
        overseer_said(ruling, "; admitted: the slot encoding %s put past the gate's key rule at every carrier",
                      encoding);
    }
    else
    {
        overseer_said(ruling, "%s",
                      (heard->admits == 0u) ? "; hands off: no party admits it" : "; hands off: a party holds it");
    }
    overseer_account(run, identity, "overseer", ruling->said);
    const unsigned long long ruled[8] = {question,
                                         ruling->agreed ? 1ull : 0ull,
                                         ruling->cause,
                                         ruling->side,
                                         ruling->guidance,
                                         ruling->accounts,
                                         ruling->taught ? 1ull : 0ull,
                                         qry_row_text(run, ruling->said, strlen(ruling->said))};
    qry_row_handed(run, OVERSEER_CONFERENCES_TABLE, ruled, 8u);
    return admitted;
}

int overseer_admits(unsigned long long low, unsigned long long high)
{
    char encoding[48];
    snprintf(encoding, sizeof(encoding), "%016llx %016llx", low, high);
    for (unsigned int at = 0u; at < s_lesson_count; at += 1u)
    {
        if ((s_lessons[at].cause == (unsigned int)OVERSEER_CAUSE_ADMITTED) &&
            (strcmp(s_lessons[at].standing, encoding) == 0))
        {
            return 1;
        }
    }
    return 0;
}

int overseer_holds(const char *standing, const char *carrier, char *why, size_t room)
{
    char joined[OVERSEER_STANDING_LONGEST];
    overseer_standing_joined(standing, joined);
    for (unsigned int at = 0u; at < s_lesson_count; at += 1u)
    {
        const OverseerLesson *const lesson = &s_lessons[at];
        if (lesson->cause == (unsigned int)OVERSEER_CAUSE_ADMITTED)
        {
            continue;
        }
        if ((strcmp(lesson->standing, joined) == 0) &&
            ((strcmp(lesson->carrier, "*") == 0) || (strcmp(lesson->carrier, carrier) == 0)))
        {
            snprintf(why, room, "overseen: the conference held it, its cause %s", s_cause_words[lesson->cause]);
            return 1;
        }
    }
    return 0;
}

unsigned int overseer_lessons_read(QryBuffer *run)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if ((run == NULL) || !qry_latest(run, OVERSEER_LESSONS, &bytes, &size, NULL))
    {
        return 0u;
    }
    unsigned int read = 0u;
    unsigned long long at = 0ull;
    while (at < size)
    {
        const unsigned char *const line = bytes + at;
        const unsigned char *const found = (const unsigned char *)memchr(line, '\n', (size_t)(size - at));
        const size_t length = (size_t)(((found != NULL) ? found : (bytes + size)) - line);
        at += (unsigned long long)length + 1ull;
        char text[OVERSEER_STANDING_LONGEST + 128u];
        overseer_trimmed((const char *)line, (const char *)line + length, text, sizeof(text));
        char cause[32];
        char carrier[OVERSEER_CARRIER_LONGEST];
        int standing_at = 0;
        if ((sscanf(text, "lesson %31s %63s %n", cause, carrier, &standing_at) >= 2) && (standing_at > 0) &&
            (text[standing_at] != '\0'))
        {
            read += (unsigned int)overseer_lesson_kept(overseer_cause_read(cause), carrier, text + standing_at);
        }
    }
    return read;
}

int overseer_lessons_handed(QryBuffer *run)
{
    if (run == NULL)
    {
        return 0;
    }
    if (s_lesson_count == 0u)
    {
        return 1;
    }
    const size_t room = (size_t)s_lesson_count * (OVERSEER_STANDING_LONGEST + OVERSEER_CARRIER_LONGEST + 32u) + 1u;
    char *const text = (char *)malloc(room);
    if (text == NULL)
    {
        return 0;
    }
    size_t length = 0u;
    for (unsigned int at = 0u; at < s_lesson_count; at += 1u)
    {
        const OverseerLesson *const lesson = &s_lessons[at];
        const int written = snprintf(text + length, room - length, "lesson %s %s %s\n", s_cause_words[lesson->cause],
                                     lesson->carrier, lesson->standing);
        // each line fits the room sized for it, and its length is not negative
        length += (written > 0) ? (size_t)written : 0u;
    }
    const int handed = qry_hand(run, OVERSEER_LESSONS, text, (unsigned long long)length, QRY_ORDINARY) != 0ull;
    free(text);
    return handed;
}

void overseer_lessons_cleared(void)
{
    s_lesson_count = 0u;
}
