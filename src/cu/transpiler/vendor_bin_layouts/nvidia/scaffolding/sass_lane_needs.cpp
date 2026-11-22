// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// Which forms a real lane requests from sass.krs, and which of those the ruleset leaves empty.
//
//     sass_lane_needs
//
// sass.krs leaves 33 of its forms empty, and filling all 33 is not the work: the work is the lane emitted to SASS.
// So the record programs the host oracle runs are arranged and decided for the SASS generator, and every form the
// core decides is counted. A form the lane never decides is not a blocker whatever the ruleset specifies about it, and
// a form it decides that the ruleset leaves empty is between here and a lane. No device, no toolchain, and nothing
// is written: decide() only reports which forms a lane is made of.
#include "../../../../../../utils/test/src/cu/engine/analysis/cycle/record_programs.h"

// the cubin writer is C and its headers carry no guard of their own: the linkage is named here
extern "C"
{
#include "../sass_assemble.h"
#include "../sass_machine.h"
}

#include "../../../../../../includes/file_defs/krs/ruleset_core_words.h"
#include "../../../../../../includes/file_defs/krs/ruleset_reader.h"
#include "../../../../../platform/platform.h"
#include "../../../../scaffolding/ruleset_strings.h"
#include "machine_ir_types.h"
#include "sass_target.h"
#include "target.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// ENGINE_IMAGE_BASE reads the ELF header the linker marks with __ehdr_start (engine_config_platform.h), and this
// tool is linked as a PE, where there is none. Nothing here requests for the image base: it is named so the link
// resolves
#if defined(__GNUC__) && !defined(__ELF__)
extern "C" const char __ehdr_start = 0;
#endif

// the schema's forms by their place in it, each with the parameters it takes, as machine_ir_types.h gives them
#define LANE_FORM_TEXT(name_, text_, parameters_, type_) {text_, parameters_},

static const struct
{
    const char *text;
    unsigned int parameters;
} s_forms[OPCODE_COUNT] = {OPCODES(LANE_FORM_TEXT)};

// 1 where the ruleset gives the form at `place` as an error or leaves it open, and that blocks a lane: a nop writes
// nothing and the lane goes on without it, and an err or an open form refuses (P10). A nop and an err read the same
// from the outside, both writing an empty text, and the ruleset specifies which it means
static int lane_form_errors(const Ruleset *rules, unsigned int place)
{
    return (rules->form_given[place] == (unsigned char)RULESET_CORE_GIVEN_ERR) || (rules->form_given[place] == 0u);
}

// the part's machine file, read once, against which a written lane is assembled
static SassMachine s_machine;
static int s_machine_read;

// Each line of `lane` assembled, and the first that the assembler refuses printed. A lane the generator writes is
// not a lane the part runs: a form the ruleset leaves empty writes nothing and breaks nothing. The text comes
// back whole with its holes in it, and only the assembler determines whether the remainder is machine code
static unsigned int lane_assembles(const std::string &lane, unsigned int *refused, std::string *first)
{
    static unsigned char code[64];
    unsigned int lines = 0u;
    size_t at = 0u;
    while (at < lane.size())
    {
        const size_t end = lane.find('\n', at);
        const size_t stop = (end == std::string::npos) ? lane.size() : end;
        const std::string line = lane.substr(at, stop - at) + "\n";
        size_t character = 0u;
        while ((character < line.size()) && ((line[character] == ' ') || (line[character] == '\t')))
        {
            character += 1u;
        }
        // a label, a note, a directive and a blank line are not instructions and assemble to nothing
        const int instruction = (character < (line.size() - 1u)) && (line[character] != '.') &&
                                (line[character] != '/') && (line.find(':') == std::string::npos) &&
                                (line.find('`') == std::string::npos);
        if (instruction != 0)
        {
            lines += 1u;
            if (sass_assemble_lines(&s_machine, line.c_str(), SASS_CONTROL_SAFE, code, sizeof(code)) == 0u)
            {
                *first = (first->empty()) ? line.substr(0u, line.size() - 1u) : *first;
                *refused += 1u;
            }
        }
        at = stop + 1u;
    }
    return lines;
}

