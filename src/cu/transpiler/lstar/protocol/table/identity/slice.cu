// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// slice.cu: the slice: nvcc's writing of the stick cut into windows, each an identity at the address it occurs at, and
// the chains written
#include "../../../../../../../includes/platform/platform.h"
#include "identity_internal.h"

// the names a window gives its registers, literals and targets: each key with its class and its count. The first keys
// lie in the caller's own room, and the rest in a cell of the temporary tier
#define RENDER_LOCAL 256u
#define RENDER_KEY_BYTES 64u

struct RenderName
{
    char key[RENDER_KEY_BYTES];
    size_t key_length;
    unsigned int type;
    unsigned int count;
};

struct RenderNames
{
    RenderName local[RENDER_LOCAL];
    unsigned int locals;
    PlatformArray more;
    unsigned int counts[5];
};

static void render_names_begun(RenderNames *names)
{
    names->locals = 0u;
    platform_array_begun(&names->more, sizeof(RenderName), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = names->counts, .bytes = sizeof(names->counts), .val = 0);
}

static void render_names_ended(RenderNames *names)
{
    platform_array_ended(&names->more);
}

// the name of the key, given the next count of its class where the window has not named it yet
static const RenderName *render_name(RenderNames *names, const char *key, size_t key_length, unsigned int type)
{
    for (unsigned int at = 0u; at < names->locals; at += 1u)
    {
        const RenderName *const name = &names->local[at];
        if ((name->key_length == key_length) &&
            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = name->key, .other = key, .bytes = key_length) == 0))
        {
            return name;
        }
    }
    for (unsigned long long at = 0ULL; at < names->more.count; at += 1ULL)
    {
        const RenderName *const name = (const RenderName *)platform_array_at(&names->more, at);
        if ((name->key_length == key_length) &&
            (EMBED_CALL(memor.cmp, MemoriaCfg, .src = name->key, .other = key, .bytes = key_length) == 0))
        {
            return name;
        }
    }
    RenderName given;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &given, .bytes = sizeof(given), .val = 0);
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = given.key, .src = key, .bytes = key_length);
    given.key_length = key_length;
    given.type = type;
    given.count = names->counts[type];
    names->counts[type] += 1u;
    if (names->locals < RENDER_LOCAL)
    {
        names->local[names->locals] = given;
        names->locals += 1u;
        return &names->local[names->locals - 1u];
    }
    (void)platform_array_appended(&names->more, &given);
    return (const RenderName *)platform_array_at(&names->more, names->more.count - 1ULL);
}

// The link written onto `out` with each register by its class and the order the window first names it in. Its class
// is r for R, p for P, ur for UR. Each literal operand goes as a part of its own, k, in the order the window first
// writes its value. Each target of a control transfer goes as a part of its own, t, in the order the window first aims
// at it. Two windows the same but for the registers they hold, the values they carry and where they sit write the same
// text
static void link_render(const Link *link, RenderNames *names, PlatformText *out)
{
    static const char *const s_classes[5] = {"r", "p", "ur", "k", "t"};
    const char *const marked = link->marked.text;
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = marked, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    unsigned int name = 0u;
    unsigned int literal = 0u;
    unsigned int target = 0u;
    size_t mark = 0u;
    for (size_t next = 0u; next < length; next += 1u)
    {
        const char part = marked[next];
        if ((part != '\x01') && (part != '\x02') && (part != '\x03'))
        {
            continue;
        }
        (void)mmgr_line_put_n(&out->line, &marked[mark], next - mark);
        char key[RENDER_KEY_BYTES];
        VerbaLinea key_line = mmgr_line_over(key, sizeof(key));
        unsigned int type = 0u;
        if (part == '\x01')
        {
            const char *const named = (name < link->named_count) ? link->named[name].text : "";
            mmgr_line_put(&key_line, named);
            type = (named[0] == 'U') ? 2u : ((named[0] == 'P') ? 1u : 0u);
            name += 1u;
        }
        else if (part == '\x02')
        {
            mmgr_line_ch(&key_line, '\x02');
            mmgr_line_put(&key_line, (literal < link->literal_count) ? link->literals[literal].text : "");
            type = 3u;
            literal += 1u;
        }
        else
        {
            mmgr_line_ch(&key_line, '\x03');
            mmgr_line_put(&key_line, (target < link->target_count) ? link->targets[target].text : "");
            type = 4u;
            target += 1u;
        }
        const size_t key_length = mmgr_line_finish(&key_line);
        const RenderName *const given = render_name(names, key, key_length, type);
        mmgr_line_put(&out->line, s_classes[given->type]);
        mmgr_line_decimal(&out->line, given->count, 1u);
        mark = next + 1u;
    }
    (void)mmgr_line_put_n(&out->line, &marked[mark], length - mark);
}

