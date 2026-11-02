// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// ipp.h: the Internet Printing Protocol, as RFC 8010 and 8011 set it out, implemented by orior_cli itself over HTTP/1.1
// (ipp.c encodes, decodes and exchanges a message, ipp_jobs.c requests what printing needs): a printer's attributes
// requested for, a job printed with its document, its state requested for and a job canceled, and the printers a CUPS
// server shares listed. A printer is named by its ipp:// address; one that takes only ipps://, IPP over TLS, is
// reported to be out of reach
#ifndef PRINT_IPP_H
#define PRINT_IPP_H
typedef struct CliIppSizeCfg CliIppSizeCfg;
typedef struct CliIppNameCfg CliIppNameCfg;
typedef struct CliIppSizeErrMsgCfg CliIppSizeErrMsgCfg;
typedef struct CliIppErrMsgCfg CliIppErrMsgCfg;
typedef struct CliIppErrMsgHostCfg CliIppErrMsgHostCfg;
typedef struct CliIppErrMsgUriCfg CliIppErrMsgUriCfg;
typedef struct CliIppMessageCfg CliIppMessageCfg;
typedef struct CliIppMessageValueCfg CliIppMessageValueCfg;
typedef struct CliIppMessageCodeCfg CliIppMessageCodeCfg;
typedef struct CliIppTagCfg CliIppTagCfg;
typedef struct CliIppMessageKindCfg CliIppMessageKindCfg;
typedef struct CliIppErrMsgUriPrinterCfg CliIppErrMsgUriPrinterCfg;
typedef struct CliIppMessageLowCfg CliIppMessageLowCfg;
typedef struct CliIppMessageTagCfg CliIppMessageTagCfg;
typedef struct CliIppCfg CliIppCfg;
typedef struct CliIppMessageOutVerbaLineaCfg CliIppMessageOutVerbaLineaCfg;
typedef struct CliIppCodeCfg CliIppCodeCfg;

#include "embed_dispatch_layout.h"
#include "print.h"

// the groups' tags, the end of the groups, and the tags of the texts orior_cli writes
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

// the most groups, attributes and values a message contains, and the bytes of their names and texts
#define CLI_IPP_GROUPS 256u

#define CLI_IPP_ATTRIBUTES 8192u

#define CLI_IPP_VALUES 32768u

#define CLI_IPP_BYTES (1u << 21u)

// a value's kinds: a number, a truth, an enum, a range of two numbers, a text by its tag, and one orior_cli reads past
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

// A request or a response: the operation or status, its groups, attributes and values, the bytes they occupy, and the
// document after them; 1 in `full` where one did not fit
typedef struct
{
    unsigned int code;
    unsigned int request_id;
    CliIppGroup *group;
    unsigned int groups;
    unsigned int group_room;
    CliIppAttribute *attribute;
    unsigned int attributes;
    unsigned int attribute_room;
    CliIppValue *value;
    unsigned int values;
    unsigned int value_room;
    unsigned char *bytes;
    unsigned int used;
    unsigned int byte_room;
    const unsigned char *data;
    unsigned long long data_length;
    int full;
} CliIppMessage;

