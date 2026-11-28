// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
/**
 * @file btc_miner.cpp
 * @brief The stratum client: builds real headers, mines them, and submits what it actually found.
 * @author dstroy0 (Douglas Quigg) <dquigg123@gmail.com>
 *
 * @note The byte orders here come from a live job from solo.ckpool.org, and not from memory.
 *       A client must assemble a real header and hash it. It must compute the merkle root, keep the
 *       extranonce from the subscribe reply, and submit the nonce in its own parameter.
 * @note An independent double hash checks every share against the share target before the client
 *       sends it. The client never submits a share that it cannot verify. Thus a defect here costs
 *       shares, and the pool gets no garbage.
 * @note The threads hold no lock. The network thread is the one writer of the work: it writes a new
 *       record beside the published one, publishes it, and drains the turnstiles of the workers before
 *       it writes that record again. A worker reads the work inside a pass. The shares go from the
 *       workers to the network thread through a ticket ring. The counters are atomic words.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "ascii_persona_bitorum/ascii_persona_bitorum.h"
#include "cellularum_laboro/cellularum_laboro.h"
#include "memoria_operor/memoria_operor.h"
#include <winsock2.h>
#include <ws2tcpip.h>

#include "../../../../includes/platform/platform.h"
#include "cuda_miner.h"
#include "embed_atomic.h"
#include "embed_ticket_ring.h"
#include "embed_turnstile.h"
#include "json_value.h"
#include "sha256_core.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "Ws2_32.lib")

namespace
{

/** @brief Where to mine. Overridable on the command line. */
const char *const DEFAULT_POOL_HOSTNAME = "solo.ckpool.org";
const char *const DEFAULT_POOL_PORT = "3333";

/** @brief The bytes of an identity, a job's id and a short hex field, each terminated. */
#define MINER_IDENTITY_BYTES 256u
#define MINER_JOB_ID_BYTES 128u
#define MINER_WORD_HEX_BYTES 16u

/** @brief The bytes of a hash as hex, terminated, and of a coinbase part as hex, terminated. */
#define MINER_HASH_HEX_BYTES 65u
#define MINER_COINBASE_HEX_BYTES 8192u

/** @brief The entries of a merkle branch at max, and the bytes of an extranonce as hex, terminated. */
#define MINER_BRANCH_MAX 64u
#define MINER_EXTRANONCE_HEX_BYTES 64u

/** @brief The bytes of a coinbase: both parts and both extranonces. */
#define MINER_COINBASE_BYTES (MINER_COINBASE_HEX_BYTES + MINER_EXTRANONCE_HEX_BYTES)

/** @brief The workers the client starts at max. */
#define MINER_WORKERS_MAX 256u

/** @brief The bytes of the pool's text that the client holds before a line ends, and of one message it sends. */
#define MINER_INBOUND_BYTES 65536u
#define MINER_PAYLOAD_BYTES 1024u

/** @brief The slots of the ticket ring of the shares: 2^6, far above the shares of one turn of the network thread. */
#define MINER_SHARE_SHIFT 6u
#define MINER_SHARE_SLOTS (1u << MINER_SHARE_SHIFT)

/**
 * @brief An example address that belongs to nobody here.
 *
 * @note This is the example address from the BIP-173 test vectors, reprinted in a great many
 *       tutorials. Anything it earns goes to whoever holds that key. The client keeps it only to
 *       refuse it by name as a payout address.
 */
const char *const KNOWN_EXAMPLE_ADDRESS = "bc1qxy2kgdygjrsqtzq2n0yrf2493p83kkfjhx0wlh";

/** @brief Who gets paid. Set from the command line or a config file; never defaulted. */
char g_payout_address[MINER_IDENTITY_BYTES];

/**
 * @brief The name stratum authorizes and submits under, where it is not the payout address.
 *
 * @note Two types of pool exist, and they key on different things. ckpool and Ocean take a
 *       Bitcoin address as the worker name. They pay to it directly. There, the address is the identity.
 *       Braiins and the other account pools take an account worker name. They keep the payout
 *       destination in an account setting. There, the address is not the identity, and the client
 *       must not send it.
 * @note The password field stays "x" either way. The stratum password of a pool is not an account
 *       password. Never put an account password on this wire.
 */
char g_stratum_user[MINER_IDENTITY_BYTES];

/** @brief What stratum should authorize and submit as. */
const char *stratum_identity()
{
    return (g_stratum_user[0] == '\0') ? g_payout_address : g_stratum_user;
}

/**
 * @brief Connect and parse, but never authorize, mine or submit.
 *
 * @note This proves the job path before a payout address exists. It exercises everything upstream
 *       of the submit: the subscribe reply, the notify parse, the coinbase, the merkle fold and the
 *       header. It sends nothing that identifies anyone.
 */
bool g_dry_run = false;

/** @brief Nonces one worker claims per trip to the shared counter. */
const uint32_t NONCE_CHUNK = 1u << 21;

// ---------------------------------------------------------------------------------------------
// Texts, bytes and hex
// ---------------------------------------------------------------------------------------------

/** @brief True where two terminated texts are equal. */
bool text_same(const char *one, const char *other)
{
    return EMBED_CALL(cellul.eq, CatenaFinitaCfg, .src = one, .cap = PLATFORM_TEXT_TO_TERMINATOR, .other = other,
                      .ci = EMBED_FALSE);
}

/** @brief The bytes of a terminated text. */
size_t text_length(const char *text)
{
    return EMBED_CALL(cellul.len, CatenaFinitaCfg, .src = text, .cap = PLATFORM_TEXT_TO_TERMINATOR);
}

/**
 * @brief A terminated text copied into a field of `capacity` bytes.
 *
 * @return True where it fits with its terminator, and false where it does not, the field then empty.
 */
bool text_set(char *field, size_t capacity, const char *text)
{
    const size_t length = text_length(text);
    if (length >= capacity)
    {
        field[0] = '\0';
        return false;
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = field, .src = text, .bytes = length + 1u);
    return true;
}

/**
 * @brief Converts a hex text to bytes.
 *
 * @param[in]  hex      Characters to convert, an even count [BORROWS].
 * @param[out] bytes    Where the bytes go [BORROWS].
 * @param[in]  capacity Bytes at bytes.
 * @return              The bytes that they encode, or 0 where the input has a bad form or passes the capacity.
 */
size_t bytes_from_hex(const char *hex, uint8_t *bytes, size_t capacity)
{
    const size_t length = text_length(hex);
    if (((length % 2u) != 0u) || ((length / 2u) > capacity))
    {
        return 0u;
    }
    for (size_t at = 0u; at < length; at += 2u)
    {
        unsigned value = 0u;
        for (size_t nibble = 0u; nibble < 2u; nibble += 1u)
        {
            // Explicit casts read the byte as its code point, and the folded letter as a count from 'a'
            const uint8_t character = (uint8_t)hex[at + nibble];
            if (!EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_HEX, .byte = character))
            {
                return 0u;
            }
            const unsigned digit = EMBED_CALL(ascii.in, AsciiCfg, .type = MMGR_ASCII_NUM, .byte = character)
                                       ? (unsigned)(character - '0')
                                       : (unsigned)(EMBED_CALL(ascii.lower, AsciiCfg, .byte = character) - 'a') + 10u;
            value = (value << 4) | digit;
        }
        bytes[at / 2u] = (uint8_t)value;
    }
    return length / 2u;
}

/**
 * @brief Renders bytes as hex in the order they sit in memory.
 *
 * @param[in]  bytes    Bytes to render [BORROWS].
 * @param[in]  count    How many.
 * @param[out] rendered 2 count + 1 bytes for the hex, terminated [BORROWS].
 */
void hex_from_bytes(const uint8_t *bytes, size_t count, char *rendered)
{
    static const char digits[] = "0123456789abcdef";
    for (size_t at = 0u; at < count; at += 1u)
    {
        rendered[2u * at] = digits[bytes[at] >> 4];
        rendered[(2u * at) + 1u] = digits[bytes[at] & 0x0Fu];
    }
    rendered[2u * count] = '\0';
}

/**
 * @brief Renders a digest the way a block explorer prints it, most significant byte first.
 *
 * @param[in]  digest   Thirty-two bytes as SHA-256 emitted them [BORROWS].
 * @param[out] rendered MINER_HASH_HEX_BYTES bytes for the reversed hex [BORROWS].
 */
void block_hash_from_digest(const uint8_t *digest, char *rendered)
{
    uint8_t reversed[32];

    for (size_t at = 0u; at < 32u; at += 1u)
    {
        reversed[at] = digest[31u - at];
    }
    hex_from_bytes(reversed, sizeof(reversed), rendered);
}

/**
 * @brief Renders a value as eight hex characters, most significant first.
 *
 * @param[in]  value    Number to render.
 * @param[out] rendered Nine bytes for the hex, terminated [BORROWS].
 */
void hex_from_word(uint32_t value, char *rendered)
{
    snprintf(rendered, 9u, "%08x", value);
}

/** @brief A hex field read as a word, as strtoul reads it. */
uint32_t word_from_hex(const char *hex)
{
    return (uint32_t)strtoul(hex, NULL, 16);
}

/**
 * @brief Writes a word into a header field, little-endian, as the header stores its numbers.
 *
 * @param[out] field Four bytes to write [BORROWS].
 * @param[in]  value The number to store.
 */
void write_header_field(uint8_t *field, uint32_t value)
{
    field[0] = (uint8_t)value;
    field[1] = (uint8_t)(value >> 8);
    field[2] = (uint8_t)(value >> 16);
    field[3] = (uint8_t)(value >> 24);
}

/**
 * @brief Reads four header bytes as one big-endian word, the order SHA-256 reads its message in.
 *
 * @param[in] bytes Four bytes to read [BORROWS].
 * @return          The schedule word that they encode.
 */
uint32_t schedule_word_from_header(const uint8_t *bytes)
{
    return ((uint32_t)bytes[0] << 24) | ((uint32_t)bytes[1] << 16) | ((uint32_t)bytes[2] << 8) | (uint32_t)bytes[3];
}

// ---------------------------------------------------------------------------------------------
// The counters, the work and the shares
// ---------------------------------------------------------------------------------------------

/** @brief A counter read, with acquire ordering. */
uint64_t counter_read(const embed_atomic_u64 *word)
{
    return EMBED_CALL(embed_atomic_u64_load, EmbedAtomicU64LoadCfg, .word = word);
}

