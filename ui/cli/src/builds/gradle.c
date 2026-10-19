// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// gradle.c: a Gradle script read: each byte classed as code, a string's or a comment's, a string's `${}` the string's;
// its blocks by their braces, each named by the last word of the call before its brace, past its arguments and its
// type arguments; the statements a block holds itself; the configurations it declares; the build a script belongs to,
// the program that runs it, and the catalogs of its `gradle` folder. One script is read at a time, under the reader's
// lock, and a script larger than CLI_GRADLE_TEXT is not read; its classes, blocks and stray braces are cells taken at
// the sizes it gives and given back as its reader is done with it
#include "gradle_internal.h"

#include <stdlib.h>

CliGradleScript g_cli_gradle;
CliLock g_cli_gradle_lock = CLI_LOCK_INIT;
CliCellsTally g_cli_gradle_cells = CLI_CELLS_TALLY("cli_gradle");

const char *const g_cli_gradle_configurations[30] = {"implementation",
                                                     "api",
                                                     "compileOnly",
                                                     "compileOnlyApi",
                                                     "runtimeOnly",
                                                     "testImplementation",
                                                     "testCompileOnly",
                                                     "testRuntimeOnly",
                                                     "annotationProcessor",
                                                     "testAnnotationProcessor",
                                                     "testFixturesImplementation",
                                                     "testFixturesApi",
                                                     "developmentOnly",
                                                     "kapt",
                                                     "ksp",
                                                     "classpath",
                                                     "coreLibraryDesugaring",
                                                     "lintChecks",
                                                     "detektPlugins",
                                                     "androidTestImplementation",
                                                     "debugImplementation",
                                                     "releaseImplementation",
                                                     "providedCompile",
                                                     "providedRuntime",
                                                     "compileClasspath",
                                                     "runtimeClasspath",
                                                     "testCompileClasspath",
                                                     "testRuntimeClasspath",
                                                     "archives",
                                                     "default"};
const char *const g_cli_gradle_helpers[12] = {"platform",     "enforcedPlatform", "project",       "files",
                                              "fileTree",     "gradleApi()",      "localGroovy()", "gradleTestKit()",
                                              "testFixtures", "constraints",      "components",    "modules"};
const char *const g_cli_gradle_suffixes[8] = {"Implementation",      "Api",  "CompileOnly", "RuntimeOnly",
                                              "AnnotationProcessor", "Kapt", "Ksp",         "CompileOnlyApi"};

// what a build script names at its top
static const char *const s_top[31] = {"plugins",
                                      "repositories",
                                      "dependencies",
                                      "java",
                                      "application",
                                      "tasks",
                                      "test",
                                      "jar",
                                      "configurations",
                                      "sourceSets",
                                      "publishing",
                                      "buildscript",
                                      "allprojects",
                                      "subprojects",
                                      "group",
                                      "version",
                                      "description",
                                      "kotlin",
                                      "wrapper",
                                      "apply",
                                      "ext",
                                      "base",
                                      "javadoc",
                                      "compileJava",
                                      "compileTestJava",
                                      "processResources",
                                      "defaultTasks",
                                      "dependencyLocking",
                                      "artifacts",
                                      "idea",
                                      "eclipse"};

