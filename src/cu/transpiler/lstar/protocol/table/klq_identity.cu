// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// klq_identity.cu: the identities a language's questions carry, read off the measuring stick and written to Lstar.klq
//
//   `orior_klq_identity slice <nvcc listing> <manifest> <stick .cu> <folder> [<ours folder>]`
//   `orior_klq_identity read <folder> <Lstar.klq> <ruleset>...`
//   `orior_klq_identity known <nvcc listing> <manifest> <folder> <candidate>...`
//   `orior_klq_identity permute <nvcc listing> <manifest> <folder>`
//   `orior_klq_identity broken <nvcc listing> <manifest> <folder> <ours>... -- <ruleset>...`
//   `orior_klq_identity stall <ours folder> <host answers> <ksc> <folder> <machine> -- <carrier>...`
//   `orior_klq_identity frame <ours folder> <host answers> <ksc> <folder> -- <carrier>...`
//   `orior_klq_identity fixed <ours folder> <host answers> <ksc> <folder> -- <carrier>...`
//   `orior_klq_identity register <ours folder> <host answers> <ksc> <folder> -- <carrier>...`
//   `orior_klq_identity curve <ours folder> <host answers> <ksc> <folder> <task>... -- <carrier>...`
//   `orior_klq_identity queue <ours folder> <host answers> <ksc> <folder> <manifest> -- <carrier>...`
//   `orior_klq_identity text_identity <ours folder> <host answers> <ksc> <folder> <Lstar.klq> <manifest> --
//   <carrier>...` `orior_klq_identity pair <ours folder> <host answers> <ksc> <folder> <Lstar.klq> <machine> --
//   <carrier>...`
//
// Two of the stick's questions whose nvcc listings are the same but for one link a side are a slice. The two links are
// the slice's identity: each an instruction with its registers read as their type and its literals as written. The
// rest of the two listings is the same register for register, and the two links name the same registers. Nothing but
// the two links sets the questions apart.
//
// Every slice whose links are its own carries an identity, and each of them is information about it. Members the same
// are the same identity. The carriers of one identity differ by address, each member's position in the chain:
// question by question, then link by link within a question. Where two carriers contend, the one whose max basal
// member sits first comes first, and an identity's address is its first carrier's.
//
// A chain reads in its grammar and carries its whole intent, and a member's place in that grammar is its context. For
// each register its link names, the nearest link before it and the nearest after it in its own chain that name the
// same register. Each goes with the placeholder it names the register in. The read takes no link's sense, only the
// names links share. Context gathers the carriers of one identity, each context the same members in one place of the
// grammar. Its max basal carrier addresses each context, and each gets a verdict of its own.
//
// A question's chain as a whole is an identity too, and so is each window of it. Each goes in the record at the first
// address it occurs at. Each primitive is a link that first occurs where it is. Each chain is a signature of the
// identities the read finds in it, its primitives and its operands. Its contents in their order give it a number.
//
// Every file a mode writes goes through the run's writer (qry_member.h). The writer writes a current file of the tree,
// Lstar.klq, the .ksc, the .kqr or the .kdm, in the file's place (qry_member_filed). A table of the run's own is a blob
// of the run named by the table's own name. The slice mode passes the run slices.txt, each slice a line, and
// chains.txt, the primitives and the chains. It passes host_questions.cpp too, compiled for the host as C++ with the
// arity every kernel shares.
//
// It holds every question whose operands and result are integers that a slice represents. Then each task of register
// pressure, and each question <ours folder> contains the engine's chain for. The host computes each over every case,
// and its answers are the blob IDENTITY_HOST_ANSWERS of the run (qry_member.h). Every <host answers> named above is
// that blob too. Then read gives each identity its verdict. The first case the two questions of one of its slices
// answer apart closes it as two operations.
//
// An identity alike on every case queried stays open with its count of cases. Nothing queries a case the host's C
// would trap on or leave undefined. Each identity goes to Lstar.klq after its keys, with the forms of the given
// rulesets whose texts write its links. With it goes the rule a link breaks where a query of the part as written would
// end it
#include "../../../../../../includes/platform/platform.h"
#include "identity/identity_internal.h"

// The pool the identity asks for: the slice's windows, one cell of about 300 MB on the temporary tier, and every pair's
// question beside them
extern "C" const unsigned long long g_program_pool_bytes = 1ULL << 30u;

// The carrier's words: `words` from `first` on. The words end at a NULL, since `words` is the process's argv
static const char *const *carrier_words(char **words, int first)
{
    return (const char *const *)(words + first);
}

