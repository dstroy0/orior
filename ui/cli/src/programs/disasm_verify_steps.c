// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// disasm_verify_steps.c: the verification's steps, each a program run with the toolchains' PATH within its patience:
// the tree's command line built with link-time optimization, the module's object lifted to IR and its functions listed,
// their symbols listed from the program, the symbols' instructions read by the disassembler and by Ghidra's analyzer
// running the plugin's script, and the program executed. A program that ends with a code other than 0 is said with its
// errors' first bytes
#include "../command_line/command_line_internal.h"
#include "../runs/runner.h"
#include "../toolchains/toolchains.h"
#include "disasm_verify.h"

// how long the build, each reading and Ghidra's analysis may take, in milliseconds
#define CLI_DISASM_BUILD_PATIENCE (30ull * 60ull * 1000ull)

#define CLI_DISASM_READ_PATIENCE (5ull * 60ull * 1000ull)

#define CLI_DISASM_GHIDRA_PATIENCE (30ull * 60ull * 1000ull)

// the longest run PATH
#define CLI_DISASM_PATH_TEXT (1u << 15u)

// the words of the step's program and the PATH it runs with
static CliWords s_words;
static char s_run_path[CLI_DISASM_PATH_TEXT];

void cli_disasm_steps_begun(void)
{
    cli_toolchains_run_path(s_run_path, sizeof(s_run_path));
}

// the words begun anew
static void verify_words_begun(void)
{
    s_words.count = 0u;
    s_words.used = 0u;
    s_words.full = 0;
    s_words.word[0] = NULL;
}

// a program run in `folder` with the toolchains' PATH and `settings` besides, within `patience` ms: what it gave, or
// NULL with why said; a program that ends with a code other than 0 is said with its errors' first bytes
static CliRan *verify_ran(const char *folder, const CliSetting *settings, unsigned int count,
                          unsigned long long patience, CliLine *said)
{
    CliRan *const ran = cli_ran_taken();
    if (!cli_ran_within(s_words.word, folder, settings, count, NULL, 0ull, patience, ran, said))
    {
        cli_ran_given(ran);
        return NULL;
    }
    if (ran->code != 0)
    {
        cli_line_text(said, s_words.word[0]);
        cli_line_text(said, ran->stopped ? " ran past its time" : " ended with ");
        if (!ran->stopped)
        {
            // an exit code is small and positive where it is not a signal's -1
            cli_line_decimal(said, (ran->code < 0) ? 0ull : (unsigned long long)ran->code, 1u);
        }
        cli_line_text(said, ": ");
        cli_line_counted(said, ran->err, (ran->err_size < 600ull) ? ran->err_size : 600ull);
        cli_ran_given(ran);
        return NULL;
    }
    return ran;
}

int cli_disasm_beside(const char *compiler, const char *const *names, char *path, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    unsigned int count = 0u;
    while (names[count] != NULL)
    {
        count += 1u;
    }
    if (cli_path_parent(compiler, folder) && cli_toolchains_program_in(folder, names, count, path))
    {
        return 1;
    }
    cli_line_text(said, names[0]);
    cli_line_text(said, " is not beside ");
    cli_line_text(said, compiler);
    return 0;
}

int cli_disasm_built(const char *top, const char *target, const char *compiler, CliLine *said)
{
    char bash[CLI_PATH_LONGEST];
    char script[CLI_PATH_LONGEST];
    if (!cli_runner_bash(bash, said) || !cli_disasm_path(script, (const char *const[]){top, "/ui/cli/build.sh", NULL}))
    {
        return 0;
    }
    verify_words_begun();
    cli_words_put(&s_words, bash);
    cli_words_put(&s_words, script);
    cli_words_put(&s_words, target);
    const CliSetting settings[3] = {{"PATH", s_run_path}, {"LTO", "1"}, {"CC", compiler}};
    CliRan *const ran = verify_ran(top, settings, 3u, CLI_DISASM_BUILD_PATIENCE, said);
    if (ran == NULL)
    {
        return 0;
    }
    cli_ran_given(ran);
    return 1;
}

