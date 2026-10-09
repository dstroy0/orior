// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// krs_write.c: a language's ruleset and its part's, written from the forms the part answered for
//
//     krs_write <language> <part> <path.khw> <layout> <mnemonics> <folder> <machine writer> <named ruleset>
//
// Every form in the part's file this writes is there because the part answered for it (P8), and no name is matched
// to an operation. The vendor's writer (`machine writer` above, run through the interface) reads the part's machine
// file and hands back each form that answered a relation: the relation, whether the answers read a word signed, and
// the form's text with each operand that stands in the relation's tuple written as its place. Which word a form writes
// is read off the word web (word_web.h): a word whose tree, each leaf reading one of the relation's words, answers the
// relation on every case of it the ladder holds (ladder.h, precept_value.h) is a word the form writes. A word's form
// takes the destination first and then the leaves by place: the answer's place is the form's first parameter, and the
// word a leaf reads is the parameter of that leaf.
//
// The parameters are named as the ruleset at `named ruleset` names the same form; a word that ruleset names no form
// of, or names with another count of parameters, is written by none here. A word one form writes is written with that
// form. A word more than one form writes is written by none: which of them is the word's is the cost's to say (P6), and
// no cost is read here.
//
// Two files are written beside the machine file: <language>.krs, which opens a ruleset of the language and names the
// part, and <part>.krs, which the reader reads after it (ruleset_flat.cu) and which holds the forms. A bank, a fixed
// register, the toolchain and the header are answered by no form, and neither file gives one.
#include "../../../engine/rmc/precept_value.h"
#include "../../../engine/rmc/word_web.h"
#include "../interface/interface.h"
#include "../protocol/counterexample/ladder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// the most forms the vendor's writer hands back, the longest line read, the most parameters a ruleset names a form
// with, and the longest parameter name
#define KRS_FORMS 16384u
#define KRS_LINE 512u
#define KRS_PARAMETERS 32u
#define KRS_NAME 64u

// the words of the web
#define KRS_WORDS ((unsigned int)(sizeof(s_word_web) / sizeof(s_word_web[0])))

// the name a form's relation is handed back under, as khw_write writes it
#define KRS_READS_LADDER "ladder."

#define LADDER_TEXT(name_, text_, words_, measured_) text_,
static const char *const s_anchor_text[] = {LADDER_ANCHORS(LADDER_TEXT)};
#undef LADDER_TEXT

// one form the part answered for, as the vendor's writer hands it back: the relation's anchor, whether the answers
// read a word signed, and its text with each operand of the relation's tuple written as {<place>}
typedef struct
{
    unsigned int anchor;
    int signed_read;
    char text[KRS_LINE];
} KrsForm;

static KrsForm s_form[KRS_FORMS];
static unsigned int s_forms;

// The vendor's writer run through the interface in its krs mode, handed the part, the machine file, the container
// layout, the vendor's table and the file to write. Its output is read and printed. 1 where it ended clean
static int krs_vendor_run(char *const *word, const char *folder, const char *answers)
{
    char output_path[1024];
    snprintf(output_path, sizeof(output_path), "%s/machine.txt", folder);
    char *const command[] = {word[7], word[2], word[3], word[4], word[5], (char *)answers, (char *)"krs", NULL};
    static char s_output[65536];
    const InterfaceProbe probe = {command, output_path, 0ull};
    InterfaceAnswer answer = {0};
    answer.output = s_output;
    answer.output_capacity = sizeof(s_output);
    EngineError error = {0};
    const long ran = interface_probe_run(&probe, &answer, &error);
    if (answer.output_bytes != 0ull)
    {
        printf("%s", s_output);
    }
    if ((ran != 0L) || (answer.ending != INTERFACE_ENDING_EXITED) || (answer.code != 0ull))
    {
        printf("  krs_write: the vendor's writer did not end clean (%s, code %llx)\n", interface_ending_name(answer.ending),
               answer.code);
        return 0;
    }
    return 1;
}

