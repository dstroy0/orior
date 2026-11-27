// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// The construction set of the register lane's VHDL on a part, measured by the machine (engine_table.md item 11(f)):
// each form of vhdl.krs written alone through the ruleset (ruleset_form_written) into a clocked entity whose ports give
// its registers and whose outputs take them, synthesized by GHDL and Yosys, and its cost the longest path from the
// ports to the registers in Yosys's generic cells (ltp -noff). A register parameter is tried as a word and as a wide
// word until GHDL analyzes the form; a predicate is a boolean and a count or an offset the integer 7. A form that
// names what no form alone contains (the lane's opening and close, its states, its memory) is not measured, and costs
// what the lane's schedule model gives a form it does not name. The set is written as a krep KCS file and read back
// whole, and the file is removed where the two differ.
#include "../../../includes/file_defs/krep/krep.h"
#include "../../platform/platform.h"
#include "../engine/engine_note.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "crc.h"
#include "memoria_operor/memoria_operor.h"
#include "vhdl_target.h"
#include "yosys_script.h"

#include <stddef.h>

// the order of the texts `left` and `right`, each read to its terminator or to `cap`: below 0 where `left`
// comes first, 0 where they are the same, above 0 where `right` does
static inline int vhdl_construction_set_text_order(const char *left, const char *right, size_t cap)
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

// the most parameters of a form, and the bytes of a form's or a parameter's name, the terminator among them
#define CONSTRUCTION_PARAMETERS 32u
#define CONSTRUCTION_NAME_BYTES 64u

// the bytes a text of the set holds at max: the ruleset, a form alone in its entity, or the names of the forms
#define CONSTRUCTION_TEXT_BYTES (1ULL << 24u)

// a form as vhdl.krs names it: its name and its parameters' names
struct ConstructionForm
{
    char name[CONSTRUCTION_NAME_BYTES];
    char parameters[CONSTRUCTION_PARAMETERS][CONSTRUCTION_NAME_BYTES];
    unsigned int parameter_count;
};

// the `length` bytes at `bytes` copied into `out`, terminated: 1 where they fit
static int construction_copied(const char *bytes, size_t length, char *out, size_t room)
{
    VerbaLinea line = mmgr_line_over(out, room);
    (void)mmgr_line_put_n(&line, bytes, length);
    return (mmgr_line_finish(&line) == length) && (mmgr_line_ok(&line) != 0);
}

// The form of the line at `line`, `length` bytes, into `form`, where the line is `form <name> <parameter>... = <text>`.
// 1 where it is such a line and each name fits its bytes
static int construction_line(const char *line, size_t length, ConstructionForm *form)
{
    const char *const found = EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = line, .cap = length, .byte = '=');
    // the equals sign lies inside the line, at or past its start
    const size_t equals = ((found != NULL) && (found < (line + length))) ? (size_t)(found - line) : length;
    // the program resident is an entity of its own around the lane, not a form a state contains
    const int resident = (length >= 18u) && (EMBED_CALL(memor.cmp, MemoriaCfg, .src = line,
                                                        .other = "form program_unit ", .bytes = 18u) == 0);
    if ((length < 5u) || (EMBED_CALL(memor.cmp, MemoriaCfg, .src = line, .other = "form ", .bytes = 5u) != 0) ||
        (equals == length) || (resident != 0))
    {
        return 0;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = form, .bytes = sizeof(*form), .val = 0);
    int named = 0;
    int fits = 1;
    size_t word = 5u;
    while (word < equals)
    {
        // the word up to its space, inside the bytes before the equals sign
        const size_t stop = (word < equals) ? word + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = &line[word],
                                                                .cap = equals - word, .other = " ")
                                            : word;
        if ((stop > word) && (named == 0))
        {
            fits = fits && construction_copied(&line[word], stop - word, form->name, sizeof(form->name));
            named = 1;
        }
        else if (stop > word)
        {
            fits = fits && (form->parameter_count < CONSTRUCTION_PARAMETERS) &&
                   construction_copied(&line[word], stop - word, form->parameters[form->parameter_count],
                                       CONSTRUCTION_NAME_BYTES);
            form->parameter_count += (form->parameter_count < CONSTRUCTION_PARAMETERS) ? 1u : 0u;
        }
        word = stop + 1u;
    }
    return fits;
}

