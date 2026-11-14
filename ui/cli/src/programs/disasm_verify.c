// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// disasm_verify.c: `imp run disasm-verify <module.c> [--function <name>]... [--toolchain llvm|gcc|apple]
// [--target <triple>]`: the loop of disasm_verify.h run over one module through the toolchain named, LLVM's where none
// is, its steps those of disasm_verify_steps.c. Each program is found as the toolchains window finds it: Ghidra's
// analyzer through the toolchain ghidra, the compiler through its own toolchain, and the lifter, the lister and the
// disassembler beside the compiler, by each name the system gives them, the newest version first. The loop's files go
// under build/harness/disasm_verify/<module> of the tree: the IR, the symbols, the two disassemblies and Ghidra's
// project, which is deleted once read
#include "disasm_verify.h"
#include "../command_line/command_line_internal.h"
#include "../command_line/pool.h"
#include "../runs/runner.h"
#include "../text/text.h"
#include "../toolchains/toolchains.h"
#include "executables.h"

// the most names a program is looked for under
#define CLI_DISASM_NAMES 8u

// A toolchain the loop reads a program through: its id as the command names it, the toolchain and program of its
// compiler, whether the compiler is GNU's, whose lifter lists the functions in place of the IR, the names the lifter of
// a module's object to its IR, the lister of a program's symbols and the disassembler go by, the newest version first
// where the system versions them, the compiler build.sh is given, and the words that instruct Ghidra's analyzer how to
// load the program so that its addresses are the disassembler's: an ELF that is position independent loaded at 0, where
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

// what the command stores as it runs: the names of the module's functions and their symbols, what it prints to the
// errors and the row it writes
static char s_names[CLI_DISASM_FUNCTIONS][CLI_NAME_MAX_LEN];
static char s_symbols[CLI_DISASM_FUNCTIONS][CLI_NAME_MAX_LEN];
static char s_err_buf[CLI_ERR_MSG_MAX_LEN];
static char s_out_text[CLI_ERR_MSG_MAX_LEN];

typedef struct
{
    const char *module;
    const char *top;
    char *name;
    const char *relative;
    VerbaLinea *line;
    const char *name_read;
    const CliDisasmReadState *llvm;
    const CliDisasmReadState *ghidra;
    int same;
    unsigned int at;
    unsigned int ghidra_at;
    const char *id;
    VerbaLinea *err_msg;
    const char *const *words;
    unsigned int count;
    const char **toolchain_out;
    const char **target_out;
    const char **required;
    unsigned int requested;
    unsigned int *requested_out;
    unsigned int functions;
    const char *objdump_text;
    unsigned long long objdump_size;
    const char *ghidra_text;
    unsigned long long ghidra_size;
    CliDisasmReadState *one;
    CliDisasmReadState *other;
} DisasmVerifyCtx;
// the module's path within the tree: `module` past the tree's top and its separator where it begins with them, else
// `module` as given, taken as within the tree already
EMBED_INLINE const char *verify_relative(const DisasmVerifyCtx *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->top, .cap = CLI_PATH_MAX_LEN});
    const int within =
        cellul.starts(&(CatenaFinitaCfg){.src = args->module, .other = args->top, .cap = CLI_PATH_MAX_LEN}) &&
        ((args->module[length] == '/') || (args->module[length] == '\\'));
    return within ? args->module + length + 1ULL : args->module;
}

// the command line's name as bin_names.tsv at the tree's top gives it, its row "cli", into `name`: 1, or 0 where the
// sheet has no such row
EMBED_INLINE int verify_cli_by_name(const DisasmVerifyCtx *args)
{
    char path[CLI_PATH_MAX_LEN];
    VerbaLinea line = {path, sizeof(path), 0ULL};
    mmgr_line_put(&line, args->top);
    mmgr_line_put(&line, "/bin_names.tsv");
    if (!mmgr_line_finish(&line))
    {
        return 0;
    }
    unsigned long long size = 0ULL;
    const unsigned char *const text = EMBED_CALL(cli_path_internal.file_alloc, CliPathCtx, .path = path,
                                                 .size_pointer = &size, .owner = g_cli_programs_owner);
    if (text == NULL)
    {
        return 0;
    }
    int found = 0;
    for (unsigned long long at = 0ULL; !found && (at < size);)
    {
        unsigned long long end = at;
        end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = (const char *)text,
                         .at = end, .size = size, .set = "\n");
        const int row =
            ((end - at) > 4ULL) &&
            cellul.starts(&(CatenaFinitaCfg){.src = (const char *)text + at, .other = "cli\t", .cap = end - at});
        if (row)
        {
            // the row's second column alone: up to the next tab, or the line's end without its carriage return
            const unsigned long long value = at + 4ULL;
            unsigned long long value_end = value;
            value_end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = (const char *)text,
                                   .at = value_end, .size = end, .set = "\t\r");
            const unsigned long long length = value_end - value;
            found = length < CLI_NAME_MAX_LEN;
            if (found)
            {
                memor.cpy(&(MemoriaCfg){.dst = args->name, .src = text + value, .bytes = length});
                args->name[length] = '\0';
            }
        }
        at = end + 1ULL;
    }
    g_cli_guard->persistent_buf_release((void *)(embed_word)text, (g_cli_programs_owner));
    return found;
}

