// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// qry_crystal.c: a finished run's .qry stored as the engine's crystal, .kcr, and read back out of it (qry_run.sh). The
// .qry's bytes are the lanes of one axis, and the engine lifts, codes and seals them as it does any sample
// (engine_ingest_set): every sample is checked against its seal and against the bytes it was made from before the call
// returns, and then loaded again here and checked against the .qry byte for byte. Nothing is let go before every one
// matched.
//
//     qry_crystal write <run .qry> <set folder>    the .qry's bytes, QRY_CRYSTAL_PART at a time, each the sample
//                                                 part<n> of the set, at <set folder>/part<n>/part<n>.kcr
//     qry_crystal read <set folder> <run .qry>     the set's samples loaded in order and their bytes joined into the
//                                                 .qry
//
// The lifting runs on the device. Exit 0 where it was done and every sample matched, 1 where it was not, 2 where the
// words are none of the above
#include "../../../engine/engine.h"
#include "../../../../platform/platform.h"
#include "../../../engine/engine_note.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"


#if defined(_WIN32)
#include <direct.h>
#define QRY_CRYSTAL_FOLDER_REMOVED(path_) _rmdir(path_)
#else
#include <unistd.h>
#define QRY_CRYSTAL_FOLDER_REMOVED(path_) rmdir(path_)
#endif

// the max bytes of the .qry one sample takes, which bounds what the device stores while it lifts one: two bytes a lane
// and four a coefficient, with the tower's blocks beside them
#define QRY_CRYSTAL_PART (256ULL << 20u)

// the max samples a set contains, and the source folder the samples are read from while the set is made, inside it
#define QRY_CRYSTAL_PARTS 4096u
#define QRY_CRYSTAL_SOURCE "source"

static void qry_crystal_error_printed(const char *what, const EngineError *error)
{
    EngineNote engine_note;
    engine_note_begun(&engine_note);
    mmgr_line_put(&engine_note.line, "qry_crystal: ");
    mmgr_line_put(&engine_note.line, what);
    mmgr_line_put(&engine_note.line, " (type ");
    mmgr_line_decimal_signed(&engine_note.line, (int)error->type);
    mmgr_line_put(&engine_note.line, ", module ");
    mmgr_line_decimal_signed(&engine_note.line, (int)error->module);
    mmgr_line_put(&engine_note.line, ", site ");
    mmgr_line_decimal(&engine_note.line, error->site, 1u);
    mmgr_line_put(&engine_note.line, ", status ");
    mmgr_line_decimal_signed(&engine_note.line, error->status);
    mmgr_line_put(&engine_note.line, ")");
    mmgr_line_ch(&engine_note.line, '\n');
    engine_note_written(&engine_note);
}

// `count` bytes at `bytes` written as an .npy of unsigned bytes along one axis, at `path`. 1, or 0 where it could not
// be written
static int qry_crystal_npy_written(const char *path, const unsigned char *bytes, unsigned long long count)
{
    char header[160];
    VerbaLinea formed_line_1 = mmgr_line_over(header, sizeof(header));
    mmgr_line_put(&formed_line_1, "{'descr': '|u1', 'fortran_order': False, 'shape': (");
    mmgr_line_decimal(&formed_line_1, count, 1u);
    mmgr_line_put(&formed_line_1, ",), }");
    (void)mmgr_line_finish(&formed_line_1);
    int length = (int)formed_line_1.at;
    // the header is padded with spaces to a line ending that leaves the data on a 64 byte boundary, as the format
    // requires
    while (((10 + length + 1) % 64) != 0)
    {
        header[length] = ' ';
        length += 1;
    }
    header[length] = '\n';
    length += 1;
    const unsigned char lead[10] = {
        0x93u, 'N', 'U', 'M', 'P', 'Y', 1u, 0u, (unsigned char)(length & 0xff), (unsigned char)((length >> 8) & 0xff)};
    PlatformFile *const file = platform_file_open(path, "wb");
    if (file == NULL)
    {
        return 0;
    }
    // the header and the count are each below the room they are written from, and widen to the sizes fwrite takes
    int written = (platform_file_write_items(lead, 1u, sizeof(lead), file) == sizeof(lead)) &&
                  (platform_file_write_items(header, 1u, (size_t)length, file) == (size_t)length) &&
                  (platform_file_write_items(bytes, 1u, (size_t)count, file) == (size_t)count);
    written = (platform_file_close(file) == 0) && written;
    return written;
}

