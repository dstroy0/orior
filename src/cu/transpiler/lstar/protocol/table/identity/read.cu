// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// read.cu: the read: the forms of the rulesets, the members and carriers of the stick, and the candidate paths read
// against them
#include "../../../../../../../includes/platform/platform.h"
#include "identity_internal.h"

// the pattern of a character a pattern reads as an operator, which a literal text escapes
static const TablePattern *special_pattern(void)
{
    static TablePatternOnce s_special;
    return table_once(&s_special, "[.^$|()\\[\\]*+?\\\\]", 0);
}

// a form's place among the forms, added after the last: NULL where the pool cannot meet the room
static IdentityForm *form_added(IdentityForms *forms)
{
    IdentityForm form;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &form, .bytes = sizeof(form), .val = 0);
    if (!platform_array_appended(&forms->forms, &form))
    {
        return NULL;
    }
    return (IdentityForm *)platform_array_at(&forms->forms, forms->forms.count - 1ULL);
}

// The form's text as the links it writes, each parameter a pattern any operand matches, into the form's cell of
// patterns. The read takes the cell once, at the count of the text's lines. 1, or 0 where the pool cannot meet it
static int form_links(const char *text, IdentityForm *form)
{
    PlatformText written;
    identity_text_begun(&written);
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    for (size_t at = 0u; at < length; at += 1u)
    {
        if ((text[at] == '\\') && ((at + 1u) < length))
        {
            mmgr_line_ch(&written.line,
                         (text[at + 1u] == 'n') ? '\n' : ((text[at + 1u] == 't') ? '\t' : text[at + 1u]));
            at += 1u;
            continue;
        }
        mmgr_line_ch(&written.line, text[at]);
    }
    const unsigned long long written_size = platform_text_size(&written);
    const CarrierPiece lines = {platform_text_bytes(&written), (size_t)written_size};
    unsigned long long line_count = 1ULL;
    for (size_t at = 0u; (lines.text != NULL) && (at < lines.length); at += 1u)
    {
        line_count += (lines.text[at] == '\n') ? 1ULL : 0ULL;
    }
    form->links = (TablePattern *)platform_cell_cleared(line_count, sizeof(TablePattern),
                                                        platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    form->link_count = 0u;
    if ((form->links == NULL) || (lines.text == NULL))
    {
        platform_text_ended(&written);
        return form->links != NULL;
    }
    static TablePatternOnce s_offset;
    static TablePatternOnce s_parameter;
    PlatformText pattern;
    identity_text_begun(&pattern);
    size_t cut = 0u;
    CarrierPiece line;
    while (carrier_piece_next(lines, '\n', &cut, &line))
    {
        Link link;
        if (link_read(line, &link) != 1)
        {
            continue;
        }
        platform_text_reset(&pattern);
        const char *const type = link.type.text;
        table_replaced(special_pattern(), type,
                       EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = type, .cap = PLATFORM_TEXT_TO_TERMINATOR), "\\$&",
                       &pattern);
        // the form writes no offset added to an address where it is 0: the parameter after a + is there or not
        identity_replaced(&pattern, table_once(&s_offset, "\\\\\\+\\{[a-z_]+\\}", 0), "(\\+[^,\\]]+)?");
        identity_replaced(&pattern, table_once(&s_parameter, "\\{[a-z_]+\\}(\\\\\\.hi)?", 0), "[^,]+");
        PlatformText source;
        identity_text_begun(&source);
        mmgr_line_ch(&source.line, '^');
        identity_text_put(&source, &pattern);
        mmgr_line_ch(&source.line, '$');
        (void)table_pattern_whole(&form->links[form->link_count], platform_text_bytes(&source));
        platform_text_ended(&source);
        form->link_count += 1u;
    }
    platform_text_ended(&pattern);
    platform_text_ended(&written);
    return 1;
}

// `text` with each character a pattern reads as an operator escaped, onto `out`
static void literal_escaped(CarrierPiece text, PlatformText *out)
{
    table_replaced(special_pattern(), text.text, text.length, "\\$&", out);
}

// The pipe rows of a ruleset's file, each a form of one pattern: the operation or its writing on the other pipe.
// 1, or 0 where the pool cannot meet a form
static int pipe_forms_read(CarrierPiece file, IdentityForms *forms)
{
    size_t cut = 0u;
    CarrierPiece line;
    while (carrier_piece_next(file, '\n', &cut, &line))
    {
        size_t word_at = 0u;
        CarrierPiece head;
        CarrierPiece operation;
        CarrierPiece writing;
        if (!identity_word_next(line, &word_at, &head) || !identity_word_next(line, &word_at, &operation) ||
            !identity_word_next(line, &word_at, &writing) || !identity_piece_is(head, "pipe"))
        {
            continue;
        }
        IdentityForm *const form = form_added(forms);
        form->links = (form != NULL) ? (TablePattern *)platform_cell_cleared(1ULL, sizeof(TablePattern),
                                                                             platform_place_g_program_owner,
                                                                             PLATFORM_TIER_PERSISTENT)
                                     : NULL;
        if ((form == NULL) || (form->links == NULL))
        {
            return 0;
        }
        PlatformText source;
        identity_text_begun(&source);
        mmgr_line_put(&source.line, "^(@!?P )?(");
        literal_escaped(operation, &source);
        mmgr_line_ch(&source.line, '|');
        literal_escaped(writing, &source);
        mmgr_line_put(&source.line, ")( .*)?$");
        VerbaLinea name_line = mmgr_line_over(form->name.text, sizeof(form->name.text));
        mmgr_line_put(&name_line, "pipe:");
        (void)mmgr_line_put_n(&name_line, operation.text, operation.length);
        mmgr_line_ch(&name_line, ':');
        (void)mmgr_line_put_n(&name_line, writing.text, writing.length);
        (void)mmgr_line_finish(&name_line);
        (void)table_pattern_whole(&form->links[0], platform_text_bytes(&source));
        form->link_count = 1u;
        platform_text_ended(&source);
    }
    return 1;
}

