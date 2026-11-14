// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// words.c: the words: a form's parameters named, a word's answers and the word written, a text named, and the folder of
// a path
#include "../../../../src/cu/engine/rmc/precept_value.h"
#include "../../../parsers/line_read.h"
#include "krs_write_internal.h"

// the parameters the ruleset at `path` names `name` with on its line `form`, `construct`, `nop` or `err`, into
// `parameter`. The count, 0 where it names none of them
unsigned int krs_parameters_named(const char *path, const char *name, char parameter[][KRS_NAME])
{
    const unsigned char *file_bytes = NULL;
    unsigned long long file_size = 0ull;
    unsigned long long file_at = 0ull;
    if (!qry_member_file_blob(path, &file_bytes, &file_size))
    {
        return 0u;
    }
    static const char *const s_naming[] = {"form", "construct", "nop", "err"};
    char line[4096];
    unsigned int count = 0u;
    while ((count == 0u) && (line_read(file_bytes, file_size, &file_at, line, sizeof(line))))
    {
        char type[KRS_NAME];
        char named[KRS_NAME];
        const char *rest = line;
        if (!line_word(&rest, type, sizeof(type)) || !line_word(&rest, named, sizeof(named)))
        {
            continue;
        }
        int naming = 0;
        for (unsigned int each = 0u; each < (sizeof(s_naming) / sizeof(s_naming[0])); each += 1u)
        {
            naming = naming || (strcmp(type, s_naming[each]) == 0);
        }
        if (!naming || (strcmp(named, name) != 0))
        {
            continue;
        }
        char word[KRS_NAME];
        while ((count < KRS_PARAMETERS) && line_word(&rest, word, sizeof(word)) && (strcmp(word, "=") != 0))
        {
            memcpy(parameter[count], word, strlen(word) + 1u);
            count += 1u;
        }
    }
    return count;
}

// 1 where `word`, its leaf k reading the relation's word order[k], answers `anchor` on every case of it the ladder
// satisfies, the relation read signed where `signed_read` specifies, and the ladder satisfies at least one
static int krs_word_answers(const Word *word, unsigned int anchor, int signed_read, const unsigned int *order)
{
    unsigned int cases = 0u;
    for (unsigned int at = 0u; at < LADDER_CASE_COUNT; at += 1u)
    {
        const LadderQuestion *const ladder_case = &s_ladder_cases[at];
        unsigned int answer = 0u;
        if ((ladder_case->anchor != anchor) || (ladder_case->words != (unsigned int)word->reads) ||
            !ladder_answer(anchor, ladder_case->word, ladder_case->words, &answer))
        {
            continue;
        }
        answer = (signed_read != 0) ? ladder_signed_answer(anchor, ladder_case->word) : answer;
        PreceptCase given;
        memset(&given, 0, sizeof(given));
        given.operands = word->reads;
        for (unsigned int leaf = 0u; leaf < word->reads; leaf += 1u)
        {
            given.operand[leaf] = ladder_case->word[order[leaf]];
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
// 0 where the word reads another count of words than the relation has or no order answers
int krs_word_written(const Word *word, const KrsForm *form, unsigned int *order)
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

// The text of `form` with each place named, into `written`, which has room for `room`: the answer's place as the first
// parameter and the relation's word order[k] as parameter k + 1, each as {<name>}. 1, or 0 where the text contains a
// place no parameter names, leaves a place the word reads unwritten, or does not fit
int krs_text_named(const KrsForm *form, const unsigned int *order, char parameter[][KRS_NAME], unsigned int parameters,
                   char *written, size_t room)
{
    const unsigned int words = s_ladder_words[form->anchor];
    unsigned int found = 0u;
    size_t at = 0u;
    for (const char *walk = form->text; *walk != '\0';)
    {
        unsigned int place = 0u;
        int length = 0;
        if (krs_place_read(walk, &place, &length))
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

// the folder `path` lies in, into `folder`, which has room for `room`: "." where it names none
void krs_folder_of(const char *path, char *folder, size_t room)
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

int krs_place_read(const char *walk, unsigned int *place, int *length)
{
    unsigned long long read = 0ull;
    const char *inside = walk + 1;
    if ((*walk != '{') || !line_number(&inside, 0, &read) || (*inside != '}') || (read > 0xffffffffull))
    {
        return 0;
    }
    // a place an unsigned int represents, and the braces around it, which an int counts
    *place = (unsigned int)read;
    *length = (int)(inside + 1 - walk);
    return 1;
}
