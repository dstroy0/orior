// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// identity_internal.h: what the modules of orior_klq_identity share: its includes, every define and struct, the const tables, and the state and functions one module reads from another
#ifndef IDENTITY_INTERNAL_H
#define IDENTITY_INTERNAL_H

#include "../../../../../includes/file_defs/kdm/kdm.h"
#include "../../../../../includes/file_defs/kernel_blob/forms_blob.h"
#include "../../../../../includes/file_defs/kernel_blob/kernel_blob.h"
#include "../../../../../includes/file_defs/kqr/kqr_record.h"
#include "../../../../../includes/file_defs/krf/krf.h"
#include "../../../../../includes/file_defs/ksc/ksc.h"
#include "../../../interface/interface.h"
#include "../../overseer/overseer.h"
#include "../../teacher/run_channel.h"
#include "carrier_flow.h"
#include "code_generator.h"
#include "concept_product.h"
#include "precepts.h"
#include "../../../../../includes/file_defs/qry/writer/qry_member.h"
#include "../../../../../includes/file_defs/klq/query_trace.h"
#include "target_internal.h"
#include "word_web.h"

#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <vector>
#include "../../../../../includes/parsers/who.h"
#include "identity_settings.h"

// the values each operand is put through: zero, the small numbers, and the edges of every width
static const unsigned long long s_values[] = {
    0x0ull,          0x1ull,          0x2ull,          0x3ull,          0x7full,
    0x80ull,         0xffull,         0x100ull,        0x7fffull,       0x8000ull,
    0xffffull,       0x7fffffffull,   0x80000000ull,   0xffffffffull,   0x100000000ull,
    0x7fffffffffffffffull, 0x8000000000000000ull, 0xffffffffffffffffull};

#define IDENTITY_VALUES (sizeof(s_values) / sizeof(s_values[0]))

// the third operand, a test's other predicate where a question reads one
#define IDENTITY_THIRDS 2u

#define IDENTITY_CASES (IDENTITY_VALUES * IDENTITY_VALUES * IDENTITY_THIRDS)

// the blob of the run the host's answers are handed as, host_questions' output, one chain a line
#define IDENTITY_HOST_ANSWERS "host_answers.txt"

// the environment's variable naming the vendor's writer and its words but the machine file and a mode's, apart by
// tabs: the program, the part, the layout and the vendor's table, which the query crate sets

// What a mode writes, held in memory until it is handed to the run's writer whole, the only thing that writes a file:
// where it goes, and its bytes so far. `filed` 1 where it is a current file of the tree, which the writer writes in its
// place (qry_member_filed), and 0 where it is a table of the run's own, a blob of the run named by the path's own name
struct RunWritten
{
    std::string path;
    int filed;
    std::string text;
};

// a link of a listing: as nvcc wrote it, its registers as written, with its registers read as their kind, and its
// position in the chain, the address nvcc wrote beside it, and each register it names with the slot it names it in: g
// for the guard, and each operand by its count from 0
struct Link
{
    std::string exact;
    std::string registers;
    std::string kind;
    std::string position;
    std::vector<std::pair<std::string, std::string>> slots;
    std::string marked;
    std::vector<std::string> named;
    std::vector<std::string> literals;
    std::vector<std::string> targets;
    std::vector<std::string> distances;
};

// one question of the stick: its number, its category and C, its links as nvcc wrote them, each with its count, and
// its chain, every link in the order nvcc wrote it
struct Question
{
    std::string number;
    std::string category;
    std::string text;
    std::map<std::string, unsigned int> exact;
    std::map<std::string, Link> links;
    std::vector<Link> chain;
};

// the first occurrence of a chain identity, the question and the link it starts at, and how often it occurs
struct Occurrence
{
    unsigned int question;
    unsigned int link;
    unsigned int count;
};

// a language's files read and held against each other and its schema
struct HeldSet
{
    RulesetRelationMemory memory;
    RulesetCoreRelations relations;
};

// a member of a slice: its question and the position of its link in that question's chain
struct Member
{
    std::string question;
    std::string position;
};

// a slice carrying an identity: the member writing its first link and the member writing its second, each with its
// context
struct Carrier
{
    Member first;
    Member second;
    std::string first_context;
    std::string second_context;
};

