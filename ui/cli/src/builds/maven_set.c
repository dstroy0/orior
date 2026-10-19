// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_set.c: a setting set. A compiler setting is written to the property that sets it where the POM's properties
// hold it or the plugin's configuration does not, and to the configuration where only it holds it; a POM with no
// properties is given them after its coordinates, a step deeper than its root. The compiler plugin's version is
// written over its own. Maven's own settings are written to the user's `settings.xml`, made where there is none. An
// element added under another goes on a line of its own, at the depth of the other's first child
#include "../text/edits.h"
#include "../text/json_tree.h"
#include "maven.h"
#include "maven_internal.h"

// the most bytes of the user's settings read and written
#define CLI_MAVEN_SET_TEXT (1u << 20u)

static CliLock s_set_lock = CLI_LOCK_INIT;
static CliXml s_xml;
static char s_text[CLI_MAVEN_SET_TEXT];
static char s_written[CLI_MAVEN_SET_TEXT + 4096u];
static char s_edit[1u << 14u];
static unsigned int s_starts[1u << 16u];

// the indent the line of the text that holds byte `at` starts with, into `out`
static void set_indent_at(const char *text, unsigned long long at, CliLine *out)
{
    unsigned long long start = at;
    while ((start > 0u) && (text[start - 1u] != '\n'))
    {
        start -= 1u;
    }
    while ((text[start] == ' ') || (text[start] == '\t'))
    {
        cli_line_character(out, text[start]);
        start += 1u;
    }
}

// the indent one step deeper takes in the text, the indent of the first child of its root, two spaces where it has
// none, into `out`
static void set_step(const CliXml *xml, CliLine *out)
{
    const unsigned int root = cli_xml_root(xml);
    const unsigned int first = (root != CLI_XML_NONE) ? xml->element[root].first : CLI_XML_NONE;
    const unsigned long long before = out->at;
    if (first != CLI_XML_NONE)
    {
        set_indent_at(xml->text, xml->element[first].from, out);
    }
    if (out->at == before)
    {
        cli_line_text(out, "  ");
    }
}

// the edit that sets the element `name` under `holder` to `value`: its text written over where it is, and where it is
// not, a line of it added at the end of `holder`, its text written into `s_edit`
static CliTextEdit set_element(const CliXml *xml, unsigned int holder, const char *name, const char *value,
                               const CliBuildLines *lines)
{
    const unsigned int at = cli_xml_child(xml, holder, name);
    if (at != CLI_XML_NONE)
    {
        const CliXmlElement *const element = &xml->element[at];
        return (CliTextEdit){cli_build_place(lines, element->inner_from), cli_build_place(lines, element->inner_to),
                             value, cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL})};
    }
    const unsigned long long end = xml->element[holder].inner_to;
    unsigned long long line_start = end;
    while ((line_start > 0u) && (xml->text[line_start - 1u] != '\n'))
    {
        line_start -= 1u;
    }
    const int blank = (cli_text_lead(xml->text + line_start, end - line_start) == end - line_start);
    const unsigned long long place = blank ? line_start : end;
    CliLine edit = {s_edit, sizeof(s_edit), 0ull};
    cli_line_text(&edit, (place == line_start) ? "" : "\n");
    set_indent_at(xml->text, end, &edit);
    set_step(xml, &edit);
    cli_line_character(&edit, '<');
    cli_line_text(&edit, name);
    cli_line_character(&edit, '>');
    cli_line_text(&edit, value);
    cli_line_text(&edit, "</");
    cli_line_text(&edit, name);
    cli_line_text(&edit, ">\n");
    const CliTextPlace there = cli_build_place(lines, place);
    return (CliTextEdit){there, there, s_edit, edit.at};
}

// the edit of the POM `pom` written as the one file's edits
static void set_file_edit(const char *pom, const CliTextEdit *edit, CliLine *out)
{
    cli_line_text(out, "[{\"path\":");
    cli_json_quoted_lossy(out, (const unsigned char *)pom,
                          cellul.len(&(CatenaFinitaCfg){.src = pom, .cap = CLI_CELLS_POOL}));
    cli_line_text(out, ",\"edits\":[");
    cli_text_edit_written(edit, out);
    cli_line_text(out, "]}]");
}

