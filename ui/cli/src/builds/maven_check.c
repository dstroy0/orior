// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_check.c: a POM checked: what prevented its XML from reading and each element it leaves open; its model version;
// each element where its parent may not contain it, with the elements near its name, outside a `configuration` and a
// `properties`; each property it names outside its comments that neither it, its parents nor Maven set, where every
// parent was read; its dependencies; a parent that is a snapshot; and each execution's phase no lifecycle has
#include "../text/text.h"
#include "maven_internal.h"

typedef struct
{
    const CliMavenCheck *check;
    const VerbaLinea *msg;
    unsigned long long from;
    unsigned long long to;
    unsigned int severity;
    const char *code;
    const CliXml *xml;
    unsigned long long at;
    unsigned int index;
} MavenCheckCtx;

// a finding over the bytes `from` to `to` that reports `msg`
EMBED_INLINE unsigned int check_finding_append(const MavenCheckCtx *args)
{
    unsigned long long from = args->from;
    unsigned long long to = args->to;
    unsigned int severity = args->severity;
    const char *code = args->code;
    return EMBED_CALL(cli_builds_lines.build_finding, CliBuildsLinesCfg, .found = args->check->found,
                      .message = args->msg, .lines = args->check->lines, .from = from, .to = to, .severity = severity,
                      .code = code);
}

void cli_maven_update(const CliMavenSizeCheckCtx *args)
{
    unsigned int finding = args->finding;
    VerbaLinea title = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
    mmgr_line_put(&title, "Update the snapshot ");
    mmgr_line_put_n(&title, args->coordinates, args->size);
    char line[CLI_PATH_MAX_LEN * 2u];
    VerbaLinea command = {line, sizeof(line), 0ULL};
    mmgr_line_put(&command, args->check->program);
    mmgr_line_put(&command, " -U dependency:get -Dartifact=");
    mmgr_line_put_n(&command, args->coordinates, args->size);
    EMBED_CALL(cli_builds_found.build_run_fix, CliBuildsFoundCfg, .found = args->check->found, .finding = finding,
               .title = &title, .folder = args->check->displayed,
               .folder_size = cellul.len(&(CatenaFinitaCfg){.src = args->check->displayed, .cap = CLI_POOL_BYTES}),
               .line_text = line, .line_size = command.at);
}

// each element where its parent may not contain it, with the fixes that name it as one its parent contains
EMBED_INLINE void check_elements(const MavenCheckCtx *args)
{
    const CliXml *const xml = args->check->xml;
    for (unsigned int index = 0u; index < xml->elements; index += 1u)
    {
        const CliXmlElement *const element = &xml->element[index];
        if (element->parent == CLI_XML_NONE)
        {
            continue;
        }
        const CliXmlElement *const parent = &xml->element[element->parent];
        unsigned int count = 0u;
        const char *const *const allowed =
            EMBED_CALL(cli_maven_size_name_internal.schema, CliMavenSizeNameCtx, .name = xml->text + parent->from,
                       .size = parent->to - parent->from, .count_pointer = &count);
        if ((allowed == NULL) ||
            EMBED_CALL(cli_maven_xml_at_internal.xml_inside_free, CliMavenXmlAtCtx, .xml = xml, .at = index))
        {
            continue;
        }
        const char *const name = xml->text + element->from;
        const unsigned long long size = element->to - element->from;
        int known = 0;
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            known |= (cellul.len(&(CatenaFinitaCfg){.src = allowed[at], .cap = CLI_POOL_BYTES}) == size) &&
                     (memor.cmp(&(MemoriaCfg){.src = allowed[at], .other = name, .bytes = (size_t)size}) == 0);
        }
        if (known)
        {
            continue;
        }
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
        mmgr_line_put(&msg, "`<");
        mmgr_line_put_n(&msg, name, size);
        mmgr_line_put(&msg, ">` is no element of `<");
        mmgr_line_put_n(&msg, xml->text + parent->from, parent->to - parent->from);
        mmgr_line_put(&msg, ">`");
        const unsigned int finding =
            EMBED_CALL(check_finding_append, MavenCheckCtx, .check = args->check, .msg = &msg, .from = element->from,
                       .to = element->to, .severity = CLI_BUILD_WARNING, .code = "pom-element");
        CliBuildAdjacent adjacent;
        EMBED_CALL(cli_builds_size.build_adjacent_begin, CliBuildsSizeCfg, .adjacent = &adjacent, .name = name,
                   .size = size);
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            EMBED_CALL(cli_builds_size_adjacent.build_adjacent_offer, CliBuildsSizeAdjacentCfg, .adjacent = &adjacent,
                       .one = allowed[at],
                       .size = cellul.len(&(CatenaFinitaCfg){.src = allowed[at], .cap = CLI_POOL_BYTES}));
        }
        for (unsigned int at = 0u; at < adjacent.count; at += 1u)
        {
            VerbaLinea title =
                EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
            mmgr_line_put(&title, "Change to <");
            mmgr_line_put_n(&title, adjacent.adjacent[at], adjacent.adjacent_size[at]);
            mmgr_line_ch(&title, '>');
            if (element->closed && (element->close_from != element->from))
            {
                EMBED_CALL(cli_builds_text.build_edit_fix_twice, CliBuildsTextCfg, .found = args->check->found,
                           .finding = finding, .title = &title, .lines = args->check->lines, .from = element->from,
                           .to = element->to, .also_from = element->close_from, .also_to = element->close_to,
                           .text = adjacent.adjacent[at], .size = adjacent.adjacent_size[at]);
            }
            else
            {
                EMBED_CALL(cli_builds_text.build_edit_fix, CliBuildsTextCfg, .found = args->check->found,
                           .finding = finding, .title = &title, .lines = args->check->lines, .from = element->from,
                           .to = element->to, .text = adjacent.adjacent[at], .size = adjacent.adjacent_size[at]);
            }
        }
    }
}