// the words each block takes, by its name, the block's name first and a NULL after the last word
static const char *const s_block_words[][14] = {
    {"repositories", "mavenCentral()", "google()", "gradlePluginPortal()", "mavenLocal()", "maven", "ivy", "flatDir",
     "exclusiveContent", NULL},
    {"plugins", "id", "alias", "kotlin", "java", "application", "version", "apply", NULL},
    {"java", "toolchain", "sourceCompatibility", "targetCompatibility", "withSourcesJar()", "withJavadocJar()",
     "modularity", "registerFeature", "disableAutoTargetJvm()", NULL},
    {"toolchain", "languageVersion", "vendor", "implementation", NULL},
    {"application", "mainClass", "applicationName", "applicationDefaultJvmArgs", "mainModule", "executableDir", NULL},
    {"test", "useJUnitPlatform()", "useJUnit()", "useTestNG()", "maxParallelForks", "testLogging", "systemProperty",
     "jvmArgs", "filter", "forkEvery", "maxHeapSize", "include", "exclude", "failFast"},
    {"tasks", "register", "named", "withType", "getByName", "create", "matching", "configureEach", NULL},
    {"buildscript", "repositories", "dependencies", "ext", NULL},
    {"pluginManagement", "repositories", "plugins", "includeBuild", "resolutionStrategy", NULL},
    {"dependencyResolutionManagement", "repositories", "versionCatalogs", "repositoriesMode", "rulesMode", "components",
     NULL},
    {"versionCatalogs", "create", NULL},
    {"publishing", "publications", "repositories", NULL},
    {"jar", "manifest", "archiveBaseName", "archiveVersion", "archiveClassifier", "from", "exclude",
     "duplicatesStrategy", NULL},
    {"configurations", "all", "create", "register", "named", "getByName", "configureEach", "matching", NULL},
    {"sourceSets", "main", "test", "create", "named", "register", NULL}};

const char *const *cli_gradle_block_words(const char *name, unsigned long long size, unsigned int *count)
{
    if (((size == 11u) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "allprojects", .bytes = 11u}) == 0)) ||
        ((size == 11u) && (memor.cmp(&(MemoriaCfg){.src = name, .other = "subprojects", .bytes = 11u}) == 0)))
    {
        *count = 31u;
        return s_top;
    }
    for (unsigned int at = 0u; at < sizeof(s_block_words) / sizeof(s_block_words[0]); at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_block_words[at][0], .cap = CLI_CELLS_POOL}) == size) &&
            (memor.cmp(&(MemoriaCfg){.src = s_block_words[at][0], .other = name, .bytes = (size_t)size}) == 0))
        {
            unsigned int words = 0u;
            while ((words < 13u) && (s_block_words[at][words + 1u] != NULL))
            {
                words += 1u;
            }
            *count = words;
            return &s_block_words[at][1];
        }
    }
    *count = 0u;
    return s_top;
}

void cli_gradle_classes(const char *text, unsigned long long size, unsigned char *class_of)
{
    memor.set(&(MemoriaCfg){.dst = class_of, .val = CLI_GRADLE_CODE, .bytes = (size_t)size});
    for (unsigned long long at = 0ull; at < size;)
    {
        const char *const rest = text + at;
        const unsigned long long left = size - at;
        unsigned char kind = CLI_GRADLE_CODE;
        unsigned long long end = at;
        unsigned long long found = 0ull;
        if (cli_text_starts(rest, left, "//"))
        {
            kind = CLI_GRADLE_COMMENT;
            end = cli_text_find(rest, left, "\n", 1u, &found) ? at + found : size;
        }
        else if (cli_text_starts(rest, left, "/*"))
        {
            kind = CLI_GRADLE_COMMENT;
            end = cli_text_find(rest + 2u, left - 2u, "*/", 2u, &found) ? at + 2u + found + 2u : size;
        }
        else if (cli_text_starts(rest, left, "\"\"\"") || cli_text_starts(rest, left, "'''"))
        {
            kind = CLI_GRADLE_STRING;
            end = cli_text_find(rest + 3u, left - 3u, rest, 3u, &found) ? at + 3u + found + 3u : size;
        }
        else if ((rest[0] == '"') || (rest[0] == '\''))
        {
            kind = CLI_GRADLE_STRING;
            unsigned long long walk = at + 1u;
            while ((walk < size) && (text[walk] != rest[0]) && (text[walk] != '\n'))
            {
                walk += (text[walk] == '\\') ? 2u : 1u;
            }
            end = (walk + 1u < size) ? walk + 1u : size;
        }
        else
        {
            at += 1u;
            continue;
        }
        memor.set(&(MemoriaCfg){.dst = class_of + at, .val = kind, .bytes = (size_t)(end - at)});
        at = end;
    }
}

