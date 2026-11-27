// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kdm_write.c: writes a part's .kdm, the arrangements of primitives that produce each operator
//
//   kdm_write <part> <path>
//
// The writer finds the arrangements from the relations and from nothing else. It reads no ruleset and no machine file,
// and it queries no target. What a target can write of them, and what each costs it, is the other half. The part
// itself fills that half.
//
// Where the path already holds a .kdm, its costs come forward onto the arrangements that this write finds. The match
// is by the arrangement, and never by where it sat in the file. A cost thus stays across a rewrite. A rewrite that
// finds an arrangement with no time leaves it untimed.
//
// This writes the section of the .kdm (kdm.h). Its tail stays as the file stored it, because the relations give none
// of it. The tail is the registers, widths, loads and stores of the part, in their block of the language of the
// ruleset, and its pipes.
#include "../../../src/cu/transpiler/lstar/protocol/gate/chain_build.h"
#include "../../../src/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"

#include "../qry/writer/qry_member.h"
#include "kdm.h"
#include "verba_scribo/verba_scribo.h"

#define KDM_TEXT_MAX_LEN 160u
#define KDM_ROWS (LADDER_ANCHOR_COUNT * CHAIN_MAX)

#define LADDER_TEXT(name_, text_, words_, measured_) text_,
static const char *const s_anchor_text[] = {LADDER_ANCHORS(LADDER_TEXT)};
#undef LADDER_TEXT

// the longest cost a row carries: an exact rational, numerator/denominator, each a run of decimal digits
#define KDM_COST_MAX_LEN 128u

// One row as it was in the file the last time: the arrangement, what it cost, and over how many runs. A cost is an
// exact rational, and the code carries it as its text in the file. Nothing here computes with one. A read of a cost
// into a number and a write back out rounds it on the way
typedef struct
{
    char text[KDM_TEXT_MAX_LEN];
    char cost[KDM_COST_MAX_LEN];
    unsigned int runs;
} KdmCostRow;

static KdmCostRow s_cost_rows[KDM_ROWS];

// the texts of the rows that one operator wrote, each once
static char s_written[CHAIN_MAX][KDM_TEXT_MAX_LEN];

// Whether `text` is an exact rational written in decimal: digits, an optional leading minus, and at max one /
// with digits after it
static int kdm_exact_text(const char *text)
{
    const char *at = text + ((text[0] == '-') ? 1 : 0);
    const size_t whole =
        EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = at, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "0123456789");
    if (whole == 0u)
    {
        return 0;
    }
    at += whole;
    if (*at == '/')
    {
        at += 1;
        const size_t below = EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = at, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                        .other = "0123456789");
        if (below == 0u)
        {
            return 0;
        }
        at += below;
    }
    return (*at == '\0') ? 1 : 0;
}
static unsigned int s_cost_row_count;

// the .kdm as the run stores it (kdm.h), its section written again and its tail retained
static KdmFile s_file;

// the rows of the .kdm at `path` into s_cost_rows, each by its chain. A path that contains nothing is a first run and
// not a failure
static void kdm_costs_read(const char *path)
{
    static KdmRow s_row;
    kdm_read(&s_file, path);
    for (unsigned int at = 0u; (at < s_file.lines) && (s_cost_row_count < KDM_ROWS); at += 1u)
    {
        if (!kdm_row_read(s_file.line[at], &s_row) ||
            (EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = s_row.chain, .cap = PLATFORM_TEXT_TO_TERMINATOR) >=
             sizeof(s_cost_rows[0].text)))
        {
            continue;
        }
        KdmCostRow *const row = &s_cost_rows[s_cost_row_count];
        {
            VerbaLinea buffer_line = mmgr_line_over(row->text, sizeof(row->text));
            mmgr_line_put(&buffer_line, s_row.chain);
            (void)mmgr_line_finish(&buffer_line);
        }
        row->runs = s_row.runs;
        // the code does not carry a cost that is not an exact rational of decimal digits. It refuses the cost, and
        // the row reads as never timed
        if ((kdm_exact_text(s_row.cost) == 0) || (EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = s_row.cost,
                                                             .cap = PLATFORM_TEXT_TO_TERMINATOR) >= sizeof(row->cost)))
        {
            row->runs = 0u;
            row->cost[0] = '\0';
        }
        else
        {
            VerbaLinea buffer_line = mmgr_line_over(row->cost, sizeof(row->cost));
            mmgr_line_put(&buffer_line, s_row.cost);
            (void)mmgr_line_finish(&buffer_line);
        }
        s_cost_row_count += 1u;
    }
}

// the row `text` was in the last time, or NULL where nothing timed it
static const KdmCostRow *kdm_cost(const char *text)
{
    for (unsigned int at = 0u; at < s_cost_row_count; at += 1u)
    {
        if ((EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_cost_rows[at].text, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = text, .ci = EMBED_FALSE)) &&
            (s_cost_rows[at].runs != 0u))
        {
            return &s_cost_rows[at];
        }
    }
    return NULL;
}

// the cases of `anchor`, into `cases`; the count found
static unsigned int anchor_cases(unsigned int anchor, LadderQuestion *cases)
{
    unsigned int found = 0u;
    for (unsigned int at = 0u; at < LADDER_CASE_COUNT; at += 1u)
    {
        if (s_ladder_cases[at].anchor == anchor)
        {
            cases[found] = s_ladder_cases[at];
            found += 1u;
        }
    }
    return found;
}