int cli_disasm_path(const CliDisasmVerifyCfg *args)
{
    VerbaLinea line = {args->path, CLI_PATH_MAX_LEN, 0ULL};
    for (unsigned int each = 0u; args->parts[each] != NULL; each += 1u)
    {
        mmgr_line_put(&line, args->parts[each]);
    }
    return mmgr_line_finish(&line) != 0;
}

// the module's object's name as build.sh names it, the path within the tree with each separator as two underscores
EMBED_INLINE void verify_object_by_name(const DisasmVerifyCtx *args)
{
    EMBED_CALL(cli_text_set_table.append_replace, CliTextSetCfg, .out = args->line, .text = args->relative,
               .set = "/\\", .with = "__");
}

// one function's row written to the output: its name, how many instructions each reader gave, and the verdict: same,
// or where the readings differ, each reader's index, address and mnemonic there
EMBED_INLINE void verify_row(const DisasmVerifyCtx *args)
{
    VerbaLinea line = {s_out_text, sizeof(s_out_text), 0ULL};
    mmgr_line_put(&line, args->name_read);
    mmgr_line_ch(&line, '\t');
    mmgr_line_decimal(&line, args->llvm->count, 1u);
    mmgr_line_ch(&line, '\t');
    mmgr_line_decimal(&line, args->ghidra->count, 1u);
    mmgr_line_ch(&line, '\t');
    if (args->same)
    {
        mmgr_line_put(&line, "same");
    }
    else if (args->ghidra->count == 0u)
    {
        mmgr_line_put(&line, "Ghidra found no function");
    }
    else
    {
        mmgr_line_put(&line, "differ at ");
        mmgr_line_decimal(&line, args->at, 1u);
        if (args->at < args->llvm->count)
        {
            mmgr_line_put(&line, ": 0x");
            mmgr_line_hex(&line, args->llvm->instruction[args->at].address, 1u);
            mmgr_line_ch(&line, ' ');
            mmgr_line_put(&line, args->llvm->instruction[args->at].mnemonic);
        }
        if (args->ghidra_at < args->ghidra->count)
        {
            mmgr_line_put(&line, " against ");
            mmgr_line_decimal(&line, args->ghidra_at, 1u);
            mmgr_line_put(&line, ": 0x");
            mmgr_line_hex(&line, args->ghidra->instruction[args->ghidra_at].address, 1u);
            mmgr_line_ch(&line, ' ');
            mmgr_line_put(&line, args->ghidra->instruction[args->ghidra_at].mnemonic);
        }
    }
    EMBED_CALL(cli_command_line_text_size_internal.out_line, CliCommandLineTextSizeCtx, .text = s_out_text,
               .size = mmgr_line_ok(&line) ? line.at : 0ULL);
}

// the toolchain `id` names, LLVM's where it names none: its row, or NULL with which given
EMBED_INLINE const CliDisasmToolchain *verify_toolchain(const DisasmVerifyCtx *args)
{
    for (unsigned int each = 0u; each < (sizeof(s_toolchains) / sizeof(s_toolchains[0])); each += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = args->id, .other = s_toolchains[each].id, .cap = CLI_NAME_MAX_LEN}))
        {
            return &s_toolchains[each];
        }
    }
    mmgr_line_put(args->err_msg, "disasm-verify reads through llvm, gcc or apple, not ");
    mmgr_line_put(args->err_msg, args->id);
    return NULL;
}

