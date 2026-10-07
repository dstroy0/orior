// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational

// The kept scheme, set before the page draws anything, so that its first frame is already in it.
document.documentElement.dataset.scheme = localStorage.getItem("orior.scheme") === "light" ? "light" : "dark";
