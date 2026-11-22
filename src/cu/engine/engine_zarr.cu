// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// engine_zarr.cu: zarr v2 and v3, n5, and their description
#include "engine_internal.h"
#include "engine_note.h"
#include "memoria_operor/memoria_linea.h"
#include "cellularum_laboro/cellularum_linea.h"

static int entry_zarr_v3_chain(const EntryJson *json, unsigned int codecs, ZarrLayout *layout, int inner)
{
    ZarrChain *const chain = (inner != 0) ? &layout->inner_chain : &layout->chain;
    if ((codecs == 0u) || (json->tokens[codecs].type != CFG_JSON_ARRAY))
    {
        return 0;
    }
    const unsigned int rank = layout->extent.rank;
    int bytes_seen = 0;
    for (unsigned int at = 0u; at < json->tokens[codecs].count; at += 1u)
    {
        const unsigned int codec = entry_json_element(json, codecs, at);
        const unsigned int configuration = entry_json_at(json, codec, "configuration");
        char name[64];
        if ((entry_json_text(json, entry_json_at(json, codec, "name"), name, sizeof(name)) == 0) ||
            (chain->crc32c != 0u))
        {
            {
                EngineNote engine_note;
                engine_note_begun(&engine_note);
                mmgr_line_put(&engine_note.line, "  zarr: a codec is unnamed, or follows crc32c");
                mmgr_line_ch(&engine_note.line, '\n');
                engine_note_written(&engine_note);
            }
            return 0;
        }
        if (mmgr_text_equal(name, "bytes", PLATFORM_TEXT_TO_TERMINATOR))
        {
            char endian[16] = "little";
            const unsigned int stated = entry_json_at(json, configuration, "endian");
            if ((stated != 0u) && (entry_json_text(json, stated, endian, sizeof(endian)) == 0))
            {
                return 0;
            }
            layout->big_endian = (mmgr_text_equal(endian, "big", PLATFORM_TEXT_TO_TERMINATOR)) ? 1u : 0u;
            bytes_seen = 1;
        }
        else if (mmgr_text_equal(name, "transpose", PLATFORM_TEXT_TO_TERMINATOR))
        {
            unsigned long long order[ENGINE_ARRAY_RANK];
            unsigned int count = 0u;
            if ((bytes_seen != 0) ||
                (entry_json_list(json, entry_json_at(json, configuration, "order"), order, &count, ENGINE_ARRAY_RANK) ==
                 0) ||
                (count != rank))
            {
                {
                    EngineNote engine_note;
                    engine_note_begun(&engine_note);
                    mmgr_line_put(&engine_note.line,
                                  "  zarr: transpose is supported only as an order list before bytes");
                    mmgr_line_ch(&engine_note.line, '\n');
                    engine_note_written(&engine_note);
                }
                return 0;
            }
            for (unsigned int axis = 0u; axis < rank; axis += 1u)
            {
                layout->order[axis] = (unsigned int)order[axis];
            }
        }
        else if (mmgr_text_equal(name, "sharding_indexed", PLATFORM_TEXT_TO_TERMINATOR))
        {
            unsigned int count = 0u;
            if ((inner != 0) || (at != 0u) ||
                (entry_json_list(json, entry_json_at(json, configuration, "chunk_shape"), layout->inner, &count,
                                 ENGINE_ARRAY_RANK) == 0) ||
                (count != rank) ||
                (entry_zarr_v3_chain(json, entry_json_at(json, configuration, "codecs"), layout, 1) == 0))
            {
                {
                    EngineNote engine_note;
                    engine_note_begun(&engine_note);
                    mmgr_line_put(&engine_note.line,
                                  "  zarr: sharding_indexed is supported only as the first codec, with its own chain");
                    mmgr_line_ch(&engine_note.line, '\n');
                    engine_note_written(&engine_note);
                }
                return 0;
            }
            layout->sharded = 1u;
            const unsigned int index = entry_json_at(json, configuration, "index_codecs");
            for (unsigned int element = 0u; (index != 0u) && (element < json->tokens[index].count); element += 1u)
            {
                const unsigned int step = entry_json_element(json, index, element);
                char step_name[64];
                char endian[16] = "little";
                if (entry_json_text(json, entry_json_at(json, step, "name"), step_name, sizeof(step_name)) == 0)
                {
                    return 0;
                }
                const unsigned int stated = entry_json_at(json, entry_json_at(json, step, "configuration"), "endian");
                if ((mmgr_text_equal(step_name, "bytes", PLATFORM_TEXT_TO_TERMINATOR)) && (stated != 0u) &&
                    entry_json_text(json, stated, endian, sizeof(endian)))
                {
                    layout->index_big_endian = (mmgr_text_equal(endian, "big", PLATFORM_TEXT_TO_TERMINATOR)) ? 1u : 0u;
                }
                layout->index_chain.crc32c |=
                    (mmgr_text_equal(step_name, "crc32c", PLATFORM_TEXT_TO_TERMINATOR)) ? 1u : 0u;
                layout->index_chain.count += ((!mmgr_text_equal(step_name, "bytes", PLATFORM_TEXT_TO_TERMINATOR)) &&
                                              (!mmgr_text_equal(step_name, "crc32c", PLATFORM_TEXT_TO_TERMINATOR)))
                                                 ? 1u
                                                 : 0u;
            }
            char location[16] = "end";
            const unsigned int placed = entry_json_at(json, configuration, "index_location");
            if ((placed != 0u) && (entry_json_text(json, placed, location, sizeof(location)) == 0))
            {
                return 0;
            }
            layout->index_at_start = (mmgr_text_equal(location, "start", PLATFORM_TEXT_TO_TERMINATOR)) ? 1u : 0u;
            bytes_seen = 1;
        }
        else if (mmgr_text_equal(name, "crc32c", PLATFORM_TEXT_TO_TERMINATOR))
        {
            chain->crc32c = 1u;
        }
        else
        {
            EngineCodec byte_codec = ENGINE_CODEC_RAW;
            if ((bytes_seen == 0) || (entry_codec_named(name, &byte_codec) == 0) || (chain->count >= ZARR_CODECS))
            {
                {
                    EngineNote engine_note;
                    engine_note_begun(&engine_note);
                    mmgr_line_put(&engine_note.line, "  zarr: the codec ");
                    mmgr_line_put(&engine_note.line, name);
                    mmgr_line_put(&engine_note.line, " is not supported here");
                    mmgr_line_ch(&engine_note.line, '\n');
                    engine_note_written(&engine_note);
                }
                return 0;
            }
            chain->codec[chain->count] = byte_codec;
            chain->count += 1u;
        }
    }
    return bytes_seen;
}

