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
//   airlock     a round reaches the carrier only once the cross has read it and handed a held list after it: a round
//               the cross did not read, or one with no held list after it, is carried by nothing, as is every round of
//               a channel with no airlock whose carrier is handed no held list. Given the real carrier and cross, a
//               code the vendor reads clean passes with nothing held, and an instruction it calls illegal is held by
//               its encoding, every reading through the run's streams (run_channel.h, measuring_stick_query.c)
//   tables      a conference held is a row of the run's conferences, each account it heard a row of its accounts and
//               its lesson a row of its lessons, each naming the conference by the number of the question's identity
//   verdicts    given the real carrier, every question its gate reads is a row of the run's table of verdicts keyed to
//               its ask, held where a rule broke, its reason a text of the table of texts, and an ask read again a
//               second row of it
//   check       a question the cross holds only for a register past the count its kernel allots is put again with the
//               count that clears it, read again and carried; one held for what no count clears keeps its count; each
//               hold and each count given is a line of the teacher's trace
//
//     overseer_check <overseer_carrier> [<cubin_run> <machine file> <layout> <cross> <nvdisasm>]
//
// The check runs in a run whose writer runs, QRY naming it (qry_run.sh): the teacher hands each round critical, and a
// critical hand returns once the writer has it on disk
#if !defined(_WIN32)
// setenv and unsetenv are POSIX, outside strict C11
#define _POSIX_C_SOURCE 200809L
#endif
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

// 1 where the blobs `name` of the run, joined, hold `text`
static int check_joined_holds(QryBuffer *run, const char *name, const char *text)
{
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

// 1 where the conference of `identity` holds `text`
static int check_conference_holds(QryBuffer *run, const char *identity, const char *text)
{
    char name[OVERSEER_NAME_LONGEST];
    overseer_conference_named(identity, name);
    return check_joined_holds(run, name, text);
}

// 1 where the latest blob `name` of the run holds `text`
static int check_latest_holds(QryBuffer *run, const char *name, const char *text)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (!qry_latest(run, name, &bytes, &size, NULL))
    {
        return 0;
    }
    const size_t length = strlen(text);
    for (unsigned long long at = 0ull; (at + length) <= size; at += 1ull)
    {
        if (memcmp(bytes + at, text, length) == 0)
        {
            return 1;
        }
    }
    return 0;
}

// The rows of `words` words each of the run's table `table` whose first column, past the who's two words, is
// `conference`: their count, the last of them copied into `last`, which holds `words`, and in `cause_reads` the count
// of those whose fourth column, an account's cause, is `cause`
static unsigned int check_rows_of(QryBuffer *run, const char *table, unsigned int words, unsigned long long conference,
                                  unsigned long long *last, unsigned long long cause, unsigned int *cause_reads)
{
    unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    unsigned int count = 0u;
    *cause_reads = 0u;
    if (!qry_joined(run, table, &bytes, &size))
    {
        return 0u;
    }
    unsigned long long row[16];
    const size_t row_size = (size_t)words * sizeof(unsigned long long);
    for (unsigned long long at = 0ull; (words <= 16u) && ((at + row_size) <= size); at += row_size)
    {
        memcpy(row, bytes + at, row_size);
        if (row[2] == conference)
        {
            memcpy(last, row, row_size);
            count += 1u;
            *cause_reads += ((words > 5u) && (row[5] == cause)) ? 1u : 0u;
        }
    }
    free(bytes);
    return count;
}

// 1 where the run's table of texts holds a row of the number `number`
static int check_text_held(QryBuffer *run, unsigned long long number)
{
    unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    int found = 0;
    if (!qry_joined(run, "texts", &bytes, &size))
    {
        return 0;
    }
    // a text's row: two words of no who, its number, its length, then its bytes filled out to a whole word
    unsigned long long opening[4];
    for (unsigned long long at = 0ull; !found && ((at + sizeof(opening)) <= size);)
    {
        memcpy(opening, bytes + at, sizeof(opening));
        found = (opening[2] == number);
        at += sizeof(opening) + (((opening[3] + 7ull) / 8ull) * 8ull);
    }
    free(bytes);
    return found;
}

