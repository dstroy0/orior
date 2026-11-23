// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// gsm_write.c: a part's relations written in gnascor's assembly, each as the cases the part answered it on
//
//     orior_gsm_write <part> <path.khw> <layout> <mnemonics> <machine writer>
//
// The face is relational (src/lng/transpiler_gsm_parser.md). A case is the operands, and then the answer for which the
// relation is valid, 1,1->2.
//
// A form enters the machine file of the part only where the part answered one relation on each case of it
// (khw_write.c). Those cases are the cases of the ladder (ladder.h), which the host computes.
//
// The writer of the vendor (`machine writer` above, run through the interface) reads the machine file. It passes back
// each form that answered a relation, and whether the answers read a word as signed.
//
// The tool writes each answered relation once, as each two-word case of it for which the ladder is valid. It reads the
// answer as signed where the answer of the part was. It writes <part>.gsm beside the machine file, a case a line in the
// order of the ladder, and each word in decimal.
//
// It runs in a query run, the .qry that QRY names (qry_member.h). The forms that the writer of the vendor passes back
// are the blob GSM_FORMS_NAME of the run. What it prints goes to the run as GSM_MACHINE_NAME.
#include "../../../src/cu/transpiler/lstar/protocol/counterexample/ladder.h"
#include "../../../src/platform/platform.h"
#include "../../parsers/line_read.h"
#include "../qry/writer/qry_member.h"
#include "../vendor_writer_run.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "verba_scribo/verba_scribo.h"

// the order of the texts `left` and `right`, each read to its terminator or to `cap`: below 0 where `left`
// comes first, 0 where they are the same, above 0 where `right` does
static inline int gsm_write_text_order(const char *left, const char *right, size_t cap)
{
    const size_t left_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = left, .cap = cap);
    const size_t right_length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = right, .cap = cap);
    const size_t shorter = (left_length < right_length) ? left_length : right_length;
    const size_t reach = (shorter < cap) ? (shorter + 1u) : cap;
    const size_t differs =
        EMBED_CALL(cellul.diff, CatenaFinitaCfg, .src = left, .cap = reach, .other = right, .ci = EMBED_FALSE);
    if (differs >= reach)
    {
        return 0;
    }
    // a byte of a text read as unsigned, and its order as a signed count
    return (int)(unsigned char)left[differs] - (int)(unsigned char)right[differs];
}

// the name for the relation of a form in the pass back, as khw_write writes it
#define GSM_READS_LADDER "ladder."

// the blobs of the run the vendor's writer passes: the forms it passes back, and what it printed
#define GSM_FORMS_NAME "gsm_forms.txt"
#define GSM_MACHINE_NAME "machine.txt"

#define LADDER_TEXT(name_, text_, words_, measured_) text_,
static const char *const s_anchor_text[] = {LADDER_ANCHORS(LADDER_TEXT)};
#undef LADDER_TEXT

// each relation the part answered, read unsigned and read signed
static int s_answered[LADDER_ANCHOR_COUNT][2];

// The writer of the vendor, run through the interface in its krs mode (vendor_writer_run.h). It gets the part, the
// machine file, the container layout, the table of the vendor and the name for the forms.
// Its output goes to the run as GSM_MACHINE_NAME. The result is 1 where it ended clean
static int gsm_vendor_run(char *const *word)
{
    // the interface's command is a list of words it does not write to; the casts only meet its declared type
    char *const command[] = {word[5], word[1], word[2], word[3], word[4], (char *)GSM_FORMS_NAME, (char *)"krs", NULL};
    return vendor_writer_run(command, GSM_MACHINE_NAME, "orior_gsm_write", NULL);
}

// the relations that the forms answered, into s_answered. The forms are the `size` bytes at `bytes` that the writer of
// the vendor passed back. The code skips a line whose relation is none that the ladder satisfies. The result is the
// count of read forms
static unsigned int gsm_relations_read(const unsigned char *bytes, unsigned long long size)
{
    char line[1024];
    unsigned int read = 0u;
    unsigned long long next = 0ULL;
    while (next < size)
    {
        const unsigned char *const end = (const unsigned char *)EMBED_CALL(memor.chr, MemoriaCfg, .src = bytes + next,
                                                                           .bytes = (size_t)(size - next), .val = '\n');
        const size_t length = (end != NULL) ? (size_t)(end - (bytes + next)) : (size_t)(size - next);
        {
            VerbaLinea buffer_line = mmgr_line_over(line, sizeof(line));
            mmgr_line_put_max(&buffer_line, (const char *)(bytes + next), (size_t)((int)length));
            (void)mmgr_line_finish(&buffer_line);
        }
        next += (unsigned long long)length + 1ULL;
        char relation[256];
        unsigned long long signed_read = 0ULL;
        const char *at = line;
        if (!line_word(&at, relation, sizeof(relation)) || !line_number(&at, 0, &signed_read))
        {
            continue;
        }
        const size_t prefix =
            EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = GSM_READS_LADDER, .cap = PLATFORM_TEXT_TO_TERMINATOR);
        const char *const name =
            (gsm_write_text_order(relation, GSM_READS_LADDER, (size_t)(prefix)) == 0) ? (relation + prefix) : relation;
        for (unsigned int anchor = 0u; anchor < (unsigned int)LADDER_ANCHOR_COUNT; anchor += 1u)
        {
            if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_anchor_text[anchor], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                           .other = name, .ci = EMBED_FALSE))
            {
                s_answered[anchor][(signed_read != 0) ? 1 : 0] = 1;
                read += 1u;
            }
        }
    }
    return read;
}

