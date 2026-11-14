// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// gradle.c: a Gradle script read: each byte classed as code, a string's or a comment's, a string's `${}` the string's;
// its blocks by their braces, each named by the last word of the call before its brace, past its arguments and its
// type arguments; the statements a block contains itself; the configurations it declares; the build a script belongs
// to, the program that runs it, and the catalogs of its `gradle` folder. One script is read at a time, under the
// reader's lock, and a script larger than CLI_GRADLE_TEXT is not read; its classes, blocks and stray braces are cells
// taken at the sizes it gives and given back as its reader is done with it
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "gradle_internal.h"

CliGradleScript g_cli_gradle;

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

const char *const *cli_gradle_block_words(const CliGradleSizeCtx *args)
{
    unsigned int *count = args->count;
    if (((args->size == 11u) &&
         (memor.cmp(&(MemoriaCfg){.src = args->name, .other = "allprojects", .bytes = 11u}) == 0)) ||
        ((args->size == 11u) &&
         (memor.cmp(&(MemoriaCfg){.src = args->name, .other = "subprojects", .bytes = 11u}) == 0)))
    {
        *count = 31u;
        return s_top;
    }
    for (unsigned int at = 0u; at < sizeof(s_block_words) / sizeof(s_block_words[0]); at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_block_words[at][0], .cap = CLI_POOL_BYTES}) == args->size) &&
            (memor.cmp(&(MemoriaCfg){
                 .src = s_block_words[at][0], .other = args->name, .bytes = (embed_word)args->size}) == 0))
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

void cli_gradle_classes(const CliGradleTextSizeClassOfCtx *args)
{
    memor.set(&(MemoriaCfg){.dst = args->class_of, .val = CLI_GRADLE_CODE, .bytes = (embed_word)args->size});
    for (unsigned long long at = 0ULL; at < args->size;)
    {
        const char *const rest = args->text + at;
        const unsigned long long left = args->size - at;
        unsigned char type = CLI_GRADLE_CODE;
        unsigned long long end = at;
        unsigned long long found = 0ULL;
        if (EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = rest, .size = left, .prefix = "//"))
        {
            type = CLI_GRADLE_COMMENT;
            end = EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = rest, .length_value = left,
                             .required = "\n", .size = 1u, .at_pointer = &found)
                      ? at + found
                      : args->size;
        }
        else if (EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = rest, .size = left,
                            .prefix = "/*"))
        {
            type = CLI_GRADLE_COMMENT;
            end = EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = rest + 2u, .length_value = left - 2u,
                             .required = "*/", .size = 2u, .at_pointer = &found)
                      ? at + 2u + found + 2u
                      : args->size;
        }
        else if (EMBED_CALL(cli_text_at_size.quoted_end, CliTextAtSizeCfg, .text = args->text, .size = args->size,
                            .at = at, .end = &end))
        {
            type = CLI_GRADLE_STRING;
        }
        else
        {
            at += 1u;
            continue;
        }
        memor.set(&(MemoriaCfg){.dst = args->class_of + at, .val = type, .bytes = (embed_word)(end - at)});
        at = end;
    }
}

typedef struct
{
    unsigned char byte;
    unsigned long long end;
    unsigned long long at;
    unsigned long long *name;
    const char *name_text;
    unsigned long long size;
} GradleCtx;
// whether a byte read as a letter of its own is whitespace
EMBED_INLINE int gradle_white_byte(const GradleCtx *args)
{
    return (args->byte == ' ') || ((args->byte >= '\t') && (args->byte <= '\r')) || (args->byte == 0x85u) ||
           (args->byte == 0xa0u);
}

// the bytes from `end` back passed while they are whitespace
EMBED_INLINE unsigned long long gradle_back(const GradleCtx *args)
{
    unsigned long long end = args->end;
    while ((end > 0u) && EMBED_CALL(gradle_white_byte, GradleCtx, .byte = (unsigned char)g_cli_gradle.text[end - 1u]))
    {
        end -= 1u;
    }
    return end;
}