// whether a byte read as a letter of its own is whitespace
static int gradle_white_byte(unsigned char byte)
{
    return (byte == ' ') || ((byte >= '\t') && (byte <= '\r')) || (byte == 0x85u) || (byte == 0xa0u);
}

// the bytes from `end` back passed while they are whitespace
static unsigned long long gradle_back(unsigned long long end)
{
    while ((end > 0u) && gradle_white_byte((unsigned char)g_cli_gradle.text[end - 1u]))
    {
        end -= 1u;
    }
    return end;
}

// the name a brace at `at` opens the block of: the last word of the call before it, past its arguments and its type
// arguments, its start into `name`: its size
static unsigned long long gradle_name_before(unsigned long long at, unsigned long long *name)
{
    const char *const text = g_cli_gradle.text;
    const unsigned char *const class_of = g_cli_gradle.class_of;
    unsigned long long end = gradle_back(at);
    static const char s_pairs[2][2] = {{')', '('}, {'>', '<'}};
    for (unsigned int pair = 0u; pair < 2u; pair += 1u)
    {
        if ((end == 0u) || (text[end - 1u] != s_pairs[pair][0]) || (class_of[end - 1u] != CLI_GRADLE_CODE))
        {
            continue;
        }
        long long depth = 0;
        while (end > 0u)
        {
            end -= 1u;
            if (class_of[end] != CLI_GRADLE_CODE)
            {
                continue;
            }
            if (text[end] == s_pairs[pair][0])
            {
                depth += 1;
            }
            else if (text[end] == s_pairs[pair][1])
            {
                depth -= 1;
                if (depth == 0)
                {
                    break;
                }
            }
        }
        end = gradle_back(end);
    }
    *name = cli_build_run_before(text, end, cli_build_word_letter);
    return end - *name;
}

void cli_gradle_released(void)
{
    CliGradleScript *const script = &g_cli_gradle;
    cli_cell_given_by(&g_cli_gradle_cells, script->stray);
    cli_cell_given_by(&g_cli_gradle_cells, script->block);
    cli_cell_given_by(&g_cli_gradle_cells, script->class_of);
    script->class_of = NULL;
    script->block = NULL;
    script->stray = NULL;
    script->blocks = 0u;
    script->strays = 0u;
}

int cli_gradle_read(const char *text, unsigned long long size)
{
    CliGradleScript *const script = &g_cli_gradle;
    script->text = text;
    script->size = size;
    script->blocks = 0u;
    script->strays = 0u;
    script->declares = 0u;
    // a byte past the text, so a script of no bytes is a cell too
    script->class_of =
        (size <= CLI_GRADLE_TEXT) ? cli_cell_taken_by(&g_cli_gradle_cells, size + 1ull, "a script's classes") : NULL;
    if (script->class_of == NULL)
    {
        return 0;
    }
    cli_gradle_classes(text, size, script->class_of);
    // the braces in code counted, so the blocks, the braces still open and the stray ones are cells of as many
    unsigned long long opens = 0ull;
    unsigned long long closes = 0ull;
    for (unsigned long long at = 0ull; at < size; at += 1u)
    {
        opens += ((script->class_of[at] == CLI_GRADLE_CODE) && (text[at] == '{')) ? 1ull : 0ull;
        closes += ((script->class_of[at] == CLI_GRADLE_CODE) && (text[at] == '}')) ? 1ull : 0ull;
    }
    opens = (opens < CLI_GRADLE_BLOCKS) ? opens : CLI_GRADLE_BLOCKS;
    closes = (closes < CLI_GRADLE_BLOCKS) ? closes : CLI_GRADLE_BLOCKS;
    script->block =
        cli_cell_taken_by(&g_cli_gradle_cells, (opens + 1ull) * sizeof(CliGradleBlock), "a script's blocks");
    script->stray =
        cli_cell_taken_by(&g_cli_gradle_cells, (closes + 1ull) * sizeof(unsigned long long), "a script's stray braces");
    unsigned int *const opened =
        cli_cell_taken_by(&g_cli_gradle_cells, (opens + 1ull) * sizeof(unsigned int), "a script's braces open");
    if ((script->block == NULL) || (script->stray == NULL) || (opened == NULL))
    {
        cli_cell_given_by(&g_cli_gradle_cells, opened);
        cli_gradle_released();
        return 0;
    }
    unsigned int open = 0u;
    for (unsigned long long at = 0ull; at < size; at += 1u)
    {
        if (script->class_of[at] != CLI_GRADLE_CODE)
        {
            continue;
        }
        if ((text[at] == '{') && (script->blocks < opens))
        {
            CliGradleBlock *const block = &script->block[script->blocks];
            block->name_size = gradle_name_before(at, &block->name);
            block->open = at;
            block->close = CLI_GRADLE_NONE;
            opened[open] = script->blocks;
            open += 1u;
            script->blocks += 1u;
        }
        else if ((text[at] == '}') && (open > 0u))
        {
            open -= 1u;
            script->block[opened[open]].close = at;
        }
        else if ((text[at] == '}') && (script->strays < closes))
        {
            script->stray[script->strays] = at;
            script->strays += 1u;
        }
    }
    cli_cell_given_by(&g_cli_gradle_cells, opened);
    return 1;
}

