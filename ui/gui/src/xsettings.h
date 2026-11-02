// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// xsettings.h: the X settings list as its manager lays it out, read for the setting a desktop asks for less motion by
// (xsettings.c)
#ifndef GUI_XSETTINGS_H
#define GUI_XSETTINGS_H

// `size` bytes of an _XSETTINGS_SETTINGS list read for Gtk/EnableAnimations, in the byte order the list names: 1 with
// whether it is on into `enabled`, or 0, `enabled` left, where the list does not hold it or ends short
int gui_xsettings_animations(const unsigned char *data, unsigned long long size, int *enabled);
#endif
