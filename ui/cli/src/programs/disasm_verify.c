// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// disasm_verify.c: `orior_cli run disasm-verify <module.c> [--function <name>]... [--toolchain llvm|gcc|apple]
// [--target <triple>]`: the loop of disasm_verify.h run over one module through the toolchain named, LLVM's where none
// is, its steps those of disasm_verify_steps.c. Each program is found as the toolchains window finds it: Ghidra's
// analyzer through the toolchain ghidra, the compiler through its own toolchain, and the lifter, the lister and the
// disassembler beside the compiler, by each name the system gives them, the newest version first. The loop's files go
// under build/harness/disasm_verify/<module> of the tree: the IR, the symbols, the two disassemblies and Ghidra's
// project, which is deleted once read
#include "disasm_verify.h"
#include "../command_line/command_line_internal.h"
#include "../runs/runner.h"
#include "../toolchains/toolchains.h"
#include "executables.h"

// the most names a program is looked for under
#define CLI_DISASM_NAMES 8u

// A toolchain the loop reads a program through: its id as the command names it, the toolchain and program of its
// compiler, whether the compiler is GNU's, whose lifter lists the functions in place of the IR, the names the lifter of
// a module's object to its IR, the lister of a program's symbols and the disassembler go by, the newest version first
// where the system versions them, the compiler build.sh is told, and the words that tell Ghidra's analyzer how to load
// the program so that its addresses are the disassembler's: an ELF that is position independent loaded at 0, where
// Ghidra would lay it at its own base; a Mach-O at the base it names, and no words
typedef struct
{
    const char *id;
    const char *toolchain;
    const char *compiler;
    int gnu;
    const char *const lifter[CLI_DISASM_NAMES];
    const char *const lister[CLI_DISASM_NAMES];
    const char *const disassembler[CLI_DISASM_NAMES];
    const char *const loader[CLI_DISASM_NAMES];
} CliDisasmToolchain;

static const CliDisasmToolchain s_toolchains[] = {
    {"llvm",
     "llvm",
     "clang",
     0,
     {"llvm-dis", "llvm-dis-21", "llvm-dis-20", "llvm-dis-19", "llvm-dis-18", "llvm-dis-17", NULL},
     {"llvm-nm", "llvm-nm-21", "llvm-nm-20", "llvm-nm-19", "llvm-nm-18", "llvm-nm-17", NULL},
     {"llvm-objdump", "llvm-objdump-21", "llvm-objdump-20", "llvm-objdump-19", "llvm-objdump-18", "llvm-objdump-17",
      NULL},
     {"-loader", "ElfLoader", "-loader-imagebase", "0", NULL}},
    {"gcc",
     "gcc",
     "gcc",
     1,
     {"lto-dump", "lto-dump-15", "lto-dump-14", "lto-dump-13", NULL},
     {"nm", NULL},
     {"objdump", NULL},
     {"-loader", "ElfLoader", "-loader-imagebase", "0", NULL}},
    {"apple",
     "llvm",
     "clang",
     0,
     {"llvm-dis", NULL},
     {"llvm-nm", "nm", NULL},
     {"llvm-objdump", "objdump", NULL},
     {NULL}},
};

// what the command holds as it runs: the names of the module's functions and their symbols, what it says to the
// errors and the row it writes
static char s_names[CLI_DISASM_FUNCTIONS][CLI_NAME_LONGEST];
static char s_symbols[CLI_DISASM_FUNCTIONS][CLI_NAME_LONGEST];
static char s_said_text[CLI_SAID_LONGEST];
static char s_out_text[CLI_SAID_LONGEST];

// the module's path within the tree: `module` past the tree's top and its separator where it begins with them, else
// `module` as given, taken as within the tree already
static const char *verify_relative(const char *module, const char *top)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = top, .cap = CLI_PATH_LONGEST});
    const int within = cellul.starts(&(CatenaFinitaCfg){.src = module, .other = top, .cap = CLI_PATH_LONGEST}) &&
                       ((module[length] == '/') || (module[length] == '\\'));
    return within ? module + length + 1ull : module;
}

