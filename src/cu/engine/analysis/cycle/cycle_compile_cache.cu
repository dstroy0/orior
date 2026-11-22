// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cycle_compile_cache.cu: the cache, compiling and linking, the PTX header
#include "../../../../platform/platform.h"
#include "../../engine_note.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "cycle_compile_internal.h"
#include "memoria_operor/memoria_linea.h"

// the target for relocatable cubin, then the target for LTO-IR
static CycleTarget s_cycle_lane_targets[2];

static const char s_cycle_cache_magic[] = "cycle image 2\n";

// the bytes a cache file holds at max: its source and its image
#define CYCLE_CACHE_FILE_MAX (1ULL << 32u)

int cycle_cache_read(const char *path, const char *source, unsigned long long length, CycleImage *image)
{
    image->bytes = NULL;
    image->count = 0ULL;
    PlatformFile *const file = platform_file_open(path, "rb");
    if (file == NULL)
    {
        return 0;
    }
    PlatformText contents;
    platform_text_begun(&contents, CYCLE_CACHE_FILE_MAX, platform_place_g_program_owner, 0);
    char block[65536];
    size_t read = platform_file_read_items(block, 1u, sizeof(block), file);
    while (read != 0u)
    {
        (void)mmgr_line_put_n(&contents.line, block, read);
        read = platform_file_read_items(block, 1u, sizeof(block), file);
    }
    platform_file_close(file);
    const char *const bytes = platform_text_bytes(&contents);
    const unsigned long long size = platform_text_size(&contents);
    const unsigned long long magic = sizeof(s_cycle_cache_magic) - 1u;
    const unsigned long long head = magic + sizeof(unsigned long long);
    unsigned long long stored = 0ULL;
    if (size >= head)
    {
        mmgr_bytes_copy(&stored, &bytes[magic], sizeof(stored));
    }
    const int ok = (mmgr_line_ok(&contents.line) != 0) && (size >= head) &&
                   (mmgr_bytes_compare(bytes, s_cycle_cache_magic, (size_t)magic) == 0) && (stored == length) &&
                   ((size - head) > stored) && (mmgr_bytes_compare(&bytes[head], source, (size_t)length) == 0);
    const int taken = ok && cycle_image_taken(image, &bytes[head + stored], size - (head + stored));
    platform_text_ended(&contents);
    return taken;
}

// the source and its image written beside the name and then renamed to it, and no reader finds half a file; a name
// already taken is left as it is
void cycle_cache_write(const char *folder, const char *path, const char *source, unsigned long long length,
                       const void *image, unsigned long long count)
{
#if defined(_WIN32)
    _mkdir(folder);
    const int process = _getpid();
#else
    mkdir(folder, 0755);
    // a pid is positive. It converts exactly
    const int process = (int)getpid();
#endif
    char partial[PLATFORM_PATH_BYTES];
    {
        VerbaLinea buffer_line = mmgr_line_over(partial, sizeof(partial));
        mmgr_line_put(&buffer_line, path);
        mmgr_line_put(&buffer_line, ".");
        mmgr_line_decimal_signed(&buffer_line, process);
        mmgr_line_put(&buffer_line, ".partial");
        (void)mmgr_line_finish(&buffer_line);
    }
    PlatformFile *const file = platform_file_open(partial, "wb");
    if (file == NULL)
    {
        return;
    }
    const int written = (platform_file_write_items(s_cycle_cache_magic, 1u, sizeof(s_cycle_cache_magic) - 1u, file) ==
                         (sizeof(s_cycle_cache_magic) - 1u)) &&
                        (platform_file_write_items(&length, sizeof(length), 1u, file) == 1u) &&
                        (platform_file_write_items(source, 1u, (size_t)length, file) == length) &&
                        (platform_file_write_items(image, 1u, (size_t)count, file) == count);
    const int closed = platform_file_close(file) == 0;
    if (!written || !closed || !platform_file_moved(partial, path))
    {
        (void)platform_file_removed(partial);
    }
}