static int qry_crystal_written(const char *qry, const char *set)
{
    PlatformFile *const file = platform_file_open(qry, "rb");
    unsigned char *const bytes =
        (unsigned char *)platform_cell_taken((size_t)QRY_CRYSTAL_PART, platform_place_g_program_owner, 0);
    char source[1024];
    {
        VerbaLinea buffer_line = mmgr_line_over(source, sizeof(source));
        mmgr_line_put(&buffer_line, set);
        mmgr_line_put(&buffer_line, "/");
        mmgr_line_put(&buffer_line, QRY_CRYSTAL_SOURCE);
        (void)mmgr_line_finish(&buffer_line);
    }
    if ((file == NULL) || (bytes == NULL) || !engine_directories_make(source, 1))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "qry_crystal: ");
            mmgr_line_put(&engine_note.line, qry);
            mmgr_line_put(&engine_note.line, " could not be read, or ");
            mmgr_line_put(&engine_note.line, source);
            mmgr_line_put(&engine_note.line, " made");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        platform_cell_released(bytes, platform_place_g_program_owner);
        if (file != NULL)
        {
            platform_file_close(file);
        }
        return 0;
    }
    // every part written as a sample's source
    static char s_names[QRY_CRYSTAL_PARTS][16];
    static char *s_named[QRY_CRYSTAL_PARTS];
    static unsigned long long s_sizes[QRY_CRYSTAL_PARTS];
    unsigned int parts = 0u;
    unsigned long long total = 0ULL;
    int whole = 1;
    while (whole && (parts < QRY_CRYSTAL_PARTS))
    {
        const size_t read = platform_file_read_items(bytes, 1u, (size_t)QRY_CRYSTAL_PART, file);
        if (read == 0u)
        {
            break;
        }
        {
            VerbaLinea buffer_line = mmgr_line_over(s_names[parts], sizeof(s_names[parts]));
            mmgr_line_put(&buffer_line, "part");
            mmgr_line_decimal(&buffer_line, parts, 4u);
            (void)mmgr_line_finish(&buffer_line);
        }
        s_named[parts] = s_names[parts];
        s_sizes[parts] = (unsigned long long)read;
        char path[1200];
        {
            VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
            mmgr_line_put(&buffer_line, source);
            mmgr_line_put(&buffer_line, "/");
            mmgr_line_put(&buffer_line, s_names[parts]);
            mmgr_line_put(&buffer_line, ".npy");
            (void)mmgr_line_finish(&buffer_line);
        }
        whole = qry_crystal_npy_written(path, bytes, (unsigned long long)read);
        total += (unsigned long long)read;
        parts += 1u;
    }
    const int ended = platform_file_ended(file) != 0;
    platform_file_close(file);
    if (!whole || !ended || (parts == 0u))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "qry_crystal: ");
            mmgr_line_put(&engine_note.line, qry);
            mmgr_line_put(&engine_note.line, " was not written whole as samples (");
            mmgr_line_decimal(&engine_note.line, parts, 1u);
            mmgr_line_put(&engine_note.line, " parts)");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        platform_cell_released(bytes, platform_place_g_program_owner);
        return 0;
    }
    // the samples lifted, coded and sealed on the device, each read back and checked against its source by the engine
    EngineError error;
    mmgr_bytes_set(&error, 0, sizeof(error));
    static EngineSampleRecord s_records[QRY_CRYSTAL_PARTS];
    mmgr_bytes_set(s_records, 0, sizeof(s_records));
    EngineSetReport report;
    mmgr_bytes_set(&report, 0, sizeof(report));
    report.samples = s_records;
    const EngineIngestRequest ingest = {source, set, s_named, parts, "x", 0u, &error, &report};
    whole = (engine_ingest_set(&ingest) != ENGINE_ERROR);
    if (!whole)
    {
        qry_crystal_error_printed("the engine did not make the set", &error);
    }
    // each sample loaded again and checked against the .qry byte for byte
    PlatformFile *const again = whole ? platform_file_open(qry, "rb") : NULL;
    unsigned long long differ = 0ULL;
    for (unsigned int part = 0u; whole && (part < parts); part += 1u)
    {
        const EngineSampleRecord *const record = &s_records[part];
        whole = (record->crystal_written != 0ULL) && (record->crystal_read != 0ULL) && (record->sealed != 0ULL) &&
                (record->voxels_differ == 0ULL) && (record->pixels_differ == 0ULL) && (record->chunks_differ == 0ULL) &&
                (record->roots_differ == 0ULL) && (record->extent_differ == 0ULL);
        unsigned long long extent[4] = {0ULL, 0ULL, 0ULL, 0ULL};
        unsigned short *lanes = NULL;
        EngineSignum root;
        mmgr_bytes_set(&error, 0, sizeof(error));
        whole = whole && (again != NULL) &&
                (engine_iapx_load(set, s_names[part], extent, &lanes, &root, NULL, &error) != ENGINE_ERROR) &&
                ((extent[0] * extent[1] * extent[2] * extent[3]) == s_sizes[part]) &&
                (platform_file_read_items(bytes, 1u, (size_t)s_sizes[part], again) == (size_t)s_sizes[part]);
        for (unsigned long long at = 0ULL; whole && (at < s_sizes[part]); at += 1ULL)
        {
            differ += (lanes[at] != (unsigned short)bytes[at]) ? 1ULL : 0ULL;
        }
        platform_cell_released(lanes, platform_place_g_program_owner);
        if (!whole)
        {
            qry_crystal_error_printed("a sample did not match", &error);
        }
    }
    if (again != NULL)
    {
        platform_file_close(again);
    }
    platform_cell_released(bytes, platform_place_g_program_owner);
    // the sources let go, whether the set matched or not
    for (unsigned int part = 0u; part < parts; part += 1u)
    {
        char path[1200];
        {
            VerbaLinea buffer_line = mmgr_line_over(path, sizeof(path));
            mmgr_line_put(&buffer_line, source);
            mmgr_line_put(&buffer_line, "/");
            mmgr_line_put(&buffer_line, s_names[part]);
            mmgr_line_put(&buffer_line, ".npy");
            (void)mmgr_line_finish(&buffer_line);
        }
        (void)platform_file_removed(path);
    }
    QRY_CRYSTAL_FOLDER_REMOVED(source);
    if (!whole || (differ != 0ULL))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "qry_crystal: ");
            mmgr_line_put(&engine_note.line, qry);
            mmgr_line_put(&engine_note.line, " did not match as the set ");
            mmgr_line_put(&engine_note.line, set);
            mmgr_line_put(&engine_note.line, ": ");
            mmgr_line_decimal(&engine_note.line, differ, 1u);
            mmgr_line_put(&engine_note.line, " bytes differ");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 0;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  qry_crystal: ");
        mmgr_line_put(output_line, qry);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, total, 1u);
        mmgr_line_put(output_line, " bytes in ");
        mmgr_line_decimal(output_line, parts, 1u);
        mmgr_line_put(output_line, " sample(s), stored as ");
        mmgr_line_decimal(output_line, report.crystal_bytes, 1u);
        mmgr_line_put(output_line, " bytes, every byte matched");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 1;
}

