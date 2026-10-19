// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// git_lists.c: the commits of other branches not applied here, the commits that touched a file, a file's text as a
// commit left it, who made a commit, and the files a commit changed
#include "git_internal.h"

// the commits a list asks git for, as git is given the count
#define CLI_GIT_LIMIT "-n200"

// the commit lines `id\x1fwhen\x1fsubject` of what git wrote put after those found
static void cli_commits_of(const char *out, unsigned long long size, CliCommits *found)
{
    const char *line = out;
    const char *const end = out + size;
    while ((line < end) && (found->count < CLI_GIT_COMMITS))
    {
        const char *stop = line;
        while ((stop < end) && (stop[0] != '\n'))
        {
            stop += 1;
        }
        const char *const next = stop + 1;
        stop -= ((stop > line) && (stop[-1] == '\r')) ? 1 : 0;
        const char *at = line;
        unsigned long long id_length = 0ull;
        unsigned long long when_length = 0ull;
        const char *const id = cli_git_field(&at, stop, '\x1f', &id_length);
        const char *const when = cli_git_field(&at, stop, '\x1f', &when_length);
        const char *const subject = (at <= stop) ? at : NULL;
        line = next;
        long long seconds = 0ll;
        if ((when == NULL) || !cli_git_seconds(when, when_length, &seconds))
        {
            continue;
        }
        CliCommit *const one = &found->item[found->count];
        found->count += 1u;
        one->id = cli_git_text_put(&found->text, id, id_length);
        one->when = seconds;
        one->subject =
            (subject != NULL) ? cli_git_text_put(&found->text, subject, (unsigned long long)(stop - subject)) : 0u;
    }
}

// What makes a commit the same as another under another id, its author, the time the author made it and its message,
// for each commit found elsewhere, and whether one of the branch's own is the same, held under the lock
typedef struct
{
    CliGitText made;
    unsigned int at[CLI_GIT_COMMITS];
    unsigned long long length[CLI_GIT_COMMITS];
    int ours[CLI_GIT_COMMITS];
    CliCommits found;
} CliElsewhere;

static CliElsewhere s_elsewhere;
static CliLock s_elsewhere_lock = CLI_LOCK_INIT;

// the records `id\x1fwhen\x1fauthor\x1fat\x1fmessage\x1e` of what git wrote handed to `each` with the commit's id, its
// time, and its author, the time the author made it and its message joined by `\x1f`, the message's end trimmed
static void cli_records(const char *out, unsigned long long size,
                        void (*each)(const char *id, unsigned long long id_length, long long when, const char *made,
                                     unsigned long long made_length))
{
    const char *record = out;
    const char *const end = out + size;
    while (record < end)
    {
        const char *stop = record;
        while ((stop < end) && (stop[0] != '\x1e'))
        {
            stop += 1;
        }
        const char *const next = stop + 1;
        while ((record < stop) && (record[0] == '\n'))
        {
            record += 1;
        }
        const char *at = record;
        unsigned long long lengths[4] = {0ull, 0ull, 0ull, 0ull};
        const char *fields[4] = {NULL, NULL, NULL, NULL};
        for (unsigned int field = 0u; field < 4u; field += 1u)
        {
            fields[field] = cli_git_field(&at, stop, '\x1f', &lengths[field]);
        }
        const char *const message = (at <= stop) ? at : NULL;
        long long when = 0ll;
        if ((fields[3] != NULL) && (message != NULL) && cli_git_seconds(fields[1], lengths[1], &when))
        {
            const char *message_end = stop;
            while ((message_end > message) &&
                   ((message_end[-1] == ' ') || ((message_end[-1] >= '\t') && (message_end[-1] <= '\r'))))
            {
                message_end -= 1;
            }
            each(fields[0], lengths[0], when, fields[2], (unsigned long long)(message_end - fields[2]));
        }
        record = next;
    }
}

