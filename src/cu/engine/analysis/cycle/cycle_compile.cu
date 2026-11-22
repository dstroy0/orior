// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cycle_compile.cu: a program's C source built by the host's compiler, and run resident on the host
//
// CYCLE_RECORD_HOST_C=1 builds each lane written as C source (code_generator("c.krs")) with the host's C++ compiler in
// place of NVRTC: the lane and its resident (program_unit) under a host prelude that reads the CUDA names they use
// with each host thread a thread block of one thread. The text is compiled to a shared library in the cache folder,
// cached there by its text, and loaded. Its resident, cycle_program, runs the lanes on the host's threads, as many as
// $CYCLE_HOST_THREADS names, else the host's hardware threads. The run copies the lane's
// inputs from the device and its records and errors back: the check (CYCLE_RECORD_CHECK=1) checks it against the
// interpreter as it checks the device's. On Windows the compiler is nvcc handing the source to the host compiler
// alone, with -ccbin $CYCLE_HOST_CCBIN where it is set; elsewhere it is $CXX, else c++
#include "../../../../platform/platform.h"
#include "../../engine_note.h"
#include "cellularum_laboro/cellularum_linea.h"
#include "cycle_compile_internal.h"
#include "cycle_record_internal.h"
#include "memoria_operor/memoria_linea.h"

#if !defined(_WIN32)
#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
extern char **environ;
#endif

// The host's reading of the CUDA names a C lane and its resident use: each host thread is a thread block of one
// thread, whose shared memory is its own (thread_local), and the grid is the host threads the run starts
// (cycle_host_grid). The resident's atomics are the host's own, sequentially consistent, and its fence a full one:
// the lanes are taken from the one counter and the last thread block out writes the program's block, as on the
// device. The resident and the grid's setter are the library's exports
static const char s_cycle_host_prelude[] = R"CYCLE(// a record program's lane, built by the host's compiler
#if defined(_WIN32)
#include <intrin.h>
#define __global__ __declspec(dllexport)
#else
#define __global__ __attribute__((visibility("default")))
#endif
#define __device__
#define __shared__ thread_local
#define __forceinline__ inline
#define __launch_bounds__(threads_)
#define __syncthreads()
#if defined(_WIN32)
#define __threadfence() _mm_mfence()
#else
#define __threadfence() __atomic_thread_fence(__ATOMIC_SEQ_CST)
#endif

struct CycleHostDimension
{
    unsigned int x;
};

static const CycleHostDimension threadIdx = {0u};
static const CycleHostDimension blockDim = {1u};
static CycleHostDimension gridDim = {1u};

extern "C" __global__ void cycle_host_grid(unsigned int blocks)
{
    gridDim.x = blocks;
}

// Windows' interlocked calls take signed words of the same width, and the bits are read back unchanged
static inline unsigned int atomicAdd(unsigned int *address, unsigned int value)
{
#if defined(_WIN32)
    return (unsigned int)_InterlockedExchangeAdd((volatile long *)address, (long)value);
#else
    return __atomic_fetch_add(address, value, __ATOMIC_SEQ_CST);
#endif
}

static inline unsigned long long atomicAdd(unsigned long long *address, unsigned long long value)
{
#if defined(_WIN32)
    return (unsigned long long)_InterlockedExchangeAdd64((volatile long long *)address, (long long)value);
#else
    return __atomic_fetch_add(address, value, __ATOMIC_SEQ_CST);
#endif
}

static inline unsigned long long atomicCAS(unsigned long long *address, unsigned long long compare,
                                           unsigned long long value)
{
#if defined(_WIN32)
    return (unsigned long long)_InterlockedCompareExchange64((volatile long long *)address, (long long)value,
                                                             (long long)compare);
#else
    __atomic_compare_exchange_n(address, &compare, value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    return compare;
#endif
}
)CYCLE";

#if defined(_WIN32)
#define CYCLE_HOST_LIBRARY ".dll"
#else
#define CYCLE_HOST_LIBRARY ".so"
#endif

// A program that the host's compiler built in this process. The process finds it again by its whole text, a cell of
// `length` bytes. `holders` counts the loaded records that reference it, and the last release unloads it
struct CycleHostProgram
{
    const void *source;
    unsigned long long length;
    void *library;
    CycleHostEntry entry;
    CycleHostGrid grid;
    unsigned long long holders;
};

