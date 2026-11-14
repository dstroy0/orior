// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_lists.c: the commits of other branches not applied here, the commits that touched a file, a file's text as a
// commit left it, who made a commit, and the files a commit changed
#include "../command_line/callback_cfg.h"
#include "../text/text.h"
#include "git_internal.h"

// the commits a list requests from git, as git is given the count
#define CLI_GIT_LIMIT "-n200"

typedef struct
{
    const char *out;
    unsigned long long size;
    CliCommits *found;
    void (*each)(const CliIdWhenCreatedCfg *args);
    const char *folder;
    const char *what;
    const char *displayed;
    VerbaLinea *out_verba_linea_pointer;
} GitListsCtx;
// the commit lines `id\x1fwhen\x1fsubject` of what git wrote put after those found
EMBED_INLINE void cli_commits_get(const GitListsCtx *args)
{
    unsigned long long size = args->size;
    const char *line = args->out;
    const char *const end = args->out + size;
    while ((line < end) && (args->found->count < CLI_GIT_COMMITS))
    {
        const char *stop = line;
        stop += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = stop, .at = 0ULL,
                           .size = (unsigned long long)(end - stop), .set = "\n");
        const char *const next = stop + 1;
        stop -= ((stop > line) && (stop[-1] == '\r')) ? 1 : 0;
        const char *at = line;
        unsigned long long id_length = 0ULL;
        unsigned long long when_length = 0ULL;
        const char *const id = EMBED_CALL(cli_git_at_internal.field, CliGitAtCtx, .at = &at, .end = stop,
                                          .stop = '\x1f', .length_pointer = &id_length);
        const char *const when = EMBED_CALL(cli_git_at_internal.field, CliGitAtCtx, .at = &at, .end = stop,
                                            .stop = '\x1f', .length_pointer = &when_length);
        const char *const subject = (at <= stop) ? at : NULL;
        line = next;
        long long seconds = 0LL;
        if ((when == NULL) || !EMBED_CALL(cli_git_length_text_internal.seconds, CliGitLengthTextCtx, .text = when,
                                          .length = when_length, .value = &seconds))
        {
            continue;
        }
        CliCommit *const one = &args->found->item[args->found->count];
        args->found->count += 1u;
        one->id = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found->text, .from = id,
                             .length = id_length);
        one->when = seconds;
        one->subject = (subject != NULL)
                           ? EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found->text,
                                        .from = subject, .length = (unsigned long long)(stop - subject))
                           : 0u;
    }
}

// What makes a commit the same as another under another id, its author, the time the author made it and its message,
// for each commit found elsewhere, and whether one of the branch's own is the same, guarded by the lock
typedef struct
{
    CliGitText made;
    unsigned int at[CLI_GIT_COMMITS];
    unsigned long long length[CLI_GIT_COMMITS];
    int ours[CLI_GIT_COMMITS];
    CliCommits found;
} CliElsewhere;

static CliElsewhere s_elsewhere;

// the records `id\x1fwhen\x1fauthor\x1fat\x1fmessage\x1e` of what git wrote handed to `each` with the commit's id, its
// time, and its author, the time the author made it and its message joined by `\x1f`, the message's end trimmed
EMBED_INLINE void cli_records(const GitListsCtx *args)
{
    unsigned long long size = args->size;
    const char *record = args->out;
    const char *const end = args->out + size;
    while (record < end)
    {
        const char *stop = record;
        while ((stop < end) && (stop[0] != '\x1e'))
        {
            stop += 1;
        }
        const char *const next = stop + 1;
        record += EMBED_CALL(cli_text_at_size_classes.end_any, CliTextAtSizeClassesCfg, .text = record, .at = 0ULL,
                             .size = (unsigned long long)(stop - record), .set = "\n");
        const char *at = record;
        unsigned long long lengths[4] = {0ULL, 0ULL, 0ULL, 0ULL};
        const char *fields[4] = {NULL, NULL, NULL, NULL};
        for (unsigned int field = 0u; field < 4u; field += 1u)
        {
            fields[field] = EMBED_CALL(cli_git_at_internal.field, CliGitAtCtx, .at = &at, .end = stop, .stop = '\x1f',
                                       .length_pointer = &lengths[field]);
        }
        const char *const message = (at <= stop) ? at : NULL;
        long long when = 0LL;
        if ((fields[3] != NULL) && (message != NULL) &&
            EMBED_CALL(cli_git_length_text_internal.seconds, CliGitLengthTextCtx, .text = fields[1],
                       .length = lengths[1], .value = &when))
        {
            const char *message_end = stop;
            message_end =
                message + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = message,
                                     .at = (unsigned long long)(message_end - message), .set = CLI_TEXT_WHITE);
            EMBED_CALL(args->each, CliIdWhenCreatedCfg, .id = fields[0], .id_length = lengths[0], .when = when,
                       .made = fields[2], .created_length = (unsigned long long)(message_end - fields[2]));
        }
        record = next;
    }
}

