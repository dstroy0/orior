// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// validate_internal.h: what the validation's sources share: the tool read from its plugin, the room one validation
// reads into under its lock, and the code, the prover's messages and the findings
#ifndef VALIDATE_INTERNAL_H
#define VALIDATE_INTERNAL_H

#include "../command_line/cells.h"
#include "../text/json_tree.h"
#include "../toolchains/toolchains.h"
#include "validate.h"

// the most nodes and bytes of a tool's plugin.json, barriers and flags a tool has, bytes of a file validated and of its
// copy, words of its code, theorems and the bytes their names take, messages of a check and the bytes they take,
// findings and the bytes they take, and nodes and bytes of one line of the prover's read
#define CLI_VALIDATE_NODES 65536u
#define CLI_VALIDATE_JSON_TEXT (1u << 20u)
#define CLI_VALIDATE_BARRIERS 64u
#define CLI_VALIDATE_FLAGS 256u
#define CLI_VALIDATE_CODE (1u << 23u)
#define CLI_VALIDATE_COPY (1u << 24u)
#define CLI_VALIDATE_WORDS (1u << 18u)
#define CLI_VALIDATE_THEOREMS 4096u
#define CLI_VALIDATE_NAMES (1u << 20u)
#define CLI_VALIDATE_MESSAGES 8192u
#define CLI_VALIDATE_MESSAGE_TEXT (1u << 22u)
#define CLI_VALIDATE_FINDINGS 8192u
#define CLI_VALIDATE_FINDING_TEXT (1u << 22u)
#define CLI_VALIDATE_LINE_NODES 4096u
#define CLI_VALIDATE_LINE_TEXT (1u << 20u)

// the most bytes the file's own settings changed and the checks run are said in
#define CLI_VALIDATE_OWN (1u << 16u)
#define CLI_VALIDATE_CHECKS_TEXT (1u << 16u)

// a place of the room's texts that holds none
#define CLI_VALIDATE_NONE (~0ull)

// A limit of the machine: its name and what it is, the arrays of the kinds and the words of the messages it stops a
// check with and of the words, `not` in its plugin.json, those messages do not hold, the word that sets it, and where
// it starts, how many times it grows a step and how far it goes
typedef struct
{
    const char *name;
    const char *about;
    unsigned int kinds;
    unsigned int says;
    unsigned int unless;
    const char *lift;
    unsigned long long start;
    unsigned long long grow;
    unsigned long long ceiling;
} CliBarrier;

// A flag: the array of its words, the severity a use of one is, and what it says
typedef struct
{
    unsigned int words;
    unsigned int severity;
    const char *says;
} CliFlag;

// A tool as its plugin.json says it: its id, name, languages and toolchain; its program, words, the names that mark a
// project, the words inside one, and its patience in seconds; its barriers and flags; and whether it audits, with the
// arrays and the line of its audit, `explains` an object
typedef struct
{
    const char *id;
    const char *name;
    unsigned int languages;
    const char *toolchain;
    const char *program;
    unsigned int args;
    unsigned int project;
    unsigned int in_project;
    unsigned long long patience;
    CliBarrier barriers[CLI_VALIDATE_BARRIERS];
    unsigned int barrier_count;
    CliFlag flags[CLI_VALIDATE_FLAGS];
    unsigned int flag_count;
    int audited;
    unsigned int declarations;
    const char *print;
    unsigned int trusted;
    unsigned int gaps;
    unsigned int explains;
    unsigned int repeats;
} CliTool;

// A word of the code: where it starts and its bytes, its line and column, from 0, and its letters
typedef struct
{
    unsigned long long start;
    unsigned int size;
    unsigned int line;
    unsigned int col;
    unsigned int letters;
} CliCodeWord;

