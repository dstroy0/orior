// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_xml.c: an XML file read as far as it goes: comments, CDATA, processing instructions and declarations passed
// whole; each opening tag an element under the one left open, a tag ending `/>` closing itself; each closing tag
// closing the nearest element of its name left open and those left open inside it, which are not closed; and an
// element's text with its CDATA's markers and its entities read. Elements past CLI_XML_ELEMENTS, comments past
// CLI_XML_COMMENTS and what prevented the file from reading past CLI_XML_ERRORS are not stored
#include "maven_internal.h"

// what is passed whole: its opening and its closing
static const char *const s_skip_open[4] = {"<!--", "<![CDATA[", "<?", "<!"};
static const char *const s_skip_close[4] = {"-->", "]]>", "?>", ">"};

int cli_xml_name_letter(const CliMavenCtx *args)
{
    return EMBED_CALL(cli_text_table.alphanumeric, CliTextCfg, .letter = args->letter) || (args->letter == '_') ||
           (args->letter == ':') || (args->letter == '.') || (args->letter == '-');
}

void cli_xml_free(const CliMavenXmlTextCtx *args)
{
    CliXml *xml = args->xml_out;
    g_cli_cells->persistent_buf_release(xml->element, &g_cli_xml_cells);
    *xml = (CliXml){xml->text, 0ull, NULL, 0u, 0u, NULL, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u};
}

typedef struct
{
    unsigned long long tags;
    unsigned int most;
    CliXml *xml;
    const char *text;
    unsigned long long size;
    unsigned long long from;
    unsigned long long to;
    unsigned int kind;
    unsigned int element;
    unsigned long long name_from;
    unsigned long long name_to;
    const CliXml *xml_read;
    unsigned long long *end;
    unsigned long long lt;
    unsigned long long gt;
} MavenXmlCtx;
// the capacity of a list a text containing `tags` `<`s can fill, no more than `most`
EMBED_INLINE unsigned int xml_capacity(const MavenXmlCtx *args)
{
    // a count no more than `most`, which an unsigned int stores
    return (args->tags < args->most) ? (unsigned int)args->tags : args->most;
}

// `xml` made to read `size` bytes of `text`, its lists' capacities in one cell sized to the `<`s the text contains,
// each no more than its most: 1, or 0 with nothing to be stored where the cell is refused
EMBED_INLINE int xml_reserve(const MavenXmlCtx *args)
{
    CliXml *xml = args->xml;
    unsigned long long tags = 0ull;
    for (unsigned long long at = 0ull; at < args->size; at += 1ull)
    {
        tags += (args->text[at] == '<') ? 1ull : 0ull;
    }
    const unsigned int elements = EMBED_CALL(xml_capacity, MavenXmlCtx, .tags = tags, .most = CLI_XML_ELEMENTS);
    const unsigned int comments = EMBED_CALL(xml_capacity, MavenXmlCtx, .tags = tags, .most = CLI_XML_COMMENTS);
    const unsigned int errors = EMBED_CALL(xml_capacity, MavenXmlCtx, .tags = tags + 1ull, .most = CLI_XML_ERRORS);
    // the lists laid one after another, the widest first, so that each starts where its kind may stand
    const unsigned long long element_bytes = (unsigned long long)elements * sizeof(CliXmlElement);
    const unsigned long long comment_bytes = (unsigned long long)comments * sizeof(unsigned long long);
    const unsigned long long error_bytes = (unsigned long long)errors * sizeof(CliXmlError);
    const unsigned long long open_bytes = (unsigned long long)elements * sizeof(unsigned int);
    unsigned char *const cell = g_cli_cells->persistent_buf_alloc(
        element_bytes + (2ull * comment_bytes) + error_bytes + open_bytes + 1ull, &g_cli_xml_cells);
    *xml = (CliXml){args->text, args->size, NULL, 0u, 0u, NULL, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u};
    if (cell == NULL)
    {
        return 0;
    }
    // each list's place in the cell is a multiple of its kind's width from the cell's start, as laid out above
    xml->element = (CliXmlElement *)(void *)cell;
    xml->element_capacity = elements;
    xml->comment_from = (unsigned long long *)(void *)(cell + element_bytes);
    xml->comment_to = (unsigned long long *)(void *)(cell + element_bytes + comment_bytes);
    xml->comment_capacity = comments;
    xml->error = (CliXmlError *)(void *)(cell + element_bytes + (2ull * comment_bytes));
    xml->error_capacity = errors;
    xml->open = (unsigned int *)(void *)(cell + element_bytes + (2ull * comment_bytes) + error_bytes);
    return 1;
}