// a commit found elsewhere kept, its subject the first line of its message
static void cli_elsewhere_each(const char *id, unsigned long long id_length, long long when, const char *made,
                               unsigned long long made_length)
{
    CliCommits *const found = &s_elsewhere.found;
    if (found->count >= CLI_GIT_COMMITS)
    {
        return;
    }
    const char *message = made;
    for (unsigned int seen = 0u; seen < 2u; seen += 1u)
    {
        while ((message < (made + made_length)) && (message[0] != '\x1f'))
        {
            message += 1;
        }
        message += (message < (made + made_length)) ? 1 : 0;
    }
    const char *line_end = message;
    while ((line_end < (made + made_length)) && (line_end[0] != '\n'))
    {
        line_end += 1;
    }
    line_end -= ((line_end > message) && (line_end[-1] == '\r')) ? 1 : 0;
    CliCommit *const one = &found->item[found->count];
    one->id = cli_git_text_put(&found->text, id, id_length);
    one->when = when;
    one->subject = cli_git_text_put(&found->text, message, (unsigned long long)(line_end - message));
    s_elsewhere.at[found->count] = cli_git_text_put(&s_elsewhere.made, made, made_length);
    s_elsewhere.length[found->count] = made_length;
    s_elsewhere.ours[found->count] = 0;
    found->count += 1u;
}

// a commit of the branch's own marks each found elsewhere that is the same
static void cli_ours_each(const char *id, unsigned long long id_length, long long when, const char *made,
                          unsigned long long made_length)
{
    (void)id;
    (void)id_length;
    (void)when;
    for (unsigned int at = 0u; at < s_elsewhere.found.count; at += 1u)
    {
        if ((s_elsewhere.length[at] == made_length) &&
            (memor.cmp(&(MemoriaCfg){.src = cli_git_text_of(&s_elsewhere.made, s_elsewhere.at[at]),
                                     .other = made,
                                     .bytes = made_length}) == 0))
        {
            s_elsewhere.ours[at] = 1;
        }
    }
}

void cli_git_elsewhere(const char *root, CliCommits *found)
{
    static const char *const s_format = "--format=%H%x1f%ct%x1f%ae%x1f%at%x1f%B%x1e";
    found->count = 0u;
    cli_git_text_begun(&found->text);
    cli_lock_take(&s_elsewhere_lock);
    s_elsewhere.found.count = 0u;
    cli_git_text_begun(&s_elsewhere.found.text);
    cli_git_text_begun(&s_elsewhere.made);
    const char *const theirs[8] = {"log",  "--branches",  "--remotes",   "--not",
                                   "HEAD", "--no-merges", CLI_GIT_LIMIT, s_format};
    CliGitGave *const gave = cli_git_taken();
    if (cli_git_asked(root, theirs, 8u, gave))
    {
        cli_records(gave->out, gave->out_size, cli_elsewhere_each);
    }
    long long oldest = 0ll;
    for (unsigned int at = 0u; at < s_elsewhere.found.count; at += 1u)
    {
        oldest = ((at == 0u) || (s_elsewhere.found.item[at].when < oldest)) ? s_elsewhere.found.item[at].when : oldest;
    }
    char since[64];
    CliLine line = {since, sizeof(since), 0ull};
    cli_line_text(&line, "--since=@");
    if (oldest < 0ll)
    {
        cli_line_character(&line, '-');
    }
    // the oldest time's size, its sign written apart
    cli_line_decimal(&line, (unsigned long long)((oldest < 0ll) ? -oldest : oldest), 1u);
    cli_line_closed(&line);
    const char *const ours[5] = {"log", "HEAD", "--no-merges", since, s_format};
    // where the branch's own commits cannot be read, no commit is said to be elsewhere: each would be
    const int read = (s_elsewhere.found.count != 0u) && cli_git_asked(root, ours, 5u, gave);
    if (read)
    {
        cli_records(gave->out, gave->out_size, cli_ours_each);
    }
    cli_git_given(gave);
    for (unsigned int at = 0u; read && (at < s_elsewhere.found.count); at += 1u)
    {
        const CliCommit *const one = &s_elsewhere.found.item[at];
        if (s_elsewhere.ours[at])
        {
            continue;
        }
        const char *const id = cli_git_text_of(&s_elsewhere.found.text, one->id);
        const char *const subject = cli_git_text_of(&s_elsewhere.found.text, one->subject);
        CliCommit *const kept = &found->item[found->count];
        found->count += 1u;
        kept->id = cli_git_text_put(&found->text, id, cellul.len(&(CatenaFinitaCfg){.src = id, .cap = CLI_CELLS_POOL}));
        kept->when = one->when;
        kept->subject = cli_git_text_put(&found->text, subject,
                                         cellul.len(&(CatenaFinitaCfg){.src = subject, .cap = CLI_CELLS_POOL}));
    }
    cli_lock_give(&s_elsewhere_lock);
}

