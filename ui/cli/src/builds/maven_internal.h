// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_internal.h: Maven's POM as its reader reads it: an XML file's elements, each its name, its tags and what it
// contains, its comments, and what prevented it from reading; what a POM and its parents give its children, their
// properties and the dependencies their management gives a version; the local repository; and the program that runs a
// build (maven_xml.c, maven_schema.c, maven_model.c, maven_check.c, maven_dependencies.c, maven_complete.c,
// maven_settings.c, maven_set.c)
#ifndef MAVEN_INTERNAL_H
#define MAVEN_INTERNAL_H
#include "../text/text.h"
#include "builds_internal.h"
#include "embed_dispatch_layout.h"

typedef struct CliMavenTextCtx CliMavenTextCtx;
typedef struct CliMavenTextSizeCtx CliMavenTextSizeCtx;
typedef struct CliMavenTextSizeModelCtx CliMavenTextSizeModelCtx;
typedef struct CliMavenSizeCtx CliMavenSizeCtx;
typedef struct CliMavenSizeOutCtx CliMavenSizeOutCtx;
typedef struct CliMavenSizeCheckCtx CliMavenSizeCheckCtx;
typedef struct CliMavenXmlCtx CliMavenXmlCtx;
typedef struct CliMavenXmlOutCtx CliMavenXmlOutCtx;
typedef struct CliMavenSizeModelCtx CliMavenSizeModelCtx;
typedef struct CliMavenOutCtx CliMavenOutCtx;
typedef struct CliMavenSizeNameCtx CliMavenSizeNameCtx;
typedef struct CliMavenXmlErrorCtx CliMavenXmlErrorCtx;
typedef struct CliMavenXmlNameCtx CliMavenXmlNameCtx;
typedef struct CliMavenXmlTextCtx CliMavenXmlTextCtx;
typedef struct CliMavenXmlProjectCtx CliMavenXmlProjectCtx;
typedef struct CliMavenCheckCtx CliMavenCheckCtx;
typedef struct CliMavenXmlAtCtx CliMavenXmlAtCtx;
typedef struct CliMavenCtx CliMavenCtx;

// the max elements of an XML file, comments, and things that prevented it from reading, and no element
#define CLI_XML_ELEMENTS 16384u
#define CLI_XML_COMMENTS 4096u
#define CLI_XML_ERRORS 1024u
#define CLI_XML_NONE 0xffffffffu

// An element: its name's span in its opening tag, what it contains, the span of the name in its closing tag where it
// has one, CLI_XML_NONE where it is not closed, and its parent and children, the first, the last and each one's next
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

// what prevented an XML file from reading
#define CLI_XML_SKIP_COMMENT 0u
#define CLI_XML_SKIP_CDATA 1u
#define CLI_XML_SKIP_QUESTION 2u
#define CLI_XML_SKIP_BANG 3u
#define CLI_XML_TAG_OPEN 4u
#define CLI_XML_NO_ELEMENT 5u
#define CLI_XML_NOT_CLOSED 6u
#define CLI_XML_CLOSES_NONE 7u

// What prevented an XML file from reading: its span, what it was, and the element or the name it was of
typedef struct
{
    unsigned long long from;
    unsigned long long to;
    unsigned int type;
    unsigned int element;
    unsigned long long name_from;
    unsigned long long name_to;
} CliXmlError;

// An XML file as read: its text, its elements, its comments' spans, what prevented it from reading, and the elements
// its text leaves open, the outermost first; each list's capacity in one cell sized to the tags the text can contain,
// no more than its max, released through persistent_buf_release
typedef struct
{
    const char *text;
    unsigned long long size;
    CliXmlElement *element;
    unsigned int elements;
    unsigned int element_capacity;
    unsigned long long *comment_from;
    unsigned long long *comment_to;
    unsigned int comments;
    unsigned int comment_capacity;
    CliXmlError *error;
    unsigned int errors;
    unsigned int error_capacity;
    unsigned int *open;
    unsigned int opens;
} CliXml;

