// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// lattice_gl.c: the lattice's GL driver: each slot's lattice drawn through the WebGL context of its canvas, which the
// page holds and calls as this driver asks (lattice/webgl.js), and only on a frame the page asks for, where the loop
// has moved since the last enough to be seen. The module moves every point and shades it (lattice.c) into its dot,
// its place, its radius and its color from the pens; the dots go to the GPU in one buffer, written over in place, and
// the GPU puts each down as it stands: a pixel's share of a dot read in one step from how far the pixel's middle
// stands from the dot's edge, a pixel wide ramp from all of it to none, and the result dithered by a 4 by 4 ordered
// threshold below one level of 255, so a shade between two levels is put down as both. The radius is taken at FITS of
// the pen's, which puts down the ink the page's own canvas puts down for the same dots.
#include "../../cli/src/view/lattice.h"

// the page's WebGL on the context of `slot`'s canvas, every object it makes named by a number above 0: a shader
// compiled from `length` bytes of source, vertex where `vertex` is 1; a program linked from two shaders, 0 where it
// does not link; a buffer; an attribute's location and a uniform, each by its name; `bytes` of dots handed to a
// buffer, written over in place where it holds as many; a frame begun, the whole drawing buffer cleared and the program
// and buffer in use; uniforms set; and `count` dots drawn from the buffer, as LatticeDot lays them, blended over what
// stands
LATTICE_IMPORT(gl_shader) uint32_t lattice_gl_shader(uint32_t slot, uint32_t vertex, const char *source, uint32_t length);
LATTICE_IMPORT(gl_program) uint32_t lattice_gl_program(uint32_t slot, uint32_t vertex, uint32_t fragment);
LATTICE_IMPORT(gl_buffer) uint32_t lattice_gl_buffer(uint32_t slot);
LATTICE_IMPORT(gl_attribute) int32_t lattice_gl_attribute(uint32_t slot, uint32_t program, const char *name, uint32_t length);
LATTICE_IMPORT(gl_uniform) uint32_t lattice_gl_uniform(uint32_t slot, uint32_t program, const char *name, uint32_t length);
LATTICE_IMPORT(gl_points) void lattice_gl_points(uint32_t slot, uint32_t buffer, const LatticeDot *dots, uint32_t bytes);
LATTICE_IMPORT(gl_begin) void lattice_gl_begin(uint32_t slot, uint32_t program, uint32_t buffer);
LATTICE_IMPORT(gl_uniform2f) void lattice_gl_uniform2f(uint32_t slot, uint32_t uniform, double first, double second);
LATTICE_IMPORT(gl_uniform1f) void lattice_gl_uniform1f(uint32_t slot, uint32_t uniform, double value);
LATTICE_IMPORT(gl_draw) void lattice_gl_draw(uint32_t slot, int32_t place, int32_t radius, int32_t shade, uint32_t count);

// a name's bytes and their length, for the page's WebGL to read
#define LATTICE_NAMED(name_) name_, sizeof(name_) - 1u

static const char s_lattice_vertex[] =
    "attribute vec2 place;\n"
    "attribute float dot_radius;\n"
    "attribute vec4 shade;\n"
    "uniform vec2 size;\n"
    "uniform float scale;\n"
    "varying vec4 color;\n"
    "varying vec2 center;\n"
    "varying float radius;\n"
    "void main() {\n"
    "  color = shade;\n"
    "  radius = dot_radius * scale;\n"
    "  center = vec2(place.x, size.y - place.y) * scale;\n"
    "  gl_Position = vec4(place.x / size.x * 2.0 - 1.0, 1.0 - place.y / size.y * 2.0, 0.0, 1.0);\n"
    "  gl_PointSize = ceil(radius * 2.0) + 2.0;\n"
    "}\n";

static const char s_lattice_fragment[] =
    "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
    "precision highp float;\n"
    "#else\n"
    "precision mediump float;\n"
    "#endif\n"
    "varying vec4 color;\n"
    "varying vec2 center;\n"
    "varying float radius;\n"
    "float ordered(vec2 at) {\n"
    "  at = floor(at);\n"
    "  return fract(at.x / 2.0 + at.y * at.y * 0.75);\n"
    "}\n"
    "void main() {\n"
    "  float inside = clamp(radius - length(gl_FragCoord.xy - center) + 0.5, 0.0, 1.0);\n"
    "  if (inside <= 0.0) {\n"
    "    discard;\n"
    "  }\n"
    "  float alpha = color.a * inside;\n"
    "  float threshold = ordered(0.5 * gl_FragCoord.xy) * 0.25 + ordered(gl_FragCoord.xy) - 0.46875;\n"
    "  gl_FragColor = clamp(vec4(color.rgb * alpha, alpha) + threshold / 255.0, 0.0, 1.0);\n"
    "}\n";

// the pens the dots are shaded from, which the page writes in its scheme's colors, and how many times it has; and the
// shades made of them, and for which writing
static LatticePens s_lattice_pens;
static uint32_t s_lattice_pens_written;
static LatticeShades s_lattice_shades;
static uint32_t s_lattice_shades_for = UINT32_MAX;

