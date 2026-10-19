// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// testing.c: a tree's tests as the window lists them, each written as the window reads one; the rules a tree gives
// pytest, read as pytest reads them, a setting's values split at spaces and commas with a TOML list's brackets and
// quotes taken off, and a setting carried on to the lines indented under it; and a name matched to a rule
#include "testing.h"
#include "../text/json_tree.h"

void cli_test_written(const CliTest *test, CliLine *out)
{
    cli_line_text(out, "{\"id\":");
    cli_json_quoted_lossy(out, (const unsigned char *)test->id,
                          cellul.len(&(CatenaFinitaCfg){.src = test->id, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"file\":");
    cli_json_quoted_lossy(out, (const unsigned char *)test->file,
                          cellul.len(&(CatenaFinitaCfg){.src = test->file, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"class\":");
    if (test->class_name == NULL)
    {
        cli_line_text(out, "null");
    }
    else
    {
        cli_json_quoted_lossy(out, (const unsigned char *)test->class_name,
                              cellul.len(&(CatenaFinitaCfg){.src = test->class_name, .cap = CLI_CELLS_POOL}));
    }
    cli_line_text(out, ",\"name\":");
    cli_json_quoted_lossy(out, (const unsigned char *)test->name,
                          cellul.len(&(CatenaFinitaCfg){.src = test->name, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"line\":");
    cli_line_decimal(out, test->line, 1u);
    cli_line_character(out, '}');
}

// `size` bytes of `value` put on `rule`, where they fit
static void testing_value_put(CliTestRules *rules, CliTestRule *rule, const char *value, unsigned long long size)
{
    if ((rule->count == CLI_TEST_VALUES) || (size + 1u > CLI_TEST_RULES_TEXT - rules->used))
    {
        return;
    }
    rule->value[rule->count] = rules->used;
    rule->count += 1u;
    memor.cpy(&(MemoriaCfg){.dst = rules->text + rules->used, .src = value, .bytes = (size_t)size});
    rules->used += (unsigned int)size;
    rules->text[rules->used] = '\0';
    rules->used += 1u;
}

void cli_test_rules_own(CliTestRules *rules)
{
    memor.set(&(MemoriaCfg){.dst = rules, .val = 0, .bytes = sizeof(*rules)});
    testing_value_put(rules, &rules->files, "test_*.py", 9u);
    testing_value_put(rules, &rules->files, "*_test.py", 9u);
    testing_value_put(rules, &rules->files, "test*.py", 8u);
    testing_value_put(rules, &rules->classes, "Test", 4u);
    testing_value_put(rules, &rules->functions, "test", 4u);
}

const char *cli_test_rule_value(const CliTestRules *rules, const CliTestRule *rule, unsigned int at)
{
    return rules->text + rule->value[at];
}

// whether a byte is a space of a line
static int testing_space(char letter)
{
    return (letter == ' ') || (letter == '\t') || (letter == '\r') || (letter == '\n') || (letter == '\f') ||
           (letter == '\v');
}

// the values `size` bytes of a setting give put on `rule` in place of what it held
static void testing_values_of(CliTestRules *rules, CliTestRule *rule, const char *raw, unsigned long long size)
{
    rule->count = 0u;
    unsigned long long start = 0ull;
    unsigned long long end = size;
    while ((start < end) && (raw[start] == '['))
    {
        start += 1u;
    }
    while ((end > start) && (raw[end - 1u] == ']'))
    {
        end -= 1u;
    }
    for (unsigned long long at = start; at <= end; at += 1u)
    {
        if ((at < end) && !testing_space(raw[at]) && (raw[at] != ','))
        {
            continue;
        }
        unsigned long long from = start;
        unsigned long long to = at;
        while ((from < to) && (testing_space(raw[from]) || (raw[from] == '"') || (raw[from] == '\'')))
        {
            from += 1u;
        }
        while ((to > from) && (testing_space(raw[to - 1u]) || (raw[to - 1u] == '"') || (raw[to - 1u] == '\'')))
        {
            to -= 1u;
        }
        if (to > from)
        {
            testing_value_put(rules, rule, raw + from, to - from);
        }
        start = at + 1u;
    }
}

// the setting `key` set from its value
static void testing_set(CliTestRules *rules, const char *key, unsigned long long key_size, const char *value,
                        unsigned long long size)
{
    static const char *const s_keys[4] = {"testpaths", "python_files", "python_classes", "python_functions"};
    CliTestRule *const rule[4] = {&rules->paths, &rules->files, &rules->classes, &rules->functions};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_keys[at], .cap = CLI_CELLS_POOL}) == key_size) &&
            (memor.cmp(&(MemoriaCfg){.src = s_keys[at], .other = key, .bytes = (size_t)key_size}) == 0))
        {
            testing_values_of(rules, rule[at], value, size);
        }
    }
}

// the bounds of `size` bytes of `text` with the spaces at both ends taken off
static void testing_trimmed(const char *text, unsigned long long size, unsigned long long *from, unsigned long long *to)
{
    *from = 0ull;
    *to = size;
    while ((*from < *to) && testing_space(text[*from]))
    {
        *from += 1u;
    }
    while ((*to > *from) && testing_space(text[*to - 1u]))
    {
        *to -= 1u;
    }
}

int cli_test_rules_in(const char *text, unsigned long long size, const char *section, CliTestRules *rules)
{
    cli_test_rules_own(rules);
    static char s_key[256];
    static char s_value[8192];
    CliLine value = {s_value, sizeof(s_value), 0ull};
    unsigned long long key_size = 0ull;
    int keyed = 0;
    int inside = 0;
    int found = 0;
    const unsigned long long section_size = cellul.len(&(CatenaFinitaCfg){.src = section, .cap = CLI_CELLS_POOL});
    for (unsigned long long start = 0ull; start < size;)
    {
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = text + start, .val = '\n', .bytes = (size_t)(size - start)});
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - text) : size;
        const char *const line = text + start;
        const unsigned long long line_size =
            ((end > start) && (text[end - 1u] == '\r')) ? end - start - 1u : end - start;
        start = end + 1u;
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        testing_trimmed(line, line_size, &from, &to);
        const char *const trimmed = line + from;
        const unsigned long long trimmed_size = to - from;
        const int indented = (line_size > 0u) && ((line[0] == ' ') || (line[0] == '\t'));
        const int listed = keyed && (trimmed_size >= 2u) && (trimmed[0] == '[') && (trimmed[1] == '"');
        if ((trimmed_size > 0u) && (trimmed[0] == '[') && !indented && !listed)
        {
            if (keyed)
            {
                testing_set(rules, s_key, key_size, s_value, value.at);
                keyed = 0;
            }
            inside = (trimmed_size == section_size) &&
                     (memor.cmp(&(MemoriaCfg){.src = trimmed, .other = section, .bytes = (size_t)section_size}) == 0);
            found |= inside;
            continue;
        }
        if (!inside || ((trimmed_size > 0u) && ((trimmed[0] == '#') || (trimmed[0] == ';'))))
        {
            continue;
        }
        if (indented && keyed)
        {
            cli_line_character(&value, ' ');
            cli_line_counted(&value, trimmed, trimmed_size);
            continue;
        }
        if (keyed)
        {
            testing_set(rules, s_key, key_size, s_value, value.at);
            keyed = 0;
        }
        const char *const equals = memor.chr(&(MemoriaCfg){.src = trimmed, .val = '=', .bytes = (size_t)trimmed_size});
        if (equals != NULL)
        {
            const unsigned long long name_size = (unsigned long long)(equals - trimmed);
            unsigned long long name_from = 0ull;
            unsigned long long name_to = 0ull;
            testing_trimmed(trimmed, name_size, &name_from, &name_to);
            unsigned long long rest_from = 0ull;
            unsigned long long rest_to = 0ull;
            testing_trimmed(equals + 1, trimmed_size - name_size - 1u, &rest_from, &rest_to);
            key_size = (name_to - name_from < sizeof(s_key)) ? name_to - name_from : 0ull;
            memor.cpy(&(MemoriaCfg){.dst = s_key, .src = trimmed + name_from, .bytes = (size_t)key_size});
            value.at = 0ull;
            cli_line_counted(&value, equals + 1 + rest_from, rest_to - rest_from);
            keyed = 1;
        }
    }
    if (keyed)
    {
        testing_set(rules, s_key, key_size, s_value, value.at);
    }
    return found;
}