// the cells the messages' groups, attributes, values and bytes are stored in, counted; and a message's cells given
// back, the message left empty, a message given back once it is done with (ipp.c)
extern CellOwner g_cli_ipp_cells;
void cli_ipp_free(const CliIppMessageCodeCfg *args);
// a message begun with no groups, the cells it occupied retained for its puts; a group begun; an attribute of `name`
// written in the last group; and a value put after the last attribute's: a number or an enum, a truth, a range, a text
// by its tag
void cli_ipp_begin(const CliIppMessageCodeCfg *args);
void cli_ipp_group(const CliIppMessageTagCfg *args);
void cli_ipp_attribute(const CliIppCfg *args);
void cli_ipp_number(const CliIppMessageKindCfg *args);
void cli_ipp_truth(const CliIppMessageKindCfg *args);
void cli_ipp_range(const CliIppMessageLowCfg *args);
void cli_ipp_text(const CliIppMessageTagCfg *args);
// `message` in IPP's encoding, version 2.0, its document left out; and the message `size` bytes contain read into
// `message`: 1, or 0 with why given
void cli_ipp_encode(const CliIppMessageOutVerbaLineaCfg *args);
int cli_ipp_decode(const CliIppMessageCfg *args);
// the first group of `tag`, NULL where none; the attribute `name` of a group, NULL where none; whether a value is a
// text, written into `out`, sized for `room`; and the first text and the first number of an attribute: 1, or 0 where
// it has none
const CliIppGroup *cli_ipp_group_get(const CliIppTagCfg *args);
const CliIppAttribute *cli_ipp_get(const CliIppNameCfg *args);
int cli_ipp_value_text(const CliIppMessageValueCfg *args);
int cli_ipp_first_text(const CliIppNameCfg *args);
int cli_ipp_first_number(const CliIppNameCfg *args);
// where an ipp:// address reaches: its host, its port and its path, each host and path sized for CLI_PATH_MAX_LEN: 1,
// or 0 with why given
int cli_ipp_address(const CliIppErrMsgCfg *args);
// `request` sent to the printer at `uri`, `document` of `size` bytes after it, and its answer read into `answer`,
// each read waited on 30 seconds at most: 1, or 0 with why given
int cli_ipp_exchange(const CliIppSizeCfg *args);

// What a printer reports of itself: its name, make, the formats it takes, whether it prints color, its state and why,
// and whether it takes jobs
#define CLI_IPP_LISTED 64u

typedef struct
{
    char name[CLI_PATH_MAX_LEN];
    char make[CLI_PATH_MAX_LEN];
    char formats[CLI_IPP_LISTED][CLI_NAME_MAX_LEN];
    unsigned int format_count;
    int color;
    int state;
    char reasons[CLI_IPP_LISTED][CLI_NAME_MAX_LEN];
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
    char name[CLI_PATH_MAX_LEN];
    char uri[CLI_PATH_MAX_LEN];
    char about[CLI_PATH_MAX_LEN];
} CliIppShared;

// what an IPP status means, in words into `err_msg`, and whether it is success (ipp_jobs.c)
int cli_ipp_status(const CliIppCodeCfg *args);
// the attributes of the printer at `uri` that printing needs; `document`, `size` bytes, printed on it, the job's id
// into `id`; a job's state, its RFC 8011 `job-state` number, 3 to 9, and why, as the
// printer words it, joined by `, `; and a job canceled. Each 1, or 0 with why given
int cli_ipp_printer(const CliIppErrMsgUriPrinterCfg *args);
int cli_ipp_print(const CliIppSizeErrMsgCfg *args);
int cli_ipp_job_state(const CliIppErrMsgUriCfg *args);
int cli_ipp_cancel(const CliIppErrMsgUriCfg *args);
// the printers the CUPS server at host:port shares, into `shared`, sized for `room`: their count, or -1 with why given
int cli_ipp_cups_printers(const CliIppErrMsgHostCfg *args);

struct CliIppSizeCfg
{
    unsigned long long size;
    VerbaLinea *err_msg;
    const char *uri;
    const CliIppMessage *request;
    const unsigned char *document;
    CliIppMessage *answer;
};

struct CliIppNameCfg
{
    const char *name;
    const CliIppMessage *message_read;
    const CliIppGroup *group;
    char *out;
    unsigned long long room;
    int *number;
};

struct CliIppSizeErrMsgCfg
{
    unsigned long long size;
    VerbaLinea *err_msg;
    const char *uri;
    const unsigned char *document;
    const CliIppJob *job;
    int *id_pointer;
};

struct CliIppErrMsgCfg
{
    VerbaLinea *err_msg;
    const char *uri;
    char *host;
    unsigned int *port;
    char *path;
};

struct CliIppErrMsgHostCfg
{
    VerbaLinea *err_msg;
    const char *host_read;
    unsigned int port_value;
    CliIppShared *shared;
    unsigned int room_u32;
};

struct CliIppErrMsgUriCfg
{
    VerbaLinea *err_msg;
    const char *uri;
    int id;
    int *state;
    VerbaLinea *reasons;
};

struct CliIppMessageCfg
{
    CliIppMessage *message;
    const unsigned char *bytes;
    unsigned long long size;
    VerbaLinea *err_msg;
};