// the host programs, a CycleHostProgram each; the array begins at its first program
static PlatformArray s_cycle_host_programs;

// the bytes a host text or a file of one holds at max
#define CYCLE_HOST_TEXT_BYTES (1ULL << 30u)

// a file's whole contents onto `contents`, which the caller began empty: 1, or 0 where the call cannot read it
static int cycle_host_file_read(const char *path, PlatformText *contents)
{
    PlatformFile *const file = platform_file_open(path, "rb");
    if (file == NULL)
    {
        return 0;
    }
    char block[65536];
    size_t read = platform_file_read_items(block, 1u, sizeof(block), file);
    while (read != 0u)
    {
        (void)mmgr_line_put_n(&contents->line, block, read);
        read = platform_file_read_items(block, 1u, sizeof(block), file);
    }
    platform_file_close(file);
    return mmgr_line_ok(&contents->line) != 0;
}

// 1 where the file at `path` holds exactly the `length` bytes of `text`
static int cycle_host_file_same(const char *path, const char *text, unsigned long long length)
{
    PlatformText contents;
    platform_text_begun(&contents, CYCLE_HOST_TEXT_BYTES, platform_place_g_program_owner, 0);
    const int same = cycle_host_file_read(path, &contents) && (platform_text_size(&contents) == length) &&
                     (mmgr_bytes_compare(platform_text_bytes(&contents), text, (size_t)length) == 0);
    platform_text_ended(&contents);
    return same;
}

static int cycle_host_file_write(const char *path, const char *contents, unsigned long long length)
{
    PlatformFile *const file = platform_file_open(path, "wb");
    if (file == NULL)
    {
        return 0;
    }
    const int written = platform_file_write_items(contents, 1u, (size_t)length, file) == length;
    return (platform_file_close(file) == 0) && written;
}

// the `parts` texts at `part` end to end into `path`, `bytes` of it
static void cycle_host_path(char *path, size_t bytes, const char *const *part, unsigned int parts)
{
    VerbaLinea line = mmgr_line_over(path, bytes);
    for (unsigned int at = 0u; at < parts; at += 1u)
    {
        mmgr_line_put(&line, part[at]);
    }
    (void)mmgr_line_finish(&line);
}

// the most words of the compiler's command
#define CYCLE_HOST_ARGUMENTS 16u