int cli_git_commits(const char *root, const char *file, CliCommits *found, CliLine *said)
{
    found->count = 0u;
    cli_git_text_begun(&found->text);
    char folder[CLI_PATH_LONGEST];
    char shown[CLI_PATH_LONGEST];
    if (!cli_git_beside(root, file, folder, shown, said))
    {
        return 0;
    }
    const char *const args[6] = {"log", "--follow", CLI_GIT_LIMIT, "--format=%H%x1f%ct%x1f%s", "--", shown};
    CliGitGave *const gave = cli_git_taken();
    if (cli_git_asked(folder, args, 6u, gave))
    {
        cli_commits_of(gave->out, gave->out_size, found);
    }
    cli_git_given(gave);
    return 1;
}

// git asked to show `what` of the file at `shown` from `folder`, `what` before the colon: what it wrote into `out`, 1,
// or 0 where it could not
static int cli_shown(const char *folder, const char *what, const char *shown, CliLine *out)
{
    char named[CLI_PATH_LONGEST + CLI_NAME_LONGEST];
    CliLine line = {named, sizeof(named), 0ull};
    cli_line_text(&line, what);
    cli_line_character(&line, ':');
    cli_line_text(&line, shown);
    if (cli_line_closed(&line) == 0ull)
    {
        return 0;
    }
    const char *const args[2] = {"show", named};
    CliGitGave *const gave = cli_git_taken();
    const int found = cli_git_asked(folder, args, 2u, gave);
    if (found)
    {
        cli_line_counted(out, gave->out, gave->out_size);
    }
    cli_git_given(gave);
    return found;
}

int cli_git_text_at(const char *root, const char *file, const char *id, CliLine *out, CliLine *said)
{
    char folder[CLI_PATH_LONGEST];
    char shown[CLI_PATH_LONGEST];
    if (!cli_git_beside(root, file, folder, shown, said))
    {
        return 0;
    }
    if (!cli_git_is_id(id))
    {
        cli_line_text(said, id);
        cli_line_text(said, " is not a commit");
        return 0;
    }
    if (!cli_shown(folder, id, shown, out))
    {
        cli_line_text(said, file);
        cli_line_text(said, " is not in ");
        cli_line_text(said, id);
        return 0;
    }
    return 1;
}