struct CliIppMessageValueCfg
{
    const CliIppMessage *message_read;
    const CliIppValue *value_ipp_value_pointer;
    char *out;
    unsigned long long room;
};

struct CliIppMessageCodeCfg
{
    CliIppMessage *message;
    unsigned int code;
    unsigned int request_id;
};

struct CliIppTagCfg
{
    unsigned int tag;
    const CliIppMessage *message_read;
    unsigned int after;
};

struct CliIppMessageKindCfg
{
    CliIppMessage *message;
    unsigned int kind;
    int value;
};

struct CliIppErrMsgUriPrinterCfg
{
    VerbaLinea *err_msg;
    const char *uri;
    CliIppPrinter *printer;
};

struct CliIppMessageLowCfg
{
    CliIppMessage *message;
    int low;
    int high;
};

struct CliIppMessageTagCfg
{
    CliIppMessage *message;
    unsigned int tag;
    const char *text;
};

struct CliIppCfg
{
    CliIppMessage *message;
    const char *name;
};

struct CliIppMessageOutVerbaLineaCfg
{
    const CliIppMessage *message_read;
    VerbaLinea *out_verba_linea_pointer;
};

struct CliIppCodeCfg
{
    unsigned int code;
    VerbaLinea *err_msg;
};

typedef struct
{
    void (*free)(const CliIppMessageCodeCfg *args);
    void (*begin)(const CliIppMessageCodeCfg *args);
} CliIppMessageCodeNs;
EMBED_TABLE_LAYOUT(CliIppMessageCodeNs, free, begin);
EMBED_TABLE_STORAGE CliIppMessageCodeNs cli_ipp_message_code EMBED_UNUSED = {
    .free = cli_ipp_free,
    .begin = cli_ipp_begin,
};

typedef struct
{
    void (*group)(const CliIppMessageTagCfg *args);
    void (*text)(const CliIppMessageTagCfg *args);
} CliIppMessageTagNs;
EMBED_TABLE_LAYOUT(CliIppMessageTagNs, group, text);
EMBED_TABLE_STORAGE CliIppMessageTagNs cli_ipp_message_tag EMBED_UNUSED = {
    .group = cli_ipp_group,
    .text = cli_ipp_text,
};

typedef struct
{
    void (*attribute)(const CliIppCfg *args);
} CliIppNs;
EMBED_TABLE_LAYOUT(CliIppNs, attribute);
EMBED_TABLE_STORAGE CliIppNs cli_ipp_table EMBED_UNUSED = {
    .attribute = cli_ipp_attribute,
};

typedef struct
{
    void (*number)(const CliIppMessageKindCfg *args);
    void (*truth)(const CliIppMessageKindCfg *args);
} CliIppMessageKindNs;
EMBED_TABLE_LAYOUT(CliIppMessageKindNs, number, truth);
EMBED_TABLE_STORAGE CliIppMessageKindNs cli_ipp_message_kind EMBED_UNUSED = {
    .number = cli_ipp_number,
    .truth = cli_ipp_truth,
};

typedef struct
{
    void (*range)(const CliIppMessageLowCfg *args);
} CliIppMessageLowNs;
EMBED_TABLE_LAYOUT(CliIppMessageLowNs, range);
EMBED_TABLE_STORAGE CliIppMessageLowNs cli_ipp_message_low EMBED_UNUSED = {
    .range = cli_ipp_range,
};

typedef struct
{
    void (*encode)(const CliIppMessageOutVerbaLineaCfg *args);
} CliIppMessageOutVerbaLineaNs;
EMBED_TABLE_LAYOUT(CliIppMessageOutVerbaLineaNs, encode);
EMBED_TABLE_STORAGE CliIppMessageOutVerbaLineaNs cli_ipp_message_out_verba_linea EMBED_UNUSED = {
    .encode = cli_ipp_encode,
};

typedef struct
{
    int (*decode)(const CliIppMessageCfg *args);
} CliIppMessageNs;
EMBED_TABLE_LAYOUT(CliIppMessageNs, decode);
EMBED_TABLE_STORAGE CliIppMessageNs cli_ipp_message EMBED_UNUSED = {
    .decode = cli_ipp_decode,
};