unsigned int cli_disasm_functions(int gnu, const char *lifter, const char *object, const char *ir,
                                  char (*names)[CLI_NAME_LONGEST], CliLine *said)
{
    verify_words_begun();
    cli_words_put(&s_words, lifter);
    if (gnu)
    {
        cli_words_put(&s_words, "-list");
        cli_words_put(&s_words, object);
    }
    else
    {
        cli_words_put(&s_words, object);
        cli_words_put(&s_words, "-o");
        cli_words_put(&s_words, ir);
    }
    const CliSetting settings[1] = {{"PATH", s_run_path}};
    CliRan *const ran = verify_ran(NULL, settings, 1u, CLI_DISASM_READ_PATIENCE, said);
    if (ran == NULL)
    {
        return 0u;
    }
    unsigned int count = 0u;
    if (gnu)
    {
        (void)cli_private_file_written(ir, ran->out, ran->out_size);
        for (unsigned long long at = 0ull; (at < ran->out_size) && (count < CLI_DISASM_FUNCTIONS);)
        {
            unsigned long long end = at;
            while ((end < ran->out_size) && (ran->out[end] != '\n'))
            {
                end += 1ull;
            }
            unsigned long long word = end;
            while ((word > at) && (ran->out[word - 1ull] != ' ') && (ran->out[word - 1ull] != '\t'))
            {
                word -= 1ull;
            }
            const int function = ((end - word) > 0ull) && ((end - word) < CLI_NAME_LONGEST) && (ran->out[at] == 'T');
            if (function)
            {
                memor.cpy(&(MemoriaCfg){.dst = names[count], .src = ran->out + word, .bytes = end - word});
                names[count][end - word] = '\0';
                count += 1u;
            }
            at = end + 1ull;
        }
    }
    cli_ran_given(ran);
    if (!gnu)
    {
        unsigned long long size = 0ull;
        const unsigned char *const text = cli_file_held(ir, &size);
        if (text == NULL)
        {
            cli_line_text(said, ir);
            cli_line_text(said, " was not written by the lifter");
            return 0u;
        }
        count = cli_disasm_functions_defined((const char *)text, size, names, CLI_DISASM_FUNCTIONS);
        cli_file_let_go(text, size);
    }
    if (count == 0u)
    {
        cli_line_text(said, "the module defines no function");
    }
    return count;
}

unsigned int cli_disasm_symbols(const char *lister, const char *program, const char (*names)[CLI_NAME_LONGEST],
                                char (*symbols)[CLI_NAME_LONGEST], unsigned int count, CliLine *said)
{
    verify_words_begun();
    cli_words_put(&s_words, lister);
    cli_words_put(&s_words, "--defined-only");
    cli_words_put(&s_words, program);
    const CliSetting settings[1] = {{"PATH", s_run_path}};
    CliRan *const ran = verify_ran(NULL, settings, 1u, CLI_DISASM_READ_PATIENCE, said);
    if (ran == NULL)
    {
        return 0u;
    }
    unsigned int held = 0u;
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        if (cli_disasm_symbol_of(ran->out, ran->out_size, names[each], symbols[each]))
        {
            held += 1u;
        }
        else
        {
            symbols[each][0] = '\0';
        }
    }
    cli_ran_given(ran);
    return held;
}

CliRan *cli_disasm_disassembled(const char *disassembler, const char *program, const char (*symbols)[CLI_NAME_LONGEST],
                                unsigned int count, const char *into, CliLine *said)
{
    static char s_symbol_list[CLI_DISASM_FUNCTIONS * CLI_NAME_LONGEST];
    CliLine listed = {s_symbol_list, sizeof(s_symbol_list), 0ull};
    cli_line_text(&listed, "--disassemble-symbols=");
    int any = 0;
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        if (symbols[each][0] != '\0')
        {
            cli_line_text(&listed, any ? "," : "");
            cli_line_text(&listed, symbols[each]);
            any = 1;
        }
    }
    if (!cli_line_closed(&listed))
    {
        cli_line_text(said, "the symbols do not fit one disassembler command");
        return NULL;
    }
    verify_words_begun();
    cli_words_put(&s_words, disassembler);
    cli_words_put(&s_words, "-d");
    cli_words_put(&s_words, "-M");
    cli_words_put(&s_words, "intel");
    cli_words_put(&s_words, s_symbol_list);
    cli_words_put(&s_words, program);
    const CliSetting settings[1] = {{"PATH", s_run_path}};
    CliRan *const ran = verify_ran(NULL, settings, 1u, CLI_DISASM_READ_PATIENCE, said);
    if (ran != NULL)
    {
        (void)cli_private_file_written(into, ran->out, ran->out_size);
    }
    return ran;
}

