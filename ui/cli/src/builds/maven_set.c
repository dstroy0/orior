// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_set.c: a setting set. A compiler setting is written to the property that sets it where the POM's properties
// contain it or the plugin's configuration does not, and to the configuration where only it contains it; a POM with no
// properties is given them after its coordinates, a step deeper than its root. The compiler plugin's version is
// written over its own. Maven's own settings are written to the user's `settings.xml`, made where there is none. An
// element added under another goes on a line of its own, at the depth of the other's first child
#include "../command_line/cells.h"
#include "../text/edits.h"
#include "../text/json_tree.h"
#include "../text/text.h"
#include "maven.h"
#include "maven_internal.h"

// the bytes the user's settings written are taken with beyond what they contained
#define CLI_MAVEN_SET_MORE 4096ull

static CliLock s_set_lock = CLI_LOCK_INIT;
static CliXml s_xml;
static char s_edit[1u << 14u];

// the indent the line of the text that contains byte `at` starts with, into `out`
static void set_indent_at(const char *text, unsigned long long at, VerbaLinea *out)
{
    unsigned long long start = at;
    start = cli_text_back_any(text, start, "\n");
    const unsigned long long stop = cli_text_past_any(
        text, start, start + cellul.len(&(CatenaFinitaCfg){.src = text + start, .cap = CLI_CELLS_POOL}), " \t");
    mmgr_line_put_n(out, text + start, (size_t)(stop - start));
}

// the indent one step deeper takes in the text, the indent of the first child of its root, two spaces where it has
// none, into `out`
static void set_step(const CliXml *xml, VerbaLinea *out)
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
        mmgr_line_put(out, "  ");
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
    line_start = cli_text_back_any(xml->text, line_start, "\n");
    const int blank = (cli_text_lead(xml->text + line_start, end - line_start) == end - line_start);
    const unsigned long long place = blank ? line_start : end;
    VerbaLinea edit = {s_edit, sizeof(s_edit), 0ull};
    mmgr_line_put(&edit, (place == line_start) ? "" : "\n");
    set_indent_at(xml->text, end, &edit);
    set_step(xml, &edit);
    mmgr_line_ch(&edit, '<');
    mmgr_line_put(&edit, name);
    mmgr_line_ch(&edit, '>');
    mmgr_line_put(&edit, value);
    mmgr_line_put(&edit, "</");
    mmgr_line_put(&edit, name);
    mmgr_line_put(&edit, ">\n");
    const CliTextPlace there = cli_build_place(lines, place);
    return (CliTextEdit){there, there, s_edit, edit.at};
}

