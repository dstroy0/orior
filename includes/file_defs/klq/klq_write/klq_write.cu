// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// klq_write.cu: writes Lstar.klq, the bridge between languages, and each given language's .klm, its names keyed to
// the bridge
//
//   orior_klq_write <folder> <ruleset>...
//
// The writer reads each ruleset as its code generator reads it. It checks its files against each other and its schema
// (ruleset_core_relation.h).
//
// The keys of the bridge are the form names of the schema, which belong to gnascor and to no target. At each key the
// bridge stores the pairs whose sameness the text leaves open:
//   - names one word apart, with texts that are the same but for their modifiers.
//   - a form that opens the literal operands of another.
//
// A pair put to no case is open, with its count of cases alike at 0.
//
// The .klm of a language keys each name that it gives and that the schema names. It names each relation of the map
// from the language to the bridge on its own line:
//   - a name that is one text with another in the language, which the bridge holds as two keys (breaks).
//   - a name given as a type other than a form (type).
//
// No name of a language collapses onto the bridge, which witnesses no two names as one text. A name that the schema
// does not name has no key, and is open.
//
// The identities of the bridge between texts and between slices with their verdicts, its primitives and its chains,
// belong to no ruleset. The writer stores each line as the bridge recorded it, and writes it after the keys.
// A pair that the bridge recorded a verdict for, put to the part, goes out with that verdict. It also goes out with the
// set, the concept and the intent that orior_klq_decoder read it into
#include "../../../../src/platform/platform.h"
#include "../../krs/ruleset_flat.h"
#include "../../qry/writer/qry_member.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "code_generator.h"
#include "memoria_operor/memoria_operor.h"
#include "target_internal.h"
#include "verba_scribo/verba_scribo.h"

#include <stddef.h>

// a language's files read and checked against each other and its schema
struct LanguageSet
{
    RulesetRelationMemory memory;
    RulesetCoreRelations relations;
};

// the languages a run reads at max
#define KLQ_LANGUAGES_MAX 32u

// the bytes of a pair's line, its terminator among them
#define KLQ_LINE_BYTES 2048u

// The languages of the run: each schema, 1 where the run read its files, and the set it read them into. The calls of
// this module share them, and the sets go back as the run ends
static const RulesetSchema *s_schemas[KLQ_LANGUAGES_MAX];
static int s_read[KLQ_LANGUAGES_MAX];
static LanguageSet s_sets[KLQ_LANGUAGES_MAX];

// each type of entry that gives a name, as its line's first word writes it
static const char *const s_type_written[] = {"form", "nop", "err", "construct"};

// the map of the language and the bridge. The code puts each together here, and then passes it to the writer of the run
// as its file. Then the bytes that each holds so far, or the room once a write did not fit
static char s_map_bytes[QRY_WRITER_RING_BYTES / 2ULL];
static size_t s_map_at;
static char s_bridge_bytes[QRY_WRITER_RING_BYTES / 2ULL];
static size_t s_bridge_at;

// a name or a line as the bytes that hold it and their count, which no terminator ends
struct KlqName
{
    const char *text;
    size_t length;
};

// the terminated `text` as a name
static KlqName klq_name_of(const char *text)
{
    const KlqName name = {text,
                          EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR)};
    return name;
}

// 1 where two names hold the same bytes
static int klq_names_same(KlqName left, KlqName right)
{
    return (left.length == right.length) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = left.text, .other = right.text, .bytes = left.length) == 0);
}

// 1 where `line` begins with the terminated `prefix`
static int klq_begins(KlqName line, const char *prefix)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = prefix, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    return (line.length >= length) &&
           (EMBED_CALL(memor.cmp, MemoriaCfg, .src = line.text, .other = prefix, .bytes = length) == 0);
}

