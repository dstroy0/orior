// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
#include "heaviest_matching.h"
#include "../../../../platform/platform.h"
#include "memoria_operor/memoria_operor.h"

_Static_assert(sizeof(long long) == 8u, "heaviest_matching: long long must be 64 bits, a path cost");

#define MATCHING_UNREACHED 0x7FFFFFFFFFFFFFFFLL

typedef struct
{
    int *head;
    int *next;
    int *target;
    int *capacity;
    long long *cost;
    int edges;
} MatchingGraph;

typedef struct
{
    long long *distance;
    int *node;
    int size;
} MatchingHeap;

static int matching_heap_before(const MatchingHeap *heap, int left, int right)
{
    if (heap->distance[left] != heap->distance[right])
    {
        return (heap->distance[left] < heap->distance[right]) ? 1 : 0;
    }
    return (heap->node[left] < heap->node[right]) ? 1 : 0;
}

static void matching_heap_swap(MatchingHeap *heap, int left, int right)
{
    const long long distance = heap->distance[left];
    const int node = heap->node[left];
    heap->distance[left] = heap->distance[right];
    heap->node[left] = heap->node[right];
    heap->distance[right] = distance;
    heap->node[right] = node;
}

static void matching_heap_push(MatchingHeap *heap, long long distance, int node)
{
    int heap_at = heap->size;
    heap->distance[heap_at] = distance;
    heap->node[heap_at] = node;
    heap->size += 1;
    while (heap_at > 0)
    {
        const int parent = (heap_at - 1) / 2;
        if (matching_heap_before(heap, heap_at, parent) == 0)
        {
            break;
        }
        matching_heap_swap(heap, heap_at, parent);
        heap_at = parent;
    }
}

static void matching_heap_pop(MatchingHeap *heap, long long *distance, int *node)
{
    *distance = heap->distance[0];
    *node = heap->node[0];
    heap->size -= 1;
    heap->distance[0] = heap->distance[heap->size];
    heap->node[0] = heap->node[heap->size];
    int heap_at = 0;
    for (;;)
    {
        const int left = (2 * heap_at) + 1;
        const int right = left + 1;
        int first = heap_at;
        if ((left < heap->size) && (matching_heap_before(heap, left, first) != 0))
        {
            first = left;
        }
        if ((right < heap->size) && (matching_heap_before(heap, right, first) != 0))
        {
            first = right;
        }
        if (first == heap_at)
        {
            break;
        }
        matching_heap_swap(heap, heap_at, first);
        heap_at = first;
    }
}

static void matching_edge(MatchingGraph *graph, int from, int to, long long weight)
{
    const int forward = graph->edges;
    graph->target[forward] = to;
    graph->capacity[forward] = 1;
    graph->cost[forward] = weight;
    graph->next[forward] = graph->head[from];
    graph->head[from] = forward;
    graph->target[forward + 1] = from;
    graph->capacity[forward + 1] = 0;
    graph->cost[forward + 1] = -weight;
    graph->next[forward + 1] = graph->head[to];
    graph->head[to] = forward + 1;
    graph->edges += 2;
}

static unsigned int matching_root(unsigned int *parent, unsigned int item)
{
    while (parent[item] != item)
    {
        parent[item] = parent[parent[item]];
        item = parent[item];
    }
    return item;
}

