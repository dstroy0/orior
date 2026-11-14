// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// The engine view's shaders. Every position is integer arithmetic in doubled voxel units until the clip boundary,
// where one conversion to f32 passes it to the rasterizer. Every pick is a select; nothing is discarded. A primitive
// that should not be drawn is moved outside the clip volume in the vertex stage.

EV.LAYOUT_WORDS = 88;

EV.LAYOUT = [
  "frames_at", "leaves_at", "cells_at", "runs_at", "links_at", "edges_at", "depth", "height",
  "width", "plane", "frame_total", "leaf_total", "leaf_first", "leaf_span", "pow2", "only_chosen",
  "run_first", "run_past", "run_total", "cell_total", "yaw_cos", "yaw_sin", "pitch_cos", "pitch_sin",
  "zoom", "pan_x", "pan_y", "half_w", "half_h", "spread", "frame_now", "progress",
  "z_scale", "min_voxels", "shade_on", "light_x", "light_y", "light_z", "chosen_weight", "glow",
  "status_mask", "palette", "ghost_frames", "ghost_fade", "slice_z", "slice_on", "window_low", "window_high",
  "hover_cell", "body", "slice_x0", "slice_y0", "slice_scale", "raw_on", "link_frames", "soft_radius",
  "soft_alpha", "wall_inset", "wall_tone", "wall_alpha", "wall_on", "slice_w", "slice_h", "slice_follow",
  "map_projection", "light_count", "body_alpha", "map_reach",
  "light0_x", "light0_y", "light0_z", "light0_strength", "light1_x", "light1_y", "light1_z", "light1_strength",
  "light2_x", "light2_y", "light2_z", "light2_strength", "light3_x", "light3_y", "light3_z", "light3_strength",
  "typical_voxels", "slide", "spare86", "spare87",
];

// Fields read as signed integers; every other field is unsigned.
EV.SIGNED = new Set(["yaw_cos", "yaw_sin", "pitch_cos", "pitch_sin", "zoom", "pan_x", "pan_y", "half_w", "half_h",
  "spread", "z_scale", "light_x", "light_y", "light_z", "slice_x0", "slice_y0", "slice_scale", "soft_radius", "slice_w", "slice_h",
  "light0_x", "light0_y", "light0_z", "light1_x", "light1_y", "light1_z", "light2_x", "light2_y", "light2_z",
  "light3_x", "light3_y", "light3_z", "map_reach"]);

EV.PRELUDE = `
struct Layout {
${EV.LAYOUT.map((name) => `  ${name}: ${EV.SIGNED.has(name) ? "i32" : "u32"},`).join("\n")}
}
const STRIDE: u32 = 65535u * 256u;
const HIDDEN: vec4<f32> = vec4<f32>(4.0, 4.0, 4.0, 1.0);
`;

// Each run's leaf: the largest leaf whose first run is at or before the run, by 24 halvings.
EV.RESOLVE = EV.PRELUDE + `
@group(0) @binding(0) var<storage, read> obj: array<u32>;
@group(0) @binding(1) var<uniform> position: Layout;
@group(0) @binding(2) var<storage, read_write> run_leaf: array<u32>;
@compute @workgroup_size(256)
fn main(@builtin(global_invocation_id) id: vec3<u32>) {
  let run = id.x + id.y * STRIDE;
  var leaf = 0u;
  for (var step = 0u; step < 24u; step++) {
    let probe = leaf + ((1u << 23u) >> step);
    let first = obj[position.leaves_at + 4u * min(probe, position.leaf_total - 1u) + 2u];
    leaf = select(leaf, probe, (probe < position.leaf_total) && (first <= run));
  }
  run_leaf[select(position.run_total, run, run < position.run_total)] = leaf;
}`;

EV.CHOSEN = `
fn chosen_bit(cell: u32) -> u32 {
  return (chosen[cell >> 5u] >> (cell & 31u)) & 1u;
}`;

