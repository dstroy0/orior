// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_check.c: a POM checked: what kept its XML from reading and each element it leaves open; its model version;
// each element where its parent may not hold it, with the elements near its name, outside a `configuration` and a
// `properties`; each property it names outside its comments that neither it, its parents nor Maven set, where every
// parent was read; its dependencies; a parent that is a snapshot; and each execution's phase no lifecycle has
#include "maven_internal.h"

// a finding over the bytes `from` to `to` that says `said`
static unsigned int check_said(const CliMavenCheck *check, const CliLine *said, unsigned long long from,
                               unsigned long long to, unsigned int severity, const char *code)
{
    return cli_build_finding(check->found, said, check->lines, from, to, severity, code);
}

void cli_maven_update(const CliMavenCheck *check, unsigned int finding, const char *coordinates,
                      unsigned long long size)
{
    CliLine title = cli_build_said(check->found);
    cli_line_text(&title, "Update the snapshot ");
    cli_line_counted(&title, coordinates, size);
    char line[CLI_PATH_LONGEST * 2u];
    CliLine command = {line, sizeof(line), 0ull};
    cli_line_text(&command, check->program);
    cli_line_text(&command, " -U dependency:get -Dartifact=");
    cli_line_counted(&command, coordinates, size);
    cli_build_run_fix(check->found, finding, &title, check->shown,
                      cellul.len(&(CatenaFinitaCfg){.src = check->shown, .cap = CLI_CELLS_POOL}), line, command.at);
}

// each element where its parent may not hold it, with the fixes that name it as one its parent holds
static void check_elements(const CliMavenCheck *check)
{
    const CliXml *const xml = check->xml;
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
            cli_maven_schema(xml->text + parent->from, parent->to - parent->from, &count);
        if ((allowed == NULL) || cli_xml_inside_free(xml, index))
        {
            continue;
        }
        const char *const name = xml->text + element->from;
        const unsigned long long size = element->to - element->from;
        int known = 0;
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            known |= (cellul.len(&(CatenaFinitaCfg){.src = allowed[at], .cap = CLI_CELLS_POOL}) == size) &&
                     (memor.cmp(&(MemoriaCfg){.src = allowed[at], .other = name, .bytes = (size_t)size}) == 0);
        }
        if (known)
        {
            continue;
        }
        CliLine said = cli_build_said(check->found);
        cli_line_text(&said, "`<");
        cli_line_counted(&said, name, size);
        cli_line_text(&said, ">` is no element of `<");
        cli_line_counted(&said, xml->text + parent->from, parent->to - parent->from);
        cli_line_text(&said, ">`");
        const unsigned int finding =
            check_said(check, &said, element->from, element->to, CLI_BUILD_WARNING, "pom-element");
        CliBuildNear near;
        cli_build_near_begun(&near, name, size);
        for (unsigned int at = 0u; at < count; at += 1u)
        {
            cli_build_near_offered(&near, allowed[at],
                                   cellul.len(&(CatenaFinitaCfg){.src = allowed[at], .cap = CLI_CELLS_POOL}));
        }
        for (unsigned int at = 0u; at < near.count; at += 1u)
        {
            CliLine title = cli_build_said(check->found);
            cli_line_text(&title, "Change to <");
            cli_line_counted(&title, near.near[at], near.near_size[at]);
            cli_line_character(&title, '>');
            if (element->closed && (element->close_from != element->from))
            {
                cli_build_edit_fix_twice(check->found, finding, &title, check->lines, element->from, element->to,
                                         element->close_from, element->close_to, near.near[at], near.near_size[at]);
            }
            else
            {
                cli_build_edit_fix(check->found, finding, &title, check->lines, element->from, element->to,
                                   near.near[at], near.near_size[at]);
            }
        }
    }
}

// whether the byte at `at` stands in one of the POM's comments
static int check_in_comment(const CliXml *xml, unsigned long long at)
{
    for (unsigned int one = 0u; one < xml->comments; one += 1u)
    {
        if ((xml->comment_from[one] <= at) && (at < xml->comment_to[one]))
        {
            return 1;
        }
    }
    return 0;
}

