// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// posix_systems.h: the calls and the facts that the command line's POSIX files take from the file of each POSIX
// system, by module. The folder linux defines them for Linux, and the folder macos defines them for macOS. A POSIX
// system with no file of its own fails at the link, which names each call that it lacks
#ifndef POSIX_SYSTEMS_H
#define POSIX_SYSTEMS_H

#include "../../../ui/cli/src/command_line/cli_internal.h"

#if !defined(_WIN32)

// ---- the keychain (posix_keychain.c; linux/linux_keychain.c, macos/macos_keychain.c) ----

// the max words of a command of the keychain, its NULL among them
#define CLI_KEYCHAIN_WORDS 10u

// the program that keeps the secrets, as a report names it
extern const char g_cli_keychain_program[];

// The secret of `args` stored under the keychain's name `named` and the account of `args`, the program run into
// `run_result`: 1 where it started, or 0 with why in `err_buf`
int cli_keychain_store_started(const char *named, const CliErrMsgServiceCtx *args, CliRunResult *run_result,
                               VerbaLinea *err_buf);

// the words of the command that reads the secret of `named` and `account` to its output, and of the command that
// takes it out. Each goes into `words`, CLI_KEYCHAIN_WORDS of them at max
void cli_keychain_lookup_words(const char **words, const char *named, const char *account);
void cli_keychain_clear_words(const char **words, const char *named, const char *account);

// ---- the system's facts (posix_system.c; linux/linux_system.c, macos/macos_system.c) ----

// the names Go and Rust give the processor that the build targets, the same on each POSIX system
#if defined(__x86_64__) || defined(_M_X64)
#define CLI_POSIX_GO_PROCESSOR "amd64"
#define CLI_POSIX_RUST_PROCESSOR "x86_64"
#elif defined(__aarch64__) || defined(_M_ARM64)
#define CLI_POSIX_GO_PROCESSOR "arm64"
#define CLI_POSIX_RUST_PROCESSOR "aarch64"
#elif defined(__i386__) || defined(_M_IX86)
#define CLI_POSIX_GO_PROCESSOR "x86"
#define CLI_POSIX_RUST_PROCESSOR "x86"
#elif defined(__arm__) || defined(_M_ARM)
#define CLI_POSIX_GO_PROCESSOR "arm"
#define CLI_POSIX_RUST_PROCESSOR "arm"
#else
#define CLI_POSIX_GO_PROCESSOR "riscv64"
#define CLI_POSIX_RUST_PROCESSOR "arm"
#endif

// the ending that a name on the PATH answers to, none, and the folders that hold the reader's settings, XDG's first
extern const char *const g_cli_posix_program_ends[1];
extern const CliSystemFolder g_cli_posix_config_folders[2];

// The facts of a POSIX system, the same on each but for the system's own. They are its name, the program that opens a
// file or a link, and 1 where zsh is the reader's shell. Then come the three folders of its fonts, Go's name of it,
// .NET's name of it and the processor, and 1 where it is Linux
#define CLI_POSIX_SYSTEM_FACTS(name_, opener_, zsh_default_, font_folders_, go_system_, dotnet_runtime_, on_linux_)    \
    {                                                                                                                  \
        .program_end = "", .program_ends = g_cli_posix_program_ends, .program_end_count = 1u, .list_separator = ':',  \
        .list_quoted = 0, .path_separator = '/', .path_separators = "/", .case_folded = 0,                            \
        .host_variable = "HOSTNAME", .home_variable = "HOME", .system_name = (name_),                                  \
        .rust_processor = CLI_POSIX_RUST_PROCESSOR, .env_bin = "bin", .opener = (opener_), .opener_word = NULL,         \
        .font_folders = (font_folders_), .font_folder_count = 3u, .fonts_nested = 1, .path_in_profiles = 1,            \
        .zsh_default = (zsh_default_), .config_folders = g_cli_posix_config_folders, .config_folder_count = 2u,        \
        .file_opener = (opener_), .python_name = "python3", .signals = 1, .lldb_watch_by_script = 0,                   \
        .go_system = (go_system_), .go_processor = CLI_POSIX_GO_PROCESSOR, .dotnet_runtime = (dotnet_runtime_),        \
        .on_linux = (on_linux_)                                                                                        \
    }

// ---- the container (posix_container.c; linux/linux_container.c, macos/macos_container.c) ----

// the words that this program started with, read of the system where its main passes none. The call leaves out its
// own name and puts the words into `words`, `capacity` of them at max: their count
unsigned int cli_arguments_own_read(const char **words, unsigned int capacity);

// ---- the clipboard (posix_clipboard.c; linux/linux_clipboard.c, macos/macos_clipboard.c) ----

// The text as UTF-8 that the pages of units_take hold after their units, NULL until cli_clipboard_units_text_made
// makes it, and its bytes
extern char *g_cli_clipboard_units_text;
extern unsigned long long g_cli_clipboard_units_text_size;

// the UTF-8 text of the units made after them in the pages: 1, or 0 where it does not fit
int cli_clipboard_units_text_made(void);

// the pages that units_take took given back to the system, the system's clipboard told first
void cli_clipboard_units_pages_free(void);

// the text that the system's clipboard serves from the pages let go, before the pages go
void cli_clipboard_units_released(void);

// ---- the transfer layer (posix_transfer.c; linux/linux_transfer.c, macos/macos_transfer.c) ----

// what the system reported of the last call that failed, after `what`, into `err_msg`
void cli_transfer_failed(VerbaLinea *err_msg, const char *what);

// a descriptor of new memory of `size` bytes that no other process can find: -1 with why given where the system makes
// none
int cli_transfer_region_made(unsigned long long size, VerbaLinea *err_msg);

// a descriptor of new memory, not yet sized, that no other process can find, as the system makes one. It answers -1
// with why given where the system makes none
int cli_transfer_memory_opened(VerbaLinea *err_msg);

// The region `fd`, written through `view` of `capacity` bytes, made fast against change as far as the system can. The
// view that the region keeps, or NULL with why given and the view let go
void *cli_transfer_region_kept(int fd, void *view, unsigned long long capacity, VerbaLinea *err_msg);

// The region `fd` from another process, `length` bytes of it in a view of `capacity`, made safe to read as far as the
// system can. The view, with the descriptor that the region keeps in `section`, or NULL with why given. A NULL leaves
// `fd` for the caller to close
const void *cli_transfer_region_taken(int fd, unsigned long long length, unsigned long long capacity,
                                      VerbaLinea *err_msg, int *section);

#endif

#endif
