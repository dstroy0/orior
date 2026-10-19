// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// debug_bytecode.c: the bytecode of the function of a Python file that holds a line, as the tree's Python compiles it,
// read by a shim given to that Python as its input: the file, then the line, counted from 1, its words
#include "../command_line/cells.h"
#include "../tests/test_runs.h"
#include "debug_internal.h"

// the shim that writes the bytecode of the function that holds a line of a Python file
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

// the last line of what the shim wrote to its errors, the whole trimmed first, into `said`; where it wrote none, that
// the bytecode could not be read
static void bytecode_failed(const CliRan *ran, CliLine *said)
{
    unsigned long long start = 0ull;
    unsigned long long end = ran->err_size;
    while ((start < end) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = ran->err[start]}) != NULL))
    {
        start += 1ull;
    }
    while ((end > start) && (cellul.chr(&(CatenaFinitaCfg){
                                 .src = " \t\n\r\f\v", .cap = CLI_CELLS_POOL, .byte = ran->err[end - 1ull]}) != NULL))
    {
        end -= 1ull;
    }
    if (end == start)
    {
        cli_line_text(said, "the bytecode could not be read");
        return;
    }
    unsigned long long last = end;
    while ((last > start) && (ran->err[last - 1ull] != '\n'))
    {
        last -= 1ull;
    }
    const unsigned long long stop = ((end > last) && (ran->err[end - 1ull] == '\r')) ? end - 1ull : end;
    cli_lossy(said, (const unsigned char *)ran->err + last, stop - last);
}

// an instruction the shim wrote, written as the window reads one
static void bytecode_instruction(const CliJson *json, unsigned int one, CliLine *out)
{
    unsigned long long line = 0ull;
    unsigned long long name_size = 0ull;
    unsigned long long argument_size = 0ull;
    const char *const name = cli_json_string(json, cli_json_member(json, one, "name"), &name_size);
    const char *const argument = cli_json_string(json, cli_json_member(json, one, "argument"), &argument_size);
    char text[CLI_NAME_LONGEST * 4u];
    CliLine joined = {text, sizeof(text), 0ull};
    cli_line_counted(&joined, (name != NULL) ? name : "", (name != NULL) ? name_size : 0ull);
    cli_line_character(&joined, ' ');
    cli_line_counted(&joined, (argument != NULL) ? argument : "", (argument != NULL) ? argument_size : 0ull);
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
    cli_line_text(out, "{\"address\":");
    // the offset as JSON writes it, quoted as the window reads an address
    char offset[64];
    CliLine offset_line = {offset, sizeof(offset), 0ull};
    const unsigned int offset_node = cli_json_member(json, one, "offset");
    if (offset_node != CLI_JSON_NONE)
    {
        cli_json_write(json, offset_node, &offset_line, 0u, 0);
    }
    else
    {
        cli_line_text(&offset_line, "null");
    }
    cli_json_quoted_lossy(out, (const unsigned char *)offset_line.out, offset_line.at);
    cli_line_text(out, ",\"text\":");
    cli_json_quoted_lossy(out, (const unsigned char *)text + start, end - start);
    cli_line_text(out, ",\"bytes\":\"\",\"symbol\":\"\",\"line\":");
    if (cli_debug_unsigned(json, cli_json_member(json, one, "line"), &line))
    {
        cli_line_decimal(out, ((line > 0ull) ? line - 1ull : 0ull) & 0xffffffffull, 1u);
    }
    else
    {
        cli_line_text(out, "null");
    }
    cli_line_character(out, '}');
}

int cli_debug_bytecode(const char *root, const char *path, unsigned int line, CliLine *out, CliLine *said)
{
    char python[CLI_PATH_LONGEST];
    if (!cli_test_python(python))
    {
        cli_line_text(said, "No Python is found to read the bytecode: File, Toolchains finds one");
        return 0;
    }
    char number[32];
    CliLine line_text = {number, sizeof(number), 0ull};
    cli_line_decimal(&line_text, (unsigned long long)line + 1ull, 1u);
    cli_line_closed(&line_text);
    const char *const words[5] = {python, "-", path, number, NULL};
    char why[CLI_SAID_LONGEST];
    CliLine unsaid = {why, sizeof(why), 0ull};
    CliRan *const ran = cli_ran_taken();
    if (!cli_ran_fed(words, root, NULL, 0u, s_shim, sizeof(s_shim) - 1u, ran, &unsaid))
    {
        cli_ran_given(ran);
        cli_line_text(said, python);
        cli_line_text(said, ": ");
        cli_line_counted(said, unsaid.out, unsaid.at);
        return 0;
    }
    if (ran->code != 0ll)
    {
        bytecode_failed(ran, said);
        cli_ran_given(ran);
        return 0;
    }
    CliJson json;
    const unsigned int value = cli_json_taken(&json, ran->out, ran->out_size, 0u, 0ull, said);
    cli_ran_given(ran);
    if (value == CLI_JSON_NONE)
    {
        cli_json_given(&json);
        return 0;
    }
    const unsigned int instructions = cli_json_member(&json, value, "instructions");
    unsigned long long name_size = 0ull;
    const char *const name = cli_json_string(&json, cli_json_member(&json, value, "name"), &name_size);
    cli_line_character(out, '[');
    cli_json_quoted_lossy(out, (const unsigned char *)((name != NULL) ? name : ""), (name != NULL) ? name_size : 0ull);
    cli_line_text(out, ",[");
    for (unsigned int at = 0u; cli_json_item(&json, instructions, at) != CLI_JSON_NONE; at += 1u)
    {
        cli_line_text(out, (at > 0u) ? "," : "");
        bytecode_instruction(&json, cli_json_item(&json, instructions, at), out);
    }
    cli_line_text(out, "]]");
    cli_json_given(&json);
    return 1;
}
