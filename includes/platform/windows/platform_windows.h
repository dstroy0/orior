// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// platform_windows.h: what the files of this folder share on Windows. A path and a text of the program are UTF-8, and
// the wide calls of Windows take UTF-16. These two calls carry a text between the two, and every file of this folder
// carries its texts through them. A file of this folder includes this header after windows.h
#ifndef PLATFORM_WINDOWS_H
#define PLATFORM_WINDOWS_H

// the UTF-16 units of a path of the platform, its terminator among them
#define PLATFORM_WIDE_PATH 4096u

// The UTF-8 text `text`, terminated, into `wide` as UTF-16 of at max `capacity` units with its terminator. 1, or 0
// where the text does not fit or is no UTF-8. A path with a byte that is no UTF-8 names no file. The call then
// refuses it and puts no replacement character in its place
int platform_wide(const char *text, wchar_t *wide, unsigned int capacity);

// The UTF-16 text `wide`, terminated, into `text` as UTF-8 of at max `capacity` bytes with its terminator. 1, or 0
// where the text does not fit
int platform_narrow(const wchar_t *wide, char *text, unsigned int capacity);

#endif