// what prevented the file from reading, recorded
EMBED_INLINE void xml_error(const MavenXmlCtx *args)
{
    if (args->xml->errors < args->xml->error_capacity)
    {
        args->xml->error[args->xml->errors] =
            (CliXmlError){args->from, args->to, args->kind, args->element, args->name_from, args->name_to};
        args->xml->errors += 1u;
    }
}

// the `>` that ends a tag whose name ends at `from`, past the quotes of its attributes: 1 with it, or 0 where none does
EMBED_INLINE int xml_tag_end(const MavenXmlCtx *args)
{
    unsigned long long *end = args->end;
    char quote = '\0';
    for (unsigned long long at = args->from; at < args->xml_read->size; at += 1u)
    {
        const char one = args->xml_read->text[at];
        if (quote != '\0')
        {
            quote = (one == quote) ? '\0' : quote;
        }
        else if ((one == '"') || (one == '\''))
        {
            quote = one;
        }
        else if (one == '>')
        {
            *end = at;
            return 1;
        }
        else if (one == '<')
        {
            return 0;
        }
    }
    return 0;
}

// whether the element at `element` is named as the bytes `from` to `to` of the text
EMBED_INLINE int xml_same_name(const MavenXmlCtx *args)
{
    const CliXmlElement *const one = &args->xml_read->element[args->element];
    return (one->to - one->from == args->to - args->from) &&
           (memor.cmp(&(MemoriaCfg){.src = args->xml_read->text + one->from,
                                    .other = args->xml_read->text + args->from,
                                    .bytes = (size_t)(args->to - args->from)}) == 0);
}

// a closing tag named `from` to `to`, its `<` at `lt`, closing the nearest element of its name left open
EMBED_INLINE void xml_closing(const MavenXmlCtx *args)
{
    unsigned int depth = args->xml->opens;
    for (unsigned int at = args->xml->opens; at > 0u; at -= 1u)
    {
        if (EMBED_CALL(xml_same_name, MavenXmlCtx, .xml_read = args->xml, .element = args->xml->open[at - 1u],
                       .from = args->from, .to = args->to))
        {
            depth = at - 1u;
            break;
        }
    }
    if (depth == args->xml->opens)
    {
        EMBED_CALL(xml_error, MavenXmlCtx, .xml = args->xml, .from = args->from, .to = args->to,
                   .kind = CLI_XML_CLOSES_NONE, .element = CLI_XML_NONE, .name_from = args->from, .name_to = args->to);
        return;
    }
    for (unsigned int left = depth + 1u; left < args->xml->opens; left += 1u)
    {
        const CliXmlElement *const element = &args->xml->element[args->xml->open[left]];
        EMBED_CALL(xml_error, MavenXmlCtx, .xml = args->xml, .from = element->from, .to = element->to,
                   .kind = CLI_XML_NOT_CLOSED, .element = args->xml->open[left], .name_from = 0ull, .name_to = 0ull);
    }
    CliXmlElement *const element = &args->xml->element[args->xml->open[depth]];
    args->xml->opens = depth;
    element->inner_to = args->lt;
    element->close_from = args->from;
    element->close_to = args->to;
    element->closed = 1;
}

// an opening tag named `from` to `to`, ending at `gt`, an element under the one left open
EMBED_INLINE void xml_opening(const MavenXmlCtx *args)
{
    if (args->xml->elements == args->xml->element_capacity)
    {
        return;
    }
    const unsigned int index = args->xml->elements;
    const unsigned int parent = (args->xml->opens > 0u) ? args->xml->open[args->xml->opens - 1u] : CLI_XML_NONE;
    const int lone = (args->gt > 0u) && (args->xml->text[args->gt - 1u] == '/');
    args->xml->element[index] = (CliXmlElement){args->from,
                                                args->to,
                                                args->gt + 1u,
                                                args->gt + 1u,
                                                lone ? args->from : 0ull,
                                                lone ? args->to : 0ull,
                                                lone,
                                                parent,
                                                CLI_XML_NONE,
                                                CLI_XML_NONE,
                                                CLI_XML_NONE,
                                                0u};
    args->xml->elements += 1u;
    if (parent != CLI_XML_NONE)
    {
        CliXmlElement *const above = &args->xml->element[parent];
        if (above->first == CLI_XML_NONE)
        {
            above->first = index;
        }
        else
        {
            args->xml->element[above->last].next = index;
        }
        above->last = index;
        above->children += 1u;
    }
    if (!lone)
    {
        args->xml->open[args->xml->opens] = index;
        args->xml->opens += 1u;
    }
}