typedef struct
{
    const CliIppGroup *(*group_get)(const CliIppTagCfg *args);
} CliIppTagNs;
EMBED_TABLE_LAYOUT(CliIppTagNs, group_get);
EMBED_TABLE_STORAGE CliIppTagNs cli_ipp_tag EMBED_UNUSED = {
    .group_get = cli_ipp_group_get,
};

typedef struct
{
    const CliIppAttribute *(*get)(const CliIppNameCfg *args);
    int (*first_text)(const CliIppNameCfg *args);
    int (*first_number)(const CliIppNameCfg *args);
} CliIppNameNs;
EMBED_TABLE_LAYOUT(CliIppNameNs, get, first_text, first_number);
EMBED_TABLE_STORAGE CliIppNameNs cli_ipp_name EMBED_UNUSED = {
    .get = cli_ipp_get,
    .first_text = cli_ipp_first_text,
    .first_number = cli_ipp_first_number,
};

typedef struct
{
    int (*value_text)(const CliIppMessageValueCfg *args);
} CliIppMessageValueNs;
EMBED_TABLE_LAYOUT(CliIppMessageValueNs, value_text);
EMBED_TABLE_STORAGE CliIppMessageValueNs cli_ipp_message_value EMBED_UNUSED = {
    .value_text = cli_ipp_value_text,
};

typedef struct
{
    int (*address)(const CliIppErrMsgCfg *args);
} CliIppErrMsgNs;
EMBED_TABLE_LAYOUT(CliIppErrMsgNs, address);
EMBED_TABLE_STORAGE CliIppErrMsgNs cli_ipp_err_msg EMBED_UNUSED = {
    .address = cli_ipp_address,
};

typedef struct
{
    int (*exchange)(const CliIppSizeCfg *args);
} CliIppSizeNs;
EMBED_TABLE_LAYOUT(CliIppSizeNs, exchange);
EMBED_TABLE_STORAGE CliIppSizeNs cli_ipp_size EMBED_UNUSED = {
    .exchange = cli_ipp_exchange,
};

typedef struct
{
    int (*status)(const CliIppCodeCfg *args);
} CliIppCodeNs;
EMBED_TABLE_LAYOUT(CliIppCodeNs, status);
EMBED_TABLE_STORAGE CliIppCodeNs cli_ipp_code EMBED_UNUSED = {
    .status = cli_ipp_status,
};

typedef struct
{
    int (*printer)(const CliIppErrMsgUriPrinterCfg *args);
} CliIppErrMsgUriPrinterNs;
EMBED_TABLE_LAYOUT(CliIppErrMsgUriPrinterNs, printer);
EMBED_TABLE_STORAGE CliIppErrMsgUriPrinterNs cli_ipp_err_msg_uri_printer EMBED_UNUSED = {
    .printer = cli_ipp_printer,
};

typedef struct
{
    int (*print)(const CliIppSizeErrMsgCfg *args);
} CliIppSizeErrMsgNs;
EMBED_TABLE_LAYOUT(CliIppSizeErrMsgNs, print);
EMBED_TABLE_STORAGE CliIppSizeErrMsgNs cli_ipp_size_err_msg EMBED_UNUSED = {
    .print = cli_ipp_print,
};

typedef struct
{
    int (*job_state)(const CliIppErrMsgUriCfg *args);
    int (*cancel)(const CliIppErrMsgUriCfg *args);
} CliIppErrMsgUriNs;
EMBED_TABLE_LAYOUT(CliIppErrMsgUriNs, job_state, cancel);
EMBED_TABLE_STORAGE CliIppErrMsgUriNs cli_ipp_err_msg_uri EMBED_UNUSED = {
    .job_state = cli_ipp_job_state,
    .cancel = cli_ipp_cancel,
};

typedef struct
{
    int (*cups_printers)(const CliIppErrMsgHostCfg *args);
} CliIppErrMsgHostNs;
EMBED_TABLE_LAYOUT(CliIppErrMsgHostNs, cups_printers);
EMBED_TABLE_STORAGE CliIppErrMsgHostNs cli_ipp_err_msg_host EMBED_UNUSED = {
    .cups_printers = cli_ipp_cups_printers,
};

#endif
