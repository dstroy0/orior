// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ipp.h: the Internet Printing Protocol, as RFC 8010 and 8011 set it out, spoken by orior itself over HTTP/1.1 (ipp.c
// encodes, decodes and exchanges a message, ipp_jobs.c asks what printing needs): a printer's attributes asked for, a
// job printed with its document, its state asked for and a job canceled, and the printers a CUPS server shares listed.
// A printer is named by its ipp:// address; one that takes only ipps://, IPP over TLS, is said to be out of reach
#ifndef PRINT_IPP_H
#define PRINT_IPP_H

#include "print.h"

// the groups' tags, the end of the groups, and the tags of the texts orior writes
#define CLI_IPP_OPERATION 0x01u
#define CLI_IPP_JOB 0x02u
#define CLI_IPP_END 0x03u
#define CLI_IPP_PRINTER 0x04u

#define CLI_IPP_NAME 0x42u
#define CLI_IPP_KEYWORD 0x44u
#define CLI_IPP_URI 0x45u
#define CLI_IPP_CHARSET 0x47u
#define CLI_IPP_LANGUAGE 0x48u
#define CLI_IPP_MIME 0x49u

// the most groups, attributes and values a message holds, and the bytes of their names and texts
#define CLI_IPP_GROUPS 256u

#define CLI_IPP_ATTRIBUTES 8192u

#define CLI_IPP_VALUES 32768u

#define CLI_IPP_BYTES (1u << 21u)

// a value's kinds: a number, a truth, an enum, a range of two numbers, a text by its tag, and one orior reads past
enum
{
    CLI_IPP_INTEGER,
    CLI_IPP_BOOLEAN,
    CLI_IPP_ENUM,
    CLI_IPP_RANGE,
    CLI_IPP_TEXT,
    CLI_IPP_OTHER
};

// A value of an attribute: its kind and tag, its number or the two of a range, and its bytes in the message's
typedef struct
{
    unsigned int kind;
    unsigned int tag;
    int low;
    int high;
    unsigned int bytes;
    unsigned int length;
} CliIppValue;

// An attribute: its name in the message's bytes, and its values, `count` from `first`
typedef struct
{
    unsigned int name;
    unsigned int name_length;
    unsigned int first;
    unsigned int count;
} CliIppAttribute;

// A group: its tag and its attributes, `count` from `first`
typedef struct
{
    unsigned int tag;
    unsigned int first;
    unsigned int count;
} CliIppGroup;

// A request or a response: the operation or status, its groups, attributes and values, the bytes they keep, and the
// document after them; 1 in `full` where one did not fit
typedef struct
{
    unsigned int code;
    unsigned int request_id;
    CliIppGroup group[CLI_IPP_GROUPS];
    unsigned int groups;
    CliIppAttribute attribute[CLI_IPP_ATTRIBUTES];
    unsigned int attributes;
    CliIppValue value[CLI_IPP_VALUES];
    unsigned int values;
    unsigned char bytes[CLI_IPP_BYTES];
    unsigned int used;
    const unsigned char *data;
    unsigned long long data_length;
    int full;
} CliIppMessage;

// a message begun with no groups; a group begun; an attribute of `name` begun in the last group; and a value put after
// the last attribute's: a number or an enum, a truth, a range, a text by its tag
void cli_ipp_begun(CliIppMessage *message, unsigned int code, unsigned int request_id);
void cli_ipp_group(CliIppMessage *message, unsigned int tag);
void cli_ipp_attribute(CliIppMessage *message, const char *name);
void cli_ipp_number(CliIppMessage *message, unsigned int kind, int value);
void cli_ipp_truth(CliIppMessage *message, int value);
void cli_ipp_range(CliIppMessage *message, int low, int high);
void cli_ipp_text(CliIppMessage *message, unsigned int tag, const char *text);
// `message` in IPP's encoding, version 2.0, its document left out; and the message `size` bytes hold read into
// `message`: 1, or 0 with why said
void cli_ipp_encode(const CliIppMessage *message, CliLine *out);
int cli_ipp_decode(const unsigned char *bytes, unsigned long long size, CliIppMessage *message, CliLine *said);
// the first group of `tag`, NULL where none; the attribute `name` of a group, NULL where none; whether a value is a
// text, written into `out`, which holds `room`; and the first text and the first number of an attribute: 1, or 0 where
// it has none
const CliIppGroup *cli_ipp_group_of(const CliIppMessage *message, unsigned int tag, unsigned int after);
const CliIppAttribute *cli_ipp_get(const CliIppMessage *message, const CliIppGroup *group, const char *name);
int cli_ipp_value_text(const CliIppMessage *message, const CliIppValue *value, char *out, unsigned long long room);
int cli_ipp_first_text(const CliIppMessage *message, const CliIppGroup *group, const char *name, char *out,
                       unsigned long long room);
int cli_ipp_first_number(const CliIppMessage *message, const CliIppGroup *group, const char *name, int *number);
// where an ipp:// address reaches: its host, its port and its path, each host and path holding CLI_PATH_LONGEST: 1, or
// 0 with why said
int cli_ipp_address(const char *uri, char *host, unsigned int *port, char *path, CliLine *said);
// `request` sent to the printer at `uri`, `document` of `size` bytes after it, and its answer read into `answer`,
// each read waited on 30 seconds at most: 1, or 0 with why said
int cli_ipp_exchange(const char *uri, const CliIppMessage *request, const unsigned char *document,
                     unsigned long long size, CliIppMessage *answer, CliLine *said);

// What a printer says of itself: its name, make, the formats it takes, whether it prints color, its state and why, and
// whether it takes jobs
#define CLI_IPP_LISTED 64u

typedef struct
{
    char name[CLI_PATH_LONGEST];
    char make[CLI_PATH_LONGEST];
    char formats[CLI_IPP_LISTED][CLI_NAME_LONGEST];
    unsigned int format_count;
    int color;
    int state;
    char reasons[CLI_IPP_LISTED][CLI_NAME_LONGEST];
    unsigned int reason_count;
    int accepting;
} CliIppPrinter;

// How a job is printed: its name, its document's format, and the job's own settings
typedef struct
{
    const char *name;
    const char *format;
    unsigned int copies;
    int landscape;
    int gray;
    const char *media;
} CliIppJob;

// A printer a CUPS server shares: its name, its ipp:// address and what it is
typedef struct
{
    char name[CLI_PATH_LONGEST];
    char uri[CLI_PATH_LONGEST];
    char about[CLI_PATH_LONGEST];
} CliIppShared;

// what an IPP status says, in words into `said`, and whether it is success (ipp_jobs.c)
int cli_ipp_status(unsigned int code, CliLine *said);
// the attributes of the printer at `uri` that printing needs; `document`, `size` bytes, printed on it, the job's id
// into `id`; a job's state, 3 pending, 4 held, 5 printing, 6 stopped, 7 canceled, 8 aborted, 9 done, and why, as the
// printer words it, joined by `, `; and a job canceled. Each 1, or 0 with why said
int cli_ipp_printer(const char *uri, CliIppPrinter *printer, CliLine *said);
int cli_ipp_print(const char *uri, const CliIppJob *job, const unsigned char *document, unsigned long long size,
                  int *id, CliLine *said);
int cli_ipp_job_state(const char *uri, int id, int *state, CliLine *reasons, CliLine *said);
int cli_ipp_cancel(const char *uri, int id, CliLine *said);
// the printers the CUPS server at host:port shares, into `shared`, which holds `room`: their count, or -1 with why said
int cli_ipp_cups_printers(const char *host, unsigned int port, CliIppShared *shared, unsigned int room, CliLine *said);

#endif