// a commit found elsewhere stored, its subject the first line of its message
static void cli_elsewhere_each(const CliIdWhenCreatedCfg *args)
{
    const char *id = args->id;
    long long when = args->when;
    CliCommits *const found = &s_elsewhere.found;
    if (found->count >= CLI_GIT_COMMITS)
    {
        return;
    }
    const char *message = args->made;
    for (unsigned int seen = 0u; seen < 2u; seen += 1u)
    {
        while ((message < (args->made + args->created_length)) && (message[0] != '\x1f'))
        {
            message += 1;
        }
        message += (message < (args->made + args->created_length)) ? 1 : 0;
    }
    const char *line_end = message;
    line_end += EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = line_end, .at = 0ULL,
                           .size = (unsigned long long)((args->made + args->created_length) - line_end), .set = "\n");
    line_end -= ((line_end > message) && (line_end[-1] == '\r')) ? 1 : 0;
    CliCommit *const one = &found->item[found->count];
    one->id = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &found->text, .from = id,
                         .length = args->id_length);
    one->when = when;
    one->subject = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &found->text, .from = message,
                              .length = (unsigned long long)(line_end - message));
    s_elsewhere.at[found->count] = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &s_elsewhere.made,
                                              .from = args->made, .length = args->created_length);
    s_elsewhere.length[found->count] = args->created_length;
    s_elsewhere.ours[found->count] = 0;
    found->count += 1u;
}

// a commit of the branch's own marks each found elsewhere that is the same
static void cli_ours_each(const CliIdWhenCreatedCfg *args)
{
    for (unsigned int at = 0u; at < s_elsewhere.found.count; at += 1u)
    {
        if ((s_elsewhere.length[at] == args->created_length) &&
            (memor.cmp(&(MemoriaCfg){.src = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                                       .text_read = &s_elsewhere.made, .at = s_elsewhere.at[at]),
                                     .other = args->made,
                                     .bytes = args->created_length}) == 0))
        {
            s_elsewhere.ours[at] = 1;
        }
    }
}

