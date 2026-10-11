// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// kdm_check.c: the .kdm's reader and writer (kdm.h, ktext.h) and the block rule (klang.h) held to what their users
// read of them, on a .kdm this check writes and on a part's own
//
//   rows       a row is read with its five fields, and a descent's with its verdict; a comment, the head of the rows,
//              a who line and a line of a block are no row
//   blocks     a block of a language ends at its own closer alone: a closer of another language, a parenthesis and a
//              closer of a block inside it of the same language end nothing; a block never closed is none
//   section    the section written again goes after a who line of the code that wrote it, and the tail stands as the
//              file held it, the who in force over it written again before it
//   pipes      the pipes and their comment let go and put again at the end, after a who line of the code that put them
//   whole      a file written whole again keeps the who of each line it held, and a line it did not goes under the
//              writer's (ktext_lines_written_by)
//   put        a file handed to the run is read as the run holds it and is not on the disk while the run runs; once
//              the run ends it is in its place (chain_check.sh); a file the run reads from the disk is the run's after,
//              read from memory
//   a part's   the section of the part's .kdm named on the command line written again as it holds it leaves the file as
//   file       it was, a who line after its first line
//
//     kdm_check <folder> [<a part's .kdm>]
//
// It runs in a query run (qry_run.sh): every file it writes is handed to the run's writer.
#include "../../../../../../../src/cu/types/file_defs/kdm/kdm.h"
#include "../../../../../../../src/cu/types/file_defs/klang.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int s_checks = 0u;
static unsigned int s_failed = 0u;

static void check_that(int held, const char *what)
{
    s_checks += 1u;
    if (held == 0)
    {
        s_failed += 1u;
        printf("  FAILED: %s\n", what);
    }
}

// `file`'s lines from `from` joined, each ended by a newline, into memory of the caller's to free
static char *joined(const KdmFile *file, unsigned int from)
{
    QryText text = {0};
    for (unsigned int at = from; at < file->lines; at += 1u)
    {
        qry_text_printed(&text, "%s\n", file->line[at]);
    }
    qry_text_added(&text, "", 1u);
    return text.bytes;
}

// 1 where the block of `lang` found in `text` holds `inside`
static int block_holds(const char *text, const char *lang, const char *inside)
{
    KlangBlock block;
    return klang_block_find(text, strlen(text), 0u, lang, &block) &&
           ((block.text_end - block.text) == strlen(inside)) &&
           (strncmp(&text[block.text], inside, strlen(inside)) == 0);
}

// the tail every check below writes after the rows: a block of the ruleset's language with a block of it inside, a
// comment and a pipe
#define KDM_CHECK_TAIL                                                                                                 \
    "__krs__(\n"                                                                                                       \
    "fixed zero = RZ\n"                                                                                                \
    "form word_add to left right = IADD3 {to}, {left}, {right}, RZ ;\n"                                                \
    "__krs__(\n"                                                                                                       \
    "form inner = NOP\n"                                                                                               \
    ")__krs__\n"                                                                                                       \
    ")__krs__\n"                                                                                                       \
    "\n"                                                                                                               \
    "# the pipes\n"                                                                                                    \
    "pipe IMAD IMAD.MOV 4 2\n"

static const char *const s_written = "kdm part\n"
                                     "# a comment of the section\n"
                                     "# operator\tnodes\tchain\tcost\truns\n"
                                     "add\t1\tadd(w0, w1)\t-\t0\n"
                                     "sub\t1\tsub(w0, w1)\t3/1\t4\t1\n" KDM_CHECK_TAIL;

static KdmFile s_file;

