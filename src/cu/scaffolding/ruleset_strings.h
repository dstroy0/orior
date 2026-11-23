// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ruleset_strings.h: the ruleset's forms, banks, fixed registers and parameters by name, as strings, for the tools
// outside the engine that hold their texts as strings. Each call reads the ruleset through the engine's own calls
// (target.h, ruleset_reader.h, ruleset_flat.h)
#ifndef RULESET_STRINGS_H
#define RULESET_STRINGS_H

#include "../../../includes/file_defs/krs/ruleset_flat.h"
#include "../../../includes/file_defs/krs/ruleset_reader.h"
#include "../engine/rmc/asm_printer.h"
#include "../engine/rmc/code_generator.h"
#include "../engine/rmc/target_internal.h"
#include "cellularum_laboro/cellularum_laboro.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

// the bytes of one register's written form, its terminator among them
#define RULESET_STRINGS_REGISTER_BYTES 256u

// register `number` of the bank `bank`; empty where the ruleset has no bank of that name
static inline std::string ruleset_register(const Ruleset *rules, const std::string &bank, unsigned int number)
{
    char written[RULESET_STRINGS_REGISTER_BYTES];
    VerbaLinea line = mmgr_line_over(written, sizeof(written));
    const int named = ruleset_register_written(rules, bank.c_str(), number, &line);
    (void)mmgr_line_finish(&line);
    return (named != 0) ? std::string(written) : std::string();
}

// the register every lane uses throughout named `name`; empty where the ruleset holds none of that name
static inline std::string ruleset_physreg(const Ruleset *rules, const std::string &name)
{
    const char *const fixed = ruleset_fixed_named(rules, name.c_str());
    return (fixed != NULL) ? std::string(fixed) : std::string();
}

// the scratch of a writing by its bank's name, as a string, and the answer the engine's scratch takes
struct RulesetStringsScratch
{
    const std::function<std::string(const std::string &bank)> *scratch;
};

static inline int ruleset_strings_scratch(void *context, const char *bank, VerbaLinea *name)
{
    const RulesetStringsScratch *const by_name = (const RulesetStringsScratch *)context;
    const std::string taken = (*by_name->scratch)(std::string(bank));
    mmgr_line_put(name, taken.c_str());
    return taken.empty() ? 0 : 1;
}

// Form `name` appended to `text`, its arguments in the order of its parameters. 0, and nothing written, where the
// ruleset writes no form of that name or the form takes another count of arguments. Also 0 where `scratch` answers
// empty for a scratch register of a construct
static inline int ruleset_opcode(const Ruleset *rules, const std::string &name,
                                 const std::vector<std::string> &arguments,
                                 const std::function<std::string(const std::string &bank)> &scratch, std::string &text)
{
    std::vector<const char *> given;
    for (const std::string &argument : arguments)
    {
        given.push_back(argument.c_str());
    }
    RulesetStringsScratch context = {&scratch};
    const RulesetNamedScratch by_name = {ruleset_strings_scratch, &context};
    PlatformText written;
    platform_text_begun(&written, RULESET_FILE_MAX, platform_place_g_program_owner, 0);
    // a form takes far fewer than 2^32 arguments
    const int ok =
        ruleset_form_written(rules, name.c_str(), given.data(), (unsigned int)given.size(), &by_name, &written.line) &&
        (mmgr_line_ok(&written.line) != 0);
    if (ok != 0)
    {
        text.append(platform_text_bytes(&written), (size_t)platform_text_size(&written));
    }
    platform_text_ended(&written);
    return ok;
}

// the words of `line` split at white space
static inline std::vector<std::string> ruleset_strings_words(const std::string &line)
{
    std::vector<std::string> words;
    std::string word;
    for (const char character : line + " ")
    {
        if ((character == ' ') || (character == '\t') || (character == '\r') || (character == '\n'))
        {
            if (!word.empty())
            {
                words.push_back(word);
            }
            word.clear();
            continue;
        }
        word += character;
    }
    return words;
}

