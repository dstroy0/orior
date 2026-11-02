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

static CliXml s_xml;
static char s_edit[1u << 14u];

typedef struct
{
    const char *text;
    unsigned long long at;
    VerbaLinea *out;
    const CliXml *xml;
    unsigned int holder;
    const char *name;
    const char *value;
    const CliBuildLines *lines;
    const char *pom;
    const CliTextEdit *edit;
    unsigned int setting;
    VerbaLinea *err_msg;
} MavenSetCtx;
// the indent the line of the text that contains byte `at` starts with, into `out`
EMBED_INLINE void set_indent_at(const MavenSetCtx *args)
{
    const char *text = args->text;
    unsigned long long at = args->at;
    unsigned long long start = at;
    start =
        EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = text, .at = start, .set = "\n");
    const unsigned long long stop = EMBED_CALL(
        cli_text_at_size_classes.past_any, CliTextAtSizeClassesCfg, .text = text, .at = start,
        .size = start + cellul.len(&(CatenaFinitaCfg){.src = text + start, .cap = CLI_CELLS_POOL}), .set = " \t");
    mmgr_line_put_n(args->out, text + start, (size_t)(stop - start));
}

// the indent one step deeper takes in the text, the indent of the first child of its root, two spaces where it has
// none, into `out`
EMBED_INLINE void set_step(const MavenSetCtx *args)
{
    const CliXml *xml = args->xml;
    const unsigned int root = EMBED_CALL(cli_maven_xml_project_internal.xml_root, CliMavenXmlProjectCtx, .xml = xml);
    const unsigned int first = (root != CLI_XML_NONE) ? xml->element[root].first : CLI_XML_NONE;
    const unsigned long long before = args->out->at;
    if (first != CLI_XML_NONE)
    {
        EMBED_CALL(set_indent_at, MavenSetCtx, .text = xml->text, .at = xml->element[first].from, .out = args->out);
    }
    if (args->out->at == before)
    {
        mmgr_line_put(args->out, "  ");
    }
}

// the edit that sets the element `name` under `holder` to `value`: its text written over where it is, and where it is
// not, a line of it added at the end of `holder`, its text written into `s_edit`
EMBED_INLINE CliTextEdit set_element(const MavenSetCtx *args)
{
    const CliXml *xml = args->xml;
    const char *name = args->name;
    const CliBuildLines *lines = args->lines;
    const unsigned int at = EMBED_CALL(cli_maven_xml_out_internal.xml_child, CliMavenXmlOutCtx, .xml = xml,
                                       .of = args->holder, .name = name);
    if (at != CLI_XML_NONE)
    {
        const CliXmlElement *const element = &xml->element[at];
        return (CliTextEdit){
            EMBED_CALL(cli_builds_lines_at.build_place, CliBuildsLinesAtCfg, .lines = lines, .at = element->inner_from),
            EMBED_CALL(cli_builds_lines_at.build_place, CliBuildsLinesAtCfg, .lines = lines, .at = element->inner_to),
            args->value, cellul.len(&(CatenaFinitaCfg){.src = args->value, .cap = CLI_CELLS_POOL})};
    }
    const unsigned long long end = xml->element[args->holder].inner_to;
    unsigned long long line_start = end;
    line_start = EMBED_CALL(cli_text_at_size_classes.back_any, CliTextAtSizeClassesCfg, .text = xml->text,
                            .at = line_start, .set = "\n");
    const int blank = (EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg, .text = xml->text + line_start,
                                  .size = end - line_start) == end - line_start);
    const unsigned long long place = blank ? line_start : end;
    VerbaLinea edit = {s_edit, sizeof(s_edit), 0ull};
    mmgr_line_put(&edit, (place == line_start) ? "" : "\n");
    EMBED_CALL(set_indent_at, MavenSetCtx, .text = xml->text, .at = end, .out = &edit);
    EMBED_CALL(set_step, MavenSetCtx, .xml = xml, .out = &edit);
    mmgr_line_ch(&edit, '<');
    mmgr_line_put(&edit, name);
    mmgr_line_ch(&edit, '>');
    mmgr_line_put(&edit, args->value);
    mmgr_line_put(&edit, "</");
    mmgr_line_put(&edit, name);
    mmgr_line_put(&edit, ">\n");
    const CliTextPlace there =
        EMBED_CALL(cli_builds_lines_at.build_place, CliBuildsLinesAtCfg, .lines = lines, .at = place);
    return (CliTextEdit){there, there, s_edit, edit.at};
}