/** @brief A counter added to, and its value before the add. */
uint64_t counter_added(embed_atomic_u64 *word, uint64_t value)
{
    return EMBED_CALL(embed_atomic_u64_fetch_add, EmbedAtomicU64FetchAddCfg, .word = word, .value = value);
}

/** @brief A counter set, with release ordering. */
void counter_set(embed_atomic_u64 *word, uint64_t value)
{
    EMBED_CALL(embed_atomic_u64_store, EmbedAtomicU64StoreCfg, .word = word, .value = value);
}

/** @brief One mining.notify, kept as the pool sent it, each field a text of a fixed bound. */
struct MiningJob
{
    char job_id[MINER_JOB_ID_BYTES];
    char prevhash_hex[MINER_HASH_HEX_BYTES];
    char coinbase_head_hex[MINER_COINBASE_HEX_BYTES];
    char coinbase_tail_hex[MINER_COINBASE_HEX_BYTES];
    char merkle_branch_hex[MINER_BRANCH_MAX][MINER_HASH_HEX_BYTES];
    unsigned int merkle_branch_count;
    char version_hex[MINER_WORD_HEX_BYTES];
    char nbits_hex[MINER_WORD_HEX_BYTES];
    char ntime_hex[MINER_WORD_HEX_BYTES];
    bool clean_jobs;
    bool valid;
};

/** @brief The work every worker reads: the job, the extranonce the pool assigned, and the share difficulty. */
struct MinerWork
{
    MiningJob job;
    char extranonce1_hex[MINER_EXTRANONCE_HEX_BYTES];
    size_t extranonce2_size;
    double difficulty;
    // The `version-rolling` mask that the pool granted at mining.configure, zero where it granted none.
    // Set once during negotiation and read when a worker rebuilds.
    uint32_t version_mask;
};

/**
 * @brief The two records of the work. The network thread writes the one that is not published, publishes
 *        it, and drains the turnstiles, and so no worker holds the other when it writes that one again.
 */
MinerWork g_work[2];

/** @brief The record the network thread published last. The network thread alone reads and writes it. */
unsigned int g_work_at = 0u;

/** @brief The address of the published record. */
embed_atomic_uptr g_work_published;

/** @brief The turnstiles a worker passes through to read the published record. Zeros at rest. */
EmbedTurnstiles g_work_turnstiles;

/** @brief What the whole client counts. A change of the generation tells the workers to read the work again. */
struct MinerState
{
    // Rises whenever the work changes. Workers compare against it and drop their current work.
    embed_atomic_u64 generation;
    embed_atomic_u64 extranonce2_counter;
    // Which rolled version the current work uses, below version_space(version_mask). When the nonce
    // space runs out, the version rolls before the extranonce2. A version roll only changes the
    // midstate of the first block. An extranonce2 roll rebuilds the coinbase and the merkle root.
    embed_atomic_u64 version_index;
    embed_atomic_u64 nonce_cursor;
    embed_atomic_u64 hashes_done;
    embed_atomic_u64 anchor_survivors;
    embed_atomic_u64 running;

    // Read through Sha256ScanRequest, which needs a volatile int instead of an atomic.
    volatile int abandon;
};

MinerState g_state;

/**
 * @brief The published work copied into the worker's own record, inside a pass through the turnstiles.
 *
 * @param[out] work    The worker's record [BORROWS].
 * @param[in]  address The worker's address, an immutable place that selects its lane [BORROWS].
 */
void work_read(MinerWork *work, const void *address)
{
    // Explicit casts carry the worker's address as the pass's entry and the published word as the record
    const unsigned int place = EMBED_CALL(embed_turnstile_enter, EmbedTurnstileEnterCfg, .set = &g_work_turnstiles,
                                          .address = (embed_atomic_uptr_value)(uintptr_t)address);
    const MinerWork *const published = (const MinerWork *)(uintptr_t)EMBED_CALL(
        embed_atomic_uptr_load, EmbedAtomicUptrLoadCfg, .word = &g_work_published);
    *work = *published;
    EMBED_CALL(embed_turnstile_leave, EmbedTurnstileLeaveCfg, .set = &g_work_turnstiles, .place = place);
}

/** @brief On the network thread: the record that is not published, a copy of the one that is, to change. */
MinerWork *work_next()
{
    MinerWork *const next = &g_work[1u - g_work_at];
    *next = g_work[g_work_at];
    return next;
}

/** @brief On the network thread: the record work_next gave published, and every pass on the other waited out. */
void work_published()
{
    // Explicit cast carries the record's address as the published word
    EMBED_CALL(embed_atomic_uptr_store, EmbedAtomicUptrStoreCfg, .word = &g_work_published,
               .value = (embed_atomic_uptr_value)(uintptr_t)&g_work[1u - g_work_at]);
    EMBED_CALL(embed_turnstiles_drain, EmbedTurnstilesDrainCfg, .set = &g_work_turnstiles);
    g_work_at = 1u - g_work_at;
}

/** @brief On the network thread: the work as it published it last. */
const MinerWork *work_current()
{
    return &g_work[g_work_at];
}

/** @brief A share that a worker found and verified. It waits for the network thread to send it. */
struct FoundShare
{
    char job_id[MINER_JOB_ID_BYTES];
    char extranonce2_hex[MINER_EXTRANONCE_HEX_BYTES];
    char ntime_hex[MINER_WORD_HEX_BYTES];
    uint32_t nonce;
    uint32_t version;
    char block_hash[MINER_HASH_HEX_BYTES];
    bool meets_block_target;
};

/** @brief The ticket ring of the shares: its slots, the share of each slot, and its turnstiles, zeros at rest. */
EmbedTicketRingSlot g_share_slots[MINER_SHARE_SLOTS];
EmbedTicketRing g_share_ring;
FoundShare g_share_store[MINER_SHARE_SLOTS];
EmbedTurnstiles g_share_turnstiles;

/** @brief The next ticket of a share. A worker takes one with a fetch-add. */
embed_atomic_u64 g_share_ticket;

/** @brief The epoch of a stamp, from the clock: it moves every 2^32 ns, far past the life of a share's stamp. */
embed_word share_epoch()
{
    return (embed_word)(platform_clock_ns() >> 32u);
}

/**
 * @brief A share put in the ring for the network thread. A slot that the network thread has not given
 *        back yet waits, so that no ticket is left with no share.
 *
 * @param[in] share   The share [BORROWS].
 * @param[in] address The worker's address, an immutable place that selects its lane [BORROWS].
 */
void share_posted(const FoundShare *share, const void *address)
{
    const embed_word ticket = (embed_word)counter_added(&g_share_ticket, 1u);
    const embed_word stamp =
        EMBED_CALL(embed_ticket_stamp, EmbedTicketStampCfg, .epoch = share_epoch(), .ticket = ticket);
    // Explicit cast carries the worker's address as the pass's entry
    const embed_atomic_uptr_value entry = (embed_atomic_uptr_value)(uintptr_t)address;
    embed_word found = 0u;
    while (!EMBED_CALL(embed_ticket_ring_pass_grant, EmbedTicketRingPassGrantCfg, .set = &g_share_turnstiles,
                       .address = entry, .ring = &g_share_ring, .stamp = stamp, .found_stamp = &found))
    {
        if (counter_read(&g_state.running) == 0u)
        {
            return;
        }
        platform_paused(1u);
    }
    FoundShare *const held = &g_share_store[ticket & (MINER_SHARE_SLOTS - 1u)];
    *held = *share;
    (void)EMBED_CALL(embed_ticket_ring_pass_push, EmbedTicketRingPassPushCfg, .set = &g_share_turnstiles,
                     .address = entry, .ring = &g_share_ring, .stamp = stamp, .job = held, .found_stamp = &found);
}

/**
 * @brief On the network thread: the share of ticket `taken` popped where the worker pushed it, copied out,
 *        and its slot given back.
 *
 * @return True where a share was there, which then is at `share`.
 */
bool share_taken(uint64_t taken, FoundShare *share)
{
    EmbedTicketRingSlot *const slot = &g_share_slots[taken & (MINER_SHARE_SLOTS - 1u)];
    const embed_word word = EMBED_CALL(embed_atomic_word_load, EmbedAtomicWordLoadCfg, .word = &slot->stamp);
    const embed_word ticket_mask = (embed_word)((((embed_word)1u) << EMBED_TICKET_BITS) - 1u);
    const embed_word flags = EMBED_CALL(embed_ticket_flags_of_stamp, EmbedTicketFlagsOfStampCfg, .stamp = word);
    const embed_word pushed = (embed_word)(EMBED_TICKET_PUSHED_BIT >> EMBED_TICKET_FLAG_SHIFT);
    const embed_word popped = (embed_word)(EMBED_TICKET_POPPED_BIT >> EMBED_TICKET_FLAG_SHIFT);
    if ((EMBED_CALL(embed_ticket_of_stamp, EmbedTicketOfStampCfg, .stamp = word) !=
         ((embed_word)taken & ticket_mask)) ||
        ((flags & pushed) == 0u) || ((flags & popped) != 0u))
    {
        return false;
    }
    const embed_word stamp = EMBED_CALL(embed_ticket_fields_of_stamp, EmbedTicketFieldsOfStampCfg, .stamp = word);
    void *job = NULL;
    embed_word found = 0u;
    // Explicit cast carries the share's own slot as the pass's entry
    if (!EMBED_CALL(embed_ticket_ring_pass_pop, EmbedTicketRingPassPopCfg, .set = &g_share_turnstiles,
                    .address = (embed_atomic_uptr_value)(uintptr_t)slot, .ring = &g_share_ring, .stamp = stamp,
                    .job = &job, .found_stamp = &found))
    {
        return false;
    }
    *share = *(const FoundShare *)job;
    (void)EMBED_CALL(embed_ticket_ring_drain_release, EmbedTicketRingDrainReleaseCfg, .set = &g_share_turnstiles,
                     .ring = &g_share_ring, .stamp = stamp, .found_stamp = &found);
    return true;
}

// ---------------------------------------------------------------------------------------------
// Header assembly
// ---------------------------------------------------------------------------------------------

