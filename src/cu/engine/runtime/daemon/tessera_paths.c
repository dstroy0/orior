// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "../../../../../includes/platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include "tessera_text.h"

#if defined(_WIN32)
#define TESSERA_SEPARATOR '\\'
#else
#define TESSERA_SEPARATOR '/'
#endif

#if defined(_WIN32)
_Alignas(8) static const char s_tessera_pipe[] = "\\\\.\\pipe\\tessera-";
#else
_Alignas(8) static const char s_tessera_socket[] = "/tessera-";
_Alignas(8) static const char s_tessera_socket_end[] = ".sock";
_Alignas(8) static const char s_tessera_local_state[] = "/.local/state/tessera";
_Alignas(8) static const char s_tessera_temporary[] = "/tmp";
#endif
_Alignas(8) static const char s_tessera_folder[] = "tessera";
_Alignas(8) static const char s_tessera_lost[] = "lostandfound";

int tessera_text_staged(VerbaLinea *line, const char *text)
{
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    char *const staged =
        (char *)platform_cell_cleared((unsigned long long)((length / 8u) + 1u), (unsigned long long)(8u),
                                      (unsigned int)platform_place_g_engine_owner, 0);
    if (staged == NULL)
    {
        line->at = line->capacity;
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = staged, .src = text, .bytes = length);
    mmgr_line_put(line, staged);
    platform_cell_released(staged, (unsigned int)platform_place_g_engine_owner);
    return 1;
}

void tessera_bytes_hex(VerbaLinea *line, const unsigned char *bytes, unsigned int count)
{
    for (unsigned int byte = 0u; byte < count; byte += 1u)
    {
        mmgr_line_hex(line, bytes[byte], 2u);
    }
}

// the value of the variable `name` in `out` of PLATFORM_ENVIRONMENT_BYTES, or NULL where it holds none or an empty one
static const char *tessera_environment(const char *name, char out[PLATFORM_ENVIRONMENT_BYTES])
{
    return (platform_environment(name, out, PLATFORM_ENVIRONMENT_BYTES) != 0ULL) ? out : NULL;
}

static int tessera_line_done(VerbaLinea *line)
{
    return mmgr_line_finish(line) != 0ULL;
}

int tessera_path_endpoint(const unsigned char device[TESSERA_DEVICE_BYTES], char *path, unsigned int capacity)
{
    VerbaLinea line = {path, capacity, 0ULL};
#if defined(_WIN32)
    mmgr_line_put(&line, s_tessera_pipe);
    // a runtime named apart (a test's) is a pipe of its own, as it is a folder of its own on Linux; a backslash, which
    // a pipe's name may not contain past its prefix, is written as '_'
    char runtime_text[PLATFORM_ENVIRONMENT_BYTES];
    const char *const runtime = tessera_environment("TESSERA_RUNTIME", runtime_text);
    for (const char *walk = runtime; (walk != NULL) && (*walk != '\0'); walk += 1)
    {
        mmgr_line_ch(&line, (*walk == '\\') ? '_' : *walk);
    }
    if (runtime != NULL)
    {
        mmgr_line_ch(&line, '-');
    }
    tessera_bytes_hex(&line, device, TESSERA_DEVICE_BYTES);
#else
    // the second read takes the buffer only where the first left it empty
    char runtime_text[PLATFORM_ENVIRONMENT_BYTES];
    const char *runtime = tessera_environment("TESSERA_RUNTIME", runtime_text);
    runtime = (runtime != NULL) ? runtime : tessera_environment("XDG_RUNTIME_DIR", runtime_text);
    if (runtime != NULL)
    {
        tessera_text_staged(&line, runtime);
    }
    else
    {
        mmgr_line_put(&line, s_tessera_temporary);
    }
    mmgr_line_put(&line, s_tessera_socket);
    tessera_bytes_hex(&line, device, TESSERA_DEVICE_BYTES);
    mmgr_line_put(&line, s_tessera_socket_end);
#endif
    return tessera_line_done(&line);
}

int tessera_path_state(const unsigned char device[TESSERA_DEVICE_BYTES], char *path, unsigned int capacity)
{
    VerbaLinea line = {path, capacity, 0ULL};
    char chosen_text[PLATFORM_ENVIRONMENT_BYTES];
    const char *const chosen = tessera_environment("TESSERA_STATE", chosen_text);
    if (chosen != NULL)
    {
        tessera_text_staged(&line, chosen);
    }
    else
    {
#if defined(_WIN32)
        char local_text[PLATFORM_ENVIRONMENT_BYTES];
        const char *const local = tessera_environment("LOCALAPPDATA", local_text);
        if (local == NULL)
        {
            return 0;
        }
        tessera_text_staged(&line, local);
        mmgr_line_ch(&line, TESSERA_SEPARATOR);
        mmgr_line_put(&line, s_tessera_folder);
#else
        char state_text[PLATFORM_ENVIRONMENT_BYTES];
        char home_text[PLATFORM_ENVIRONMENT_BYTES];
        const char *const state = tessera_environment("XDG_STATE_HOME", state_text);
        const char *const home = tessera_environment("HOME", home_text);
        if ((state == NULL) && (home == NULL))
        {
            return 0;
        }
        tessera_text_staged(&line, (state != NULL) ? state : home);
        if (state != NULL)
        {
            mmgr_line_ch(&line, TESSERA_SEPARATOR);
            mmgr_line_put(&line, s_tessera_folder);
        }
        else
        {
            mmgr_line_put(&line, s_tessera_local_state);
        }
#endif
    }
    mmgr_line_ch(&line, TESSERA_SEPARATOR);
    tessera_bytes_hex(&line, device, TESSERA_DEVICE_BYTES);
    return tessera_line_done(&line);
}

int tessera_path_lost(const unsigned char device[TESSERA_DEVICE_BYTES], unsigned long long identity,
                      const EngineSignum *signum, char *path, unsigned int capacity)
{
    if (!tessera_path_state(device, path, capacity))
    {
        return 0;
    }
    VerbaLinea line = {path, capacity, 0ULL};
    line.at = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    mmgr_line_ch(&line, TESSERA_SEPARATOR);
    mmgr_line_put(&line, s_tessera_lost);
    mmgr_line_ch(&line, TESSERA_SEPARATOR);
    mmgr_line_hex(&line, identity, 16u);
    mmgr_line_ch(&line, '-');
    tessera_bytes_hex(&line, signum->bytes, ENGINE_SIGNUM_BYTES);
    return tessera_line_done(&line);
}
