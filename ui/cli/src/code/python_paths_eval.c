// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// python_paths_eval.c: what an expression read from a script may be, as lists of paths: `__file__`, strings, the
// names set before it, the folder above, a join, and the calls of `os.path` and `pathlib` that give a path
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "python_paths_internal.h"

// the max folders one folder contains that a loop over its entries takes
#define CLI_PY_FOLDERS 4096u
#define CLI_PY_FOLDER_TEXT (1u << 20u)

#if defined(_WIN32)
#define CLI_PY_SEPARATOR '\\'
#else
#define CLI_PY_SEPARATOR '/'
#endif

void cli_py_value_append(const CliPythonPathsTextTypeCtx *args)
{
    if ((args->text_u32 != CLI_PY_NONE) && (g_cli_py.values < CLI_PY_VALUES))
    {
        g_cli_py.value[g_cli_py.values] = args->text_u32;
        g_cli_py.values += 1u;
    }
}

CliValues cli_py_values_from(const CliPythonPathsFirstCtx *args)
{
    return (CliValues){args->first, g_cli_py.values - args->first};
}

// no values
static CliValues cli_none(void)
{
    return (CliValues){g_cli_py.values, 0u};
}

typedef struct
{
    const VerbaLinea *line;
    char character;
    const char *part;
    const char *base;
    VerbaLinea *out;
    CliValues one;
    CliValues other;
    int join;
    CliValues values;
    long long steps;
    const CliExpr *call;
    unsigned int at;
    CliValues all;
    unsigned int from;
    const CliExpr *index;
    const CliExpr *many;
} PythonPathsEvalCtx;
// a text written into `line` put as a value
EMBED_INLINE void cli_line_append(const PythonPathsEvalCtx *args)
{
    if (mmgr_line_ok(args->line))
    {
        EMBED_CALL(cli_python_paths_text_type_internal.py_value_append, CliPythonPathsTextTypeCtx,
                   .text_u32 = EMBED_CALL(cli_python_paths_text_internal.py_text_append, CliPythonPathsTextCtx,
                                          .text = args->line->out, .length = args->line->at));
    }
}

// whether a byte parts a path
EMBED_INLINE int cli_parts(const PythonPathsEvalCtx *args)
{
    return (args->character == '/') || (args->character == '\\');
}

void cli_py_parent(const CliPythonPathsOutCtx *args)
{
    const unsigned long long last_end =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = args->path,
                   .at = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}), .set = "/\\");
    const char *const last = (last_end != 0ULL) ? (args->path + last_end - 1ULL) : NULL;
    if (last == NULL)
    {
        return;
    }
    // the slash lies in the path, at or past its start
    const unsigned long long at = (unsigned long long)(last - args->path);
    unsigned long long length;
    if (at == 0ULL)
    {
        length = 1ULL;
    }
    else if ((at == 2ULL) && (args->path[1] == ':'))
    {
        length = 3ULL;
    }
    else
    {
        length = at;
    }
    mmgr_line_put_n(args->out, args->path, length);
}

// whether a path is alone when it is joined onto another
EMBED_INLINE int cli_absolute(const PythonPathsEvalCtx *args)
{
#if defined(_WIN32)
    const int drive = (((args->part[0] | 0x20) >= 'a') && ((args->part[0] | 0x20) <= 'z')) && (args->part[1] == ':') &&
                      EMBED_CALL(cli_parts, PythonPathsEvalCtx, .character = args->part[2]);
    return drive || ((args->part[0] == '\\') && (args->part[1] == '\\'));
#else
    return args->part[0] == '/';
#endif
}

// `part` joined onto `base` as `os.path.join` joins them, written into `out`
EMBED_INLINE void cli_join(const PythonPathsEvalCtx *args)
{
    if (cli_absolute(args) || (args->base[0] == '\0'))
    {
        mmgr_line_put(args->out, args->part);
        return;
    }
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->base, .cap = CLI_POOL_BYTES});
    mmgr_line_put(args->out, args->base);
    if (!EMBED_CALL(cli_parts, PythonPathsEvalCtx, .character = args->base[length - 1ULL]))
    {
        mmgr_line_ch(args->out, CLI_PY_SEPARATOR);
    }
    mmgr_line_put(args->out, args->part);
}