/**
 * @brief Folds a coinbase hash through a merkle branch to the root the header carries.
 *
 * @param[in]  coinbase      Serialized coinbase transaction [BORROWS].
 * @param[in]  coinbase_size Bytes of it.
 * @param[in]  job           The job whose branch entries fold in, as the pool sent them [BORROWS].
 * @param[out] merkle_root   Thirty-two bytes of root, in header order [BORROWS].
 * @note Every hash here stays in internal order. The branch entries concatenate as sent, and the
 *       root drops into the header as computed. Thus nothing in this function reverses anything.
 */
void merkle_root_from_coinbase(const uint8_t *coinbase, size_t coinbase_size, const MiningJob &job,
                               uint8_t *merkle_root)
{
    sha256_double_hash(coinbase, coinbase_size, merkle_root);

    for (unsigned int at = 0u; at < job.merkle_branch_count; at += 1u)
    {
        uint8_t entry[32];
        uint8_t joined[64];

        if (bytes_from_hex(job.merkle_branch_hex[at], entry, sizeof(entry)) != 32u)
        {
            continue;
        }
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = joined, .src = merkle_root, .bytes = 32u);
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = joined + 32u, .src = entry, .bytes = 32u);
        sha256_double_hash(joined, sizeof(joined), merkle_root);
    }
}

/**
 * @brief The version bits BIP320 lets a miner roll, requested in mining.configure.
 *
 * @note Bits 13 through 28. The pool answers with the subset that it permits, and this client rolls
 *       only what the pool grants. A grant of zero means no roll, and the job version stays as sent.
 */
const uint32_t VERSION_ROLLING_REQUEST_MASK = 0x1fffe000u;

/**
 * @brief Counts the one bits of a word. The version space is 2 to that count.
 *
 * @param[in] value The word to count.
 * @return          How many bits are one.
 */
uint32_t set_bit_count(uint32_t value)
{
    uint32_t count = 0u;
    while (value != 0u)
    {
        value &= value - 1u;
        count += 1u;
    }
    return count;
}

/**
 * @brief How many distinct versions the mask can express: 2 to the count of one bits.
 *
 * @param[in] mask The negotiated `version-rolling` mask.
 * @return         The count of distinct rolled versions, as a sixty-four bit value a full
 *                 sixteen-bit mask does not overflow the shift.
 */
uint64_t version_space(uint32_t mask)
{
    return 1ull << set_bit_count(mask);
}

/**
 * @brief Deposits the low bits of an index into the set-bit positions of the mask, low to high.
 *
 * @param[in] index Which rolled version to build, below version_space(mask).
 * @param[in] mask  The negotiated `version-rolling` mask.
 * @return          The mask bits set as `index` selects them, other bits zero.
 * @note This is the portable form of a bit deposit. The first one bit of the mask carries index
 *       bit zero, the second carries index bit one, and so on. Thus consecutive indices walk
 *       distinct versions, and do not skip or repeat one.
 */
uint32_t deposit_into_mask(uint64_t index, uint32_t mask)
{
    uint32_t result = 0u;
    uint32_t source_bit = 1u;
    for (uint32_t position = 0u; position < 32u; position += 1u)
    {
        const uint32_t mask_bit = 1u << position;
        if ((mask & mask_bit) != 0u)
        {
            if ((index & (uint64_t)source_bit) != 0u)
            {
                result |= mask_bit;
            }
            source_bit <<= 1;
        }
    }
    return result;
}

/**
 * @brief The header version for a given roll index: the job version with the masked bits replaced.
 *
 * @param[in] base_version The version the pool sent in the job.
 * @param[in] mask         The negotiated `version-rolling` mask, zero to disable.
 * @param[in] index        Which rolled version, below version_space(mask).
 * @return                 The version to write into the header.
 */
uint32_t version_from_index(uint32_t base_version, uint32_t mask, uint64_t index)
{
    return (base_version & ~mask) | deposit_into_mask(index, mask);
}

/**
 * @brief Gathers the masked bits of a version back into the roll index, the inverse of the deposit.
 *
 * @param[in] value The version whose masked bits to read.
 * @param[in] mask  The negotiated `version-rolling` mask.
 * @return          The index that deposit_into_mask would turn back into those bits.
 * @note Only the self-check needs the inverse. Where deposit then gather is the identity, the roll
 *       walks distinct versions. Thus the check runs the real function against its inverse.
 */
uint64_t gather_from_mask(uint32_t value, uint32_t mask)
{
    uint64_t result = 0u;
    uint32_t source_bit = 1u;
    for (uint32_t position = 0u; position < 32u; position += 1u)
    {
        const uint32_t mask_bit = 1u << position;
        if ((mask & mask_bit) != 0u)
        {
            if ((value & mask_bit) != 0u)
            {
                result |= (uint64_t)source_bit;
            }
            source_bit <<= 1;
        }
    }
    return result;
}

/**
 * @brief Runs the `version-rolling` helpers against their own contract, with no network or device.
 *
 * @return Zero where every check passed, else the count that failed.
 * @note This is the positive control before a rate or a share depends on the roll. The roll must
 *       walk distinct versions and touch only the granted bits. Where the pool grants no mask, the
 *       roll must not change the job version.
 */
int run_version_rolling_checks()
{
    int failed = 0;
    const uint32_t mask = VERSION_ROLLING_REQUEST_MASK;
    const uint32_t base = 0x20000000u;

    if (set_bit_count(mask) != 16u)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  [FAIL] the BIP320 request mask does not carry sixteen bits");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        failed += 1;
    }
    if (version_space(0u) != 1ull)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  [FAIL] a zero mask does not collapse to one version");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        failed += 1;
    }
    if (version_space(mask) != 65536ull)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  [FAIL] the sixteen-bit mask does not span 65536 versions");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        failed += 1;
    }
    if (version_from_index(base, 0u, 0u) != base)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  [FAIL] with no mask the version is not the job version");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        failed += 1;
    }
    if (version_from_index(base, mask, 1u) != (base | 0x00002000u))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  [FAIL] roll index one does not set the lowest granted bit");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        failed += 1;
    }
    if (deposit_into_mask(version_space(mask) - 1ull, mask) != mask)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  [FAIL] the last index does not fill the whole mask");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        failed += 1;
    }

    // Walk the whole version space: every roll must touch only masked bits, and deposit then gather
    // must be the identity, which is distinctness. The check first clears to zero any base bits
    // inside the mask, because the miner owns the masked region once the pool grants it.
    const uint32_t dirty_base = 0x2000e000u;
    bool bits_ok = true;
    bool bijective = true;
    for (uint64_t index = 0u; index < version_space(mask); index += 1u)
    {
        const uint32_t rolled = version_from_index(dirty_base, mask, index);
        if (((rolled ^ dirty_base) & ~mask) != 0u)
        {
            bits_ok = false;
        }
        if (gather_from_mask(rolled, mask) != index)
        {
            bijective = false;
        }
    }
    if (!bits_ok)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  [FAIL] a rolled version changed a bit outside the granted mask");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        failed += 1;
    }
    if (!bijective)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "  [FAIL] the roll repeats or skips a version over the mask space");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        failed += 1;
    }

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "  version-rolling self-check: ");
        mmgr_line_put(output_line, failed == 0 ? "every check held" : "CHECKS FAILED");
        mmgr_line_put(output_line, " (");
        mmgr_line_decimal(output_line, (unsigned long long)version_space(mask), 1u);
        mmgr_line_put(output_line, " versions per extranonce2)");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    return failed;
}

/**
 * @brief Builds the eighty header bytes for one job, one extranonce2, and one rolled version.
 *
 * @param[in]  job             The job to build from [BORROWS].
 * @param[in]  extranonce1_hex Extranonce the pool assigned at subscribe [BORROWS].
 * @param[in]  extranonce2_hex Extranonce this client chose [BORROWS].
 * @param[in]  version         The header version, already rolled where the pool grants it.
 * @param[out] header          Eighty bytes to fill [BORROWS].
 * @return                     True where every field decoded.
 * @note The prevhash arrives with each four byte word byte-reversed relative to the header. A
 *       reversal of each word recovers the header field. A reversal of the whole field yields a hash
 *       three zero characters short of the current target. A live job confirms this.
 */
bool build_header(const MiningJob &job, const char *extranonce1_hex, const char *extranonce2_hex, uint32_t version,
                  uint8_t *header)
{
    uint8_t prevhash[32];
    uint8_t coinbase[MINER_COINBASE_BYTES];

    if (bytes_from_hex(job.prevhash_hex, prevhash, sizeof(prevhash)) != 32u)
    {
        return false;
    }
    // the coinbase: its head, both extranonces and its tail, each put after the one before
    const size_t head = bytes_from_hex(job.coinbase_head_hex, coinbase, sizeof(coinbase));
    const size_t first = bytes_from_hex(extranonce1_hex, coinbase + head, sizeof(coinbase) - head);
    const size_t second = bytes_from_hex(extranonce2_hex, coinbase + head + first, sizeof(coinbase) - head - first);
    const size_t tail = bytes_from_hex(job.coinbase_tail_hex, coinbase + head + first + second,
                                       sizeof(coinbase) - head - first - second);
    if ((head == 0u) || (tail == 0u))
    {
        return false;
    }

    uint8_t merkle_root[32];
    merkle_root_from_coinbase(coinbase, head + first + second + tail, job, merkle_root);

    const uint32_t nbits = word_from_hex(job.nbits_hex);
    const uint32_t ntime = word_from_hex(job.ntime_hex);

    write_header_field(header, version);
    for (size_t word = 0u; word < 8u; word += 1u)
    {
        for (size_t byte = 0u; byte < 4u; byte += 1u)
        {
            header[4u + (word * 4u) + byte] = prevhash[(word * 4u) + (3u - byte)];
        }
    }
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = header + 36u, .src = merkle_root, .bytes = 32u);
    write_header_field(header + 68u, ntime);
    write_header_field(header + 72u, nbits);
    write_header_field(header + 76u, 0u);
    return true;
}

// ---------------------------------------------------------------------------------------------
// Mining workers
// ---------------------------------------------------------------------------------------------

/**
 * @brief Verifies a candidate independently of the scan that produced it.
 *
 * @param[in]  header        Eighty header bytes with the nonce already written [BORROWS].
 * @param[in]  share_target  Share threshold, most significant word first [BORROWS].
 * @param[out] digest        Thirty-two bytes of the hash that results [BORROWS].
 * @return                   True where the hash is at or below the share target.
 * @note This runs the plain double hash, and not the scan path. Thus a scan defect cannot smuggle a
 *       bad share past it. A pass of the anchor establishes nothing, and this check exists for that
 *       reason.
 */