// the names of form `form`'s parameters in the order the ruleset's own line for it gives them. None where it gives no
// line for the form
static inline std::vector<std::string> ruleset_parameters(const Ruleset *rules, const std::string &form)
{
    std::vector<std::string> names;
    PlatformText read;
    ruleset_text_begun(&read);
    char why[RULESET_ERROR_BYTES];
    VerbaLinea error = mmgr_line_over(why, sizeof(why));
    (void)ruleset_text(rules->schema, rules->path, &read, &error);
    const std::string text(platform_text_bytes(&read), (size_t)platform_text_size(&read));
    platform_text_ended(&read);
    size_t at = 0u;
    while (at < text.size())
    {
        const size_t end = (text.find('\n', at) == std::string::npos) ? text.size() : text.find('\n', at);
        const std::vector<std::string> words = ruleset_strings_words(text.substr(at, end - at));
        at = end + 1u;
        const int named =
            (words.size() >= 2u) && (words[1] == form) &&
            ((words[0] == "form") || (words[0] == "err") || (words[0] == "nop") || (words[0] == "construct"));
        for (size_t word = 2u; (named != 0) && (word < words.size()) && (words[word] != "="); word += 1u)
        {
            names.push_back(words[word]);
        }
        if (named != 0)
        {
            break;
        }
    }
    return names;
}

// the paths of the ruleset at `path` as strings, in the order ruleset_text reads them (ruleset_paths)
static inline unsigned int ruleset_strings_paths(const RulesetSchema *schema, const std::string &path,
                                                 std::string paths[RULESET_PATHS_MAX])
{
    char found[RULESET_PATHS_MAX][PLATFORM_PATH_BYTES];
    const unsigned int count = ruleset_paths(schema, path.c_str(), found);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        paths[at] = found[at];
    }
    return count;
}

// the folder of the rulesets as a string, empty where it does not fit (ruleset_folder)
static inline std::string ruleset_strings_folder(void)
{
    char folder[PLATFORM_PATH_BYTES];
    return (ruleset_folder(folder, sizeof(folder)) != 0) ? std::string(folder) : std::string();
}

// the ruleset at `path` read whole into `text` as a string, with why in `error` where a read did not succeed
// (ruleset_text)
static inline int ruleset_text_strings(const RulesetSchema *schema, const std::string &path, std::string *text,
                                       std::string *error)
{
    PlatformText read;
    ruleset_text_begun(&read);
    char why[RULESET_ERROR_BYTES];
    VerbaLinea line = mmgr_line_over(why, sizeof(why));
    const int ok = ruleset_text(schema, path.c_str(), &read, &line);
    (void)mmgr_line_finish(&line);
    *text = std::string(platform_text_bytes(&read), (size_t)platform_text_size(&read));
    *error = why;
    platform_text_ended(&read);
    return ok;
}

// the file at `path` read whole onto the end of `text` as a string, as the run stores it (ruleset_bytes_read)
static inline int ruleset_bytes_strings(const std::string &path, std::string *text)
{
    PlatformText read;
    ruleset_text_begun(&read);
    const int ok = ruleset_bytes_read(path.c_str(), &read);
    text->append(platform_text_bytes(&read), (size_t)platform_text_size(&read));
    platform_text_ended(&read);
    return ok;
}

// the files at `paths`, `count` of them, read against each other as strings, the path of one that did not open in
// `error` (ruleset_relations_read)
static inline int ruleset_relations_strings(const std::string *paths, unsigned int count,
                                            const RulesetCoreSchema *schema, RulesetRelationMemory *memory,
                                            RulesetCoreRelations *relations, std::string *error)
{
    std::vector<char> held((size_t)count * PLATFORM_PATH_BYTES, '\0');
    char (*const given)[PLATFORM_PATH_BYTES] = (char (*)[PLATFORM_PATH_BYTES])held.data();
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        (void)EMBED_CALL(cellul.copy, CatenaFinitaCfg, .src = paths[at].c_str(), .cap = PLATFORM_PATH_BYTES,
                         .dst = given[at]);
    }
    char why[RULESET_ERROR_BYTES];
    VerbaLinea line = mmgr_line_over(why, sizeof(why));
    const int ok = ruleset_relations_read(given, count, schema, memory, relations, &line);
    (void)mmgr_line_finish(&line);
    *error = why;
    return ok;
}

// the bytes a lane's text holds at max
#define RULESET_STRINGS_LANE_BYTES (1ULL << 30u)

// a program's lane in `generator`'s language under `header` as a string, empty where it writes none (Target::program)
static inline std::string lane_program_text(Target &generator, const EngineRecordLayout *layout,
                                            const TargetInfo *target, const std::string &header, unsigned int *places,
                                            unsigned int *live)
{
    PlatformText text;
    platform_text_begun(&text, RULESET_STRINGS_LANE_BYTES, platform_place_g_program_owner, 0);
    const int wrote = generator.program(layout, target, header.c_str(), &text, places, live);
    const std::string lane =
        (wrote != 0) ? std::string(platform_text_bytes(&text), (size_t)platform_text_size(&text)) : std::string();
    platform_text_ended(&text);
    return lane;
}