void cli_xml_parse(const CliMavenXmlTextCtx *args)
{
    if (!EMBED_CALL(xml_reserve, MavenXmlCtx, .xml = args->xml_out, .text = args->text, .size = args->size))
    {
        return;
    }
    for (unsigned long long at = 0ull; at < args->size;)
    {
        const char *const found =
            memor.chr(&(MemoriaCfg){.src = args->text + at, .val = '<', .bytes = (size_t)(args->size - at)});
        if (found == NULL)
        {
            return;
        }
        const unsigned long long lt = (unsigned long long)(found - args->text);
        const unsigned long long left = args->size - lt;
        unsigned int skip = 4u;
        for (unsigned int one = 0u; (skip == 4u) && (one < 4u); one += 1u)
        {
            skip = EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = found, .size = left,
                              .prefix = s_skip_open[one])
                       ? one
                       : skip;
        }
        if (skip < 4u)
        {
            const unsigned long long open =
                cellul.len(&(CatenaFinitaCfg){.src = s_skip_open[skip], .cap = CLI_CELLS_POOL});
            const unsigned long long close =
                cellul.len(&(CatenaFinitaCfg){.src = s_skip_close[skip], .cap = CLI_CELLS_POOL});
            unsigned long long end = 0ull;
            if (!EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = found + open, .length_value = left - open,
                            .required = s_skip_close[skip], .size = close, .at_pointer = &end))
            {
                EMBED_CALL(xml_error, MavenXmlCtx, .xml = args->xml_out, .from = lt, .to = lt + open, .kind = skip,
                           .element = CLI_XML_NONE, .name_from = 0ull, .name_to = 0ull);
                return;
            }
            at = lt + open + end + close;
            if ((skip == 0u) && (args->xml_out->comments < args->xml_out->comment_capacity))
            {
                args->xml_out->comment_from[args->xml_out->comments] = lt;
                args->xml_out->comment_to[args->xml_out->comments] = at;
                args->xml_out->comments += 1u;
            }
            continue;
        }
        const int closing =
            EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = found, .size = left, .prefix = "</");
        const unsigned long long name_from = lt + (closing ? 2u : 1u);
        unsigned long long name_to = name_from;
        while (name_to < args->size)
        {
            unsigned long long length = 0ull;
            if (!EMBED_CALL(cli_maven_internal.xml_name_letter, CliMavenCtx,
                            .letter = EMBED_CALL(cli_text_at.letter, CliTextAtCfg, .text = args->text,
                                                 .size = args->size, .at = name_to, .length = &length)))
            {
                break;
            }
            name_to += length;
        }
        unsigned long long gt = 0ull;
        if (!EMBED_CALL(xml_tag_end, MavenXmlCtx, .xml_read = args->xml_out, .from = name_to, .end = &gt))
        {
            EMBED_CALL(xml_error, MavenXmlCtx, .xml = args->xml_out, .from = lt,
                       .to = (name_to > lt + 1u) ? name_to : lt + 1u, .kind = CLI_XML_TAG_OPEN, .element = CLI_XML_NONE,
                       .name_from = 0ull, .name_to = 0ull);
            return;
        }
        at = gt + 1u;
        if (name_to == name_from)
        {
            EMBED_CALL(xml_error, MavenXmlCtx, .xml = args->xml_out, .from = lt, .to = lt + 1u,
                       .kind = CLI_XML_NO_ELEMENT, .element = CLI_XML_NONE, .name_from = 0ull, .name_to = 0ull);
            continue;
        }
        if (closing)
        {
            EMBED_CALL(xml_closing, MavenXmlCtx, .xml = args->xml_out, .lt = lt, .from = name_from, .to = name_to);
            continue;
        }
        EMBED_CALL(xml_opening, MavenXmlCtx, .xml = args->xml_out, .from = name_from, .to = name_to, .gt = gt);
    }
}

void cli_xml_error_write(const CliMavenXmlErrorCtx *args)
{
    if (args->error->kind < 4u)
    {
        mmgr_line_put(args->out, "This `");
        mmgr_line_put(args->out, s_skip_open[args->error->kind]);
        mmgr_line_put(args->out, "` is not closed with `");
        mmgr_line_put(args->out, s_skip_close[args->error->kind]);
        mmgr_line_ch(args->out, '`');
    }
    else if (args->error->kind == CLI_XML_TAG_OPEN)
    {
        mmgr_line_put(args->out, "This tag is not closed with `>`");
    }
    else if (args->error->kind == CLI_XML_NO_ELEMENT)
    {
        mmgr_line_put(args->out, "This `<` starts no element");
    }
    else if (args->error->kind == CLI_XML_NOT_CLOSED)
    {
        const CliXmlElement *const element = &args->xml->element[args->error->element];
        mmgr_line_put(args->out, "`<");
        mmgr_line_put_n(args->out, args->xml->text + element->from, element->to - element->from);
        mmgr_line_put(args->out, ">` is not closed");
    }
    else
    {
        mmgr_line_put(args->out, "`</");
        mmgr_line_put_n(args->out, args->xml->text + args->error->name_from,
                        args->error->name_to - args->error->name_from);
        mmgr_line_put(args->out, ">` closes no element");
    }
}

