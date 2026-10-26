// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef OVERSEER_H
#define OVERSEER_H

// The overseer: the run watching its learner, and stepping in only where a question crashes the system.
//
// The teacher answers what the part answers: a minimally adequate teacher. A question the part refuses or faults on is
// censored by its identity, and the learner goes on as though nothing had been learned of it. The overseer makes the
// teacher more. A question the part refused, or a launch that faulted the part or hung, calls a
// conference: every party with an account of the question hands it to the run (qry_member.h, QRY_CONFERENCE_CALL),
// and the overseer reads them all. A question that answers, alike, apart or gray, calls none, and the learner learns as
// it would.
//
// The roster is open. Whatever has an account of the question joins by handing one under the conference's name: the
// carrier's proctor, the part's fault telemetry (the listener) and the gate in the carrier's answer, the teacher, the
// learner's statement of what it put, the coherence sets of the pair it was querying about, and the counterexamples the
// pair contains. An account is one line, `<expert>: <what it saw>`, and may end in clauses the conference reads, each
// after a `;`:
//
//   cause <instruction|address|register|time|exception>   what the expert reads the crash as: the instruction's own
//                                                         encoding, an address the code reached, a register past what
//                                                         the kernel contains, a launch that did not return, or an
//                                                         exception that names none of those and agrees with each
//   stand-in <addresses|controls|scratch>                 what the statement's stand-in does: reads or writes memory
//                                                         through an address, decides which instruction runs next, or
//                                                         takes scratch registers past the carrier's own
//   carrier <name>                                        the carrier the statement was put in
//   standing <text>                                       the stand-in as written, its lines joined by a space: the
//                                                         last clause of its account, taking the rest of the line
//   learning <text>                                       what the learner was after: the sets the pair is read into
//   counterexample <text>                                 a first case apart the pair already contains
//
// The ruling is by consensus. The cause ruled is a cause every expert that reads a cause agrees with; an exception
// agrees with any of the other four. Where two experts read two causes, or none reads one, the conference rules
// nothing, and the learner goes on as it would, the dissent recorded. Where the cause is agreed, the overseer reads
// which side of the statement brought it on: the stand-in, where its own encoding is the cause, its scratch is, or it
// does what the cause names (an address, a launch that never returned through control); and otherwise the link's
// context, the carrier around the stand-in. Then it guides:
//
//   the stand-in   the learner puts the pair the other way round, the stand-in retained as the carrier's own form,
//   which
//                  the part has run; where that way is put too, the relation waits for a later pass
//   the context    the learner puts the stand-in at its next link
//
// and records a lesson: the stand-in as written is blocked from the part from then on, at every carrier where its
// encoding or its scratch is the cause, and at that carrier alone otherwise. A lesson is as narrow as the conference
// that taught it, a stand-in as written and not a form or a class, so that no question the part would answer is blocked
// from the learner. The lessons are the run's blob OVERSEER_LESSONS, read where a run carried them in and handed whole
// as each is taught, and a question a lesson blocks is answered as blocked, the part never seeing it.
//
// The ruling is handed to the conference as the overseer's own account. Every account the overseer receives, every
// conference it convenes and every lesson it teaches is a row of the run's tables of accounts, conferences and lessons
// (accounts.oracle.tsv, conferences.oracle.tsv, lessons.oracle.tsv, qry_member.h), each naming its conference by the
// number of the question's identity in the table of texts. The overseer is called from one thread at a time.
//
// A second kind of conference is called by a block and not a crash: the gate blocks a question whose operation key no
// form of the machine file and no instruction of the kernel's own contains (cubin_safe.h, rule 3), which is gray and
// not a refusal, and the parties with a reading of the question hand it under the conference's name as above, each
// ending in a vote: `admit` where its reading admits the question to the part, `block` where it blocks it, and `gray`
// where it reads neither. The cross votes on its reading of the probe place, admit where the vendor's disassembler
// reads a legal instruction that is no control transfer, wait or memory access and names no register past the count;
// the gate reports what it blocked and why, gray; the record reports what it contains of the identity, admit where
// nothing or a block of this kind, block where a refusal. The ruling is by consensus the other way: admitted where one
// party at least admits and none blocks (overseer_admission). An admission is a lesson of its own cause, as narrow as
// the conference, the question's probe encoding at every carrier (`lesson admitted * <low> <high>`): the gate puts an
// encoding a lesson admits past rule 3 (orior_cubin_run --lessons), and a query the gate blocked under the rule is
// queried again, since the part never saw it.

#include "../../../../includes/file_defs/qry/writer/qry_member.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

// the blob the lessons are stored in, a lesson a line: `lesson <cause> <carrier or *> <the stand-in as written>`, and
// for an admission `lesson admitted * <low> <high>`, the probe encoding's two words in hex
#define OVERSEER_LESSONS "overseer.txt"