// A message of the prover's, as `lean --json` writes one a line: its severity, lines from 0 and columns, and where its
// text and its kind are in the messages' texts
typedef struct
{
    unsigned int severity;
    unsigned int line;
    unsigned int col;
    unsigned int end_line;
    unsigned int end_col;
    unsigned long long text;
    unsigned long long text_size;
    unsigned long long kind;
    unsigned long long kind_size;
} CliProverMessage;

// The messages of one check and the texts they take
typedef struct
{
    CliProverMessage message[CLI_VALIDATE_MESSAGES];
    unsigned int count;
    char text[CLI_VALIDATE_MESSAGE_TEXT];
    unsigned long long used;
} CliProverMessages;

// One thing the check found: its place, severity and kind, the barrier that stopped the check there, -1 for none, and
// where its message and what lifting it came to are in the findings' texts, CLI_VALIDATE_NONE for none
typedef struct
{
    unsigned int line;
    unsigned int col;
    unsigned int end_line;
    unsigned int end_col;
    unsigned int severity;
    const char *kind;
    int barrier;
    unsigned long long message;
    unsigned long long lifted;
} CliFinding;

// The room one validation reads into: the tool's plugin.json and the tool; the code and its words; the theorems'
// names; the copy checked; the first check's messages and the last's; one line of the prover's read; the findings;
// the toolchains, the PATH's folders, the toolchain found and the PATH a check runs with; a check's output; each
// limit grown by its barrier's name, the first barrier of that name standing for it, whether it stops the check at
// its ceiling and whether it went through once lifted; the file's own settings changed and the checks run, said; and
// whether a check ran, whether the last ran out of time, and the lines of the file
typedef struct
{
    CliJson json;
    CliTool tool;
    char code[CLI_VALIDATE_CODE];
    unsigned long long code_size;
    CliCodeWord words[CLI_VALIDATE_WORDS];
    unsigned int word_count;
    char names[CLI_VALIDATE_NAMES];
    unsigned long long name_used;
    unsigned long long theorems[CLI_VALIDATE_THEOREMS];
    unsigned int theorem_count;
    char copy[CLI_VALIDATE_COPY];
    CliProverMessages first;
    CliProverMessages last;
    CliJsonNode line_nodes[CLI_VALIDATE_LINE_NODES];
    CfgJsonToken line_tokens[CLI_VALIDATE_LINE_NODES];
    char line_text[CLI_VALIDATE_LINE_TEXT];
    CliFinding findings[CLI_VALIDATE_FINDINGS];
    unsigned int finding_count;
    char finding_text[CLI_VALIDATE_FINDING_TEXT];
    unsigned long long finding_used;
    CliJson manifest;
    CliFound found;
    char path[CLI_PATH_TEXT];
    char *output;
    unsigned long long output_size;
    unsigned long long grown[CLI_VALIDATE_BARRIERS];
    int has_grown[CLI_VALIDATE_BARRIERS];
    int stuck[CLI_VALIDATE_BARRIERS];
    unsigned long long stuck_at[CLI_VALIDATE_BARRIERS];
    int lifted[CLI_VALIDATE_BARRIERS];
    char own[CLI_VALIDATE_OWN];
    unsigned long long own_size;
    char checks[CLI_VALIDATE_CHECKS_TEXT];
    unsigned long long checks_size;
    int ran;
    int late;
    unsigned long long lines;
} CliValidateRoom;

// the room, a cell taken with the validation's lock and given back with it; and the lock taken with the room: 1, or 0
// with the lock given back where the cells cannot meet it; and the room given back with the lock (validate.c)
extern CliValidateRoom *g_cli_validate;
int cli_validate_held(void);
void cli_validate_released(void);

// whether the JSON array `array` holds the string of `length` bytes `text`; and whether `length` bytes of `text`
// hold `part` (validate_tool.c)
int cli_validate_holds(const CliJson *json, unsigned int array, const char *text, unsigned long long length);
int cli_validate_contains(const char *text, unsigned long long length, const char *part);
// the tool plugin `id` read into the room's tool: 1, or 0 where no tool plugin is of that id
int cli_validate_tool_read(const char *id);

