// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// testing.c: a tree's tests as the window lists them, each written as the window reads one; the rules a tree gives
// pytest, read as pytest reads them, a setting's values split at spaces and commas with a TOML list's brackets and
// quotes taken off, and a setting carried on to the lines indented under it; and a name matched to a rule
#include "testing.h"
#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../text/text.h"

void cli_test_write(const CliTestingCfg *args)
{
    mmgr_line_put(args->out, "{\"id\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)args->test->id,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->test->id, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(args->out, ",\"file\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)args->test->file,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->test->file, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(args->out, ",\"class\":");
    if (args->test->class_name == NULL)
    {
        mmgr_line_put(args->out, "null");
    }
    else
    {
        EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
                   .text_bytes = (const unsigned char *)args->test->class_name,
                   .length = cellul.len(&(CatenaFinitaCfg){.src = args->test->class_name, .cap = CLI_CELLS_POOL}));
    }
    mmgr_line_put(args->out, ",\"name\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)args->test->name,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->test->name, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(args->out, ",\"line\":");
    mmgr_line_decimal(args->out, args->test->line, 1u);
    mmgr_line_ch(args->out, '}');
}

typedef struct
{
    CliTestRules *rules;
    CliTestRule *rule;
    const char *value;
    unsigned long long size;
    char letter;
    const char *raw;
    const char *key;
    unsigned long long key_size;
    const char *text;
    unsigned long long *from;
    unsigned long long *to;
} TestingCtx;
// `size` bytes of `value` put on `rule`, where they fit
EMBED_INLINE void testing_value_append(const TestingCtx *args)
{
    if ((args->rule->count == CLI_TEST_VALUES) || (args->size + 1u > CLI_TEST_RULES_TEXT - args->rules->used))
    {
        return;
    }
    args->rule->value[args->rule->count] = args->rules->used;
    args->rule->count += 1u;
    memor.cpy(
        &(MemoriaCfg){.dst = args->rules->text + args->rules->used, .src = args->value, .bytes = (size_t)args->size});
    args->rules->used += (unsigned int)args->size;
    args->rules->text[args->rules->used] = '\0';
    args->rules->used += 1u;
}

void cli_test_rules_own(const CliTestingRulesCfg *args)
{
    memor.set(&(MemoriaCfg){.dst = args->rules, .val = 0, .bytes = sizeof(*args->rules)});
    EMBED_CALL(testing_value_append, TestingCtx, .rules = args->rules, .rule = &args->rules->files,
               .value = "test_*.py", .size = 9u);
    EMBED_CALL(testing_value_append, TestingCtx, .rules = args->rules, .rule = &args->rules->files,
               .value = "*_test.py", .size = 9u);
    EMBED_CALL(testing_value_append, TestingCtx, .rules = args->rules, .rule = &args->rules->files, .value = "test*.py",
               .size = 8u);
    EMBED_CALL(testing_value_append, TestingCtx, .rules = args->rules, .rule = &args->rules->classes, .value = "Test",
               .size = 4u);
    EMBED_CALL(testing_value_append, TestingCtx, .rules = args->rules, .rule = &args->rules->functions, .value = "test",
               .size = 4u);
}

const char *cli_test_rule_value(const CliTestingRulesRuleCfg *args)
{
    return args->rules_read->text + args->rule->value[args->at];
}

// whether a byte is a space of a line
EMBED_INLINE int testing_space(const TestingCtx *args)
{
    return (args->letter == ' ') || (args->letter == '\t') || (args->letter == '\r') || (args->letter == '\n') ||
           (args->letter == '\f') || (args->letter == '\v');
}

// the values `size` bytes of a setting give put on `rule` in place of what it contained
EMBED_INLINE void testing_values_get(const TestingCtx *args)
{
    unsigned long long size = args->size;
    args->rule->count = 0u;
    unsigned long long start = 0ULL;
    unsigned long long end = size;
    start = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = args->raw, .at = start,
                       .size = end, .set = "[");
    end = (start + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->raw + start,
                              .at = end - start, .set = "]"));
    for (unsigned long long at = start; at <= end; at += 1u)
    {
        if ((at < end) && !EMBED_CALL(testing_space, TestingCtx, .letter = args->raw[at]) && (args->raw[at] != ','))
        {
            continue;
        }
        unsigned long long from = start;
        unsigned long long to = at;
        from = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = args->raw, .at = from,
                          .size = to, .set = CLI_TEXT_WHITE "\"'");
        to = from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = args->raw + from,
                               .at = to - from, .set = CLI_TEXT_WHITE "\"'");
        if (to > from)
        {
            EMBED_CALL(testing_value_append, TestingCtx, .rules = args->rules, .rule = args->rule,
                       .value = args->raw + from, .size = to - from);
        }
        start = at + 1u;
    }
}

