-- SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
-- The proofs core_radius rests on, checked by Lean's kernel: the tail of the proof on one ellipse, and the bounds
-- at N each run writes.
import Lake

open Lake DSL

package CoreRadius where
  leanOptions := #[⟨`autoImplicit, false⟩]

require mathlib from git
  "https://github.com/leanprover-community/mathlib4.git" @ "d13f23b723b8a846827a245b89c10fc7d3f11612"

@[default_target] lean_lib CoreRadius
