// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// python_paths_eval.c: what an expression read from a script may be, as lists of paths: `__file__`, strings, the
// names set before it, the folder above, a join, and the calls of `os.path` and `pathlib` that give a path
#include "../text/text.h"
#include "python_paths_internal.h"

// the most folders one folder holds that a loop over its entries takes
#define CLI_PY_FOLDERS 4096u
#define CLI_PY_FOLDER_TEXT (1u << 20u)

#if defined(_WIN32)
#define CLI_PY_SEPARATOR '\\'
#else
#define CLI_PY_SEPARATOR '/'
#endif

void cli_py_value_put(unsigned int text)
{
    if ((text != CLI_PY_NONE) && (g_cli_py.values < CLI_PY_VALUES))
    {
        g_cli_py.value[g_cli_py.values] = text;
        g_cli_py.values += 1u;
    }
}

CliValues cli_py_values_from(unsigned int first)
{
    return (CliValues){first, g_cli_py.values - first};
}

// no values
static CliValues cli_none(void)
{
    return (CliValues){g_cli_py.values, 0u};
}

// a text written into `line` put as a value
static void cli_line_put(const CliLine *line)
{
    if (cli_line_fits(line))
    {
        cli_py_value_put(cli_py_text_put(line->out, line->at));
    }
}

// whether a byte parts a path
static int cli_parts(char letter)
{
    return (letter == '/') || (letter == '\\');
}

void cli_py_parent(const char *path, CliLine *out)
{
    const char *last = NULL;
    for (const char *at = path; at[0] != '\0'; at += 1)
    {
        last = cli_parts(at[0]) ? at : last;
    }
    if (last == NULL)
    {
        return;
    }
    // the slash lies in the path, at or past its start
    const unsigned long long at = (unsigned long long)(last - path);
    const unsigned long long length = (at == 0ull) ? 1ull : (((at == 2ull) && (path[1] == ':')) ? 3ull : at);
    cli_line_counted(out, path, length);
}

// whether a path stands alone when it is joined onto another
static int cli_absolute(const char *part)
{
#if defined(_WIN32)
    const int drive =
        (((part[0] | 0x20) >= 'a') && ((part[0] | 0x20) <= 'z')) && (part[1] == ':') && cli_parts(part[2]);
    return drive || ((part[0] == '\\') && (part[1] == '\\'));
#else
    return part[0] == '/';
#endif
}

// `part` joined onto `base` as `os.path.join` joins them, written into `out`
static void cli_joined(const char *base, const char *part, CliLine *out)
{
    if (cli_absolute(part) || (base[0] == '\0'))
    {
        cli_line_text(out, part);
        return;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = base, .cap = CLI_CELLS_POOL});
    cli_line_text(out, base);
    if (!cli_parts(base[length - 1ull]))
    {
        cli_line_character(out, CLI_PY_SEPARATOR);
    }
    cli_line_text(out, part);
}

// every pairing of the values of `one` and `other`, joined, or added where `join` is 0, at most CLI_PY_PAIRS of them
static CliValues cli_pairs(CliValues one, CliValues other, int join)
{
    static char s_made[CLI_PATH_LONGEST * 2u];
    const unsigned int mark = g_cli_py.values;
    unsigned int made = 0u;
    for (unsigned int left = 0u; (left < one.count) && (made < CLI_PY_PAIRS); left += 1u)
    {
        for (unsigned int right = 0u; (right < other.count) && (made < CLI_PY_PAIRS); right += 1u)
        {
            const char *const base = cli_py_text(g_cli_py.value[one.first + left]);
            const char *const part = cli_py_text(g_cli_py.value[other.first + right]);
            CliLine line = {s_made, sizeof(s_made), 0ull};
            if (join)
            {
                cli_joined(base, part, &line);
            }
            else
            {
                cli_line_text(&line, base);
                cli_line_text(&line, part);
            }
            cli_line_put(&line);
            made += 1u;
        }
    }
    return cli_py_values_from(mark);
}

