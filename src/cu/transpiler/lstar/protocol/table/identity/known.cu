// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// known.cu: the known and the permute: the engine's writing of the stick sifted through what nvcc writes, and nvcc's
// chains put together by their categories
#include "../../../../../../platform/platform.h"
#include "identity_internal.h"

// The first occurrence of a window where nvcc's chains contain it, its constituents the same link for link. It
// goes into `first`: 1, or 0 where nvcc's chains contain no such window
static int window_known(const Questions *questions, const Windows *windows, const Question *candidate,
                        unsigned long long link, unsigned long long length, unsigned long long identity,
                        Occurrence *first)
{
    Occurrence found;
    if (!window_occurrence(windows, identity, &found) ||
        !windows_same(&questions->question[found.question], found.link, candidate, link, length))
    {
        return 0;
    }
    *first = found;
    return 1;
}

// the window rendered as known.txt and permutations.txt write it, each line's end but the last a semicolon, onto `line`
static void window_joined(const Question *question, unsigned long long first, unsigned long long length,
                          VerbaLinea *line)
{
    PlatformText rendered;
    identity_text_begun(&rendered);
    window_render(question, first, length, &rendered);
    const unsigned long long size = platform_text_size(&rendered);
    const char *const bytes = platform_text_bytes(&rendered);
    for (unsigned long long at = 0ULL; (bytes != NULL) && ((at + 1ULL) < size); at += 1ULL)
    {
        mmgr_line_ch(line, (bytes[at] == '\n') ? ';' : bytes[at]);
    }
    platform_text_ended(&rendered);
}

// what the sift of the candidates counts
struct KnownCounts
{
    unsigned long long candidates;
    unsigned int wholes;
    unsigned int permutations;
    unsigned int strangers;
    unsigned int unknown_primitives;
    unsigned int unknown_pairs;
    unsigned int flagged;
};

// One candidate sifted through the identities nvcc's chains contain, onto `file`, and counted. `identities` holds the
// candidate's links
static void candidate_sifted(const Questions *questions, const Windows *windows, const Question *candidate,
                             unsigned long long *identities, RunWritten *file, KnownCounts *counts)
{
    VerbaLinea *const line = &file->text.line;
    // a chain's links are far below 2^32
    const unsigned int links = (unsigned int)candidate->links;
    const unsigned long long whole_identity = windows_walked(candidate, 0ULL, ~0ULL, NULL);
    Occurrence whole = {0u, 0u, 0u};
    const int known_whole = window_known(questions, windows, candidate, 0ULL, links, whole_identity, &whole);
    mmgr_line_put(line, "candidate ");
    mmgr_line_put(line, candidate->number.text);
    mmgr_line_put(line, " ");
    mmgr_line_hex(line, whole_identity, 16u);
    mmgr_line_put(line, " ");
    if (known_whole)
    {
        mmgr_line_put(line, "known ");
        address_text(questions, whole.question, whole.link, line);
    }
    else
    {
        mmgr_line_put(line, "unknown");
    }
    mmgr_line_ch(line, '\n');
    if (known_whole)
    {
        counts->wholes += 1u;
        return;
    }
    mmgr_line_put(line, "constituents");
    for (unsigned int link = 0u; link < links;)
    {
        const unsigned long long made = window_identities(candidate, link, identities);
        unsigned int length = 0u;
        // a chain's links are far below 2^32
        for (unsigned int longest = (unsigned int)made; (longest > 0u) && (length == 0u); longest -= 1u)
        {
            Occurrence first = {0u, 0u, 0u};
            if (window_known(questions, windows, candidate, link, longest, identities[longest - 1u], &first))
            {
                mmgr_line_ch(line, ' ');
                address_text(questions, first.question, first.link, line);
                mmgr_line_ch(line, '/');
                mmgr_line_decimal(line, longest, 1u);
                length = longest;
            }
        }
        if (length == 0u)
        {
            mmgr_line_put(line, " ?");
            mmgr_line_put(line, candidate->chain[link].position.text);
            length = 1u;
        }
        link += length;
    }
    mmgr_line_ch(line, '\n');
    unsigned int unknown_here = 0u;
    for (unsigned int link = 0u; link < links; link += 1u)
    {
        unsigned long long pair[2] = {0ULL, 0ULL};
        (void)windows_walked(candidate, link, 2ULL, pair);
        Occurrence ignored = {0u, 0u, 0u};
        const int alone = window_known(questions, windows, candidate, link, 1ULL, pair[0], &ignored);
        const int paired =
            ((link + 1u) < links) ? window_known(questions, windows, candidate, link, 2ULL, pair[1], &ignored) : 1;
        const int next_alone = ((link + 1u) < links)
                                   ? window_known(questions, windows, candidate, link + 1u, 1ULL,
                                                  windows_walked(candidate, link + 1u, 1ULL, NULL), &ignored)
                                   : 0;
        const Link *const written = &candidate->chain[link];
        const int ruled = rule_broken(written->exact.text);
        if (!alone)
        {
            mmgr_line_put(line, "unknown ");
            mmgr_line_put(line, written->position.text);
            mmgr_line_put(line, " ");
            window_render(candidate, link, 1ULL, &file->text);
            mmgr_line_put(line, ruled ? " rule 2: its form transfers control or waits" : "");
            mmgr_line_ch(line, '\n');
            counts->unknown_primitives += 1u;
            unknown_here += 1u;
            counts->flagged += ruled ? 1u : 0u;
        }
        // two links each known alone and never written side by side
        if (alone && next_alone && !paired)
        {
            mmgr_line_put(line, "unpaired ");
            mmgr_line_put(line, written->position.text);
            mmgr_line_put(line, " ");
            window_joined(candidate, link, 2ULL, line);
            mmgr_line_put(line, ";\n");
            counts->unknown_pairs += 1u;
            unknown_here += 1u;
        }
    }
    if (unknown_here == 0u)
    {
        counts->permutations += 1u;
    }
    else
    {
        counts->strangers += 1u;
    }
}

