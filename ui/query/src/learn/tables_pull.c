// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// tables_pull.c: the tables of every architecture wanted that lacks them pulled through the harness's image, and the
// two readings of a pulled table, `read <arch> [--bank <size>] <low> <high>...` and `fields <arch>`, each a run of the
// `orior_tables_read` tool over the architecture's instruction set
#include "tables_internal.h"

// The tables of every architecture wanted that lacks them pulled: 0 where each now stands, 1 where the funnel found
// no tables in some capture, -1 with why given. Each program runs through the writer's tee, the capture as
// `tables_capture.console` and the funnel as `orior_tables_funnel.console`
static int query_pulled(QueryContext *context, const QueryRun *run, const char *const *words, unsigned int count,
                        VerbaLinea *err_msg)
{
    char out[QUERY_PATH_MAX_LEN];
    char tables[QUERY_PATH_MAX_LEN];
    query_at(context, QUERY_TABLES_OUT, out);
    query_tables_in(out, "tables", tables);
    static char s_wanted[QUERY_ARCHITECTURES_MOST][16];
    static char s_missing[QUERY_ARCHITECTURES_MOST][16];
    const unsigned int wanted = query_architectures(context, run, words, count, s_wanted, err_msg);
    if (wanted == 0u)
    {
        return -1;
    }
    unsigned int missing = 0u;
    char instructions[QUERY_PATH_MAX_LEN];
    char latencies[QUERY_PATH_MAX_LEN];
    char line_text[QUERY_PATH_MAX_LEN + 128u];
    VerbaLinea line = {line_text, sizeof(line_text), 0ull};
    for (unsigned int at = 0u; at < wanted; at += 1u)
    {
        query_tables_of(out, s_wanted[at], instructions, latencies);
        if (query_is_file(instructions) && query_is_file(latencies))
        {
            line.at = 0ull;
            mmgr_line_put(&line, "  ");
            mmgr_line_put(&line, s_wanted[at]);
            mmgr_line_put(&line, ": its tables stand");
            mmgr_line_finish(&line);
            query_print(context, line_text);
            continue;
        }
        memor.cpy(&(MemoriaCfg){.dst = s_missing[missing], .src = s_wanted[at], .bytes = 16u});
        missing += 1u;
    }
    if (missing == 0u)
    {
        line.at = 0ull;
        mmgr_line_put(&line, "tables: every one of ");
        mmgr_line_decimal(&line, wanted, 0u);
        mmgr_line_put(&line, " architectures stands under ");
        mmgr_line_put(&line, tables);
        mmgr_line_finish(&line);
        query_print(context, line_text);
        return 0;
    }
    static char s_disassembler[QUERY_PATH_MAX_LEN];
    static char s_cubins[QUERY_ARCHITECTURES_MOST][QUERY_PATH_MAX_LEN];
    if (!query_disassembler(context, run, out, s_disassembler, err_msg))
    {
        return -1;
    }
    for (unsigned int at = 0u; at < missing; at += 1u)
    {
        if (!query_key_cubin(context, run, out, s_missing[at], s_cubins[at], err_msg))
        {
            return -1;
        }
    }
    static char s_docker[QUERY_PATH_MAX_LEN];
    if (!query_program(context, QUERY_ROLE_DOCKER, s_docker, err_msg))
    {
        return -1;
    }
    char mount[QUERY_PATH_MAX_LEN + 8u];
    VerbaLinea mount_line = {mount, sizeof(mount), 0ull};
    mmgr_line_put(&mount_line, context->root);
    mmgr_line_put(&mount_line, ":/tree");
    mmgr_line_finish(&mount_line);
    char source[QUERY_PATH_MAX_LEN];
    char capture_script[QUERY_PATH_MAX_LEN];
    char intercept[QUERY_PATH_MAX_LEN];
    char captures[QUERY_PATH_MAX_LEN];
    char inside_disassembler[QUERY_PATH_MAX_LEN];
    query_at(context, QUERY_TABLES_SOURCE, source);
    query_tables_in(source, "tables_capture.sh", capture_script);
    query_tables_in(source, "tables_intercept.c", intercept);
    query_tables_in(out, "captures", captures);
    static QueryWords s_docker_words;
    memor.set(&(MemoriaCfg){.dst = &s_docker_words, .val = 0, .bytes = sizeof(s_docker_words)});
    static const char *const s_head[6] = {"run", "--rm", "-v", NULL, QUERY_TABLES_IMAGE, "sh"};
    query_words_add(&s_docker_words, s_docker);
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        query_words_add(&s_docker_words, (at == 3u) ? mount : s_head[at]);
    }
    const char *const inside_of[4] = {capture_script, s_disassembler, intercept, captures};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        query_in_container(context->root, inside_of[at], inside_disassembler);
        query_words_text(&s_docker_words, inside_disassembler);
    }
    for (unsigned int at = 0u; at < missing; at += 1u)
    {
        query_in_container(context->root, s_cubins[at], inside_disassembler);
        query_words_text(&s_docker_words, inside_disassembler);
    }
    line.at = 0ull;
    mmgr_line_put(&line, "  the disassembler run over ");
    mmgr_line_decimal(&line, missing, 0u);
    mmgr_line_put(&line, " key cubins in " QUERY_TABLES_IMAGE);
    mmgr_line_finish(&line);
    query_print(context, line_text);
    long long code = -1ll;
    if (!query_context_teed(context, run, "tables_capture", s_docker_words.word, &code, err_msg))
    {
        return -1;
    }
    if (code != 0ll)
    {
        mmgr_line_put(err_msg, "the capture in " QUERY_TABLES_IMAGE " did not end clean");
        return -1;
    }
    static char s_funnel[QUERY_PATH_MAX_LEN];
    if (!query_context_tool(context, "orior_tables_funnel", s_funnel, err_msg) || !query_folder_made(tables))
    {
        return -1;
    }
    unsigned int written = 0u;
    for (unsigned int at = 0u; at < missing; at += 1u)
    {
        char capture_name[32];
        char capture[QUERY_PATH_MAX_LEN];
        VerbaLinea capture_line = {capture_name, sizeof(capture_name), 0ull};
        mmgr_line_put(&capture_line, s_missing[at]);
        mmgr_line_put(&capture_line, ".capture");
        mmgr_line_finish(&capture_line);
        query_tables_in(captures, capture_name, capture);
        query_tables_of(out, s_missing[at], instructions, latencies);
        const char *const funnel[5] = {s_funnel, capture, instructions, latencies, NULL};
        if (!query_context_teed(context, run, "orior_tables_funnel", funnel, &code, err_msg))
        {
            return -1;
        }
        line.at = 0ull;
        mmgr_line_put(&line, "  ");
        mmgr_line_put(&line, s_missing[at]);
        if (code == 0ll)
        {
            written += 1u;
            mmgr_line_put(&line, ": its tables written");
        }
        else
        {
            mmgr_line_put(&line, ": the funnel found no tables in its capture, exit ");
            mmgr_line_decimal(&line, (unsigned long long)code, 0u);
        }
        mmgr_line_finish(&line);
        query_print(context, line_text);
    }
    line.at = 0ull;
    mmgr_line_put(&line, "tables: ");
    mmgr_line_decimal(&line, written, 0u);
    mmgr_line_put(&line, " of ");
    mmgr_line_decimal(&line, missing, 0u);
    mmgr_line_put(&line, " architectures written under ");
    mmgr_line_put(&line, tables);
    mmgr_line_finish(&line);
    query_print(context, line_text);
    return (written == missing) ? 0 : 1;
}