// the forms vhdl.krs gives, in its order, into `forms`, a ConstructionForm each, which the call begins
static void construction_forms(const char *path, PlatformArray *forms)
{
    platform_array_begun(forms, sizeof(ConstructionForm), platform_place_g_program_owner, 0);
    PlatformText text;
    platform_text_begun(&text, CONSTRUCTION_TEXT_BYTES, platform_place_g_program_owner, 0);
    PlatformFile *const file = platform_file_open(path, "rb");
    if (file != NULL)
    {
        char block[4096];
        size_t read = platform_file_read_items(block, 1u, sizeof(block), file);
        while (read != 0u)
        {
            (void)mmgr_line_put_n(&text.line, block, read);
            read = platform_file_read_items(block, 1u, sizeof(block), file);
        }
        platform_file_close(file);
    }
    const char *const bytes = platform_text_bytes(&text);
    const size_t size = (size_t)platform_text_size(&text);
    size_t at = 0u;
    while (at < size)
    {
        const char *const newline =
            EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = &bytes[at], .cap = size - at, .byte = '\n');
        // a line ends at or past its start
        const size_t end = (newline != NULL) ? (size_t)(newline - bytes) : size;
        ConstructionForm form;
        if (construction_line(&bytes[at], end - at, &form) != 0)
        {
            (void)platform_array_appended(forms, &form);
        }
        at = end + 1u;
    }
    platform_text_ended(&text);
}

// what a parameter is bound to: a predicate a boolean, a count or an offset an integer, and a register a word or a
// wide word
enum ConstructionType
{
    CONSTRUCTION_BOOLEAN = 0,
    CONSTRUCTION_INTEGER = 1,
    CONSTRUCTION_REGISTER = 2
};

static ConstructionType construction_type(const char *parameter)
{
    static const char *const predicates[] = {"where", "also"};
    static const char *const integers[] = {"bits", "offset", "at", "count", "state", "error"};
    // a parameter the form takes and does not write, its name opening with an underscore, is of its name's type
    const char *const named = (parameter[0] == '_') ? (parameter + 1) : parameter;
    for (const char *const name : predicates)
    {
        if (vhdl_construction_set_text_order(named, name, PLATFORM_TEXT_TO_TERMINATOR) == 0)
        {
            return CONSTRUCTION_BOOLEAN;
        }
    }
    for (const char *const name : integers)
    {
        if (vhdl_construction_set_text_order(named, name, PLATFORM_TEXT_TO_TERMINATOR) == 0)
        {
            return CONSTRUCTION_INTEGER;
        }
    }
    return CONSTRUCTION_REGISTER;
}

// `length` bytes of `bytes` written to the file at `path`: 1 where every byte went
static int construction_write(const char *path, const char *bytes, unsigned long long length)
{
    return platform_file_written(path, bytes, length);
}

// the file `name` in the folder `work` into `path`, `room` bytes of it
static const char *construction_path(const char *work, const char *name, char *path, size_t room)
{
    VerbaLinea line = mmgr_line_over(path, room);
    mmgr_line_put(&line, work);
    mmgr_line_ch(&line, '/');
    mmgr_line_put(&line, name);
    (void)mmgr_line_finish(&line);
    return path;
}

// the number after `label` on the first line of `path` that contains it, and 1 where a line contains it
static int construction_after(const char *path, const char *label, unsigned long long *number)
{
    PlatformFile *const file = platform_file_open(path, "rb");
    if (file == NULL)
    {
        return 0;
    }
    char line[1024];
    int found = 0;
    while ((found == 0) && (platform_file_text_read(line, sizeof(line), file) != NULL))
    {
        const char *const at = EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = line, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                                          .other = label, .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
        found = at != NULL;
        *number = found ? EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg,
                                     .src = at + EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = label,
                                                            .cap = PLATFORM_TEXT_TO_TERMINATOR),
                                     .end = NULL, .base = 10)
                        : *number;
    }
    platform_file_close(file);
    return found;
}