// the command's `count` words read: the toolchain and the target their flags name into `toolchain_out` and
// `target_out`, and each function a --function names into `required`, up to CLI_DISASM_FUNCTIONS of them, their count
// into `requested_out`; the module, the last word no flag takes, or NULL where every word is a flag's
EMBED_INLINE const char *verify_words_read(const DisasmVerifyCtx *args)
{
    const char *module = NULL;
    for (unsigned int each = 0u; each < args->count; each += 1u)
    {
        const int function =
            cellul.eq(&(CatenaFinitaCfg){.src = args->words[each], .other = "--function", .cap = CLI_NAME_MAX_LEN});
        const int toolchain =
            cellul.eq(&(CatenaFinitaCfg){.src = args->words[each], .other = "--toolchain", .cap = CLI_NAME_MAX_LEN});
        const int triple =
            cellul.eq(&(CatenaFinitaCfg){.src = args->words[each], .other = "--target", .cap = CLI_NAME_MAX_LEN});
        if ((function || toolchain || triple) && ((each + 1u) < args->count))
        {
            each += 1u;
            if (function && (*args->requested_out < CLI_DISASM_FUNCTIONS))
            {
                args->required[*args->requested_out] = args->words[each];
                *args->requested_out += 1u;
            }
            *args->toolchain_out = toolchain ? args->words[each] : *args->toolchain_out;
            *args->target_out = triple ? args->words[each] : *args->target_out;
        }
        else
        {
            module = args->words[each];
        }
    }
    return module;
}

// the module's `functions` names in s_names narrowed, in their order, to those of the `requested` names in `required`,
// every one where none is: how many are left
EMBED_INLINE unsigned int verify_requested_only(const DisasmVerifyCtx *args)
{
    unsigned int named_count = 0u;
    for (unsigned int each = 0u; each < args->functions; each += 1u)
    {
        int all_requested = args->requested == 0u;
        unsigned int one = 0u;
        while (!all_requested && (one < args->requested))
        {
            all_requested = cellul.eq(
                &(CatenaFinitaCfg){.src = s_names[each], .other = args->required[one], .cap = CLI_NAME_MAX_LEN});
            one += 1u;
        }
        if (all_requested)
        {
            memor.cpy(&(MemoriaCfg){.dst = s_names[named_count], .src = s_names[each], .bytes = CLI_NAME_MAX_LEN});
            named_count += 1u;
        }
    }
    return named_count;
}

// each of the `functions` with a symbol in s_symbols read from the disassembler's text and from Ghidra's into `one` and
// `other`, the two readings compared and the function's row written: 1 where any function's readings differ, else 0
EMBED_INLINE int verify_readings_compared(const DisasmVerifyCtx *args)
{
    int code = 0;
    for (unsigned int each = 0u; each < args->functions; each += 1u)
    {
        if (s_symbols[each][0] != '\0')
        {
            (void)EMBED_CALL(cli_disasm_verify_size.disasm_objdump_read, CliDisasmVerifySizeCfg,
                             .text = args->objdump_text, .size = args->objdump_size, .symbol = s_symbols[each],
                             .read_state = args->one);
            (void)EMBED_CALL(cli_disasm_verify_size.disasm_ghidra_read, CliDisasmVerifySizeCfg,
                             .text = args->ghidra_text, .size = args->ghidra_size, .symbol = s_symbols[each],
                             .read_state = args->other);
            unsigned int at = 0u;
            unsigned int other_at = 0u;
            const int same = EMBED_CALL(cli_disasm_verify_one.disasm_same, CliDisasmVerifyOneCfg, .one = args->one,
                                        .other = args->other, .at = &at, .other_at = &other_at);
            EMBED_CALL(verify_row, DisasmVerifyCtx, .name_read = s_symbols[each], .llvm = args->one,
                       .ghidra = args->other, .same = same, .at = at, .ghidra_at = other_at);
            code = same ? code : 1;
        }
    }
    return code;
}

