// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// krs_write.c: a language's ruleset and its part's, written from the forms the part answered for
//
//     krs_write <language> <part> <path.khw> <layout> <mnemonics> <machine writer> <named ruleset>
//
// Every form in the part's file this writes is there because the part answered for it (P8), and no name is matched
// to an operation. The vendor's writer (`machine writer` above, run through the interface) reads the part's machine
// file and hands back each form that answered a relation: the relation, whether the answers read a word signed, and
// the form's text with each operand that stands in the relation's tuple written as its place. Which word a form writes
// is read off the word web (word_web.h): a word whose tree, each leaf reading one of the relation's words, answers the
// relation on every case of it the ladder holds (ladder.h, precept_value.h) is a word the form writes. A word's form
// takes the destination first and then the leaves by place: the answer's place is the form's first parameter, and the
// word a leaf reads is the parameter of that leaf.
//
// The parameters are named as the ruleset at `named ruleset` names the same form; a word that ruleset names no form
// of, or names with another count of parameters, is written by none here. A word one form writes is written with that
// form. A word more than one form writes is ranked by P6 off each form's cost, its least launch m and its spread s as
// the part's .ksc beside the machine file keeps them (`run answers <m> cost <low> <high> <s>`): the least form is the
// word's where the next least is past it by more than the greatest spread, and otherwise the part's ruleset keeps the
// form it gave the word before this run (Step 11), as it does where a form writing it has no cost read yet. A word no
// form of the run writes keeps the form the ruleset gave it before as well: a form is let go only where the part
// refuses it, and a run that hands none for a word says nothing of the kind.
//
// Two files are written beside the machine file: <language>.krs, which opens a ruleset of the language and names the
// part, and <part>.krs, which the reader reads after it (ruleset_flat.cu) and which holds the forms. A bank, a fixed
// register, the toolchain and the header are answered by no form, and neither file gives one.
//
// It runs in a query run, the .qry QRY names (qry_buffer.h): the forms the vendor's writer hands back are the blob
// KRS_FORMS_NAME of the run's buffer, and what it prints is handed to the run's buffer as KRS_MACHINE_NAME.
#include "krs_write/fixed.h"
#include "krs_write/head.h"
#include "krs_write/krs_write_internal.h"

