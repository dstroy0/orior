// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// inspect_scan.c: a text read by its letters, its line and its column counted in UTF-16 units as the editor and the
// protocol count columns; the tokens and the comments a reader records; and a set of names
#include "../text/json_tree.h"
#include "../text/regexp.h"
#include "inspect_internal.h"

CliInspectTok *g_cli_inspect_tok;
unsigned int g_cli_inspect_toks;
CliInspectSpan *g_cli_inspect_comment;
unsigned int g_cli_inspect_comments;
int g_cli_inspect_over;

void cli_inspect_scan_layout(const CliInspectPartitionCtx *args)
{
    g_cli_inspect_tok =
        EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx, .partition = args->partition,
                   .bytes = (unsigned long long)CLI_INSPECT_TOKENS * sizeof(CliInspectTok));
    g_cli_inspect_comment =
        EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx, .partition = args->partition,
                   .bytes = (unsigned long long)CLI_INSPECT_COMMENTS * sizeof(CliInspectSpan));
}

void cli_inspect_scan_begin(const CliInspectSizeScanCtx *args)
{
    CliInspectScan *scan = args->scan_out;
    *scan = (CliInspectScan){args->src, args->size, 0ULL, 0u, 0u};
}

typedef struct
{
    const CliInspectScan *scan;
    unsigned long long pos;
    unsigned long long *reserved;
    const CliInspectSet *set;
    const char *at;
    unsigned int size;
} InspectScanCtx;
// the letter at `pos` of the scan's text, its bytes into `reserved`
EMBED_INLINE unsigned int scan_letter(const InspectScanCtx *args)
{
    unsigned long long *reserved = args->reserved;
    if (args->pos >= args->scan->size)
    {
        *reserved = 0ULL;
        return CLI_INSPECT_END;
    }
    unsigned int letter = 0u;
    *reserved = EMBED_CALL(cli_json_tree_length.lossy_letter, CliJsonTreeLengthCfg,
                           .text_bytes = (const unsigned char *)args->scan->src, .length = args->scan->size,
                           .at = args->pos, .letter = &letter);
    return letter;
}

unsigned int cli_inspect_peek(const CliInspectScanAheadCtx *args)
{
    unsigned long long reserved = 0ULL;
    return EMBED_CALL(scan_letter, InspectScanCtx, .scan = args->scan, .pos = args->scan->pos, .reserved = &reserved);
}

unsigned int cli_inspect_peek_at(const CliInspectScanAheadCtx *args)
{
    unsigned long long pos = args->scan->pos;
    unsigned long long reserved = 0ULL;
    for (unsigned int at = 0u; at < args->ahead; at += 1u)
    {
        if (EMBED_CALL(scan_letter, InspectScanCtx, .scan = args->scan, .pos = pos, .reserved = &reserved) ==
            CLI_INSPECT_END)
        {
            return CLI_INSPECT_END;
        }
        pos += reserved;
    }
    return EMBED_CALL(scan_letter, InspectScanCtx, .scan = args->scan, .pos = pos, .reserved = &reserved);
}

unsigned int cli_inspect_bump(const CliInspectSizeScanCtx *args)
{
    unsigned long long reserved = 0ULL;
    const unsigned int letter = EMBED_CALL(scan_letter, InspectScanCtx, .scan = args->scan_out,
                                           .pos = args->scan_out->pos, .reserved = &reserved);
    if (letter == CLI_INSPECT_END)
    {
        return letter;
    }
    args->scan_out->pos += reserved;
    if (letter == '\n')
    {
        args->scan_out->line += 1u;
        args->scan_out->col = 0u;
    }
    else
    {
        args->scan_out->col += (letter >= 0x10000u) ? 2u : 1u;
    }
    return letter;
}

CliInspectPosition cli_inspect_position(const CliInspectScanCtx *args)
{
    return (CliInspectPosition){args->scan->line, args->scan->col};
}

void cli_inspect_skip_line(const CliInspectSizeScanCtx *args)
{
    const CliInspectPosition from =
        EMBED_CALL(cli_inspect_scan_internal.place, CliInspectScanCtx, .scan = args->scan_out);
    unsigned int letter =
        EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = args->scan_out);
    while ((letter != CLI_INSPECT_END) && (letter != '\n') && (letter != '\r'))
    {
        (void)cli_inspect_size_scan_internal.bump(args);
        letter = EMBED_CALL(cli_inspect_scan_ahead_internal.peek, CliInspectScanAheadCtx, .scan = args->scan_out);
    }
    EMBED_CALL(cli_inspect_from_to_internal.comment_append, CliInspectFromToCtx, .from = from,
               .to = EMBED_CALL(cli_inspect_scan_internal.place, CliInspectScanCtx, .scan = args->scan_out));
}

void cli_inspect_tok_append(const CliInspectScanCtx *args)
{
    if (g_cli_inspect_toks == CLI_INSPECT_TOKENS)
    {
        g_cli_inspect_over = 1;
        return;
    }
    // a text read is at most 4 GiB, which a token's bytes count
    g_cli_inspect_tok[g_cli_inspect_toks] = (CliInspectTok){
        args->type, (unsigned char)(args->field != 0),    (unsigned int)args->start, (unsigned int)args->scan->pos,
        args->from, cli_inspect_scan_internal.place(args)};
    g_cli_inspect_toks += 1u;
}

