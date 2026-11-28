// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// The compiler on the device checked against the compiler on the host (engine_table.md item 11(f)(a)).
// record_test's programs, drawn from the same stream in the same order (record_image.h), are arranged on the host
// by keymath and key_schedule and on the device by the same cores (keymath_core.h, key_schedule_core.h, layout_device):
// the two layouts must agree word for word. In each language of the register lane, PTX, C and VHDL, the forms the
// device decides (codegen_device_instrs) must be the host's (CodeGenerator::decided) item for item, as the lane runs
// and split into states by a schedule model, with the schedule's states, max chained cost and forms over the budget
// the host's. The text the device writes from the host's layout (codegen_device) and from its own
// (codegen_device_steps) must be the code generator's byte for byte, where the assembly printer takes the language's
// ruleset. Last, the bootstrap's fixed point: the assembly printer's own lane, written by the device with the assembly
// printer, must be the code generator's. The test is one job on the device's tessera daemon, submitted before its first
// device work.
#include "../../../../../../includes/file_defs/krs/ruleset_reader.h"
#include "../../../../../../includes/platform/platform.h"
#include "../../engine/analysis/cycle/record_image.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "code_generator.h"
#include "codegen_device.h"
#include "memoria_operor/memoria_operor.h"
#include "sim.h"

#include <cuda_runtime.h>

#include <stddef.h>

// the max the test puts on the device at once: a lane's forms, records and bytes, and the assembly printer, each a
// few megabytes at max for these programs. The first run peaked 8 MB over its process's CUDA context
#define DEVICE_TEST_DECLARED (64ull << 20u)

// the schedule model each lane is split by: every form a cost of 1, four to a state, one write a state
#define DEVICE_TEST_BUDGET 4u

#define DEVICE_TEST_LANGUAGES 3u

// the bytes of what a check names and of a reason a call gives, the terminator among them, and of a lane's text
#define DEVICE_TEST_WHAT_BYTES 1536u
#define DEVICE_TEST_WHY_BYTES 1024u
#define DEVICE_TEST_TEXT_BYTES (1ULL << 30u)

// the header each lane is written under, which the assembly printer takes whole
static const char s_device_header[] = "// codegen_device_test\n";

// the checks made and failed, and the lanes a language does not support
struct DeviceResults
{
    unsigned int checks;
    unsigned int failed;
    unsigned int unsupported;
};

// what a check names, as the test writes it: its bytes and the line over them
struct DeviceWhat
{
    char text[DEVICE_TEST_WHAT_BYTES];
    VerbaLinea line;
};

// a check's words begun with `head`
static void device_what_begun(DeviceWhat *what, const char *head)
{
    what->line = mmgr_line_over(what->text, sizeof(what->text));
    mmgr_line_put(&what->line, head);
}

static void device_check(DeviceResults *results, int passed, DeviceWhat *what)
{
    (void)mmgr_line_finish(&what->line);
    results->checks += 1u;
    results->failed += passed ? 0u : 1u;
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, passed ? "ok  " : "FAIL");
        mmgr_line_put(output_line, " ");
        mmgr_line_put(output_line, what->text);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
}

// a check whose words are `head` then `rest`
static void device_named_check(DeviceResults *results, int passed, const char *head, const char *rest)
{
    DeviceWhat what;
    device_what_begun(&what, head);
    mmgr_line_put(&what.line, rest);
    device_check(results, passed, &what);
}

// 1 where the `length` bytes at `line` from `from` read `label`, then a number of at least 1 into `number`, then a word
static int device_claim_read(const char *line, size_t length, size_t from, const char *label,
                             unsigned long long *number)
{
    const size_t label_length =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = label, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    if (((from + label_length) > length) ||
        (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &line[from], .other = label, .bytes = label_length) != 0))
    {
        return 0;
    }
    const char *const digits = &line[from + label_length];
    const char *end = NULL;
    *number =
        (unsigned long long)EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = digits, .end = &end, .base = 10u);
    // the number ends inside the line, past its start
    const size_t after = (end != NULL) ? (size_t)(end - line) : length;
    return (end != NULL) && (end != digits) && ((after + 1u) < length) && (line[after] == ' ') &&
           (line[after + 1u] != ' ');
}

