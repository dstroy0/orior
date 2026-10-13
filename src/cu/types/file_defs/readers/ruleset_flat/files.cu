// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// files.cu: the files read: a file's bytes from the run or the disk, a file appended, the paths of a part, a ruleset's text, and what the relations and the maps said
#include "ruleset_flat_internal.h"

// The run's buffer (qry/buffer) is linked into a program of a run, and many a program reads rulesets with no run at
// all. Where it is not linked these stand in for its two calls the reader makes, weakly: no run, and every file read
// whole from the disk, as qry_file_read reads one where no run holds it. Linked, its own are taken
extern "C" QryBuffer *ruleset_run_none(void)
{
    return NULL;
}

extern "C" int ruleset_file_read_disk(QryBuffer *buffer, const char *path, unsigned char **bytes,
                                      unsigned long long *size)
{
    (void)buffer;
    *bytes = NULL;
    *size = 0ull;
    FILE *const file = fopen(path, "rb");
    if (file == NULL)
    {
        return 0;
    }
    size_t room = 65536u;
    size_t length = 0u;
    unsigned char *read = (unsigned char *)malloc(room + 1u);
    size_t got = 0u;
    while ((read != NULL) && ((got = fread(read + length, 1u, room - length, file)) != 0u))
    {
        length += got;
        if (length == room)
        {
            unsigned char *const larger = (unsigned char *)realloc(read, (room * 2u) + 1u);
            free((larger == NULL) ? read : NULL);
            read = larger;
            room *= 2u;
        }
    }
    fclose(file);
    *bytes = read;
    *size = length;
    return read != NULL;
}

#if defined(_MSC_VER)
#pragma comment(linker, "/alternatename:qry_run=ruleset_run_none")
#pragma comment(linker, "/alternatename:qry_file_read=ruleset_file_read_disk")
#else
extern "C" __attribute__((weak)) QryBuffer *qry_run(void)
{
    return ruleset_run_none();
}

extern "C" __attribute__((weak)) int qry_file_read(QryBuffer *buffer, const char *path, unsigned char **bytes,
                                                   unsigned long long *size)
{
    return ruleset_file_read_disk(buffer, path, bytes, size);
}
#endif

// the folder a part's files are read from: src/cu/transpiler/lstar/protocol/table in the tree this file was built from, where
// the part's machine file, its .kdm and its .ksc are
static std::string ruleset_part_folder(void)
{
    const std::string file = __FILE__;
    const size_t slash = file.find_last_of("/\\");
    const std::string here = (slash == std::string::npos) ? std::string() : file.substr(0u, slash + 1u);
    return here + "../../../transpiler/lstar/protocol/table";
}

int ruleset_bytes_read(const std::string &path, std::string *text)
{
    unsigned char *bytes = NULL;
    unsigned long long size = 0ull;
    if (!qry_file_read(qry_run(), path.c_str(), &bytes, &size))
    {
        return 0;
    }
    const unsigned long long room = (text->size() < RULESET_FILE_MAX) ? (RULESET_FILE_MAX - text->size()) : 0ull;
    // a ruleset the reader holds is below RULESET_FILE_MAX letters, which a size_t counts; one past it is cut there
    // and refused by the caller
    text->append((const char *)bytes, (size_t)((size < room) ? size : room));
    free(bytes);
    return 1;
}

// the file at `path` read whole onto the end of `text`, as the run holds it (ruleset_bytes_read)
int ruleset_append(const std::string &path, std::string *text)
{
    return ruleset_bytes_read(path, text);
}

// the lines of a part's .kdm or of a .ksc a ruleset reads: the text of each of its blocks of the ruleset's language,
// `__krs__(` to `)__krs__` (klang.h), each ended by a newline. The arrangements, the pipes, the questions and the
// comments are the file's own and stand outside every such block
static std::string ruleset_lines_read(const std::string &kdm)
{
    std::string lines;
    KlangBlock block;
    size_t from = 0u;
    while (klang_block_find(kdm.data(), kdm.size(), from, "krs", &block))
    {
        lines += kdm.substr(block.text, block.text_end - block.text);
        if (!lines.empty() && (lines.back() != '\n'))
        {
            lines.push_back('\n');
        }
        from = block.closed;
    }
    return lines;
}