void cli_test_rules_of(const char *root, CliTestRules *rules)
{
    static const char *const s_files[4] = {"pytest.ini", "pyproject.toml", "tox.ini", "setup.cfg"};
    static const char *const s_sections[4] = {"[pytest]", "[tool.pytest.ini_options]", "[pytest]", "[tool:pytest]"};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        char path[CLI_PATH_LONGEST];
        cli_full(root, s_files[at], path);
        unsigned long long size = 0ull;
        const unsigned char *const bytes = qry_member_file_mapped(path, &size);
        if (bytes == NULL)
        {
            continue;
        }
        const char *const text = (const char *)bytes;
        const int read = cli_utf8_whole(text, size) && cli_test_rules_in(text, size, s_sections[at], rules);
        qry_member_file_unmapped(bytes, size);
        if (read)
        {
            return;
        }
    }
    cli_test_rules_own(rules);
}

// the bytes the letter of UTF-8 at `text` takes, of the `size` left
static unsigned long long testing_letter(const char *text, unsigned long long size)
{
    const unsigned char lead = (unsigned char)text[0];
    const unsigned long long want = (lead < 0xc0u) ? 1u : (lead < 0xe0u) ? 2u : (lead < 0xf0u) ? 3u : 4u;
    return (want < size) ? want : size;
}