bool verify_share(const uint8_t *header, const uint32_t *share_target, uint8_t *digest)
{
    sha256_double_hash(header, BITCOIN_HEADER_BYTES, digest);
    return sha256_digest_within_target(digest, share_target) != 0;
}

/**
 * @brief Counters that outlive one run of the process.
 *
 * @note A figure per run resets on restart. An anchor count that drops from 1020 to 85 then looks
 *       like a fault, but it is only a fresh process. A rate that is a lifetime average reads low
 *       for a long time after another program shares the device. A figure needs its "since when".
 * @note Three views, because they answer different questions. The window rate says whether the
 *       device is healthy now. The run totals say what this run did. The lifetime totals say
 *       whether the work accumulates across restarts.
 */
struct LifetimeTotals
{
    uint64_t hashes = 0u;
    uint64_t anchors = 0u;
    uint64_t submitted = 0u;
    uint64_t accepted = 0u;
    uint64_t rejected = 0u;
    double seconds = 0.0;
    uint64_t runs = 0u;
};

/** @brief Where the lifetime counters live between runs. */
const char *const TOTALS_PATH = "miner_totals.txt";

/**
 * @brief Reads the lifetime counters. An absent file is a first run, and not an error.
 *
 * @return What previous runs accumulated, or zeros.
 * @note One value per line, and no other content. Thus a person can read and check the file by
 *       eye. A miner that reports totals that nobody can verify reports nothing.
 */
LifetimeTotals load_totals()
{
    LifetimeTotals held;
    FILE *const handle = fopen(TOTALS_PATH, "r");
    if (handle == nullptr)
    {
        return held;
    }
    if (fscanf(handle, "%llu %llu %llu %llu %llu %lf %llu", (unsigned long long *)&held.hashes,
               (unsigned long long *)&held.anchors, (unsigned long long *)&held.submitted,
               (unsigned long long *)&held.accepted, (unsigned long long *)&held.rejected, &held.seconds,
               (unsigned long long *)&held.runs) != 7)
    {
        // A truncated file, or a file with a bad form, starts over. It does not carry half a total forward.
        held = LifetimeTotals();
    }
    fclose(handle);
    return held;
}

/** @brief Writes the lifetime counters. */
void save_totals(const LifetimeTotals &held)
{
    FILE *const handle = fopen(TOTALS_PATH, "w");
    if (handle == nullptr)
    {
        return;
    }
    fprintf(handle, "%llu %llu %llu %llu %llu %.3f %llu\n", (unsigned long long)held.hashes,
            (unsigned long long)held.anchors, (unsigned long long)held.submitted, (unsigned long long)held.accepted,
            (unsigned long long)held.rejected, held.seconds, (unsigned long long)held.runs);
    fclose(handle);
}

/** @brief Winners that one device launch may report. The launch drops the rest. */
const uint32_t DEVICE_FOUND_LIMIT = 64u;

/** @brief Nonces that one device launch claims. Sixty-four million is about thirty milliseconds
 *         at 2 GH/s. That keeps the launch overhead small. The abandon flag still gets a check
 *         often enough for a prompt start on a new job. */
const uint64_t DEVICE_NONCE_CHUNK = 1ull << 26;

/** @brief Nonzero where the operator asked for the device instead of the eight-lane arm. */
bool g_use_device = true;

/** @brief The extranonce2 of a counter, as hex of the width the pool asked for, the low digits kept or zeros put first.
 */
void extranonce2_of(uint64_t counter, size_t extranonce2_size, char *rendered)
{
    char counter_hex[9];
    hex_from_word((uint32_t)counter, counter_hex);
    const size_t needed = extranonce2_size * 2u;
    const size_t have = 8u;
    // a width that the field does not hold renders as the empty extranonce, which the header then refuses
    if (needed >= MINER_EXTRANONCE_HEX_BYTES)
    {
        rendered[0] = '\0';
        return;
    }
    if (have >= needed)
    {
        EMBED_CALL(memor.cpy, MemoriaCfg, .dst = rendered, .src = counter_hex + (have - needed), .bytes = needed + 1u);
        return;
    }
    EMBED_CALL(memor.set, MemoriaCfg, .dst = rendered, .bytes = needed - have, .val = '0');
    EMBED_CALL(memor.cpy, MemoriaCfg, .dst = rendered + (needed - have), .src = counter_hex, .bytes = have + 1u);
}

/**
 * @brief One mining worker: claims nonce ranges for the current job until the work changes.
 *
 * @param[in] context The worker's index, an int. Only the device's choice reads it [BORROWS].
 */
void mining_worker(void *context)
{
    const int worker_index = *(const int *)context;

    // One device context per worker. Worker zero takes it, and the rest fall back to the eight-lane
    // arm. One feeder at 2 GH/s already saturates the device. A second context would contend for
    // the same SMs, and cost another set of buffers.
    CudaMinerContext *device = nullptr;
    if (g_use_device && (worker_index == 0))
    {
        device = cuda_miner_create(DEVICE_FOUND_LIMIT);
        if (device == nullptr)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "device refused a context; worker 0 falls back to the eight-lane arm");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }

    uint64_t known_generation = UINT64_MAX;
    // the worker's own copy of the work, which it reads in a pass and keeps until the work changes
    static MinerWork s_worker_work[MINER_WORKERS_MAX];
    MinerWork *const work = &s_worker_work[(unsigned int)worker_index % MINER_WORKERS_MAX];
    char extranonce2_hex[MINER_EXTRANONCE_HEX_BYTES];
    uint8_t header[BITCOIN_HEADER_BYTES];
    Sha256ScanRequest request;
    uint32_t share_target[SHA256_STATE_WORDS];
    uint32_t block_target[SHA256_STATE_WORDS];
    bool header_ready = false;
    uint32_t rolled_version = 0u;

    while (counter_read(&g_state.running) != 0u)
    {
        const uint64_t current_generation = counter_read(&g_state.generation);

        if (current_generation != known_generation)
        {
            known_generation = current_generation;
            header_ready = false;

            work_read(work, work);
            if (!work->job.valid)
            {
                continue;
            }
            sha256_share_target_from_difficulty(share_target, work->difficulty);

            extranonce2_of(counter_read(&g_state.extranonce2_counter), work->extranonce2_size, extranonce2_hex);

            // The rolled version for this unit of work. With no mask granted, it is the version of
            // the job. The header is then byte for byte the header of a client without a roll.
            const uint32_t base_version = word_from_hex(work->job.version_hex);
            rolled_version = version_from_index(base_version, work->version_mask, counter_read(&g_state.version_index));

            if (!build_header(work->job, work->extranonce1_hex, extranonce2_hex, rolled_version, header))
            {
                platform_paused(50u);
                continue;
            }

            EMBED_CALL(memor.set, MemoriaCfg, .dst = &request, .bytes = sizeof(request), .val = 0);
            sha256_header_midstate(&request.midstate, header);
            request.merkle_root_tail = schedule_word_from_header(header + 64u);
            request.ntime = schedule_word_from_header(header + 68u);
            request.nbits = schedule_word_from_header(header + 72u);
            request.abandon = &g_state.abandon;
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = request.share_target, .src = share_target,
                       .bytes = sizeof(share_target));
            sha256_target_from_nbits(block_target, word_from_hex(work->job.nbits_hex));
            header_ready = true;
        }

        if (!header_ready)
        {
            platform_paused(50u);
            continue;
        }

        // A device chunk must be much larger than a CPU chunk, or the launch overhead dominates.
        // Two million nonces is a millisecond of work at 2 GH/s, and the round trip costs more than
        // that. All workers share the cursor. A device worker simply claims more of it per turn.
        const uint64_t chunk = (device != nullptr) ? DEVICE_NONCE_CHUNK : NONCE_CHUNK;
        const uint64_t claimed = counter_added(&g_state.nonce_cursor, chunk);
        if (claimed >= 0x100000000ULL)
        {
            // This version and extranonce2 have no nonce space left. Roll the version first, because
            // that only changes the midstate of the first block. When the version space runs out,
            // fall back to the extranonce2, which rebuilds the coinbase and the merkle root. Either
            // way, every worker rebuilds from the new generation. The worker whose swap sets the cursor
            // back to 0 rolls, and a worker whose swap finds another value leaves the roll to it
            uint64_t seen = counter_read(&g_state.nonce_cursor);
            if ((seen >= 0x100000000ULL) && EMBED_CALL(embed_atomic_u64_swap_if, EmbedAtomicU64SwapIfCfg,
                                                       .word = &g_state.nonce_cursor, .expected = &seen, .value = 0u))
            {
                bool advanced_version = false;
                if (work->version_mask != 0u)
                {
                    const uint64_t next = counter_read(&g_state.version_index) + 1u;
                    if (next < version_space(work->version_mask))
                    {
                        counter_set(&g_state.version_index, next);
                        advanced_version = true;
                    }
                }
                if (!advanced_version)
                {
                    (void)counter_added(&g_state.extranonce2_counter, 1u);
                    counter_set(&g_state.version_index, 0u);
                }
                (void)counter_added(&g_state.generation, 1u);
            }
            continue;
        }

        request.nonce_start = (uint32_t)claimed;
        request.nonce_count = (uint32_t)((claimed + chunk > 0x100000000ULL) ? (0x100000000ULL - claimed) : chunk);

        Sha256ScanResult result;

        if (device != nullptr)
        {
            CudaScanParameters parameters;
            EMBED_CALL(memor.set, MemoriaCfg, .dst = &parameters, .bytes = sizeof(parameters), .val = 0);
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = parameters.midstate, .src = request.midstate.word,
                       .bytes = sizeof(parameters.midstate));
            parameters.merkle_root_tail = request.merkle_root_tail;
            parameters.ntime = request.ntime;
            parameters.nbits = request.nbits;
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = parameters.share_target, .src = request.share_target,
                       .bytes = sizeof(parameters.share_target));
            parameters.anchor_is_sound = (request.share_target[0] == 0u) ? 1u : 0u;

            // The anchored arm where the anchor is sound, the raw arm otherwise. Both find the same
            // winners; the choice affects only speed.
            CudaScanArm chosen_arm = cuda_miner_choose(&parameters);

            uint32_t found_nonces[DEVICE_FOUND_LIMIT];
            uint32_t found_count = 0u;
            const int launched =
                chosen_arm(device, &parameters, request.nonce_start, request.nonce_count, found_nonces, &found_count);
            if (launched == 0)
            {
                // A failed launch is not a scanned range. A count of it would inflate the rate and
                // silently skip nonces. Thus the loop drops the chunk and tries the next one.
                platform_paused(50u);
                continue;
            }

            result.nonces_evaluated = request.nonce_count;

            // The anchor count of the device, and not its winner count.
            result.anchors_survived = (uint32_t)cuda_miner_anchor_survivors(device);
            result.found = (found_count > 0u) ? 1 : 0;
            result.winning_nonce = 0u;

            // Winners come back in no order. Submit the lowest.
            for (uint32_t at = 0u; at < found_count; at += 1u)
            {
                if ((at == 0u) || (found_nonces[at] < result.winning_nonce))
                {
                    result.winning_nonce = found_nonces[at];
                }
            }
        }
        else
        {
            sha256_scan_avx2(&request, &result);
        }

        (void)counter_added(&g_state.hashes_done, result.nonces_evaluated);
        (void)counter_added(&g_state.anchor_survivors, result.anchors_survived);

        if (result.found == 0)
        {
            continue;
        }

        write_header_field(header + 76u, result.winning_nonce);
        uint8_t digest[32];
        if (!verify_share(header, share_target, digest))
        {
            // The scan claimed a share, and the plain double hash does not agree. Report it and
            // send nothing. A disagreement between the two is a defect, and not a share.
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "\n[DEFECT] scan reported nonce ");
                mmgr_line_decimal(output_line, result.winning_nonce, 1u);
                mmgr_line_put(output_line, " which independent verification rejects. Not submitting.");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            continue;
        }

        FoundShare share;
        (void)text_set(share.job_id, sizeof(share.job_id), work->job.job_id);
        (void)text_set(share.extranonce2_hex, sizeof(share.extranonce2_hex), extranonce2_hex);
        (void)text_set(share.ntime_hex, sizeof(share.ntime_hex), work->job.ntime_hex);
        share.nonce = result.winning_nonce;
        share.version = rolled_version;
        block_hash_from_digest(digest, share.block_hash);
        share.meets_block_target = sha256_digest_within_target(digest, block_target) != 0;
        share_posted(&share, work);
    }

    if (device != nullptr)
    {
        cuda_miner_destroy(device);
    }
}