// `length` bytes of `text` written after what the text at `bytes` contains, `*at` of them so far
static void klq_text_put(char *bytes, size_t room, size_t *at, const char *text, size_t length)
{
    if (length != 0u)
    {
        const VerbaTextusCfg written = {bytes, room, *at, text, length};
        *at = verba_textus.put(&written);
    }
}

// `name` written after what the map contains
static void map_name(KlqName name)
{
    klq_text_put(s_map_bytes, sizeof(s_map_bytes), &s_map_at, name.text, name.length);
}

// `text` written after what the map contains
static void map_text(const char *text)
{
    map_name(klq_name_of(text));
}

// `name` written after what the bridge contains
static void bridge_name(KlqName name)
{
    klq_text_put(s_bridge_bytes, sizeof(s_bridge_bytes), &s_bridge_at, name.text, name.length);
}

// `text` written after what the bridge contains
static void bridge_text(const char *text)
{
    bridge_name(klq_name_of(text));
}

// 1 where every write after `at` bytes of a text of `room` fit
static int text_fits(size_t room, size_t at)
{
    const VerbaFinisCfg written = {NULL, room, at};
    return verba_finis.ok(&written) != 0;
}

// the name of entry `at` of `set`
static KlqName entry_name(const LanguageSet *set, unsigned int at)
{
    const RulesetCoreEntry *const entry = &set->relations.entries[at];
    const KlqName name = {(const char *)&set->relations.text[entry->name.first], entry->name.length};
    return name;
}

// the place of `name` among the first `count` forms of `schema`, or `count` where it is none of them
static unsigned int klq_form_found(const RulesetSchema *schema, unsigned int count, KlqName name)
{
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if (klq_names_same(klq_name_of(schema->forms[at].text), name))
        {
            return at;
        }
    }
    return count;
}

// 1 where entry `at` of `set` is the first to give its name, and the schema `schema` names it as a form
static int entry_keyed(const LanguageSet *set, const RulesetSchema *schema, unsigned int at)
{
    const RulesetCoreEntry *const entry = &set->relations.entries[at];
    return ruleset_core_relation_gives(entry->type) &&
           (ruleset_core_relation_giver(&set->relations, entry->name) == at) &&
           (klq_form_found(schema, schema->form_count, entry_name(set, at)) < schema->form_count);
}

// a note on stdout: `head`, then `name`, then `tail`
static void klq_noted(const char *head, KlqName name, const char *tail)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, head);
    (void)mmgr_line_put_n(output_line, name.text, name.length);
    mmgr_line_put(output_line, tail);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

// the ruleset of `schema` read into `set` the way its code generator reads it. 1 where the read of its files succeeded
static int language_read(const RulesetSchema *schema, LanguageSet *set)
{
    char folder[PLATFORM_PATH_BYTES];
    char path[PLATFORM_PATH_BYTES];
    VerbaLinea path_line = mmgr_line_over(path, sizeof(path));
    if (ruleset_folder(folder, sizeof(folder)) != 0)
    {
        mmgr_line_put(&path_line, folder);
        mmgr_line_ch(&path_line, '/');
        mmgr_line_put(&path_line, schema->file);
    }
    (void)mmgr_line_finish(&path_line);
    char paths[RULESET_PATHS_MAX][PLATFORM_PATH_BYTES];
    const unsigned int count = ruleset_paths(schema, path, paths);
    RulesetFlat flat;
    ruleset_flat_schema(schema, &flat);
    char why[RULESET_ERROR_BYTES];
    VerbaLinea error = mmgr_line_over(why, sizeof(why));
    // the read of the relations reads the flat schema, and nothing reads it after
    const int read = ruleset_relations_read(paths, count, &flat.schema, &set->memory, &set->relations, &error);
    (void)mmgr_line_finish(&error);
    ruleset_flat_ended(&flat);
    if (read == 0)
    {
        klq_noted("  ", klq_name_of(why), " could not be opened");
    }
    return read;
}

