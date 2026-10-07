// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! orior without its window: the tree's jobs, how they run, its files, its definitions and its
//! bridge, and the command line over all of them. The window is a layer on this crate and adds no job,
//! no command and no reading of the tree of its own.
//!
//! Errors are filed as issues on orior's repository, as report.rs says.
//!
//! The commands are one list, commands.json, read by the command line here and by the window for its
//! menus. A menu's title and one of its commands are the words the command line takes for it.

pub mod bridge;
pub mod catalog;
pub mod cli;
pub mod commands;
pub mod defs;
pub mod files;
pub mod format;
pub mod git;
pub mod home;
pub mod plugins;
pub mod report;
pub mod root;
pub mod runner;
pub mod toolchains;
