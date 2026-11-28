// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_sass_writings.c: a search on the part for a writing. The program takes every form of the machine file
// that writes a register from registers, predicates and numbers alone. It puts each in place of the IADD3 of the
// kernel, with the first two words of the case as its sources, and runs it on the part over every case at once. It
// reads it back against what each ladder relation and each precept gives each case.
//
// A form that gives every case the word of a relation is a writing of that relation in one instruction. Only a run
// finds it: no name, listing or compiler says which form adds. A number whose field is eight bits wide is a truth table
// of three inputs. The program puts a form that holds one with each of its 256 values (precept_value.h).
//
// The program compiles nothing and runs no disassembler. It writes the cubins, and interface_sass_run runs them. That
// program checks each against cubin_safe, and loads it through the driver. It runs it over the cases file that this one
// writes, one thread for each case.
//
// - `interface_sass_writings <pattern cubin> <kernel text> <machine file> <folder>`
// - `interface_sass_writings read <folder> <record>`
// - `interface_sass_writings chains <pattern cubin> <kernel text> <machine file> <folder> <kdm>`
// - `interface_sass_writings chains-read <folder> <record>`
//
// The first form writes `<folder>/list.txt`, the cubins that it names, `<folder>/cases.txt`, and `<folder>/forms.txt`,
// with one line for the instruction that each cubin holds. The second reads `<folder>/answers.txt`, which
// interface_sass_run writes, against every relation, and writes the record.
//
// The third writes every arrangement of the .kdm node by node into `<folder>/chains`. Each node is the first writing
// found for its precept. The fourth reads the answers of the chains back against the relation that each is a row of.
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