// the cell of an XML file read given back, the file left read as empty; each read is given back before `xml` is read
// again or let go
// `size` bytes of an XML file's text read into `xml`, as far as it goes, in a cell of its own, whatever `xml` contained
// before not looked at; the message of what prevented it from reading
// written; the root element, CLI_XML_NONE where there is none; the first child of `of` named `name`, CLI_XML_NONE
// where it has none; whether an element is named `name`; the text an element contains where it contains no elements,
// its entities read, into `out`: 1, or 0 where it contains elements or is not closed; the text of the child `name` of
// `of`; the element at the end of `path`, `count` names, under `of`, each step the first child of its name; whether an
// element is inside a `configuration` or `properties`, whose children its plugin or its reader makes of it
// (maven_xml.c); and the children the schema lets an element of `size` bytes of `name` contain, into `count`: NULL
// where it gives none (maven_schema.c)
void cli_xml_parse(const CliMavenXmlTextCtx *args);
void cli_xml_error_write(const CliMavenXmlErrorCtx *args);
unsigned int cli_xml_root(const CliMavenXmlProjectCtx *args);
unsigned int cli_xml_child(const CliMavenXmlOutCtx *args);
int cli_xml_by_name(const CliMavenXmlNameCtx *args);
int cli_xml_text(const CliMavenXmlOutCtx *args);
int cli_xml_text_get(const CliMavenXmlOutCtx *args);
unsigned int cli_xml_at_path(const CliMavenXmlCtx *args);
int cli_xml_inside_free(const CliMavenXmlAtCtx *args);
const char *const *cli_maven_schema(const CliMavenSizeNameCtx *args);

// whether a character goes in an element's name
int cli_xml_name_character(const CliMavenCtx *args);

// the scopes a dependency takes, the phases of Maven's lifecycles, the packagings and types, the booleans, the
// properties a POM's `<properties>` usually sets, the first words of the properties Maven, Java and the system give
// a POM, and those it can name of its project (maven_schema.c)
extern const char *const g_cli_maven_scopes[6];
extern const char *const g_cli_maven_phases[29];
extern const char *const g_cli_maven_packagings[8];
extern const char *const g_cli_maven_types[7];
extern const char *const g_cli_maven_booleans[2];
extern const char *const g_cli_maven_common_properties[6];
extern const char *const g_cli_maven_input_heads[16];
extern const char *const g_cli_maven_project_properties[11];

// the max properties of a POM and its parents, the max dependencies their management gives a version, and the bytes
// they take
#define CLI_MAVEN_PROPERTIES 4096u
#define CLI_MAVEN_MANAGED 4096u
#define CLI_MAVEN_MODEL_TEXT (1u << 20u)

// What a POM gives its children: its properties, each a name and a value, the dependencies its management gives a
// version, by `group:artifact`, and whether it and each parent above it were read and one of its managed dependencies
// imports a POM, each a text of the model's. Its lists and its texts are in one cell taken at their max as it is
// read, released through persistent_buf_release
typedef struct
{
    unsigned int *name_at;
    unsigned int *name_size;
    unsigned int *value_at;
    unsigned int *value_size;
    unsigned int properties;
    unsigned int property_capacity;
    unsigned int *managed_at;
    unsigned int *managed_size;
    unsigned int managed;
    unsigned int managed_capacity;
    int imports;
    int whole;
    char *text;
    unsigned int used;
    unsigned int text_capacity;
} CliMavenModel;

// the cells the models read are allocated in, counted; a model's cell given back, the model left empty, a model given
// back before it is read again or let go; a model's cell taken, its capacities none where it is refused; and `size`
// bytes of `bytes` stored in the model's text: where they start, CLI_MAVEN_MODEL_TEXT where they do not fit
void cli_maven_model_alloc(const CliMavenTextSizeModelCtx *args);
unsigned int cli_maven_model_text_store(const CliMavenSizeModelCtx *args);

// what the POM at `path`, `size` bytes of its text, gives its children, its parents read up to eight above it, into
// `model`; the value of the property `name` the model sets, NULL where it sets none; `value` with each property it
// names given, as far as the model gives them, eight at max, into `out`; whether Maven, Java or the system give a
// property of `size` bytes of `name`; the local repository, the one the user's settings name or `~/.m2/repository`,
// into `out`, sized for CLI_PATH_MAX_LEN: 1, or 0 where no home folder is known; the user's `settings.xml`, the same;
// and the program that runs the Maven build at `folder` of the tree at `root`, its wrapper, the nearest above it in the
// tree, where one is, into `out`, sized for CLI_PATH_MAX_LEN (maven_model.c)
void cli_maven_model_get(const CliMavenTextSizeModelCtx *args);
const char *cli_maven_property(const CliMavenSizeCtx *args);
void cli_maven_resolve(const CliMavenSizeOutCtx *args);
int cli_maven_free(const CliMavenSizeNameCtx *args);
int cli_maven_local_repository(const CliMavenOutCtx *args);
int cli_maven_user_settings(const CliMavenOutCtx *args);
void cli_maven_program(const CliMavenOutCtx *args);