// every pairing of the values of `one` and `other`, joined, or added where `join` is 0, at max CLI_PY_PAIRS of them
EMBED_INLINE CliValues cli_pairs(const PythonPathsEvalCtx *args)
{
    static char s_created[CLI_PATH_MAX_LEN * 2u];
    const unsigned int mark = g_cli_py.values;
    unsigned int made = 0u;
    for (unsigned int left = 0u; (left < args->one.count) && (made < CLI_PY_PAIRS); left += 1u)
    {
        for (unsigned int right = 0u; (right < args->other.count) && (made < CLI_PY_PAIRS); right += 1u)
        {
            const char *const base = EMBED_CALL(cli_python_paths_at_internal.py_text, CliPythonPathsAtCtx,
                                                .at = g_cli_py.value[args->one.first + left]);
            const char *const part = EMBED_CALL(cli_python_paths_at_internal.py_text, CliPythonPathsAtCtx,
                                                .at = g_cli_py.value[args->other.first + right]);
            VerbaLinea line = {s_created, sizeof(s_created), 0ULL};
            if (args->join)
            {
                EMBED_CALL(cli_join, PythonPathsEvalCtx, .base = base, .part = part, .out = &line);
            }
            else
            {
                mmgr_line_put(&line, base);
                mmgr_line_put(&line, part);
            }
            EMBED_CALL(cli_line_append, PythonPathsEvalCtx, .line = &line);
            made += 1u;
        }
    }
    return EMBED_CALL(cli_python_paths_first_internal.py_values_from, CliPythonPathsFirstCtx, .first = mark);
}

// each value's folder above, `steps` times over
EMBED_INLINE CliValues cli_parents(const PythonPathsEvalCtx *args)
{
    static char s_at[CLI_PATH_MAX_LEN];
    static char s_up[CLI_PATH_MAX_LEN];
    const unsigned int mark = g_cli_py.values;
    for (unsigned int index = 0u; index < args->values.count; index += 1u)
    {
        memor.cpy(
            &(MemoriaCfg){.dst = s_at,
                          .src = EMBED_CALL(cli_python_paths_at_internal.py_text, CliPythonPathsAtCtx,
                                            .at = g_cli_py.value[args->values.first + index]),
                          .bytes = cellul.len(&(CatenaFinitaCfg){
                                       .src = EMBED_CALL(cli_python_paths_at_internal.py_text, CliPythonPathsAtCtx,
                                                         .at = g_cli_py.value[args->values.first + index]),
                                       .cap = CLI_POOL_BYTES}) +
                                   1u});
        for (long long step = 0LL; step < args->steps; step += 1LL)
        {
            VerbaLinea up = {s_up, sizeof(s_up), 0ULL};
            EMBED_CALL(cli_python_paths_out_internal.py_parent, CliPythonPathsOutCtx, .path = s_at, .out = &up);
            const unsigned long long length = up.at;
            mmgr_line_finish(&up);
            memor.cpy(&(MemoriaCfg){
                .dst = s_at,
                .src = (length == 0ULL) ? "" : s_up,
                .bytes =
                    cellul.len(&(CatenaFinitaCfg){.src = (length == 0ULL) ? "" : s_up, .cap = CLI_POOL_BYTES}) + 1u});
        }
        EMBED_CALL(cli_python_paths_text_type_internal.py_value_append, CliPythonPathsTextTypeCtx,
                   .text_u32 =
                       EMBED_CALL(cli_python_paths_text_internal.py_text_append, CliPythonPathsTextCtx, .text = s_at,
                                  .length = cellul.len(&(CatenaFinitaCfg){.src = s_at, .cap = CLI_POOL_BYTES})));
    }
    return EMBED_CALL(cli_python_paths_first_internal.py_values_from, CliPythonPathsFirstCtx, .first = mark);
}

