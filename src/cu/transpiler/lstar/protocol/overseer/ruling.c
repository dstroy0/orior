// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ruling.c: the ruling: a lesson stored, what the overseer stated, the conference convened and ruled, the admission of
// a blocked encoding, what the lessons admit and block, and the lessons read, handed and cleared
#include "overseer_internal.h"
#include "verba_scribo/verba_scribo.h"
#include "../table_text.h"
#include "../../../../includes/parsers/who.h"
#include "../../../../includes/parsers/line_read.h"

// the lesson of `cause` at `carrier` applying `standing` stored, once: 1 where it is new
static int overseer_lesson_stored(unsigned int cause, const char *carrier, const char *standing)
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
    (void)verba_finis.finish(&(VerbaFinisCfg){.out = s_lessons[s_lesson_count].carrier, .cap = OVERSEER_CARRIER_MAX_LEN, .at = verba_textus.put_clip(&(VerbaTextusCfg){.out = s_lessons[s_lesson_count].carrier, .cap = OVERSEER_CARRIER_MAX_LEN, .at = 0u, .text = carrier})});
    (void)verba_finis.finish(&(VerbaFinisCfg){.out = s_lessons[s_lesson_count].standing, .cap = OVERSEER_STANDING_MAX_LEN, .at = verba_textus.put_clip(&(VerbaTextusCfg){.out = s_lessons[s_lesson_count].standing, .cap = OVERSEER_STANDING_MAX_LEN, .at = 0u, .text = standing})});
    s_lesson_count += 1u;
    return 1;
}

// `text` added to the end of the ruling's words, cut at their room
static void overseer_text_added(OverseerRuling *ruling, const char *text)
{
    const size_t length = strlen(ruling->text);
    (void)verba_finis.finish(&(VerbaFinisCfg){.out = ruling->text + length, .cap = sizeof(ruling->text) - length, .at = verba_textus.put_clip(&(VerbaTextusCfg){.out = ruling->text + length, .cap = sizeof(ruling->text) - length, .at = 0u, .text = text})});
}

// the conference and the panic call, each every record of its name the run contains, gathered
static unsigned char s_conference[QRY_WRITER_RING_BYTES / 2ull];
static unsigned char s_call[QRY_WRITER_RING_BYTES / 2ull];