void window_render(const Question *question, unsigned long long first, unsigned long long length, PlatformText *out)
{
    RenderNames names;
    render_names_begun(&names);
    for (unsigned long long at = first; at < (first + length); at += 1ULL)
    {
        link_render(&question->chain[at], &names, out);
        mmgr_line_ch(&out->line, '\n');
    }
    render_names_ended(&names);
}

int windows_same(const Question *one, unsigned long long first, const Question *other, unsigned long long other_first,
                 unsigned long long length)
{
    PlatformText left;
    PlatformText right;
    identity_text_begun(&left);
    identity_text_begun(&right);
    window_render(one, first, length, &left);
    window_render(other, other_first, length, &right);
    const unsigned long long size = platform_text_size(&left);
    const int same = (size == platform_text_size(&right)) &&
                     (EMBED_CALL(memor.cmp, MemoriaCfg, .src = platform_text_bytes(&left),
                                 .other = platform_text_bytes(&right), .bytes = (size_t)size) == 0);
    platform_text_ended(&right);
    platform_text_ended(&left);
    return same;
}

unsigned long long windows_walked(const Question *question, unsigned long long first, unsigned long long most,
                                  unsigned long long *identities)
{
    RenderNames names;
    render_names_begun(&names);
    PlatformText text;
    identity_text_begun(&text);
    unsigned long long hash = 14695981039346656037ULL;
    const unsigned long long chain = question->links;
    for (unsigned long long at = first; (at < chain) && ((at - first) < most); at += 1ULL)
    {
        platform_text_reset(&text);
        link_render(&question->chain[at], &names, &text);
        mmgr_line_ch(&text.line, '\n');
        const unsigned long long size = platform_text_size(&text);
        const char *const bytes = platform_text_bytes(&text);
        for (unsigned long long each = 0ULL; each < size; each += 1ULL)
        {
            hash = (hash ^ (unsigned char)bytes[each]) * 1099511628211ULL;
        }
        if (identities != NULL)
        {
            identities[at - first] = hash;
        }
    }
    platform_text_ended(&text);
    render_names_ended(&names);
    return hash;
}

unsigned long long window_identities(const Question *question, unsigned long long first, unsigned long long *identities)
{
    (void)windows_walked(question, first, ~0ULL, identities);
    return (first < question->links) ? (question->links - first) : 0ULL;
}

void address_text(const Questions *questions, unsigned int question, unsigned int link, VerbaLinea *line)
{
    const Question *const one = &questions->question[question];
    mmgr_line_put(line, one->number.text);
    mmgr_line_ch(line, ':');
    mmgr_line_put(line, one->chain[link].position.text);
}

// the longest chain of `questions`, the links a cell of one chain's identities holds
static unsigned long long chain_longest(const Questions *questions)
{
    unsigned long long longest = 0ULL;
    for (unsigned long long at = 0ULL; at < questions->count; at += 1ULL)
    {
        longest = (questions->question[at].links > longest) ? questions->question[at].links : longest;
    }
    return longest;
}

