# SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
# measuring_stick.py: the measuring stick, every function of the CUDA language each in a kernel of its own, written to
# one .cu file and a manifest. Each category is taken whole: every C++ operator on every type it applies to, every
# conversion between two types, every statement, every memory space, every built-in variable, every way a device
# function is called, and the integer, type casting, single and double precision intrinsics, the math library, the
# atomics, the warp functions and the synchronization functions as the CUDA Math API and the CUDA C++ Programming
# Guide list them. The kernels are numbered in the order the tables give them, and the manifest names each number's
# function as its C text.
#
# Every kernel has the one arity: (const unsigned long long *in, unsigned long long *out, unsigned int count). A thread
# past count returns; each thread reads its operands as in[4 . thread + k], each turned to its type as a cast does,
# and writes its result to out[thread], an integer widened and a floating value by its bits. The frame around a
# function is the same text in every kernel of the same types, and what differs between two kernels is the function.
#
#     python measuring_stick.py <stick .cu> <manifest>
import sys

# the types, each with its name in C, whether it is an integer, and whether it is floating
TYPES = [
    ("bool", "bool"),
    ("signed char", "integer"),
    ("unsigned char", "integer"),
    ("short", "integer"),
    ("unsigned short", "integer"),
    ("int", "integer"),
    ("unsigned int", "integer"),
    ("long long", "integer"),
    ("unsigned long long", "integer"),
    ("float", "floating"),
    ("double", "floating"),
]
INTEGERS = [name for name, kind in TYPES if kind == "integer"]
FLOATING = [name for name, kind in TYPES if kind == "floating"]
NUMBERS = INTEGERS + FLOATING

# C++'s binary operators, each with the types it applies to
BINARY = [
    ("+", NUMBERS), ("-", NUMBERS), ("*", NUMBERS), ("/", NUMBERS), ("%", INTEGERS),
    ("&", INTEGERS), ("|", INTEGERS), ("^", INTEGERS), ("<<", INTEGERS), (">>", INTEGERS),
    ("==", NUMBERS), ("!=", NUMBERS), ("<", NUMBERS), (">", NUMBERS), ("<=", NUMBERS), (">=", NUMBERS),
    ("&&", NUMBERS), ("||", NUMBERS),
]
COMPARING = {"==", "!=", "<", ">", "<=", ">=", "&&", "||"}
# the compound assignments, each the assignment of a binary operator above
COMPOUND = ["+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<=", ">>="]
# the unary operators, each as an expression over a, and the types it applies to
UNARY = [
    ("-a", NUMBERS), ("+a", NUMBERS), ("~a", INTEGERS), ("!a", NUMBERS),
    ("++a", NUMBERS), ("a++", NUMBERS), ("--a", NUMBERS), ("a--", NUMBERS),
]

# the statements, each over int a and int b, leaving int r
STATEMENTS = [
    "int r = a; if (b > 0) { r = a + 1; }",
    "int r; if (b > 0) { r = a + 1; } else { r = a - 1; }",
    "int r; if (b > 2) { r = a + 3; } else if (b > 1) { r = a + 2; } else if (b > 0) { r = a + 1; } else { r = a; }",
    "int r; switch (b & 7) { case 0: r = a; break; case 1: r = a + 1; break; case 2: r = a * 2; break; "
    "case 3: r = a - 3; break; case 4: r = a ^ 4; break; case 5: r = a | 5; break; case 6: r = a & 6; break; "
    "default: r = -a; break; }",
    "int r; switch (b) { case 1: r = a; break; case 100: r = a + 1; break; case 10000: r = a + 2; break; "
    "case -7: r = a + 3; break; default: r = 0; break; }",
    "int r = 0; switch (b & 3) { case 0: r += a; case 1: r += 1; case 2: r += 2; break; default: r = a; }",
    "int r = 0; for (int i = 0; i < b; i++) { r += a; }",
    "int r = 0; for (int i = 0; i < 8; i++) { r += a >> i; }",
    "int r = 0; int i = 0; while (i < b) { r ^= a + i; i++; }",
    "int r = 0; int i = 0; do { r += a; i++; } while (i < b);",
    "int r = 0; for (int i = 0; i < b; i++) { if (r > a) { break; } r += i; }",
    "int r = 0; for (int i = 0; i < b; i++) { if ((i & 1) != 0) { continue; } r += i; }",
    "int r = 0; for (int i = 0; i < b; i++) { for (int j = 0; j < a; j++) { r += i * j; } }",
    "int r = a; if (b > 0) { goto skip; } r = -a; skip: r += 1;",
    "int r = 0; int i = 0; again: r += a; i++; if (i < b) { goto again; }",
    "int r = a; if (b < 0) { return; } r += b;",
    "int r = (b > 0) ? ((a > 0) ? 1 : 2) : ((a > 0) ? 3 : 4);",
    "int r = ((a != 0) && ((b / a) > 1)) ? 1 : 0;",
    "int r = a; while (r > 1) { r = ((r & 1) != 0) ? ((3 * r) + 1) : (r / 2); if (r == b) { break; } }",
]