// the compiler run on `source` into `library`, its output written to `log`; 1 where it ran and succeeded
static int cycle_host_compiler_run(const char *source, const char *library, const char *log)
{
    const char *arguments[CYCLE_HOST_ARGUMENTS + 1u];
    unsigned int count = 0u;
#if defined(_WIN32)
    char bin[PLATFORM_ENVIRONMENT_BYTES];
    const int named = (platform_environment("CYCLE_HOST_CCBIN", bin, sizeof(bin)) != 0ULL) && (bin[0] != '\0');
    arguments[count] = "nvcc";
    count += 1u;
    if (named != 0)
    {
        arguments[count] = "-ccbin";
        arguments[count + 1u] = bin;
        count += 2u;
    }
    const char *const flags[] = {"--shared", "--cudart", "none", "-O1", "-x", "c++"};
#else
    char compiler[PLATFORM_ENVIRONMENT_BYTES];
    const int named = (platform_environment("CXX", compiler, sizeof(compiler)) != 0ULL) && (compiler[0] != '\0');
    arguments[count] = (named != 0) ? compiler : "c++";
    count += 1u;
    const char *const flags[] = {"-shared", "-fPIC", "-O1", "-x", "c++"};
#endif
    for (unsigned int flag = 0u; flag < (sizeof(flags) / sizeof(flags[0])); flag += 1u)
    {
        arguments[count] = flags[flag];
        count += 1u;
    }
    arguments[count] = source;
    arguments[count + 1u] = "-o";
    arguments[count + 2u] = library;
    count += 3u;
    arguments[count] = NULL;
#if defined(_WIN32)
    PlatformText command;
    platform_text_begun(&command, PLATFORM_ENVIRONMENT_PATH_BYTES, platform_place_g_program_owner, 1);
    for (unsigned int at = 0u; at < count; at += 1u)
    {
        mmgr_line_put(&command.line, (at == 0u) ? "\"" : " \"");
        mmgr_line_put(&command.line, arguments[at]);
        mmgr_line_ch(&command.line, '"');
    }
    // CreateProcessA can write into its line, and the text's own cell holds the line
    char *const line = (char *)platform_text_bytes(&command);
    SECURITY_ATTRIBUTES inherited;
    mmgr_bytes_set(&inherited, 0, sizeof(inherited));
    inherited.nLength = sizeof(inherited);
    inherited.bInheritHandle = TRUE;
    const HANDLE output =
        CreateFileA(log, GENERIC_WRITE, FILE_SHARE_READ, &inherited, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if ((output == INVALID_HANDLE_VALUE) || (mmgr_line_ok(&command.line) == 0))
    {
        if (output != INVALID_HANDLE_VALUE)
        {
            CloseHandle(output);
        }
        platform_text_ended(&command);
        return 0;
    }
    STARTUPINFOA startup;
    mmgr_bytes_set(&startup, 0, sizeof(startup));
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup.hStdOutput = output;
    startup.hStdError = output;
    PROCESS_INFORMATION process;
    mmgr_bytes_set(&process, 0, sizeof(process));
    DWORD status = 1u;
    const BOOL started = CreateProcessA(NULL, line, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &startup, &process);
    if (started)
    {
        WaitForSingleObject(process.hProcess, INFINITE);
        GetExitCodeProcess(process.hProcess, &status);
        CloseHandle(process.hThread);
        CloseHandle(process.hProcess);
    }
    CloseHandle(output);
    platform_text_ended(&command);
    return started && (status == 0u);
#else
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, 1, log, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    posix_spawn_file_actions_adddup2(&actions, 1, 2);
    pid_t child = 0;
    // posix_spawnp reads its words and never writes them, and takes them as the system's own pointers
    const int started = posix_spawnp(&child, arguments[0], &actions, NULL, (char *const *)arguments, environ) == 0;
    posix_spawn_file_actions_destroy(&actions);
    int status = 1;
    const int waited = started && (waitpid(child, &status, 0) == child);
    return waited && WIFEXITED(status) && (WEXITSTATUS(status) == 0);
#endif
}

static void *cycle_host_library_open(const char *path)
{
#if defined(_WIN32)
    // a module handle is stored as a plain pointer beside the Linux one
    return (void *)LoadLibraryA(path);
#else
    return dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
}

static void cycle_host_library_close(void *library)
{
#if defined(_WIN32)
    FreeLibrary((HMODULE)library);
#else
    dlclose(library);
#endif
}

// the host program of this process whose text is the `length` bytes at `text`, else NULL
static CycleHostProgram *cycle_host_program_found(const char *text, unsigned long long length)
{
    for (unsigned long long at = 0ULL; at < s_cycle_host_programs.count; at += 1ULL)
    {
        CycleHostProgram *const program = (CycleHostProgram *)platform_array_at(&s_cycle_host_programs, at);
        if ((program->length == length) && platform_movable_same(program->source, 0ULL, text, length))
        {
            return program;
        }
    }
    return NULL;
}

// The host text of a C lane found in this process, else in the cache folder, else compiled there and cached, then
// loaded, from `text`, which the caller wrote and gives back. The cache holds the text beside its library, and a
// library is used only where the text beside it is this text byte for byte. Each is written beside its name and
// renamed to it, and no reader finds half a file. 0 where there is no cache folder, or the compile or the load failed
static int cycle_host_program_built(const EngineRecordLayout *layout, CycleRecord *record, PlatformText *text,
                                    unsigned long long written, int report)
{
    const char *const bytes = platform_text_bytes(text);
    const unsigned long long length = platform_text_size(text);
    if (s_cycle_host_programs.element == 0ULL)
    {
        platform_array_begun(&s_cycle_host_programs, sizeof(CycleHostProgram), platform_place_g_program_owner, 0);
    }
    CycleHostProgram *const held = cycle_host_program_found(bytes, length);
    if (held != NULL)
    {
        held->holders += 1ULL;
        record->host_program = held->entry;
        record->host_grid = held->grid;
        record->compiled = 1u;
        if (report != 0)
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  cycle: a program of ");
            mmgr_line_decimal(&engine_note.line, layout->steps, 1u);
            mmgr_line_put(&engine_note.line, " steps found in this process, as a host library");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 1;
    }
    char folder[PLATFORM_PATH_BYTES];
    if (cycle_cache_folder(folder, sizeof(folder)) == 0)
    {
        return 0;
    }
#if defined(_WIN32)
    _mkdir(folder);
    const int process = _getpid();
    const char *const separator = "\\";
#else
    mkdir(folder, 0755);
    // a pid is positive. It converts exactly
    const int process = (int)getpid();
    const char *const separator = "/";
#endif
    char stem[PLATFORM_PATH_BYTES];
    {
        VerbaLinea buffer_line = mmgr_line_over(stem, sizeof(stem));
        mmgr_line_put(&buffer_line, folder);
        mmgr_line_put(&buffer_line, separator);
        mmgr_line_hex(&buffer_line, cycle_source_hash(bytes, length), 16u);
        mmgr_line_put(&buffer_line, ".host");
        (void)mmgr_line_finish(&buffer_line);
    }
    char named_source[PLATFORM_PATH_BYTES];
    char named_library[PLATFORM_PATH_BYTES];
    const char *const source_parts[2] = {stem, ".cpp"};
    const char *const library_parts[2] = {stem, CYCLE_HOST_LIBRARY};
    cycle_host_path(named_source, sizeof(named_source), source_parts, 2u);
    cycle_host_path(named_library, sizeof(named_library), library_parts, 2u);
    const int found = cycle_host_file_same(named_source, bytes, length);
    unsigned long long compile_nanoseconds = 0ULL;
    char log[PLATFORM_PATH_BYTES];
    log[0] = '\0';
    if (!found)
    {
        char partial[PLATFORM_PATH_BYTES];
        {
            VerbaLinea buffer_line = mmgr_line_over(partial, sizeof(partial));
            mmgr_line_put(&buffer_line, stem);
            mmgr_line_put(&buffer_line, ".");
            mmgr_line_decimal_signed(&buffer_line, process);
            (void)mmgr_line_finish(&buffer_line);
        }
        char partial_source[PLATFORM_PATH_BYTES];
        char partial_library[PLATFORM_PATH_BYTES];
        const char *const log_parts[2] = {partial, ".log"};
        const char *const partial_source_parts[2] = {partial, ".cpp"};
        const char *const partial_library_parts[2] = {partial, CYCLE_HOST_LIBRARY};
        cycle_host_path(log, sizeof(log), log_parts, 2u);
        cycle_host_path(partial_source, sizeof(partial_source), partial_source_parts, 2u);
        cycle_host_path(partial_library, sizeof(partial_library), partial_library_parts, 2u);
        const unsigned long long began = platform_clock_ns();
        const int built = cycle_host_file_write(partial_source, bytes, length) &&
                          cycle_host_compiler_run(partial_source, partial_library, log);
        // the monotonic clock never runs back
        compile_nanoseconds = platform_clock_ns() - began;
        // a name another process took first is left as it is, and its library is loaded
        if (built)
        {
            (void)platform_file_moved(partial_library, named_library);
            (void)platform_file_moved(partial_source, named_source);
        }
        const char *const left[4] = {CYCLE_HOST_LIBRARY, ".cpp", ".lib", ".exp"};
        for (unsigned int at = 0u; at < 4u; at += 1u)
        {
            char left_path[PLATFORM_PATH_BYTES];
            const char *const left_parts[2] = {partial, left[at]};
            cycle_host_path(left_path, sizeof(left_path), left_parts, 2u);
            (void)platform_file_removed(left_path);
        }
        if (built)
        {
            (void)platform_file_removed(log);
            log[0] = '\0';
        }
    }
    CycleHostProgram program;
    mmgr_bytes_set(&program, 0, sizeof(program));
    program.library = cycle_host_library_open(named_library);
    program.holders = 1ULL;
    if (program.library != NULL)
    {
#if defined(_WIN32)
        // the resident's and the grid setter's addresses are cast back to the functions the host prelude gives them
        program.entry = (CycleHostEntry)GetProcAddress((HMODULE)program.library, "cycle_program");
        program.grid = (CycleHostGrid)GetProcAddress((HMODULE)program.library, "cycle_host_grid");
#else
        program.entry = (CycleHostEntry)dlsym(program.library, "cycle_program");
        program.grid = (CycleHostGrid)dlsym(program.library, "cycle_host_grid");
#endif
    }
    // the source the process keeps to find the program by, in a movable cell
    program.source = cycle_kept(bytes, length);
    program.length = length;
    const int loaded = (program.entry != NULL) && (program.grid != NULL) && (program.source != NULL) &&
                       cycle_host_file_same(named_source, bytes, length) &&
                       platform_array_appended(&s_cycle_host_programs, &program);
    if (loaded == 0)
    {
        platform_movable_released(program.source, platform_place_g_program_owner);
        if (program.library != NULL)
        {
            cycle_host_library_close(program.library);
        }
        if (report != 0)
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  cycle: a program of ");
            mmgr_line_decimal(&engine_note.line, layout->steps, 1u);
            mmgr_line_put(&engine_note.line, " steps did not build on the host (");
            mmgr_line_put(&engine_note.line,
                          (log[0] == '\0') ? "its library did not load" : "the compiler errored, its log ");
            mmgr_line_put(&engine_note.line, log);
            mmgr_line_put(&engine_note.line, ")");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 0;
    }
    record->host_program = program.entry;
    record->host_grid = program.grid;
    record->compiled = 1u;
    if ((report != 0) && found)
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  cycle: a program of ");
        mmgr_line_decimal(&engine_note.line, layout->steps, 1u);
        mmgr_line_put(&engine_note.line, " steps read from the cache as a host library");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    else if (report != 0)
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "  cycle: a program of ");
        mmgr_line_decimal(&engine_note.line, layout->steps, 1u);
        mmgr_line_put(&engine_note.line, " steps as C source on the host: written in ");
        mmgr_line_decimal(&engine_note.line, written, 1u);
        mmgr_line_put(&engine_note.line, " ns to ");
        mmgr_line_decimal(&engine_note.line, length, 1u);
        mmgr_line_put(&engine_note.line, " bytes, compiled in ");
        mmgr_line_decimal(&engine_note.line, compile_nanoseconds, 1u);
        mmgr_line_put(&engine_note.line, " ns");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    return 1;
}