void cli_inspect_comment_append(const CliInspectFromToCtx *args)
{
    if (g_cli_inspect_comments == CLI_INSPECT_COMMENTS)
    {
        g_cli_inspect_over = 1;
        return;
    }
    g_cli_inspect_comment[g_cli_inspect_comments] = (CliInspectSpan){args->from, args->to};
    g_cli_inspect_comments += 1u;
}

int cli_inspect_alphabetic(const CliInspectCtx *args)
{
    if (args->letter < 0x80u)
    {
        return ((args->letter >= 'a') && (args->letter <= 'z')) || ((args->letter >= 'A') && (args->letter <= 'Z'));
    }
    return (args->letter != CLI_INSPECT_END) && EMBED_CALL(cli_regexp_letter.property, CliRegexpLetterCfg,
                                                           .property = CLI_REGEXP_LETTER, .letter = args->letter);
}

int cli_inspect_alphanumeric(const CliInspectCtx *args)
{
    if (args->letter < 0x80u)
    {
        return cli_inspect_internal.alphabetic(args) || ((args->letter >= '0') && (args->letter <= '9'));
    }
    return (args->letter != CLI_INSPECT_END) && (EMBED_CALL(cli_regexp_letter.property, CliRegexpLetterCfg,
                                                            .property = CLI_REGEXP_LETTER, .letter = args->letter) ||
                                                 EMBED_CALL(cli_regexp_letter.property, CliRegexpLetterCfg,
                                                            .property = CLI_REGEXP_NUMBER, .letter = args->letter));
}

int cli_inspect_white(const CliInspectCtx *args)
{
    return ((args->letter >= 0x09u) && (args->letter <= 0x0du)) || (args->letter == 0x20u) || (args->letter == 0x85u) ||
           (args->letter == 0xa0u) || (args->letter == 0x1680u) ||
           ((args->letter >= 0x2000u) && (args->letter <= 0x200au)) || (args->letter == 0x2028u) ||
           (args->letter == 0x2029u) || (args->letter == 0x202fu) || (args->letter == 0x205fu) ||
           (args->letter == 0x3000u);
}

// the entry of a set a name hashes to, FNV-1a over its bytes
EMBED_INLINE unsigned int set_entry(const InspectScanCtx *args)
{
    unsigned int hash = 2166136261u;
    for (unsigned int index = 0u; index < args->size; index += 1u)
    {
        hash = (hash ^ (unsigned char)args->at[index]) * 16777619u;
    }
    return hash & (args->set->buckets - 1u);
}

void cli_inspect_set_layout(const CliInspectPartitionSetCtx *args)
{
    unsigned int buckets = 16u;
    while ((buckets / 2u) < g_cli_inspect_max.names)
    {
        buckets <<= 1u;
    }
    args->set->buckets = buckets;
    args->set->count = 0u;
    args->set->at =
        EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx, .partition = args->partition,
                   .bytes = (unsigned long long)buckets * sizeof(const char *));
    args->set->size =
        EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx, .partition = args->partition,
                   .bytes = (unsigned long long)buckets * sizeof(unsigned int));
    args->set->reserved =
        EMBED_CALL(cli_inspect_partition_internal.partition, CliInspectPartitionCtx, .partition = args->partition,
                   .bytes = (unsigned long long)(buckets / 2u) * sizeof(unsigned int));
    if (args->set->at != NULL)
    {
        memor.set(
            &(MemoriaCfg){.dst = (void *)args->set->at, .val = 0, .bytes = (size_t)buckets * sizeof(const char *)});
    }
}

void cli_inspect_set_clear(const CliInspectPartitionSetCtx *args)
{
    for (unsigned int index = 0u; index < args->set->count; index += 1u)
    {
        args->set->at[args->set->reserved[index]] = NULL;
    }
    args->set->count = 0u;
}

void cli_inspect_set_append(const CliInspectSetCtx *args)
{
    unsigned int entry =
        EMBED_CALL(set_entry, InspectScanCtx, .set = args->set, .at = args->at, .size = args->size_u32);
    while (args->set->at[entry] != NULL)
    {
        if ((args->set->size[entry] == args->size_u32) &&
            (memor.cmp(&(MemoriaCfg){.src = args->set->at[entry], .other = args->at, .bytes = args->size_u32}) == 0))
        {
            return;
        }
        entry = (entry + 1u) & (args->set->buckets - 1u);
    }
    if (args->set->count == (args->set->buckets / 2u))
    {
        return;
    }
    args->set->at[entry] = args->at;
    args->set->size[entry] = args->size_u32;
    args->set->reserved[args->set->count] = entry;
    args->set->count += 1u;
}

int cli_inspect_set_has(const CliInspectAtCtx *args)
{
    unsigned int entry =
        EMBED_CALL(set_entry, InspectScanCtx, .set = args->set_read, .at = args->at, .size = args->size_u32);
    while (args->set_read->at[entry] != NULL)
    {
        if ((args->set_read->size[entry] == args->size_u32) &&
            (memor.cmp(&(MemoriaCfg){.src = args->set_read->at[entry], .other = args->at, .bytes = args->size_u32}) ==
             0))
        {
            return 1;
        }
        entry = (entry + 1u) & (args->set_read->buckets - 1u);
    }
    return 0;
}
