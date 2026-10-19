// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_xml.c: an XML file read as far as it goes: comments, CDATA, processing instructions and declarations passed
// whole; each opening tag an element under the one left open, a tag ending `/>` closing itself; each closing tag
// closing the nearest element of its name left open and those left open inside it, which are not closed; and an
// element's text with its CDATA's markers and its entities read. Elements past CLI_XML_ELEMENTS, comments past
// CLI_XML_COMMENTS and what kept the file from reading past CLI_XML_ERRORS are not kept
#include "maven_internal.h"

// what is passed whole: its opening and its closing
static const char *const s_skip_open[4] = {"<!--", "<![CDATA[", "<?", "<!"};
static const char *const s_skip_close[4] = {"-->", "]]>", "?>", ">"};

int cli_xml_name_letter(unsigned int letter)
{
    return cli_text_alphanumeric(letter) || (letter == '_') || (letter == ':') || (letter == '.') || (letter == '-');
}

// what kept the file from reading kept
static void xml_error(CliXml *xml, unsigned long long from, unsigned long long to, unsigned int kind,
                      unsigned int element, unsigned long long name_from, unsigned long long name_to)
{
    if (xml->errors < CLI_XML_ERRORS)
    {
        xml->error[xml->errors] = (CliXmlError){from, to, kind, element, name_from, name_to};
        xml->errors += 1u;
    }
}

