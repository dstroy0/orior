// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// klq_identity.cu: the identities a language's questions carry, read off the measuring stick and written to Lstar.klq
//
//   klq_identity slice <nvcc listing> <manifest> <stick .cu> <folder> [<ours folder>]
//   klq_identity read <folder> <Lstar.klq> <ruleset>...
//   klq_identity known <nvcc listing> <manifest> <folder> <candidate>...
//   klq_identity permute <nvcc listing> <manifest> <folder>
//   klq_identity broken <nvcc listing> <manifest> <folder> <ours>... -- <ruleset>...
//   klq_identity stall <ours folder> <host answers> <ksc> <folder> <machine> -- <carrier>...
//   klq_identity frame <ours folder> <host answers> <ksc> <folder> -- <carrier>...
//   klq_identity fixed <ours folder> <host answers> <ksc> <folder> -- <carrier>...
//   klq_identity register <ours folder> <host answers> <ksc> <folder> -- <carrier>...
//   klq_identity curve <ours folder> <host answers> <ksc> <folder> <task>... -- <carrier>...
//   klq_identity queue <ours folder> <host answers> <ksc> <folder> <manifest> -- <carrier>...
//   klq_identity text_identity <ours folder> <host answers> <ksc> <folder> <Lstar.klq> <manifest> -- <carrier>...
//   klq_identity pair <ours folder> <host answers> <ksc> <folder> <Lstar.klq> <machine> -- <carrier>...
//
// Two of the stick's questions whose nvcc listings are the same but for one link a side are a slice, and the two
// links are the slice's identity: each an instruction with its registers read as their kind and its literals as
// written. The rest of the two listings is the same register for register, and the two links name the same registers:
// nothing but the two links sets the questions apart. An identity is carried by every slice whose links are its own,
// and each of them is information about it. Members the same are the same identity, and the carriers of one identity
// are told apart by address, the position of each member in the chain: question by question, then link by link within
// a question. Where two carriers contend, the one whose most basal member sits first comes first, and an identity's
// address is its first carrier's.
//
// A chain reads in its grammar and holds its whole intent, and a member's place in that grammar is its context: for
// each register its link names, the nearest link before it and the nearest after it in its own chain that name the same
// register, with the slot each names it in. No link's meaning is read, only the names links share. The carriers of one
// identity are gathered by context, each context the same members in one place of the grammar, addressed by its most
// basal carrier and given a verdict of its own.
//
// A question's chain as a whole is an identity too, and so is each window of it: each held at the first address it
// occurs at, each primitive a link that first occurs where it stands, and each chain a signature of the identities it
// is read as, its primitives and its operands, with a number its contents in their order give it.
//
// Every file a mode writes goes through the run's writer (qry_member.h): a current file of the tree, Lstar.klq, the
// .ksc, the .kqr or the .kdm, written by it in the file's place (qry_member_filed), and a table of the run's own a blob of the
// run named by the table's own name. slice hands the run slices.txt, each slice a line, chains.txt, the primitives and
// the chains, and host_questions.cpp, every question whose operands and result are integers that a slice holds, that is
// a task of register pressure, or that <ours folder> holds the engine's chain for, compiled for the host as C++ with
// the arity every kernel shares. The host computes each over every case, and its answers are the blob
// IDENTITY_HOST_ANSWERS of the run (qry_member.h), as is every <host answers> named above; read then gives
// each identity its verdict: the first case the two questions of one of its slices answer apart closes it as two
// operations, and an identity alike on every case asked stays open with its count of cases. A case the host's C would
// trap on or leave undefined is asked of nothing. Each identity is written to Lstar.klq after its keys, with the forms
// of the given rulesets whose texts write its links, and with the rule a link breaks where asking the part as written
// would end it
#include "identity/identity_internal.h"