void cli_git_elsewhere(const CliGitRootFileErrMsgCfg *args)
{
    static const char *const s_format = "--format=%H%x1f%ct%x1f%ae%x1f%at%x1f%B%x1e";
    args->found->count = 0u;
    EMBED_CALL(cli_git_text.text_begin, CliGitTextCfg, .text = &args->found->text);
    s_elsewhere.found.count = 0u;
    EMBED_CALL(cli_git_text.text_begin, CliGitTextCfg, .text = &s_elsewhere.found.text);
    EMBED_CALL(cli_git_text.text_begin, CliGitTextCfg, .text = &s_elsewhere.made);
    const char *const theirs[8] = {"log",  "--branches",  "--remotes",   "--not",
                                   "HEAD", "--no-merges", CLI_GIT_LIMIT, s_format};
    CliGitResult *const result = cli_git_alloc();
    if (EMBED_CALL(cli_git_result_internal.query, CliGitResultCtx, .root = args->root, .request = theirs, .count = 8u,
                   .result = result))
    {
        EMBED_CALL(cli_records, GitListsCtx, .out = result->out, .size = result->out_size, .each = cli_elsewhere_each);
    }
    long long oldest = 0LL;
    for (unsigned int at = 0u; at < s_elsewhere.found.count; at += 1u)
    {
        oldest = ((at == 0u) || (s_elsewhere.found.item[at].when < oldest)) ? s_elsewhere.found.item[at].when : oldest;
    }
    char since[64];
    VerbaLinea line = {since, sizeof(since), 0ULL};
    mmgr_line_put(&line, "--since=@");
    if (oldest < 0LL)
    {
        mmgr_line_ch(&line, '-');
    }
    // the oldest time's size, its sign written apart
    mmgr_line_decimal(&line, (unsigned long long)((oldest < 0LL) ? -oldest : oldest), 1u);
    mmgr_line_finish(&line);
    const char *const ours[5] = {"log", "HEAD", "--no-merges", since, s_format};
    // where the branch's own commits cannot be read, no commit is reported to be elsewhere: each would be
    const int read = (s_elsewhere.found.count != 0u) &&
                     EMBED_CALL(cli_git_result_internal.query, CliGitResultCtx, .root = args->root, .request = ours,
                                .count = 5u, .result = result);
    if (read)
    {
        EMBED_CALL(cli_records, GitListsCtx, .out = result->out, .size = result->out_size, .each = cli_ours_each);
    }
    EMBED_CALL(cli_git_result_internal.free, CliGitResultCtx, .result = result);
    for (unsigned int at = 0u; read && (at < s_elsewhere.found.count); at += 1u)
    {
        const CliCommit *const one = &s_elsewhere.found.item[at];
        if (s_elsewhere.ours[at])
        {
            continue;
        }
        const char *const id = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                          .text_read = &s_elsewhere.found.text, .at = one->id);
        const char *const subject = EMBED_CALL(cli_git_text_at_table.text_get, CliGitTextAtCfg,
                                               .text_read = &s_elsewhere.found.text, .at = one->subject);
        CliCommit *const commit = &args->found->item[args->found->count];
        args->found->count += 1u;
        commit->id = EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found->text, .from = id,
                                .length = cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_POOL_BYTES}));
        commit->when = one->when;
        commit->subject =
            EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found->text, .from = subject,
                       .length = cellul.len(&(CatenaFinitaCfg){.src = subject, .cap = CLI_POOL_BYTES}));
    }
    EMBED_CALL(cli_git_text.text_release, CliGitTextCfg, .text = &s_elsewhere.found.text);
    EMBED_CALL(cli_git_text.text_release, CliGitTextCfg, .text = &s_elsewhere.made);
}

int cli_git_commits(const CliGitRootFileErrMsgCfg *args)
{
    args->found->count = 0u;
    EMBED_CALL(cli_git_text.text_begin, CliGitTextCfg, .text = &args->found->text);
    char folder[CLI_PATH_MAX_LEN];
    char displayed[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_git_root_err_msg_internal.adjacent, CliGitRootErrMsgCtx, .root = args->root, .file = args->file,
                    .folder = folder, .displayed = displayed, .err_msg = args->err_msg))
    {
        return 0;
    }
    const char *const arguments[6] = {"log", "--follow", CLI_GIT_LIMIT, "--format=%H%x1f%ct%x1f%s", "--", displayed};
    CliGitResult *const result = cli_git_alloc();
    if (EMBED_CALL(cli_git_result_internal.query, CliGitResultCtx, .root = folder, .request = arguments, .count = 6u,
                   .result = result))
    {
        EMBED_CALL(cli_commits_get, GitListsCtx, .out = result->out, .size = result->out_size, .found = args->found);
    }
    EMBED_CALL(cli_git_result_internal.free, CliGitResultCtx, .result = result);
    return 1;
}

// git run to show `what` of the file at `displayed` from `folder`, `what` before the colon: what it wrote into `out`,
// 1, or 0 where it could not
EMBED_INLINE int cli_show(const GitListsCtx *args)
{
    char named[CLI_PATH_MAX_LEN + CLI_NAME_MAX_LEN];
    VerbaLinea line = {named, sizeof(named), 0ULL};
    mmgr_line_put(&line, args->what);
    mmgr_line_ch(&line, ':');
    mmgr_line_put(&line, args->displayed);
    if (mmgr_line_finish(&line) == 0ULL)
    {
        return 0;
    }
    const char *const arguments[2] = {"show", named};
    CliGitResult *const result = cli_git_alloc();
    const int found = EMBED_CALL(cli_git_result_internal.query, CliGitResultCtx, .root = args->folder,
                                 .request = arguments, .count = 2u, .result = result);
    if (found)
    {
        mmgr_line_put_n(args->out_verba_linea_pointer, result->out, result->out_size);
    }
    EMBED_CALL(cli_git_result_internal.free, CliGitResultCtx, .result = result);
    return found;
}