// every form `program` decides, counted into `queried`
static void lane_decide(const HostProgram *program, int reuse, unsigned int *queried, unsigned int *positioned)
{
    HostLoaded loaded;
    if (host_load(program, reuse, &loaded) == 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, program->name);
            mmgr_line_put(output_line, ": not arranged");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return;
    }
    *positioned += 1u;
    std::vector<MachineInstr> items;
    unsigned int places = 0u;
    if (lane_decided_items(sass_target(), &loaded.layout, &places, &items) == 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, program->name);
            mmgr_line_put(output_line, ": ");
            mmgr_line_decimal(output_line, loaded.layout.steps, 1u);
            mmgr_line_put(output_line, " steps, the core decides no lane for it");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        host_free(&loaded);
        return;
    }
    for (const MachineInstr &item : items)
    {
        // the header is in the items as a form of all ones, and is the listing's opening, not a form
        if (item.form < OPCODE_COUNT)
        {
            queried[item.form] += 1u;
        }
    }
    // and the lane as the generator writes it, to determine whether an empty form stops it or is passed over in silence
    const TargetInfo target = {0ULL, 8, 6, 12, 0, ""};
    unsigned int lane_places = 0u;
    unsigned int live = 0u;
    const std::string lane =
        lane_program_text(sass_target(), &loaded.layout, &target, std::string(), &lane_places, &live);
    unsigned int lines = 0u;
    for (size_t at = 0u; at < lane.size(); at += 1u)
    {
        lines += (lane[at] == '\n') ? 1u : 0u;
    }
    unsigned int refused = 0u;
    std::string first;
    const unsigned int instructions = (s_machine_read != 0) ? lane_assembles(lane, &refused, &first) : 0u;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put_columns(output_line, program->name, 16u);
        mmgr_line_put(output_line, " ");
        mmgr_line_decimal_columns(output_line, loaded.layout.steps, 4u);
        mmgr_line_put(output_line, " steps, ");
        mmgr_line_decimal_columns(output_line, items.size(), 5u);
        mmgr_line_put(output_line, " items, lane ");
        mmgr_line_put(output_line, lane.empty() ? "REFUSED" : "written");
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, lines, 1u);
        mmgr_line_put(output_line, " lines, ");
        mmgr_line_decimal(output_line, instructions, 1u);
        mmgr_line_put(output_line, " instructions, ");
        mmgr_line_decimal(output_line, refused, 1u);
        mmgr_line_put(output_line, " the assembler refuses");
        mmgr_line_put(output_line, first.empty() ? "" : ", first: ");
        mmgr_line_put(output_line, first.c_str());
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // the first lane written out whole: what a form that writes nothing leaves behind can be read
    static int dumped;
    if ((dumped == 0) && !lane.empty())
    {
        PlatformFile *const file = platform_file_open("build/sass_lane_needs/lane.sass", "wb");
        if (file != NULL)
        {
            platform_file_write_items(lane.data(), 1u, lane.size(), file);
            platform_file_close(file);
            dumped = 1;
        }
    }
    host_free(&loaded);
}