int main(int count, char **words)
{
    if ((count >= 10) && (std::string(words[1]) == "pair") && (std::string(words[8]) == "--"))
    {
        std::vector<const char *> carrier(words + 9, words + count);
        carrier.push_back(NULL);
        // the frame asked, the fixed registers asked and the banks walked first in the same run: the container
        // returned and the least opening, then the slot, zero and what each register holds at it, then the last
        // register and the last predicate, each answer written to the .ksc and its record closed before the pairs open
        // theirs; an ask the record holds is not put again. The part's fields the walks read are in the .ksc already,
        // written by khw_write when the part was learned
        identity_frame(words[2], words[3], words[4], words[5], carrier.data());
        record_close();
        identity_fixed(words[2], words[3], words[4], words[5], carrier.data());
        record_close();
        identity_register(words[2], words[3], words[4], words[5], carrier.data());
        record_close();
        identity_predicate(words[2], words[3], words[4], words[5], carrier.data());
        record_close();
        identity_past(words[2], words[3], words[4], words[5], carrier.data());
        record_close();
        return identity_pair(words[2], words[3], words[4], words[6], words[7], carrier.data());
    }
    if ((count >= 10) && (std::string(words[1]) == "cost") &&
        ((std::string(words[8]) == "--") || ((count >= 11) && (std::string(words[9]) == "--"))))
    {
        // the machine file's forms placed by the vendor's writer, costed beside the arrangements where named
        const int with_forms = (std::string(words[8]) != "--");
        std::vector<const char *> carrier(words + (with_forms ? 10 : 9), words + count);
        carrier.push_back(NULL);
        return identity_cost(words[2], words[3], words[4], words[5], words[6], words[7], with_forms ? words[8] : NULL,
                             carrier.data());
    }
    if ((count >= 10) && (std::string(words[1]) == "text_identity") && (std::string(words[8]) == "--"))
    {
        std::vector<const char *> carrier(words + 9, words + count);
        carrier.push_back(NULL);
        return identity_text(words[2], words[3], words[4], words[6], words[7], carrier.data());
    }
    if ((count >= 9) && (std::string(words[1]) == "queue") && (std::string(words[7]) == "--"))
    {
        std::vector<const char *> carrier(words + 8, words + count);
        carrier.push_back(NULL);
        return identity_queue(words[2], words[3], words[4], words[5], words[6], carrier.data());
    }
    // the carrier's words follow a lone --
    if ((count >= 9) && (std::string(words[1]) == "stall") && (std::string(words[7]) == "--"))
    {
        std::vector<const char *> carrier(words + 8, words + count);
        carrier.push_back(NULL);
        return identity_stall(words[2], words[3], words[4], words[5], words[6], carrier.data());
    }
    if ((count >= 8) && (std::string(words[1]) == "frame") && (std::string(words[6]) == "--"))
    {
        std::vector<const char *> carrier(words + 7, words + count);
        carrier.push_back(NULL);
        return identity_frame(words[2], words[3], words[4], words[5], carrier.data());
    }
    if ((count >= 8) && (std::string(words[1]) == "fixed") && (std::string(words[6]) == "--"))
    {
        std::vector<const char *> carrier(words + 7, words + count);
        carrier.push_back(NULL);
        return identity_fixed(words[2], words[3], words[4], words[5], carrier.data());
    }
    if ((count >= 8) && (std::string(words[1]) == "register") && (std::string(words[6]) == "--"))
    {
        std::vector<const char *> carrier(words + 7, words + count);
        carrier.push_back(NULL);
        return identity_register(words[2], words[3], words[4], words[5], carrier.data());
    }
    if ((count >= 8) && (std::string(words[1]) == "past") && (std::string(words[6]) == "--"))
    {
        std::vector<const char *> carrier(words + 7, words + count);
        carrier.push_back(NULL);
        return identity_past(words[2], words[3], words[4], words[5], carrier.data());
    }
    // the tasks are given up to a lone --, the carrier's words after it
    if ((count >= 9) && (std::string(words[1]) == "curve"))
    {
        std::vector<std::string> tasks;
        int at = 6;
        for (; (at < count) && (std::string(words[at]) != "--"); at += 1)
        {
            tasks.push_back(words[at]);
        }
        if (!tasks.empty() && ((at + 1) < count))
        {
            std::vector<const char *> carrier(words + at + 1, words + count);
            carrier.push_back(NULL);
            return identity_curve(words[2], words[3], words[4], words[5], tasks, carrier.data());
        }
    }
    // the paths of ours are given up to a lone --, the rulesets after it
    if ((count >= 6) && (std::string(words[1]) == "broken"))
    {
        std::vector<std::string> ours_paths;
        int at = 5;
        for (; (at < count) && (std::string(words[at]) != "--"); at += 1)
        {
            ours_paths.push_back(words[at]);
        }
        at += (at < count) ? 1 : 0;
        return identity_broken(words[2], words[3], words[4], candidate_paths(ours_paths), count - at, words + at);
    }
    if ((count == 5) && (std::string(words[1]) == "permute"))
    {
        return identity_permute(words[2], words[3], words[4]);
    }
    if ((count >= 5) && (std::string(words[1]) == "known"))
    {
        return identity_known(words[2], words[3], words[4],
                              candidate_paths(std::vector<std::string>(words + 5, words + count)));
    }
    if (((count == 6) || (count == 7)) && (std::string(words[1]) == "slice"))
    {
        return identity_slice(words[2], words[3], words[4], words[5], (count == 7) ? words[6] : "");
    }
    if ((count >= 4) && (std::string(words[1]) == "read"))
    {
        return identity_read(words[2], words[3], count - 4, words + 4);
    }
    printf("klq_identity slice <nvcc listing> <manifest> <stick .cu> <folder> [<ours folder>]\n");
    printf("klq_identity read <folder> <Lstar.klq> <ruleset>...\n");
    printf("klq_identity known <nvcc listing> <manifest> <folder> <candidate>...\n");
    printf("klq_identity permute <nvcc listing> <manifest> <folder>\n");
    printf("klq_identity broken <nvcc listing> <manifest> <folder> <ours>... -- <ruleset>...\n");
    printf("klq_identity stall <ours folder> <host answers> <ksc> <folder> <machine> -- <carrier>...\n");
    printf("klq_identity register <ours folder> <host answers> <ksc> <folder> -- <carrier>...\n");
    printf("klq_identity queue <ours folder> <host answers> <ksc> <folder> <manifest> -- <carrier>...\n");
    printf("klq_identity pair <ours folder> <host answers> <ksc> <folder> <Lstar.klq> <machine> -- <carrier>...\n");
    printf("klq_identity text_identity <ours folder> <host answers> <ksc> <folder> <Lstar.klq> <manifest> -- "
           "<carrier>...\n");
    printf("klq_identity curve <ours folder> <host answers> <ksc> <folder> <task>... -- <carrier>...\n");
    printf("klq_identity cost <ours folder> <host answers> <ksc> <folder> <kdm> <machine> [<placed forms>] -- "
           "<carrier>...\n");
    return 1;
}