// what a parameter binds to in VHDL, by its type and width: none for an integer, then a boolean, a word and a wide word
static const char *construction_type_text(unsigned int bound)
{
    static const char *const s_types[4] = {"", "boolean", "cycle_word", "cycle_wide"};
    return s_types[bound];
}

// The form alone in a clocked entity onto `text`. Each parameter is a variable that a port gives at the clock and a
// port takes after the form. The package the lane's opening holds comes first
static void construction_entity(const char *package, const char *form, const unsigned int *bound, unsigned int count,
                                PlatformText *text)
{
    VerbaLinea *const line = &text->line;
    mmgr_line_put(line, package);
    mmgr_line_put(line, "\nlibrary ieee;\nuse ieee.std_logic_1164.all;\nuse ieee.numeric_std.all;\n"
                        "use work.cycle_program.all;\n\nentity form_alone is\n    port (clock : in std_ulogic");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (bound[at] != 0u)
        {
            mmgr_line_put(line, ";\n          x");
            mmgr_line_decimal(line, at, 1u);
            mmgr_line_put(line, "_in : in ");
            mmgr_line_put(line, construction_type_text(bound[at]));
            mmgr_line_put(line, ";\n          x");
            mmgr_line_decimal(line, at, 1u);
            mmgr_line_put(line, "_out : out ");
            mmgr_line_put(line, construction_type_text(bound[at]));
        }
    }
    mmgr_line_put(
        line, ";\n          carry_in : in natural range 0 to 1;\n          carry_out : out natural range 0 to 1);\n"
              "end entity form_alone;\n\narchitecture measured of form_alone is\nbegin\n    alone : process (clock)\n");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (bound[at] != 0u)
        {
            mmgr_line_put(line, "        variable x");
            mmgr_line_decimal(line, at, 1u);
            mmgr_line_put(line, " : ");
            mmgr_line_put(line, construction_type_text(bound[at]));
            mmgr_line_put(line, ";\n");
        }
    }
    mmgr_line_put(line,
                  "        variable carry : natural range 0 to 1;\n        variable total : unsigned(32 downto 0);\n"
                  "        variable product : unsigned(63 downto 0);\n    begin\n        if rising_edge(clock) then\n");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (bound[at] != 0u)
        {
            mmgr_line_put(line, "            x");
            mmgr_line_decimal(line, at, 1u);
            mmgr_line_put(line, " := x");
            mmgr_line_decimal(line, at, 1u);
            mmgr_line_put(line, "_in;\n");
        }
    }
    mmgr_line_put(line, "            carry := carry_in;\n");
    mmgr_line_put(line, form);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (bound[at] != 0u)
        {
            mmgr_line_put(line, "            x");
            mmgr_line_decimal(line, at, 1u);
            mmgr_line_put(line, "_out <= x");
            mmgr_line_decimal(line, at, 1u);
            mmgr_line_put(line, ";\n");
        }
    }
    mmgr_line_put(
        line, "            carry_out <= carry;\n        end if;\n    end process alone;\nend architecture measured;\n");
}

// the bytes of one argument of a form alone, its terminator among them
#define CONSTRUCTION_ARGUMENT_BYTES 16u