// the place of `needle` in the `length` bytes at `text`, or `length` where it is not there
static size_t device_find(const char *text, size_t length, const char *needle)
{
    const size_t needle_length =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = needle, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    for (size_t at = 0u; (at + needle_length) <= length; at += 1u)
    {
        if (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &text[at], .other = needle, .bytes = needle_length) == 0)
        {
            return at;
        }
    }
    return length;
}

// 1 where every line of a lane's ledger reads `<register> <holder> claimed <n> <form> released <n> <form>`, the claim
// no later than the release, and the ledger holds a line at all
static int device_claims_well_formed(const char *claims)
{
    const size_t size = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = claims, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    int well = size != 0u;
    for (size_t at = 0u; well && (at < size);)
    {
        const char *const newline =
            EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = &claims[at], .cap = size - at, .byte = '\n');
        // a line ends at or past its start
        const size_t end = (newline != NULL) ? (size_t)(newline - claims) : size;
        const char *const line = &claims[at];
        const size_t length = end - at;
        at = end + 1u;
        const size_t claimed = device_find(line, length, " claimed ");
        const size_t released = device_find(line, length, " released ");
        const size_t space = device_find(line, length, " ");
        unsigned long long claimed_at = 0ull;
        unsigned long long released_at = 0ull;
        well = (claimed != length) && (released != length) && (claimed > 0u) && (space < claimed) &&
               (released > claimed) && device_claim_read(line, length, claimed, " claimed ", &claimed_at) &&
               device_claim_read(line, length, released, " released ", &released_at) && (claimed_at >= 1ull) &&
               (claimed_at <= released_at);
    }
    return well;
}

// 1 where two lists of forms, a MachineInstr each, are the same item for item
static int device_items_same(const PlatformArray *left, const PlatformArray *right)
{
    return (left->count == right->count) &&
           ((left->count == 0ull) || (EMBED_CALL(memor.cmp, MemoriaCfg, .src = left->cell, .other = right->cell,
                                                 .bytes = (size_t)(left->count * sizeof(MachineInstr))) == 0));
}

// what the device gave, or why it did not, onto `what`
static void device_why(DeviceWhat *what, int ok, const char *error)
{
    if (ok == 0)
    {
        mmgr_line_put(&what->line, " (");
        mmgr_line_put(&what->line, error);
        mmgr_line_put(&what->line, ")");
    }
}

// Where the device's text first leaves the code generator's, onto `what`: the byte, and a few lines of each from the
// line it is on. Nothing where the two are the same
static void device_text_differ(DeviceWhat *what, const PlatformText *host, const PlatformText *device)
{
    const char *const host_text = host->cell;
    const char *const device_text = device->cell;
    const unsigned long long host_length = platform_text_size(host);
    const unsigned long long device_length = platform_text_size(device);
    unsigned long long at = 0ull;
    while ((at < host_length) && (at < device_length) && (host_text[at] == device_text[at]))
    {
        at += 1ull;
    }
    if ((at == host_length) && (at == device_length))
    {
        return;
    }
    unsigned long long from = at;
    while ((from > 0ull) && (host_text[from - 1ull] != '\n'))
    {
        from -= 1ull;
    }
    VerbaLinea *const line = &what->line;
    mmgr_line_put(line, " (");
    mmgr_line_decimal(line, host_length, 1u);
    mmgr_line_put(line, " bytes on the host, ");
    mmgr_line_decimal(line, device_length, 1u);
    mmgr_line_put(line, " from the device, first differing at byte ");
    mmgr_line_decimal(line, at, 1u);
    mmgr_line_put(line, ")\n    host:   ");
    (void)mmgr_line_put_n(line, &host_text[from],
                          (size_t)(((host_length - from) < 160ull) ? (host_length - from) : 160ull));
    mmgr_line_put(line, "\n    device: ");
    const unsigned long long device_from = (from < device_length) ? from : device_length;
    (void)mmgr_line_put_n(line, &device_text[device_from],
                          (size_t)(((device_length - device_from) < 160ull) ? (device_length - device_from) : 160ull));
}

// a reason a call gives: its bytes and the line over them
struct DeviceWhy
{
    char text[DEVICE_TEST_WHY_BYTES];
    VerbaLinea line;
};

static void device_why_begun(DeviceWhy *why)
{
    why->line = mmgr_line_over(why->text, sizeof(why->text));
}

static const char *device_why_text(DeviceWhy *why)
{
    (void)mmgr_line_finish(&why->line);
    return why->text;
}