static void entry_fill(const EntryJson *json, unsigned int token, ZarrLayout *layout)
{
    unsigned long long value = 0ULL;
    const int counted = (token != 0u) && cfg_json_unsigned(json->text, &json->tokens[token], &value);
    for (unsigned int place = 0u; place < 8u; place += 1u)
    {
        layout->fill[place] = (counted != 0) ? (unsigned char)((value >> (8u * place)) & 0xFFULL) : 0u;
    }
}

static int entry_zarr_v3(const EntryJson *json, ZarrLayout *layout)
{
    unsigned int rank = 0u;
    char type[32];
    if ((entry_json_list(json, entry_json_at(json, 0u, "shape"), layout->extent.sizes, &rank, ENGINE_ARRAY_RANK) ==
         0) ||
        (rank == 0u) || (entry_json_text(json, entry_json_at(json, 0u, "data_type"), type, sizeof(type)) == 0) ||
        (entry_element_named(type, &layout->extent, &layout->big_endian) == 0))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  zarr: the array's shape or data type is not supported");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 0;
    }
    layout->extent.rank = rank;
    unsigned int chunk_rank = 0u;
    const unsigned int grid = entry_json_at(json, entry_json_at(json, 0u, "chunk_grid"), "configuration");
    if ((entry_json_list(json, entry_json_at(json, grid, "chunk_shape"), layout->chunk, &chunk_rank,
                         ENGINE_ARRAY_RANK) == 0) ||
        (chunk_rank != rank))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  zarr: only a regular chunk grid is supported");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 0;
    }
    const unsigned int encoding = entry_json_at(json, 0u, "chunk_key_encoding");
    char encoding_name[16] = "default";
    char separator[4] = "";
    entry_json_text(json, entry_json_at(json, encoding, "name"), encoding_name, sizeof(encoding_name));
    const int split =
        entry_json_text(json, entry_json_at(json, entry_json_at(json, encoding, "configuration"), "separator"),
                        separator, sizeof(separator));
    layout->prefixed = (mmgr_text_equal(encoding_name, "v2", PLATFORM_TEXT_TO_TERMINATOR)) ? 0u : 1u;
    layout->separator = (split != 0) ? separator[0] : ((layout->prefixed != 0u) ? '/' : '.');
    for (unsigned int axis = 0u; axis < rank; axis += 1u)
    {
        layout->order[axis] = axis;
    }
    entry_fill(json, entry_json_at(json, 0u, "fill_value"), layout);
    layout->format = ZARR_FORMAT_V3;
    const int chained = entry_zarr_v3_chain(json, entry_json_at(json, 0u, "codecs"), layout, 0);
    if ((chained == 0) || ((layout->sharded != 0u) && ((layout->chain.count != 0u) || (layout->chain.crc32c != 0u))))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  zarr: the codec chain is not supported");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 0;
    }
    return 1;
}

