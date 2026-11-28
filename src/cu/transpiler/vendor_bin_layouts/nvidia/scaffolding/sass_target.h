// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef SASS_TARGET_H
#define SASS_TARGET_H

// The lane as SASS, a language of the register lane (code_generator.h). Its ruleset is sass.krs, how to write SASS.
// Then come sm_86.krs and sm_86.kdm, the operations of the part and its registers, and last the forms of sass.ksc. The
// probes of the cell read all of them. Its header is the opening that a listing carries, queried from nvdisasm.
//
// The call sass_assemble_lines assembles its text, and the ruleset leaves empty every form that no probe gave. No
// route writes a program with this generator. The probes of the cell read it, to write their questions in the own
// code of the machine

#include "../../../../engine/rmc/code_generator.h"

class SassTarget : public CodeGenerator
{
  public:
    SassTarget(void);

    // the part has one register file, and the banks go into it end to end. They are R0 through R235, the registers that
    // sm_86.kdm does not pin. R236 is sign_base, R237 the scratch, and R238 and R239 the launch. R240 through R252 are
    // the fixed words and wides of the lane, and RZ is R255
    unsigned int register_file_size(void) const override;

    int program_unit_written(void) const override;
};

// the SASS code generator a process owns, its ruleset read at its first call to ruleset()
SassTarget &sass_target(void);

#endif