// a text begun for a lane
static void device_text_begun(PlatformText *text)
{
    platform_text_begun(text, DEVICE_TEST_TEXT_BYTES, platform_place_g_program_owner, 0);
}

// 1 where two texts hold the same bytes
static int device_texts_same(const PlatformText *left, const PlatformText *right)
{
    const unsigned long long length = platform_text_size(left);
    return (length == platform_text_size(right)) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = left->cell, .other = right->cell, .bytes = (size_t)length) == 0);
}

// the cells of one language's lane of a program, each given back as the lane ends
struct DeviceLane
{
    unsigned int *scratch;
    PlatformArray host_items;
    PlatformArray device_items;
    PlatformText text;
    PlatformText written;
    PlatformText stepped;
};

static void device_lane_begun(DeviceLane *lane)
{
    lane->scratch = NULL;
    platform_array_begun(&lane->host_items, sizeof(MachineInstr), platform_place_g_program_owner, 0);
    platform_array_begun(&lane->device_items, sizeof(MachineInstr), platform_place_g_program_owner, 0);
    device_text_begun(&lane->text);
    device_text_begun(&lane->written);
    device_text_begun(&lane->stepped);
}

static void device_lane_ended(DeviceLane *lane)
{
    platform_cell_released(lane->scratch, platform_place_g_program_owner);
    platform_array_ended(&lane->host_items);
    platform_array_ended(&lane->device_items);
    platform_text_ended(&lane->text);
    platform_text_ended(&lane->written);
    platform_text_ended(&lane->stepped);
}

// The lane's forms on the device against the host's, as the lane runs and split into states by the test's model. 0
// where the host decides no lane for it, which the language then does not support
static int device_forms(DeviceResults *results, const char *lane_name, CodeGenerator *generator,
                        const EngineRecordLayout *layout, DeviceLane *lane, unsigned int *places)
{
    const unsigned int scratch_count = 4u * generator->schema()->form_count;
    if (generator->decided(layout, places, &lane->host_items) == 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, lane_name);
            mmgr_line_put(output_line, ": ");
            mmgr_line_decimal(output_line, layout->steps, 1u);
            mmgr_line_put(output_line,
                          " steps, not supported (a step the lane does not support, or a form breaks the lane)");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        results->unsupported += 1u;
        return 0;
    }
    // the lane as program() writes it: whole, or split where the language always splits it
    ScheduleCosts program_costs;
    const ScheduleCosts *const written_costs =
        generator->program_schedule_costs(&program_costs) ? &program_costs : NULL;
    ScheduleReport written_report = {0u, 0u, 0u};
    DeviceWhy why;
    device_why_begun(&why);
    const int decided = codegen_device_instrs(layout, lane->scratch, scratch_count, *places, written_costs,
                                              &written_report, &lane->device_items, &why.line);
    DeviceWhat what;
    device_what_begun(&what, lane_name);
    mmgr_line_put(&what.line, ": the device decides the host's ");
    mmgr_line_decimal(&what.line, lane->host_items.count, 1u);
    mmgr_line_put(&what.line, " forms");
    device_why(&what, decided, device_why_text(&why));
    device_check(results, decided && device_items_same(&lane->host_items, &lane->device_items), &what);
    // the lane split into states
    const ScheduleModel model = {NULL, 0u, 1u, DEVICE_TEST_BUDGET, 1u};
    ScheduleReport host_report = {0u, 0u, 0u};
    ScheduleReport device_report = {0u, 0u, 0u};
    unsigned int scheduled_places = 0u;
    if (generator->decided(layout, model, &host_report, &scheduled_places, &lane->host_items) != 0)
    {
        const ScheduleCosts costs = generator->schedule_costs(model);
        device_why_begun(&why);
        const int scheduled_decided = codegen_device_instrs(layout, lane->scratch, scratch_count, scheduled_places,
                                                            &costs, &device_report, &lane->device_items, &why.line);
        device_what_begun(&what, lane_name);
        mmgr_line_put(&what.line, " split into ");
        mmgr_line_decimal(&what.line, host_report.states, 1u);
        mmgr_line_put(&what.line, " states: the device splits it as the host does");
        device_why(&what, scheduled_decided, device_why_text(&why));
        device_check(results,
                     scheduled_decided && device_items_same(&lane->host_items, &lane->device_items) &&
                         (device_report.states == host_report.states) && (device_report.max == host_report.max) &&
                         (device_report.over == host_report.over),
                     &what);
    }
    else
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, lane_name);
        mmgr_line_put(output_line, ": the host does not split the lane by the test's schedule model");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 1;
}

