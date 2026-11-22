// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// cycle_compile_toolchain.cu: the environment, the compiler and linker, and the cache's names
#include "cycle_compile_internal.h"
#include "../../../../platform/platform.h"

CycleCompiler g_cycle_compiler;

CycleLinker g_cycle_linker;

int cycle_environment_set(const char *name)
{
    const char *const value = getenv(name);
    return ((value != NULL) && (value[0] == '1')) ? 1 : 0;
}

// a count of microseconds the environment names, or `otherwise` where it names none or one that is not a whole number
unsigned long long cycle_environment_microseconds(const char *name, unsigned long long otherwise)
{
    const char *const value = getenv(name);
    if ((value == NULL) || (value[0] < '0') || (value[0] > '9'))
    {
        return otherwise;
    }
    char *end = NULL;
    const unsigned long long named = strtoull(value, &end, 10);
    return ((end != NULL) && (*end == '\0')) ? named : otherwise;
}

static void *cycle_compiler_symbol(void *library, const char *name)
{
#if defined(_WIN32)
    // a symbol's address is the function it names, stored as a plain pointer until it is cast back to that function
    return (void *)GetProcAddress((HMODULE)library, name);
#else
    return dlsym(library, name);
#endif
}

int cycle_compiler_ready(void)
{
    CycleCompiler *const compiler = &g_cycle_compiler;
    if (compiler->tried != 0)
    {
        return compiler->ready;
    }
    compiler->tried = 1;
    char name[64];
#if defined(_WIN32)
    {
        VerbaLinea buffer_line = mmgr_line_over(name, sizeof(name));
        mmgr_line_put(&buffer_line, "nvrtc64_");
        mmgr_line_decimal_signed(&buffer_line, CUDART_VERSION / 1000);
        mmgr_line_put(&buffer_line, "0_0.dll");
        (void)mmgr_line_finish(&buffer_line);
    }
    // a module handle is stored as a plain pointer beside the Linux one
    void *const library = (void *)LoadLibraryA(name);
#else
    {
        VerbaLinea buffer_line = mmgr_line_over(name, sizeof(name));
        mmgr_line_put(&buffer_line, "libnvrtc.so.");
        mmgr_line_decimal_signed(&buffer_line, CUDART_VERSION / 1000);
        (void)mmgr_line_finish(&buffer_line);
    }
    void *library = dlopen(name, RTLD_NOW);
    library = (library != NULL) ? library : dlopen("libnvrtc.so", RTLD_NOW);
#endif
    if (library == NULL)
    {
        return 0;
    }
    // each symbol's address is cast back to the function NVRTC's header gives it
    const decltype(&nvrtcVersion) version = (decltype(&nvrtcVersion))cycle_compiler_symbol(library, "nvrtcVersion");
    compiler->create = (decltype(&nvrtcCreateProgram))cycle_compiler_symbol(library, "nvrtcCreateProgram");
    compiler->compile = (decltype(&nvrtcCompileProgram))cycle_compiler_symbol(library, "nvrtcCompileProgram");
    compiler->cubin_size = (decltype(&nvrtcGetCUBINSize))cycle_compiler_symbol(library, "nvrtcGetCUBINSize");
    compiler->cubin = (decltype(&nvrtcGetCUBIN))cycle_compiler_symbol(library, "nvrtcGetCUBIN");
    compiler->ltoir_size = (decltype(&nvrtcGetLTOIRSize))cycle_compiler_symbol(library, "nvrtcGetLTOIRSize");
    compiler->ltoir = (decltype(&nvrtcGetLTOIR))cycle_compiler_symbol(library, "nvrtcGetLTOIR");
    compiler->ptx_size = (decltype(&nvrtcGetPTXSize))cycle_compiler_symbol(library, "nvrtcGetPTXSize");
    compiler->ptx = (decltype(&nvrtcGetPTX))cycle_compiler_symbol(library, "nvrtcGetPTX");
    compiler->log_size = (decltype(&nvrtcGetProgramLogSize))cycle_compiler_symbol(library, "nvrtcGetProgramLogSize");
    compiler->log = (decltype(&nvrtcGetProgramLog))cycle_compiler_symbol(library, "nvrtcGetProgramLog");
    compiler->destroy = (decltype(&nvrtcDestroyProgram))cycle_compiler_symbol(library, "nvrtcDestroyProgram");
    compiler->ready = (version != NULL) && (compiler->create != NULL) && (compiler->compile != NULL) &&
                      (compiler->cubin_size != NULL) && (compiler->cubin != NULL) && (compiler->ltoir_size != NULL) &&
                      (compiler->ltoir != NULL) && (compiler->log_size != NULL) && (compiler->log != NULL) &&
                      (compiler->destroy != NULL) && (version(&compiler->major, &compiler->minor) == NVRTC_SUCCESS);
    return compiler->ready;
}

// one nvJitLink call by its versioned name, __nvJitLink<call>_<major>_<minor> of the toolkit built against
static void *cycle_linker_symbol(void *library, const char *call)
{
    char name[64];
    {
        VerbaLinea buffer_line = mmgr_line_over(name, sizeof(name));
        mmgr_line_put(&buffer_line, "__nvJitLink");
        mmgr_line_put(&buffer_line, call);
        mmgr_line_put(&buffer_line, "_");
        mmgr_line_decimal_signed(&buffer_line, CUDART_VERSION / 1000);
        mmgr_line_put(&buffer_line, "_");
        mmgr_line_decimal_signed(&buffer_line, (CUDART_VERSION % 1000) / 10);
        (void)mmgr_line_finish(&buffer_line);
    }
    return cycle_compiler_symbol(library, name);
}

