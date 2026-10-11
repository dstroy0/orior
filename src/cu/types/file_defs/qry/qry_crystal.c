// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_crystal.c: a finished run's .qry kept as the engine's crystal, .kcr, and read back out of it (qry_run.sh). The
// .qry's bytes are the lanes of one axis, and the engine lifts, codes and seals them as it does any sample
// (engine_ingest_set): every sample is checked against its seal and against the bytes it was made from before the call
// returns, and then loaded again here and held to the .qry byte for byte. Nothing is let go before every one held.
//
//     qry_crystal write <run .qry> <set folder>    the .qry's bytes, QRY_CRYSTAL_PART at a time, each the sample
//                                                 part<n> of the set, at <set folder>/part<n>/part<n>.kcr
//     qry_crystal read <set folder> <run .qry>     the set's samples loaded in order and their bytes joined into the
//                                                 .qry
//
// The lifting runs on the device. Exit 0 where it was done and every sample held, 1 where it was not, 2 where the words
// are none of the above
#include "../../../engine/engine.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define QRY_CRYSTAL_FOLDER_REMOVED(path_) _rmdir(path_)
#else
#include <unistd.h>
#define QRY_CRYSTAL_FOLDER_REMOVED(path_) rmdir(path_)
#endif

// the most bytes of the .qry one sample takes, which bounds what the device holds while it lifts one: two bytes a lane
// and four a coefficient, with the tower's blocks beside them
#define QRY_CRYSTAL_PART (256ull << 20u)

// the most samples a set holds, and the source folder the samples are read from while the set is made, inside it
#define QRY_CRYSTAL_PARTS 4096u
#define QRY_CRYSTAL_SOURCE "source"

static void qry_crystal_error_said(const char *what, const EngineError *error)
{
    fprintf(stderr, "qry_crystal: %s (kind %d, module %d, site %u, status %d)\n", what, (int)error->kind,
            (int)error->module, error->site, error->status);
}

// `count` bytes at `bytes` written as an .npy of unsigned bytes along one axis, at `path`. 1, or 0 where it could not
// be written
static int qry_crystal_npy_written(const char *path, const unsigned char *bytes, unsigned long long count)
{
    char header[160];
    int length = snprintf(header, sizeof(header), "{'descr': '|u1', 'fortran_order': False, 'shape': (%llu,), }", count);
    // the header is padded with spaces to a line ending that leaves the data on a 64 byte boundary, as the format asks
    while (((10 + length + 1) % 64) != 0)
    {
        header[length] = ' ';
        length += 1;
    }
    header[length] = '\n';
    length += 1;
    const unsigned char lead[10] = {0x93u, 'N', 'U', 'M', 'P', 'Y', 1u, 0u, (unsigned char)(length & 0xff),
                                    (unsigned char)((length >> 8) & 0xff)};
    FILE *const file = fopen(path, "wb");
    if (file == NULL)
    {
        return 0;
    }
    // the header and the count are each below the room they are written from, and widen to the sizes fwrite takes
    int written = (fwrite(lead, 1u, sizeof(lead), file) == sizeof(lead)) &&
                  (fwrite(header, 1u, (size_t)length, file) == (size_t)length) &&
                  (fwrite(bytes, 1u, (size_t)count, file) == (size_t)count);
    written = (fclose(file) == 0) && written;
    return written;
}