const unsigned char *cli_disasm_ghidra(const char *analyzer, const char *const *loader, const char *top,
                                       const char *folder, const char *program, const char (*symbols)[CLI_NAME_LONGEST],
                                       unsigned int count, const char *into, unsigned long long *size, CliLine *said)
{
    char source[CLI_PATH_LONGEST];
    char scripts[CLI_PATH_LONGEST];
    char script[CLI_PATH_LONGEST];
    char names[CLI_PATH_LONGEST];
    char project[CLI_PATH_LONGEST];
    if (!cli_disasm_path(source, (const char *const[]){top, "/ui/plugins/ghidra/FunctionsDisassembled.java", NULL}) ||
        !cli_disasm_path(scripts, (const char *const[]){folder, "/ghidra_scripts", NULL}) ||
        !cli_disasm_path(script, (const char *const[]){folder, "/ghidra_scripts/FunctionsDisassembled.java", NULL}) ||
        !cli_disasm_path(names, (const char *const[]){folder, "/functions.txt", NULL}) ||
        !cli_disasm_path(project, (const char *const[]){folder, "/ghidra_project", NULL}))
    {
        cli_line_text(said, "the loop's paths do not fit");
        return NULL;
    }
    unsigned long long script_size = 0ull;
    const unsigned char *const script_text = cli_file_held(source, &script_size);
    if (script_text == NULL)
    {
        cli_line_text(said, source);
        cli_line_text(said, " could not be read");
        return NULL;
    }
    const int kept = cli_private_folder_made(scripts) && cli_private_folder_made(project) &&
                     cli_private_file_written(script, script_text, script_size);
    cli_file_let_go(script_text, script_size);
    static char s_names_text[CLI_DISASM_FUNCTIONS * CLI_NAME_LONGEST];
    CliLine listed = {s_names_text, sizeof(s_names_text), 0ull};
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        if (symbols[each][0] != '\0')
        {
            cli_line_text(&listed, symbols[each]);
            cli_line_character(&listed, '\n');
        }
    }
    if (!kept || !cli_line_fits(&listed) || !cli_private_file_written(names, s_names_text, listed.at))
    {
        cli_line_text(said, "the script and the names could not be written under ");
        cli_line_text(said, folder);
        return NULL;
    }
    verify_words_begun();
    cli_words_put(&s_words, analyzer);
    cli_words_put(&s_words, project);
    cli_words_put(&s_words, "verify");
    cli_words_put(&s_words, "-import");
    cli_words_put(&s_words, program);
    for (unsigned int each = 0u; loader[each] != NULL; each += 1u)
    {
        cli_words_put(&s_words, loader[each]);
    }
    cli_words_put(&s_words, "-overwrite");
    cli_words_put(&s_words, "-deleteProject");
    cli_words_put(&s_words, "-noanalysis");
    cli_words_put(&s_words, "-scriptPath");
    cli_words_put(&s_words, scripts);
    cli_words_put(&s_words, "-postScript");
    cli_words_put(&s_words, "FunctionsDisassembled.java");
    cli_words_put(&s_words, names);
    cli_words_put(&s_words, into);
    // the output emptied first, so that a script the analyzer could not run leaves nothing to read, not an older run's
    (void)cli_private_file_written(into, "", 0ull);
    const CliSetting settings[1] = {{"PATH", s_run_path}};
    CliRan *const ran = verify_ran(folder, settings, 1u, CLI_DISASM_GHIDRA_PATIENCE, said);
    if (ran == NULL)
    {
        return NULL;
    }
    cli_ran_given(ran);
    const unsigned char *const text = cli_file_held(into, size);
    if ((text == NULL) || (*size == 0ull))
    {
        if (text != NULL)
        {
            cli_file_let_go(text, *size);
        }
        cli_line_text(said, "Ghidra's script wrote nothing to ");
        cli_line_text(said, into);
        cli_line_text(said, ": the analyzer's log in the run's errors says why");
        return NULL;
    }
    return text;
}

int cli_disasm_executed(const char *program, CliLine *said)
{
    verify_words_begun();
    cli_words_put(&s_words, program);
    cli_words_put(&s_words, "help");
    const CliSetting settings[1] = {{"PATH", s_run_path}};
    CliRan *const ran = verify_ran(NULL, settings, 1u, CLI_DISASM_READ_PATIENCE, said);
    if (ran == NULL)
    {
        return 0;
    }
    cli_ran_given(ran);
    return 1;
}