// the edit of the POM `pom` written as the edits of that file alone
EMBED_INLINE void set_file_edit(const MavenSetCtx *args)
{
    const CliTextEdit *edit = args->edit;
    VerbaLinea *out = args->out;
    mmgr_line_put(out, "[{\"path\":");
    EMBED_CALL(cli_json_tree_length_out.json_quoted_lossy, CliJsonTreeLengthOutCfg, .out = out,
               .text_bytes = (const unsigned char *)args->pom,
               .length = cellul.len(&(CatenaFinitaCfg){.src = args->pom, .cap = CLI_CELLS_POOL}));
    mmgr_line_put(out, ",\"edits\":[");
    EMBED_CALL(cli_edits.text_edit_write, CliEditsCfg, .edit = edit, .out = out);
    mmgr_line_put(out, "]}]");
}

// a compiler setting, at `setting` among them, set in the POM
EMBED_INLINE int set_compiler(const MavenSetCtx *args)
{
    const CliBuildLines *lines = args->lines;
    const unsigned int project =
        EMBED_CALL(cli_maven_xml_project_internal.xml_root, CliMavenXmlProjectCtx, .xml = &s_xml);
    if ((project == CLI_XML_NONE) || !EMBED_CALL(cli_maven_xml_name_internal.xml_by_name, CliMavenXmlNameCtx,
                                                 .xml = &s_xml, .element = project, .name = "project"))
    {
        mmgr_line_put(args->err_msg, "This is no POM");
        return 0;
    }
    const char *const property = g_cli_maven_compiler[args->setting][2];
    const char *const element = g_cli_maven_compiler[args->setting][3];
    const unsigned int plugin = EMBED_CALL(cli_maven_xml_project_internal.compiler_plugin, CliMavenXmlProjectCtx,
                                           .xml = &s_xml, .project = project);
    const unsigned int configuration = (plugin != CLI_XML_NONE)
                                           ? EMBED_CALL(cli_maven_xml_out_internal.xml_child, CliMavenXmlOutCtx,
                                                        .xml = &s_xml, .of = plugin, .name = "configuration")
                                           : CLI_XML_NONE;
    const unsigned int properties = EMBED_CALL(cli_maven_xml_out_internal.xml_child, CliMavenXmlOutCtx, .xml = &s_xml,
                                               .of = project, .name = "properties");
    const int in_properties =
        (properties != CLI_XML_NONE) && (EMBED_CALL(cli_maven_xml_out_internal.xml_child, CliMavenXmlOutCtx,
                                                    .xml = &s_xml, .of = properties, .name = property) != CLI_XML_NONE);
    const int in_configuration = (configuration != CLI_XML_NONE) &&
                                 (EMBED_CALL(cli_maven_xml_out_internal.xml_child, CliMavenXmlOutCtx, .xml = &s_xml,
                                             .of = configuration, .name = element) != CLI_XML_NONE);
    CliTextEdit edit;
    if (!in_properties && in_configuration)
    {
        edit = EMBED_CALL(set_element, MavenSetCtx, .xml = &s_xml, .holder = configuration, .name = element,
                          .value = args->value, .lines = lines);
    }
    else if (properties != CLI_XML_NONE)
    {
        edit = EMBED_CALL(set_element, MavenSetCtx, .xml = &s_xml, .holder = properties, .name = property,
                          .value = args->value, .lines = lines);
    }
    else
    {
        static const char *const s_after[6] = {"packaging", "version", "artifactId",
                                               "groupId",   "parent",  "modelVersion"};
        unsigned long long at = s_xml.element[project].inner_from;
        for (unsigned int one = 0u; one < 6u; one += 1u)
        {
            const unsigned int found = EMBED_CALL(cli_maven_xml_out_internal.xml_child, CliMavenXmlOutCtx,
                                                  .xml = &s_xml, .of = project, .name = s_after[one]);
            if (found != CLI_XML_NONE)
            {
                const CliXmlElement *const after = &s_xml.element[found];
                at = after->closed ? after->close_to + 1u : after->to;
                break;
            }
        }
        char unit[256];
        VerbaLinea step = {unit, sizeof(unit), 0ull};
        EMBED_CALL(set_step, MavenSetCtx, .xml = &s_xml, .out = &step);
        VerbaLinea text = {s_edit, sizeof(s_edit), 0ull};
        mmgr_line_put(&text, "\n\n");
        mmgr_line_put_n(&text, unit, step.at);
        mmgr_line_put(&text, "<properties>\n");
        mmgr_line_put_n(&text, unit, step.at);
        mmgr_line_put_n(&text, unit, step.at);
        mmgr_line_ch(&text, '<');
        mmgr_line_put(&text, property);
        mmgr_line_ch(&text, '>');
        mmgr_line_put(&text, args->value);
        mmgr_line_put(&text, "</");
        mmgr_line_put(&text, property);
        mmgr_line_put(&text, ">\n");
        mmgr_line_put_n(&text, unit, step.at);
        mmgr_line_put(&text, "</properties>");
        const CliTextPlace there =
            EMBED_CALL(cli_builds_lines_at.build_place, CliBuildsLinesAtCfg, .lines = lines, .at = at);
        edit = (CliTextEdit){there, there, s_edit, text.at};
    }
    EMBED_CALL(set_file_edit, MavenSetCtx, .pom = args->pom, .edit = &edit, .out = args->out);
    return 1;
}