int overseer_convened(QryShared *run, const char *identity, int other_way_put, OverseerRuling *ruling)
{
    memset(ruling, 0, sizeof(*ruling));
    memset(&s_conference_tally, 0, sizeof(s_conference_tally));
    OverseerTally *const tally = &s_conference_tally;
    char name[OVERSEER_NAME_MAX_LEN];
    overseer_conference_named(identity, name);
    const unsigned char *const conference = s_conference;
    const unsigned long long gathered = qry_member_gathered(name, s_conference, sizeof(s_conference));
    const unsigned long long conference_size = (gathered == QRY_MEMBER_UNREAD) ? 0ull : gathered;
    // a question no party called a conference of, one answered as blocked or halted by the line, is received by no one
    if ((run == NULL) || (conference_size == 0ull))
    {
        return 0;
    }
    // the conference named in each row of it by the number of the question's identity
    const unsigned long long question = table_text(identity, strlen(identity));
    overseer_accounts_read(tally, conference, conference_size, NULL, 0ull, run, question);
    // a panic on the line: every party's account of it received too, those the conference already contains received
    // once
    if (qry_member_panicked() != 0ull)
    {
        const unsigned long long call_size = qry_member_gathered(QRY_PANIC_CALL, s_call, sizeof(s_call));
        if (call_size != QRY_MEMBER_UNREAD)
        {
            overseer_accounts_read(tally, s_call, call_size, conference, conference_size, run, question);
        }
    }
    ruling->accounts = tally->accounts;
    if (tally->accounts == 0u)
    {
        return 0;
    }
    // the cause every expert that reads one agrees with: an exception agrees with each of the others
    unsigned int specific = OVERSEER_CAUSE_NONE;
    int agreed = (tally->vote_count != 0u);
    for (unsigned int at = 0u; at < tally->vote_count; at += 1u)
    {
        const unsigned int cause = tally->votes[at].cause;
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
    // every cause read the question's own: none reads a launch that did not return
    int own = (tally->vote_count != 0u);
    for (unsigned int at = 0u; at < tally->vote_count; at += 1u)
    {
        own = own && (tally->votes[at].cause != (unsigned int)OVERSEER_CAUSE_TIME) &&
              (tally->votes[at].cause != (unsigned int)OVERSEER_CAUSE_NONE);
    }
    ruling->own = own;
    char count[96];
    VerbaLinea count_line = {count, sizeof(count), 0ull};
    mmgr_line_decimal(&count_line, tally->accounts, 0u);
    mmgr_line_put(&count_line, " accounts received, from");
    mmgr_line_finish(&count_line);
    overseer_text_added(ruling, count);
    for (unsigned int at = 0u; at < tally->expert_count; at += 1u)
    {
        overseer_text_added(ruling, (at == 0u) ? " " : ", ");
        overseer_text_added(ruling, tally->experts[at]);
    }
    if (agreed)
    {
        overseer_text_added(ruling, "; the cause agreed: ");
        overseer_text_added(ruling, s_cause_words[ruling->cause]);
    }
    else if (own)
    {
        overseer_text_added(ruling, "; no consensus on the clause, every cause read the question's own:");
        for (unsigned int at = 0u; at < tally->vote_count; at += 1u)
        {
            overseer_text_added(ruling, (at == 0u) ? " " : ", ");
            overseer_text_added(ruling, tally->votes[at].expert);
            overseer_text_added(ruling, " reads ");
            overseer_text_added(ruling, s_cause_words[tally->votes[at].cause]);
        }
    }
    else if (tally->vote_count == 0u)
    {
        overseer_text_added(ruling, "; no expert reads a cause");
    }
    else
    {
        overseer_text_added(ruling, "; no consensus:");
        for (unsigned int at = 0u; at < tally->vote_count; at += 1u)
        {
            overseer_text_added(ruling, (at == 0u) ? " " : ", ");
            overseer_text_added(ruling, tally->votes[at].expert);
            overseer_text_added(ruling, " reads ");
            overseer_text_added(ruling, s_cause_words[tally->votes[at].cause]);
        }
    }
    // the side of the statement the agreed cause names: the stand-in where its own encoding or scratch is the cause or
    // it does what the cause names, and the link's context otherwise, a statement no account gives naming neither
    const int stated = (tally->standing[0] != '\0') && (tally->carrier[0] != '\0');
    const unsigned int cause = ruling->cause;
    const int stand_in = (cause == OVERSEER_CAUSE_INSTRUCTION) ||
                         ((cause == OVERSEER_CAUSE_REGISTER) && tally->scratch) ||
                         ((cause == OVERSEER_CAUSE_ADDRESS) && tally->addresses) ||
                         ((cause == OVERSEER_CAUSE_TIME) && tally->controls) ||
                         ((cause == OVERSEER_CAUSE_EXCEPTION) && (tally->addresses || tally->controls));
    ruling->side = (!agreed || !stated) ? (unsigned int)OVERSEER_SIDE_UNKNOWN
                   : stand_in           ? (unsigned int)OVERSEER_SIDE_STAND_IN
                                        : (unsigned int)OVERSEER_SIDE_CONTEXT;
    if (ruling->side == OVERSEER_SIDE_STAND_IN)
    {
        overseer_text_added(ruling, "; the stand-in brought it on");
    }
    else if (ruling->side == OVERSEER_SIDE_CONTEXT)
    {
        overseer_text_added(ruling, "; the link's context brought it on");
    }
    overseer_text_added(ruling, "; the learner was after: ");
    overseer_text_added(ruling, (tally->learning[0] != '\0') ? tally->learning : "nothing an account reads");
    if (tally->counterexample[0] != '\0')
    {
        overseer_text_added(ruling, "; the pair contains the counterexample ");
        overseer_text_added(ruling, tally->counterexample);
    }
    ruling->guidance = (ruling->side == OVERSEER_SIDE_STAND_IN)
                           ? (other_way_put ? (unsigned int)OVERSEER_WAITS : (unsigned int)OVERSEER_OTHER_WAY)
                       : (ruling->side == OVERSEER_SIDE_CONTEXT) ? (unsigned int)OVERSEER_ANOTHER_LINK
                                                                 : (unsigned int)OVERSEER_HANDS_OFF;
    static const char *const s_guidance_text[] = {
        "; guidance: hands off, the learner goes on as it would", "; guidance: put the stand-in at its next link",
        "; guidance: put the pair the other way round", "; guidance: the relation waits for a later pass"};
    overseer_text_added(ruling, s_guidance_text[ruling->guidance]);
    // the lesson, as narrow as the conference: at every carrier where the stand-in's encoding or scratch is the
    // cause, and at the statement's carrier otherwise
    if (ruling->side != OVERSEER_SIDE_UNKNOWN)
    {
        const int everywhere = (ruling->side == OVERSEER_SIDE_STAND_IN) &&
                               ((cause == OVERSEER_CAUSE_INSTRUCTION) || (cause == OVERSEER_CAUSE_REGISTER));
        const char *const carrier = everywhere ? "*" : tally->carrier;
        ruling->taught = overseer_lesson_stored(cause, carrier, tally->standing);
        if (ruling->taught)
        {
            // the lesson a row of the run's table of lessons: its conference, its cause, its carrier, `*` at every
            // carrier, and the stand-in as written
            unsigned long long lesson[2u + 4] = {0ull,
                                                 0ull,
                                                 question,
                                                 cause,
                                                 table_text(carrier, strlen(carrier)),
                                                 table_text(tally->standing, strlen(tally->standing))};
            who_words(WHO_HERE, lesson);
            qry_member_append(OVERSEER_LESSONS_TABLE, lesson, sizeof(lesson), QRY_ORDINARY);
        }
        overseer_text_added(ruling, "; lesson: the stand-in ");
        overseer_text_added(ruling, tally->standing);
        overseer_text_added(ruling, " blocked from the part ");
        overseer_text_added(ruling, everywhere ? "at every carrier" : "at the carrier");
        if (!everywhere)
        {
            overseer_text_added(ruling, " ");
            overseer_text_added(ruling, tally->carrier);
        }
        if (ruling->taught)
        {
            overseer_lessons_handed(run);
        }
    }
    overseer_account(run, identity, "overseer", ruling->text);
    // the conference a row of the run's table of conferences: the ruling, its accounts received, the lesson where one
    // was taught, and its words
    unsigned long long ruled[2u + 8] = {0ull,
                                        0ull,
                                        question,
                                        ruling->agreed ? 1ull : 0ull,
                                        ruling->cause,
                                        ruling->side,
                                        ruling->guidance,
                                        ruling->accounts,
                                        ruling->taught ? 1ull : 0ull,
                                        table_text(ruling->text, strlen(ruling->text))};
    who_words(WHO_HERE, ruled);
    qry_member_append(OVERSEER_CONFERENCES_TABLE, ruled, sizeof(ruled), QRY_ORDINARY);
    return 1;
}

int overseer_admission(QryShared *run, const char *identity, const char *encoding, OverseerRuling *ruling)
{
    memset(ruling, 0, sizeof(*ruling));
    memset(&s_conference_tally, 0, sizeof(s_conference_tally));
    OverseerTally *const tally = &s_conference_tally;
    char name[OVERSEER_NAME_MAX_LEN];
    overseer_conference_named(identity, name);
    const unsigned char *const conference = s_conference;
    const unsigned long long gathered = qry_member_gathered(name, s_conference, sizeof(s_conference));
    const unsigned long long conference_size = (gathered == QRY_MEMBER_UNREAD) ? 0ull : gathered;
    if ((run == NULL) || (conference_size == 0ull))
    {
        return 0;
    }
    const unsigned long long question = table_text(identity, strlen(identity));
    overseer_accounts_read(tally, conference, conference_size, NULL, 0ull, run, question);
    ruling->accounts = tally->accounts;
    if (tally->accounts == 0u)
    {
        return 0;
    }
    // admitted by consensus the other way round: one party at least admits and none blocks
    const int admitted = (tally->admits != 0u) && (tally->blocks == 0u);
    ruling->agreed = admitted;
    ruling->cause = admitted ? (unsigned int)OVERSEER_CAUSE_ADMITTED : (unsigned int)OVERSEER_CAUSE_NONE;
    ruling->side = (unsigned int)OVERSEER_SIDE_UNKNOWN;
    ruling->guidance = admitted ? (unsigned int)OVERSEER_PUT : (unsigned int)OVERSEER_HANDS_OFF;
    char count[160];
    VerbaLinea count_line = {count, sizeof(count), 0ull};
    mmgr_line_decimal(&count_line, tally->accounts, 0u);
    mmgr_line_put(&count_line, " accounts received, ");
    mmgr_line_decimal(&count_line, tally->admits, 0u);
    mmgr_line_put(&count_line, " admit, ");
    mmgr_line_decimal(&count_line, tally->blocks, 0u);
    mmgr_line_put(&count_line, " block, from");
    mmgr_line_finish(&count_line);
    overseer_text_added(ruling, count);
    for (unsigned int at = 0u; at < tally->expert_count; at += 1u)
    {
        overseer_text_added(ruling, (at == 0u) ? " " : ", ");
        overseer_text_added(ruling, tally->experts[at]);
    }
    if (admitted)
    {
        // the lesson, as narrow as the conference: the probe's encoding, at every carrier
        ruling->taught = overseer_lesson_stored((unsigned int)OVERSEER_CAUSE_ADMITTED, "*", encoding);
        if (ruling->taught)
        {
            unsigned long long lesson[2u + 4] = {0ull, 0ull, question, (unsigned int)OVERSEER_CAUSE_ADMITTED,
                                                  table_text("*", 1u),
                                                  table_text(encoding, strlen(encoding))};
            who_words(WHO_HERE, lesson);
            qry_member_append(OVERSEER_LESSONS_TABLE, lesson, sizeof(lesson), QRY_ORDINARY);
            overseer_lessons_handed(run);
        }
        overseer_text_added(ruling, "; admitted: the probe encoding ");
        overseer_text_added(ruling, encoding);
        overseer_text_added(ruling, " put past the gate's key rule at every carrier");
    }
    else
    {
        overseer_text_added(ruling, (tally->admits == 0u) ? "; hands off: no party admits it"
                                                          : "; hands off: a party blocks it");
    }
    overseer_account(run, identity, "overseer", ruling->text);
    unsigned long long ruled[2u + 8] = {0ull,
                                        0ull,
                                        question,
                                        ruling->agreed ? 1ull : 0ull,
                                        ruling->cause,
                                        ruling->side,
                                        ruling->guidance,
                                        ruling->accounts,
                                        ruling->taught ? 1ull : 0ull,
                                        table_text(ruling->text, strlen(ruling->text))};
    who_words(WHO_HERE, ruled);
    qry_member_append(OVERSEER_CONFERENCES_TABLE, ruled, sizeof(ruled), QRY_ORDINARY);
    return admitted;
}

int overseer_admits(unsigned long long low, unsigned long long high)
{
    char encoding[48];
    VerbaLinea encoding_line = {encoding, sizeof(encoding), 0ull};
    mmgr_line_hex(&encoding_line, low, 16u);
    mmgr_line_ch(&encoding_line, ' ');
    mmgr_line_hex(&encoding_line, high, 16u);
    mmgr_line_finish(&encoding_line);
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

int overseer_blocks(const char *standing, const char *carrier, char *why, size_t room)
{
    char joined[OVERSEER_STANDING_MAX_LEN];
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
            VerbaLinea why_line = {why, room, 0ull};
            mmgr_line_put(&why_line, "overseen: the conference blocked it, its cause ");
            mmgr_line_put(&why_line, s_cause_words[lesson->cause]);
            mmgr_line_finish(&why_line);
            return 1;
        }
    }
    return 0;
}

