// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#if !defined(_WIN32)
// syscall and pid_t are outside strict C11
#define _GNU_SOURCE
#endif
#include "tessera_measure.h"
#include "../../../../platform/platform.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define NOMINMAX
#include <pdh.h>
#include <pdhmsg.h>
#include <windows.h>
#else
#include <dlfcn.h>
#include <poll.h>
#include <signal.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

#define TESSERA_NVML_SUCCESS 0
#define TESSERA_NVML_INSUFFICIENT_SIZE 7
#define TESSERA_NVML_NOT_AVAILABLE 0xFFFFFFFFFFFFFFFFULL
#define TESSERA_UUID_TEXT 41u

typedef struct
{
    unsigned long long total;
    unsigned long long free;
    unsigned long long used;
} TesseraNvmlMemory;

typedef struct
{
    unsigned int pid;
    unsigned long long used;
    unsigned int gpu_instance;
    unsigned int compute_instance;
} TesseraNvmlProcess;

typedef int (*TesseraNvmlInit)(void);
typedef int (*TesseraNvmlShutdown)(void);
typedef int (*TesseraNvmlHandle)(const char *uuid, void **device);
typedef int (*TesseraNvmlMemoryInfo)(void *device, TesseraNvmlMemory *memory);
typedef int (*TesseraNvmlProcesses)(void *device, unsigned int *count, TesseraNvmlProcess *processes);

struct TesseraMeasure
{
    void *library;
    void *device;
    unsigned long long luid;
    TesseraNvmlShutdown shutdown;
    TesseraNvmlMemoryInfo memory_info;
    TesseraNvmlProcesses processes;
    int reported;
    int host;
    unsigned long long host_capacity;
#if defined(_WIN32)
    PDH_HQUERY query;
    PDH_HCOUNTER counter;
#endif
};

struct TesseraProcess
{
    unsigned long long pid;
#if defined(_WIN32)
    HANDLE handle;
#else
    int descriptor;
    unsigned long long started;
#endif
};

static void *tessera_symbol(void *library, const char *name)
{
#if defined(_WIN32)
    // a procedure address is converted to a data pointer only to be converted back to its own type
    return (void *)GetProcAddress((HMODULE)library, name);
#else
    return dlsym(library, name);
#endif
}

static void tessera_uuid_text(const unsigned char device[TESSERA_DEVICE_BYTES], char text[TESSERA_UUID_TEXT])
{
    static const char digits[17] = "0123456789abcdef";
    static const unsigned int dashes[4] = {4u, 6u, 8u, 10u};
    unsigned int at = 0u;
    mmgr_bytes_copy(text, "GPU-", 4u);
    at = 4u;
    unsigned int dash = 0u;
    for (unsigned int byte = 0u; byte < TESSERA_DEVICE_BYTES; byte += 1u)
    {
        if ((dash < 4u) && (byte == dashes[dash]))
        {
            text[at] = '-';
            at += 1u;
            dash += 1u;
        }
        text[at] = digits[device[byte] >> 4u];
        text[at + 1u] = digits[device[byte] & 0x0Fu];
        at += 2u;
    }
    text[at] = '\0';
}

int tessera_measure_reported(const TesseraMeasure *measure)
{
    return measure->reported;
}

int tessera_measure_host(const TesseraMeasure *measure)
{
    return measure->host;
}

TesseraMeasure *tessera_measure_open(const unsigned char device[TESSERA_DEVICE_BYTES], unsigned long long luid)
{
    TesseraMeasure *const measure =
        (TesseraMeasure *)platform_cell_cleared(1u, sizeof(TesseraMeasure), platform_place_g_program_owner, 0);
    if (measure == NULL)
    {
        return NULL;
    }
    measure->luid = luid;
    // the host's processors: no library reads them, and each job's process reports its own use
    if (tessera_device_names_host(device))
    {
        const unsigned long long mask = tessera_self_host_mask();
        measure->host = 1;
        measure->reported = 1;
        // a processor count is at max sixty-four. It converts exactly
        measure->host_capacity = (unsigned long long)engine_word_population(mask) * TESSERA_HOST_PROCESSOR;
        if (mask == 0ULL)
        {
            tessera_measure_close(measure);
            return NULL;
        }
        return measure;
    }
    // under WSL NVML still reads the device whole, but each job's process reports its own bytes
    measure->reported = tessera_self_paravirtual();
#if defined(_WIN32)
    // the module handle is stored as an opaque pointer and converted back where it is used
    measure->library = (void *)LoadLibraryA("nvml.dll");
#else
    measure->library = dlopen("libnvidia-ml.so.1", RTLD_NOW);
#endif
    // each symbol is converted back to the type it was exported with
    const TesseraNvmlInit init = (TesseraNvmlInit)tessera_symbol(measure->library, "nvmlInit_v2");
    // each symbol is converted back to the type it was exported with
    const TesseraNvmlHandle handle = (TesseraNvmlHandle)tessera_symbol(measure->library, "nvmlDeviceGetHandleByUUID");
    // each symbol is converted back to the type it was exported with
    measure->shutdown = (TesseraNvmlShutdown)tessera_symbol(measure->library, "nvmlShutdown");
    // each symbol is converted back to the type it was exported with
    measure->memory_info = (TesseraNvmlMemoryInfo)tessera_symbol(measure->library, "nvmlDeviceGetMemoryInfo");
    // each symbol is converted back to the type it was exported with
    measure->processes =
        (TesseraNvmlProcesses)tessera_symbol(measure->library, "nvmlDeviceGetComputeRunningProcesses_v3");
    char uuid[TESSERA_UUID_TEXT];
    tessera_uuid_text(device, uuid);
    int ok = (measure->library != NULL) && (init != NULL) && (handle != NULL) && (measure->shutdown != NULL) &&
             (measure->memory_info != NULL) && (measure->processes != NULL) && (init() == TESSERA_NVML_SUCCESS) &&
             (handle(uuid, &measure->device) == TESSERA_NVML_SUCCESS);
#if defined(_WIN32)
    ok = ok && (PdhOpenQueryA(NULL, 0u, &measure->query) == ERROR_SUCCESS) &&
         (PdhAddEnglishCounterA(measure->query, "\\GPU Process Memory(*)\\Dedicated Usage", 0u, &measure->counter) ==
          ERROR_SUCCESS);
#endif
    if (!ok)
    {
        tessera_measure_close(measure);
        return NULL;
    }
    return measure;
}