// The two readings of a pulled table, `read <arch> [--bank <size>] <low> <high>...` and `fields <arch>`: 1 where the
// words request one, its mode in `mode`, 0 where they request a pull, -1 with why given
int query_reading(const char *const *words, unsigned int count, const char **mode, VerbaLinea *err_msg)
{
    if (count == 0u)
    {
        return 0;
    }
    *mode = (cellul.eq(&(CatenaFinitaCfg){.src = words[0], .other = "read", .cap = CLI_CELLS_POOL}))     ? "class"
            : (cellul.eq(&(CatenaFinitaCfg){.src = words[0], .other = "fields", .cap = CLI_CELLS_POOL})) ? "spans"
                                                                                                         : NULL;
    if (*mode == NULL)
    {
        return 0;
    }
    if (count < 2u)
    {
        mmgr_line_put(err_msg, "tables ");
        mmgr_line_put(err_msg, words[0]);
        mmgr_line_put(err_msg, " takes the architecture, sm_86, first");
        return -1;
    }
    const unsigned int tail = count - 2u;
    if ((cellul.eq(&(CatenaFinitaCfg){.src = *mode, .other = "spans", .cap = CLI_CELLS_POOL})) && (tail != 0u))
    {
        mmgr_line_put(err_msg, "tables fields takes the architecture alone");
        return -1;
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = *mode, .other = "class", .cap = CLI_CELLS_POOL}))
    {
        const int banked =
            (tail != 0u) && (cellul.eq(&(CatenaFinitaCfg){.src = words[2], .other = "--bank", .cap = CLI_CELLS_POOL}));
        const unsigned int encodings = (banked && (tail >= 2u)) ? (tail - 2u) : (banked ? 0u : tail);
        if ((banked && (tail < 2u)) || (encodings < 2u) || ((encodings % 2u) != 0u))
        {
            mmgr_line_put(err_msg, "tables read takes the architecture, --bank <size> where the bank's size is known, "
                                   "then each encoding's two words");
            return -1;
        }
    }
    return 1;
}