// the `>` that ends a tag whose name ends at `from`, past the quotes of its attributes: 1 with it, or 0 where none does
static int xml_tag_end(const CliXml *xml, unsigned long long from, unsigned long long *end)
{
    char quote = '\0';
    for (unsigned long long at = from; at < xml->size; at += 1u)
    {
        const char one = xml->text[at];
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
static int xml_same_name(const CliXml *xml, unsigned int element, unsigned long long from, unsigned long long to)
{
    const CliXmlElement *const one = &xml->element[element];
    return (one->to - one->from == to - from) &&
           (memor.cmp(&(MemoriaCfg){
                .src = xml->text + one->from, .other = xml->text + from, .bytes = (size_t)(to - from)}) == 0);
}

// a closing tag named `from` to `to`, its `<` at `lt`, closing the nearest element of its name left open
static void xml_closing(CliXml *xml, unsigned long long lt, unsigned long long from, unsigned long long to)
{
    unsigned int depth = xml->opens;
    for (unsigned int at = xml->opens; at > 0u; at -= 1u)
    {
        if (xml_same_name(xml, xml->open[at - 1u], from, to))
        {
            depth = at - 1u;
            break;
        }
    }
    if (depth == xml->opens)
    {
        xml_error(xml, from, to, CLI_XML_CLOSES_NONE, CLI_XML_NONE, from, to);
        return;
    }
    for (unsigned int left = depth + 1u; left < xml->opens; left += 1u)
    {
        const CliXmlElement *const element = &xml->element[xml->open[left]];
        xml_error(xml, element->from, element->to, CLI_XML_NOT_CLOSED, xml->open[left], 0ull, 0ull);
    }
    CliXmlElement *const element = &xml->element[xml->open[depth]];
    xml->opens = depth;
    element->inner_to = lt;
    element->close_from = from;
    element->close_to = to;
    element->closed = 1;
}

// an opening tag named `from` to `to`, ending at `gt`, an element under the one left open
static void xml_opening(CliXml *xml, unsigned long long from, unsigned long long to, unsigned long long gt)
{
    if (xml->elements == CLI_XML_ELEMENTS)
    {
        return;
    }
    const unsigned int index = xml->elements;
    const unsigned int parent = (xml->opens > 0u) ? xml->open[xml->opens - 1u] : CLI_XML_NONE;
    const int lone = (gt > 0u) && (xml->text[gt - 1u] == '/');
    xml->element[index] =
        (CliXmlElement){from, to,     gt + 1u,      gt + 1u,      lone ? from : 0ull, lone ? to : 0ull,
                        lone, parent, CLI_XML_NONE, CLI_XML_NONE, CLI_XML_NONE,       0u};
    xml->elements += 1u;
    if (parent != CLI_XML_NONE)
    {
        CliXmlElement *const above = &xml->element[parent];
        if (above->first == CLI_XML_NONE)
        {
            above->first = index;
        }
        else
        {
            xml->element[above->last].next = index;
        }
        above->last = index;
        above->children += 1u;
    }
    if (!lone)
    {
        xml->open[xml->opens] = index;
        xml->opens += 1u;
    }
}

void cli_xml_parse(const char *text, unsigned long long size, CliXml *xml)
{
    xml->text = text;
    xml->size = size;
    xml->elements = 0u;
    xml->comments = 0u;
    xml->errors = 0u;
    xml->opens = 0u;
    for (unsigned long long at = 0ull; at < size;)
    {
        const char *const found = memor.chr(&(MemoriaCfg){.src = text + at, .val = '<', .bytes = (size_t)(size - at)});
        if (found == NULL)
        {
            return;
        }
        const unsigned long long lt = (unsigned long long)(found - text);
        const unsigned long long left = size - lt;
        unsigned int skip = 4u;
        for (unsigned int one = 0u; (skip == 4u) && (one < 4u); one += 1u)
        {
            skip = cli_text_starts(found, left, s_skip_open[one]) ? one : skip;
        }
        if (skip < 4u)
        {
            const unsigned long long open =
                cellul.len(&(CatenaFinitaCfg){.src = s_skip_open[skip], .cap = CLI_CELLS_POOL});
            const unsigned long long close =
                cellul.len(&(CatenaFinitaCfg){.src = s_skip_close[skip], .cap = CLI_CELLS_POOL});
            unsigned long long end = 0ull;
            if (!cli_text_find(found + open, left - open, s_skip_close[skip], close, &end))
            {
                xml_error(xml, lt, lt + open, skip, CLI_XML_NONE, 0ull, 0ull);
                return;
            }
            at = lt + open + end + close;
            if ((skip == 0u) && (xml->comments < CLI_XML_COMMENTS))
            {
                xml->comment_from[xml->comments] = lt;
                xml->comment_to[xml->comments] = at;
                xml->comments += 1u;
            }
            continue;
        }
        const int closing = cli_text_starts(found, left, "</");
        const unsigned long long name_from = lt + (closing ? 2u : 1u);
        unsigned long long name_to = name_from;
        while (name_to < size)
        {
            unsigned long long length = 0ull;
            if (!cli_xml_name_letter(cli_text_letter(text, size, name_to, &length)))
            {
                break;
            }
            name_to += length;
        }
        unsigned long long gt = 0ull;
        if (!xml_tag_end(xml, name_to, &gt))
        {
            xml_error(xml, lt, (name_to > lt + 1u) ? name_to : lt + 1u, CLI_XML_TAG_OPEN, CLI_XML_NONE, 0ull, 0ull);
            return;
        }
        at = gt + 1u;
        if (name_to == name_from)
        {
            xml_error(xml, lt, lt + 1u, CLI_XML_NO_ELEMENT, CLI_XML_NONE, 0ull, 0ull);
            continue;
        }
        if (closing)
        {
            xml_closing(xml, lt, name_from, name_to);
            continue;
        }
        xml_opening(xml, name_from, name_to, gt);
    }
}

void cli_xml_error_said(const CliXml *xml, const CliXmlError *error, CliLine *out)
{
    if (error->kind < 4u)
    {
        cli_line_text(out, "This `");
        cli_line_text(out, s_skip_open[error->kind]);
        cli_line_text(out, "` is not closed with `");
        cli_line_text(out, s_skip_close[error->kind]);
        cli_line_character(out, '`');
    }
    else if (error->kind == CLI_XML_TAG_OPEN)
    {
        cli_line_text(out, "This tag is not closed with `>`");
    }
    else if (error->kind == CLI_XML_NO_ELEMENT)
    {
        cli_line_text(out, "This `<` starts no element");
    }
    else if (error->kind == CLI_XML_NOT_CLOSED)
    {
        const CliXmlElement *const element = &xml->element[error->element];
        cli_line_text(out, "`<");
        cli_line_counted(out, xml->text + element->from, element->to - element->from);
        cli_line_text(out, ">` is not closed");
    }
    else
    {
        cli_line_text(out, "`</");
        cli_line_counted(out, xml->text + error->name_from, error->name_to - error->name_from);
        cli_line_text(out, ">` closes no element");
    }
}

unsigned int cli_xml_root(const CliXml *xml)
{
    for (unsigned int at = 0u; at < xml->elements; at += 1u)
    {
        if (xml->element[at].parent == CLI_XML_NONE)
        {
            return at;
        }
    }
    return CLI_XML_NONE;
}

int cli_xml_named(const CliXml *xml, unsigned int element, const char *name)
{
    const CliXmlElement *const one = &xml->element[element];
    return (cellul.len(&(CatenaFinitaCfg){.src = name, .cap = CLI_CELLS_POOL}) == one->to - one->from) &&
           (memor.cmp(&(MemoriaCfg){
                .src = xml->text + one->from, .other = name, .bytes = (size_t)(one->to - one->from)}) == 0);
}

unsigned int cli_xml_child(const CliXml *xml, unsigned int of, const char *name)
{
    for (unsigned int at = xml->element[of].first; at != CLI_XML_NONE; at = xml->element[at].next)
    {
        if (cli_xml_named(xml, at, name))
        {
            return at;
        }
    }
    return CLI_XML_NONE;
}

int cli_xml_text(const CliXml *xml, unsigned int of, CliLine *out)
{
    const CliXmlElement *const element = &xml->element[of];
    if ((element->children > 0u) || !element->closed || (element->inner_to < element->inner_from))
    {
        return 0;
    }
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    cli_text_trimmed(xml->text + element->inner_from, element->inner_to - element->inner_from, &from, &to);
    const char *raw = xml->text + element->inner_from + from;
    unsigned long long size = to - from;
    if (cli_text_starts(raw, size, "<![CDATA[") && cli_text_ends(raw + 9u, size - 9u, "]]>"))
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
            entity = cli_text_starts(raw + at, size - at, s_entities[one]) ? one : entity;
        }
        if (entity < 5u)
        {
            cli_line_character(out, s_letters[entity]);
            at += cellul.len(&(CatenaFinitaCfg){.src = s_entities[entity], .cap = CLI_CELLS_POOL});
            continue;
        }
        cli_line_character(out, raw[at]);
        at += 1u;
    }
    return 1;
}

int cli_xml_text_of(const CliXml *xml, unsigned int of, const char *name, CliLine *out)
{
    const unsigned int child = (of != CLI_XML_NONE) ? cli_xml_child(xml, of, name) : CLI_XML_NONE;
    return (child != CLI_XML_NONE) && cli_xml_text(xml, child, out);
}

unsigned int cli_xml_at_path(const CliXml *xml, unsigned int of, const char *const *path, unsigned int count)
{
    unsigned int at = of;
    for (unsigned int step = 0u; (at != CLI_XML_NONE) && (step < count); step += 1u)
    {
        at = cli_xml_child(xml, at, path[step]);
    }
    return at;
}

int cli_xml_inside_free(const CliXml *xml, unsigned int at)
{
    for (unsigned int parent = xml->element[at].parent; parent != CLI_XML_NONE; parent = xml->element[parent].parent)
    {
        if (cli_xml_named(xml, parent, "configuration") || cli_xml_named(xml, parent, "properties"))
        {
            return 1;
        }
    }
    return 0;
}