// each value's folder above, `steps` times over
static CliValues cli_parents(CliValues values, long long steps)
{
    static char s_at[CLI_PATH_LONGEST];
    static char s_up[CLI_PATH_LONGEST];
    const unsigned int mark = g_cli_py.values;
    for (unsigned int index = 0u; index < values.count; index += 1u)
    {
        memor.cpy(&(MemoriaCfg){
            .dst = s_at,
            .src = cli_py_text(g_cli_py.value[values.first + index]),
            .bytes = cellul.len(&(CatenaFinitaCfg){.src = cli_py_text(g_cli_py.value[values.first + index]),
                                                   .cap = CLI_CELLS_POOL}) +
                     1u});
        for (long long step = 0ll; step < steps; step += 1ll)
        {
            CliLine up = {s_up, sizeof(s_up), 0ull};
            cli_py_parent(s_at, &up);
            const unsigned long long length = up.at;
            cli_line_closed(&up);
            memor.cpy(&(MemoriaCfg){
                .dst = s_at,
                .src = (length == 0ull) ? "" : s_up,
                .bytes =
                    cellul.len(&(CatenaFinitaCfg){.src = (length == 0ull) ? "" : s_up, .cap = CLI_CELLS_POOL}) + 1u});
        }
        cli_py_value_put(cli_py_text_put(s_at, cellul.len(&(CatenaFinitaCfg){.src = s_at, .cap = CLI_CELLS_POOL})));
    }
    return cli_py_values_from(mark);
}

int cli_py_dotted(unsigned int expr, CliLine *out)
{
    const CliExpr *const one = &g_cli_py.expr[expr];
    if (one->kind == CLI_EXPR_NAME)
    {
        cli_line_text(out, cli_py_text(one->text));
        return 1;
    }
    if ((one->kind == CLI_EXPR_ATTR) && cli_py_dotted(one->left, out))
    {
        cli_line_character(out, '.');
        cli_line_text(out, cli_py_text(one->text));
        return 1;
    }
    return 0;
}

// what the argument `at` of a call may be, none where it has no such argument
static CliValues cli_argument(const CliExpr *call, unsigned int at)
{
    return (at < call->count) ? cli_py_eval(g_cli_py.item[call->first + at]) : cli_none();
}

// a list of joined paths: `all` joined with each of the call's arguments from `from` on
static CliValues cli_joined_all(CliValues all, const CliExpr *call, unsigned int from)
{
    for (unsigned int at = from; at < call->count; at += 1u)
    {
        all = cli_pairs(all, cli_argument(call, at), 1);
    }
    return all;
}

// what a call may be: the calls of `os.path` and `pathlib` that give a path
static CliValues cli_called(const CliExpr *call)
{
    char dotted[CLI_NAME_LONGEST];
    CliLine line = {dotted, sizeof(dotted), 0ull};
    const int named = cli_py_dotted(call->left, &line);
    cli_line_closed(&line);
    const char *const name = named ? dotted : "";
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "os.path.dirname", .cap = CLI_CELLS_POOL}))
    {
        return cli_parents(cli_argument(call, 0u), 1ll);
    }
    static const char *const s_same[5] = {"os.path.abspath", "os.path.realpath", "os.path.normpath", "str",
                                          "os.fspath"};
    static const char *const s_join[5] = {"os.path.join", "Path", "pathlib.Path", "PurePath", "pathlib.PurePath"};
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_same[at], .cap = CLI_CELLS_POOL}))
        {
            return cli_argument(call, 0u);
        }
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_join[at], .cap = CLI_CELLS_POOL}))
        {
            return cli_joined_all(cli_argument(call, 0u), call, 1u);
        }
    }
    const CliExpr *const callee = &g_cli_py.expr[call->left];
    if (callee->kind != CLI_EXPR_ATTR)
    {
        return cli_none();
    }
    const char *const method = cli_py_text(callee->text);
    if ((cellul.eq(&(CatenaFinitaCfg){.src = method, .other = "resolve", .cap = CLI_CELLS_POOL})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = method, .other = "absolute", .cap = CLI_CELLS_POOL})))
    {
        return cli_py_eval(callee->left);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = method, .other = "joinpath", .cap = CLI_CELLS_POOL}))
    {
        return cli_joined_all(cli_py_eval(callee->left), call, 0u);
    }
    return cli_none();
}

