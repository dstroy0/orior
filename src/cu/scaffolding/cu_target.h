// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#ifndef CU_TARGET_H
#define CU_TARGET_H

// The lane as CUDA C++, a language of the register lane (code_generator.h): its ruleset is cu.krs, nvcc compiles it,
// and its header is the prelude, whose launch its opening and its resident read. Each form is written in the language
// itself: a .cu file read against cu.krs is read form by form, and each form written at once in another ruleset of the
// same schema

#include "../transpiler/codegen/code_generator.h"

class CuTarget : public CodeGenerator
{
  public:
    CuTarget(void);
};

// the CUDA source's code generator a process holds, its ruleset read at its first call to ruleset()
CuTarget &cu_target(void);

#endif