// a compiler setting, at `setting` among them, set in the POM
static int set_compiler(const char *pom, unsigned int setting, const char *value, const CliBuildLines *lines,
                        CliLine *out, CliLine *said)
{
    const unsigned int project = cli_xml_root(&s_xml);
    if ((project == CLI_XML_NONE) || !cli_xml_named(&s_xml, project, "project"))
    {
        cli_line_text(said, "This is no POM");
        return 0;
    }
    const char *const property = g_cli_maven_compiler[setting][2];
    const char *const element = g_cli_maven_compiler[setting][3];
    const unsigned int plugin = cli_maven_compiler_plugin(&s_xml, project);
    const unsigned int configuration =
        (plugin != CLI_XML_NONE) ? cli_xml_child(&s_xml, plugin, "configuration") : CLI_XML_NONE;
    const unsigned int properties = cli_xml_child(&s_xml, project, "properties");
    const int in_properties =
        (properties != CLI_XML_NONE) && (cli_xml_child(&s_xml, properties, property) != CLI_XML_NONE);
    const int in_configuration =
        (configuration != CLI_XML_NONE) && (cli_xml_child(&s_xml, configuration, element) != CLI_XML_NONE);
    CliTextEdit edit;
    if (!in_properties && in_configuration)
    {
        edit = set_element(&s_xml, configuration, element, value, lines);
    }
    else if (properties != CLI_XML_NONE)
    {
        edit = set_element(&s_xml, properties, property, value, lines);
    }
    else
    {
        static const char *const s_after[6] = {"packaging", "version", "artifactId",
                                               "groupId",   "parent",  "modelVersion"};
        unsigned long long at = s_xml.element[project].inner_from;
        for (unsigned int one = 0u; one < 6u; one += 1u)
        {
            const unsigned int found = cli_xml_child(&s_xml, project, s_after[one]);
            if (found != CLI_XML_NONE)
            {
                const CliXmlElement *const after = &s_xml.element[found];
                at = after->closed ? after->close_to + 1u : after->to;
                break;
            }
        }
        char unit[256];
        CliLine step = {unit, sizeof(unit), 0ull};
        set_step(&s_xml, &step);
        CliLine text = {s_edit, sizeof(s_edit), 0ull};
        cli_line_text(&text, "\n\n");
        cli_line_counted(&text, unit, step.at);
        cli_line_text(&text, "<properties>\n");
        cli_line_counted(&text, unit, step.at);
        cli_line_counted(&text, unit, step.at);
        cli_line_character(&text, '<');
        cli_line_text(&text, property);
        cli_line_character(&text, '>');
        cli_line_text(&text, value);
        cli_line_text(&text, "</");
        cli_line_text(&text, property);
        cli_line_text(&text, ">\n");
        cli_line_counted(&text, unit, step.at);
        cli_line_text(&text, "</properties>");
        const CliTextPlace there = cli_build_place(lines, at);
        edit = (CliTextEdit){there, there, s_edit, text.at};
    }
    set_file_edit(pom, &edit, out);
    return 1;
}

// the compiler plugin's version set in the POM
static int set_plugin_version(const char *pom, const char *value, const CliBuildLines *lines, CliLine *out,
                              CliLine *said)
{
    const unsigned int project = cli_xml_root(&s_xml);
    if ((project == CLI_XML_NONE) || !cli_xml_named(&s_xml, project, "project"))
    {
        cli_line_text(said, "This is no POM");
        return 0;
    }
    const unsigned int plugin = cli_maven_compiler_plugin(&s_xml, project);
    if (plugin == CLI_XML_NONE)
    {
        cli_line_text(said, "The POM names no maven-compiler-plugin");
        return 0;
    }
    const unsigned int version = cli_xml_child(&s_xml, plugin, "version");
    if (version == CLI_XML_NONE)
    {
        cli_line_text(said, "The POM's maven-compiler-plugin names no version to set");
        return 0;
    }
    const CliXmlElement *const element = &s_xml.element[version];
    const CliTextEdit edit = {cli_build_place(lines, element->inner_from), cli_build_place(lines, element->inner_to),
                              value, cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL})};
    set_file_edit(pom, &edit, out);
    return 1;
}