// The rows of the run's table of verdicts of the ask whose code is the latest blob question.bin, in their order: the
// rule, held, carried and reason of each, into `rule`, `held`, `carried` and `reason`, each of room `room`. Their count
static unsigned int check_verdicts(QryBuffer *run, unsigned long long *rule, unsigned long long *held,
                                   unsigned long long *carried, unsigned long long *reason, unsigned int room)
{
    const unsigned char *code = NULL;
    unsigned long long code_size = 0ull;
    unsigned long long ask = 0ull;
    unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    unsigned int count = 0u;
    if (!qry_latest(run, "question.bin", &code, &code_size, &ask) || !qry_joined(run, "verdicts", &bytes, &size))
    {
        return 0u;
    }
    // a verdict's row: the who's two words, then its ask, rule, held, the byte it held at, its reason and carried
    unsigned long long row[8];
    for (unsigned long long at = 0ull; ((at + sizeof(row)) <= size) && (count < room); at += sizeof(row))
    {
        memcpy(row, bytes + at, sizeof(row));
        if (row[2] == ask)
        {
            rule[count] = row[3];
            held[count] = row[4];
            reason[count] = row[6];
            carried[count] = row[7];
            count += 1u;
        }
    }
    free(bytes);
    return count;
}

// 1 where the latest blob `name` of the run is there and holds no byte
static int check_latest_empty(QryBuffer *run, const char *name)
{
    const unsigned char *bytes = NULL;
    unsigned long long size = 1ull;
    return qry_latest(run, name, &bytes, &size, NULL) && (size == 0ull);
}