// The device's read of the ruleset's file against the host's, and its build of the assembly printer's ruleset against
// `text_rules`
static void device_rules(DeviceResults *results, const char *lane_name, const Ruleset *rules, const TargetInfo *target,
                         const AsmPrinterRuleset *text_rules)
{
    Ruleset device_read{};
    device_read.schema = rules->schema;
    platform_array_begun(&device_read.folds, sizeof(RulesetFold), platform_place_g_program_owner, 0);
    ruleset_released(&device_read);
    DeviceWhy why;
    device_why_begun(&why);
    const int read_ran = ruleset_read_device(&device_read, rules->path, &why.line);
    DeviceWhat what;
    device_what_begun(&what, lane_name);
    mmgr_line_put(&what.line, ": the device reads the ruleset's file as the host does");
    device_why(&what, read_ran, device_why_text(&why));
    device_check(results, read_ran && ruleset_same(&device_read, rules), &what);
    ruleset_released(&device_read);
    AsmPrinterRuleset device_built;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &device_built, .bytes = sizeof(device_built), .val = 0);
    device_why_begun(&why);
    const int rules_built = asm_printer_ruleset_device(rules, target, s_device_header, &device_built, &why.line);
    device_what_begun(&what, lane_name);
    mmgr_line_put(&what.line, ": the device builds the assembly printer's ruleset word for word the host's");
    device_why(&what, rules_built, device_why_text(&why));
    device_check(results, rules_built && asm_printer_ruleset_same(&device_built, text_rules), &what);
    if (rules_built)
    {
        asm_printer_ruleset_release(&device_built);
    }
}

// one language's lane of a program: its forms as the lane runs and split, and its text, the device's checked against
// the host's
static void device_language(DeviceResults *results, const char *name, CodeGenerator *generator, const char *language,
                            const EngineRecordLayout *layout, const LayoutRequest *request)
{
    char lane_name[DEVICE_TEST_WHAT_BYTES];
    VerbaLinea lane_line = mmgr_line_over(lane_name, sizeof(lane_name));
    mmgr_line_put(&lane_line, name);
    mmgr_line_put(&lane_line, " in ");
    mmgr_line_put(&lane_line, language);
    (void)mmgr_line_finish(&lane_line);
    const Ruleset *const rules = generator->ruleset(1);
    if (rules == NULL)
    {
        device_named_check(results, 0, lane_name, ": the ruleset is read");
        return;
    }
    const unsigned int scratch_banks[3] = {REGCLASS_TEMPORARY, REGCLASS_WIDE, REGCLASS_PREDICATE};
    DeviceLane lane;
    device_lane_begun(&lane);
    lane.scratch = ruleset_scratch(rules, scratch_banks);
    unsigned int places = 0u;
    if ((lane.scratch == NULL) || (device_forms(results, lane_name, generator, layout, &lane, &places) == 0))
    {
        device_lane_ended(&lane);
        return;
    }
    // the text, where the assembly printer takes the language's ruleset
    const TargetInfo target = {0ull, 0, 0, 0, 0, ""};
    unsigned int live = 0u;
    (void)generator->program(layout, &target, s_device_header, &lane.text, &places, &live);
    AsmPrinterRuleset text_rules;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &text_rules, .bytes = sizeof(text_rules), .val = 0);
    DeviceWhy why;
    device_why_begun(&why);
    if (asm_printer_ruleset_build(rules, &target, s_device_header, &text_rules, &why.line) == 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, lane_name);
            mmgr_line_put(output_line, ": the assembly printer does not take the ruleset (");
            mmgr_line_put(output_line, device_why_text(&why));
            mmgr_line_put(output_line, ")");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        device_lane_ended(&lane);
        return;
    }
    device_rules(results, lane_name, rules, &target, &text_rules);
    ScheduleCosts program_costs;
    const ScheduleCosts *const written_costs =
        generator->program_schedule_costs(&program_costs) ? &program_costs : NULL;
    const unsigned long long text_length = platform_text_size(&lane.text);
    device_why_begun(&why);
    const int wrote = codegen_device(layout, &text_rules, places, written_costs, &lane.written, &why.line);
    DeviceWhat what;
    device_what_begun(&what, lane_name);
    mmgr_line_put(&what.line, ": the device writes the code generator's ");
    mmgr_line_decimal(&what.line, text_length, 1u);
    mmgr_line_put(&what.line, " bytes from the host's layout");
    device_why(&what, wrote, device_why_text(&why));
    if (wrote)
    {
        device_text_differ(&what, &lane.text, &lane.written);
    }
    device_check(results, wrote && (text_length != 0ull) && device_texts_same(&lane.written, &lane.text), &what);
    device_why_begun(&why);
    const int stepped_wrote =
        codegen_device_steps(request, &text_rules, places, written_costs, &lane.stepped, &why.line);
    device_what_begun(&what, lane_name);
    mmgr_line_put(&what.line, ": the device writes them from its own layout");
    device_why(&what, stepped_wrote, device_why_text(&why));
    if (stepped_wrote)
    {
        device_text_differ(&what, &lane.text, &lane.stepped);
    }
    device_check(results, stepped_wrote && (text_length != 0ull) && device_texts_same(&lane.stepped, &lane.text),
                 &what);
    asm_printer_ruleset_release(&text_rules);
    device_lane_ended(&lane);
}

