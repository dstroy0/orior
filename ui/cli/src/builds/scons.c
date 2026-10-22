// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// scons.c: an SConstruct or SConscript read: each byte classed as code, a string's or a comment's; each call of a name
// in code and its arguments, split at the commas outside brackets, the strings and the keywords among them; the names
// its `Import` calls take and those it exports; and the SConscripts its `SConscript` calls read, as the first argument
// gives them or as `dirs` and `name` do. One file is read at a time, under the reader's lock, and a file larger than
// CLI_SCONS_TEXT is not read
#include "scons_internal.h"

CliSconsFile g_cli_scons;
CliLock g_cli_scons_lock = CLI_LOCK_INIT;

const char *const g_cli_scons_globals[76] = {"Environment",
                                             "DefaultEnvironment",
                                             "Program",
                                             "Library",
                                             "StaticLibrary",
                                             "SharedLibrary",
                                             "LoadableModule",
                                             "Object",
                                             "StaticObject",
                                             "SharedObject",
                                             "Command",
                                             "Default",
                                             "Alias",
                                             "AlwaysBuild",
                                             "Depends",
                                             "Ignore",
                                             "Requires",
                                             "Install",
                                             "InstallAs",
                                             "Glob",
                                             "Split",
                                             "SConscript",
                                             "Import",
                                             "Export",
                                             "Return",
                                             "Help",
                                             "AddOption",
                                             "GetOption",
                                             "SetOption",
                                             "ARGUMENTS",
                                             "ARGLIST",
                                             "BUILD_TARGETS",
                                             "COMMAND_LINE_TARGETS",
                                             "DEFAULT_TARGETS",
                                             "Dir",
                                             "File",
                                             "Entry",
                                             "Clean",
                                             "NoClean",
                                             "Precious",
                                             "SideEffect",
                                             "Variables",
                                             "BoolVariable",
                                             "EnumVariable",
                                             "ListVariable",
                                             "PathVariable",
                                             "PackageVariable",
                                             "Builder",
                                             "Action",
                                             "Scanner",
                                             "Configure",
                                             "Exit",
                                             "EnsurePythonVersion",
                                             "EnsureSConsVersion",
                                             "Decider",
                                             "CacheDir",
                                             "VariantDir",
                                             "Repository",
                                             "SConsignFile",
                                             "Tool",
                                             "WhereIs",
                                             "Platform",
                                             "Execute",
                                             "Flatten",
                                             "Mkdir",
                                             "Copy",
                                             "Delete",
                                             "Move",
                                             "Touch",
                                             "Chmod",
                                             "Literal",
                                             "Value",
                                             "GetLaunchDir",
                                             "Progress",
                                             "FindFile",
                                             "Local"};
const char *const g_cli_scons_methods[52] = {"Append",        "AppendUnique",
                                             "AppendENVPath", "Prepend",
                                             "PrependUnique", "PrependENVPath",
                                             "Replace",       "Clone",
                                             "Dictionary",    "Detect",
                                             "Dump",          "Glob",
                                             "MergeFlags",    "ParseConfig",
                                             "ParseFlags",    "SetDefault",
                                             "subst",         "Tool",
                                             "WhereIs",       "Alias",
                                             "AlwaysBuild",   "Command",
                                             "Default",       "Depends",
                                             "Ignore",        "Install",
                                             "InstallAs",     "Program",
                                             "Library",       "StaticLibrary",
                                             "SharedLibrary", "LoadableModule",
                                             "Object",        "StaticObject",
                                             "SharedObject",  "SConscript",
                                             "Split",         "Dir",
                                             "File",          "Entry",
                                             "Clean",         "Precious",
                                             "SideEffect",    "VariantDir",
                                             "CacheDir",      "Decider",
                                             "Requires",      "NoClean",
                                             "Execute",       "Configure",
                                             "Help",          "get"};
const char *const g_cli_scons_variables[44] = {
    "CC",         "CXX",        "CFLAGS",     "CCFLAGS",     "CXXFLAGS",    "CPPFLAGS",     "CPPPATH",     "CPPDEFINES",
    "LIBS",       "LIBPATH",    "LINKFLAGS",  "LINK",        "AR",          "AS",           "ASFLAGS",     "RANLIB",
    "SHCC",       "SHCXX",      "SHCCFLAGS",  "SHCXXFLAGS",  "SHLINKFLAGS", "SHLIBPREFIX",  "SHLIBSUFFIX", "LIBPREFIX",
    "LIBSUFFIX",  "PROGPREFIX", "PROGSUFFIX", "OBJPREFIX",   "OBJSUFFIX",   "ENV",          "TOOLS",       "BUILDERS",
    "PLATFORM",   "FRAMEWORKS", "RPATH",      "TARGET_ARCH", "HOST_ARCH",   "MSVC_VERSION", "CCCOMSTR",    "CXXCOMSTR",
    "LINKCOMSTR", "ARCOMSTR",   "tools",      "toolpath"};
const char *const g_cli_scons_setters[16] = {"Environment",   "Append",       "AppendUnique",  "Prepend",
                                             "PrependUnique", "Replace",      "Clone",         "SetDefault",
                                             "Program",       "Library",      "StaticLibrary", "SharedLibrary",
                                             "Object",        "StaticObject", "SharedObject",  "LoadableModule"};

