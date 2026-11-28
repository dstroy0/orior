// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// What the cycle_compile_*.cu pieces share: its includes, types and the functions one piece calls in another
#ifndef CYCLE_COMPILE_INTERNAL_H
#define CYCLE_COMPILE_INTERNAL_H

#include "../../../../../includes/platform/platform.h"
#include "cycle_shared.h"

// The record program, compiled. No hand-written code links with it. The own forms of the language write every record
// operation into the lane, and the resident kernel that runs the lanes after it (program_unit). One text is the whole
// program.
//
// The compiler writes a program first as PTX (code_generator("ptx.krs")). It unrolls each step at its widths into
// straight-line assembly over registers that the lane uses. A step that loops on its values is a loop of the same
// forms. The tool nvJitLink assembles and links it alone, with no compiler between.
//
// Where the compiler cannot write the lane so, or its PTX does not build, the lane is C source
// (code_generator("c.krs")). It is the same lane in c.krs, with its file and signs in the shared memory of the thread
// block. The count file_limbs is the max that it stores live at once. NVRTC compiles it as relocatable code, and the
// program links it alone in the same way. A program of any length builds in both ways.
//
// A thread block holds as many threads as its shared memory fits of the places of the C lane, and a PTX lane takes
// none. The interpreter above stays the oracle. It is also the fallback for a program that builds in neither way, or
// that this process cannot load. The same is true where shared memory cannot fit the places of one thread.
//
// The compiled program runs as a resident program with a block (EngineProgramBlock) in device memory. Its thread blocks
// take lanes one round at a time from one counter, and check in to the block as they go. They leave after the launch
// runs out its time to live, or when the command of the block tells them to.
//
// The last one out writes where the program is. The run launches it again from there until every lane is complete. A
// launch never outlives the watchdog of the display driver, and no lane runs twice. The host reads the block only
// after a launch ended.
//
// Eight switches, read at each load or run:
//
// - CYCLE_RECORD_INTERPRET=1 keeps every program on the interpreter.
// - CYCLE_RECORD_CHECK=1 runs both on every launch, and gives an error on the launch where their records or errors
//   differ.
// - CYCLE_RECORD_REPORT=1 reports on stderr where each program came from and how long each kernel ran.
// - CYCLE_RECORD_TTL=<microseconds> sets the time to live of a launch.
// - CYCLE_RECORD_LTO=1 builds the programs as LTO-IR, and links each with link-time optimization, which writes no PTX.
// - CYCLE_RECORD_NVRTC=1 writes every lane as C source.
// - CYCLE_RECORD_HOST_C=1 writes it so, and builds it with the compiler of the host, to run on the host
//   (cycle_compile.cu).
// - CYCLE_RECORD_RETAIN_PTX=1 turns rule (i) off (cycle_record_route).
//
// No switch turns off the writes of the device. The device writes each lane from its step table, and the program
// checks its text against the text of the host code generator (cycle_codegen_on_device). That includes the own lane of
// the assembly printer, which runs on the interpreter while it writes that one.

static_assert(ENGINE_RECORD_MEMBERS_MAX == 3u, "cycle: the compiled program's launch carries three members");

// NVRTC, loaded once a process first compiles
struct CycleCompiler
{
    int tried;
    int ready;
    int major;
    int minor;
    decltype(&nvrtcCreateProgram) create;
    decltype(&nvrtcCompileProgram) compile;
    decltype(&nvrtcGetCUBINSize) cubin_size;
    decltype(&nvrtcGetCUBIN) cubin;
    decltype(&nvrtcGetLTOIRSize) ltoir_size;
    decltype(&nvrtcGetLTOIR) ltoir;
    // the program reads PTX once in a process, for its header alone (cycle_ptx_header). An NVRTC without it leaves the
    // lane of every program to the C source
    decltype(&nvrtcGetPTXSize) ptx_size;
    decltype(&nvrtcGetPTX) ptx;
    decltype(&nvrtcGetProgramLogSize) log_size;
    decltype(&nvrtcGetProgramLog) log;
    decltype(&nvrtcDestroyProgram) destroy;
};

extern CycleCompiler g_cycle_compiler;