// a cell of the temporary tier that holds the identities of the longest chain's windows from one link. NULL where the
// pool cannot meet it, said
static unsigned long long *identities_taken(const Questions *questions)
{
    const unsigned long long longest = chain_longest(questions);
    unsigned long long *const identities =
        (unsigned long long *)platform_cell_taken(((longest != 0ULL) ? longest : 1ULL) * sizeof(unsigned long long),
                                                  platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if (identities == NULL)
    {
        identity_path_said("orior_klq_identity", ": the pool cannot meet the identities of one chain");
    }
    return identities;
}

int windows_counted(const Questions *questions, Windows *windows)
{
    windows->window = NULL;
    windows->count = 0ULL;
    // a chain of L links has L(L+1)/2 windows
    unsigned long long count = 0ULL;
    for (unsigned long long at = 0ULL; at < questions->count; at += 1ULL)
    {
        const unsigned long long links = questions->question[at].links;
        count += (links * (links + 1ULL)) / 2ULL;
    }
    unsigned long long *const identities = identities_taken(questions);
    windows->window = (identities != NULL)
                          ? (Window *)platform_cell_taken(((count != 0ULL) ? count : 1ULL) * sizeof(Window),
                                                          platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY)
                          : NULL;
    if (windows->window == NULL)
    {
        platform_cell_released(identities, platform_place_g_program_owner);
        identity_path_said("orior_klq_identity", ": the pool cannot meet the windows of the chains");
        return 0;
    }
    for (unsigned int question = 0u; question < questions->count; question += 1u)
    {
        const Question *const current = &questions->question[question];
        for (unsigned int link = 0u; link < current->links; link += 1u)
        {
            const unsigned long long made = window_identities(current, link, identities);
            for (unsigned long long at = 0ULL; at < made; at += 1ULL)
            {
                const Window window = {identities[at], question, link};
                windows->window[windows->count] = window;
                windows->count += 1ULL;
            }
        }
    }
    platform_cell_released(identities, platform_place_g_program_owner);
    // the windows ordered by identity, then by where each first occurs
    identity_sorted_in_place(windows->window, windows->count, [](const Window &one, const Window &other) {
        if (one.identity != other.identity)
        {
            return one.identity < other.identity;
        }
        return (one.question != other.question) ? (one.question < other.question) : (one.link < other.link);
    });
    return 1;
}

int window_occurrence(const Windows *windows, unsigned long long identity, Occurrence *occurrence)
{
    const unsigned long long first = identity_lower(windows->window, windows->count,
                                                    [identity](const Window &one) { return one.identity < identity; });
    const unsigned long long after = identity_lower(windows->window, windows->count,
                                                    [identity](const Window &one) { return one.identity <= identity; });
    EMBED_CALL(memor.set, MemoriaCfg, .dst = occurrence, .bytes = sizeof(*occurrence), .val = 0);
    if (first == after)
    {
        return 0;
    }
    occurrence->question = windows->window[first].question;
    occurrence->link = windows->window[first].link;
    // a count of windows is far below 2^32
    occurrence->count = (unsigned int)(after - first);
    return 1;
}

// the occurrence of `identity` among the windows counted, all zero where none has it
static Occurrence window_at(const Windows *windows, unsigned long long identity)
{
    Occurrence occurrence;
    (void)window_occurrence(windows, identity, &occurrence);
    return occurrence;
}

// the windowed chains of `questions` written as the run's chains.txt in `folder`: 1, or 0 where the write failed
static int chains_write(const Questions *questions, const char *folder)
{
    Windows windows;
    if (!windows_counted(questions, &windows))
    {
        return 0;
    }
    unsigned long long *const identities = identities_taken(questions);
    unsigned long long total_links = 0ULL;
    for (unsigned long long at = 0ULL; at < questions->count; at += 1ULL)
    {
        total_links += questions->question[at].links;
    }
    // the identities of the constituents read, at max one a link, and of the wholes, one a question
    unsigned long long *const constituents = (unsigned long long *)platform_cell_taken(
        ((total_links != 0ULL) ? total_links : 1ULL) * sizeof(unsigned long long), platform_place_g_program_owner,
        PLATFORM_TIER_TEMPORARY);
    unsigned long long *const wholes = (unsigned long long *)platform_cell_taken(
        ((questions->count != 0ULL) ? questions->count : 1ULL) * sizeof(unsigned long long),
        platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    if ((identities == NULL) || (constituents == NULL) || (wholes == NULL))
    {
        platform_cell_released(wholes, platform_place_g_program_owner);
        platform_cell_released(constituents, platform_place_g_program_owner);
        platform_cell_released(identities, platform_place_g_program_owner);
        windows_ended(&windows);
        identity_path_said(folder, "/chains.txt: the pool cannot meet the identities of the chains");
        return 0;
    }
    unsigned long long constituent_identities = 0ULL;
    unsigned long long whole_count = 0ULL;
    RunWritten file;
    run_written_begun(&file, identity_path_joined(folder, "/chains.txt").text, 0);
    PlatformText primitives;
    PlatformText chains;
    identity_kept_begun(&primitives);
    identity_kept_begun(&chains);
    unsigned int primitive_count = 0u;
    unsigned int constituent_count = 0u;
    unsigned int reaffirmed = 0u;
    for (unsigned int question = 0u; question < questions->count; question += 1u)
    {
        const Question *const current = &questions->question[question];
        if (current->links == 0ULL)
        {
            continue;
        }
        const unsigned long long whole_identity = windows_walked(current, 0ULL, ~0ULL, NULL);
        const Occurrence whole = window_at(&windows, whole_identity);
        wholes[whole_count] = whole_identity;
        whole_count += 1ULL;
        reaffirmed += ((whole.question != question) || (whole.link != 0u)) ? 1u : 0u;
        PlatformText read;
        PlatformText primitive_row;
        PlatformText operand_row;
        identity_text_begun(&read);
        identity_text_begun(&primitive_row);
        identity_text_begun(&operand_row);
        for (unsigned int link = 0u; link < current->links;)
        {
            const unsigned long long made = window_identities(current, link, identities);
            unsigned int length = 0u;
            // a chain's links are far below 2^32
            for (unsigned int longest = (unsigned int)made; (longest > 0u) && (length == 0u); longest -= 1u)
            {
                const Occurrence first = window_at(&windows, identities[longest - 1u]);
                const int lower = (first.question < question) || ((first.question == question) && (first.link < link));
                // the constituents of a reaffirmation are the same, link for link, as those of its first occurrence
                if (lower && windows_same(&questions->question[first.question], first.link, current, link, longest))
                {
                    mmgr_line_ch(&read.line, ' ');
                    address_text(questions, first.question, first.link, &read.line);
                    mmgr_line_ch(&read.line, '/');
                    mmgr_line_decimal(&read.line, longest, 1u);
                    constituents[constituent_identities] = identities[longest - 1u];
                    constituent_identities += 1ULL;
                    length = longest;
                }
            }
            if (length == 0u)
            {
                mmgr_line_put(&primitives.line, "primitive ");
                address_text(questions, question, link, &primitives.line);
                mmgr_line_ch(&primitives.line, ' ');
                mmgr_line_decimal(&primitives.line, window_at(&windows, identities[0]).count, 1u);
                mmgr_line_ch(&primitives.line, ' ');
                window_render(current, link, 1ULL, &primitives);
                mmgr_line_ch(&read.line, ' ');
                address_text(questions, question, link, &read.line);
                mmgr_line_put(&read.line, "/1");
                primitive_count += 1u;
                length = 1u;
            }
            constituent_count += 1u;
            for (unsigned int within = link; within < (link + length); within += 1u)
            {
                const Occurrence primitive = window_at(&windows, windows_walked(current, within, 1ULL, NULL));
                mmgr_line_ch(&primitive_row.line, ' ');
                address_text(questions, primitive.question, primitive.link, &primitive_row.line);
                // the literal operands and the distances of the targets, in the order the link writes them
                const Link *const written = &current->chain[within];
                unsigned int literal = 0u;
                unsigned int target = 0u;
                // each mark of an operand, reached from the one before it
                const char *const marked = written->marked.text;
                for (const char *part = marked + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = marked,
                                                            .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "\x02\x03");
                     *part != '\0'; part += 1 + EMBED_CALL(cellul.span_not, CatenaFinitaCfg, .src = part + 1,
                                                           .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = "\x02\x03"))
                {
                    if (*part == '\x02')
                    {
                        mmgr_line_ch(&operand_row.line, ' ');
                        mmgr_line_put(&operand_row.line,
                                      (literal < written->literal_count) ? written->literals[literal].text : "");
                        literal += 1u;
                    }
                    else if (*part == '\x03')
                    {
                        mmgr_line_ch(&operand_row.line, ' ');
                        mmgr_line_put(&operand_row.line,
                                      (target < written->distance_count) ? written->distances[target].text : "");
                        target += 1u;
                    }
                }
            }
            link += length;
        }
        // the number of the whole is the hash of its contents in their order, the same wherever and in whichever stick
        // a read finds it
        mmgr_line_put(&chains.line, "chain ");
        address_text(questions, question, 0u, &chains.line);
        mmgr_line_ch(&chains.line, ' ');
        address_text(questions, whole.question, whole.link, &chains.line);
        mmgr_line_ch(&chains.line, ' ');
        mmgr_line_decimal(&chains.line, whole.count, 1u);
        mmgr_line_ch(&chains.line, ' ');
        mmgr_line_hex(&chains.line, whole_identity, 16u);
        mmgr_line_put(&chains.line, "\nconstituents");
        identity_text_put(&chains, &read);
        mmgr_line_put(&chains.line, "\nprimitives");
        identity_text_put(&chains, &primitive_row);
        mmgr_line_put(&chains.line, "\noperands");
        identity_text_put(&chains, &operand_row);
        mmgr_line_ch(&chains.line, '\n');
        platform_text_ended(&operand_row);
        platform_text_ended(&primitive_row);
        platform_text_ended(&read);
    }
    identity_text_put(&file.text, &primitives);
    identity_text_put(&file.text, &chains);
    platform_text_ended(&chains);
    platform_text_ended(&primitives);
    windows_ended(&windows);
    platform_cell_released(identities, platform_place_g_program_owner);
    const unsigned long long distinct_wholes = identity_numbers_settled(wholes, whole_count);
    const unsigned long long distinct_constituents = identity_numbers_settled(constituents, constituent_identities);
    platform_cell_released(wholes, platform_place_g_program_owner);
    platform_cell_released(constituents, platform_place_g_program_owner);
    const int closed = run_closed(&file);
    run_written_ended(&file);
    if (!closed)
    {
        identity_path_said(file.path.text, " could not be written");
        return 0;
    }
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "  ");
    mmgr_line_put(output_line, file.path.text);
    mmgr_line_put(output_line, ": ");
    mmgr_line_decimal(output_line, questions->count, 1u);
    mmgr_line_put(output_line, " chains, ");
    mmgr_line_decimal(output_line, distinct_wholes, 1u);
    mmgr_line_put(output_line, " whole identities, ");
    mmgr_line_decimal(output_line, reaffirmed, 1u);
    mmgr_line_put(output_line, " chains reaffirming a lower address whole, ");
    mmgr_line_decimal(output_line, primitive_count, 1u);
    mmgr_line_put(output_line, " primitives, ");
    mmgr_line_decimal(output_line, constituent_count, 1u);
    mmgr_line_put(output_line, " constituents read, ");
    mmgr_line_decimal(output_line, distinct_constituents, 1u);
    mmgr_line_put(output_line, " identities among them");
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
    return 1;
}