// the forms the vendor's writer handed back in the file at `path`, read into s_form. A line whose relation is none the
// ladder holds is left out. The count read
static unsigned int krs_forms_read(const char *path)
{
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        printf("  krs_write: %s could not be read\n", path);
        return 0u;
    }
    char line[KRS_LINE + 300u];
    s_forms = 0u;
    while ((fgets(line, (int)sizeof(line), file) != NULL) && (s_forms < KRS_FORMS))
    {
        line[strcspn(line, "\r\n")] = '\0';
        char relation[256];
        int signed_read = 0;
        int at = 0;
        if (sscanf(line, "%255s %d %n", relation, &signed_read, &at) != 2)
        {
            continue;
        }
        const size_t prefix = strlen(KRS_READS_LADDER);
        const char *const name = (strncmp(relation, KRS_READS_LADDER, prefix) == 0) ? (relation + prefix) : relation;
        unsigned int anchor = (unsigned int)LADDER_ANCHOR_COUNT;
        for (unsigned int each = 0u; each < (unsigned int)LADDER_ANCHOR_COUNT; each += 1u)
        {
            anchor = (strcmp(s_anchor_text[each], name) == 0) ? each : anchor;
        }
        if ((anchor == (unsigned int)LADDER_ANCHOR_COUNT) || (strlen(line + at) >= KRS_LINE))
        {
            continue;
        }
        s_form[s_forms].anchor = anchor;
        s_form[s_forms].signed_read = signed_read;
        snprintf(s_form[s_forms].text, sizeof(s_form[s_forms].text), "%s", line + at);
        s_forms += 1u;
    }
    fclose(file);
    return s_forms;
}

// the parameters the ruleset at `path` names `name` with on its line `form`, `construct`, `nop` or `err`, into
// `parameter`. The count, 0 where it names none of them
static unsigned int krs_parameters_named(const char *path, const char *name, char parameter[][KRS_NAME])
{
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        return 0u;
    }
    static const char *const s_naming[] = {"form", "construct", "nop", "err"};
    char line[4096];
    unsigned int count = 0u;
    while ((count == 0u) && (fgets(line, (int)sizeof(line), file) != NULL))
    {
        char kind[KRS_NAME];
        char named[KRS_NAME];
        int at = 0;
        if (sscanf(line, "%63s %63s %n", kind, named, &at) != 2)
        {
            continue;
        }
        int naming = 0;
        for (unsigned int each = 0u; each < (sizeof(s_naming) / sizeof(s_naming[0])); each += 1u)
        {
            naming = naming || (strcmp(kind, s_naming[each]) == 0);
        }
        if (!naming || (strcmp(named, name) != 0))
        {
            continue;
        }
        const char *rest = line + at;
        char word[KRS_NAME];
        int step = 0;
        while ((count < KRS_PARAMETERS) && (sscanf(rest, "%63s %n", word, &step) == 1) && (strcmp(word, "=") != 0))
        {
            snprintf(parameter[count], KRS_NAME, "%s", word);
            count += 1u;
            rest += step;
        }
    }
    fclose(file);
    return count;
}

// 1 where `word`, its leaf k reading the relation's word order[k], answers `anchor` on every case of it the ladder
// holds, the relation read signed where `signed_read` says, and the ladder holds at least one
static int krs_word_answers(const Word *word, unsigned int anchor, int signed_read, const unsigned int *order)
{
    unsigned int cases = 0u;
    for (unsigned int at = 0u; at < LADDER_CASE_COUNT; at += 1u)
    {
        const LadderQuestion *const held = &s_ladder_cases[at];
        unsigned int answer = 0u;
        if ((held->anchor != anchor) || (held->words != (unsigned int)word->reads) ||
            !ladder_answer(anchor, held->word, held->words, &answer))
        {
            continue;
        }
        answer = (signed_read != 0) ? ladder_signed_answer(anchor, held->word) : answer;
        PreceptCase given;
        memset(&given, 0, sizeof(given));
        given.operands = word->reads;
        for (unsigned int leaf = 0u; leaf < word->reads; leaf += 1u)
        {
            given.operand[leaf] = held->word[order[leaf]];
        }
        unsigned int value = 0u;
        if (!precept_value(word->node, word->nodes, &given, &value) || (value != answer))
        {
            return 0;
        }
        cases += 1u;
    }
    return (cases != 0u) ? 1 : 0;
}

