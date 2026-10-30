// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lossy_test.c: bytes read as UTF-8 a byte that starts no whole letter at a time read as U+FFFD: letters of one to
// four bytes kept, each byte of a broken or overlong letter one U+FFFD, a line that fills holding every whole letter
// that fits and no part of the next, and a line that grows holding them all
#include "../../../../../ui/cli/src/command_line/file_writing.h"
#include "cli_check.h"

// `size` bytes of `text` read into a line of `capacity` bytes: whether they fit, and its text into `kept`, sized
// for 64 bytes
static int lossy_read(const char *text, unsigned long long size, unsigned long long capacity, char *kept)
{
    VerbaLinea line = {kept, capacity, 0ull};
    cli_lossy(&line, (const unsigned char *)text, size);
    const int held = mmgr_line_ok(&line);
    (void)mmgr_line_finish(&line);
    return held;
}

int main(void)
{
    (void)cli_cells_made(CLI_CELLS_POOL);
    cli_check_begun();
    char kept[64];
    const char ascii[] = "plain text, every byte a letter";
    cli_check(lossy_read(ascii, sizeof(ascii) - 1u, sizeof(kept), kept), "a text that fits is held");
    cli_check_text(kept, sizeof(ascii) - 1u, ascii, "ASCII is read as it is");
    const char letters[] = "a\xc3\xa9 \xe2\x82\xac \xf0\x9f\x98\x80 z";
    (void)lossy_read(letters, sizeof(letters) - 1u, sizeof(kept), kept);
    cli_check_text(kept, sizeof(letters) - 1u, letters, "letters of two, three and four bytes are read as they are");
    const char broken[] = "a\xff"
                          "b\xc3";
    const char broken_read[] = "a\xef\xbf\xbd"
                               "b\xef\xbf\xbd";
    (void)lossy_read(broken, sizeof(broken) - 1u, sizeof(kept), kept);
    cli_check_text(kept, sizeof(broken_read) - 1u, broken_read,
                   "a byte that starts no letter and a letter cut short each read as U+FFFD");
    const char overlong[] = "\xed\xa0\x80"
                            "c";
    const char overlong_read[] = "\xef\xbf\xbd\xef\xbf\xbd\xef\xbf\xbd"
                                 "c";
    (void)lossy_read(overlong, sizeof(overlong) - 1u, sizeof(kept), kept);
    cli_check_text(kept, sizeof(overlong_read) - 1u, overlong_read, "each byte of a surrogate read as U+FFFD");
    // "ab", the euro sign's three bytes, "cd": a line of five bytes has room for four and the euro sign does not fit
    const char filled[] = "ab\xe2\x82\xac"
                          "cd";
    cli_check(!lossy_read(filled, sizeof(filled) - 1u, 5ull, kept), "a line too short for the text is full");
    cli_check_text(kept, 2u, "ab", "a full line holds the letters before the first that did not fit");
    cli_check(!lossy_read(filled, sizeof(filled) - 1u, 6ull, kept), "and one with room for the euro sign is full too");
    cli_check_text(kept, 5u, "ab\xe2\x82\xac", "it holds the euro sign whole and nothing after it");
    VerbaLinea grown;
    cli_check(cli_growing_line_made(&grown, 4u, &g_checks_cells), "a line that grows is made");
    cli_lossy(&grown, (const unsigned char *)letters, sizeof(letters) - 1u);
    cli_check(mmgr_line_ok(&grown), "a line that grows holds every letter");
    cli_check_text(grown.out, grown.at, letters, "and holds them as they were read");
    cli_cell_release(grown.out, &g_checks_cells);
    return cli_checked();
}
