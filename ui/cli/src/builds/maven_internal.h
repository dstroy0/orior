// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_internal.h: Maven's POM as its reader reads it: an XML file's elements, each its name, its tags and what it
// holds, its comments, and what kept it from reading; what a POM and its parents give its children, their properties
// and the dependencies their management gives a version; the local repository; and the program that runs a build
// (maven_xml.c, maven_schema.c, maven_model.c, maven_check.c, maven_dependencies.c, maven_complete.c,
// maven_settings.c, maven_set.c)
#ifndef MAVEN_INTERNAL_H
#define MAVEN_INTERNAL_H

#include "../text/text.h"
#include "builds_internal.h"

// the most elements of an XML file, comments, and things that kept it from reading, and no element
#define CLI_XML_ELEMENTS 16384u
#define CLI_XML_COMMENTS 4096u
#define CLI_XML_ERRORS 1024u
#define CLI_XML_NONE 0xffffffffu

// An element: its name's span in its opening tag, what it holds, the span of the name in its closing tag where it has
// one, CLI_XML_NONE where it is not closed, and its parent and children, the first, the last and each one's next
typedef struct
{
    unsigned long long from;
    unsigned long long to;
    unsigned long long inner_from;
    unsigned long long inner_to;
    unsigned long long close_from;
    unsigned long long close_to;
    int closed;
    unsigned int parent;
    unsigned int first;
    unsigned int last;
    unsigned int next;
    unsigned int children;
} CliXmlElement;

// what kept an XML file from reading
#define CLI_XML_SKIP_COMMENT 0u
#define CLI_XML_SKIP_CDATA 1u
#define CLI_XML_SKIP_QUESTION 2u
#define CLI_XML_SKIP_BANG 3u
#define CLI_XML_TAG_OPEN 4u
#define CLI_XML_NO_ELEMENT 5u
#define CLI_XML_NOT_CLOSED 6u
#define CLI_XML_CLOSES_NONE 7u

// What kept an XML file from reading: its span, what it was, and the element or the name it was of
typedef struct
{
    unsigned long long from;
    unsigned long long to;
    unsigned int kind;
    unsigned int element;
    unsigned long long name_from;
    unsigned long long name_to;
} CliXmlError;

// An XML file as read: its text, its elements, its comments' spans, what kept it from reading, and the elements its
// text leaves open, the outermost first
typedef struct
{
    const char *text;
    unsigned long long size;
    CliXmlElement element[CLI_XML_ELEMENTS];
    unsigned int elements;
    unsigned long long comment_from[CLI_XML_COMMENTS];
    unsigned long long comment_to[CLI_XML_COMMENTS];
    unsigned int comments;
    CliXmlError error[CLI_XML_ERRORS];
    unsigned int errors;
    unsigned int open[CLI_XML_ELEMENTS];
    unsigned int opens;
} CliXml;

// `size` bytes of an XML file's text read into `xml`, as far as it goes; the message of what kept it from reading
// written; the root element, CLI_XML_NONE where there is none; the first child of `of` named `name`, CLI_XML_NONE
// where it has none; whether an element is named `name`; the text an element holds where it holds no elements, its
// entities read, into `out`: 1, or 0 where it holds elements or is not closed; the text of the child `name` of `of`;
// the element at the end of `path`, `count` names, under `of`, each step the first child of its name; whether an
// element stands inside a `configuration` or `properties`, whose children its plugin or its reader makes of it
// (maven_xml.c); and the children the schema lets an element of `size` bytes of `name` hold, into `count`: NULL where
// it gives none (maven_schema.c)
void cli_xml_parse(const char *text, unsigned long long size, CliXml *xml);
void cli_xml_error_said(const CliXml *xml, const CliXmlError *error, CliLine *out);
unsigned int cli_xml_root(const CliXml *xml);
unsigned int cli_xml_child(const CliXml *xml, unsigned int of, const char *name);
int cli_xml_named(const CliXml *xml, unsigned int element, const char *name);
int cli_xml_text(const CliXml *xml, unsigned int of, CliLine *out);
int cli_xml_text_of(const CliXml *xml, unsigned int of, const char *name, CliLine *out);
unsigned int cli_xml_at_path(const CliXml *xml, unsigned int of, const char *const *path, unsigned int count);
int cli_xml_inside_free(const CliXml *xml, unsigned int at);
const char *const *cli_maven_schema(const char *name, unsigned long long size, unsigned int *count);

// whether a letter goes in an element's name
int cli_xml_name_letter(unsigned int letter);