// the lane split into states by `model` as a string, empty where it writes none (CodeGenerator::scheduled)
static inline std::string lane_scheduled_text(CodeGenerator &generator, const EngineRecordLayout *layout,
                                              const TargetInfo *target, const std::string &header, unsigned int *places,
                                              unsigned int *live, const ScheduleModel &model, ScheduleReport *report)
{
    PlatformText text;
    platform_text_begun(&text, RULESET_STRINGS_LANE_BYTES, platform_place_g_program_owner, 0);
    const int wrote = generator.scheduled(layout, target, header.c_str(), &text, places, live, model, report);
    const std::string lane =
        (wrote != 0) ? std::string(platform_text_bytes(&text), (size_t)platform_text_size(&text)) : std::string();
    platform_text_ended(&text);
    return lane;
}

// the items of `decided` copied into `items`, and the array given back
static inline void lane_items_taken(PlatformArray *decided, std::vector<MachineInstr> *items)
{
    const MachineInstr *const first = (const MachineInstr *)decided->cell;
    items->assign(first, first + decided->count);
    platform_array_ended(decided);
}

// the forms program() writes the lane in, into `items` (CodeGenerator::decided)
static inline int lane_decided_items(CodeGenerator &generator, const EngineRecordLayout *layout, unsigned int *places,
                                     std::vector<MachineInstr> *items)
{
    PlatformArray decided;
    platform_array_begun(&decided, sizeof(MachineInstr), platform_place_g_program_owner, 0);
    const int ok = generator.decided(layout, places, &decided);
    lane_items_taken(&decided, items);
    return ok;
}

// the forms scheduled() writes the lane in, into `items` (CodeGenerator::decided)
static inline int lane_decided_items(CodeGenerator &generator, const EngineRecordLayout *layout,
                                     const ScheduleModel &model, ScheduleReport *report, unsigned int *places,
                                     std::vector<MachineInstr> *items)
{
    PlatformArray decided;
    platform_array_begun(&decided, sizeof(MachineInstr), platform_place_g_program_owner, 0);
    const int ok = generator.decided(layout, model, report, places, &decided);
    lane_items_taken(&decided, items);
    return ok;
}

// a schedule model from a map of each form's cost by its name, which holds the costs it points at
struct ScheduleModelStrings
{
    std::vector<ScheduleCost> costs;
    ScheduleModel model;
};

// `costs`, by each form's name, as the model that splits a lane. `costs` must outlive the model, which points at its
// names
static inline void schedule_model_strings(const std::map<std::string, unsigned int> &costs, unsigned int other,
                                          unsigned int budget, unsigned int writes, ScheduleModelStrings *made)
{
    made->costs.clear();
    for (const auto &cost : costs)
    {
        made->costs.push_back(ScheduleCost{cost.first.c_str(), cost.second});
    }
    // a construction set names far fewer than 2^32 forms
    made->model = ScheduleModel{made->costs.data(), (unsigned int)made->costs.size(), other, budget, writes};
}

// the bytes of a reason a call gives, its terminator among them
#define RULESET_STRINGS_ERROR_BYTES 1024u

// the assembly printer built for `rules`, with why in `error` as a string where it is not (asm_printer_ruleset_build)
static inline int asm_printer_build_strings(const Ruleset *rules, const TargetInfo *target, const std::string &header,
                                            AsmPrinterRuleset *text_rules, std::string *error)
{
    char why[RULESET_STRINGS_ERROR_BYTES];
    VerbaLinea line = mmgr_line_over(why, sizeof(why));
    const int built = asm_printer_ruleset_build(rules, target, header.c_str(), text_rules, &line);
    (void)mmgr_line_finish(&line);
    *error = why;
    return built;
}

// the host oracle's text of `items` into `text`, with why in `error` as a string where it did not run
// (asm_printer_host)
static inline int asm_printer_host_strings(const AsmPrinterRuleset *text_rules, const std::vector<MachineInstr> &items,
                                           std::string *text, std::string *error)
{
    char why[RULESET_STRINGS_ERROR_BYTES];
    VerbaLinea line = mmgr_line_over(why, sizeof(why));
    PlatformText written;
    platform_text_begun(&written, RULESET_STRINGS_LANE_BYTES, platform_place_g_program_owner, 0);
    const int ran = asm_printer_host(text_rules, items.data(), items.size(), &written, &line);
    (void)mmgr_line_finish(&line);
    *error = why;
    *text =
        (ran != 0) ? std::string(platform_text_bytes(&written), (size_t)platform_text_size(&written)) : std::string();
    platform_text_ended(&written);
    return ran;
}

#endif