int cli_py_dot(const CliPythonPathsExprCtx *args)
{
    const CliExpr *const one = &g_cli_py.expr[args->expr];
    if (one->type == CLI_EXPR_NAME)
    {
        mmgr_line_put(args->out,
                      EMBED_CALL(cli_python_paths_at_internal.py_text, CliPythonPathsAtCtx, .at = one->text));
        return 1;
    }
    if ((one->type == CLI_EXPR_ATTR) &&
        EMBED_CALL(cli_python_paths_expr_internal.py_dot, CliPythonPathsExprCtx, .expr = one->left, .out = args->out))
    {
        mmgr_line_ch(args->out, '.');
        mmgr_line_put(args->out,
                      EMBED_CALL(cli_python_paths_at_internal.py_text, CliPythonPathsAtCtx, .at = one->text));
        return 1;
    }
    return 0;
}

// what the argument `at` of a call may be, none where it has no such argument
EMBED_INLINE CliValues cli_argument(const PythonPathsEvalCtx *args)
{
    return (args->at < args->call->count) ? EMBED_CALL(cli_python_paths_expr_internal.py_eval, CliPythonPathsExprCtx,
                                                       .expr = g_cli_py.item[args->call->first + args->at])
                                          : cli_none();
}

// a list of joined paths: `all` joined with each of the call's arguments from `from` on
EMBED_INLINE CliValues cli_joined_all(const PythonPathsEvalCtx *args)
{
    CliValues all = args->all;
    for (unsigned int at = args->from; at < args->call->count; at += 1u)
    {
        all =
            EMBED_CALL(cli_pairs, PythonPathsEvalCtx, .one = all,
                       .other = EMBED_CALL(cli_argument, PythonPathsEvalCtx, .call = args->call, .at = at), .join = 1);
    }
    return all;
}

// what a call may be: the calls of `os.path` and `pathlib` that give a path
EMBED_INLINE CliValues cli_call(const PythonPathsEvalCtx *args)
{
    char dotted[CLI_NAME_MAX_LEN];
    VerbaLinea line = {dotted, sizeof(dotted), 0ULL};
    const int named = EMBED_CALL(cli_python_paths_expr_internal.py_dot, CliPythonPathsExprCtx, .expr = args->call->left,
                                 .out = &line);
    mmgr_line_finish(&line);
    const char *const name = named ? dotted : "";
    if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "os.path.dirname", .cap = CLI_POOL_BYTES}))
    {
        return EMBED_CALL(cli_parents, PythonPathsEvalCtx,
                          .values = EMBED_CALL(cli_argument, PythonPathsEvalCtx, .call = args->call, .at = 0u),
                          .steps = 1LL);
    }
    static const char *const s_same[5] = {"os.path.abspath", "os.path.realpath", "os.path.normpath", "str",
                                          "os.fspath"};
    static const char *const s_join[5] = {"os.path.join", "Path", "pathlib.Path", "PurePath", "pathlib.PurePath"};
    for (unsigned int at = 0u; at < 5u; at += 1u)
    {
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_same[at], .cap = CLI_POOL_BYTES}))
        {
            return EMBED_CALL(cli_argument, PythonPathsEvalCtx, .call = args->call, .at = 0u);
        }
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = s_join[at], .cap = CLI_POOL_BYTES}))
        {
            return EMBED_CALL(cli_joined_all, PythonPathsEvalCtx,
                              .all = EMBED_CALL(cli_argument, PythonPathsEvalCtx, .call = args->call, .at = 0u),
                              .call = args->call, .from = 1u);
        }
    }
    const CliExpr *const callee = &g_cli_py.expr[args->call->left];
    if (callee->type != CLI_EXPR_ATTR)
    {
        return cli_none();
    }
    const char *const method =
        EMBED_CALL(cli_python_paths_at_internal.py_text, CliPythonPathsAtCtx, .at = callee->text);
    if ((cellul.eq(&(CatenaFinitaCfg){.src = method, .other = "resolve", .cap = CLI_POOL_BYTES})) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = method, .other = "absolute", .cap = CLI_POOL_BYTES})))
    {
        return EMBED_CALL(cli_python_paths_expr_internal.py_eval, CliPythonPathsExprCtx, .expr = callee->left);
    }
    if (cellul.eq(&(CatenaFinitaCfg){.src = method, .other = "joinpath", .cap = CLI_POOL_BYTES}))
    {
        return EMBED_CALL(
            cli_joined_all, PythonPathsEvalCtx,
            .all = EMBED_CALL(cli_python_paths_expr_internal.py_eval, CliPythonPathsExprCtx, .expr = callee->left),
            .call = args->call, .from = 0u);
    }
    return cli_none();
}