int forms_read(int count, char **files, IdentityForms *forms)
{
    platform_array_begun(&forms->forms, sizeof(IdentityForm), platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
    static char s_paths[RULESET_PATHS_MAX][PLATFORM_PATH_BYTES];
    for (int given = 0; given < count; given += 1)
    {
        LanguageSet set;
        EMBED_CALL(memor.set, MemoriaCfg, .dst = &set, .bytes = sizeof(set), .val = 0);
        const RulesetSchema *const schema = code_generator(files[given]).schema();
        char folder[PLATFORM_PATH_BYTES];
        if (ruleset_folder(folder, sizeof(folder)) == 0)
        {
            folder[0] = '\0';
        }
        char path[PLATFORM_PATH_BYTES];
        VerbaLinea path_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&path_line, folder);
        mmgr_line_ch(&path_line, '/');
        mmgr_line_put(&path_line, schema->file);
        (void)mmgr_line_finish(&path_line);
        const unsigned int paths_count = ruleset_paths(schema, path, s_paths);
        RulesetFlat flat;
        ruleset_flat_schema(schema, &flat);
        char why[RULESET_ERROR_BYTES];
        VerbaLinea error = mmgr_line_over(why, sizeof(why));
        // the read of the relations reads the flat schema, and nothing reads it after
        const int read =
            ruleset_relations_read(s_paths, paths_count, &flat.schema, &set.memory, &set.relations, &error);
        (void)mmgr_line_finish(&error);
        ruleset_flat_ended(&flat);
        if (read == 0)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  ");
            mmgr_line_put(output_line, why);
            mmgr_line_put(output_line, " could not be opened");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
            return 0;
        }
        int whole = 1;
        for (unsigned int at = 0u; whole && (at < set.relations.entry_count); at += 1u)
        {
            const RulesetCoreEntry *const entry = &set.relations.entries[at];
            if (entry->type != RULESET_CORE_ENTRY_FORM)
            {
                continue;
            }
            IdentityForm *const form = form_added(forms);
            if (form == NULL)
            {
                whole = 0;
                break;
            }
            // Explicit casts read the relations' bytes as the text that they are
            (void)IDENTITY_SET(form->name, (const char *)&set.relations.text[entry->name.first], entry->name.length);
            PlatformText text;
            identity_text_begun(&text);
            (void)mmgr_line_put_n(&text.line, (const char *)&set.relations.text[entry->text.first], entry->text.length);
            whole = form_links(platform_text_bytes(&text), form);
            platform_text_ended(&text);
        }
        ruleset_relation_memory_ended(&set.memory);
        // a pipe row chooses between an operation and its writing on the other pipe. The row's choice placed a link of
        // either, whichever way it went
        for (unsigned int at = 0u; whole && (at < paths_count); at += 1u)
        {
            // each file as the run stores it (ruleset_bytes_read)
            PlatformText file_text;
            ruleset_text_begun(&file_text);
            (void)ruleset_bytes_read(s_paths[at], &file_text);
            const unsigned long long file_size = platform_text_size(&file_text);
            const char *const file_bytes = platform_text_bytes(&file_text);
            const CarrierPiece file = {(file_bytes != NULL) ? file_bytes : "", (size_t)file_size};
            whole = pipe_forms_read(file, forms);
            platform_text_ended(&file_text);
        }
        if (!whole)
        {
            identity_path_said(files[given], ": the pool cannot meet its forms");
            return 0;
        }
    }
    return 1;
}

// 1 where some link of `form` matches the terminated `type` whole
static int form_writes(const IdentityForm *form, const char *type)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = type, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    for (unsigned int at = 0u; at < form->link_count; at += 1u)
    {
        TableMatch match;
        if (table_matched(&form->links[at], type, length, &match))
        {
            return 1;
        }
    }
    return 0;
}

void forms_writing(const IdentityForms *forms, const char *type, PlatformText *out)
{
    const unsigned long long before = platform_text_size(out);
    for (unsigned long long at = 0ULL; at < forms->forms.count; at += 1ULL)
    {
        const IdentityForm *const form = identity_form(forms, at);
        if (form_writes(form, type))
        {
            if (platform_text_size(out) != before)
            {
                mmgr_line_ch(&out->line, ' ');
            }
            mmgr_line_put(&out->line, form->name.text);
        }
    }
    if (platform_text_size(out) == before)
    {
        mmgr_line_ch(&out->line, '-');
    }
}

