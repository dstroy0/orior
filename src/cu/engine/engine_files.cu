// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// engine_files.cu: directories, decoders and the JSON reader of an entry
#include "../../platform/platform.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "embed_atomic.h"
#include "engine_internal.h"
#include "engine_note.h"
#include "memoria_operor/memoria_operor.h"

static int entry_directory_end(const char *walk, size_t at, size_t length, int include_last)
{
    const int separator = (walk[at] == '/') || (walk[at] == '\\');
    const int last = (at == length) && (include_last != 0);
    const int drive = (at == 2u) && (walk[1] == ':');
    return ((separator != 0) || (last != 0)) && (drive == 0);
}

int entry_directories_make(const char *path, int include_last, unsigned int *made)
{
    *made = 0u;
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    char *const walk = (char *)platform_cell_taken(length + 1u, platform_place_g_program_owner, 0);
    if (walk == NULL)
    {
        return 0;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = walk, .src = path, .bytes = length + 1u);
    int ok = 1;
    for (size_t at = 1u; ok && (at <= length); at += 1u)
    {
        if (entry_directory_end(walk, at, length, include_last) == 0)
        {
            continue;
        }
        const char character = walk[at];
        walk[at] = '\0';
        const int created = ENGINE_DIRECTORY_MAKE(walk) == 0;
        ok = created || (errno == EEXIST);
        *made += created ? 1u : 0u;
        walk[at] = character;
    }
    platform_cell_released(walk, platform_place_g_program_owner);
    return ok;
}

void entry_directories_remove(const char *path, int include_last, unsigned int made)
{
    if (made == 0u)
    {
        return;
    }
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = path, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    char *const walk = (char *)platform_cell_taken(length + 1u, platform_place_g_program_owner, 0);
    if (walk == NULL)
    {
        return;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = walk, .src = path, .bytes = length + 1u);
    unsigned int left = made;
    for (size_t at = length; (left != 0u) && (at >= 1u); at -= 1u)
    {
        if (entry_directory_end(walk, at, length, include_last) == 0)
        {
            continue;
        }
        walk[at] = '\0';
        ENGINE_DIRECTORY_REMOVE(walk);
        left -= 1u;
    }
    platform_cell_released(walk, platform_place_g_program_owner);
}

extern "C" int engine_directories_make(const char *path, int include_last)
{
    unsigned int made = 0u;
    return entry_directories_make(path, include_last, &made);
}

extern "C" int engine_program_directory(char *out, size_t capacity)
{
    if ((out == NULL) || (capacity < 2u))
    {
        return 0;
    }
#ifdef _WIN32
    // the capacity is a buffer size the caller allocates, which a DWORD counts on every Windows target
    const DWORD written = GetModuleFileNameA(NULL, out, (DWORD)capacity);
    const size_t length = (size_t)written;
#else
    const ssize_t written = readlink("/proc/self/exe", out, capacity - 1u);
    // a failed read is -1, recorded as no length at all
    const size_t length = (written > 0) ? (size_t)written : 0u;
#endif
    size_t directory_length = (length < capacity) ? length : 0u;
    out[directory_length] = '\0';
    while ((directory_length != 0u) && (out[directory_length - 1u] != '/') && (out[directory_length - 1u] != '\\'))
    {
        directory_length -= 1u;
    }
    if (directory_length == 0u)
    {
        out[0] = '\0';
        return 0;
    }
    out[directory_length - 1u] = '\0';
    return 1;
}

extern "C" int engine_sample_path(char *out, size_t capacity, const char *set, const char *sample, const char *suffix)
{
    VerbaLinea formed_line_1 = mmgr_line_over(out, capacity);
    mmgr_line_put(&formed_line_1, set);
    mmgr_line_put(&formed_line_1, "/");
    mmgr_line_put(&formed_line_1, sample);
    mmgr_line_put(&formed_line_1, "/");
    mmgr_line_put(&formed_line_1, sample);
    mmgr_line_put(&formed_line_1, suffix);
    (void)mmgr_line_finish(&formed_line_1);
    const int written = (int)formed_line_1.at;
    return (written > 0) && ((size_t)written < capacity);
}

static long long entry_raw_decode(const EngineBytesRequest *request)
{
    if ((request == NULL) || (request->in_bytes > request->out_capacity))
    {
        return ENGINE_BYTES_ERROR;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request->out, .src = request->in, .bytes = (size_t)request->in_bytes);
    return (long long)request->in_bytes;
}

