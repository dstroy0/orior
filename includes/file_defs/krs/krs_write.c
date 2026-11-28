// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// krs_write.c: a language's ruleset and its part's, written from the forms the part answered for.
//
//     orior_krs_write <language> <part> <path.khw> <layout> <mnemonics> <machine writer> <named ruleset>
//
// Every form in the part's file is there because the part answered for it (P8). No name matches an operation. The
// vendor's writer, `machine writer` above, runs through the interface and reads the part's machine file. It passes
// back each form that answered a relation: the relation, whether the answers read a word signed, and the form's text.
// In that text each operand in the relation's tuple is its place.
//
// The word web (word_web.h) says which word a form writes. Each leaf of a word's tree is one of the relation's words.
// The word writes the relation if its tree answers it on every case the ladder satisfies (ladder.h, precept_value.h).
//
// A word's form takes the answer first and then the leaves by place. The answer's place is the form's first
// parameter. The word a leaf reads is the parameter of that leaf.
//
// The parameters have the names the ruleset at `named ruleset` gives the same form. This writer writes no word that
// ruleset names no form of, or names with another count of parameters. One form that writes a word is the word's
// form. If more than one form writes a word, P6 ranks them by each form's cost. The cost is the least launch m and the
// spread s that the part's .ksc beside the machine file stores, `run answers <m> cost <low> <high> <s>`. The least
// form is the word's if the next least is past it by more than the greatest spread.
//
// Otherwise the part's ruleset keeps the form it gave the word before this run (Step 11). It also keeps it if a form
// that writes the word has no cost yet. A word no form of the run writes keeps the form the ruleset gave it before.
// This writer drops no form. A form the part refuses moves where it belongs. A run with no form for a word reports
// nothing of the type.
//
// A form that answers a frame relation (frame_relation.h) writes no word of the web. The table beside this writer,
// krs_write/frame_forms.tsv, maps its relation to the schema's form. The writer writes it as that form, with the
// parameters the ruleset at `named ruleset` gives it, one line a row of the table. The writers of one placed text
// are one. A row the run passes no form for keeps the line the ruleset gave its form before, as a word does.
//
// The writer writes the lane's structure `nop`. The structure is each form the schema gives as SCHEMA_FORM_STRUCTURE
// (schema_forms.h). These forms are the declarations, the notes, the start and the end of the lane and the kernel, and
// the unit. They are also the launch's and the shared memory's forms, the states and the loads. These name no
// operation and have no tree. The kernel the system accepted is the lane's frame on the part (Open 6.3).
//
// A position whose relation is true of nothing has every form and the empty text as survivors, and P6 keeps the
// cheapest, the empty text. Every other form the part has not answered for stays open, and the reader reads it so
// (P10).
//
// The writer writes two files beside the machine file. The first, <language>.krs, opens a ruleset of the language
// and names the part. The second, <part>.krs, contains the forms, and the reader reads it after the first
// (ruleset_flat.cu). No form answers a bank, a fixed register, the toolchain or the header, and neither file gives one.
//
// The writer runs in a query run, the .qry QRY names (qry_member.h). The forms the vendor's writer passes back are the
// blob FORMS_BLOB_NAME of the run (forms_blob.h). What the writer prints goes into the run as KRS_MACHINE_NAME.
#include "../../../src/cu/engine/rmc/schema_forms.h"
#include "../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "krs_write/fixed.h"
#include "krs_write/head.h"
#include "krs_write/krs_write_internal.h"
#include "verba_scribo/verba_scribo.h"

// The part's ruleset and the language's head. This writer builds each here, and each then goes to the run's writer
// as its file. Each holds the bytes it contains so far, and the room after a write did not fit.
static char s_forms_bytes[QRY_WRITER_RING_BYTES / 2ULL];
static size_t s_forms_at;
static char s_head_bytes[QRY_WRITER_RING_BYTES / 2ULL];
static size_t s_head_at;

// Write `text` after what the part's ruleset contains.
static void forms_text(const char *text)
{
    s_forms_at = verba_textus.put(
        &(VerbaTextusCfg){.out = s_forms_bytes, .cap = sizeof(s_forms_bytes), .at = s_forms_at, .text = text});
}