# memory spaces and the ways memory is reached, each over int a and int b, leaving int r
MEMORY = [
    "__shared__ int s[256]; s[threadIdx.x & 255u] = a; __syncthreads(); int r = s[(threadIdx.x + 1u) & 255u];",
    "extern __shared__ int d[]; d[threadIdx.x] = a; __syncthreads(); int r = d[threadIdx.x ^ 1u];",
    "int l[16]; for (int i = 0; i < 16; i++) { l[i] = a + i; } int r = l[b & 15];",
    "int r = measuring_stick_constant[b & 15] + a;",
    "int r = (int)__ldg(&in[(4u * thread) + 2u]) + a;",
    "int r = (int)*(volatile const unsigned long long *)&in[(4u * thread) + 2u] + a;",
    "const unsigned long long *p = in + (4u * thread); p += 2; int r = (int)*p + a;",
    "int g[4][4]; for (int i = 0; i < 16; i++) { g[i >> 2][i & 3] = a * i; } int r = g[b & 3][(b >> 2) & 3];",
    "struct { int x; short y; char z; } v; v.x = a; v.y = (short)b; v.z = (char)(a ^ b); int r = v.x + v.y + v.z;",
    "struct { int x; int y; } v[4]; for (int i = 0; i < 4; i++) { v[i].x = a + i; v[i].y = b - i; } "
    "int r = v[b & 3].x * v[a & 3].y;",
    "union { float f; int i; } v; v.i = a; v.f += 1.0f; int r = v.i;",
    "int r = (int)(((const unsigned int *)in)[(8u * thread) + 1u]) + a;",
    "int r = (int)(((const unsigned char *)in)[(32u * thread) + (b & 31)]) + a;",
]

# the built-in variables
BUILT_IN = [
    "threadIdx.x", "threadIdx.y", "threadIdx.z", "blockIdx.x", "blockIdx.y", "blockIdx.z",
    "blockDim.x", "blockDim.y", "blockDim.z", "gridDim.x", "gridDim.y", "gridDim.z", "warpSize",
]

# the ways a device function is called, each over int a and int b, leaving int r
CALLS = [
    "int r = measuring_stick_noinline(a, b);",
    "int r = measuring_stick_forceinline(a, b);",
    "int r = measuring_stick_recursive(a & 15, b);",
    "int (*f)(int, int) = ((b & 1) != 0) ? measuring_stick_noinline : measuring_stick_other; int r = f(a, b);",
]

# a function and the types of its operands and result, each a type's name in C or a pointer to one
I, U, LL, ULL, F, D = "int", "unsigned int", "long long", "unsigned long long", "float", "double"

# the integer intrinsics (CUDA Math API, Integer Intrinsics)
INTEGER_INTRINSICS = [
    ("__brev", U, [U]), ("__brevll", ULL, [ULL]), ("__byte_perm", U, [U, U, U]), ("__clz", I, [I]),
    ("__clzll", I, [LL]), ("__ffs", I, [I]), ("__ffsll", I, [LL]), ("__fns", U, [U, U, I]),
    ("__funnelshift_l", U, [U, U, U]), ("__funnelshift_lc", U, [U, U, U]), ("__funnelshift_r", U, [U, U, U]),
    ("__funnelshift_rc", U, [U, U, U]), ("__hadd", I, [I, I]), ("__mul24", I, [I, I]),
    ("__mul64hi", LL, [LL, LL]), ("__mulhi", I, [I, I]), ("__popc", I, [U]), ("__popcll", I, [ULL]),
    ("__rhadd", I, [I, I]), ("__sad", U, [I, I, U]), ("__uhadd", U, [U, U]), ("__umul24", U, [U, U]),
    ("__umul64hi", ULL, [ULL, ULL]), ("__umulhi", U, [U, U]), ("__urhadd", U, [U, U]), ("__usad", U, [U, U, U]),
    ("min", I, [I, I]), ("min", U, [U, U]), ("min", LL, [LL, LL]), ("min", ULL, [ULL, ULL]),
    ("max", I, [I, I]), ("max", U, [U, U]), ("max", LL, [LL, LL]), ("max", ULL, [ULL, ULL]),
    ("abs", I, [I]), ("labs", "long", ["long"]), ("llabs", LL, [LL]),
]