// the language `stem` read into `set` against `schema`, its map to the bridge written to `path`. 1 where the write
// succeeded
static int map_write(const char *path, KlqName stem, const LanguageSet *set, const RulesetSchema *schema)
{
    s_map_at = 0u;
    map_text("klm ");
    map_name(stem);
    map_text("\n");
    unsigned int keys = 0u;
    unsigned int relations = 0u;
    for (unsigned int at = 0u; at < set->relations.entry_count; at += 1u)
    {
        if (!entry_keyed(set, schema, at))
        {
            continue;
        }
        const KlqName name = entry_name(set, at);
        map_text("key ");
        map_name(name);
        map_text(" ");
        map_name(name);
        map_text("\n");
        keys += 1u;
        const unsigned int type = set->relations.entries[at].type;
        if (type != RULESET_CORE_ENTRY_FORM)
        {
            map_text("type ");
            map_name(name);
            map_text(" ");
            map_text(s_type_written[type]);
            map_text("\n");
            relations += 1u;
        }
        for (unsigned int earlier = 0u; earlier < at; earlier += 1u)
        {
            if (entry_keyed(set, schema, earlier) && ruleset_core_map_same(&set->relations, at, earlier))
            {
                map_text("breaks ");
                map_name(name);
                map_text(" ");
                map_name(entry_name(set, earlier));
                map_text("\n");
                relations += 1u;
            }
        }
    }
    if (!text_fits(sizeof(s_map_bytes), s_map_at) || !qry_member_filed(path, s_map_bytes, s_map_at))
    {
        klq_noted("  ", klq_name_of(path), " could not be written");
        return 0;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, path);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, keys, 1u);
        mmgr_line_put(output_line, " keys, ");
        mmgr_line_decimal(output_line, relations, 1u);
        mmgr_line_put(output_line, " relations of the map witnessed");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 1;
}

// a pair the bridge keeps: its two names, entries `entry` and `other` of the set of language `set`
struct KlqPair
{
    unsigned int set;
    unsigned int entry;
    unsigned int other;
};

// the pair's two names
static KlqName pair_first(const KlqPair *pair)
{
    return entry_name(&s_sets[pair->set], pair->entry);
}

static KlqName pair_second(const KlqPair *pair)
{
    return entry_name(&s_sets[pair->set], pair->other);
}

// the pair `added` stored after the `*count` of `pairs`, where they do not hold its names yet
static void pair_add(KlqPair *pairs, unsigned int *count, const KlqPair *added)
{
    for (unsigned int at = 0u; at < *count; at += 1u)
    {
        if (klq_names_same(pair_first(&pairs[at]), pair_first(added)) &&
            klq_names_same(pair_second(&pairs[at]), pair_second(added)))
        {
            return;
        }
    }
    pairs[*count] = *added;
    *count += 1u;
}

// the types of a line of the bridge that the writer keeps. A line is a pair's verdict, one of its sets, its concept or
// its intent, or a line that no ruleset writes
#define KLQ_LINE_VERDICT 0u
#define KLQ_LINE_SET 1u
#define KLQ_LINE_CONCEPT 2u
#define KLQ_LINE_INTENT 3u
#define KLQ_LINE_IDENTITY 4u

// a line of the bridge that the writer keeps: the line, the pair line it is beneath where it is a pair's, and its type
struct KlqLine
{
    KlqName line;
    KlqName pair;
    unsigned int type;
};