// ---------------------------------------------------------------------------------------------
// Stratum connection
// ---------------------------------------------------------------------------------------------

/**
 * @brief Sends a whole payload. It loops over partial writes.
 *
 * @param[in] connection Socket to write to.
 * @param[in] payload    Bytes to send, terminated [BORROWS].
 * @return               True where all of it went out.
 */
bool send_payload(SOCKET connection, const char *payload)
{
    const size_t size = text_length(payload);
    size_t sent_total = 0u;

    while (sent_total < size)
    {
        const int sent = send(connection, payload + sent_total, (int)(size - sent_total), 0);
        if (sent == SOCKET_ERROR)
        {
            return false;
        }
        sent_total += (size_t)sent;
    }
    return true;
}

/** @brief A line that tells of a field that passes its bound. */
void report_refused(const char *what)
{
    VerbaLinea *const output_line = platform_line_out();
    mmgr_line_put(output_line, "\n[REFUSED] ");
    mmgr_line_put(output_line, what);
    mmgr_line_ch(output_line, '\n');
    platform_line_out_ended();
}

/**
 * @brief Reads a mining.notify into a job.
 *
 * @param[in]  document The message [BORROWS].
 * @param[in]  params   Parameter array as sent.
 * @param[out] job      Where the job lands [BORROWS].
 * @return              True where every field was present, the right type, and inside its bound.
 * @note The order is job_id, prevhash, coinb1, coinb2, merkle_branch, version, nbits, ntime,
 *       clean_jobs. A live job confirms this order, and it is not an assumption.
 */
bool job_from_notify(const JsonDocument *document, unsigned int params, MiningJob &job)
{
    if ((json_type(document, params) != JsonType::Array) || (json_count(document, params) < 9u))
    {
        return false;
    }
    const unsigned int field[9] = {
        json_at(document, params, 0u), json_at(document, params, 1u), json_at(document, params, 2u),
        json_at(document, params, 3u), json_at(document, params, 4u), json_at(document, params, 5u),
        json_at(document, params, 6u), json_at(document, params, 7u), json_at(document, params, 8u)};
    if (!json_is_string(document, field[0]) || !json_is_string(document, field[1]) ||
        !json_is_string(document, field[2]) || !json_is_string(document, field[3]) ||
        !json_is_string(document, field[5]) || !json_is_string(document, field[6]) ||
        !json_is_string(document, field[7]))
    {
        return false;
    }

    bool fits = text_set(job.job_id, sizeof(job.job_id), json_text(document, field[0])) &&
                text_set(job.prevhash_hex, sizeof(job.prevhash_hex), json_text(document, field[1])) &&
                text_set(job.coinbase_head_hex, sizeof(job.coinbase_head_hex), json_text(document, field[2])) &&
                text_set(job.coinbase_tail_hex, sizeof(job.coinbase_tail_hex), json_text(document, field[3]));
    job.merkle_branch_count = 0u;

    const unsigned int branch = field[4];
    for (unsigned int at = 0u; fits && (at < json_count(document, branch)); at += 1u)
    {
        const unsigned int entry = json_at(document, branch, at);
        if (json_is_string(document, entry))
        {
            fits = (job.merkle_branch_count < MINER_BRANCH_MAX) &&
                   text_set(job.merkle_branch_hex[job.merkle_branch_count], MINER_HASH_HEX_BYTES,
                            json_text(document, entry));
            job.merkle_branch_count += fits ? 1u : 0u;
        }
    }

    fits = fits && text_set(job.version_hex, sizeof(job.version_hex), json_text(document, field[5])) &&
           text_set(job.nbits_hex, sizeof(job.nbits_hex), json_text(document, field[6])) &&
           text_set(job.ntime_hex, sizeof(job.ntime_hex), json_text(document, field[7]));
    if (!fits)
    {
        report_refused("a field of mining.notify passes the bound of its text");
        return false;
    }
    job.clean_jobs = document->node[field[8]].boolean;
    job.valid = true;
    return true;
}

/**
 * @brief Prints what a new job actually resolved to, a byte order defect is visible immediately.
 *
 * @param[in] job The job just installed [BORROWS].
 */
void report_job(const MiningJob &job)
{
    uint8_t prevhash[32];
    char previous_block[MINER_HASH_HEX_BYTES] = "";

    if (bytes_from_hex(job.prevhash_hex, prevhash, sizeof(prevhash)) == 32u)
    {
        uint8_t header_order[32];
        for (size_t word = 0u; word < 8u; word += 1u)
        {
            for (size_t byte = 0u; byte < 4u; byte += 1u)
            {
                header_order[(word * 4u) + byte] = prevhash[(word * 4u) + (3u - byte)];
            }
        }
        block_hash_from_digest(header_order, previous_block);
    }

    const size_t leading_zeros = EMBED_CALL(cellul.span, CatenaFinitaCfg, .src = previous_block,
                                            .cap = text_length(previous_block), .other = "0");

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "\n[JOB] ");
        mmgr_line_put(output_line, job.job_id);
        mmgr_line_put(output_line, "  branch ");
        mmgr_line_decimal(output_line, job.merkle_branch_count, 1u);
        mmgr_line_put(output_line, "  ntime ");
        mmgr_line_put(output_line, job.ntime_hex);
        mmgr_line_put(output_line, "  nbits ");
        mmgr_line_put(output_line, job.nbits_hex);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "      previous block ");
        mmgr_line_put(output_line, previous_block);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "      leading zeros ");
        mmgr_line_decimal(output_line, leading_zeros, 1u);
        mmgr_line_put(output_line, " ");
        mmgr_line_put(output_line, (leading_zeros >= 15u) ? "(consistent with a real chain tip)"
                                                          : "(TOO FEW: prevhash byte order is wrong)");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
}

/**
 * @brief Builds a header from a live job and reports whether every field landed where it should.
 *
 * @param[in] job             The job just received [BORROWS].
 * @param[in] extranonce1_hex Extranonce the pool assigned [BORROWS].
 * @param[in] extranonce2_size How wide the pool wants extranonce2.
 * @return                    True where every check passed.
 * @note These checks catch a byte order defect without a message from a pool. A
 *       correctly assembled header hashes to something, and that hash has to be consistent with the
 *       difficulty the same job declares. A header assembled wrongly still hashes; it just hashes
 *       to noise, and noise does not sit near a target.
 */