int cycle_host_program_load(const EngineRecordLayout *layout, CycleRecord *record, const char *source,
                            unsigned int places, unsigned long long written, int report)
{
    // the prelude, the lane, then one thread's places: a word each, then a sign byte each
    PlatformText text;
    platform_text_begun(&text, CYCLE_HOST_TEXT_BYTES, platform_place_g_program_owner, 0);
    mmgr_line_put(&text.line, s_cycle_host_prelude);
    mmgr_line_put(&text.line, source);
    mmgr_line_put(&text.line, "\nthread_local u32 cycle_words[");
    mmgr_line_decimal(&text.line, places + ((places + 3u) / 4u) + 1u, 1u);
    mmgr_line_put(&text.line, "];");
    mmgr_line_ch(&text.line, '\n');
    const int loaded =
        (mmgr_line_ok(&text.line) != 0) && cycle_host_program_built(layout, record, &text, written, report);
    platform_text_ended(&text);
    return loaded;
}

// a reference to a host program loaded in this process given back, where `entry` is one; the last reference released
// unloads it
void cycle_host_program_release(CycleHostEntry entry)
{
    for (unsigned long long at = 0ULL; (entry != NULL) && (at < s_cycle_host_programs.count); at += 1ULL)
    {
        CycleHostProgram *const program = (CycleHostProgram *)platform_array_at(&s_cycle_host_programs, at);
        if (program->entry == entry)
        {
            program->holders -= 1ULL;
            if (program->holders == 0ULL)
            {
                cycle_host_library_close(program->library);
                platform_movable_released(program->source, platform_place_g_program_owner);
                // the programs after it move down one place, and the order of the rest holds
                const unsigned long long after = s_cycle_host_programs.count - at - 1ULL;
                mmgr_bytes_move(program, program + 1, (size_t)(after * sizeof(CycleHostProgram)));
                (void)platform_array_counted(&s_cycle_host_programs, s_cycle_host_programs.count - 1ULL);
            }
            break;
        }
    }
}