// The lines of the bridge as the run stored it, read into `lines` with their count into `count`. `lines` holds room
// for one a line of `bytes`. A pair's verdict is the first beneath it, and its concept and its intent the last. The
// other lines are the bridge's identities between texts and between slices with their verdicts, its primitives and
// its chains. A line of these is not the first of the bridge, a key, a pair or a line beneath a pair. The writer
// writes each as the read found it
static void bridge_lines_read(const char *bytes, size_t size, KlqLine *lines, unsigned int *count)
{
    static const char s_set_suffix[] = "_coherence";
    const size_t suffix = sizeof(s_set_suffix) - 1u;
    KlqName pair_key = {NULL, 0u};
    int first = 1;
    *count = 0u;
    for (size_t at = 0u; at < size;)
    {
        size_t end = at;
        while ((end < size) && (bytes[end] != '\n'))
        {
            end += 1u;
        }
        KlqName line = {&bytes[at], end - at};
        at = end + 1u;
        line.length -= ((line.length != 0u) && (line.text[line.length - 1u] == '\r')) ? 1u : 0u;
        const int verdict = klq_begins(line, "open ") || klq_begins(line, "closed ");
        const int set =
            (EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = line.text, .cap = line.length, .byte = ' ') == NULL) &&
            (line.length > suffix) &&
            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = &line.text[line.length - suffix], .other = s_set_suffix,
                        .bytes = suffix) == 0);
        const int concept_line = klq_begins(line, "concept_coherence ");
        const int intent = klq_begins(line, "intent_coherence ");
        const int beneath_pair = (pair_key.length != 0u) && (verdict || set || concept_line || intent);
        if (beneath_pair)
        {
            const unsigned int type =
                verdict ? KLQ_LINE_VERDICT : (set ? KLQ_LINE_SET : (concept_line ? KLQ_LINE_CONCEPT : KLQ_LINE_INTENT));
            lines[*count] = {line, pair_key, type};
            *count += 1u;
        }
        const int ruleset_line = first || klq_begins(line, "key ") || klq_begins(line, "pair ");
        if (klq_begins(line, "pair "))
        {
            pair_key = line;
        }
        else if (!beneath_pair)
        {
            pair_key.length = 0u;
        }
        if (!beneath_pair && !ruleset_line && (line.length != 0u))
        {
            lines[*count] = {line, {NULL, 0u}, KLQ_LINE_IDENTITY};
            *count += 1u;
        }
        first = 0;
    }
}

// the lines of the bridge beneath `pair` written again: its verdict, or open 0 where it has none, its sets, its concept
// and its intent
static void bridge_pair_written(KlqName pair, const KlqLine *lines, unsigned int count)
{
    const KlqLine *verdict = NULL;
    const KlqLine *concept_line = NULL;
    const KlqLine *intent = NULL;
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        const KlqLine *const line = &lines[at];
        const int same = (line->type != KLQ_LINE_IDENTITY) && klq_names_same(line->pair, pair);
        verdict = ((verdict == NULL) && same && (line->type == KLQ_LINE_VERDICT)) ? line : verdict;
        concept_line = (same && (line->type == KLQ_LINE_CONCEPT)) ? line : concept_line;
        intent = (same && (line->type == KLQ_LINE_INTENT)) ? line : intent;
    }
    if (verdict != NULL)
    {
        bridge_name(verdict->line);
    }
    else
    {
        bridge_text("open 0");
    }
    bridge_text("\n");
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        if ((lines[at].type == KLQ_LINE_SET) && klq_names_same(lines[at].pair, pair))
        {
            bridge_name(lines[at].line);
            bridge_text("\n");
        }
    }
    const KlqLine *const last[2] = {concept_line, intent};
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        if (last[at] != NULL)
        {
            bridge_name(last[at]->line);
            bridge_text("\n");
        }
    }
}

// a path of `folder`, then `name`, then `tail` into `path`
static void klq_path(const char *folder, KlqName name, const char *tail, char *path)
{
    VerbaLinea line = mmgr_line_over(path, PLATFORM_PATH_BYTES);
    mmgr_line_put(&line, folder);
    mmgr_line_ch(&line, '/');
    (void)mmgr_line_put_n(&line, name.text, name.length);
    mmgr_line_put(&line, tail);
    (void)mmgr_line_finish(&line);
}