// the compiler plugin's version set in the POM
EMBED_INLINE int set_plugin_version(const MavenSetCtx *args)
{
    const CliBuildLines *lines = args->lines;
    const unsigned int project =
        EMBED_CALL(cli_maven_xml_project_internal.xml_root, CliMavenXmlProjectCtx, .xml = &s_xml);
    if ((project == CLI_XML_NONE) || !EMBED_CALL(cli_maven_xml_name_internal.xml_by_name, CliMavenXmlNameCtx,
                                                 .xml = &s_xml, .element = project, .name = "project"))
    {
        mmgr_line_put(args->err_msg, "This is no POM");
        return 0;
    }
    const unsigned int plugin = EMBED_CALL(cli_maven_xml_project_internal.compiler_plugin, CliMavenXmlProjectCtx,
                                           .xml = &s_xml, .project = project);
    if (plugin == CLI_XML_NONE)
    {
        mmgr_line_put(args->err_msg, "The POM names no maven-compiler-plugin");
        return 0;
    }
    const unsigned int version = EMBED_CALL(cli_maven_xml_out_internal.xml_child, CliMavenXmlOutCtx, .xml = &s_xml,
                                            .of = plugin, .name = "version");
    if (version == CLI_XML_NONE)
    {
        mmgr_line_put(args->err_msg, "The POM's maven-compiler-plugin names no version to set");
        return 0;
    }
    const CliXmlElement *const element = &s_xml.element[version];
    const CliTextEdit edit = {
        EMBED_CALL(cli_builds_lines_at.build_place, CliBuildsLinesAtCfg, .lines = lines, .at = element->inner_from),
        EMBED_CALL(cli_builds_lines_at.build_place, CliBuildsLinesAtCfg, .lines = lines, .at = element->inner_to),
        args->value, cellul.len(&(CatenaFinitaCfg){.src = args->value, .cap = CLI_CELLS_POOL})};
    EMBED_CALL(set_file_edit, MavenSetCtx, .pom = args->pom, .edit = &edit, .out = args->out);
    return 1;
}