// `count` device words copied into a cell of the host, which the caller gives back. `taken` is 1 where the copy went or
// there was nothing to copy, else 0
static unsigned int *cycle_host_words_read(const unsigned int *device, size_t count, EngineError *error, int *taken)
{
    unsigned int *const words = (unsigned int *)platform_cell_cleared((count != 0u) ? count : 1u, sizeof(unsigned int),
                                                                      platform_place_g_program_owner, 0);
    *taken = (words != NULL) &&
             ((device == NULL) || (count == 0u) ||
              CYCLE_STATUS_CHECK(cudaMemcpy(words, device, count * sizeof(unsigned int), cudaMemcpyDeviceToHost),
                                 device, error));
    return words;
}

// the host threads a run starts: $CYCLE_HOST_THREADS where it names a whole number above 0, else the host's processors,
// and never more than the lanes
static unsigned int cycle_host_threads(unsigned long long lanes)
{
    char named_text[PLATFORM_ENVIRONMENT_BYTES];
    const char *const named =
        (platform_environment("CYCLE_HOST_THREADS", named_text, sizeof(named_text)) != 0ULL) ? named_text : NULL;
    const char *end = NULL;
    const unsigned long long requested =
        ((named != NULL) && (named[0] >= '0') && (named[0] <= '9')) ? mmgr_text_unsigned_in(named, &end, 10) : 0ULL;
    const unsigned long long hardware = platform_processors();
    const unsigned long long threads =
        ((requested != 0ULL) && (end != NULL) && (*end == '\0')) ? requested : ((hardware != 0ULL) ? hardware : 1ULL);
    const unsigned long long used = (threads < lanes) ? threads : lanes;
    // at max the lanes and at max what the environment or the hardware names; a run of more than 2^32 - 1 threads
    // is limited to that
    return (unsigned int)((used == 0ULL) ? 1ULL : ((used > 0xFFFFFFFFULL) ? 0xFFFFFFFFULL : used));
}