unsigned int overseer_lessons_read(QryShared *run)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if ((run == NULL) || !qry_member_blob(OVERSEER_LESSONS, &bytes, &size, NULL))
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
        char text[OVERSEER_STANDING_MAX_LEN + 128u];
        overseer_trimmed((const char *)line, (const char *)line + length, text, sizeof(text));
        char cause[32];
        char carrier[OVERSEER_CARRIER_MAX_LEN];
        const char *standing = text;
        int lesson = line_letters(&standing, "lesson ") && line_word(&standing, cause, sizeof(cause)) &&
                     line_word(&standing, carrier, sizeof(carrier));
        while (lesson && ((*standing == ' ') || (*standing == '\t')))
        {
            standing += 1;
        }
        if (lesson && (*standing != '\0'))
        {
            read += (unsigned int)overseer_lesson_stored(overseer_cause_read(cause), carrier, standing);
        }
    }
    return read;
}

int overseer_lessons_handed(QryShared *run)
{
    if (run == NULL)
    {
        return 0;
    }
    if (s_lesson_count == 0u)
    {
        return 1;
    }
    // a line a lesson, each sized for its longest
    static char s_lessons_text[OVERSEER_LESSONS_MOST * (OVERSEER_STANDING_MAX_LEN + OVERSEER_CARRIER_MAX_LEN + 32u)];
    VerbaLinea text = {s_lessons_text, sizeof(s_lessons_text), 0ull};
    for (unsigned int at = 0u; at < s_lesson_count; at += 1u)
    {
        const OverseerLesson *const lesson = &s_lessons[at];
        mmgr_line_put(&text, "lesson ");
        mmgr_line_put(&text, s_cause_words[lesson->cause]);
        mmgr_line_ch(&text, ' ');
        mmgr_line_put(&text, lesson->carrier);
        mmgr_line_ch(&text, ' ');
        mmgr_line_put(&text, lesson->standing);
        mmgr_line_ch(&text, '\n');
    }
    return mmgr_line_ok(&text) && (qry_member_append(OVERSEER_LESSONS, text.out, text.at, QRY_ORDINARY) != 0);
}

void overseer_lessons_cleared(void)
{
    s_lesson_count = 0u;
}