// The entries of a context, split at its semicolons, into `entries` in the order of their bytes, each once, as a
// std::set holds them. `entries` holds at least the context's bytes plus one. The answer is the count
static unsigned long long context_entries(CarrierPiece context, CarrierPiece *entries)
{
    unsigned long long count = 0ULL;
    size_t cut = 0u;
    CarrierPiece entry;
    while (carrier_piece_next(context, ';', &cut, &entry))
    {
        entries[count] = entry;
        count += 1ULL;
    }
    identity_sorted_in_place(entries, count, [](const CarrierPiece &one, const CarrierPiece &other) {
        return identity_piece_order(one, other) < 0;
    });
    unsigned long long kept = 0ULL;
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        if ((kept == 0ULL) || !identity_piece_same(entries[kept - 1ULL], entries[at]))
        {
            entries[kept] = entries[at];
            kept += 1ULL;
        }
    }
    return kept;
}

// 1 where the `count` sorted entries at `entries` hold `entry`
static int context_holds(const CarrierPiece *entries, unsigned long long count, CarrierPiece entry)
{
    const unsigned long long at = identity_lower(
        entries, count, [entry](const CarrierPiece &one) { return identity_piece_order(one, entry) < 0; });
    return (at < count) && identity_piece_same(entries[at], entry);
}

// the carrier as its address writes it, `question:position question:position`, onto `line`
static void carrier_text(const Carrier *carrier, VerbaLinea *line)
{
    (void)mmgr_line_put_n(line, carrier->first.question.text, carrier->first.question.length);
    mmgr_line_ch(line, ':');
    (void)mmgr_line_put_n(line, carrier->first.position.text, carrier->first.position.length);
    mmgr_line_ch(line, ' ');
    (void)mmgr_line_put_n(line, carrier->second.question.text, carrier->second.question.length);
    mmgr_line_ch(line, ':');
    (void)mmgr_line_put_n(line, carrier->second.position.text, carrier->second.position.length);
}

// A verdict over the `count` carriers at `carriers`, written after them: the first case one carrier's two questions
// answer apart, or the count of cases alike. 1 where it closed, 0 where it stayed open over cases queried, -1 where
// it queried nothing
static int carriers_verdict(RunWritten *file, const Carrier *const *carriers, unsigned long long count,
                            const HostAnswers *answers)
{
    unsigned int alike = 0u;
    VerbaLinea *const line = &file->text.line;
    for (unsigned long long carrier_at = 0ULL; carrier_at < count; carrier_at += 1ULL)
    {
        const Carrier *const slice = carriers[carrier_at];
        const HostCases *const lead =
            host_cases_of_bytes(answers, slice->first.question.text, slice->first.question.length);
        const HostCases *const rite =
            host_cases_of_bytes(answers, slice->second.question.text, slice->second.question.length);
        if ((lead == NULL) || (rite == NULL))
        {
            continue;
        }
        for (unsigned int at = 0u; at < IDENTITY_CASES; at += 1u)
        {
            // a chain the host gives fewer answers than cases reads none past its last
            const char *const one = host_case(lead, at);
            const char *const other = host_case(rite, at);
            if (identity_same(one, "-") || identity_same(other, "-"))
            {
                continue;
            }
            if (!identity_same(one, other))
            {
                mmgr_line_put(line, "closed ");
                carrier_text(slice, line);
                mmgr_line_put(line, " ");
                case_text(at, line);
                mmgr_line_put(line, "->");
                mmgr_line_put(line, one);
                mmgr_line_put(line, ",");
                mmgr_line_put(line, other);
                mmgr_line_ch(line, '\n');
                return 1;
            }
            alike += 1u;
        }
    }
    mmgr_line_put(line, "open ");
    mmgr_line_decimal(line, alike, 1u);
    mmgr_line_ch(line, '\n');
    return (alike != 0u) ? 0 : -1;
}

// the member a slice's cell names, `question:position`, the whole cell both where it holds no colon
static Member member_read(CarrierPiece text)
{
    const char *const colon =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = text.text, .cap = text.length, .byte = ':');
    if (colon == NULL)
    {
        const Member whole = {text, text};
        return whole;
    }
    // the colon lies within the cell
    const size_t at = (size_t)(colon - text.text);
    const CarrierPiece question = {text.text, at};
    const Member member = {question, carrier_after(text, at + 1u)};
    return member;
}

// the number the `length` bytes of `piece` write in `base`
static unsigned long long piece_number(CarrierPiece piece, unsigned int base)
{
    char number[32] = {0};
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = number, .src = piece.text,
               .bytes = (piece.length < 31u) ? piece.length : 31u);
    return EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = number, .end = NULL, .base = base);
}

// 1 where `one` sits before `other` in the chain: by question, then by position in it
static int member_before(const Member *one, const Member *other)
{
    if (!identity_piece_same(one->question, other->question))
    {
        return piece_number(one->question, 10) < piece_number(other->question, 10);
    }
    return piece_number(one->position, 16) < piece_number(other->position, 16);
}

