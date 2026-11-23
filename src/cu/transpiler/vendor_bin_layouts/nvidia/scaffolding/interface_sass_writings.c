// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_sass_writings.c: a writing searched for on the part. Every form of the machine file that writes a register
// from registers, predicates and numbers alone is put in place of the kernel's IADD3 with the case's first two words as
// its sources, run on the part over every case at once, and read back against what each ladder relation and each
// precept gives each case. A form that gives every case a relation's word is a writing of that relation in one
// instruction, found by running it and by nothing else: no name, listing or compiler specifies which form adds. A
// number whose field is eight bits wide is a truth table of three inputs, and a form containing one is put with each of
// its 256 values (precept_value.h).
//
// Nothing is compiled and no disassembler is run. The cubins are written here and run by interface_sass_run, which
// checks each against cubin_safe, loads it through the driver and runs it over the cases file this writes, a thread a
// case.
//
//     interface_sass_writings <pattern cubin> <kernel text> <machine file> <folder>
//     interface_sass_writings read <folder> <record>
//     interface_sass_writings chains <pattern cubin> <kernel text> <machine file> <folder> <kdm>
//     interface_sass_writings chains-read <folder> <record>
//
// The first writes <folder>/list.txt, the cubins it names, <folder>/cases.txt and <folder>/forms.txt, the
// instruction each cubin contains a line. The second reads <folder>/answers.txt, which interface_sass_run writes,
// against every relation and writes the record. The third writes every arrangement of the .kdm node by node, each node
// the first writing found for its precept, into <folder>/chains, and the fourth reads the chains' answers back against
// the relation each is a row of.
#include "../../../../engine/engine_note.h"
#include "../../../../engine/rmc/precept_value.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "interface_sass_writings/writings_internal.h"

int program_main(int count, char **words)
{
    if ((count == 5) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[1], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                    .other = "krs-read", .ci = EMBED_FALSE)))
    {
        return writings_krs_read(words[2], words[3], words[4]);
    }
    if (((count == 7) || (count == 8)) &&
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[1], .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "chains",
                    .ci = EMBED_FALSE)))
    {
        char into[1024];
        {
            VerbaLinea buffer_line = mmgr_line_over(into, sizeof(into));
            mmgr_line_put(&buffer_line, words[5]);
            mmgr_line_put(&buffer_line, "/chains");
            (void)mmgr_line_finish(&buffer_line);
        }
        return writings_chains_write(words[2], words[3], words[4], words[5], words[6], (count == 8) ? words[7] : into);
    }
    if ((count >= 5) && ((count % 2) == 1) &&
        (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[1], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                    .other = "descent-read", .ci = EMBED_FALSE)))
    {
        return writings_descent_read(words[2], (unsigned int)(count - 3) / 2u, &words[3]);
    }
    if ((count == 4) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[1], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                    .other = "chains-read", .ci = EMBED_FALSE)))
    {
        return writings_chains_read(words[2], words[3]);
    }
    if ((count == 4) && (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = words[1], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                    .other = "read", .ci = EMBED_FALSE)))
    {
        return writings_read(words[2], words[3]);
    }
    if (count != 5)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(
                &engine_note.line,
                "interface_sass_writings <pattern cubin> <kernel text> <machine file> "
                "<folder>\ninterface_sass_writings read <folder> <record>\ninterface_sass_writings chains <pattern "
                "cubin> <kernel text> <machine file> <folder> <kdm> [<into>]\ninterface_sass_writings chains-read "
                "<folder> <record>\ninterface_sass_writings descent-read <record> <into> <cases> [<into> <cases>...]");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    return writings_write(words[1], words[2], words[3], words[4]);
}