int program_main(int argc, char **argv)
{
    if ((argc < 3) || ((unsigned int)(argc - 2) > KLQ_LANGUAGES_MAX))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "orior_klq_write <folder> <ruleset>..., at max 32 rulesets");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    const char *const folder = argv[1];
    // the count of rulesets is at max KLQ_LANGUAGES_MAX
    const unsigned int languages = (unsigned int)(argc - 2);
    // the keys of the bridge, the schemas' form names, a cell of the run of every form of every schema
    unsigned long long form_total = 0ULL;
    for (unsigned int language = 0u; language < languages; language += 1u)
    {
        s_schemas[language] = code_generator(argv[2u + language]).schema();
        form_total += s_schemas[language]->form_count;
    }
    const char **const keys = (const char **)platform_cell_cleared(
        (form_total != 0ULL) ? form_total : 1ULL, sizeof(const char *), platform_place_g_program_owner, 0);
    if (keys == NULL)
    {
        return 1;
    }
    unsigned int key_count = 0u;
    unsigned long long relation_total = 0ULL;
    int failed = 0;
    for (unsigned int language = 0u; language < languages; language += 1u)
    {
        const RulesetSchema *const schema = s_schemas[language];
        for (unsigned int at = 0u; at < schema->form_count; at += 1u)
        {
            if (klq_form_found(schema, at, klq_name_of(schema->forms[at].text)) == at)
            {
                keys[key_count] = schema->forms[at].text;
                key_count += 1u;
            }
        }
        s_read[language] = language_read(schema, &s_sets[language]);
        if (s_read[language] == 0)
        {
            failed = 1;
            continue;
        }
        const char *const file = argv[2u + language];
        const char *const dot =
            EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = file, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '.');
        // the stem ends at the last dot, which is at or past the file's first byte
        const KlqName stem = {file, (dot != NULL) ? (size_t)(dot - file)
                                                  : EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = file,
                                                               .cap = PLATFORM_TEXT_TO_TERMINATOR)};
        char map_path[PLATFORM_PATH_BYTES];
        klq_path(folder, stem, ".klm", map_path);
        failed |= (map_write(map_path, stem, &s_sets[language], schema) == 0) ? 1 : 0;
        relation_total += s_sets[language].relations.relation_count;
    }
    // the pairs whose sameness the text leaves open, a cell of the run of every relation of every language
    KlqPair *const pairs = (KlqPair *)platform_cell_cleared((relation_total != 0ULL) ? relation_total : 1ULL,
                                                            sizeof(KlqPair), platform_place_g_program_owner, 0);
    unsigned int pair_count = 0u;
    unsigned int unkeyed = 0u;
    for (unsigned int language = 0u; (pairs != NULL) && (language < languages); language += 1u)
    {
        if (s_read[language] == 0)
        {
            continue;
        }
        const LanguageSet *const set = &s_sets[language];
        const RulesetCoreRelations *const relations = &set->relations;
        const unsigned int relation_count = (relations->relation_count < relations->relation_capacity)
                                                ? relations->relation_count
                                                : relations->relation_capacity;
        for (unsigned int at = 0u; at < relation_count; at += 1u)
        {
            const RulesetCoreRelation *const relation = &relations->relations[at];
            if ((relation->type != RULESET_CORE_RELATION_MODIFIERS) && (relation->type != RULESET_CORE_RELATION_OPENED))
            {
                continue;
            }
            if (!entry_keyed(set, s_schemas[language], relation->entry) ||
                !entry_keyed(set, s_schemas[language], relation->other))
            {
                unkeyed += 1u;
                continue;
            }
            const KlqPair added = {language, relation->entry, relation->other};
            pair_add(pairs, &pair_count, &added);
        }
    }
    char bridge_path[PLATFORM_PATH_BYTES];
    klq_path(folder, klq_name_of("Lstar"), ".klq", bridge_path);
    // the bridge as the run stores it (qry_member_file), empty on a first run, and a line kept of each of its lines
    const unsigned char *bridge_bytes = NULL;
    unsigned long long bridge_size = 0ULL;
    const int bridge_found = qry_member_file_blob(bridge_path, &bridge_bytes, &bridge_size);
    // a file read whole is at max what a size_t counts, and a line of it is at least its newline
    const size_t bridge_length = (bridge_found != 0) ? (size_t)bridge_size : 0u;
    KlqLine *const lines = (KlqLine *)platform_cell_cleared((unsigned long long)bridge_length + 1ULL, sizeof(KlqLine),
                                                            platform_place_g_program_owner, 0);
    unsigned int line_count = 0u;
    if (lines != NULL)
    {
        // Explicit cast reads the file's bytes as the text that they are
        bridge_lines_read((const char *)bridge_bytes, bridge_length, lines, &line_count);
    }
    s_bridge_at = 0u;
    bridge_text("klq L*\n");
    unsigned int written = 0u;
    for (unsigned int at = 0u; at < key_count; at += 1u)
    {
        const KlqName key = klq_name_of(keys[at]);
        int earlier = 0;
        for (unsigned int before = 0u; before < at; before += 1u)
        {
            earlier |= klq_names_same(klq_name_of(keys[before]), key) ? 1 : 0;
        }
        if (earlier != 0)
        {
            continue;
        }
        bridge_text("key ");
        bridge_name(key);
        bridge_text("\n");
        written += 1u;
        for (unsigned int pair = 0u; pair < pair_count; pair += 1u)
        {
            if (!klq_names_same(pair_first(&pairs[pair]), key))
            {
                continue;
            }
            char pair_text[KLQ_LINE_BYTES];
            VerbaLinea pair_line = mmgr_line_over(pair_text, sizeof(pair_text));
            mmgr_line_put(&pair_line, "pair ");
            (void)mmgr_line_put_n(&pair_line, key.text, key.length);
            mmgr_line_ch(&pair_line, ' ');
            const KlqName second = pair_second(&pairs[pair]);
            (void)mmgr_line_put_n(&pair_line, second.text, second.length);
            const KlqName pair_name = {pair_text, mmgr_line_finish(&pair_line)};
            bridge_name(pair_name);
            bridge_text("\n");
            bridge_pair_written(pair_name, lines, line_count);
        }
    }
    unsigned int identities = 0u;
    for (unsigned int at = 0u; at < line_count; at += 1u)
    {
        if (lines[at].type == KLQ_LINE_IDENTITY)
        {
            bridge_name(lines[at].line);
            bridge_text("\n");
            identities += 1u;
        }
    }
    const int filed =
        text_fits(sizeof(s_bridge_bytes), s_bridge_at) && qry_member_filed(bridge_path, s_bridge_bytes, s_bridge_at);
    if (filed == 0)
    {
        klq_noted("  ", klq_name_of(bridge_path), " could not be written");
    }
    else
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, bridge_path);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, written, 1u);
        mmgr_line_put(output_line, " keys, ");
        mmgr_line_decimal(output_line, pair_count, 1u);
        mmgr_line_put(output_line, " pairs open, ");
        mmgr_line_decimal(output_line, unkeyed, 1u);
        mmgr_line_put(output_line, " pairs of names the schema does not name, ");
        mmgr_line_decimal(output_line, identities, 1u);
        mmgr_line_put(output_line, " lines no ruleset writes retained");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // the run's cells go back in the reverse order of their takes
    platform_cell_released(lines, platform_place_g_program_owner);
    platform_cell_released(pairs, platform_place_g_program_owner);
    for (unsigned int language = languages; language > 0u; language -= 1u)
    {
        if (s_read[language - 1u] != 0)
        {
            ruleset_relation_memory_ended(&s_sets[language - 1u].memory);
        }
    }
    platform_cell_released(keys, platform_place_g_program_owner);
    return (filed == 0) ? 1 : failed;
}