// Each leaf in range: its runs where it is retained, else zero; past the range, zero.
EV.RETAIN = EV.PRELUDE + `
@group(0) @binding(0) var<storage, read> obj: array<u32>;
@group(0) @binding(1) var<uniform> position: Layout;
@group(0) @binding(2) var<storage, read> chosen: array<u32>;
@group(0) @binding(3) var<storage, read_write> prefix: array<u32>;
${EV.CHOSEN}
@compute @workgroup_size(256)
fn main(@builtin(global_invocation_id) id: vec3<u32>) {
  let slot = id.x + id.y * STRIDE;
  let at = position.leaves_at + 4u * (position.leaf_first + slot);
  let cell = obj[at];
  let big = u32(obj[position.cells_at + 4u * cell] >= position.min_voxels);
  let retained = select(1u, chosen_bit(cell), position.only_chosen == 1u) * big * u32(slot < position.leaf_span);
  prefix[select(position.pow2, slot, slot < position.pow2)] = obj[at + 3u] * retained;
}`;

// One Sklansky level: every slot with the level's bit set adds the last slot of the half block below it.
EV.LEVEL = EV.PRELUDE + `
struct Level { bit: u32, spare0: u32, spare1: u32, spare2: u32 }
@group(0) @binding(0) var<uniform> level: Level;
@group(0) @binding(1) var<uniform> position: Layout;
@group(0) @binding(2) var<storage, read_write> prefix: array<u32>;
@compute @workgroup_size(256)
fn main(@builtin(global_invocation_id) id: vec3<u32>) {
  let walk = id.x + id.y * STRIDE;
  let low = (1u << level.bit) - 1u;
  let block = (walk >> level.bit) << (level.bit + 1u);
  let slot = block | (1u << level.bit) | (walk & low);
  let source = block | low;
  prefix[select(position.pow2, slot, slot < position.pow2)] += prefix[min(source, position.pow2)];
}`;

// Each run in range lands at its leaf's retained runs before it plus its place in its leaf; a run not retained lands in slot 0.
EV.SCATTER = EV.PRELUDE + `
@group(0) @binding(0) var<storage, read> obj: array<u32>;
@group(0) @binding(1) var<uniform> position: Layout;
@group(0) @binding(2) var<storage, read> chosen: array<u32>;
@group(0) @binding(3) var<storage, read> prefix: array<u32>;
@group(0) @binding(4) var<storage, read> run_leaf: array<u32>;
@group(0) @binding(5) var<storage, read_write> compact: array<u32>;
${EV.CHOSEN}
@compute @workgroup_size(256)
fn main(@builtin(global_invocation_id) id: vec3<u32>) {
  let run = position.run_first + id.x + id.y * STRIDE;
  let leaf = run_leaf[min(run, position.run_total)];
  let at = position.leaves_at + 4u * leaf;
  let cell = obj[at];
  let big = u32(obj[position.cells_at + 4u * cell] >= position.min_voxels);
  let retained = select(1u, chosen_bit(cell), position.only_chosen == 1u) * big * u32(run < position.run_past);
  let before = prefix[min(leaf - position.leaf_first, position.pow2)] - obj[at + 3u] * retained;
  compact[retained * (before + (run - obj[at + 2u]) + 1u)] = run;
}`;

EV.COUNT = EV.PRELUDE + `
@group(0) @binding(0) var<uniform> position: Layout;
@group(0) @binding(1) var<storage, read> prefix: array<u32>;
@group(0) @binding(2) var<storage, read_write> drawn: array<u32, 8>;
@compute @workgroup_size(1)
fn main() {
  // Two sets of drawIndirect arguments over the same retained runs: 18 vertices for a box's three faces, 6 for a capsule.
  drawn[0] = 18u;
  drawn[1] = prefix[position.pow2 - 1u];
  drawn[2] = 0u;
  drawn[3] = 0u;
  drawn[4] = 6u;
  drawn[5] = prefix[position.pow2 - 1u];
  drawn[6] = 0u;
  drawn[7] = 0u;
}`;
