// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cycle_compile_cache.cu: the cache, compiling and linking, the PTX header
#include "cycle_compile_internal.h"
#include "../../../../platform/platform.h"
#include "../../engine_note.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"

// the target for relocatable cubin, then the target for LTO-IR
static CycleTarget s_cycle_lane_targets[2];

static const char s_cycle_cache_magic[] = "cycle image 2\n";

// the image a cache file contains for this very source; empty where there is none
std::vector<char> cycle_cache_read(const std::string &path, const std::string &source)
{
    std::vector<char> image;
    PlatformFile *const file = platform_file_open(path.c_str(), "rb");
    if (file == NULL)
    {
        return image;
    }
    std::vector<char> contents;
    char block[65536];
    size_t read = platform_file_read_items(block, 1u, sizeof(block), file);
    while (read != 0u)
    {
        contents.insert(contents.end(), block, block + read);
        read = platform_file_read_items(block, 1u, sizeof(block), file);
    }
    platform_file_close(file);
    const size_t magic = sizeof(s_cycle_cache_magic) - 1u;
    const size_t head = magic + sizeof(unsigned long long);
    unsigned long long length = 0ULL;
    if (contents.size() >= head)
    {
        mmgr_bytes_copy(&length, &contents[magic], sizeof(length));
    }
    const int ok = (contents.size() >= head) && (mmgr_bytes_compare(&contents[0], s_cycle_cache_magic, magic) == 0) &&
                   (length == source.size()) && ((contents.size() - head) > length) &&
                   (mmgr_bytes_compare(&contents[head], source.data(), source.size()) == 0);
    if (ok)
    {
        // a source length the file read whole is below its size. It converts exactly
        image.assign(contents.begin() + (ptrdiff_t)(head + length), contents.end());
    }
    return image;
}

// the source and its image written beside the name and then renamed to it, and no reader finds half a file; a name
// already taken is left as it is
void cycle_cache_write(const std::string &folder, const std::string &path, const std::string &source,
                       const std::vector<char> &image)
{
#if defined(_WIN32)
    _mkdir(folder.c_str());
    const int process = _getpid();
#else
    mkdir(folder.c_str(), 0755);
    // a pid is positive. It converts exactly
    const int process = (int)getpid();
#endif
    char suffix[32];
    {
        VerbaLinea buffer_line = mmgr_line_over(suffix, sizeof(suffix));
        mmgr_line_put(&buffer_line, ".");
        mmgr_line_decimal_signed(&buffer_line, process);
        mmgr_line_put(&buffer_line, ".partial");
        (void)mmgr_line_finish(&buffer_line);
    }
    const std::string partial = path + suffix;
    PlatformFile *const file = platform_file_open(partial.c_str(), "wb");
    if (file == NULL)
    {
        return;
    }
    const unsigned long long length = source.size();
    const int written = (platform_file_write_items(s_cycle_cache_magic, 1u, sizeof(s_cycle_cache_magic) - 1u, file) ==
                         (sizeof(s_cycle_cache_magic) - 1u)) &&
                        (platform_file_write_items(&length, sizeof(length), 1u, file) == 1u) &&
                        (platform_file_write_items(source.data(), 1u, source.size(), file) == source.size()) &&
                        (platform_file_write_items(image.data(), 1u, image.size(), file) == image.size());
    const int closed = platform_file_close(file) == 0;
    if (!written || !closed || !platform_file_moved(partial.c_str(), path.c_str()))
    {
        (void)platform_file_removed(partial.c_str());
    }
}

// the source compiled by NVRTC as relocatable code for the device's architecture: a relocatable cubin, or LTO-IR where
// `lto`; empty where it errors, its log then on stderr when reporting
std::vector<char> cycle_program_compile(const std::string &source, const char *name, int major, int minor, int lto,
                                        int report)
{
    CycleCompiler *const compiler = &g_cycle_compiler;
    std::vector<char> image;
    nvrtcProgram program = NULL;
    if (compiler->create(&program, source.c_str(), name, 0, NULL, NULL) != NVRTC_SUCCESS)
    {
        return image;
    }
    char architecture[48];
    {
        VerbaLinea buffer_line = mmgr_line_over(architecture, sizeof(architecture));
        mmgr_line_put(&buffer_line, "--gpu-architecture=sm_");
        mmgr_line_decimal_signed(&buffer_line, major);
        mmgr_line_decimal_signed(&buffer_line, minor);
        (void)mmgr_line_finish(&buffer_line);
    }
    const char *const options[] = {architecture, "--std=c++17", "--relocatable-device-code=true", "-dlto"};
    const nvrtcResult compiled = compiler->compile(program, (lto != 0) ? 4 : 3, options);
    size_t size = 0u;
    const nvrtcResult sized = (lto != 0) ? compiler->ltoir_size(program, &size) : compiler->cubin_size(program, &size);
    if ((compiled == NVRTC_SUCCESS) && (sized == NVRTC_SUCCESS) && (size != 0u))
    {
        image.resize(size);
        const nvrtcResult taken =
            (lto != 0) ? compiler->ltoir(program, image.data()) : compiler->cubin(program, image.data());
        if (taken != NVRTC_SUCCESS)
        {
            image.clear();
        }
    }
    size_t log_size = 0u;
    if ((compiled != NVRTC_SUCCESS) && (report != 0) && (compiler->log_size(program, &log_size) == NVRTC_SUCCESS) &&
        (log_size > 1u))
    {
        std::vector<char> log(log_size);
        if (compiler->log(program, log.data()) == NVRTC_SUCCESS)
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  cycle: NVRTC errored ");
            mmgr_line_put(&engine_note.line, name);
            mmgr_line_put(&engine_note.line, " (");
            mmgr_line_decimal_signed(&engine_note.line, (int)compiled);
            mmgr_line_put(&engine_note.line, "):\n");
            mmgr_line_put(&engine_note.line, log.data());
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
    }
    compiler->destroy(&program);
    return image;
}