// what an index may be: `parents[n]` of a path, and `rsplit(sep, ...)[0]` of a string
EMBED_INLINE CliValues cli_index(const PythonPathsEvalCtx *args)
{
    const CliExpr *const of = &g_cli_py.expr[args->index->left];
    const CliExpr *const at = &g_cli_py.expr[args->index->right];
    if (at->type != CLI_EXPR_NUMBER)
    {
        return cli_none();
    }
    if ((of->type == CLI_EXPR_ATTR) &&
        (cellul.eq(&(CatenaFinitaCfg){
            .src = EMBED_CALL(cli_python_paths_at_internal.py_text, CliPythonPathsAtCtx, .at = of->text),
            .other = "parents",
            .cap = CLI_POOL_BYTES})))
    {
        return EMBED_CALL(
            cli_parents, PythonPathsEvalCtx,
            .values = EMBED_CALL(cli_python_paths_expr_internal.py_eval, CliPythonPathsExprCtx, .expr = of->left),
            .steps = at->number + 1LL);
    }
    if ((of->type != CLI_EXPR_CALL) || (at->number != 0LL) || (of->count == 0u))
    {
        return cli_none();
    }
    const CliExpr *const callee = &g_cli_py.expr[of->left];
    const CliExpr *const sep = &g_cli_py.expr[g_cli_py.item[of->first]];
    if ((callee->type != CLI_EXPR_ATTR) || (sep->type != CLI_EXPR_TEXT) ||
        (!cellul.eq(&(CatenaFinitaCfg){
            .src = EMBED_CALL(cli_python_paths_at_internal.py_text, CliPythonPathsAtCtx, .at = callee->text),
            .other = "rsplit",
            .cap = CLI_POOL_BYTES})))
    {
        return cli_none();
    }
    const CliValues values =
        EMBED_CALL(cli_python_paths_expr_internal.py_eval, CliPythonPathsExprCtx, .expr = callee->left);
    const char *const parting = EMBED_CALL(cli_python_paths_at_internal.py_text, CliPythonPathsAtCtx, .at = sep->text);
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = parting, .cap = CLI_POOL_BYTES});
    const unsigned int mark = g_cli_py.values;
    for (unsigned int one = 0u; one < values.count; one += 1u)
    {
        const char *const text = EMBED_CALL(cli_python_paths_at_internal.py_text, CliPythonPathsAtCtx,
                                            .at = g_cli_py.value[values.first + one]);
        const char *last = NULL;
        for (const char *found =
                 (length != 0ULL)
                     ? cellul.find(&(CatenaFinitaCfg){
                           .src = text, .cap = CLI_POOL_BYTES, .other = parting, .other_cap = CLI_POOL_BYTES})
                     : NULL;
             found != NULL;
             found = cellul.find(&(CatenaFinitaCfg){
                 .src = found + 1, .cap = CLI_POOL_BYTES, .other = parting, .other_cap = CLI_POOL_BYTES}))
        {
            last = found;
        }
        // the parting found lies in the text, at or past its start
        EMBED_CALL(cli_python_paths_text_type_internal.py_value_append, CliPythonPathsTextTypeCtx,
                   .text_u32 = (last != NULL)
                                   ? EMBED_CALL(cli_python_paths_text_internal.py_text_append, CliPythonPathsTextCtx,
                                                .text = text, .length = (unsigned long long)(last - text))
                                   : g_cli_py.value[values.first + one]);
    }
    return EMBED_CALL(cli_python_paths_first_internal.py_values_from, CliPythonPathsFirstCtx, .first = mark);
}

