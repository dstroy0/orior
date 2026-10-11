// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// overseer_check.c: the overseer held to overseer.h on accounts handed to a run, and the teacher's conference held on
// a carrier that reaches no part (overseer_carrier.c)
//
//   consensus   two experts that read one cause agree: the ruling names it, the side that brought it on, the
//               guidance and the lesson, and is handed to the conference as the overseer's account
//   dissent     two experts that read two causes rule nothing, and the learner goes on as it would
//   exception   an exception agrees with the cause another expert reads
//   context     a cause the stand-in does not do is the link's context's, and the stand-in goes on to its next link
//   roster      an expert the overseer names nowhere joins by handing an account, and its cause is counted
//   waits       the stand-in brought it on, and the pair is put the other way round already: the relation waits
//   silent      a conference where no expert reads a cause rules nothing, and a question no party called a
//               conference of is heard by no one
//   lessons     a lesson holds its stand-in as written at its carrier, or at every carrier where its encoding is the
//               cause, and holds nothing else; the lessons handed to the run are read back whole
//   teacher     a question the carrier refuses calls a conference holding the teacher's account and the carrier's;
//               one whose carrier outlasts its time calls one holding the teacher's
//
//     overseer_check <overseer_carrier>
//
// The check runs in a run whose writer runs, QRY naming it (qry_run.sh): the teacher hands each round critical, and a
// critical hand returns once the writer has it on disk
#include "../../../../../../../src/cu/transpiler/lstar/protocol/overseer/overseer.h"
#include "../../../../../../../src/cu/transpiler/lstar/protocol/teacher/run_channel.h"
#include "../../../../../../../src/cu/types/file_defs/qry/qry_buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

// the accounts of one conference, each `expert` and `text` in turn, ended by NULL, handed to `run`
static void check_accounts(QryBuffer *run, const char *identity, const char *const *accounts)
{
    for (unsigned int at = 0u; accounts[at] != NULL; at += 2u)
    {
        overseer_account(run, identity, accounts[at], accounts[at + 1u]);
    }
}

// 1 where the latest blob of the conference of `identity` holds `text`
static int check_conference_holds(QryBuffer *run, const char *identity, const char *text)
{
    char name[OVERSEER_NAME_LONGEST];
    overseer_conference_named(identity, name);
    unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    int holds = 0;
    if (qry_joined(run, name, &bytes, &size) && (size != 0ull))
    {
        char *const text_of = (char *)malloc((size_t)size + 1u);
        if (text_of != NULL)
        {
            memcpy(text_of, bytes, (size_t)size);
            text_of[size] = '\0';
            holds = strstr(text_of, text) != NULL;
            free(text_of);
        }
    }
    free(bytes);
    return holds;
}

// a question's identity as R keeps it (record.h): the hash of its code, its registers, its shape and launches, its
// count of cases and the hash of its cases, into `identity`, which holds 96
static void check_identity(const RunQuestion *question, char *identity)
{
    unsigned long long code = 0xcbf29ce484222325ull;
    for (unsigned long long at = 0ull; at < question->code_size; at += 1ull)
    {
        code = (code ^ question->code[at]) * 0x100000001b3ull;
    }
    unsigned long long cases = 0xcbf29ce484222325ull;
    const unsigned char *const words = (const unsigned char *)question->word;
    const size_t bytes = (size_t)question->cases * sizeof(question->word[0]);
    for (size_t at = 0u; at < bytes; at += 1u)
    {
        cases = (cases ^ words[at]) * 0x100000001b3ull;
    }
    snprintf(identity, 96u, "%016llx %u %u %u %u %u %016llx", code, question->registers, question->threads,
             question->blocks, question->launches, question->cases, cases);
}

static RunQuestion s_question;
static unsigned char s_code[32];