bool dry_run_check_job(const MiningJob &job, const char *extranonce1_hex, size_t extranonce2_size)
{
    uint8_t header[BITCOIN_HEADER_BYTES];
    char extranonce2_hex[MINER_EXTRANONCE_HEX_BYTES] = "";
    bool all_held = true;

    if ((2u * extranonce2_size) < sizeof(extranonce2_hex))
    {
        EMBED_CALL(memor.set, MemoriaCfg, .dst = extranonce2_hex, .bytes = 2u * extranonce2_size, .val = '0');
        extranonce2_hex[2u * extranonce2_size] = '\0';
    }
    const uint32_t base_version = word_from_hex(job.version_hex);
    if (!build_header(job, extranonce1_hex, extranonce2_hex, base_version, header))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "      [FAIL] header could not be assembled");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return false;
    }

    char rendered[(2u * BITCOIN_HEADER_BYTES) + 1u];
    hex_from_bytes(header, BITCOIN_HEADER_BYTES, rendered);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "      header      ");
        mmgr_line_put(output_line, rendered);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    // The merkle root must not be all zeros or the literal the old client used.
    bool merkle_is_zero = true;
    for (size_t at = 36u; at < 68u; at += 1u)
    {
        if (header[at] != 0u)
        {
            merkle_is_zero = false;
        }
    }
    hex_from_bytes(header + 36u, 32u, rendered);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "      merkle root ");
        mmgr_line_put(output_line, rendered);
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, merkle_is_zero ? "[FAIL] all zero" : "[ok]");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    all_held = all_held && !merkle_is_zero;

    // The version, ntime and nbits must round-trip back to what the pool sent.
    const uint32_t version_back =
        (uint32_t)header[0] | ((uint32_t)header[1] << 8) | ((uint32_t)header[2] << 16) | ((uint32_t)header[3] << 24);
    const uint32_t nbits_back = (uint32_t)header[72] | ((uint32_t)header[73] << 8) | ((uint32_t)header[74] << 16) |
                                ((uint32_t)header[75] << 24);
    const bool version_ok = (version_back == word_from_hex(job.version_hex));
    const bool nbits_ok = (nbits_back == word_from_hex(job.nbits_hex));
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "      version     ");
        mmgr_line_hex(output_line, version_back, 8u);
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, version_ok ? "[ok]" : "[FAIL]");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "      nbits       ");
        mmgr_line_hex(output_line, nbits_back, 8u);
        mmgr_line_put(output_line, "  ");
        mmgr_line_put(output_line, nbits_ok ? "[ok]" : "[FAIL]");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    all_held = all_held && version_ok && nbits_ok;

    // The declared difficulty has to be a sane network number. A misread nbits gives an absurd one.
    uint32_t block_target[SHA256_STATE_WORDS];
    sha256_target_from_nbits(block_target, nbits_back);
    const bool target_ok = (block_target[0] == 0u) && (block_target[1] != 0u || block_target[2] != 0u);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "      net target  ");
        mmgr_line_hex(output_line, block_target[0], 8u);
        mmgr_line_hex(output_line, block_target[1], 8u);
        mmgr_line_hex(output_line, block_target[2], 8u);
        mmgr_line_put(output_line, "...  ");
        mmgr_line_put(output_line, target_ok ? "[ok]" : "[FAIL] nbits did not expand sanely");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    all_held = all_held && target_ok;

    // Hash the header once. A single nonce should not meet the target. But the digest must have a
    // good form, and the scan path must agree with the plain double hash on the same input.
    uint8_t digest[32];
    sha256_double_hash(header, BITCOIN_HEADER_BYTES, digest);
    char digest_hex[MINER_HASH_HEX_BYTES];
    block_hash_from_digest(digest, digest_hex);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "      hash@nonce0 ");
        mmgr_line_put(output_line, digest_hex);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    Sha256ScanRequest request;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &request, .bytes = sizeof(request), .val = 0);
    sha256_header_midstate(&request.midstate, header);
    request.merkle_root_tail = schedule_word_from_header(header + 64u);
    request.ntime = schedule_word_from_header(header + 68u);
    request.nbits = schedule_word_from_header(header + 72u);
    request.nonce_start = 0u;
    request.nonce_count = 1u;
    // A threshold of all ones accepts anything. Thus the scan must run its compare path, and report
    // the nonce, and not reject it. That makes the two paths comparable on one input.
    EMBED_CALL(memor.set, MemoriaCfg, .dst = request.share_target, .bytes = sizeof(request.share_target), .val = 0xFF);

    Sha256ScanResult result;
    sha256_scan_scalar(&request, &result);
    const bool scan_agrees = (result.found != 0) && (result.winning_nonce == 0u);
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "      scan path agrees with plain double hash: ");
        mmgr_line_put(output_line, scan_agrees ? "[ok]" : "[FAIL]");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    all_held = all_held && scan_agrees;

    return all_held;
}

/** @brief The JSON document of the line the network thread reads, held for the life of the process. */
JsonDocument g_message;

/** @brief The workers and the index each reads. */
PlatformWorker g_workers[MINER_WORKERS_MAX];
int g_worker_index[MINER_WORKERS_MAX];

} // namespace