int cli_scons_read(const char *text, unsigned long long size, unsigned char *class_of)
{
    if (size > CLI_SCONS_TEXT)
    {
        return 0;
    }
    CliSconsFile *const file = &g_cli_scons;
    file->text = text;
    file->size = size;
    file->class_of = class_of;
    memor.set(&(MemoriaCfg){.dst = class_of, .val = CLI_SCONS_CODE, .bytes = (size_t)size});
    for (unsigned long long at = 0ull; at < size;)
    {
        const char *const rest = text + at;
        const unsigned long long left = size - at;
        unsigned long long found = 0ull;
        unsigned char kind = CLI_SCONS_COMMENT;
        unsigned long long end = size;
        if (rest[0] == '#')
        {
            end = cli_text_find(rest, left, "\n", 1u, &found) ? at + found : size;
        }
        else if (cli_text_quoted_end(text, size, at, &end))
        {
            kind = CLI_SCONS_STRING;
        }
        else
        {
            at += 1u;
            continue;
        }
        memor.set(&(MemoriaCfg){.dst = class_of + at, .val = kind, .bytes = (size_t)(end - at)});
        at = end;
    }
    return 1;
}

void cli_scons_calls(const char *name, CliSconsSpan each, void *context)
{
    const char *const text = g_cli_scons.text;
    const unsigned long long size = g_cli_scons.size;
    const unsigned char *const class_of = g_cli_scons.class_of;
    const size_t length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL});
    for (unsigned long long at = 0ull; at + length <= size; at += 1u)
    {
        unsigned long long before = 0ull;
        if ((memor.cmp(&(MemoriaCfg){.src = text + at, .other = name, .bytes = length}) != 0) ||
            (class_of[at] != CLI_SCONS_CODE) ||
            ((at > 0u) && cli_build_word_letter(cli_text_letter_before(text, at, &before))))
        {
            continue;
        }
        unsigned long long open = at + length;
        while ((open < size) && ((text[open] == ' ') || (text[open] == '\t')))
        {
            open += 1u;
        }
        if ((open >= size) || (text[open] != '('))
        {
            at += length - 1u;
            continue;
        }
        long long depth = 0;
        unsigned long long end = size;
        for (unsigned long long walk = open; walk < size; walk += 1u)
        {
            if (class_of[walk] != CLI_SCONS_CODE)
            {
                continue;
            }
            const char byte = text[walk];
            if ((byte == '(') || (byte == '[') || (byte == '{'))
            {
                depth += 1;
            }
            else if ((byte == ')') || (byte == ']') || (byte == '}'))
            {
                depth -= 1;
                if (depth == 0)
                {
                    end = walk;
                    break;
                }
            }
        }
        each(context, open + 1u, end);
        at += length - 1u;
    }
}

void cli_scons_strings(unsigned long long from, unsigned long long to, CliSconsSpan each, void *context)
{
    const char *const text = g_cli_scons.text;
    const unsigned long long size = g_cli_scons.size;
    const unsigned char *const class_of = g_cli_scons.class_of;
    to = (to < size) ? to : size;
    for (unsigned long long at = from; at < to;)
    {
        if ((class_of[at] != CLI_SCONS_STRING) || ((at > 0u) && (class_of[at - 1u] == CLI_SCONS_STRING)))
        {
            at += 1u;
            continue;
        }
        unsigned long long end = at;
        while ((end < size) && (class_of[end] == CLI_SCONS_STRING))
        {
            end += 1u;
        }
        const unsigned long long quote =
            (cli_text_starts(text + at, size - at, "\"\"\"") || cli_text_starts(text + at, size - at, "'''")) ? 3u : 1u;
        const unsigned long long inner_end =
            ((end - at >= 2u * quote) && (text[end - 1u] == text[at])) ? end - quote : end;
        each(context, at + quote, (inner_end > at + quote) ? inner_end : at + quote);
        at = end;
    }
}

void cli_scons_arguments(unsigned long long from, unsigned long long to, CliSconsSpan each, void *context)
{
    const char *const text = g_cli_scons.text;
    long long depth = 0;
    unsigned long long start = from;
    for (unsigned long long at = from; at < to; at += 1u)
    {
        if (g_cli_scons.class_of[at] != CLI_SCONS_CODE)
        {
            continue;
        }
        const char byte = text[at];
        depth += ((byte == '(') || (byte == '[') || (byte == '{')) ? 1 : 0;
        depth -= ((byte == ')') || (byte == ']') || (byte == '}')) ? 1 : 0;
        if ((byte == ',') && (depth == 0))
        {
            each(context, start, at);
            start = at + 1u;
        }
    }
    unsigned long long trimmed_from = 0ull;
    unsigned long long trimmed_to = 0ull;
    cli_text_trimmed(text + start, (to > start) ? to - start : 0u, &trimmed_from, &trimmed_to);
    if (trimmed_to > trimmed_from)
    {
        each(context, start, to);
    }
}

int cli_scons_keyword(unsigned long long from, unsigned long long to, unsigned long long *name,
                      unsigned long long *name_size, unsigned long long *value)
{
    const char *const part = g_cli_scons.text + from;
    const char *const eq = memor.chr(&(MemoriaCfg){.src = part, .val = '=', .bytes = (size_t)(to - from)});
    if (eq == NULL)
    {
        return 0;
    }
    const unsigned long long at = (unsigned long long)(eq - part);
    unsigned long long name_from = 0ull;
    unsigned long long name_to = 0ull;
    cli_text_trimmed(part, at, &name_from, &name_to);
    if ((name_to == name_from) || ((from + at + 1u < to) && (part[at + 1u] == '=')))
    {
        return 0;
    }
    for (unsigned long long walk = name_from; walk < name_to;)
    {
        unsigned long long length = 0ull;
        if (!cli_build_word_letter(cli_text_letter(part, name_to, walk, &length)))
        {
            return 0;
        }
        walk += length;
    }
    *name = from + name_from;
    *name_size = name_to - name_from;
    *value = from + at + 1u;
    return 1;
}
