// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// query_words_test.c: the words of `orior_cli protocol`: the list names every loop once; leave is taken off the words; a
// loop missing a program says so and starts no run; an unknown command is refused with the list; the table's whole
// names are those with every kind; the steps run the protocol in order and their names are offered to start from; the
// base is the one word the cross takes; the readings of a table are told apart from a pull; the version and the
// architectures are read off nvcc; the query's words are read and a file not there is said; and the writers take their
// words
#include "../../../../../ui/query/src/learn/tables_internal.h"
#include "../../../../../ui/query/src/query_loops.h"
#include "cli_check.h"

static char s_root[QUERY_PATH_LONGEST];
static char s_said[QUERY_SAID_LONGEST];
static unsigned long long s_said_size;

// a line said, kept
static void words_said(void *context, const char *text, unsigned long long length)
{
    (void)context;
    CliLine line = {s_said, sizeof(s_said), 0ull};
    cli_line_counted(&line, text, length);
    s_said_size = cli_line_closed(&line);
}

// nothing is found, each said by its name
static int words_nothing(void *context, QueryRole role, char *path, size_t room, CliLine *said)
{
    (void)context;
    (void)room;
    path[0] = '\0';
    cli_line_text(said, query_role_name(role));
    cli_line_text(said, " is not found");
    return 0;
}

// no PATH beyond the process's own
static const char *words_no_path(void *context)
{
    (void)context;
    return "";
}

// the list names every loop once; leave is taken off the words; a loop missing a program says so and starts no run;
// and an unknown command is refused with the list
static void words_dispatch(void)
{
    const QueryCommand *commands = NULL;
    const unsigned int count = query_commands(".", &commands);
    char ids[256];
    CliLine line = {ids, sizeof(ids), 0ull};
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_line_text(&line, (at > 0u) ? "," : "");
        cli_line_text(&line, commands[at].id);
    }
    cli_check_text(ids, line.at, "course,tables,decode,query,learn,cross,write,pairs,stick,kept,sweep",
                   "the list names every loop once");
    const char *words[5] = {"turns", "--leave", "none", "--base", "a.cu"};
    unsigned int left = 5u;
    int given = 0;
    QueryLeave leave = QUERY_LEAVE_ALL;
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(query_leave_taken(words, &left, &given, &leave, &said) && given && (leave == QUERY_LEAVE_NONE) &&
                  (left == 3u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = words[0], .other = "turns", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = words[1], .other = "--base", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = words[2], .other = "a.cu", .cap = CLI_CELLS_POOL})),
              "leave is taken off the words");
    const char *bad[2] = {"--leave", "maybe"};
    left = 2u;
    cli_check(!query_leave_taken(bad, &left, &given, &leave, &said), "a leave no one knows is refused");
    const char *none[1] = {"turns"};
    left = 1u;
    cli_check(query_leave_taken(none, &left, &given, &leave, &said) && !given, "and none given is none");
    static unsigned long long s_numbers[QUERY_RUNS_MOST];
    static char s_paths[QUERY_RUNS_MOST][QUERY_PATH_LONGEST];
    char folder[QUERY_PATH_LONGEST];
    cli_full(s_root, QUERY_LEARN_FOLDER, folder);
    const unsigned int before = query_runs_each(folder, s_numbers, s_paths, QUERY_RUNS_MOST);
    const QueryPrograms nothing = {words_nothing, words_no_path, NULL};
    const QuerySink sink = {words_said, NULL};
    static const char *const s_learn[2] = {"learn", "turns"};
    s_said_size = 0ull;
    cli_check(
        (query_run(s_root, &nothing, s_learn, 2u, sink) < 0) &&
            ((cellul.find(&(CatenaFinitaCfg){
                  .src = s_said, .cap = CLI_CELLS_POOL, .other = "is not found", .other_cap = CLI_CELLS_POOL}) !=
              NULL) ||
             (cellul.find(&(CatenaFinitaCfg){
                  .src = s_said, .cap = CLI_CELLS_POOL, .other = "no nvdisasm", .other_cap = CLI_CELLS_POOL}) != NULL)),
        "a loop missing a program says so");
    cli_check(query_runs_each(folder, s_numbers, s_paths, QUERY_RUNS_MOST) == before, "and starts no run");
    const QueryPrograms on_path = query_on_path();
    static const char *const s_dance[1] = {"dance"};
    s_said_size = 0ull;
    cli_check((query_run(".", &on_path, s_dance, 1u, sink) < 0) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = s_said, .cap = CLI_CELLS_POOL, .other = "learn", .other_cap = CLI_CELLS_POOL}) != NULL) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = s_said, .cap = CLI_CELLS_POOL, .other = "sweep", .other_cap = CLI_CELLS_POOL}) != NULL),
              "an unknown command is refused with the list");
    s_said_size = 0ull;
    cli_check((query_run(".", &on_path, NULL, 0u, sink) < 0) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = s_said, .cap = CLI_CELLS_POOL, .other = "learn", .other_cap = CLI_CELLS_POOL}) != NULL),
              "and no command too");
}

