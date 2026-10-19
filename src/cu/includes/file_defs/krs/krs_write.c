// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
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
// form of the run writes keeps the form the ruleset gave it before as well: no form is dropped here, a form the part
// refuses is moved where it belongs, and a run that hands none for a word says nothing of the kind.
//
// A form answering a frame relation (frame_relation.h) writes no word of the web: it is written as the schema's form
// the table beside this writer, krs_write/frame_forms.tsv, maps its relation to, with that form's parameters as the
// ruleset at `named ruleset` names them, one line a row of the table, the writers of one placed text being one. A row
// the run hands no form for keeps the line the ruleset gave its form before, as a word does.
//
// The lane's structure, each form the schema gives as SCHEMA_FORM_STRUCTURE (schema_forms.h), is written `nop`: the
// declarations, the notes, the lane's and the kernel's opening and close, the launch's and the shared memory's, the
// states and the asks, and the unit name no operation and have no tree, and the kernel the system accepted is the
// lane's frame on the part (The frame asked, Open 6.3). A slot whose relation holds of nothing has every form and
// the empty text as survivors, and P6 keeps the cheapest, the empty text. Every other form the part has not answered
// for is left open, and the reader reads it so (P10).
//
// Two files are written beside the machine file: <language>.krs, which opens a ruleset of the language and names the
// part, and <part>.krs, which the reader reads after it (ruleset_flat.cu) and which holds the forms. A bank, a fixed
// register, the toolchain and the header are answered by no form, and neither file gives one.
//
// It runs in a query run, the .qry QRY names (qry_member.h): the forms the vendor's writer hands back are the blob
// FORMS_BLOB_NAME of the run (forms_blob.h), and what it prints is appended to the run as KRS_MACHINE_NAME.
#include "../../../engine/rmc/schema_forms.h"
#include "krs_write/fixed.h"
#include "krs_write/head.h"
#include "krs_write/krs_write_internal.h"
#include "verba_scribo/verba_scribo.h"

// the part's ruleset and the language's head, each put together here and then handed to the run's writer as its file,
// and the bytes each holds so far, the room once a write did not fit
static char s_forms_bytes[QRY_WRITER_RING_BYTES / 2ull];
static size_t s_forms_at;
static char s_head_bytes[QRY_WRITER_RING_BYTES / 2ull];
static size_t s_head_at;

// `text` written after what the part's ruleset holds
static void forms_text(const char *text)
{
    s_forms_at = verba_textus.put(
        &(VerbaTextusCfg){.out = s_forms_bytes, .cap = sizeof(s_forms_bytes), .at = s_forms_at, .text = text});
}

// `text` written after what the language's head holds
static void head_text(const char *text)
{
    s_head_at = verba_textus.put(
        &(VerbaTextusCfg){.out = s_head_bytes, .cap = sizeof(s_head_bytes), .at = s_head_at, .text = text});
}

// the schema's forms, each with its parameter count and its kind
#define KRS_SCHEMA_ROW(name_, text_, parameters_, kind_) {text_, parameters_, kind_},
static const struct
{
    const char *name;
    unsigned int parameters;
    unsigned int kind;
} s_schema[] = {OPCODES(KRS_SCHEMA_ROW)};