// What a POM's check works with: its XML, its model, the lines, the findings, its folder in the tree and the program
// that runs its build
typedef struct
{
    const CliXml *xml;
    const CliMavenModel *model;
    const CliBuildLines *lines;
    CliBuildFindings *found;
    char displayed[CLI_PATH_MAX_LEN];
    char program[CLI_PATH_MAX_LEN];
} CliMavenCheck;

// the findings of the POM at `path` of the tree at `root`, `size` bytes of its text; the fix that fetches the newest
// build of the snapshot `coordinates` (maven_check.c); and the findings of the dependencies of the `<dependencies>` at
// `list` (maven_dependencies.c)
void cli_maven_check(const CliMavenTextCtx *args);
void cli_maven_update(const CliMavenSizeCheckCtx *args);
void cli_maven_dependencies(const CliMavenCheckCtx *args);

// the properties that set each of the compiler's settings, each its key, its label, its property and the element of the
// compiler plugin's configuration that sets it there; the `maven-compiler-plugin` of a POM's build or its plugin
// management, CLI_XML_NONE where it has none; and Maven's home folder, the one `MAVEN_HOME` or `M2_HOME` names or the
// one above the `bin` that contains `mvn`, into `out`, sized for CLI_PATH_MAX_LEN: 1, or 0 where none is known
// (maven_settings.c)
extern const char *const g_cli_maven_compiler[4][4];
unsigned int cli_maven_compiler_plugin(const CliMavenXmlProjectCtx *args);
int cli_maven_home(const CliMavenOutCtx *args);

// what completes the word before byte `at` of the POM at `path`: an element's name after `<`, the name of the element
// left open after `</`, a property's after `${`, and in an element's text the values it takes, the local repository's
// groups, artifacts and versions among them (maven_complete.c)
void cli_maven_complete(const CliMavenTextSizeCtx *args);

struct CliMavenTextCtx
{
    const char *text;
    unsigned long long size;
    const char *path;
    const char *root;
    const CliBuildLines *lines;
    CliBuildFindings *found;
};

struct CliMavenTextSizeCtx
{
    const char *text;
    unsigned long long size;
    const char *path;
    unsigned long long at_u64;
    CliCompletions *items;
};

struct CliMavenTextSizeModelCtx
{
    const char *text;
    unsigned long long size;
    CliMavenModel *model;
    const char *path;
};

struct CliMavenSizeCtx
{
    unsigned long long size;
    const char *name;
    const CliMavenModel *model_read;
    unsigned long long *value_size;
};

struct CliMavenSizeOutCtx
{
    unsigned long long size;
    VerbaLinea *out;
    const CliMavenModel *model_read;
    const char *value;
};

struct CliMavenSizeCheckCtx
{
    unsigned long long size;
    const CliMavenCheck *check;
    unsigned int finding;
    const char *coordinates;
};

struct CliMavenXmlCtx
{
    const CliXml *xml;
    unsigned int of;
    const char *const *path_pointer;
    unsigned int count;
};

struct CliMavenXmlOutCtx
{
    const CliXml *xml;
    VerbaLinea *out;
    unsigned int of;
    const char *name;
};

struct CliMavenSizeModelCtx
{
    unsigned long long size;
    CliMavenModel *model;
    const char *bytes;
};

struct CliMavenOutCtx
{
    char *out_capacity;
    const char *root;
    const char *folder;
};

struct CliMavenSizeNameCtx
{
    unsigned long long size;
    const char *name;
    unsigned int *count_pointer;
};

struct CliMavenXmlErrorCtx
{
    const CliXml *xml;
    const CliXmlError *error;
    VerbaLinea *out;
};

struct CliMavenXmlNameCtx
{
    const CliXml *xml;
    const char *name;
    unsigned int element;
};