int program_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    const Ruleset *const rules = sass_target().ruleset(1);
    if (rules == NULL)
    {
        return 1;
    }
    s_machine_read = sass_machine_read(&s_machine, "src/cu/transpiler/lstar/protocol/table/sm_86.khw");
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "machine ");
        mmgr_line_put(output_line, s_machine.part);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, s_machine.forms, 1u);
        mmgr_line_put(output_line, " forms");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    static unsigned int queried[OPCODE_COUNT];
    unsigned int positioned = 0u;
    HostProgram program;
    HostProgram bare;
    // the record programs the host oracle runs, each decided with the registers fresh and again with them reused
    for (int reuse = 0; reuse <= 1; reuse += 1)
    {
        host_arithmetic(&program);
        lane_decide(&program, reuse, queried, &positioned);
        host_division(&program);
        lane_decide(&program, reuse, queried, &positioned);
        host_bitwise(&program);
        lane_decide(&program, reuse, queried, &positioned);
        host_members(&program);
        lane_decide(&program, reuse, queried, &positioned);
        host_affine_limit(&program);
        lane_decide(&program, reuse, queried, &positioned);
        host_bare_divisor(&bare);
        lane_decide(&bare, reuse, queried, &positioned);
        host_inexact(&program, &bare);
        lane_decide(&program, reuse, queried, &positioned);
        // the two tables the program reads through, sized as the host oracle sizes them
        unsigned int *const wide = (unsigned int *)platform_cell_taken(
            (unsigned long long)(512u * sizeof(unsigned int)), (unsigned int)platform_place_g_program_owner, 0);
        unsigned int *const narrow = (unsigned int *)platform_cell_taken(
            (unsigned long long)(4096u * sizeof(unsigned int)), (unsigned int)platform_place_g_program_owner, 0);
        host_tables(&program, wide, narrow);
        lane_decide(&program, reuse, queried, &positioned);
        platform_cell_released(wide, (unsigned int)platform_place_g_program_owner);
        platform_cell_released(narrow, (unsigned int)platform_place_g_program_owner);
    }
    unsigned int decided = 0u;
    unsigned int blocking = 0u;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_decimal(output_line, positioned, 1u);
        mmgr_line_put(output_line, " layouts decided for SASS");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // A form the code generator leaves out is never requested from the ruleset, whatever the lane's items contain. SASS
    // answers 0 to program_unit_written: its resident reaches it already built in the part's own compiler's cubin,
    // and the lane is written into that. Counting such a form as blocking reads the lane's item list where the
    // question is what program() puts to the ruleset
    const int writes_unit = sass_target().program_unit_written();
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line,
                      "forms the lane requests for that sass.krs gives as an error, which is what refuses it:");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    for (unsigned int place = 0u; place < OPCODE_COUNT; place += 1u)
    {
        const int queried_of_ruleset = (place != (unsigned int)OPCODE_PROGRAM_UNIT) || (writes_unit != 0);
        decided += (queried[place] != 0u) ? 1u : 0u;
        if ((queried[place] != 0u) && queried_of_ruleset && (lane_form_errors(rules, place) != 0))
        {
            blocking += 1u;
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "  ");
                mmgr_line_put_columns(output_line, s_forms[place].text, 24u);
                mmgr_line_put(output_line, " requested ");
                mmgr_line_decimal(output_line, queried[place], 1u);
                mmgr_line_put(output_line, " times");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
    }
    if (writes_unit == 0)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  (program_unit is requested ");
        mmgr_line_decimal(output_line, queried[OPCODE_PROGRAM_UNIT], 1u);
        mmgr_line_put(output_line, " times by the lane's items and never of the ruleset: SASS takes its\n   resident "
                                   "from the part's own compiler and writes the lane into that cubin)");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "forms sass.krs gives as an error that no lane here requests for:");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    for (unsigned int place = 0u; place < OPCODE_COUNT; place += 1u)
    {
        if ((queried[place] == 0u) && (lane_form_errors(rules, place) != 0))
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, s_forms[place].text, 24u);
            mmgr_line_put(output_line, " never requested");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_decimal(output_line, decided, 1u);
        mmgr_line_put(output_line, " of the schema's ");
        mmgr_line_decimal(output_line, OPCODE_COUNT, 1u);
        mmgr_line_put(output_line, " forms are requested for; ");
        mmgr_line_decimal(output_line, blocking, 1u);
        mmgr_line_put(output_line, " of those sass.krs gives as an error");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 0;
}