int main(int count, char **words)
{
    if (count != 2)
    {
        printf("overseer_check <overseer_carrier>\n");
        return 2;
    }
    QryBuffer *const run = qry_run();
    check_that(run != NULL, "the check runs in a run whose writer runs");
    if (run == NULL)
    {
        return 1;
    }
    overseer_lessons_cleared();
    OverseerRuling ruling;

    // consensus: the proctor and the listener read one address, and the stand-in addresses memory
    static const char *const s_consensus[] = {
        "teacher",
        "the question of 32 bytes of code, 16 registers, over 1 cases, the part refused it; the line is quiet",
        "proctor",
        "the launch was refused, the part left whole: CUDA_ERROR_ILLEGAL_ADDRESS (700); cause address",
        "listener",
        "the part stamped 1 Xid critical errors against the launch, Xid 31; cause address",
        "gate",
        "the code read safe, its register fields bound by 16; the held list loaded; the code holds no slot",
        "learner",
        "the pair a b, b put in place of a at 12:0040; carrier 12; stand-in addresses; standing LDG.E R0, [R2.64]",
        "coherence",
        "the pair a b; learning equality_coherence unknown_coherence",
        "counterexample",
        "the pair holds no case apart yet",
        NULL};
    check_accounts(run, "check-consensus", s_consensus);
    check_that(overseer_convened(run, "check-consensus", 0, &ruling) == 1, "consensus: the conference is heard");
    check_that(ruling.accounts == 7u, "consensus: every account is heard once");
    check_that(ruling.agreed && (ruling.cause == OVERSEER_CAUSE_ADDRESS), "consensus: the cause agreed is an address");
    check_that(ruling.side == OVERSEER_SIDE_STAND_IN, "consensus: the stand-in that addresses memory brought it on");
    check_that(ruling.guidance == OVERSEER_OTHER_WAY, "consensus: the pair is put the other way round");
    check_that(ruling.taught == 1, "consensus: a lesson is taught");
    check_that(strstr(ruling.said, "equality_coherence") != NULL,
               "consensus: the ruling says what the learner was after");
    check_that(check_conference_holds(run, "check-consensus", "overseer: 7 accounts heard"),
               "consensus: the ruling is handed to the conference as the overseer's account");

    // dissent: two causes, no ruling
    static const char *const s_dissent[] = {
        "proctor", "CUDA_ERROR_ILLEGAL_INSTRUCTION (715); cause instruction", "listener", "Xid 31; cause address",
        "learner", "the pair a c; carrier 12; standing IADD3 R0, R1, R2, RZ", NULL};
    check_accounts(run, "check-dissent", s_dissent);
    check_that(overseer_convened(run, "check-dissent", 0, &ruling) == 1, "dissent: the conference is heard");
    check_that(!ruling.agreed && (ruling.guidance == OVERSEER_HANDS_OFF) && !ruling.taught,
               "dissent: two causes rule nothing, the learner left to go on as it would");
    check_that(strstr(ruling.said, "no consensus: proctor reads instruction, listener reads address") != NULL,
               "dissent: the ruling keeps the dissent");

    // exception: an exception agrees with an instruction, and an encoding is held at every carrier
    static const char *const s_exception[] = {
        "proctor", "CUDA_ERROR_LAUNCH_FAILED (719); cause exception",         "listener", "Xid 13; cause instruction",
        "learner", "the pair d e; carrier 7; standing IMAD.X R1, R2, R3, R4", NULL};
    check_accounts(run, "check-exception", s_exception);
    check_that(overseer_convened(run, "check-exception", 0, &ruling) == 1, "exception: the conference is heard");
    check_that(ruling.agreed && (ruling.cause == OVERSEER_CAUSE_INSTRUCTION) && (ruling.side == OVERSEER_SIDE_STAND_IN),
               "exception: an exception agrees with an instruction, the stand-in's encoding");

    // context: an address the stand-in does not reach is the link's context's
    static const char *const s_context[] = {
        "proctor", "CUDA_ERROR_ILLEGAL_ADDRESS (700); cause address",        "listener", "Xid 31; cause address",
        "learner", "the pair f g; carrier 5; standing IADD3 R0, R1, R2, RZ", NULL};
    check_accounts(run, "check-context", s_context);
    check_that(overseer_convened(run, "check-context", 0, &ruling) == 1, "context: the conference is heard");
    check_that((ruling.side == OVERSEER_SIDE_CONTEXT) && (ruling.guidance == OVERSEER_ANOTHER_LINK),
               "context: the stand-in goes on to its next link");

    // roster and waits: an expert named nowhere joins, and a stand-in that controls, put both ways, waits
    static const char *const s_roster[] = {"survivors", "the gate's survivors held none of it; cause time",
                                           "proctor",   "CUDA_ERROR_LAUNCH_TIMEOUT (702); cause time",
                                           "learner",   "the pair h i; carrier 3; stand-in controls; standing BRA 0x40",
                                           NULL};
    check_accounts(run, "check-roster", s_roster);
    check_that(overseer_convened(run, "check-roster", 1, &ruling) == 1, "roster: the conference is heard");
    check_that(strstr(ruling.said, "survivors") != NULL, "roster: an expert named nowhere is heard");
    check_that(ruling.agreed && (ruling.cause == OVERSEER_CAUSE_TIME) && (ruling.side == OVERSEER_SIDE_STAND_IN),
               "roster: its cause is counted, and the stand-in that controls brought it on");
    check_that(ruling.guidance == OVERSEER_WAITS, "waits: put the other way round already, the relation waits");

    // silent: no cause, and no conference
    static const char *const s_silent[] = {"teacher", "the carrier was ended, out of time, code 0; the line is quiet",
                                           NULL};
    check_accounts(run, "check-silent", s_silent);
    check_that(overseer_convened(run, "check-silent", 0, &ruling) == 1, "silent: the conference is heard");
    check_that(!ruling.agreed && (ruling.guidance == OVERSEER_HANDS_OFF) &&
                   (strstr(ruling.said, "no expert reads a cause") != NULL),
               "silent: no cause read, nothing ruled");
    check_that(overseer_convened(run, "check-never", 0, &ruling) == 0,
               "silent: a question with no conference is heard by no one");

    // lessons: as narrow as their conferences, and read back whole from the run
    char why[160];
    check_that(overseer_holds("LDG.E R0, [R2.64]", "12", why, sizeof(why)),
               "lessons: an address is held at its carrier");
    check_that(!overseer_holds("LDG.E R0, [R2.64]", "13", why, sizeof(why)), "lessons: and at no other");
    check_that(overseer_holds("IMAD.X R1, R2, R3, R4", "99", why, sizeof(why)),
               "lessons: an encoding is held at every carrier");
    check_that(overseer_holds("IADD3 R0, R1, R2, RZ", "5", why, sizeof(why)) &&
                   !overseer_holds("IADD3 R0, R1, R2, RZ", "6", why, sizeof(why)) &&
                   !overseer_holds("IADD3 R0, R1, R2, RZ", "12", why, sizeof(why)),
               "lessons: a context is held at its carrier, and a dissent teaches nothing");
    check_that(!overseer_holds("IADD3 R0, R1, R2, R3", "5", why, sizeof(why)),
               "lessons: another writing of the form is not held");
    check_that(overseer_lessons_handed(run) == 1, "lessons: handed to the run");
    overseer_lessons_cleared();
    check_that(!overseer_holds("BRA 0x40", "3", why, sizeof(why)), "lessons: let go, none holds");
    check_that(overseer_lessons_read(run) == 4u, "lessons: the four taught read back from the run");
    check_that(overseer_holds("BRA 0x40", "3", why, sizeof(why)) &&
                   overseer_holds("IMAD.X R1, R2, R3, R4", "1", why, sizeof(why)),
               "lessons: read back, each holds as it did");

    // teacher: a question the carrier refuses calls a conference of the teacher's account and the carrier's
    const char *const refusing[] = {words[1], "refuse", NULL};
    check_that(run_channel_open(refusing, 10000000ull) == 1, "teacher: the channel opens on the refusing carrier");
    for (unsigned int at = 0u; at < sizeof(s_code); at += 1u)
    {
        s_code[at] = (unsigned char)(at * 7u);
    }
    s_question.code = s_code;
    s_question.code_size = sizeof(s_code);
    s_question.registers = 16u;
    s_question.slot = RUN_SLOT_NONE;
    s_question.cases = 1u;
    run_channel_ask(&s_question);
    check_that(s_question.outcome == RUN_ILLEGAL, "teacher: the refusal comes back illegal");
    char identity[96];
    check_identity(&s_question, identity);
    check_that(check_conference_holds(run, identity, "teacher: the question of 32 bytes of code"),
               "teacher: the refusal's conference holds the teacher's account");
    check_that(check_conference_holds(run, identity, "proctor: the launch of question.bin") &&
                   check_conference_holds(run, identity, "listener: the part stamped 1 Xid"),
               "teacher: and the carrier's accounts, as the carrier said them");
    overseer_account(
        run, identity, "learner",
        "the pair j k, k put in place of j at 9:0010; carrier 9; stand-in addresses; standing ST.E [R4.64], R0");
    check_that(overseer_convened(run, identity, 0, &ruling) == 1 && ruling.agreed &&
                   (ruling.cause == OVERSEER_CAUSE_ADDRESS) && (ruling.guidance == OVERSEER_OTHER_WAY),
               "teacher: the conference rules on the carrier's and the learner's accounts");
    run_channel_close();

    // teacher: a carrier that outlasts its time calls a conference too
    const char *const hanging[] = {words[1], "hang", NULL};
    check_that(run_channel_open(hanging, 1000000ull) == 1, "teacher: the channel opens on the hanging carrier");
    s_code[0] = 0xffu;
    run_channel_ask(&s_question);
    check_that(s_question.outcome == RUN_NOTHING, "teacher: the carrier ended out of its time answers nothing");
    check_identity(&s_question, identity);
    check_that(check_conference_holds(run, identity, "teacher: the question of 32 bytes of code"),
               "teacher: the ended carrier's question calls a conference");
    run_channel_close();

    printf("  overseer: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