// what an index may be: `parents[n]` of a path, and `rsplit(sep, ...)[0]` of a string
static CliValues cli_indexed(const CliExpr *index)
{
    const CliExpr *const of = &g_cli_py.expr[index->left];
    const CliExpr *const at = &g_cli_py.expr[index->right];
    if (at->kind != CLI_EXPR_NUMBER)
    {
        return cli_none();
    }
    if ((of->kind == CLI_EXPR_ATTR) &&
        (cellul.eq(&(CatenaFinitaCfg){.src = cli_py_text(of->text), .other = "parents", .cap = CLI_CELLS_POOL})))
    {
        return cli_parents(cli_py_eval(of->left), at->number + 1ll);
    }
    if ((of->kind != CLI_EXPR_CALL) || (at->number != 0ll) || (of->count == 0u))
    {
        return cli_none();
    }
    const CliExpr *const callee = &g_cli_py.expr[of->left];
    const CliExpr *const sep = &g_cli_py.expr[g_cli_py.item[of->first]];
    if ((callee->kind != CLI_EXPR_ATTR) || (sep->kind != CLI_EXPR_TEXT) ||
        (!cellul.eq(&(CatenaFinitaCfg){.src = cli_py_text(callee->text), .other = "rsplit", .cap = CLI_CELLS_POOL})))
    {
        return cli_none();
    }
    const CliValues values = cli_py_eval(callee->left);
    const char *const parting = cli_py_text(sep->text);
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = parting, .cap = CLI_CELLS_POOL});
    const unsigned int mark = g_cli_py.values;
    for (unsigned int one = 0u; one < values.count; one += 1u)
    {
        const char *const text = cli_py_text(g_cli_py.value[values.first + one]);
        const char *last = NULL;
        for (const char *found =
                 (length != 0ull)
                     ? cellul.find(&(CatenaFinitaCfg){
                           .src = text, .cap = CLI_CELLS_POOL, .other = parting, .other_cap = CLI_CELLS_POOL})
                     : NULL;
             found != NULL;
             found = cellul.find(&(CatenaFinitaCfg){
                 .src = found + 1, .cap = CLI_CELLS_POOL, .other = parting, .other_cap = CLI_CELLS_POOL}))
        {
            last = found;
        }
        // the parting found lies in the text, at or past its start
        cli_py_value_put((last != NULL) ? cli_py_text_put(text, (unsigned long long)(last - text))
                                        : g_cli_py.value[values.first + one]);
    }
    return cli_py_values_from(mark);
}

// the lists of a tuple or list put one after another
static CliValues cli_many(const CliExpr *many)
{
    const unsigned int base = g_cli_py.ranged;
    for (unsigned int at = 0u; at < many->count; at += 1u)
    {
        const CliValues one = cli_py_eval(g_cli_py.item[many->first + at]);
        if (g_cli_py.ranged < CLI_PY_ITEMS)
        {
            g_cli_py.range[g_cli_py.ranged] = one;
            g_cli_py.ranged += 1u;
        }
    }
    const unsigned int mark = g_cli_py.values;
    for (unsigned int at = base; at < g_cli_py.ranged; at += 1u)
    {
        const CliValues one = g_cli_py.range[at];
        for (unsigned int place = 0u; place < one.count; place += 1u)
        {
            cli_py_value_put(g_cli_py.value[one.first + place]);
        }
    }
    g_cli_py.ranged = base;
    return cli_py_values_from(mark);
}

