// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_bytecode.c: the bytecode of the function of a Python file that contains a line, as the tree's Python compiles
// it, read by a shim given to that Python as its input: the file, then the line, counted from 1, its words
#include "../command_line/cells.h"
#include "../tests/test_runs.h"
#include "../text/text.h"
#include "debug_internal.h"

// the shim that writes the bytecode of the function that contains a line of a Python file
static const char s_shim[] =
    "\n"
    "import dis, json, sys\n"
    "path, line = sys.argv[1], int(sys.argv[2])\n"
    "with open(path, encoding=\"utf-8\") as handle:\n"
    "    top = compile(handle.read(), path, \"exec\")\n"
    "best, codes = top, [top]\n"
    "while codes:\n"
    "    code = codes.pop()\n"
    "    lines = [number for _, _, number in code.co_lines() if number]\n"
    "    if lines and min(lines) <= line <= max(lines) and code is not top:\n"
    "        best = code\n"
    "    codes.extend(inner for inner in code.co_consts if hasattr(inner, \"co_lines\"))\n"
    "out = []\n"
    "for one in dis.get_instructions(best):\n"
    "    out.append({\"offset\": one.offset, \"name\": one.opname, \"argument\": one.argrepr, \"line\": "
    "one.positions.lineno if one.positions else None})\n"
    "print(json.dumps({\"name\": best.co_qualname if hasattr(best, \"co_qualname\") else best.co_name, "
    "\"instructions\": out}))\n";

typedef struct
{
    const CliRunResult *run_result;
    VerbaLinea *err_msg;
    const CliJson *json;
    unsigned int one;
    VerbaLinea *out;
} DebugBytecodeCtx;
// the last line of what the shim wrote to its errors, the whole trimmed first, into `err_msg`; where it wrote none,
// that the bytecode could not be read
EMBED_INLINE void bytecode_fail(const DebugBytecodeCtx *args)
{
    unsigned long long start = 0ull;
    unsigned long long end = args->run_result->err_size;
    while ((start < end) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = args->run_result->err[start]}) != NULL))
    {
        start += 1ull;
    }
    while ((end > start) &&
           (cellul.chr(&(CatenaFinitaCfg){
                .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = args->run_result->err[end - 1ull]}) != NULL))
    {
        end -= 1ull;
    }
    if (end == start)
    {
        mmgr_line_put(args->err_msg, "the bytecode could not be read");
        return;
    }
    unsigned long long last = end;
    last = (start + EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg,
                               .text = args->run_result->err + start, .at = last - start, .set = "\n"));
    const unsigned long long stop = ((end > last) && (args->run_result->err[end - 1ull] == '\r')) ? end - 1ull : end;
    EMBED_CALL(cli_json_tree_length_out.lossy, CliJsonTreeLengthOutCfg, .out = args->err_msg,
               .text_bytes = (const unsigned char *)args->run_result->err + last, .length = stop - last);
}

// an instruction the shim wrote, written as the window reads one
EMBED_INLINE void bytecode_instruction(const DebugBytecodeCtx *args)
{
    const CliJson *json = args->json;
    VerbaLinea *out = args->out;
    unsigned long long line = 0ull;
    unsigned long long name_size = 0ull;
    unsigned long long argument_size = 0ull;
    const char *const name =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->one, .name = "name"),
                   .length_pointer = &name_size);
    const char *const argument =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->one, .name = "argument"),
                   .length_pointer = &argument_size);
    char text[CLI_NAME_MAX_LEN * 4u];
    VerbaLinea joined = {text, sizeof(text), 0ull};
    mmgr_line_put_n(&joined, (name != NULL) ? name : "", name_size);
    mmgr_line_ch(&joined, ' ');
    mmgr_line_put_n(&joined, (argument != NULL) ? argument : "", argument_size);
    unsigned long long start = 0ull;
    unsigned long long end = joined.at;
    while ((start < end) &&
           (cellul.chr(&(CatenaFinitaCfg){.src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = text[start]}) != NULL))
    {
        start += 1ull;
    }
    while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = text[end - 1ull]}) != NULL))
    {
        end -= 1ull;
    }
    mmgr_line_put(out, "{\"address\":");
    // the offset as JSON writes it, quoted as the window reads an address
    char offset[64];
    VerbaLinea offset_line = {offset, sizeof(offset), 0ull};
    const unsigned int offset_node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                .json_read = json, .object = args->one, .name = "offset");
    if (offset_node != CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree_node_out.json_write, CliJsonTreeNodeOutCfg, .json_read = json, .node = offset_node,
                   .out = &offset_line, .indent = 0u, .sorted = 0);
    }
    else
    {
        mmgr_line_put(&offset_line, "null");
    }
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)offset_line.out, .length = offset_line.at);
    mmgr_line_put(out, ",\"text\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)text + start, .length = end - start);
    mmgr_line_put(out, ",\"bytes\":\"\",\"symbol\":\"\",\"line\":");
    if (EMBED_CALL(cli_debug_json_node_value_u_64s_internal.debug_unsigned, CliDebugJsonNodeValueU64sCtx, .json = json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = json,
                                      .object = args->one, .name = "line"),
                   .value_u64s = &line))
    {
        mmgr_line_decimal(out, ((line > 0ull) ? line - 1ull : 0ull) & 0xffffffffull, 1u);
    }
    else
    {
        mmgr_line_put(out, "null");
    }
    mmgr_line_ch(out, '}');
}

