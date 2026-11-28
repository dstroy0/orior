// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// pages.c: the window's pages through the platform's reserve, commit and release (platform_internal.h)
#include "pages.h"

#include "../../../includes/platform/platform_internal.h"

const GuiBlockPages g_gui_pages = {platform_pages_reserved, platform_pages_committed, platform_pages_released};