int cli_git_text_at(const CliGitRootFileCfg *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char displayed[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_git_root_err_msg_internal.adjacent, CliGitRootErrMsgCtx, .root = args->root, .file = args->file,
                    .folder = folder, .displayed = displayed, .err_msg = args->err_msg))
    {
        return 0;
    }
    if (!EMBED_CALL(cli_git_internal.is_id, CliGitCtx, .id = args->id))
    {
        mmgr_line_put(args->err_msg, args->id);
        mmgr_line_put(args->err_msg, " is not a commit");
        return 0;
    }
    if (!EMBED_CALL(cli_show, GitListsCtx, .folder = folder, .what = args->id, .displayed = displayed,
                    .out_verba_linea_pointer = args->out))
    {
        mmgr_line_put(args->err_msg, args->file);
        mmgr_line_put(args->err_msg, " is not in ");
        mmgr_line_put(args->err_msg, args->id);
        return 0;
    }
    return 1;
}

int cli_git_text_before(const CliGitRootFileCfg *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char displayed[CLI_PATH_MAX_LEN];
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    char parent[CLI_NAME_MAX_LEN];
    VerbaLinea line = {parent, sizeof(parent), 0ULL};
    mmgr_line_put(&line, args->id);
    mmgr_line_put(&line, "^1");
    return EMBED_CALL(cli_git_internal.is_id, CliGitCtx, .id = args->id) && (mmgr_line_finish(&line) != 0ULL) &&
           EMBED_CALL(cli_git_root_err_msg_internal.adjacent, CliGitRootErrMsgCtx, .root = args->root,
                      .file = args->file, .folder = folder, .displayed = displayed, .err_msg = &err_msg) &&
           EMBED_CALL(cli_show, GitListsCtx, .folder = folder, .what = parent, .displayed = displayed,
                      .out_verba_linea_pointer = args->out);
}

int cli_git_head_text(const CliGitRootFileStageCfg *args)
{
    char folder[CLI_PATH_MAX_LEN];
    char displayed[CLI_PATH_MAX_LEN];
    char why[CLI_NAME_MAX_LEN];
    VerbaLinea err_msg = {why, sizeof(why), 0ULL};
    return EMBED_CALL(cli_git_root_err_msg_internal.adjacent, CliGitRootErrMsgCtx, .root = args->root,
                      .file = args->file, .folder = folder, .displayed = displayed, .err_msg = &err_msg) &&
           EMBED_CALL(cli_show, GitListsCtx, .folder = folder, .what = "HEAD", .displayed = displayed,
                      .out_verba_linea_pointer = args->out);
}

int cli_git_new_by(const CliGitRootIdCfg *args)
{
    if (!EMBED_CALL(cli_git_internal.is_id, CliGitCtx, .id = args->id))
    {
        return 0;
    }
    const char *const arguments[4] = {"log", "-1", "--format=%an%x00%s", args->id};
    CliGitResult *const result = cli_git_alloc();
    int found = EMBED_CALL(cli_git_result_internal.query, CliGitResultCtx, .root = args->root, .request = arguments,
                           .count = 4u, .result = result);
    const char *from = result->out;
    const char *to = result->out + result->out_size;
    to = from + EMBED_CALL(cli_text_at_size_classes.back_end, CliTextAtSizeClassesCfg, .text = from,
                           .at = (unsigned long long)(to - from), .set = CLI_TEXT_WHITE);
    const char *const nul = memor.chr(&(MemoriaCfg){.src = from, .val = 0, .bytes = (unsigned long long)(to - from)});
    const char *const zero = (nul != NULL) ? nul : to;
    found = found && (zero < to);
    if (found)
    {
        mmgr_line_put_n(args->name_verba_linea_pointer, from, (unsigned long long)(zero - from));
        mmgr_line_put_n(args->subject, zero + 1, (unsigned long long)(to - zero - 1));
    }
    EMBED_CALL(cli_git_result_internal.free, CliGitResultCtx, .result = result);
    return found;
}