unsigned int cli_gradle_block_at(unsigned long long at)
{
    unsigned int inner = CLI_GRADLE_BLOCKS;
    for (unsigned int one = 0u; one < g_cli_gradle.blocks; one += 1u)
    {
        const CliGradleBlock *const block = &g_cli_gradle.block[one];
        if ((block->open < at) && ((block->close == CLI_GRADLE_NONE) || (block->close >= at)))
        {
            inner = one;
        }
    }
    return inner;
}

int cli_gradle_block_named(const CliGradleBlock *block, const char *name)
{
    return (cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) == block->name_size) &&
           (memor.cmp(&(MemoriaCfg){
                .src = g_cli_gradle.text + block->name, .other = name, .bytes = (size_t)block->name_size}) == 0);
}

void cli_gradle_statements(const CliBuildLines *lines, unsigned int block, unsigned long long end,
                           CliGradleStatement each, void *context)
{
    const CliGradleBlock *const held = &g_cli_gradle.block[block];
    const char *const text = g_cli_gradle.text;
    const unsigned int first = cli_build_place(lines, held->open + 1u).line;
    const unsigned int last = cli_build_place(lines, end).line;
    for (unsigned int line = first; line <= last; line += 1u)
    {
        unsigned long long start = cli_build_line_start(lines, line);
        start = (start > held->open + 1u) ? start : held->open + 1u;
        unsigned long long found = 0ull;
        unsigned long long line_end =
            (start < g_cli_gradle.size) && cli_text_find(text + start, g_cli_gradle.size - start, "\n", 1u, &found)
                ? start + found
                : g_cli_gradle.size;
        line_end = (line_end < end) ? line_end : end;
        if (line_end <= start)
        {
            continue;
        }
        const unsigned long long at = start + cli_text_lead(text + start, line_end - start);
        if ((at >= line_end) || (g_cli_gradle.class_of[at] != CLI_GRADLE_CODE) || (cli_gradle_block_at(at) != block))
        {
            continue;
        }
        unsigned long long word = at;
        while (word < line_end)
        {
            unsigned long long length = 0ull;
            if (!cli_build_word_letter(cli_text_letter(text, line_end, word, &length)))
            {
                break;
            }
            word += length;
        }
        if (word > at)
        {
            each(context, at, word - at, word, line_end);
        }
    }
}