// whether the byte at `at` stands in one of the POM's comments
EMBED_INLINE int check_in_comment(const MavenCheckCtx *args)
{
    for (unsigned int one = 0u; one < args->xml->comments; one += 1u)
    {
        if ((args->xml->comment_from[one] <= args->at) && (args->at < args->xml->comment_to[one]))
        {
            return 1;
        }
    }
    return 0;
}

// each property the POM names outside its comments that neither it, its parents nor Maven set
EMBED_INLINE void check_properties(const MavenCheckCtx *args)
{
    const CliXml *const xml = args->check->xml;
    const char *const text = xml->text;
    const CliMavenModel *const model = args->check->model;
    for (unsigned long long at = 0ULL; at + 1u < xml->size;)
    {
        unsigned long long off = 0ULL;
        if (!EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = text + at, .length_value = xml->size - at,
                        .required = "${", .size = 2u, .at_pointer = &off))
        {
            return;
        }
        const unsigned long long start = at + off;
        at = start + 2u;
        if (EMBED_CALL(check_in_comment, MavenCheckCtx, .xml = xml, .at = start))
        {
            continue;
        }
        unsigned long long end = at;
        end = EMBED_CALL(cli_text_at_size_classes.to_any, CliTextAtSizeClassesCfg, .text = text, .at = end,
                         .size = xml->size, .set = "}\n<");
        if ((end >= xml->size) || (text[end] != '}'))
        {
            continue;
        }
        unsigned long long value = 0ULL;
        if ((end == at) ||
            (EMBED_CALL(cli_maven_size_internal.property, CliMavenSizeCtx, .model_read = model, .name = text + at,
                        .size = end - at, .value_size = &value) != NULL) ||
            EMBED_CALL(cli_maven_size_name_internal.free, CliMavenSizeNameCtx, .name = text + at, .size = end - at))
        {
            continue;
        }
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
        mmgr_line_put(&msg, "No property `");
        mmgr_line_put_n(&msg, text + at, end - at);
        mmgr_line_put(&msg, "` is set, by the POM, its parents or Maven");
        const unsigned int finding =
            EMBED_CALL(check_finding_append, MavenCheckCtx, .check = args->check, .msg = &msg, .from = at, .to = end,
                       .severity = CLI_BUILD_WARNING, .code = "pom-property");
        CliBuildAdjacent adjacent;
        EMBED_CALL(cli_builds_size.build_adjacent_begin, CliBuildsSizeCfg, .adjacent = &adjacent, .name = text + at,
                   .size = end - at);
        for (unsigned int one = 0u; one < model->properties; one += 1u)
        {
            EMBED_CALL(cli_builds_size_adjacent.build_adjacent_offer, CliBuildsSizeAdjacentCfg, .adjacent = &adjacent,
                       .one = model->text + model->name_at[one], .size = model->name_size[one]);
        }
        for (unsigned int one = 0u; one < adjacent.count; one += 1u)
        {
            VerbaLinea title =
                EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
            mmgr_line_put(&title, "Change to ${");
            mmgr_line_put_n(&title, adjacent.adjacent[one], adjacent.adjacent_size[one]);
            mmgr_line_ch(&title, '}');
            EMBED_CALL(cli_builds_text.build_edit_fix, CliBuildsTextCfg, .found = args->check->found,
                       .finding = finding, .title = &title, .lines = args->check->lines, .from = at, .to = end,
                       .text = adjacent.adjacent[one], .size = adjacent.adjacent_size[one]);
        }
    }
}