int cli_debug_bytecode(const CliDebugRootOutCfg *args)
{
    char python[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_test_runs.test_python, CliTestRunsCfg, .python = python))
    {
        mmgr_line_put(args->err_msg, "No Python is found to read the bytecode: File, Toolchains finds one");
        return 0;
    }
    char number[32];
    VerbaLinea line_text = {number, sizeof(number), 0ull};
    mmgr_line_decimal(&line_text, (unsigned long long)args->line + 1ull, 1u);
    mmgr_line_finish(&line_text);
    const char *const words[5] = {python, "-", args->path, number, NULL};
    char why[CLI_ERR_MSG_MAX_LEN];
    VerbaLinea err_buf = {why, sizeof(why), 0ull};
    CliRunResult *const run_result = cli_run_result_alloc();
    if (!EMBED_CALL(cli_words_internal.run_result_feed, CliWordsCtx, .words = words, .folder = args->root,
                    .settings = NULL, .count = 0u, .input = s_shim, .length = sizeof(s_shim) - 1u,
                    .run_result = run_result, .err_msg = &err_buf))
    {
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        mmgr_line_put(args->err_msg, python);
        mmgr_line_put(args->err_msg, ": ");
        mmgr_line_put_n(args->err_msg, err_buf.out, err_buf.at);
        return 0;
    }
    if (run_result->code != 0ll)
    {
        EMBED_CALL(bytecode_fail, DebugBytecodeCtx, .run_result = run_result, .err_msg = args->err_msg);
        EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
        return 0;
    }
    CliJson json;
    const unsigned int value =
        EMBED_CALL(cli_json_tree_text_size_err_msg.json_alloc, CliJsonTreeTextSizeErrMsgCfg, .json = &json,
                   .text = run_result->out, .size = run_result->out_size, .nodes_more = 0u, .text_more = 0ull,
                   .err_msg = args->err_msg, .owner = &g_cli_debug_cells);
    EMBED_CALL(cli_words_internal.run_result_free, CliWordsCtx, .run_result = run_result);
    if (value == CLI_JSON_NONE)
    {
        EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
        return 0;
    }
    const unsigned int instructions = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg,
                                                 .json_read = &json, .object = value, .name = "instructions");
    unsigned long long name_size = 0ull;
    const char *const name =
        EMBED_CALL(cli_json_tree_node_length.json_string, CliJsonTreeNodeLengthCfg, .json_read = &json,
                   .node = EMBED_CALL(cli_json_tree_object.json_member, CliJsonTreeObjectCfg, .json_read = &json,
                                      .object = value, .name = "name"),
                   .length_pointer = &name_size);
    mmgr_line_ch(args->out, '[');
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = args->out,
               .text_bytes = (const unsigned char *)((name != NULL) ? name : ""), .length = name_size);
    mmgr_line_put(args->out, ",[");
    for (unsigned int at = 0u; EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg,
                                          .json_read = &json, .array = instructions, .index = at) != CLI_JSON_NONE;
         at += 1u)
    {
        mmgr_line_put(args->out, (at > 0u) ? "," : "");
        EMBED_CALL(bytecode_instruction, DebugBytecodeCtx, .json = &json,
                   .one = EMBED_CALL(cli_json_tree_array_index.json_item, CliJsonTreeArrayIndexCfg, .json_read = &json,
                                     .array = instructions, .index = at),
                   .out = args->out);
    }
    mmgr_line_put(args->out, "]]");
    EMBED_CALL(cli_json_tree.json_free, CliJsonTreeCfg, .json = &json);
    return 1;
}