void tessera_measure_close(TesseraMeasure *measure)
{
    if (measure == NULL)
    {
        return;
    }
#if defined(_WIN32)
    if (measure->query != NULL)
    {
        PdhCloseQuery(measure->query);
    }
#endif
    if ((measure->device != NULL) && (measure->shutdown != NULL))
    {
        measure->shutdown();
    }
    if (measure->library != NULL)
    {
#if defined(_WIN32)
        // the stored module handle is converted back to the type it was loaded as
        FreeLibrary((HMODULE)measure->library);
#else
        dlclose(measure->library);
#endif
    }
    platform_cell_released(measure, platform_place_g_program_owner);
}

int tessera_measure_device(TesseraMeasure *measure, unsigned long long *capacity, unsigned long long *in_use)
{
    if (measure->host)
    {
        *capacity = measure->host_capacity;
        *in_use = 0ULL;
        return 1;
    }
    TesseraNvmlMemory memory;
    mmgr_bytes_set(&memory, 0, sizeof(memory));
    if (measure->memory_info(measure->device, &memory) != TESSERA_NVML_SUCCESS)
    {
        return 0;
    }
    *capacity = memory.total;
    *in_use = memory.used;
    return 1;
}

#if defined(_WIN32)
static int tessera_instance_field(const char *name, const char *label, unsigned long long *value)
{
    const char *const found = mmgr_text_find(name, label, PLATFORM_TEXT_TO_TERMINATOR);
    if (found == NULL)
    {
        return 0;
    }
    const char *const start = found + mmgr_text_length(label, PLATFORM_TEXT_TO_TERMINATOR);
    const int hexadecimal = (start[0] == '0') && ((start[1] == 'x') || (start[1] == 'X'));
    char *end = NULL;
    *value = strtoull(hexadecimal ? (start + 2) : start, &end, hexadecimal ? 16 : 10);
    return end != (hexadecimal ? (start + 2) : start);
}

static int tessera_instance_luid(const char *name, unsigned long long *luid)
{
    const char *const found = mmgr_text_find(name, "luid_", PLATFORM_TEXT_TO_TERMINATOR);
    unsigned long long high = 0ULL;
    unsigned long long low = 0ULL;
    if ((found == NULL) || !tessera_instance_field(found, "luid_", &high))
    {
        return 0;
    }
    const char *const second = mmgr_text_find(found + 5, "_0x", PLATFORM_TEXT_TO_TERMINATOR);
    if ((second == NULL) || !tessera_instance_field(second, "_", &low))
    {
        return 0;
    }
    *luid = (high << 32u) | (low & 0xFFFFFFFFULL);
    return 1;
}
#endif