// `size` bytes of text with their comments and strings blanked, every letter kept in its place as one, so that each
// word found in it is a word of the code, into the room's code: 1, or 0 where it is larger than the room holds; each
// word of the code with its line and column into the room's words; the full name of each theorem the code declares,
// with the namespaces open where it is declared, into the room's theorems; and each use of a flagged word, as a word
// or the last part of a dotted name, `@` before it or not, a finding (validate_code.c)
int cli_validate_code_of(const char *text, unsigned long long size);
void cli_validate_words_of(void);
void cli_validate_theorems_in(void);
void cli_validate_flags_in(void);

// the messages in a check's output, a line that is no message kept as an error with no place, as a crash writes one,
// into `messages`; the barrier a message says stopped the check, by its kind or its words, -1 for none; the axioms a
// theorem rests on, from the message that answers the line asking for them: 1 with the name's start and size and the
// list's, 0 where the message answers none, `none` 1 where it rests on no axioms (validate_messages.c)
void cli_validate_messages_of(const char *output, unsigned long long size, CliProverMessages *messages);
int cli_validate_barrier_of(const CliProverMessages *messages, const CliProverMessage *message);
int cli_validate_axioms_of(const char *text, unsigned long long size, unsigned long long *name,
                           unsigned long long *name_size, unsigned long long *list, unsigned long long *list_size);

// the copy of `size` bytes of text that is checked, into the room's copy: each `set_option <name> <value>` of the
// options `names`, `count` of them in the order of their names, set to its value of `values` where that is larger,
// each change said into `changed`, "; " between them, and the lines that ask for each theorem's axioms after it: the
// copy's size, or more than the room holds where it does not fit (validate_messages.c)
unsigned long long cli_validate_copy_of(const char *text, unsigned long long size, const char *const *names,
                                        const unsigned long long *values, unsigned int count, CliLine *changed);

// the limits grown, by their barriers' names in order, into `keys`, which holds CLI_VALIDATE_BARRIERS: their count
// (validate.c)
unsigned int cli_validate_grown_keys(unsigned int *keys);

// the words that run the program on `file` into `words`, the limits' settings just before the file, where the prover
// reads its own options, the first of them at `from` and `settings` of them, and the folder they run from into `dir`,
// which holds CLI_PATH_LONGEST; and one check run, taking at most the tool's patience: its output, the error stream
// after it where it says anything, into the room's output, the seconds it took into `seconds`, and whether it ran
// out of time into the room. Each 1, or 0 with why said (validate_run.c)
int cli_validate_command_for(const char *program, const char *file, const char *project, CliWords *words,
                             unsigned int *from, unsigned int *settings, char *dir, CliLine *said);
int cli_validate_checked(const CliWords *words, const char *dir, double *seconds, CliLine *said);

// the last check's messages: each axiom answer kept, each barrier's error passed over, and the rest the mathematics,
// each a finding: the count of axiom answers; and each axiom a theorem rests on past those trusted said, as a finding
// where the proof does not stand on a gap the prover put there, the theorem's name into `untrusted`
// (validate_axioms.c)
unsigned int cli_validate_mathematics(void);
void cli_validate_axioms_reported(unsigned int answers, CliLine *untrusted);

// the first barrier of the name barrier `at` has, which stands for that name's limit (validate.c)
unsigned int cli_validate_key_of(unsigned int at);

// a finding put with its message, `lifted` NULL for none; and what the checks run found, from the first check's
// messages and the last's, the flags and the axioms, written as the report of the file `path` and its `project`, NULL
// for none (validate_report.c)
void cli_validate_finding_put(const CliFinding *finding, const char *message, unsigned long long length,
                              const char *lifted);
void cli_validate_reported(const char *path, const char *project, CliLine *report);

#endif
