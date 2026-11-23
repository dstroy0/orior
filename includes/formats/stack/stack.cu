// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "../../../src/platform/platform.h"
#include "memoria_operor/memoria_operor.h"
#include "stack.h"

#include <cuda_runtime.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define STACK_UNCACHED_BLOCK (64ULL << 20u)

#define STACK_UNCACHED_ALIGN 4096ULL

extern "C" PlatformFile *stack_open(const char *path, unsigned int header[4])
{
    PlatformFile *const file = platform_file_open(path, "rb");
    unsigned int words[5] = {0u, 0u, 0u, 0u, 0u};
    if ((file == NULL) || (platform_file_read_items(words, sizeof(unsigned int), 5u, file) != 5u))
    {
        if (file != NULL)
        {
            platform_file_close(file);
        }
        return NULL;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = header, .src = words, .bytes = 4u * sizeof(unsigned int));
    return file;
}

extern "C" int stack_upload(const char *path, unsigned long long extent[4], unsigned short **device_lanes)
{
    unsigned int header[4] = {0u, 0u, 0u, 0u};
    PlatformFile *const stack = stack_open(path, header);
    for (unsigned int axis = 0u; axis < 4u; axis += 1u)
    {
        extent[axis] = header[axis];
    }
    const size_t lanes = (size_t)(extent[0] * extent[1] * extent[2] * extent[3]);
    unsigned short *const volume =
        (unsigned short *)platform_cell_taken((lanes + 1u) * sizeof(unsigned short), platform_place_g_program_owner, 0);
    *device_lanes = NULL;
    const int ok =
        (stack != NULL) && (volume != NULL) &&
        (platform_file_read_items(volume, sizeof(unsigned short), lanes, stack) == lanes) &&
        (cudaMalloc((void **)device_lanes, lanes * sizeof(unsigned short)) == cudaSuccess) &&
        (cudaMemcpy(*device_lanes, volume, lanes * sizeof(unsigned short), cudaMemcpyHostToDevice) == cudaSuccess);
    if (stack != NULL)
    {
        platform_file_close(stack);
    }
    platform_cell_released(volume, platform_place_g_program_owner);
    return ok;
}