int slice_found(const Question *one, const Question *other, const Link **left, const Link **right)
{
    PlatformArray first;
    PlatformArray second;
    platform_array_begun(&first, sizeof(unsigned long long), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    platform_array_begun(&second, sizeof(unsigned long long), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    links_past(one, other, &first);
    links_past(other, one, &second);
    const int single = (first.count == 1ULL) && (second.count == 1ULL);
    *left = NULL;
    *right = NULL;
    if (single)
    {
        // a place links_past gives is the first occurrence of its link's exact text
        *left = &one->chain[*(const unsigned long long *)platform_array_at(&first, 0ULL)];
        *right = &other->chain[*(const unsigned long long *)platform_array_at(&second, 0ULL)];
    }
    platform_array_ended(&first);
    platform_array_ended(&second);
    return single && identity_same((*left)->registers.text, (*right)->registers.text) &&
           !identity_same((*left)->type.text, (*right)->type.text);
}

// the bytes of a slice's identity, its terminator among them: two links' types and a tab between
#define SLICE_IDENTITY_BYTES (2u * IDENTITY_LINE_BYTES)

struct SliceIdentity
{
    char text[SLICE_IDENTITY_BYTES];
};

int identity_slice(const char *listing, const char *manifest, const char *stick_path, const char *folder,
                   const char *ours)
{
    Questions questions;
    if (!questions_read(listing, manifest, &questions))
    {
        return 1;
    }
    // the stick's text, kept until the run writes the host's questions from it
    size_t stick_size = 0u;
    char *const stick_bytes = identity_file_read(stick_path, &stick_size);
    if (stick_bytes != NULL)
    {
        identity_text_mode(stick_bytes, &stick_size);
    }
    const CarrierPiece stick = {(stick_bytes != NULL) ? stick_bytes : "", stick_size};
    // the numbers the host computes, at max two a slice and one a question, and the identities of the slices
    PlatformArray queried;
    PlatformArray identities;
    platform_array_begun(&queried, sizeof(IdentityName), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    platform_array_begun(&identities, sizeof(SliceIdentity), platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    RunWritten file;
    run_written_begun(&file, identity_path_joined(folder, "/slices.txt").text, 0);
    unsigned int slices = 0u;
    for (unsigned long long one = 0ULL; one < questions.count; one += 1ULL)
    {
        for (unsigned long long other = one + 1ULL; other < questions.count; other += 1ULL)
        {
            const Link *found_left = NULL;
            const Link *found_right = NULL;
            if (!slice_found(&questions.question[one], &questions.question[other], &found_left, &found_right))
            {
                continue;
            }
            // Each slice a line: its two members, each a question and the position of its link in that question's
            // chain. Then the identity, the two links in order, the first member writing the first. Then each member's
            // context
            const int swapped = identity_order_text(found_right->type.text, found_left->type.text) < 0;
            const Question *const lead = &questions.question[swapped ? other : one];
            const Question *const rite = &questions.question[swapped ? one : other];
            const Link *const lead_link = swapped ? found_right : found_left;
            const Link *const rite_link = swapped ? found_left : found_right;
            SliceIdentity identity;
            VerbaLinea identity_line = mmgr_line_over(identity.text, sizeof(identity.text));
            mmgr_line_put(&identity_line, lead_link->type.text);
            mmgr_line_ch(&identity_line, '\t');
            mmgr_line_put(&identity_line, rite_link->type.text);
            (void)mmgr_line_finish(&identity_line);
            VerbaLinea *const line = &file.text.line;
            mmgr_line_put(line, lead->number.text);
            mmgr_line_ch(line, ':');
            mmgr_line_put(line, lead_link->position.text);
            mmgr_line_ch(line, '\t');
            mmgr_line_put(line, rite->number.text);
            mmgr_line_ch(line, ':');
            mmgr_line_put(line, rite_link->position.text);
            mmgr_line_ch(line, '\t');
            mmgr_line_put(line, identity.text);
            mmgr_line_ch(line, '\t');
            member_context(lead, lead_link->position.text, &file.text);
            mmgr_line_ch(line, '\t');
            member_context(rite, rite_link->position.text, &file.text);
            mmgr_line_ch(line, '\n');
            (void)platform_array_appended(&identities, &identity);
            slices += 1u;
            if (question_integer(lead) && question_integer(rite))
            {
                (void)platform_array_appended(&queried, &lead->number);
                (void)platform_array_appended(&queried, &rite->number);
            }
        }
    }
    const int closed = run_closed(&file);
    run_written_ended(&file);
    if (!closed)
    {
        identity_path_said(file.path.text, " could not be written");
        platform_cell_released(stick_bytes, platform_place_g_program_owner);
        platform_array_ended(&identities);
        platform_array_ended(&queried);
        questions_ended(&questions);
        return 1;
    }
    // the host computes each integer question the run channel puts to the part, a slice's member or not. That is a
    // task of register pressure, which slices with nothing, and every question the engine writes a chain for in `ours`
    for (unsigned long long at = 0ULL; at < questions.count; at += 1ULL)
    {
        const Question *const question = &questions.question[at];
        int written = 0;
        if (ours[0] != '\0')
        {
            char path[PLATFORM_PATH_BYTES];
            VerbaLinea path_line = mmgr_line_over(path, sizeof(path));
            mmgr_line_put(&path_line, ours);
            mmgr_line_ch(&path_line, '/');
            mmgr_line_put(&path_line, question->number.text);
            mmgr_line_put(&path_line, ".bin");
            (void)mmgr_line_finish(&path_line);
            unsigned long long bytes = 0ULL;
            written = platform_file_size(path, &bytes) != 0;
        }
        if (question_integer(question) && (identity_same(question->category.text, "pressure") || (written != 0)))
        {
            (void)platform_array_appended(&queried, &question->number);
        }
    }
    SliceIdentity *const identity_cells = (SliceIdentity *)identities.cell;
    identity_sorted_in_place(identity_cells, identities.count,
                             [](const SliceIdentity &one, const SliceIdentity &other) {
                                 return identity_order_text(one.text, other.text) < 0;
                             });
    unsigned long long distinct = 0ULL;
    for (unsigned long long at = 0ULL; at < identities.count; at += 1ULL)
    {
        distinct +=
            ((at == 0ULL) || !identity_same(identity_cells[at].text, identity_cells[at - 1ULL].text)) ? 1ULL : 0ULL;
    }
    platform_array_ended(&identities);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, file.path.text);
        mmgr_line_put(output_line, ": ");
        mmgr_line_decimal(output_line, questions.count, 1u);
        mmgr_line_put(output_line, " questions, ");
        mmgr_line_decimal(output_line, slices, 1u);
        mmgr_line_put(output_line, " slices, ");
        mmgr_line_decimal(output_line, distinct, 1u);
        mmgr_line_put(output_line, " identities");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    const int chained = chains_write(&questions, folder);
    const unsigned long long numbers = identity_names_settled((IdentityName *)queried.cell, queried.count);
    const int written = chained && host_questions_write(identity_path_joined(folder, "/host_questions.cpp").text, stick,
                                                        (const IdentityName *)queried.cell, numbers);
    platform_cell_released(stick_bytes, platform_place_g_program_owner);
    platform_array_ended(&queried);
    questions_ended(&questions);
    return written ? 0 : 1;
}