// the program arranged on the host and on the device, and each language's lane of it
static void device_run(void *context, const HostProgram *program, int reuse, unsigned int *const *atoms,
                       const unsigned long long *bodies, const unsigned int *index, int errors)
{
    (void)atoms;
    (void)bodies;
    (void)index;
    (void)errors;
    DeviceResults *const results = (DeviceResults *)context;
    char name[DEVICE_TEST_WHAT_BYTES];
    VerbaLinea name_line = mmgr_line_over(name, sizeof(name));
    mmgr_line_put(&name_line, program->name);
    mmgr_line_put(&name_line, reuse ? " (registers reused)" : "");
    (void)mmgr_line_finish(&name_line);
    HostLoaded loaded;
    if (host_load(program, reuse, &loaded) == 0)
    {
        device_named_check(results, 0, name, " is arranged on the host");
        return;
    }
    LayoutRequest request{};
    request.steps = program->steps;
    request.count = program->count;
    request.field_bits = program->field_bits;
    request.field_offset = program->field_offset;
    request.fields = program->fields;
    request.members = program->members;
    request.in_limbs = program->in_limbs;
    request.outputs = program->outputs;
    request.output_count = program->output_count;
    request.tables = (program->table_count != 0u) ? program->tables : NULL;
    request.table_count = program->table_count;
    request.reuse = reuse;
    EngineRecordLayout device_layout{};
    DeviceWhy why;
    device_why_begun(&why);
    const int device_built = layout_device(&request, &device_layout, &why.line);
    DeviceWhat what;
    device_what_begun(&what, name);
    mmgr_line_put(&what.line, ": the device arranges the host's ");
    mmgr_line_decimal(&what.line, loaded.layout.steps, 1u);
    mmgr_line_put(&what.line, " steps and ");
    mmgr_line_decimal(&what.line, loaded.layout.file_limbs, 1u);
    mmgr_line_put(&what.line, " limbs of file word for word");
    device_why(&what, device_built, device_why_text(&why));
    device_check(results, device_built && layout_same(&device_layout, &loaded.layout), &what);
    if (device_built)
    {
        key_schedule_record_release(&device_layout);
    }
    CodeGenerator *const generators[DEVICE_TEST_LANGUAGES] = {&code_generator("ptx.krs"), &code_generator("c.krs"),
                                                              &code_generator("vhdl.krs")};
    const char *const languages[DEVICE_TEST_LANGUAGES] = {"PTX", "C", "VHDL"};
    for (unsigned int language = 0u; language < DEVICE_TEST_LANGUAGES; language += 1u)
    {
        device_language(results, name, generators[language], languages[language], &loaded.layout, &request);
    }
    host_free(&loaded);
}