static long matching_component(const HeaviestMatchingRequest *args, const unsigned int *members,
                               unsigned int member_count, int *local, unsigned char *chosen)
{
    int befores = 0;
    int afters = 0;
    for (unsigned int member = 0u; member < member_count; member += 1u)
    {
        const unsigned int pair = members[member];
        if (local[args->before[pair]] < 0)
        {
            befores += 1;
            local[args->before[pair]] = befores;
        }
    }
    for (unsigned int member = 0u; member < member_count; member += 1u)
    {
        const unsigned int pair = members[member];
        const unsigned int heap_at = args->before_count + args->after[pair];
        if (local[heap_at] < 0)
        {
            afters += 1;
            local[heap_at] = befores + afters;
        }
    }
    const int nodes = befores + afters + 2;
    const int sink = nodes - 1;
    const int edge_capacity = 2 * (befores + afters + (int)member_count);

    MatchingGraph graph;
    graph.head = (int *)platform_cell_taken((size_t)nodes * sizeof(int), platform_place_g_program_owner,
                                            PLATFORM_TIER_TEMPORARY);
    graph.next = (int *)platform_cell_taken((size_t)edge_capacity * sizeof(int), platform_place_g_program_owner,
                                            PLATFORM_TIER_TEMPORARY);
    graph.target = (int *)platform_cell_taken((size_t)edge_capacity * sizeof(int), platform_place_g_program_owner,
                                              PLATFORM_TIER_TEMPORARY);
    graph.capacity = (int *)platform_cell_taken((size_t)edge_capacity * sizeof(int), platform_place_g_program_owner,
                                                PLATFORM_TIER_TEMPORARY);
    graph.cost = (long long *)platform_cell_taken((size_t)edge_capacity * sizeof(long long),
                                                  platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    graph.edges = 0;
    long long *const potential =
        (long long *)platform_cell_taken((unsigned long long)((size_t)nodes * sizeof(long long)),
                                         (unsigned int)platform_place_g_engine_owner, PLATFORM_TIER_TEMPORARY);
    long long *const distance =
        (long long *)platform_cell_taken((unsigned long long)((size_t)nodes * sizeof(long long)),
                                         (unsigned int)platform_place_g_engine_owner, PLATFORM_TIER_TEMPORARY);
    int *const arrived =
        (int *)platform_cell_taken((unsigned long long)((size_t)nodes * sizeof(int)),
                                   (unsigned int)platform_place_g_engine_owner, PLATFORM_TIER_TEMPORARY);
    unsigned char *const settled = (unsigned char *)platform_cell_taken(
        (unsigned long long)((size_t)nodes), (unsigned int)platform_place_g_engine_owner, PLATFORM_TIER_TEMPORARY);
    MatchingHeap heap;
    heap.distance = (long long *)platform_cell_taken(((size_t)edge_capacity + 1u) * sizeof(long long),
                                                     platform_place_g_program_owner, PLATFORM_TIER_TEMPORARY);
    heap.node = (int *)platform_cell_taken(((size_t)edge_capacity + 1u) * sizeof(int), platform_place_g_program_owner,
                                           PLATFORM_TIER_TEMPORARY);
    heap.size = 0;

    long answer = -1;
    if ((graph.head != NULL) && (graph.next != NULL) && (graph.target != NULL) && (graph.capacity != NULL) &&
        (graph.cost != NULL) && (potential != NULL) && (distance != NULL) && (arrived != NULL) && (settled != NULL) &&
        (heap.distance != NULL) && (heap.node != NULL))
    {
        for (int node = 0; node < nodes; node += 1)
        {
            graph.head[node] = -1;
            potential[node] = 0LL;
        }
        for (int before = 1; before <= befores; before += 1)
        {
            matching_edge(&graph, 0, before, 0LL);
        }
        for (unsigned int member = 0u; member < member_count; member += 1u)
        {
            const unsigned int pair = members[member];
            const int from = local[args->before[pair]];
            const int to = local[args->before_count + args->after[pair]];
            const long long weight = -(long long)args->counts[pair];
            matching_edge(&graph, from, to, weight);
            if (weight < potential[to])
            {
                potential[to] = weight;
            }
        }
        for (int after = befores + 1; after < sink; after += 1)
        {
            matching_edge(&graph, after, sink, 0LL);
            if (potential[after] < potential[sink])
            {
                potential[sink] = potential[after];
            }
        }

        answer = 0;
        for (;;)
        {
            for (int node = 0; node < nodes; node += 1)
            {
                distance[node] = MATCHING_UNREACHED;
                arrived[node] = -1;
                settled[node] = 0u;
            }
            distance[0] = 0LL;
            heap.size = 0;
            matching_heap_push(&heap, 0LL, 0);
            while (heap.size > 0)
            {
                long long reached = 0LL;
                int node = 0;
                matching_heap_pop(&heap, &reached, &node);
                if (settled[node] != 0u)
                {
                    continue;
                }
                settled[node] = 1u;
                for (int edge = graph.head[node]; edge >= 0; edge = graph.next[edge])
                {
                    if (graph.capacity[edge] <= 0)
                    {
                        continue;
                    }
                    const int next = graph.target[edge];
                    const long long candidate = reached + graph.cost[edge] + potential[node] - potential[next];
                    if (candidate < distance[next])
                    {
                        distance[next] = candidate;
                        arrived[next] = edge;
                        matching_heap_push(&heap, candidate, next);
                    }
                }
            }
            if (distance[sink] == MATCHING_UNREACHED)
            {
                break;
            }
            const long long true_cost = distance[sink] - potential[0] + potential[sink];
            if (true_cost >= 0LL)
            {
                break;
            }
            const long long cap = distance[sink];
            for (int node = 0; node < nodes; node += 1)
            {
                potential[node] += (distance[node] < cap) ? distance[node] : cap;
            }
            for (int node = sink; node != 0; node = graph.target[arrived[node] ^ 1])
            {
                graph.capacity[arrived[node]] -= 1;
                graph.capacity[arrived[node] ^ 1] += 1;
            }
            answer += 1;
        }

        for (unsigned int member = 0u; member < member_count; member += 1u)
        {
            const int forward = (2 * befores) + (2 * (int)member);
            chosen[members[member]] = (graph.capacity[forward] == 0) ? 1u : 0u;
        }
    }

    for (unsigned int member = 0u; member < member_count; member += 1u)
    {
        const unsigned int pair = members[member];
        local[args->before[pair]] = -1;
        local[args->before_count + args->after[pair]] = -1;
    }
    platform_cell_released(graph.head, platform_place_g_program_owner);
    platform_cell_released(graph.next, platform_place_g_program_owner);
    platform_cell_released(graph.target, platform_place_g_program_owner);
    platform_cell_released(graph.capacity, platform_place_g_program_owner);
    platform_cell_released(graph.cost, platform_place_g_program_owner);
    platform_cell_released(potential, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(distance, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(arrived, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(settled, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(heap.distance, platform_place_g_program_owner);
    platform_cell_released(heap.node, platform_place_g_program_owner);
    return answer;
}

long heaviest_matching_run(const HeaviestMatchingRequest *args)
{
    if ((args == NULL) ||
        ((args->pairs != 0u) &&
         ((args->before == NULL) || (args->after == NULL) || (args->counts == NULL) || (args->chosen == NULL))) ||
        (args->before_count > 0x3FFFFFFFu) || (args->after_count > 0x3FFFFFFFu) || (args->pairs > 0x3FFFFFFFu))
    {
        return HEAVIEST_MATCHING_ERROR;
    }
    for (unsigned int pair = 0u; pair < args->pairs; pair += 1u)
    {
        if ((args->before[pair] >= args->before_count) || (args->after[pair] >= args->after_count) ||
            (args->counts[pair] == 0u))
        {
            return HEAVIEST_MATCHING_ERROR;
        }
    }
    const size_t objects = (size_t)args->before_count + (size_t)args->after_count;
    unsigned int *const parent =
        (unsigned int *)platform_cell_taken((unsigned long long)((objects + 1u) * sizeof(unsigned int)),
                                            (unsigned int)platform_place_g_engine_owner, PLATFORM_TIER_TEMPORARY);
    unsigned int *const component_of =
        (unsigned int *)platform_cell_taken((unsigned long long)(((size_t)args->pairs + 1u) * sizeof(unsigned int)),
                                            (unsigned int)platform_place_g_engine_owner, PLATFORM_TIER_TEMPORARY);
    unsigned int *const starts = (unsigned int *)platform_cell_cleared(
        (unsigned long long)(objects + 2u), (unsigned long long)(sizeof(unsigned int)),
        (unsigned int)platform_place_g_engine_owner, PLATFORM_TIER_TEMPORARY);
    unsigned int *const members =
        (unsigned int *)platform_cell_taken((unsigned long long)(((size_t)args->pairs + 1u) * sizeof(unsigned int)),
                                            (unsigned int)platform_place_g_engine_owner, PLATFORM_TIER_TEMPORARY);
    int *const local = (int *)platform_cell_taken((unsigned long long)((objects + 1u) * sizeof(int)),
                                                  (unsigned int)platform_place_g_engine_owner, PLATFORM_TIER_TEMPORARY);
    unsigned char *const staged =
        (unsigned char *)platform_cell_cleared((unsigned long long)((size_t)args->pairs + 1u), (unsigned long long)(1u),
                                               (unsigned int)platform_place_g_engine_owner, PLATFORM_TIER_TEMPORARY);
    long answer = HEAVIEST_MATCHING_ERROR;
    if ((parent != NULL) && (component_of != NULL) && (starts != NULL) && (members != NULL) && (local != NULL) &&
        (staged != NULL))
    {
        for (size_t object = 0u; object < objects; object += 1u)
        {
            parent[object] = (unsigned int)object;
            local[object] = -1;
        }
        for (unsigned int pair = 0u; pair < args->pairs; pair += 1u)
        {
            const unsigned int left = matching_root(parent, args->before[pair]);
            const unsigned int right = matching_root(parent, args->before_count + args->after[pair]);
            if (left != right)
            {
                parent[right] = left;
            }
        }
        for (unsigned int pair = 0u; pair < args->pairs; pair += 1u)
        {
            component_of[pair] = matching_root(parent, args->before[pair]);
            starts[component_of[pair] + 1u] += 1u;
        }
        for (size_t object = 0u; object < objects; object += 1u)
        {
            starts[object + 1u] += starts[object];
        }
        unsigned int *const filled =
            (unsigned int *)platform_cell_taken((unsigned long long)((objects + 1u) * sizeof(unsigned int)),
                                                (unsigned int)platform_place_g_engine_owner, PLATFORM_TIER_TEMPORARY);
        if (filled != NULL)
        {
            EMBED_CALL(memor.cpy, MemoriaCfg, .dst = filled, .src = starts,
                       .bytes = (objects + 1u) * sizeof(unsigned int));
            for (unsigned int pair = 0u; pair < args->pairs; pair += 1u)
            {
                members[filled[component_of[pair]]] = pair;
                filled[component_of[pair]] += 1u;
            }
            platform_cell_released(filled, (unsigned int)platform_place_g_engine_owner);
            answer = 0;
            for (size_t object = 0u; (object < objects) && (answer >= 0); object += 1u)
            {
                const unsigned int count = starts[object + 1u] - starts[object];
                if (count == 0u)
                {
                    continue;
                }
                const long chosen = matching_component(args, &members[starts[object]], count, local, staged);
                answer = (chosen < 0) ? HEAVIEST_MATCHING_ERROR : (answer + chosen);
            }
            if (answer >= 0)
            {
                EMBED_CALL(memor.cpy, MemoriaCfg, .dst = args->chosen, .src = staged, .bytes = (size_t)args->pairs);
            }
        }
    }
    platform_cell_released(parent, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(component_of, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(starts, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(members, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(local, (unsigned int)platform_place_g_engine_owner);
    platform_cell_released(staged, (unsigned int)platform_place_g_engine_owner);
    return answer;
}