// one host thread of a run: the program's resident on its launch
struct CycleHostThread
{
    CycleHostEntry entry;
    const CycleCompiledLaunch *program;
};

static void cycle_host_thread_run(void *context)
{
    const CycleHostThread *const thread = (const CycleHostThread *)context;
    thread->entry(*thread->program);
}

// the cells of one host run, each given back as the run ends
struct CycleHostRun
{
    unsigned int *in[ENGINE_RECORD_MEMBERS_MAX];
    unsigned int *index;
    unsigned int *tables;
    unsigned int *out;
    unsigned int *error_count;
    PlatformWorker *workers;
};

static void cycle_host_run_ended(CycleHostRun *run)
{
    for (unsigned int member = 0u; member < ENGINE_RECORD_MEMBERS_MAX; member += 1u)
    {
        platform_cell_released(run->in[member], platform_place_g_program_owner);
    }
    platform_cell_released(run->index, platform_place_g_program_owner);
    platform_cell_released(run->tables, platform_place_g_program_owner);
    platform_cell_released(run->out, platform_place_g_program_owner);
    platform_cell_released(run->error_count, platform_place_g_program_owner);
    platform_cell_released(run->workers, platform_place_g_program_owner);
}

// The host program run resident, as the device's is (cycle_record_resident): its block arranged for this run, then
// the program started on the host's threads, each a thread block of one thread (cycle_host_threads), again from
// where its block records it is until every lane is done. The threads take lanes from the one counter, and the last
// one out writes the host's copy of the block. The lanes' members, index and tables are read from the device first,
// and the records and the error count are written back to it last. The host has no clock the program reads: its
// launch time is taken around each start
int cycle_host_resident(const CycleRecord *record, CycleCompiledLaunch program, EngineError *error)
{
    CycleHostRun run;
    mmgr_bytes_set(&run, 0, sizeof(run));
    unsigned int *const device_out = program.out;
    unsigned int *const device_error = program.error;
    // a lane count and a record's limbs are the ones the device's buffers were allocated to, which fit in memory
    const size_t out_words = (size_t)(program.count * record->out_limbs);
    int index_taken = 0;
    int tables_taken = 0;
    int error_taken = 0;
    run.index = cycle_host_words_read(program.index, (size_t)(program.count * record->members), error, &index_taken);
    run.tables = cycle_host_words_read(program.tables, (size_t)record->table_words, error, &tables_taken);
    run.error_count = cycle_host_words_read(program.error, 1u, error, &error_taken);
    int ok = index_taken && tables_taken && error_taken;
    for (unsigned int member = 0u; (ok != 0) && (member < record->members); member += 1u)
    {
        int member_taken = 0;
        run.in[member] = cycle_host_words_read(
            program.in[member], (size_t)(program.bodies[member] * record->in_limbs[member]), error, &member_taken);
        ok = member_taken;
        program.in[member] = run.in[member];
    }
    run.out = (unsigned int *)platform_cell_cleared((out_words != 0u) ? out_words : 1u, sizeof(unsigned int),
                                                    platform_place_g_program_owner, 0);
    const unsigned int threads = cycle_host_threads(program.count);
    run.workers =
        (PlatformWorker *)platform_cell_cleared(threads, sizeof(PlatformWorker), platform_place_g_program_owner, 0);
    ok = ok && CYCLE_CHECK((run.out != NULL) && (run.workers != NULL), &run, error, ENGINE_ERROR_RESOURCE);
    if (ok == 0)
    {
        cycle_host_run_ended(&run);
        return 0;
    }
    CycleHot hot;
    mmgr_bytes_set(&hot, 0, sizeof(hot));
    program.index = (program.index != NULL) ? run.index : NULL;
    program.tables = (program.tables != NULL) ? run.tables : NULL;
    program.out = run.out;
    program.error = run.error_count;
    EngineProgramBlock *const block = record->block;
    const unsigned long long ttl =
        1000ULL * cycle_environment_microseconds("CYCLE_RECORD_TTL", CYCLE_PROGRAM_TTL_MICROSECONDS);
    const unsigned long long wdt = 1000ULL * CYCLE_PROGRAM_WDT_MICROSECONDS;
    const EngineSignum signature = block->signature;
    const unsigned long long generation = block->generation + 1ULL;
    mmgr_bytes_set(block, 0, sizeof(EngineProgramBlock));
    block->signature = signature;
    block->generation = generation;
    block->command = ENGINE_PROGRAM_RUN;
    block->grant_threads = threads;
    block->state = ENGINE_PROGRAM_PLACED;
    block->span = record->file_limbs;
    block->ttl = ttl;
    block->wdt = wdt;
    block->lanes = program.count;
    // the records' address is stored as a 64-bit word, as every word of the block is
    block->result = (unsigned long long)(uintptr_t)run.out;
    block->result_words = out_words;
    block->checksum = cycle_block_seal(block);
    program.hot = &hot;
    // the block is 64-bit words throughout, and the program reads it as them
    program.block = (unsigned long long *)block;
    program.ttl = ttl;
    program.checkin_every = wdt / CYCLE_PROGRAM_CHECKINS_PER_WDT;
    program.places = record->places;
    const CycleHostThread thread = {record->host_program, &program};
    int running = ok;
    while (running != 0)
    {
        ok = CYCLE_CHECK(block->checksum == cycle_block_seal(block), block, error, ENGINE_ERROR_LOGIC);
        if (ok == 0)
        {
            break;
        }
        block->launches += 1ULL;
        block->owner = block->launches;
        block->state = ENGINE_PROGRAM_RUNNING;
        program.launch_number = block->launches;
        hot.launch_start = 0ULL;
        hot.finished = 0ULL;
        record->host_grid(threads);
        const unsigned long long began = platform_clock_ns();
        unsigned int refused = 0u;
        for (unsigned int at = 0u; at < threads; at += 1u)
        {
            // a thread the system refuses is a thread block that never checks in, and the launch faults
            refused +=
                (platform_worker_started(&run.workers[at], cycle_host_thread_run, (void *)&thread) == 0) ? 1u : 0u;
        }
        for (unsigned int at = 0u; at < threads; at += 1u)
        {
            platform_worker_joined(&run.workers[at]);
        }
        const unsigned long long ended = platform_clock_ns();
        const unsigned long long state = block->state;
        ok = CYCLE_CHECK((refused == 0u) && ((state == ENGINE_PROGRAM_YIELDED) || (state == ENGINE_PROGRAM_DONE)),
                         block, error, ENGINE_ERROR_LOGIC);
        // the monotonic clock never runs back
        block->exectime = ended - began;
        block->error = run.error_count[0];
        block->runtime += block->exectime;
        if (ok == 0)
        {
            block->state = ENGINE_PROGRAM_FAULT;
            // an engine module and a site are small non-negative counts, which fit in a 64-bit word
            block->error_module = (unsigned long long)error->module;
            block->error_site = (unsigned long long)error->site;
        }
        block->checksum = cycle_block_seal(block);
        running = (ok != 0) && (state == ENGINE_PROGRAM_YIELDED);
    }
    ok = ok &&
         CYCLE_STATUS_CHECK(cudaMemcpy(device_error, run.error_count, sizeof(unsigned int), cudaMemcpyHostToDevice),
                            device_error, error) &&
         ((out_words == 0u) ||
          CYCLE_STATUS_CHECK(cudaMemcpy(device_out, run.out, out_words * sizeof(unsigned int), cudaMemcpyHostToDevice),
                             device_out, error));
    cycle_host_run_ended(&run);
    return ok;
}