static EngineIngestTools s_ingest_tools;

// the workers a run of items takes: the whole processors tessera_run passed on ($TESSERA_RUN_PROCESSORS), 1 where it
// names none, and never more than the items
static unsigned long long entry_each_workers(unsigned long long items)
{
    char named_text[PLATFORM_ENVIRONMENT_BYTES];
    const char *const named =
        (platform_environment("TESSERA_RUN_PROCESSORS", named_text, sizeof(named_text)) != 0ULL) ? named_text : NULL;
    const char *end = NULL;
    const unsigned long long requested_workers =
        ((named != NULL) && (named[0] >= '1') && (named[0] <= '9'))
            ? EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = named, .end = &end, .base = 10)
            : 0ULL;
    const unsigned long long workers =
        ((requested_workers != 0ULL) && (end != NULL) && (*end == '\0')) ? requested_workers : 1ULL;
    return (workers < items) ? workers : items;
}

// the workers that one run of items takes at max, the one that calls among them
#define ENTRY_EACH_WORKERS_MAX 256u

// what the workers of one run of items share: the counter of the next item, the mark of a failure, and the work
struct EntryEach
{
    embed_atomic_u64 next;
    embed_atomic_u32 failed;
    unsigned long long items;
    EngineEachWork work;
    void *context;
};

// one worker: the next item from the counter, until the items run out or one failed
static void entry_each_run(void *supplied)
{
    // Explicit cast takes the pointer that the run gave as the state that its workers share
    EntryEach *const each = (EntryEach *)supplied;
    for (;;)
    {
        const EmbedAtomicU64FetchAddCfg take = {&each->next, 1ULL};
        const unsigned long long item = embed_atomic_u64_fetch_add(&take);
        const EmbedAtomicU32LoadCfg seen = {&each->failed};
        if ((item >= each->items) || (embed_atomic_u32_load(&seen) != 0u))
        {
            return;
        }
        if (each->work(each->context, item) == 0)
        {
            const EmbedAtomicU32StoreCfg mark = {&each->failed, 1u};
            embed_atomic_u32_store(&mark);
        }
    }
}

// every item on the workers, each taking the next item from one counter until none is left or one failed. A worker
// the system refuses leaves its share to the ones that run
static int entry_each(unsigned long long items, EngineEachWork work, void *context)
{
    EntryEach each = {EMBED_ATOMIC_INIT(0ULL), EMBED_ATOMIC_INIT(0u), items, work, context};
    const unsigned long long asked = entry_each_workers(items);
    const unsigned long long workers = (asked < ENTRY_EACH_WORKERS_MAX) ? asked : ENTRY_EACH_WORKERS_MAX;
    PlatformWorker started[ENTRY_EACH_WORKERS_MAX];
    for (unsigned long long worker = 1ULL; worker < workers; worker += 1ULL)
    {
        (void)platform_worker_started(&started[worker], entry_each_run, &each);
    }
    entry_each_run(&each);
    for (unsigned long long worker = 1ULL; worker < workers; worker += 1ULL)
    {
        platform_worker_joined(&started[worker]);
    }
    const EmbedAtomicU32LoadCfg seen = {&each.failed};
    return (embed_atomic_u32_load(&seen) == 0u) ? 1 : 0;
}

static long long entry_blosc_decode(const EngineBytesRequest *request)
{
    BloscDecodeRequest blosc;
    blosc.bytes = *request;
    blosc.decode = s_ingest_tools.decode;
    return blosc_decode(&blosc);
}

// Spans of one archive read ahead, each on a thread of its own: two are buffered, the one being read from and the one
// after it. Only one reader runs at a time, since two readers on one disk seek against each other: a span requested
// while the other's reader runs waits, and starts the moment the thread that requested for both joins that reader.
// Every read the tools make comes from that thread: the spans need no lock. The process's last act on them joins a
// reader still running
#define ENTRY_SPAN_COUNT 2u

struct EntrySpan
{
    PlatformWorker reader;
    char path[ENTRY_PATH_CAPACITY];
    unsigned long long first;
    unsigned long long length;
    unsigned char *bytes;
    long long read;
    unsigned long long requested_order;
    int waiting;

    ~EntrySpan()
    {
        platform_worker_joined(&reader);
        platform_cell_released(bytes, platform_place_g_program_owner);
    }
};

