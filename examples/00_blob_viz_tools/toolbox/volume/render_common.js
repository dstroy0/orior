// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
//
// The drawing shaders: boxes warped out of runs, cells as blobs, links and edges as lines, and the slice.
//
// A frame change is a transition, never a cut. Across one transition the runs of frame t slide by their cell's
// step toward its children and dissolve out, while the runs of frame t + 1 slide in from their parents and dissolve
// in; progress runs 0 to 4096 and is eased by an integer smoothstep. The dissolve grain is a hash of the run.

EV.RENDER_BINDINGS = `
@group(0) @binding(0) var<storage, read> obj: array<u32>;
@group(0) @binding(1) var<uniform> position: Layout;
@group(0) @binding(2) var<storage, read> chosen: array<u32>;
@group(0) @binding(3) var<storage, read> motion: array<i32>;
`;

EV.RENDER_COMMON = `
var<private> OKABE: array<u32, 8> = array<u32, 8>(0xE69F00u, 0x56B4E9u, 0x009E73u, 0xF0E442u, 0x0072B2u, 0xD55E00u, 0xCC79A7u, 0xBBBBBBu);
var<private> RAMP: array<u32, 8> = array<u32, 8>(0x440154u, 0x46327Eu, 0x365C8Du, 0x277F8Eu, 0x1FA187u, 0x4AC16Du, 0xA0DA39u, 0xFDE725u);
var<private> STATUS: array<u32, 5> = array<u32, 5>(0x009E73u, 0xE69F00u, 0xD55E00u, 0xCC79A7u, 0x999999u);

struct Out {
  @builtin(position) place: vec4<f32>,
  @location(0) @interpolate(flat) color: vec4<f32>,
  @location(1) @interpolate(flat) pick: u32,
}
struct Drawn {
  @location(0) color: vec4<f32>,
  @location(1) pick: u32,
}

${EV.CHOSEN}

fn unpack(packed: u32) -> vec3<u32> {
  return vec3<u32>((packed >> 16u) & 255u, (packed >> 8u) & 255u, packed & 255u);
}

fn hue(key: u32) -> vec3<u32> {
  var mixed = key * 2654435761u;
  mixed = mixed ^ (mixed >> 15u);
  mixed = mixed * 2246822519u;
  mixed = mixed ^ (mixed >> 13u);
  return vec3<u32>(64u + (mixed & 191u), 64u + ((mixed >> 8u) & 191u), 64u + ((mixed >> 16u) & 191u));
}

// 0 by cell, 1 by lineage, 2 by volume, 3 lineage in the Okabe-Ito colors that stay apart under color blindness.
fn paint(cell: u32) -> vec3<u32> {
  let root = u32(motion[cell * 8u + 6u]);
  let size = max(obj[position.cells_at + 4u * cell], 1u);
  let level = min(((31u - countLeadingZeros(size)) * 8u) / 23u, 7u);
  let okabe = unpack(OKABE[(root * 2654435761u) >> 29u]);
  return select(select(select(hue(cell), hue(root), position.palette == 1u), unpack(RAMP[level]), position.palette == 2u),
                okabe, position.palette == 3u);
}

fn ease(progress: u32) -> i32 {
  let p = i32(min(progress, 4096u));
  return (((p * p) >> 12u) * (12288 - 2 * p)) >> 12u;
}

// The turn: yaw about z, then pitch about the turned x, both as integers scaled by 2^14.
fn project(x: i32, y: i32, z: i32, frame: u32) -> vec4<f32> {
  let x1 = (position.yaw_cos * x - position.yaw_sin * y) >> 14u;
  let y1 = (position.yaw_sin * x + position.yaw_cos * y) >> 14u;
  let y2 = (position.pitch_cos * y1 - position.pitch_sin * z) >> 14u;
  let z2 = (position.pitch_sin * y1 + position.pitch_cos * z) >> 14u;
  let shift = ((i32(frame) - i32(position.frame_now)) * position.spread) - ((ease(position.progress) * position.spread) >> 12u);
  return vec4<f32>(f32((x1 + shift) * position.zoom + position.pan_x) / f32(position.half_w * 256),
                   f32(z2 * position.zoom + position.pan_y) / f32(position.half_h * 256),
                   f32(8192 + y2) / 16384.0, 1.0);
}

// A cell's step across the transition: toward its children for frame t, in from its parents for frame t + 1.
fn step_of(cell: u32, frame: u32) -> vec3<i32> {
  let at = cell * 8u;
  let eased = ease(position.progress);
  let toward = vec3<i32>(motion[at], motion[at + 1u], (motion[at + 2u] * position.z_scale) / 2);
  let inward = vec3<i32>(motion[at + 3u], motion[at + 4u], (motion[at + 5u] * position.z_scale) / 2);
  let now = frame == position.frame_now;
  let next = frame == position.frame_now + 1u;
  return select(vec3<i32>(0), (toward * eased) / 4096, now) - select(vec3<i32>(0), (inward * (4096 - eased)) / 4096, next);
}

fn centroid(cell: u32) -> vec3<i32> {
  let at = position.cells_at + 4u * cell;
  let size = max(obj[at], 1u);
  let doubled_z = i32((2u * obj[at + 1u] + size) / size);
  return vec3<i32>(i32((2u * obj[at + 3u] + size) / size) - i32(position.width),
                   i32((2u * obj[at + 2u] + size) / size) - i32(position.height),
                   (position.z_scale * (doubled_z - i32(position.depth))) / 2);
}

fn frame_of_cell(cell: u32) -> u32 {
  var frame = 0u;
  for (var step = 0u; step < 8u; step++) {
    let probe = frame + (128u >> step);
    let first = obj[position.frames_at + 10u * min(probe, position.frame_total - 1u) + 5u];
    frame = select(frame, probe, (probe < position.frame_total) && (first <= cell));
  }
  return frame;
}

// Shaded, faded by age, grayed where cells are chosen and this one is not, lifted where it is hovered or glows.
fn lit(cell: u32, frame: u32, shade: u32) -> vec4<f32> {
  let base = (paint(cell) * shade) / 256u;
  let age = select(0u, position.frame_now - frame, frame < position.frame_now);
  let bright = 256u - min(age * position.ghost_fade, 224u);
  let faded = (base * bright) / 256u;
  let picked = chosen_bit(cell);
  let dim = (position.chosen_weight * (1u - picked)) / 1u;
  let gray = vec3<u32>((faded.x + faded.y + faded.z) / 10u);
  let displayed = (faded * (256u - dim) + gray * dim) / 256u;
  let lift = 48u * u32(position.hover_cell == cell + 1u) + 64u * position.glow * picked;
  let raised = min(displayed + vec3<u32>(lift), vec3<u32>(255u));
  return vec4<f32>(vec3<f32>(raised) / 255.0, 1.0);
}
`;