// A seed from the own state of the file: the name of the part and the runs already folded into it. An order can
// repeat with no time. Nothing stays in one order once timings come in
static unsigned int kdm_seed(const char *part)
{
    unsigned int seed = 0x9e3779b9u;
    for (unsigned int at = 0u; part[at] != '\0'; at += 1u)
    {
        seed = (seed * 131u) + (unsigned char)part[at];
    }
    for (unsigned int at = 0u; at < s_cost_row_count; at += 1u)
    {
        seed += s_cost_rows[at].runs;
    }
    return seed;
}

// the section, each line after the first up to the tail (kdm.h). The code puts it together in s_section_bytes, and then
// writes it in the place of the file. `s_section_at` is the bytes so far, or the room once a write did not fit
static char s_section_bytes[QRY_WRITER_RING_BYTES / 2ULL];
static size_t s_section_at;

// `text` written after what the section contains
static void section_text(const char *text)
{
    s_section_at = verba_textus.put(
        &(VerbaTextusCfg){.out = s_section_bytes, .cap = sizeof(s_section_bytes), .at = s_section_at, .text = text});
}

// `value` written in base ten after what the section contains
static void section_number(unsigned long long value)
{
    s_section_at = verba_numerus.u64(
        &(VerbaNumerusCfg){.out = s_section_bytes, .cap = sizeof(s_section_bytes), .at = s_section_at, .val = value});
}

int program_main(int count, char **word)
{
    if (count < 3)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "kdm_write <part> <path>");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 2;
    }
    const char *const part = word[1];
    kdm_costs_read(word[2]);
    const unsigned int seed = kdm_seed(part);
    section_text("# Every arrangement of primitives that produces an operator, and what each costs this part.\n");
    section_text("# Found from the relations alone. No ruleset, no machine file and no naming went into a row\n");
    section_text("# here. A cost is an exact rational, numerator/denominator in the part's clock count, and a cost\n");
    section_text("# of - over 0 runs has not been timed on anything.\n");
    section_text("#\n");
    section_text("# The rows of an operator are in no order. Timed in the order they were found, a chain's\n");
    section_text("# reading carries where it sat and not what it costs; shuffled, that washes out over runs and\n");
    section_text("# what is left is the difference between the arrangements. The order moves whenever a run is\n");
    section_text("# folded in, which is why it moves and a cost does not.\n\n");
    section_text(KDM_ROWS_HEAD);
    section_text("\n");

    static ChainSet set;
    static LadderQuestion cases[LADDER_CASE_COUNT];
    char text[KDM_TEXT_MAX_LEN];
    unsigned int written = 0u;
    unsigned int timed = 0u;
    for (unsigned int anchor = 0u; anchor < (unsigned int)LADDER_ANCHOR_COUNT; anchor += 1u)
    {
        const unsigned int found = anchor_cases(anchor, cases);
        if (found == 0u)
        {
            continue;
        }
        const unsigned int chains = chain_build(&set, cases, found);
        chain_shuffle(&set, seed + anchor);
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, s_anchor_text[anchor], 8u);
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal_columns(output_line, found, 2u);
            mmgr_line_put(output_line, " cases  ");
            mmgr_line_decimal_columns(output_line, chains, 5u);
            mmgr_line_put(output_line, " chains  ");
            mmgr_line_decimal_columns(output_line, set.tried, 8u);
            mmgr_line_put(output_line, " tried");
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, (set.over != 0u) ? "  and more than the set contains" : "");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        // the text of an arrangement names it. Two chains of the set that write one text are one row: the first
        unsigned int distinct = 0u;
        for (unsigned int at = 0u; at < chains; at += 1u)
        {
            chain_text(&set.chain[at], text, sizeof(text));
            unsigned int seen = 0u;
            while ((seen < distinct) &&
                   (!EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = s_written[seen], .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                .other = text, .ci = EMBED_FALSE)))
            {
                seen += 1u;
            }
            if (seen < distinct)
            {
                continue;
            }
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = s_written[distinct], .src = text, .bytes = sizeof(text));
            distinct += 1u;
            const KdmCostRow *const cost_row = kdm_cost(text);
            if (cost_row == NULL)
            {
                section_text(s_anchor_text[anchor]);
                section_text("\t");
                section_number(set.chain[at].nodes);
                section_text("\t");
                section_text(text);
                section_text("\t-\t0\n");
            }
            else
            {
                section_text(s_anchor_text[anchor]);
                section_text("\t");
                section_number(set.chain[at].nodes);
                section_text("\t");
                section_text(text);
                section_text("\t");
                section_text(cost_row->cost);
                section_text("\t");
                section_number(cost_row->runs);
                section_text("\n");
                timed += 1u;
            }
            written += 1u;
        }
        if (chains == 0u)
        {
            // an operator that no arrangement of this length reaches. The report of it is the point. The part must
            // answer a direct query about it, or a search must find a longer chain for it
            section_text(s_anchor_text[anchor]);
            section_text("\t0\t-\t-\t0\n");
        }
    }
    const char **lines = NULL;
    unsigned int line_count = 0u;
    const int put = verba_finis.ok(&(VerbaFinisCfg){.cap = sizeof(s_section_bytes), .at = s_section_at}) &&
                    kfile_lines_cut(s_section_bytes, s_section_at, &lines, &line_count) &&
                    kdm_section_written(&s_file, part, lines, line_count) && kdm_write(&s_file, word[2]);
    platform_cell_released(lines, platform_place_g_program_owner);
    kdm_free(&s_file);
    if (!put)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  kdm_write: ");
            mmgr_line_put(output_line, word[2]);
            mmgr_line_put(output_line, " could not be written");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  kdm_write: ");
        mmgr_line_decimal(output_line, written, 1u);
        mmgr_line_put(output_line, " chains into ");
        mmgr_line_put(output_line, word[2]);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, timed, 1u);
        mmgr_line_put(output_line, " of them timed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 0;
}