ROUNDINGS = ["rd", "rn", "ru", "rz"]


def rounded(stem, result, operands, roundings=ROUNDINGS):
    return [(stem + "_" + rounding, result, operands) for rounding in roundings]


# the type casting intrinsics (CUDA Math API, Type Casting Intrinsics)
CASTING_INTRINSICS = (
    rounded("__double2float", F, [D]) + [("__double2hiint", I, [D])] + rounded("__double2int", I, [D])
    + rounded("__double2ll", LL, [D]) + [("__double2loint", I, [D])] + rounded("__double2uint", U, [D])
    + rounded("__double2ull", ULL, [D]) + [("__double_as_longlong", LL, [D])] + rounded("__float2int", I, [F])
    + rounded("__float2ll", LL, [F]) + rounded("__float2uint", U, [F]) + rounded("__float2ull", ULL, [F])
    + [("__float_as_int", I, [F]), ("__float_as_uint", U, [F]), ("__hiloint2double", D, [I, I]),
       ("__int2double_rn", D, [I])] + rounded("__int2float", F, [I]) + [("__int_as_float", F, [I])]
    + rounded("__ll2double", D, [LL]) + rounded("__ll2float", F, [LL]) + [("__longlong_as_double", D, [LL]),
                                                                           ("__uint2double_rn", D, [U])]
    + rounded("__uint2float", F, [U]) + [("__uint_as_float", F, [U])] + rounded("__ull2double", D, [ULL])
    + rounded("__ull2float", F, [ULL])
)

# the single precision intrinsics (CUDA Math API, Single Precision Intrinsics)
SINGLE_INTRINSICS = (
    [("__cosf", F, [F]), ("__exp10f", F, [F]), ("__expf", F, [F])] + rounded("__fadd", F, [F, F])
    + rounded("__fdiv", F, [F, F]) + [("__fdividef", F, [F, F])] + rounded("__fmaf", F, [F, F, F])
    + rounded("__fmul", F, [F, F]) + rounded("__frcp", F, [F]) + [("__frsqrt_rn", F, [F])]
    + rounded("__fsqrt", F, [F]) + rounded("__fsub", F, [F, F])
    + [("__log10f", F, [F]), ("__log2f", F, [F]), ("__logf", F, [F]), ("__powf", F, [F, F]),
       ("__saturatef", F, [F]), ("__sincosf", "void", [F, "float *", "float *"]), ("__sinf", F, [F]),
       ("__tanf", F, [F])]
)

# the double precision intrinsics (CUDA Math API, Double Precision Intrinsics)
DOUBLE_INTRINSICS = (
    rounded("__dadd", D, [D, D]) + rounded("__ddiv", D, [D, D]) + rounded("__dmul", D, [D, D])
    + rounded("__drcp", D, [D]) + rounded("__dsqrt", D, [D]) + rounded("__dsub", D, [D, D])
    + rounded("__fma", D, [D, D, D])
)