// the part the ruleset at `path` names on its line `part <part>`, empty where it names none or cannot be opened
static std::string ruleset_part_named(const std::string &path)
{
    std::string text;
    if (ruleset_append(path, &text) == 0)
    {
        return std::string();
    }
    const std::string named = "part ";
    size_t at = 0u;
    while (at < text.size())
    {
        const size_t end = (text.find('\n', at) == std::string::npos) ? text.size() : text.find('\n', at);
        const std::string line = text.substr(at, end - at);
        if (line.compare(0u, named.size(), named) == 0)
        {
            return line.substr(named.size(), line.find_first_of(" \t\r", named.size()) - named.size());
        }
        at = end + 1u;
    }
    return std::string();
}

unsigned int ruleset_paths(const RulesetSchema *schema, const std::string &path, std::string paths[RULESET_PATHS_MAX])
{
    (void)schema;
    unsigned int count = 0u;
    paths[count] = path;
    count += 1u;
    const std::string named = ruleset_part_named(path);
    if (!named.empty())
    {
        const std::string part = ruleset_part_folder() + "/" + named;
        paths[count] = part + ".krs";
        paths[count + 1u] = part + ".kdm";
        count += 2u;
    }
    // the folds the .ksc beside the file holds as forms, where it has one, as the run holds it
    const std::string ksc = path.substr(0u, path.find_last_of('.')) + ".ksc";
    std::string held;
    if (ruleset_bytes_read(ksc, &held))
    {
        paths[count] = ksc;
        count += 1u;
    }
    return count;
}

int ruleset_text(const RulesetSchema *schema, const std::string &path, std::string *text, std::string *error)
{
    text->clear();
    std::string paths[RULESET_PATHS_MAX];
    const unsigned int count = ruleset_paths(schema, path, paths);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        std::string read;
        if (ruleset_append(paths[at], (at == 0u) ? text : &read) == 0)
        {
            *error = (at == 0u) ? std::string("it could not be opened")
                                : ("the part's " + paths[at] + " could not be opened");
            return 0;
        }
        if (at == 0u)
        {
            continue;
        }
        if ((!text->empty()) && (text->back() != '\n'))
        {
            text->push_back('\n');
        }
        // a part's .krs opens with krs 1 as every ruleset does, and the ruleset's own file has given that line; of a
        // .kdm and a .ksc only the ruleset lines are read
        const size_t opening = read.find('\n');
        const int krs = paths[at].compare(paths[at].size() - 4u, 4u, ".krs") == 0;
        text->append((krs == 0) ? ruleset_lines_read(read)
                                : ((opening == std::string::npos) ? std::string() : read.substr(opening + 1u)));
    }
    if (text->size() >= RULESET_FILE_MAX)
    {
        *error = "it is more than the reader holds";
        return 0;
    }
    return 1;
}

const char *ruleset_relation_said(unsigned int kind)
{
    static const char *const s_said[RULESET_CORE_RELATION_KINDS] = {
        "is given again after",
        "has the text of",
        "is under another name",
        "is, but for its modifiers and one word of its name,",
        "opens literal operands of",
        "writes a name no file gives",
        "gives another count of arguments than",
        "reaches itself through its line",
        "never writes its parameter",
        "writes a place its head does not take, at letter",
        "has nothing after its equals",
        "uses the numeric name of the register of",
        "is not named by the schema",
        "is named by the schema and given by no file",
        "takes another count of parameters than the schema gives",
        "is the arrangement of",
        "counts other than the questions written, which are",
        "answers with a class its file does not name",
    };
    return (kind < RULESET_CORE_RELATION_KINDS) ? s_said[kind] : "";
}

const char *ruleset_map_said(unsigned int kind)
{
    static const char *const s_said[RULESET_CORE_MAP_KINDS] = {
        "is one text in the first set and two in the second with",
        "is two texts in the first set and one in the second with",
        "is given as another kind in the second set:",
        "is given by no file of the second set",
    };
    return (kind < RULESET_CORE_MAP_KINDS) ? s_said[kind] : "";
}