// nvJitLink, loaded once in a process, at the first link. Each call goes by the versioned name of the header that the
// build used
struct CycleLinker
{
    int tried;
    int ready;
    decltype(&nvJitLinkCreate) create;
    decltype(&nvJitLinkAddData) add;
    decltype(&nvJitLinkComplete) complete;
    decltype(&nvJitLinkGetLinkedCubinSize) cubin_size;
    decltype(&nvJitLinkGetLinkedCubin) cubin;
    decltype(&nvJitLinkGetErrorLogSize) log_size;
    decltype(&nvJitLinkGetErrorLog) log;
    decltype(&nvJitLinkDestroy) destroy;
};

extern CycleLinker g_cycle_linker;

// what the program writes a lane for, one device and one type of link, named once in a process. It has the device, and
// the text that names it with the prelude that a C lane opens with. The first line of each program carries its hash.
// The program compiles nothing for it. The text of every program holds its own resident (program_unit), and the
// program builds each program alone
struct CycleTarget
{
    int tried;
    int ready;
    int major;
    int minor;
    unsigned long long hash;
    PlatformText source;
};

// A program that this process compiled. The process finds it again by its whole source, a cell of `length` bytes.
// `holders` counts the loaded records that reference it, and the last release unloads it
struct CycleCompiledProgram
{
    const void *source;
    unsigned long long length;
    cudaLibrary_t library;
    cudaKernel_t kernel;
    unsigned long long holders;
};

// `count` bytes in a cell of the temporary tier, an image built from a source, which the caller gives back before its
// work ends
struct CycleImage
{
    unsigned char *bytes;
    unsigned long long count;
};

// `count` bytes of `from` copied into a cell of their own in `image`, with a byte 0 after them: 1, or 0 where the pool
// cannot meet the cell
int cycle_image_taken(CycleImage *image, const void *from, unsigned long long count);

// the cell of `image` given back, and the image empty
void cycle_image_ended(CycleImage *image);

// `count` bytes of `from` that the process keeps, in a movable cell of their own with a byte 0 after them. The
// handle, or NULL where the pool cannot meet the cell
const void *cycle_kept(const void *from, unsigned long long count);

int cycle_environment_set(const char *name);

int cycle_compiler_ready(void);

int cycle_linker_ready(void);

// the target's text onto `text`, which the caller began empty
void cycle_target_source(int major, int minor, int lto, PlatformText *text);

// the folder of the cache into `folder`, `bytes` of it: 1, or 0 where there is none or it does not fit
int cycle_cache_folder(char *folder, size_t bytes);

unsigned long long cycle_source_hash(const char *source, unsigned long long length);

// the file of a source of `length` bytes in the cache, into `path`, `bytes` of it
void cycle_cache_path(const char *folder, const char *source, unsigned long long length, char *path, size_t bytes);

// the image a cache file holds for this very source into `image`: 1, or 0, and `image` empty, where there is none
int cycle_cache_read(const char *path, const char *source, unsigned long long length, CycleImage *image);

void cycle_cache_write(const char *folder, const char *path, const char *source, unsigned long long length,
                       const void *image, unsigned long long count);

// the source compiled into `image`: 1, or 0, and `image` empty, where it errors
int cycle_program_compile(const char *source, const char *name, int major, int minor, int lto, int report,
                          CycleImage *image);

// the `count` bytes of `object` linked into `cubin`: 1, or 0, and `cubin` empty, where it errors
int cycle_program_link(const CycleTarget *lane_target, const void *object, unsigned long long count, int lto, int ptx,
                       int report, CycleImage *cubin);

const CycleTarget *cycle_target(int major, int minor, int lto, int report);

// the bytes of PTX's header, its terminator among them
#define CYCLE_PTX_HEADER_BYTES 512u

// the header of PTX as this toolkit writes it for the device. The program asks NVRTC for it once in a process, with a
// compile of an empty kernel to PTX. It caches the answer against the question. It is the one part of the rules of PTX
// that the lane does not carry itself
struct CyclePtxHeader
{
    int tried;
    int major;
    int minor;
    char lines[CYCLE_PTX_HEADER_BYTES];
};

// the header's three lines, empty where the call cannot query NVRTC or NVRTC does not answer [BORROWS]
const char *cycle_ptx_header(int major, int minor, int report);

#endif
