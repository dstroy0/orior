// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// One program for the window and the command line. It is built as a console program on every
// platform: a terminal waits on it, reads its output and gets its exit code. Opened with no words
// from outside a terminal, it lets go of the console Windows made for it and only the window shows.

fn main() {
    std::process::exit(orior_ui_lib::start())
}
