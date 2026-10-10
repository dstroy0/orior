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

pub mod binary;
pub mod bridge;
pub mod builds;
pub mod catalog;
pub mod checkers;
pub mod cli;
pub mod commands;
pub mod containers;
pub mod dap;
pub mod debug;
pub mod deploy;
pub mod defs;
pub mod devcontainer;
pub mod digest;
pub mod editorconfig;
pub mod executables;
pub mod envs;
pub mod files;
pub mod fill;
pub mod format;
pub mod git;
pub mod highlight;
pub mod history;
pub mod home;
pub mod inspect;
pub mod kept;
pub mod link;
pub mod lsp;
pub mod patterns;
pub mod regexp;
pub mod plugins;
pub mod profile;
pub mod python_paths;
pub mod recording;
pub mod report;
pub mod root;
pub mod schema;
pub mod sea;
pub mod serve;
pub mod run_file;
pub mod runner;
pub mod servers;
pub mod shape;
pub mod structure;
pub mod symbols;
pub mod templates;
pub mod test_runs;
pub mod testing;
pub mod toolchains;
pub mod validate;
pub mod watch;