int cli_disasm_verify_words(const CliDisasmVerifyWordsCfg *args)
{
    VerbaLinea err_msg = {s_err_buf, sizeof(s_err_buf), 0ULL};
    const char *toolchain_id = "llvm";
    const char *target = "x86_64-unknown-linux-gnu";
    const char *required[CLI_DISASM_FUNCTIONS];
    unsigned int requested_count = 0u;
    const char *const module = EMBED_CALL(verify_words_read, DisasmVerifyCtx, .words = args->words,
                                          .count = args->count, .toolchain_out = &toolchain_id, .target_out = &target,
                                          .required = required, .requested_out = &requested_count);
    if (module == NULL)
    {
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx,
                   .text =
                       "disasm-verify takes the module to verify, and --function, --toolchain and --target after it");
        return CLI_NO_CODE;
    }
    const CliDisasmToolchain *const toolchain =
        EMBED_CALL(verify_toolchain, DisasmVerifyCtx, .id = toolchain_id, .err_msg = &err_msg);
    char top[CLI_PATH_MAX_LEN];
    char cli_name[CLI_NAME_MAX_LEN];
    if ((toolchain == NULL) || !EMBED_CALL(cli_tree_internal.tree_find, CliTreeCtx, .tree = top) ||
        !EMBED_CALL(verify_cli_by_name, DisasmVerifyCtx, .top = top, .name = cli_name))
    {
        (void)mmgr_line_finish(&err_msg);
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx,
                   .text = (err_msg.at > 0ULL)
                               ? s_err_buf
                               : "no orior tree contains the working folder, and ORIOR_ROOT names none");
        return CLI_NO_CODE;
    }
    const char *const relative = EMBED_CALL(verify_relative, DisasmVerifyCtx, .module = module, .top = top);
    char compiler[CLI_PATH_MAX_LEN];
    char analyzer[CLI_PATH_MAX_LEN];
    char lifter[CLI_PATH_MAX_LEN];
    char lister[CLI_PATH_MAX_LEN];
    char disassembler[CLI_PATH_MAX_LEN];
    cli_disasm_steps_begin();
    if (!EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = toolchain->toolchain,
                    .name = toolchain->compiler, .path = compiler, .err_msg = &err_msg) ||
        !EMBED_CALL(cli_executables_tool.program, CliExecutablesToolCfg, .tool = "ghidra", .name = "analyzeHeadless",
                    .path = analyzer, .err_msg = &err_msg) ||
        !EMBED_CALL(cli_disasm_verify_path.disasm_adjacent, CliDisasmVerifyPathCfg, .compiler = compiler,
                    .names_letters = toolchain->lifter, .path = lifter, .err_msg = &err_msg) ||
        !EMBED_CALL(cli_disasm_verify_path.disasm_adjacent, CliDisasmVerifyPathCfg, .compiler = compiler,
                    .names_letters = toolchain->lister, .path = lister, .err_msg = &err_msg) ||
        !EMBED_CALL(cli_disasm_verify_path.disasm_adjacent, CliDisasmVerifyPathCfg, .compiler = compiler,
                    .names_letters = toolchain->disassembler, .path = disassembler, .err_msg = &err_msg))
    {
        (void)mmgr_line_finish(&err_msg);
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx, .text = s_err_buf);
        return CLI_NO_CODE;
    }
    char folder[CLI_PATH_MAX_LEN];
    char object[CLI_PATH_MAX_LEN];
    char program[CLI_PATH_MAX_LEN];
    char ir[CLI_PATH_MAX_LEN];
    char llvm_text[CLI_PATH_MAX_LEN];
    char ghidra_text[CLI_PATH_MAX_LEN];
    VerbaLinea object_line = {object, sizeof(object), 0ULL};
    mmgr_line_put(&object_line, top);
    mmgr_line_put(&object_line, "/build/cli/");
    mmgr_line_put(&object_line, target);
    mmgr_line_put(&object_line, "/objects/");
    EMBED_CALL(verify_object_by_name, DisasmVerifyCtx, .relative = relative, .line = &object_line);
    mmgr_line_put(&object_line, ".o");
    VerbaLinea folder_line = {folder, sizeof(folder), 0ULL};
    mmgr_line_put(&folder_line, top);
    mmgr_line_put(&folder_line, "/build/harness/disasm_verify/");
    EMBED_CALL(verify_object_by_name, DisasmVerifyCtx, .relative = relative, .line = &folder_line);
    if (!mmgr_line_finish(&object_line) || !mmgr_line_finish(&folder_line) ||
        !EMBED_CALL(cli_words_err_msg_internal.private_folder_init, CliWordsErrMsgCtx, .folder = folder) ||
        !EMBED_CALL(cli_disasm_verify.disasm_path, CliDisasmVerifyCfg, .path = program,
                    .parts = (const char *const[]){top, "/build/cli/", target, "/", cli_name, NULL}) ||
        !EMBED_CALL(cli_disasm_verify.disasm_path, CliDisasmVerifyCfg, .path = ir,
                    .parts = (const char *const[]){folder, "/module.ll", NULL}) ||
        !EMBED_CALL(cli_disasm_verify.disasm_path, CliDisasmVerifyCfg, .path = llvm_text,
                    .parts = (const char *const[]){folder, "/llvm.txt", NULL}) ||
        !EMBED_CALL(cli_disasm_verify.disasm_path, CliDisasmVerifyCfg, .path = ghidra_text,
                    .parts = (const char *const[]){folder, "/ghidra.txt", NULL}))
    {
        EMBED_CALL(
            cli_command_line_internal.err, CliCommandLineCtx,
            .text = "the loop's paths do not fit, or its folder could not be made under build/harness/disasm_verify");
        return CLI_NO_CODE;
    }
    const int built = EMBED_CALL(cli_disasm_verify_compiler.disasm_build, CliDisasmVerifyCompilerCfg, .top = top,
                                 .target = target, .compiler = compiler, .err_msg = &err_msg);
    unsigned int functions =
        built ? EMBED_CALL(cli_disasm_verify_ir.disasm_functions, CliDisasmVerifyIrCfg, .gnu = toolchain->gnu,
                           .lifter = lifter, .object = object, .ir = ir, .names = s_names, .err_msg = &err_msg)
              : 0u;
    if (functions == 0u)
    {
        (void)mmgr_line_finish(&err_msg);
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx, .text = s_err_buf);
        return CLI_NO_CODE;
    }
    functions = EMBED_CALL(verify_requested_only, DisasmVerifyCtx, .functions = functions, .required = required,
                           .requested = requested_count);
    if ((EMBED_CALL(cli_disasm_verify_count_err_msg_lister.disasm_symbols, CliDisasmVerifyCountErrMsgListerCfg,
                    .lister = lister, .program = program, .names_callback = s_names, .symbols_callback = s_symbols,
                    .count = functions, .err_msg = &err_msg) == 0u) ||
        !EMBED_CALL(cli_disasm_verify_count_err_msg.disasm_execute, CliDisasmVerifyCountErrMsgCfg, .program = program,
                    .err_msg = &err_msg))
    {
        (void)mmgr_line_finish(&err_msg);
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx,
                   .text = mmgr_line_ok(&err_msg) && (err_msg.at > 0ULL)
                               ? s_err_buf
                               : "the program contains none of the module's functions");
        return CLI_NO_CODE;
    }
    CliRunResult *const disassembled = EMBED_CALL(
        cli_disasm_verify_count_err_msg.disasm_disassemble, CliDisasmVerifyCountErrMsgCfg, .disassembler = disassembler,
        .program = program, .symbols = s_symbols, .count = functions, .into = llvm_text, .err_msg = &err_msg);
    unsigned long long ghidra_size = 0ULL;
    const unsigned char *const ghidra =
        (disassembled != NULL)
            ? EMBED_CALL(cli_disasm_verify_count.disasm_ghidra, CliDisasmVerifyCountCfg, .analyzer = analyzer,
                         .loader = toolchain->loader, .top = top, .folder = folder, .program = program,
                         .symbols = s_symbols, .count = functions, .into = ghidra_text, .size_pointer = &ghidra_size,
                         .err_msg = &err_msg)
            : NULL;
    CliDisasmReadState one = {
        g_cli_guard->temporary_buf_alloc(CLI_DISASM_INSTRUCTIONS * sizeof(CliDisasmInstruction), g_cli_programs_owner),
        0u, 0};
    CliDisasmReadState other = {
        g_cli_guard->temporary_buf_alloc(CLI_DISASM_INSTRUCTIONS * sizeof(CliDisasmInstruction), g_cli_programs_owner),
        0u, 0};
    int code = CLI_NO_CODE;
    if ((ghidra == NULL) || (one.instruction == NULL) || (other.instruction == NULL))
    {
        (void)mmgr_line_finish(&err_msg);
        EMBED_CALL(cli_command_line_internal.err, CliCommandLineCtx,
                   .text = (ghidra == NULL) ? s_err_buf : "the readings' cells could not be taken");
    }
    else
    {
        code =
            EMBED_CALL(verify_readings_compared, DisasmVerifyCtx, .functions = functions,
                       .objdump_text = disassembled->out, .objdump_size = disassembled->out_size,
                       .ghidra_text = (const char *)ghidra, .ghidra_size = ghidra_size, .one = &one, .other = &other);
    }
    g_cli_guard->persistent_buf_release(one.instruction, g_cli_programs_owner);
    g_cli_guard->persistent_buf_release(other.instruction, g_cli_programs_owner);
    g_cli_guard->persistent_buf_release((void *)(embed_word)ghidra, (g_cli_programs_owner));
    if (disassembled != NULL)
    {
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = disassembled);
    }
    return code;
}