// Each candidate chain sifted through the identities nvcc's chains contain, before anything of it goes to the
// part. Its whole has a known address, or it reads as the longest known windows from each link. Every link whose
// primitive nvcc never writes, and every two links side by side nvcc never writes so, goes apart as unknown. A
// part unknown that rule 2 blocks from the part carries the rule. The run's blob known.txt contains a candidate a
// group of lines
int identity_known(const char *listing, const char *manifest, const char *folder, const IdentityPath *paths,
                   unsigned long long count)
{
    Questions questions;
    if (!questions_read(listing, manifest, &questions))
    {
        return 1;
    }
    Windows windows;
    if (!windows_counted(&questions, &windows))
    {
        questions_ended(&questions);
        return 1;
    }
    RunWritten file;
    run_written_begun(&file, identity_path_joined(folder, "/known.txt").text, 0);
    KnownCounts counts;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &counts, .bytes = sizeof(counts), .val = 0);
    int whole = 1;
    // the candidates file by file, each file's chains in the order it writes them
    for (unsigned long long at = 0ULL; whole && (at < count); at += 1ULL)
    {
        Questions candidates;
        if (!candidates_read(paths[at].text, &candidates))
        {
            whole = 0;
            break;
        }
        for (unsigned long long candidate_at = 0ULL; candidate_at < candidates.count; candidate_at += 1ULL)
        {
            const Question *const candidate = &candidates.question[candidate_at];
            if (candidate->links == 0ULL)
            {
                continue;
            }
            unsigned long long *const identities = (unsigned long long *)platform_cell_taken(
                candidate->links * sizeof(unsigned long long), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
            if (identities == NULL)
            {
                identity_path_said(paths[at].text, ": the pool cannot meet the identities of a candidate");
                whole = 0;
                break;
            }
            candidate_sifted(&questions, &windows, candidate, identities, &file, &counts);
            platform_cell_released(identities, platform_place_g_program_owner);
        }
        counts.candidates += candidates.count;
        questions_ended(&candidates);
    }
    windows_ended(&windows);
    questions_ended(&questions);
    const int closed = whole && run_closed(&file);
    run_written_ended(&file);
    if (!closed)
    {
        identity_path_said(file.path.text, " could not be written");
        return 1;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, file.path.text);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, counts.candidates, 1u);
        mmgr_line_put(output_line, " candidates, ");
        mmgr_line_decimal(output_line, counts.wholes, 1u);
        mmgr_line_put(output_line, " known whole, ");
        mmgr_line_decimal(output_line, counts.permutations, 1u);
        mmgr_line_put(output_line, " not known whole but every link and every two side by side known, ");
        mmgr_line_decimal(output_line, counts.strangers, 1u);
        mmgr_line_put(output_line, " with a part nvcc never writes");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  parts unknown: ");
    mmgr_line_decimal(output_line, counts.unknown_primitives, 1u);
    mmgr_line_put(output_line, " links, ");
    mmgr_line_decimal(output_line, counts.flagged, 1u);
    mmgr_line_put(output_line, " of them blocked from the part by rule 2, ");
    mmgr_line_decimal(output_line, counts.unknown_pairs, 1u);
    mmgr_line_put(output_line, " pairs side by side");
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return 0;
}