// the longest conference name, an account, the overseer's ruling, and a stand-in as written
#define OVERSEER_NAME_MAX_LEN 160u
#define OVERSEER_ACCOUNT_MAX_LEN 2048u
#define OVERSEER_RULING_MAX_LEN 2048u
#define OVERSEER_STANDING_MAX_LEN 512u

    // what the experts agree the crash was; and admitted, no crash, the cause of a lesson that admits an encoding
    typedef enum
    {
        OVERSEER_CAUSE_NONE,
        OVERSEER_CAUSE_INSTRUCTION,
        OVERSEER_CAUSE_ADDRESS,
        OVERSEER_CAUSE_REGISTER,
        OVERSEER_CAUSE_TIME,
        OVERSEER_CAUSE_EXCEPTION,
        OVERSEER_CAUSE_ADMITTED
    } OverseerCause;

    // which side of the statement brought it on
    typedef enum
    {
        OVERSEER_SIDE_UNKNOWN,
        OVERSEER_SIDE_STAND_IN,
        OVERSEER_SIDE_CONTEXT
    } OverseerSide;

    // what the learner does next
    typedef enum
    {
        // no consensus: the learner goes on as it would
        OVERSEER_HANDS_OFF,
        // the stand-in at its next link
        OVERSEER_ANOTHER_LINK,
        // the pair the other way round
        OVERSEER_OTHER_WAY,
        // the relation waits for a later pass
        OVERSEER_WAITS,
        // the question put, its encoding admitted past the gate's rule 3
        OVERSEER_PUT
    } OverseerGuidance;

    // the conference's ruling
    typedef struct
    {
        // 1 where the experts agreed a cause
        int agreed;
        // 1 where every cause an expert reads is the question's own, its instruction, an address it reached or a
        // register it named, an exception among them, and one at least reads a cause: the part was left whole and
        // the question is to blame, whichever clause the experts name, and a run may go on past it
        int own;
        unsigned int cause;
        unsigned int side;
        unsigned int guidance;
        // the accounts received, and 1 where a lesson was taught
        unsigned int accounts;
        int taught;
        // the ruling as the overseer's account states it
        char text[OVERSEER_RULING_MAX_LEN];
    } OverseerRuling;

    // the name of the conference of the question `identity`, into `name`, which has room for OVERSEER_NAME_MAX_LEN
    void overseer_conference_named(const char *identity, char *name);

    // `expert`'s account `text` handed to the conference of the question `identity` in the run `run`: 1, or 0 where it
    // was not handed
    int overseer_account(QryShared *run, const char *identity, const char *expert, const char *text);

    // The conference of the question `identity` convened: every account handed to it read, with the panic call's where
    // a panic is on the line, and the ruling by consensus into `ruling`, handed to the conference as the overseer's
    // account. `other_way_put` is 1 where the learner has put the pair the other way round already. 1 where any account
    // was received, 0 where none was
    int overseer_convened(QryShared *run, const char *identity, int other_way_put, OverseerRuling *ruling);

    // The conference of the question `identity` the gate blocked for an unknown operation key, convened: every account
    // handed to it read and its vote counted, and the ruling into `ruling`, handed to the conference as the overseer's
    // account: admitted, guidance OVERSEER_PUT, where one party at least admits and none blocks, and a lesson of the
    // cause admitted taught for `encoding`, the probe's two words in hex `<low> <high>`, at every carrier; hands off
    // otherwise. 1 where it was admitted, 0 where it was not or no account was received
    int overseer_admission(QryShared *run, const char *identity, const char *encoding, OverseerRuling *ruling);

    // 1 where a lesson admits the probe encoding `low` and `high` past the gate's rule 3, by its two words as a lesson
    // of the cause admitted writes them; 0 where none does
    int overseer_admits(unsigned long long low, unsigned long long high);

    // 1 where a lesson blocks the stand-in `standing`, as written, off the part at the carrier `carrier`, with the
    // lesson's cause into `why`, which has room for `room`; 0 where none does. A lesson of the cause admitted blocks
    // nothing
    int overseer_blocks(const char *standing, const char *carrier, char *why, size_t room);

    // the lessons a run carried in, the latest blob OVERSEER_LESSONS of `run`, read: their count
    unsigned int overseer_lessons_read(QryShared *run);

    // every lesson stored handed to `run` whole as OVERSEER_LESSONS: 1, where it was handed or none is stored, or 0
    // where it was not handed
    int overseer_lessons_handed(QryShared *run);

    // a stand-in as written with its lines joined by a space, into `joined`, which has room for
    // OVERSEER_STANDING_MAX_LEN
    void overseer_standing_joined(const char *standing, char *joined);

    // the lessons let go
    void overseer_lessons_cleared(void);

#ifdef __cplusplus
}
#endif

#endif