// 1 where `one` sits before `other`: contention defers to the max basal member of each, then to the other member
static int carrier_before(const Carrier &one, const Carrier &other)
{
    const int one_swapped = member_before(&one.second, &one.first);
    const int other_swapped = member_before(&other.second, &other.first);
    const Member *const one_basal = one_swapped ? &one.second : &one.first;
    const Member *const other_basal = other_swapped ? &other.second : &other.first;
    if (member_before(one_basal, other_basal) || member_before(other_basal, one_basal))
    {
        return member_before(one_basal, other_basal);
    }
    return member_before(one_swapped ? &one.first : &one.second, other_swapped ? &other.first : &other.second);
}

// the bytes of an identity's sides, its terminator among them: two links' types and a tab between
#define SIDES_BYTES (2u * IDENTITY_LINE_BYTES)

// one slice of slices.txt: the identity it carries, its two sides apart by a tab, and its carrier
struct SliceRow
{
    char sides[SIDES_BYTES];
    Carrier carrier;
};

// the carriers of one identity, the rows from `first` on, `count` of them, its sides the max basal member's first
struct Addressed
{
    char sides[SIDES_BYTES];
    unsigned long long first;
    unsigned long long count;
};

// a line of the identity's .klq: `head`, the carrier's address, then a line's end
static void file_carrier_said(RunWritten *file, const char *head, const Carrier *carrier)
{
    mmgr_line_put(&file->text.line, head);
    carrier_text(carrier, &file->text.line);
    mmgr_line_ch(&file->text.line, '\n');
}

// The rows of slices.txt, each a slice, into one cell of the temporary tier taken once at their count. They go in the
// order of their sides as a std::map orders its keys, and in the order written within one. The rows' pieces lie in
// `text`.
// NULL where the pool cannot meet the cell
static SliceRow *slice_rows_read(CarrierPiece text, unsigned long long *count)
{
    *count = 0ULL;
    unsigned long long lines = 1ULL;
    for (size_t at = 0u; at < text.length; at += 1u)
    {
        lines += (text.text[at] == '\n') ? 1ULL : 0ULL;
    }
    SliceRow *const rows = (SliceRow *)platform_cell_cleared(lines, sizeof(SliceRow), platform_place_g_program_owner,
                                                             PLATFORM_TIER_TEMPORARY);
    if (rows == NULL)
    {
        return NULL;
    }
    size_t cut = 0u;
    CarrierPiece line;
    while (carrier_piece_next(text, '\n', &cut, &line))
    {
        CarrierPiece cells[6];
        for (unsigned int cell = 0u; cell < 6u; cell += 1u)
        {
            cells[cell].text = "";
            cells[cell].length = 0u;
        }
        size_t cell_cut = 0u;
        CarrierPiece cell;
        for (unsigned int at = 0u; (at < 6u) && carrier_piece_next(line, '\t', &cell_cut, &cell); at += 1u)
        {
            cells[at] = cell;
        }
        SliceRow *const row = &rows[*count];
        VerbaLinea key_line = mmgr_line_over(row->sides, sizeof(row->sides));
        (void)mmgr_line_put_n(&key_line, cells[2].text, cells[2].length);
        mmgr_line_ch(&key_line, '\t');
        (void)mmgr_line_put_n(&key_line, cells[3].text, cells[3].length);
        (void)mmgr_line_finish(&key_line);
        const Carrier carrier = {member_read(cells[0]), member_read(cells[1]), cells[4], cells[5]};
        row->carrier = carrier;
        *count += 1ULL;
    }
    if (!identity_sorted(rows, *count, [](const SliceRow &one, const SliceRow &other) {
            return identity_order_text(one.sides, other.sides) < 0;
        }))
    {
        platform_cell_released(rows, platform_place_g_program_owner);
        *count = 0ULL;
        return NULL;
    }
    return rows;
}

// The identities of the rows, each its carriers sorted with the max basal member first. They go into one cell of the
// temporary tier in the order of their max basal carriers. NULL where the pool cannot meet the cell
static Addressed *identities_addressed(SliceRow *rows, unsigned long long count, unsigned long long *identities)
{
    *identities = 0ULL;
    Addressed *const addressed = (Addressed *)platform_cell_cleared(
        (count != 0ULL) ? count : 1ULL, sizeof(Addressed), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if (addressed == NULL)
    {
        return NULL;
    }
    for (unsigned long long at = 0ULL; at < count;)
    {
        unsigned long long end = at;
        while ((end < count) && identity_same(rows[end].sides, rows[at].sides))
        {
            end += 1ULL;
        }
        Addressed *const identity = &addressed[*identities];
        identity->first = at;
        identity->count = end - at;
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = identity->sides, .src = rows[at].sides,
                   .bytes = sizeof(identity->sides));
        (void)identity_sorted(&rows[at], end - at, [](const SliceRow &one, const SliceRow &other) {
            return carrier_before(one.carrier, other.carrier);
        });
        // the max basal member writes an identity's first link
        Carrier *const first = &rows[at].carrier;
        if (member_before(&first->second, &first->first))
        {
            for (unsigned long long carrier_at = at; carrier_at < end; carrier_at += 1ULL)
            {
                Carrier *const carrier = &rows[carrier_at].carrier;
                const Member member = carrier->first;
                carrier->first = carrier->second;
                carrier->second = member;
                const CarrierPiece context = carrier->first_context;
                carrier->first_context = carrier->second_context;
                carrier->second_context = context;
            }
            const char *const key = rows[at].sides;
            const char *const tab =
                EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = key, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\t');
            // the tab lies within the key, which holds one
            const size_t tab_at =
                (tab != NULL) ? (size_t)(tab - key)
                              : EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = key, .cap = PLATFORM_TEXT_TO_TERMINATOR);
            VerbaLinea sides_line = mmgr_line_over(identity->sides, sizeof(identity->sides));
            mmgr_line_put(&sides_line, (tab != NULL) ? (tab + 1) : "");
            mmgr_line_ch(&sides_line, '\t');
            (void)mmgr_line_put_n(&sides_line, key, tab_at);
            (void)mmgr_line_finish(&sides_line);
        }
        *identities += 1ULL;
        at = end;
    }
    const SliceRow *const sorted = rows;
    (void)identity_sorted(addressed, *identities, [sorted](const Addressed &one, const Addressed &other) {
        return carrier_before(sorted[one.first].carrier, sorted[other.first].carrier);
    });
    return addressed;
}