// one of Maven's own settings, the element `name`, set to `value` in the user's `settings.xml`, written to it
static int set_user(const MavenSetCtx *args)
{
    const char *name = args->name;
    VerbaLinea *out = args->out;
    char path[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_maven_out_internal.user_settings, CliMavenOutCtx, .out_room = path))
    {
        mmgr_line_put(args->err_msg, "No home folder contains the user's settings");
        return 0;
    }
    unsigned long long mapped_size = 0ull;
    const unsigned char *const mapped = EMBED_CALL(cli_path_internal.file_take, CliPathCtx, .path = path,
                                                   .size_pointer = &mapped_size, .owner = &g_cli_build_cells);
    // the file's bytes are read as its text; a file not there is read as empty
    const char *const text = (mapped != NULL) ? (const char *)mapped : "";
    const unsigned long long size = mapped_size;
    const unsigned long long written_room = size + CLI_MAVEN_SET_MORE;
    char *const written_cell = g_cli_cells->temporary_buf_alloc(written_room, &g_cli_build_cells);
    VerbaLinea written = mmgr_line_over(written_cell, written_room);
    unsigned int *starts = NULL;
    int settings = 1;
    if (EMBED_CALL(cli_text_size_from_u_64s.lead, CliTextSizeFromU64sCfg, .text = text, .size = size) == size)
    {
        mmgr_line_put(&written, "<settings xmlns=\"http://maven.apache.org/SETTINGS/1.0.0\">\n  <");
        mmgr_line_put(&written, name);
        mmgr_line_ch(&written, '>');
        mmgr_line_put(&written, args->value);
        mmgr_line_put(&written, "</");
        mmgr_line_put(&written, name);
        mmgr_line_put(&written, ">\n</settings>\n");
    }
    else
    {
        EMBED_CALL(cli_maven_xml_text_internal.xml_parse, CliMavenXmlTextCtx, .text = text, .size = size,
                   .xml_out = &s_xml);
        const unsigned int root =
            EMBED_CALL(cli_maven_xml_project_internal.xml_root, CliMavenXmlProjectCtx, .xml = &s_xml);
        settings = (root != CLI_XML_NONE) && EMBED_CALL(cli_maven_xml_name_internal.xml_by_name, CliMavenXmlNameCtx,
                                                        .xml = &s_xml, .element = root, .name = "settings");
        if (settings)
        {
            CliBuildLines lines;
            starts = EMBED_CALL(cli_builds_text_size.build_lines_take, CliBuildsTextSizeCfg, .text = text, .size = size,
                                .lines_out = &lines);
            const CliTextEdit edit = EMBED_CALL(set_element, MavenSetCtx, .xml = &s_xml, .holder = root, .name = name,
                                                .value = args->value, .lines = &lines);
            (void)EMBED_CALL(cli_edits_out.text_apply, CliEditsOutCfg, .text = text, .size = size, .edits = &edit,
                             .count = 1u, .out = &written);
        }
    }
    char parent[CLI_PATH_MAX_LEN];
    const int done =
        settings && mmgr_line_ok(&written) &&
        !(EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = path, .out = parent) &&
          !EMBED_CALL(query_folder_context_internal.folder_init, QueryFolderContextCtx, .folder = parent)) &&
        EMBED_CALL(query_path_bytes_size_internal.file_write, QueryPathBytesSizeCtx, .path = path, .bytes = written.out,
                   .size = written.at);
    g_cli_cells->persistent_buf_release(starts, &g_cli_build_cells);
    g_cli_cells->persistent_buf_release(written.out, &g_cli_build_cells);
    EMBED_CALL(cli_owner_internal.file_free, CliOwnerCtx, .bytes_bytes_other = mapped, .owner = &g_cli_build_cells);
    if (done)
    {
        mmgr_line_put(out, "[]");
    }
    else
    {
        mmgr_line_put(args->err_msg, path);
        mmgr_line_put(args->err_msg, settings ? ": it could not be written" : " contains no <settings>");
    }
    return done;
}