// a question the permutations put: the window it is asked in at its first occurrence, how often it arises, the link
// put in and the link it stands for
struct Permuted
{
    unsigned int question;
    unsigned int link;
    unsigned int count;
    std::string window;
    std::string from;
    std::string to;
};

// what one form's writing came to over the questions nvcc makes in fewer steps: the questions, and its links of ours
// alone there, of a kind nvcc writes nothing of in the question
struct FormFolded
{
    std::set<std::string> questions;
    unsigned int alone;
};

// a field of an instruction as the system's .ksc gives it: its first bit and how many bits it takes
struct StallField
{
    unsigned int first;
    unsigned int bits;
};

// one question the walk puts: a chain of ours, its code with every stall the longest, and the place of the writer
// whose stall is turned, the instruction after it reading what it wrote
struct StallProbe
{
    std::string number;
    std::vector<unsigned char> code;
    size_t writer;
};

// The record of the run channel's asks, the part's .ksc rows
//
//     run <answer> <word> ask <code> <registers> <threads> <blocks> <cases> <host> [<refusal>]
//
// each ask by the hashes of its code, of its cases and of the host's answers to them, and the registers and the shape
// it is put with, which together fix what it comes to. The word is the host's place of the first case apart, or
// ffffffff where every case is alike, and 0 where the part refused it or left nothing. An ask the record holds is not
// put again. A timed ask is put every time, each time a sample of its own, and an ask the gate holds is the host's
// and recorded nowhere. The record is read when a mode opens the run channel and written back when it ends
struct AskRecorded
{
    std::string answer;
    unsigned int word;
    std::string refusal;
};

// ksc_answers_written, the who the code that calls it
#define ksc_answers_write(ksc_, asked_, rows_) ksc_answers_written((ksc_), (asked_), (rows_), WHO_HERE)

// A log the query writes, held in memory until it is handed whole to the run as a blob of its .qry
// (qry_member.h): the name its blobs are written under and the bytes not yet handed
struct AskLog
{
    const char *name;
    std::string bytes;
};

// One ask a pair's thread hands to the round, 1 where it is dangerous: its stand-in reads or writes memory through an
// address, or sends control elsewhere, and it is carried in a process of its own
struct CarriedAsk
{
    RunQuestion *question;
    int dangerous;
};

// identities ordered as numbers, for a map to find one by
struct IdentityOrder
{
    bool operator()(const KqrIdentity &one, const KqrIdentity &other) const
    {
        return std::tie(one.code, one.registers, one.threads, one.blocks, one.launches, one.cases, one.cases_hash) <
               std::tie(other.code, other.registers, other.threads, other.blocks, other.launches, other.cases,
                        other.cases_hash);
    }
};

// the most questions one process carries, for the memory each holds while it is put
#define STALL_CARRIED_MOST 1024u

// one question of the stall walk: the pair it asks of, the chain whose host answers it is held to, and the stall
struct StallAsk
{
    std::pair<std::string, std::string> pair;
    std::string number;
    unsigned long long step;
};

struct Ruleset;

// the ruleset the identity writes forms in, the latest IDENTITY_RULESET of the run, as its code generator reads it;
// NULL where the run names none or it is not read
static inline const Ruleset *identity_ruleset(void)
{
    static char s_path[IDENTITY_PATH_LONGEST];
    const unsigned char *named = NULL;
    unsigned long long size = 0ull;
    if (!qry_member_blob(IDENTITY_RULESET, &named, &size, NULL) || (size == 0ull) || (size >= sizeof(s_path)))
    {
        return NULL;
    }
    // a path below IDENTITY_PATH_LONGEST bytes, which a size_t counts
    memcpy(s_path, named, (size_t)size);
    s_path[size] = '\0';
    return code_generator(s_path).ruleset(1);
}

// the seed the order of a magnitude's links is drawn from, the latest IDENTITY_SEED of the run; 1 where it names none
static inline unsigned long identity_seed(void)
{
    const unsigned char *held = NULL;
    unsigned long long size = 0ull;
    unsigned long long seed = 1ull;
    if (qry_member_blob(IDENTITY_SEED, &held, &size, NULL) && (size == sizeof(seed)))
    {
        memcpy(&seed, held, sizeof(seed));
    }
    // a seed the crate gives fits the generator's word
    return (unsigned long)seed;
}