struct CliMavenXmlTextCtx
{
    CliXml *xml_out;
    const char *text;
    unsigned long long size;
};

struct CliMavenXmlProjectCtx
{
    const CliXml *xml;
    unsigned int project;
};

struct CliMavenCheckCtx
{
    const CliMavenCheck *check;
    unsigned int list;
};

struct CliMavenXmlAtCtx
{
    const CliXml *xml;
    unsigned int at;
};

struct CliMavenCtx
{
    unsigned int character;
};

typedef struct
{
    void (*xml_parse)(const CliMavenXmlTextCtx *args);
} CliMavenXmlTextNs;
EMBED_TABLE_LAYOUT(CliMavenXmlTextNs, xml_parse);
EMBED_TABLE_STORAGE CliMavenXmlTextNs cli_maven_xml_text_internal EMBED_UNUSED = {
    .xml_parse = cli_xml_parse,
};

typedef struct
{
    void (*xml_error_write)(const CliMavenXmlErrorCtx *args);
} CliMavenXmlErrorNs;
EMBED_TABLE_LAYOUT(CliMavenXmlErrorNs, xml_error_write);
EMBED_TABLE_STORAGE CliMavenXmlErrorNs cli_maven_xml_error_internal EMBED_UNUSED = {
    .xml_error_write = cli_xml_error_write,
};

typedef struct
{
    unsigned int (*xml_root)(const CliMavenXmlProjectCtx *args);
    unsigned int (*compiler_plugin)(const CliMavenXmlProjectCtx *args);
} CliMavenXmlProjectNs;
EMBED_TABLE_LAYOUT(CliMavenXmlProjectNs, xml_root, compiler_plugin);
EMBED_TABLE_STORAGE CliMavenXmlProjectNs cli_maven_xml_project_internal EMBED_UNUSED = {
    .xml_root = cli_xml_root,
    .compiler_plugin = cli_maven_compiler_plugin,
};

typedef struct
{
    unsigned int (*xml_child)(const CliMavenXmlOutCtx *args);
    int (*xml_text)(const CliMavenXmlOutCtx *args);
    int (*xml_text_get)(const CliMavenXmlOutCtx *args);
} CliMavenXmlOutNs;
EMBED_TABLE_LAYOUT(CliMavenXmlOutNs, xml_child, xml_text, xml_text_get);
EMBED_TABLE_STORAGE CliMavenXmlOutNs cli_maven_xml_out_internal EMBED_UNUSED = {
    .xml_child = cli_xml_child,
    .xml_text = cli_xml_text,
    .xml_text_get = cli_xml_text_get,
};

typedef struct
{
    int (*xml_by_name)(const CliMavenXmlNameCtx *args);
} CliMavenXmlNameNs;
EMBED_TABLE_LAYOUT(CliMavenXmlNameNs, xml_by_name);
EMBED_TABLE_STORAGE CliMavenXmlNameNs cli_maven_xml_name_internal EMBED_UNUSED = {
    .xml_by_name = cli_xml_by_name,
};

typedef struct
{
    unsigned int (*xml_at_path)(const CliMavenXmlCtx *args);
} CliMavenXmlNs;
EMBED_TABLE_LAYOUT(CliMavenXmlNs, xml_at_path);
EMBED_TABLE_STORAGE CliMavenXmlNs cli_maven_xml_internal EMBED_UNUSED = {
    .xml_at_path = cli_xml_at_path,
};

typedef struct
{
    int (*xml_inside_free)(const CliMavenXmlAtCtx *args);
} CliMavenXmlAtNs;
EMBED_TABLE_LAYOUT(CliMavenXmlAtNs, xml_inside_free);
EMBED_TABLE_STORAGE CliMavenXmlAtNs cli_maven_xml_at_internal EMBED_UNUSED = {
    .xml_inside_free = cli_xml_inside_free,
};

typedef struct
{
    const char *const *(*schema)(const CliMavenSizeNameCtx *args);
    int (*free)(const CliMavenSizeNameCtx *args);
} CliMavenSizeNameNs;
EMBED_TABLE_LAYOUT(CliMavenSizeNameNs, schema, free);
EMBED_TABLE_STORAGE CliMavenSizeNameNs cli_maven_size_name_internal EMBED_UNUSED = {
    .schema = cli_maven_schema,
    .free = cli_maven_free,
};

