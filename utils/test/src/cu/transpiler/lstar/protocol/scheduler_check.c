// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scheduler_check.c: a round checked against P13 on the run channel's dry carrier, which writes each question it is
// passed in the order it is passed them and answers none
//
//   order      the question that settles the max entries is put first, and two that settle as many in the order
//              passed
//   all        every question of a round is put, a round past SCHEDULER_ROUND_MAX to more than one process
//   gated      a question nothing carried answers nothing, and the round counts none answered
//   halted     a carrier that puts a panic on the line as it carries a round's first process ends that process, its
//              ending the first question's answer; every question after is blocked with the line's reason, and no
//              second process starts, the carrier having run once (overseer_carrier panic)
//   stopped    a panic on the line ruled stops the run, and every question put after it is refused with that reason; a
//              refusal the conference rules the instruction's own, the carrier ended clean, closes the line and
//              stops nothing
//
//     scheduler_check <folder> [<overseer_carrier>]
//
// The dry channel appends every question to the run (qry_member.h). The check runs in a run of its own, QRY naming
// it, and reads the round back from it. The halted round is checked where the check's carrier is given
#include "../../../../../../../includes/file_defs/qry/writer/qry_member.h"
#include "../../../../../../../src/platform/platform.h"
#include "../../../../../../../src/cu/transpiler/lstar/protocol/order/scheduler.h"
#include "../../../../../../../src/cu/transpiler/lstar/protocol/teacher/run_channel/run_channel_internal.h"

#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include <stdio.h>
#include <stdlib.h>

static unsigned int s_checks = 0u;
static unsigned int s_failed = 0u;