// the setting `key` set from its value
EMBED_INLINE void testing_set(const TestingCtx *args)
{
    static const char *const s_keys[4] = {"testpaths", "python_files", "python_classes", "python_functions"};
    CliTestRule *const rule[4] = {&args->rules->paths, &args->rules->files, &args->rules->classes,
                                  &args->rules->functions};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_keys[at], .cap = CLI_CELLS_POOL}) == args->key_size) &&
            (memor.cmp(&(MemoriaCfg){.src = s_keys[at], .other = args->key, .bytes = (size_t)args->key_size}) == 0))
        {
            EMBED_CALL(testing_values_get, TestingCtx, .rules = args->rules, .rule = rule[at], .raw = args->value,
                       .size = args->size);
        }
    }
}

// the bounds of `size` bytes of `text` with the spaces at both ends taken off
EMBED_INLINE void testing_trim(const TestingCtx *args)
{
    unsigned long long *from = args->from;
    unsigned long long *to = args->to;
    *from = 0ULL;
    *to = args->size;
    while ((*from < *to) && EMBED_CALL(testing_space, TestingCtx, .letter = args->text[*from]))
    {
        *from += 1u;
    }
    while ((*to > *from) && EMBED_CALL(testing_space, TestingCtx, .letter = args->text[*to - 1u]))
    {
        *to -= 1u;
    }
}

int cli_test_rules_in(const CliTestingRulesCfg *args)
{
    cli_testing_rules.test_rules_own(args);
    static char s_key[256];
    static char s_value[8192];
    VerbaLinea value = {s_value, sizeof(s_value), 0ULL};
    unsigned long long key_size = 0ULL;
    int keyed = 0;
    int inside = 0;
    int found = 0;
    const unsigned long long section_size = cellul.len(&(CatenaFinitaCfg){.src = args->section, .cap = CLI_CELLS_POOL});
    for (unsigned long long start = 0ULL; start < args->size;)
    {
        const unsigned long long end =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = args->text, .at = start, .size = args->size).stop;
        const char *const line = args->text + start;
        const unsigned long long line_size =
            ((end > start) && (args->text[end - 1u] == '\r')) ? end - start - 1u : end - start;
        start = end + 1u;
        unsigned long long from = 0ULL;
        unsigned long long to = 0ULL;
        EMBED_CALL(testing_trim, TestingCtx, .text = line, .size = line_size, .from = &from, .to = &to);
        const char *const trimmed = line + from;
        const unsigned long long trimmed_size = to - from;
        const int indented = (line_size > 0u) && ((line[0] == ' ') || (line[0] == '\t'));
        const int list_entries = keyed && (trimmed_size >= 2u) && (trimmed[0] == '[') && (trimmed[1] == '"');
        if ((trimmed_size > 0u) && (trimmed[0] == '[') && !indented && !list_entries)
        {
            if (keyed)
            {
                EMBED_CALL(testing_set, TestingCtx, .rules = args->rules, .key = s_key, .key_size = key_size,
                           .value = s_value, .size = value.at);
                keyed = 0;
            }
            inside =
                (trimmed_size == section_size) &&
                (memor.cmp(&(MemoriaCfg){.src = trimmed, .other = args->section, .bytes = (size_t)section_size}) == 0);
            found |= inside;
            continue;
        }
        if (!inside || ((trimmed_size > 0u) && ((trimmed[0] == '#') || (trimmed[0] == ';'))))
        {
            continue;
        }
        if (indented && keyed)
        {
            mmgr_line_ch(&value, ' ');
            mmgr_line_put_n(&value, trimmed, trimmed_size);
            continue;
        }
        if (keyed)
        {
            EMBED_CALL(testing_set, TestingCtx, .rules = args->rules, .key = s_key, .key_size = key_size,
                       .value = s_value, .size = value.at);
            keyed = 0;
        }
        const char *const equals = memor.chr(&(MemoriaCfg){.src = trimmed, .val = '=', .bytes = (size_t)trimmed_size});
        if (equals != NULL)
        {
            const unsigned long long name_size = (unsigned long long)(equals - trimmed);
            unsigned long long name_from = 0ULL;
            unsigned long long name_to = 0ULL;
            EMBED_CALL(testing_trim, TestingCtx, .text = trimmed, .size = name_size, .from = &name_from,
                       .to = &name_to);
            unsigned long long remainder_from = 0ULL;
            unsigned long long remainder_to = 0ULL;
            EMBED_CALL(testing_trim, TestingCtx, .text = equals + 1, .size = trimmed_size - name_size - 1u,
                       .from = &remainder_from, .to = &remainder_to);
            key_size = (name_to - name_from < sizeof(s_key)) ? name_to - name_from : 0ULL;
            memor.cpy(&(MemoriaCfg){.dst = s_key, .src = trimmed + name_from, .bytes = (size_t)key_size});
            value.at = 0ULL;
            mmgr_line_put_n(&value, equals + 1 + remainder_from, remainder_to - remainder_from);
            keyed = 1;
        }
    }
    if (keyed)
    {
        EMBED_CALL(testing_set, TestingCtx, .rules = args->rules, .key = s_key, .key_size = key_size, .value = s_value,
                   .size = value.at);
    }
    return found;
}