// a parent that is a snapshot noted, with the fix that fetches its newest build
EMBED_INLINE void check_parent(const MavenCheckCtx *args)
{
    const CliMavenCheck *check = args->check;
    const CliXml *const xml = check->xml;
    static char s_texts[3][1u << 12u];
    static const char *const s_names[3] = {"groupId", "artifactId", "version"};
    unsigned long long sizes[3] = {0ULL, 0ULL, 0ULL};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        VerbaLinea one = {s_texts[at], sizeof(s_texts[at]), 0ULL};
        if (!EMBED_CALL(cli_maven_xml_out_internal.xml_text_get, CliMavenXmlOutCtx, .xml = xml, .of = args->index,
                        .name = s_names[at], .out = &one) ||
            !mmgr_line_ok(&one))
        {
            return;
        }
        sizes[at] = one.at;
    }
    if (!EMBED_CALL(cli_builds_size_version.build_snapshot, CliBuildsSizeVersionCfg, .version = s_texts[2],
                    .size = sizes[2]))
    {
        return;
    }
    const unsigned int version = EMBED_CALL(cli_maven_xml_out_internal.xml_child, CliMavenXmlOutCtx, .xml = xml,
                                            .of = args->index, .name = "version");
    const CliXmlElement *const at = &xml->element[(version != CLI_XML_NONE) ? version : args->index];
    static char s_coordinates[1u << 14u];
    VerbaLinea coordinates = {s_coordinates, sizeof(s_coordinates), 0ULL};
    for (unsigned int one = 0u; one < 3u; one += 1u)
    {
        mmgr_line_put(&coordinates, (one > 0u) ? ":" : "");
        mmgr_line_put_n(&coordinates, s_texts[one], sizes[one]);
    }
    VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = check->found);
    mmgr_line_put(&msg, "The parent `");
    mmgr_line_put_n(&msg, s_coordinates, coordinates.at);
    mmgr_line_put(&msg, "` is a snapshot, whose newest build Maven fetches with `-U`");
    const unsigned int finding =
        EMBED_CALL(check_finding_append, MavenCheckCtx, .check = check, .msg = &msg, .from = at->inner_from,
                   .to = at->inner_to, .severity = CLI_BUILD_NOTE, .code = "snapshot");
    mmgr_line_put(&coordinates, ":pom");
    EMBED_CALL(cli_maven_size_check_internal.update, CliMavenSizeCheckCtx, .check = check, .finding = finding,
               .coordinates = s_coordinates, .size = coordinates.at);
}