static void check_that(int passed, const char *what)
{
    s_checks += 1u;
    if (passed == 0)
    {
        s_failed += 1u;
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  FAILED: ");
            mmgr_line_put(output_line, what);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
}

#define CHECK_QUESTIONS 300u

// the records `name` of the run counted: each is one line
static unsigned int check_records_counted(const char *name)
{
    static unsigned char s_gathered[4096];
    const unsigned long long size = qry_member_gathered(name, s_gathered, sizeof(s_gathered));
    unsigned int counted = 0u;
    for (unsigned long long at = 0ull; (size != QRY_MEMBER_UNREAD) && (at < size); at += 1ull)
    {
        counted += (s_gathered[at] == '\n') ? 1u : 0u;
    }
    return counted;
}

int program_main(int count, char **words)
{
    if ((count != 2) && (count != 3))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "scheduler_check <folder> [<overseer_carrier>]");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 2;
    }
    // the run the check runs in
    (void)words;
    check_that(qry_member_run() != NULL, "the check runs in a run");
    // the airlock lets a round through a channel whose carrier reads a gated list alone (run_channel_checked): the dry
    // carrier reads none, and the list is named so that the round passes the airlock as a live one does
    const char *const carrier[] = {"dry", "--gated", "gated.txt", NULL};
    check_that(run_channel_open(carrier, 1000000ull) == 1, "the dry channel opens");
    RunQuestion *const pool =
        (RunQuestion *)platform_cell_cleared(CHECK_QUESTIONS, sizeof(RunQuestion), platform_place_g_program_owner, 0);
    if (pool == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  no room for the questions");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 2;
    }
    static unsigned char s_code[CHECK_QUESTIONS][16];
    RunQuestion *question[CHECK_QUESTIONS];
    unsigned int settles[CHECK_QUESTIONS];
    // each question's code its own number, so that the order written shows which was put when
    for (unsigned int at = 0u; at < CHECK_QUESTIONS; at += 1u)
    {
        s_code[at][0] = (unsigned char)(at & 0xffu);
        s_code[at][1] = (unsigned char)(at >> 8u);
        pool[at].code = s_code[at];
        pool[at].code_size = 16ull;
        pool[at].registers = 8u;
        pool[at].cases = 1u;
        question[at] = &pool[at];
        // every seventh question settles three entries, the rest one
        settles[at] = ((at % 7u) == 0u) ? 3u : 1u;
    }
    const unsigned int answered = scheduler_round(question, settles, CHECK_QUESTIONS);
    run_channel_close();
    check_that(answered == 0u, "a round nothing carried answers nothing");
    unsigned int gated = 0u;
    for (unsigned int at = 0u; at < CHECK_QUESTIONS; at += 1u)
    {
        gated += (pool[at].outcome == RUN_GATED) ? 1u : 0u;
    }
    check_that(gated == CHECK_QUESTIONS, "every question of the round is put, and reads gated");

    // the order the dry channel passed them in, read back off each question's code in the run
    unsigned int written[CHECK_QUESTIONS];
    unsigned int written_count = 0u;
    const unsigned char *list = NULL;
    unsigned long long list_size = 0ull;
    const int listed = qry_member_blob("questions.txt", &list, &list_size, NULL);
    check_that(listed, "the dry channel passed its list to the run's buffer");
    unsigned long long at_line = 0ull;
    while (listed && (at_line < list_size) && (written_count < CHECK_QUESTIONS))
    {
        const unsigned char *const line_end = (const unsigned char *)EMBED_CALL(
            memor.chr, MemoriaCfg, .src = list + at_line, .bytes = (size_t)(list_size - at_line), .val = '\n');
        const size_t line_length =
            (line_end != NULL) ? (size_t)(line_end - (list + at_line)) : (size_t)(list_size - at_line);
        char line[256];
        const size_t copied = (line_length < (sizeof(line) - 1u)) ? line_length : (sizeof(line) - 1u);
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = line, .src = list + at_line, .bytes = copied);
        line[copied] = '\0';
        at_line += (unsigned long long)line_length + 1ull;
        char code_name[256];
        const unsigned char *code = NULL;
        unsigned long long code_size = 0ull;
        if ((sscanf(line, "%255s", code_name) == 1) && qry_member_blob(code_name, &code, &code_size, NULL) &&
            (code_size >= 2ull))
        {
            written[written_count] = (unsigned int)code[0] | ((unsigned int)code[1] << 8u);
            written_count += 1u;
        }
    }
    check_that(written_count == CHECK_QUESTIONS, "the dry channel passed every question of the round");
    // within each process the three-settling questions come first, in the order passed, then the rest
    int ordered = 1;
    for (unsigned int first = 0u; first < written_count; first += SCHEDULER_ROUND_MAX)
    {
        const unsigned int last =
            ((first + SCHEDULER_ROUND_MAX) < written_count) ? (first + SCHEDULER_ROUND_MAX) : written_count;
        for (unsigned int at = first + 1u; at < last; at += 1u)
        {
            const unsigned int before = written[at - 1u];
            const unsigned int now = written[at];
            const unsigned int before_weight = ((before % 7u) == 0u) ? 3u : 1u;
            const unsigned int now_weight = ((now % 7u) == 0u) ? 3u : 1u;
            ordered = ordered && ((before_weight > now_weight) || ((before_weight == now_weight) && (before < now)));
        }
    }
    check_that(ordered, "the max settling first, and the order passed between equals");

    // the halt inside a round: the check's carrier puts a panic on the line as it carries the round's first process
    // and ends with it. The questions from the third on are the round, so that the conferences of the first two stay
    // for the stop below
    if (count == 3)
    {
        const char *const panicking[] = {words[2], "panic", "--gated", "gated.txt", NULL};
        check_that(run_channel_open(panicking, 1000000ull) == 1, "the panicking carrier's channel opens");
        const unsigned int starts_before = check_records_counted("carrier_started");
        const unsigned int carried = scheduler_round(question + 2, settles + 2, CHECK_QUESTIONS - 2u);
        run_channel_close();
        check_that((carried == 0u) && (run_channel_on_line() != 0ull),
                   "a carrier that panics on its first question answers nothing, and its panic is on the line");
        // the scheduler puts the max settling question first: whichever it was, the carrier's ending is its answer,
        // and every other question of the round is blocked by the line
        unsigned int ended = 0u;
        unsigned int halted = 0u;
        for (unsigned int at = 2u; at < CHECK_QUESTIONS; at += 1u)
        {
            const int no_channel = pool[at].outcome == RUN_NO_CHANNEL;
            ended += (no_channel && (EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = pool[at].refused,
                                                .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "the carrier exited",
                                                .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE) != NULL))
                         ? 1u
                         : 0u;
            halted += (no_channel && (EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = pool[at].refused,
                                                 .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "a panic is on the line",
                                                 .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE) != NULL))
                          ? 1u
                          : 0u;
        }
        check_that(ended == 1u, "the carrier's ending is the answer of the one question it was carrying");
        check_that(halted == CHECK_QUESTIONS - 3u,
                   "every other question of the round is blocked with the line's reason");
        check_that(check_records_counted("carrier_started") == starts_before + 1u,
                   "the carrier ran once: no second process starts after the panic");
        check_that(!run_channel_panic_ruled(), "the halt is the line's, not a ruling's: nothing has stopped the run");
    }

    // the stop: a panic on the line ruled, and every question of the round after it refused with the stop's reason, on
    // the dry channel as on a live one
    check_that(run_channel_open(carrier, 1000000ull) == 1, "the dry channel opens again");
    check_that(!run_channel_panic_ruled(), "the run is not stopped before a panic");
    // a clean refusal: the carrier ended clean, and every account the conference receives reads the cause as the
    // question's own instruction. The line closes and the run goes on
    qry_member_append("panic.txt", (const unsigned char *)"refused", 7ull, QRY_PANIC);
    check_that(qry_member_panicked() != 0ull, "a refusal's panic opens the line");
    KqrIdentity numbered;
    char identity[96];
    char conference[160];
    run_channel_identity(question[0], &numbered);
    kqr_identity_text(&numbered, identity, sizeof(identity));
    snprintf(conference, sizeof(conference), "%s_%s", QRY_CONFERENCE_CALL, identity);
    for (char *space = EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = conference, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                  .byte = ' ');
         space != NULL;
         space = EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = space, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = ' '))
    {
        *space = '_';
    }
    const char *const account = "carrier: the part refused the launch as an illegal instruction; cause instruction\n";
    qry_member_append(conference, (const unsigned char *)account,
                      EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = account, .cap = PLATFORM_TEXT_TO_TERMINATOR),
                      QRY_PANIC);
    s_channel.carrier_clean = 1;
    run_channel_line_ruled(question[0]);
    check_that(!run_channel_panic_ruled() && (qry_member_panicked() == 0ull),
               "a clean refusal ruled the instruction's own closes the line and stops nothing");
    // a panic no conference received an account of, on a question of its own: the line stays open and the run stops
    qry_member_append("panic.txt", (const unsigned char *)"faulted", 7ull, QRY_PANIC);
    check_that(qry_member_panicked() != 0ull, "a panic opens the line");
    s_channel.carrier_clean = 0;
    run_channel_line_ruled(question[1]);
    check_that(run_channel_panic_ruled(), "a ruling on a panic stops the run");
    const unsigned int after = scheduler_round(question, settles, CHECK_QUESTIONS);
    run_channel_close();
    unsigned int stopped = 0u;
    for (unsigned int at = 0u; at < CHECK_QUESTIONS; at += 1u)
    {
        stopped +=
            ((pool[at].outcome == RUN_NO_CHANNEL) &&
             (EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = pool[at].refused, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                         .other = "stopped", .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE) != NULL))
                ? 1u
                : 0u;
    }
    check_that((after == 0u) && (stopped == CHECK_QUESTIONS),
               "every question after the stop is refused with the stop's reason");
    platform_cell_released(pool, platform_place_g_program_owner);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "scheduler_check: ");
        mmgr_line_decimal(output_line, s_checks, 1u);
        mmgr_line_put(output_line, " checks, ");
        mmgr_line_decimal(output_line, s_failed, 1u);
        mmgr_line_put(output_line, " failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (s_failed == 0u) ? 0 : 1;
}