// each property the POM names outside its comments that neither it, its parents nor Maven set
static void check_properties(const CliMavenCheck *check)
{
    const CliXml *const xml = check->xml;
    const char *const text = xml->text;
    const CliMavenModel *const model = check->model;
    for (unsigned long long at = 0ull; at + 1u < xml->size;)
    {
        unsigned long long off = 0ull;
        if (!cli_text_find(text + at, xml->size - at, "${", 2u, &off))
        {
            return;
        }
        const unsigned long long start = at + off;
        at = start + 2u;
        if (check_in_comment(xml, start))
        {
            continue;
        }
        unsigned long long end = at;
        while ((end < xml->size) && (text[end] != '}') && (text[end] != '\n') && (text[end] != '<'))
        {
            end += 1u;
        }
        if ((end >= xml->size) || (text[end] != '}'))
        {
            continue;
        }
        unsigned long long value = 0ull;
        if ((end == at) || (cli_maven_property(model, text + at, end - at, &value) != NULL) ||
            cli_maven_given(text + at, end - at))
        {
            continue;
        }
        CliLine said = cli_build_said(check->found);
        cli_line_text(&said, "No property `");
        cli_line_counted(&said, text + at, end - at);
        cli_line_text(&said, "` is set, by the POM, its parents or Maven");
        const unsigned int finding = check_said(check, &said, at, end, CLI_BUILD_WARNING, "pom-property");
        CliBuildNear near;
        cli_build_near_begun(&near, text + at, end - at);
        for (unsigned int one = 0u; one < model->properties; one += 1u)
        {
            cli_build_near_offered(&near, model->text + model->name_at[one], model->name_size[one]);
        }
        for (unsigned int one = 0u; one < near.count; one += 1u)
        {
            CliLine title = cli_build_said(check->found);
            cli_line_text(&title, "Change to ${");
            cli_line_counted(&title, near.near[one], near.near_size[one]);
            cli_line_character(&title, '}');
            cli_build_edit_fix(check->found, finding, &title, check->lines, at, end, near.near[one],
                               near.near_size[one]);
        }
    }
}

// a parent that is a snapshot noted, with the fix that fetches its newest build
static void check_parent(const CliMavenCheck *check, unsigned int index)
{
    const CliXml *const xml = check->xml;
    static char s_texts[3][1u << 12u];
    static const char *const s_names[3] = {"groupId", "artifactId", "version"};
    unsigned long long sizes[3] = {0ull, 0ull, 0ull};
    for (unsigned int at = 0u; at < 3u; at += 1u)
    {
        CliLine one = {s_texts[at], sizeof(s_texts[at]), 0ull};
        if (!cli_xml_text_of(xml, index, s_names[at], &one) || !cli_line_fits(&one))
        {
            return;
        }
        sizes[at] = one.at;
    }
    if (!cli_build_snapshot(s_texts[2], sizes[2]))
    {
        return;
    }
    const unsigned int version = cli_xml_child(xml, index, "version");
    const CliXmlElement *const at = &xml->element[(version != CLI_XML_NONE) ? version : index];
    static char s_coordinates[1u << 14u];
    CliLine coordinates = {s_coordinates, sizeof(s_coordinates), 0ull};
    for (unsigned int one = 0u; one < 3u; one += 1u)
    {
        cli_line_text(&coordinates, (one > 0u) ? ":" : "");
        cli_line_counted(&coordinates, s_texts[one], sizes[one]);
    }
    CliLine said = cli_build_said(check->found);
    cli_line_text(&said, "The parent `");
    cli_line_counted(&said, s_coordinates, coordinates.at);
    cli_line_text(&said, "` is a snapshot, whose newest build Maven fetches with `-U`");
    const unsigned int finding = check_said(check, &said, at->inner_from, at->inner_to, CLI_BUILD_NOTE, "snapshot");
    cli_line_text(&coordinates, ":pom");
    cli_maven_update(check, finding, s_coordinates, coordinates.at);
}

// an execution's phase no lifecycle of Maven's has, with the phases near it
static void check_phase(const CliMavenCheck *check, unsigned int index)
{
    const CliXml *const xml = check->xml;
    static char s_phase[1u << 12u];
    CliLine phase = {s_phase, sizeof(s_phase), 0ull};
    unsigned long long found = 0ull;
    if (!cli_xml_text(xml, index, &phase) || !cli_line_fits(&phase) ||
        cli_text_find(s_phase, phase.at, "${", 2u, &found))
    {
        return;
    }
    for (unsigned int at = 0u; at < 29u; at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = g_cli_maven_phases[at], .cap = CLI_CELLS_POOL}) == phase.at) &&
            (memor.cmp(&(MemoriaCfg){.src = g_cli_maven_phases[at], .other = s_phase, .bytes = phase.at}) == 0))
        {
            return;
        }
    }
    const CliXmlElement *const element = &xml->element[index];
    CliLine said = cli_build_said(check->found);
    cli_line_character(&said, '`');
    cli_line_counted(&said, s_phase, phase.at);
    cli_line_text(&said, "` is no phase of Maven's lifecycles");
    const unsigned int finding =
        check_said(check, &said, element->inner_from, element->inner_to, CLI_BUILD_WARNING, "pom-phase");
    CliBuildNear near;
    cli_build_near_begun(&near, s_phase, phase.at);
    for (unsigned int at = 0u; at < 29u; at += 1u)
    {
        cli_build_near_offered(&near, g_cli_maven_phases[at],
                               cellul.len(&(CatenaFinitaCfg){.src = g_cli_maven_phases[at], .cap = CLI_CELLS_POOL}));
    }
    for (unsigned int at = 0u; at < near.count; at += 1u)
    {
        CliLine title = cli_build_said(check->found);
        cli_line_text(&title, "Change to ");
        cli_line_counted(&title, near.near[at], near.near_size[at]);
        cli_build_edit_fix(check->found, finding, &title, check->lines, element->inner_from, element->inner_to,
                           near.near[at], near.near_size[at]);
    }
}