// The form bound as `binding` written alone into `text`, and its bindings into `bound`. Each register is a word, or a
// wide word where `binding` holds its bit. 0 where the binding makes a predicate or an integer wide, or the ruleset
// cannot write the form
static int construction_bound(const Ruleset *rules, const ConstructionForm *form, unsigned int binding,
                              unsigned int *bound, PlatformText *text)
{
    char written[CONSTRUCTION_PARAMETERS][CONSTRUCTION_ARGUMENT_BYTES];
    const char *arguments[CONSTRUCTION_PARAMETERS];
    int skipped = 0;
    for (unsigned int at = 0u; at < form->parameter_count; at += 1u)
    {
        const ConstructionType type = construction_type(form->parameters[at]);
        const int wide = ((binding >> at) & 1u) != 0u;
        // a binding that makes a predicate or an integer wide is the binding before it
        skipped = skipped || (wide && (type != CONSTRUCTION_REGISTER));
        VerbaLinea argument = mmgr_line_over(written[at], sizeof(written[at]));
        if (type == CONSTRUCTION_INTEGER)
        {
            mmgr_line_ch(&argument, '7');
        }
        else
        {
            mmgr_line_ch(&argument, 'x');
            mmgr_line_decimal(&argument, at, 1u);
        }
        (void)mmgr_line_finish(&argument);
        arguments[at] = written[at];
        bound[at] = (type == CONSTRUCTION_INTEGER) ? 0u : (type == CONSTRUCTION_BOOLEAN) ? 1u : wide ? 3u : 2u;
    }
    platform_text_reset(text);
    return (skipped == 0) &&
           ruleset_form_written(rules, form->name, arguments, form->parameter_count, NULL, &text->line);
}

// the form's cost measured in `work`: each binding of its registers to words and wide words tried until GHDL analyzes
// the form alone, which is then synthesized; 1 and the cost where one binding succeeds
static int construction_measure(const Ruleset *rules, const char *package, const char *work,
                                const ConstructionForm *form, unsigned long long *cost)
{
    char alone_path[PLATFORM_PATH_BYTES];
    char library_path[PLATFORM_PATH_BYTES];
    char analysis_log[PLATFORM_PATH_BYTES];
    char verilog_path[PLATFORM_PATH_BYTES];
    char synthesis_log[PLATFORM_PATH_BYTES];
    char script_path[PLATFORM_PATH_BYTES];
    char yosys_log[PLATFORM_PATH_BYTES];
    (void)construction_path(work, "alone.vhd", alone_path, sizeof(alone_path));
    (void)construction_path(work, "work-obj08.cf", library_path, sizeof(library_path));
    (void)construction_path(work, "analyze.log", analysis_log, sizeof(analysis_log));
    (void)construction_path(work, "alone.v", verilog_path, sizeof(verilog_path));
    (void)construction_path(work, "synth.log", synthesis_log, sizeof(synthesis_log));
    (void)construction_path(work, "alone.ys", script_path, sizeof(script_path));
    (void)construction_path(work, "yosys.log", yosys_log, sizeof(yosys_log));
    PlatformText text;
    PlatformText entity;
    PlatformText script;
    platform_text_begun(&text, CONSTRUCTION_TEXT_BYTES, platform_place_g_program_owner, 0);
    platform_text_begun(&entity, CONSTRUCTION_TEXT_BYTES, platform_place_g_program_owner, 0);
    platform_text_begun(&script, CONSTRUCTION_TEXT_BYTES, platform_place_g_program_owner, 0);
    int measured = 0;
    int failed = 0;
    // a form's registers are few: the words and wide words they can be bound as are few
    const unsigned int bindings = 1u << form->parameter_count;
    for (unsigned int binding = 0u; (measured == 0) && (failed == 0) && (binding < bindings); binding += 1u)
    {
        unsigned int bound[CONSTRUCTION_PARAMETERS];
        if (construction_bound(rules, form, binding, bound, &text) == 0)
        {
            continue;
        }
        platform_text_reset(&entity);
        construction_entity(package, platform_text_bytes(&text), bound, form->parameter_count, &entity);
        if (!construction_write(alone_path, platform_text_bytes(&entity), platform_text_size(&entity)))
        {
            failed = 1;
            continue;
        }
        // the library of the last analysis goes, and GHDL reads the form alone into analyze.log, in the work folder
        (void)platform_file_removed(library_path);
        const char *const analysis[] = {"ghdl", "-a", "--std=08", "alone.vhd", NULL};
        if (platform_program_run_in(analysis, work, analysis_log, NULL, 0) != 0L)
        {
            continue;
        }
        platform_text_reset(&script);
        const int scripted = yosys_script().synthesis("alone.v", "form_alone", "", &script);
        // GHDL writes the Verilog of the form to alone.v and its errors to synth.log, and then Yosys reads alone.ys
        const char *const synthesis[] = {"ghdl",      "--synth", "--std=08",   "--out=verilog",
                                         "alone.vhd", "-e",      "form_alone", NULL};
        const char *const yosys[] = {"yosys", "-s", "alone.ys", NULL};
        *cost = 0ULL;
        measured = scripted &&
                   construction_write(script_path, platform_text_bytes(&script), platform_text_size(&script)) &&
                   (platform_program_run_in(synthesis, work, verilog_path, synthesis_log, 0) == 0L) &&
                   (platform_program_run_in(yosys, work, yosys_log, NULL, 0) == 0L) &&
                   construction_after(yosys_log, "(length=", cost);
        failed = (measured == 0) ? 1 : 0;
    }
    platform_text_ended(&script);
    platform_text_ended(&entity);
    platform_text_ended(&text);
    return measured;
}