# the single precision math functions (CUDA Math API, Single Precision Mathematical Functions), each by its operands:
# one float, two, three, or another shape given whole
SINGLE_ONE = [
    "acosf", "acoshf", "asinf", "asinhf", "atanf", "atanhf", "cbrtf", "ceilf", "cosf", "coshf", "cospif",
    "cyl_bessel_i0f", "cyl_bessel_i1f", "erfcf", "erfcinvf", "erfcxf", "erff", "erfinvf", "exp10f", "exp2f", "expf",
    "expm1f", "fabsf", "floorf", "j0f", "j1f", "lgammaf", "log10f", "log1pf", "log2f", "logbf", "logf",
    "nearbyintf", "normcdff", "normcdfinvf", "rcbrtf", "rintf", "roundf", "rsqrtf", "sinf", "sinhf", "sinpif",
    "sqrtf", "tanf", "tanhf", "tgammaf", "truncf", "y0f", "y1f",
]
SINGLE_TWO = [
    "atan2f", "copysignf", "fdimf", "fdividef", "fmaxf", "fminf", "fmodf", "hypotf", "nextafterf", "powf",
    "remainderf", "rhypotf",
]
SINGLE_THREE = ["fmaf", "norm3df", "rnorm3df"]
SINGLE_OTHER = [
    ("frexpf", F, [F, "int *"]), ("ilogbf", I, [F]), ("jnf", F, [I, F]), ("ldexpf", F, [F, I]),
    ("llrintf", LL, [F]), ("llroundf", LL, [F]), ("lrintf", "long", [F]), ("lroundf", "long", [F]),
    ("modff", F, [F, "float *"]), ("norm4df", F, [F, F, F, F]), ("normf", F, [I, "const float *"]),
    ("remquof", F, [F, F, "int *"]), ("rnorm4df", F, [F, F, F, F]), ("rnormf", F, [I, "const float *"]),
    ("scalblnf", F, [F, "long"]), ("scalbnf", F, [F, I]), ("sincosf", "void", [F, "float *", "float *"]),
    ("sincospif", "void", [F, "float *", "float *"]), ("ynf", F, [I, F]), ("isfinite", "bool", [F]),
    ("isinf", "bool", [F]), ("isnan", "bool", [F]), ("signbit", "bool", [F]),
]

# the double precision math functions (CUDA Math API, Double Precision Mathematical Functions)
DOUBLE_ONE = [name[:-1] for name in SINGLE_ONE if name not in ("fdividef",)]
DOUBLE_TWO = [name[:-1] for name in SINGLE_TWO if name not in ("fdividef",)]
DOUBLE_THREE = ["fma", "norm3d", "rnorm3d"]
DOUBLE_OTHER = [
    ("frexp", D, [D, "int *"]), ("ilogb", I, [D]), ("jn", D, [I, D]), ("ldexp", D, [D, I]), ("llrint", LL, [D]),
    ("llround", LL, [D]), ("lrint", "long", [D]), ("lround", "long", [D]), ("modf", D, [D, "double *"]),
    ("norm4d", D, [D, D, D, D]), ("norm", D, [I, "const double *"]), ("remquo", D, [D, D, "int *"]),
    ("rnorm4d", D, [D, D, D, D]), ("rnorm", D, [I, "const double *"]), ("scalbln", D, [D, "long"]),
    ("scalbn", D, [D, I]), ("sincos", "void", [D, "double *", "double *"]),
    ("sincospi", "void", [D, "double *", "double *"]), ("yn", D, [I, D]), ("isfinite", "bool", [D]),
    ("isinf", "bool", [D]), ("isnan", "bool", [D]), ("signbit", "bool", [D]),
]

# the atomic functions (CUDA C++ Programming Guide, Atomic Functions), each with its operand type
ATOMICS = (
    [("atomicAdd", t) for t in (I, U, ULL, F, D)] + [("atomicSub", t) for t in (I, U)]
    + [("atomicExch", t) for t in (I, U, ULL, F)] + [("atomicMin", t) for t in (I, U, LL, ULL)]
    + [("atomicMax", t) for t in (I, U, LL, ULL)] + [("atomicInc", U), ("atomicDec", U)]
    + [("atomicCAS", t) for t in (I, U, ULL)] + [("atomicAnd", t) for t in (I, U, ULL)]
    + [("atomicOr", t) for t in (I, U, ULL)] + [("atomicXor", t) for t in (I, U, ULL)]
)

# the warp functions (CUDA C++ Programming Guide, Warp Vote, Warp Match, Warp Reduce and Warp Shuffle Functions),
# each over int a and int b, leaving r
WARP = [
    "int r = __all_sync(0xffffffffu, a > b);", "int r = __any_sync(0xffffffffu, a > b);",
    "unsigned int r = __ballot_sync(0xffffffffu, a > b);", "unsigned int r = __activemask();",
    "unsigned int r = __match_any_sync(0xffffffffu, a);", "int p; unsigned int r = __match_all_sync(0xffffffffu, a, &p);",
    "unsigned int r = __reduce_add_sync(0xffffffffu, (unsigned int)a);",
    "int r = __reduce_min_sync(0xffffffffu, a);", "int r = __reduce_max_sync(0xffffffffu, a);",
    "unsigned int r = __reduce_and_sync(0xffffffffu, (unsigned int)a);",
    "unsigned int r = __reduce_or_sync(0xffffffffu, (unsigned int)a);",
    "unsigned int r = __reduce_xor_sync(0xffffffffu, (unsigned int)a);",
    "int r = __shfl_sync(0xffffffffu, a, b & 31);", "int r = __shfl_up_sync(0xffffffffu, a, 1);",
    "int r = __shfl_down_sync(0xffffffffu, a, 1);", "int r = __shfl_xor_sync(0xffffffffu, a, 1);",
    "__syncwarp(); int r = a;",
]