// a configuration the script declares kept, where it is not kept already
static void gradle_declare(const char *name, unsigned long long size)
{
    CliGradleScript *const script = &g_cli_gradle;
    if (script->declares < CLI_GRADLE_DECLARED)
    {
        script->declared[script->declares] = name;
        script->declared_size[script->declares] = size;
        script->declares += 1u;
    }
}

// a statement of a `configurations` block declared where it is no word the block takes
static void gradle_declared_statement(void *context, unsigned long long word, unsigned long long word_size,
                                      unsigned long long rest, unsigned long long rest_end)
{
    (void)context;
    (void)rest;
    (void)rest_end;
    unsigned int count = 0u;
    const char *const *const words = cli_gradle_block_words("configurations", 14u, &count);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = words[at], .cap = CLI_CELLS_POOL}) == word_size) &&
            (memor.cmp(
                 &(MemoriaCfg){.src = words[at], .other = g_cli_gradle.text + word, .bytes = (size_t)word_size}) == 0))
        {
            return;
        }
    }
    gradle_declare(g_cli_gradle.text + word, word_size);
}

void cli_gradle_declared(const CliBuildLines *lines)
{
    const char *const text = g_cli_gradle.text;
    const unsigned long long size = g_cli_gradle.size;
    static const char *const s_calls[3] = {"create(", "register(", "maybeCreate("};
    for (unsigned long long at = 0ull; at < size; at += 1u)
    {
        for (unsigned int call = 0u; call < 3u; call += 1u)
        {
            const unsigned long long length =
                cellul.len(&(CatenaFinitaCfg){.src = s_calls[call], .cap = CLI_CELLS_POOL});
            unsigned long long before = 0ull;
            if (!cli_text_starts(text + at, size - at, s_calls[call]) ||
                (g_cli_gradle.class_of[at] != CLI_GRADLE_CODE) ||
                ((at > 0u) && cli_build_word_letter(cli_text_letter_before(text, at, &before))))
            {
                continue;
            }
            const unsigned long long rest = at + length;
            if ((rest < size) && ((text[rest] == '"') || (text[rest] == '\'')))
            {
                unsigned long long end = rest + 1u;
                while ((end < size) && (text[end] != '"') && (text[end] != '\''))
                {
                    end += 1u;
                }
                gradle_declare(text + rest + 1u, end - rest - 1u);
            }
        }
    }
    for (unsigned long long start = 0ull; start < size;)
    {
        const char *const newline =
            memor.chr(&(MemoriaCfg){.src = text + start, .val = '\n', .bytes = (size_t)(size - start)});
        const unsigned long long end = (newline != NULL) ? (unsigned long long)(newline - text) : size;
        unsigned long long from = 0ull;
        unsigned long long to = 0ull;
        cli_text_trimmed(text + start, end - start, &from, &to);
        const char *const line = text + start + from;
        const unsigned long long line_size = to - from;
        unsigned long long found = 0ull;
        if (cli_text_starts(line, line_size, "val ") &&
            (cli_text_find(line + 4u, line_size - 4u, "by configurations.creating", 26u, &found) ||
             cli_text_find(line + 4u, line_size - 4u, "by configurations.registering", 29u, &found) ||
             cli_text_find(line + 4u, line_size - 4u, "by creating", 11u, &found)))
        {
            unsigned long long walk = 0ull;
            unsigned long long word = 0ull;
            unsigned long long length = 0ull;
            if (cli_text_word(line + 4u, line_size - 4u, &walk, &word, &length))
            {
                gradle_declare(line + 4u + word, length);
            }
        }
        start = end + 1u;
    }
    for (unsigned int block = 0u; block < g_cli_gradle.blocks; block += 1u)
    {
        const CliGradleBlock *const held = &g_cli_gradle.block[block];
        if (cli_gradle_block_named(held, "configurations"))
        {
            cli_gradle_statements(lines, block, (held->close != CLI_GRADLE_NONE) ? held->close : size,
                                  gradle_declared_statement, NULL);
        }
    }
}
