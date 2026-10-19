// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// maven_schema.c: what a POM's elements may hold, and the words its values take: the children each element may hold,
// an element inside a `configuration` or a `properties` being its plugin's or its reader's and not checked; the scopes
// a dependency takes; the phases of Maven's lifecycles; the packagings and types; the properties a POM's `<properties>`
// most often sets; the first words of the properties Maven, Java and the system give a POM; and those it can name of
// its project
#include "maven_internal.h"

const char *const g_cli_maven_scopes[6] = {"compile", "provided", "runtime", "test", "system", "import"};
const char *const g_cli_maven_phases[29] = {"pre-clean",
                                            "clean",
                                            "post-clean",
                                            "validate",
                                            "initialize",
                                            "generate-sources",
                                            "process-sources",
                                            "generate-resources",
                                            "process-resources",
                                            "compile",
                                            "process-classes",
                                            "generate-test-sources",
                                            "process-test-sources",
                                            "generate-test-resources",
                                            "process-test-resources",
                                            "test-compile",
                                            "process-test-classes",
                                            "test",
                                            "prepare-package",
                                            "package",
                                            "pre-integration-test",
                                            "integration-test",
                                            "post-integration-test",
                                            "verify",
                                            "install",
                                            "deploy",
                                            "pre-site",
                                            "site",
                                            "post-site"};
const char *const g_cli_maven_packagings[8] = {"jar", "war", "ear", "pom", "maven-plugin", "ejb", "rar", "bundle"};
const char *const g_cli_maven_types[7] = {"jar", "war", "pom", "test-jar", "ejb-client", "zip", "maven-plugin"};
const char *const g_cli_maven_booleans[2] = {"true", "false"};
const char *const g_cli_maven_common_properties[6] = {"maven.compiler.release",
                                                      "maven.compiler.source",
                                                      "maven.compiler.target",
                                                      "project.build.sourceEncoding",
                                                      "project.reporting.outputEncoding",
                                                      "maven.compiler.encoding"};
const char *const g_cli_maven_given_heads[16] = {"project", "pom",     "env",      "settings",  "java", "os",
                                                 "user",    "maven",   "file",     "line",      "path", "sun",
                                                 "session", "basedir", "revision", "changelist"};
const char *const g_cli_maven_project_properties[11] = {"project.groupId",         "project.artifactId",
                                                        "project.version",         "project.name",
                                                        "project.basedir",         "project.baseUri",
                                                        "project.build.directory", "project.build.outputDirectory",
                                                        "project.build.finalName", "project.build.sourceDirectory",
                                                        "project.parent.version"};

