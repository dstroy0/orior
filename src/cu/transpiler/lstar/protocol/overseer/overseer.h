// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef OVERSEER_H
#define OVERSEER_H

// The overseer: the run that watches its learner, and steps in only where a question crashes the system.
//
// The teacher answers what the part answers: a minimally adequate teacher. The teacher censors a question that the part
// refuses or faults on, by its identity. The learner then goes on as though it learned nothing of it.
//
// The overseer makes the teacher more. A question that the part refused, or a launch that faulted the part or hung,
// calls a conference. Every party with an account of the question passes it to the run (qry_member.h,
// QRY_CONFERENCE_CALL), and the overseer reads them all. A question that answers, alike, apart or gray, calls none, and
// the learner learns as it would.
//
// The roster is open: whatever has an account of the question joins, and hands it in under the conference's name. The
// parties are the carrier's proctor, the part's fault telemetry (the listener) and the gate in the carrier's answer.
// They also include the teacher and the learner's statement of what it put. The coherence sets of the pair that it asks
// about count too, with the counterexamples that the pair contains. An account is one line, `<expert>: <what it saw>`.
// It may end in clauses that the conference reads, each after a `;`:
//   - `cause <instruction|address|register|time|exception>`: what the expert reads the crash as. That is the
//     instruction's own encoding, an address that the code reached, or a register past what the kernel contains. It can
//     also be a launch that did not return, or an exception. An exception names none of those, and agrees with each.
//   - `stand-in <addresses|controls|scratch>`: what the statement's stand-in does. It reads or writes memory through an
//     address, decides which instruction runs next, or takes scratch registers past the carrier's own.
//   - `carrier <name>`: the carrier that holds the statement.
//   - `located <text>`: the stand-in as written, its lines joined by a space. It is the last clause of its account, and
//     takes the rest of the line.
//   - `learning <text>`: what the learner was after, the sets that the learner reads the pair into.
//   - `counterexample <text>`: a first case apart that the pair already contains.
//
// The verdict is by consensus. Its cause is a cause that every expert with a cause agrees with, and an exception agrees
// with any of the other four. Where two experts read two causes, or none reads one, the conference finds nothing. The
// learner then goes on as it would, with the dissent on record.
//
// Where the experts agree a cause, the overseer reads which side of the statement brought it on. That is the stand-in
// where its own encoding is the cause, or its scratch is. It is also the stand-in where it does what the cause names
// (an address, or a launch that never returned through control). Otherwise it is the link's context, the carrier around
// the stand-in. Then it guides:
//   - the stand-in: the learner puts the pair the other way round. The stand-in stays as the carrier's own form, a form
//     that the part already ran. Where the learner already put that way too, the relation waits for a later pass.
//   - the context: the learner puts the stand-in at its next link.
//
// The overseer also records a lesson. From then on, the lesson blocks the stand-in as written from the part. It blocks
// it at every carrier where its encoding or its scratch is the cause, and at that carrier alone otherwise. A lesson is
// as narrow as the conference that taught it: a stand-in as written, and not a form or a class. No lesson then blocks
// from the learner a question that the part would answer.
//
// The lessons are the run's blob OVERSEER_LESSONS. The overseer reads them where a run carried them in, and passes them
// whole as it teaches each one. The teacher answers `blocked` to a question that a lesson blocks, and the part never
// sees it.
//
// The overseer passes the verdict to the conference as its own account. Each account that the overseer receives is a
// row of the run's table of accounts. Each conference that it convenes is a row of the table of conferences, and each
// lesson a row of the table of lessons. The tables are accounts.oracle.tsv, conferences.oracle.tsv and
// lessons.oracle.tsv (qry_member.h). Each row names its conference by the number of the question's identity in the
// table of texts. One thread at a time calls the overseer.
//
// A block, and not a crash, calls a second type of conference. The gate blocks a question whose operation key no form
// of the machine file and no instruction of the kernel's own contains (cubin_safe.h, rule 3). That is gray, and not a
// refusal. The parties with a reading of the question hand it in under the conference's name as above, and each ends in
// a vote. The vote is `admit` where its reading admits the question to the part, `block` where it blocks it, and `gray`
// where it reads neither. The votes come from three parties:
//   - the cross votes on its reading of the probe place. It votes admit where the vendor's disassembler reads a legal
//     instruction that names no register past the count. That instruction must be no control transfer, wait or memory
//     access.
//   - the gate reports what it blocked and why, and votes gray.
//   - the record reports what it contains of the identity. It votes admit where that is nothing or a block of this
//     type, and block where it is a refusal.
//
// The verdict is by consensus the other way: admitted where one party at least admits and none blocks
// (overseer_admission). An admission is a lesson of its own cause, as narrow as the conference: the question's probe
// encoding at every carrier (`lesson admitted * <low> <high>`). The gate puts an encoding that a lesson admits past
// rule 3 (orior_cubin_run --lessons). The run asks again a query that the gate blocked under the rule, because the part
// never saw it.