static EntrySpan s_spans[ENTRY_SPAN_COUNT];

static unsigned long long s_span_requests;

static void entry_span_release(EntrySpan *span)
{
    platform_worker_joined(&span->reader);
    platform_cell_released(span->bytes, platform_place_g_program_owner);
    span->bytes = NULL;
    span->path[0] = '\0';
    span->first = 0ULL;
    span->length = 0ULL;
    span->read = 0LL;
    span->requested_order = 0ULL;
    span->waiting = 0;
}

// the reader of a span: the span read from its file into its bytes
static void entry_span_reader(void *supplied)
{
    // Explicit cast takes the pointer that the start gave as the span that the reader fills
    EntrySpan *const span = (EntrySpan *)supplied;
    const EngineFileRange range = {span->path, span->first, span->length, span->bytes};
    span->read = stack_file_read(&range);
}

// the reader of the span started. Where the system refuses the thread, the span reads on the thread that calls
static void entry_span_start(EntrySpan *span)
{
    span->waiting = 0;
    if (platform_worker_started(&span->reader, entry_span_reader, span) == 0)
    {
        entry_span_reader(span);
    }
}

// a range inside a buffered span is copied from it once its reader is done, and a span waiting behind that reader
// starts; any other range is read from the file
static long long entry_span_read(const EngineFileRange *range)
{
    EntrySpan *span = NULL;
    for (unsigned int at = 0u; (span == NULL) && (at < ENTRY_SPAN_COUNT); at += 1u)
    {
        EntrySpan *const candidate = &s_spans[at];
        const int inside =
            (range != NULL) && (range->path != NULL) && (candidate->bytes != NULL) &&
            (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = range->path, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                        .other = candidate->path, .ci = EMBED_FALSE)) &&
            (range->offset >= candidate->first) && ((range->offset - candidate->first) <= candidate->length) &&
            (range->bytes <= (candidate->length - (range->offset - candidate->first)));
        span = inside ? candidate : NULL;
    }
    if (span == NULL)
    {
        return stack_file_read(range);
    }
    if (span->waiting != 0)
    {
        entry_span_start(span);
    }
    if (span->reader.running != 0)
    {
        platform_worker_joined(&span->reader);
    }
    for (unsigned int at = 0u; at < ENTRY_SPAN_COUNT; at += 1u)
    {
        if (s_spans[at].waiting != 0)
        {
            entry_span_start(&s_spans[at]);
        }
    }
    // a span the reader did not read whole is let go, and the range is read from the file
    if (span->read != (long long)span->length)
    {
        entry_span_release(span);
        return stack_file_read(range);
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = range->out, .src = span->bytes + (range->offset - span->first),
               .bytes = (size_t)range->bytes);
    // a range's bytes are bounded to the span's length above, which a long long represents
    return (long long)range->bytes;
}

