// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! orior-cli: the command line alone, for a machine with no window to open. A command that acts in
//! the window says which program opens it.

use orior_cli::cli::{self, Outcome};
use orior_cli::report;

fn main() {
    report::catch_panics();
    let code = match cli::run(std::env::args().skip(1).collect()) {
        Outcome::Exit(code) => code,
        Outcome::Window(launch) => {
            eprintln!("{} acts in the window, which orior opens: orior {}", launch.words(), launch.words());
            cli::WRONG
        }
    };
    std::process::exit(code)
}