// the command line's name as bin_names.tsv at the tree's top gives it, its row "cli", into `name`: 1, or 0 where the
// sheet has no such row
static int verify_cli_named(const char *top, char *name)
{
    char path[CLI_PATH_LONGEST];
    CliLine line = {path, sizeof(path), 0ull};
    cli_line_text(&line, top);
    cli_line_text(&line, "/bin_names.tsv");
    if (!cli_line_closed(&line))
    {
        return 0;
    }
    unsigned long long size = 0ull;
    const unsigned char *const text = cli_file_held(path, &size);
    if (text == NULL)
    {
        return 0;
    }
    int found = 0;
    for (unsigned long long at = 0ull; !found && (at < size);)
    {
        unsigned long long end = at;
        while ((end < size) && (text[end] != '\n'))
        {
            end += 1ull;
        }
        const int row =
            ((end - at) > 4ull) &&
            cellul.starts(&(CatenaFinitaCfg){.src = (const char *)text + at, .other = "cli\t", .cap = end - at});
        if (row)
        {
            // the row's second column alone: up to the next tab, or the line's end without its carriage return
            const unsigned long long value = at + 4ull;
            unsigned long long value_end = value;
            while ((value_end < end) && (text[value_end] != '\t') && (text[value_end] != '\r'))
            {
                value_end += 1ull;
            }
            const unsigned long long length = value_end - value;
            found = length < CLI_NAME_LONGEST;
            if (found)
            {
                memor.cpy(&(MemoriaCfg){.dst = name, .src = text + value, .bytes = length});
                name[length] = '\0';
            }
        }
        at = end + 1ull;
    }
    cli_file_let_go(text, size);
    return found;
}

int cli_disasm_path(char *path, const char *const *parts)
{
    CliLine line = {path, CLI_PATH_LONGEST, 0ull};
    for (unsigned int each = 0u; parts[each] != NULL; each += 1u)
    {
        cli_line_text(&line, parts[each]);
    }
    return cli_line_closed(&line) != 0;
}

// the module's object's name as build.sh names it, the path within the tree with each separator as two underscores
static void verify_object_named(const char *relative, CliLine *line)
{
    for (const char *letter = relative; *letter != '\0'; letter += 1)
    {
        if ((*letter == '/') || (*letter == '\\'))
        {
            cli_line_text(line, "__");
        }
        else
        {
            cli_line_character(line, *letter);
        }
    }
}

// one function's row written to the output: its name, how many instructions each reader gave, and the verdict: same,
// or where the readings differ, each reader's index, address and mnemonic there
static void verify_row(const char *name, const CliDisasmReading *llvm, const CliDisasmReading *ghidra, int same,
                       unsigned int at, unsigned int ghidra_at)
{
    CliLine line = {s_out_text, sizeof(s_out_text), 0ull};
    cli_line_text(&line, name);
    cli_line_character(&line, '\t');
    cli_line_decimal(&line, llvm->count, 1u);
    cli_line_character(&line, '\t');
    cli_line_decimal(&line, ghidra->count, 1u);
    cli_line_character(&line, '\t');
    if (same)
    {
        cli_line_text(&line, "same");
    }
    else if (ghidra->count == 0u)
    {
        cli_line_text(&line, "Ghidra found no function");
    }
    else
    {
        cli_line_text(&line, "differ at ");
        cli_line_decimal(&line, at, 1u);
        if (at < llvm->count)
        {
            cli_line_text(&line, ": 0x");
            cli_line_hex(&line, llvm->instruction[at].address, 1u);
            cli_line_character(&line, ' ');
            cli_line_text(&line, llvm->instruction[at].mnemonic);
        }
        if (ghidra_at < ghidra->count)
        {
            cli_line_text(&line, " against ");
            cli_line_decimal(&line, ghidra_at, 1u);
            cli_line_text(&line, ": 0x");
            cli_line_hex(&line, ghidra->instruction[ghidra_at].address, 1u);
            cli_line_character(&line, ' ');
            cli_line_text(&line, ghidra->instruction[ghidra_at].mnemonic);
        }
    }
    cli_out_line(s_out_text, cli_line_fits(&line) ? line.at : 0ull);
}