extern "C" long engine_source_prefetch(const char *source, const char *sample, EngineError *error)
{
    if (error == NULL)
    {
        return ENGINE_ERROR;
    }
    // the span requested longest ago is let go for this one
    EntrySpan *at = &s_spans[0];
    for (unsigned int other = 1u; other < ENTRY_SPAN_COUNT; other += 1u)
    {
        at = (s_spans[other].requested_order < at->requested_order) ? &s_spans[other] : at;
    }
    entry_span_release(at);
    const EngineIngestTools *const tools = entry_ingest_tools();
    const ZipArchive *const archive = ((source != NULL) && (sample != NULL) && entry_is_file(source))
                                          ? zip_archive_cached(tools, source, error)
                                          : NULL;
    unsigned long long first = 0ULL;
    unsigned long long count = 0ULL;
    if ((archive == NULL) || !zip_folder_find(archive, sample, &first, &count) || (count == 0ULL) ||
        (EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = source, .cap = PLATFORM_TEXT_TO_TERMINATOR) >=
         sizeof(at->path)))
    {
        return ENGINE_ERROR;
    }
    // the members' span, from the first local header to past the last member's data: its local header is 30 bytes,
    // and its name and extra field at max 65,535 bytes each
    unsigned long long begin = ~0ULL;
    unsigned long long end = 0ULL;
    for (unsigned long long at = first; at < (first + count); at += 1ULL)
    {
        ZipEntry entry;
        if (!zip_entry_at(archive, at, &entry, error))
        {
            return ENGINE_ERROR;
        }
        const unsigned long long past = entry.local_offset + 30ULL + (2ULL * 65535ULL) + entry.compressed;
        begin = (entry.local_offset < begin) ? entry.local_offset : begin;
        end = (past > end) ? past : end;
    }
    end = (end < archive->file_bytes) ? end : archive->file_bytes;
    at->bytes = (end > begin)
                    ? (unsigned char *)platform_cell_taken((size_t)(end - begin), platform_place_g_program_owner, 0)
                    : NULL;
    if (!ENGINE_CHECK(at->bytes != NULL, &at->bytes, error, ENGINE_ERROR_RESOURCE))
    {
        return ENGINE_ERROR;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = at->path, .src = source,
               .bytes =
                   EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = source, .cap = PLATFORM_TEXT_TO_TERMINATOR) + 1u);
    at->first = begin;
    at->length = end - begin;
    at->read = 0LL;
    s_span_requests += 1ULL;
    at->requested_order = s_span_requests;
    // it starts now where no other reader runs, and otherwise once the thread that requested joins the one that does
    int running = 0;
    for (unsigned int other = 0u; other < ENTRY_SPAN_COUNT; other += 1u)
    {
        running |= ((&s_spans[other] != at) && (s_spans[other].reader.running != 0)) ? 1 : 0;
    }
    if (running != 0)
    {
        at->waiting = 1;
    }
    else
    {
        entry_span_start(at);
    }
    return 0L;
}

const EngineIngestTools *entry_ingest_tools(void)
{
    EngineIngestTools *const tools = &s_ingest_tools;
    tools->decode[ENGINE_CODEC_RAW] = entry_raw_decode;
    tools->decode[ENGINE_CODEC_ZSTD] = zstd_decode;
    tools->decode[ENGINE_CODEC_ZLIB] = inflate_zlib_decode;
    tools->decode[ENGINE_CODEC_GZIP] = inflate_gzip_decode;
    tools->decode[ENGINE_CODEC_DEFLATE] = inflate_raw_decode;
    tools->decode[ENGINE_CODEC_LZ4] = lz4_block_decode;
    tools->decode[ENGINE_CODEC_LZ4_FRAME] = lz4_frame_decode;
    tools->decode[ENGINE_CODEC_SNAPPY] = snappy_decode;
    tools->decode[ENGINE_CODEC_BLOSCLZ] = blosclz_decode;
    tools->decode[ENGINE_CODEC_BLOSC] = entry_blosc_decode;
    tools->decode[ENGINE_CODEC_LZ4_SIZED] = lz4_numcodecs_decode;
    tools->read = entry_span_read;
    tools->size = stack_file_size;
    tools->each = entry_each;
    return tools;
}

int entry_exists(const char *path)
{
#ifdef _WIN32
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    struct stat status;
    return stat(path, &status) == 0;
#endif
}

int entry_is_file(const char *path)
{
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesA(path);
    return (attributes != INVALID_FILE_ATTRIBUTES) && ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0u);
#else
    struct stat status;
    return (stat(path, &status) == 0) && S_ISREG(status.st_mode);
#endif
}

int entry_joined(char *out, size_t capacity, const char *root, const char *leaf)
{
    VerbaLinea formed_line_2 = mmgr_line_over(out, capacity);
    mmgr_line_put(&formed_line_2, root);
    mmgr_line_put(&formed_line_2, "/");
    mmgr_line_put(&formed_line_2, leaf);
    (void)mmgr_line_finish(&formed_line_2);
    const int written = (int)formed_line_2.at;
    return (written > 0) && ((size_t)written < capacity);
}

void entry_json_release(EntryJson *json)
{
    platform_cell_released(json->text, platform_place_g_program_owner);
    platform_cell_released(json->tokens, platform_place_g_program_owner);
    EMBED_CALL(memor.set, MemoriaCfg, .dst = json, .bytes = sizeof(*json), .val = 0);
}

static int entry_file_read_all(const char *path, char **bytes, size_t *length)
{
    const long long size = stack_file_size(path);
    if (size < 0LL)
    {
        return 0;
    }
    char *const text = (char *)platform_cell_taken((size_t)size + 1u, platform_place_g_program_owner, 0);
    EngineFileRange range;
    range.path = path;
    range.offset = 0ULL;
    range.bytes = (unsigned long long)size;
    range.out = (unsigned char *)text;
    if ((text == NULL) || (stack_file_read(&range) != size))
    {
        platform_cell_released(text, platform_place_g_program_owner);
        return 0;
    }
    text[size] = '\0';
    *bytes = text;
    *length = (size_t)size;
    return 1;
}

