// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksg_tool.h: what the ksg program's sources share: a path with a suffix after it, a seal's stored trailer, what is
// reported, and the approvals made, added and verified (ksg.c, ksg_approvals.c)
#ifndef KSG_TOOL_H
#define KSG_TOOL_H

#include "ksa.h"

// the max approvers one check requires
#define KSG_APPROVERS_MAX 16u

// `path` with `suffix` after it into `out`, which has room for KSG_PATH_MAX_LEN: 1, or 0 where it does not fit
int ksg_path_with(const char *path, const char *suffix, char *out);
// the trailer of the seal stored beside `path` into `trailer`, which has room for KSG_TRAILER_BYTES: 1, or 0 where
// there is no seal of ksg's type beside it
int ksg_trailer_read(const char *path, unsigned char *trailer);

// The report of the program, gathered in ksg.c and written to the standard output. The calls:
// - `text` written after what the report holds, with the report written out first where `text` does not fit;
// - `value` written in base ten;
// - `count` bytes written as hex, two digits a byte;
// - the report written out and emptied.
void ksg_print(const char *text);
void ksg_print_number(unsigned long long value);
void ksg_print_hex(const unsigned char *bytes, unsigned int count);
void ksg_print_flush(void);

// a key pair of the scheme `name`, made from the randomness of the system. The secret goes to `secret_path`, shut to
// every reader but its owner, and the public key to `public_path`. The return is 1, or 0 with the reason reported
int ksg_keys_made(const char *name, const char *secret_path, const char *public_path);
// the public key at `public_path` read into `key`, its scheme's tag and its key: 1, or 0 with why reported
int ksg_public_read(const char *public_path, KsaApproval *key);
// the seal beside `path`, whose trailer is `trailer`, approved with the secret at `secret_path`. The approval goes in
// the list beside it, in place of an approval of the same key there before. The return is 1, or 0 with the reason
// reported
int ksg_approval_added(const char *secret_path, const char *path, const unsigned char *trailer);
// each approval in the list beside `path`, verified against the seal whose trailer is `trailer` and reported. Then
// each of the `count` keys of `required`, found among the valid ones. The return is 1 where every approval of a known
// scheme verified and every required key did, else 0
int ksg_approvals_checked(const char *path, const unsigned char *trailer, const KsaApproval *required,
                          unsigned int count);

#endif