// the files of the table at `root`, each empty
static void words_table(const char *root, const char *const *files, unsigned int count)
{
    char table[QUERY_PATH_LONGEST];
    char path[QUERY_PATH_LONGEST];
    cli_full(root, QUERY_TABLE, table);
    (void)query_removed(root);
    (void)query_folder_made(table);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        cli_full(table, files[at], path);
        (void)query_file_written(path, "", 0ull);
    }
}

// the whole names the table holds of `kinds`, joined by `,`
static const char *words_whole(const char *root, const char *const *kinds, unsigned int count)
{
    static char s_names[64][QUERY_WHOLE_LONGEST];
    static const char *s_named[64];
    static char s_joined[512];
    const unsigned int named = query_whole(root, kinds, count, s_names, s_named);
    CliLine line = {s_joined, sizeof(s_joined), 0ull};
    for (unsigned int at = 0u; at < named; at += 1u)
    {
        cli_line_text(&line, (at > 0u) ? "," : "");
        cli_line_text(&line, s_named[at]);
    }
    (void)cli_line_closed(&line);
    return s_joined;
}

// the table's whole names are those with every kind
static void words_whole_names(void)
{
    char root[QUERY_PATH_LONGEST];
    cli_full(s_root, "build/query_test/whole", root);
    static const char *const s_files[9] = {"cu.klm",    "cu.krs",    "sass.klm",  "sass.krs", "c.krs",
                                           "sm_86.khw", "sm_86.kqr", "sm_86.ksc", "sm_89.khw"};
    words_table(root, s_files, 9u);
    static const char *const s_languages[2] = {"klm", "krs"};
    static const char *const s_parts[3] = {"khw", "kqr", "ksc"};
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = words_whole(root, s_languages, 2u), .other = "cu,sass", .cap = CLI_CELLS_POOL}),
              "the languages whole");
    cli_check(
        cellul.eq(&(CatenaFinitaCfg){.src = words_whole(root, s_parts, 3u), .other = "sm_86", .cap = CLI_CELLS_POOL}),
        "and the parts whole");
    (void)query_removed(root);
}