CliValues cli_py_eval(unsigned int expr)
{
    const CliExpr *const one = &g_cli_py.expr[expr];
    const unsigned int mark = g_cli_py.values;
    switch (one->kind)
    {
    case CLI_EXPR_NAME: {
        const char *const name = cli_py_text(one->text);
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "__file__", .cap = CLI_CELLS_POOL}))
        {
            cli_py_value_put(cli_py_text_put(
                g_cli_py.file, cellul.len(&(CatenaFinitaCfg){.src = g_cli_py.file, .cap = CLI_CELLS_POOL})));
            return cli_py_values_from(mark);
        }
        for (unsigned int at = 0u; at < g_cli_py.names; at += 1u)
        {
            if (cellul.eq(&(CatenaFinitaCfg){
                    .src = cli_py_text(g_cli_py.name[at].name), .other = name, .cap = CLI_CELLS_POOL}))
            {
                return g_cli_py.name[at].values;
            }
        }
        return cli_none();
    }
    case CLI_EXPR_TEXT:
        cli_py_value_put(one->text);
        return cli_py_values_from(mark);
    case CLI_EXPR_MANY:
        return cli_many(one);
    case CLI_EXPR_JOIN:
    case CLI_EXPR_ADD: {
        const CliValues left = cli_py_eval(one->left);
        const CliValues right = cli_py_eval(one->right);
        return cli_pairs(left, right, one->kind == CLI_EXPR_JOIN);
    }
    case CLI_EXPR_ATTR: {
        const char *const name = cli_py_text(one->text);
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "parent", .cap = CLI_CELLS_POOL}))
        {
            return cli_parents(cli_py_eval(one->left), 1ll);
        }
        return (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "path", .cap = CLI_CELLS_POOL}))
                   ? cli_py_eval(one->left)
                   : cli_none();
    }
    case CLI_EXPR_INDEX:
        return cli_indexed(one);
    case CLI_EXPR_CALL:
        return cli_called(one);
    default:
        return cli_none();
    }
}

// The names of a folder's folders as they are read
typedef struct
{
    char text[CLI_PY_FOLDER_TEXT];
    unsigned int used;
    const char *name[CLI_PY_FOLDERS];
    unsigned int count;
} CliPyFolders;

static CliPyFolders s_folders;

static void cli_folder_each(void *context, const char *name, int is_folder)
{
    (void)context;
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) + 1ull;
    if (!is_folder || (s_folders.count >= CLI_PY_FOLDERS) || ((s_folders.used + length) > CLI_PY_FOLDER_TEXT))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_folders.text + s_folders.used, .src = name, .bytes = length});
    s_folders.name[s_folders.count] = s_folders.text + s_folders.used;
    s_folders.count += 1u;
    // the text is below CLI_PY_FOLDER_TEXT bytes, which an unsigned int holds
    s_folders.used += (unsigned int)length;
}

CliValues cli_py_folders_in(const char *folder)
{
    static char s_path[CLI_PATH_LONGEST];
    s_folders.used = 0u;
    s_folders.count = 0u;
    const unsigned int mark = g_cli_py.values;
    if (!query_folder_each(folder, cli_folder_each, NULL))
    {
        return cli_none();
    }
    // the paths sort as their folder's names do, the folder before them the same
    for (unsigned int at = 1u; at < s_folders.count; at += 1u)
    {
        const char *const held = s_folders.name[at];
        unsigned int place = at;
        while ((place > 0u) && (cli_text_order(s_folders.name[place - 1u], held) > 0))
        {
            s_folders.name[place] = s_folders.name[place - 1u];
            place -= 1u;
        }
        s_folders.name[place] = held;
    }
    for (unsigned int at = 0u; at < s_folders.count; at += 1u)
    {
        CliLine line = {s_path, sizeof(s_path), 0ull};
        cli_joined(folder, s_folders.name[at], &line);
        cli_line_put(&line);
    }
    return cli_py_values_from(mark);
}

CliValues cli_py_upward(const char *path)
{
    static char s_at[CLI_PATH_LONGEST];
    static char s_up[CLI_PATH_LONGEST];
    const unsigned int mark = g_cli_py.values;
    memor.cpy(&(MemoriaCfg){
        .dst = s_at, .src = path, .bytes = cellul.len(&(CatenaFinitaCfg){.src = path, .cap = CLI_CELLS_POOL}) + 1u});
    cli_py_value_put(cli_py_text_put(s_at, cellul.len(&(CatenaFinitaCfg){.src = s_at, .cap = CLI_CELLS_POOL})));
    for (;;)
    {
        CliLine up = {s_up, sizeof(s_up), 0ull};
        cli_py_parent(s_at, &up);
        const unsigned long long length = up.at;
        cli_line_closed(&up);
        if ((length == 0ull) || (cellul.eq(&(CatenaFinitaCfg){.src = s_up, .other = s_at, .cap = CLI_CELLS_POOL})))
        {
            return cli_py_values_from(mark);
        }
        cli_py_value_put(cli_py_text_put(s_up, length));
        memor.cpy(&(MemoriaCfg){.dst = s_at,
                                .src = s_up,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_up, .cap = CLI_CELLS_POOL}) + 1u});
    }
}
