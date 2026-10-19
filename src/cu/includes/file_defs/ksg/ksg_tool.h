// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ksg_tool.h: what the ksg program's sources share: a path with a suffix after it, a seal's kept trailer, what is
// said, and the approvals made, added and held (ksg.c, ksg_approvals.c)
#ifndef KSG_TOOL_H
#define KSG_TOOL_H

#include "ksa.h"

// the most approvers one check asks for
#define KSG_APPROVERS_MOST 16u

// `path` with `suffix` after it into `out`, which holds KSG_PATH_LONGEST: 1, or 0 where it does not fit
int ksg_path_with(const char *path, const char *suffix, char *out);
// the trailer of the seal kept beside `path` into `trailer`, which holds KSG_TRAILER_BYTES: 1, or 0 where there is no
// seal of ksg's kind beside it
int ksg_trailer_kept(const char *path, unsigned char *trailer);

// The program's saying, gathered in ksg.c and written to the standard output: `text` said after what it holds, what it
// holds written out first where `text` does not fit after it; `value` said in base ten; `count` bytes said as hex, two
// digits a byte; and what it holds written out and the saying emptied
void ksg_said(const char *text);
void ksg_said_number(unsigned long long value);
void ksg_said_hex(const unsigned char *bytes, unsigned int count);
void ksg_said_out(void);

// a key pair of the scheme `name` made from the system's randomness, the secret written to `secret_path`, kept from
// every reader but its owner, and the public to `public_path`: 1, or 0 with why said
int ksg_keys_made(const char *name, const char *secret_path, const char *public_path);
// the public key at `public_path` read into `key`, its scheme's tag and its key: 1, or 0 with why said
int ksg_public_read(const char *public_path, KsaApproval *key);
// the seal beside `path`, whose trailer is `trailer`, approved with the secret at `secret_path` and the approval put in
// the list beside it, an approval of the same key there before replaced: 1, or 0 with why said
int ksg_approval_added(const char *secret_path, const char *path, const unsigned char *trailer);
// each approval in the list beside `path` held to the seal whose trailer is `trailer` and said, and each of the
// `count` keys of `required` found among those that hold: 1 where every approval of a known scheme held and every key
// required did, else 0
int ksg_approvals_checked(const char *path, const unsigned char *trailer, const KsaApproval *required,
                          unsigned int count);

#endif
