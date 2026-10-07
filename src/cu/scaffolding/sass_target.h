// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef SASS_TARGET_H
#define SASS_TARGET_H

// The lane as SASS, a language of the register lane (code_generator.h): its ruleset is sass.krs, how SASS is written,
// then sm_86.krs and sm_86.kdm, the part's operations and its registers, and last the forms of sass.ksc, all read out
// of the cell's probes, and its header is the opening a listing carries, asked of nvdisasm. sass_assemble_lines
// assembles its text, and the ruleset leaves empty every form no probe gave. No route writes a program with this generator: the cell's
// probes read it, to write their questions in the machine's own code

#include "../engine/rmc/code_generator.h"

class SassTarget : public CodeGenerator
{
  public:
    SassTarget(void);

    // the part holds one register file and the banks are laid into it end to end: R0 through R235, the registers
    // sm_86.kdm does not pin (R236 is sign_base, R237 the scratch, R238 and R239 the launch, R240 through R252 the
    // lane's fixed words and wides, and RZ is R255)
    unsigned int register_file_holds(void) const override;

    int program_unit_written(void) const override;
};

// the SASS code generator a process holds, its ruleset read at its first call to ruleset()
SassTarget &sass_target(void);

#endif