void cli_test_rules_get(const CliTestingRulesCfg *args)
{
    static const char *const s_files[4] = {"pytest.ini", "pyproject.toml", "tox.ini", "setup.cfg"};
    static const char *const s_sections[4] = {"[pytest]", "[tool.pytest.ini_options]", "[pytest]", "[tool:pytest]"};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        char path[CLI_PATH_MAX_LEN];
        EMBED_CALL(cli_root_internal.full, CliRootCtx, .root = args->root, .relative = s_files[at], .joined = path);
        unsigned long long size = 0ULL;
        const unsigned char *const bytes = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                      .size_pointer = &size, .owner = &g_cli_tests_cells);
        if (bytes == NULL)
        {
            continue;
        }
        const char *const text = (const char *)bytes;
        const int read = EMBED_CALL(cli_json_tree_size.utf8_valid, CliJsonTreeSizeCfg, .bytes = text, .size = size) &&
                         EMBED_CALL(cli_testing_rules.test_rules_in, CliTestingRulesCfg, .text = text, .size = size,
                                    .section = s_sections[at], .rules = args->rules);
        EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = bytes, .owner = &g_cli_tests_cells);
        if (read)
        {
            return;
        }
    }
    cli_testing_rules.test_rules_own(args);
}

// the bytes the letter of UTF-8 at `text` takes, of the `size` left
EMBED_INLINE unsigned long long testing_letter(const TestingCtx *args)
{
    const unsigned char lead = (unsigned char)args->text[0];
    unsigned long long want;
    if (lead < 0xc0u)
    {
        want = 1u;
    }
    else if (lead < 0xe0u)
    {
        want = 2u;
    }
    else if (lead < 0xf0u)
    {
        want = 3u;
    }
    else
    {
        want = 4u;
    }
    return (want < args->size) ? want : args->size;
}

int cli_test_glob(const CliTestingSizeCfg *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->pattern, .cap = CLI_CELLS_POOL});
    unsigned long long matched = 0ULL;
    unsigned long long at = 0ULL;
    unsigned long long star = length;
    unsigned long long mark = 0ULL;
    while (at < args->size)
    {
        const unsigned long long letter =
            EMBED_CALL(testing_letter, TestingCtx, .text = args->name + at, .size = args->size - at);
        const unsigned long long own =
            (matched < length)
                ? EMBED_CALL(testing_letter, TestingCtx, .text = args->pattern + matched, .size = length - matched)
                : 0ULL;
        if ((matched < length) && ((args->pattern[matched] == '?') ||
                                   ((own == letter) && (memor.cmp(&(MemoriaCfg){.src = args->pattern + matched,
                                                                                .other = args->name + at,
                                                                                .bytes = (size_t)own}) == 0))))
        {
            matched += (args->pattern[matched] == '?') ? 1u : own;
            at += letter;
        }
        else if ((matched < length) && (args->pattern[matched] == '*'))
        {
            star = matched;
            mark = at;
            matched += 1u;
        }
        else if (star < length)
        {
            matched = star + 1u;
            mark += EMBED_CALL(testing_letter, TestingCtx, .text = args->name + mark, .size = args->size - mark);
            at = mark;
        }
        else
        {
            return 0;
        }
    }
    matched = EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = args->pattern,
                         .at = matched, .size = length, .set = "*");
    return matched == length;
}

int cli_test_by_name(const CliTestingRulesRuleSizeCfg *args)
{
    for (unsigned int at = 0u; at < args->rule->count; at += 1u)
    {
        const char *const value = EMBED_CALL(cli_testing_rules_rule.test_rule_value, CliTestingRulesRuleCfg,
                                             .rules_read = args->rules_read, .rule = args->rule, .at = at);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL});
        const int globbed =
            (cellul.chr(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL, .byte = '*'}) != NULL) ||
            (cellul.chr(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL, .byte = '?'}) != NULL);
        if (globbed ? EMBED_CALL(cli_testing_size.test_glob, CliTestingSizeCfg, .pattern = value, .name = args->name,
                                 .size = args->size)
                    : ((length <= args->size) &&
                       (memor.cmp(&(MemoriaCfg){.src = value, .other = args->name, .bytes = length}) == 0)))
        {
            return 1;
        }
    }
    return 0;
}