// a type of link at its first occurrence: the question and the link it is at, and the type, which lies in the link
// [BORROWS]
struct TypeFirst
{
    unsigned int question;
    unsigned int link;
    const char *type;
};

// one member of a category: the root of its category, its place in the order of first occurrence, and its type
struct CategoryMember
{
    unsigned int root;
    unsigned int order;
    unsigned int type;
};

// one category: the first of its members in the members ordered by root, and how many they are
struct Category
{
    unsigned int first;
    unsigned int count;
};

// one window the permutations put that nvcc never writes. Its identity, its place in the order put, the question and
// its link, and the type put in
struct PermutedPut
{
    unsigned long long identity;
    unsigned long long order;
    unsigned int question;
    unsigned int link;
    unsigned int member;
};

// The types of link and their categories, in cells of the temporary tier. Each type at its first occurrence ordered by
// type, each type's parent among them, each category's members, and the category of each root or CATEGORY_NONE
#define CATEGORY_NONE (~0u)

struct Categories
{
    TypeFirst *types;
    unsigned int type_count;
    unsigned int *parent;
    CategoryMember *members;
    Category *categories;
    unsigned int category_count;
    unsigned int *category_of_root;
};

static void categories_ended(Categories *categories)
{
    platform_cell_released(categories->category_of_root, platform_place_g_program_owner);
    platform_cell_released(categories->categories, platform_place_g_program_owner);
    platform_cell_released(categories->members, platform_place_g_program_owner);
    platform_cell_released(categories->parent, platform_place_g_program_owner);
    platform_cell_released(categories->types, platform_place_g_program_owner);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = categories, .bytes = sizeof(*categories), .val = 0);
}

// the place of the type `type` among the types
static unsigned int type_place(const Categories *categories, const char *type)
{
    // the types are far below 2^32
    return (unsigned int)identity_lower(categories->types, categories->type_count, [type](const TypeFirst &one) {
        return identity_order_text(one.type, type) < 0;
    });
}

// the root of the type at `type` among the categories, each type joined to the types a slice finds it with
static unsigned int category_root(Categories *categories, unsigned int type)
{
    unsigned int root = type;
    while (categories->parent[root] != root)
    {
        root = categories->parent[root];
    }
    for (unsigned int walk = type; walk != root;)
    {
        const unsigned int next = categories->parent[walk];
        categories->parent[walk] = root;
        walk = next;
    }
    return root;
}

// a cell of the temporary tier of `count` elements of `size` bytes, at least one
static void *permute_cell(unsigned long long count, unsigned long long size)
{
    return platform_cell_taken(((count != 0ULL) ? count : 1ULL) * size, platform_place_g_program_owner,
                               PLATFORM_TIER_TEMPORARY);
}