// the edit of the POM `pom` written as the edits of that file alone
static void set_file_edit(const char *pom, const CliTextEdit *edit, VerbaLinea *out)
{
    mmgr_line_put(out, "[{\"path\":");
    cli_json_quoted_lossy(out, (const unsigned char *)pom,
                          cellul.len(&(CatenaFinitaCfg){.src = pom, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"edits\":[");
    cli_text_edit_written(edit, out);
    mmgr_line_put(out, "]}]");
}

// a compiler setting, at `setting` among them, set in the POM
static int set_compiler(const char *pom, unsigned int setting, const char *value, const CliBuildLines *lines,
                        VerbaLinea *out, VerbaLinea *err_msg)
{
    const unsigned int project = cli_xml_root(&s_xml);
    if ((project == CLI_XML_NONE) || !cli_xml_named(&s_xml, project, "project"))
    {
        mmgr_line_put(err_msg, "This is no POM");
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
        VerbaLinea step = {unit, sizeof(unit), 0ull};
        set_step(&s_xml, &step);
        VerbaLinea text = {s_edit, sizeof(s_edit), 0ull};
        mmgr_line_put(&text, "\n\n");
        mmgr_line_put_n(&text, unit, step.at);
        mmgr_line_put(&text, "<properties>\n");
        mmgr_line_put_n(&text, unit, step.at);
        mmgr_line_put_n(&text, unit, step.at);
        mmgr_line_ch(&text, '<');
        mmgr_line_put(&text, property);
        mmgr_line_ch(&text, '>');
        mmgr_line_put(&text, value);
        mmgr_line_put(&text, "</");
        mmgr_line_put(&text, property);
        mmgr_line_put(&text, ">\n");
        mmgr_line_put_n(&text, unit, step.at);
        mmgr_line_put(&text, "</properties>");
        const CliTextPlace there = cli_build_place(lines, at);
        edit = (CliTextEdit){there, there, s_edit, text.at};
    }
    set_file_edit(pom, &edit, out);
    return 1;
}

// the compiler plugin's version set in the POM
static int set_plugin_version(const char *pom, const char *value, const CliBuildLines *lines, VerbaLinea *out,
                              VerbaLinea *err_msg)
{
    const unsigned int project = cli_xml_root(&s_xml);
    if ((project == CLI_XML_NONE) || !cli_xml_named(&s_xml, project, "project"))
    {
        mmgr_line_put(err_msg, "This is no POM");
        return 0;
    }
    const unsigned int plugin = cli_maven_compiler_plugin(&s_xml, project);
    if (plugin == CLI_XML_NONE)
    {
        mmgr_line_put(err_msg, "The POM names no maven-compiler-plugin");
        return 0;
    }
    const unsigned int version = cli_xml_child(&s_xml, plugin, "version");
    if (version == CLI_XML_NONE)
    {
        mmgr_line_put(err_msg, "The POM's maven-compiler-plugin names no version to set");
        return 0;
    }
    const CliXmlElement *const element = &s_xml.element[version];
    const CliTextEdit edit = {cli_build_place(lines, element->inner_from), cli_build_place(lines, element->inner_to),
                              value, cellul.len(&(CatenaFinitaCfg){.src = value, .cap = CLI_CELLS_POOL})};
    set_file_edit(pom, &edit, out);
    return 1;
}

// one of Maven's own settings, the element `name`, set to `value` in the user's `settings.xml`, written to it
static int set_user(const char *name, const char *value, VerbaLinea *out, VerbaLinea *err_msg)
{
    char path[CLI_PATH_MAX_LEN];
    if (!cli_maven_user_settings(path))
    {
        mmgr_line_put(err_msg, "No home folder contains the user's settings");
        return 0;
    }
    unsigned long long mapped_size = 0ull;
    const unsigned char *const mapped = cli_file_taken(path, &mapped_size, &g_cli_build_cells);
    // the file's bytes are read as its text; a file not there is read as empty
    const char *const text = (mapped != NULL) ? (const char *)mapped : "";
    const unsigned long long size = mapped_size;
    const unsigned long long written_room = size + CLI_MAVEN_SET_MORE;
    char *const written_cell = g_cli_cells->temporary_buf_alloc(written_room, &g_cli_build_cells);
    VerbaLinea written = mmgr_line_over(written_cell, written_room);
    unsigned int *starts = NULL;
    int settings = 1;
    if (cli_text_lead(text, size) == size)
    {
        mmgr_line_put(&written, "<settings xmlns=\"http://maven.apache.org/SETTINGS/1.0.0\">\n  <");
        mmgr_line_put(&written, name);
        mmgr_line_ch(&written, '>');
        mmgr_line_put(&written, value);
        mmgr_line_put(&written, "</");
        mmgr_line_put(&written, name);
        mmgr_line_put(&written, ">\n</settings>\n");
    }
    else
    {
        cli_xml_parse(text, size, &s_xml);
        const unsigned int root = cli_xml_root(&s_xml);
        settings = (root != CLI_XML_NONE) && cli_xml_named(&s_xml, root, "settings");
        if (settings)
        {
            CliBuildLines lines;
            starts = cli_build_lines_taken(text, size, &lines);
            const CliTextEdit edit = set_element(&s_xml, root, name, value, &lines);
            (void)cli_text_applied(text, size, &edit, 1u, &written);
        }
    }
    char parent[CLI_PATH_MAX_LEN];
    const int done = settings && mmgr_line_ok(&written) &&
                     !(cli_path_parent(path, parent) && !query_folder_made(parent)) &&
                     query_file_written(path, written.out, written.at);
    g_cli_cells->persistent_buf_release(starts, &g_cli_build_cells);
    g_cli_cells->persistent_buf_release(written.out, &g_cli_build_cells);
    cli_file_given(mapped, &g_cli_build_cells);
    if (done)
    {
        mmgr_line_put(out, "[]");
    }
    else
    {
        mmgr_line_put(err_msg, path);
        mmgr_line_put(err_msg, settings ? ": it could not be written" : " contains no <settings>");
    }
    return done;
}

int cli_maven_set(const char *pom, const char *text, unsigned long long size, const char *key, const char *value,
                  VerbaLinea *out, VerbaLinea *err_msg)
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
        unsigned int *const starts = cli_build_lines_taken(text, size, &lines);
        done = (setting < 4u) ? set_compiler(pom, setting, s_value, &lines, out, err_msg)
                              : set_plugin_version(pom, s_value, &lines, out, err_msg);
        g_cli_cells->persistent_buf_release(starts, &g_cli_build_cells);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "local-repository", .cap = CLI_CELLS_POOL}))
    {
        done = set_user("localRepository", s_value, out, err_msg);
    }
    else if ((cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "offline", .cap = CLI_CELLS_POOL})) &&
             ((cellul.eq(&(CatenaFinitaCfg){.src = s_value, .other = "true", .cap = CLI_CELLS_POOL})) ||
              (cellul.eq(&(CatenaFinitaCfg){.src = s_value, .other = "false", .cap = CLI_CELLS_POOL}))))
    {
        done = set_user("offline", s_value, out, err_msg);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = key, .other = "offline", .cap = CLI_CELLS_POOL}))
    {
        mmgr_line_ch(err_msg, '`');
        mmgr_line_put(err_msg, s_value);
        mmgr_line_put(err_msg, "` is no answer: true or false");
    }
    else
    {
        mmgr_line_ch(err_msg, '`');
        mmgr_line_put(err_msg, key);
        mmgr_line_put(err_msg, "` is no setting that can be set");
    }
    cli_xml_given(&s_xml);
    cli_lock_give(&s_set_lock);
    return done;
}