int main(int count, char **words)
{
    if (count < 2)
    {
        printf("kdm_check <folder> [<a part's .kdm>]\n");
        return 2;
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s/kdm_check.kdm", words[1]);
    if (qry_filed(qry_run(), path, s_written, strlen(s_written)) == 0ull)
    {
        printf("  %s could not be written: no run's writer runs, or it stands outside the tree\n", path);
        return 2;
    }
    check_that(kdm_read(&s_file, path) && (s_file.lines == 15u), "a .kdm is read, a line each");
    static KdmRow s_row;
    check_that(kdm_row_read(s_file.line[3], &s_row) && (strcmp(s_row.operation, "add") == 0) && (s_row.nodes == 1u) &&
                   (strcmp(s_row.chain, "add(w0, w1)") == 0) && (strcmp(s_row.cost, "-") == 0) && (s_row.runs == 0u) &&
                   (s_row.verdict == -1),
               "a row is read with its five fields and no verdict");
    check_that(kdm_row_read(s_file.line[4], &s_row) && (strcmp(s_row.cost, "3/1") == 0) && (s_row.runs == 4u) &&
                   (s_row.verdict == 1),
               "a descent's row is read with its verdict");
    check_that(!kdm_row_read(s_file.line[0], &s_row) && !kdm_row_read(s_file.line[1], &s_row) &&
                   !kdm_row_read(s_file.line[2], &s_row) && !kdm_row_read(s_file.line[6], &s_row) &&
                   !kdm_row_read("who -", &s_row),
               "the first line, a comment, the head of the rows, a line of a block and a who line are no rows");
    check_that(kdm_tail_at(&s_file) == 5u, "the tail starts after the last row");

    unsigned int first = 0u;
    unsigned int past = 0u;
    check_that(ktext_block_lines(&s_file, "krs", 0u, &first, &past) && (first == 6u) && (past == 11u),
               "a block's lines run to its own closer, past the closer of the block inside it");
    check_that(block_holds("a __gsm__( x )__krs__ ) y )__gsm__ b", "gsm", " x )__krs__ ) y "),
               "a closer of another language and a parenthesis end no block");
    check_that(block_holds("__c__(f(x))__c__", "c", "f(x)"), "a block ends at its closer alone");
    check_that(block_holds("__krs__(a __krs__(b)__krs__ c)__krs__ d", "krs", "a __krs__(b)__krs__ c"),
               "a block inside a block of the same language is counted");
    KlangBlock block;
    check_that(!klang_block_find("__krs__( never closed", 21u, 0u, "krs", &block) &&
                   !klang_block_find("__gsm__(x)__gsm__", 17u, 0u, "krs", &block),
               "a block never closed is none, and a block of another language is no block of this one");

    const char *const section[4] = {"# a new comment", "# operator\tnodes\tchain\tcost\truns",
                                    "add\t1\tadd(w0, w1)\t2/1\t3", "sub\t1\tsub(w0, w1)\t-\t0"};
    const QryWho wrote = QRY_WHO_HERE;
    check_that(kdm_section_written_by(&s_file, "part", section, 4u, wrote), "the section is written again");
    char code[QRY_WHO_CODE_LETTERS + 1u];
    qry_who_code(wrote, code, sizeof(code));
    char expected[2048];
    snprintf(expected, sizeof(expected),
             "kdm part\n"
             "who %s\n"
             "# a new comment\n"
             "# operator\tnodes\tchain\tcost\truns\n"
             "add\t1\tadd(w0, w1)\t2/1\t3\n"
             "sub\t1\tsub(w0, w1)\t-\t0\n"
             "who -\n" KDM_CHECK_TAIL,
             code);
    char *text = joined(&s_file, 0u);
    check_that((text != NULL) && (strcmp(text, expected) == 0),
               "the section goes after a who line of the code that wrote it, and the tail stands as it was, the who in "
               "force over it, none, written again before it");
    free(text);
    QryWho there;
    check_that(ktext_who_at(&s_file, 4u, &there) && (there.source == wrote.source) && (there.line == wrote.line),
               "the who in force over the rows is the code that wrote them");
    check_that(!ktext_who_at(&s_file, 8u, &there), "and over the tail no who is in force, as before");

    const QryWho again = QRY_WHO_HERE;
    check_that(kdm_section_written_by(&s_file, "part", section, 4u, again), "the section is written again once more");
    qry_who_code(again, code, sizeof(code));
    snprintf(expected, sizeof(expected),
             "kdm part\n"
             "who %s\n"
             "# a new comment\n"
             "# operator\tnodes\tchain\tcost\truns\n"
             "add\t1\tadd(w0, w1)\t2/1\t3\n"
             "sub\t1\tsub(w0, w1)\t-\t0\n"
             "who -\n" KDM_CHECK_TAIL,
             code);
    text = joined(&s_file, 0u);
    check_that((text != NULL) && (strcmp(text, expected) == 0),
               "and the who line before the tail is kept as the tail's own, not written twice");
    free(text);

    const char *const comment[1] = {"# the pipes"};
    const char *const pipes[1] = {"pipe IADD3 IADD3 8 1"};
    const QryWho piped = QRY_WHO_HERE;
    check_that(kdm_pipes_replaced_by(&s_file, comment, 1u, pipes, 1u, piped), "the pipes are replaced");
    qry_who_code(piped, code, sizeof(code));
    snprintf(expected, sizeof(expected),
             ")__krs__\n"
             "who %s\n"
             "\n"
             "# the pipes\n"
             "pipe IADD3 IADD3 8 1\n",
             code);
    text = joined(&s_file, s_file.lines - 5u);
    check_that((text != NULL) && (strcmp(text, expected) == 0) && (s_file.lines == 18u),
               "the pipes and their comment let go and put again at the end, after a who line of the code that put "
               "them");
    free(text);
    unsigned int pipe_lines = 0u;
    for (unsigned int at = 0u; at < s_file.lines; at += 1u)
    {
        pipe_lines += (strncmp(s_file.line[at], "pipe ", 5u) == 0) ? 1u : 0u;
    }
    check_that(pipe_lines == 1u, "and the pipe before them is let go");
    check_that(kdm_write(&s_file, path) == 1, "the .kdm is written through the run's writer");
    check_that(kdm_read(&s_file, path) && (s_file.lines == 18u), "and read back, line for line");
    kdm_free(&s_file);

    // a file written whole again: each line it held keeps its who, and a line it did not goes under the writer's
    const char *const held[4] = {"kdm part", "# a comment", "add\t1\tadd(w0, w1)\t-\t0", "sub\t1\tsub(w0, w1)\t-\t0"};
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        ktext_line_add(&s_file, held[at]);
    }
    const QryWho whole = QRY_WHO_HERE;
    check_that(ktext_lines_written_by(&s_file, 1u, held, 4u, whole) && (s_file.lines == 4u),
               "a file written again as it was is the same lines, no who line put");
    const char *const changed[4] = {"kdm part", "# a comment", "add\t1\tadd(w0, w1)\t2/1\t3",
                                    "sub\t1\tsub(w0, w1)\t-\t0"};
    check_that(ktext_lines_written_by(&s_file, 1u, changed, 4u, whole), "a file is written again with a line changed");
    qry_who_code(whole, code, sizeof(code));
    snprintf(expected, sizeof(expected),
             "kdm part\n"
             "# a comment\n"
             "who %s\n"
             "add\t1\tadd(w0, w1)\t2/1\t3\n"
             "who -\n"
             "sub\t1\tsub(w0, w1)\t-\t0\n",
             code);
    text = joined(&s_file, 0u);
    check_that((text != NULL) && (strcmp(text, expected) == 0),
               "the changed line goes under a who line of the writer's, and the line after it keeps none, as before");
    free(text);
    const QryWho later_whole = QRY_WHO_HERE;
    check_that(ktext_lines_written_by(&s_file, 1u, changed, 4u, later_whole), "and is written again by another");
    text = joined(&s_file, 0u);
    check_that((text != NULL) && (strcmp(text, expected) == 0),
               "the line it held keeps the who that wrote it, not the later writer's");
    free(text);
    kdm_free(&s_file);

    // a file the run writes is the run's while the run runs: read as the run holds it, not on the disk, and in its
    // place once the run ends, which chain_check.sh looks for. Its text names this run's .qry, which no other run's
    // file names
    char put_path[1024];
    snprintf(put_path, sizeof(put_path), "%s/kdm_check_put.kdm", words[1]);
    char put_text[1200];
    snprintf(put_text, sizeof(put_text), "kdm put\n# %s\n", qry_path(qry_run()));
    check_that(qry_filed(qry_run(), put_path, put_text, strlen(put_text)) != 0ull, "a file is handed to the run");
    unsigned char *put_held = NULL;
    unsigned long long put_size = 0ull;
    check_that(qry_file_read(qry_run(), put_path, &put_held, &put_size) && (put_size == strlen(put_text)) &&
                   (memcmp(put_held, put_text, (size_t)put_size) == 0),
               "and read back as the run holds it");
    free(put_held);
    put_held = NULL;
    put_size = 0ull;
    const int on_disk = qry_file_read(NULL, put_path, &put_held, &put_size) && (put_size == strlen(put_text)) &&
                        (memcmp(put_held, put_text, (size_t)put_size) == 0);
    free(put_held);
    check_that(!on_disk, "and not on the disk while the run runs");

    if (count >= 3)
    {
        check_that(kdm_read(&s_file, words[2]) && (s_file.lines != 0u), "a part's .kdm reads");
        // read once from the disk, the file is the run's: every read of it after is from memory
        char part_name[QRY_NAME_LONGEST + 1u];
        const unsigned char *part_held = NULL;
        unsigned long long part_size = 0ull;
        check_that(qry_file_named(words[2], part_name, sizeof(part_name)) &&
                       qry_latest(qry_run(), part_name, &part_held, &part_size, NULL) && (part_size != 0ull),
                   "and once read from the disk, the run holds it, and reads it from memory after");
        char *const before = joined(&s_file, 0u);
        const unsigned int lines = s_file.lines;
        const unsigned int tail = kdm_tail_at(&s_file);
        const char **const held = (const char **)calloc((size_t)tail + 1u, sizeof(char *));
        unsigned int section_lines = 0u;
        for (unsigned int at = 1u; (held != NULL) && (at < tail); at += 1u)
        {
            held[section_lines] = s_file.line[at];
            section_lines += 1u;
        }
        // the section's lines stand in the file until it is written again: they are copied first
        QryText copied = {0};
        for (unsigned int at = 0u; at < section_lines; at += 1u)
        {
            qry_text_printed(&copied, "%s\n", held[at]);
        }
        const char **cut = NULL;
        unsigned int cuts = 0u;
        const QryWho part = QRY_WHO_HERE;
        const int written = ktext_lines_cut(&copied, &cut, &cuts) && (cuts == section_lines) &&
                            kdm_section_written_by(&s_file, "", cut, cuts, part);
        qry_who_code(part, code, sizeof(code));
        char *const after = joined(&s_file, 0u);
        char head[64];
        snprintf(head, sizeof(head), "who %s\n", code);
        const size_t first_line = (before != NULL) ? (strcspn(before, "\n") + 1u) : 0u;
        const int same = written && (after != NULL) && (before != NULL) && (strncmp(after, before, first_line) == 0) &&
                         (strncmp(after + first_line, head, strlen(head)) == 0) &&
                         (strcmp(after + first_line + strlen(head), before + first_line) == 0);
        printf("  %s: %u lines, a section of %u written again, %s\n", words[2], lines, section_lines,
               same ? "as it was with a who line after its first" : "DIFFERING");
        check_that(same, "a part's section written again as it holds it leaves it as it was, a who line after its "
                         "first");
        free(cut);
        qry_text_freed(&copied);
        free(held);
        free(before);
        free(after);
        kdm_free(&s_file);
    }

    printf("kdm_check: %u checks, %u failed\n", s_checks, s_failed);
    return (s_failed == 0u) ? 0 : 1;
}