#include "../../../../../../includes/file_defs/qry/writer/qry_member.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

// the blob that holds the lessons, a lesson a line: `lesson <cause> <carrier or *> <the stand-in as written>`, and for
// an admission `lesson admitted * <low> <high>`, the probe encoding's two words in hex
#define OVERSEER_LESSONS "overseer.txt"

// the longest conference name, an account, the overseer's verdict, and a stand-in as written
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

    // the conference's verdict
    typedef struct
    {
        // 1 where the experts agreed a cause
        int agreed;
        // 1 where every cause that an expert reads is the question's own. That is its instruction, an address that it
        // reached or a register that it named, an exception among them. At least one expert also reads a cause. The
        // part then stays whole and the question is to blame, whichever clause the experts name, and a run may go on
        // past it
        int own;
        unsigned int cause;
        unsigned int side;
        unsigned int guidance;
        // the accounts received, and 1 where the overseer taught a lesson
        unsigned int accounts;
        int taught;
        // the verdict as the overseer's account states it
        char text[OVERSEER_RULING_MAX_LEN];
    } OverseerRuling;

    // the name of the conference of the question `identity`, into `name`, which has room for OVERSEER_NAME_MAX_LEN
    void overseer_conference_named(const char *identity, char *name);

    // `expert`'s account `text`, passed to the conference of the question `identity` in the run `run`: 1, or 0 where
    // the pass fails
    int overseer_account(QryShared *run, const char *identity, const char *expert, const char *text);

    // The conference of the question `identity`, convened. It reads every account passed to it, with the panic call's
    // where a panic is on the line. It puts the verdict by consensus into `ruling`, and passes it to the conference as
    // the overseer's account. `other_way_put` is 1 where the learner already put the pair the other way round. Returns
    // 1 where the conference received any account, and 0 where it received none
    int overseer_convened(QryShared *run, const char *identity, int other_way_put, OverseerRuling *ruling);

    // The conference of the question `identity` that the gate blocked for an unknown operation key, convened. It reads
    // every account passed to it, and counts its vote. It puts the verdict into `ruling`, and passes it to the
    // conference as the overseer's account. The verdict admits the question, with guidance OVERSEER_PUT, where one
    // party at least admits and none blocks, and keeps its hands off otherwise. On an admission, the overseer teaches a
    // lesson of the cause admitted for `encoding` at every carrier, with the probe's two words in hex `<low> <high>`.
    // Returns 1 where it admitted the question, and 0 where it did not or received no account
    int overseer_admission(QryShared *run, const char *identity, const char *encoding, OverseerRuling *ruling);

    // 1 where a lesson admits the probe encoding `low` and `high` past the gate's rule 3. The match is by its two
    // words, in the form that a lesson of the cause admitted writes them. 0 where none does
    int overseer_admits(unsigned long long low, unsigned long long high);

    // 1 where a lesson blocks the stand-in `located`, as written, off the part at the carrier `carrier`. The lesson's
    // cause goes into `why`, which has room for `room`. 0 where none does. A lesson of the cause admitted blocks
    // nothing
    int overseer_blocks(const char *located, const char *carrier, char *why, size_t room);

    // the lessons a run carried in, the latest blob OVERSEER_LESSONS of `run`, read: their count
    unsigned int overseer_lessons_read(QryShared *run);

    // every stored lesson, passed to `run` whole as OVERSEER_LESSONS. Returns 1 where the pass succeeds or no lesson
    // exists, and 0 where the pass fails
    int overseer_lessons_passed(QryShared *run);

    // a stand-in as written with its lines joined by a space, into `joined`, which has room for
    // OVERSEER_STANDING_MAX_LEN
    void overseer_located_joined(const char *located, char *joined);

    // the lessons let go
    void overseer_lessons_cleared(void);

#ifdef __cplusplus
}
#endif

#endif