// what the read of the identities counts
struct ReadCounts
{
    unsigned int number;
    unsigned int contexts;
    unsigned int closed;
    unsigned int open;
    unsigned int unqueried;
    unsigned int many;
    unsigned int split;
    unsigned int flagged;
    unsigned int formed;
};

// The contexts of one identity, its carriers gathered by context in the order of their max basal carrier, onto `file`.
// Each context is the same members in one place of the grammar, with a verdict of its own. `group` and `carriers` each
// hold the identity's count of carriers. 1, or 0 where the pool cannot meet a cell
static int identity_contexts(RunWritten *file, const SliceRow *rows, const Addressed *identity,
                             const HostAnswers *answers, unsigned int *group, const Carrier **carriers,
                             ReadCounts *counts)
{
    VerbaLinea *const line = &file->text.line;
    unsigned int groups = 0u;
    for (unsigned long long carrier_at = 0ULL; carrier_at < identity->count; carrier_at += 1ULL)
    {
        const Carrier *const slice = &rows[identity->first + carrier_at].carrier;
        unsigned int found = groups;
        for (unsigned long long before = 0ULL; (found == groups) && (before < carrier_at); before += 1ULL)
        {
            const Carrier *const earlier = &rows[identity->first + before].carrier;
            if (identity_piece_same(earlier->first_context, slice->first_context) &&
                identity_piece_same(earlier->second_context, slice->second_context))
            {
                found = group[before];
            }
        }
        group[carrier_at] = found;
        groups += (found == groups) ? 1u : 0u;
    }
    counts->many += (groups > 1u) ? 1u : 0u;
    int verdicts[2] = {0, 0};
    for (unsigned int place = 0u; place < groups; place += 1u)
    {
        unsigned long long members = 0ULL;
        for (unsigned long long carrier_at = 0ULL; carrier_at < identity->count; carrier_at += 1ULL)
        {
            if (group[carrier_at] == place)
            {
                carriers[members] = &rows[identity->first + carrier_at].carrier;
                members += 1ULL;
            }
        }
        const Carrier *const basal = carriers[0];
        file_carrier_said(file, "context ", basal);
        // the read writes an entry both members contain once, and one a member alone contains under its side
        CarrierPiece *const first_entries =
            (CarrierPiece *)platform_cell_taken((basal->first_context.length + 1u) * sizeof(CarrierPiece),
                                                platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        CarrierPiece *const second_entries =
            (CarrierPiece *)platform_cell_taken((basal->second_context.length + 1u) * sizeof(CarrierPiece),
                                                platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        if ((first_entries == NULL) || (second_entries == NULL))
        {
            platform_cell_released(second_entries, platform_place_g_program_owner);
            platform_cell_released(first_entries, platform_place_g_program_owner);
            return 0;
        }
        const unsigned long long first_count = context_entries(basal->first_context, first_entries);
        const unsigned long long second_count = context_entries(basal->second_context, second_entries);
        for (unsigned long long at = 0ULL; at < first_count; at += 1ULL)
        {
            mmgr_line_put(line, context_holds(second_entries, second_count, first_entries[at]) ? "" : "first ");
            (void)mmgr_line_put_n(line, first_entries[at].text, first_entries[at].length);
            mmgr_line_ch(line, '\n');
        }
        for (unsigned long long at = 0ULL; at < second_count; at += 1ULL)
        {
            if (!context_holds(first_entries, first_count, second_entries[at]))
            {
                mmgr_line_put(line, "second ");
                (void)mmgr_line_put_n(line, second_entries[at].text, second_entries[at].length);
                mmgr_line_ch(line, '\n');
            }
        }
        platform_cell_released(second_entries, platform_place_g_program_owner);
        platform_cell_released(first_entries, platform_place_g_program_owner);
        for (unsigned long long at = 0ULL; at < members; at += 1ULL)
        {
            file_carrier_said(file, "address ", carriers[at]);
        }
        const int verdict = carriers_verdict(file, carriers, members, answers);
        counts->contexts += 1u;
        counts->closed += (verdict == 1) ? 1u : 0u;
        counts->open += (verdict == 0) ? 1u : 0u;
        counts->unqueried += (verdict == -1) ? 1u : 0u;
        verdicts[0] |= (verdict == 1) ? 1 : 0;
        verdicts[1] |= (verdict == 0) ? 1 : 0;
    }
    counts->split += (verdicts[0] && verdicts[1]) ? 1u : 0u;
    return 1;
}

// One identity, its sides, the rule each side breaks and the forms that write each, onto `file`
static void identity_sides(RunWritten *file, const SliceRow *rows, const Addressed *identity,
                           const IdentityForms *forms, ReadCounts *counts)
{
    VerbaLinea *const line = &file->text.line;
    const char *const key = identity->sides;
    const char *const tab =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = key, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\t');
    // the tab lies within the key, which holds one
    const size_t tab_at = (tab != NULL)
                              ? (size_t)(tab - key)
                              : EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = key, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    IdentityLine sides[2];
    (void)IDENTITY_SET(sides[0], key, tab_at);
    (void)IDENTITY_SET_TEXT(sides[1], (tab != NULL) ? (key + tab_at + 1u) : "");
    // an identity's address is its max basal carrier's
    mmgr_line_put(line, "slice_identity ");
    carrier_text(&rows[identity->first].carrier, line);
    mmgr_line_put(line, "\nfirst ");
    mmgr_line_put(line, sides[0].text);
    mmgr_line_put(line, "\nsecond ");
    mmgr_line_put(line, sides[1].text);
    mmgr_line_ch(line, '\n');
    counts->number += 1u;
    for (unsigned int side = 0u; side < 2u; side += 1u)
    {
        if (rule_broken(sides[side].text))
        {
            mmgr_line_put(line, "rule 2 ");
            mmgr_line_put(line, sides[side].text);
            mmgr_line_put(line, ": its form transfers control or waits");
            mmgr_line_ch(line, '\n');
            counts->flagged += 1u;
        }
    }
    for (unsigned int at = 0u; at < 2u; at += 1u)
    {
        for (unsigned long long form_at = 0ULL; form_at < forms->forms.count; form_at += 1ULL)
        {
            const IdentityForm *const form = identity_form(forms, form_at);
            if (form_writes(form, sides[at].text))
            {
                mmgr_line_put(line, "form ");
                mmgr_line_put(line, (at == 0u) ? "first" : "second");
                mmgr_line_put(line, " ");
                mmgr_line_put(line, form->name.text);
                mmgr_line_ch(line, '\n');
                counts->formed += 1u;
            }
        }
    }
}

// What the read of the identities holds while it writes them. The host's answers, the text of the slices, its rows
// and identities, the forms, and the groups and carriers of one identity
struct ReadHeld
{
    HostAnswers answers;
    char *slices;
    SliceRow *rows;
    Addressed *addressed;
    IdentityForms forms;
    unsigned int *group;
    const Carrier **carriers;
};

// every cell the read holds given back
static void read_held_ended(ReadHeld *held)
{
    platform_cell_released(held->carriers, platform_place_g_program_owner);
    platform_cell_released(held->group, platform_place_g_program_owner);
    forms_ended(&held->forms);
    platform_cell_released(held->addressed, platform_place_g_program_owner);
    platform_cell_released(held->rows, platform_place_g_program_owner);
    platform_cell_released(held->slices, platform_place_g_program_owner);
    host_answers_ended(&held->answers);
}

// The answers, the slices and the forms the read writes from, into `held`, which read_held_ended gives back. 1, or 0
// with the reason said
static int read_held_read(const char *folder, int count, char **files, ReadHeld *held,
                          unsigned long long *identity_count)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = held, .bytes = sizeof(*held), .val = 0);
    *identity_count = 0ULL;
    if (!host_answers_read(IDENTITY_HOST_ANSWERS, 0, &held->answers))
    {
        identity_path_said(IDENTITY_HOST_ANSWERS, ": the pool cannot meet the host's answers");
        return 0;
    }
    // Members the same make the identity the same. The carriers of one identity go by address: the position of each in
    // the chain, the max basal member first
    size_t slices_size = 0u;
    held->slices = run_blob_read("slices.txt", &slices_size);
    const CarrierPiece slices_text = {(held->slices != NULL) ? held->slices : "", slices_size};
    unsigned long long row_count = 0ULL;
    held->rows = slice_rows_read(slices_text, &row_count);
    held->addressed = (held->rows != NULL) ? identities_addressed(held->rows, row_count, identity_count) : NULL;
    if (held->addressed == NULL)
    {
        identity_path_said(folder, "/slices.txt: the pool cannot meet its slices");
        return 0;
    }
    if (*identity_count == 0ULL)
    {
        identity_path_said(folder, "/slices.txt contains no slice");
        return 0;
    }
    if (!forms_read(count, files, &held->forms))
    {
        return 0;
    }
    // the groups and the carriers of one identity, at max every row's
    held->group = (unsigned int *)platform_cell_taken(row_count * sizeof(unsigned int), platform_place_g_program_owner,
                                                      PLATFORM_TIER_TEMPORARY);
    held->carriers = (const Carrier **)platform_cell_taken(row_count * sizeof(const Carrier *),
                                                           platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if ((held->group == NULL) || (held->carriers == NULL))
    {
        identity_path_said(folder, "/slices.txt: the pool cannot meet the carriers of an identity");
        return 0;
    }
    return 1;
}

int identity_read(const char *folder, const char *klq, int count, char **files)
{
    ReadHeld held;
    unsigned long long identity_count = 0ULL;
    if (!read_held_read(folder, count, files, &held, &identity_count))
    {
        read_held_ended(&held);
        return 1;
    }
    RunWritten file;
    run_written_begun(&file, klq, 1);
    {
        size_t size = 0u;
        char *const klq_text = run_file_read(klq, &size);
        const CarrierPiece text = {(klq_text != NULL) ? klq_text : "", size};
        size_t cut = 0u;
        CarrierPiece line;
        while (carrier_piece_next(text, '\n', &cut, &line) &&
               !((line.length >= 15u) &&
                 (EMBED_CALL(memor.cmp, MemoriaCfg, .src = line.text, .other = "slice_identity ", .bytes = 15u) == 0)))
        {
            (void)mmgr_line_put_n(&file.text.line, line.text, line.length);
            mmgr_line_ch(&file.text.line, '\n');
        }
        platform_cell_released(klq_text, platform_place_g_program_owner);
    }
    ReadCounts counts;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &counts, .bytes = sizeof(counts), .val = 0);
    int whole = 1;
    for (unsigned long long identity_at = 0ULL; whole && (identity_at < identity_count); identity_at += 1ULL)
    {
        identity_sides(&file, held.rows, &held.addressed[identity_at], &held.forms, &counts);
        whole = identity_contexts(&file, held.rows, &held.addressed[identity_at], &held.answers, held.group,
                                  held.carriers, &counts);
    }
    read_held_ended(&held);
    // the chains after the identities, as slice wrote them
    VerbaLinea *const line = &file.text.line;
    {
        size_t size = 0u;
        char *const blob = run_blob_read("chains.txt", &size);
        const CarrierPiece text = {(blob != NULL) ? blob : "", size};
        size_t cut = 0u;
        CarrierPiece chain_line;
        while (carrier_piece_next(text, '\n', &cut, &chain_line))
        {
            (void)mmgr_line_put_n(line, chain_line.text, chain_line.length);
            mmgr_line_ch(line, '\n');
        }
        platform_cell_released(blob, platform_place_g_program_owner);
    }
    const int written = whole && run_closed(&file);
    run_written_ended(&file);
    if (!written)
    {
        identity_path_said(klq, whole ? " could not be written" : ": the pool cannot meet a context");
        return 1;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, klq);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, counts.number, 1u);
        mmgr_line_put(output_line, " identities in ");
        mmgr_line_decimal(output_line, counts.contexts, 1u);
        mmgr_line_put(output_line, " contexts, ");
        mmgr_line_decimal(output_line, counts.many, 1u);
        mmgr_line_put(output_line, " identities in more than one, ");
        mmgr_line_decimal(output_line, counts.split, 1u);
        mmgr_line_put(output_line, " closed in one context and open in another");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  contexts: ");
    mmgr_line_decimal(output_line, counts.closed, 1u);
    mmgr_line_put(output_line, " closed, ");
    mmgr_line_decimal(output_line, counts.open, 1u);
    mmgr_line_put(output_line, " open over the cases queried, ");
    mmgr_line_decimal(output_line, counts.unqueried, 1u);
    mmgr_line_put(output_line, " queried of nothing; ");
    mmgr_line_decimal(output_line, counts.flagged, 1u);
    mmgr_line_put(output_line, " links a rule blocks from the part, ");
    mmgr_line_decimal(output_line, counts.formed, 1u);
    mmgr_line_put(output_line, " forms written beside them");
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return 0;
}

