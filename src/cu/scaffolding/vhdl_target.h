// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef VHDL_TARGET_H
#define VHDL_TARGET_H

// The lane as VHDL-2008, a language of the register lane (code_generator.h). Its ruleset is vhdl.krs, and GHDL analyzes
// and runs it. Its opening lines are the own lane_open of the ruleset, and the header that it gets is empty.
//
// The lane is a clocked process, and its text always splits into states. The call program() splits it only where the
// lane itself needs a split, at each error and each return. The call scheduled() also splits it by the construction set
// of a target

#include "../engine/rmc/code_generator.h"

class VhdlTarget : public CodeGenerator
{
  public:
    VhdlTarget(void);

    const ScheduleModel *program_schedule_model(void) const override;
};

// the VHDL code generator a process retains, its ruleset read at its first call to ruleset()
VhdlTarget &vhdl_target(void);

#endif
