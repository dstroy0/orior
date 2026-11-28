// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "sass_target.h"

// the lane as SASS: sass.krs, how to write SASS, then sm_86.krs and sm_86.kdm, the operations of the part and its
// registers. A cubin writer assembles them, and the header is the own opening of the listing
SassTarget::SassTarget(void) : CodeGenerator("sass.krs")
{
}

// R0 through R235. The code of a kernel names R0 through R252 (sm_86.ksc, the answer of the part). The file sm_86.kdm
// pins the top of those for the own use of the lane:
//
// - R240 up are its fixed registers.
// - R238 and R239 are the launch of the call to the lane. launch_open moves it there, and launch_load_wide reads every
//   parameter through it.
// - R237 is the scratch of the file, and R236 holds sign_base.
//
// The program refuses a lane whose banks reach R236, and does not write over what those hold. This is the count of
// the file, and not what a lane can take and stay fast. An SM has 65536 registers and runs 1536 threads. 42 for each
// thread is full occupancy, and every one past that costs residency
unsigned int SassTarget::register_file_size(void) const
{
    return 236u;
}

// The resident reaches SASS already built. The file ptx.krs stores it as PTX that runs, and the own compiler of the
// part turns that into SASS that runs. The program builds a program with a write of the lane into the cycle_lane of
// that cubin. It leaves cycle_program as the compiler wrote it. A check on the part showed that every instruction of
// the resident comes through, except the own instructions of the empty lane.
//
// For that reason, sass.krs gives program_unit as an error. This file leaves the form out, and does not call for it
int SassTarget::program_unit_written(void) const
{
    return 0;
}

SassTarget &sass_target(void)
{
    static SassTarget generator;
    return generator;
}