int query_tables_run(QueryContext *context, const char *const *words, unsigned int count, VerbaLinea *err_msg)
{
    const char *mode = NULL;
    const int parsed = query_reading(words, count, &mode, err_msg);
    if (parsed < 0)
    {
        return -1;
    }
    static QueryRun s_run;
    if (!query_context_started(context, &s_run, QUERY_TABLES_OUT "/qry", err_msg))
    {
        return -1;
    }
    char why[QUERY_LINE_MAX_LEN];
    VerbaLinea failed = {why, sizeof(why), 0ull};
    int code = -1;
    if (parsed == 0)
    {
        code = query_pulled(context, &s_run, words, count, &failed);
    }
    else
    {
        // `read` and `fields` run the reader over the architecture's table through the writer's tee
        char out[QUERY_PATH_MAX_LEN];
        char instructions[QUERY_PATH_MAX_LEN];
        char latencies[QUERY_PATH_MAX_LEN];
        query_at(context, QUERY_TABLES_OUT, out);
        query_tables_of(out, words[1], instructions, latencies);
        static char s_reader[QUERY_PATH_MAX_LEN];
        if (!query_is_file(instructions))
        {
            mmgr_line_put(&failed, "no table for ");
            mmgr_line_put(&failed, words[1]);
            mmgr_line_put(&failed, ": run orior_cli protocol tables ");
            mmgr_line_put(&failed, words[1]);
            mmgr_line_put(&failed, " first");
        }
        else if (query_context_tool(context, "orior_tables_read", s_reader, &failed))
        {
            static QueryWords s_words;
            memor.set(&(MemoriaCfg){.dst = &s_words, .val = 0, .bytes = sizeof(s_words)});
            query_words_add(&s_words, s_reader);
            query_words_add(&s_words, instructions);
            query_words_add(&s_words, mode);
            for (unsigned int at = 2u; at < count; at += 1u)
            {
                query_words_add(&s_words, words[at]);
            }
            long long ran = -1ll;
            // an exit code is far below what an int represents
            code =
                query_context_teed(context, &s_run, "orior_tables_read", s_words.word, &ran, &failed) ? (int)ran : -1;
        }
    }
    if ((code < 0) && (mmgr_line_finish(&failed) != 0ull))
    {
        query_print(context, why);
    }
    if (!query_context_ended(context, &s_run, err_msg))
    {
        return -1;
    }
    if (code < 0)
    {
        mmgr_line_put_most(err_msg, why, failed.at);
    }
    return code;
}