int cycle_linker_ready(void)
{
    CycleLinker *const linker = &g_cycle_linker;
    if (linker->tried != 0)
    {
        return linker->ready;
    }
    linker->tried = 1;
    char name[64];
#if defined(_WIN32)
    {
        VerbaLinea buffer_line = mmgr_line_over(name, sizeof(name));
        mmgr_line_put(&buffer_line, "nvJitLink_");
        mmgr_line_decimal_signed(&buffer_line, CUDART_VERSION / 1000);
        mmgr_line_put(&buffer_line, "0_0.dll");
        (void)mmgr_line_finish(&buffer_line);
    }
    // a module handle is stored as a plain pointer beside the Linux one
    void *const library = (void *)LoadLibraryA(name);
#else
    {
        VerbaLinea buffer_line = mmgr_line_over(name, sizeof(name));
        mmgr_line_put(&buffer_line, "libnvJitLink.so.");
        mmgr_line_decimal_signed(&buffer_line, CUDART_VERSION / 1000);
        (void)mmgr_line_finish(&buffer_line);
    }
    void *library = dlopen(name, RTLD_NOW);
    library = (library != NULL) ? library : dlopen("libnvJitLink.so", RTLD_NOW);
#endif
    if (library == NULL)
    {
        return 0;
    }
    // each symbol's address is cast back to the function nvJitLink's header gives it
    linker->create = (decltype(&nvJitLinkCreate))cycle_linker_symbol(library, "Create");
    linker->add = (decltype(&nvJitLinkAddData))cycle_linker_symbol(library, "AddData");
    linker->complete = (decltype(&nvJitLinkComplete))cycle_linker_symbol(library, "Complete");
    linker->cubin_size = (decltype(&nvJitLinkGetLinkedCubinSize))cycle_linker_symbol(library, "GetLinkedCubinSize");
    linker->cubin = (decltype(&nvJitLinkGetLinkedCubin))cycle_linker_symbol(library, "GetLinkedCubin");
    linker->log_size = (decltype(&nvJitLinkGetErrorLogSize))cycle_linker_symbol(library, "GetErrorLogSize");
    linker->log = (decltype(&nvJitLinkGetErrorLog))cycle_linker_symbol(library, "GetErrorLog");
    linker->destroy = (decltype(&nvJitLinkDestroy))cycle_linker_symbol(library, "Destroy");
    linker->ready = (linker->create != NULL) && (linker->add != NULL) && (linker->complete != NULL) &&
                    (linker->cubin_size != NULL) && (linker->cubin != NULL) && (linker->log_size != NULL) &&
                    (linker->log != NULL) && (linker->destroy != NULL);
    return linker->ready;
}

// the target's text for the device and the type of link, which names both, then the prelude
std::string cycle_target_source(int major, int minor, int lto)
{
    std::string text;
    {
        TextNote text_note;
        text_note_begun(&text_note, &text);
        mmgr_line_put(&text_note.line, "// a record program's lane, for sm_");
        mmgr_line_decimal_signed(&text_note.line, major);
        mmgr_line_decimal_signed(&text_note.line, minor);
        mmgr_line_put(&text_note.line, ", NVRTC ");
        mmgr_line_decimal_signed(&text_note.line, g_cycle_compiler.major);
        mmgr_line_put(&text_note.line, ".");
        mmgr_line_decimal_signed(&text_note.line, g_cycle_compiler.minor);
        mmgr_line_put(&text_note.line, ", as ");
        mmgr_line_put(&text_note.line, (lto != 0) ? "LTO-IR" : "relocatable cubin");
        mmgr_line_ch(&text_note.line, '\n');
        text_note_written(&text_note);
    }
    text += g_cycle_prelude;
    return text;
}

// the folder compiled programs are cached in across processes: $CYCLE_CACHE, else the user's cache, then cycle
std::string cycle_cache_folder(void)
{
    const char *const named = getenv("CYCLE_CACHE");
    if ((named != NULL) && (named[0] != '\0'))
    {
        return std::string(named);
    }
#if defined(_WIN32)
    const char *const local = getenv("LOCALAPPDATA");
    return (local != NULL) ? (std::string(local) + "\\cycle") : std::string();
#else
    const char *const cache = getenv("XDG_CACHE_HOME");
    const char *const home = getenv("HOME");
    if ((cache != NULL) && (cache[0] != '\0'))
    {
        return std::string(cache) + "/cycle";
    }
    if (home == NULL)
    {
        return std::string();
    }
    mkdir((std::string(home) + "/.cache").c_str(), 0755);
    return std::string(home) + "/.cache/cycle";
#endif
}

// a source's FNV-1a
unsigned long long cycle_source_hash(const std::string &source)
{
    unsigned long long hash = 0xCBF29CE484222325ULL;
    for (size_t at = 0u; at < source.size(); at += 1u)
    {
        // a source byte is read as its unsigned value
        hash = (hash ^ (unsigned long long)(unsigned char)source[at]) * 0x100000001B3ULL;
    }
    return hash;
}

// a source's file in the cache, the image built from it (a program's linked cubin, or an answer requested from NVRTC):
// its source's FNV-1a in hex. The name need not be unique: a file is used only where the source it contains is this
// source, byte for byte
std::string cycle_cache_path(const std::string &folder, const std::string &source)
{
    char name[32];
    {
        VerbaLinea buffer_line = mmgr_line_over(name, sizeof(name));
        mmgr_line_hex(&buffer_line, cycle_source_hash(source), 16u);
        mmgr_line_put(&buffer_line, ".image");
        (void)mmgr_line_finish(&buffer_line);
    }
#if defined(_WIN32)
    return folder + "\\" + name;
#else
    return folder + "/" + name;
#endif
}