// a note on stderr after the set's name: `what`, then `between`, then `after`
static void construction_noted(const char *what, const char *between, const char *after)
{
    EngineNote engine_note;
    engine_note_begun(&engine_note);
    mmgr_line_put(&engine_note.line, "  vhdl construction set: ");
    mmgr_line_put(&engine_note.line, what);
    mmgr_line_put(&engine_note.line, between);
    mmgr_line_put(&engine_note.line, after);
    mmgr_line_ch(&engine_note.line, '\n');
    engine_note_written(&engine_note);
}

// The costs and the names of the measured forms as one table, written to `path` as a krep KCS file and read back
// whole. The names pack eight bytes a word, the least significant first, and zeros pad the last word. 1 where the file
// read back holds the table, else the file goes
static int construction_table(const char *path, const PlatformArray *costs, const PlatformText *names_text)
{
    const char *const names = names_text->cell;
    const unsigned long long name_bytes = platform_text_size(names_text);
    const unsigned long long name_words = (name_bytes + 7ULL) / 8ULL;
    const unsigned long long word_count = costs->count + name_words;
    unsigned long long *const words = (unsigned long long *)platform_cell_cleared(
        (word_count != 0ULL) ? word_count : 1ULL, sizeof(unsigned long long), platform_place_g_program_owner, 0);
    if (words == NULL)
    {
        return 0;
    }
    if (costs->count != 0ULL)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = words, .src = costs->cell,
                   .bytes = (size_t)(costs->count * sizeof(unsigned long long)));
    }
    for (unsigned long long at = 0ULL; at < name_bytes; at += 1ULL)
    {
        words[costs->count + (at / 8ULL)] |= (unsigned long long)(unsigned char)names[at] << (8u * (at % 8ULL));
    }
    const KrepFormTable table = {costs->count, name_words, crc_words(CRC_TABLE, words, (size_t)word_count), words};
    EngineError error;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &error, .bytes = sizeof(error), .val = 0);
    KrepFormTable back;
    const int written = krep_forms_write(path, &table, &error);
    const int read = written && krep_forms_read(path, &back, &error);
    const int same = read && (back.forms == table.forms) && (back.name_words == table.name_words) &&
                     (back.crc == table.crc) &&
                     (EMBED_CALL(memor.cmp, MemoriaCfg, .src = back.words, .other = table.words,
                                 .bytes = (size_t)(word_count * sizeof(unsigned long long))) == 0);
    if (read)
    {
        krep_forms_release(&back);
    }
    if (!same)
    {
        (void)platform_file_removed(path);
    }
    platform_cell_released(words, platform_place_g_program_owner);
    return same;
}