unsigned int cli_xml_root(const CliMavenXmlProjectCtx *args)
{
    for (unsigned int at = 0u; at < args->xml->elements; at += 1u)
    {
        if (args->xml->element[at].parent == CLI_XML_NONE)
        {
            return at;
        }
    }
    return CLI_XML_NONE;
}

int cli_xml_by_name(const CliMavenXmlNameCtx *args)
{
    const CliXmlElement *const one = &args->xml->element[args->element];
    return (cellul.len(&(CatenaFinitaCfg){.src = args->name, .cap = CLI_CELLS_POOL}) == one->to - one->from) &&
           (memor.cmp(&(MemoriaCfg){
                .src = args->xml->text + one->from, .other = args->name, .bytes = (size_t)(one->to - one->from)}) == 0);
}

unsigned int cli_xml_child(const CliMavenXmlOutCtx *args)
{
    for (unsigned int at = args->xml->element[args->of].first; at != CLI_XML_NONE; at = args->xml->element[at].next)
    {
        if (EMBED_CALL(cli_maven_xml_name_internal.xml_by_name, CliMavenXmlNameCtx, .xml = args->xml, .element = at,
                       .name = args->name))
        {
            return at;
        }
    }
    return CLI_XML_NONE;
}

int cli_xml_text(const CliMavenXmlOutCtx *args)
{
    const CliXmlElement *const element = &args->xml->element[args->of];
    if ((element->children > 0u) || !element->closed || (element->inner_to < element->inner_from))
    {
        return 0;
    }
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = args->xml->text + element->inner_from,
               .size = element->inner_to - element->inner_from, .from_u64s = &from, .to_u64s = &to);
    const char *raw = args->xml->text + element->inner_from + from;
    unsigned long long size = to - from;
    if (EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = raw, .size = size,
                   .prefix = "<![CDATA[") &&
        EMBED_CALL(cli_text_size_suffix.ends, CliTextSizeSuffixCfg, .text = raw + 9u, .size = size - 9u,
                   .suffix = "]]>"))
    {
        raw += 9u;
        size -= 12u;
    }
    static const char *const s_entities[5] = {"&lt;", "&gt;", "&quot;", "&apos;", "&amp;"};
    static const char s_letters[5] = {'<', '>', '"', '\'', '&'};
    for (unsigned long long at = 0ull; at < size;)
    {
        unsigned int entity = 5u;
        for (unsigned int one = 0u; (entity == 5u) && (one < 5u); one += 1u)
        {
            entity = EMBED_CALL(cli_text_size_prefix.starts, CliTextSizePrefixCfg, .text = raw + at, .size = size - at,
                                .prefix = s_entities[one])
                         ? one
                         : entity;
        }
        if (entity < 5u)
        {
            mmgr_line_ch(args->out, s_letters[entity]);
            at += cellul.len(&(CatenaFinitaCfg){.src = s_entities[entity], .cap = CLI_CELLS_POOL});
            continue;
        }
        mmgr_line_ch(args->out, raw[at]);
        at += 1u;
    }
    return 1;
}

int cli_xml_text_get(const CliMavenXmlOutCtx *args)
{
    const unsigned int child = (args->of != CLI_XML_NONE) ? cli_maven_xml_out_internal.xml_child(args) : CLI_XML_NONE;
    return (child != CLI_XML_NONE) && EMBED_CALL(cli_maven_xml_out_internal.xml_text, CliMavenXmlOutCtx,
                                                 .xml = args->xml, .of = child, .out = args->out);
}

unsigned int cli_xml_at_path(const CliMavenXmlCtx *args)
{
    unsigned int at = args->of;
    for (unsigned int step = 0u; (at != CLI_XML_NONE) && (step < args->count); step += 1u)
    {
        at = EMBED_CALL(cli_maven_xml_out_internal.xml_child, CliMavenXmlOutCtx, .xml = args->xml, .of = at,
                        .name = args->path_pointer[step]);
    }
    return at;
}

int cli_xml_inside_free(const CliMavenXmlAtCtx *args)
{
    for (unsigned int parent = args->xml->element[args->at].parent; parent != CLI_XML_NONE;
         parent = args->xml->element[parent].parent)
    {
        if (EMBED_CALL(cli_maven_xml_name_internal.xml_by_name, CliMavenXmlNameCtx, .xml = args->xml, .element = parent,
                       .name = "configuration") ||
            EMBED_CALL(cli_maven_xml_name_internal.xml_by_name, CliMavenXmlNameCtx, .xml = args->xml, .element = parent,
                       .name = "properties"))
        {
            return 1;
        }
    }
    return 0;
}
