// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef YOSYS_SCRIPT_H
#define YOSYS_SCRIPT_H

// The synthesis by Yosys of a lane that GHDL wrote as Verilog, as a script that its ruleset yosys.krs writes. The
// ruleset decides which passes run, not the caller. It writes no lane: the call program() is always empty, and the
// call synthesis() writes the script

#include "../engine/rmc/target.h"

class YosysScript : public Target
{
  public:
    YosysScript(void);

    int program(const EngineRecordLayout *layout, const TargetInfo *target, const char *header, PlatformText *text,
                unsigned int *places, unsigned int *live) override;

    // The script that reads `source`, synthesizes it from the module `top` and measures it, onto `script`. Where
    // `memories` is not empty, the script writes the statistics of the coarse netlist to the log `memories` before the
    // fine stage. The return is 1 where it wrote the script, and 0 where the read of the ruleset failed or the ruleset
    // writes it broken
    int synthesis(const char *source, const char *top, const char *memories, PlatformText *script);
};

// the Yosys script a process retains, its ruleset read at its first call to ruleset()
YosysScript &yosys_script(void);

#endif