static int qry_crystal_written(const char *qry, const char *set)
{
    FILE *const file = fopen(qry, "rb");
    unsigned char *const bytes = (unsigned char *)malloc((size_t)QRY_CRYSTAL_PART);
    char source[1024];
    snprintf(source, sizeof(source), "%s/%s", set, QRY_CRYSTAL_SOURCE);
    if ((file == NULL) || (bytes == NULL) || !engine_directories_make(source, 1))
    {
        fprintf(stderr, "qry_crystal: %s could not be read, or %s made\n", qry, source);
        free(bytes);
        if (file != NULL)
        {
            fclose(file);
        }
        return 0;
    }
    // every part written as a sample's source
    static char s_names[QRY_CRYSTAL_PARTS][16];
    static char *s_named[QRY_CRYSTAL_PARTS];
    static unsigned long long s_sizes[QRY_CRYSTAL_PARTS];
    unsigned int parts = 0u;
    unsigned long long total = 0ull;
    int whole = 1;
    while (whole && (parts < QRY_CRYSTAL_PARTS))
    {
        const size_t read = fread(bytes, 1u, (size_t)QRY_CRYSTAL_PART, file);
        if (read == 0u)
        {
            break;
        }
        snprintf(s_names[parts], sizeof(s_names[parts]), "part%04u", parts);
        s_named[parts] = s_names[parts];
        s_sizes[parts] = (unsigned long long)read;
        char path[1200];
        snprintf(path, sizeof(path), "%s/%s.npy", source, s_names[parts]);
        whole = qry_crystal_npy_written(path, bytes, (unsigned long long)read);
        total += (unsigned long long)read;
        parts += 1u;
    }
    const int ended = feof(file) != 0;
    fclose(file);
    if (!whole || !ended || (parts == 0u))
    {
        fprintf(stderr, "qry_crystal: %s was not written whole as samples (%u parts)\n", qry, parts);
        free(bytes);
        return 0;
    }
    // the samples lifted, coded and sealed on the device, each read back and held to its source by the engine
    EngineError error;
    memset(&error, 0, sizeof(error));
    static EngineSampleRecord s_records[QRY_CRYSTAL_PARTS];
    memset(s_records, 0, sizeof(s_records));
    EngineSetReport report;
    memset(&report, 0, sizeof(report));
    report.samples = s_records;
    const EngineIngestRequest ingest = {source, set, s_named, parts, "x", 0u, &error, &report};
    whole = (engine_ingest_set(&ingest) != ENGINE_ERROR);
    if (!whole)
    {
        qry_crystal_error_said("the engine did not make the set", &error);
    }
    // each sample loaded again and held to the .qry byte for byte
    FILE *const again = whole ? fopen(qry, "rb") : NULL;
    unsigned long long differ = 0ull;
    for (unsigned int part = 0u; whole && (part < parts); part += 1u)
    {
        const EngineSampleRecord *const record = &s_records[part];
        whole = (record->crystal_written != 0ull) && (record->crystal_read != 0ull) && (record->sealed != 0ull) &&
                (record->voxels_differ == 0ull) && (record->pixels_differ == 0ull) &&
                (record->chunks_differ == 0ull) && (record->roots_differ == 0ull) && (record->extent_differ == 0ull);
        unsigned long long extent[4] = {0ull, 0ull, 0ull, 0ull};
        unsigned short *lanes = NULL;
        EngineSignum root;
        memset(&error, 0, sizeof(error));
        whole = whole && (again != NULL) &&
                (engine_iapx_load(set, s_names[part], extent, &lanes, &root, NULL, &error) != ENGINE_ERROR) &&
                ((extent[0] * extent[1] * extent[2] * extent[3]) == s_sizes[part]) &&
                (fread(bytes, 1u, (size_t)s_sizes[part], again) == (size_t)s_sizes[part]);
        for (unsigned long long at = 0ull; whole && (at < s_sizes[part]); at += 1ull)
        {
            differ += (lanes[at] != (unsigned short)bytes[at]) ? 1ull : 0ull;
        }
        free(lanes);
        if (!whole)
        {
            qry_crystal_error_said("a sample did not hold", &error);
        }
    }
    if (again != NULL)
    {
        fclose(again);
    }
    free(bytes);
    // the sources let go, whether the set held or not
    for (unsigned int part = 0u; part < parts; part += 1u)
    {
        char path[1200];
        snprintf(path, sizeof(path), "%s/%s.npy", source, s_names[part]);
        remove(path);
    }
    QRY_CRYSTAL_FOLDER_REMOVED(source);
    if (!whole || (differ != 0ull))
    {
        fprintf(stderr, "qry_crystal: %s did not hold as the set %s: %llu bytes differ\n", qry, set, differ);
        return 0;
    }
    printf("  qry_crystal: %s, %llu bytes in %u sample(s), kept as %llu bytes, every byte held\n", qry, total, parts,
           report.crystal_bytes);
    return 1;
}

static int qry_crystal_read(const char *set, const char *qry)
{
    FILE *const file = fopen(qry, "wb");
    if (file == NULL)
    {
        fprintf(stderr, "qry_crystal: %s could not be written\n", qry);
        return 0;
    }
    int whole = 1;
    unsigned int parts = 0u;
    unsigned long long total = 0ull;
    unsigned char *bytes = NULL;
    for (; whole && (parts < QRY_CRYSTAL_PARTS); parts += 1u)
    {
        char name[16];
        char path[1200];
        snprintf(name, sizeof(name), "part%04u", parts);
        engine_sample_path(path, sizeof(path), set, name, ".kcr");
        FILE *const held = fopen(path, "rb");
        if (held == NULL)
        {
            break;
        }
        fclose(held);
        unsigned long long extent[4] = {0ull, 0ull, 0ull, 0ull};
        unsigned short *lanes = NULL;
        EngineSignum root;
        EngineError error;
        memset(&error, 0, sizeof(error));
        whole = (engine_iapx_load(set, name, extent, &lanes, &root, NULL, &error) != ENGINE_ERROR);
        const unsigned long long count = extent[0] * extent[1] * extent[2] * extent[3];
        unsigned char *const larger = whole ? (unsigned char *)realloc(bytes, (size_t)count + 1u) : NULL;
        whole = whole && (larger != NULL);
        bytes = (larger != NULL) ? larger : bytes;
        for (unsigned long long at = 0ull; whole && (at < count); at += 1ull)
        {
            // a lane of an unsigned byte holds that byte
            bytes[at] = (unsigned char)lanes[at];
        }
        whole = whole && (fwrite(bytes, 1u, (size_t)count, file) == (size_t)count);
        total += whole ? count : 0ull;
        free(lanes);
        if (!whole)
        {
            qry_crystal_error_said("a sample could not be loaded", &error);
        }
    }
    free(bytes);
    whole = (fclose(file) == 0) && whole && (parts != 0u);
    if (!whole)
    {
        fprintf(stderr, "qry_crystal: %s was not read whole out of %s\n", qry, set);
        remove(qry);
        return 0;
    }
    printf("  qry_crystal: %s, %llu bytes out of %u sample(s) of %s\n", qry, total, parts, set);
    return 1;
}

int main(int count, char **words)
{
    if ((count == 4) && (strcmp(words[1], "write") == 0))
    {
        return qry_crystal_written(words[2], words[3]) ? 0 : 1;
    }
    if ((count == 4) && (strcmp(words[1], "read") == 0))
    {
        return qry_crystal_read(words[2], words[3]) ? 0 : 1;
    }
    fprintf(stderr, "qry_crystal write <run .qry> <set folder> | read <set folder> <run .qry>\n");
    return 2;
}