// Write `given`, the line the ruleset gave a form before, after what the part's ruleset contains. Each fixed
// register the line names by its number gets its placeholder. A line that does not fit stays as it was.
static void forms_given(const char *given)
{
    char line[KRS_LINE * 2u];
    {
        VerbaLinea buffer_line = mmgr_line_over(line, sizeof(line));
        mmgr_line_put(&buffer_line, given);
        (void)mmgr_line_finish(&buffer_line);
    }
    (void)krs_fixed_placed(line, sizeof(line));
    forms_text(line);
    forms_text("\n");
}

// Write `text` after what the language's head contains.
static void head_text(const char *text)
{
    s_head_at = verba_textus.put(
        &(VerbaTextusCfg){.out = s_head_bytes, .cap = sizeof(s_head_bytes), .at = s_head_at, .text = text});
}

// The schema's forms, each with its parameter count and its type.
#define KRS_SCHEMA_ROW(name_, text_, parameters_, type_) {text_, parameters_, type_},
static const struct
{
    const char *name;
    unsigned int parameters;
    unsigned int type;
} s_schema[] = {OPCODES(KRS_SCHEMA_ROW)};

// Write the lane's structure after what the part's ruleset contains. Each form the schema gives as structure is one
// `nop` line, with the parameters the ruleset at `named_ruleset` names. Return the count written. The writer reports
// a form that ruleset names with another count of parameters than the schema's, and leaves it out.
static unsigned int krs_structure_written(const char *named_ruleset)
{
    unsigned int written = 0u;
    for (unsigned int at = 0u; at < (sizeof(s_schema) / sizeof(s_schema[0])); at += 1u)
    {
        if (s_schema[at].type != SCHEMA_FORM_STRUCTURE)
        {
            continue;
        }
        char parameter[KRS_PARAMETERS][KRS_NAME];
        const unsigned int parameters = krs_parameters_named(named_ruleset, s_schema[at].name, parameter);
        if (parameters != s_schema[at].parameters)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  orior_krs_write: ");
                mmgr_line_put(output_line, named_ruleset);
                mmgr_line_put(output_line, " names ");
                mmgr_line_put(output_line, s_schema[at].name);
                mmgr_line_put(output_line, " with ");
                mmgr_line_decimal(output_line, parameters, 1u);
                mmgr_line_put(output_line, " parameters, where the schema gives it ");
                mmgr_line_decimal(output_line, s_schema[at].parameters, 1u);
                mmgr_line_put(output_line, ", and it is not written");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
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

int program_main(int count, char **word)
{
    if (count != 8)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(
                output_line,
                "orior_krs_write <language> <part> <path.khw> <layout> <mnemonics> <machine writer> <named ruleset>");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 2;
    }
    const char *const language = word[1];
    const char *const part = word[2];
    const char *const machine = word[3];
    const char *const named_ruleset = word[7];
    QryShared *const run = qry_member_run();
    if (run == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  orior_krs_write: no query writer runs: ");
            mmgr_line_put(output_line, QRY_ENVIRONMENT);
            mmgr_line_put(output_line, " names no .qry a writer has open");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    // Read the forms where the vendor's writer appends them: the latest record of their name. The writer took it
    // past every place the schedule reserved before the writer ran.
    const unsigned long long before = qry_member_rings_settled();
    if (!krs_vendor_run(word))
    {
        return 1;
    }
    qry_member_rings_settled();
    const unsigned char *forms = NULL;
    unsigned long long forms_size = 0ULL;
    unsigned long long place = 0ULL;
    if (!qry_member_blob(FORMS_BLOB_NAME, &forms, &forms_size, &place) || (place < before))
    {
        return 1;
    }
    krs_forms_read(forms, forms_size);
    char rulesets[1024];
    krs_folder_of(machine, rulesets, sizeof(rulesets));
    char path[1200];
    char ksc[1200];
    // Read the costs the part's .ksc stores, and the lines its ruleset gave before this run, before the ruleset
    // changes.
    {
        VerbaLinea buffer_line = mmgr_line_over(ksc, sizeof(ksc));
        mmgr_line_put(&buffer_line, rulesets);
        mmgr_line_put(&buffer_line, "/");
        mmgr_line_put(&buffer_line, part);
        mmgr_line_put(&buffer_line, ".ksc");
        (void)mmgr_line_finish(&buffer_line);
    }
    krs_costs_read(ksc);
    // Read the fixed registers the language's head names. A form's text names each by its placeholder.
    char language_krs[1200];
    {
        VerbaLinea buffer_line = mmgr_line_over(language_krs, sizeof(language_krs));
        mmgr_line_put(&buffer_line, rulesets);
        mmgr_line_put(&buffer_line, "/");
        mmgr_line_put(&buffer_line, language);
        mmgr_line_put(&buffer_line, ".krs");
        (void)mmgr_line_finish(&buffer_line);
    }
    krs_fixed_read(ksc, language_krs);
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, rulesets);
        mmgr_line_put(&buffer_line, "/");
        mmgr_line_put(&buffer_line, part);
        mmgr_line_put(&buffer_line, ".krs");
        (void)mmgr_line_finish(&buffer_line);
    }
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
            // No form the part answered for in this run writes the word. The ruleset keeps the form it gave the
            // word before. The writer drops no form. A form the part refuses moves where it belongs. A run with no
            // form for a word reports nothing of the type. A dropped form would lose a lesson the part gave.
            const char *const given_text = krs_given_of(each->name);
            if (given_text != NULL)
            {
                forms_given(given_text);
                given += 1u;
                {
                    VerbaLinea *const output_line = platform_line_out();
                    mmgr_line_put(output_line, "  orior_krs_write: no form answered for in this run writes ");
                    mmgr_line_put(output_line, each->name);
                    mmgr_line_put(output_line, "; the ruleset retains its form");
                    mmgr_line_ch(output_line, '\n');
                    platform_line_out_ended();
                }
            }
            continue;
        }
        // One form writes the word, or P6 separates the least of several. Otherwise the survivors are one cost, and
        // the ruleset keeps the form it gave the word (Step 11). If it gave none yet, the survivors are one answer of
        // the part, and the word takes the first form of the fewest operands.
        unsigned int ranked = (writers == 1u) ? 0u : krs_ranked(s_writer, writers);
        if (ranked == writers)
        {
            const char *const given_text = krs_given_of(each->name);
            if (given_text != NULL)
            {
                forms_given(given_text);
                given += 1u;
                {
                    VerbaLinea *const output_line = platform_line_out();
                    mmgr_line_put(output_line, "  orior_krs_write: ");
                    mmgr_line_decimal(output_line, writers, 1u);
                    mmgr_line_put(output_line, " forms write ");
                    mmgr_line_put(output_line, each->name);
                    mmgr_line_put(output_line, ", one cost or a cost not read (P6); the ruleset retains its form");
                    mmgr_line_ch(output_line, '\n');
                    platform_line_out_ended();
                }
                continue;
            }
            ranked = krs_fewest_operands(s_writer, writers);
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  orior_krs_write: ");
                mmgr_line_decimal(output_line, writers, 1u);
                mmgr_line_put(output_line, " forms write ");
                mmgr_line_put(output_line, each->name);
                mmgr_line_put(output_line, ", one cost or a cost not read (P6); the ruleset gave it none, and the one "
                                           "of the fewest operands is written");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
        const unsigned int first = s_writer[ranked];
        const unsigned int *const first_order = s_order[ranked];
        char parameter[KRS_PARAMETERS][KRS_NAME];
        const unsigned int parameters = krs_parameters_named(named_ruleset, each->name, parameter);
        if (parameters != ((unsigned int)each->reads + 1u))
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  orior_krs_write: ");
                mmgr_line_put(output_line, named_ruleset);
                mmgr_line_put(output_line, " names ");
                mmgr_line_put(output_line, each->name);
                mmgr_line_put(output_line, " with ");
                mmgr_line_decimal(output_line, parameters, 1u);
                mmgr_line_put(output_line, " parameters, where the word takes a destination and ");
                mmgr_line_decimal(output_line, (unsigned int)each->reads, 1u);
                mmgr_line_put(output_line, ", and it is not written");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            continue;
        }
        char text[KRS_LINE * 2u];
        if (!krs_text_named(&s_form[first], first_order, parameter, parameters, text, sizeof(text)) ||
            !krs_fixed_placed(text, sizeof(text)))
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  orior_krs_write: ");
                mmgr_line_put(output_line, each->name);
                mmgr_line_put(output_line, " is written by a form whose operands do not carry its words");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
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
    // Write the frame forms, each as the schema's form the table maps its relation to, one line a row of the table.
    // A row the run passes no form for keeps the line the ruleset gave its form before.
    if (krs_frame_read(NULL) == 0u)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line,
                      "  orior_krs_write: the table of frame forms could not be read, and no frame form is written");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    given += krs_frame_written(s_forms_bytes, sizeof(s_forms_bytes), &s_forms_at, s_frame_form, s_frame_forms,
                               named_ruleset, path, ksc);
    const unsigned int structure = krs_structure_written(named_ruleset);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  orior_krs_write: the lane's structure, ");
        mmgr_line_decimal(output_line, structure, 1u);
        mmgr_line_put(output_line, " forms, written nop");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (!verba_finis.ok(&(VerbaFinisCfg){.cap = sizeof(s_forms_bytes), .at = s_forms_at}) ||
        !qry_member_filed(path, s_forms_bytes, s_forms_at))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  orior_krs_write: ");
            mmgr_line_put(output_line, path);
            mmgr_line_put(output_line, " could not be written");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, rulesets);
        mmgr_line_put(&buffer_line, "/");
        mmgr_line_put(&buffer_line, language);
        mmgr_line_put(&buffer_line, ".krs");
        (void)mmgr_line_finish(&buffer_line);
    }
    head_text("krs 1\nruleset ");
    head_text(language);
    head_text("\n");
    // Write what turns the language's text into a program the part loads, and where the text's first lines come
    // from (krs.oracle.tsv). Each line is the part's answer in its .ksc (Open 6.3). The lines name the container
    // the vendor's writer makes, loaded and returned from, and the least head a kernel runs with. A line the part has
    // not answered for stays out, and the reader refuses the ruleset without it.
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = language, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "sass",
                   .ci = EMBED_FALSE))
    {
        char missing[128];
        krs_head_written(s_head_bytes, sizeof(s_head_bytes), &s_head_at, ksc, missing, sizeof(missing));
        if (missing[0] != '\0')
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  orior_krs_write: the part has not answered for its ");
            mmgr_line_put(output_line, missing);
            mmgr_line_put(output_line, "; the line is left out, and the reader refuses ");
            mmgr_line_put(output_line, language);
            mmgr_line_put(output_line, ".krs without it");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    head_text("part ");
    head_text(part);
    head_text("\n");
    // Write the part's register banks. Each bank is a template that the code generator fills with the register's
    // number. A general register is R{n} for every role the part uses in its file, and an even one for a wide's
    // pair. A predicate is P{n}, and an immediate is the number itself.
    //
    // The rows register last and predicate last in the part's .ksc bound the numbers. The bank line names, it does
    // not bound.
    if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = language, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "sass",
                   .ci = EMBED_FALSE))
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
        // Write the fixed registers the kernel the system accepted leaves at its position. Each is the part's answer
        // in its .ksc (Open 6.2). Zero is the register that reads 0 after every write to it. The lane's number, the
        // record and the index are where a register stores each at the position.
        //
        // A fixed register no row names keeps the line the ruleset gave it before this run, a hand line among them.
        // A line gives way only to the part's row for its register. The ten the banks' queries place (Open 6.1) stay
        // so until then. A register with neither stays out, and the reader refuses the ruleset without it.
        char unanswered[128];
        krs_fixed_written(s_head_bytes, sizeof(s_head_bytes), &s_head_at, ksc, path, unanswered, sizeof(unanswered));
        if (unanswered[0] != '\0')
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  orior_krs_write: the part has not answered for its fixed ");
            mmgr_line_put(output_line, unanswered);
            mmgr_line_put(output_line, ", and the ruleset gave no line; each is left out, and the reader refuses ");
            mmgr_line_put(output_line, language);
            mmgr_line_put(output_line, ".krs without it");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    if (!verba_finis.ok(&(VerbaFinisCfg){.cap = sizeof(s_head_bytes), .at = s_head_at}) ||
        !qry_member_filed(path, s_head_bytes, s_head_at))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  orior_krs_write: ");
            mmgr_line_put(output_line, path);
            mmgr_line_put(output_line, " could not be written");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  orior_krs_write: ");
        mmgr_line_decimal(output_line, given, 1u);
        mmgr_line_put(output_line, " form lines, the web's ");
        mmgr_line_decimal(output_line, KRS_WORDS, 1u);
        mmgr_line_put(output_line, " words' and the frame's, written from the ");
        mmgr_line_decimal(output_line, s_forms, 1u);
        mmgr_line_put(output_line, " forms and ");
        mmgr_line_decimal(output_line, s_frame_forms, 1u);
        mmgr_line_put(output_line, " frame forms the part answered for, in ");
        mmgr_line_put(output_line, rulesets);
        mmgr_line_put(output_line, "/");
        mmgr_line_put(output_line, part);
        mmgr_line_put(output_line, ".krs beside ");
        mmgr_line_put(output_line, language);
        mmgr_line_put(output_line, ".krs");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 0;
}