# the synchronization and memory fence functions, and the clocks (CUDA C++ Programming Guide)
SYNC = [
    "__syncthreads(); int r = a;", "int r = __syncthreads_count(a > b);", "int r = __syncthreads_and(a > b);",
    "int r = __syncthreads_or(a > b);", "__threadfence(); int r = a;", "__threadfence_block(); int r = a;",
    "__threadfence_system(); int r = a;", "long long r = (long long)clock() + a;", "long long r = clock64() + a;",
]

OPERAND_NAMES = ["a", "b", "c", "e"]


def read(type_name, at):
    """the operand at in[4 . thread + at] turned to `type_name` as a cast does"""
    return "(" + type_name + ")in[(4u * thread) + " + str(at) + "u]"


def written(type_name):
    """the result r written to out[thread]: an integer widened, a floating value by its bits"""
    if type_name == F:
        return "out[thread] = (unsigned long long)__float_as_uint(r);"
    if type_name == D:
        return "out[thread] = (unsigned long long)__double_as_longlong(r);"
    return "out[thread] = (unsigned long long)r;"


class Stick:
    def __init__(self):
        self.kernels = []

    def add(self, category, text, body):
        self.kernels.append((category, text, body))

    def operands(self, types):
        return " ".join("const " + t + " " + OPERAND_NAMES[at] + " = " + read(t, at) + ";"
                        for at, t in enumerate(types))

    def expression(self, category, text, types, result, expression):
        self.add(category, text, self.operands(types) + " const " + result + " r = " + expression + "; "
                 + written(result))

    def function(self, category, name, result, operands):
        """a function called with its operands, each pointer operand given a local of its type"""
        declared = []
        arguments = []
        values = 0
        out_type = None
        for at, operand in enumerate(operands):
            if operand.endswith("*"):
                base = operand.replace("const ", "").replace("*", "").strip()
                local = "o" + str(at)
                declared.append(base + " " + local + "[4] = {" + ", ".join([read(base, k) for k in range(4)]) + "};")
                arguments.append(local)
                out_type = out_type or (base, local)
            else:
                declared.append("const " + operand + " " + OPERAND_NAMES[values] + " = " + read(operand, values) + ";")
                arguments.append(OPERAND_NAMES[values])
                values += 1
        call = name + "(" + ", ".join(arguments) + ")"
        text = result + " " + call
        if result == "void":
            base, local = out_type
            body = " ".join(declared) + " " + call + "; const " + base + " r = " + local + "[0]; " + written(base)
        else:
            body = " ".join(declared) + " const " + result + " r = " + call + "; " + written(result)
        self.add(category, text, body)