// The order the leaves of `word` read the relation's words in for `form` to write it, into `order`: the first, counted
// as the orders of the words are counted, under which the word's tree answers the form's relation on every case. 1, or
// 0 where the word reads another count of words than the relation holds or no order answers
static int krs_word_written(const Word *word, const KrsForm *form, unsigned int *order)
{
    const unsigned int words = s_ladder_words[form->anchor];
    if ((s_ladder_measured[form->anchor] != 0) || ((unsigned int)word->reads != words) || (words == 0u) ||
        (words > LADDER_WORDS))
    {
        return 0;
    }
    unsigned int orders = 1u;
    for (unsigned int at = 0u; at < words; at += 1u)
    {
        orders *= words;
    }
    for (unsigned int walk = 0u; walk < orders; walk += 1u)
    {
        // the last leaf the lowest place of the count, so that the relation's own order is the first one found
        unsigned int rest = walk;
        unsigned int seen = 0u;
        for (unsigned int leaf = words; leaf > 0u; leaf -= 1u)
        {
            order[leaf - 1u] = rest % words;
            rest /= words;
            seen |= 1u << order[leaf - 1u];
        }
        if ((seen == ((1u << words) - 1u)) && krs_word_answers(word, form->anchor, form->signed_read, order))
        {
            return 1;
        }
    }
    return 0;
}

// The text of `form` with each place named, into `written`, which holds `room`: the answer's place as the first
// parameter and the relation's word order[k] as parameter k + 1, each as {<name>}. 1, or 0 where the text holds a
// place no parameter names, leaves a place the word reads unwritten, or does not fit
static int krs_text_named(const KrsForm *form, const unsigned int *order, char parameter[][KRS_NAME],
                          unsigned int parameters, char *written, size_t room)
{
    const unsigned int words = s_ladder_words[form->anchor];
    unsigned int found = 0u;
    size_t at = 0u;
    for (const char *walk = form->text; *walk != '\0';)
    {
        unsigned int place = 0u;
        int length = 0;
        if ((*walk == '{') && (sscanf(walk, "{%u}%n", &place, &length) == 1) && (length != 0))
        {
            unsigned int named = KRS_PARAMETERS;
            named = (place == words) ? 0u : named;
            for (unsigned int leaf = 0u; leaf < words; leaf += 1u)
            {
                named = (order[leaf] == place) ? (leaf + 1u) : named;
            }
            if ((named >= parameters) || ((at + strlen(parameter[named]) + 3u) >= room))
            {
                return 0;
            }
            at += (size_t)snprintf(written + at, room - at, "{%s}", parameter[named]);
            found |= 1u << place;
            walk += length;
            continue;
        }
        if ((at + 2u) >= room)
        {
            return 0;
        }
        written[at] = *walk;
        at += 1u;
        written[at] = '\0';
        walk += 1;
    }
    return (found == ((1u << (words + 1u)) - 1u)) ? 1 : 0;
}

// the folder `path` lies in, into `folder`, which holds `room`: "." where it names none
static void krs_folder_of(const char *path, char *folder, size_t room)
{
    const char *const slash = strrchr(path, '/');
    const char *const back = strrchr(path, '\\');
    const char *const last = ((slash != NULL) && ((back == NULL) || (slash > back))) ? slash : back;
    if (last == NULL)
    {
        snprintf(folder, room, ".");
        return;
    }
    snprintf(folder, room, "%.*s", (int)(last - path), path);
}

