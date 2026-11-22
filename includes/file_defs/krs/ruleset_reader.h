// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef RULESET_READER_H
#define RULESET_READER_H

// What the base of the code generator (target_*.cu) and the code generator of each ruleset (code_generator.cu) share.
// It holds a ruleset as read, the schema that a language reads it against, and the writer that a language writes its
// forms with

#include "../../../src/cu/engine/rmc/target.h"

// a form, a bank or a fixed register as a .krs file writes it, and the parameters it takes
struct RulesetName
{
    const char *text;
    unsigned int parameters;
};

#define OPCODE_WRITTEN(name_, text_, parameters_, type_) {text_, parameters_},
#define REGCLASS_WRITTEN(name_, text_) {text_, 1u},
#define PHYSREG_WRITTEN(name_, text_) {text_, 0u},

// what a code generator needs from its ruleset. That is the file of the read, and the toolchain and header of its path.
// Each is NULL where it takes the one that the file names. It is also the forms, banks and fixed registers
// that the code generator names
struct RulesetSchema
{
    const char *file;
    const char *toolchain;
    const char *header;
    const RulesetName *forms;
    unsigned int form_count;
    const RulesetName *banks;
    unsigned int bank_count;
    const RulesetName *fixed;
    unsigned int fixed_count;
};

// a text cut at its parameters. Each pieces[k] comes before the argument of parameter placeholders[k]. The last piece
// comes after the last argument, which gives one piece more than placeholders. A terminator ends each piece
struct InstrTemplate
{
    const char **pieces;
    unsigned int *placeholders;
    unsigned int placeholder_count;
};

// what one argument of the line of a construct is. It is text written as it is, or the own parameter of the construct
// at a placeholder. Else it is a scratch register of a bank, which `number` names within one write of the construct
enum PseudoOperandType
{
    PSEUDO_TEXT = 0,
    PSEUDO_PARAMETER = 1,
    PSEUDO_SCRATCH = 2
};

struct PseudoOperand
{
    PseudoOperandType type;
    unsigned int placeholder;
    unsigned int number;
    const char *text;
};

// one line of a construct: a form, or a construct given before it in the file, and its arguments
struct PseudoLine
{
    unsigned int form;
    PseudoOperand *arguments;
    unsigned int argument_count;
};

// a form built from more basic forms (engine_table.md item 11(f) 5). Where the own instruction of the target for a form
// leaves its rules, the ruleset gives the form as a construct. The construct has the same name and parameters. Each
// write of the form writes the lines of the construct. No lines is the form as its text gives it
struct Pseudo
{
    PseudoLine *lines;
    unsigned int line_count;
};

// A scratch register that a construct takes. It is a fresh register of the bank at that place in the schema, which
// `taken` writes onto `name`. `taken` answers 0, and writes nothing, where the writer has none of that bank to give
struct ScratchRegisters
{
    int (*taken)(void *context, unsigned int bank, VerbaLinea *name);
    void *context;
};

// the bytes of the type of a fold, its terminator among them
#define RULESET_FOLD_TYPE_BYTES 16u

// a fold that the system does of a form, as the classification beside the ruleset gives it on the compile channel. It
// holds the form by its place in the schema, and what the caller put for the parameter ("number" or "register"). It
// also holds the parameter by its place
struct RulesetFold
{
    unsigned int form;
    char type[RULESET_FOLD_TYPE_BYTES];
    unsigned int parameter;
};

// the bytes of the reason that a read of a ruleset failed, its terminator among them
#define RULESET_ERROR_BYTES 512u

// A ruleset read from its file against the schema of its code generator:
//   - the path of the read, and the reason for an error where there was one.
//   - its own name, the toolchain that builds its text, and where its header comes from.
//   - the shared, write_ports and part lines that it gives, each empty where it gives none.
//   - the written form of a register of each bank, the written form of each fixed register, and each form, by their
//     places in the schema.
//   - which of them the file gave, to find one given twice or left out.
//
// Every list and text after `error` is in `cell`, which ruleset_released gives back
struct Ruleset
{
    const RulesetSchema *schema;
    int tried;
    int ready;
    char path[PLATFORM_PATH_BYTES];
    char error[RULESET_ERROR_BYTES];
    const char *name;
    const char *toolchain;
    const char *header;
    const char *shared;
    const char *write_ports;
    const char *part;
    InstrTemplate *banks;
    const char **fixed;
    InstrTemplate *forms;
    Pseudo *constructs;
    unsigned char *bank_given;
    unsigned char *fixed_given;
    unsigned char *form_given;
    // the construct in the read, its place among the forms (the form count where there is none), and the names of its
    // parameters
    unsigned int building;
    const char **building_parameters;
    unsigned int building_parameter_count;
    void *cell;
    // the folds of the .ksc of the ruleset's name beside it, a RulesetFold each, none where there is no such file
    PlatformArray folds;
};

// the cells of `rules` given back, and every list and text of it empty
void ruleset_released(Ruleset *rules);

// the folds of the .ksc of the name of `rules` beside its file, read into rules->folds in place of its old contents.
// Each line is `compile folds <word> <form>: the system folds the <type> put for <parameter>`. The code finds the
// parameter by its name in the own line of the ruleset for the form. The code skips a line that names a form or
// parameter that the ruleset does not give
void ruleset_folds_read(Ruleset *rules);

// the place that the parameter `parameter` takes among the parameters of the form `form`, in the order that the
// ruleset's own line for the form gives them. RULESET_NO_PARAMETER where the ruleset gives no such line, or the line no
// such parameter
#define RULESET_NO_PARAMETER 0xFFFFFFFFu

unsigned int ruleset_parameter_place(const Ruleset *rules, const char *form, const char *parameter);

// Form `name` of `rules` written onto `text`, its `count` arguments in the order of its parameters. The scratch of a
// construct comes from `scratch`. Where the arguments are not as many as the form takes, the call sets `broken` and
// writes nothing. A `scratch` of NULL is a writer with no scratch to give: a construct that takes scratch breaks the
// writing
void ruleset_write_taking(const Ruleset *rules, VerbaLinea *text, unsigned int name, const char *const *arguments,
                          unsigned int count, const ScratchRegisters *scratch, int *broken);

// The scratch that each form of `rules` takes in one write, four words a form, by its place in the schema. The caller
// gives the cell back. NULL where the pool cannot meet the cell. The words are the registers that it takes of
// the banks at places banks[0], banks[1] and banks[2]. The fourth is 1 where it takes one of any other bank.
//
// A form that the ruleset gives as its text takes none. A construct takes each scratch register that it names once,
// and what each form that it writes takes
unsigned int *ruleset_scratch(const Ruleset *rules, const unsigned int *banks);

#endif