static int qry_crystal_read(const char *set, const char *qry)
{
    PlatformFile *const file = platform_file_open(qry, "wb");
    if (file == NULL)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "qry_crystal: ");
            mmgr_line_put(&engine_note.line, qry);
            mmgr_line_put(&engine_note.line, " could not be written");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 0;
    }
    int whole = 1;
    unsigned int parts = 0u;
    unsigned long long total = 0ULL;
    unsigned char *bytes = NULL;
    // the bytes of the cell that `bytes` names, which the next grow copies
    unsigned long long bytes_room = 0ULL;
    for (; whole && (parts < QRY_CRYSTAL_PARTS); parts += 1u)
    {
        char name[16];
        char path[1200];
        {
            VerbaLinea buffer_line = mmgr_line_over(name, sizeof(name));
            mmgr_line_put(&buffer_line, "part");
            mmgr_line_decimal(&buffer_line, parts, 4u);
            (void)mmgr_line_finish(&buffer_line);
        }
        engine_sample_path(path, sizeof(path), set, name, ".kcr");
        PlatformFile *const sample_file = platform_file_open(path, "rb");
        if (sample_file == NULL)
        {
            break;
        }
        platform_file_close(sample_file);
        unsigned long long extent[4] = {0ULL, 0ULL, 0ULL, 0ULL};
        unsigned short *lanes = NULL;
        EngineSignum root;
        EngineError error;
        mmgr_bytes_set(&error, 0, sizeof(error));
        whole = (engine_iapx_load(set, name, extent, &lanes, &root, NULL, &error) != ENGINE_ERROR);
        const unsigned long long count = extent[0] * extent[1] * extent[2] * extent[3];
        unsigned char *const larger = whole ? (unsigned char *)platform_cell_grown(bytes, bytes_room, count + 1ULL,
                                                                                   platform_place_g_program_owner, 0)
                                            : NULL;
        whole = whole && (larger != NULL);
        bytes = (larger != NULL) ? larger : bytes;
        bytes_room = (larger != NULL) ? (count + 1ULL) : bytes_room;
        for (unsigned long long at = 0ULL; whole && (at < count); at += 1ULL)
        {
            // a lane of an unsigned byte represents that byte
            bytes[at] = (unsigned char)lanes[at];
        }
        whole = whole && (platform_file_write_items(bytes, 1u, (size_t)count, file) == (size_t)count);
        total += whole ? count : 0ULL;
        platform_cell_released(lanes, platform_place_g_program_owner);
        if (!whole)
        {
            qry_crystal_error_printed("a sample could not be loaded", &error);
        }
    }
    platform_cell_released(bytes, platform_place_g_program_owner);
    whole = (platform_file_close(file) == 0) && whole && (parts != 0u);
    if (!whole)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "qry_crystal: ");
            mmgr_line_put(&engine_note.line, qry);
            mmgr_line_put(&engine_note.line, " was not read whole out of ");
            mmgr_line_put(&engine_note.line, set);
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        (void)platform_file_removed(qry);
        return 0;
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  qry_crystal: ");
        mmgr_line_put(output_line, qry);
        mmgr_line_put(output_line, ", ");
        mmgr_line_decimal(output_line, total, 1u);
        mmgr_line_put(output_line, " bytes out of ");
        mmgr_line_decimal(output_line, parts, 1u);
        mmgr_line_put(output_line, " sample(s) of ");
        mmgr_line_put(output_line, set);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return 1;
}

int program_main(int count, char **words)
{
    if ((count == 4) && (mmgr_text_equal(words[1], "write", PLATFORM_TEXT_TO_TERMINATOR)))
    {
        return qry_crystal_written(words[2], words[3]) ? 0 : 1;
    }
    if ((count == 4) && (mmgr_text_equal(words[1], "read", PLATFORM_TEXT_TO_TERMINATOR)))
    {
        return qry_crystal_read(words[2], words[3]) ? 0 : 1;
    }
    {
        EngineNote engine_note;
        engine_note_begun(&engine_note);
        mmgr_line_put(&engine_note.line, "qry_crystal write <run .qry> <set folder> | read <set folder> <run .qry>");
        mmgr_line_ch(&engine_note.line, '\n');
        engine_note_written(&engine_note);
    }
    return 2;
}