// the package body that lane_open writes, cut at the end of its package body, into `package`: 1, or 0 with why on
// stderr
static int construction_package(const Ruleset *rules, PlatformText *package)
{
    static const char s_package_end[] = "end package body cycle_program;\n";
    if (!ruleset_form_written(rules, "lane_open", NULL, 0u, NULL, &package->line))
    {
        construction_noted("the ruleset wrote no lane_open", "", "");
        return 0;
    }
    const char *const written = platform_text_bytes(package);
    const char *const end =
        EMBED_CALL(cellul.find, CatenaFinitaCfg, .src = written, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                   .other = s_package_end, .other_cap = PLATFORM_TEXT_TO_TERMINATOR, .ci = EMBED_FALSE);
    if (end == NULL)
    {
        construction_noted("lane_open contains no package body", "", "");
        return 0;
    }
    // the package body ends inside the text, past its start
    platform_text_cut(package, (unsigned long long)(end - written) + (sizeof(s_package_end) - 1u));
    return 1;
}

int program_main(int argc, char **argv)
{
    // each line reaches the log as it is written, where stdout is a file: each form is synthesized alone
    platform_line_out_every_write();
    if (argc != 4)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "usage: vhdl_construction_set <work folder> <vhdl.krs> <out .kcs>");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 2;
    }
    const char *const work = argv[1];
    const Ruleset *const rules = vhdl_target().ruleset(1);
    PlatformArray forms;
    construction_forms(argv[2], &forms);
    if (rules == NULL)
    {
        construction_noted("the ruleset could not be read", "", "");
        platform_array_ended(&forms);
        return 1;
    }
    if (forms.count == 0ULL)
    {
        construction_noted(argv[2], "", " contains no form");
        platform_array_ended(&forms);
        return 1;
    }
    PlatformText package;
    platform_text_begun(&package, CONSTRUCTION_TEXT_BYTES, platform_place_g_program_owner, 0);
    if (construction_package(rules, &package) == 0)
    {
        platform_text_ended(&package);
        platform_array_ended(&forms);
        return 1;
    }
    PlatformArray costs;
    platform_array_begun(&costs, sizeof(unsigned long long), platform_place_g_program_owner, 0);
    PlatformText names;
    platform_text_begun(&names, CONSTRUCTION_TEXT_BYTES, platform_place_g_program_owner, 0);
    unsigned int unmeasured = 0u;
    for (unsigned long long at = 0ULL; at < forms.count; at += 1ULL)
    {
        const ConstructionForm *const form = (const ConstructionForm *)platform_array_at(&forms, at);
        unsigned long long cost = 0ULL;
        if (construction_measure(rules, platform_text_bytes(&package), work, form, &cost) == 0)
        {
            unmeasured += 1u;
            continue;
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put_columns(output_line, form->name, 24u);
            mmgr_line_put(output_line, " ");
            mmgr_line_decimal(output_line, cost, 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        (void)platform_array_appended(&costs, &cost);
        mmgr_line_put(&names.line, form->name);
        mmgr_line_ch(&names.line, '\0');
    }
    const int same = construction_table(argv[3], &costs, &names);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  vhdl construction set: ");
        mmgr_line_decimal(output_line, costs.count, 1u);
        mmgr_line_put(output_line, " forms measured, ");
        mmgr_line_decimal(output_line, unmeasured, 1u);
        mmgr_line_put(output_line, " not measured, ");
        mmgr_line_put(output_line, same ? "written and read back whole to" : "NOT written to");
        mmgr_line_put(output_line, " ");
        mmgr_line_put(output_line, argv[3]);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // two checks: some form measured, and the table read back whole
    const unsigned int failed = ((costs.count == 0ULL) ? 1u : 0u) + (same ? 0u : 1u);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  vhdl construction set: 2 checks, ");
        mmgr_line_decimal(output_line, failed, 1u);
        mmgr_line_put(output_line, " failed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    platform_text_ended(&names);
    platform_array_ended(&costs);
    platform_text_ended(&package);
    platform_array_ended(&forms);
    return (failed == 0u) ? 0 : 1;
}
