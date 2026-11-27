// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// head.c: the lines of a SASS ruleset's head, each the part's answer in its .ksc or left out
#include "head.h"
#include "../../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"

#include "../../ksc/ksc.h"
#include "verba_scribo/verba_scribo.h"

unsigned int krs_head_written(char *out, size_t cap, size_t *at, const char *ksc_path, char *missing, size_t room)
{
    static KscFile s_file;
    static KscRow s_row;
    int returned = 0;
    int least = 0;
    ksc_read(&s_file, ksc_path);
    for (unsigned int line = 0u; line < s_file.lines; line += 1u)
    {
        if (!ksc_row_read(s_file.line[line], "run", &s_row) ||
            (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_row.answer, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                         .other = "answers", .ci = EMBED_FALSE)))
        {
            continue;
        }
        returned = returned ||
                   (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_row.question, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                               .other = "container returned", .ci = EMBED_FALSE));
        least = least || (EMBED_CALL(cellul.starts, CatenaFinitaCfg, .src = s_row.question,
                                     .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "opening least", .ci = EMBED_FALSE));
    }
    ksc_free(&s_file);
    unsigned int written = 0u;
    if (returned)
    {
        *at = verba_textus.put(&(VerbaTextusCfg){.out = out, .cap = cap, .at = *at, .text = "toolchain cubin_write\n"});
        written += 1u;
    }
    if (least)
    {
        *at = verba_textus.put(&(VerbaTextusCfg){.out = out, .cap = cap, .at = *at, .text = "header container\n"});
        written += 1u;
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(missing, room);
        mmgr_line_put(&buffer_line, returned ? "" : "toolchain");
        mmgr_line_put(&buffer_line, (!returned && !least) ? ", " : "");
        mmgr_line_put(&buffer_line, least ? "" : "header");
        (void)mmgr_line_finish(&buffer_line);
    }
    return written;
}