// what the walk of one folder of candidates holds: the folder as its paths begin, and the paths of its .dis files
struct CandidateFolder
{
    const char *folder;
    PlatformArray *found;
};

// one entry of a folder of candidates, kept where its name has the .dis extension
static void candidate_entry(void *context, const char *name, int folder)
{
    (void)folder;
    CandidateFolder *const walk = (CandidateFolder *)context;
    const char *const dot =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '.');
    // a name that begins at its one dot has no extension, as a path's extension reads it
    if ((dot == NULL) || (dot == name) ||
        !EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = dot, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = ".dis",
                    .ci = EMBED_FALSE))
    {
        return;
    }
    IdentityPath path;
    VerbaLinea path_line = mmgr_line_over(path.text, sizeof(path.text));
    mmgr_line_put(&path_line, walk->folder);
    const size_t folder_length =
        EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = walk->folder, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const char last = (folder_length != 0u) ? walk->folder[folder_length - 1u] : '\0';
    if ((last != '/') && (last != '\\'))
    {
        mmgr_line_ch(&path_line, '/');
    }
    mmgr_line_put(&path_line, name);
    const size_t length = mmgr_line_finish(&path_line);
#if defined(_WIN32)
    // a path's generic form writes each separator as a forward slash
    for (size_t at = 0u; at < length; at += 1u)
    {
        path.text[at] = (path.text[at] == '\\') ? '/' : path.text[at];
    }