struct RegisterProbe
{
    std::string number;
    std::vector<unsigned char> code;
    unsigned long long renamed;
    std::set<unsigned long long> named;
};

// the threads every point of a curve launches in all, and the launches its time is taken over; the asks in a row the
// band of the fewest registers' times holds without widening before it is taken as sustained, and the most asks one
// band or one point's bounces is given
#define CURVE_THREADS (1u << 20u)

#define CURVE_LAUNCHES 100u

#define CURVE_SUSTAIN 5u

#define CURVE_ASKS_MOST 64u

// the times a launch of one point takes over every ask of it: the least and the most
struct CurveBand
{
    unsigned long long low;
    unsigned long long high;
};

// Every carrier of ours the host computes: its text by its question, its links read for what each writes, reads and
// loads with the link writing the register its cases are loaded through, and its vector at its least, the registers its
// chain names, then the operands of a case it loads
struct Carriers
{
    std::map<std::string, std::string> texts;
    std::map<std::string, std::pair<std::vector<CarrierLink>, long>> flows;
    std::map<std::string, std::pair<unsigned int, unsigned int>> magnitudes;
};

// each link a form stands at, in each carrier, with its vector: the carrier's delta before the form, the links
// between a case's load and the form, then the carrier's magnitude
struct FoundLink
{
    const std::pair<const std::string, std::string> *chain;
    std::smatch found;
    std::pair<long, std::pair<unsigned int, unsigned int>> vector;
};

// the times an arrangement is written over itself in one timed block
#define COST_REPEATS 16u

// one arrangement of a .kdm: its line, its fields, its tree with the root last, what came of it, and the least its
// clock read
struct CostRow
{
    size_t line;
    std::vector<std::string> fields;
    std::vector<PreceptNode> nodes;
    std::string verdict;
    unsigned long long least;
};

// the link an operator's arrangements are put at: the word they produce, the link, its two operands and the register
// it writes, the first register past the chain's own, the link's line and its lines, and the chain cut with the clock
// read twice in the link's place
struct OperatorLink
{
    const Word *word;
    FoundLink link;
    std::vector<std::string> operands;
    std::string to;
    unsigned int base;
    long link_at;
    long form_lines;
    std::string clock_cut;
};

extern std::map<std::string, AskRecorded> s_record;
extern std::string s_record_ksc;
extern KqrRecord s_query_record;
extern QueryTrace s_query_trace;
extern thread_local AskLog *s_ask_trace;
extern thread_local AskLog *s_ask_log;
extern AskLog s_trace_whole;
extern AskLog s_log_whole;
extern thread_local int t_pair;
extern thread_local int t_dangerous;
extern QryShared *s_ask_run;
extern thread_local unsigned int s_apart_at;
extern std::mutex s_turn_lock;
extern std::condition_variable s_turn_changed;
extern int s_turn;
extern std::vector<std::vector<CarriedAsk>> s_round_asks;
extern unsigned long long s_round_handed;
extern unsigned long long s_round_held;
extern unsigned long long s_round_processes;
extern thread_local int t_turned;
extern thread_local int t_other_way;