// the toolchain `id` names, LLVM's where it names none: its row, or NULL with which said
static const CliDisasmToolchain *verify_toolchain(const char *id, CliLine *said)
{
    for (unsigned int each = 0u; each < (sizeof(s_toolchains) / sizeof(s_toolchains[0])); each += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = id, .other = s_toolchains[each].id, .cap = CLI_NAME_LONGEST}))
        {
            return &s_toolchains[each];
        }
    }
    cli_line_text(said, "disasm-verify reads through llvm, gcc or apple, not ");
    cli_line_text(said, id);
    return NULL;
}

int cli_disasm_verify_words(const char *const *words, unsigned int count)
{
    CliLine said = {s_said_text, sizeof(s_said_text), 0ull};
    const char *module = NULL;
    const char *toolchain_id = "llvm";
    const char *target = "x86_64-unknown-linux-gnu";
    const char *wanted[CLI_DISASM_FUNCTIONS];
    unsigned int wanted_count = 0u;
    for (unsigned int each = 0u; each < count; each += 1u)
    {
        const int function =
            cellul.eq(&(CatenaFinitaCfg){.src = words[each], .other = "--function", .cap = CLI_NAME_LONGEST});
        const int toolchain =
            cellul.eq(&(CatenaFinitaCfg){.src = words[each], .other = "--toolchain", .cap = CLI_NAME_LONGEST});
        const int triple =
            cellul.eq(&(CatenaFinitaCfg){.src = words[each], .other = "--target", .cap = CLI_NAME_LONGEST});
        if ((function || toolchain || triple) && (each + 1u < count))
        {
            each += 1u;
            if (function && (wanted_count < CLI_DISASM_FUNCTIONS))
            {
                wanted[wanted_count] = words[each];
                wanted_count += 1u;
            }
            toolchain_id = toolchain ? words[each] : toolchain_id;
            target = triple ? words[each] : target;
        }
        else
        {
            module = words[each];
        }
    }
    if (module == NULL)
    {
        cli_err("disasm-verify takes the module to verify, and --function, --toolchain and --target after it");
        return CLI_NO_CODE;
    }
    const CliDisasmToolchain *const toolchain = verify_toolchain(toolchain_id, &said);
    char top[CLI_PATH_LONGEST];
    char cli_name[CLI_NAME_LONGEST];
    if ((toolchain == NULL) || !cli_tree_found(top) || !verify_cli_named(top, cli_name))
    {
        (void)cli_line_closed(&said);
        cli_err((said.at > 0ull) ? s_said_text : "no orior tree holds the working folder, and ORIOR_ROOT names none");
        return CLI_NO_CODE;
    }
    const char *const relative = verify_relative(module, top);
    char compiler[CLI_PATH_LONGEST];
    char analyzer[CLI_PATH_LONGEST];
    char lifter[CLI_PATH_LONGEST];
    char lister[CLI_PATH_LONGEST];
    char disassembler[CLI_PATH_LONGEST];
    cli_disasm_steps_begun();
    if (!cli_executables_program(toolchain->toolchain, toolchain->compiler, compiler, &said) ||
        !cli_executables_program("ghidra", "analyzeHeadless", analyzer, &said) ||
        !cli_disasm_beside(compiler, toolchain->lifter, lifter, &said) ||
        !cli_disasm_beside(compiler, toolchain->lister, lister, &said) ||
        !cli_disasm_beside(compiler, toolchain->disassembler, disassembler, &said))
    {
        (void)cli_line_closed(&said);
        cli_err(s_said_text);
        return CLI_NO_CODE;
    }
    char folder[CLI_PATH_LONGEST];
    char object[CLI_PATH_LONGEST];
    char program[CLI_PATH_LONGEST];
    char ir[CLI_PATH_LONGEST];
    char llvm_text[CLI_PATH_LONGEST];
    char ghidra_text[CLI_PATH_LONGEST];
    CliLine object_line = {object, sizeof(object), 0ull};
    cli_line_text(&object_line, top);
    cli_line_text(&object_line, "/build/cli/");
    cli_line_text(&object_line, target);
    cli_line_text(&object_line, "-lto/objects/");
    verify_object_named(relative, &object_line);
    cli_line_text(&object_line, ".o");
    CliLine folder_line = {folder, sizeof(folder), 0ull};
    cli_line_text(&folder_line, top);
    cli_line_text(&folder_line, "/build/harness/disasm_verify/");
    verify_object_named(relative, &folder_line);
    if (!cli_line_closed(&object_line) || !cli_line_closed(&folder_line) || !cli_private_folder_made(folder) ||
        !cli_disasm_path(program, (const char *const[]){top, "/build/cli/", target, "-lto/", cli_name, NULL}) ||
        !cli_disasm_path(ir, (const char *const[]){folder, "/module.ll", NULL}) ||
        !cli_disasm_path(llvm_text, (const char *const[]){folder, "/llvm.txt", NULL}) ||
        !cli_disasm_path(ghidra_text, (const char *const[]){folder, "/ghidra.txt", NULL}))
    {
        cli_err("the loop's paths do not fit, or its folder could not be made under build/harness/disasm_verify");
        return CLI_NO_CODE;
    }
    unsigned int functions = 0u;
    if (!cli_disasm_built(top, target, compiler, &said) ||
        ((functions = cli_disasm_functions(toolchain->gnu, lifter, object, ir, s_names, &said)) == 0u))
    {
        (void)cli_line_closed(&said);
        cli_err(s_said_text);
        return CLI_NO_CODE;
    }
    // the functions named on the command line alone, where any were
    unsigned int kept = 0u;
    for (unsigned int each = 0u; each < functions; each += 1u)
    {
        int asked = wanted_count == 0u;
        for (unsigned int one = 0u; !asked && (one < wanted_count); one += 1u)
        {
            asked = cellul.eq(&(CatenaFinitaCfg){.src = s_names[each], .other = wanted[one], .cap = CLI_NAME_LONGEST});
        }
        if (asked)
        {
            memor.cpy(&(MemoriaCfg){.dst = s_names[kept], .src = s_names[each], .bytes = CLI_NAME_LONGEST});
            kept += 1u;
        }
    }
    functions = kept;
    if ((cli_disasm_symbols(lister, program, s_names, s_symbols, functions, &said) == 0u) ||
        !cli_disasm_executed(program, &said))
    {
        (void)cli_line_closed(&said);
        cli_err(cli_line_fits(&said) && (said.at > 0ull) ? s_said_text
                                                         : "the program holds none of the module's functions");
        return CLI_NO_CODE;
    }
    CliRan *const disassembled = cli_disasm_disassembled(disassembler, program, s_symbols, functions, llvm_text, &said);
    unsigned long long ghidra_size = 0ull;
    const unsigned char *const ghidra = (disassembled != NULL)
                                            ? cli_disasm_ghidra(analyzer, toolchain->loader, top, folder, program,
                                                                s_symbols, functions, ghidra_text, &ghidra_size, &said)
                                            : NULL;
    CliDisasmReading one = {cli_cell_taken(CLI_DISASM_INSTRUCTIONS * sizeof(CliDisasmInstruction),
                                           "a function's instructions as LLVM reads them"),
                            0u, 0};
    CliDisasmReading other = {cli_cell_taken(CLI_DISASM_INSTRUCTIONS * sizeof(CliDisasmInstruction),
                                             "a function's instructions as Ghidra reads them"),
                              0u, 0};
    int code = 0;
    if ((ghidra == NULL) || (one.instruction == NULL) || (other.instruction == NULL))
    {
        (void)cli_line_closed(&said);
        cli_err((ghidra == NULL) ? s_said_text : "the readings' cells could not be taken");
        code = CLI_NO_CODE;
    }
    for (unsigned int each = 0u; (code != CLI_NO_CODE) && (each < functions); each += 1u)
    {
        if (s_symbols[each][0] == '\0')
        {
            continue;
        }
        (void)cli_disasm_objdump_read(disassembled->out, disassembled->out_size, s_symbols[each], &one);
        (void)cli_disasm_ghidra_read((const char *)ghidra, ghidra_size, s_symbols[each], &other);
        unsigned int at = 0u;
        unsigned int other_at = 0u;
        const int same = cli_disasm_same(&one, &other, &at, &other_at);
        verify_row(s_symbols[each], &one, &other, same, at, other_at);
        code = same ? code : 1;
    }
    cli_cell_given(one.instruction);
    cli_cell_given(other.instruction);
    if (ghidra != NULL)
    {
        cli_file_let_go(ghidra, ghidra_size);
    }
    if (disassembled != NULL)
    {
        cli_ran_given(disassembled);
    }
    return code;
}