// The bootstrap's fixed point: the assembly printer's own lane in a language, written by the device with the text
// program from the step table the device arranged, must be the code generator's text byte for byte: the assembly
// printer writes itself
static void device_bootstrap(DeviceResults *results, CodeGenerator *generator, const char *language)
{
    char lane_name[DEVICE_TEST_WHAT_BYTES];
    VerbaLinea lane_line = mmgr_line_over(lane_name, sizeof(lane_name));
    mmgr_line_put(&lane_line, "the assembly printer's own lane in ");
    mmgr_line_put(&lane_line, language);
    (void)mmgr_line_finish(&lane_line);
    const Ruleset *const rules = generator->ruleset(1);
    const TargetInfo target = {0ull, 0, 0, 0, 0, ""};
    AsmPrinterRuleset text_rules;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &text_rules, .bytes = sizeof(text_rules), .val = 0);
    DeviceWhy why;
    device_why_begun(&why);
    if ((rules == NULL) || (asm_printer_ruleset_build(rules, &target, s_device_header, &text_rules, &why.line) == 0))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, lane_name);
            mmgr_line_put(output_line, ": the assembly printer does not take the ruleset (");
            mmgr_line_put(output_line, (rules == NULL) ? "it is not read" : device_why_text(&why));
            mmgr_line_put(output_line, ")");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return;
    }
    const AsmPrinterProgram &program = text_rules.program;
    const unsigned int in_limbs[ENGINE_RECORD_MEMBERS_MAX] = {ASM_PRINTER_RECORD_LIMBS, 0u, 0u};
    LayoutRequest request{};
    request.steps = program.steps;
    // the assembly printer's steps are a few hundred, and its fields and tables a handful
    request.count = program.step_count;
    request.field_bits = program.field_bits;
    request.field_offset = program.field_offset;
    request.fields = ASM_PRINTER_FIELDS;
    request.members = 1u;
    request.in_limbs = in_limbs;
    request.outputs = &program.output;
    request.output_count = 1u;
    request.tables = program.tables;
    request.table_count = ASM_PRINTER_PROGRAM_TABLES;
    request.reuse = 1;
    unsigned int places = 0u;
    unsigned int live = 0u;
    PlatformText text;
    PlatformText written;
    device_text_begun(&text);
    device_text_begun(&written);
    (void)generator->program(&program.layout, &target, s_device_header, &text, &places, &live);
    const unsigned long long text_length = platform_text_size(&text);
    device_named_check(results, (text_length == 0ull) || device_claims_well_formed(generator->register_claims()),
                       lane_name, ": every register the lane uses is claimed and released by its items");
    ScheduleCosts program_costs;
    const ScheduleCosts *const written_costs =
        generator->program_schedule_costs(&program_costs) ? &program_costs : NULL;
    device_why_begun(&why);
    const int wrote = codegen_device_steps(&request, &text_rules, places, written_costs, &written, &why.line);
    DeviceWhat what;
    device_what_begun(&what, lane_name);
    mmgr_line_put(&what.line, ", ");
    mmgr_line_decimal(&what.line, program.layout.steps, 1u);
    mmgr_line_put(&what.line, " steps: the device writes it with the text program, the code generator's ");
    mmgr_line_decimal(&what.line, text_length, 1u);
    mmgr_line_put(&what.line, " bytes");
    device_why(&what, wrote, device_why_text(&why));
    if (wrote)
    {
        device_text_differ(&what, &text, &written);
    }
    device_check(results, wrote && (text_length != 0ull) && device_texts_same(&written, &text), &what);
    platform_text_ended(&written);
    platform_text_ended(&text);
    asm_printer_ruleset_release(&text_rules);
}

int program_main(int count, char **arguments)
{
    DeviceResults results = {0u, 0u, 0u};
    char job_capacity[SIM_LINE_CAPACITY];
    SimResults job;
    sim_open(&job, job_capacity);
    const int admitted = sim_job_submit(&job, "codegen_device_test", count, arguments, DEVICE_TEST_DECLARED);
    if (admitted != 0)
    {
        record_image_programs(&results, device_run);
        device_bootstrap(&results, &code_generator("ptx.krs"), "PTX");
        device_bootstrap(&results, &code_generator("c.krs"), "C");
        device_bootstrap(&results, &code_generator("vhdl.krs"), "VHDL");
    }
    sim_job_release(&job);
    sim_flush(&job);
    device_named_check(&results, (admitted != 0) && (job.failures == 0ull),
                       "tessera: the device's daemon admits the test's job and it releases", "");
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  codegen device test: ");
        mmgr_line_decimal(output_line, results.checks, 1u);
        mmgr_line_put(output_line, " checks, ");
        mmgr_line_decimal(output_line, results.failed, 1u);
        mmgr_line_put(output_line, " failed, ");
        mmgr_line_decimal(output_line, results.unsupported, 1u);
        mmgr_line_put(output_line, " lanes not supported");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return (results.failed == 0u) ? 0 : 1;
}
