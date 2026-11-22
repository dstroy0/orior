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

int YosysScript::synthesis(const char *source, const char *top, const char *memories, PlatformText *script)
{
    const Ruleset *const rules = ruleset(1);
    if (rules == NULL)
    {
        return 0;
    }
    int broken = 0;
    const char *const source_argument[1] = {source};
    const char *const top_argument[1] = {top};
    const char *const memories_argument[1] = {memories};
    ruleset_write_taking(rules, &script->line, YOSYS_READ_SOURCE, source_argument, 1u, NULL, &broken);
    ruleset_write_taking(rules, &script->line, YOSYS_COARSE, top_argument, 1u, NULL, &broken);
    if (memories[0] != '\0')
    {
        ruleset_write_taking(rules, &script->line, YOSYS_MEMORIES, memories_argument, 1u, NULL, &broken);
    }
    ruleset_write_taking(rules, &script->line, YOSYS_FINE, NULL, 0u, NULL, &broken);
    ruleset_write_taking(rules, &script->line, YOSYS_MEASURE, NULL, 0u, NULL, &broken);
    return ((broken == 0) && (mmgr_line_ok(&script->line) != 0)) ? 1 : 0;
}

YosysScript &yosys_script(void)
{
    static YosysScript script;
    return script;
}