int cli_git_touch(const CliGitRootErrMsgCfg *args)
{
    args->found_touched_list_pointer->count = 0u;
    EMBED_CALL(cli_git_text.text_begin, CliGitTextCfg, .text = &args->found_touched_list_pointer->text);
    if (!EMBED_CALL(cli_git_internal.is_id, CliGitCtx, .id = args->id))
    {
        mmgr_line_put(args->err_msg, args->id);
        mmgr_line_put(args->err_msg, " is not a commit");
        return 0;
    }
    char parent[CLI_NAME_MAX_LEN];
    VerbaLinea line = {parent, sizeof(parent), 0ULL};
    mmgr_line_put(&line, args->id);
    mmgr_line_put(&line, "^1");
    mmgr_line_finish(&line);
    const char *const verify[4] = {"rev-parse", "-q", "--verify", parent};
    CliGitResult *const result = cli_git_alloc();
    const int from = EMBED_CALL(cli_git_result_internal.query, CliGitResultCtx, .root = args->root, .request = verify,
                                .count = 4u, .result = result);
    const char *const arguments[9] = {
        "diff-tree", "-r", "-M", "--name-status", "-z", "--relative", "--no-commit-id", from ? parent : "--root",
        args->id};
    if (!EMBED_CALL(cli_git_result_internal.query, CliGitResultCtx, .root = args->root, .request = arguments,
                    .count = 9u, .result = result))
    {
        EMBED_CALL(cli_git_result_internal.free, CliGitResultCtx, .result = result);
        mmgr_line_put(args->err_msg, args->id);
        mmgr_line_put(args->err_msg, " is not a commit");
        return 0;
    }
    // the fields of what git wrote, the empty ones passed
    const char *at = result->out;
    const char *const end = result->out + result->out_size;
    const char *fields[3] = {NULL, NULL, NULL};
    unsigned long long lengths[3] = {0ULL, 0ULL, 0ULL};
    while (args->found_touched_list_pointer->count < CLI_GIT_TOUCHED)
    {
        unsigned int read = 0u;
        char state = 'M';
        unsigned int want = 1u;
        while (read < want)
        {
            do
            {
                fields[read] = EMBED_CALL(cli_git_at_internal.field, CliGitAtCtx, .at = &at, .end = end, .stop = '\0',
                                          .length_pointer = &lengths[read]);
            } while ((fields[read] != NULL) && (lengths[read] == 0ULL));
            if (fields[read] == NULL)
            {
                break;
            }
            if (read == 0u)
            {
                state = fields[0][0];
                want = ((state == 'R') || (state == 'C')) ? 3u : 2u;
            }
            read += 1u;
        }
        if ((read < 2u) || (((state == 'R') || (state == 'C')) && (read < 3u)))
        {
            break;
        }
        CliTouched *const one = &args->found_touched_list_pointer->item[args->found_touched_list_pointer->count];
        args->found_touched_list_pointer->count += 1u;
        if ((state == 'R') || (state == 'C'))
        {
            one->path =
                EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found_touched_list_pointer->text,
                           .from = fields[2], .length = lengths[2]);
            one->state = (state == 'R') ? 'R' : 'A';
            one->has_was = state == 'R';
            one->was = one->has_was ? EMBED_CALL(cli_git_text.text_append, CliGitTextCfg,
                                                 .text = &args->found_touched_list_pointer->text, .from = fields[1],
                                                 .length = lengths[1])
                                    : 0u;
        }
        else
        {
            one->path =
                EMBED_CALL(cli_git_text.text_append, CliGitTextCfg, .text = &args->found_touched_list_pointer->text,
                           .from = fields[1], .length = lengths[1]);
            one->state = ((state == 'A') || (state == 'D')) ? state : 'M';
            one->has_was = 0;
            one->was = 0u;
        }
    }
    EMBED_CALL(cli_git_result_internal.free, CliGitResultCtx, .result = result);
    return 1;
}