int main(int count, char **word)
{
    if (count != 8)
    {
        printf("krs_write <language> <part> <path.khw> <layout> <mnemonics> <machine writer> <named ruleset>\n");
        return 2;
    }
    const char *const language = word[1];
    const char *const part = word[2];
    const char *const machine = word[3];
    const char *const named_ruleset = word[7];
    QryBuffer *const run = qry_run();
    if (run == NULL)
    {
        printf("  krs_write: no query writer runs: %s names no .qry a writer holds open\n", QRY_ENVIRONMENT);
        return 1;
    }
    // the forms the vendor's writer hands, told from any handed before them by their sequence
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    unsigned long long before = 0ull;
    if (!qry_latest(run, KRS_FORMS_NAME, &bytes, &size, &before))
    {
        before = 0ull;
    }
    unsigned long long sequence = 0ull;
    if (!krs_vendor_run(word))
    {
        return 1;
    }
    if (!qry_latest(run, KRS_FORMS_NAME, &bytes, &size, &sequence) || (sequence <= before))
    {
        printf("  krs_write: the vendor's writer handed the run's buffer no %s\n", KRS_FORMS_NAME);
        return 1;
    }
    krs_forms_read(bytes, size);
    char rulesets[1024];
    krs_folder_of(machine, rulesets, sizeof(rulesets));
    char path[1200];
    // the costs the part's .ksc keeps, and the lines its ruleset gave before this run, read before the ruleset is
    // written again
    snprintf(path, sizeof(path), "%s/%s.ksc", rulesets, part);
    krs_costs_read(path);
    snprintf(path, sizeof(path), "%s/%s.krs", rulesets, part);
    krs_given_read(path);
    QryText forms = {0};
    qry_text_printed(&forms, "krs 1\n");
    unsigned int given = 0u;
    for (unsigned int at = 0u; at < KRS_WORDS; at += 1u)
    {
        const Word *const each = &s_word_web[at];
        static unsigned int s_writer[KRS_FORMS];
        static unsigned int s_order[KRS_FORMS][LADDER_WORDS];
        unsigned int writers = 0u;
        for (unsigned int form = 0u; form < s_forms; form += 1u)
        {
            if (krs_word_written(each, &s_form[form], s_order[writers]))
            {
                s_writer[writers] = form;
                writers += 1u;
            }
        }
        if (writers == 0u)
        {
            // no form the part answered for in this run writes the word: the ruleset keeps the form it gave the word
            // before. A form is let go only where the part refuses it, and a run that hands none for a word says
            // nothing of the kind; dropping it would lose what was learned, to be learned again
            const char *const kept = krs_given_of(each->name);
            if (kept != NULL)
            {
                qry_text_printed(&forms, "%s\n", kept);
                given += 1u;
                printf("  krs_write: no form answered for in this run writes %s; the ruleset keeps its form\n",
                       each->name);
            }
            continue;
        }
        // one form writes the word, or P6 separates the least of several; otherwise the survivors are one cost and the
        // ruleset keeps the form it gave the word (Step 11)
        const unsigned int ranked = (writers == 1u) ? 0u : krs_ranked(s_writer, writers);
        if (ranked == writers)
        {
            const char *const kept = krs_given_of(each->name);
            if (kept != NULL)
            {
                qry_text_printed(&forms, "%s\n", kept);
                given += 1u;
            }
            printf("  krs_write: %u forms write %s, one cost or a cost not read (P6); %s\n", writers, each->name,
                   (kept != NULL) ? "the ruleset keeps its form" : "the ruleset gave it none, and none is written");
            continue;
        }
        const unsigned int first = s_writer[ranked];
        const unsigned int *const first_order = s_order[ranked];
        char parameter[KRS_PARAMETERS][KRS_NAME];
        const unsigned int parameters = krs_parameters_named(named_ruleset, each->name, parameter);
        if (parameters != ((unsigned int)each->reads + 1u))
        {
            printf("  krs_write: %s names %s with %u parameters, where the word takes a destination and %u, and it is "
                   "not written\n",
                   named_ruleset, each->name, parameters, (unsigned int)each->reads);
            continue;
        }
        char text[KRS_LINE * 2u];
        if (!krs_text_named(&s_form[first], first_order, parameter, parameters, text, sizeof(text)))
        {
            printf("  krs_write: %s is written by a form whose operands do not carry its words\n", each->name);
            continue;
        }
        qry_text_printed(&forms, "form %s", each->name);
        for (unsigned int place = 0u; place < parameters; place += 1u)
        {
            qry_text_printed(&forms, " %s", parameter[place]);
        }
        qry_text_printed(&forms, " = %s\\n\n", text);
        given += 1u;
    }
    if (qry_text_filed(qry_run(), path, &forms) == 0ull)
    {
        printf("  krs_write: %s could not be written\n", path);
        return 1;
    }
    snprintf(path, sizeof(path), "%s/%s.krs", rulesets, language);
    char ksc[1200];
    snprintf(ksc, sizeof(ksc), "%s/%s.ksc", rulesets, part);
    QryText head = {0};
    qry_text_printed(&head, "krs 1\nruleset %s\n", language);
    // What turns the language's text into a program the part loads, and where the text's opening lines come from
    // (krs.oracle.tsv): each the part's answer in its .ksc (The frame asked, Open 6.3), the container the vendor's
    // writer makes loaded and returned from, and the least opening a kernel runs with. A line the part has not
    // answered for is left out, and the reader refuses the ruleset without it
    if (strcmp(language, "sass") == 0)
    {
        char missing[128];
        krs_head_written(&head, ksc, missing, sizeof(missing));
        if (missing[0] != '\0')
        {
            printf("  krs_write: the part has not answered for its %s; the line is left out, and the reader refuses "
                   "%s.krs without it\n",
                   missing, language);
        }
    }
    qry_text_printed(&head, "part %s\n", part);
    // the part's register banks, each a naming template the code generator fills with the register's number: a general
    // register R{n} for every role the part holds in its file, an even one for a wide's pair, a predicate P{n}, and an
    // immediate the number itself. register last and predicate last in the part's .ksc bound the numbers; the bank
    // line names, it does not bound
    if (strcmp(language, "sass") == 0)
    {
        qry_text_printed(&head, "bank file = R{n}\n");
        qry_text_printed(&head, "bank sign = R{n}\n");
        qry_text_printed(&head, "bank out = R{n}\n");
        qry_text_printed(&head, "bank atom = R{n}\n");
        qry_text_printed(&head, "bank temporary = R{n}\n");
        qry_text_printed(&head, "bank wide = R{n}\n");
        qry_text_printed(&head, "bank predicate = P{n}\n");
        qry_text_printed(&head, "bank member = R{n}\n");
        qry_text_printed(&head, "bank immediate = {n}\n");
        // The fixed registers the kernel the system accepted leaves at its slot, each the part's answer in its .ksc
        // (The fixed registers asked, Open 6.2): zero, the register that reads 0 whatever is written to it, and the
        // lane's number, the record and the index where a register holds each at the slot. A fixed register no row
        // names keeps the line the ruleset gave it before this run, a hand line among them, since a line is let go
        // only where a row replaces it; the ten the banks' asks place (Open 6.1) stand so until then. A register with
        // neither is left out, and the reader refuses the ruleset without it
        char unanswered[128];
        krs_fixed_written(&head, ksc, path, unanswered, sizeof(unanswered));
        if (unanswered[0] != '\0')
        {
            printf("  krs_write: the part has not answered for its fixed %s, and the ruleset gave no line; each is "
                   "left out, and the reader refuses %s.krs without it\n",
                   unanswered, language);
        }
    }
    if (qry_text_filed(qry_run(), path, &head) == 0ull)
    {
        printf("  krs_write: %s could not be written\n", path);
        return 1;
    }
    printf("  krs_write: %u of the %u words of the web written from the %u forms the part answered for, in %s/%s.krs "
           "beside %s.krs\n",
           given, KRS_WORDS, s_forms, rulesets, part, language);
    return 0;
}