// the folder `path` lies in, into `folder`, which has room for `room`: "." where it names none
static void gsm_folder_of(const char *path, char *folder, size_t room)
{
    const char *const slash =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '/');
    const char *const back =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\\');
    const char *const last = ((slash != NULL) && ((back == NULL) || (slash > back))) ? slash : back;
    if (last == NULL)
    {
        {
            VerbaLinea buffer_line = mmgr_line_over(folder, room);
            mmgr_line_put(&buffer_line, ".");
            (void)mmgr_line_finish(&buffer_line);
        }
        return;
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(folder, room);
        mmgr_line_put_max(&buffer_line, path, (size_t)((int)(last - path)));
        (void)mmgr_line_finish(&buffer_line);
    }
}

int program_main(int count, char **word)
{
    if (count != 6)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "orior_gsm_write <part> <path.khw> <layout> <mnemonics> <machine writer>");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 2;
    }
    const char *const part = word[1];
    const char *const machine = word[2];
    QryShared *const run = qry_member_run();
    if (run == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  orior_gsm_write: no query writer runs: ");
            mmgr_line_put(output_line, QRY_ENVIRONMENT);
            mmgr_line_put(output_line, " names no .qry a writer has open");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    // the forms the vendor's writer passes, distinguished from any passed before them by their sequence
    const unsigned char *bytes = NULL;
    unsigned long long size = 0ULL;
    unsigned long long before = 0ULL;
    if (!qry_member_blob(GSM_FORMS_NAME, &bytes, &size, &before))
    {
        before = 0ULL;
    }
    unsigned long long sequence = 0ULL;
    if (!gsm_vendor_run(word))
    {
        return 1;
    }
    if (!qry_member_blob(GSM_FORMS_NAME, &bytes, &size, &sequence) || (sequence <= before))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  orior_gsm_write: the vendor's writer passed the run's buffer no ");
            mmgr_line_put(output_line, GSM_FORMS_NAME);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    const unsigned int forms = gsm_relations_read(bytes, size);
    char rulesets[1024];
    gsm_folder_of(machine, rulesets, sizeof(rulesets));
    char path[1200];
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, rulesets);
        mmgr_line_put(&buffer_line, "/");
        mmgr_line_put(&buffer_line, part);
        mmgr_line_put(&buffer_line, ".gsm");
        (void)mmgr_line_finish(&buffer_line);
    }
    static char s_out_bytes[QRY_WRITER_RING_BYTES / 2ULL];
    const size_t room = sizeof(s_out_bytes);
    size_t out = 0u;
    unsigned int relations = 0u;
    unsigned int cases = 0u;
    for (unsigned int anchor = 0u; anchor < (unsigned int)LADDER_ANCHOR_COUNT; anchor += 1u)
    {
        for (unsigned int read_as = 0u; read_as < 2u; read_as += 1u)
        {
            if ((s_answered[anchor][read_as] == 0) || (s_ladder_measured[anchor] != 0))
            {
                continue;
            }
            relations += 1u;
            for (unsigned int at = 0u; at < LADDER_CASE_COUNT; at += 1u)
            {
                const LadderQuestion *const ladder_case = &s_ladder_cases[at];
                unsigned int answer = 0u;
                if ((ladder_case->anchor != anchor) || (ladder_case->words != 2u) ||
                    !ladder_answer(anchor, ladder_case->word, ladder_case->words, &answer))
                {
                    continue;
                }
                answer = (read_as != 0u) ? ladder_signed_answer(anchor, ladder_case->word) : answer;
                out = verba_numerus.u64(&(VerbaNumerusCfg){
                    .out = s_out_bytes, .cap = room, .at = out, .val = (unsigned long long)(ladder_case->word[0])});
                out = verba_littera.ch(&(VerbaLitteraCfg){.out = s_out_bytes, .cap = room, .at = out, .ch = ','});
                out = verba_numerus.u64(&(VerbaNumerusCfg){
                    .out = s_out_bytes, .cap = room, .at = out, .val = (unsigned long long)(ladder_case->word[1])});
                out = verba_textus.put(&(VerbaTextusCfg){.out = s_out_bytes, .cap = room, .at = out, .text = "->"});
                out = verba_numerus.u64(
                    &(VerbaNumerusCfg){.out = s_out_bytes, .cap = room, .at = out, .val = (unsigned long long)answer});
                out = verba_littera.ch(&(VerbaLitteraCfg){.out = s_out_bytes, .cap = room, .at = out, .ch = '\n'});
                cases += 1u;
            }
        }
    }
    if (!verba_finis.ok(&(VerbaFinisCfg){.cap = room, .at = out}) || !qry_member_filed(path, s_out_bytes, out))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  orior_gsm_write: ");
            mmgr_line_put(output_line, path);
            mmgr_line_put(output_line, " could not be written");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  orior_gsm_write: ");
        mmgr_line_decimal(output_line, relations, 1u);
        mmgr_line_put(output_line, " relations the ");
        mmgr_line_decimal(output_line, forms, 1u);
        mmgr_line_put(output_line, " forms answered, ");
        mmgr_line_decimal(output_line, cases, 1u);
        mmgr_line_put(output_line, " cases, written to ");
        mmgr_line_put(output_line, path);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 0;
}