#else
    (void)length;
#endif
    (void)platform_array_appended(walk->found, &path);
}

void candidate_paths(const IdentityPath *given, unsigned long long count, PlatformArray *paths)
{
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        PlatformArray found;
        platform_array_begun(&found, sizeof(IdentityPath), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
        CandidateFolder walk = {given[at].text, &found};
        if (!platform_folder_listed(given[at].text, candidate_entry, &walk))
        {
            platform_array_ended(&found);
            (void)platform_array_appended(paths, &given[at]);
            continue;
        }
        IdentityPath *const listed = (IdentityPath *)found.cell;
        identity_sorted_in_place(listed, found.count, [](const IdentityPath &one, const IdentityPath &other) {
            return identity_order_text(one.text, other.text) < 0;
        });
        for (unsigned long long each = 0ULL; each < found.count; each += 1ULL)
        {
            (void)platform_array_appended(paths, &listed[each]);
        }
        platform_array_ended(&found);
    }
}

int candidates_read(const char *path, Questions *candidates)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = candidates, .bytes = sizeof(*candidates), .val = 0);
    size_t size = 0u;
    char *const file = identity_file_read(path, &size);
    static TablePatternOnce s_function;
    const TablePattern *const function_pattern = table_once(&s_function, "Function : ([A-Za-z0-9_]+)", 0);
    const char *const slash =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '/');
    const char *const back =
        EMBED_CALL(cellul.chr_last, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '\\');
    const char *const last = ((back != NULL) && ((slash == NULL) || (back > slash))) ? back : slash;
    const char *const stem = (last != NULL) ? (last + 1) : path;
    const char *const dot =
        EMBED_CALL(cellul.chr, CatenaFinitaCfg, .src = stem, .cap = PLATFORM_TEXT_TO_TERMINATOR, .byte = '.');
    // the dot lies within the stem
    const size_t stem_length =
        (dot != NULL) ? (size_t)(dot - stem)
                      : EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = stem, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const CarrierPiece text = {(file != NULL) ? file : "", size};
    // Two walks of the file. The first counts its candidates and its links. The second fills the three cells of the
    // candidates, each taken once at its count. A link before any function begins a candidate named for the stem
    unsigned long long counts[2] = {0ULL, 0ULL};
    int whole = 1;
    for (unsigned int walk = 0u; whole && (walk < 2u); walk += 1u)
    {
        if (walk == 1u)
        {
            if (counts[0] == 0ULL)
            {
                break;
            }
            candidates->question = (Question *)platform_cell_cleared(
                counts[0], sizeof(Question), platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
            candidates->links = (Link *)platform_cell_cleared((counts[1] != 0ULL) ? counts[1] : 1ULL, sizeof(Link),
                                                              platform_place_g_program_owner, PLATFORM_TIER_PERSISTENT);
            candidates->places = (unsigned long long *)platform_cell_cleared(
                (counts[1] != 0ULL) ? counts[1] : 1ULL, sizeof(unsigned long long), platform_place_g_program_owner,
                PLATFORM_TIER_PERSISTENT);
            whole = (candidates->question != NULL) && (candidates->links != NULL) && (candidates->places != NULL);
            if (!whole)
            {
                break;
            }
        }
        size_t cut = 0u;
        CarrierPiece line;
        int ended = 0;
        unsigned long long question_at = 0ULL;
        unsigned long long link_at = 0ULL;
        Question *candidate = NULL;
        while (identity_line_next(text, &cut, &line))
        {
            TableMatch function;
            if (table_search(function_pattern, line.text, line.length, &function))
            {
                if (walk == 1u)
                {
                    size_t name_length = 0u;
                    const char *const name = table_group(&function, 1u, line.text, &name_length);
                    candidate = &candidates->question[question_at];
                    (void)IDENTITY_SET(candidate->number, name, name_length);
                    candidate->chain = &candidates->links[link_at];
                    candidate->by_exact = &candidates->places[link_at];
                }
                question_at += 1ULL;
                ended = 0;
                continue;
            }
            Link link;
            const int read = ended ? 0 : link_read(line, &link);
            ended = ended || (read < 0);
            if (read <= 0)
            {
                continue;
            }
            if (question_at == 0ULL)
            {
                if (walk == 1u)
                {
                    candidate = &candidates->question[0];
                    (void)IDENTITY_SET(candidate->number, stem, stem_length);
                    candidate->chain = &candidates->links[link_at];
                    candidate->by_exact = &candidates->places[link_at];
                }
                question_at = 1ULL;
            }
            if (walk == 1u)
            {
                candidates->links[link_at] = link;
                candidate->links += 1ULL;
            }
            link_at += 1ULL;
        }
        counts[0] = question_at;
        counts[1] = link_at;
    }
    platform_cell_released(file, platform_place_g_program_owner);
    candidates->count = (whole && (candidates->question != NULL)) ? counts[0] : 0ULL;
    for (unsigned long long at = 0ULL; whole && (at < candidates->count); at += 1ULL)
    {
        whole = question_ordered(&candidates->question[at]);
    }
    if (!whole)
    {
        questions_ended(candidates);
        identity_path_said(path, ": the pool cannot meet its candidates");
    }
    return whole;
}
