// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

//! orior's own printing, with no dialog of the system's: a listing laid out in pages, written as a
//! PDF or drawn as pixels by orior itself, and sent from orior's own queue to a printer on the
//! network by IPP or its raw port, to a printer of the system's through its driver or CUPS, or to a
//! PDF file.

pub mod font;
pub mod ipp;
pub mod page;
pub mod pdf;
pub mod raster;