std::string run_blob_text(const char *name);
std::string run_file_text(const std::string &path);
void run_print(RunWritten *written, const char *format, ...);
__attribute__((format(gnu_printf, 2, 3))) void ask_written(AskLog *log, const char *format, ...);
int run_closed(const RunWritten *written);
int rule_broken(const std::string &link);
std::string trimmed(const std::string &text);
int link_read(const std::string &line, Link *link);
int questions_read(const char *listing, const char *manifest, std::vector<Question> *questions);
std::string member_context(const Question &question, const std::string &position);
std::vector<std::string> links_past(const Question &one, const Question &other);
int question_integer(const Question &question);
int host_questions_write(const std::string &path, const std::string &stick, const std::set<std::string> &numbers);
std::string window_render(const Question &question, size_t first, size_t length);
std::vector<unsigned long long> window_identities(const Question &question, size_t first);
std::string address_text(const std::vector<Question> &questions, unsigned int question, unsigned int link);
std::unordered_map<unsigned long long, Occurrence> windows_held(const std::vector<Question> &questions);
int slice_found(const Question &one, const Question &other, const Link **left, const Link **right);
int identity_slice(const char *listing, const char *manifest, const char *stick_path, const std::string &folder, const std::string &ours);
int forms_read(int count, char **files, std::vector<std::pair<std::string, std::vector<std::regex>>> *forms);
std::string forms_writing(const std::vector<std::pair<std::string, std::vector<std::regex>>> &forms, const std::string &kind);
int identity_read(const std::string &folder, const std::string &klq, int count, char **files);
std::vector<std::string> candidate_paths(const std::vector<std::string> &given);
void candidates_read(const std::string &path, std::vector<Question> *candidates);
int identity_known(const char *listing, const char *manifest, const std::string &folder, const std::vector<std::string> &paths);
int identity_permute(const char *listing, const char *manifest, const std::string &folder);
int identity_broken(const char *listing, const char *manifest, const std::string &folder, const std::vector<std::string> &ours_paths, int count, char **files);
unsigned long long stall_bits(const std::vector<unsigned char> &code, size_t index, const StallField &field);
void stall_bits_write(std::vector<unsigned char> *code, size_t index, const StallField &field, unsigned long long value);
std::string stall_operation(const Link &link, std::vector<std::string> *operands);
std::set<std::string> stall_written(const Link &link);
std::set<std::string> stall_read(const Link &link);
std::map<std::string, StallField> stall_fields(const char *ksc);
unsigned int whole_cases(RunQuestion *question, const std::set<unsigned long long> &qualifiers, unsigned int run);
void stall_cases(RunQuestion *question, std::vector<unsigned int> *places);
unsigned long long record_hash(const void *bytes, size_t size, unsigned long long hash);
int ksc_answers_written(const char *ksc, const std::string &asked, const std::vector<std::string> &rows, Who who);
void cuts_asked(const std::vector<std::vector<unsigned char>> &codes, const std::vector<int> &dangerous, const std::vector<unsigned int> &slots, const RunQuestion *question, unsigned long long *asks, unsigned int mark, std::vector<RunQuestion> *asked);
void record_close(void);
void record_open(const char *ksc, const char *mode);
void ask_handed(AskLog *log);
void ask_files_opened(void);
void ask_flushed(AskLog *log);
std::string case_text(unsigned int at);
std::string question_identity(const RunQuestion *question);
void asks_answered(const std::vector<CarriedAsk> &carried);
void asks_carried(RunQuestion *const *asked, const int *dangerous, unsigned int count);
int bracket_holds(const std::string &host, unsigned long long answered);
int question_alike(const std::vector<unsigned char> &code, const std::vector<std::string> &host, const std::vector<unsigned int> &places, RunQuestion *question, unsigned long long *asks);
int stall_alike(const std::vector<unsigned char> &code, const std::vector<std::string> &host, const std::vector<unsigned int> &places, RunQuestion *question, unsigned long long *asks);
std::map<std::string, std::vector<std::string>> host_answers_read(const char *answers);
int chain_code_read(const char *engine, const std::string &number, std::vector<unsigned char> *code, std::vector<Link> *links);
unsigned int chain_registers(const char *engine, const std::string &number);
int ksc_answers_written(const char *ksc, const std::string &asked, const std::vector<std::string> &rows, Who who);
void arrangement_probes(const char *engine, const std::map<std::string, std::vector<std::string>> &host, const std::string &kdm, const Ruleset *sass, const char *machine_path, const StallField &stall, unsigned long long longest, const std::set<std::pair<std::string, std::string>> &held, size_t most, std::vector<StallAsk> *asks, std::vector<std::vector<unsigned char>> *codes);
int identity_stall(const char *engine, const char *answers, const char *ksc, const std::string &folder, const char *machine_path, const char *const *carrier);
long long walk_step(const std::function<int(long long)> &holds, long long from, long long step, long long end);
long long walk_halved(const std::function<int(long long)> &holds, long long truthy, long long falsy);
int identity_frame(const char *engine, const char *answers, const char *ksc, const std::string &folder, const char *const *carrier);
int frame_kernel_read(const unsigned char **code, unsigned int *places, unsigned int *registers);
int question_read(const std::vector<unsigned char> &code, RunQuestion *question, unsigned long long *asks);
int identity_fixed(const char *engine, const char *answers, const char *ksc, const std::string &folder, const char *const *carrier);
int identity_register(const char *engine, const char *answers, const char *ksc, const std::string &folder, const char *const *carrier);
int identity_predicate(const char *engine, const char *answers, const char *ksc, const std::string &folder, const char *const *carrier);
int identity_past(const char *engine, const char *answers, const char *ksc, const std::string &folder, const char *const *carrier);
int identity_curve(const char *engine, const char *answers, const char *ksc, const std::string &folder, const std::vector<std::string> &tasks, const char *const *carrier);
int identity_queue(const char *engine, const char *answers, const char *ksc, const std::string &folder, const char *manifest, const char *const *carrier);
std::string case_text(unsigned int at);
int identity_text(const char *engine, const char *answers, const char *ksc, const char *klq, const char *manifest, const char *const *carrier);
int vector_alike(const std::vector<unsigned char> &code, const std::vector<std::string> &host, const std::vector<unsigned int> &places, RunQuestion *question, unsigned long long *asks, unsigned int mark);
std::string carrier_cut_at_read(const std::vector<CarrierLink> &links, long cases_defined, const std::string &text, long link_at, long form_lines, const std::string &read, const std::string &standing = std::string(), int wide = 0);
std::string read_shape(const std::vector<CarrierLink> &links, const std::vector<std::string> &lines, long cases_defined, const std::string &form_text, long at);
std::vector<std::string> form_tokens(const std::string &text);
int form_unified(const Ruleset *sass, const std::string &to_text, const std::string &from_text, std::map<char, std::string> *bound, std::map<char, std::string> *fixed);
int form_launched(const std::string &text);
std::set<std::string> form_conditions(const std::string &text);
int form_controls(const std::string &text);
int form_addresses(const std::string &text);
int form_dangerous(const std::string &text);
int form_operates(const std::string &text);
int form_commutes(const std::string &text);
int form_verbs(const std::string &text);
int form_const_loaded(const std::string &first, const std::string &second);
int form_switches(const std::string &text);
int form_ranged(const std::string &text);
void carriers_read(const char *engine, const std::map<std::string, std::vector<std::string>> &host, Carriers *carriers);
std::function<std::string(const std::string &)> scratch_past(const Ruleset *sass, unsigned int held, unsigned int *taken);
std::string marked(const Ruleset *sass, const std::string &form, std::vector<std::string> *names);
std::vector<FoundLink> links_of(Carriers &carriers, unsigned long seed, const std::string &text, std::map<char, unsigned int> *group, const std::string &drawn_for);
void cuts_asked(const std::vector<std::vector<unsigned char>> &codes, const std::vector<int> &dangerous, const std::vector<unsigned int> &slots, const RunQuestion *question, unsigned long long *asks, unsigned int mark, std::vector<RunQuestion> *asked);
std::string pair_categories(const Ruleset *sass, const std::string &first, const std::string &second);
std::vector<std::string> pair_beneath(const std::vector<std::string> &bridge, const std::string &first, const std::string &second);
std::string pair_verdicts_joined(const std::string &one_way, const std::string &other_way);
void pair_row_handed(const std::string &first, const std::string &second, int read, unsigned long long reads, const std::string &verdict);
int identity_pair(const char *engine, const char *answers, const char *ksc, const char *klq, const char *machine_path, const char *const *carrier);
const Word *word_produced(const std::vector<PreceptNode> &nodes);
std::string arrangement_written(const Ruleset *sass, const std::vector<PreceptNode> &nodes, const std::vector<std::string> &operands, const std::string &ones, const std::string &to, unsigned int next);
void kdm_rows_read(const char *kdm, std::vector<std::string> *lines, std::vector<CostRow> *rows, std::map<std::string, std::vector<size_t>> *operators);
int identity_cost(const char *engine, const char *answers, const char *ksc, const std::string &folder, const char *kdm, const char *machine_path, const char *forms, const char *const *carrier);

// `texts` put into code by the vendor's writer against the machine file at `machine` (assembled.cu): each text's code,
// empty where a line of it did not assemble or the writer did not run
std::vector<std::vector<unsigned char>> vendor_assembled(const char *machine, const std::vector<std::string> &texts);
std::vector<unsigned char> vendor_assembled(const char *machine, const std::string &text);

#endif