// an execution's phase no lifecycle of Maven's has, with the phases near it
EMBED_INLINE void check_phase(const MavenCheckCtx *args)
{
    const CliXml *const xml = args->check->xml;
    static char s_phase[1u << 12u];
    VerbaLinea phase = {s_phase, sizeof(s_phase), 0ULL};
    unsigned long long found = 0ULL;
    if (!EMBED_CALL(cli_maven_xml_out_internal.xml_text, CliMavenXmlOutCtx, .xml = xml, .of = args->index,
                    .out = &phase) ||
        !mmgr_line_ok(&phase) ||
        EMBED_CALL(cli_text_size_at.find, CliTextSizeAtCfg, .text = s_phase, .length_value = phase.at, .required = "${",
                   .size = 2u, .at_pointer = &found))
    {
        return;
    }
    for (unsigned int at = 0u; at < 29u; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = g_cli_maven_phases[at], .cap = CLI_POOL_BYTES}) == phase.at) &&
            (memor.cmp(&(MemoriaCfg){.src = g_cli_maven_phases[at], .other = s_phase, .bytes = phase.at}) == 0))
        {
            return;
        }
    }
    const CliXmlElement *const element = &xml->element[args->index];
    VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
    mmgr_line_ch(&msg, '`');
    mmgr_line_put_n(&msg, s_phase, phase.at);
    mmgr_line_put(&msg, "` is no phase of Maven's lifecycles");
    const unsigned int finding =
        EMBED_CALL(check_finding_append, MavenCheckCtx, .check = args->check, .msg = &msg, .from = element->inner_from,
                   .to = element->inner_to, .severity = CLI_BUILD_WARNING, .code = "pom-phase");
    CliBuildAdjacent adjacent;
    EMBED_CALL(cli_builds_size.build_adjacent_begin, CliBuildsSizeCfg, .adjacent = &adjacent, .name = s_phase,
               .size = phase.at);
    for (unsigned int at = 0u; at < 29u; at += 1u)
    {
        EMBED_CALL(cli_builds_size_adjacent.build_adjacent_offer, CliBuildsSizeAdjacentCfg, .adjacent = &adjacent,
                   .one = g_cli_maven_phases[at],
                   .size = cellul.len(&(CatenaFinitaCfg){.src = g_cli_maven_phases[at], .cap = CLI_POOL_BYTES}));
    }
    for (unsigned int at = 0u; at < adjacent.count; at += 1u)
    {
        VerbaLinea title = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = args->check->found);
        mmgr_line_put(&title, "Change to ");
        mmgr_line_put_n(&title, adjacent.adjacent[at], adjacent.adjacent_size[at]);
        EMBED_CALL(cli_builds_text.build_edit_fix, CliBuildsTextCfg, .found = args->check->found, .finding = finding,
                   .title = &title, .lines = args->check->lines, .from = element->inner_from, .to = element->inner_to,
                   .text = adjacent.adjacent[at], .size = adjacent.adjacent_size[at]);
    }
}