void cli_maven_check(const char *root, const char *path, const char *text, unsigned long long size,
                     const CliBuildLines *lines, CliBuildFindings *found)
{
    static CliXml s_xml;
    static CliMavenModel s_model;
    static CliMavenCheck s_check;
    static CliLock s_check_lock = CLI_LOCK_INIT;
    cli_lock_take(&s_check_lock);
    cli_xml_parse(text, size, &s_xml);
    s_check.xml = &s_xml;
    s_check.model = &s_model;
    s_check.lines = lines;
    s_check.found = found;
    for (unsigned int at = 0u; at < s_xml.errors; at += 1u)
    {
        CliLine said = cli_build_said(found);
        cli_xml_error_said(&s_xml, &s_xml.error[at], &said);
        (void)check_said(&s_check, &said, s_xml.error[at].from, s_xml.error[at].to, CLI_BUILD_ERROR, "pom-xml");
    }
    for (unsigned int at = 0u; at < s_xml.opens; at += 1u)
    {
        const CliXmlError left = {0ull, 0ull, CLI_XML_NOT_CLOSED, s_xml.open[at], 0ull, 0ull};
        CliLine said = cli_build_said(found);
        cli_xml_error_said(&s_xml, &left, &said);
        const CliXmlElement *const element = &s_xml.element[s_xml.open[at]];
        (void)check_said(&s_check, &said, element->from, element->to, CLI_BUILD_ERROR, "pom-xml");
    }
    const unsigned int project = cli_xml_root(&s_xml);
    if ((project == CLI_XML_NONE) || !cli_xml_named(&s_xml, project, "project"))
    {
        cli_xml_given(&s_xml);
        cli_lock_give(&s_check_lock);
        return;
    }
    cli_maven_model_of(path, text, size, &s_model);
    const unsigned int version = cli_xml_child(&s_xml, project, "modelVersion");
    static char s_version[1u << 12u];
    CliLine model_version = {s_version, sizeof(s_version), 0ull};
    if ((version != CLI_XML_NONE) && cli_xml_text(&s_xml, version, &model_version) &&
        !((model_version.at == 5u) && (memor.cmp(&(MemoriaCfg){.src = s_version, .other = "4.0.0", .bytes = 5u}) == 0)))
    {
        CliLine said = cli_build_said(found);
        cli_line_text(&said, "A POM's `<modelVersion>` is `4.0.0`");
        (void)check_said(&s_check, &said, s_xml.element[version].inner_from, s_xml.element[version].inner_to,
                         CLI_BUILD_ERROR, "pom-model");
    }
    check_elements(&s_check);
    if (s_model.whole)
    {
        check_properties(&s_check);
    }
    char folder[CLI_PATH_LONGEST];
    if (!cli_path_parent(path, folder))
    {
        CliLine copy = {folder, sizeof(folder), 0ull};
        cli_line_text(&copy, root);
        (void)cli_line_closed(&copy);
    }
    cli_maven_program(root, folder, s_check.program);
    cli_build_inside(root, folder, s_check.shown);
    for (unsigned int index = 0u; index < s_xml.elements; index += 1u)
    {
        const CliXmlElement *const element = &s_xml.element[index];
        if (cli_xml_named(&s_xml, index, "dependencies") && !cli_xml_inside_free(&s_xml, index))
        {
            cli_maven_dependencies(&s_check, index);
        }
        else if (cli_xml_named(&s_xml, index, "parent") && (element->parent == project))
        {
            check_parent(&s_check, index);
        }
        else if (cli_xml_named(&s_xml, index, "phase") &&
                 cli_xml_named(&s_xml, (element->parent != CLI_XML_NONE) ? element->parent : index, "execution"))
        {
            check_phase(&s_check, index);
        }
    }
    cli_xml_given(&s_xml);
    cli_maven_model_given(&s_model);
    cli_lock_give(&s_check_lock);
}