int entry_json_load(const char *path, EntryJson *json)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = json, .bytes = sizeof(*json), .val = 0);
    if (entry_file_read_all(path, &json->text, &json->length) == 0)
    {
        return 0;
    }
    const unsigned int capacity = (unsigned int)(json->length / 2u) + 4u;
    json->tokens =
        (CfgJsonToken *)platform_cell_taken((size_t)capacity * sizeof(CfgJsonToken), platform_place_g_program_owner, 0);
    CfgJsonParse parse;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &parse, .bytes = sizeof(parse), .val = 0);
    const int ok = (json->tokens != NULL) &&
                   cfg_json_parse_metadata(json->text, json->length, json->tokens, capacity, &parse) &&
                   (json->tokens[0].type == CFG_JSON_OBJECT);
    if (ok == 0)
    {
        {
            EngineNote engine_note;
            engine_note_begun(&engine_note);
            mmgr_line_put(&engine_note.line, "  ");
            mmgr_line_put(&engine_note.line, path);
            mmgr_line_put(&engine_note.line, ": ");
            mmgr_line_put(&engine_note.line, (parse.reason != NULL) ? parse.reason : "not one JSON object");
            mmgr_line_ch(&engine_note.line, '\n');
            engine_note_written(&engine_note);
        }
        entry_json_release(json);
        return 0;
    }
    json->count = parse.tokens;
    return 1;
}

unsigned int entry_json_at(const EntryJson *json, unsigned int object, const char *name)
{
    const int ok = (object < json->count) && (json->tokens[object].type == CFG_JSON_OBJECT);
    return (ok != 0) ? cfg_json_member(json->text, json->tokens, object, name) : 0u;
}

unsigned int entry_json_element(const EntryJson *json, unsigned int array, unsigned int index)
{
    if ((array == 0u) || (array >= json->count) || (json->tokens[array].type != CFG_JSON_ARRAY) ||
        (index >= json->tokens[array].count))
    {
        return 0u;
    }
    unsigned int at = array + 1u;
    for (unsigned int step = 0u; step < index; step += 1u)
    {
        at = json->tokens[at].next;
    }
    return at;
}

int entry_json_text(const EntryJson *json, unsigned int token, char *out, size_t capacity)
{
    return (token != 0u) && cfg_json_string(json->text, &json->tokens[token], out, capacity);
}

int entry_json_list(const EntryJson *json, unsigned int array, unsigned long long *values, unsigned int *count,
                    unsigned int capacity)
{
    if ((array == 0u) || (json->tokens[array].type != CFG_JSON_ARRAY) || (json->tokens[array].count > capacity))
    {
        return 0;
    }
    *count = json->tokens[array].count;
    int ok = 1;
    for (unsigned int at = 0u; ok && (at < *count); at += 1u)
    {
        ok = cfg_json_unsigned(json->text, &json->tokens[entry_json_element(json, array, at)], &values[at]);
    }
    return ok;
}