// 1 where the word is the terminated `text`
static int word_is(const char *word, const char *text)
{
    return EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = word, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = text,
                      .ci = EMBED_FALSE);
}

// the place of the first lone -- in `words` from `first`, or `count` where none follows
static int marker_at(char **words, int first, int count)
{
    int at = first;
    while ((at < count) && !word_is(words[at], "--"))
    {
        at += 1;
    }
    return at;
}

// The words from `first` to `last` as paths, in a cell of the temporary tier the caller gives back. NULL where the
// pool cannot meet it
static IdentityPath *words_as_paths(char **words, int first, int last)
{
    // `last` is at least `first`, and the words of a command are far below 2^31
    const unsigned long long count = (unsigned long long)(last - first);
    IdentityPath *const paths =
        (IdentityPath *)platform_cell_taken(((count != 0ULL) ? count : 1ULL) * sizeof(IdentityPath),
                                            platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    for (unsigned long long at = 0ULL; (paths != NULL) && (at < count); at += 1ULL)
    {
        (void)identity_name_set(paths[at].text, sizeof(paths[at].text), words[first + (int)at],
                                PLATFORM_TEXT_TO_TERMINATOR);
    }
    return paths;
}

// The files the words from `first` to `last` name, into `paths`, which the caller ends: 1, or 0 where the pool cannot
// meet a cell
static int candidates_given(char **words, int first, int last, PlatformArray *paths)
{
    platform_array_begun(paths, sizeof(IdentityPath), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    IdentityPath *const given = words_as_paths(words, first, last);
    if (given == NULL)
    {
        return 0;
    }
    // `last` is at least `first`
    candidate_paths(given, (unsigned long long)(last - first), paths);
    platform_cell_released(given, platform_place_g_program_owner);
    return 1;
}

// the words from `first` to `last` as task names, into a cell of the temporary tier the caller gives back, or NULL
static IdentityName *words_as_names(char **words, int first, int last)
{
    // `last` is at least `first`
    const unsigned long long count = (unsigned long long)(last - first);
    IdentityName *const names =
        (IdentityName *)platform_cell_taken(((count != 0ULL) ? count : 1ULL) * sizeof(IdentityName),
                                            platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    for (unsigned long long at = 0ULL; (names != NULL) && (at < count); at += 1ULL)
    {
        (void)IDENTITY_SET_TEXT(names[at], words[first + (int)at]);
    }
    return names;
}

int program_main(int count, char **words)
{
    if ((count >= 10) && word_is(words[1], "pair") && word_is(words[8], "--"))
    {
        const char *const *const given = carrier_words(words, 9);
        const char *const folder = words[5];
        // The frame queried, the fixed registers queried and the banks walked first in the same run. The container
        // returned and the least opening, then the probe place, zero and what each register contains at it. Then the
        // last register and the last predicate. Each answer goes to the .ksc and its record closes before the pairs
        // open theirs. The run never puts again a query the record contains. The part's fields the walks read are in
        // the .ksc already, written by khw_write when the run learned the part
        identity_frame(words[2], words[3], words[4], folder, given);
        record_close();
        identity_fixed(words[2], words[3], words[4], folder, given);
        record_close();
        identity_register(words[2], words[3], words[4], folder, given);
        record_close();
        identity_predicate(words[2], words[3], words[4], folder, given);
        record_close();
        identity_past(words[2], words[3], words[4], folder, given);
        record_close();
        return identity_pair(words[2], words[3], words[4], words[6], words[7], given);
    }
    if ((count >= 10) && word_is(words[1], "cost") &&
        (word_is(words[8], "--") || ((count >= 11) && word_is(words[9], "--"))))
    {
        // the machine file's forms placed by the vendor's writer, costed beside the arrangements where named
        const int with_forms = !word_is(words[8], "--");
        return identity_cost(words[2], words[3], words[4], words[5], words[6], words[7], with_forms ? words[8] : NULL,
                             carrier_words(words, with_forms ? 10 : 9));
    }
    if ((count >= 10) && word_is(words[1], "text_identity") && word_is(words[8], "--"))
    {
        return identity_text(words[2], words[3], words[4], words[6], words[7], carrier_words(words, 9));
    }
    if ((count >= 9) && word_is(words[1], "queue") && word_is(words[7], "--"))
    {
        return identity_queue(words[2], words[3], words[4], words[5], words[6], carrier_words(words, 8));
    }
    // the carrier's words follow a lone --
    if ((count >= 9) && word_is(words[1], "stall") && word_is(words[7], "--"))
    {
        return identity_stall(words[2], words[3], words[4], words[5], words[6], carrier_words(words, 8));
    }
    if ((count >= 8) && word_is(words[1], "frame") && word_is(words[6], "--"))
    {
        return identity_frame(words[2], words[3], words[4], words[5], carrier_words(words, 7));
    }
    if ((count >= 8) && word_is(words[1], "fixed") && word_is(words[6], "--"))
    {
        return identity_fixed(words[2], words[3], words[4], words[5], carrier_words(words, 7));
    }
    if ((count >= 8) && word_is(words[1], "register") && word_is(words[6], "--"))
    {
        return identity_register(words[2], words[3], words[4], words[5], carrier_words(words, 7));
    }
    if ((count >= 8) && word_is(words[1], "past") && word_is(words[6], "--"))
    {
        return identity_past(words[2], words[3], words[4], words[5], carrier_words(words, 7));
    }
    // the tasks go up to a lone --, the carrier's words after it
    if ((count >= 9) && word_is(words[1], "curve"))
    {
        const int at = marker_at(words, 6, count);
        IdentityName *const tasks = words_as_names(words, 6, at);
        if ((tasks != NULL) && (at > 6) && ((at + 1) < count))
        {
            // the tasks lie between the mode's words and the --
            const int answer = identity_curve(words[2], words[3], words[4], words[5], tasks,
                                              (unsigned long long)(at - 6), carrier_words(words, at + 1));
            platform_cell_released(tasks, platform_place_g_program_owner);
            return answer;
        }
        platform_cell_released(tasks, platform_place_g_program_owner);
    }
    // the paths of ours go up to a lone --, the rulesets after it
    if ((count >= 6) && word_is(words[1], "broken"))
    {
        const int marker = marker_at(words, 5, count);
        const int at = marker + ((marker < count) ? 1 : 0);
        PlatformArray paths;
        const int read = candidates_given(words, 5, marker, &paths);
        const int answer = read ? identity_broken(words[2], words[3], words[4], (const IdentityPath *)paths.cell,
                                                  paths.count, count - at, words + at)
                                : 1;
        platform_array_ended(&paths);
        return answer;
    }
    if ((count == 5) && word_is(words[1], "permute"))
    {
        return identity_permute(words[2], words[3], words[4]);
    }
    if ((count >= 5) && word_is(words[1], "known"))
    {
        PlatformArray paths;
        const int read = candidates_given(words, 5, count, &paths);
        const int answer =
            read ? identity_known(words[2], words[3], words[4], (const IdentityPath *)paths.cell, paths.count) : 1;
        platform_array_ended(&paths);
        return answer;
    }
    if (((count == 6) || (count == 7)) && word_is(words[1], "slice"))
    {
        return identity_slice(words[2], words[3], words[4], words[5], (count == 7) ? words[6] : "");
    }
    if ((count >= 4) && word_is(words[1], "read"))
    {
        return identity_read(words[2], words[3], count - 4, words + 4);
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line,
                      "orior_klq_identity slice <nvcc listing> <manifest> <stick .cu> <folder> [<ours folder>]");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity read <folder> <Lstar.klq> <ruleset>...");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity known <nvcc listing> <manifest> <folder> <candidate>...");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity permute <nvcc listing> <manifest> <folder>");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line,
                      "orior_klq_identity broken <nvcc listing> <manifest> <folder> <ours>... -- <ruleset>...");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line,
                      "orior_klq_identity stall <ours folder> <host answers> <ksc> <folder> <machine> -- <carrier>...");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line,
                      "orior_klq_identity register <ours folder> <host answers> <ksc> <folder> -- <carrier>...");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(
            output_line,
            "orior_klq_identity queue <ours folder> <host answers> <ksc> <folder> <manifest> -- <carrier>...");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity pair <ours folder> <host answers> <ksc> <folder> <Lstar.klq> "
                                   "<machine> -- <carrier>...");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity text_identity <ours folder> <host answers> <ksc> <folder> "
                                   "<Lstar.klq> <manifest> -- <carrier>...");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line,
                      "orior_klq_identity curve <ours folder> <host answers> <ksc> <folder> <task>... -- <carrier>...");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "orior_klq_identity cost <ours folder> <host answers> <ksc> <folder> <kdm> "
                                   "<machine> [<placed forms>] -- <carrier>...");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 1;
}