// a program's relocatable image linked alone by nvJitLink into one cubin, its resident and its lane in the one text,
// with link-time optimization where `lto`; where `ptx` the program is PTX's text, NUL and all, which nvJitLink
// assembles as it links. Empty where it errors, its log then on stderr when reporting
std::vector<char> cycle_program_link(const CycleTarget *lane_target, const std::vector<char> &object, int lto, int ptx,
                                     int report)
{
    CycleLinker *const linker = &g_cycle_linker;
    std::vector<char> cubin;
    char architecture[32];
    {
        VerbaLinea buffer_line = mmgr_line_over(architecture, sizeof(architecture));
        mmgr_line_put(&buffer_line, "-arch=sm_");
        mmgr_line_decimal_signed(&buffer_line, lane_target->major);
        mmgr_line_decimal_signed(&buffer_line, lane_target->minor);
        (void)mmgr_line_finish(&buffer_line);
    }
    const char *options[] = {architecture, "-lto"};
    nvJitLinkHandle handle = NULL;
    if (linker->create(&handle, (lto != 0) ? 2u : 1u, options) != NVJITLINK_SUCCESS)
    {
        return cubin;
    }
    const nvJitLinkInputType type = (lto != 0) ? NVJITLINK_INPUT_LTOIR : NVJITLINK_INPUT_CUBIN;
    const nvJitLinkInputType program_type = (ptx != 0) ? NVJITLINK_INPUT_PTX : type;
    size_t size = 0u;
    const int linked =
        (linker->add(handle, program_type, object.data(), object.size(), "cycle_program") == NVJITLINK_SUCCESS) &&
        (linker->complete(handle) == NVJITLINK_SUCCESS) && (linker->cubin_size(handle, &size) == NVJITLINK_SUCCESS) &&
        (size != 0u);
    if (linked)
    {
        cubin.resize(size);
        if (linker->cubin(handle, cubin.data()) != NVJITLINK_SUCCESS)
        {
            cubin.clear();
        }
    }
    size_t log_size = 0u;
    if (!linked && (report != 0) && (linker->log_size(handle, &log_size) == NVJITLINK_SUCCESS) && (log_size > 1u))
    {
        std::vector<char> log(log_size);
        if (linker->log(handle, log.data()) == NVJITLINK_SUCCESS)
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  cycle: nvJitLink errored on a record program:\n");
            mmgr_line_put(&engine_note.line, log.data());
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
    }
    linker->destroy(&handle);
    return cubin;
}

// the target for the device and the type of link, named once a process; it builds nothing and is always ready
const CycleTarget *cycle_target(int major, int minor, int lto, int report)
{
    CycleTarget *const lane_target = &s_cycle_lane_targets[(lto != 0) ? 1 : 0];
    if ((lane_target->tried != 0) && (lane_target->major == major) && (lane_target->minor == minor))
    {
        return lane_target;
    }
    lane_target->tried = 1;
    lane_target->ready = 1;
    lane_target->major = major;
    lane_target->minor = minor;
    lane_target->source = cycle_target_source(major, minor, lto);
    lane_target->hash = cycle_source_hash(lane_target->source);
    if (report != 0)
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  cycle: lanes written for sm_");
        mmgr_line_decimal_signed(&engine_note.line, major);
        mmgr_line_decimal_signed(&engine_note.line, minor);
        mmgr_line_put(&engine_note.line, " as ");
        mmgr_line_put(&engine_note.line, (lto != 0) ? "LTO-IR" : "relocatable cubin");
        mmgr_line_put(&engine_note.line, " against ");
        mmgr_line_hex(&engine_note.line, lane_target->hash, 16u);
        mmgr_line_put(&engine_note.line, ", each with its own resident");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    return lane_target;
}