int cli_git_text_before(const char *root, const char *file, const char *id, CliLine *out)
{
    char folder[CLI_PATH_LONGEST];
    char shown[CLI_PATH_LONGEST];
    char why[CLI_NAME_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    char parent[CLI_NAME_LONGEST];
    CliLine line = {parent, sizeof(parent), 0ull};
    cli_line_text(&line, id);
    cli_line_text(&line, "^1");
    return cli_git_is_id(id) && (cli_line_closed(&line) != 0ull) && cli_git_beside(root, file, folder, shown, &said) &&
           cli_shown(folder, parent, shown, out);
}

int cli_git_head_text(const char *root, const char *file, CliLine *out)
{
    char folder[CLI_PATH_LONGEST];
    char shown[CLI_PATH_LONGEST];
    char why[CLI_NAME_LONGEST];
    CliLine said = {why, sizeof(why), 0ull};
    return cli_git_beside(root, file, folder, shown, &said) && cli_shown(folder, "HEAD", shown, out);
}

int cli_git_made_by(const char *root, const char *id, CliLine *name, CliLine *subject)
{
    if (!cli_git_is_id(id))
    {
        return 0;
    }
    const char *const args[4] = {"log", "-1", "--format=%an%x00%s", id};
    CliGitGave *const gave = cli_git_taken();
    int found = cli_git_asked(root, args, 4u, gave);
    const char *from = gave->out;
    const char *to = gave->out + gave->out_size;
    while ((to > from) && ((to[-1] == ' ') || ((to[-1] >= '\t') && (to[-1] <= '\r'))))
    {
        to -= 1;
    }
    const char *zero = from;
    while ((zero < to) && (zero[0] != '\0'))
    {
        zero += 1;
    }
    found = found && (zero < to);
    if (found)
    {
        cli_line_counted(name, from, (unsigned long long)(zero - from));
        cli_line_counted(subject, zero + 1, (unsigned long long)(to - zero - 1));
    }
    cli_git_given(gave);
    return found;
}

int cli_git_touched(const char *root, const char *id, CliTouchedList *found, CliLine *said)
{
    found->count = 0u;
    cli_git_text_begun(&found->text);
    if (!cli_git_is_id(id))
    {
        cli_line_text(said, id);
        cli_line_text(said, " is not a commit");
        return 0;
    }
    char parent[CLI_NAME_LONGEST];
    CliLine line = {parent, sizeof(parent), 0ull};
    cli_line_text(&line, id);
    cli_line_text(&line, "^1");
    cli_line_closed(&line);
    const char *const verify[4] = {"rev-parse", "-q", "--verify", parent};
    CliGitGave *const gave = cli_git_taken();
    const int from = cli_git_asked(root, verify, 4u, gave);
    const char *const args[9] = {
        "diff-tree", "-r", "-M", "--name-status", "-z", "--relative", "--no-commit-id", from ? parent : "--root", id};
    if (!cli_git_asked(root, args, 9u, gave))
    {
        cli_git_given(gave);
        cli_line_text(said, id);
        cli_line_text(said, " is not a commit");
        return 0;
    }
    // the fields of what git wrote, the empty ones passed
    const char *at = gave->out;
    const char *const end = gave->out + gave->out_size;
    const char *fields[3] = {NULL, NULL, NULL};
    unsigned long long lengths[3] = {0ull, 0ull, 0ull};
    while (found->count < CLI_GIT_TOUCHED)
    {
        unsigned int read = 0u;
        char state = 'M';
        for (unsigned int want = 1u; read < want; read += 1u)
        {
            do
            {
                fields[read] = cli_git_field(&at, end, '\0', &lengths[read]);
            } while ((fields[read] != NULL) && (lengths[read] == 0ull));
            if (fields[read] == NULL)
            {
                break;
            }
            if (read == 0u)
            {
                state = fields[0][0];
                want = ((state == 'R') || (state == 'C')) ? 3u : 2u;
            }
        }
        if ((read < 2u) || (((state == 'R') || (state == 'C')) && (read < 3u)))
        {
            break;
        }
        CliTouched *const one = &found->item[found->count];
        found->count += 1u;
        if ((state == 'R') || (state == 'C'))
        {
            one->path = cli_git_text_put(&found->text, fields[2], lengths[2]);
            one->state = (state == 'R') ? 'R' : 'A';
            one->has_was = state == 'R';
            one->was = one->has_was ? cli_git_text_put(&found->text, fields[1], lengths[1]) : 0u;
        }
        else
        {
            one->path = cli_git_text_put(&found->text, fields[1], lengths[1]);
            one->state = ((state == 'A') || (state == 'D')) ? state : 'M';
            one->has_was = 0;
            one->was = 0u;
        }
    }
    cli_git_given(gave);
    return 1;
}