int cli_test_glob(const char *pattern, const char *name, unsigned long long size)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = pattern, .cap = CLI_CELLS_POOL});
    unsigned long long held = 0ull;
    unsigned long long at = 0ull;
    unsigned long long star = length;
    unsigned long long mark = 0ull;
    while (at < size)
    {
        const unsigned long long letter = testing_letter(name + at, size - at);
        const unsigned long long own = (held < length) ? testing_letter(pattern + held, length - held) : 0ull;
        if ((held < length) &&
            ((pattern[held] == '?') ||
             ((own == letter) &&
              (memor.cmp(&(MemoriaCfg){.src = pattern + held, .other = name + at, .bytes = (size_t)own}) == 0))))
        {
            held += (pattern[held] == '?') ? 1u : own;
            at += letter;
        }
        else if ((held < length) && (pattern[held] == '*'))
        {
            star = held;
            mark = at;
            held += 1u;
        }
        else if (star < length)
        {
            held = star + 1u;
            mark += testing_letter(name + mark, size - mark);
            at = mark;
        }
        else
        {
            return 0;
        }
    }
    while ((held < length) && (pattern[held] == '*'))
    {
        held += 1u;
    }
    return held == length;
}

int cli_test_named(const CliTestRules *rules, const CliTestRule *rule, const char *name, unsigned long long size)
{
    for (unsigned int at = 0u; at < rule->count; at += 1u)
    {
        const char *const value = cli_test_rule_value(rules, rule, at);
        const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL});
        const int globbed =
            (cellul.chr(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL, .byte = '*'}) != NULL) ||
            (cellul.chr(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL, .byte = '?'}) != NULL);
        if (globbed
                ? cli_test_glob(value, name, size)
                : ((length <= size) && (memor.cmp(&(MemoriaCfg){.src = value, .other = name, .bytes = length}) == 0)))
        {
            return 1;
        }
    }
    return 0;
}