void cli_maven_check(const CliMavenTextCtx *args)
{
    const char *root = args->root;
    const CliBuildLines *lines = args->lines;
    CliBuildFindings *found = args->found;
    static CliXml s_xml;
    static CliMavenModel s_model;
    static CliMavenCheck s_check;
    EMBED_CALL(cli_maven_xml_text_internal.xml_parse, CliMavenXmlTextCtx, .text = args->text, .size = args->size,
               .xml_out = &s_xml);
    s_check.xml = &s_xml;
    s_check.model = &s_model;
    s_check.lines = lines;
    s_check.found = found;
    for (unsigned int at = 0u; at < s_xml.errors; at += 1u)
    {
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = found);
        EMBED_CALL(cli_maven_xml_error_internal.xml_error_write, CliMavenXmlErrorCtx, .xml = &s_xml,
                   .error = &s_xml.error[at], .out = &msg);
        (void)EMBED_CALL(check_finding_append, MavenCheckCtx, .check = &s_check, .msg = &msg,
                         .from = s_xml.error[at].from, .to = s_xml.error[at].to, .severity = CLI_BUILD_ERROR,
                         .code = "pom-xml");
    }
    for (unsigned int at = 0u; at < s_xml.opens; at += 1u)
    {
        const CliXmlError left = {0ULL, 0ULL, CLI_XML_NOT_CLOSED, s_xml.open[at], 0ULL, 0ULL};
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = found);
        EMBED_CALL(cli_maven_xml_error_internal.xml_error_write, CliMavenXmlErrorCtx, .xml = &s_xml, .error = &left,
                   .out = &msg);
        const CliXmlElement *const element = &s_xml.element[s_xml.open[at]];
        (void)EMBED_CALL(check_finding_append, MavenCheckCtx, .check = &s_check, .msg = &msg, .from = element->from,
                         .to = element->to, .severity = CLI_BUILD_ERROR, .code = "pom-xml");
    }
    const unsigned int project =
        EMBED_CALL(cli_maven_xml_project_internal.xml_root, CliMavenXmlProjectCtx, .xml = &s_xml);
    if ((project == CLI_XML_NONE) || !EMBED_CALL(cli_maven_xml_name_internal.xml_by_name, CliMavenXmlNameCtx,
                                                 .xml = &s_xml, .element = project, .name = "project"))
    {
        g_cli_guard->persistent_buf_release(s_xml.element, g_cli_xml_owner);
        *(&s_xml) = (CliXml){s_xml.text, 0ULL, NULL, 0u, 0u, NULL, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u};
        return;
    }
    EMBED_CALL(cli_maven_text_size_model_internal.model_get, CliMavenTextSizeModelCtx, .path = args->path,
               .text = args->text, .size = args->size, .model = &s_model);
    const unsigned int version = EMBED_CALL(cli_maven_xml_out_internal.xml_child, CliMavenXmlOutCtx, .xml = &s_xml,
                                            .of = project, .name = "modelVersion");
    static char s_version[1u << 12u];
    VerbaLinea model_version = {s_version, sizeof(s_version), 0ULL};
    if ((version != CLI_XML_NONE) &&
        EMBED_CALL(cli_maven_xml_out_internal.xml_text, CliMavenXmlOutCtx, .xml = &s_xml, .of = version,
                   .out = &model_version) &&
        !((model_version.at == 5u) && (memor.cmp(&(MemoriaCfg){.src = s_version, .other = "4.0.0", .bytes = 5u}) == 0)))
    {
        VerbaLinea msg = EMBED_CALL(cli_builds_lines.build_msg_line, CliBuildsLinesCfg, .found = found);
        mmgr_line_put(&msg, "A POM's `<modelVersion>` is `4.0.0`");
        (void)EMBED_CALL(check_finding_append, MavenCheckCtx, .check = &s_check, .msg = &msg,
                         .from = s_xml.element[version].inner_from, .to = s_xml.element[version].inner_to,
                         .severity = CLI_BUILD_ERROR, .code = "pom-model");
    }
    EMBED_CALL(check_elements, MavenCheckCtx, .check = &s_check);
    if (s_model.whole)
    {
        EMBED_CALL(check_properties, MavenCheckCtx, .check = &s_check);
    }
    char folder[CLI_PATH_MAX_LEN];
    if (!EMBED_CALL(cli_path_out_kind_internal.path_parent, CliPathOutKindCtx, .path = args->path, .out = folder))
    {
        VerbaLinea copy = {folder, sizeof(folder), 0ULL};
        mmgr_line_put(&copy, root);
        (void)mmgr_line_finish(&copy);
    }
    EMBED_CALL(cli_maven_out_internal.program, CliMavenOutCtx, .root = root, .folder = folder,
               .out_capacity = s_check.program);
    EMBED_CALL(cli_builds_root.build_inside, CliBuildsRootCfg, .root = root, .folder = folder,
               .out_capacity = s_check.displayed);
    for (unsigned int index = 0u; index < s_xml.elements; index += 1u)
    {
        const CliXmlElement *const element = &s_xml.element[index];
        if (EMBED_CALL(cli_maven_xml_name_internal.xml_by_name, CliMavenXmlNameCtx, .xml = &s_xml, .element = index,
                       .name = "dependencies") &&
            !EMBED_CALL(cli_maven_xml_at_internal.xml_inside_free, CliMavenXmlAtCtx, .xml = &s_xml, .at = index))
        {
            EMBED_CALL(cli_maven_check_internal.dependencies, CliMavenCheckCtx, .check = &s_check, .list = index);
        }
        else if (EMBED_CALL(cli_maven_xml_name_internal.xml_by_name, CliMavenXmlNameCtx, .xml = &s_xml,
                            .element = index, .name = "parent") &&
                 (element->parent == project))
        {
            EMBED_CALL(check_parent, MavenCheckCtx, .check = &s_check, .index = index);
        }
        else if (EMBED_CALL(cli_maven_xml_name_internal.xml_by_name, CliMavenXmlNameCtx, .xml = &s_xml,
                            .element = index, .name = "phase") &&
                 EMBED_CALL(cli_maven_xml_name_internal.xml_by_name, CliMavenXmlNameCtx, .xml = &s_xml,
                            .element = (element->parent != CLI_XML_NONE) ? element->parent : index,
                            .name = "execution"))
        {
            EMBED_CALL(check_phase, MavenCheckCtx, .check = &s_check, .index = index);
        }
    }
    g_cli_guard->persistent_buf_release(s_xml.element, g_cli_xml_owner);
    *(&s_xml) = (CliXml){s_xml.text, 0ULL, NULL, 0u, 0u, NULL, NULL, 0u, 0u, NULL, 0u, 0u, NULL, 0u};
    g_cli_guard->persistent_buf_release(s_model.name_at, g_cli_maven_model_owner);
    *(&s_model) = (CliMavenModel){NULL, NULL, NULL, NULL, 0u, 0u, NULL, NULL, 0u, 0u, 0, 0, NULL, 0u, 0u};
}