// the painter of a `slot`: made, or found not to link, its program and buffer, where its attributes and uniforms stand
enum LatticePainterState
{
    LATTICE_UNMADE,
    LATTICE_MADE,
    LATTICE_UNLINKED,
};

typedef struct
{
    enum LatticePainterState state;
    uint32_t program;
    uint32_t buffer;
    int32_t place;
    int32_t radius;
    int32_t shade;
    uint32_t size;
    uint32_t scale;
} LatticePainter;

static LatticePainter s_lattice_painters[LATTICE_SLOTS];

// how many pens the dots are shaded from, which the page writes
LATTICE_EXPORT(lattice_pen_levels) uint32_t lattice_pen_levels(void)
{
    return LATTICE_LEVELS;
}

LATTICE_EXPORT(lattice_pen_colors) float *lattice_pen_colors(void)
{
    return s_lattice_pens.color;
}

LATTICE_EXPORT(lattice_pen_radii) float *lattice_pen_radii(void)
{
    return s_lattice_pens.radius;
}

// the pens written anew: the next dots made are shaded from them
LATTICE_EXPORT(lattice_pens_changed) void lattice_pens_changed(void)
{
    s_lattice_pens_written += 1u;
}

// the shades made from the pens where the pens were written since
static const LatticeShades *lattice_shades(void)
{
    if (s_lattice_shades_for != s_lattice_pens_written)
    {
        lattice_shaded(&s_lattice_pens, &s_lattice_shades);
        s_lattice_shades_for = s_lattice_pens_written;
    }
    return &s_lattice_shades;
}

// the dots of `slot` made, `width` by `height` pixels with the loop at `seconds`: every point moved, then shaded from
// the pens the page wrote. 1, or 0 where `slot` is past the last or the memory cannot hold them
LATTICE_EXPORT(lattice_dots_made) uint32_t lattice_dots_made(uint32_t slot, double width, double height, double seconds)
{
    return (slot < LATTICE_SLOTS) && lattice_dots_moved(lattice_slot(slot), width, height, seconds, lattice_shades());
}

// the painter of `slot` let go with its canvas; the next canvas of `slot` makes its own
LATTICE_EXPORT(lattice_forget) void lattice_forget(uint32_t slot)
{
    if (slot < LATTICE_SLOTS)
    {
        s_lattice_painters[slot] = (LatticePainter){0};
    }
}

// the painter of `slot` made on its canvas's context: its shaders compiled, its program linked, its buffer and the
// places of its attributes and uniforms read; one that does not link is kept as not linking
static void lattice_painter_made(uint32_t slot, LatticePainter *painter)
{
    const uint32_t vertex = lattice_gl_shader(slot, 1u, LATTICE_NAMED(s_lattice_vertex));
    const uint32_t fragment = lattice_gl_shader(slot, 0u, LATTICE_NAMED(s_lattice_fragment));
    const uint32_t program = lattice_gl_program(slot, vertex, fragment);
    if (program == 0u)
    {
        painter->state = LATTICE_UNLINKED;
        return;
    }
    *painter = (LatticePainter){
        .state = LATTICE_MADE,
        .program = program,
        .buffer = lattice_gl_buffer(slot),
        .place = lattice_gl_attribute(slot, program, LATTICE_NAMED("place")),
        .radius = lattice_gl_attribute(slot, program, LATTICE_NAMED("dot_radius")),
        .shade = lattice_gl_attribute(slot, program, LATTICE_NAMED("shade")),
        .size = lattice_gl_uniform(slot, program, LATTICE_NAMED("size")),
        .scale = lattice_gl_uniform(slot, program, LATTICE_NAMED("scale")),
    };
}

// draws `slot`'s lattice, `width` by `height` pixels at `scale` device pixels to one, with the loop at `seconds`: its
// dots made and handed over, then put down. 1 where it was drawn, 0 where `slot` is past the last, its painter does
// not link or the memory cannot hold its dots
LATTICE_EXPORT(lattice_paint) uint32_t lattice_paint(uint32_t slot, double width, double height, double scale, double seconds)
{
    if (slot >= LATTICE_SLOTS)
    {
        return 0u;
    }
    LatticePainter *const painter = &s_lattice_painters[slot];
    if (painter->state == LATTICE_UNMADE)
    {
        lattice_painter_made(slot, painter);
    }
    LatticeSlot *const lattice = lattice_slot(slot);
    if ((painter->state != LATTICE_MADE) || !lattice_dots_moved(lattice, width, height, seconds, lattice_shades()))
    {
        return 0u;
    }
    lattice_gl_points(slot, painter->buffer, lattice->dots, lattice->count * (uint32_t)sizeof(LatticeDot));
    lattice_gl_begin(slot, painter->program, painter->buffer);
    lattice_gl_uniform2f(slot, painter->size, width, height);
    lattice_gl_uniform1f(slot, painter->scale, scale);
    lattice_gl_draw(slot, painter->place, painter->radius, painter->shade, lattice->count);
    return 1u;
}
