// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The bridge's pairs put to the part, and the other modes of `klq_identity`.

use std::path::Path;

use crate::commands::Context;

/// The pairs of the bridge put to `part` in the language `to`, dry where `dry`.
pub fn pair(context: &mut Context, part: &str, to: &str, dry: bool, seed: Option<&str>, trace: Option<&Path>) -> Result<i32, String> {
    let _ = (context, part, to, dry, seed, trace);
    Err("the pairs are not yet run from the crate".into())
}
