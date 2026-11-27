// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// interface_sass_probe_cubin.c: a cubin written from text. The probe learns what the part contains by reading its
// tools; this is the other direction, putting instructions of the interface's own into a cubin the part will load and
// run, with a cubin the toolchain built located as the pattern for everything an ELF carries that no instruction
// states
#include "../../../../../../includes/platform/platform.h"
#include "../cubin_write.h"
#include "../sass_assemble.h"
#include "interface_sass_probe.h"
#include "memoria_operor/memoria_operor.h"

// the max bytes a cubin, its code and its text take
#define SASS_CUBIN_BYTES 262144u
#define SASS_CODE_BYTES 65536u
#define SASS_TEXT_BYTES 262144u
#define SASS_EXITS 256u

static unsigned char s_pattern[SASS_CUBIN_BYTES];
static unsigned char s_cubin[SASS_CUBIN_BYTES];
static unsigned char s_code[SASS_CODE_BYTES];
static char s_text[SASS_TEXT_BYTES];
static unsigned int s_exits[SASS_EXITS];

// `path` read whole into `bytes`, which has room for `room` of them: how many were read, 0 where the file was not read
static unsigned long long sass_file_read(const char *path, unsigned char *bytes, unsigned long long room)
{
    PlatformFile *const file = platform_file_open(path, "rb");
    if (file == NULL)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  cubin: ");
            mmgr_line_put(output_line, path);
            mmgr_line_put(output_line, " could not be read");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0ULL;
    }
    const unsigned long long read = (unsigned long long)platform_file_read_items(bytes, 1u, (size_t)room, file);
    platform_file_close(file);
    return read;
}

static int sass_file_write(const char *path, const unsigned char *bytes, unsigned long long size)
{
    PlatformFile *const file = platform_file_open(path, "wb");
    const int written = (file != NULL) && (platform_file_write_items(bytes, 1u, (size_t)size, file) == size);
    const int closed = (file != NULL) && (platform_file_close(file) == 0);
    if (!written || !closed)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  cubin: ");
        mmgr_line_put(output_line, path);
        mmgr_line_put(output_line, " could not be written");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return written && closed;
}

unsigned int sass_cubin_text(const char *folder, const char *name, char *text, unsigned int room)
{
    char path[1024];
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, folder);
        mmgr_line_put(&buffer_line, "/");
        mmgr_line_put(&buffer_line, name);
        mmgr_line_put(&buffer_line, ".sass");
        (void)mmgr_line_finish(&buffer_line);
    }
    static unsigned char s_listing[SASS_TEXT_BYTES];
    const unsigned long long listing_size = sass_file_read(path, s_listing, sizeof(s_listing) - 1u);
    if (listing_size == 0ULL)
    {
        return 0u;
    }
    s_listing[listing_size] = '\0';
    const unsigned int text_size = sass_text_read((const char *)s_listing, "interface_probe", text, room);
    // the text the listing was turned back into, stored beside it for a reader
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, folder);
        mmgr_line_put(&buffer_line, "/");
        mmgr_line_put(&buffer_line, name);
        mmgr_line_put(&buffer_line, ".text");
        (void)mmgr_line_finish(&buffer_line);
    }
    sass_file_write(path, (const unsigned char *)text, text_size);
    return text_size;
}

int sass_cubin_kernel(const SassMachine *machine, const char *folder, const char *pattern, const char *text,
                      const char *into, const char *kernel)
{
    char path[1024];
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, folder);
        mmgr_line_put(&buffer_line, "/");
        mmgr_line_put(&buffer_line, pattern);
        mmgr_line_put(&buffer_line, ".cubin");
        (void)mmgr_line_finish(&buffer_line);
    }
    const unsigned long long pattern_size = sass_file_read(path, s_pattern, sizeof(s_pattern));
    if (pattern_size == 0ULL)
    {
        return 0;
    }
    const unsigned int instructions = sass_assemble_lines(machine, text, SASS_CONTROL_SAFE, s_code, sizeof(s_code));
    if (instructions == 0u)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  cubin: ");
            mmgr_line_put(output_line, into);
            mmgr_line_put(output_line, " assembled nothing");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 0;
    }
    const unsigned long long code_size = 16ULL * instructions;
    CubinWrite written;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &written, .bytes = sizeof(written), .val = 0);
    written.pattern = s_pattern;
    written.pattern_size = pattern_size;
    written.kernel = kernel;
    written.code = s_code;
    written.code_size = code_size;
    written.registers = cubin_registers_read(s_pattern, pattern_size, kernel);
    written.exit_count = cubin_exits_find(s_code, code_size, sass_exit_encoding(machine), s_exits, SASS_EXITS);
    written.exits = s_exits;
    unsigned long long size = 0ULL;
    if (!cubin_write(&written, s_cubin, sizeof(s_cubin), &size))
    {
        return 0;
    }
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, folder);
        mmgr_line_put(&buffer_line, "/");
        mmgr_line_put(&buffer_line, into);
        mmgr_line_put(&buffer_line, ".cubin");
        (void)mmgr_line_finish(&buffer_line);
    }
    return sass_file_write(path, s_cubin, size);
}

int sass_cubin_from_text(const SassMachine *machine, const char *folder, const char *pattern, const char *text,
                         const char *into)
{
    return sass_cubin_kernel(machine, folder, pattern, text, into, "interface_probe");
}

int sass_cubin_round(const SassMachine *machine, const char *folder, const char *name)
{
    char into[256];
    {
        VerbaLinea buffer_line = mmgr_line_over(into, sizeof(into));
        mmgr_line_put(&buffer_line, name);
        mmgr_line_put(&buffer_line, "_written");
        (void)mmgr_line_finish(&buffer_line);
    }
    return (sass_cubin_text(folder, name, s_text, sizeof(s_text)) != 0u) &&
           sass_cubin_from_text(machine, folder, name, s_text, into);
}

int sass_cubin_lane_into(const SassMachine *machine, const char *folder, const char *text, const char *into)
{
    return sass_cubin_kernel(machine, folder, "resident", text, into, "cycle_lane");
}