static int entry_zarr_v2(const EntryJson *json, ZarrLayout *layout)
{
    unsigned int rank = 0u;
    unsigned int chunk_rank = 0u;
    char type[32];
    if ((entry_json_list(json, entry_json_at(json, 0u, "shape"), layout->extent.sizes, &rank, ENGINE_ARRAY_RANK) ==
         0) ||
        (rank == 0u) ||
        (entry_json_list(json, entry_json_at(json, 0u, "chunks"), layout->chunk, &chunk_rank, ENGINE_ARRAY_RANK) ==
         0) ||
        (chunk_rank != rank) || (entry_json_text(json, entry_json_at(json, 0u, "dtype"), type, sizeof(type)) == 0) ||
        (entry_element_named(type, &layout->extent, &layout->big_endian) == 0))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  zarr: the v2 array's shape, chunks or dtype is not supported");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 0;
    }
    layout->extent.rank = rank;
    const unsigned int filters = entry_json_at(json, 0u, "filters");
    if ((filters != 0u) && (json->tokens[filters].type != CFG_JSON_NULL) &&
        !((json->tokens[filters].type == CFG_JSON_ARRAY) && (json->tokens[filters].count == 0u)))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  zarr: v2 filters are not supported");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 0;
    }
    const unsigned int compressor = entry_json_at(json, 0u, "compressor");
    if ((compressor != 0u) && (json->tokens[compressor].type == CFG_JSON_OBJECT))
    {
        char name[32];
        EngineCodec codec = ENGINE_CODEC_RAW;
        if ((entry_json_text(json, entry_json_at(json, compressor, "id"), name, sizeof(name)) == 0) ||
            (entry_codec_named(name, &codec) == 0))
        {
            {
                EngineNote engine_note;
                engine_note_begun(&engine_note);
                mmgr_line_put(&engine_note.line, "  zarr: the v2 compressor is not supported");
                mmgr_line_ch(&engine_note.line, '\n');
                engine_note_written(&engine_note);
            }
            return 0;
        }
        layout->chain.codec[0] = codec;
        layout->chain.count = 1u;
    }
    char order[4] = "C";
    entry_json_text(json, entry_json_at(json, 0u, "order"), order, sizeof(order));
    char separator[4] = ".";
    entry_json_text(json, entry_json_at(json, 0u, "dimension_separator"), separator, sizeof(separator));
    for (unsigned int axis = 0u; axis < rank; axis += 1u)
    {
        layout->order[axis] = (order[0] == 'F') ? (rank - 1u - axis) : axis;
    }
    layout->separator = separator[0];
    layout->prefixed = 0u;
    entry_fill(json, entry_json_at(json, 0u, "fill_value"), layout);
    layout->format = ZARR_FORMAT_V2;
    return 1;
}