// a log on stderr after `head`
static void cycle_log_noted(const char *head, const char *log)
{
    EngineNote engine_note;
    engine_note_begun(&engine_note);
    mmgr_line_put(&engine_note.line, head);
    mmgr_line_put(&engine_note.line, log);
    mmgr_line_ch(&engine_note.line, '\n');
    engine_note_written(&engine_note);
}

// the source compiled by NVRTC as relocatable code for the device's architecture: a relocatable cubin, or LTO-IR where
// `lto`; 0 where it errors, its log then on stderr when reporting
int cycle_program_compile(const char *source, const char *name, int major, int minor, int lto, int report,
                          CycleImage *image)
{
    CycleCompiler *const compiler = &g_cycle_compiler;
    image->bytes = NULL;
    image->count = 0ULL;
    nvrtcProgram program = NULL;
    if (compiler->create(&program, source, name, 0, NULL, NULL) != NVRTC_SUCCESS)
    {
        return 0;
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
        image->bytes = (unsigned char *)platform_cell_taken(size, platform_place_g_program_owner, 0);
        // NVRTC writes the image as its own bytes, which a char pointer reads
        const nvrtcResult taken = (image->bytes == NULL) ? NVRTC_ERROR_OUT_OF_MEMORY
                                  : (lto != 0)           ? compiler->ltoir(program, (char *)image->bytes)
                                                         : compiler->cubin(program, (char *)image->bytes);
        image->count = (taken == NVRTC_SUCCESS) ? size : 0ULL;
        if (taken != NVRTC_SUCCESS)
        {
            cycle_image_ended(image);
        }
    }
    size_t log_size = 0u;
    if ((compiled != NVRTC_SUCCESS) && (report != 0) && (compiler->log_size(program, &log_size) == NVRTC_SUCCESS) &&
        (log_size > 1u))
    {
        char *const log = (char *)platform_cell_taken(log_size, platform_place_g_program_owner, 1);
        if ((log != NULL) && (compiler->log(program, log) == NVRTC_SUCCESS))
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  cycle: NVRTC errored ");
            mmgr_line_put(&engine_note.line, name);
            mmgr_line_put(&engine_note.line, " (");
            mmgr_line_decimal_signed(&engine_note.line, (int)compiled);
            mmgr_line_put(&engine_note.line, "):\n");
            mmgr_line_put(&engine_note.line, log);
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        platform_cell_released(log, platform_place_g_program_owner);
    }
    compiler->destroy(&program);
    return (image->bytes != NULL) ? 1 : 0;
}

// a program's relocatable image linked alone by nvJitLink into one cubin, its resident and its lane in the one text,
// with link-time optimization where `lto`; where `ptx` the program is PTX's text, NUL and all, which nvJitLink
// assembles as it links. 0 where it errors, its log then on stderr when reporting
int cycle_program_link(const CycleTarget *lane_target, const void *object, unsigned long long count, int lto, int ptx,
                       int report, CycleImage *cubin)
{
    CycleLinker *const linker = &g_cycle_linker;
    cubin->bytes = NULL;
    cubin->count = 0ULL;
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
        return 0;
    }
    const nvJitLinkInputType type = (lto != 0) ? NVJITLINK_INPUT_LTOIR : NVJITLINK_INPUT_CUBIN;
    const nvJitLinkInputType program_type = (ptx != 0) ? NVJITLINK_INPUT_PTX : type;
    size_t size = 0u;
    const int linked =
        (linker->add(handle, program_type, object, (size_t)count, "cycle_program") == NVJITLINK_SUCCESS) &&
        (linker->complete(handle) == NVJITLINK_SUCCESS) && (linker->cubin_size(handle, &size) == NVJITLINK_SUCCESS) &&
        (size != 0u);
    if (linked)
    {
        cubin->bytes = (unsigned char *)platform_cell_taken(size, platform_place_g_program_owner, 0);
        cubin->count = size;
        if ((cubin->bytes == NULL) || (linker->cubin(handle, cubin->bytes) != NVJITLINK_SUCCESS))
        {
            cycle_image_ended(cubin);
        }
    }
    size_t log_size = 0u;
    if (!linked && (report != 0) && (linker->log_size(handle, &log_size) == NVJITLINK_SUCCESS) && (log_size > 1u))
    {
        char *const log = (char *)platform_cell_taken(log_size, platform_place_g_program_owner, 1);
        if ((log != NULL) && (linker->log(handle, log) == NVJITLINK_SUCCESS))
        {
            cycle_log_noted("  cycle: nvJitLink errored on a record program:\n", log);
        }
        platform_cell_released(log, platform_place_g_program_owner);
    }
    linker->destroy(&handle);
    return (cubin->bytes != NULL) ? 1 : 0;
}