// a question's identity as R keeps it (kqr.h): the hash of its code, its registers, its shape and launches, its
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
    if ((count != 2) && (count != 7))
    {
        printf("overseer_check <overseer_carrier> [<cubin_run> <machine file> <layout> <cross> <nvdisasm>]\n");
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

    // tables: the conference a row of the run's conferences, each account it heard a row of its accounts and its
    // lesson a row of its lessons, each naming the conference by the number of the question's identity
    const unsigned long long consensus = qry_row_text(run, "check-consensus", strlen("check-consensus"));
    unsigned long long last[10];
    unsigned int reads = 0u;
    check_that((check_rows_of(run, "conferences", 10u, consensus, last, 0ull, &reads) == 1u) && (last[3] == 1ull) &&
                   (last[4] == OVERSEER_CAUSE_ADDRESS) && (last[5] == OVERSEER_SIDE_STAND_IN) &&
                   (last[6] == OVERSEER_OTHER_WAY) && (last[7] == 7ull) && (last[8] == 1ull) &&
                   check_text_held(run, last[9]),
               "tables: the conference is a row of the run's conferences, its ruling, its seven accounts, its lesson "
               "and its words");
    check_that((check_rows_of(run, "accounts", 6u, consensus, last, OVERSEER_CAUSE_ADDRESS, &reads) == 7u) &&
                   (reads == 2u),
               "tables: each of its seven accounts is a row of the run's accounts, two of them reading an address");
    check_that((check_rows_of(run, "lessons", 6u, consensus, last, 0ull, &reads) == 1u) &&
                   (last[3] == OVERSEER_CAUSE_ADDRESS) && check_text_held(run, last[5]),
               "tables: its lesson is a row of the run's lessons, its cause and the stand-in as written");

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

    // teacher: a question the carrier refuses, through an airlock whose cross reads the round, calls a conference of
    // the teacher's account and the carrier's
    const char *const refusing[] = {words[1], "refuse",  "--held", "held.txt", "--pop",    words[1],
                                    "pop",    "--cross", words[1], "cross",    "held.txt", NULL};
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
    check_that(check_latest_holds(run, "airlock.txt", " airlock_answers.txt "),
               "airlock: the round's list is handed with its answers renamed for the pop");
    char line[256];
    snprintf(line, sizeof(line), "teacher: %s carried: illegal, refused CUDA_ERROR_ILLEGAL_ADDRESS", identity);
    check_that(check_latest_holds(run, "trace", line),
               "trace: the teacher's line for the refused question is in the run");
    run_channel_close();

    // trace: every path traces where QRY_TRACE is unset; a plain name keeps only its path, a name after `-` keeps its
    // path out, 0 keeps every path out, and a panic's or a critical trace always reaches the run
    static const struct
    {
        const char *chosen;
        const char *path;
        unsigned int priority;
        int traced;
        const char *what;
    } s_traces[] = {
        {"", "teacher", QRY_ORDINARY, 1, "trace: unset, every path traces"},
        {"teacher", "teacher", QRY_ORDINARY, 1, "trace: a plain name traces its path"},
        {"teacher", "airlock", QRY_ORDINARY, 0, "trace: and keeps every other path out"},
        {"teacher,airlock", "airlock", QRY_ORDINARY, 1, "trace: a list of names traces each"},
        {"-airlock", "airlock", QRY_ORDINARY, 0, "trace: a name after - keeps its path out"},
        {"-airlock", "teacher", QRY_ORDINARY, 1, "trace: and every other path traces"},
        {"0", "teacher", QRY_ORDINARY, 0, "trace: 0 keeps every path out"},
        {"0", "teacher", QRY_PANIC, 1, "trace: but a panic's trace always reaches the run"},
        {"airlock", "teacher", QRY_CRITICAL, 1, "trace: and a critical one's"},
    };
    for (size_t at = 0u; at < (sizeof(s_traces) / sizeof(s_traces[0])); at += 1u)
    {
#if defined(_WIN32)
        _putenv_s(QRY_TRACE_ENVIRONMENT, s_traces[at].chosen);
#else
        setenv(QRY_TRACE_ENVIRONMENT, s_traces[at].chosen, 1);
#endif
        check_that(qry_traced(s_traces[at].path, s_traces[at].priority) == s_traces[at].traced, s_traces[at].what);
    }
#if defined(_WIN32)
    _putenv_s(QRY_TRACE_ENVIRONMENT, "");
#else
    unsetenv(QRY_TRACE_ENVIRONMENT);
#endif

    // airlock: a round the cross does not read, or reads and hands no held list after, is carried by nothing
    static const char *const s_crossings[] = {"unread", "silent"};
    for (unsigned int crossing = 0u; crossing < 2u; crossing += 1u)
    {
        const char *const unread[] = {words[1],   "refuse", "--held",  "held.txt", "--pop",
                                      words[1],   "pop",    "--cross", words[1],   s_crossings[crossing],
                                      "held.txt", NULL};
        check_that(run_channel_open(unread, 10000000ull) == 1, "airlock: the channel opens");
        s_code[1] = (unsigned char)(0xa0u + crossing);
        run_channel_ask(&s_question);
        check_identity(&s_question, identity);
        check_that((s_question.outcome == RUN_NO_CHANNEL) && (strstr(s_question.refused, "the airlock") != NULL) &&
                       !check_conference_holds(run, identity, "proctor:"),
                   (crossing == 0u) ? "airlock: a round the cross did not read never reaches the carrier"
                                    : "airlock: a round with no held list handed after it never reaches the carrier");
        run_channel_close();
    }

    // airlock: a live channel with none, and a carrier handed no held list, carries nothing; an airlock whose carrier
    // reads no held list does not open
    const char *const open_air[] = {words[1], "refuse", NULL};
    check_that(run_channel_open(open_air, 10000000ull) == 1, "airlock: a channel with none opens");
    s_code[1] = 0xb0u;
    run_channel_ask(&s_question);
    check_that((s_question.outcome == RUN_NO_CHANNEL) && (strstr(s_question.refused, "has none") != NULL),
               "airlock: and carries nothing");
    run_channel_close();
    const char *const unheld[] = {words[1],  "refuse", "--pop", words[1],   "pop",
                                  "--cross", words[1], "cross", "held.txt", NULL};
    check_that(run_channel_open(unheld, 10000000ull) == 0,
               "airlock: one whose carrier reads no held list does not open");

    // check: a question the cross holds only for a register past its count is put again with the count that clears
    // it and read again, and one held for what no count clears is kept held, each hold and each count a line of the
    // teacher's trace
    const char *const counted[] = {words[1], "refuse",  "--held", "held.txt",  "--pop",    words[1],
                                   "pop",    "--cross", words[1], "registers", "held.txt", NULL};
    check_that(run_channel_open(counted, 10000000ull) == 1, "check: the channel opens on a cross that holds registers");
    s_code[1] = 0xc0u;
    s_question.registers = 16u;
    check_identity(&s_question, identity);
    run_channel_ask(&s_question);
    check_that(s_question.registers == 41u,
               "check: a question held only for registers is put again with the count that clears it");
    check_that(check_latest_holds(run, "questions.txt", "question.bin 41 cases.txt") &&
                   (s_question.outcome == RUN_ILLEGAL),
               "check: read again, it passes and is carried with that count");
    snprintf(line, sizeof(line), "teacher: %s checked: held, the stand-in reads R40, past the 16 registers", identity);
    check_that(check_joined_holds(run, "trace", line), "check: its hold is a line of the teacher's trace");
    snprintf(line, sizeof(line), "teacher: %s checked: put again with 41 registers in place of 16", identity);
    check_that(check_joined_holds(run, "trace", line), "check: and so is the count it was given");
    run_channel_close();
    const char *const illegal[] = {words[1], "refuse",  "--held", "held.txt", "--pop",    words[1],
                                   "pop",    "--cross", words[1], "illegal",  "held.txt", NULL};
    check_that(run_channel_open(illegal, 10000000ull) == 1, "check: the channel opens on a cross that holds encodings");
    s_code[1] = 0xc1u;
    s_question.registers = 16u;
    check_identity(&s_question, identity);
    run_channel_ask(&s_question);
    snprintf(line, sizeof(line), "teacher: %s checked: kept held: no count of registers clears it", identity);
    check_that((s_question.registers == 16u) && check_joined_holds(run, "trace", line),
               "check: a question held for what no count clears keeps its count, and is traced kept held");
    run_channel_close();
    s_question.registers = 16u;

    // airlock, with the real tools where they are given: the carrier's dry pop and the vendor's cross read the round
    // through the run's streams, a clean code passes with nothing held, and an illegal instruction is held by its
    // encoding
    if (count == 7)
    {
        const char *const real[] = {words[1], "refuse", "--held",     "held.txt", "--pop",   words[2],
                                    "--dry",  "--full", words[3],     words[4],   "--cross", words[5],
                                    words[6], "SM86",   "airlock.md", "held.txt", NULL};
        check_that(run_channel_open(real, 60000000ull) == 1, "airlock: the channel opens on the real pop and cross");
        // EXIT, then a NOP: what the vendor reads clean
        static const unsigned char s_clean[32] = {0x4du, 0x79u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
                                                  0x00u, 0x00u, 0x80u, 0x03u, 0x00u, 0xeau, 0x0fu, 0x00u,
                                                  0x18u, 0x79u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
                                                  0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0xc0u, 0x0fu, 0x00u};
        memcpy(s_code, s_clean, sizeof(s_clean));
        run_channel_ask(&s_question);
        check_that(s_question.outcome == RUN_ILLEGAL, "airlock: a clean code passes the real airlock to the carrier");
        check_that(check_latest_holds(run, "airlock_cross.txt", "questions read: 1") &&
                       check_latest_empty(run, "held.txt"),
                   "airlock: the vendor read it and holds nothing");
        unsigned long long rule[4];
        unsigned long long held[4];
        unsigned long long carried[4];
        unsigned long long reason[4];
        check_that((check_verdicts(run, rule, held, carried, reason, 4u) == 1u) &&
                       (held[0] == ((rule[0] != 0ull) ? 1ull : 0ull)) && (carried[0] == 0ull) &&
                       check_text_held(run, reason[0]),
                   "verdicts: the pop's gate reads the ask once, a row keyed to it, held where a rule broke, the part "
                   "taken out, its reason a text of the table of texts");
        // MOV R40, c[0x0][0x28], then EXIT: a register the vendor reads past the 16 the question's kernel allots
        static const unsigned char s_past[32] = {0x02u, 0x7au, 0x28u, 0x00u, 0x00u, 0x0au, 0x00u, 0x00u,
                                                 0x00u, 0x0fu, 0x00u, 0x00u, 0x00u, 0xe4u, 0x0fu, 0x00u,
                                                 0x4du, 0x79u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u, 0x00u,
                                                 0x00u, 0x00u, 0x80u, 0x03u, 0x00u, 0xeau, 0x0fu, 0x00u};
        memcpy(s_code, s_past, sizeof(s_past));
        s_question.registers = 16u;
        check_identity(&s_question, identity);
        run_channel_ask(&s_question);
        snprintf(line, sizeof(line), "teacher: %s checked: held, the vendor reads R40 at byte ", identity);
        check_that((s_question.registers == 41u) && check_joined_holds(run, "trace", line),
                   "check: a register the vendor reads past the count is put again with the count that clears it");
        check_that((s_question.outcome == RUN_ILLEGAL) && check_latest_empty(run, "held.txt"),
                   "check: read again with that count, the vendor holds nothing and it is carried");
        check_that(check_verdicts(run, rule, held, carried, reason, 4u) == 2u,
                   "verdicts: the ask read again with the count that clears it is a second row of one ask");
        s_question.registers = 16u;
        // all ones after the EXIT: an instruction the vendor calls illegal
        memset(s_code + 16, 0xff, 16u);
        run_channel_ask(&s_question);
        check_that(check_latest_holds(run, "held.txt", "ffffffffffffffff ffffffffffffffff"),
                   "airlock: the instruction the vendor calls illegal is held by its encoding");
        run_channel_close();
    }

    // teacher, last: a carrier that outlasts its time calls a conference too. Its ending opens the panic line, and no
    // question is put in the run after it
    const char *const hanging[] = {words[1], "hang",    "--held", "held.txt", "--pop",    words[1],
                                   "pop",    "--cross", words[1], "cross",    "held.txt", NULL};
    check_that(run_channel_open(hanging, 1000000ull) == 1, "teacher: the channel opens on the hanging carrier");
    memset(s_code, 0, sizeof(s_code));
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