// the lists of a tuple or list put one after another
EMBED_INLINE CliValues cli_many(const PythonPathsEvalCtx *args)
{
    const unsigned int base = g_cli_py.ranged;
    for (unsigned int at = 0u; at < args->many->count; at += 1u)
    {
        const CliValues one = EMBED_CALL(cli_python_paths_expr_internal.py_eval, CliPythonPathsExprCtx,
                                         .expr = g_cli_py.item[args->many->first + at]);
        if (g_cli_py.ranged < g_cli_py.items_max)
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
            EMBED_CALL(cli_python_paths_text_type_internal.py_value_append, CliPythonPathsTextTypeCtx,
                       .text_u32 = g_cli_py.value[one.first + place]);
        }
    }
    g_cli_py.ranged = base;
    return EMBED_CALL(cli_python_paths_first_internal.py_values_from, CliPythonPathsFirstCtx, .first = mark);
}

CliValues cli_py_eval(const CliPythonPathsExprCtx *args)
{
    const CliExpr *const one = &g_cli_py.expr[args->expr];
    const unsigned int mark = g_cli_py.values;
    switch (one->type)
    {
    case CLI_EXPR_NAME: {
        const char *const name = EMBED_CALL(cli_python_paths_at_internal.py_text, CliPythonPathsAtCtx, .at = one->text);
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "__file__", .cap = CLI_POOL_BYTES}))
        {
            EMBED_CALL(cli_python_paths_text_type_internal.py_value_append, CliPythonPathsTextTypeCtx,
                       .text_u32 = EMBED_CALL(
                           cli_python_paths_text_internal.py_text_append, CliPythonPathsTextCtx, .text = g_cli_py.file,
                           .length = cellul.len(&(CatenaFinitaCfg){.src = g_cli_py.file, .cap = CLI_POOL_BYTES})));
            return EMBED_CALL(cli_python_paths_first_internal.py_values_from, CliPythonPathsFirstCtx, .first = mark);
        }
        for (unsigned int at = 0u; at < g_cli_py.names; at += 1u)
        {
            if (cellul.eq(&(CatenaFinitaCfg){.src = EMBED_CALL(cli_python_paths_at_internal.py_text,
                                                               CliPythonPathsAtCtx, .at = g_cli_py.name[at].name),
                                             .other = name,
                                             .cap = CLI_POOL_BYTES}))
            {
                return g_cli_py.name[at].values;
            }
        }
        return cli_none();
    }
    case CLI_EXPR_TEXT:
        EMBED_CALL(cli_python_paths_text_type_internal.py_value_append, CliPythonPathsTextTypeCtx,
                   .text_u32 = one->text);
        return EMBED_CALL(cli_python_paths_first_internal.py_values_from, CliPythonPathsFirstCtx, .first = mark);
    case CLI_EXPR_MANY:
        return EMBED_CALL(cli_many, PythonPathsEvalCtx, .many = one);
    case CLI_EXPR_JOIN:
    case CLI_EXPR_ADD: {
        const CliValues left =
            EMBED_CALL(cli_python_paths_expr_internal.py_eval, CliPythonPathsExprCtx, .expr = one->left);
        const CliValues right =
            EMBED_CALL(cli_python_paths_expr_internal.py_eval, CliPythonPathsExprCtx, .expr = one->right);
        return EMBED_CALL(cli_pairs, PythonPathsEvalCtx, .one = left, .other = right,
                          .join = one->type == CLI_EXPR_JOIN);
    }
    case CLI_EXPR_ATTR: {
        const char *const name = EMBED_CALL(cli_python_paths_at_internal.py_text, CliPythonPathsAtCtx, .at = one->text);
        if (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "parent", .cap = CLI_POOL_BYTES}))
        {
            return EMBED_CALL(
                cli_parents, PythonPathsEvalCtx,
                .values = EMBED_CALL(cli_python_paths_expr_internal.py_eval, CliPythonPathsExprCtx, .expr = one->left),
                .steps = 1LL);
        }
        return (cellul.eq(&(CatenaFinitaCfg){.src = name, .other = "path", .cap = CLI_POOL_BYTES}))
                   ? EMBED_CALL(cli_python_paths_expr_internal.py_eval, CliPythonPathsExprCtx, .expr = one->left)
                   : cli_none();
    }
    case CLI_EXPR_INDEX:
        return EMBED_CALL(cli_index, PythonPathsEvalCtx, .index = one);
    case CLI_EXPR_CALL:
        return EMBED_CALL(cli_call, PythonPathsEvalCtx, .call = one);
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