// the name a brace at `at` opens the block of: the last word of the call before it, past its arguments and its type
// arguments, its start into `name`: its size
EMBED_INLINE unsigned long long gradle_name_before(const GradleCtx *args)
{
    unsigned long long *name = args->name;
    const char *const text = g_cli_gradle.text;
    const unsigned char *const class_of = g_cli_gradle.class_of;
    unsigned long long end = EMBED_CALL(gradle_back, GradleCtx, .end = args->at);
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
            const int code = class_of[end] == CLI_GRADLE_CODE;
            const int closes = code && (text[end] == s_pairs[pair][0]);
            const int opens = code && (text[end] == s_pairs[pair][1]);
            depth += closes ? 1 : 0;
            depth -= opens ? 1 : 0;
            if (opens && (depth == 0))
            {
                break;
            }
        }
        end = EMBED_CALL(gradle_back, GradleCtx, .end = end);
    }
    *name = EMBED_CALL(cli_builds_text_at.build_run_before, CliBuildsTextAtCfg, .text = text, .at = end,
                       .of = cli_build_word_letter);
    return end - *name;
}

int cli_gradle_read(const CliGradleTextSizeCtx *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    CliGradleScript *const script = &g_cli_gradle;
    script->text = text;
    script->size = size;
    script->blocks = 0u;
    script->strays = 0u;
    script->declares = 0u;
    // a byte past the text, so a script of no bytes is a cell too
    script->class_of =
        (size <= CLI_GRADLE_TEXT) ? g_cli_guard->temporary_buf_alloc(size + 1ULL, g_cli_gradle_owner) : NULL;
    if (script->class_of == NULL)
    {
        return 0;
    }
    EMBED_CALL(cli_gradle_text_size_class_of_internal.classes, CliGradleTextSizeClassOfCtx, .text = text, .size = size,
               .class_of = script->class_of);
    // the braces in code counted, so the blocks, the braces still open and the stray ones are cells of as many
    unsigned long long opens = 0ULL;
    unsigned long long closes = 0ULL;
    for (unsigned long long at = 0ULL; at < size; at += 1u)
    {
        opens += ((script->class_of[at] == CLI_GRADLE_CODE) && (text[at] == '{')) ? 1ULL : 0ULL;
        closes += ((script->class_of[at] == CLI_GRADLE_CODE) && (text[at] == '}')) ? 1ULL : 0ULL;
    }
    opens = (opens < CLI_GRADLE_BLOCKS) ? opens : CLI_GRADLE_BLOCKS;
    closes = (closes < CLI_GRADLE_BLOCKS) ? closes : CLI_GRADLE_BLOCKS;
    script->block = g_cli_guard->temporary_buf_alloc((opens + 1ULL) * sizeof(CliGradleBlock), g_cli_gradle_owner);
    script->stray = g_cli_guard->temporary_buf_alloc((closes + 1ULL) * sizeof(unsigned long long), g_cli_gradle_owner);
    unsigned int *const opened =
        g_cli_guard->temporary_buf_alloc((opens + 1ULL) * sizeof(unsigned int), g_cli_gradle_owner);
    if ((script->block == NULL) || (script->stray == NULL) || (opened == NULL))
    {
        g_cli_guard->persistent_buf_release(opened, g_cli_gradle_owner);
        CliGradleScript *const script = &g_cli_gradle;
        g_cli_guard->persistent_buf_release(script->stray, g_cli_gradle_owner);
        g_cli_guard->persistent_buf_release(script->block, g_cli_gradle_owner);
        g_cli_guard->persistent_buf_release(script->class_of, g_cli_gradle_owner);
        script->class_of = NULL;
        script->block = NULL;
        script->stray = NULL;
        script->blocks = 0u;
        script->strays = 0u;
        return 0;
    }
    unsigned int open = 0u;
    for (unsigned long long at = 0ULL; at < size; at += 1u)
    {
        if (script->class_of[at] != CLI_GRADLE_CODE)
        {
            continue;
        }
        if ((text[at] == '{') && (script->blocks < opens))
        {
            CliGradleBlock *const block = &script->block[script->blocks];
            block->name_size = EMBED_CALL(gradle_name_before, GradleCtx, .at = at, .name = &block->name);
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
    g_cli_guard->persistent_buf_release(opened, g_cli_gradle_owner);
    return 1;
}

unsigned int cli_gradle_block_at(const CliGradleTextCtx *args)
{
    unsigned int inner = CLI_GRADLE_BLOCKS;
    for (unsigned int one = 0u; one < g_cli_gradle.blocks; one += 1u)
    {
        const CliGradleBlock *const block = &g_cli_gradle.block[one];
        if ((block->open < args->at) && ((block->close == CLI_GRADLE_NONE) || (block->close >= args->at)))
        {
            inner = one;
        }
    }
    return inner;
}

int cli_gradle_block_by_name(const CliGradleCtx *args)
{
    return (cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES}) == args->block->name_size) &&
           (memor.cmp(&(MemoriaCfg){.src = g_cli_gradle.text + args->block->name,
                                    .other = args->name,
                                    .bytes = (embed_word)args->block->name_size}) == 0);
}

