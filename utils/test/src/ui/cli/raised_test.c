// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// raised_test.c: the raised write's check: a refusal is told from any other failure, and the raised helper copies the
// staged text over the file
#include "../../../../../ui/cli/src/machine/raised.h"
#include "cli_check.h"

int main(void)
{
    cli_check(cli_raised_is_refused("a.txt: Access is denied. (os error 5)"), "Windows' access denied is a refusal");
    cli_check(cli_raised_is_refused("a.txt: Permission denied (os error 13)"), "permission denied is a refusal");
    cli_check(cli_raised_is_refused("a.txt: Read-only file system (os error 30)"), "a read-only file system is one");
    cli_check(!cli_raised_is_refused("a.txt: The system cannot find the path specified. (os error 3)"),
              "a path not found is not");
    cli_check(!cli_raised_is_refused("a.txt: No space left on device (os error 28)"), "a full device is not");
    char folder[CLI_PATH_LONGEST];
    char staged[CLI_PATH_LONGEST];
    char target[CLI_PATH_LONGEST];
    cli_check_folder("raised", folder);
    cli_full(folder, "staged.txt", staged);
    cli_full(folder, "target.txt", target);
    (void)query_file_written(staged, "new text", 8ull);
    (void)query_file_written(target, "old", 3ull);
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(cli_raised_copy_over(staged, target, &said), "the staged text is copied over");
    char text[64];
    const unsigned long long size = qry_member_file_read(target, (unsigned char *)text, sizeof(text));
    cli_check_text(text, (size == QRY_MEMBER_UNREAD) ? 0ull : size, "new text", "the file holds the staged text");
    return cli_checked();
}