int entry_element_named(const char *name, EngineArrayExtent *extent, unsigned int *big_endian)
{
    static const char *const NAMES[10] = {"uint8", "uint16", "uint32", "uint64",  "int8",
                                          "int16", "int32",  "int64",  "float32", "float64"};
    static const unsigned int BYTES[10] = {1u, 2u, 4u, 8u, 1u, 2u, 4u, 8u, 4u, 8u};
    static const EngineElementType TYPES[10] = {
        ENGINE_ELEMENT_UNSIGNED, ENGINE_ELEMENT_UNSIGNED, ENGINE_ELEMENT_UNSIGNED, ENGINE_ELEMENT_UNSIGNED,
        ENGINE_ELEMENT_SIGNED,   ENGINE_ELEMENT_SIGNED,   ENGINE_ELEMENT_SIGNED,   ENGINE_ELEMENT_SIGNED,
        ENGINE_ELEMENT_FLOAT,    ENGINE_ELEMENT_FLOAT};
    for (unsigned int at = 0u; at < 10u; at += 1u)
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = NAMES[at],
                       .ci = EMBED_FALSE))
        {
            extent->element_bytes = BYTES[at];
            extent->element_type = TYPES[at];
            return 1;
        }
    }
    const size_t length = EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR);
    const int ordered =
        (length >= 3u) && ((name[0] == '<') || (name[0] == '>') || (name[0] == '|') || (name[0] == '='));
    const char type = ordered ? name[1] : '\0';
    const unsigned long width =
        ordered ? EMBED_CALL(cellul.to_ulong_in, TransfiguroCfg, .src = &name[2], .end = NULL, .base = 10) : 0UL;
    if ((ordered == 0) || ((type != 'u') && (type != 'i') && (type != 'f')) ||
        ((width != 1UL) && (width != 2UL) && (width != 4UL) && (width != 8UL)))
    {
        return 0;
    }
    extent->element_bytes = (unsigned int)width;
    extent->element_type = (type == 'u')   ? ENGINE_ELEMENT_UNSIGNED
                           : (type == 'i') ? ENGINE_ELEMENT_SIGNED
                                           : ENGINE_ELEMENT_FLOAT;
    *big_endian = (name[0] == '>') ? 1u : 0u;
    return 1;
}

int entry_codec_named(const char *name, EngineCodec *codec)
{
    static const char *const NAMES[6] = {"blosc", "zstd", "gzip", "zlib", "lz4", "snappy"};
    static const EngineCodec CODECS[6] = {ENGINE_CODEC_BLOSC, ENGINE_CODEC_ZSTD,      ENGINE_CODEC_GZIP,
                                          ENGINE_CODEC_ZLIB,  ENGINE_CODEC_LZ4_SIZED, ENGINE_CODEC_SNAPPY};
    for (unsigned int at = 0u; at < 6u; at += 1u)
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = name, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = NAMES[at],
                       .ci = EMBED_FALSE))
        {
            *codec = CODECS[at];
            return 1;
        }
    }
    return 0;
}

char entry_axis_named(const char *name)
{
    static const char *const NAMES[7] = {"t", "time", "c", "channel", "z", "y", "x"};
    static const char AXES[7] = {'t', 't', 'c', 'c', 'z', 'y', 'x'};
    char lowered[16];
    size_t length = 0u;
    while ((name[length] != '\0') && (length + 1u < sizeof(lowered)))
    {
        lowered[length] = (char)(((name[length] >= 'A') && (name[length] <= 'Z')) ? (name[length] + 32) : name[length]);
        length += 1u;
    }
    lowered[length] = '\0';
    for (unsigned int at = 0u; at < 7u; at += 1u)
    {
        if (EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = lowered, .cap = PLATFORM_TEXT_TO_TERMINATOR,
                       .other = NAMES[at], .ci = EMBED_FALSE))
        {
            return AXES[at];
        }
    }
    return '\0';
}

int entry_ome(const EntryJson *json, unsigned int attributes, EntryOme *ome)
{
    EMBED_CALL(memor.set, MemoriaCfg, .dst = ome, .bytes = sizeof(*ome), .val = 0);
    unsigned int scales = entry_json_at(json, attributes, "multiscales");
    scales = (scales != 0u) ? scales : entry_json_at(json, entry_json_at(json, attributes, "ome"), "multiscales");
    const unsigned int first = entry_json_element(json, scales, 0u);
    const unsigned int datasets = entry_json_at(json, first, "datasets");
    if (entry_json_text(json, entry_json_at(json, entry_json_element(json, datasets, 0u), "path"), ome->path,
                        sizeof(ome->path)) == 0)
    {
        return 0;
    }
    const unsigned int axes = entry_json_at(json, first, "axes");
    const unsigned int named =
        ((axes != 0u) && (json->tokens[axes].type == CFG_JSON_ARRAY)) ? json->tokens[axes].count : 0u;
    ome->rank = (named <= ENGINE_ARRAY_RANK) ? named : 0u;
    for (unsigned int at = 0u; at < ome->rank; at += 1u)
    {
        const unsigned int axis = entry_json_element(json, axes, at);
        const unsigned int name_token =
            (json->tokens[axis].type == CFG_JSON_OBJECT) ? entry_json_at(json, axis, "name") : axis;
        char name[32];
        ome->axes[at] = entry_json_text(json, name_token, name, sizeof(name)) ? entry_axis_named(name) : '\0';
    }
    return 1;
}