static void cli_folder_each(const CliEachNameIsFolderCfg *args)
{
    const unsigned long long length = cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_POOL_BYTES}) + 1ULL;
    if (!args->is_folder || (s_folders.count >= CLI_PY_FOLDERS) || ((s_folders.used + length) > CLI_PY_FOLDER_TEXT))
    {
        return;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_folders.text + s_folders.used, .src = args->name, .bytes = length});
    s_folders.name[s_folders.count] = s_folders.text + s_folders.used;
    s_folders.count += 1u;
    // the text is below CLI_PY_FOLDER_TEXT bytes, which an unsigned int stores
    s_folders.used += (unsigned int)length;
}

CliValues cli_py_folders_in(const CliPythonPathsCtx *args)
{
    static char s_path[CLI_PATH_MAX_LEN];
    s_folders.used = 0u;
    s_folders.count = 0u;
    const unsigned int mark = g_cli_py.values;
    if (!EMBED_CALL(query_folder_each_internal.folder_each, QueryFolderEachCtx, .folder = args->folder,
                    .each = cli_folder_each, .context_bytess = NULL))
    {
        return cli_none();
    }
    // the paths sort as their folder's names do, the folder before them the same
    for (unsigned int at = 1u; at < s_folders.count; at += 1u)
    {
        const char *const pivot = s_folders.name[at];
        unsigned int place = at;
        while ((place > 0u) && (EMBED_CALL(cli_text_size_one.order, CliTextSizeOneCfg,
                                           .one = s_folders.name[place - 1u], .other_pointer = pivot) > 0))
        {
            s_folders.name[place] = s_folders.name[place - 1u];
            place -= 1u;
        }
        s_folders.name[place] = pivot;
    }
    for (unsigned int at = 0u; at < s_folders.count; at += 1u)
    {
        VerbaLinea line = {s_path, sizeof(s_path), 0ULL};
        EMBED_CALL(cli_join, PythonPathsEvalCtx, .base = args->folder, .part = s_folders.name[at], .out = &line);
        EMBED_CALL(cli_line_append, PythonPathsEvalCtx, .line = &line);
    }
    return EMBED_CALL(cli_python_paths_first_internal.py_values_from, CliPythonPathsFirstCtx, .first = mark);
}

CliValues cli_py_upward(const CliPythonPathsOutCtx *args)
{
    static char s_at[CLI_PATH_MAX_LEN];
    static char s_up[CLI_PATH_MAX_LEN];
    const unsigned int mark = g_cli_py.values;
    memor.cpy(&(MemoriaCfg){.dst = s_at,
                            .src = args->path,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = args->path, .cap = CLI_POOL_BYTES}) + 1u});
    EMBED_CALL(cli_python_paths_text_type_internal.py_value_append, CliPythonPathsTextTypeCtx,
               .text_u32 =
                   EMBED_CALL(cli_python_paths_text_internal.py_text_append, CliPythonPathsTextCtx, .text = s_at,
                              .length = cellul.len(&(CatenaFinitaCfg){.src = s_at, .cap = CLI_POOL_BYTES})));
    for (;;)
    {
        VerbaLinea up = {s_up, sizeof(s_up), 0ULL};
        EMBED_CALL(cli_python_paths_out_internal.py_parent, CliPythonPathsOutCtx, .path = s_at, .out = &up);
        const unsigned long long length = up.at;
        mmgr_line_finish(&up);
        if ((length == 0ULL) || (cellul.eq(&(CatenaFinitaCfg){.src = s_up, .other = s_at, .cap = CLI_POOL_BYTES})))
        {
            return EMBED_CALL(cli_python_paths_first_internal.py_values_from, CliPythonPathsFirstCtx, .first = mark);
        }
        EMBED_CALL(cli_python_paths_text_type_internal.py_value_append, CliPythonPathsTextTypeCtx,
                   .text_u32 = EMBED_CALL(cli_python_paths_text_internal.py_text_append, CliPythonPathsTextCtx,
                                          .text = s_up, .length = length));
        memor.cpy(&(MemoriaCfg){.dst = s_at,
                                .src = s_up,
                                .bytes = cellul.len(&(CatenaFinitaCfg){.src = s_up, .cap = CLI_POOL_BYTES}) + 1u});
    }
}