def build():
    stick = Stick()
    for operator, types in BINARY:
        for t in types:
            result = "bool" if operator in COMPARING else t
            stick.expression("operator", t + " a " + operator + " b", [t, t], result, "a " + operator + " b")
    for operator in COMPOUND:
        types = dict(BINARY)[operator[:-1]]
        for t in types:
            stick.add("compound", t + " a " + operator + " b",
                      stick.operands([t, t]).replace("const " + t + " a", t + " a", 1) + " a " + operator
                      + " b; const " + t + " r = a; " + written(t))
    for expression, types in UNARY:
        for t in types:
            mutable = expression.startswith(("++", "--")) or expression.endswith(("++", "--"))
            operands = stick.operands([t])
            if mutable:
                operands = operands.replace("const " + t + " a", t + " a", 1)
            result = "bool" if expression == "!a" else t
            stick.add("unary", t + " " + expression, operands + " const " + result + " r = " + expression + "; "
                      + written(result))
    for source, _ in TYPES:
        for target, _ in TYPES:
            if source != target:
                stick.expression("conversion", "(" + target + ")(" + source + ")", [source], target,
                                 "(" + target + ")a")
    for t, _ in TYPES:
        stick.add("conditional", "c ? a : b over " + t,
                  stick.operands([t, t, "bool"]).replace("const bool c", "const bool c", 1)
                  + " const " + t + " r = c ? a : b; " + written(t))
    for text in STATEMENTS:
        stick.add("statement", text, stick.operands([I, I]) + " " + text + " " + written(I))
    for text in MEMORY:
        stick.add("memory", text, stick.operands([I, I]) + " " + text + " " + written(I))
    for name in BUILT_IN:
        stick.add("built-in", name, "const unsigned int r = " + name + "; " + written(U))
    for text in CALLS:
        stick.add("call", text, stick.operands([I, I]) + " " + text + " " + written(I))
    for category, table in (("integer intrinsic", INTEGER_INTRINSICS), ("casting intrinsic", CASTING_INTRINSICS),
                            ("single intrinsic", SINGLE_INTRINSICS), ("double intrinsic", DOUBLE_INTRINSICS)):
        for name, result, operands in table:
            stick.function(category, name, result, operands)
    for names, count, t, category in ((SINGLE_ONE, 1, F, "single math"), (SINGLE_TWO, 2, F, "single math"),
                                      (SINGLE_THREE, 3, F, "single math"), (DOUBLE_ONE, 1, D, "double math"),
                                      (DOUBLE_TWO, 2, D, "double math"), (DOUBLE_THREE, 3, D, "double math")):
        for name in names:
            stick.function(category, name, t, [t] * count)
    for category, table in (("single math", SINGLE_OTHER), ("double math", DOUBLE_OTHER)):
        for name, result, operands in table:
            stick.function(category, name, result, operands)
    for name, t in ATOMICS:
        place = "((" + t + " *)out)"
        if name == "atomicCAS":
            call = name + "(&" + place + "[thread], a, b)"
        else:
            call = name + "(&" + place + "[thread], a)"
        stick.add("atomic", t + " " + name, stick.operands([t, t]) + " const " + t + " r = " + call + "; "
                  + written(t).replace("out[thread] =", "out[thread + count] ="))
    for text in WARP:
        result = text.split(" r = ")[0].split(";")[-1].strip()
        stick.add("warp", text, stick.operands([I, I]) + " " + text + " " + written(result))
    for text in SYNC:
        result = text.split(" r = ")[0].split(";")[-1].strip()
        stick.add("sync", text, stick.operands([I, I]) + " " + text + " " + written(result))
    return stick


# what every kernel shares: the device functions the calls reach and the constant memory read
PRELUDE = """// written by measuring_stick.py whole on every run
#include <cuda_runtime.h>

__constant__ int measuring_stick_constant[16] = {1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610, 987, 1597};

__device__ __noinline__ int measuring_stick_noinline(int a, int b)
{
    return (a * b) + (a ^ b);
}

__device__ __noinline__ int measuring_stick_other(int a, int b)
{
    return (a - b) | (a & b);
}

__device__ __forceinline__ int measuring_stick_forceinline(int a, int b)
{
    return (a * b) + (a ^ b);
}

__device__ __noinline__ int measuring_stick_recursive(int a, int b)
{
    return (a <= 0) ? b : (a + measuring_stick_recursive(a - 1, b));
}
"""


def main():
    if len(sys.argv) != 3:
        sys.stderr.write("measuring_stick.py <stick .cu> <manifest>\n")
        return 2
    stick = build()
    with open(sys.argv[1], "w", newline="\n") as source:
        source.write(PRELUDE)
        for number, (category, text, body) in enumerate(stick.kernels):
            source.write("\n// " + category + ": " + text + "\n")
            source.write("extern \"C\" __global__ void measuring_stick_%04u(const unsigned long long *in, "
                         "unsigned long long *out, unsigned int count)\n{\n" % number)
            source.write("    const unsigned int thread = (blockIdx.x * blockDim.x) + threadIdx.x;\n")
            source.write("    if (thread >= count)\n    {\n        return;\n    }\n")
            source.write("    " + body + "\n}\n")
    with open(sys.argv[2], "w", newline="\n") as manifest:
        manifest.write("number\tcategory\tfunction\n")
        for number, (category, text, _) in enumerate(stick.kernels):
            manifest.write("%04u\t%s\t%s\n" % (number, category, text))
    print("measuring stick: %u kernels" % len(stick.kernels))
    return 0


if __name__ == "__main__":
    sys.exit(main())