typedef struct
{
    int (*xml_name_character)(const CliMavenCtx *args);
} CliMavenNsTable;
EMBED_TABLE_LAYOUT(CliMavenNsTable, xml_name_character);
EMBED_TABLE_STORAGE CliMavenNsTable cli_maven_internal EMBED_UNUSED = {
    .xml_name_character = cli_xml_name_character,
};

typedef struct
{
    void (*model_alloc)(const CliMavenTextSizeModelCtx *args);
    void (*model_get)(const CliMavenTextSizeModelCtx *args);
} CliMavenTextSizeModelNs;
EMBED_TABLE_LAYOUT(CliMavenTextSizeModelNs, model_alloc, model_get);
EMBED_TABLE_STORAGE CliMavenTextSizeModelNs cli_maven_text_size_model_internal EMBED_UNUSED = {
    .model_alloc = cli_maven_model_alloc,
    .model_get = cli_maven_model_get,
};

typedef struct
{
    unsigned int (*model_text_store)(const CliMavenSizeModelCtx *args);
} CliMavenSizeModelNs;
EMBED_TABLE_LAYOUT(CliMavenSizeModelNs, model_text_store);
EMBED_TABLE_STORAGE CliMavenSizeModelNs cli_maven_size_model_internal EMBED_UNUSED = {
    .model_text_store = cli_maven_model_text_store,
};

typedef struct
{
    const char *(*property)(const CliMavenSizeCtx *args);
} CliMavenSizeNs;
EMBED_TABLE_LAYOUT(CliMavenSizeNs, property);
EMBED_TABLE_STORAGE CliMavenSizeNs cli_maven_size_internal EMBED_UNUSED = {
    .property = cli_maven_property,
};

typedef struct
{
    void (*resolve)(const CliMavenSizeOutCtx *args);
} CliMavenSizeOutNs;
EMBED_TABLE_LAYOUT(CliMavenSizeOutNs, resolve);
EMBED_TABLE_STORAGE CliMavenSizeOutNs cli_maven_size_out_internal EMBED_UNUSED = {
    .resolve = cli_maven_resolve,
};

typedef struct
{
    int (*local_repository)(const CliMavenOutCtx *args);
    int (*user_settings)(const CliMavenOutCtx *args);
    void (*program)(const CliMavenOutCtx *args);
    int (*home)(const CliMavenOutCtx *args);
} CliMavenOutNs;
EMBED_TABLE_LAYOUT(CliMavenOutNs, local_repository, user_settings, program, home);
EMBED_TABLE_STORAGE CliMavenOutNs cli_maven_out_internal EMBED_UNUSED = {
    .local_repository = cli_maven_local_repository,
    .user_settings = cli_maven_user_settings,
    .program = cli_maven_program,
    .home = cli_maven_home,
};

typedef struct
{
    void (*check)(const CliMavenTextCtx *args);
} CliMavenTextNs;
EMBED_TABLE_LAYOUT(CliMavenTextNs, check);
EMBED_TABLE_STORAGE CliMavenTextNs cli_maven_text_internal EMBED_UNUSED = {
    .check = cli_maven_check,
};

typedef struct
{
    void (*update)(const CliMavenSizeCheckCtx *args);
} CliMavenSizeCheckNs;
EMBED_TABLE_LAYOUT(CliMavenSizeCheckNs, update);
EMBED_TABLE_STORAGE CliMavenSizeCheckNs cli_maven_size_check_internal EMBED_UNUSED = {
    .update = cli_maven_update,
};

typedef struct
{
    void (*dependencies)(const CliMavenCheckCtx *args);
} CliMavenCheckNs;
EMBED_TABLE_LAYOUT(CliMavenCheckNs, dependencies);
EMBED_TABLE_STORAGE CliMavenCheckNs cli_maven_check_internal EMBED_UNUSED = {
    .dependencies = cli_maven_dependencies,
};

typedef struct
{
    void (*complete)(const CliMavenTextSizeCtx *args);
} CliMavenTextSizeNs;
EMBED_TABLE_LAYOUT(CliMavenTextSizeNs, complete);
EMBED_TABLE_STORAGE CliMavenTextSizeNs cli_maven_text_size_internal EMBED_UNUSED = {
    .complete = cli_maven_complete,
};

#endif