static int entry_n5(const EntryJson *json, ZarrLayout *layout)
{
    unsigned long long dimensions[ENGINE_ARRAY_RANK];
    unsigned long long blocks[ENGINE_ARRAY_RANK];
    unsigned int rank = 0u;
    unsigned int block_rank = 0u;
    char type[32];
    unsigned int ignored = 0u;
    if ((entry_json_list(json, entry_json_at(json, 0u, "dimensions"), dimensions, &rank, ENGINE_ARRAY_RANK) == 0) ||
        (rank == 0u) ||
        (entry_json_list(json, entry_json_at(json, 0u, "blockSize"), blocks, &block_rank, ENGINE_ARRAY_RANK) == 0) ||
        (block_rank != rank) || (entry_json_text(json, entry_json_at(json, 0u, "dataType"), type, sizeof(type)) == 0) ||
        (entry_element_named(type, &layout->extent, &ignored) == 0))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  n5: the dataset's dimensions, blockSize or dataType is not supported");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 0;
    }
    layout->extent.rank = rank;
    for (unsigned int axis = 0u; axis < rank; axis += 1u)
    {
        layout->extent.sizes[axis] = dimensions[rank - 1u - axis];
        layout->chunk[axis] = blocks[rank - 1u - axis];
        layout->order[axis] = axis;
    }
    const unsigned int compression = entry_json_at(json, 0u, "compression");
    char codec_name[32] = "raw";
    if (compression != 0u)
    {
        entry_json_text(json, entry_json_at(json, compression, "type"), codec_name, sizeof(codec_name));
    }
    else
    {
        entry_json_text(json, entry_json_at(json, 0u, "compressionType"), codec_name, sizeof(codec_name));
    }
    const unsigned int zlib = entry_json_at(json, compression, "useZlib");
    if (mmgr_text_equal(codec_name, "gzip", PLATFORM_TEXT_TO_TERMINATOR))
    {
        layout->chain.codec[0] =
            ((zlib != 0u) && (json->tokens[zlib].type == CFG_JSON_TRUE)) ? ENGINE_CODEC_ZLIB : ENGINE_CODEC_GZIP;
        layout->chain.count = 1u;
    }
    else if ((mmgr_text_equal(codec_name, "blosc", PLATFORM_TEXT_TO_TERMINATOR)) ||
             (mmgr_text_equal(codec_name, "zstd", PLATFORM_TEXT_TO_TERMINATOR)))
    {
        layout->chain.codec[0] = (codec_name[0] == 'b') ? ENGINE_CODEC_BLOSC : ENGINE_CODEC_ZSTD;
        layout->chain.count = 1u;
    }
    else if (!mmgr_text_equal(codec_name, "raw", PLATFORM_TEXT_TO_TERMINATOR))
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  n5: the compression ");
            mmgr_line_put(&engine_note.line, codec_name);
            mmgr_line_put(&engine_note.line, " is not supported");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        return 0;
    }
    layout->big_endian = 1u;
    layout->separator = '/';
    layout->prefixed = 0u;
    layout->format = ZARR_FORMAT_N5;
    const unsigned int axes = entry_json_at(json, 0u, "axes");
    for (unsigned int at = 0u; (axes != 0u) && (json->tokens[axes].count == rank) && (at < rank); at += 1u)
    {
        char name[32];
        layout->extent.axes[rank - 1u - at] =
            entry_json_text(json, entry_json_element(json, axes, at), name, sizeof(name)) ? entry_axis_named(name)
                                                                                          : '\0';
    }
    return 1;
}