// the lane's structure written after what the part's ruleset holds, one `nop` line a form the schema gives as
// structure, its parameters as the ruleset at `named_ruleset` names them. The count written; a form that ruleset names
// with another count of parameters than the schema's is said and left out
static unsigned int krs_structure_written(const char *named_ruleset)
{
    unsigned int written = 0u;
    for (unsigned int at = 0u; at < (sizeof(s_schema) / sizeof(s_schema[0])); at += 1u)
    {
        if (s_schema[at].kind != SCHEMA_FORM_STRUCTURE)
        {
            continue;
        }
        char parameter[KRS_PARAMETERS][KRS_NAME];
        const unsigned int parameters = krs_parameters_named(named_ruleset, s_schema[at].name, parameter);
        if (parameters != s_schema[at].parameters)
        {
            printf("  krs_write: %s names %s with %u parameters, where the schema gives it %u, and it is not written\n",
                   named_ruleset, s_schema[at].name, parameters, s_schema[at].parameters);
            continue;
        }
        forms_text("nop ");
        forms_text(s_schema[at].name);
        for (unsigned int place = 0u; place < parameters; place += 1u)
        {
            forms_text(" ");
            forms_text(parameter[place]);
        }
        forms_text("\n");
        written += 1u;
    }
    return written;
}

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
    QryShared *const run = qry_member_run();
    if (run == NULL)
    {
        printf("  krs_write: no query writer runs: %s names no .qry a writer holds open\n", QRY_ENVIRONMENT);
        return 1;
    }
    // the forms the vendor's writer appends, read where it appends them: the latest record of their name, one the
    // writer took past every place the schedule held before the writer ran
    const unsigned long long before = qry_member_rings_settled();
    if (!krs_vendor_run(word))
    {
        return 1;
    }
    qry_member_rings_settled();
    const unsigned char *forms = NULL;
    unsigned long long forms_size = 0ull;
    unsigned long long place = 0ull;
    if (!qry_member_blob(FORMS_BLOB_NAME, &forms, &forms_size, &place) || (place < before))
    {
        return 1;
    }
    krs_forms_read(forms, forms_size);
    char rulesets[1024];
    krs_folder_of(machine, rulesets, sizeof(rulesets));
    char path[1200];
    char ksc[1200];
    // the costs the part's .ksc keeps, and the lines its ruleset gave before this run, read before the ruleset is
    // written again
    snprintf(ksc, sizeof(ksc), "%s/%s.ksc", rulesets, part);
    krs_costs_read(ksc);
    snprintf(path, sizeof(path), "%s/%s.krs", rulesets, part);
    krs_given_read(path);
    forms_text("krs 1\n");
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
            // before. No form is dropped: one the part refuses is moved where it belongs, and a run that hands none
            // for a word says nothing of the kind; dropping it would lose what was learned, to be learned again
            const char *const kept = krs_given_of(each->name);
            if (kept != NULL)
            {
                forms_text(kept);
                forms_text("\n");
                given += 1u;
                printf("  krs_write: no form answered for in this run writes %s; the ruleset keeps its form\n",
                       each->name);
            }
            continue;
        }
        // one form writes the word, or P6 separates the least of several; otherwise the survivors are one cost and the
        // ruleset keeps the form it gave the word (Step 11), or, where it gave none yet, the survivors being one answer
        // of the part's, takes the one of the fewest operands, the first seen among those
        unsigned int ranked = (writers == 1u) ? 0u : krs_ranked(s_writer, writers);
        if (ranked == writers)
        {
            const char *const kept = krs_given_of(each->name);
            if (kept != NULL)
            {
                forms_text(kept);
                forms_text("\n");
                given += 1u;
                printf("  krs_write: %u forms write %s, one cost or a cost not read (P6); the ruleset keeps its form\n",
                       writers, each->name);
                continue;
            }
            ranked = krs_fewest_operands(s_writer, writers);
            printf(
                "  krs_write: %u forms write %s, one cost or a cost not read (P6); the ruleset gave it none, and the "
                "one of the fewest operands is written\n",
                writers, each->name);
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
        forms_text("form ");
        forms_text(each->name);
        for (unsigned int place = 0u; place < parameters; place += 1u)
        {
            forms_text(" ");
            forms_text(parameter[place]);
        }
        forms_text(" = ");
        forms_text(text);
        forms_text("\\n\n");
        given += 1u;
    }
    // the frame forms, each as the schema's form the table maps its relation to, one line a row of the table; a row
    // the run hands no form for keeps the line the ruleset gave its form before
    if (krs_frame_read(NULL) == 0u)
    {
        printf("  krs_write: the table of frame forms could not be read, and no frame form is written\n");
    }
    given += krs_frame_written(s_forms_bytes, sizeof(s_forms_bytes), &s_forms_at, s_frame_form, s_frame_forms,
                               named_ruleset, path, ksc);
    const unsigned int structure = krs_structure_written(named_ruleset);
    printf("  krs_write: the lane's structure, %u forms, written nop\n", structure);
    if (!verba_finis.ok(&(VerbaFinisCfg){.cap = sizeof(s_forms_bytes), .at = s_forms_at}) ||
        !qry_member_filed(path, s_forms_bytes, s_forms_at))
    {
        printf("  krs_write: %s could not be written\n", path);
        return 1;
    }
    snprintf(path, sizeof(path), "%s/%s.krs", rulesets, language);
    head_text("krs 1\nruleset ");
    head_text(language);
    head_text("\n");
    // What turns the language's text into a program the part loads, and where the text's opening lines come from
    // (krs.oracle.tsv): each the part's answer in its .ksc (The frame asked, Open 6.3), the container the vendor's
    // writer makes loaded and returned from, and the least opening a kernel runs with. A line the part has not
    // answered for is left out, and the reader refuses the ruleset without it
    if (strcmp(language, "sass") == 0)
    {
        char missing[128];
        krs_head_written(s_head_bytes, sizeof(s_head_bytes), &s_head_at, ksc, missing, sizeof(missing));
        if (missing[0] != '\0')
        {
            printf("  krs_write: the part has not answered for its %s; the line is left out, and the reader refuses "
                   "%s.krs without it\n",
                   missing, language);
        }
    }
    head_text("part ");
    head_text(part);
    head_text("\n");
    // the part's register banks, each a naming template the code generator fills with the register's number: a general
    // register R{n} for every role the part holds in its file, an even one for a wide's pair, a predicate P{n}, and an
    // immediate the number itself. register last and predicate last in the part's .ksc bound the numbers; the bank
    // line names, it does not bound
    if (strcmp(language, "sass") == 0)
    {
        head_text("bank file = R{n}\n");
        head_text("bank sign = R{n}\n");
        head_text("bank out = R{n}\n");
        head_text("bank atom = R{n}\n");
        head_text("bank temporary = R{n}\n");
        head_text("bank wide = R{n}\n");
        head_text("bank predicate = P{n}\n");
        head_text("bank member = R{n}\n");
        head_text("bank immediate = {n}\n");
        // The fixed registers the kernel the system accepted leaves at its slot, each the part's answer in its .ksc
        // (The fixed registers asked, Open 6.2): zero, the register that reads 0 whatever is written to it, and the
        // lane's number, the record and the index where a register holds each at the slot. A fixed register no row
        // names keeps the line the ruleset gave it before this run, a hand line among them, since a line gives way
        // only to the part's row for its register; the ten the banks' asks place (Open 6.1) stand so until then. A
        // register with neither is left out, and the reader refuses the ruleset without it
        char unanswered[128];
        krs_fixed_written(s_head_bytes, sizeof(s_head_bytes), &s_head_at, ksc, path, unanswered, sizeof(unanswered));
        if (unanswered[0] != '\0')
        {
            printf("  krs_write: the part has not answered for its fixed %s, and the ruleset gave no line; each is "
                   "left out, and the reader refuses %s.krs without it\n",
                   unanswered, language);
        }
    }
    if (!verba_finis.ok(&(VerbaFinisCfg){.cap = sizeof(s_head_bytes), .at = s_head_at}) ||
        !qry_member_filed(path, s_head_bytes, s_head_at))
    {
        printf("  krs_write: %s could not be written\n", path);
        return 1;
    }
    printf("  krs_write: %u form lines, the web's %u words' and the frame's, written from the %u forms and %u frame "
           "forms the part answered for, in %s/%s.krs beside %s.krs\n",
           given, KRS_WORDS, s_forms, s_frame_forms, rulesets, part, language);
    return 0;
}