// the steps run the protocol in order and their names are offered to start from; and the base is the single word the
// cross takes
static void words_course(void)
{
    char names[512];
    CliLine line = {names, sizeof(names), 0ull};
    query_names_joined(&line);
    cli_check_text(
        names, line.at,
        "vendor tables, discover, turns, registers, write krs, stick engine, pairs stick, pairs past, costs, "
        "write krs",
        "the steps run the protocol in order");
    unsigned int first = 9u;
    char why[1024];
    CliLine said = {why, sizeof(why), 0ull};
    cli_check(query_course_from(NULL, 0u, &first, &said) && (first == 0u), "the first where none is named");
    static const char *const s_costs[2] = {"--from", "costs"};
    cli_check(query_course_from(s_costs, 2u, &first, &said) && (first == 8u), "a step by its name");
    static const char *const s_krs[2] = {"--from", "write krs"};
    cli_check(query_course_from(s_krs, 2u, &first, &said) && (first == 4u), "a name two steps share names the first");
    static const char *const s_bare[1] = {"--from"};
    cli_check(!query_course_from(s_bare, 1u, &first, &said), "a flag with no name is refused");
    static const char *const s_fly[2] = {"--from", "fly"};
    said.at = 0ull;
    cli_check(!query_course_from(s_fly, 2u, &first, &said) && (cli_line_closed(&said) != 0ull) &&
                  (cellul.find(&(CatenaFinitaCfg){.src = why,
                                                  .cap = CLI_CELLS_POOL,
                                                  .other = "no step of the course is fly",
                                                  .other_cap = CLI_CELLS_POOL}) != NULL),
              "and a name no step has");
    const char *base = "x";
    cli_check(query_base_of(NULL, 0u, &base, &said) && (base == NULL), "the cross with no base");
    static const char *const s_base[2] = {"--base", "a.cu"};
    cli_check(query_base_of(s_base, 2u, &base, &said) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = base, .other = "a.cu", .cap = CLI_CELLS_POOL})),
              "and with one");
    static const char *const s_word[1] = {"a.cu"};
    cli_check(!query_base_of(s_word, 1u, &base, &said) && !query_base_of(s_base, 1u, &base, &said),
              "a base not given as the cross takes it is refused");
}

// the reading `text`'s words ask for: 1 with its mode into `mode`, 0 for a pull, -1 refused
static int words_reading(const char *text, const char **mode)
{
    static char s_text[256];
    static const char *s_words[16];
    memor.cpy(&(MemoriaCfg){
        .dst = s_text, .src = text, .bytes = cellul.len(&(CatenaFinitaCfg){.src = text, .cap = CLI_CELLS_POOL}) + 1u});
    unsigned int count = 0u;
    for (char *at = s_text; (at[0] != '\0') && (count < 16u);)
    {
        while (at[0] == ' ')
        {
            at[0] = '\0';
            at += 1;
        }
        if (at[0] == '\0')
        {
            break;
        }
        s_words[count] = at;
        count += 1u;
        while ((at[0] != ' ') && (at[0] != '\0'))
        {
            at += 1;
        }
    }
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    *mode = NULL;
    return query_reading(s_words, count, mode, &said);
}

// the readings of a table are told apart from a pull; and the version and the architectures are read off nvcc
static void words_tables(void)
{
    const char *mode = NULL;
    cli_check(words_reading("sm_86 sm_90", &mode) == 0, "architectures alone ask for a pull");
    cli_check(words_reading("", &mode) == 0, "and nothing too");
    cli_check((words_reading("fields sm_86", &mode) == 1) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = mode, .other = "spans", .cap = CLI_CELLS_POOL})),
              "the fields of a table");
    cli_check(words_reading("fields sm_86 extra", &mode) == -1, "take the architecture alone");
    cli_check(words_reading("fields", &mode) == -1, "and take it");
    cli_check((words_reading("read sm_86 --bank 0x174 0000460000047ab9 000fc80000000a00", &mode) == 1) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = mode, .other = "class", .cap = CLI_CELLS_POOL})),
              "a reading of encodings");
    cli_check(words_reading("read sm_86 0000460000047ab9", &mode) == -1, "an encoding is two words");
    cli_check(words_reading("read sm_86 --bank", &mode) == -1, "and a bank its size");
    static const char s_version[] =
        "Cuda compilation tools, release 13.3, V13.3.73\nBuild cuda_13.3.r13.3/compiler.38244171_0";
    char version[64];
    cli_check(query_version_of((const unsigned char *)s_version, sizeof(s_version) - 1u, version, sizeof(version)) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = version, .other = "13.3.73", .cap = CLI_CELLS_POOL})),
              "the version is read off nvcc");
    cli_check(!query_version_of((const unsigned char *)"nothing", 7ull, version, sizeof(version)),
              "and none of nothing");
    static const char s_arches[] = "compute_75 compute_80 compute_120\n";
    char arches[8][16];
    const unsigned int count = query_architectures_of((const unsigned char *)s_arches, sizeof(s_arches) - 1u, arches);
    cli_check((count == 3u) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = arches[0], .other = "sm_75", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = arches[1], .other = "sm_80", .cap = CLI_CELLS_POOL})) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = arches[2], .other = "sm_120", .cap = CLI_CELLS_POOL})),
              "the architectures are read off nvcc");
    char archive[128];
    query_archive_named("13.3.73", archive, sizeof(archive));
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = archive, .other = "cuda_nvdisasm-linux-x86_64-13.3.73-archive", .cap = CLI_CELLS_POOL}),
              "the vendor's archive by version");
    char inside[QUERY_PATH_LONGEST];
    query_in_container("D:\\tree", "D:\\tree\\build\\vendor\\nvidia\\cubins\\sm_86.cubin", inside);
    cli_check(cellul.eq(&(CatenaFinitaCfg){
                  .src = inside, .other = "/tree/build/vendor/nvidia/cubins/sm_86.cubin", .cap = CLI_CELLS_POOL}),
              "a path as the container sees it");
}