// the most bytes of the target's text, its prelude among them
#define CYCLE_TARGET_SOURCE_BYTES (1ULL << 24u)

// the target for the device and the type of link, named once a process; it builds nothing and is always ready
const CycleTarget *cycle_target(int major, int minor, int lto, int report)
{
    CycleTarget *const lane_target = &s_cycle_lane_targets[(lto != 0) ? 1 : 0];
    if ((lane_target->tried != 0) && (lane_target->major == major) && (lane_target->minor == minor))
    {
        return lane_target;
    }
    // a text with no max has not begun: it begins at the target's first naming
    if (lane_target->source.max == 0ULL)
    {
        platform_text_begun(&lane_target->source, CYCLE_TARGET_SOURCE_BYTES, platform_place_g_program_owner, 0);
    }
    lane_target->tried = 1;
    lane_target->ready = 1;
    lane_target->major = major;
    lane_target->minor = minor;
    platform_text_reset(&lane_target->source);
    cycle_target_source(major, minor, lto, &lane_target->source);
    lane_target->hash =
        cycle_source_hash(platform_text_bytes(&lane_target->source), platform_text_size(&lane_target->source));
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

// The first line of `ptx` that opens with `opening` copied onto `lines` with a newline after it: 1, or 0 where no line
// opens so
static int cycle_ptx_header_line(const char *ptx, const char *opening, VerbaLinea *lines)
{
    const size_t opening_length = mmgr_text_length(opening, PLATFORM_TEXT_TO_TERMINATOR);
    const char *line = ptx;
    while (*line != '\0')
    {
        const char *const end = mmgr_text_char(line, '\n', PLATFORM_TEXT_TO_TERMINATOR);
        // a line ends past its start, and its length is never negative
        const size_t length =
            (end != NULL) ? (size_t)(end - line) : mmgr_text_length(line, PLATFORM_TEXT_TO_TERMINATOR);
        if ((length >= opening_length) && (mmgr_bytes_compare(line, opening, opening_length) == 0))
        {
            (void)mmgr_line_put_n(lines, line, length);
            mmgr_line_ch(lines, '\n');
            return 1;
        }
        line = (end != NULL) ? (end + 1) : (line + length);
    }
    return 0;
}

// The .version, .target and .address_size lines of a PTX text, the first of each, in that order, into `lines`, `bytes`
// of it. Empty where one is missing or the address size is not 64, which the lane's pointers are written for
static void cycle_ptx_header_lines(const char *ptx, char *lines, size_t bytes)
{
    VerbaLinea written = mmgr_line_over(lines, bytes);
    char address[32];
    VerbaLinea address_line = mmgr_line_over(address, sizeof(address));
    const int complete = cycle_ptx_header_line(ptx, ".version ", &written) &&
                         cycle_ptx_header_line(ptx, ".target ", &written) &&
                         cycle_ptx_header_line(ptx, ".address_size ", &address_line);
    (void)mmgr_line_finish(&address_line);
    const int wide = complete && (mmgr_text_order(address, ".address_size 64\n", sizeof(address)) == 0);
    mmgr_line_put(&written, address);
    (void)mmgr_line_finish(&written);
    if ((wide == 0) || (mmgr_line_ok(&written) == 0))
    {
        lines[0] = '\0';
    }
}

static CyclePtxHeader s_cycle_ptx_header;

// the bytes of the question put to NVRTC for PTX's header, its terminator among them
#define CYCLE_PTX_QUESTION_BYTES 256u

// the header's three lines; empty where NVRTC cannot be queried or does not answer, which leaves programs to the C
// source
const char *cycle_ptx_header(int major, int minor, int report)
{
    CyclePtxHeader *const header = &s_cycle_ptx_header;
    if ((header->tried != 0) && (header->major == major) && (header->minor == minor))
    {
        return header->lines;
    }
    header->tried = 1;
    header->major = major;
    header->minor = minor;
    header->lines[0] = '\0';
    CycleCompiler *const compiler = &g_cycle_compiler;
    if ((compiler->ptx_size == NULL) || (compiler->ptx == NULL))
    {
        return header->lines;
    }
    char question[CYCLE_PTX_QUESTION_BYTES];
    {
        VerbaLinea question_line = mmgr_line_over(question, sizeof(question));
        mmgr_line_put(&question_line, "// PTX's header, requested for sm_");
        mmgr_line_decimal_signed(&question_line, major);
        mmgr_line_decimal_signed(&question_line, minor);
        mmgr_line_put(&question_line, " of NVRTC ");
        mmgr_line_decimal_signed(&question_line, compiler->major);
        mmgr_line_put(&question_line, ".");
        mmgr_line_decimal_signed(&question_line, compiler->minor);
        mmgr_line_ch(&question_line, '\n');
        mmgr_line_put(&question_line, "extern \"C\" __global__ void cycle_probe(void)\n{\n}\n");
        (void)mmgr_line_finish(&question_line);
    }
    const unsigned long long question_length = mmgr_text_length(question, sizeof(question));
    char folder[PLATFORM_PATH_BYTES];
    char path[PLATFORM_PATH_BYTES];
    const int cached = cycle_cache_folder(folder, sizeof(folder));
    path[0] = '\0';
    if (cached != 0)
    {
        cycle_cache_path(folder, question, question_length, path, sizeof(path));
    }
    CycleImage answer = {NULL, 0ULL};
    const int found = (cached != 0) && cycle_cache_read(path, question, question_length, &answer);
    if (found != 0)
    {
        VerbaLinea answer_line = mmgr_line_over(header->lines, sizeof(header->lines));
        (void)mmgr_line_put_n(&answer_line, (const char *)answer.bytes, (size_t)answer.count);
        (void)mmgr_line_finish(&answer_line);
        if (mmgr_line_ok(&answer_line) == 0)
        {
            header->lines[0] = '\0';
        }
    }
    cycle_image_ended(&answer);
    nvrtcProgram program = NULL;
    if (!found && (compiler->create(&program, question, "cycle_probe.cu", 0, NULL, NULL) == NVRTC_SUCCESS))
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
            char *const ptx = (char *)platform_cell_taken(size, platform_place_g_program_owner, 1);
            if ((ptx != NULL) && (compiler->ptx(program, ptx) == NVRTC_SUCCESS))
            {
                ptx[size - 1u] = '\0';
                cycle_ptx_header_lines(ptx, header->lines, sizeof(header->lines));
            }
            platform_cell_released(ptx, platform_place_g_program_owner);
        }
        compiler->destroy(&program);
        if ((header->lines[0] != '\0') && (cached != 0))
        {
            cycle_cache_write(folder, path, question, question_length, header->lines,
                              mmgr_text_length(header->lines, sizeof(header->lines)));
        }
    }
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
        mmgr_line_put(&engine_note.line, (header->lines[0] == '\0') ? "none" : header->lines);
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    return header->lines;
}
