// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! A run's tables and blobs read from the command line through the decoder (kdecode.c): `decode` hands its words
//! to the decoder as they stand, `--table <table> <run.qry>`, `--blob <name> <run.qry>` and the rest, and says what
//! it prints.

use crate::commands::Context;

/// The decoder run with `words`, its lines said. Its exit code.
pub fn run(context: &mut Context, words: &[String]) -> Result<i32, String> {
    if words.is_empty() {
        return Err("decode takes the decoder's words: [--raw] [--from <key>] [--to <key>] [--count <rows>] --table <table> <run.qry>, or [--latest] --blob <name> <run.qry>".into());
    }
    let decoder = context.tool("kdecode")?;
    let output = std::process::Command::new(&decoder).args(words).output().map_err(|e| format!("{}: {e}", decoder.display()))?;
    for line in String::from_utf8_lossy(&output.stdout).lines().chain(String::from_utf8_lossy(&output.stderr).lines()) {
        context.say(line);
    }
    Ok(output.status.code().unwrap_or(-1))
}
