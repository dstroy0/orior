// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! The query: the run's lifecycle and every loop of the protocol, each a command of `orior
//! protocol`. The writer, the carrier, the cross and the teacher are C programs the crate builds
//! when their sources are newer and runs as processes, the run a database they share; the crate
//! holds what the scripts held, which program runs after which and with what words. Every program
//! it needs comes by role through `programs::Programs`, which the command line implements over its
//! toolchains.

pub mod programs;