void cli_gradle_statements(const CliGradleLinesCtx *args)
{
    const CliBuildLines *lines = args->lines;
    const CliGradleBlock *const block_at = &g_cli_gradle.block[args->block_u32];
    const char *const text = g_cli_gradle.text;
    const unsigned int first =
        EMBED_CALL(cli_builds_lines_at.build_position, CliBuildsLinesAtCfg, .lines = lines, .at = block_at->open + 1u)
            .line;
    const unsigned int last =
        EMBED_CALL(cli_builds_lines_at.build_position, CliBuildsLinesAtCfg, .lines = lines, .at = args->end).line;
    for (unsigned int line = first; line <= last; line += 1u)
    {
        unsigned long long start =
            EMBED_CALL(cli_builds_line.build_line_start, CliBuildsLineCfg, .lines = lines, .line = line);
        start = (start > block_at->open + 1u) ? start : block_at->open + 1u;
        unsigned long long found = 0ULL;
        unsigned long long line_end =
            (start < g_cli_gradle.size) && EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = text + start,
                                                      .length_value = g_cli_gradle.size - start, .required = "\n",
                                                      .size = 1u, .at_pointer = &found)
                ? start + found
                : g_cli_gradle.size;
        line_end = (line_end < args->end) ? line_end : args->end;
        if (line_end <= start)
        {
            continue;
        }
        const unsigned long long at = start + EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg,
                                                         .text = text + start, .size = line_end - start);
        if ((at >= line_end) || (g_cli_gradle.class_of[at] != CLI_GRADLE_CODE) ||
            (EMBED_CALL(cli_gradle_text_internal.block_at, CliGradleTextCtx, .at = at) != args->block_u32))
        {
            continue;
        }
        unsigned long long word_text = at;
        while (word_text < line_end)
        {
            unsigned long long length = 0ULL;
            if (!EMBED_CALL(cli_each_letter.build_word_letter, CliEachLetterCfg,
                            .letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = text, .size = line_end,
                                                 .at = word_text, .length = &length)))
            {
                break;
            }
            word_text += length;
        }
        if (word_text > at)
        {
            EMBED_CALL(args->each, CliGradleStatementCfg, .context = args->context, .word_text = at,
                       .word_size = word_text - at, .rest = word_text, .remainder_end = line_end);
        }
    }
}

// a configuration the script declares stored, where it is not stored already
EMBED_INLINE void gradle_declare(const GradleCtx *args)
{
    CliGradleScript *const script = &g_cli_gradle;
    if (script->declares < CLI_GRADLE_DECLARED)
    {
        script->declared[script->declares] = args->name_text;
        script->declared_size[script->declares] = args->size;
        script->declares += 1u;
    }
}