// the .version, .target and .address_size lines of a PTX text, the first of each, in that order. Empty where one is
// missing or the address size is not 64, which the lane's pointers are written for
static std::string cycle_ptx_header_lines(const char *ptx)
{
    std::string version;
    std::string target;
    std::string address_size;
    const char *line = ptx;
    while (*line != '\0')
    {
        const char *const end = mmgr_text_char(line, '\n', PLATFORM_TEXT_TO_TERMINATOR);
        // a line ends past its start, and its length is never negative
        const size_t length =
            (end != NULL) ? (size_t)(end - line) : mmgr_text_length(line, PLATFORM_TEXT_TO_TERMINATOR);
        const std::string text_line(line, length);
        if (version.empty() && (text_line.compare(0u, 9u, ".version ") == 0))
        {
            version = text_line;
        }
        else if (target.empty() && (text_line.compare(0u, 8u, ".target ") == 0))
        {
            target = text_line;
        }
        else if (address_size.empty() && (text_line.compare(0u, 14u, ".address_size ") == 0))
        {
            address_size = text_line;
        }
        line = (end != NULL) ? (end + 1) : (line + length);
    }
    const int complete = !version.empty() && !target.empty() && (address_size == ".address_size 64");
    return complete ? (version + "\n" + target + "\n" + address_size + "\n") : std::string();
}

static CyclePtxHeader s_cycle_ptx_header;

// the header's three lines; empty where NVRTC cannot be queried or does not answer, which leaves programs to the C
// source
const std::string &cycle_ptx_header(int major, int minor, int report)
{
    CyclePtxHeader *const header = &s_cycle_ptx_header;
    if ((header->tried != 0) && (header->major == major) && (header->minor == minor))
    {
        return header->lines;
    }
    header->tried = 1;
    header->major = major;
    header->minor = minor;
    header->lines.clear();
    CycleCompiler *const compiler = &g_cycle_compiler;
    if ((compiler->ptx_size == NULL) || (compiler->ptx == NULL))
    {
        return header->lines;
    }
    std::string question;
    {
        TextNote text_note;
        text_note_begun(&text_note, &question);
        mmgr_line_put(&text_note.line, "// PTX's header, requested for sm_");
        mmgr_line_decimal_signed(&text_note.line, major);
        mmgr_line_decimal_signed(&text_note.line, minor);
        mmgr_line_put(&text_note.line, " of NVRTC ");
        mmgr_line_decimal_signed(&text_note.line, compiler->major);
        mmgr_line_put(&text_note.line, ".");
        mmgr_line_decimal_signed(&text_note.line, compiler->minor);
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    question += "extern \"C\" __global__ void cycle_probe(void)\n{\n}\n";
    const std::string folder = cycle_cache_folder();
    const std::string path = folder.empty() ? std::string() : cycle_cache_path(folder, question);
    const std::vector<char> cached = path.empty() ? std::vector<char>() : cycle_cache_read(path, question);
    std::string answer(cached.begin(), cached.end());
    const int found = !answer.empty();
    nvrtcProgram program = NULL;
    if (!found && (compiler->create(&program, question.c_str(), "cycle_probe.cu", 0, NULL, NULL) == NVRTC_SUCCESS))
    {
        char architecture[48];
        {
            VerbaLinea buffer_line = mmgr_line_over(architecture, sizeof(architecture));
            mmgr_line_put(&buffer_line, "--gpu-architecture=compute_");
            mmgr_line_decimal_signed(&buffer_line, major);
            mmgr_line_decimal_signed(&buffer_line, minor);
            (void)mmgr_line_finish(&buffer_line);
        }
        const char *const options[] = {architecture};
        size_t size = 0u;
        if ((compiler->compile(program, 1, options) == NVRTC_SUCCESS) &&
            (compiler->ptx_size(program, &size) == NVRTC_SUCCESS) && (size > 1u))
        {
            std::vector<char> ptx(size);
            if (compiler->ptx(program, ptx.data()) == NVRTC_SUCCESS)
            {
                ptx[size - 1u] = '\0';
                answer = cycle_ptx_header_lines(ptx.data());
            }
        }
        compiler->destroy(&program);
        if (!answer.empty() && !path.empty())
        {
            cycle_cache_write(folder, path, question, std::vector<char>(answer.begin(), answer.end()));
        }
    }
    header->lines = answer;
    if (report != 0)
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  cycle: PTX's header for sm_");
        mmgr_line_decimal_signed(&engine_note.line, major);
        mmgr_line_decimal_signed(&engine_note.line, minor);
        mmgr_line_put(&engine_note.line, " ");
        mmgr_line_put(&engine_note.line, found ? "read from the cache" : "requested from NVRTC");
        mmgr_line_put(&engine_note.line, ": ");
        mmgr_line_put(&engine_note.line, answer.empty() ? "none" : answer.c_str());
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    return header->lines;
}