// the query's words read at `root`: 1 with its names joined by `,` and `dry` after a `|` into `out`, or 0 with why
// into `out`
static int words_query(const char *root, const char *const *words, unsigned int count, char *out,
                       unsigned long long room)
{
    QueryContext context;
    memor.set(&(MemoriaCfg){.dst = &context, .val = 0, .bytes = sizeof(context)});
    context.root = root;
    const char *names[3] = {NULL, NULL, NULL};
    unsigned int named = 0u;
    int dry = 0;
    CliLine said = {out, room, 0ull};
    const int read = query_query_words(&context, words, count, names, &named, &dry, &said);
    if (read)
    {
        said.at = 0ull;
        for (unsigned int at = 0u; at < named; at += 1u)
        {
            cli_line_text(&said, (at > 0u) ? "," : "");
            cli_line_text(&said, names[at]);
        }
        cli_line_text(&said, dry ? "|dry" : "|");
    }
    (void)cli_line_closed(&said);
    return read;
}

// the query's words are read, and a file not there is said
static void words_query_words(void)
{
    static const char *const s_whole[9] = {"Lstar.klq", "cu.klm",    "cu.krs",    "sass.klm", "sass.krs",
                                           "sm_86.khw", "sm_86.kqr", "sm_86.ksc", "c.krs"};
    char root[QUERY_PATH_LONGEST];
    char out[1024];
    cli_full(s_root, "build/query_test/query_values", root);
    words_table(root, s_whole, 9u);
    static const char *const s_two[2] = {"--cu", "--sass"};
    cli_check(words_query(root, s_two, 2u, out, sizeof(out)) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = out, .other = "cu,sass|", .cap = CLI_CELLS_POOL})),
              "two languages");
    static const char *const s_three[3] = {"--cu", "--sass", "--sm_86"};
    cli_check(words_query(root, s_three, 3u, out, sizeof(out)) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = out, .other = "cu,sass,sm_86|", .cap = CLI_CELLS_POOL})),
              "and a part");
    static const char *const s_flags[7] = {"--from", "cu", "--to", "sass", "--part", "sm_86", "--dry"};
    cli_check(words_query(root, s_flags, 7u, out, sizeof(out)) &&
                  (cellul.eq(&(CatenaFinitaCfg){.src = out, .other = "cu,sass,sm_86|dry", .cap = CLI_CELLS_POOL})),
              "as the window's job says them, dry");
    static const char *const s_one[1] = {"--cu"};
    static const char *const s_plain[2] = {"cu", "--sass"};
    static const char *const s_more[4] = {"--cu", "--sass", "--sm_86", "--more"};
    static const char *const s_path[2] = {"--../cu", "--sass"};
    static const char *const s_dry[3] = {"--cu", "--sass", "--dry"};
    cli_check(!words_query(root, s_one, 1u, out, sizeof(out)) && !words_query(root, s_plain, 2u, out, sizeof(out)) &&
                  !words_query(root, s_more, 4u, out, sizeof(out)) && !words_query(root, s_path, 2u, out, sizeof(out)),
              "words not a query's are refused");
    cli_check(!words_query(root, s_dry, 3u, out, sizeof(out)), "dry runs the pairs, which take a part");
    static const char *const s_short[3] = {"--cu", "--sass", "--sm_89"};
    cli_check(
        !words_query(root, s_short, 3u, out, sizeof(out)) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = out, .cap = CLI_CELLS_POOL, .other = "sm_89.khw", .other_cap = CLI_CELLS_POOL}) != NULL) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = out, .cap = CLI_CELLS_POOL, .other = "sm_89.kqr", .other_cap = CLI_CELLS_POOL}) != NULL) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = out, .cap = CLI_CELLS_POOL, .other = "sm_89.ksc", .other_cap = CLI_CELLS_POOL}) != NULL) &&
            (cellul.find(&(CatenaFinitaCfg){
                 .src = out, .cap = CLI_CELLS_POOL, .other = "cu.klm", .other_cap = CLI_CELLS_POOL}) == NULL),
        "a part's files not there are said");
    static const char *const s_host[3] = {"--c", "--rs", "--host"};
    cli_check(!words_query(root, s_host, 3u, out, sizeof(out)) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = out, .cap = CLI_CELLS_POOL, .other = "c.klm", .other_cap = CLI_CELLS_POOL}) != NULL) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = out, .cap = CLI_CELLS_POOL, .other = "c.krs", .other_cap = CLI_CELLS_POOL}) == NULL) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = out, .cap = CLI_CELLS_POOL, .other = "rs.krs", .other_cap = CLI_CELLS_POOL}) != NULL) &&
                  (cellul.find(&(CatenaFinitaCfg){
                       .src = out, .cap = CLI_CELLS_POOL, .other = "host.khw", .other_cap = CLI_CELLS_POOL}) != NULL),
              "and a language's");
    (void)query_removed(root);
    char why[512];
    CliLine said = {why, sizeof(why), 0ull};
    static const char *const s_bridge[3] = {"bridge", "cu.krs", "c.krs"};
    static const char *const s_krs[2] = {"krs", "more"};
    static const char *const s_gsm[1] = {"gsm"};
    static const char *const s_kdm[1] = {"kdm"};
    cli_check((query_write_words(s_bridge, 1u, &said) == QUERY_WRITE_BRIDGE) &&
                  (query_write_words(s_bridge, 3u, &said) == QUERY_WRITE_BRIDGE),
              "the bridge's writer, over the rulesets named or all");
    cli_check((query_write_words(s_krs, 1u, &said) == QUERY_WRITE_KRS) &&
                  (query_write_words(s_gsm, 1u, &said) == QUERY_WRITE_GSM),
              "the ruleset's and the machine file's writers");
    cli_check((query_write_words(s_krs, 2u, &said) < 0) && (query_write_words(s_kdm, 1u, &said) < 0) &&
                  (query_write_words(NULL, 0u, &said) < 0),
              "and nothing else");
}

int main(void)
{
    char found[CLI_PATH_LONGEST];
    if (!cli_tree_found(found))
    {
        cli_check(1, "no tree is found, and the protocol's words are not read");
        return cli_checked();
    }
    memor.cpy(&(MemoriaCfg){.dst = s_root,
                            .src = found,
                            .bytes = cellul.len(&(CatenaFinitaCfg){.src = found, .cap = CLI_CELLS_POOL}) + 1u});
    for (char *at = s_root; at[0] != '\0'; at += 1)
    {
        at[0] = (at[0] == '\\') ? '/' : at[0];
    }
    words_dispatch();
    words_whole_names();
    words_course();
    words_tables();
    words_query_words();
    return cli_checked();
}