// the children each element may hold, its name first and a NULL after the last child
static const char *const s_schema[][31] = {
    {"project",
     "modelVersion",
     "parent",
     "groupId",
     "artifactId",
     "version",
     "packaging",
     "name",
     "description",
     "url",
     "inceptionYear",
     "organization",
     "licenses",
     "developers",
     "contributors",
     "mailingLists",
     "prerequisites",
     "modules",
     "scm",
     "issueManagement",
     "ciManagement",
     "distributionManagement",
     "properties",
     "dependencyManagement",
     "dependencies",
     "repositories",
     "pluginRepositories",
     "build",
     "reporting",
     "profiles",
     NULL},
    {"parent", "groupId", "artifactId", "version", "relativePath", NULL},
    {"organization", "name", "url", NULL},
    {"licenses", "license", NULL},
    {"license", "name", "url", "distribution", "comments", NULL},
    {"developers", "developer", NULL},
    {"contributors", "contributor", NULL},
    {"developer", "id", "name", "email", "url", "organization", "organizationUrl", "roles", "timezone", "properties",
     NULL},
    {"contributor", "name", "email", "url", "organization", "organizationUrl", "roles", "timezone", "properties", NULL},
    {"roles", "role", NULL},
    {"mailingLists", "mailingList", NULL},
    {"mailingList", "name", "subscribe", "unsubscribe", "post", "archive", "otherArchives", NULL},
    {"otherArchives", "otherArchive", NULL},
    {"prerequisites", "maven", NULL},
    {"modules", "module", NULL},
    {"scm", "connection", "developerConnection", "tag", "url", NULL},
    {"issueManagement", "system", "url", NULL},
    {"ciManagement", "system", "url", "notifiers", NULL},
    {"notifiers", "notifier", NULL},
    {"notifier", "type", "sendOnError", "sendOnFailure", "sendOnSuccess", "sendOnWarning", "address", "configuration",
     NULL},
    {"distributionManagement", "repository", "snapshotRepository", "site", "downloadUrl", "relocation", "status", NULL},
    {"repository", "uniqueVersion", "releases", "snapshots", "id", "name", "url", "layout", NULL},
    {"snapshotRepository", "uniqueVersion", "releases", "snapshots", "id", "name", "url", "layout", NULL},
    {"site", "id", "name", "url", NULL},
    {"relocation", "groupId", "artifactId", "version", "message", NULL},
    {"dependencyManagement", "dependencies", NULL},
    {"dependencies", "dependency", NULL},
    {"dependency", "groupId", "artifactId", "version", "type", "classifier", "scope", "systemPath", "exclusions",
     "optional", NULL},
    {"exclusions", "exclusion", NULL},
    {"exclusion", "groupId", "artifactId", NULL},
    {"repositories", "repository", NULL},
    {"pluginRepositories", "pluginRepository", NULL},
    {"pluginRepository", "releases", "snapshots", "id", "name", "url", "layout", NULL},
    {"releases", "enabled", "updatePolicy", "checksumPolicy", NULL},
    {"snapshots", "enabled", "updatePolicy", "checksumPolicy", NULL},
    {"build", "sourceDirectory", "scriptSourceDirectory", "testSourceDirectory", "outputDirectory",
     "testOutputDirectory", "extensions", "defaultGoal", "resources", "testResources", "directory", "finalName",
     "filters", "pluginManagement", "plugins", NULL},
    {"extensions", "extension", NULL},
    {"extension", "groupId", "artifactId", "version", NULL},
    {"resources", "resource", NULL},
    {"testResources", "testResource", NULL},
    {"resource", "targetPath", "filtering", "directory", "includes", "excludes", NULL},
    {"testResource", "targetPath", "filtering", "directory", "includes", "excludes", NULL},
    {"includes", "include", NULL},
    {"excludes", "exclude", NULL},
    {"filters", "filter", NULL},
    {"pluginManagement", "plugins", NULL},
    {"plugins", "plugin", NULL},
    {"plugin", "groupId", "artifactId", "version", "extensions", "executions", "dependencies", "inherited",
     "configuration", "reportSets", NULL},
    {"executions", "execution", NULL},
    {"execution", "id", "phase", "goals", "inherited", "configuration", NULL},
    {"goals", "goal", NULL},
    {"reporting", "excludeDefaults", "outputDirectory", "plugins", NULL},
    {"reportSets", "reportSet", NULL},
    {"reportSet", "id", "reports", "inherited", "configuration", NULL},
    {"reports", "report", NULL},
    {"profiles", "profile", NULL},
    {"profile", "id", "activation", "build", "modules", "distributionManagement", "properties", "dependencyManagement",
     "dependencies", "repositories", "pluginRepositories", "reporting", NULL},
    {"activation", "activeByDefault", "jdk", "os", "property", "file", "packaging", NULL},
    {"os", "name", "family", "arch", "version", NULL},
    {"property", "name", "value", NULL},
    {"file", "missing", "exists", NULL}};

const char *const *cli_maven_schema(const char *name, unsigned long long size, unsigned int *count)
{
    for (unsigned int at = 0u; at < sizeof(s_schema) / sizeof(s_schema[0]); at += 1u)
    {
        if ((cellul.len(&(CatenaFinitaCfg){.src = s_schema[at][0], .cap = CLI_CELLS_POOL}) == size) &&
            (memor.cmp(&(MemoriaCfg){.src = s_schema[at][0], .other = name, .bytes = (size_t)size}) == 0))
        {
            unsigned int children = 0u;
            while ((children < 30u) && (s_schema[at][children + 1u] != NULL))
            {
                children += 1u;
            }
            *count = children;
            return &s_schema[at][1];
        }
    }
    *count = 0u;
    return NULL;
}