int program_main(int argument_count, char **arguments)
{
    const char *pool_hostname = DEFAULT_POOL_HOSTNAME;
    const char *pool_port = DEFAULT_POOL_PORT;

    // Vardiff can climb to 10000 after a flood of false submissions. At this hash rate, that is one
    // share every six hours. Ask for a difficulty that produces shares while a person watches the
    // client.
    double suggested_difficulty = 16.0;

    // The positive control for `version-rolling`. It runs before anything touches the network or device.
    for (int index = 1; index < argument_count; index += 1)
    {
        if (text_same(arguments[index], "--check"))
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "BTC version-rolling checks");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            return run_version_rolling_checks() == 0 ? 0 : 1;
        }
    }

    // Zero means the default, which leaves one core for the person at the keyboard.
    unsigned requested_threads = 0u;

    // A miner is background work. At normal priority on a desktop, it starves the window manager,
    // even with a free core. When every core is busy, the priority decides what runs, and the core
    // count does not.
    bool background_priority = true;

    for (int index = 1; index < argument_count; index += 1)
    {
        const char *const argument = arguments[index];
        const bool has_value = (index + 1) < argument_count;

        if (text_same(argument, "--address") && has_value)
        {
            if (!text_set(g_payout_address, sizeof(g_payout_address), arguments[index + 1]))
            {
                report_refused("--address passes the bound of its text");
                return 1;
            }
            index += 1;
        }
        else if (text_same(argument, "--pool") && has_value)
        {
            pool_hostname = arguments[index + 1];
            index += 1;
        }
        else if (text_same(argument, "--port") && has_value)
        {
            pool_port = arguments[index + 1];
            index += 1;
        }
        else if (text_same(argument, "--difficulty") && has_value)
        {
            suggested_difficulty = strtod(arguments[index + 1], NULL);
            index += 1;
        }
        else if (text_same(argument, "--threads") && has_value)
        {
            requested_threads = (unsigned)strtoul(arguments[index + 1], NULL, 10);
            index += 1;
        }
        else if (text_same(argument, "--normal-priority"))
        {
            background_priority = false;
        }
        else if (text_same(argument, "--user") && has_value)
        {
            if (!text_set(g_stratum_user, sizeof(g_stratum_user), arguments[index + 1]))
            {
                report_refused("--user passes the bound of its text");
                return 1;
            }
            index += 1u;
        }
        else if (text_same(argument, "--no-gpu"))
        {
            g_use_device = false;
        }
        else if (text_same(argument, "--dry-run"))
        {
            g_dry_run = true;
        }
    }

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "BTC");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "AVX2: ");
        mmgr_line_put(output_line, (sha256_has_avx2() != 0) ? "yes" : "no");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (sha256_has_avx2() == 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "This build needs AVX2. Refusing to run a scalar miner.");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }

    // Fail closed. A miner with no address, or with the example address, mines for someone else.
    // A refusal costs a restart. A guess costs the earnings.
    //
    // An account pool identifies by worker name, and keeps the payout destination in an account
    // setting. Thus --user alone is a complete identity there. A demand for an address too would
    // refuse a correct configuration. An address pool still needs the address.
    if ((g_payout_address[0] == '\0') && (g_stratum_user[0] == '\0') && !g_dry_run)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "\nNo identity given. Pass --address <your bitcoin address> for a pool that");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "pays an address, such as ckpool or Ocean, or --user <account.worker> for one");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "that pays an account, such as Braiins.");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "Refusing to mine without one. Pass --dry-run to exercise the job path");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "without authorizing, mining or submitting.");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }
    if (text_same(g_payout_address, KNOWN_EXAMPLE_ADDRESS))
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "\nThat is the BIP-173 example address, not a wallet you control.");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "It was hardcoded in the previous version. Refusing to mine to it.");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }

    if (g_stratum_user[0] != '\0')
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "worker: ");
            mmgr_line_put(output_line, g_stratum_user);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "payout: set in the pool account, not by this client");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        if (g_payout_address[0] != '\0')
        {
            // Say so, and do not silently prefer one. An operator who passes both probably believes
            // that the address has an effect here. On an account pool, it has none.
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "note:   --address is ignored on an account pool; the account decides");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
    }
    else
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "paying: ");
        mmgr_line_put(output_line, g_payout_address);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }

    if (background_priority)
    {
        // BELOW_NORMAL keeps the miner off the critical path for input and drawing.
        // PROCESS_MODE_BACKGROUND_BEGIN would also throttle its I/O and page priority. That is too
        // much for a process with an open socket.
        if (SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS) != 0)
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "priority: below normal, so the desktop stays responsive");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }

    // the work at rest: the first record, no job, an extranonce2 of four bytes and difficulty 1, published
    g_work[0].extranonce2_size = 4u;
    g_work[0].difficulty = 1.0;
    // Explicit cast carries the record's address as the published word
    EMBED_CALL(embed_atomic_uptr_store, EmbedAtomicUptrStoreCfg, .word = &g_work_published,
               .value = (embed_atomic_uptr_value)(uintptr_t)&g_work[0]);
    counter_set(&g_state.running, 1u);
    EMBED_CALL(embed_ticket_ring_begin, EmbedTicketRingBeginCfg, .ring = &g_share_ring, .slots = g_share_slots,
               .shift = MINER_SHARE_SHIFT);

    WSADATA winsock;
    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "WSAStartup failed");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        return 1;
    }

    addrinfo hints;
    addrinfo *resolved = nullptr;
    EMBED_CALL(memor.set, MemoriaCfg, .dst = &hints, .bytes = sizeof(hints), .val = 0);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(pool_hostname, pool_port, &hints, &resolved) != 0)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "cannot resolve ");
            mmgr_line_put(output_line, pool_hostname);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        WSACleanup();
        return 1;
    }

    const SOCKET connection = socket(resolved->ai_family, resolved->ai_socktype, resolved->ai_protocol);
    if (connection == INVALID_SOCKET)
    {
        freeaddrinfo(resolved);
        WSACleanup();
        return 1;
    }

    BOOL nodelay = TRUE;
    setsockopt(connection, IPPROTO_TCP, TCP_NODELAY, (const char *)&nodelay, sizeof(nodelay));
    DWORD receive_timeout = 200;
    setsockopt(connection, SOL_SOCKET, SO_RCVTIMEO, (const char *)&receive_timeout, sizeof(receive_timeout));

    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "connecting to ");
        mmgr_line_put(output_line, pool_hostname);
        mmgr_line_put(output_line, ":");
        mmgr_line_put(output_line, pool_port);
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    if (connect(connection, resolved->ai_addr, (int)resolved->ai_addrlen) == SOCKET_ERROR)
    {
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "connect failed");
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
        closesocket(connection);
        freeaddrinfo(resolved);
        WSACleanup();
        return 1;
    }
    freeaddrinfo(resolved);

    char payload[MINER_PAYLOAD_BYTES];
    // BIP310: negotiate `version-rolling` before subscribe. The pool answers on id 0 with the mask
    // that it grants, a subset of the BIP320 request mask 0x1fffe000. A pool without support errors
    // or ignores id 0. Then version_mask stays at zero, and the miner does not roll the version.
    send_payload(connection, "{\"id\":0,\"method\":\"mining.configure\","
                             "\"params\":[[\"version-rolling\"],{\"version-rolling.mask\":\"1fffe000\"}]}\r\n");

    send_payload(connection, "{\"id\":1,\"method\":\"mining.subscribe\",\"params\":[\"btc/0.1\"]}\r\n");
    if (!g_dry_run)
    {
        {
            VerbaLinea line = mmgr_line_over(payload, sizeof(payload));
            mmgr_line_put(&line, "{\"id\":2,\"method\":\"mining.authorize\",\"params\":[\"");
            mmgr_line_put(&line, stratum_identity());
            mmgr_line_put(&line, "\",\"x\"]}\r\n");
            (void)mmgr_line_finish(&line);
        }
        send_payload(connection, payload);

        // Ask for a difficulty this hash rate actually produces shares at. Vardiff aims at roughly
        // one share every ten to thirty seconds; the pool is free to ignore this and its own
        // set_difficulty always wins.
        if (suggested_difficulty > 0.0)
        {
            char request[192];
            snprintf(request, sizeof(request),
                     "{\"id\":3,\"method\":\"mining.suggest_difficulty\",\"params\":[%.4f]}"
                     "\r\n",
                     suggested_difficulty);
            send_payload(connection, request);
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "suggested difficulty: ");
                mmgr_line_fixed(output_line, (double)(suggested_difficulty), 4u, 0u, EMBED_FALSE);
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
        }
    }

    // Leave one thread for the person at the machine. A miner that takes every core makes the
    // desktop unusable. The last core is worth less than the ability to type. Override with
    // --threads on a dedicated box.
    const unsigned available = platform_processors();
    const unsigned wanted = (requested_threads > 0u) ? requested_threads : ((available > 1u) ? (available - 1u) : 1u);
    const unsigned worker_count = (wanted < MINER_WORKERS_MAX) ? wanted : MINER_WORKERS_MAX;
    unsigned started_workers = 0u;
    if (g_dry_run)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "DRY RUN: subscribing and parsing only. Not authorizing, mining or submitting.");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    else
    {
        for (unsigned index = 0u; index < worker_count; index += 1u)
        {
            g_worker_index[index] = (int)index;
            started_workers +=
                (platform_worker_started(&g_workers[index], mining_worker, &g_worker_index[index]) != 0) ? 1u : 0u;
        }
        {
            VerbaLinea *const output_line = platform_line_out();
            mmgr_line_put(output_line, "workers: ");
            mmgr_line_decimal(output_line, worker_count, 1u);
            mmgr_line_ch(output_line, '\n');
            platform_line_out_ended();
        }
    }

    // the pool's text that waits for its line to end, `inbound_used` bytes of it
    static char s_inbound[MINER_INBOUND_BYTES];
    size_t inbound_used = 0u;
    char buffer[16384];
    uint64_t submitted = 0u;
    uint64_t accepted = 0u;
    uint64_t rejected = 0u;
    uint64_t next_submit_id = 100u;
    uint64_t share_taken_ticket = 0u;
    unsigned long long last_report = platform_clock_ns();
    const unsigned long long started = platform_clock_ns();

    // What previous runs accumulated. Thus the report can show both this run and the lifetime total.
    LifetimeTotals lifetime = load_totals();
    uint64_t last_hashes = 0u;
    if (lifetime.runs > 0u)
    {
        VerbaLinea *const output_line = platform_line_out();
        mmgr_line_put(output_line, "lifetime: ");
        mmgr_line_fixed(output_line, (double)(lifetime.seconds / 3600.0), 1u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " hours, ");
        mmgr_line_fixed(output_line, (double)((double)lifetime.hashes / 1.0e12), 2u, 0u, EMBED_FALSE);
        mmgr_line_put(output_line, " TH, ");
        mmgr_line_decimal(output_line, (unsigned long long)lifetime.anchors, 1u);
        mmgr_line_put(output_line, " anchors, ");
        mmgr_line_decimal(output_line, (unsigned long long)lifetime.accepted, 1u);
        mmgr_line_put(output_line, "/");
        mmgr_line_decimal(output_line, (unsigned long long)lifetime.submitted, 1u);
        mmgr_line_put(output_line, " shares over ");
        mmgr_line_decimal(output_line, (unsigned long long)lifetime.runs, 1u);
        mmgr_line_put(output_line, " runs");
        mmgr_line_ch(output_line, '\n');
        platform_line_out_ended();
    }
    // The count goes here, and every periodic save carries it forward. An increment of a copy
    // would let the next report write back the old value, and the run count would never rise.
    lifetime.runs += 1u;
    save_totals(lifetime);

    while (true)
    {
        const int received = recv(connection, buffer, (int)sizeof(buffer) - 1, 0);
        if (received > 0)
        {
            if ((inbound_used + (size_t)received) > sizeof(s_inbound))
            {
                // a line longer than the text the client holds is refused whole, and the text starts over
                report_refused("a line from the pool passes the bytes the client holds; dropped");
                inbound_used = 0u;
            }
            if ((inbound_used + (size_t)received) <= sizeof(s_inbound))
            {
                EMBED_CALL(memor.cpy, MemoriaCfg, .dst = s_inbound + inbound_used, .src = buffer,
                           .bytes = (size_t)received);
                inbound_used += (size_t)received;
            }

            const char *line_end =
                (const char *)EMBED_CALL(memor.chr, MemoriaCfg, .src = s_inbound, .bytes = inbound_used, .val = '\n');
            while (line_end != NULL)
            {
                // the line, its line break made a terminator and its trailing returns and spaces cut, then the
                // bytes after it moved to the front once the message is read
                char *const line = s_inbound;
                size_t line_size = (size_t)(line_end - s_inbound);
                const size_t consumed = line_size + 1u;
                while ((line_size > 0u) && ((line[line_size - 1u] == '\r') || (line[line_size - 1u] == ' ')))
                {
                    line_size -= 1u;
                }

                const unsigned int message = (line_size != 0u) ? json_parse(line, line_size, &g_message) : JSON_ABSENT;
                EMBED_CALL(memor.move_down, MemoriaCfg, .dst = s_inbound, .src = s_inbound + consumed,
                           .bytes = inbound_used - consumed);
                inbound_used -= consumed;
                line_end = (const char *)EMBED_CALL(memor.chr, MemoriaCfg, .src = s_inbound, .bytes = inbound_used,
                                                    .val = '\n');

                if (message == JSON_ABSENT)
                {
                    continue;
                }

                const JsonDocument *const document = &g_message;
                const unsigned int method = json_member(document, message, "method");
                const unsigned int params = json_member(document, message, "params");

                if (json_is_string(document, method) && text_same(json_text(document, method), "mining.notify"))
                {
                    static MiningJob s_job;
                    if (job_from_notify(document, params, s_job))
                    {
                        MinerWork *const next = work_next();
                        next->job = s_job;
                        work_published();
                        g_state.abandon = 1;
                        counter_set(&g_state.nonce_cursor, 0u);
                        (void)counter_added(&g_state.generation, 1u);
                        g_state.abandon = 0;
                        report_job(s_job);

                        if (g_dry_run)
                        {
                            const MinerWork *const current = work_current();
                            const bool held_up =
                                dry_run_check_job(s_job, current->extranonce1_hex, current->extranonce2_size);
                            {
                                VerbaLinea *const output_line = platform_line_out();
                                mmgr_line_put(output_line, "\n  DRY RUN RESULT: ");
                                mmgr_line_put(output_line, held_up ? "every check held" : "CHECKS FAILED");
                                mmgr_line_ch(output_line, '\n');
                                platform_line_out_ended();
                            }
                            counter_set(&g_state.running, 0u);
                            closesocket(connection);
                            WSACleanup();
                            return held_up ? 0 : 1;
                        }
                    }
                    continue;
                }

                if (json_is_string(document, method) && text_same(json_text(document, method), "mining.set_difficulty"))
                {
                    const unsigned int first = json_at(document, params, 0u);
                    if (json_type(document, first) == JsonType::Number)
                    {
                        MinerWork *const next = work_next();
                        next->difficulty = document->node[first].number;
                        work_published();
                        (void)counter_added(&g_state.generation, 1u);
                        {
                            VerbaLinea *const output_line = platform_line_out();
                            mmgr_line_put(output_line, "\n[DIFF] share difficulty ");
                            mmgr_line_fixed(output_line, (double)(document->node[first].number), 4u, 0u, EMBED_FALSE);
                            mmgr_line_ch(output_line, '\n');
                            platform_line_out_ended();
                        }
                    }
                    continue;
                }

                const unsigned int identifier = json_member(document, message, "id");
                const unsigned int outcome = json_member(document, message, "result");
                const bool numbered = json_type(document, identifier) == JsonType::Number;
                const double number = document->node[identifier].number;

                if (numbered && (number == 0.0))
                {
                    // mining.configure reply: the `version-rolling` mask that the pool granted, if any.
                    const unsigned int enabled = json_member(document, outcome, "version-rolling");
                    const unsigned int granted = json_member(document, outcome, "version-rolling.mask");
                    uint32_t mask = 0u;
                    if ((json_type(document, enabled) == JsonType::Boolean) && document->node[enabled].boolean &&
                        json_is_string(document, granted))
                    {
                        mask = word_from_hex(json_text(document, granted)) & VERSION_ROLLING_REQUEST_MASK;
                    }
                    {
                        MinerWork *const next = work_next();
                        next->version_mask = mask;
                        work_published();
                    }
                    if (mask != 0u)
                    {
                        VerbaLinea *const output_line = platform_line_out();
                        mmgr_line_put(output_line, "[CONFIGURE] version-rolling on, mask ");
                        mmgr_line_hex(output_line, mask, 8u);
                        mmgr_line_put(output_line, ", ");
                        mmgr_line_decimal(output_line, (unsigned long long)version_space(mask), 1u);
                        mmgr_line_put(output_line, " versions per extranonce2");
                        mmgr_line_ch(output_line, '\n');
                        platform_line_out_ended();
                    }
                    else
                    {
                        VerbaLinea *const output_line = platform_line_out();
                        mmgr_line_put(output_line,
                                      "[CONFIGURE] version-rolling not granted; mining the nonce and extranonce2 only");
                        mmgr_line_ch(output_line, '\n');
                        platform_line_out_ended();
                    }
                    continue;
                }

                if (numbered && (number == 1.0))
                {
                    // Subscribe reply: the extranonce is its purpose, and the client must keep it.
                    // Element one is extranonce1, element two is the extranonce2 width.
                    const unsigned int extranonce1 = json_at(document, outcome, 1u);
                    const unsigned int extranonce2 = json_at(document, outcome, 2u);
                    if (json_is_string(document, extranonce1))
                    {
                        MinerWork *const next = work_next();
                        if (!text_set(next->extranonce1_hex, sizeof(next->extranonce1_hex),
                                      json_text(document, extranonce1)))
                        {
                            report_refused("the extranonce1 of the subscribe reply passes the bound of its text");
                        }
                        if (json_type(document, extranonce2) == JsonType::Number)
                        {
                            next->extranonce2_size = (size_t)document->node[extranonce2].number;
                        }
                        work_published();
                        {
                            const MinerWork *const current = work_current();
                            VerbaLinea *const output_line = platform_line_out();
                            mmgr_line_put(output_line, "[SUBSCRIBE] extranonce1 ");
                            mmgr_line_put(output_line, current->extranonce1_hex);
                            mmgr_line_put(output_line, ", extranonce2 ");
                            mmgr_line_decimal(output_line, current->extranonce2_size, 1u);
                            mmgr_line_put(output_line, " bytes");
                            mmgr_line_ch(output_line, '\n');
                            platform_line_out_ended();
                        }
                    }
                    continue;
                }

                if (numbered && (number >= 100.0))
                {
                    if ((json_type(document, outcome) == JsonType::Boolean) && document->node[outcome].boolean)
                    {
                        accepted += 1u;
                        {
                            VerbaLinea *const output_line = platform_line_out();
                            mmgr_line_put(output_line, "\n[SHARE] accepted (");
                            mmgr_line_decimal(output_line, (unsigned long long)accepted, 1u);
                            mmgr_line_put(output_line, " of ");
                            mmgr_line_decimal(output_line, (unsigned long long)submitted, 1u);
                            mmgr_line_put(output_line, ")");
                            mmgr_line_ch(output_line, '\n');
                            platform_line_out_ended();
                        }
                    }
                    else
                    {
                        rejected += 1u;
                        const unsigned int failure = json_member(document, message, "error");
                        const char *reason = "unspecified";
                        if (json_type(document, failure) == JsonType::Array)
                        {
                            for (unsigned int at = 0u; at < json_count(document, failure); at += 1u)
                            {
                                const unsigned int part = json_at(document, failure, at);
                                if (json_is_string(document, part))
                                {
                                    reason = json_text(document, part);
                                }
                            }
                        }
                        {
                            VerbaLinea *const output_line = platform_line_out();
                            mmgr_line_put(output_line, "\n[SHARE] REJECTED: ");
                            mmgr_line_put(output_line, reason);
                            mmgr_line_ch(output_line, '\n');
                            platform_line_out_ended();
                        }
                    }
                    continue;
                }
            }
        }
        else if (received == 0)
        {
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "\npool closed the connection");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            break;
        }
        else
        {
            const int failure = WSAGetLastError();
            if ((failure != WSAETIMEDOUT) && (failure != WSAEWOULDBLOCK))
            {
                {
                    VerbaLinea *const output_line = platform_line_out();
                    mmgr_line_put(output_line, "\nreceive failed (");
                    mmgr_line_decimal_signed(output_line, failure);
                    mmgr_line_put(output_line, ")");
                    mmgr_line_ch(output_line, '\n');
                    platform_line_out_ended();
                }
                break;
            }
        }

        // every share the workers pushed, in the order of its ticket
        const uint32_t submit_version_mask = work_current()->version_mask;
        FoundShare share;
        while (share_taken(share_taken_ticket, &share))
        {
            share_taken_ticket += 1u;
            char nonce_hex[9];
            hex_from_word(share.nonce, nonce_hex);
            {
                VerbaLinea line = mmgr_line_over(payload, sizeof(payload));
                mmgr_line_put(&line, "{\"id\":");
                mmgr_line_decimal(&line, next_submit_id, 1u);
                mmgr_line_put(&line, ",\"method\":\"mining.submit\",\"params\":[\"");
                mmgr_line_put(&line, stratum_identity());
                mmgr_line_put(&line, "\",\"");
                mmgr_line_put(&line, share.job_id);
                mmgr_line_put(&line, "\",\"");
                mmgr_line_put(&line, share.extranonce2_hex);
                mmgr_line_put(&line, "\",\"");
                mmgr_line_put(&line, share.ntime_hex);
                mmgr_line_put(&line, "\",\"");
                mmgr_line_put(&line, nonce_hex);
                mmgr_line_put(&line, "\"");
                if (submit_version_mask != 0u)
                {
                    // BIP310: the sixth parameter carries ONLY the bits that the miner rolled, masked to
                    // the granted bits. The full version sets bits outside the mask (0x20000000 of the job
                    // among them), and the pool rejects it as SInvalidVersion. The pool reconstructs the
                    // header as (job_version & ~mask) | this: the header that the miner hashed.
                    char rolled_hex[9];
                    hex_from_word(share.version & submit_version_mask, rolled_hex);
                    mmgr_line_put(&line, ",\"");
                    mmgr_line_put(&line, rolled_hex);
                    mmgr_line_put(&line, "\"");
                }
                mmgr_line_put(&line, "]}\r\n");
                (void)mmgr_line_finish(&line);
            }
            next_submit_id += 1u;
            submitted += 1u;

            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "\n[SUBMIT] nonce ");
                mmgr_line_put(output_line, nonce_hex);
                mmgr_line_put(output_line, "  hash ");
                mmgr_line_put(output_line, share.block_hash);
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            if (share.meets_block_target)
            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "[BLOCK] this hash meets the network target.");
                mmgr_line_ch(output_line, '\n');
                platform_line_out_ended();
            }
            send_payload(connection, payload);
        }

        const unsigned long long now = platform_clock_ns();
        if ((double)(now - last_report) / 1.0e9 >= 2.0)
        {
            const double elapsed = (double)(now - started) / 1.0e9;
            const uint64_t hashes = counter_read(&g_state.hashes_done);
            const uint64_t anchors = counter_read(&g_state.anchor_survivors);
            const double rate = (elapsed > 0.0) ? ((double)hashes / elapsed) : 0.0;

            // The window rate. It says whether the device is healthy *now*. A lifetime average takes
            // many minutes to recover after another program shares the device for a few minutes.
            // Thus it reports a fault long after the fault ends, and hides a fault that just began.
            const double window_seconds = (double)(now - last_report) / 1.0e9;
            const double window_rate = (window_seconds > 0.0) ? ((double)(hashes - last_hashes) / window_seconds) : 0.0;

            // The anchor count is a live correctness check, worth a display against its expected
            // value. A survivor is a one in 2^32 event. Thus the ratio of seen to expected stays
            // near one while the digests are right. It walks off immediately if a kernel change
            // breaks one.
            const double anchors_due = (double)(lifetime.hashes + hashes) / 4294967296.0;
            const double anchor_ratio =
                (anchors_due > 0.0) ? ((double)(lifetime.anchors + anchors) / anchors_due) : 0.0;

            {
                VerbaLinea *const output_line = platform_line_out();
                mmgr_line_put(output_line, "\r[");
                mmgr_line_fixed(output_line, (double)(elapsed), 0u, 0u, EMBED_FALSE);
                mmgr_line_put(output_line, "s] now ");
                mmgr_line_fixed(output_line, (double)(window_rate / 1.0e6), 0u, 0u, EMBED_FALSE);
                mmgr_line_put(output_line, "  run ");
                mmgr_line_fixed(output_line, (double)(rate / 1.0e6), 0u, 0u, EMBED_FALSE);
                mmgr_line_put(output_line, " MH/s  h ");
                mmgr_line_decimal(output_line, (unsigned long long)hashes, 1u);
                mmgr_line_put(output_line, "  anc ");
                mmgr_line_decimal(output_line, (unsigned long long)anchors, 1u);
                mmgr_line_put(output_line, " (");
                mmgr_line_fixed(output_line, (double)(anchor_ratio), 2u, 0u, EMBED_FALSE);
                mmgr_line_put(output_line, "x due)  sh ");
                mmgr_line_decimal(output_line, (unsigned long long)accepted, 1u);
                mmgr_line_put(output_line, "/");
                mmgr_line_decimal(output_line, (unsigned long long)submitted, 1u);
                mmgr_line_put(output_line, "  | life ");
                mmgr_line_fixed(output_line, (double)((lifetime.seconds + elapsed) / 3600.0), 1u, 0u, EMBED_FALSE);
                mmgr_line_put(output_line, "h  ");
                mmgr_line_fixed(output_line, (double)((double)(lifetime.hashes + hashes) / 1.0e12), 2u, 0u,
                                EMBED_FALSE);
                mmgr_line_put(output_line, " TH  anc ");
                mmgr_line_decimal(output_line, (unsigned long long)(lifetime.anchors + anchors), 1u);
                mmgr_line_put(output_line, "  sh ");
                mmgr_line_decimal(output_line, (unsigned long long)(lifetime.accepted + accepted), 1u);
                mmgr_line_put(output_line, "/");
                mmgr_line_decimal(output_line, (unsigned long long)(lifetime.submitted + submitted), 1u);
                mmgr_line_put(output_line, " -");
                mmgr_line_decimal(output_line, (unsigned long long)(lifetime.rejected + rejected), 1u);
                mmgr_line_put(output_line, "   ");
                platform_line_out_ended();
            }
            platform_line_flushed();

            // Saved at every report, and not at exit. A kill ends a miner far more often than a
            // request to stop. Totals that survive only a clean shutdown do not survive.
            LifetimeTotals writing = lifetime;
            writing.hashes += hashes;
            writing.anchors += anchors;
            writing.submitted += submitted;
            writing.accepted += accepted;
            writing.rejected += rejected;
            writing.seconds += elapsed;
            save_totals(writing);

            last_hashes = hashes;
            last_report = now;
        }
    }

    counter_set(&g_state.running, 0u);
    g_state.abandon = 1;
    for (unsigned index = 0u; index < started_workers; index += 1u)
    {
        platform_worker_joined(&g_workers[index]);
    }

    closesocket(connection);
    WSACleanup();
    return 0;
}