// The types of the questions' links each at its first occurrence, each its own category. Then joined by the slices,
// and the categories' members in the order of first occurrence. 1, or 0 where the pool cannot meet a cell
static int categories_read(const Questions *questions, Categories *categories)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = categories, .bytes = sizeof(*categories), .val = 0);
    unsigned long long links = 0ULL;
    for (unsigned long long at = 0ULL; at < questions->count; at += 1ULL)
    {
        links += questions->question[at].links;
    }
    categories->types = (TypeFirst *)permute_cell(links, sizeof(TypeFirst));
    if (categories->types == NULL)
    {
        return 0;
    }
    unsigned long long count = 0ULL;
    for (unsigned int question = 0u; question < questions->count; question += 1u)
    {
        const Question *const current = &questions->question[question];
        for (unsigned int link = 0u; link < current->links; link += 1u)
        {
            const TypeFirst first = {question, link, current->chain[link].type.text};
            categories->types[count] = first;
            count += 1ULL;
        }
    }
    // the types ordered by their bytes, each kept at its first occurrence
    if (!identity_sorted(categories->types, count, [](const TypeFirst &one, const TypeFirst &other) {
            return identity_order_text(one.type, other.type) < 0;
        }))
    {
        categories_ended(categories);
        return 0;
    }
    unsigned int kept = 0u;
    for (unsigned long long at = 0ULL; at < count; at += 1ULL)
    {
        if ((kept == 0u) || !identity_same(categories->types[kept - 1u].type, categories->types[at].type))
        {
            categories->types[kept] = categories->types[at];
            kept += 1u;
        }
    }
    categories->type_count = kept;
    categories->parent = (unsigned int *)permute_cell(kept, sizeof(unsigned int));
    categories->members = (CategoryMember *)permute_cell(kept, sizeof(CategoryMember));
    categories->categories = (Category *)permute_cell(kept, sizeof(Category));
    categories->category_of_root = (unsigned int *)permute_cell(kept, sizeof(unsigned int));
    if ((categories->parent == NULL) || (categories->members == NULL) || (categories->categories == NULL) ||
        (categories->category_of_root == NULL))
    {
        categories_ended(categories);
        return 0;
    }
    for (unsigned int at = 0u; at < kept; at += 1u)
    {
        categories->parent[at] = at;
        categories->category_of_root[at] = CATEGORY_NONE;
    }
    for (unsigned long long one = 0ULL; one < questions->count; one += 1ULL)
    {
        for (unsigned long long other = one + 1ULL; other < questions->count; other += 1ULL)
        {
            const Link *left = NULL;
            const Link *right = NULL;
            if (slice_found(&questions->question[one], &questions->question[other], &left, &right))
            {
                const unsigned int left_root = category_root(categories, type_place(categories, left->type.text));
                const unsigned int right_root = category_root(categories, type_place(categories, right->type.text));
                categories->parent[left_root] = right_root;
            }
        }
    }
    // each type in the order of its first occurrence, then grouped by its category's root in that order
    for (unsigned int at = 0u; at < kept; at += 1u)
    {
        const CategoryMember member = {0u, 0u, at};
        categories->members[at] = member;
    }
    const TypeFirst *const types = categories->types;
    identity_sorted_in_place(
        categories->members, kept, [types](const CategoryMember &one, const CategoryMember &other) {
            const TypeFirst &left = types[one.type];
            const TypeFirst &right = types[other.type];
            return (left.question != right.question) ? (left.question < right.question) : (left.link < right.link);
        });
    for (unsigned int at = 0u; at < kept; at += 1u)
    {
        categories->members[at].order = at;
        categories->members[at].root = category_root(categories, categories->members[at].type);
    }
    if (!identity_sorted(categories->members, kept,
                         [](const CategoryMember &one, const CategoryMember &other) { return one.root < other.root; }))
    {
        categories_ended(categories);
        return 0;
    }
    for (unsigned int at = 0u; at < kept;)
    {
        unsigned int end = at;
        while ((end < kept) && (categories->members[end].root == categories->members[at].root))
        {
            end += 1u;
        }
        const Category category = {at, end - at};
        categories->categories[categories->category_count] = category;
        categories->category_count += 1u;
        at = end;
    }
    // the categories in the order of their first members
    const CategoryMember *const members = categories->members;
    identity_sorted_in_place(categories->categories, categories->category_count,
                             [members](const Category &one, const Category &other) {
                                 return members[one.first].order < members[other.first].order;
                             });
    for (unsigned int at = 0u; at < categories->category_count; at += 1u)
    {
        categories->category_of_root[members[categories->categories[at].first].root] = at;
    }
    return 1;
}

// The window the permutation puts at link `link` of `current`. The link before it, the link with the member put in,
// and the link after, each where the chain holds it. The window's links go into `links`, three at max
static void permuted_window(const Questions *questions, const Categories *categories, const Question *current,
                            unsigned int link, unsigned int member, Link *links, Question *window)
{
    const Link *const own = &current->chain[link];
    const TypeFirst *const at = &categories->types[member];
    Link substitute = questions->question[at->question].chain[at->link];
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = substitute.named, .src = own->named, .bytes = sizeof(substitute.named));
    substitute.named_count = own->named_count;
    substitute.position = own->position;
    // a chain's links are far below 2^32
    const unsigned int chain_length = (unsigned int)current->links;
    const unsigned int last = (chain_length < (link + 2u)) ? chain_length : (link + 2u);
    unsigned int count = 0u;
    for (unsigned int within = (link == 0u) ? 0u : (link - 1u); within < last; within += 1u)
    {
        links[count] = (within == link) ? substitute : current->chain[within];
        count += 1u;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = window, .bytes = sizeof(*window), .val = 0);
    window->number = current->number;
    window->chain = links;
    window->links = count;
}