int entry_zarr_describe(const char *root, const char *member, ZarrLayout *layout, char *array_root, size_t capacity)
{
    mmgr_bytes_set(layout, 0, sizeof(*layout));
    char path[ENTRY_PATH_CAPACITY];
    EntryJson json;
    mmgr_bytes_set(&json, 0, sizeof(json));
    EntryOme ome;
    mmgr_bytes_set(&ome, 0, sizeof(ome));
    int ok = 0;
    if (entry_joined(path, sizeof(path), root, "zarr.json") && entry_json_load(path, &json))
    {
        char node[16] = "";
        entry_json_text(&json, entry_json_at(&json, 0u, "node_type"), node, sizeof(node));
        const int group = (mmgr_text_equal(node, "group", PLATFORM_TEXT_TO_TERMINATOR));
        const int described = group ? entry_ome(&json, entry_json_at(&json, 0u, "attributes"), &ome) : 0;
        const char *const leaf = (member != NULL) ? member : (described ? ome.path : NULL);
        entry_json_release(&json);
        if (group && (leaf == NULL))
        {
            {
                EngineNote engine_note;
                engine_note_begun(&engine_note);
                mmgr_line_put(&engine_note.line, "  ");
                mmgr_line_put(&engine_note.line, root);
                mmgr_line_put(&engine_note.line, ": a group with no multiscales; name the array");
                mmgr_line_ch(&engine_note.line, '\n');
                engine_note_written(&engine_note);
            }
            return 0;
        }
        ok = (group ? entry_joined(array_root, capacity, root, leaf) : entry_joined(array_root, capacity, root, ".")) &&
             entry_joined(path, sizeof(path), array_root, "zarr.json") && entry_json_load(path, &json);
        ok = ok && entry_zarr_v3(&json, layout);
        if (ok)
        {
            entry_json_release(&json);
        }
    }
    else if (entry_joined(path, sizeof(path), root, ".zarray") && entry_exists(path))
    {
        ok = entry_joined(array_root, capacity, root, ".") && entry_json_load(path, &json);
        ok = ok && entry_zarr_v2(&json, layout);
        if (ok)
        {
            entry_json_release(&json);
        }
    }
    else if (entry_joined(path, sizeof(path), root, ".zgroup") && entry_exists(path))
    {
        const int attributed = entry_joined(path, sizeof(path), root, ".zattrs") && entry_json_load(path, &json);
        const int described = attributed && entry_ome(&json, 0u, &ome);
        if (attributed)
        {
            entry_json_release(&json);
        }
        const char *const leaf = (member != NULL) ? member : (described ? ome.path : NULL);
        ok = (leaf != NULL) && entry_joined(array_root, capacity, root, leaf) &&
             entry_joined(path, sizeof(path), array_root, ".zarray") && entry_json_load(path, &json);
        ok = ok && entry_zarr_v2(&json, layout);
        if (ok)
        {
            entry_json_release(&json);
        }
    }
    else if (entry_joined(path, sizeof(path), root, "attributes.json") && entry_json_load(path, &json))
    {
        const int dataset = (entry_json_at(&json, 0u, "dimensions") != 0u);
        entry_json_release(&json);
        const char *const leaf = (member != NULL) ? member : (dataset ? "." : "s0");
        ok = entry_joined(array_root, capacity, root, leaf) &&
             entry_joined(path, sizeof(path), array_root, "attributes.json") && entry_json_load(path, &json);
        ok = ok && entry_n5(&json, layout);
        if (ok)
        {
            entry_json_release(&json);
        }
    }
    if (ok == 0)
    {
        entry_json_release(&json);
        return 0;
    }
    for (unsigned int axis = 0u; (ome.rank == layout->extent.rank) && (axis < ome.rank); axis += 1u)
    {
        layout->extent.axes[axis] = ome.axes[axis];
    }
    return 1;
}