// a statement of a `configurations` block declared where it is no word the block takes
static void gradle_declared_statement(const CliGradleStatementCfg *args)
{
    unsigned int count = 0u;
    const char *const *const words = EMBED_CALL(cli_gradle_size_internal.block_words, CliGradleSizeCtx,
                                                .name = "configurations", .size = 14u, .count = &count);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = words[at], .cap = CLI_POOL_BYTES}) == args->word_size) &&
            (memor.cmp(&(MemoriaCfg){.src = words[at],
                                     .other = g_cli_gradle.text + args->word_text,
                                     .bytes = (embed_word)args->word_size}) == 0))
        {
            return;
        }
    }
    EMBED_CALL(gradle_declare, GradleCtx, .name_text = g_cli_gradle.text + args->word_text, .size = args->word_size);
}

void cli_gradle_declare(const CliGradleLinesCtx *args)
{
    const char *const text = g_cli_gradle.text;
    const unsigned long long size = g_cli_gradle.size;
    static const char *const s_calls[3] = {"create(", "register(", "maybeCreate("};
    for (unsigned long long at = 0ULL; at < size; at += 1u)
    {
        for (unsigned int call = 0u; call < 3u; call += 1u)
        {
            const unsigned long long length =
                cellul.len(&(CatenaFinitaCfg){.src = s_calls[call], .cap = CLI_POOL_BYTES});
            unsigned long long before = 0ULL;
            if (!EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = text + at, .size = size - at,
                            .prefix = s_calls[call]) ||
                (g_cli_gradle.class_of[at] != CLI_GRADLE_CODE) ||
                ((at > 0u) && EMBED_CALL(cli_each_letter.build_word_letter, CliEachLetterCfg,
                                         .letter = EMBED_CALL(cli_text_length.letter_before, CliTextLengthCfg,
                                                              .text = text, .end_value = at, .length = &before))))
            {
                continue;
            }
            const unsigned long long rest = at + length;
            if ((rest < size) && ((text[rest] == '"') || (text[rest] == '\'')))
            {
                unsigned long long end = rest + 1u;
                end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = end,
                                 .size = size, .set = "\"'");
                EMBED_CALL(gradle_declare, GradleCtx, .name_text = text + rest + 1u, .size = end - rest - 1u);
            }
        }
    }
    for (unsigned long long start = 0ULL; start < size;)
    {
        const unsigned long long end =
            EMBED_CALL(cli_text_at.line_at, CliTextAtCfg, .text = text, .at = start, .size = size).stop;
        unsigned long long from = 0ULL;
        unsigned long long to = 0ULL;
        EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = text + start, .size = end - start,
                   .from_u64s = &from, .to_u64s = &to);
        const char *const line = text + start + from;
        const unsigned long long line_size = to - from;
        unsigned long long found = 0ULL;
        if (EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = line, .size = line_size,
                       .prefix = "val ") &&
            (EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = line + 4u, .length_value = line_size - 4u,
                        .required = "by configurations.creating", .size = 26u, .at_pointer = &found) ||
             EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = line + 4u, .length_value = line_size - 4u,
                        .required = "by configurations.registering", .size = 29u, .at_pointer = &found) ||
             EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = line + 4u, .length_value = line_size - 4u,
                        .required = "by creating", .size = 11u, .at_pointer = &found)))
        {
            unsigned long long walk = 0ULL;
            unsigned long long word_text = 0ULL;
            unsigned long long length = 0ULL;
            if (EMBED_CALL(cli_text_size_length.word_text, CliTextSizeLengthCfg, .text = line + 4u,
                           .size = line_size - 4u, .at_pointer = &walk, .word_text = &word_text, .length = &length))
            {
                EMBED_CALL(gradle_declare, GradleCtx, .name_text = line + 4u + word_text, .size = length);
            }
        }
        start = end + 1u;
    }
    for (unsigned int block = 0u; block < g_cli_gradle.blocks; block += 1u)
    {
        const CliGradleBlock *const block_at = &g_cli_gradle.block[block];
        if (EMBED_CALL(cli_gradle_internal.block_by_name, CliGradleCtx, .block = block_at, .name = "configurations"))
        {
            EMBED_CALL(cli_gradle_lines_internal.statements, CliGradleLinesCtx, .lines = args->lines,
                       .block_u32 = block, .end = (block_at->close != CLI_GRADLE_NONE) ? block_at->close : size,
                       .each = gradle_declared_statement, .context = NULL);
        }
    }
}