extern "C" long long stack_file_size(const char *path)
{
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    if ((path == NULL) || (GetFileAttributesExA(path, GetFileExInfoStandard, &attributes) == 0) ||
        ((attributes.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0u))
    {
        return ENGINE_BYTES_ERROR;
    }
    return (long long)(((unsigned long long)attributes.nFileSizeHigh << 32u) |
                       (unsigned long long)attributes.nFileSizeLow);
#else
    struct stat status;
    if ((path == NULL) || (stat(path, &status) != 0) || !S_ISREG(status.st_mode))
    {
        return ENGINE_BYTES_ERROR;
    }
    return (long long)status.st_size;
#endif
}

extern "C" long long stack_file_read(const EngineFileRange *range)
{
    if ((range == NULL) || (range->path == NULL) || ((range->out == NULL) && (range->bytes != 0ULL)))
    {
        return ENGINE_BYTES_ERROR;
    }
    const unsigned long long first = range->offset;
    const unsigned long long last = range->offset + range->bytes;
    const unsigned long long start = first & ~(STACK_UNCACHED_ALIGN - 1ULL);
    const unsigned long long span = ((last - start) + STACK_UNCACHED_ALIGN - 1ULL) & ~(STACK_UNCACHED_ALIGN - 1ULL);
    const unsigned long long block =
        (span < STACK_UNCACHED_BLOCK) ? ((span != 0ULL) ? span : STACK_UNCACHED_ALIGN) : STACK_UNCACHED_BLOCK;
#ifdef _WIN32
    HANDLE const file = CreateFileA(range->path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                    FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    unsigned char *const buffer =
        (unsigned char *)VirtualAlloc(NULL, (SIZE_T)block, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    LARGE_INTEGER seek;
    seek.QuadPart = (LONGLONG)start;
    int ok =
        (file != INVALID_HANDLE_VALUE) && (buffer != NULL) && (SetFilePointerEx(file, seek, NULL, FILE_BEGIN) != 0);
#else
    int descriptor = open(range->path, O_RDONLY | O_DIRECT);
    if (descriptor < 0)
    {
        descriptor = open(range->path, O_RDONLY);
        if (descriptor >= 0)
        {
            (void)posix_fadvise(descriptor, 0, 0, POSIX_FADV_DONTNEED);
        }
    }
    // pages of the system, as VirtualAlloc gives them on Windows: a page start meets the alignment of a direct read
    void *const pages = mmap(NULL, (size_t)block, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    int ok = (descriptor >= 0) && (pages != MAP_FAILED);
    unsigned char *const buffer = (unsigned char *)pages;
#endif
    unsigned long long copied = 0ULL;
    int ended = 0;
    for (unsigned long long offset = start; ok && (ended == 0) && (offset < last); offset += block)
    {
        unsigned long long got = 0ULL;
#ifdef _WIN32
        DWORD read = 0u;
        ok = (ReadFile(file, buffer, (DWORD)block, &read, NULL) != 0);
        got = read;
#else
        const ssize_t read = pread(descriptor, buffer, (size_t)block, (off_t)offset);
        ok = (read >= 0);
        got = ok ? (unsigned long long)read : 0ULL;
#endif
        ended = (got < block);
        const unsigned long long from = (offset > first) ? offset : first;
        const unsigned long long copied_end = ((offset + got) < last) ? (offset + got) : last;
        if (ok && (copied_end > from))
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = range->out + (from - first), .src = buffer + (from - offset),
                       .bytes = (size_t)(copied_end - from));
            copied += copied_end - from;
        }
    }
#ifdef _WIN32
    if (buffer != NULL)
    {
        VirtualFree(buffer, 0, MEM_RELEASE);
    }
    if (file != INVALID_HANDLE_VALUE)
    {
        CloseHandle(file);
    }
#else
    if (pages != MAP_FAILED)
    {
        (void)munmap(pages, (size_t)block);
    }
    if (descriptor >= 0)
    {
        close(descriptor);
    }
#endif
    return ok ? (long long)copied : ENGINE_BYTES_ERROR;
}

extern "C" int stack_read_uncached(const char *path, unsigned long long lanes, unsigned short *volume)
{
    const unsigned long long first = (unsigned long long)STACK_VOXELS_AT;
    const unsigned long long last = first + (2ULL * lanes);
#ifdef _WIN32
    HANDLE const file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                    FILE_FLAG_NO_BUFFERING | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    unsigned char *const buffer =
        (unsigned char *)VirtualAlloc(NULL, (SIZE_T)STACK_UNCACHED_BLOCK, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    LARGE_INTEGER size;
    size.QuadPart = 0;
    int ok = (file != INVALID_HANDLE_VALUE) && (buffer != NULL) && (GetFileSizeEx(file, &size) != 0) &&
             ((unsigned long long)size.QuadPart == last);
#else
    int descriptor = open(path, O_RDONLY | O_DIRECT);
    if (descriptor < 0)
    {
        descriptor = open(path, O_RDONLY);
        if (descriptor >= 0)
        {
            (void)posix_fadvise(descriptor, 0, 0, POSIX_FADV_DONTNEED);
        }
    }
    struct stat status;
    // pages of the system, as VirtualAlloc gives them on Windows: a page start meets the alignment of a direct read
    void *const pages =
        mmap(NULL, (size_t)STACK_UNCACHED_BLOCK, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    int ok = (descriptor >= 0) && (pages != MAP_FAILED) && (fstat(descriptor, &status) == 0) &&
             ((unsigned long long)status.st_size == last);
    unsigned char *const buffer = (unsigned char *)pages;
#endif
    for (unsigned long long offset = 0ULL; ok && (offset < last); offset += STACK_UNCACHED_BLOCK)
    {
        unsigned long long got = 0ULL;
#ifdef _WIN32
        DWORD read = 0u;
        ok = (ReadFile(file, buffer, (DWORD)STACK_UNCACHED_BLOCK, &read, NULL) != 0);
        got = read;
#else
        const ssize_t read = pread(descriptor, buffer, (size_t)STACK_UNCACHED_BLOCK, (off_t)offset);
        ok = (read >= 0);
        got = ok ? (unsigned long long)read : 0ULL;
#endif
        const unsigned long long from = (offset > first) ? offset : first;
        const unsigned long long to = ((offset + got) < last) ? (offset + got) : last;
        ok = ok && ((got == STACK_UNCACHED_BLOCK) || ((offset + got) >= last));
        if (ok && (to > from))
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = (unsigned char *)volume + (from - first),
                       .src = buffer + (from - offset), .bytes = (size_t)(to - from));
        }
    }
#ifdef _WIN32
    if (buffer != NULL)
    {
        VirtualFree(buffer, 0, MEM_RELEASE);
    }
    if (file != INVALID_HANDLE_VALUE)
    {
        CloseHandle(file);
    }
#else
    if (pages != MAP_FAILED)
    {
        (void)munmap(pages, (size_t)STACK_UNCACHED_BLOCK);
    }
    if (descriptor >= 0)
    {
        close(descriptor);
    }
#endif
    return ok;
}
