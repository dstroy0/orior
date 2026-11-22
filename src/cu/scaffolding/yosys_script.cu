// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "yosys_script.h"

#include "../../../includes/file_defs/krs/ruleset_reader.h"

// the script's forms, by their places in the schema
enum YosysForm
{
    YOSYS_READ_SOURCE = 0,
    YOSYS_COARSE = 1,
    YOSYS_MEMORIES = 2,
    YOSYS_FINE = 3,
    YOSYS_MEASURE = 4,
    YOSYS_FORM_COUNT = 5
};

static const RulesetName s_yosys_forms[YOSYS_FORM_COUNT] = {
    {"read_source", 1u}, {"coarse", 1u}, {"memories", 1u}, {"fine", 0u}, {"measure", 0u}};

// the script's forms and no bank or fixed register, read from yosys.krs, which Yosys reads and whose opening line is
// the ruleset's own read_source
static const RulesetSchema s_yosys_schema = {
    "yosys.krs", "yosys", "read_source", s_yosys_forms, YOSYS_FORM_COUNT, NULL, 0u, NULL, 0u};

YosysScript::YosysScript(void) : Target(&s_yosys_schema)
{
}

int YosysScript::program(const EngineRecordLayout *layout, const TargetInfo *target, const char *header,
                         PlatformText *text, unsigned int *places, unsigned int *live)
{
    (void)layout;
    (void)target;
    (void)header;
    platform_text_reset(text);
    *places = 0u;
    *live = 0u;
    return 0;
}

// the bytes a script holds at max
#define YOSYS_SCRIPT_BYTES (1ULL << 20u)

std::string YosysScript::synthesis(const std::string &source, const std::string &top, const std::string &memories)
{
    const Ruleset *const rules = ruleset(1);
    if (rules == NULL)
    {
        return std::string();
    }
    PlatformText text;
    platform_text_begun(&text, YOSYS_SCRIPT_BYTES, platform_place_g_program_owner, 0);
    int broken = 0;
    const char *const source_argument[1] = {source.c_str()};
    const char *const top_argument[1] = {top.c_str()};
    const char *const memories_argument[1] = {memories.c_str()};
    ruleset_write_taking(rules, &text.line, YOSYS_READ_SOURCE, source_argument, 1u, NULL, &broken);
    ruleset_write_taking(rules, &text.line, YOSYS_COARSE, top_argument, 1u, NULL, &broken);
    if (!memories.empty())
    {
        ruleset_write_taking(rules, &text.line, YOSYS_MEMORIES, memories_argument, 1u, NULL, &broken);
    }
    ruleset_write_taking(rules, &text.line, YOSYS_FINE, NULL, 0u, NULL, &broken);
    ruleset_write_taking(rules, &text.line, YOSYS_MEASURE, NULL, 0u, NULL, &broken);
    const std::string script = ((broken == 0) && (mmgr_line_ok(&text.line) != 0))
                                   ? std::string(platform_text_bytes(&text), (size_t)platform_text_size(&text))
                                   : std::string();
    platform_text_ended(&text);
    return script;
}

YosysScript &yosys_script(void)
{
    static YosysScript script;
    return script;
}