int cli_maven_set(const CliMavenKeyCfg *args)
{
    const char *text = args->text;
    unsigned long long size = args->size;
    static char s_value[1u << 13u];
    unsigned long long from = 0ull;
    unsigned long long to = 0ull;
    EMBED_CALL(cli_text_size_from_u_64s.trim, CliTextSizeFromU64sCfg, .text = args->value,
               .size = cellul.len(&(CatenaFinitaCfg){.src = args->value, .cap = CLI_CELLS_POOL}), .from_u64s = &from,
               .to_u64s = &to);
    if (to - from >= sizeof(s_value))
    {
        to = from + sizeof(s_value) - 1u;
    }
    memor.cpy(&(MemoriaCfg){.dst = s_value, .src = args->value + from, .bytes = (size_t)(to - from)});
    s_value[to - from] = '\0';
    int done = 0;
    unsigned int setting = 4u;
    for (unsigned int at = 0u; at < 4u; at += 1u)
    {
        setting = (cellul.eq(&(CatenaFinitaCfg){
                      .src = args->key, .other = g_cli_maven_compiler[at][0], .cap = CLI_CELLS_POOL}))
                      ? at
                      : setting;
    }
    if ((setting < 4u) ||
        (cellul.eq(&(CatenaFinitaCfg){.src = args->key, .other = "compiler-plugin", .cap = CLI_CELLS_POOL})))
    {
        EMBED_CALL(cli_maven_xml_text_internal.xml_parse, CliMavenXmlTextCtx, .text = text, .size = size,
                   .xml_out = &s_xml);
        CliBuildLines lines;
        unsigned int *const starts = EMBED_CALL(cli_builds_text_size.build_lines_take, CliBuildsTextSizeCfg,
                                                .text = text, .size = size, .lines_out = &lines);
        done = (setting < 4u)
                   ? EMBED_CALL(set_compiler, MavenSetCtx, .pom = args->pom, .setting = setting, .value = s_value,
                                .lines = &lines, .out = args->out, .err_msg = args->err_msg)
                   : EMBED_CALL(set_plugin_version, MavenSetCtx, .pom = args->pom, .value = s_value, .lines = &lines,
                                .out = args->out, .err_msg = args->err_msg);
        g_cli_cells->persistent_buf_release(starts, &g_cli_build_cells);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->key, .other = "local-repository", .cap = CLI_CELLS_POOL}))
    {
        done = EMBED_CALL(set_user, MavenSetCtx, .name = "localRepository", .value = s_value, .out = args->out,
                          .err_msg = args->err_msg);
    }
    else if ((cellul.eq(&(CatenaFinitaCfg){.src = args->key, .other = "offline", .cap = CLI_CELLS_POOL})) &&
             ((cellul.eq(&(CatenaFinitaCfg){.src = s_value, .other = "true", .cap = CLI_CELLS_POOL})) ||
              (cellul.eq(&(CatenaFinitaCfg){.src = s_value, .other = "false", .cap = CLI_CELLS_POOL}))))
    {
        done = EMBED_CALL(set_user, MavenSetCtx, .name = "offline", .value = s_value, .out = args->out,
                          .err_msg = args->err_msg);
    }
    else if (cellul.eq(&(CatenaFinitaCfg){.src = args->key, .other = "offline", .cap = CLI_CELLS_POOL}))
    {
        mmgr_line_ch(args->err_msg, '`');
        mmgr_line_put(args->err_msg, s_value);
        mmgr_line_put(args->err_msg, "` is no answer: true or false");
    }
    else
    {
        mmgr_line_ch(args->err_msg, '`');
        mmgr_line_put(args->err_msg, args->key);
        mmgr_line_put(args->err_msg, "` is no setting that can be set");
    }
    EMBED_CALL(cli_maven_xml_text_internal.xml_free, CliMavenXmlTextCtx, .xml_out = &s_xml);
    return done;
}
