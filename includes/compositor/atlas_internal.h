// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// atlas_internal.h: what the compositor's block reaches of the atlas: the bytes the atlas takes for the limits, and
// the atlas placed in the block from `place` on, the contrast tables made, the place after it the result. The
// compositor's own source alone includes this (atlas.c)
#ifndef GUI_COMPOSITOR_ATLAS_INTERNAL_H
#define GUI_COMPOSITOR_ATLAS_INTERNAL_H
#include "compositor.h"

// the bytes the atlas takes for `limits`, 0 where its limits are out of their range
embed_u64 gui_compositor_atlas_bytes(const GuiCompositorLimitsCfg *limits);
// the atlas of `compositor` placed from `place` on: the place past its last byte
unsigned char *gui_compositor_atlas_place(GuiCompositor *compositor, unsigned char *place);

#endif