int main(int count, char **word)
{
    if (count != 9)
    {
        printf("krs_write <language> <part> <path.khw> <layout> <mnemonics> <folder> <machine writer> <named ruleset>\n");
        return 2;
    }
    const char *const language = word[1];
    const char *const part = word[2];
    const char *const machine = word[3];
    const char *const folder = word[6];
    const char *const named_ruleset = word[8];
    char answers[1024];
    snprintf(answers, sizeof(answers), "%s/krs_forms.txt", folder);
    if (!krs_vendor_run(word, folder, answers))
    {
        return 1;
    }
    krs_forms_read(answers);
    char rulesets[1024];
    krs_folder_of(machine, rulesets, sizeof(rulesets));
    char path[1200];
    snprintf(path, sizeof(path), "%s/%s.krs", rulesets, part);
    FILE *const forms = fopen(path, "wb");
    if (forms == NULL)
    {
        printf("  krs_write: %s could not be written\n", path);
        return 1;
    }
    fprintf(forms, "krs 1\n");
    unsigned int given = 0u;
    for (unsigned int at = 0u; at < KRS_WORDS; at += 1u)
    {
        const Word *const each = &s_word_web[at];
        unsigned int writers = 0u;
        unsigned int first = 0u;
        unsigned int order[LADDER_WORDS];
        unsigned int first_order[LADDER_WORDS] = {0u};
        for (unsigned int form = 0u; form < s_forms; form += 1u)
        {
            if (krs_word_written(each, &s_form[form], order))
            {
                first = (writers == 0u) ? form : first;
                if (writers == 0u)
                {
                    memcpy(first_order, order, sizeof(first_order));
                }
                writers += 1u;
            }
        }
        if (writers == 0u)
        {
            continue;
        }
        if (writers > 1u)
        {
            printf("  krs_write: %u forms write %s, which the cost decides (P6), and none is written\n", writers,
                   each->name);
            continue;
        }
        char parameter[KRS_PARAMETERS][KRS_NAME];
        const unsigned int parameters = krs_parameters_named(named_ruleset, each->name, parameter);
        if (parameters != ((unsigned int)each->reads + 1u))
        {
            printf("  krs_write: %s names %s with %u parameters, where the word takes a destination and %u, and it is "
                   "not written\n",
                   named_ruleset, each->name, parameters, (unsigned int)each->reads);
            continue;
        }
        char text[KRS_LINE * 2u];
        if (!krs_text_named(&s_form[first], first_order, parameter, parameters, text, sizeof(text)))
        {
            printf("  krs_write: %s is written by a form whose operands do not carry its words\n", each->name);
            continue;
        }
        fprintf(forms, "form %s", each->name);
        for (unsigned int place = 0u; place < parameters; place += 1u)
        {
            fprintf(forms, " %s", parameter[place]);
        }
        fprintf(forms, " = %s\\n\n", text);
        given += 1u;
    }
    if (fclose(forms) != 0)
    {
        printf("  krs_write: %s could not be written\n", path);
        return 1;
    }
    snprintf(path, sizeof(path), "%s/%s.krs", rulesets, language);
    FILE *const head = fopen(path, "wb");
    if (head == NULL)
    {
        printf("  krs_write: %s could not be written\n", path);
        return 1;
    }
    fprintf(head, "krs 1\nruleset %s\npart %s\n", language, part);
    if (fclose(head) != 0)
    {
        printf("  krs_write: %s could not be written\n", path);
        return 1;
    }
    printf("  krs_write: %u of the %u words of the web written from the %u forms the part answered for, in %s/%s.krs "
           "beside %s.krs\n",
           given, KRS_WORDS, s_forms, rulesets, part, language);
    return 0;
}