// The parts of nvcc's chains put together by their categories. The walk reads no part for what it means. Two
// links a slice sets apart are for each other, and each link joined to another so, at any remove, is of its
// category. Every other member of its category that names as many registers goes in for each link of each chain.
// The registers of the link it is for go to it.
//
// The window of the link before it and the link after queries it. A window nvcc writes somewhere goes as known.
// One it never writes is a question, recorded at the first address it arises at with its count. A link put in
// that rule 2 blocks from the part carries the rule. The run's blob permutations.txt contains the categories,
// each with its members at the first address each occurs at. Then it contains the questions in the order of
// their addresses
int identity_permute(const char *listing, const char *manifest, const char *folder)
{
    Questions questions;
    if (!questions_read(listing, manifest, &questions))
    {
        return 1;
    }
    Windows windows;
    Categories categories;
    if (!windows_counted(&questions, &windows))
    {
        questions_ended(&questions);
        return 1;
    }
    if (!categories_read(&questions, &categories))
    {
        windows_ended(&windows);
        questions_ended(&questions);
        identity_path_said(folder, "/permutations.txt: the pool cannot meet the categories");
        return 1;
    }
    PlatformArray puts;
    platform_array_begun(&puts, sizeof(PermutedPut), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    unsigned long long known = 0ULL;
    unsigned long long put = 0ULL;
    Link window_links[3];
    Question window;
    for (unsigned int question = 0u; question < questions.count; question += 1u)
    {
        const Question *const current = &questions.question[question];
        for (unsigned int link = 0u; link < current->links; link += 1u)
        {
            const Link *const own = &current->chain[link];
            const unsigned int root = category_root(&categories, type_place(&categories, own->type.text));
            const unsigned int category_at = categories.category_of_root[root];
            const Category *const category =
                (category_at != CATEGORY_NONE) ? &categories.categories[category_at] : NULL;
            for (unsigned int member_at = 0u; (category != NULL) && (member_at < category->count); member_at += 1u)
            {
                const unsigned int member = categories.members[category->first + member_at].type;
                const TypeFirst *const at = &categories.types[member];
                const Link *const source = &questions.question[at->question].chain[at->link];
                if (identity_same(at->type, own->type.text) || (source->named_count != own->named_count))
                {
                    continue;
                }
                permuted_window(&questions, &categories, current, link, member, window_links, &window);
                // a window holds at max three links
                const unsigned int length = (unsigned int)window.links;
                const unsigned long long identity = windows_walked(&window, 0ULL, ~0ULL, NULL);
                Occurrence found;
                if (window_occurrence(&windows, identity, &found) &&
                    windows_same(&questions.question[found.question], found.link, &window, 0ULL, length))
                {
                    known += 1ULL;
                    put += 1ULL;
                    continue;
                }
                const PermutedPut permuted = {identity, put, question, link, member};
                (void)platform_array_appended(&puts, &permuted);
                put += 1ULL;
            }
        }
    }
    windows_ended(&windows);
    // the puts of one identity together, each run's first its first occurrence and its length its count
    PermutedPut *const put_cells = (PermutedPut *)puts.cell;
    (void)identity_sorted(put_cells, puts.count, [](const PermutedPut &one, const PermutedPut &other) {
        return one.identity < other.identity;
    });
    unsigned long long distinct = 0ULL;
    for (unsigned long long at = 0ULL; at < puts.count;)
    {
        unsigned long long end = at;
        while ((end < puts.count) && (put_cells[end].identity == put_cells[at].identity))
        {
            end += 1ULL;
        }
        // the run's first goes forward to the place of its question, its count in the place of its identity
        PermutedPut first = put_cells[at];
        first.identity = end - at;
        put_cells[distinct] = first;
        distinct += 1ULL;
        at = end;
    }
    // the questions in the order each first arose
    identity_sorted_in_place(put_cells, distinct,
                             [](const PermutedPut &one, const PermutedPut &other) { return one.order < other.order; });
    RunWritten file;
    run_written_begun(&file, identity_path_joined(folder, "/permutations.txt").text, 0);
    VerbaLinea *const line = &file.text.line;
    unsigned int joined = 0u;
    for (unsigned int category_at = 0u; category_at < categories.category_count; category_at += 1u)
    {
        const Category *const category = &categories.categories[category_at];
        if (category->count < 2u)
        {
            continue;
        }
        joined += 1u;
        const TypeFirst *const first = &categories.types[categories.members[category->first].type];
        mmgr_line_put(line, "category ");
        address_text(&questions, first->question, first->link, line);
        mmgr_line_put(line, " ");
        mmgr_line_decimal(line, category->count, 1u);
        mmgr_line_ch(line, '\n');
        for (unsigned int member_at = 0u; member_at < category->count; member_at += 1u)
        {
            const TypeFirst *const at = &categories.types[categories.members[category->first + member_at].type];
            mmgr_line_put(line, "member ");
            address_text(&questions, at->question, at->link, line);
            mmgr_line_put(line, " ");
            mmgr_line_put(line, at->type);
            mmgr_line_ch(line, '\n');
        }
    }
    unsigned int flagged = 0u;
    PlatformText rendered;
    identity_text_begun(&rendered);
    for (unsigned long long order_at = 0ULL; order_at < distinct; order_at += 1ULL)
    {
        const PermutedPut *const question = &put_cells[order_at];
        const Question *const current = &questions.question[question->question];
        const char *const to = categories.types[question->member].type;
        mmgr_line_put(line, "question ");
        address_text(&questions, question->question, question->link, line);
        mmgr_line_put(line, " ");
        mmgr_line_decimal(line, question->identity, 1u);
        mmgr_line_put(line, "\nfrom ");
        mmgr_line_put(line, current->chain[question->link].type.text);
        mmgr_line_put(line, "\nto ");
        mmgr_line_put(line, to);
        mmgr_line_put(line, "\nwindow ");
        // the window less its last line's end, each line's end within it a semicolon
        permuted_window(&questions, &categories, current, question->link, question->member, window_links, &window);
        platform_text_reset(&rendered);
        window_render(&window, 0ULL, window.links, &rendered);
        const unsigned long long window_length = platform_text_size(&rendered);
        const char *const window_text = platform_text_bytes(&rendered);
        for (unsigned long long at = 0ULL; (window_text != NULL) && ((at + 1ULL) < window_length); at += 1ULL)
        {
            mmgr_line_ch(line, (window_text[at] == '\n') ? ';' : window_text[at]);
        }
        mmgr_line_ch(line, '\n');
        if (rule_broken(to))
        {
            mmgr_line_put(line, "rule 2 ");
            mmgr_line_put(line, to);
            mmgr_line_put(line, ": its form transfers control or waits");
            mmgr_line_ch(line, '\n');
            flagged += 1u;
        }
    }
    platform_text_ended(&rendered);
    platform_array_ended(&puts);
    const unsigned int type_count = categories.type_count;
    categories_ended(&categories);
    questions_ended(&questions);
    const int closed = run_closed(&file);
    run_written_ended(&file);
    if (!closed)
    {
        identity_path_said(file.path.text, " could not be written");
        return 1;
    }
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  ");
    mmgr_line_put(output_line, file.path.text);
    mmgr_line_put(output_line, ": ");
    mmgr_line_decimal(output_line, type_count, 1u);
    mmgr_line_put(output_line, " types of link in ");
    mmgr_line_decimal(output_line, joined, 1u);
    mmgr_line_put(output_line, " categories of more than one, ");
    mmgr_line_decimal(output_line, put, 1u);
    mmgr_line_put(output_line, " windows put, ");
    mmgr_line_decimal(output_line, known, 1u);
    mmgr_line_put(output_line, " of them nvcc writes, ");
    mmgr_line_decimal(output_line, distinct, 1u);
    mmgr_line_put(output_line, " questions, ");
    mmgr_line_decimal(output_line, flagged, 1u);
    mmgr_line_put(output_line, " of them blocked from the part by rule 2");
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return 0;
}