// the scopes a dependency takes, the phases of Maven's lifecycles, the packagings and types, the booleans, the
// properties a POM's `<properties>` most often sets, the first words of the properties Maven, Java and the system give
// a POM, and those it can name of its project (maven_schema.c)
extern const char *const g_cli_maven_scopes[6];
extern const char *const g_cli_maven_phases[29];
extern const char *const g_cli_maven_packagings[8];
extern const char *const g_cli_maven_types[7];
extern const char *const g_cli_maven_booleans[2];
extern const char *const g_cli_maven_common_properties[6];
extern const char *const g_cli_maven_given_heads[16];
extern const char *const g_cli_maven_project_properties[11];

// the most properties of a POM and its parents, the most dependencies their management gives a version, and the bytes
// they take
#define CLI_MAVEN_PROPERTIES 4096u
#define CLI_MAVEN_MANAGED 4096u
#define CLI_MAVEN_MODEL_TEXT (1u << 20u)

// What a POM gives its children: its properties, each a name and a value, the dependencies its management gives a
// version, by `group:artifact`, and whether it and each parent above it were read and one of its managed dependencies
// imports a POM, each a text of the model's
typedef struct
{
    unsigned int name_at[CLI_MAVEN_PROPERTIES];
    unsigned int name_size[CLI_MAVEN_PROPERTIES];
    unsigned int value_at[CLI_MAVEN_PROPERTIES];
    unsigned int value_size[CLI_MAVEN_PROPERTIES];
    unsigned int properties;
    unsigned int managed_at[CLI_MAVEN_MANAGED];
    unsigned int managed_size[CLI_MAVEN_MANAGED];
    unsigned int managed;
    int imports;
    int whole;
    char text[CLI_MAVEN_MODEL_TEXT];
    unsigned int used;
} CliMavenModel;

// what the POM at `path`, `size` bytes of its text, gives its children, its parents read up to eight above it, into
// `model`; the value of the property `name` the model sets, NULL where it sets none; `value` with each property it
// names given, as far as the model gives them, eight at most, into `out`; whether Maven, Java or the system give a
// property of `size` bytes of `name`; the local repository, the one the user's settings name or `~/.m2/repository`,
// into `out`, which holds CLI_PATH_LONGEST: 1, or 0 where no home folder is known; the user's `settings.xml`, the same;
// and the program that runs the Maven build at `folder` of the tree at `root`, its wrapper, the nearest above it in the
// tree, where one is, into `out`, which holds CLI_PATH_LONGEST (maven_model.c)
void cli_maven_model_of(const char *path, const char *text, unsigned long long size, CliMavenModel *model);
const char *cli_maven_property(const CliMavenModel *model, const char *name, unsigned long long size,
                               unsigned long long *value_size);
void cli_maven_resolve(const char *value, unsigned long long size, const CliMavenModel *model, CliLine *out);
int cli_maven_given(const char *name, unsigned long long size);
int cli_maven_local_repository(char *out);
int cli_maven_user_settings(char *out);
void cli_maven_program(const char *root, const char *folder, char *out);

// What a POM's check works with: its XML, its model, the lines, the findings, its folder in the tree and the program
// that runs its build
typedef struct
{
    const CliXml *xml;
    const CliMavenModel *model;
    const CliBuildLines *lines;
    CliBuildFindings *found;
    char shown[CLI_PATH_LONGEST];
    char program[CLI_PATH_LONGEST];
} CliMavenCheck;

// the findings of the POM at `path` of the tree at `root`, `size` bytes of its text; the fix that fetches the newest
// build of the snapshot `coordinates` (maven_check.c); and the findings of the dependencies of the `<dependencies>` at
// `list` (maven_dependencies.c)
void cli_maven_check(const char *root, const char *path, const char *text, unsigned long long size,
                     const CliBuildLines *lines, CliBuildFindings *found);
void cli_maven_update(const CliMavenCheck *check, unsigned int finding, const char *coordinates,
                      unsigned long long size);
void cli_maven_dependencies(const CliMavenCheck *check, unsigned int list);

// the properties that set each of the compiler's settings, each its key, its label, its property and the element of the
// compiler plugin's configuration that sets it there; the `maven-compiler-plugin` of a POM's build or its plugin
// management, CLI_XML_NONE where it has none; and Maven's home folder, the one `MAVEN_HOME` or `M2_HOME` names or the
// one above the `bin` that holds `mvn`, into `out`, which holds CLI_PATH_LONGEST: 1, or 0 where none is known
// (maven_settings.c)
extern const char *const g_cli_maven_compiler[4][4];
unsigned int cli_maven_compiler_plugin(const CliXml *xml, unsigned int project);
int cli_maven_home(char *out);

// what completes the word before byte `at` of the POM at `path`: an element's name after `<`, the name of the element
// left open after `</`, a property's after `${`, and in an element's text the values it takes, the local repository's
// groups, artifacts and versions among them (maven_complete.c)
void cli_maven_complete(const char *path, const char *text, unsigned long long size, unsigned long long at,
                        CliCompletions *items);

#endif