int tessera_measure_process(TesseraMeasure *measure, unsigned long long pid, unsigned long long *used)
{
    *used = 0ULL;
    // a reported measure reads no pid from outside: that process's own report is for it
    if (measure->reported)
    {
        return 0;
    }
#if defined(_WIN32)
    if (PdhCollectQueryData(measure->query) != ERROR_SUCCESS)
    {
        return 0;
    }
    DWORD bytes = 0u;
    DWORD count = 0u;
    PDH_STATUS status = PdhGetFormattedCounterArrayA(measure->counter, PDH_FMT_LARGE, &bytes, &count, NULL);
    if (status != (PDH_STATUS)PDH_MORE_DATA)
    {
        return status == ERROR_SUCCESS;
    }
    PDH_FMT_COUNTERVALUE_ITEM_A *const items = (PDH_FMT_COUNTERVALUE_ITEM_A *)platform_cell_taken(
        (unsigned long long)(bytes), (unsigned int)platform_place_g_engine_owner, 0);
    if (items == NULL)
    {
        return 0;
    }
    status = PdhGetFormattedCounterArrayA(measure->counter, PDH_FMT_LARGE, &bytes, &count, items);
    int ok = status == ERROR_SUCCESS;
    for (DWORD at = 0u; ok && (at < count); at += 1u)
    {
        unsigned long long owner = 0ULL;
        unsigned long long luid = 0ULL;
        const int named =
            tessera_instance_field(items[at].szName, "pid_", &owner) && tessera_instance_luid(items[at].szName, &luid);
        if (named && (owner == pid) && (luid == measure->luid) && (items[at].FmtValue.largeValue > 0))
        {
            // a positive byte count read as a signed large value is exact unsigned
            *used += (unsigned long long)items[at].FmtValue.largeValue;
        }
    }
    platform_cell_released(items, (unsigned int)platform_place_g_engine_owner);
    return ok;
#else
    unsigned int count = 0u;
    int status = measure->processes(measure->device, &count, NULL);
    if (status == TESSERA_NVML_SUCCESS)
    {
        return 1;
    }
    if (status != TESSERA_NVML_INSUFFICIENT_SIZE)
    {
        return 0;
    }
    TesseraNvmlProcess *const list = (TesseraNvmlProcess *)platform_cell_cleared(
        (unsigned long long)((size_t)count + 1u), (unsigned long long)(sizeof(TesseraNvmlProcess)),
        (unsigned int)platform_place_g_engine_owner, 0);
    if (list == NULL)
    {
        return 0;
    }
    count += 1u;
    status = measure->processes(measure->device, &count, list);
    int ok = status == TESSERA_NVML_SUCCESS;
    for (unsigned int at = 0u; ok && (at < count); at += 1u)
    {
        if (list[at].pid == pid)
        {
            ok = list[at].used != TESSERA_NVML_NOT_AVAILABLE;
            *used += ok ? list[at].used : 0ULL;
        }
    }
    platform_cell_released(list, (unsigned int)platform_place_g_engine_owner);
    return ok;
#endif
}

#if !defined(_WIN32)
static unsigned long long tessera_started(unsigned long long pid)
{
    char path[ENGINE_PATH_CAPACITY];
    {
        VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
        mmgr_line_put(&buffer_line, "/proc/");
        mmgr_line_decimal(&buffer_line, pid, 1u);
        mmgr_line_put(&buffer_line, "/stat");
        (void)mmgr_line_finish(&buffer_line);
    }
    PlatformFile *const stat = platform_file_open(path, "r");
    if (stat == NULL)
    {
        return 0ULL;
    }
    char line[ENGINE_PATH_CAPACITY];
    const int read = platform_file_text_read(line, sizeof(line), stat) != NULL;
    platform_file_close(stat);
    const char *const after_name = read ? strrchr(line, ')') : NULL;
    if (after_name == NULL)
    {
        return 0ULL;
    }
    const char *field = after_name + 2;
    for (unsigned int skipped = 0u; (field != NULL) && (skipped < 19u); skipped += 1u)
    {
        field = mmgr_text_char(field, ' ', PLATFORM_TEXT_TO_TERMINATOR);
        field = (field != NULL) ? (field + 1) : NULL;
    }
    return (field != NULL) ? strtoull(field, NULL, 10) : 0ULL;
}
#endif

TesseraProcess *tessera_process_open(unsigned long long pid)
{
    TesseraProcess *const process =
        (TesseraProcess *)platform_cell_cleared(1u, sizeof(TesseraProcess), platform_place_g_program_owner, 0);
    if (process == NULL)
    {
        return NULL;
    }
    process->pid = pid;
#if defined(_WIN32)
    // a process id fits the 32 bits Windows gives it
    process->handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, (DWORD)pid);
    if (process->handle == NULL)
    {
        platform_cell_released(process, platform_place_g_program_owner);
        return NULL;
    }
#else
#if defined(SYS_pidfd_open)
    // a process id fits the pid_t the kernel gives it
    process->descriptor = (int)syscall(SYS_pidfd_open, (pid_t)pid, 0u);
#else
    process->descriptor = -1;
#endif
    process->started = tessera_started(pid);
    if ((process->descriptor < 0) && (process->started == 0ULL))
    {
        platform_cell_released(process, platform_place_g_program_owner);
        return NULL;
    }
#endif
    return process;
}

int tessera_process_lives(const TesseraProcess *process)
{
#if defined(_WIN32)
    return WaitForSingleObject(process->handle, 0u) == WAIT_TIMEOUT;
#else
    if (process->descriptor >= 0)
    {
        struct pollfd watch;
        watch.fd = process->descriptor;
        watch.events = POLLIN;
        watch.revents = 0;
        return poll(&watch, 1u, 0) == 0;
    }
    return tessera_started(process->pid) == process->started;
#endif
}

void tessera_process_release(TesseraProcess *process)
{
    if (process == NULL)
    {
        return;
    }
#if defined(_WIN32)
    CloseHandle(process->handle);
#else
    if (process->descriptor >= 0)
    {
        close(process->descriptor);
    }
#endif
    platform_cell_released(process, platform_place_g_program_owner);
}