// one of Maven's own settings, the element `name`, set to `value` in the user's `settings.xml`, written to it
static int set_user(const char *name, const char *value, CliLine *out, CliLine *said)
{
    char path[CLI_PATH_LONGEST];
    if (!cli_maven_user_settings(path))
    {
        cli_line_text(said, "No home folder holds the user's settings");
        return 0;
    }
    unsigned long long size = qry_member_file_read(path, (unsigned char *)s_text, sizeof(s_text));
    size = (size == QRY_MEMBER_UNREAD) ? 0ull : size;
    CliLine written = {s_written, sizeof(s_written), 0ull};
    if (cli_text_lead(s_text, size) == size)
    {
        cli_line_text(&written, "<settings xmlns=\"http://maven.apache.org/SETTINGS/1.0.0\">\n  <");
        cli_line_text(&written, name);
        cli_line_character(&written, '>');
        cli_line_text(&written, value);
        cli_line_text(&written, "</");
        cli_line_text(&written, name);
        cli_line_text(&written, ">\n</settings>\n");
    }
    else
    {
        cli_xml_parse(s_text, size, &s_xml);
        const unsigned int root = cli_xml_root(&s_xml);
        if ((root == CLI_XML_NONE) || !cli_xml_named(&s_xml, root, "settings"))
        {
            cli_line_text(said, path);
            cli_line_text(said, " holds no <settings>");
            return 0;
        }
        CliBuildLines lines;
        cli_build_lines_of(s_text, size, s_starts, sizeof(s_starts) / sizeof(s_starts[0]), &lines);
        const CliTextEdit edit = set_element(&s_xml, root, name, value, &lines);
        (void)cli_text_applied(s_text, size, &edit, 1u, &written);
    }
    char parent[CLI_PATH_LONGEST];
    if (!cli_line_fits(&written) || (cli_path_parent(path, parent) && !query_folder_made(parent)) ||
        !query_file_written(path, s_written, written.at))
    {
        cli_line_text(said, path);
        cli_line_text(said, ": it could not be written");
        return 0;
    }
    cli_line_text(out, "[]");
    return 1;
}

int cli_maven_set(const char *pom, const char *text, unsigned long long size, const char *key, const char *value,
                  CliLine *out, CliLine *said)
{
    static char s_value[1u << 13u];
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    cli_text_trimmed(value, cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL}), &from, &to);
    if (to - from >= sizeof(s_value))
    {
        to = from + sizeof(s_value) - 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_value, .src = value + from, .bytes = (size_t)(to - from)});
    s_value[to - from] = '\0';
    cli_lock_take(&s_set_lock);
    int done = 0;
    unsigned int setting = 4u;
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        setting =
            (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = g_cli_maven_compiler[at][0], .cap = CLI_CELLS_POOL}))
                ? at
                : setting;
    }
    if ((setting < 4u) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "compiler-plugin", .cap = CLI_CELLS_POOL})))
    {
        cli_xml_parse(text, size, &s_xml);
        CliBuildLines lines;
        cli_build_lines_of(text, size, s_starts, sizeof(s_starts) / sizeof(s_starts[0]), &lines);
        done = (setting < 4u) ? set_compiler(pom, setting, s_value, &lines, out, said)
                              : set_plugin_version(pom, s_value, &lines, out, said);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "local-repository", .cap = CLI_CELLS_POOL}))
    {
        done = set_user("localRepository", s_value, out, said);
    }
    else if ((cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "offline", .cap = CLI_CELLS_POOL})) &&
             ((cellul.eq(&(CatenaFinitaCfg){.src = s_value, .other = "true", .cap = CLI_CELLS_POOL})) ||
              (cellul.eq(&(CatenaFinitaCfg){.src = s_value, .other = "false", .cap = CLI_CELLS_POOL}))))
    {
        done = set_user("offline", s_value, out, said);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "offline", .cap = CLI_CELLS_POOL}))
    {
        cli_line_character(said, '`');
        cli_line_text(said, s_value);
        cli_line_text(said, "` is no answer: true or false");
    }
    else
    {
        cli_line_character(said, '`');
        cli_line_text(said, key);
        cli_line_text(said, "` is no setting that can be set");
    }
    cli_lock_give(&s_set_lock);
    return done;
}
