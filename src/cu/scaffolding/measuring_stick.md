# The measuring stick: nvcc's listing, the answer key

Written by `measuring_stick.sh` whole on every run. Each kernel of the measuring stick (measuring_stick.py), every function of the CUDA language in a frame of its own, is compiled by nvcc for sm_86 and read here. What the engine builds for each function is held against it, a kernel at parity where the engine's code uses the same operations as nvcc's, each as many times.

- kernels: 1016
- instructions: 55072
- operations nvcc writes over the stick: 326, of which sass.krs writes 60

- kernels the engine answers: 452, of which at parity with nvcc: 192; kernels that put a question: 564
- the engine's instructions: 18292 in its lanes' text, 18292 read back by nvdisasm, 18292 of them the operation the text wrote

## The engine against nvcc

Each kernel the engine answers: its record steps, nvcc's instructions and the engine's as nvdisasm reads them back, and the operations only one of the two writes, each by how many more times it writes it.

| kernel | function | steps | nvcc | engine | only nvcc | only the engine |
|---|---|---|---|---|---|---|
| 0000 | `signed char a + b` | 17 | 20 | 19 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1 |
| 0001 | `unsigned char a + b` | 17 | 19 | 19 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1, MOV 1 |
| 0002 | `short a + b` | 17 | 20 | 19 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1 |
| 0003 | `unsigned short a + b` | 17 | 19 | 19 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1, MOV 1 |
| 0004 | `int a + b` | 16 | 18 | 18 |  |  |
| 0005 | `unsigned int a + b` | 16 | 18 | 18 |  |  |
| 0006 | `long long a + b` | 15 | 18 | 18 |  |  |
| 0007 | `unsigned long long a + b` | 15 | 18 | 18 |  |  |
| 0010 | `signed char a - b` | 17 | 20 | 19 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1 |
| 0011 | `unsigned char a - b` | 17 | 19 | 19 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1, MOV 1 |
| 0012 | `short a - b` | 17 | 20 | 19 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1 |
| 0013 | `unsigned short a - b` | 17 | 19 | 19 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1, MOV 1 |
| 0014 | `int a - b` | 16 | 18 | 18 |  |  |
| 0015 | `unsigned int a - b` | 16 | 18 | 18 |  |  |
| 0016 | `long long a - b` | 15 | 18 | 18 |  |  |
| 0017 | `unsigned long long a - b` | 15 | 18 | 18 |  |  |
| 0020 | `signed char a * b` | 17 | 20 | 19 | MOV 1 |  |
| 0021 | `unsigned char a * b` | 17 | 19 | 19 |  |  |
| 0022 | `short a * b` | 17 | 20 | 19 | MOV 1 |  |
| 0023 | `unsigned short a * b` | 17 | 19 | 19 |  |  |
| 0024 | `int a * b` | 16 | 18 | 18 |  |  |
| 0025 | `unsigned int a * b` | 16 | 18 | 18 |  |  |
| 0026 | `long long a * b` | 15 | 20 | 20 |  |  |
| 0027 | `unsigned long long a * b` | 15 | 20 | 20 |  |  |
| 0048 | `signed char a & b` | 17 | 20 | 19 | MOV 1 |  |
| 0049 | `unsigned char a & b` | 17 | 18 | 19 | LDG.E.U8 2, MOV 1, SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, LDG.E 2, LOP3.LUT 1 |
| 0050 | `short a & b` | 17 | 20 | 19 | MOV 1 |  |
| 0051 | `unsigned short a & b` | 17 | 18 | 19 | LDG.E.U16 2, MOV 1, SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, LDG.E 2, LOP3.LUT 1 |
| 0052 | `int a & b` | 16 | 18 | 18 |  |  |
| 0053 | `unsigned int a & b` | 16 | 18 | 18 |  |  |
| 0054 | `long long a & b` | 15 | 18 | 18 |  |  |
| 0055 | `unsigned long long a & b` | 15 | 18 | 18 |  |  |
| 0056 | `signed char a \| b` | 17 | 20 | 19 | MOV 1 |  |
| 0057 | `unsigned char a \| b` | 17 | 18 | 19 | LDG.E.U8 2, MOV 1, SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, LDG.E 2, LOP3.LUT 1 |
| 0058 | `short a \| b` | 17 | 20 | 19 | MOV 1 |  |
| 0059 | `unsigned short a \| b` | 17 | 18 | 19 | LDG.E.U16 2, MOV 1, SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, LDG.E 2, LOP3.LUT 1 |
| 0060 | `int a \| b` | 16 | 18 | 18 |  |  |
| 0061 | `unsigned int a \| b` | 16 | 18 | 18 |  |  |
| 0062 | `long long a \| b` | 15 | 18 | 18 |  |  |
| 0063 | `unsigned long long a \| b` | 15 | 18 | 18 |  |  |
| 0064 | `signed char a ^ b` | 17 | 20 | 19 | MOV 1 |  |
| 0065 | `unsigned char a ^ b` | 17 | 18 | 19 | LDG.E.U8 2, MOV 1, SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, LDG.E 2, LOP3.LUT 1 |
| 0066 | `short a ^ b` | 17 | 20 | 19 | MOV 1 |  |
| 0067 | `unsigned short a ^ b` | 17 | 18 | 19 | LDG.E.U16 2, MOV 1, SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, LDG.E 2, LOP3.LUT 1 |
| 0068 | `int a ^ b` | 16 | 18 | 18 |  |  |
| 0069 | `unsigned int a ^ b` | 16 | 18 | 18 |  |  |
| 0070 | `long long a ^ b` | 15 | 18 | 18 |  |  |
| 0071 | `unsigned long long a ^ b` | 15 | 18 | 18 |  |  |
| 0072 | `signed char a << b` | 18 | 20 | 19 | LDG.E.S8 1, MOV 1 | LDG.E 1 |
| 0073 | `unsigned char a << b` | 18 | 19 | 19 | LDG.E.U8 1 | LDG.E 1 |
| 0074 | `short a << b` | 18 | 20 | 19 | LDG.E.S16 1, MOV 1 | LDG.E 1 |
| 0075 | `unsigned short a << b` | 18 | 19 | 19 | LDG.E.U16 1 | LDG.E 1 |
| 0076 | `int a << b` | 16 | 18 | 18 |  |  |
| 0077 | `unsigned int a << b` | 16 | 18 | 18 |  |  |
| 0078 | `long long a << b` | 15 | 18 | 18 | LDG.E 1 | LDG.E.64 1 |
| 0079 | `unsigned long long a << b` | 15 | 18 | 18 | LDG.E 1 | LDG.E.64 1 |
| 0080 | `signed char a >> b` | 19 | 20 | 19 | MOV 1 |  |
| 0081 | `unsigned char a >> b` | 19 | 19 | 19 |  |  |
| 0082 | `short a >> b` | 19 | 20 | 19 | MOV 1 |  |
| 0083 | `unsigned short a >> b` | 19 | 19 | 19 |  |  |
| 0084 | `int a >> b` | 16 | 18 | 18 |  |  |
| 0085 | `unsigned int a >> b` | 16 | 18 | 18 |  |  |
| 0086 | `long long a >> b` | 15 | 18 | 18 | LDG.E 1 | LDG.E.64 1 |
| 0087 | `unsigned long long a >> b` | 15 | 18 | 18 | LDG.E 1 | LDG.E.64 1 |
| 0088 | `signed char a == b` | 19 | 20 | 19 | LDG.E.U8 2, LOP3.LUT 2 | ISETP.NE.AND 1, LDG.E.S8 2 |
| 0089 | `unsigned char a == b` | 19 | 20 | 19 | LOP3.LUT 2 | ISETP.NE.AND 1 |
| 0090 | `short a == b` | 19 | 21 | 19 | IMAD.MOV.U32 1, LDG.E.U16 2, LOP3.LUT 1, PRMT 1 | LDG.E.S16 2, MOV 1 |
| 0091 | `unsigned short a == b` | 19 | 21 | 19 | IMAD.MOV.U32 1, LOP3.LUT 1, PRMT 1 | MOV 1 |
| 0092 | `int a == b` | 17 | 19 | 19 |  |  |
| 0093 | `unsigned int a == b` | 17 | 19 | 19 |  |  |
| 0094 | `long long a == b` | 17 | 20 | 20 |  |  |
| 0095 | `unsigned long long a == b` | 17 | 20 | 20 |  |  |
| 0098 | `signed char a != b` | 19 | 20 | 19 | LDG.E.U8 2, LOP3.LUT 2 | ISETP.NE.AND 1, LDG.E.S8 2 |
| 0099 | `unsigned char a != b` | 19 | 20 | 19 | LOP3.LUT 2 | ISETP.NE.AND 1 |
| 0100 | `short a != b` | 19 | 21 | 19 | IMAD.MOV.U32 1, LDG.E.U16 2, LOP3.LUT 1, PRMT 1 | LDG.E.S16 2, MOV 1 |
| 0101 | `unsigned short a != b` | 19 | 21 | 19 | IMAD.MOV.U32 1, LOP3.LUT 1, PRMT 1 | MOV 1 |
| 0102 | `int a != b` | 17 | 19 | 19 |  |  |
| 0103 | `unsigned int a != b` | 17 | 19 | 19 |  |  |
| 0104 | `long long a != b` | 17 | 20 | 20 |  |  |
| 0105 | `unsigned long long a != b` | 17 | 20 | 20 |  |  |
| 0108 | `signed char a < b` | 19 | 21 | 19 | IMAD.SHL.U32 1, LDG.E 2, SHF.L.U32 1 | LDG.E.S8 2 |
| 0109 | `unsigned char a < b` | 19 | 19 | 19 |  |  |
| 0110 | `short a < b` | 19 | 21 | 19 | IMAD.U32 1, LDG.E 2, SHF.L.U32 1 | LDG.E.S16 2 |
| 0111 | `unsigned short a < b` | 19 | 19 | 19 |  |  |
| 0112 | `int a < b` | 17 | 19 | 19 |  |  |
| 0113 | `unsigned int a < b` | 17 | 19 | 19 |  |  |
| 0114 | `long long a < b` | 17 | 20 | 20 |  |  |
| 0115 | `unsigned long long a < b` | 17 | 20 | 20 |  |  |
| 0118 | `signed char a > b` | 19 | 21 | 19 | IMAD.SHL.U32 1, LDG.E 2, SHF.L.U32 1 | LDG.E.S8 2 |
| 0119 | `unsigned char a > b` | 19 | 19 | 19 |  |  |
| 0120 | `short a > b` | 19 | 21 | 19 | IMAD.U32 1, LDG.E 2, SHF.L.U32 1 | LDG.E.S16 2 |
| 0121 | `unsigned short a > b` | 19 | 19 | 19 |  |  |
| 0122 | `int a > b` | 17 | 19 | 19 |  |  |
| 0123 | `unsigned int a > b` | 17 | 19 | 19 |  |  |
| 0124 | `long long a > b` | 17 | 20 | 20 |  |  |
| 0125 | `unsigned long long a > b` | 17 | 20 | 20 |  |  |
| 0128 | `signed char a <= b` | 19 | 21 | 19 | IMAD.SHL.U32 1, LDG.E 2, SHF.L.U32 1 | LDG.E.S8 2 |
| 0129 | `unsigned char a <= b` | 19 | 19 | 19 |  |  |
| 0130 | `short a <= b` | 19 | 21 | 19 | IMAD.U32 1, LDG.E 2, SHF.L.U32 1 | LDG.E.S16 2 |
| 0131 | `unsigned short a <= b` | 19 | 19 | 19 |  |  |
| 0132 | `int a <= b` | 17 | 19 | 19 |  |  |
| 0133 | `unsigned int a <= b` | 17 | 19 | 19 |  |  |
| 0134 | `long long a <= b` | 17 | 20 | 20 |  |  |
| 0135 | `unsigned long long a <= b` | 17 | 20 | 20 |  |  |
| 0138 | `signed char a >= b` | 19 | 21 | 19 | IMAD.SHL.U32 1, LDG.E 2, SHF.L.U32 1 | LDG.E.S8 2 |
| 0139 | `unsigned char a >= b` | 19 | 19 | 19 |  |  |
| 0140 | `short a >= b` | 19 | 21 | 19 | IMAD.U32 1, LDG.E 2, SHF.L.U32 1 | LDG.E.S16 2 |
| 0141 | `unsigned short a >= b` | 19 | 19 | 19 |  |  |
| 0142 | `int a >= b` | 17 | 19 | 19 |  |  |
| 0143 | `unsigned int a >= b` | 17 | 19 | 19 |  |  |
| 0144 | `long long a >= b` | 17 | 20 | 20 |  |  |
| 0145 | `unsigned long long a >= b` | 17 | 20 | 20 |  |  |
| 0148 | `signed char a && b` | 20 | 20 | 20 | LDG.E.U8 2 | LDG.E.S8 2 |
| 0149 | `unsigned char a && b` | 20 | 20 | 20 |  |  |
| 0150 | `short a && b` | 20 | 20 | 20 | LDG.E.U16 2 | LDG.E.S16 2 |
| 0151 | `unsigned short a && b` | 20 | 20 | 20 |  |  |
| 0152 | `int a && b` | 18 | 20 | 20 |  |  |
| 0153 | `unsigned int a && b` | 18 | 20 | 20 |  |  |
| 0154 | `long long a && b` | 18 | 22 | 22 | IMAD.MOV.U32 1 | MOV 1 |
| 0155 | `unsigned long long a && b` | 18 | 22 | 22 | IMAD.MOV.U32 1 | MOV 1 |
| 0158 | `signed char a \|\| b` | 20 | 19 | 20 | LDG.E.U8 2, LOP3.LUT 1 | ISETP.NE.AND 1, ISETP.NE.OR 1, LDG.E.S8 2 |
| 0159 | `unsigned char a \|\| b` | 20 | 19 | 20 | LOP3.LUT 1 | ISETP.NE.AND 1, ISETP.NE.OR 1 |
| 0160 | `short a \|\| b` | 20 | 21 | 20 | IMAD.MOV.U32 1, LDG.E.U16 2, LOP3.LUT 1, PRMT 1 | ISETP.NE.OR 1, LDG.E.S16 2, MOV 1 |
| 0161 | `unsigned short a \|\| b` | 20 | 21 | 20 | IMAD.MOV.U32 1, LOP3.LUT 1, PRMT 1 | ISETP.NE.OR 1, MOV 1 |
| 0162 | `int a \|\| b` | 18 | 19 | 20 | LOP3.LUT 1 | ISETP.NE.AND 1, ISETP.NE.OR 1 |
| 0163 | `unsigned int a \|\| b` | 18 | 19 | 20 | LOP3.LUT 1 | ISETP.NE.AND 1, ISETP.NE.OR 1 |
| 0164 | `long long a \|\| b` | 18 | 20 | 22 | LOP3.LUT 2 | ISETP.NE.AND.EX 1, ISETP.NE.OR.EX 1, ISETP.NE.U32.AND 2 |
| 0165 | `unsigned long long a \|\| b` | 18 | 20 | 22 | LOP3.LUT 2 | ISETP.NE.AND.EX 1, ISETP.NE.OR.EX 1, ISETP.NE.U32.AND 2 |
| 0168 | `signed char a += b` | 17 | 20 | 19 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1 |
| 0169 | `unsigned char a += b` | 17 | 19 | 19 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1, MOV 1 |
| 0170 | `short a += b` | 17 | 20 | 19 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1 |
| 0171 | `unsigned short a += b` | 17 | 19 | 19 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1, MOV 1 |
| 0172 | `int a += b` | 16 | 18 | 18 |  |  |
| 0173 | `unsigned int a += b` | 16 | 18 | 18 |  |  |
| 0174 | `long long a += b` | 15 | 18 | 18 |  |  |
| 0175 | `unsigned long long a += b` | 15 | 18 | 18 |  |  |
| 0178 | `signed char a -= b` | 17 | 20 | 19 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1 |
| 0179 | `unsigned char a -= b` | 17 | 19 | 19 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1, MOV 1 |
| 0180 | `short a -= b` | 17 | 20 | 19 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1 |
| 0181 | `unsigned short a -= b` | 17 | 19 | 19 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1, MOV 1 |
| 0182 | `int a -= b` | 16 | 18 | 18 |  |  |
| 0183 | `unsigned int a -= b` | 16 | 18 | 18 |  |  |
| 0184 | `long long a -= b` | 15 | 18 | 18 |  |  |
| 0185 | `unsigned long long a -= b` | 15 | 18 | 18 |  |  |
| 0188 | `signed char a *= b` | 17 | 20 | 19 | MOV 1 |  |
| 0189 | `unsigned char a *= b` | 17 | 19 | 19 |  |  |
| 0190 | `short a *= b` | 17 | 20 | 19 | MOV 1 |  |
| 0191 | `unsigned short a *= b` | 17 | 19 | 19 |  |  |
| 0192 | `int a *= b` | 16 | 18 | 18 |  |  |
| 0193 | `unsigned int a *= b` | 16 | 18 | 18 |  |  |
| 0194 | `long long a *= b` | 15 | 20 | 20 |  |  |
| 0195 | `unsigned long long a *= b` | 15 | 20 | 20 |  |  |
| 0216 | `signed char a &= b` | 17 | 20 | 19 | MOV 1 |  |
| 0217 | `unsigned char a &= b` | 17 | 18 | 19 | LDG.E.U8 2, MOV 1, SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, LDG.E 2, LOP3.LUT 1 |
| 0218 | `short a &= b` | 17 | 20 | 19 | MOV 1 |  |
| 0219 | `unsigned short a &= b` | 17 | 18 | 19 | LDG.E.U16 2, MOV 1, SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, LDG.E 2, LOP3.LUT 1 |
| 0220 | `int a &= b` | 16 | 18 | 18 |  |  |
| 0221 | `unsigned int a &= b` | 16 | 18 | 18 |  |  |
| 0222 | `long long a &= b` | 15 | 18 | 18 |  |  |
| 0223 | `unsigned long long a &= b` | 15 | 18 | 18 |  |  |
| 0224 | `signed char a \|= b` | 17 | 20 | 19 | MOV 1 |  |
| 0225 | `unsigned char a \|= b` | 17 | 18 | 19 | LDG.E.U8 2, MOV 1, SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, LDG.E 2, LOP3.LUT 1 |
| 0226 | `short a \|= b` | 17 | 20 | 19 | MOV 1 |  |
| 0227 | `unsigned short a \|= b` | 17 | 18 | 19 | LDG.E.U16 2, MOV 1, SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, LDG.E 2, LOP3.LUT 1 |
| 0228 | `int a \|= b` | 16 | 18 | 18 |  |  |
| 0229 | `unsigned int a \|= b` | 16 | 18 | 18 |  |  |
| 0230 | `long long a \|= b` | 15 | 18 | 18 |  |  |
| 0231 | `unsigned long long a \|= b` | 15 | 18 | 18 |  |  |
| 0232 | `signed char a ^= b` | 17 | 20 | 19 | MOV 1 |  |
| 0233 | `unsigned char a ^= b` | 17 | 18 | 19 | LDG.E.U8 2, MOV 1, SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, LDG.E 2, LOP3.LUT 1 |
| 0234 | `short a ^= b` | 17 | 20 | 19 | MOV 1 |  |
| 0235 | `unsigned short a ^= b` | 17 | 18 | 19 | LDG.E.U16 2, MOV 1, SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, LDG.E 2, LOP3.LUT 1 |
| 0236 | `int a ^= b` | 16 | 18 | 18 |  |  |
| 0237 | `unsigned int a ^= b` | 16 | 18 | 18 |  |  |
| 0238 | `long long a ^= b` | 15 | 18 | 18 |  |  |
| 0239 | `unsigned long long a ^= b` | 15 | 18 | 18 |  |  |
| 0240 | `signed char a <<= b` | 18 | 20 | 19 | LDG.E.S8 1, MOV 1 | LDG.E 1 |
| 0241 | `unsigned char a <<= b` | 18 | 19 | 19 | LDG.E.U8 1 | LDG.E 1 |
| 0242 | `short a <<= b` | 18 | 20 | 19 | LDG.E.S16 1, MOV 1 | LDG.E 1 |
| 0243 | `unsigned short a <<= b` | 18 | 19 | 19 | LDG.E.U16 1 | LDG.E 1 |
| 0244 | `int a <<= b` | 16 | 18 | 18 |  |  |
| 0245 | `unsigned int a <<= b` | 16 | 18 | 18 |  |  |
| 0246 | `long long a <<= b` | 15 | 18 | 18 | LDG.E 1 | LDG.E.64 1 |
| 0247 | `unsigned long long a <<= b` | 15 | 18 | 18 | LDG.E 1 | LDG.E.64 1 |
| 0248 | `signed char a >>= b` | 19 | 20 | 19 | MOV 1 |  |
| 0249 | `unsigned char a >>= b` | 19 | 19 | 19 |  |  |
| 0250 | `short a >>= b` | 19 | 20 | 19 | MOV 1 |  |
| 0251 | `unsigned short a >>= b` | 19 | 19 | 19 |  |  |
| 0252 | `int a >>= b` | 16 | 18 | 18 |  |  |
| 0253 | `unsigned int a >>= b` | 16 | 18 | 18 |  |  |
| 0254 | `long long a >>= b` | 15 | 18 | 18 | LDG.E 1 | LDG.E.64 1 |
| 0255 | `unsigned long long a >>= b` | 15 | 18 | 18 | LDG.E 1 | LDG.E.64 1 |
| 0256 | `signed char -a` | 16 | 21 | 18 | IMAD.MOV.U32 1, IMAD.X 1, SHF.L.U32 2, SHF.R.S32.HI 1, UMOV 1 | IMAD.SHL.U32 1, MOV 1, PRMT 1 |
| 0257 | `unsigned char -a` | 16 | 18 | 18 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1, MOV 1 |
| 0258 | `short -a` | 16 | 21 | 18 | IMAD.MOV.U32 1, IMAD.X 1, SHF.L.U32 2, SHF.R.S32.HI 1, UMOV 1 | IMAD.SHL.U32 1, MOV 1, PRMT 1 |
| 0259 | `unsigned short -a` | 16 | 18 | 18 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1, MOV 1 |
| 0260 | `int -a` | 15 | 19 | 17 | IADD3.X 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, UMOV 1 | MOV 1, SHF.L.U32 1 |
| 0261 | `unsigned int -a` | 15 | 17 | 17 |  |  |
| 0262 | `long long -a` | 15 | 17 | 19 | SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, MOV 1 |
| 0263 | `unsigned long long -a` | 15 | 17 | 19 | SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, MOV 1 |
| 0266 | `signed char +a` | 15 | 16 | 16 |  |  |
| 0267 | `unsigned char +a` | 15 | 16 | 16 |  |  |
| 0268 | `short +a` | 15 | 16 | 16 |  |  |
| 0269 | `unsigned short +a` | 15 | 16 | 16 |  |  |
| 0270 | `int +a` | 14 | 16 | 16 |  |  |
| 0271 | `unsigned int +a` | 14 | 16 | 16 |  |  |
| 0272 | `long long +a` | 13 | 15 | 15 |  |  |
| 0273 | `unsigned long long +a` | 13 | 15 | 15 |  |  |
| 0276 | `signed char ~a` | 16 | 19 | 18 | MOV 1 |  |
| 0277 | `unsigned char ~a` | 16 | 17 | 18 | LDG.E.U8 1, MOV 1, SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, LDG.E 1, LOP3.LUT 1 |
| 0278 | `short ~a` | 16 | 19 | 18 | MOV 1 |  |
| 0279 | `unsigned short ~a` | 16 | 17 | 18 | LDG.E.U16 1, MOV 1, SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, LDG.E 1, LOP3.LUT 1 |
| 0280 | `int ~a` | 15 | 17 | 17 |  |  |
| 0281 | `unsigned int ~a` | 15 | 17 | 17 |  |  |
| 0282 | `long long ~a` | 14 | 17 | 17 |  |  |
| 0283 | `unsigned long long ~a` | 14 | 17 | 17 |  |  |
| 0284 | `signed char !a` | 17 | 18 | 18 | LDG.E.U8 1 | LDG.E.S8 1 |
| 0285 | `unsigned char !a` | 17 | 18 | 18 |  |  |
| 0286 | `short !a` | 17 | 18 | 18 | LDG.E.U16 1 | LDG.E.S16 1 |
| 0287 | `unsigned short !a` | 17 | 18 | 18 |  |  |
| 0288 | `int !a` | 16 | 18 | 18 |  |  |
| 0289 | `unsigned int !a` | 16 | 18 | 18 |  |  |
| 0290 | `long long !a` | 16 | 19 | 19 |  |  |
| 0291 | `unsigned long long !a` | 16 | 19 | 19 |  |  |
| 0294 | `signed char ++a` | 17 | 18 | 18 | IMAD.MOV.U32 1, LDG.E 1, LEA 1, SHF.L.U32 1, SHF.R.S32.HI 1 | IADD3 1, IMAD.SHL.U32 1, LDG.E.S8 1, MOV 1, PRMT 1 |
| 0295 | `unsigned char ++a` | 17 | 18 | 18 | LDG.E 1 | LDG.E.U8 1 |
| 0296 | `short ++a` | 17 | 18 | 18 | IMAD.MOV.U32 1, LDG.E 1, LEA 1, SHF.L.U32 1, SHF.R.S32.HI 1 | IADD3 1, IMAD.SHL.U32 1, LDG.E.S16 1, MOV 1, PRMT 1 |
| 0297 | `unsigned short ++a` | 17 | 18 | 18 | LDG.E 1 | LDG.E.U16 1 |
| 0298 | `int ++a` | 15 | 17 | 17 |  |  |
| 0299 | `unsigned int ++a` | 15 | 17 | 17 |  |  |
| 0300 | `long long ++a` | 15 | 17 | 19 | SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, MOV 1 |
| 0301 | `unsigned long long ++a` | 15 | 17 | 19 | SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, MOV 1 |
| 0304 | `signed char a++` | 17 | 16 | 18 | MOV 1, SHF.L.U32 1 | IADD3 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, PRMT 1 |
| 0305 | `unsigned char a++` | 17 | 16 | 18 | MOV 1, SHF.L.U32 1 | IADD3 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, LOP3.LUT 1 |
| 0306 | `short a++` | 17 | 16 | 18 | MOV 1, SHF.L.U32 1 | IADD3 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, PRMT 1 |
| 0307 | `unsigned short a++` | 17 | 16 | 18 | MOV 1, SHF.L.U32 1 | IADD3 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, LOP3.LUT 1 |
| 0308 | `int a++` | 15 | 16 | 17 |  | IADD3 1 |
| 0309 | `unsigned int a++` | 15 | 16 | 17 |  | IADD3 1 |
| 0310 | `long long a++` | 15 | 15 | 19 | SHF.L.U32 1 | IADD3 1, IADD3.X 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, MOV 1 |
| 0311 | `unsigned long long a++` | 15 | 15 | 19 | SHF.L.U32 1 | IADD3 1, IADD3.X 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, MOV 1 |
| 0314 | `signed char --a` | 17 | 18 | 18 | IMAD.MOV.U32 1, LDG.E 1, LEA 1, SHF.L.U32 1, SHF.R.S32.HI 1 | IADD3 1, IMAD.SHL.U32 1, LDG.E.S8 1, MOV 1, PRMT 1 |
| 0315 | `unsigned char --a` | 17 | 18 | 18 | LDG.E 1 | LDG.E.U8 1 |
| 0316 | `short --a` | 17 | 18 | 18 | IMAD.MOV.U32 1, LDG.E 1, LEA 1, SHF.L.U32 1, SHF.R.S32.HI 1 | IADD3 1, IMAD.SHL.U32 1, LDG.E.S16 1, MOV 1, PRMT 1 |
| 0317 | `unsigned short --a` | 17 | 18 | 18 | LDG.E 1 | LDG.E.U16 1 |
| 0318 | `int --a` | 15 | 17 | 17 |  |  |
| 0319 | `unsigned int --a` | 15 | 17 | 17 |  |  |
| 0320 | `long long --a` | 15 | 17 | 19 | SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, MOV 1 |
| 0321 | `unsigned long long --a` | 15 | 17 | 19 | SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, MOV 1 |
| 0324 | `signed char a--` | 17 | 16 | 18 | MOV 1, SHF.L.U32 1 | IADD3 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, PRMT 1 |
| 0325 | `unsigned char a--` | 17 | 16 | 18 | MOV 1, SHF.L.U32 1 | IADD3 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, LOP3.LUT 1 |
| 0326 | `short a--` | 17 | 16 | 18 | MOV 1, SHF.L.U32 1 | IADD3 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, PRMT 1 |
| 0327 | `unsigned short a--` | 17 | 16 | 18 | MOV 1, SHF.L.U32 1 | IADD3 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, LOP3.LUT 1 |
| 0328 | `int a--` | 15 | 16 | 17 |  | IADD3 1 |
| 0329 | `unsigned int a--` | 15 | 16 | 17 |  | IADD3 1 |
| 0330 | `long long a--` | 15 | 15 | 19 | SHF.L.U32 1 | IADD3 1, IADD3.X 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, MOV 1 |
| 0331 | `unsigned long long a--` | 15 | 15 | 19 | SHF.L.U32 1 | IADD3 1, IADD3.X 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, MOV 1 |
| 0334 | `(signed char)(bool)` | 17 | 19 | 20 | MOV 1 | PRMT 1, SHF.R.S32.HI 1 |
| 0335 | `(unsigned char)(bool)` | 17 | 19 | 20 |  | LOP3.LUT 1 |
| 0336 | `(short)(bool)` | 17 | 19 | 20 | MOV 1 | PRMT 1, SHF.R.S32.HI 1 |
| 0337 | `(unsigned short)(bool)` | 17 | 19 | 20 |  | LOP3.LUT 1 |
| 0338 | `(int)(bool)` | 16 | 19 | 19 |  |  |
| 0339 | `(unsigned int)(bool)` | 16 | 19 | 19 |  |  |
| 0340 | `(long long)(bool)` | 16 | 19 | 19 |  |  |
| 0341 | `(unsigned long long)(bool)` | 16 | 19 | 19 |  |  |
| 0344 | `(bool)(signed char)` | 17 | 19 | 18 | ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.U8 1 | ISETP.NE.AND 1, LDG.E.S8 1 |
| 0345 | `(unsigned char)(signed char)` | 15 | 16 | 16 |  |  |
| 0346 | `(short)(signed char)` | 16 | 16 | 17 |  | PRMT 1 |
| 0347 | `(unsigned short)(signed char)` | 16 | 17 | 17 |  |  |
| 0348 | `(int)(signed char)` | 15 | 16 | 16 |  |  |
| 0349 | `(unsigned int)(signed char)` | 15 | 16 | 16 |  |  |
| 0350 | `(long long)(signed char)` | 15 | 16 | 16 |  |  |
| 0351 | `(unsigned long long)(signed char)` | 15 | 16 | 16 |  |  |
| 0354 | `(bool)(unsigned char)` | 17 | 19 | 18 | ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1 | ISETP.NE.AND 1 |
| 0355 | `(signed char)(unsigned char)` | 15 | 16 | 16 |  |  |
| 0356 | `(short)(unsigned char)` | 16 | 16 | 17 | MOV 1 | PRMT 1, SHF.R.S32.HI 1 |
| 0357 | `(unsigned short)(unsigned char)` | 16 | 16 | 17 |  | LOP3.LUT 1 |
| 0358 | `(int)(unsigned char)` | 15 | 16 | 16 |  |  |
| 0359 | `(unsigned int)(unsigned char)` | 15 | 16 | 16 |  |  |
| 0360 | `(long long)(unsigned char)` | 15 | 16 | 16 |  |  |
| 0361 | `(unsigned long long)(unsigned char)` | 15 | 16 | 16 |  |  |
| 0364 | `(bool)(short)` | 17 | 18 | 18 | LDG.E.U16 1 | LDG.E.S16 1 |
| 0365 | `(signed char)(short)` | 15 | 16 | 16 |  |  |
| 0366 | `(unsigned char)(short)` | 15 | 16 | 16 |  |  |
| 0367 | `(unsigned short)(short)` | 15 | 16 | 16 |  |  |
| 0368 | `(int)(short)` | 15 | 16 | 16 |  |  |
| 0369 | `(unsigned int)(short)` | 15 | 16 | 16 |  |  |
| 0370 | `(long long)(short)` | 15 | 16 | 16 |  |  |
| 0371 | `(unsigned long long)(short)` | 15 | 16 | 16 |  |  |
| 0374 | `(bool)(unsigned short)` | 17 | 18 | 18 |  |  |
| 0375 | `(signed char)(unsigned short)` | 15 | 16 | 16 |  |  |
| 0376 | `(unsigned char)(unsigned short)` | 15 | 16 | 16 |  |  |
| 0377 | `(short)(unsigned short)` | 15 | 16 | 16 |  |  |
| 0378 | `(int)(unsigned short)` | 15 | 16 | 16 |  |  |
| 0379 | `(unsigned int)(unsigned short)` | 15 | 16 | 16 |  |  |
| 0380 | `(long long)(unsigned short)` | 15 | 16 | 16 |  |  |
| 0381 | `(unsigned long long)(unsigned short)` | 15 | 16 | 16 |  |  |
| 0384 | `(bool)(int)` | 16 | 18 | 18 |  |  |
| 0385 | `(signed char)(int)` | 15 | 16 | 16 |  |  |
| 0386 | `(unsigned char)(int)` | 15 | 16 | 16 |  |  |
| 0387 | `(short)(int)` | 15 | 16 | 16 |  |  |
| 0388 | `(unsigned short)(int)` | 15 | 16 | 16 |  |  |
| 0389 | `(unsigned int)(int)` | 14 | 16 | 16 |  |  |
| 0390 | `(long long)(int)` | 14 | 16 | 16 |  |  |
| 0391 | `(unsigned long long)(int)` | 14 | 16 | 16 |  |  |
| 0394 | `(bool)(unsigned int)` | 16 | 18 | 18 |  |  |
| 0395 | `(signed char)(unsigned int)` | 15 | 16 | 16 |  |  |
| 0396 | `(unsigned char)(unsigned int)` | 15 | 16 | 16 |  |  |
| 0397 | `(short)(unsigned int)` | 15 | 16 | 16 |  |  |
| 0398 | `(unsigned short)(unsigned int)` | 15 | 16 | 16 |  |  |
| 0399 | `(int)(unsigned int)` | 14 | 16 | 16 |  |  |
| 0400 | `(long long)(unsigned int)` | 14 | 16 | 16 |  |  |
| 0401 | `(unsigned long long)(unsigned int)` | 14 | 16 | 16 |  |  |
| 0404 | `(bool)(long long)` | 16 | 19 | 19 |  |  |
| 0405 | `(signed char)(long long)` | 15 | 16 | 17 | LDG.E.S8 1 | LDG.E.64 1, PRMT 1 |
| 0406 | `(unsigned char)(long long)` | 15 | 16 | 17 | LDG.E.U8 1 | LDG.E.64 1, LOP3.LUT 1 |
| 0407 | `(short)(long long)` | 15 | 16 | 17 | LDG.E.S16 1 | LDG.E.64 1, PRMT 1 |
| 0408 | `(unsigned short)(long long)` | 15 | 16 | 17 | LDG.E.U16 1 | LDG.E.64 1, LOP3.LUT 1 |
| 0409 | `(int)(long long)` | 14 | 16 | 16 |  |  |
| 0410 | `(unsigned int)(long long)` | 14 | 16 | 16 |  |  |
| 0411 | `(unsigned long long)(long long)` | 13 | 15 | 15 |  |  |
| 0414 | `(bool)(unsigned long long)` | 16 | 19 | 19 |  |  |
| 0415 | `(signed char)(unsigned long long)` | 15 | 16 | 17 | LDG.E.S8 1 | LDG.E.64 1, PRMT 1 |
| 0416 | `(unsigned char)(unsigned long long)` | 15 | 16 | 17 | LDG.E.U8 1 | LDG.E.64 1, LOP3.LUT 1 |
| 0417 | `(short)(unsigned long long)` | 15 | 16 | 17 | LDG.E.S16 1 | LDG.E.64 1, PRMT 1 |
| 0418 | `(unsigned short)(unsigned long long)` | 15 | 16 | 17 | LDG.E.U16 1 | LDG.E.64 1, LOP3.LUT 1 |
| 0419 | `(int)(unsigned long long)` | 14 | 16 | 16 |  |  |
| 0420 | `(unsigned int)(unsigned long long)` | 14 | 16 | 16 |  |  |
| 0421 | `(long long)(unsigned long long)` | 13 | 15 | 15 |  |  |
| 0444 | `c ? a : b over bool` | 22 | 27 | 27 | IMAD.MOV.U32 2, LOP3.LUT 1 | MOV 2, SEL 1 |
| 0445 | `c ? a : b over signed char` | 21 | 22 | 22 | IMAD.MOV.U32 1, LDG.E.U16 2 | LDG.E.S8 2, SEL 1 |
| 0446 | `c ? a : b over unsigned char` | 21 | 21 | 22 | LDG.E 2 | LDG.E.U8 2, SEL 1 |
| 0447 | `c ? a : b over short` | 21 | 22 | 22 | IMAD.MOV.U32 1, LDG.E.U16 2 | LDG.E.S16 2, SEL 1 |
| 0448 | `c ? a : b over unsigned short` | 21 | 21 | 22 | LDG.E 2 | LDG.E.U16 2, SEL 1 |
| 0449 | `c ? a : b over int` | 18 | 20 | 21 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1, MOV 1, SEL 1 |
| 0450 | `c ? a : b over unsigned int` | 18 | 21 | 21 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1, SEL 1 |
| 0451 | `c ? a : b over long long` | 17 | 21 | 23 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1, MOV 3 |
| 0452 | `c ? a : b over unsigned long long` | 17 | 21 | 23 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1, MOV 3 |
| 0471 | `int r = (b > 0) ? ((a > 0) ? 1 : 2) : ((a > 0) ? 3 : 4);` | 23 | 24 | 25 | IMAD.MOV.U32 3, SHF.L.U32 1 | IMAD.SHL.U32 1, ISETP.GT.AND 1, MOV 1, SEL 1, SHF.R.S32.HI 1 |
| 0488 | `threadIdx.y` | 12 | 14 | 14 |  |  |
| 0489 | `threadIdx.z` | 12 | 14 | 14 |  |  |
| 0491 | `blockIdx.y` | 12 | 15 | 15 |  |  |
| 0492 | `blockIdx.z` | 12 | 15 | 15 |  |  |
| 0493 | `blockDim.x` | 12 | 14 | 14 |  |  |
| 0494 | `blockDim.y` | 12 | 14 | 14 |  |  |
| 0495 | `blockDim.z` | 12 | 14 | 14 |  |  |
| 0496 | `gridDim.x` | 12 | 14 | 14 |  |  |
| 0497 | `gridDim.y` | 12 | 14 | 14 |  |  |
| 0498 | `gridDim.z` | 12 | 14 | 14 |  |  |
| 0499 | `warpSize` | 12 | 14 | 14 |  |  |
| 0913 | `int (a == b) ? a : b` | 18 | 16 | 20 | MOV 1, SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, ISETP.NE.AND 1, LDG.E 2, SEL 1 |
| 0914 | `int (a == b) && (c != 0)` | 20 | 21 | 26 |  | ISETP.NE.AND 1, LOP3.LUT 1, MOV 1, SEL 2 |
| 0915 | `int (a == b) \|\| (c != 0)` | 19 | 21 | 21 | ISETP.EQ.OR 1, ISETP.NE.AND 1 | ISETP.EQ.AND 1, ISETP.NE.OR 1 |
| 0916 | `int (a != b) ? a : b` | 18 | 16 | 20 | MOV 1, SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, ISETP.NE.AND 1, LDG.E 2, SEL 1 |
| 0917 | `int (a != b) && (c != 0)` | 20 | 21 | 26 |  | ISETP.NE.AND 1, LOP3.LUT 1, MOV 1, SEL 2 |
| 0918 | `int (a != b) \|\| (c != 0)` | 19 | 21 | 21 |  |  |
| 0919 | `int (a < b) ? a : b` | 18 | 19 | 20 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1, LDG.E 1, MOV 1 |
| 0920 | `int (a < b) && (c != 0)` | 20 | 21 | 26 |  | ISETP.NE.AND 1, LOP3.LUT 1, MOV 1, SEL 2 |
| 0921 | `int (a < b) \|\| (c != 0)` | 19 | 21 | 21 | ISETP.LT.OR 1, ISETP.NE.AND 1 | ISETP.LT.AND 1, ISETP.NE.OR 1 |
| 0922 | `int (a > b) ? a : b` | 18 | 19 | 20 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1, LDG.E 1, MOV 1 |
| 0923 | `int (a > b) && (c != 0)` | 20 | 21 | 26 |  | ISETP.NE.AND 1, LOP3.LUT 1, MOV 1, SEL 2 |
| 0924 | `int (a > b) \|\| (c != 0)` | 19 | 21 | 21 | ISETP.GT.OR 1, ISETP.NE.AND 1 | ISETP.GT.AND 1, ISETP.NE.OR 1 |
| 0925 | `int (a <= b) ? a : b` | 18 | 19 | 20 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1, LDG.E 1, MOV 1 |
| 0926 | `int (a <= b) && (c != 0)` | 20 | 21 | 26 |  | ISETP.NE.AND 1, LOP3.LUT 1, MOV 1, SEL 2 |
| 0927 | `int (a <= b) \|\| (c != 0)` | 19 | 21 | 21 | ISETP.LE.OR 1, ISETP.NE.AND 1 | ISETP.LE.AND 1, ISETP.NE.OR 1 |
| 0928 | `int (a >= b) ? a : b` | 18 | 19 | 20 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1, LDG.E 1, MOV 1 |
| 0929 | `int (a >= b) && (c != 0)` | 20 | 21 | 26 |  | ISETP.NE.AND 1, LOP3.LUT 1, MOV 1, SEL 2 |
| 0930 | `int (a >= b) \|\| (c != 0)` | 19 | 21 | 21 | ISETP.GE.OR 1, ISETP.NE.AND 1 | ISETP.GE.AND 1, ISETP.NE.OR 1 |
| 0931 | `int a == 0` | 18 | 18 | 20 |  | LDG.E 2 |
| 0932 | `int a != 0` | 18 | 18 | 20 |  | LDG.E 2 |
| 0933 | `int (a != 0) && (c != 0)` | 19 | 20 | 21 |  | LDG.E 1 |
| 0934 | `int (a != 0) \|\| (c != 0)` | 19 | 19 | 21 | LOP3.LUT 1 | ISETP.NE.AND 1, ISETP.NE.OR 1, LDG.E 1 |
| 0935 | `int a < 0` | 18 | 18 | 20 | LDG.E.64 1, LOP3.LUT 1, SHF.R.U64 1 | ISETP.GE.AND 1, LDG.E 3, SEL 1 |
| 0936 | `unsigned int (a == b) ? a : b` | 18 | 16 | 20 | MOV 1, SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, ISETP.NE.AND 1, LDG.E 2, SEL 1 |
| 0937 | `unsigned int (a == b) && (c != 0)` | 20 | 21 | 26 |  | ISETP.NE.AND 1, LOP3.LUT 1, MOV 1, SEL 2 |
| 0938 | `unsigned int (a == b) \|\| (c != 0)` | 19 | 21 | 21 | ISETP.EQ.OR 1, ISETP.NE.AND 1 | ISETP.EQ.AND 1, ISETP.NE.OR 1 |
| 0939 | `unsigned int (a != b) ? a : b` | 18 | 16 | 20 | MOV 1, SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, ISETP.NE.AND 1, LDG.E 2, SEL 1 |
| 0940 | `unsigned int (a != b) && (c != 0)` | 20 | 21 | 26 |  | ISETP.NE.AND 1, LOP3.LUT 1, MOV 1, SEL 2 |
| 0941 | `unsigned int (a != b) \|\| (c != 0)` | 19 | 21 | 21 |  |  |
| 0942 | `unsigned int (a < b) ? a : b` | 18 | 19 | 20 |  | LDG.E 1 |
| 0943 | `unsigned int (a < b) && (c != 0)` | 20 | 21 | 26 |  | ISETP.NE.AND 1, LOP3.LUT 1, MOV 1, SEL 2 |
| 0944 | `unsigned int (a < b) \|\| (c != 0)` | 19 | 21 | 21 | ISETP.LT.U32.OR 1, ISETP.NE.AND 1 | ISETP.LT.U32.AND 1, ISETP.NE.OR 1 |
| 0945 | `unsigned int (a > b) ? a : b` | 18 | 19 | 20 |  | LDG.E 1 |
| 0946 | `unsigned int (a > b) && (c != 0)` | 20 | 21 | 26 |  | ISETP.NE.AND 1, LOP3.LUT 1, MOV 1, SEL 2 |
| 0947 | `unsigned int (a > b) \|\| (c != 0)` | 19 | 21 | 21 | ISETP.GT.U32.OR 1, ISETP.NE.AND 1 | ISETP.GT.U32.AND 1, ISETP.NE.OR 1 |
| 0948 | `unsigned int (a <= b) ? a : b` | 18 | 19 | 20 |  | LDG.E 1 |
| 0949 | `unsigned int (a <= b) && (c != 0)` | 20 | 21 | 26 |  | ISETP.NE.AND 1, LOP3.LUT 1, MOV 1, SEL 2 |
| 0950 | `unsigned int (a <= b) \|\| (c != 0)` | 19 | 21 | 21 | ISETP.LE.U32.OR 1, ISETP.NE.AND 1 | ISETP.LE.U32.AND 1, ISETP.NE.OR 1 |
| 0951 | `unsigned int (a >= b) ? a : b` | 18 | 19 | 20 |  | LDG.E 1 |
| 0952 | `unsigned int (a >= b) && (c != 0)` | 20 | 21 | 26 |  | ISETP.NE.AND 1, LOP3.LUT 1, MOV 1, SEL 2 |
| 0953 | `unsigned int (a >= b) \|\| (c != 0)` | 19 | 21 | 21 | ISETP.GE.U32.OR 1, ISETP.NE.AND 1 | ISETP.GE.U32.AND 1, ISETP.NE.OR 1 |
| 0954 | `unsigned int a == 0` | 18 | 18 | 20 |  | LDG.E 2 |
| 0955 | `unsigned int a != 0` | 18 | 18 | 20 |  | LDG.E 2 |
| 0956 | `unsigned int (a != 0) && (c != 0)` | 19 | 20 | 21 |  | LDG.E 1 |
| 0957 | `unsigned int (a != 0) \|\| (c != 0)` | 19 | 19 | 21 | LOP3.LUT 1 | ISETP.NE.AND 1, ISETP.NE.OR 1, LDG.E 1 |
| 0958 | `long long (a == b) ? a : b` | 17 | 15 | 23 | SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 1, SEL 2 |
| 0959 | `long long (a == b) && (c != 0)` | 20 | 22 | 27 | IMAD.MOV.U32 1 | ISETP.NE.AND 1, LOP3.LUT 1, MOV 2, SEL 2 |
| 0960 | `long long (a == b) \|\| (c != 0)` | 19 | 22 | 22 | IMAD.MOV.U32 1, ISETP.EQ.OR.EX 1, ISETP.NE.AND 1 | ISETP.EQ.AND.EX 1, ISETP.NE.OR 1, MOV 1 |
| 0961 | `long long (a != b) ? a : b` | 17 | 15 | 23 | SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 1, SEL 2 |
| 0962 | `long long (a != b) && (c != 0)` | 20 | 22 | 27 | IMAD.MOV.U32 1 | ISETP.NE.AND 1, LOP3.LUT 1, MOV 2, SEL 2 |
| 0963 | `long long (a != b) \|\| (c != 0)` | 19 | 22 | 22 | IMAD.MOV.U32 1, ISETP.NE.AND 1, ISETP.NE.OR.EX 1 | ISETP.NE.AND.EX 1, ISETP.NE.OR 1, MOV 1 |
| 0964 | `long long (a < b) ? a : b` | 17 | 20 | 23 | IMAD.MOV.U32 1, ISETP.LT.AND.EX 1, ISETP.LT.U32.AND 1, SHF.L.U32 1 | IMAD.SHL.U32 1, ISETP.GE.AND.EX 1, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3 |
| 0965 | `long long (a < b) && (c != 0)` | 20 | 22 | 27 | IMAD.MOV.U32 1 | ISETP.NE.AND 1, LOP3.LUT 1, MOV 2, SEL 2 |
| 0966 | `long long (a < b) \|\| (c != 0)` | 19 | 22 | 22 | IMAD.MOV.U32 1, ISETP.LT.OR.EX 1, ISETP.NE.AND 1 | ISETP.LT.AND.EX 1, ISETP.NE.OR 1, MOV 1 |
| 0967 | `long long (a > b) ? a : b` | 17 | 20 | 23 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1, LDG.E 1, MOV 3 |
| 0968 | `long long (a > b) && (c != 0)` | 20 | 22 | 27 | IMAD.MOV.U32 1 | ISETP.NE.AND 1, LOP3.LUT 1, MOV 2, SEL 2 |
| 0969 | `long long (a > b) \|\| (c != 0)` | 19 | 22 | 22 | IMAD.MOV.U32 1, ISETP.GT.OR.EX 1, ISETP.NE.AND 1 | ISETP.GT.AND.EX 1, ISETP.NE.OR 1, MOV 1 |
| 0970 | `long long (a <= b) ? a : b` | 17 | 20 | 23 | IMAD.MOV.U32 1, ISETP.LT.AND.EX 1, ISETP.LT.U32.AND 1, SHF.L.U32 1 | IMAD.SHL.U32 1, ISETP.GT.AND.EX 1, ISETP.GT.U32.AND 1, LDG.E 1, MOV 3 |
| 0971 | `long long (a <= b) && (c != 0)` | 20 | 22 | 27 | IMAD.MOV.U32 1 | ISETP.NE.AND 1, LOP3.LUT 1, MOV 2, SEL 2 |
| 0972 | `long long (a <= b) \|\| (c != 0)` | 19 | 22 | 22 | IMAD.MOV.U32 1, ISETP.LE.OR.EX 1, ISETP.NE.AND 1 | ISETP.LE.AND.EX 1, ISETP.NE.OR 1, MOV 1 |
| 0973 | `long long (a >= b) ? a : b` | 17 | 20 | 23 | IMAD.MOV.U32 1, ISETP.GT.AND.EX 1, ISETP.GT.U32.AND 1, SHF.L.U32 1 | IMAD.SHL.U32 1, ISETP.GE.AND.EX 1, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3 |
| 0974 | `long long (a >= b) && (c != 0)` | 20 | 22 | 27 | IMAD.MOV.U32 1 | ISETP.NE.AND 1, LOP3.LUT 1, MOV 2, SEL 2 |
| 0975 | `long long (a >= b) \|\| (c != 0)` | 19 | 22 | 22 | IMAD.MOV.U32 1, ISETP.GE.OR.EX 1, ISETP.NE.AND 1 | ISETP.GE.AND.EX 1, ISETP.NE.OR 1, MOV 1 |
| 0976 | `long long a == 0` | 18 | 19 | 21 |  | LDG.E 1, LDG.E.64 1 |
| 0977 | `long long a != 0` | 18 | 19 | 21 |  | LDG.E 1, LDG.E.64 1 |
| 0978 | `long long (a != 0) && (c != 0)` | 19 | 21 | 22 | IMAD.MOV.U32 1 | LDG.E.64 1, MOV 1 |
| 0979 | `long long (a != 0) \|\| (c != 0)` | 19 | 21 | 22 | IMAD.MOV.U32 1 | LDG.E.64 1, MOV 1 |
| 0980 | `long long a < 0` | 19 | 17 | 23 | SHF.L.U32 1, SHF.R.U32.HI 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, ISETP.GE.AND.EX 1, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 1, SEL 1 |
| 0981 | `unsigned long long (a == b) ? a : b` | 17 | 15 | 23 | SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 1, SEL 2 |
| 0982 | `unsigned long long (a == b) && (c != 0)` | 20 | 22 | 27 | IMAD.MOV.U32 1 | ISETP.NE.AND 1, LOP3.LUT 1, MOV 2, SEL 2 |
| 0983 | `unsigned long long (a == b) \|\| (c != 0)` | 19 | 22 | 22 | IMAD.MOV.U32 1, ISETP.EQ.OR.EX 1, ISETP.NE.AND 1 | ISETP.EQ.AND.EX 1, ISETP.NE.OR 1, MOV 1 |
| 0984 | `unsigned long long (a != b) ? a : b` | 17 | 15 | 23 | SHF.L.U32 1 | IMAD.MOV.U32 1, IMAD.SHL.U32 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 1, SEL 2 |
| 0985 | `unsigned long long (a != b) && (c != 0)` | 20 | 22 | 27 | IMAD.MOV.U32 1 | ISETP.NE.AND 1, LOP3.LUT 1, MOV 2, SEL 2 |
| 0986 | `unsigned long long (a != b) \|\| (c != 0)` | 19 | 22 | 22 | IMAD.MOV.U32 1, ISETP.NE.AND 1, ISETP.NE.OR.EX 1 | ISETP.NE.AND.EX 1, ISETP.NE.OR 1, MOV 1 |
| 0987 | `unsigned long long (a < b) ? a : b` | 17 | 20 | 23 | IMAD.MOV.U32 1, ISETP.LT.U32.AND 1, ISETP.LT.U32.AND.EX 1, SHF.L.U32 1 | IMAD.SHL.U32 1, ISETP.GE.U32.AND 1, ISETP.GE.U32.AND.EX 1, LDG.E 1, MOV 3 |
| 0988 | `unsigned long long (a < b) && (c != 0)` | 19 | 22 | 22 | IMAD.MOV.U32 1 | MOV 1 |
| 0989 | `unsigned long long (a < b) \|\| (c != 0)` | 19 | 22 | 22 | IMAD.MOV.U32 1, ISETP.LT.U32.OR.EX 1, ISETP.NE.AND 1 | ISETP.LT.U32.AND.EX 1, ISETP.NE.OR 1, MOV 1 |
| 0990 | `unsigned long long (a > b) ? a : b` | 17 | 20 | 23 | IMAD.MOV.U32 1, SHF.L.U32 1 | IMAD.SHL.U32 1, LDG.E 1, MOV 3 |
| 0991 | `unsigned long long (a > b) && (c != 0)` | 20 | 22 | 27 | IMAD.MOV.U32 1 | ISETP.NE.AND 1, LOP3.LUT 1, MOV 2, SEL 2 |
| 0992 | `unsigned long long (a > b) \|\| (c != 0)` | 19 | 22 | 22 | IMAD.MOV.U32 1, ISETP.GT.U32.OR.EX 1, ISETP.NE.AND 1 | ISETP.GT.U32.AND.EX 1, ISETP.NE.OR 1, MOV 1 |
| 0993 | `unsigned long long (a <= b) ? a : b` | 17 | 20 | 23 | IMAD.MOV.U32 1, ISETP.LT.U32.AND 1, ISETP.LT.U32.AND.EX 1, SHF.L.U32 1 | IMAD.SHL.U32 1, ISETP.GT.U32.AND 1, ISETP.GT.U32.AND.EX 1, LDG.E 1, MOV 3 |
| 0994 | `unsigned long long (a <= b) && (c != 0)` | 20 | 22 | 27 | IMAD.MOV.U32 1 | ISETP.NE.AND 1, LOP3.LUT 1, MOV 2, SEL 2 |
| 0995 | `unsigned long long (a <= b) \|\| (c != 0)` | 19 | 22 | 22 | IMAD.MOV.U32 1, ISETP.LE.U32.OR.EX 1, ISETP.NE.AND 1 | ISETP.LE.U32.AND.EX 1, ISETP.NE.OR 1, MOV 1 |
| 0996 | `unsigned long long (a >= b) ? a : b` | 17 | 20 | 23 | IMAD.MOV.U32 1, ISETP.GT.U32.AND 1, ISETP.GT.U32.AND.EX 1, SHF.L.U32 1 | IMAD.SHL.U32 1, ISETP.GE.U32.AND 1, ISETP.GE.U32.AND.EX 1, LDG.E 1, MOV 3 |
| 0997 | `unsigned long long (a >= b) && (c != 0)` | 20 | 22 | 27 | IMAD.MOV.U32 1 | ISETP.NE.AND 1, LOP3.LUT 1, MOV 2, SEL 2 |
| 0998 | `unsigned long long (a >= b) \|\| (c != 0)` | 19 | 22 | 22 | IMAD.MOV.U32 1, ISETP.GE.U32.OR.EX 1, ISETP.NE.AND 1 | ISETP.GE.U32.AND.EX 1, ISETP.NE.OR 1, MOV 1 |
| 0999 | `unsigned long long a == 0` | 18 | 19 | 21 |  | LDG.E 1, LDG.E.64 1 |
| 1000 | `unsigned long long a != 0` | 18 | 19 | 21 |  | LDG.E 1, LDG.E.64 1 |
| 1001 | `unsigned long long (a != 0) && (c != 0)` | 19 | 21 | 22 | IMAD.MOV.U32 1 | LDG.E.64 1, MOV 1 |
| 1002 | `unsigned long long (a != 0) \|\| (c != 0)` | 19 | 21 | 22 | IMAD.MOV.U32 1 | LDG.E.64 1, MOV 1 |
| 1003 | `int a * b + c` | 17 | 19 | 19 |  |  |
| 1004 | `unsigned int a * b + c` | 17 | 19 | 19 |  |  |
| 1005 | `(unsigned long long)a * b + c` | 17 | 20 | 20 | UMOV 1 | IADD3.X 1 |
| 1006 | `8 values over 8 rounds` | 94 | 93 | 96 | LEA 6 | IMAD 6, LOP3.LUT 3 |
| 1007 | `16 values over 8 rounds` | 174 | 169 | 176 | LEA 14 | IMAD 14, LOP3.LUT 7 |
| 1008 | `32 values over 8 rounds` | 334 | 321 | 336 | LEA 30 | IMAD 30, LOP3.LUT 15 |
| 1009 | `48 values over 8 rounds` | 494 | 473 | 496 | LEA 46 | IMAD 46, LOP3.LUT 23 |
| 1010 | `64 values over 8 rounds` | 654 | 625 | 656 | LEA 62 | IMAD 62, LOP3.LUT 31 |
| 1011 | `96 values over 8 rounds` | 974 | 929 | 976 | LEA 94 | IMAD 94, LOP3.LUT 47 |
| 1012 | `128 values over 8 rounds` | 1294 | 1234 | 1296 | LEA 126, MOV 1 | IMAD 126, LOP3.LUT 63 |
| 1013 | `160 values over 8 rounds` | 1614 | 1695 | 1616 | IADD3 158, LEA 158 | IMAD 158, LOP3.LUT 79 |
| 1014 | `192 values over 8 rounds` | 1934 | 2031 | 1936 | IADD3 190, LEA 190 | IMAD 190, LOP3.LUT 95 |
| 1015 | `224 values over 8 rounds` | 2254 | 2367 | 2256 | IADD3 222, LEA 222 | IMAD 222, LOP3.LUT 111 |

## The questions the engine puts

Each kernel the engine does not answer, by the question it puts.

| question | kernels | first kernel |
|---|---|---|
| a value of type float | 207 | 0008 |
| a value of type double | 191 | 0009 |
| a call or an element nothing here types: atomicMax | 4 | 0869 |
| a call or an element nothing here types: atomicMin | 4 | 0865 |
| a call or an element nothing here types: max | 4 | 0534 |
| a call or an element nothing here types: min | 4 | 0530 |
| a statement nothing here reads: int r; | 4 | 0456 |
| no reading of (a%b) through cu.krs | 4 | 0045 |
| no reading of (a/b) through cu.krs | 4 | 0035 |
| a call or an element nothing here types: atomicAdd | 3 | 0854 |
| a call or an element nothing here types: atomicAnd | 3 | 0878 |
| a call or an element nothing here types: atomicCAS | 3 | 0875 |
| a call or an element nothing here types: atomicExch | 3 | 0861 |
| a call or an element nothing here types: atomicOr | 3 | 0881 |
| a call or an element nothing here types: atomicXor | 3 | 0884 |
| a call or an element nothing here types: atomicSub | 2 | 0859 |
| a name nothing here types: const | 2 | 0485 |
| a statement nothing here reads: double o1[4] = {(double)in[(4u * thread) + 0u], (double)in[(4u * thread) + 1u], (double)in[(4u * thread) + 2u], (double)in[(4u * thread) + 3u]} | 2 | 0841 |
| a statement nothing here reads: float o1[4] = {(float)in[(4u * thread) + 0u], (float)in[(4u * thread) + 1u], (float)in[(4u * thread) + 2u], (float)in[(4u * thread) + 3u]} | 2 | 0818 |
| no reading of ((int)a%(int)b) through cu.krs | 2 | 0044 |
| no reading of ((int)a/(int)b) through cu.krs | 2 | 0034 |
| no reading of ((longlong)a%(longlong)b) through cu.krs | 2 | 0046 |
| no reading of ((longlong)a/(longlong)b) through cu.krs | 2 | 0036 |
| no reading of (short)((int)(short)a@word%(int)(short)b@word) through cu.krs | 2 | 0042 |
| no reading of (short)((int)(short)a@word/(int)(short)b@word) through cu.krs | 2 | 0032 |
| no reading of (signedchar)((int)(signedchar)a@word%(int)(signedchar)b@word) through cu.krs | 2 | 0040 |
| no reading of (signedchar)((int)(signedchar)a@word/(int)(signedchar)b@word) through cu.krs | 2 | 0030 |
| no reading of (unsignedchar)((unsignedchar)a@word%(unsignedchar)b@word) through cu.krs | 2 | 0041 |
| no reading of (unsignedchar)((unsignedchar)a@word/(unsignedchar)b@word) through cu.krs | 2 | 0031 |
| no reading of (unsignedshort)((unsignedshort)a@word%(unsignedshort)b@word) through cu.krs | 2 | 0043 |
| no reading of (unsignedshort)((unsignedshort)a@word/(unsignedshort)b@word) through cu.krs | 2 | 0033 |
| a call or an element nothing here types: __activemask | 1 | 0890 |
| a call or an element nothing here types: __all_sync | 1 | 0887 |
| a call or an element nothing here types: __any_sync | 1 | 0888 |
| a call or an element nothing here types: __ballot_sync | 1 | 0889 |
| a call or an element nothing here types: __brev | 1 | 0504 |
| a call or an element nothing here types: __brevll | 1 | 0505 |
| a call or an element nothing here types: __byte_perm | 1 | 0506 |
| a call or an element nothing here types: __clz | 1 | 0507 |
| a call or an element nothing here types: __clzll | 1 | 0508 |
| a call or an element nothing here types: __ffs | 1 | 0509 |
| a call or an element nothing here types: __ffsll | 1 | 0510 |
| a call or an element nothing here types: __fns | 1 | 0511 |
| a call or an element nothing here types: __funnelshift_l | 1 | 0512 |
| a call or an element nothing here types: __funnelshift_lc | 1 | 0513 |
| a call or an element nothing here types: __funnelshift_r | 1 | 0514 |
| a call or an element nothing here types: __funnelshift_rc | 1 | 0515 |
| a call or an element nothing here types: __hadd | 1 | 0516 |
| a call or an element nothing here types: __ldg | 1 | 0478 |
| a call or an element nothing here types: __match_any_sync | 1 | 0891 |
| a call or an element nothing here types: __mul24 | 1 | 0517 |
| a call or an element nothing here types: __mul64hi | 1 | 0518 |
| a call or an element nothing here types: __mulhi | 1 | 0519 |
| a call or an element nothing here types: __popc | 1 | 0520 |
| a call or an element nothing here types: __popcll | 1 | 0521 |
| a call or an element nothing here types: __reduce_add_sync | 1 | 0893 |
| a call or an element nothing here types: __reduce_and_sync | 1 | 0896 |
| a call or an element nothing here types: __reduce_max_sync | 1 | 0895 |
| a call or an element nothing here types: __reduce_min_sync | 1 | 0894 |
| a call or an element nothing here types: __reduce_or_sync | 1 | 0897 |
| a call or an element nothing here types: __reduce_xor_sync | 1 | 0898 |
| a call or an element nothing here types: __rhadd | 1 | 0522 |
| a call or an element nothing here types: __sad | 1 | 0523 |
| a call or an element nothing here types: __shfl_down_sync | 1 | 0901 |
| a call or an element nothing here types: __shfl_sync | 1 | 0899 |
| a call or an element nothing here types: __shfl_up_sync | 1 | 0900 |
| a call or an element nothing here types: __shfl_xor_sync | 1 | 0902 |
| a call or an element nothing here types: __syncthreads_and | 1 | 0906 |
| a call or an element nothing here types: __syncthreads_count | 1 | 0905 |
| a call or an element nothing here types: __syncthreads_or | 1 | 0907 |
| a call or an element nothing here types: __uhadd | 1 | 0524 |
| a call or an element nothing here types: __umul24 | 1 | 0525 |
| a call or an element nothing here types: __umul64hi | 1 | 0526 |
| a call or an element nothing here types: __umulhi | 1 | 0527 |
| a call or an element nothing here types: __urhadd | 1 | 0528 |
| a call or an element nothing here types: __usad | 1 | 0529 |
| a call or an element nothing here types: `*` | 1 | 0479 |
| a call or an element nothing here types: abs | 1 | 0538 |
| a call or an element nothing here types: atomicDec | 1 | 0874 |
| a call or an element nothing here types: atomicInc | 1 | 0873 |
| a call or an element nothing here types: clock | 1 | 0911 |
| a call or an element nothing here types: clock64 | 1 | 0912 |
| a call or an element nothing here types: llabs | 1 | 0540 |
| a call or an element nothing here types: measuring_stick_constant | 1 | 0477 |
| a call or an element nothing here types: measuring_stick_forceinline | 1 | 0501 |
| a call or an element nothing here types: measuring_stick_noinline | 1 | 0500 |
| a call or an element nothing here types: measuring_stick_recursive | 1 | 0502 |
| a name nothing here types: blockIdx.x | 1 | 0490 |
| a name nothing here types: threadIdx.x | 1 | 0487 |
| a statement nothing here reads: __shared__ int s[256]; | 1 | 0474 |
| a statement nothing here reads: __syncthreads(); | 1 | 0904 |
| a statement nothing here reads: __syncwarp(); | 1 | 0903 |
| a statement nothing here reads: __threadfence(); | 1 | 0908 |
| a statement nothing here reads: __threadfence_block(); | 1 | 0909 |
| a statement nothing here reads: __threadfence_system(); | 1 | 0910 |
| a statement nothing here reads: again: r += a; | 1 | 0469 |
| a statement nothing here reads: const unsigned long long *p = in + (4u * thread); | 1 | 0480 |
| a statement nothing here reads: do { r += a; i++; } | 1 | 0464 |
| a statement nothing here reads: extern __shared__ int d[]; | 1 | 0475 |
| a statement nothing here reads: for (int i = 0; i < 8; i++) { r += a >> i; } | 1 | 0462 |
| a statement nothing here reads: for (int i = 0; i < b; i++) { for (int j = 0; j < a; j++) { r += i * j; } } | 1 | 0467 |
| a statement nothing here reads: for (int i = 0; i < b; i++) { if ((i & 1) != 0) { continue; } r += i; } | 1 | 0466 |
| a statement nothing here reads: for (int i = 0; i < b; i++) { if (r > a) { break; } r += i; } | 1 | 0465 |
| a statement nothing here reads: for (int i = 0; i < b; i++) { r += a; } | 1 | 0461 |
| a statement nothing here reads: if (b > 0) { goto skip; } | 1 | 0468 |
| a statement nothing here reads: int (*f)(int, int) = ((b & 1) != 0) ? measuring_stick_noinline : measuring_stick_other; | 1 | 0503 |
| a statement nothing here reads: int g[4][4]; | 1 | 0481 |
| a statement nothing here reads: int l[16]; | 1 | 0476 |
| a statement nothing here reads: int p; | 1 | 0892 |
| a statement nothing here reads: struct { int x; int y; } | 1 | 0483 |
| a statement nothing here reads: struct { int x; short y; char z; } | 1 | 0482 |
| a statement nothing here reads: switch (b & 3) { case 0: r += a; case 1: r += 1; case 2: r += 2; break; default: r = a; } | 1 | 0460 |
| a statement nothing here reads: union { float f; int i; } | 1 | 0484 |
| a statement nothing here reads: while (i < b) { r ^= a + i; i++; } | 1 | 0463 |
| a statement nothing here reads: while (r > 1) { r = ((r & 1) != 0) ? ((3 * r) + 1) : (r / 2); if (r == b) { break; } } | 1 | 0473 |
| a value of type long | 1 | 0539 |
| no reading of ((((a!=0u)&&((int)((int)b/(int)a)>(int)1u)))?1u:0u) through cu.krs | 1 | 0472 |
| sass.krs's exit_if assembles with no reading of its operands | 1 | 0470 |
| sass.krs's wide_from_word_if assembles with no reading of its operands | 1 | 0455 |

## By category

| category | kernels | nvcc instructions |
|---|---|---|
| operator | 171 | 4214 |
| compound | 88 | 2540 |
| unary | 78 | 1334 |
| conversion | 110 | 1862 |
| conditional | 11 | 241 |
| statement | 19 | 853 |
| memory | 13 | 382 |
| built-in | 13 | 182 |
| call | 4 | 168 |
| integer intrinsic | 37 | 791 |
| casting intrinsic | 71 | 1212 |
| single intrinsic | 41 | 1752 |
| double intrinsic | 28 | 1462 |
| single math | 87 | 9342 |
| double math | 86 | 15867 |
| atomic | 33 | 646 |
| warp | 17 | 302 |
| sync | 9 | 172 |
| test | 90 | 1813 |
| pressure | 10 | 9937 |

## Operations nvcc writes that no form of sass.krs writes

| operation | kernels it is in | first kernel | its function |
|---|---|---|---|
| `IMAD.SHL.U32` | 396 | 0030 | `signed char a / b` |
| `I2F.U64` | 184 | 0008 | `float a + b` |
| `I2F.F64.U64` | 170 | 0009 | `double a + b` |
| `BSSY` | 139 | 0036 | `long long a / b` |
| `BSYNC` | 139 | 0036 | `long long a / b` |
| `FSEL` | 124 | 0031 | `unsigned char a / b` |
| `IMAD.IADD` | 100 | 0030 | `signed char a / b` |
| `FFMA` | 90 | 0038 | `float a / b` |
| `DMUL` | 88 | 0029 | `double a * b` |
| `FMUL` | 85 | 0028 | `float a * b` |
| `CALL.REL.NOINC` | 83 | 0031 | `unsigned char a / b` |
| `RET.REL.NODEC` | 83 | 0031 | `unsigned char a / b` |
| `FSETP.GEU.AND` | 81 | 0031 | `unsigned char a / b` |
| `FSETP.NEU.AND` | 74 | 0039 | `double a / b` |
| `DFMA` | 71 | 0039 | `double a / b` |
| `MUFU.RCP` | 71 | 0030 | `signed char a / b` |
| `DADD` | 69 | 0009 | `double a + b` |
| `FADD` | 69 | 0008 | `float a + b` |
| `LEA.HI` | 54 | 0524 | `unsigned int __uhadd(a, b)` |
| `FSETP.GT.AND` | 48 | 0031 | `unsigned char a / b` |
| `LEA` | 45 | 0038 | `float a / b` |
| `DSETP.NEU.AND` | 43 | 0097 | `double a == b` |
| `MUFU.RCP64H` | 41 | 0039 | `double a / b` |
| `MUFU.RSQ` | 41 | 0038 | `float a / b` |
| `FSETP.GTU.AND` | 39 | 0039 | `double a / b` |
| `DSETP.GTU.AND` | 37 | 0137 | `double a <= b` |
| `IMNMX` | 34 | 0039 | `double a / b` |
| `ISETP.GT.U32.OR` | 34 | 0038 | `float a / b` |
| `FSETP.GE.AND` | 27 | 0146 | `float a >= b` |
| `MUFU.RSQ64H` | 26 | 0670 | `double __dsqrt_rn(a)` |
| `PLOP3.LUT` | 26 | 0038 | `float a / b` |
| `F2I.FTZ.U32.TRUNC.NTZ` | 25 | 0030 | `signed char a / b` |
| `I2FP.F32.S32` | 25 | 0392 | `(float)(int)` |
| `UMOV` | 25 | 0256 | `signed char -a` |
| `FADD.FTZ` | 24 | 0038 | `float a / b` |
| `CS2R` | 23 | 0461 | `int r = 0; for (int i = 0; i < b; i++) { r += a; }` |
| `FSETP.NEU.FTZ.AND` | 23 | 0038 | `float a / b` |
| `ISETP.EQ.OR` | 21 | 0039 | `double a / b` |
| `DSETP.NAN.AND` | 20 | 0039 | `double a / b` |
| `F2F.F32.F64` | 20 | 0443 | `(float)(double)` |
| `F2I.NTZ` | 20 | 0565 | `int __float2int_rn(a)` |
| `FSETP.GTU.FTZ.AND` | 20 | 0038 | `float a / b` |
| `MUFU.EX2` | 19 | 0613 | `float __exp10f(a)` |
| `DMUL.RP` | 18 | 0039 | `double a / b` |
| `DSETP.GE.AND` | 18 | 0147 | `double a >= b` |
| `FFMA.RM` | 18 | 0038 | `float a / b` |
| `BREAK` | 17 | 0619 | `float __fdiv_rd(a, b)` |
| `FFMA.RZ` | 17 | 0038 | `float a / b` |
| `FLO.U32` | 17 | 0507 | `int __clz(a)` |
| `FFMA.RP` | 15 | 0038 | `float a / b` |
| `DSETP.GT.AND` | 14 | 0127 | `double a > b` |
| `F2I.U32.TRUNC.NTZ` | 14 | 0031 | `unsigned char a / b` |
| `FMUL.RZ` | 14 | 0031 | `unsigned char a / b` |
| `FRND.TRUNC` | 14 | 0690 | `float coshf(a)` |
| `I2F.RP` | 14 | 0030 | `signed char a / b` |
| `IMAD.U32` | 14 | 0110 | `short a < b` |
| `DSETP.GEU.AND` | 13 | 0117 | `double a < b` |
| `I2F.F64` | 13 | 0393 | `(double)(int)` |
| `I2F.U32.RP` | 13 | 0035 | `unsigned int a / b` |
| `F2I.F64` | 12 | 0547 | `int __double2int_rn(a)` |
| `FMUL.FTZ` | 12 | 0637 | `float __fsqrt_rd(a)` |
| `I2F.F64.S64` | 12 | 0413 | `(double)(long long)` |
| `LDL.64` | 12 | 0753 | `double cos(a)` |
| `SHF.L.U32.HI` | 12 | 0513 | `unsigned int __funnelshift_lc(a, b, c)` |
| `LDG.E.128.CONSTANT` | 11 | 0753 | `double cos(a)` |
| `MUFU.LG2` | 11 | 0645 | `float __log10f(a)` |
| `WARPSYNC` | 11 | 0669 | `double __dsqrt_rd(a)` |
| `F2I.U64.TRUNC` | 10 | 0036 | `long long a / b` |
| `FRND` | 10 | 0691 | `float cospif(a)` |
| `FSETP.GEU.FTZ.AND` | 10 | 0637 | `float __fsqrt_rd(a)` |
| `I2FP.F32.U32` | 10 | 0362 | `(float)(unsigned char)` |
| `IMAD.WIDE` | 10 | 0753 | `double cos(a)` |
| `LEA.HI.X` | 10 | 0753 | `double cos(a)` |
| `R2P` | 10 | 0707 | `float lgammaf(a)` |
| `STL.64` | 10 | 0753 | `double cos(a)` |
| `UIADD3` | 10 | 0689 | `float cosf(a)` |
| `UIADD3.X` | 10 | 0689 | `float cosf(a)` |
| `DFMA.RM` | 9 | 0657 | `double __ddiv_rd(a, b)` |
| `DFMA.RP` | 9 | 0659 | `double __ddiv_ru(a, b)` |
| `F2F.F64.F32` | 9 | 0433 | `(double)(float)` |
| `FADD.RZ` | 9 | 0618 | `float __fadd_rz(a, b)` |
| `I2F.U64.RP` | 9 | 0036 | `long long a / b` |
| `FCHK` | 8 | 0038 | `float a / b` |
| `FMNMX` | 8 | 0734 | `float fmaxf(a, b)` |
| `I2F.F64.U32` | 8 | 0403 | `(double)(unsigned int)` |
| `I2F.U16` | 8 | 0031 | `unsigned char a / b` |
| `DSETP.MAX.AND` | 7 | 0797 | `double fmax(a, b)` |
| `DSETP.MIN.AND` | 7 | 0798 | `double fmin(a, b)` |
| `F2I.TRUNC.NTZ` | 7 | 0425 | `(signed char)(float)` |
| `FRND.F64.TRUNC` | 7 | 0771 | `double lgamma(a)` |
| `DFMA.RZ` | 6 | 0660 | `double __ddiv_rz(a, b)` |
| `F2I.F64.TRUNC` | 6 | 0435 | `(signed char)(double)` |
| `F2I.S64.F64` | 6 | 0551 | `long long __double2ll_rn(a)` |
| `FRND.F64` | 6 | 0755 | `double cospi(a)` |
| `FSETP.LT.AND` | 6 | 0738 | `float nextafterf(a, b)` |
| `DADD.RZ` | 5 | 0656 | `double __dadd_rz(a, b)` |
| `DSETP.NE.AND` | 5 | 0670 | `double __dsqrt_rn(a)` |
| `I2FP.F32.U32.RZ` | 5 | 0031 | `unsigned char a / b` |
| `IMNMX.U32` | 5 | 0459 | `int r; switch (b) { case 1: r = a; break; case 100: r = a + 1; break; case 10000: r = a + 2; break; case -7: r = a + 3; break; default: r = 0; break; }` |
| `ATOMG.E.ADD.STRONG.GPU` | 4 | 0854 | `int atomicAdd` |
| `BREV` | 4 | 0504 | `unsigned int __brev(a)` |
| `F2I.U32.F64.TRUNC` | 4 | 0436 | `(unsigned char)(double)` |
| `I2F.U16.RZ` | 4 | 0041 | `unsigned char a % b` |
| `LDL` | 4 | 0728 | `float y0f(a)` |
| `STL` | 4 | 0728 | `float y0f(a)` |
| `ATOMG.E.EXCH.STRONG.GPU` | 3 | 0861 | `int atomicExch` |
| `B2R.RESULT` | 3 | 0905 | `int r = __syncthreads_count(a > b);` |
| `BAR.SYNC.DEFER_BLOCKING` | 3 | 0474 | `__shared__ int s[256]; s[threadIdx.x & 255u] = a; __syncthreads(); int r = s[(threadIdx.x + 1u) & 255u];` |
| `DSETP.LE.AND` | 3 | 0799 | `double fmod(a, b)` |
| `F2I.S64.F64.TRUNC` | 3 | 0441 | `(long long)(double)` |
| `F2I.S64.TRUNC` | 3 | 0431 | `(long long)(float)` |
| `FFMA.SAT` | 3 | 0696 | `float erfcxf(a)` |
| `FRND.FLOOR` | 3 | 0704 | `float floorf(a)` |
| `FSETP.EQ.OR` | 3 | 0739 | `float powf(a, b)` |
| `FSETP.LEU.OR` | 3 | 0736 | `float fmodf(a, b)` |
| `IMAD.U32.X` | 3 | 0799 | `double fmod(a, b)` |
| `ISETP.EQ.OR.EX` | 3 | 0802 | `double pow(a, b)` |
| `ISETP.LT.OR` | 3 | 0465 | `int r = 0; for (int i = 0; i < b; i++) { if (r > a) { break; } r += i; }` |
| `LDC` | 3 | 0459 | `int r; switch (b) { case 1: r = a; break; case 100: r = a + 1; break; case 10000: r = a + 2; break; case -7: r = a + 3; break; default: r = 0; break; }` |
| `LEA.HI.SX32` | 3 | 0462 | `int r = 0; for (int i = 0; i < 8; i++) { r += a >> i; }` |
| `MUFU.SIN` | 3 | 0650 | `void __sincosf(a, o1, o2)` |
| `POPC` | 3 | 0511 | `unsigned int __fns(a, b, c)` |
| `ATOMG.E.AND.STRONG.GPU` | 2 | 0878 | `int atomicAnd` |
| `ATOMG.E.CAS.STRONG.GPU` | 2 | 0875 | `int atomicCAS` |
| `ATOMG.E.OR.STRONG.GPU` | 2 | 0881 | `int atomicOr` |
| `ATOMG.E.XOR.STRONG.GPU` | 2 | 0884 | `int atomicXor` |
| `BRX` | 2 | 0459 | `int r; switch (b) { case 1: r = a; break; case 100: r = a + 1; break; case 10000: r = a + 2; break; case -7: r = a + 3; break; default: r = 0; break; }` |
| `CCTL.IVALL` | 2 | 0908 | `__threadfence(); int r = a;` |
| `DADD.RM` | 2 | 0653 | `double __dadd_rd(a, b)` |
| `DADD.RP` | 2 | 0655 | `double __dadd_ru(a, b)` |
| `DMUL.RM` | 2 | 0657 | `double __ddiv_rd(a, b)` |
| `DMUL.RZ` | 2 | 0660 | `double __ddiv_rz(a, b)` |
| `DSETP.EQ.OR` | 2 | 0751 | `double cbrt(a)` |
| `DSETP.GTU.OR` | 2 | 0801 | `double nextafter(a, b)` |
| `DSETP.LEU.AND` | 2 | 0803 | `double remainder(a, b)` |
| `DSETP.NE.OR` | 2 | 0665 | `double __drcp_rd(a)` |
| `DSETP.NEU.OR` | 2 | 0803 | `double remainder(a, b)` |
| `ERRBAR` | 2 | 0908 | `__threadfence(); int r = a;` |
| `F2I.S64` | 2 | 0569 | `long long __float2ll_rn(a)` |
| `F2I.U64.F64.TRUNC` | 2 | 0442 | `(unsigned long long)(double)` |
| `FADD.RM` | 2 | 0615 | `float __fadd_rd(a, b)` |
| `FADD.RP` | 2 | 0617 | `float __fadd_ru(a, b)` |
| `FSETP.GTU.OR` | 2 | 0738 | `float nextafterf(a, b)` |
| `FSETP.LEU.AND` | 2 | 0740 | `float remainderf(a, b)` |
| `I2F.F64.S16` | 2 | 0353 | `(double)(signed char)` |
| `I2F.F64.U16` | 2 | 0363 | `(double)(unsigned char)` |
| `I2F.S16` | 2 | 0352 | `(float)(signed char)` |
| `I2F.S64` | 2 | 0412 | `(float)(long long)` |
| `IMAD.WIDE.U32.X` | 2 | 0518 | `long long __mul64hi(a, b)` |
| `ISETP.GE.OR` | 2 | 0802 | `double pow(a, b)` |
| `ISETP.GT.OR` | 2 | 0502 | `int r = measuring_stick_recursive(a & 15, b);` |
| `ISETP.NE.U32.AND.EX` | 2 | 0503 | `int (*f)(int, int) = ((b & 1) != 0) ? measuring_stick_noinline : measuring_stick_other; int r = f(a, b);` |
| `ISETP.NE.U32.OR` | 2 | 0740 | `float remainderf(a, b)` |
| `LDL.128` | 2 | 0841 | `double norm(a, o1)` |
| `LDS` | 2 | 0474 | `__shared__ int s[256]; s[threadIdx.x & 255u] = a; __syncthreads(); int r = s[(threadIdx.x + 1u) & 255u];` |
| `LEA.HI.X.SX32` | 2 | 0911 | `long long r = (long long)clock() + a;` |
| `MUFU.COS` | 2 | 0612 | `float __cosf(a)` |
| `STL.128` | 2 | 0841 | `double norm(a, o1)` |
| `STS` | 2 | 0474 | `__shared__ int s[256]; s[threadIdx.x & 255u] = a; __syncthreads(); int r = s[(threadIdx.x + 1u) & 255u];` |
| `VOTE.ANY` | 2 | 0888 | `int r = __any_sync(0xffffffffu, a > b);` |
| `ATOMG.E.ADD.64.STRONG.GPU` | 1 | 0856 | `unsigned long long atomicAdd` |
| `ATOMG.E.ADD.F32.FTZ.RN.STRONG.GPU` | 1 | 0857 | `float atomicAdd` |
| `ATOMG.E.ADD.F64.RN.STRONG.GPU` | 1 | 0858 | `double atomicAdd` |
| `ATOMG.E.AND.64.STRONG.GPU` | 1 | 0880 | `unsigned long long atomicAnd` |
| `ATOMG.E.CAS.64.STRONG.GPU` | 1 | 0877 | `unsigned long long atomicCAS` |
| `ATOMG.E.DEC.STRONG.GPU` | 1 | 0874 | `unsigned int atomicDec` |
| `ATOMG.E.EXCH.64.STRONG.GPU` | 1 | 0863 | `unsigned long long atomicExch` |
| `ATOMG.E.INC.STRONG.GPU` | 1 | 0873 | `unsigned int atomicInc` |
| `ATOMG.E.MAX.64.STRONG.GPU` | 1 | 0872 | `unsigned long long atomicMax` |
| `ATOMG.E.MAX.S32.STRONG.GPU` | 1 | 0869 | `int atomicMax` |
| `ATOMG.E.MAX.S64.STRONG.GPU` | 1 | 0871 | `long long atomicMax` |
| `ATOMG.E.MAX.STRONG.GPU` | 1 | 0870 | `unsigned int atomicMax` |
| `ATOMG.E.MIN.64.STRONG.GPU` | 1 | 0868 | `unsigned long long atomicMin` |
| `ATOMG.E.MIN.S32.STRONG.GPU` | 1 | 0865 | `int atomicMin` |
| `ATOMG.E.MIN.S64.STRONG.GPU` | 1 | 0867 | `long long atomicMin` |
| `ATOMG.E.MIN.STRONG.GPU` | 1 | 0866 | `unsigned int atomicMin` |
| `ATOMG.E.OR.64.STRONG.GPU` | 1 | 0883 | `unsigned long long atomicOr` |
| `ATOMG.E.XOR.64.STRONG.GPU` | 1 | 0886 | `unsigned long long atomicXor` |
| `BAR.RED.AND.DEFER_BLOCKING` | 1 | 0906 | `int r = __syncthreads_and(a > b);` |
| `BAR.RED.OR.DEFER_BLOCKING` | 1 | 0907 | `int r = __syncthreads_or(a > b);` |
| `BAR.RED.POPC.DEFER_BLOCKING` | 1 | 0905 | `int r = __syncthreads_count(a > b);` |
| `DEPBAR.LE` | 1 | 0911 | `long long r = (long long)clock() + a;` |
| `DSETP.LT.AND` | 1 | 0801 | `double nextafter(a, b)` |
| `F2F.F32.F64.RM` | 1 | 0541 | `float __double2float_rd(a)` |
| `F2F.F32.F64.RP` | 1 | 0543 | `float __double2float_ru(a)` |
| `F2F.F32.F64.RZ` | 1 | 0544 | `float __double2float_rz(a)` |
| `F2I.CEIL.NTZ` | 1 | 0566 | `int __float2int_ru(a)` |
| `F2I.F64.CEIL` | 1 | 0548 | `int __double2int_ru(a)` |
| `F2I.F64.FLOOR` | 1 | 0546 | `int __double2int_rd(a)` |
| `F2I.FLOOR.NTZ` | 1 | 0564 | `int __float2int_rd(a)` |
| `F2I.S64.CEIL` | 1 | 0570 | `long long __float2ll_ru(a)` |
| `F2I.S64.F64.CEIL` | 1 | 0552 | `long long __double2ll_ru(a)` |
| `F2I.S64.F64.FLOOR` | 1 | 0550 | `long long __double2ll_rd(a)` |
| `F2I.S64.FLOOR` | 1 | 0568 | `long long __float2ll_rd(a)` |
| `F2I.U32.CEIL.NTZ` | 1 | 0574 | `unsigned int __float2uint_ru(a)` |
| `F2I.U32.F64` | 1 | 0556 | `unsigned int __double2uint_rn(a)` |
| `F2I.U32.F64.CEIL` | 1 | 0557 | `unsigned int __double2uint_ru(a)` |
| `F2I.U32.F64.FLOOR` | 1 | 0555 | `unsigned int __double2uint_rd(a)` |
| `F2I.U32.FLOOR.NTZ` | 1 | 0572 | `unsigned int __float2uint_rd(a)` |
| `F2I.U32.NTZ` | 1 | 0573 | `unsigned int __float2uint_rn(a)` |
| `F2I.U64` | 1 | 0577 | `unsigned long long __float2ull_rn(a)` |
| `F2I.U64.CEIL` | 1 | 0578 | `unsigned long long __float2ull_ru(a)` |
| `F2I.U64.F64` | 1 | 0560 | `unsigned long long __double2ull_rn(a)` |
| `F2I.U64.F64.CEIL` | 1 | 0561 | `unsigned long long __double2ull_ru(a)` |
| `F2I.U64.F64.FLOOR` | 1 | 0559 | `unsigned long long __double2ull_rd(a)` |
| `F2I.U64.FLOOR` | 1 | 0576 | `unsigned long long __float2ull_rd(a)` |
| `FADD.SAT` | 1 | 0649 | `float __saturatef(a)` |
| `FLO.U32.SH` | 1 | 0509 | `int __ffs(a)` |
| `FMUL.RM` | 1 | 0628 | `float __fmul_rd(a, b)` |
| `FMUL.RP` | 1 | 0630 | `float __fmul_ru(a, b)` |
| `FRND.CEIL` | 1 | 0688 | `float ceilf(a)` |
| `FRND.F64.CEIL` | 1 | 0752 | `double ceil(a)` |
| `FRND.F64.FLOOR` | 1 | 0768 | `double floor(a)` |
| `FSETP.EQ.AND` | 1 | 0739 | `float powf(a, b)` |
| `FSETP.GEU.OR` | 1 | 0739 | `float powf(a, b)` |
| `FSETP.NEU.OR` | 1 | 0166 | `float a \|\| b` |
| `I2F.F64.S64.RM` | 1 | 0589 | `double __ll2double_rd(a)` |
| `I2F.F64.S64.RP` | 1 | 0591 | `double __ll2double_ru(a)` |
| `I2F.F64.S64.RZ` | 1 | 0592 | `double __ll2double_rz(a)` |
| `I2F.F64.U64.RM` | 1 | 0604 | `double __ull2double_rd(a)` |
| `I2F.F64.U64.RP` | 1 | 0606 | `double __ull2double_ru(a)` |
| `I2F.F64.U64.RZ` | 1 | 0607 | `double __ull2double_rz(a)` |
| `I2F.RM` | 1 | 0584 | `float __int2float_rd(a)` |
| `I2F.S64.RM` | 1 | 0593 | `float __ll2float_rd(a)` |
| `I2F.S64.RP` | 1 | 0595 | `float __ll2float_ru(a)` |
| `I2F.S64.RZ` | 1 | 0596 | `float __ll2float_rz(a)` |
| `I2F.U32.RM` | 1 | 0599 | `float __uint2float_rd(a)` |
| `I2F.U64.RM` | 1 | 0608 | `float __ull2float_rd(a)` |
| `I2F.U64.RZ` | 1 | 0611 | `float __ull2float_rz(a)` |
| `I2FP.F32.S32.RZ` | 1 | 0587 | `float __int2float_rz(a)` |
| `IMAD.HI` | 1 | 0519 | `int __mulhi(a, b)` |
| `ISETP.GE.OR.EX` | 1 | 0975 | `long long (a >= b) \|\| (c != 0)` |
| `ISETP.GE.U32.OR` | 1 | 0953 | `unsigned int (a >= b) \|\| (c != 0)` |
| `ISETP.GE.U32.OR.EX` | 1 | 0998 | `unsigned long long (a >= b) \|\| (c != 0)` |
| `ISETP.GT.OR.EX` | 1 | 0969 | `long long (a > b) \|\| (c != 0)` |
| `ISETP.GT.U32.OR.EX` | 1 | 0992 | `unsigned long long (a > b) \|\| (c != 0)` |
| `ISETP.LE.OR` | 1 | 0927 | `int (a <= b) \|\| (c != 0)` |
| `ISETP.LE.OR.EX` | 1 | 0972 | `long long (a <= b) \|\| (c != 0)` |
| `ISETP.LE.U32.OR` | 1 | 0950 | `unsigned int (a <= b) \|\| (c != 0)` |
| `ISETP.LE.U32.OR.EX` | 1 | 0995 | `unsigned long long (a <= b) \|\| (c != 0)` |
| `ISETP.LT.OR.EX` | 1 | 0966 | `long long (a < b) \|\| (c != 0)` |
| `ISETP.LT.U32.OR` | 1 | 0944 | `unsigned int (a < b) \|\| (c != 0)` |
| `ISETP.LT.U32.OR.EX` | 1 | 0989 | `unsigned long long (a < b) \|\| (c != 0)` |
| `LDC.64` | 1 | 0503 | `int (*f)(int, int) = ((b & 1) != 0) ? measuring_stick_noinline : measuring_stick_other; int r = f(a, b);` |
| `LDG.E.STRONG.SYS` | 1 | 0479 | `int r = (int)*(volatile const unsigned long long *)&in[(4u * thread) + 2u] + a;` |
| `MATCH.ALL` | 1 | 0892 | `int p; unsigned int r = __match_all_sync(0xffffffffu, a, &p);` |
| `MATCH.ANY` | 1 | 0891 | `unsigned int r = __match_any_sync(0xffffffffu, a);` |
| `MEMBAR.SC.CTA` | 1 | 0909 | `__threadfence_block(); int r = a;` |
| `MEMBAR.SC.GPU` | 1 | 0908 | `__threadfence(); int r = a;` |
| `MEMBAR.SC.SYS` | 1 | 0910 | `__threadfence_system(); int r = a;` |
| `REDUX` | 1 | 0896 | `unsigned int r = __reduce_and_sync(0xffffffffu, (unsigned int)a);` |
| `REDUX.MAX.S32` | 1 | 0895 | `int r = __reduce_max_sync(0xffffffffu, a);` |
| `REDUX.MIN.S32` | 1 | 0894 | `int r = __reduce_min_sync(0xffffffffu, a);` |
| `REDUX.OR` | 1 | 0897 | `unsigned int r = __reduce_or_sync(0xffffffffu, (unsigned int)a);` |
| `REDUX.SUM.S32` | 1 | 0893 | `unsigned int r = __reduce_add_sync(0xffffffffu, (unsigned int)a);` |
| `REDUX.XOR` | 1 | 0898 | `unsigned int r = __reduce_xor_sync(0xffffffffu, (unsigned int)a);` |
| `SGXT.U32` | 1 | 0511 | `unsigned int __fns(a, b, c)` |
| `SHFL.BFLY` | 1 | 0902 | `int r = __shfl_xor_sync(0xffffffffu, a, 1);` |
| `SHFL.DOWN` | 1 | 0901 | `int r = __shfl_down_sync(0xffffffffu, a, 1);` |
| `SHFL.IDX` | 1 | 0899 | `int r = __shfl_sync(0xffffffffu, a, b & 31);` |
| `SHFL.UP` | 1 | 0900 | `int r = __shfl_up_sync(0xffffffffu, a, 1);` |
| `USHF.R.S32.HI` | 1 | 0911 | `long long r = (long long)clock() + a;` |
| `VABSDIFF` | 1 | 0523 | `unsigned int __sad(a, b, c)` |
| `VABSDIFF.U32` | 1 | 0529 | `unsigned int __usad(a, b, c)` |
| `VOTE.ALL` | 1 | 0887 | `int r = __all_sync(0xffffffffu, a > b);` |
| `VOTEU.ANY` | 1 | 0890 | `unsigned int r = __activemask();` |

## Every kernel

| kernel | category | function | nvcc | its operations |
|---|---|---|---|---|
| 0000 | operator | `signed char a + b` | 20 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 1, PRMT 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0001 | operator | `unsigned char a + b` | 19 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0002 | operator | `short a + b` | 20 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 1, PRMT 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0003 | operator | `unsigned short a + b` | 19 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0004 | operator | `int a + b` | 18 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0005 | operator | `unsigned int a + b` | 18 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0006 | operator | `long long a + b` | 18 | BRA 1, EXIT 2, IADD3 1, IADD3.X 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0007 | operator | `unsigned long long a + b` | 18 | BRA 1, EXIT 2, IADD3 1, IADD3.X 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0008 | operator | `float a + b` | 20 | BRA 1, EXIT 2, FADD 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0009 | operator | `double a + b` | 19 | BRA 1, DADD 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0010 | operator | `signed char a - b` | 20 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 1, PRMT 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0011 | operator | `unsigned char a - b` | 19 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0012 | operator | `short a - b` | 20 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 1, PRMT 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0013 | operator | `unsigned short a - b` | 19 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0014 | operator | `int a - b` | 18 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0015 | operator | `unsigned int a - b` | 18 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0016 | operator | `long long a - b` | 18 | BRA 1, EXIT 2, IADD3 1, IADD3.X 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0017 | operator | `unsigned long long a - b` | 18 | BRA 1, EXIT 2, IADD3 1, IADD3.X 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0018 | operator | `float a - b` | 20 | BRA 1, EXIT 2, FADD 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0019 | operator | `double a - b` | 19 | BRA 1, DADD 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0020 | operator | `signed char a * b` | 20 | BRA 1, EXIT 2, IMAD 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 3, PRMT 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0021 | operator | `unsigned char a * b` | 19 | BRA 1, EXIT 2, IMAD 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0022 | operator | `short a * b` | 20 | BRA 1, EXIT 2, IMAD 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 3, PRMT 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0023 | operator | `unsigned short a * b` | 19 | BRA 1, EXIT 2, IMAD 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0024 | operator | `int a * b` | 18 | BRA 1, EXIT 2, IMAD 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0025 | operator | `unsigned int a * b` | 18 | BRA 1, EXIT 2, IMAD 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0026 | operator | `long long a * b` | 20 | BRA 1, EXIT 2, IADD3 1, IMAD 3, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0027 | operator | `unsigned long long a * b` | 20 | BRA 1, EXIT 2, IADD3 1, IMAD 3, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0028 | operator | `float a * b` | 20 | BRA 1, EXIT 2, FMUL 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0029 | operator | `double a * b` | 19 | BRA 1, DMUL 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0030 | operator | `signed char a / b` | 45 | BRA 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, I2F.RP 1, IABS 3, IADD3 4, IMAD 3, IMAD.HI.U32 2, IMAD.IADD 1, IMAD.MOV 2, IMAD.MOV.U32 4, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, ISETP.GT.U32.AND 1, ISETP.NE.AND 1, LDG.E.S8 2, LOP3.LUT 2, MOV 1, MUFU.RCP 1, PRMT 2, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0031 | operator | `unsigned char a / b` | 40 | BRA 1, CALL.REL.NOINC 1, EXIT 2, F2I.U32.TRUNC.NTZ 1, FMUL 2, FMUL.RZ 1, FSEL 2, FSETP.GEU.AND 1, FSETP.GT.AND 1, I2F.U16 1, I2FP.F32.U32.RZ 1, IADD3 1, IMAD 1, IMAD.MOV.U32 5, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.U32.AND 1, LDG.E.U8 2, LOP3.LUT 2, MOV 4, MUFU.RCP 1, RET.REL.NODEC 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0032 | operator | `short a / b` | 45 | BRA 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, I2F.RP 1, IABS 3, IADD3 4, IMAD 3, IMAD.HI.U32 2, IMAD.IADD 1, IMAD.MOV 2, IMAD.MOV.U32 4, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, ISETP.GT.U32.AND 1, ISETP.NE.AND 1, LDG.E.S16 2, LOP3.LUT 2, MOV 1, MUFU.RCP 1, PRMT 2, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0033 | operator | `unsigned short a / b` | 40 | BRA 1, CALL.REL.NOINC 1, EXIT 2, F2I.U32.TRUNC.NTZ 1, FMUL 2, FMUL.RZ 1, FSEL 2, FSETP.GEU.AND 1, FSETP.GT.AND 1, I2F.U16 1, I2FP.F32.U32.RZ 1, IADD3 1, IMAD 1, IMAD.MOV.U32 5, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.U32.AND 1, LDG.E.U16 2, LOP3.LUT 2, MOV 4, MUFU.RCP 1, RET.REL.NODEC 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0034 | operator | `int a / b` | 43 | BRA 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, I2F.RP 1, IABS 3, IADD3 6, IMAD 3, IMAD.HI.U32 2, IMAD.IADD 1, IMAD.MOV.U32 5, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, ISETP.GT.U32.AND 1, ISETP.NE.AND 1, LDG.E 2, LOP3.LUT 2, MUFU.RCP 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0035 | operator | `unsigned int a / b` | 35 | BRA 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, I2F.U32.RP 1, IADD3 4, IMAD 3, IMAD.HI.U32 2, IMAD.MOV 2, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 3, ISETP.NE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 3, MUFU.RCP 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0036 | operator | `long long a / b` | 128 | BRA 3, BSSY 1, BSYNC 1, CALL.REL.NOINC 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, F2I.U64.TRUNC 1, I2F.U32.RP 1, I2F.U64.RP 1, IADD3 17, IADD3.X 6, IMAD 12, IMAD.HI.U32 11, IMAD.IADD 1, IMAD.MOV 1, IMAD.MOV.U32 9, IMAD.SHL.U32 1, IMAD.WIDE.U32 8, IMAD.X 9, ISETP.GE.AND 3, ISETP.GE.U32.AND 5, ISETP.GE.U32.AND.EX 2, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 3, LDG.E.64 2, LOP3.LUT 3, MOV 1, MUFU.RCP 2, RET.REL.NODEC 1, S2R 2, SEL 14, STG.E.64 1, ULDC.64 1 |
| 0037 | operator | `unsigned long long a / b` | 112 | BRA 3, BSSY 1, BSYNC 1, CALL.REL.NOINC 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, F2I.U64.TRUNC 1, I2F.U32.RP 1, I2F.U64.RP 1, IADD3 14, IADD3.X 8, IMAD 12, IMAD.HI.U32 11, IMAD.IADD 1, IMAD.MOV 1, IMAD.MOV.U32 6, IMAD.SHL.U32 1, IMAD.WIDE.U32 8, IMAD.X 4, ISETP.GE.U32.AND 5, ISETP.GE.U32.AND.EX 2, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 3, LDG.E.64 2, LOP3.LUT 2, MOV 4, MUFU.RCP 2, RET.REL.NODEC 1, S2R 2, SEL 8, STG.E.64 1, ULDC.64 1 |
| 0038 | operator | `float a / b` | 138 | BRA 18, BSSY 3, BSYNC 3, CALL.REL.NOINC 1, EXIT 2, FADD.FTZ 2, FCHK 1, FFMA 14, FFMA.RM 1, FFMA.RP 1, FFMA.RZ 1, FSETP.GTU.FTZ.AND 2, FSETP.NEU.FTZ.AND 4, I2F.U64 2, IADD3 7, IMAD 3, IMAD.IADD 4, IMAD.MOV 1, IMAD.MOV.U32 12, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 4, ISETP.GE.U32.AND 2, ISETP.GT.AND 1, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 3, LDG.E.64 2, LEA 1, LOP3.LUT 17, MOV 1, MUFU.RCP 2, MUFU.RSQ 1, PLOP3.LUT 4, RET.REL.NODEC 1, S2R 2, SEL 2, SHF.L.U32 1, SHF.R.U32.HI 5, STG.E.64 1, ULDC.64 1 |
| 0039 | operator | `double a / b` | 130 | BRA 11, BSSY 2, BSYNC 2, CALL.REL.NOINC 1, DFMA 17, DMUL 4, DMUL.RP 1, DSETP.NAN.AND 2, EXIT 2, FFMA 1, FSEL 2, FSETP.GEU.AND 3, FSETP.GT.AND 1, FSETP.GTU.AND 1, FSETP.NEU.AND 2, I2F.F64.U64 2, IADD3 4, IMAD 1, IMAD.IADD 2, IMAD.MOV 1, IMAD.MOV.U32 23, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMNMX 2, ISETP.EQ.OR 1, ISETP.GE.U32.AND 4, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 2, LDG.E.64 2, LOP3.LUT 18, MOV 1, MUFU.RCP64H 2, RET.REL.NODEC 1, S2R 2, SEL 3, STG.E.64 1, ULDC.64 1 |
| 0040 | operator | `signed char a % b` | 43 | BRA 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, I2F.RP 1, IABS 3, IADD3 2, IMAD 3, IMAD.HI.U32 2, IMAD.IADD 1, IMAD.MOV 3, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 2, ISETP.NE.AND 1, LDG.E.S8 2, LOP3.LUT 1, MOV 3, MUFU.RCP 1, PRMT 2, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0041 | operator | `unsigned char a % b` | 42 | BRA 1, CALL.REL.NOINC 1, EXIT 2, F2I.U32.TRUNC.NTZ 1, FMUL 2, FMUL.RZ 1, FSEL 2, FSETP.GEU.AND 1, FSETP.GT.AND 1, I2F.U16 1, I2F.U16.RZ 1, IADD3 2, IMAD 2, IMAD.MOV.U32 5, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.U32.AND 1, LDG.E.U8 2, LOP3.LUT 2, MOV 4, MUFU.RCP 1, RET.REL.NODEC 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0042 | operator | `short a % b` | 43 | BRA 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, I2F.RP 1, IABS 3, IADD3 2, IMAD 3, IMAD.HI.U32 2, IMAD.IADD 1, IMAD.MOV 3, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 2, ISETP.NE.AND 1, LDG.E.S16 2, LOP3.LUT 1, MOV 3, MUFU.RCP 1, PRMT 2, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0043 | operator | `unsigned short a % b` | 42 | BRA 1, CALL.REL.NOINC 1, EXIT 2, F2I.U32.TRUNC.NTZ 1, FMUL 2, FMUL.RZ 1, FSEL 2, FSETP.GEU.AND 1, FSETP.GT.AND 1, I2F.U16 1, I2F.U16.RZ 1, IADD3 2, IMAD 2, IMAD.MOV.U32 5, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.U32.AND 1, LDG.E.U16 2, LOP3.LUT 2, MOV 4, MUFU.RCP 1, RET.REL.NODEC 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0044 | operator | `int a % b` | 41 | BRA 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, I2F.RP 1, IABS 3, IADD3 4, IMAD 3, IMAD.HI.U32 2, IMAD.IADD 1, IMAD.MOV 1, IMAD.MOV.U32 3, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 2, ISETP.NE.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, MUFU.RCP 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0045 | operator | `unsigned int a % b` | 34 | BRA 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, I2F.U32.RP 1, IADD3 4, IMAD 3, IMAD.HI.U32 2, IMAD.IADD 1, IMAD.MOV.U32 3, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 3, ISETP.NE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 1, MUFU.RCP 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0046 | operator | `long long a % b` | 121 | BRA 3, BSSY 1, BSYNC 1, CALL.REL.NOINC 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, F2I.U64.TRUNC 1, I2F.U32.RP 1, I2F.U64.RP 1, IADD3 15, IADD3.X 6, IMAD 12, IMAD.HI.U32 11, IMAD.IADD 1, IMAD.MOV 1, IMAD.MOV.U32 7, IMAD.SHL.U32 1, IMAD.WIDE.U32 8, IMAD.X 8, ISETP.GE.AND 2, ISETP.GE.U32.AND 5, ISETP.GE.U32.AND.EX 2, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 3, LDG.E.64 2, LOP3.LUT 2, MOV 3, MUFU.RCP 2, RET.REL.NODEC 1, S2R 2, SEL 12, STG.E.64 1, ULDC.64 1 |
| 0047 | operator | `unsigned long long a % b` | 107 | BRA 3, BSSY 1, BSYNC 1, CALL.REL.NOINC 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, F2I.U64.TRUNC 1, I2F.U32.RP 1, I2F.U64.RP 1, IADD3 13, IADD3.X 8, IMAD 12, IMAD.HI.U32 11, IMAD.IADD 1, IMAD.MOV.U32 8, IMAD.WIDE.U32 8, IMAD.X 3, ISETP.GE.U32.AND 5, ISETP.GE.U32.AND.EX 2, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 3, LDG.E.64 2, LOP3.LUT 2, MOV 2, MUFU.RCP 2, RET.REL.NODEC 1, S2R 2, SEL 6, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0048 | operator | `signed char a & b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, PRMT 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0049 | operator | `unsigned char a & b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0050 | operator | `short a & b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, PRMT 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0051 | operator | `unsigned short a & b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0052 | operator | `int a & b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0053 | operator | `unsigned int a & b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0054 | operator | `long long a & b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LOP3.LUT 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0055 | operator | `unsigned long long a & b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LOP3.LUT 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0056 | operator | `signed char a \| b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, PRMT 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0057 | operator | `unsigned char a \| b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0058 | operator | `short a \| b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, PRMT 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0059 | operator | `unsigned short a \| b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0060 | operator | `int a \| b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0061 | operator | `unsigned int a \| b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0062 | operator | `long long a \| b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LOP3.LUT 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0063 | operator | `unsigned long long a \| b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LOP3.LUT 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0064 | operator | `signed char a ^ b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, PRMT 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0065 | operator | `unsigned char a ^ b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0066 | operator | `short a ^ b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, PRMT 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0067 | operator | `unsigned short a ^ b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0068 | operator | `int a ^ b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0069 | operator | `unsigned int a ^ b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0070 | operator | `long long a ^ b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LOP3.LUT 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0071 | operator | `unsigned long long a ^ b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LOP3.LUT 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0072 | operator | `signed char a << b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 2, MOV 2, PRMT 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0073 | operator | `unsigned char a << b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 2, LOP3.LUT 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0074 | operator | `short a << b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S16 2, MOV 2, PRMT 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0075 | operator | `unsigned short a << b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 2, LOP3.LUT 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0076 | operator | `int a << b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SHF.L.U32 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0077 | operator | `unsigned int a << b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 3, S2R 2, SHF.L.U32 2, STG.E.64 1, ULDC.64 1 |
| 0078 | operator | `long long a << b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 2, SHF.L.U64.HI 1, STG.E.64 1, ULDC.64 1 |
| 0079 | operator | `unsigned long long a << b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 2, SHF.L.U64.HI 1, STG.E.64 1, ULDC.64 1 |
| 0080 | operator | `signed char a >> b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 2, MOV 2, PRMT 1, S2R 2, SHF.R.S32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0081 | operator | `unsigned char a >> b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 2, LOP3.LUT 1, MOV 2, S2R 2, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0082 | operator | `short a >> b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S16 2, MOV 2, PRMT 1, S2R 2, SHF.R.S32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0083 | operator | `unsigned short a >> b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 2, LOP3.LUT 1, MOV 2, S2R 2, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0084 | operator | `int a >> b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0085 | operator | `unsigned int a >> b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 3, S2R 2, SHF.L.U32 1, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0086 | operator | `long long a >> b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, SHF.R.S64 1, STG.E.64 1, ULDC.64 1 |
| 0087 | operator | `unsigned long long a >> b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.U32.HI 1, SHF.R.U64 1, STG.E.64 1, ULDC.64 1 |
| 0088 | operator | `signed char a == b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 2, LOP3.LUT 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0089 | operator | `unsigned char a == b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 2, LOP3.LUT 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0090 | operator | `short a == b` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E.U16 2, LOP3.LUT 1, MOV 1, PRMT 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0091 | operator | `unsigned short a == b` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E.U16 2, LOP3.LUT 1, MOV 1, PRMT 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0092 | operator | `int a == b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0093 | operator | `unsigned int a == b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0094 | operator | `long long a == b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0095 | operator | `unsigned long long a == b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0096 | operator | `float a == b` | 21 | BRA 1, EXIT 2, FSETP.NEU.AND 1, I2F.U64 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0097 | operator | `double a == b` | 21 | BRA 1, DSETP.NEU.AND 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SEL 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0098 | operator | `signed char a != b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 2, LOP3.LUT 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0099 | operator | `unsigned char a != b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 2, LOP3.LUT 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0100 | operator | `short a != b` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E.U16 2, LOP3.LUT 1, MOV 1, PRMT 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0101 | operator | `unsigned short a != b` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E.U16 2, LOP3.LUT 1, MOV 1, PRMT 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0102 | operator | `int a != b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0103 | operator | `unsigned int a != b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0104 | operator | `long long a != b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0105 | operator | `unsigned long long a != b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0106 | operator | `float a != b` | 21 | BRA 1, EXIT 2, FSETP.NEU.AND 1, I2F.U64 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0107 | operator | `double a != b` | 21 | BRA 1, DSETP.NEU.AND 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SEL 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0108 | operator | `signed char a < b` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0109 | operator | `unsigned char a < b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, LDG.E.U8 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0110 | operator | `short a < b` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0111 | operator | `unsigned short a < b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, LDG.E.U16 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0112 | operator | `int a < b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0113 | operator | `unsigned int a < b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0114 | operator | `long long a < b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND.EX 1, ISETP.GE.U32.AND 2, LDG.E.64 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0115 | operator | `unsigned long long a < b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, ISETP.GE.U32.AND.EX 1, LDG.E.64 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0116 | operator | `float a < b` | 21 | BRA 1, EXIT 2, FSETP.GEU.AND 1, I2F.U64 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0117 | operator | `double a < b` | 21 | BRA 1, DSETP.GEU.AND 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SEL 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0118 | operator | `signed char a > b` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0119 | operator | `unsigned char a > b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, LDG.E.U8 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0120 | operator | `short a > b` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0121 | operator | `unsigned short a > b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, LDG.E.U16 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0122 | operator | `int a > b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0123 | operator | `unsigned int a > b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0124 | operator | `long long a > b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND.EX 1, ISETP.GT.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0125 | operator | `unsigned long long a > b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, ISETP.GT.U32.AND.EX 1, LDG.E.64 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0126 | operator | `float a > b` | 21 | BRA 1, EXIT 2, FSETP.GT.AND 1, I2F.U64 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0127 | operator | `double a > b` | 21 | BRA 1, DSETP.GT.AND 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SEL 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0128 | operator | `signed char a <= b` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0129 | operator | `unsigned char a <= b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, LDG.E.U8 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0130 | operator | `short a <= b` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0131 | operator | `unsigned short a <= b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, LDG.E.U16 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0132 | operator | `int a <= b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0133 | operator | `unsigned int a <= b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0134 | operator | `long long a <= b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND.EX 1, ISETP.GT.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0135 | operator | `unsigned long long a <= b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, ISETP.GT.U32.AND.EX 1, LDG.E.64 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0136 | operator | `float a <= b` | 21 | BRA 1, EXIT 2, FSETP.GTU.AND 1, I2F.U64 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0137 | operator | `double a <= b` | 21 | BRA 1, DSETP.GTU.AND 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SEL 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0138 | operator | `signed char a >= b` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0139 | operator | `unsigned char a >= b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, LDG.E.U8 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0140 | operator | `short a >= b` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0141 | operator | `unsigned short a >= b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, LDG.E.U16 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0142 | operator | `int a >= b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0143 | operator | `unsigned int a >= b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0144 | operator | `long long a >= b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND.EX 1, ISETP.GE.U32.AND 2, LDG.E.64 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0145 | operator | `unsigned long long a >= b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, ISETP.GE.U32.AND.EX 1, LDG.E.64 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0146 | operator | `float a >= b` | 21 | BRA 1, EXIT 2, FSETP.GE.AND 1, I2F.U64 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0147 | operator | `double a >= b` | 21 | BRA 1, DSETP.GE.AND 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SEL 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0148 | operator | `signed char a && b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 2, LDG.E.U8 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0149 | operator | `unsigned char a && b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 2, LDG.E.U8 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0150 | operator | `short a && b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 2, LDG.E.U16 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0151 | operator | `unsigned short a && b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 2, LDG.E.U16 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0152 | operator | `int a && b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 2, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0153 | operator | `unsigned int a && b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 2, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0154 | operator | `long long a && b` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 2, ISETP.NE.U32.AND 2, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0155 | operator | `unsigned long long a && b` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 2, ISETP.NE.U32.AND 2, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0156 | operator | `float a && b` | 22 | BRA 1, EXIT 2, FSETP.NEU.AND 2, I2F.U64 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0157 | operator | `double a && b` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 2, ISETP.NE.U32.AND 2, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0158 | operator | `signed char a \|\| b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 2, LOP3.LUT 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0159 | operator | `unsigned char a \|\| b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 2, LOP3.LUT 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0160 | operator | `short a \|\| b` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E.U16 2, LOP3.LUT 1, MOV 1, PRMT 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0161 | operator | `unsigned short a \|\| b` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E.U16 2, LOP3.LUT 1, MOV 1, PRMT 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0162 | operator | `int a \|\| b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0163 | operator | `unsigned int a \|\| b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0164 | operator | `long long a \|\| b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LOP3.LUT 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0165 | operator | `unsigned long long a \|\| b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LOP3.LUT 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0166 | operator | `float a \|\| b` | 22 | BRA 1, EXIT 2, FSETP.NEU.AND 1, FSETP.NEU.OR 1, I2F.U64 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0167 | operator | `double a \|\| b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LOP3.LUT 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0168 | compound | `signed char a += b` | 20 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 1, PRMT 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0169 | compound | `unsigned char a += b` | 19 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0170 | compound | `short a += b` | 20 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 1, PRMT 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0171 | compound | `unsigned short a += b` | 19 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0172 | compound | `int a += b` | 18 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0173 | compound | `unsigned int a += b` | 18 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0174 | compound | `long long a += b` | 18 | BRA 1, EXIT 2, IADD3 1, IADD3.X 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0175 | compound | `unsigned long long a += b` | 18 | BRA 1, EXIT 2, IADD3 1, IADD3.X 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0176 | compound | `float a += b` | 20 | BRA 1, EXIT 2, FADD 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0177 | compound | `double a += b` | 19 | BRA 1, DADD 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0178 | compound | `signed char a -= b` | 20 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 1, PRMT 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0179 | compound | `unsigned char a -= b` | 19 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0180 | compound | `short a -= b` | 20 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 1, PRMT 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0181 | compound | `unsigned short a -= b` | 19 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0182 | compound | `int a -= b` | 18 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0183 | compound | `unsigned int a -= b` | 18 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0184 | compound | `long long a -= b` | 18 | BRA 1, EXIT 2, IADD3 1, IADD3.X 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0185 | compound | `unsigned long long a -= b` | 18 | BRA 1, EXIT 2, IADD3 1, IADD3.X 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0186 | compound | `float a -= b` | 20 | BRA 1, EXIT 2, FADD 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0187 | compound | `double a -= b` | 19 | BRA 1, DADD 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0188 | compound | `signed char a *= b` | 20 | BRA 1, EXIT 2, IMAD 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 3, PRMT 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0189 | compound | `unsigned char a *= b` | 19 | BRA 1, EXIT 2, IMAD 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0190 | compound | `short a *= b` | 20 | BRA 1, EXIT 2, IMAD 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 3, PRMT 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0191 | compound | `unsigned short a *= b` | 19 | BRA 1, EXIT 2, IMAD 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0192 | compound | `int a *= b` | 18 | BRA 1, EXIT 2, IMAD 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0193 | compound | `unsigned int a *= b` | 18 | BRA 1, EXIT 2, IMAD 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0194 | compound | `long long a *= b` | 20 | BRA 1, EXIT 2, IADD3 1, IMAD 3, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0195 | compound | `unsigned long long a *= b` | 20 | BRA 1, EXIT 2, IADD3 1, IMAD 3, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0196 | compound | `float a *= b` | 20 | BRA 1, EXIT 2, FMUL 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0197 | compound | `double a *= b` | 19 | BRA 1, DMUL 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0198 | compound | `signed char a /= b` | 44 | BRA 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, I2F.RP 1, IABS 3, IADD3 3, IMAD 3, IMAD.HI.U32 2, IMAD.IADD 1, IMAD.MOV 3, IMAD.MOV.U32 4, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, ISETP.GT.U32.AND 1, ISETP.NE.AND 1, LDG.E.S8 2, LOP3.LUT 2, MUFU.RCP 1, PRMT 2, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0199 | compound | `unsigned char a /= b` | 40 | BRA 1, CALL.REL.NOINC 1, EXIT 2, F2I.U32.TRUNC.NTZ 1, FMUL 2, FMUL.RZ 1, FSEL 2, FSETP.GEU.AND 1, FSETP.GT.AND 1, I2F.U16 1, I2FP.F32.U32.RZ 1, IADD3 1, IMAD 1, IMAD.MOV.U32 5, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.U32.AND 1, LDG.E.U8 2, LOP3.LUT 2, MOV 4, MUFU.RCP 1, RET.REL.NODEC 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0200 | compound | `short a /= b` | 44 | BRA 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, I2F.RP 1, IABS 3, IADD3 3, IMAD 3, IMAD.HI.U32 2, IMAD.IADD 1, IMAD.MOV 3, IMAD.MOV.U32 4, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, ISETP.GT.U32.AND 1, ISETP.NE.AND 1, LDG.E.S16 2, LOP3.LUT 2, MUFU.RCP 1, PRMT 2, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0201 | compound | `unsigned short a /= b` | 40 | BRA 1, CALL.REL.NOINC 1, EXIT 2, F2I.U32.TRUNC.NTZ 1, FMUL 2, FMUL.RZ 1, FSEL 2, FSETP.GEU.AND 1, FSETP.GT.AND 1, I2F.U16 1, I2FP.F32.U32.RZ 1, IADD3 1, IMAD 1, IMAD.MOV.U32 5, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.U32.AND 1, LDG.E.U16 2, LOP3.LUT 2, MOV 4, MUFU.RCP 1, RET.REL.NODEC 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0202 | compound | `int a /= b` | 43 | BRA 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, I2F.RP 1, IABS 3, IADD3 6, IMAD 3, IMAD.HI.U32 2, IMAD.IADD 1, IMAD.MOV.U32 5, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, ISETP.GT.U32.AND 1, ISETP.NE.AND 1, LDG.E 2, LOP3.LUT 2, MUFU.RCP 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0203 | compound | `unsigned int a /= b` | 35 | BRA 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, I2F.U32.RP 1, IADD3 4, IMAD 3, IMAD.HI.U32 2, IMAD.MOV 2, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 3, ISETP.NE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 3, MUFU.RCP 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0204 | compound | `long long a /= b` | 128 | BRA 3, BSSY 1, BSYNC 1, CALL.REL.NOINC 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, F2I.U64.TRUNC 1, I2F.U32.RP 1, I2F.U64.RP 1, IADD3 17, IADD3.X 6, IMAD 12, IMAD.HI.U32 11, IMAD.IADD 1, IMAD.MOV 1, IMAD.MOV.U32 9, IMAD.SHL.U32 1, IMAD.WIDE.U32 8, IMAD.X 9, ISETP.GE.AND 3, ISETP.GE.U32.AND 5, ISETP.GE.U32.AND.EX 2, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 3, LDG.E.64 2, LOP3.LUT 3, MOV 1, MUFU.RCP 2, RET.REL.NODEC 1, S2R 2, SEL 14, STG.E.64 1, ULDC.64 1 |
| 0205 | compound | `unsigned long long a /= b` | 112 | BRA 3, BSSY 1, BSYNC 1, CALL.REL.NOINC 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, F2I.U64.TRUNC 1, I2F.U32.RP 1, I2F.U64.RP 1, IADD3 14, IADD3.X 8, IMAD 12, IMAD.HI.U32 11, IMAD.IADD 1, IMAD.MOV 1, IMAD.MOV.U32 6, IMAD.SHL.U32 1, IMAD.WIDE.U32 8, IMAD.X 4, ISETP.GE.U32.AND 5, ISETP.GE.U32.AND.EX 2, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 3, LDG.E.64 2, LOP3.LUT 2, MOV 4, MUFU.RCP 2, RET.REL.NODEC 1, S2R 2, SEL 8, STG.E.64 1, ULDC.64 1 |
| 0206 | compound | `float a /= b` | 138 | BRA 18, BSSY 3, BSYNC 3, CALL.REL.NOINC 1, EXIT 2, FADD.FTZ 2, FCHK 1, FFMA 14, FFMA.RM 1, FFMA.RP 1, FFMA.RZ 1, FSETP.GTU.FTZ.AND 2, FSETP.NEU.FTZ.AND 4, I2F.U64 2, IADD3 7, IMAD 3, IMAD.IADD 4, IMAD.MOV 1, IMAD.MOV.U32 12, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 4, ISETP.GE.U32.AND 2, ISETP.GT.AND 1, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 3, LDG.E.64 2, LEA 1, LOP3.LUT 17, MOV 1, MUFU.RCP 2, MUFU.RSQ 1, PLOP3.LUT 4, RET.REL.NODEC 1, S2R 2, SEL 2, SHF.L.U32 1, SHF.R.U32.HI 5, STG.E.64 1, ULDC.64 1 |
| 0207 | compound | `double a /= b` | 130 | BRA 11, BSSY 2, BSYNC 2, CALL.REL.NOINC 1, DFMA 17, DMUL 4, DMUL.RP 1, DSETP.NAN.AND 2, EXIT 2, FFMA 1, FSEL 2, FSETP.GEU.AND 3, FSETP.GT.AND 1, FSETP.GTU.AND 1, FSETP.NEU.AND 2, I2F.F64.U64 2, IADD3 4, IMAD 1, IMAD.IADD 2, IMAD.MOV 1, IMAD.MOV.U32 23, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMNMX 2, ISETP.EQ.OR 1, ISETP.GE.U32.AND 4, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 2, LDG.E.64 2, LOP3.LUT 18, MOV 1, MUFU.RCP64H 2, RET.REL.NODEC 1, S2R 2, SEL 3, STG.E.64 1, ULDC.64 1 |
| 0208 | compound | `signed char a %= b` | 42 | BRA 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, I2F.RP 1, IABS 3, IADD3 3, IMAD 3, IMAD.HI.U32 2, IMAD.IADD 1, IMAD.MOV 2, IMAD.MOV.U32 3, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 2, ISETP.NE.AND 1, LDG.E.S8 2, LOP3.LUT 1, MOV 1, MUFU.RCP 1, PRMT 2, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0209 | compound | `unsigned char a %= b` | 42 | BRA 1, CALL.REL.NOINC 1, EXIT 2, F2I.U32.TRUNC.NTZ 1, FMUL 2, FMUL.RZ 1, FSEL 2, FSETP.GEU.AND 1, FSETP.GT.AND 1, I2F.U16 1, I2F.U16.RZ 1, IADD3 2, IMAD 2, IMAD.MOV.U32 5, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.U32.AND 1, LDG.E.U8 2, LOP3.LUT 2, MOV 4, MUFU.RCP 1, RET.REL.NODEC 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0210 | compound | `short a %= b` | 42 | BRA 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, I2F.RP 1, IABS 3, IADD3 3, IMAD 3, IMAD.HI.U32 2, IMAD.IADD 1, IMAD.MOV 2, IMAD.MOV.U32 3, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 2, ISETP.NE.AND 1, LDG.E.S16 2, LOP3.LUT 1, MOV 1, MUFU.RCP 1, PRMT 2, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0211 | compound | `unsigned short a %= b` | 42 | BRA 1, CALL.REL.NOINC 1, EXIT 2, F2I.U32.TRUNC.NTZ 1, FMUL 2, FMUL.RZ 1, FSEL 2, FSETP.GEU.AND 1, FSETP.GT.AND 1, I2F.U16 1, I2F.U16.RZ 1, IADD3 2, IMAD 2, IMAD.MOV.U32 5, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.U32.AND 1, LDG.E.U16 2, LOP3.LUT 2, MOV 4, MUFU.RCP 1, RET.REL.NODEC 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0212 | compound | `int a %= b` | 41 | BRA 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, I2F.RP 1, IABS 3, IADD3 4, IMAD 3, IMAD.HI.U32 2, IMAD.IADD 1, IMAD.MOV 1, IMAD.MOV.U32 3, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 2, ISETP.NE.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, MUFU.RCP 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0213 | compound | `unsigned int a %= b` | 34 | BRA 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, I2F.U32.RP 1, IADD3 4, IMAD 3, IMAD.HI.U32 2, IMAD.IADD 1, IMAD.MOV.U32 3, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 3, ISETP.NE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 1, MUFU.RCP 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0214 | compound | `long long a %= b` | 121 | BRA 3, BSSY 1, BSYNC 1, CALL.REL.NOINC 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, F2I.U64.TRUNC 1, I2F.U32.RP 1, I2F.U64.RP 1, IADD3 15, IADD3.X 6, IMAD 12, IMAD.HI.U32 11, IMAD.IADD 1, IMAD.MOV 1, IMAD.MOV.U32 7, IMAD.SHL.U32 1, IMAD.WIDE.U32 8, IMAD.X 8, ISETP.GE.AND 2, ISETP.GE.U32.AND 5, ISETP.GE.U32.AND.EX 2, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 3, LDG.E.64 2, LOP3.LUT 2, MOV 3, MUFU.RCP 2, RET.REL.NODEC 1, S2R 2, SEL 12, STG.E.64 1, ULDC.64 1 |
| 0215 | compound | `unsigned long long a %= b` | 107 | BRA 3, BSSY 1, BSYNC 1, CALL.REL.NOINC 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, F2I.U64.TRUNC 1, I2F.U32.RP 1, I2F.U64.RP 1, IADD3 13, IADD3.X 8, IMAD 12, IMAD.HI.U32 11, IMAD.IADD 1, IMAD.MOV.U32 8, IMAD.WIDE.U32 8, IMAD.X 3, ISETP.GE.U32.AND 5, ISETP.GE.U32.AND.EX 2, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 3, LDG.E.64 2, LOP3.LUT 2, MOV 2, MUFU.RCP 2, RET.REL.NODEC 1, S2R 2, SEL 6, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0216 | compound | `signed char a &= b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, PRMT 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0217 | compound | `unsigned char a &= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0218 | compound | `short a &= b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, PRMT 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0219 | compound | `unsigned short a &= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0220 | compound | `int a &= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0221 | compound | `unsigned int a &= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0222 | compound | `long long a &= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LOP3.LUT 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0223 | compound | `unsigned long long a &= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LOP3.LUT 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0224 | compound | `signed char a \|= b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, PRMT 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0225 | compound | `unsigned char a \|= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0226 | compound | `short a \|= b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, PRMT 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0227 | compound | `unsigned short a \|= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0228 | compound | `int a \|= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0229 | compound | `unsigned int a \|= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0230 | compound | `long long a \|= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LOP3.LUT 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0231 | compound | `unsigned long long a \|= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LOP3.LUT 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0232 | compound | `signed char a ^= b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, PRMT 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0233 | compound | `unsigned char a ^= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0234 | compound | `short a ^= b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, PRMT 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0235 | compound | `unsigned short a ^= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0236 | compound | `int a ^= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0237 | compound | `unsigned int a ^= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0238 | compound | `long long a ^= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LOP3.LUT 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0239 | compound | `unsigned long long a ^= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LOP3.LUT 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0240 | compound | `signed char a <<= b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 2, MOV 2, PRMT 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0241 | compound | `unsigned char a <<= b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 2, LOP3.LUT 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0242 | compound | `short a <<= b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S16 2, MOV 2, PRMT 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0243 | compound | `unsigned short a <<= b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 2, LOP3.LUT 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0244 | compound | `int a <<= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SHF.L.U32 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0245 | compound | `unsigned int a <<= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 3, S2R 2, SHF.L.U32 2, STG.E.64 1, ULDC.64 1 |
| 0246 | compound | `long long a <<= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 2, SHF.L.U64.HI 1, STG.E.64 1, ULDC.64 1 |
| 0247 | compound | `unsigned long long a <<= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 2, SHF.L.U64.HI 1, STG.E.64 1, ULDC.64 1 |
| 0248 | compound | `signed char a >>= b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 2, MOV 2, PRMT 1, S2R 2, SHF.R.S32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0249 | compound | `unsigned char a >>= b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 2, LOP3.LUT 1, MOV 2, S2R 2, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0250 | compound | `short a >>= b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S16 2, MOV 2, PRMT 1, S2R 2, SHF.R.S32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0251 | compound | `unsigned short a >>= b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 2, LOP3.LUT 1, MOV 2, S2R 2, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0252 | compound | `int a >>= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0253 | compound | `unsigned int a >>= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 3, S2R 2, SHF.L.U32 1, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0254 | compound | `long long a >>= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, SHF.R.S64 1, STG.E.64 1, ULDC.64 1 |
| 0255 | compound | `unsigned long long a >>= b` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.U32.HI 1, SHF.R.U64 1, STG.E.64 1, ULDC.64 1 |
| 0256 | unary | `signed char -a` | 21 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, IMAD.X 1, ISETP.GE.U32.AND 1, LDG.E 1, S2R 2, SHF.L.U32 2, SHF.R.S32.HI 2, STG.E.64 1, ULDC.64 1, UMOV 1 |
| 0257 | unary | `unsigned char -a` | 18 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LOP3.LUT 1, MOV 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0258 | unary | `short -a` | 21 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, IMAD.X 1, ISETP.GE.U32.AND 1, LDG.E 1, S2R 2, SHF.L.U32 2, SHF.R.S32.HI 2, STG.E.64 1, ULDC.64 1, UMOV 1 |
| 0259 | unary | `unsigned short -a` | 18 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LOP3.LUT 1, MOV 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0260 | unary | `int -a` | 19 | BRA 1, EXIT 2, IADD3 1, IADD3.X 1, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1, UMOV 1 |
| 0261 | unary | `unsigned int -a` | 17 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0262 | unary | `long long -a` | 17 | BRA 1, EXIT 2, IADD3 1, IADD3.X 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0263 | unary | `unsigned long long -a` | 17 | BRA 1, EXIT 2, IADD3 1, IADD3.X 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0264 | unary | `float -a` | 18 | BRA 1, EXIT 2, FADD 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0265 | unary | `double -a` | 17 | BRA 1, DADD 1, EXIT 2, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0266 | unary | `signed char +a` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0267 | unary | `unsigned char +a` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0268 | unary | `short +a` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S16 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0269 | unary | `unsigned short +a` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0270 | unary | `int +a` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0271 | unary | `unsigned int +a` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0272 | unary | `long long +a` | 15 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0273 | unary | `unsigned long long +a` | 15 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0274 | unary | `float +a` | 17 | BRA 1, EXIT 2, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0275 | unary | `double +a` | 16 | BRA 1, EXIT 2, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0276 | unary | `signed char ~a` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LOP3.LUT 1, MOV 2, PRMT 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0277 | unary | `unsigned char ~a` | 17 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 1, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0278 | unary | `short ~a` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LOP3.LUT 1, MOV 2, PRMT 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0279 | unary | `unsigned short ~a` | 17 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 1, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0280 | unary | `int ~a` | 17 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LOP3.LUT 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0281 | unary | `unsigned int ~a` | 17 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0282 | unary | `long long ~a` | 17 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0283 | unary | `unsigned long long ~a` | 17 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0284 | unary | `signed char !a` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E.U8 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0285 | unary | `unsigned char !a` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E.U8 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0286 | unary | `short !a` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E.U16 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0287 | unary | `unsigned short !a` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E.U16 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0288 | unary | `int !a` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0289 | unary | `unsigned int !a` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0290 | unary | `long long !a` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0291 | unary | `unsigned long long !a` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0292 | unary | `float !a` | 19 | BRA 1, EXIT 2, FSETP.NEU.AND 1, I2F.U64 1, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0293 | unary | `double !a` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0294 | unary | `signed char ++a` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LEA 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0295 | unary | `unsigned char ++a` | 18 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LOP3.LUT 1, MOV 2, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0296 | unary | `short ++a` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LEA 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0297 | unary | `unsigned short ++a` | 18 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LOP3.LUT 1, MOV 2, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0298 | unary | `int ++a` | 17 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0299 | unary | `unsigned int ++a` | 17 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0300 | unary | `long long ++a` | 17 | BRA 1, EXIT 2, IADD3 1, IADD3.X 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0301 | unary | `unsigned long long ++a` | 17 | BRA 1, EXIT 2, IADD3 1, IADD3.X 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0302 | unary | `float ++a` | 18 | BRA 1, EXIT 2, FADD 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0303 | unary | `double ++a` | 17 | BRA 1, DADD 1, EXIT 2, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0304 | unary | `signed char a++` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0305 | unary | `unsigned char a++` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0306 | unary | `short a++` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S16 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0307 | unary | `unsigned short a++` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0308 | unary | `int a++` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0309 | unary | `unsigned int a++` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0310 | unary | `long long a++` | 15 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0311 | unary | `unsigned long long a++` | 15 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0312 | unary | `float a++` | 17 | BRA 1, EXIT 2, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0313 | unary | `double a++` | 16 | BRA 1, EXIT 2, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0314 | unary | `signed char --a` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LEA 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0315 | unary | `unsigned char --a` | 18 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LOP3.LUT 1, MOV 2, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0316 | unary | `short --a` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LEA 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0317 | unary | `unsigned short --a` | 18 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LOP3.LUT 1, MOV 2, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0318 | unary | `int --a` | 17 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0319 | unary | `unsigned int --a` | 17 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0320 | unary | `long long --a` | 17 | BRA 1, EXIT 2, IADD3 1, IADD3.X 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0321 | unary | `unsigned long long --a` | 17 | BRA 1, EXIT 2, IADD3 1, IADD3.X 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0322 | unary | `float --a` | 18 | BRA 1, EXIT 2, FADD 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0323 | unary | `double --a` | 17 | BRA 1, DADD 1, EXIT 2, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0324 | unary | `signed char a--` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0325 | unary | `unsigned char a--` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0326 | unary | `short a--` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S16 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0327 | unary | `unsigned short a--` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0328 | unary | `int a--` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0329 | unary | `unsigned int a--` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0330 | unary | `long long a--` | 15 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0331 | unary | `unsigned long long a--` | 15 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0332 | unary | `float a--` | 17 | BRA 1, EXIT 2, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0333 | unary | `double a--` | 16 | BRA 1, EXIT 2, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0334 | conversion | `(signed char)(bool)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0335 | conversion | `(unsigned char)(bool)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0336 | conversion | `(short)(bool)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0337 | conversion | `(unsigned short)(bool)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0338 | conversion | `(int)(bool)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0339 | conversion | `(unsigned int)(bool)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0340 | conversion | `(long long)(bool)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0341 | conversion | `(unsigned long long)(bool)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0342 | conversion | `(float)(bool)` | 19 | BRA 1, EXIT 2, FSEL 1, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0343 | conversion | `(double)(bool)` | 19 | BRA 1, EXIT 2, FSEL 1, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0344 | conversion | `(bool)(signed char)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.U8 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0345 | conversion | `(unsigned char)(signed char)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0346 | conversion | `(short)(signed char)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0347 | conversion | `(unsigned short)(signed char)` | 17 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 1, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0348 | conversion | `(int)(signed char)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0349 | conversion | `(unsigned int)(signed char)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0350 | conversion | `(long long)(signed char)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0351 | conversion | `(unsigned long long)(signed char)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0352 | conversion | `(float)(signed char)` | 17 | BRA 1, EXIT 2, I2F.S16 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0353 | conversion | `(double)(signed char)` | 16 | BRA 1, EXIT 2, I2F.F64.S16 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0354 | conversion | `(bool)(unsigned char)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.U8 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0355 | conversion | `(signed char)(unsigned char)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0356 | conversion | `(short)(unsigned char)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0357 | conversion | `(unsigned short)(unsigned char)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0358 | conversion | `(int)(unsigned char)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0359 | conversion | `(unsigned int)(unsigned char)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0360 | conversion | `(long long)(unsigned char)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0361 | conversion | `(unsigned long long)(unsigned char)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0362 | conversion | `(float)(unsigned char)` | 17 | BRA 1, EXIT 2, I2FP.F32.U32 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0363 | conversion | `(double)(unsigned char)` | 16 | BRA 1, EXIT 2, I2F.F64.U16 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0364 | conversion | `(bool)(short)` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E.U16 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0365 | conversion | `(signed char)(short)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0366 | conversion | `(unsigned char)(short)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0367 | conversion | `(unsigned short)(short)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0368 | conversion | `(int)(short)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S16 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0369 | conversion | `(unsigned int)(short)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S16 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0370 | conversion | `(long long)(short)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S16 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0371 | conversion | `(unsigned long long)(short)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S16 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0372 | conversion | `(float)(short)` | 17 | BRA 1, EXIT 2, I2F.S16 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0373 | conversion | `(double)(short)` | 16 | BRA 1, EXIT 2, I2F.F64.S16 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0374 | conversion | `(bool)(unsigned short)` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E.U16 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0375 | conversion | `(signed char)(unsigned short)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0376 | conversion | `(unsigned char)(unsigned short)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0377 | conversion | `(short)(unsigned short)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S16 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0378 | conversion | `(int)(unsigned short)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0379 | conversion | `(unsigned int)(unsigned short)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0380 | conversion | `(long long)(unsigned short)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0381 | conversion | `(unsigned long long)(unsigned short)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0382 | conversion | `(float)(unsigned short)` | 17 | BRA 1, EXIT 2, I2FP.F32.U32 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0383 | conversion | `(double)(unsigned short)` | 16 | BRA 1, EXIT 2, I2F.F64.U16 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0384 | conversion | `(bool)(int)` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0385 | conversion | `(signed char)(int)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0386 | conversion | `(unsigned char)(int)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0387 | conversion | `(short)(int)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S16 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0388 | conversion | `(unsigned short)(int)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0389 | conversion | `(unsigned int)(int)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0390 | conversion | `(long long)(int)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0391 | conversion | `(unsigned long long)(int)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0392 | conversion | `(float)(int)` | 17 | BRA 1, EXIT 2, I2FP.F32.S32 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0393 | conversion | `(double)(int)` | 16 | BRA 1, EXIT 2, I2F.F64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0394 | conversion | `(bool)(unsigned int)` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0395 | conversion | `(signed char)(unsigned int)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0396 | conversion | `(unsigned char)(unsigned int)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0397 | conversion | `(short)(unsigned int)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S16 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0398 | conversion | `(unsigned short)(unsigned int)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0399 | conversion | `(int)(unsigned int)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0400 | conversion | `(long long)(unsigned int)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0401 | conversion | `(unsigned long long)(unsigned int)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0402 | conversion | `(float)(unsigned int)` | 17 | BRA 1, EXIT 2, I2FP.F32.U32 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0403 | conversion | `(double)(unsigned int)` | 16 | BRA 1, EXIT 2, I2F.F64.U32 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0404 | conversion | `(bool)(long long)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0405 | conversion | `(signed char)(long long)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0406 | conversion | `(unsigned char)(long long)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0407 | conversion | `(short)(long long)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S16 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0408 | conversion | `(unsigned short)(long long)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0409 | conversion | `(int)(long long)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0410 | conversion | `(unsigned int)(long long)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0411 | conversion | `(unsigned long long)(long long)` | 15 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0412 | conversion | `(float)(long long)` | 17 | BRA 1, EXIT 2, I2F.S64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0413 | conversion | `(double)(long long)` | 16 | BRA 1, EXIT 2, I2F.F64.S64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0414 | conversion | `(bool)(unsigned long long)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0415 | conversion | `(signed char)(unsigned long long)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S8 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0416 | conversion | `(unsigned char)(unsigned long long)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U8 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0417 | conversion | `(short)(unsigned long long)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.S16 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0418 | conversion | `(unsigned short)(unsigned long long)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.U16 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0419 | conversion | `(int)(unsigned long long)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0420 | conversion | `(unsigned int)(unsigned long long)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0421 | conversion | `(long long)(unsigned long long)` | 15 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0422 | conversion | `(float)(unsigned long long)` | 17 | BRA 1, EXIT 2, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0423 | conversion | `(double)(unsigned long long)` | 16 | BRA 1, EXIT 2, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0424 | conversion | `(bool)(float)` | 19 | BRA 1, EXIT 2, FSETP.NEU.AND 1, I2F.U64 1, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0425 | conversion | `(signed char)(float)` | 19 | BRA 1, EXIT 2, F2I.TRUNC.NTZ 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, PRMT 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0426 | conversion | `(unsigned char)(float)` | 19 | BRA 1, EXIT 2, F2I.U32.TRUNC.NTZ 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0427 | conversion | `(short)(float)` | 19 | BRA 1, EXIT 2, F2I.TRUNC.NTZ 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, PRMT 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0428 | conversion | `(unsigned short)(float)` | 19 | BRA 1, EXIT 2, F2I.U32.TRUNC.NTZ 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0429 | conversion | `(int)(float)` | 18 | BRA 1, EXIT 2, F2I.TRUNC.NTZ 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0430 | conversion | `(unsigned int)(float)` | 18 | BRA 1, EXIT 2, F2I.U32.TRUNC.NTZ 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0431 | conversion | `(long long)(float)` | 17 | BRA 1, EXIT 2, F2I.S64.TRUNC 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0432 | conversion | `(unsigned long long)(float)` | 17 | BRA 1, EXIT 2, F2I.U64.TRUNC 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0433 | conversion | `(double)(float)` | 17 | BRA 1, EXIT 2, F2F.F64.F32 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0434 | conversion | `(bool)(double)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0435 | conversion | `(signed char)(double)` | 19 | BRA 1, EXIT 2, F2I.F64.TRUNC 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, PRMT 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0436 | conversion | `(unsigned char)(double)` | 19 | BRA 1, EXIT 2, F2I.U32.F64.TRUNC 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0437 | conversion | `(short)(double)` | 19 | BRA 1, EXIT 2, F2I.F64.TRUNC 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, PRMT 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0438 | conversion | `(unsigned short)(double)` | 19 | BRA 1, EXIT 2, F2I.U32.F64.TRUNC 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0439 | conversion | `(int)(double)` | 18 | BRA 1, EXIT 2, F2I.F64.TRUNC 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0440 | conversion | `(unsigned int)(double)` | 18 | BRA 1, EXIT 2, F2I.U32.F64.TRUNC 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0441 | conversion | `(long long)(double)` | 17 | BRA 1, EXIT 2, F2I.S64.F64.TRUNC 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0442 | conversion | `(unsigned long long)(double)` | 17 | BRA 1, EXIT 2, F2I.U64.F64.TRUNC 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0443 | conversion | `(float)(double)` | 18 | BRA 1, EXIT 2, F2F.F32.F64 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0444 | conditional | `c ? a : b over bool` | 27 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 3, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 3, ISETP.NE.U32.AND 3, LDG.E.64 3, LOP3.LUT 1, S2R 2, SEL 2, STG.E.64 1, ULDC.64 1 |
| 0445 | conditional | `c ? a : b over signed char` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, LDG.E.U16 2, MOV 1, PRMT 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0446 | conditional | `c ? a : b over unsigned char` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E 2, LDG.E.64 1, LOP3.LUT 1, MOV 2, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0447 | conditional | `c ? a : b over short` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, LDG.E.U16 2, MOV 1, PRMT 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0448 | conditional | `c ? a : b over unsigned short` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E 2, LDG.E.64 1, LOP3.LUT 1, MOV 2, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0449 | conditional | `c ? a : b over int` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E 2, LDG.E.64 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0450 | conditional | `c ? a : b over unsigned int` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E 2, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0451 | conditional | `c ? a : b over long long` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 3, S2R 2, SEL 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0452 | conditional | `c ? a : b over unsigned long long` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 3, S2R 2, SEL 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0453 | conditional | `c ? a : b over float` | 23 | BRA 1, EXIT 2, I2F.U64 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 3, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0454 | conditional | `c ? a : b over double` | 22 | BRA 1, EXIT 2, I2F.F64.U64 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 3, S2R 2, SEL 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0455 | statement | `int r = a; if (b > 0) { r = a + 1; }` | 20 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, LDG.E 2, MOV 1, S2R 2, SEL 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0456 | statement | `int r; if (b > 0) { r = a + 1; } else { r = a - 1; }` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.IADD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, LDG.E 2, MOV 1, S2R 2, SEL 1, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0457 | statement | `int r; if (b > 2) { r = a + 3; } else if (b > 1) { r = a + 2; } else if (b > 0) { r = a + 1; } else { r = a; }` | 24 | BRA 1, EXIT 2, IMAD 1, IMAD.IADD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 2, ISETP.NE.AND 1, LDG.E 2, S2R 2, SEL 3, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0458 | statement | `int r; switch (b & 7) { case 0: r = a; break; case 1: r = a + 1; break; case 2: r = a * 2; break; case 3: r = a - 3; break; case 4: r = a ^ 4; break; case 5: r = a \| 5; break; case 6: r = a & 6; break; default: r = -a; break; }` | 50 | BRA 15, BSSY 1, BSYNC 1, EXIT 2, IADD3 2, IMAD 1, IMAD.MOV 1, IMAD.MOV.U32 2, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 2, ISETP.NE.AND 7, LDG.E 1, LDG.E.U16 1, LOP3.LUT 4, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0459 | statement | `int r; switch (b) { case 1: r = a; break; case 100: r = a + 1; break; case 10000: r = a + 2; break; case -7: r = a + 3; break; default: r = 0; break; }` | 38 | BRA 6, BRX 1, BSSY 1, BSYNC 1, EXIT 2, IADD3 4, IMAD 1, IMAD.MOV.U32 4, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, IMNMX.U32 1, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, ISETP.NE.AND 2, LDC 1, LDG.E 2, S2R 2, SHF.R.S32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0460 | statement | `int r = 0; switch (b & 3) { case 0: r += a; case 1: r += 1; case 2: r += 2; break; default: r = a; }` | 30 | BRA 4, BSSY 1, BSYNC 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 3, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 3, LDG.E 1, LDG.E.U16 1, LOP3.LUT 1, MOV 2, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0461 | statement | `int r = 0; for (int i = 0; i < b; i++) { r += a; }` | 72 | BRA 10, BSSY 4, BSYNC 4, CS2R 1, EXIT 2, IADD3 19, IMAD 1, IMAD.IADD 2, IMAD.MOV.U32 4, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, ISETP.GT.AND 4, ISETP.NE.AND 3, ISETP.NE.OR 1, LDG.E 2, LOP3.LUT 1, PLOP3.LUT 3, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0462 | statement | `int r = 0; for (int i = 0; i < 8; i++) { r += a >> i; }` | 23 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LEA.HI.SX32 7, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0463 | statement | `int r = 0; int i = 0; while (i < b) { r ^= a + i; i++; }` | 53 | BRA 6, BSSY 3, BSYNC 3, CS2R 1, EXIT 2, IADD3 9, IMAD 1, IMAD.IADD 3, IMAD.MOV.U32 5, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, ISETP.NE.AND 3, LDG.E 2, LOP3.LUT 4, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0464 | statement | `int r = 0; int i = 0; do { r += a; i++; } while (i < b);` | 69 | BRA 9, BSSY 3, BSYNC 3, EXIT 2, IADD3 19, IMAD 1, IMAD.IADD 2, IMAD.MOV.U32 5, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMNMX 1, ISETP.GE.U32.AND 2, ISETP.GT.AND 4, ISETP.NE.AND 3, ISETP.NE.OR 1, LDG.E 2, LOP3.LUT 1, PLOP3.LUT 3, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0465 | statement | `int r = 0; for (int i = 0; i < b; i++) { if (r > a) { break; } r += i; }` | 32 | BRA 3, BSSY 2, BSYNC 2, CS2R 1, EXIT 2, IADD3 2, IMAD 1, IMAD.MOV.U32 3, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 2, ISETP.GE.U32.AND 1, ISETP.LE.AND 1, ISETP.LT.OR 1, LDG.E 2, MOV 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0466 | statement | `int r = 0; for (int i = 0; i < b; i++) { if ((i & 1) != 0) { continue; } r += i; }` | 81 | BRA 10, BSSY 4, BSYNC 4, CS2R 1, EXIT 2, IADD3 24, IMAD 1, IMAD.IADD 2, IMAD.MOV.U32 6, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, ISETP.GT.AND 4, ISETP.NE.AND 3, ISETP.NE.OR 1, ISETP.NE.U32.AND 1, LDG.E 1, LEA 1, LOP3.LUT 2, PLOP3.LUT 3, S2R 2, SEL 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0467 | statement | `int r = 0; for (int i = 0; i < b; i++) { for (int j = 0; j < a; j++) { r += i * j; } }` | 124 | BRA 13, BSSY 5, BSYNC 5, EXIT 2, IADD3 24, IMAD 27, IMAD.IADD 3, IMAD.MOV.U32 11, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, ISETP.GE.AND 3, ISETP.GE.U32.AND 2, ISETP.GT.AND 4, ISETP.NE.AND 4, ISETP.NE.OR 1, LDG.E 2, LEA 4, LOP3.LUT 1, MOV 1, PLOP3.LUT 3, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0468 | statement | `int r = a; if (b > 0) { goto skip; } r = -a; skip: r += 1;` | 20 | BRA 1, EXIT 2, IADD3 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, LDG.E 2, MOV 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0469 | statement | `int r = 0; int i = 0; again: r += a; i++; if (i < b) { goto again; }` | 69 | BRA 9, BSSY 3, BSYNC 3, EXIT 2, IADD3 19, IMAD 1, IMAD.IADD 2, IMAD.MOV.U32 5, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMNMX 1, ISETP.GE.U32.AND 2, ISETP.GT.AND 4, ISETP.NE.AND 3, ISETP.NE.OR 1, LDG.E 2, LOP3.LUT 1, PLOP3.LUT 3, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0470 | statement | `int r = a; if (b < 0) { return; } r += b;` | 20 | BRA 1, EXIT 3, IADD3 1, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, LDG.E 2, MOV 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0471 | statement | `int r = (b > 0) ? ((a > 0) ? 1 : 2) : ((a > 0) ? 3 : 4);` | 24 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 4, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 2, LDG.E 2, MOV 2, S2R 2, SEL 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0472 | statement | `int r = ((a != 0) && ((b / a) > 1)) ? 1 : 0;` | 50 | BRA 2, BSSY 1, BSYNC 1, EXIT 2, F2I.FTZ.U32.TRUNC.NTZ 1, I2F.RP 1, IABS 3, IADD3 5, IMAD 3, IMAD.HI.U32 2, IMAD.IADD 1, IMAD.MOV 1, IMAD.MOV.U32 5, IMAD.SHL.U32 1, IMAD.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, ISETP.GT.AND 1, ISETP.GT.U32.AND 1, ISETP.NE.AND 2, LDG.E 2, LOP3.LUT 2, MUFU.RCP 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1, UMOV 1 |
| 0473 | statement | `int r = a; while (r > 1) { r = ((r & 1) != 0) ? ((3 * r) + 1) : (r / 2); if (r == b) { break; } }` | 33 | BRA 3, BSSY 1, BSYNC 1, EXIT 2, IMAD 2, IMAD.MOV.U32 4, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, ISETP.NE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 4, S2R 2, SHF.R.S32.HI 1, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0474 | memory | `__shared__ int s[256]; s[threadIdx.x & 255u] = a; __syncthreads(); int r = s[(threadIdx.x + 1u) & 255u];` | 23 | BAR.SYNC.DEFER_BLOCKING 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LDS 1, LOP3.LUT 2, MOV 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, STS 1, ULDC.64 1 |
| 0475 | memory | `extern __shared__ int d[]; d[threadIdx.x] = a; __syncthreads(); int r = d[threadIdx.x ^ 1u];` | 21 | BAR.SYNC.DEFER_BLOCKING 1, BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LDS 1, LOP3.LUT 1, MOV 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, STS 1, ULDC.64 1 |
| 0476 | memory | `int l[16]; for (int i = 0; i < 16; i++) { l[i] = a + i; } int r = l[b & 15];` | 67 | BRA 1, EXIT 2, IADD3 15, IMAD 1, IMAD.MOV.U32 19, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, ISETP.EQ.AND 16, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0477 | memory | `int r = measuring_stick_constant[b & 15] + a;` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.IADD 1, IMAD.MOV.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDC 1, LDG.E 2, LOP3.LUT 1, MOV 1, S2R 2, SHF.L.U32 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0478 | memory | `int r = (int)__ldg(&in[(4u * thread) + 2u]) + a;` | 20 | BRA 1, EXIT 2, IADD3 2, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 1, LDG.E.CONSTANT 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0479 | memory | `int r = (int)*(volatile const unsigned long long *)&in[(4u * thread) + 2u] + a;` | 18 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LDG.E.STRONG.SYS 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0480 | memory | `const unsigned long long *p = in + (4u * thread); p += 2; int r = (int)*p + a;` | 18 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0481 | memory | `int g[4][4]; for (int i = 0; i < 16; i++) { g[i >> 2][i & 3] = a * i; } int r = g[b & 3][(b >> 2) & 3];` | 68 | BRA 1, EXIT 2, IMAD 12, IMAD.IADD 1, IMAD.MOV.U32 9, IMAD.SHL.U32 4, IMAD.WIDE.U32 2, ISETP.EQ.AND 16, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 2, MOV 10, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0482 | memory | `struct { int x; short y; char z; } v; v.x = a; v.y = (short)b; v.z = (char)(a ^ b); int r = v.x + v.y + v.z;` | 21 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, PRMT 2, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0483 | memory | `struct { int x; int y; } v[4]; for (int i = 0; i < 4; i++) { v[i].x = a + i; v[i].y = b - i; } int r = v[b & 3].x * v[a & 3].y;` | 44 | BRA 1, EXIT 2, IADD3 6, IMAD 2, IMAD.MOV.U32 10, IMAD.SHL.U32 3, IMAD.WIDE.U32 2, ISETP.EQ.AND 8, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 2, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0484 | memory | `union { float f; int i; } v; v.i = a; v.f += 1.0f; int r = v.i;` | 17 | BRA 1, EXIT 2, FADD 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0485 | memory | `int r = (int)(((const unsigned int *)in)[(8u * thread) + 1u]) + a;` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.IADD 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 2, MOV 3, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0486 | memory | `int r = (int)(((const unsigned char *)in)[(32u * thread) + (b & 31)]) + a;` | 23 | BRA 1, EXIT 2, IADD3 2, IADD3.X 1, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LDG.E.U8 1, LOP3.LUT 1, MOV 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0487 | built-in | `threadIdx.x` | 13 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 1, ISETP.GE.U32.AND 1, MOV 3, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0488 | built-in | `threadIdx.y` | 14 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 1, ISETP.GE.U32.AND 1, MOV 3, S2R 3, STG.E.64 1, ULDC.64 1 |
| 0489 | built-in | `threadIdx.z` | 14 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 1, ISETP.GE.U32.AND 1, MOV 3, S2R 3, STG.E.64 1, ULDC.64 1 |
| 0490 | built-in | `blockIdx.x` | 13 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 1, ISETP.GE.U32.AND 1, MOV 3, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0491 | built-in | `blockIdx.y` | 15 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 1, ISETP.GE.U32.AND 1, MOV 4, S2R 2, S2UR 1, STG.E.64 1, ULDC.64 1 |
| 0492 | built-in | `blockIdx.z` | 15 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 1, ISETP.GE.U32.AND 1, MOV 4, S2R 2, S2UR 1, STG.E.64 1, ULDC.64 1 |
| 0493 | built-in | `blockDim.x` | 14 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 1, ISETP.GE.U32.AND 1, MOV 4, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0494 | built-in | `blockDim.y` | 14 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 1, ISETP.GE.U32.AND 1, MOV 4, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0495 | built-in | `blockDim.z` | 14 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 1, ISETP.GE.U32.AND 1, MOV 4, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0496 | built-in | `gridDim.x` | 14 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 1, ISETP.GE.U32.AND 1, MOV 4, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0497 | built-in | `gridDim.y` | 14 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 1, ISETP.GE.U32.AND 1, MOV 4, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0498 | built-in | `gridDim.z` | 14 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 1, ISETP.GE.U32.AND 1, MOV 4, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0499 | built-in | `warpSize` | 14 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 1, ISETP.GE.U32.AND 1, MOV 4, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0500 | call | `int r = measuring_stick_noinline(a, b);` | 24 | BRA 1, CALL.REL.NOINC 1, EXIT 2, IMAD 2, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 3, RET.REL.NODEC 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0501 | call | `int r = measuring_stick_forceinline(a, b);` | 19 | BRA 1, EXIT 2, IMAD 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0502 | call | `int r = measuring_stick_recursive(a & 15, b);` | 91 | BRA 8, BSSY 4, BSYNC 4, CALL.REL.NOINC 1, EXIT 2, IADD3 34, IMAD 1, IMAD.IADD 2, IMAD.MOV.U32 8, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMNMX 1, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, ISETP.GT.AND 3, ISETP.GT.OR 1, ISETP.NE.AND 1, LDG.E 2, LOP3.LUT 3, MOV 1, PLOP3.LUT 3, RET.REL.NODEC 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0503 | call | `int (*f)(int, int) = ((b & 1) != 0) ? measuring_stick_noinline : measuring_stick_other; int r = f(a, b);` | 34 | BRA 1, BSSY 1, BSYNC 1, CALL.REL.NOINC 1, EXIT 2, IADD3 1, IMAD 2, IMAD.MOV.U32 3, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.U32.AND 1, ISETP.NE.U32.AND.EX 1, LDC.64 1, LDG.E 2, LOP3.LUT 3, MOV 2, RET.REL.NODEC 2, S2R 2, SEL 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0504 | integer intrinsic | `unsigned int __brev(a)` | 17 | BRA 1, BREV 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0505 | integer intrinsic | `unsigned long long __brevll(a)` | 17 | BRA 1, BREV 2, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0506 | integer intrinsic | `unsigned int __byte_perm(a, b, c)` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 3, LOP3.LUT 1, MOV 2, PRMT 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0507 | integer intrinsic | `int __clz(a)` | 18 | BRA 1, EXIT 2, FLO.U32 1, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0508 | integer intrinsic | `int __clzll(a)` | 23 | BRA 1, EXIT 2, FLO.U32 1, IADD3 2, IMAD 1, IMAD.MOV 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0509 | integer intrinsic | `int __ffs(a)` | 19 | BRA 1, BREV 1, EXIT 2, FLO.U32.SH 1, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0510 | integer intrinsic | `int __ffsll(a)` | 26 | BRA 1, EXIT 2, FLO.U32 1, IADD3 4, IADD3.X 2, IMAD 1, IMAD.MOV 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.U32.AND 1, LDG.E.64 1, LOP3.LUT 2, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0511 | integer intrinsic | `unsigned int __fns(a, b, c)` | 71 | BRA 4, BREV 1, BSSY 1, BSYNC 1, EXIT 2, IADD3 6, IMAD 1, IMAD.IADD 5, IMAD.MOV 1, IMAD.MOV.U32 6, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.EQ.AND 1, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, ISETP.LT.AND 1, ISETP.LT.U32.AND 5, LDG.E 3, LOP3.LUT 2, PLOP3.LUT 2, POPC 4, PRMT 2, S2R 2, SEL 3, SGXT.U32 4, SHF.L.U32 2, SHF.R.U32.HI 5, STG.E.64 1, ULDC.64 1 |
| 0512 | integer intrinsic | `unsigned int __funnelshift_l(a, b, c)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 3, MOV 3, S2R 2, SHF.L.U32 1, SHF.L.W.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0513 | integer intrinsic | `unsigned int __funnelshift_lc(a, b, c)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 3, MOV 3, S2R 2, SHF.L.U32 1, SHF.L.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0514 | integer intrinsic | `unsigned int __funnelshift_r(a, b, c)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 3, MOV 3, S2R 2, SHF.L.U32 1, SHF.R.W.U32 1, STG.E.64 1, ULDC.64 1 |
| 0515 | integer intrinsic | `unsigned int __funnelshift_rc(a, b, c)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 3, MOV 3, S2R 2, SHF.L.U32 1, SHF.R.U32 1, STG.E.64 1, ULDC.64 1 |
| 0516 | integer intrinsic | `int __hadd(a, b)` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LEA.HI.SX32 1, LOP3.LUT 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0517 | integer intrinsic | `int __mul24(a, b)` | 22 | BRA 1, EXIT 2, IMAD 2, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 1, S2R 2, SHF.L.U32 2, SHF.R.S32.HI 3, STG.E.64 1, ULDC.64 1 |
| 0518 | integer intrinsic | `long long __mul64hi(a, b)` | 33 | BRA 1, EXIT 2, IADD3 3, IADD3.X 1, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 5, IMAD.WIDE.U32.X 1, IMAD.X 2, ISETP.GE.U32.AND 1, ISETP.LT.AND 2, LDG.E.64 2, MOV 2, S2R 2, SEL 4, STG.E.64 1, ULDC.64 1 |
| 0519 | integer intrinsic | `int __mulhi(a, b)` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.HI 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0520 | integer intrinsic | `int __popc(a)` | 17 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, POPC 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0521 | integer intrinsic | `int __popcll(a)` | 19 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, POPC 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0522 | integer intrinsic | `int __rhadd(a, b)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.IADD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 2, MOV 1, S2R 2, SHF.R.S32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0523 | integer intrinsic | `unsigned int __sad(a, b, c)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 3, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1, VABSDIFF 1 |
| 0524 | integer intrinsic | `unsigned int __uhadd(a, b)` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LEA.HI 1, LOP3.LUT 2, MOV 2, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0525 | integer intrinsic | `unsigned int __umul24(a, b)` | 20 | BRA 1, EXIT 2, IMAD 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0526 | integer intrinsic | `unsigned long long __umul64hi(a, b)` | 23 | BRA 1, EXIT 2, IADD3 1, IADD3.X 1, IMAD 1, IMAD.WIDE.U32 5, IMAD.WIDE.U32.X 1, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0527 | integer intrinsic | `unsigned int __umulhi(a, b)` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.HI.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0528 | integer intrinsic | `unsigned int __urhadd(a, b)` | 21 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 3, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 2, S2R 2, SHF.L.U32 1, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0529 | integer intrinsic | `unsigned int __usad(a, b, c)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 3, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1, VABSDIFF.U32 1 |
| 0530 | integer intrinsic | `int min(a, b)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, LDG.E 2, S2R 2, SEL 1, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0531 | integer intrinsic | `unsigned int min(a, b)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0532 | integer intrinsic | `long long min(a, b)` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LT.AND.EX 1, ISETP.LT.U32.AND 1, LDG.E.64 2, S2R 2, SEL 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0533 | integer intrinsic | `unsigned long long min(a, b)` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LT.U32.AND 1, ISETP.LT.U32.AND.EX 1, LDG.E.64 2, S2R 2, SEL 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0534 | integer intrinsic | `int max(a, b)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, LDG.E 2, S2R 2, SEL 1, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0535 | integer intrinsic | `unsigned int max(a, b)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0536 | integer intrinsic | `long long max(a, b)` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND.EX 1, ISETP.GT.U32.AND 1, LDG.E.64 2, S2R 2, SEL 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0537 | integer intrinsic | `unsigned long long max(a, b)` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, ISETP.GT.U32.AND.EX 1, LDG.E.64 2, S2R 2, SEL 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0538 | integer intrinsic | `int abs(a)` | 19 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, LDG.E 1, MOV 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0539 | integer intrinsic | `long labs(a)` | 19 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, LDG.E 1, MOV 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0540 | integer intrinsic | `long long llabs(a)` | 20 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMAD.X 1, ISETP.GE.U32.AND 1, ISETP.LT.AND 1, LDG.E.64 1, MOV 1, S2R 2, SEL 2, STG.E.64 1, ULDC.64 1 |
| 0541 | casting intrinsic | `float __double2float_rd(a)` | 18 | BRA 1, EXIT 2, F2F.F32.F64.RM 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0542 | casting intrinsic | `float __double2float_rn(a)` | 18 | BRA 1, EXIT 2, F2F.F32.F64 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0543 | casting intrinsic | `float __double2float_ru(a)` | 18 | BRA 1, EXIT 2, F2F.F32.F64.RP 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0544 | casting intrinsic | `float __double2float_rz(a)` | 18 | BRA 1, EXIT 2, F2F.F32.F64.RZ 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0545 | casting intrinsic | `int __double2hiint(a)` | 18 | BRA 1, EXIT 2, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0546 | casting intrinsic | `int __double2int_rd(a)` | 18 | BRA 1, EXIT 2, F2I.F64.FLOOR 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0547 | casting intrinsic | `int __double2int_rn(a)` | 18 | BRA 1, EXIT 2, F2I.F64 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0548 | casting intrinsic | `int __double2int_ru(a)` | 18 | BRA 1, EXIT 2, F2I.F64.CEIL 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0549 | casting intrinsic | `int __double2int_rz(a)` | 18 | BRA 1, EXIT 2, F2I.F64.TRUNC 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0550 | casting intrinsic | `long long __double2ll_rd(a)` | 17 | BRA 1, EXIT 2, F2I.S64.F64.FLOOR 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0551 | casting intrinsic | `long long __double2ll_rn(a)` | 17 | BRA 1, EXIT 2, F2I.S64.F64 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0552 | casting intrinsic | `long long __double2ll_ru(a)` | 17 | BRA 1, EXIT 2, F2I.S64.F64.CEIL 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0553 | casting intrinsic | `long long __double2ll_rz(a)` | 17 | BRA 1, EXIT 2, F2I.S64.F64.TRUNC 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0554 | casting intrinsic | `int __double2loint(a)` | 17 | BRA 1, EXIT 2, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0555 | casting intrinsic | `unsigned int __double2uint_rd(a)` | 18 | BRA 1, EXIT 2, F2I.U32.F64.FLOOR 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0556 | casting intrinsic | `unsigned int __double2uint_rn(a)` | 18 | BRA 1, EXIT 2, F2I.U32.F64 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0557 | casting intrinsic | `unsigned int __double2uint_ru(a)` | 18 | BRA 1, EXIT 2, F2I.U32.F64.CEIL 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0558 | casting intrinsic | `unsigned int __double2uint_rz(a)` | 18 | BRA 1, EXIT 2, F2I.U32.F64.TRUNC 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0559 | casting intrinsic | `unsigned long long __double2ull_rd(a)` | 17 | BRA 1, EXIT 2, F2I.U64.F64.FLOOR 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0560 | casting intrinsic | `unsigned long long __double2ull_rn(a)` | 17 | BRA 1, EXIT 2, F2I.U64.F64 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0561 | casting intrinsic | `unsigned long long __double2ull_ru(a)` | 17 | BRA 1, EXIT 2, F2I.U64.F64.CEIL 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0562 | casting intrinsic | `unsigned long long __double2ull_rz(a)` | 17 | BRA 1, EXIT 2, F2I.U64.F64.TRUNC 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0563 | casting intrinsic | `long long __double_as_longlong(a)` | 16 | BRA 1, EXIT 2, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0564 | casting intrinsic | `int __float2int_rd(a)` | 18 | BRA 1, EXIT 2, F2I.FLOOR.NTZ 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0565 | casting intrinsic | `int __float2int_rn(a)` | 18 | BRA 1, EXIT 2, F2I.NTZ 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0566 | casting intrinsic | `int __float2int_ru(a)` | 18 | BRA 1, EXIT 2, F2I.CEIL.NTZ 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0567 | casting intrinsic | `int __float2int_rz(a)` | 18 | BRA 1, EXIT 2, F2I.TRUNC.NTZ 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0568 | casting intrinsic | `long long __float2ll_rd(a)` | 17 | BRA 1, EXIT 2, F2I.S64.FLOOR 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0569 | casting intrinsic | `long long __float2ll_rn(a)` | 17 | BRA 1, EXIT 2, F2I.S64 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0570 | casting intrinsic | `long long __float2ll_ru(a)` | 17 | BRA 1, EXIT 2, F2I.S64.CEIL 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0571 | casting intrinsic | `long long __float2ll_rz(a)` | 17 | BRA 1, EXIT 2, F2I.S64.TRUNC 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0572 | casting intrinsic | `unsigned int __float2uint_rd(a)` | 18 | BRA 1, EXIT 2, F2I.U32.FLOOR.NTZ 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0573 | casting intrinsic | `unsigned int __float2uint_rn(a)` | 18 | BRA 1, EXIT 2, F2I.U32.NTZ 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0574 | casting intrinsic | `unsigned int __float2uint_ru(a)` | 18 | BRA 1, EXIT 2, F2I.U32.CEIL.NTZ 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0575 | casting intrinsic | `unsigned int __float2uint_rz(a)` | 18 | BRA 1, EXIT 2, F2I.U32.TRUNC.NTZ 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0576 | casting intrinsic | `unsigned long long __float2ull_rd(a)` | 17 | BRA 1, EXIT 2, F2I.U64.FLOOR 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0577 | casting intrinsic | `unsigned long long __float2ull_rn(a)` | 17 | BRA 1, EXIT 2, F2I.U64 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0578 | casting intrinsic | `unsigned long long __float2ull_ru(a)` | 17 | BRA 1, EXIT 2, F2I.U64.CEIL 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0579 | casting intrinsic | `unsigned long long __float2ull_rz(a)` | 17 | BRA 1, EXIT 2, F2I.U64.TRUNC 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0580 | casting intrinsic | `int __float_as_int(a)` | 17 | BRA 1, EXIT 2, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0581 | casting intrinsic | `unsigned int __float_as_uint(a)` | 17 | BRA 1, EXIT 2, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0582 | casting intrinsic | `double __hiloint2double(a, b)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0583 | casting intrinsic | `double __int2double_rn(a)` | 16 | BRA 1, EXIT 2, I2F.F64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0584 | casting intrinsic | `float __int2float_rd(a)` | 17 | BRA 1, EXIT 2, I2F.RM 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0585 | casting intrinsic | `float __int2float_rn(a)` | 17 | BRA 1, EXIT 2, I2FP.F32.S32 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0586 | casting intrinsic | `float __int2float_ru(a)` | 17 | BRA 1, EXIT 2, I2F.RP 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0587 | casting intrinsic | `float __int2float_rz(a)` | 17 | BRA 1, EXIT 2, I2FP.F32.S32.RZ 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0588 | casting intrinsic | `float __int_as_float(a)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0589 | casting intrinsic | `double __ll2double_rd(a)` | 16 | BRA 1, EXIT 2, I2F.F64.S64.RM 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0590 | casting intrinsic | `double __ll2double_rn(a)` | 16 | BRA 1, EXIT 2, I2F.F64.S64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0591 | casting intrinsic | `double __ll2double_ru(a)` | 16 | BRA 1, EXIT 2, I2F.F64.S64.RP 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0592 | casting intrinsic | `double __ll2double_rz(a)` | 16 | BRA 1, EXIT 2, I2F.F64.S64.RZ 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0593 | casting intrinsic | `float __ll2float_rd(a)` | 17 | BRA 1, EXIT 2, I2F.S64.RM 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0594 | casting intrinsic | `float __ll2float_rn(a)` | 17 | BRA 1, EXIT 2, I2F.S64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0595 | casting intrinsic | `float __ll2float_ru(a)` | 17 | BRA 1, EXIT 2, I2F.S64.RP 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0596 | casting intrinsic | `float __ll2float_rz(a)` | 17 | BRA 1, EXIT 2, I2F.S64.RZ 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0597 | casting intrinsic | `double __longlong_as_double(a)` | 15 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0598 | casting intrinsic | `double __uint2double_rn(a)` | 16 | BRA 1, EXIT 2, I2F.F64.U32 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0599 | casting intrinsic | `float __uint2float_rd(a)` | 17 | BRA 1, EXIT 2, I2F.U32.RM 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0600 | casting intrinsic | `float __uint2float_rn(a)` | 17 | BRA 1, EXIT 2, I2FP.F32.U32 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0601 | casting intrinsic | `float __uint2float_ru(a)` | 17 | BRA 1, EXIT 2, I2F.U32.RP 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0602 | casting intrinsic | `float __uint2float_rz(a)` | 17 | BRA 1, EXIT 2, I2FP.F32.U32.RZ 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0603 | casting intrinsic | `float __uint_as_float(a)` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0604 | casting intrinsic | `double __ull2double_rd(a)` | 16 | BRA 1, EXIT 2, I2F.F64.U64.RM 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0605 | casting intrinsic | `double __ull2double_rn(a)` | 16 | BRA 1, EXIT 2, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0606 | casting intrinsic | `double __ull2double_ru(a)` | 16 | BRA 1, EXIT 2, I2F.F64.U64.RP 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0607 | casting intrinsic | `double __ull2double_rz(a)` | 16 | BRA 1, EXIT 2, I2F.F64.U64.RZ 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0608 | casting intrinsic | `float __ull2float_rd(a)` | 17 | BRA 1, EXIT 2, I2F.U64.RM 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0609 | casting intrinsic | `float __ull2float_rn(a)` | 17 | BRA 1, EXIT 2, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0610 | casting intrinsic | `float __ull2float_ru(a)` | 17 | BRA 1, EXIT 2, I2F.U64.RP 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0611 | casting intrinsic | `float __ull2float_rz(a)` | 17 | BRA 1, EXIT 2, I2F.U64.RZ 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0612 | single intrinsic | `float __cosf(a)` | 19 | BRA 1, EXIT 2, FMUL.RZ 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, MUFU.COS 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0613 | single intrinsic | `float __exp10f(a)` | 23 | BRA 1, EXIT 2, FMUL 3, FSETP.GEU.AND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 4, MUFU.EX2 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0614 | single intrinsic | `float __expf(a)` | 23 | BRA 1, EXIT 2, FMUL 3, FSETP.GEU.AND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 4, MUFU.EX2 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0615 | single intrinsic | `float __fadd_rd(a, b)` | 20 | BRA 1, EXIT 2, FADD.RM 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0616 | single intrinsic | `float __fadd_rn(a, b)` | 20 | BRA 1, EXIT 2, FADD 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0617 | single intrinsic | `float __fadd_ru(a, b)` | 20 | BRA 1, EXIT 2, FADD.RP 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0618 | single intrinsic | `float __fadd_rz(a, b)` | 20 | BRA 1, EXIT 2, FADD.RZ 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0619 | single intrinsic | `float __fdiv_rd(a, b)` | 140 | BRA 14, BREAK 5, BSSY 4, BSYNC 4, CALL.REL.NOINC 1, EXIT 2, FADD.FTZ 2, FFMA 6, FFMA.RM 1, FFMA.RP 1, FFMA.RZ 1, FSETP.GTU.FTZ.AND 2, FSETP.NEU.FTZ.AND 3, I2F.U64 2, IADD3 8, IMAD 4, IMAD.IADD 2, IMAD.MOV 3, IMAD.MOV.U32 12, IMAD.SHL.U32 6, IMAD.WIDE.U32 2, ISETP.GE.AND 4, ISETP.GE.U32.AND 1, ISETP.GT.AND 2, ISETP.GT.U32.AND 2, ISETP.GT.U32.OR 1, ISETP.NE.AND 5, ISETP.NE.U32.AND 3, LDG.E.64 2, LEA.HI 2, LOP3.LUT 11, MOV 1, MUFU.RCP 1, MUFU.RSQ 1, PLOP3.LUT 2, RET.REL.NODEC 1, S2R 2, SEL 9, SHF.L.U32 1, SHF.R.U32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0620 | single intrinsic | `float __fdiv_rn(a, b)` | 138 | BRA 18, BSSY 3, BSYNC 3, CALL.REL.NOINC 1, EXIT 2, FADD.FTZ 2, FCHK 1, FFMA 14, FFMA.RM 1, FFMA.RP 1, FFMA.RZ 1, FSETP.GTU.FTZ.AND 2, FSETP.NEU.FTZ.AND 4, I2F.U64 2, IADD3 7, IMAD 3, IMAD.IADD 4, IMAD.MOV 1, IMAD.MOV.U32 12, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 4, ISETP.GE.U32.AND 2, ISETP.GT.AND 1, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 3, LDG.E.64 2, LEA 1, LOP3.LUT 17, MOV 1, MUFU.RCP 2, MUFU.RSQ 1, PLOP3.LUT 4, RET.REL.NODEC 1, S2R 2, SEL 2, SHF.L.U32 1, SHF.R.U32.HI 5, STG.E.64 1, ULDC.64 1 |
| 0621 | single intrinsic | `float __fdiv_ru(a, b)` | 140 | BRA 14, BREAK 5, BSSY 4, BSYNC 4, CALL.REL.NOINC 1, EXIT 2, FADD.FTZ 2, FFMA 6, FFMA.RM 1, FFMA.RP 1, FFMA.RZ 1, FSETP.GTU.FTZ.AND 2, FSETP.NEU.FTZ.AND 3, I2F.U64 2, IADD3 8, IMAD 4, IMAD.IADD 2, IMAD.MOV 3, IMAD.MOV.U32 12, IMAD.SHL.U32 6, IMAD.WIDE.U32 2, ISETP.EQ.AND 2, ISETP.GE.AND 4, ISETP.GE.U32.AND 1, ISETP.GT.AND 2, ISETP.GT.U32.AND 2, ISETP.GT.U32.OR 1, ISETP.NE.AND 3, ISETP.NE.U32.AND 3, LDG.E.64 2, LEA.HI 2, LOP3.LUT 11, MOV 1, MUFU.RCP 1, MUFU.RSQ 1, PLOP3.LUT 2, RET.REL.NODEC 1, S2R 2, SEL 9, SHF.L.U32 1, SHF.R.U32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0622 | single intrinsic | `float __fdiv_rz(a, b)` | 116 | BRA 13, BREAK 5, BSSY 4, BSYNC 4, CALL.REL.NOINC 1, EXIT 2, FADD.FTZ 2, FFMA 6, FFMA.RZ 1, FSETP.GTU.FTZ.AND 2, FSETP.NEU.FTZ.AND 2, I2F.U64 2, IADD3 6, IMAD 4, IMAD.IADD 2, IMAD.MOV.U32 12, IMAD.SHL.U32 6, IMAD.WIDE.U32 2, ISETP.GE.AND 3, ISETP.GE.U32.AND 1, ISETP.GT.AND 2, ISETP.GT.U32.AND 2, ISETP.GT.U32.OR 1, ISETP.NE.U32.AND 2, LDG.E.64 2, LEA.HI 2, LOP3.LUT 9, MOV 1, MUFU.RCP 1, MUFU.RSQ 1, PLOP3.LUT 2, RET.REL.NODEC 1, S2R 2, SEL 4, SHF.R.U32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0623 | single intrinsic | `float __fdividef(a, b)` | 24 | BRA 1, EXIT 2, FMUL 3, FSETP.GEU.AND 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, MUFU.RCP 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0624 | single intrinsic | `float __fmaf_rd(a, b, c)` | 22 | BRA 1, EXIT 2, FFMA.RM 1, I2F.U64 3, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 3, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0625 | single intrinsic | `float __fmaf_rn(a, b, c)` | 22 | BRA 1, EXIT 2, FFMA 1, I2F.U64 3, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 3, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0626 | single intrinsic | `float __fmaf_ru(a, b, c)` | 22 | BRA 1, EXIT 2, FFMA.RP 1, I2F.U64 3, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 3, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0627 | single intrinsic | `float __fmaf_rz(a, b, c)` | 22 | BRA 1, EXIT 2, FFMA.RZ 1, I2F.U64 3, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 3, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0628 | single intrinsic | `float __fmul_rd(a, b)` | 20 | BRA 1, EXIT 2, FMUL.RM 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0629 | single intrinsic | `float __fmul_rn(a, b)` | 20 | BRA 1, EXIT 2, FMUL 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0630 | single intrinsic | `float __fmul_ru(a, b)` | 20 | BRA 1, EXIT 2, FMUL.RP 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0631 | single intrinsic | `float __fmul_rz(a, b)` | 20 | BRA 1, EXIT 2, FMUL.RZ 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0632 | single intrinsic | `float __frcp_rd(a)` | 92 | BRA 9, BSSY 2, BSYNC 2, CALL.REL.NOINC 1, EXIT 2, FADD.FTZ 3, FFMA 4, FFMA.RM 4, FFMA.RP 1, FSETP.NEU.FTZ.AND 1, I2F.U64 1, IADD3 4, IMAD 1, IMAD.MOV 2, IMAD.MOV.U32 10, IMAD.SHL.U32 4, IMAD.WIDE.U32 2, ISETP.EQ.U32.AND 1, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, ISETP.GT.U32.AND 2, ISETP.NE.AND 1, ISETP.NE.U32.AND 2, LDG.E.64 1, LOP3.LUT 13, MOV 1, MUFU.RCP 4, RET.REL.NODEC 1, S2R 2, SEL 2, SHF.L.U32 1, SHF.R.U32.HI 3, STG.E.64 1, ULDC.64 1 |
| 0633 | single intrinsic | `float __frcp_rn(a)` | 85 | BRA 7, BSSY 2, BSYNC 2, CALL.REL.NOINC 1, EXIT 2, FADD.FTZ 3, FFMA 7, FFMA.RM 1, FFMA.RP 1, FSETP.NEU.FTZ.AND 1, I2F.U64 1, IADD3 4, IMAD 1, IMAD.MOV 2, IMAD.MOV.U32 8, IMAD.SHL.U32 4, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 2, ISETP.NE.AND 1, ISETP.NE.U32.AND 1, LDG.E.64 1, LOP3.LUT 11, MOV 1, MUFU.RCP 5, PLOP3.LUT 1, RET.REL.NODEC 1, S2R 2, SEL 2, SHF.L.U32 1, SHF.R.U32.HI 3, STG.E.64 1, ULDC.64 1 |
| 0634 | single intrinsic | `float __frcp_ru(a)` | 92 | BRA 9, BSSY 2, BSYNC 2, CALL.REL.NOINC 1, EXIT 2, FADD.FTZ 3, FFMA 4, FFMA.RM 1, FFMA.RP 4, FSETP.NEU.FTZ.AND 1, I2F.U64 1, IADD3 4, IMAD 1, IMAD.MOV 2, IMAD.MOV.U32 9, IMAD.SHL.U32 4, IMAD.WIDE.U32 2, ISETP.EQ.U32.AND 1, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, ISETP.GT.U32.AND 2, ISETP.NE.AND 1, ISETP.NE.U32.AND 2, LDG.E.64 1, LOP3.LUT 14, MOV 1, MUFU.RCP 4, RET.REL.NODEC 1, S2R 2, SEL 2, SHF.L.U32 1, SHF.R.U32.HI 3, STG.E.64 1, ULDC.64 1 |
| 0635 | single intrinsic | `float __frcp_rz(a)` | 74 | BRA 8, BSSY 2, BSYNC 2, CALL.REL.NOINC 1, EXIT 2, FADD.FTZ 3, FFMA 3, FFMA.RZ 5, I2F.U64 1, IADD3 3, IMAD 1, IMAD.MOV.U32 5, IMAD.SHL.U32 3, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, ISETP.GT.U32.AND 2, ISETP.NE.AND 1, ISETP.NE.U32.AND 1, LDG.E.64 1, LOP3.LUT 9, MOV 4, MUFU.RCP 5, RET.REL.NODEC 1, S2R 2, SHF.L.U32 1, SHF.R.U32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0636 | single intrinsic | `float __frsqrt_rn(a)` | 40 | BRA 2, BSSY 1, BSYNC 1, EXIT 2, FFMA 5, FMUL 3, FSETP.GEU.AND 1, I2F.U64 1, IADD3 2, IMAD 1, IMAD.IADD 1, IMAD.MOV.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, LDG.E.64 1, LEA.HI.SX32 1, LOP3.LUT 2, MOV 4, MUFU.RSQ 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0637 | single intrinsic | `float __fsqrt_rd(a)` | 65 | BRA 7, BSSY 2, BSYNC 2, CALL.REL.NOINC 1, EXIT 2, FADD.FTZ 5, FFMA 7, FFMA.RM 2, FMUL.FTZ 5, FSETP.GEU.FTZ.AND 1, FSETP.GTU.FTZ.AND 1, FSETP.NEU.FTZ.AND 1, I2F.U64 1, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 12, MUFU.RSQ 2, RET.REL.NODEC 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0638 | single intrinsic | `float __fsqrt_rn(a)` | 55 | BRA 6, BSSY 1, BSYNC 1, CALL.REL.NOINC 1, EXIT 2, FADD.FTZ 2, FFMA 5, FMUL.FTZ 5, FSETP.GEU.FTZ.AND 1, FSETP.GTU.FTZ.AND 1, FSETP.NEU.FTZ.AND 1, I2F.U64 1, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 10, MUFU.RSQ 2, RET.REL.NODEC 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0639 | single intrinsic | `float __fsqrt_ru(a)` | 58 | BRA 6, BSSY 2, BSYNC 2, CALL.REL.NOINC 1, EXIT 2, FADD.FTZ 3, FFMA 3, FFMA.RP 2, FMUL.FTZ 5, FSETP.GEU.FTZ.AND 1, FSETP.GTU.FTZ.AND 1, FSETP.NEU.FTZ.AND 1, I2F.U64 1, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 10, MUFU.RSQ 2, RET.REL.NODEC 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0640 | single intrinsic | `float __fsqrt_rz(a)` | 65 | BRA 7, BSSY 2, BSYNC 2, CALL.REL.NOINC 1, EXIT 2, FADD.FTZ 5, FFMA 7, FFMA.RZ 2, FMUL.FTZ 5, FSETP.GEU.FTZ.AND 1, FSETP.GTU.FTZ.AND 1, FSETP.NEU.FTZ.AND 1, I2F.U64 1, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 12, MUFU.RSQ 2, RET.REL.NODEC 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0641 | single intrinsic | `float __fsub_rd(a, b)` | 20 | BRA 1, EXIT 2, FADD.RM 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0642 | single intrinsic | `float __fsub_rn(a, b)` | 20 | BRA 1, EXIT 2, FADD 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0643 | single intrinsic | `float __fsub_ru(a, b)` | 20 | BRA 1, EXIT 2, FADD.RP 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0644 | single intrinsic | `float __fsub_rz(a, b)` | 20 | BRA 1, EXIT 2, FADD.RZ 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0645 | single intrinsic | `float __log10f(a)` | 22 | BRA 1, EXIT 2, FADD 1, FMUL 2, FSETP.GEU.AND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, MUFU.LG2 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0646 | single intrinsic | `float __log2f(a)` | 22 | BRA 1, EXIT 2, FADD 1, FMUL 1, FSETP.GEU.AND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 4, MUFU.LG2 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0647 | single intrinsic | `float __logf(a)` | 22 | BRA 1, EXIT 2, FADD 1, FMUL 2, FSETP.GEU.AND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, MUFU.LG2 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0648 | single intrinsic | `float __powf(a, b)` | 28 | BRA 1, EXIT 2, FADD 1, FMUL 4, FSETP.GEU.AND 2, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, MUFU.EX2 1, MUFU.LG2 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0649 | single intrinsic | `float __saturatef(a)` | 18 | BRA 1, EXIT 2, FADD.SAT 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0650 | single intrinsic | `void __sincosf(a, o1, o2)` | 19 | BRA 1, EXIT 2, FMUL.RZ 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, MUFU.SIN 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0651 | single intrinsic | `float __sinf(a)` | 19 | BRA 1, EXIT 2, FMUL.RZ 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, MUFU.SIN 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0652 | single intrinsic | `float __tanf(a)` | 25 | BRA 1, EXIT 2, FMUL 3, FMUL.RZ 1, FSETP.GEU.AND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, MUFU.COS 1, MUFU.RCP 1, MUFU.SIN 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0653 | double intrinsic | `double __dadd_rd(a, b)` | 19 | BRA 1, DADD.RM 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0654 | double intrinsic | `double __dadd_rn(a, b)` | 19 | BRA 1, DADD 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0655 | double intrinsic | `double __dadd_ru(a, b)` | 19 | BRA 1, DADD.RP 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0656 | double intrinsic | `double __dadd_rz(a, b)` | 19 | BRA 1, DADD.RZ 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0657 | double intrinsic | `double __ddiv_rd(a, b)` | 91 | BRA 8, BSSY 1, BSYNC 1, CALL.REL.NOINC 1, DFMA 8, DFMA.RM 2, DMUL 2, DMUL.RM 1, DSETP.NAN.AND 2, EXIT 2, FSETP.GEU.AND 2, I2F.F64.U64 2, IADD3 4, IMAD 1, IMAD.IADD 1, IMAD.MOV.U32 19, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMNMX 2, ISETP.EQ.OR 1, ISETP.GE.U32.AND 2, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 2, LDG.E.64 2, LOP3.LUT 12, MOV 1, MUFU.RCP64H 1, RET.REL.NODEC 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0658 | double intrinsic | `double __ddiv_rn(a, b)` | 130 | BRA 11, BSSY 2, BSYNC 2, CALL.REL.NOINC 1, DFMA 17, DMUL 4, DMUL.RP 1, DSETP.NAN.AND 2, EXIT 2, FFMA 1, FSEL 2, FSETP.GEU.AND 3, FSETP.GT.AND 1, FSETP.GTU.AND 1, FSETP.NEU.AND 2, I2F.F64.U64 2, IADD3 4, IMAD 1, IMAD.IADD 2, IMAD.MOV 1, IMAD.MOV.U32 23, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMNMX 2, ISETP.EQ.OR 1, ISETP.GE.U32.AND 4, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 2, LDG.E.64 2, LOP3.LUT 18, MOV 1, MUFU.RCP64H 2, RET.REL.NODEC 1, S2R 2, SEL 3, STG.E.64 1, ULDC.64 1 |
| 0659 | double intrinsic | `double __ddiv_ru(a, b)` | 91 | BRA 8, BSSY 1, BSYNC 1, CALL.REL.NOINC 1, DFMA 8, DFMA.RP 2, DMUL 2, DMUL.RP 1, DSETP.NAN.AND 2, EXIT 2, FSETP.GEU.AND 2, I2F.F64.U64 2, IADD3 4, IMAD 1, IMAD.IADD 1, IMAD.MOV.U32 19, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMNMX 2, ISETP.EQ.OR 1, ISETP.GE.U32.AND 2, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 2, LDG.E.64 2, LOP3.LUT 12, MOV 1, MUFU.RCP64H 1, RET.REL.NODEC 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0660 | double intrinsic | `double __ddiv_rz(a, b)` | 91 | BRA 8, BSSY 1, BSYNC 1, CALL.REL.NOINC 1, DFMA 8, DFMA.RZ 2, DMUL 2, DMUL.RZ 1, DSETP.NAN.AND 2, EXIT 2, FSETP.GEU.AND 2, I2F.F64.U64 2, IADD3 4, IMAD 1, IMAD.IADD 1, IMAD.MOV.U32 19, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMNMX 2, ISETP.EQ.OR 1, ISETP.GE.U32.AND 2, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 2, LDG.E.64 2, LOP3.LUT 12, MOV 1, MUFU.RCP64H 1, RET.REL.NODEC 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0661 | double intrinsic | `double __dmul_rd(a, b)` | 19 | BRA 1, DMUL.RM 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0662 | double intrinsic | `double __dmul_rn(a, b)` | 19 | BRA 1, DMUL 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0663 | double intrinsic | `double __dmul_ru(a, b)` | 19 | BRA 1, DMUL.RP 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0664 | double intrinsic | `double __dmul_rz(a, b)` | 19 | BRA 1, DMUL.RZ 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0665 | double intrinsic | `double __drcp_rd(a)` | 120 | BRA 9, BREAK 3, BSSY 3, BSYNC 3, CALL.REL.NOINC 1, DFMA 7, DFMA.RM 1, DFMA.RP 1, DFMA.RZ 1, DMUL 2, DSETP.GTU.AND 1, DSETP.NE.OR 1, DSETP.NEU.AND 2, EXIT 2, I2F.F64.U64 1, IADD3 8, IMAD 3, IMAD.MOV 1, IMAD.MOV.U32 25, IMAD.SHL.U32 3, IMAD.WIDE.U32 2, IMAD.X 1, ISETP.GE.AND 3, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, ISETP.GT.U32.AND 2, ISETP.NE.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, LOP3.LUT 9, MOV 1, MUFU.RCP64H 1, RET.REL.NODEC 1, S2R 2, SEL 6, SHF.L.U32 1, SHF.L.U64.HI 1, SHF.R.S32.HI 1, SHF.R.S64 1, SHF.R.U32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0666 | double intrinsic | `double __drcp_rn(a)` | 73 | BRA 7, BSSY 2, BSYNC 2, CALL.REL.NOINC 1, DFMA 18, DMUL 3, DSETP.GTU.AND 1, EXIT 2, FSETP.GEU.AND 1, I2F.F64.U64 1, IADD3 4, IMAD 1, IMAD.MOV.U32 8, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 3, LDG.E.64 1, LOP3.LUT 4, MOV 3, MUFU.RCP64H 3, RET.REL.NODEC 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0667 | double intrinsic | `double __drcp_ru(a)` | 121 | BRA 9, BREAK 3, BSSY 3, BSYNC 3, CALL.REL.NOINC 1, DFMA 7, DFMA.RM 1, DFMA.RP 1, DFMA.RZ 1, DMUL 2, DSETP.GTU.AND 1, DSETP.NE.OR 1, DSETP.NEU.AND 2, EXIT 2, I2F.F64.U64 1, IADD3 8, IMAD 3, IMAD.MOV 1, IMAD.MOV.U32 25, IMAD.SHL.U32 3, IMAD.WIDE.U32 2, IMAD.X 1, ISETP.GE.AND 3, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, ISETP.GT.U32.AND 2, ISETP.NE.AND 2, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, LOP3.LUT 9, MOV 1, MUFU.RCP64H 1, RET.REL.NODEC 1, S2R 2, SEL 6, SHF.L.U32 1, SHF.L.U64.HI 1, SHF.R.S32.HI 1, SHF.R.S64 1, SHF.R.U32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0668 | double intrinsic | `double __drcp_rz(a)` | 99 | BRA 8, BREAK 3, BSSY 3, BSYNC 3, CALL.REL.NOINC 1, DFMA 7, DFMA.RZ 1, DMUL 2, DSETP.GTU.AND 1, DSETP.NEU.AND 2, EXIT 2, I2F.F64.U64 1, IADD3 6, IMAD 3, IMAD.MOV.U32 21, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, ISETP.GE.AND 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, ISETP.GT.U32.AND 2, LDG.E.64 1, LOP3.LUT 7, MOV 6, MUFU.RCP64H 1, RET.REL.NODEC 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, SHF.R.S64 1, SHF.R.U32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0669 | double intrinsic | `double __dsqrt_rd(a)` | 85 | BRA 6, BREAK 3, BSSY 2, BSYNC 2, CALL.REL.NOINC 1, DFMA 7, DFMA.RM 1, DMUL 4, DSETP.GEU.AND 1, DSETP.GTU.AND 1, DSETP.NEU.AND 2, EXIT 2, F2F.F32.F64 1, F2F.F64.F32 1, I2F.F64.U64 1, IADD3 4, IMAD 2, IMAD.MOV.U32 14, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, ISETP.NE.OR 1, LDG.E.64 1, LEA 1, LEA.HI 1, LOP3.LUT 2, MOV 10, MUFU.RSQ 1, RET.REL.NODEC 1, S2R 2, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1, WARPSYNC 1 |
| 0670 | double intrinsic | `double __dsqrt_rn(a)` | 79 | BRA 8, BSSY 2, BSYNC 2, CALL.REL.NOINC 1, DADD 1, DFMA 10, DFMA.RM 1, DFMA.RP 1, DMUL 7, DSETP.GT.AND 1, DSETP.NE.AND 1, EXIT 2, FSEL 2, I2F.F64.U64 1, IADD3 5, IMAD 1, IMAD.MOV.U32 10, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMAD.X 1, ISETP.GE.AND 1, ISETP.GE.U32.AND 3, ISETP.GT.AND 1, LDG.E.64 1, MOV 6, MUFU.RSQ64H 2, RET.REL.NODEC 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0671 | double intrinsic | `double __dsqrt_ru(a)` | 85 | BRA 6, BREAK 3, BSSY 2, BSYNC 2, CALL.REL.NOINC 1, DFMA 7, DFMA.RP 1, DMUL 4, DSETP.GEU.AND 1, DSETP.GTU.AND 1, DSETP.NEU.AND 2, EXIT 2, F2F.F32.F64 1, F2F.F64.F32 1, I2F.F64.U64 1, IADD3 4, IMAD 2, IMAD.MOV.U32 14, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, ISETP.NE.OR 1, LDG.E.64 1, LEA 1, LEA.HI 1, LOP3.LUT 2, MOV 10, MUFU.RSQ 1, RET.REL.NODEC 1, S2R 2, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1, WARPSYNC 1 |
| 0672 | double intrinsic | `double __dsqrt_rz(a)` | 85 | BRA 6, BREAK 3, BSSY 2, BSYNC 2, CALL.REL.NOINC 1, DFMA 7, DFMA.RZ 1, DMUL 4, DSETP.GEU.AND 1, DSETP.GTU.AND 1, DSETP.NEU.AND 2, EXIT 2, F2F.F32.F64 1, F2F.F64.F32 1, I2F.F64.U64 1, IADD3 4, IMAD 2, IMAD.MOV.U32 14, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, ISETP.NE.OR 1, LDG.E.64 1, LEA 1, LEA.HI 1, LOP3.LUT 2, MOV 10, MUFU.RSQ 1, RET.REL.NODEC 1, S2R 2, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1, WARPSYNC 1 |
| 0673 | double intrinsic | `double __dsub_rd(a, b)` | 19 | BRA 1, DADD.RM 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0674 | double intrinsic | `double __dsub_rn(a, b)` | 19 | BRA 1, DADD 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0675 | double intrinsic | `double __dsub_ru(a, b)` | 19 | BRA 1, DADD.RP 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0676 | double intrinsic | `double __dsub_rz(a, b)` | 19 | BRA 1, DADD.RZ 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0677 | double intrinsic | `double __fma_rd(a, b, c)` | 21 | BRA 1, DFMA.RM 1, EXIT 2, I2F.F64.U64 3, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 3, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0678 | double intrinsic | `double __fma_rn(a, b, c)` | 21 | BRA 1, DFMA 1, EXIT 2, I2F.F64.U64 3, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 3, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0679 | double intrinsic | `double __fma_ru(a, b, c)` | 21 | BRA 1, DFMA.RP 1, EXIT 2, I2F.F64.U64 3, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 3, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0680 | double intrinsic | `double __fma_rz(a, b, c)` | 21 | BRA 1, DFMA.RZ 1, EXIT 2, I2F.F64.U64 3, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 3, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0681 | single math | `float acosf(a)` | 43 | BRA 1, EXIT 2, FADD 1, FFMA 10, FMUL 4, FSEL 3, FSETP.GT.AND 2, FSETP.NEU.AND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 6, MUFU.RSQ 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0682 | single math | `float acoshf(a)` | 61 | BRA 2, BSSY 1, BSYNC 1, EXIT 2, FADD 4, FADD.RZ 1, FFMA 15, FMUL 4, FSEL 3, FSETP.NEU.AND 2, I2F.U64 1, I2FP.F32.S32 1, IADD3 3, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, ISETP.GT.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 6, MUFU.RSQ 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0683 | single math | `float asinf(a)` | 42 | BRA 1, EXIT 2, FFMA 9, FMUL 5, FSEL 3, FSETP.GT.AND 1, FSETP.GTU.AND 1, FSETP.NEU.AND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 6, MUFU.RSQ 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0684 | single math | `float asinhf(a)` | 60 | BRA 2, BSSY 1, BSYNC 1, EXIT 2, FADD 2, FADD.RZ 1, FFMA 15, FMUL 3, FSEL 3, FSETP.GT.AND 1, FSETP.GTU.AND 1, FSETP.NEU.AND 1, I2F.U64 1, I2FP.F32.S32 1, IADD3 3, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, LDG.E.64 1, LOP3.LUT 2, MOV 6, MUFU.RCP 1, MUFU.RSQ 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0685 | single math | `float atanf(a)` | 36 | BRA 1, EXIT 2, FFMA 9, FMUL 2, FSEL 2, FSETP.GT.AND 1, FSETP.GTU.AND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 5, MUFU.RCP 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0686 | single math | `float atanhf(a)` | 57 | BRA 2, BSSY 1, BSYNC 1, EXIT 2, FADD 3, FADD.RZ 1, FFMA 12, FMUL 4, FSEL 2, FSETP.GT.AND 1, FSETP.NEU.AND 1, I2F.U64 1, I2FP.F32.S32 1, IADD3 3, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, LDG.E.64 1, LOP3.LUT 2, MOV 7, MUFU.RCP 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0687 | single math | `float cbrtf(a)` | 34 | BRA 1, EXIT 2, FADD 2, FFMA 2, FMUL 5, FSEL 2, FSETP.GEU.AND 2, FSETP.NEU.AND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 4, MUFU.EX2 1, MUFU.LG2 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0688 | single math | `float ceilf(a)` | 18 | BRA 1, EXIT 2, FRND.CEIL 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0689 | single math | `float cosf(a)` | 153 | BRA 6, BSSY 2, BSYNC 2, DMUL 1, EXIT 2, F2F.F32.F64 1, F2I.NTZ 1, FFMA 9, FMUL 3, FSEL 4, FSETP.GE.AND 1, FSETP.NEU.AND 1, I2F.F64.S64 1, I2F.U64 1, I2FP.F32.S32 1, IADD3 8, IADD3.X 1, IMAD 1, IMAD.IADD 2, IMAD.MOV 1, IMAD.MOV.U32 36, IMAD.SHL.U32 5, IMAD.U32 2, IMAD.WIDE.U32 3, ISETP.EQ.AND 21, ISETP.GE.U32.AND 1, ISETP.NE.AND 3, LDG.E.64 1, LDG.E.CONSTANT 1, LEA.HI 1, LOP3.LUT 9, MOV 5, S2R 2, SHF.L.U32 2, SHF.L.U32.HI 1, SHF.R.U32.HI 5, STG.E.64 1, UIADD3 1, UIADD3.X 1, ULDC.64 2, UMOV 1 |
| 0690 | single math | `float coshf(a)` | 33 | BRA 1, EXIT 2, FADD 1, FFMA 3, FMUL 3, FMUL.FTZ 1, FRND.TRUNC 1, FSEL 1, FSETP.GT.AND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 4, MUFU.EX2 1, MUFU.RCP 1, S2R 2, SHF.L.U32 2, STG.E.64 1, ULDC.64 1 |
| 0691 | single math | `float cospif(a)` | 40 | BRA 1, EXIT 2, F2I.NTZ 1, FADD 2, FFMA 10, FMUL 2, FRND 1, FSETP.GT.AND 1, I2F.U64 1, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.U32.AND 1, LDG.E.64 1, LOP3.LUT 2, MOV 5, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0692 | single math | `float cyl_bessel_i0f(a)` | 74 | BRA 3, BSSY 1, BSYNC 1, EXIT 2, FADD 5, FFMA 25, FMUL 5, FSEL 2, FSETP.GE.AND 2, FSETP.GEU.AND 1, FSETP.GT.AND 1, FSETP.NEU.AND 1, I2F.U64 1, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 9, MUFU.RCP 1, MUFU.RSQ 1, S2R 2, SHF.L.U32 2, STG.E.64 1, ULDC.64 1 |
| 0693 | single math | `float cyl_bessel_i1f(a)` | 76 | BRA 3, BSSY 1, BSYNC 1, EXIT 2, FADD 5, FFMA 24, FMUL 7, FSEL 2, FSETP.GE.AND 2, FSETP.GEU.AND 1, FSETP.GT.AND 1, FSETP.NEU.AND 1, I2F.U64 1, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 2, MOV 9, MUFU.RCP 1, MUFU.RSQ 1, S2R 2, SHF.L.U32 2, STG.E.64 1, ULDC.64 1 |
| 0694 | single math | `float erfcf(a)` | 65 | BRA 1, EXIT 2, FADD 6, FFMA 20, FMUL 8, FRND.TRUNC 1, FSEL 2, FSETP.GEU.AND 1, FSETP.GT.AND 2, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 6, MUFU.EX2 1, MUFU.RCP 2, S2R 2, SHF.L.U32 2, STG.E.64 1, ULDC.64 1 |
| 0695 | single math | `float erfcinvf(a)` | 58 | BRA 3, BSSY 1, BSYNC 1, EXIT 2, FADD 2, FFMA 18, FMUL 4, FSEL 3, FSETP.GE.AND 1, FSETP.GEU.AND 1, FSETP.GT.AND 1, FSETP.GTU.AND 1, FSETP.NEU.AND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 5, MUFU.LG2 2, MUFU.RSQ 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0696 | single math | `float erfcxf(a)` | 86 | BRA 4, BSSY 2, BSYNC 2, EXIT 2, FADD 6, FFMA 25, FFMA.RM 1, FFMA.SAT 1, FMUL 10, FMUL.RZ 1, FSEL 1, FSETP.GEU.AND 3, FSETP.NEU.AND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 11, MUFU.EX2 1, MUFU.RCP 3, S2R 2, SHF.L.U32 2, STG.E.64 1, ULDC.64 1 |
| 0697 | single math | `float erff(a)` | 44 | BRA 1, EXIT 2, FADD 1, FFMA 7, FMUL 1, FSEL 8, FSETP.GE.AND 1, I2F.U64 1, IMAD 1, IMAD.MOV.U32 5, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 5, MUFU.EX2 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0698 | single math | `float erfinvf(a)` | 46 | BRA 2, BSSY 1, BSYNC 1, EXIT 2, FFMA 15, FMUL 3, FSEL 1, FSETP.GEU.AND 1, FSETP.NEU.AND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 5, MUFU.LG2 1, MUFU.RSQ 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0699 | single math | `float exp10f(a)` | 27 | BRA 1, EXIT 2, FADD 1, FFMA 2, FFMA.RM 1, FFMA.SAT 1, FMUL 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 5, MUFU.EX2 1, S2R 2, SHF.L.U32 2, STG.E.64 1, ULDC.64 1 |
| 0700 | single math | `float exp2f(a)` | 22 | BRA 1, EXIT 2, FMUL 2, FSETP.GEU.AND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 4, MUFU.EX2 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0701 | single math | `float expf(a)` | 27 | BRA 1, EXIT 2, FADD 1, FFMA 2, FFMA.RM 1, FFMA.SAT 1, FMUL 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 5, MUFU.EX2 1, S2R 2, SHF.L.U32 2, STG.E.64 1, ULDC.64 1 |
| 0702 | single math | `float expm1f(a)` | 41 | BRA 1, EXIT 2, FADD 3, FFMA 10, FSEL 2, FSETP.GE.AND 1, FSETP.GT.AND 1, FSETP.NEU.AND 1, I2F.U64 1, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 6, S2R 2, SHF.L.U32 2, STG.E.64 1, ULDC.64 1 |
| 0703 | single math | `float fabsf(a)` | 18 | BRA 1, EXIT 2, FADD 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0704 | single math | `float floorf(a)` | 18 | BRA 1, EXIT 2, FRND.FLOOR 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0705 | single math | `float j0f(a)` | 215 | BRA 9, BSSY 3, BSYNC 3, DMUL 1, EXIT 2, F2F.F32.F64 1, F2I.NTZ 2, FADD 7, FFMA 33, FMUL 13, FSEL 5, FSETP.GE.AND 1, FSETP.GEU.AND 1, FSETP.GTU.AND 1, FSETP.NEU.AND 2, I2F.F64.S64 1, I2F.U64 1, I2FP.F32.S32 2, I2FP.F32.U32 1, IADD3 9, IADD3.X 1, IMAD 1, IMAD.IADD 1, IMAD.MOV 1, IMAD.MOV.U32 24, IMAD.SHL.U32 5, IMAD.U32 1, IMAD.WIDE.U32 3, ISETP.EQ.AND 21, ISETP.GE.U32.AND 1, ISETP.NE.AND 3, LDG.E.64 1, LDG.E.CONSTANT 1, LEA.HI 1, LOP3.LUT 10, MOV 24, MUFU.RCP 1, MUFU.RSQ 1, S2R 2, SHF.L.U32 2, SHF.L.U32.HI 1, SHF.R.U32.HI 5, STG.E.64 1, UIADD3 1, UIADD3.X 1, ULDC.64 2, UMOV 1 |
| 0706 | single math | `float j1f(a)` | 215 | BRA 9, BSSY 3, BSYNC 3, DMUL 1, EXIT 2, F2F.F32.F64 1, F2I.NTZ 2, FADD 5, FFMA 32, FMUL 13, FSEL 7, FSETP.GE.AND 1, FSETP.GEU.AND 3, FSETP.GTU.AND 1, FSETP.NEU.AND 2, I2F.F64.S64 1, I2F.U64 1, I2FP.F32.S32 2, I2FP.F32.U32 1, IADD3 8, IADD3.X 1, IMAD 1, IMAD.IADD 2, IMAD.MOV 1, IMAD.MOV.U32 27, IMAD.SHL.U32 4, IMAD.U32 1, IMAD.WIDE.U32 3, ISETP.EQ.AND 21, ISETP.GE.U32.AND 1, ISETP.NE.AND 3, LDG.E.64 1, LDG.E.CONSTANT 1, LEA.HI 1, LOP3.LUT 11, MOV 19, MUFU.RCP 1, MUFU.RSQ 1, S2R 2, SHF.L.U32 3, SHF.L.U32.HI 1, SHF.R.U32.HI 5, STG.E.64 1, UIADD3 1, UIADD3.X 1, ULDC.64 2, UMOV 1 |
| 0707 | single math | `float lgammaf(a)` | 232 | BRA 13, BSSY 2, BSYNC 2, EXIT 2, F2I.NTZ 1, FADD 14, FFMA 93, FMUL 17, FRND 1, FRND.FLOOR 1, FSEL 14, FSETP.GE.AND 5, FSETP.GEU.AND 5, FSETP.NEU.AND 6, I2F.U64 1, I2FP.F32.S32 4, IADD3 8, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 5, LDG.E.64 1, LOP3.LUT 4, MOV 22, MUFU.RCP 2, R2P 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0708 | single math | `float log10f(a)` | 44 | BRA 1, EXIT 2, FADD 1, FFMA 12, FMUL 3, FSEL 2, FSETP.GEU.AND 1, FSETP.NEU.AND 1, I2F.U64 1, I2FP.F32.S32 1, IADD3 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, LDG.E.64 1, LOP3.LUT 1, MOV 5, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0709 | single math | `float log1pf(a)` | 48 | BRA 2, BSSY 1, BSYNC 1, EXIT 2, FADD 1, FADD.RZ 1, FFMA 12, FMUL 2, FSEL 1, FSETP.NEU.AND 1, I2F.U64 1, I2FP.F32.S32 1, IADD3 3, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, LDG.E.64 1, LOP3.LUT 1, MOV 6, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0710 | single math | `float log2f(a)` | 45 | BRA 1, EXIT 2, FADD 2, FFMA 12, FMUL 3, FSEL 2, FSETP.GEU.AND 1, FSETP.NEU.AND 1, I2F.U64 1, I2FP.F32.S32 1, IADD3 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, LDG.E.64 1, LOP3.LUT 1, MOV 5, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0711 | single math | `float logbf(a)` | 33 | BRA 2, BSSY 1, BSYNC 1, EXIT 2, FADD 1, FLO.U32 1, FMUL 1, FSEL 1, FSETP.NEU.AND 1, I2F.U64 1, I2FP.F32.S32 2, IADD3 2, IMAD 1, IMAD.MOV.U32 4, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, ISETP.GT.U32.AND 1, LDG.E.64 1, LEA.HI 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0712 | single math | `float logf(a)` | 43 | BRA 1, EXIT 2, FADD 1, FFMA 12, FMUL 2, FSEL 2, FSETP.GEU.AND 1, FSETP.NEU.AND 1, I2F.U64 1, I2FP.F32.S32 1, IADD3 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, LDG.E.64 1, LOP3.LUT 1, MOV 5, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0713 | single math | `float nearbyintf(a)` | 18 | BRA 1, EXIT 2, FRND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0714 | single math | `float normcdff(a)` | 80 | BRA 1, EXIT 2, FADD 9, FFMA 23, FMUL 12, FRND.TRUNC 1, FSEL 3, FSETP.GEU.AND 2, FSETP.GT.AND 3, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 2, MOV 7, MUFU.EX2 1, MUFU.RCP 2, S2R 2, SHF.L.U32 2, STG.E.64 1, ULDC.64 1 |
| 0715 | single math | `float normcdfinvf(a)` | 60 | BRA 3, BSSY 1, BSYNC 1, EXIT 2, FADD 3, FFMA 19, FMUL 4, FSEL 3, FSETP.GE.AND 1, FSETP.GEU.AND 1, FSETP.GT.AND 1, FSETP.GTU.AND 1, FSETP.NEU.AND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 5, MUFU.LG2 2, MUFU.RSQ 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0716 | single math | `float rcbrtf(a)` | 34 | BRA 1, EXIT 2, FADD 2, FFMA 2, FMUL 5, FSEL 2, FSETP.GEU.AND 2, FSETP.NEU.AND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 3, MUFU.EX2 1, MUFU.LG2 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0717 | single math | `float rintf(a)` | 18 | BRA 1, EXIT 2, FRND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0718 | single math | `float roundf(a)` | 21 | BRA 1, EXIT 2, FADD.RZ 1, FRND.TRUNC 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 4, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0719 | single math | `float rsqrtf(a)` | 22 | BRA 1, EXIT 2, FMUL 2, FSETP.GEU.AND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 4, MUFU.RSQ 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0720 | single math | `float sinf(a)` | 152 | BRA 6, BSSY 2, BSYNC 2, DMUL 1, EXIT 2, F2F.F32.F64 1, F2I.NTZ 1, FFMA 9, FMUL 3, FSEL 4, FSETP.GE.AND 1, FSETP.NEU.AND 1, I2F.F64.S64 1, I2F.U64 1, I2FP.F32.S32 1, IADD3 7, IADD3.X 1, IMAD 1, IMAD.IADD 2, IMAD.MOV 1, IMAD.MOV.U32 37, IMAD.SHL.U32 4, IMAD.U32 1, IMAD.WIDE.U32 3, ISETP.EQ.AND 21, ISETP.GE.U32.AND 1, ISETP.NE.AND 3, LDG.E.64 1, LDG.E.CONSTANT 1, LEA.HI 1, LOP3.LUT 9, MOV 5, S2R 2, SHF.L.U32 3, SHF.L.U32.HI 1, SHF.R.U32.HI 5, STG.E.64 1, UIADD3 1, UIADD3.X 1, ULDC.64 2, UMOV 1 |
| 0721 | single math | `float sinhf(a)` | 42 | BRA 1, EXIT 2, FADD 1, FFMA 7, FMUL 5, FMUL.FTZ 1, FRND.TRUNC 1, FSEL 1, FSETP.GE.AND 1, FSETP.GT.AND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 2, MOV 5, MUFU.EX2 1, MUFU.RCP 1, S2R 2, SHF.L.U32 2, STG.E.64 1, ULDC.64 1 |
| 0722 | single math | `float sinpif(a)` | 41 | BRA 1, EXIT 2, F2I.NTZ 1, FADD 2, FFMA 10, FMUL 2, FRND 1, FRND.TRUNC 1, FSETP.NEU.AND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.U32.AND 1, LDG.E.64 1, LOP3.LUT 2, MOV 6, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0723 | single math | `float sqrtf(a)` | 55 | BRA 6, BSSY 1, BSYNC 1, CALL.REL.NOINC 1, EXIT 2, FADD.FTZ 2, FFMA 5, FMUL.FTZ 5, FSETP.GEU.FTZ.AND 1, FSETP.GTU.FTZ.AND 1, FSETP.NEU.FTZ.AND 1, I2F.U64 1, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 10, MUFU.RSQ 2, RET.REL.NODEC 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0724 | single math | `float tanf(a)` | 149 | BRA 6, BSSY 2, BSYNC 2, DMUL 1, EXIT 2, F2F.F32.F64 1, F2I.NTZ 1, FFMA 9, FMUL 4, FSEL 1, FSETP.GE.AND 1, FSETP.NEU.AND 2, I2F.F64.S64 1, I2F.U64 1, I2FP.F32.S32 1, IADD3 7, IADD3.X 1, IMAD 1, IMAD.IADD 2, IMAD.MOV 1, IMAD.MOV.U32 34, IMAD.SHL.U32 4, IMAD.U32 1, IMAD.WIDE.U32 3, ISETP.EQ.AND 21, ISETP.GE.U32.AND 1, ISETP.NE.AND 3, ISETP.NE.U32.AND 1, LDG.E.64 1, LDG.E.CONSTANT 1, LEA.HI 1, LOP3.LUT 8, MOV 5, MUFU.RCP 1, S2R 2, SHF.L.U32 3, SHF.L.U32.HI 1, SHF.R.U32.HI 5, STG.E.64 1, UIADD3 1, UIADD3.X 1, ULDC.64 2, UMOV 1 |
| 0725 | single math | `float tanhf(a)` | 34 | BRA 1, EXIT 2, FADD 1, FFMA 6, FMUL 2, FSEL 1, FSETP.GE.AND 2, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 5, MUFU.EX2 1, MUFU.RCP 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0726 | single math | `float tgammaf(a)` | 179 | BRA 6, BSSY 1, BSYNC 1, EXIT 2, F2I.NTZ 2, F2I.TRUNC.NTZ 1, FADD 24, FFMA 50, FMUL 23, FRND 3, FRND.TRUNC 1, FSEL 9, FSETP.GE.AND 1, FSETP.GEU.AND 7, FSETP.GT.AND 3, FSETP.GTU.AND 1, FSETP.NEU.AND 2, I2F.U64 1, I2FP.F32.S32 1, IADD3 3, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.U32.AND 2, LDG.E.64 1, LEA 1, LOP3.LUT 4, MOV 15, MUFU.RCP 4, S2R 2, SEL 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0727 | single math | `float truncf(a)` | 18 | BRA 1, EXIT 2, FRND.TRUNC 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0728 | single math | `float y0f(a)` | 379 | BRA 21, BSSY 4, BSYNC 4, DMUL 2, EXIT 2, F2F.F32.F64 2, F2I.NTZ 4, FADD 15, FFMA 99, FMUL 30, FSEL 14, FSETP.GE.AND 2, FSETP.GEU.AND 3, FSETP.GTU.AND 5, FSETP.NEU.AND 5, I2F.F64.S64 2, I2F.U64 1, I2FP.F32.S32 5, I2FP.F32.U32 2, IADD3 24, IADD3.X 2, IMAD 1, IMAD.IADD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 2, IMAD.WIDE.U32 4, ISETP.GE.U32.AND 2, ISETP.NE.AND 6, LDG.E.64 1, LDG.E.CONSTANT 2, LDL 6, LEA 4, LEA.HI 2, LOP3.LUT 21, MOV 38, MUFU.RCP 2, MUFU.RSQ 2, S2R 2, SHF.L.U32 7, SHF.L.U32.HI 2, SHF.R.U32.HI 10, STG.E.64 1, STL 4, UIADD3 2, UIADD3.X 2, ULDC.64 3, UMOV 2 |
| 0729 | single math | `float y1f(a)` | 507 | BRA 41, BSSY 8, BSYNC 8, CALL.REL.NOINC 2, DMUL 2, EXIT 2, F2F.F32.F64 2, F2I.NTZ 4, FADD 15, FADD.FTZ 2, FCHK 2, FFMA 115, FFMA.RM 1, FFMA.RP 1, FFMA.RZ 1, FMUL 25, FSEL 11, FSETP.GE.AND 2, FSETP.GEU.AND 3, FSETP.GTU.AND 5, FSETP.GTU.FTZ.AND 2, FSETP.NEU.AND 5, FSETP.NEU.FTZ.AND 4, I2F.F64.S64 2, I2F.U64 1, I2FP.F32.S32 5, I2FP.F32.U32 2, IADD3 32, IADD3.X 2, IMAD 5, IMAD.IADD 3, IMAD.MOV 2, IMAD.MOV.U32 20, IMAD.SHL.U32 2, IMAD.U32 2, IMAD.WIDE.U32 4, ISETP.GE.AND 4, ISETP.GE.U32.AND 3, ISETP.GT.AND 1, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 9, LDG.E.64 1, LDG.E.CONSTANT 2, LDL 6, LEA 3, LEA.HI 2, LOP3.LUT 39, MOV 32, MUFU.RCP 5, MUFU.RSQ 3, PLOP3.LUT 4, RET.REL.NODEC 1, S2R 2, SEL 2, SHF.L.U32 8, SHF.L.U32.HI 2, SHF.R.U32.HI 15, STG.E.64 1, STL 4, UIADD3 2, UIADD3.X 2, ULDC.64 3, UMOV 4 |
| 0730 | single math | `float atan2f(a, b)` | 173 | BRA 21, BSSY 4, BSYNC 4, CALL.REL.NOINC 1, EXIT 2, FADD 1, FADD.FTZ 2, FCHK 1, FFMA 25, FFMA.RM 1, FFMA.RP 1, FFMA.RZ 1, FMUL 2, FSEL 5, FSETP.GT.AND 1, FSETP.GTU.AND 1, FSETP.GTU.FTZ.AND 2, FSETP.NEU.AND 2, FSETP.NEU.FTZ.AND 4, I2F.U64 2, IADD3 7, IMAD 3, IMAD.IADD 4, IMAD.MOV 1, IMAD.MOV.U32 15, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 6, ISETP.GE.U32.AND 2, ISETP.GT.AND 2, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 3, LDG.E.64 2, LEA 1, LOP3.LUT 18, MOV 1, MUFU.RCP 2, MUFU.RSQ 1, PLOP3.LUT 4, RET.REL.NODEC 1, S2R 2, SEL 2, SHF.L.U32 1, SHF.R.U32.HI 5, STG.E.64 1, ULDC.64 1 |
| 0731 | single math | `float copysignf(a, b)` | 20 | BRA 1, EXIT 2, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0732 | single math | `float fdimf(a, b)` | 22 | BRA 1, EXIT 2, FADD 1, FSEL 1, FSETP.GTU.AND 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0733 | single math | `float fdividef(a, b)` | 138 | BRA 18, BSSY 3, BSYNC 3, CALL.REL.NOINC 1, EXIT 2, FADD.FTZ 2, FCHK 1, FFMA 14, FFMA.RM 1, FFMA.RP 1, FFMA.RZ 1, FSETP.GTU.FTZ.AND 2, FSETP.NEU.FTZ.AND 4, I2F.U64 2, IADD3 7, IMAD 3, IMAD.IADD 4, IMAD.MOV 1, IMAD.MOV.U32 12, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 4, ISETP.GE.U32.AND 2, ISETP.GT.AND 1, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 3, LDG.E.64 2, LEA 1, LOP3.LUT 17, MOV 1, MUFU.RCP 2, MUFU.RSQ 1, PLOP3.LUT 4, RET.REL.NODEC 1, S2R 2, SEL 2, SHF.L.U32 1, SHF.R.U32.HI 5, STG.E.64 1, ULDC.64 1 |
| 0734 | single math | `float fmaxf(a, b)` | 20 | BRA 1, EXIT 2, FMNMX 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0735 | single math | `float fminf(a, b)` | 20 | BRA 1, EXIT 2, FMNMX 1, I2F.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0736 | single math | `float fmodf(a, b)` | 91 | BRA 10, BSSY 3, BSYNC 3, EXIT 2, FADD 8, FFMA 8, FFMA.RZ 1, FMUL 6, FRND.TRUNC 2, FSEL 4, FSETP.GE.AND 2, FSETP.GEU.AND 3, FSETP.GTU.AND 2, FSETP.LEU.OR 1, I2F.U64 2, IADD3 3, IMAD 1, IMAD.IADD 1, IMAD.MOV.U32 3, IMAD.WIDE.U32 2, IMNMX.U32 1, ISETP.GE.U32.AND 2, ISETP.GT.U32.AND 2, ISETP.NE.AND 2, LDG.E.64 2, LOP3.LUT 8, MUFU.RCP 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0737 | single math | `float hypotf(a, b)` | 71 | BRA 6, BSSY 1, BSYNC 1, CALL.REL.NOINC 1, EXIT 2, FADD 2, FADD.FTZ 2, FFMA 6, FMUL 4, FMUL.FTZ 5, FSEL 1, FSETP.GEU.FTZ.AND 1, FSETP.GTU.FTZ.AND 1, FSETP.NEU.AND 2, FSETP.NEU.FTZ.AND 1, I2F.U64 2, IADD3 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, IMNMX 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, LDG.E.64 2, LOP3.LUT 3, MOV 9, MUFU.RSQ 2, RET.REL.NODEC 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0738 | single math | `float nextafterf(a, b)` | 48 | BRA 5, BSSY 1, BSYNC 1, EXIT 2, FADD 1, FSETP.GEU.AND 1, FSETP.GT.AND 3, FSETP.GTU.AND 1, FSETP.GTU.OR 1, FSETP.LT.AND 2, FSETP.NEU.AND 2, I2F.U64 2, IADD3 4, IMAD 1, IMAD.MOV 4, IMAD.MOV.U32 5, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LOP3.LUT 2, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0739 | single math | `float powf(a, b)` | 112 | BRA 7, BSSY 1, BSYNC 1, EXIT 2, F2I.NTZ 1, FADD 15, FFMA 19, FMUL 10, FRND 1, FRND.FLOOR 1, FRND.TRUNC 1, FSEL 5, FSETP.EQ.AND 1, FSETP.EQ.OR 2, FSETP.GEU.AND 3, FSETP.GEU.OR 1, FSETP.GT.AND 2, FSETP.GTU.AND 1, FSETP.GTU.OR 1, FSETP.NEU.AND 6, I2F.U64 2, I2FP.F32.S32 1, IADD3 3, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LEA 1, LOP3.LUT 3, MOV 8, MUFU.RCP 1, S2R 2, SEL 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0740 | single math | `float remainderf(a, b)` | 112 | BRA 10, BSSY 3, BSYNC 3, EXIT 2, F2I.U32.TRUNC.NTZ 2, FADD 10, FFMA 8, FFMA.RZ 1, FMUL 6, FRND.TRUNC 2, FSEL 4, FSETP.GE.AND 2, FSETP.GEU.AND 3, FSETP.GTU.AND 2, FSETP.LEU.AND 1, FSETP.LEU.OR 1, FSETP.NEU.AND 1, I2F.U64 2, IADD3 2, IMAD 1, IMAD.IADD 3, IMAD.MOV.U32 7, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMNMX.U32 1, ISETP.GE.U32.AND 3, ISETP.GT.AND 1, ISETP.GT.U32.AND 2, ISETP.NE.AND 2, ISETP.NE.U32.OR 1, LDG.E.64 2, LOP3.LUT 8, MOV 2, MUFU.RCP 2, S2R 2, SEL 1, SHF.L.U32 2, SHF.R.U32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0741 | single math | `float rhypotf(a, b)` | 36 | BRA 1, EXIT 2, FADD 2, FFMA 1, FMUL 6, FSEL 1, FSETP.GEU.AND 1, FSETP.NEU.AND 1, I2F.U64 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, IMNMX 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LOP3.LUT 1, MOV 3, MUFU.RSQ 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0742 | single math | `float fmaf(a, b, c)` | 22 | BRA 1, EXIT 2, FFMA 1, I2F.U64 3, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 3, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0743 | single math | `float norm3df(a, b, c)` | 77 | BRA 6, BSSY 1, BSYNC 1, CALL.REL.NOINC 1, EXIT 2, FADD 2, FADD.FTZ 2, FFMA 7, FMNMX 4, FMUL 5, FMUL.FTZ 5, FSEL 1, FSETP.GEU.FTZ.AND 1, FSETP.GT.AND 1, FSETP.GTU.FTZ.AND 1, FSETP.NEU.AND 1, FSETP.NEU.FTZ.AND 1, I2F.U64 3, IADD3 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, LDG.E.64 3, LOP3.LUT 3, MOV 9, MUFU.RSQ 2, RET.REL.NODEC 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0744 | single math | `float rnorm3df(a, b, c)` | 43 | BRA 1, EXIT 2, FADD 2, FFMA 2, FMNMX 4, FMUL 7, FSEL 1, FSETP.GEU.AND 1, FSETP.GTU.AND 1, FSETP.NEU.AND 1, I2F.U64 3, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 3, LOP3.LUT 1, MOV 3, MUFU.RSQ 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0745 | double math | `double acos(a)` | 87 | BRA 6, BSSY 2, BSYNC 2, DADD 8, DFMA 32, DMUL 6, EXIT 2, I2F.F64.U64 1, IADD3 3, IMAD 1, IMAD.MOV.U32 6, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 3, ISETP.GE.U32.AND 1, ISETP.GT.AND 2, LDG.E.64 1, MOV 3, MUFU.RSQ64H 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0746 | double math | `double acosh(a)` | 234 | BRA 14, BSSY 3, BSYNC 3, CALL.REL.NOINC 1, DADD 15, DFMA 49, DMUL 13, DMUL.RP 1, DSETP.NAN.AND 2, EXIT 2, FFMA 1, FSEL 10, FSETP.GEU.AND 3, FSETP.GT.AND 2, FSETP.GTU.AND 1, FSETP.LT.AND 1, FSETP.NEU.AND 3, I2F.F64.U64 1, IADD3 8, IMAD 1, IMAD.IADD 2, IMAD.MOV 1, IMAD.MOV.U32 42, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMNMX 2, ISETP.EQ.OR 1, ISETP.GE.AND 1, ISETP.GE.U32.AND 5, ISETP.GT.AND 1, ISETP.GT.U32.AND 2, ISETP.GT.U32.OR 1, ISETP.NE.AND 3, LDG.E.64 1, LEA.HI 1, LOP3.LUT 21, MOV 1, MUFU.RCP64H 3, MUFU.RSQ64H 1, RET.REL.NODEC 1, S2R 2, SEL 3, STG.E.64 1, ULDC.64 1 |
| 0747 | double math | `double asin(a)` | 80 | BRA 3, BSSY 1, BSYNC 1, DADD 3, DFMA 33, DMUL 5, DSETP.NE.AND 1, EXIT 2, FSEL 4, FSETP.GEU.AND 1, I2F.F64.U64 1, IADD3 1, IMAD 1, IMAD.MOV.U32 7, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 4, MUFU.RSQ64H 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0748 | double math | `double asinh(a)` | 238 | BRA 14, BSSY 3, BSYNC 3, CALL.REL.NOINC 1, DADD 13, DFMA 51, DMUL 15, DMUL.RP 1, DSETP.NAN.AND 2, EXIT 2, FFMA 1, FSEL 8, FSETP.GEU.AND 3, FSETP.GT.AND 2, FSETP.GTU.AND 1, FSETP.LT.AND 1, FSETP.NEU.AND 3, I2F.F64.U64 1, IADD3 7, IMAD 1, IMAD.IADD 2, IMAD.MOV 1, IMAD.MOV.U32 45, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMNMX 2, ISETP.EQ.OR 1, ISETP.GE.AND 1, ISETP.GE.U32.AND 5, ISETP.GT.AND 1, ISETP.GT.U32.AND 2, ISETP.GT.U32.OR 1, ISETP.NE.AND 2, LDG.E.64 1, LEA.HI 1, LOP3.LUT 23, MOV 1, MUFU.RCP64H 4, MUFU.RSQ64H 1, RET.REL.NODEC 1, S2R 2, SEL 3, STG.E.64 1, ULDC.64 1 |
| 0749 | double math | `double atan(a)` | 58 | BRA 2, BSSY 1, BSYNC 1, DADD 2, DFMA 22, DMUL 2, DSETP.GT.AND 1, DSETP.NEU.AND 1, EXIT 2, FSEL 4, I2F.F64.U64 1, IMAD 1, IMAD.MOV.U32 4, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 3, MUFU.RCP64H 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0750 | double math | `double atanh(a)` | 241 | BRA 15, BSSY 4, BSYNC 4, CALL.REL.NOINC 2, DADD 14, DFMA 49, DMUL 14, DMUL.RP 1, DSETP.NAN.AND 2, EXIT 2, FFMA 2, FSEL 4, FSETP.GEU.AND 4, FSETP.GT.AND 3, FSETP.GTU.AND 1, FSETP.LT.AND 1, FSETP.NEU.AND 3, I2F.F64.U64 1, IADD3 7, IMAD 1, IMAD.IADD 2, IMAD.MOV 1, IMAD.MOV.U32 49, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMNMX 2, ISETP.EQ.OR 1, ISETP.GE.AND 1, ISETP.GE.U32.AND 5, ISETP.GT.AND 1, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 2, LDG.E.64 1, LEA.HI 1, LOP3.LUT 22, MOV 2, MUFU.RCP64H 4, RET.REL.NODEC 1, S2R 2, SEL 3, STG.E.64 1, ULDC.64 1 |
| 0751 | double math | `double cbrt(a)` | 56 | BRA 1, DADD 1, DFMA 6, DMUL 3, DSETP.EQ.OR 1, DSETP.NAN.AND 1, EXIT 2, F2F.F32.F64 1, F2F.F64.F32 1, F2I.NTZ 1, FMUL 2, FSEL 2, I2F.F64.U64 1, I2FP.F32.S32 1, IADD3 2, IMAD 2, IMAD.IADD 1, IMAD.MOV.U32 5, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E.64 1, LEA 1, LOP3.LUT 2, MOV 4, MUFU.EX2 1, MUFU.LG2 1, MUFU.RCP64H 1, S2R 2, SHF.R.U32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0752 | double math | `double ceil(a)` | 17 | BRA 1, EXIT 2, FRND.F64.CEIL 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0753 | double math | `double cos(a)` | 209 | BRA 7, BSSY 2, BSYNC 2, CALL.REL.NOINC 1, CS2R 2, DFMA 12, DMUL 3, DSETP.GE.AND 1, DSETP.NEU.AND 1, EXIT 2, F2I.F64 1, FLO.U32 1, FSEL 2, I2F.F64 1, I2F.F64.U64 1, IADD3 22, IADD3.X 7, IMAD 6, IMAD.HI.U32 4, IMAD.IADD 2, IMAD.MOV 3, IMAD.MOV.U32 23, IMAD.SHL.U32 6, IMAD.WIDE 1, IMAD.WIDE.U32 7, IMAD.X 9, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, ISETP.GT.AND 2, ISETP.GT.AND.EX 1, ISETP.GT.U32.AND 1, ISETP.NE.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 2, LDG.E.128.CONSTANT 3, LDG.E.64 1, LDG.E.64.CONSTANT 1, LDL.64 3, LEA.HI 1, LEA.HI.X 1, LOP3.LUT 19, MOV 1, R2P 1, RET.REL.NODEC 1, S2R 2, SEL 8, SHF.L.U32 3, SHF.L.U64.HI 6, SHF.R.U32.HI 8, SHF.R.U64 5, STG.E.64 1, STL.64 2, ULDC.64 2, UMOV 1 |
| 0754 | double math | `double cosh(a)` | 53 | BRA 2, BSSY 1, BSYNC 1, DADD 2, DFMA 18, DSETP.GTU.AND 1, EXIT 2, FSEL 2, I2F.F64.U64 1, IADD3 1, IMAD 1, IMAD.MOV.U32 5, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, LDG.E.64 1, LEA 1, LOP3.LUT 1, MOV 3, MUFU.RCP64H 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0755 | double math | `double cospi(a)` | 51 | BRA 1, DFMA 11, DMUL 3, EXIT 2, F2I.S64.F64 1, FRND.F64 1, FSEL 2, I2F.F64.U64 1, IADD3 2, IMAD 1, IMAD.MOV.U32 4, IMAD.SHL.U32 2, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 2, LDG.E.128.CONSTANT 3, LDG.E.64 1, LOP3.LUT 1, MOV 4, R2P 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0756 | double math | `double cyl_bessel_i0(a)` | 118 | BRA 4, BSSY 2, BSYNC 2, DADD 3, DFMA 54, DMUL 8, DSETP.GEU.AND 1, EXIT 2, FSEL 2, FSETP.GEU.AND 2, I2F.F64.U64 1, IADD3 2, IMAD 2, IMAD.MOV.U32 11, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, LDG.E.64 1, LEA 2, LEA.HI 1, MOV 6, MUFU.RCP64H 1, MUFU.RSQ64H 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0757 | double math | `double cyl_bessel_i1(a)` | 120 | BRA 4, BSSY 2, BSYNC 2, DADD 3, DFMA 53, DMUL 10, DSETP.GEU.AND 1, EXIT 2, FSEL 2, FSETP.GEU.AND 2, I2F.F64.U64 1, IADD3 2, IMAD 2, IMAD.MOV.U32 11, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, LDG.E.64 1, LEA 2, LEA.HI 1, LOP3.LUT 1, MOV 6, MUFU.RCP64H 1, MUFU.RSQ64H 1, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0758 | double math | `double erfc(a)` | 116 | BRA 3, BSSY 1, BSYNC 1, DADD 7, DFMA 50, DMUL 6, EXIT 2, FSEL 7, I2F.F64.U64 1, IMAD 2, IMAD.IADD 1, IMAD.MOV.U32 12, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.EQ.AND 1, ISETP.GE.AND 2, ISETP.GE.U32.AND 2, ISETP.GT.U32.AND 1, ISETP.NE.AND 1, LDG.E.64 1, LEA 1, LEA.HI 1, LOP3.LUT 1, MOV 2, MUFU.RCP64H 2, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0759 | double math | `double erfcinv(a)` | 428 | BRA 24, BSSY 8, BSYNC 8, CALL.REL.NOINC 3, CS2R 1, DADD 24, DFMA 129, DMUL 27, DMUL.RP 1, DSETP.GTU.AND 1, DSETP.NAN.AND 2, DSETP.NEU.AND 2, EXIT 2, FFMA 2, FSEL 8, FSETP.GE.AND 1, FSETP.GEU.AND 4, FSETP.GT.AND 2, FSETP.GTU.AND 1, FSETP.NEU.AND 3, I2F.F64.U64 1, IADD3 10, IMAD 2, IMAD.IADD 2, IMAD.MOV 1, IMAD.MOV.U32 77, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMNMX 2, ISETP.EQ.OR 1, ISETP.GE.AND 1, ISETP.GE.U32.AND 6, ISETP.GT.AND 2, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 3, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, LEA.HI 1, LOP3.LUT 29, MOV 7, MUFU.RCP64H 6, MUFU.RSQ64H 3, RET.REL.NODEC 2, S2R 4, SEL 3, SHF.R.U32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0760 | double math | `double erfcx(a)` | 189 | BRA 11, BSSY 5, BSYNC 5, CALL.REL.NOINC 1, DADD 8, DFMA 73, DMUL 11, DSETP.GTU.AND 1, DSETP.NEU.AND 1, EXIT 2, FSEL 2, FSETP.GEU.AND 4, I2F.F64.U64 2, IADD3 4, IMAD 3, IMAD.IADD 1, IMAD.MOV.U32 20, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 3, ISETP.GT.AND 1, LDG.E.64 1, LEA 2, LEA.HI 1, LOP3.LUT 5, MOV 6, MUFU.RCP64H 5, RET.REL.NODEC 1, S2R 4, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0761 | double math | `double erf(a)` | 71 | BRA 1, DADD 3, DFMA 37, DMUL 1, DSETP.GE.AND 1, EXIT 2, F2F.F32.F64 1, F2F.F64.F32 2, FMUL 1, FRND 1, FSEL 2, I2F.F64.U64 1, IMAD 1, IMAD.MOV.U32 3, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 3, MUFU.EX2 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0762 | double math | `double erfinv(a)` | 206 | BRA 15, BSSY 3, BSYNC 3, CALL.REL.NOINC 1, DADD 9, DFMA 84, DFMA.RM 1, DFMA.RP 1, DMUL 11, DSETP.GT.AND 1, DSETP.GTU.AND 1, DSETP.NE.AND 1, DSETP.NEU.AND 1, EXIT 2, FSEL 3, FSETP.GEU.AND 1, I2F.F64.U64 2, IADD3 6, IADD3.X 1, IMAD 2, IMAD.MOV.U32 23, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 2, ISETP.GE.U32.AND 4, ISETP.GT.AND 1, LDG.E.64 1, LOP3.LUT 3, MOV 9, MUFU.RCP64H 1, MUFU.RSQ64H 2, RET.REL.NODEC 1, S2R 4, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0763 | double math | `double exp10(a)` | 57 | BRA 2, BSSY 1, BSYNC 1, DADD 2, DFMA 15, DMUL 2, DSETP.GTU.AND 1, EXIT 2, FSEL 3, FSETP.GEU.AND 2, I2F.F64.U64 1, IADD3 1, IMAD 3, IMAD.MOV.U32 5, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, LDG.E.64 1, LEA 1, LEA.HI 1, MOV 3, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0764 | double math | `double exp2(a)` | 54 | BRA 2, BSSY 1, BSYNC 1, DADD 4, DFMA 12, DMUL 2, DSETP.GTU.AND 1, EXIT 2, FSEL 3, FSETP.GEU.AND 2, I2F.F64.U64 1, IMAD 3, IMAD.IADD 1, IMAD.MOV.U32 3, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, LDG.E.64 1, LEA 1, LEA.HI 1, MOV 3, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0765 | double math | `double exp(a)` | 50 | BRA 2, BSSY 1, BSYNC 1, DADD 2, DFMA 14, DMUL 1, EXIT 2, FSETP.GEU.AND 2, I2F.F64.U64 1, IMAD 2, IMAD.IADD 1, IMAD.MOV.U32 4, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LEA 2, LEA.HI 1, MOV 4, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0766 | double math | `double expm1(a)` | 67 | BRA 3, BSSY 1, BSYNC 1, DADD 4, DFMA 15, DMUL 1, DSETP.GTU.AND 1, EXIT 2, FSEL 7, FSETP.GT.AND 1, FSETP.LT.AND 1, I2F.F64.U64 1, IADD3 1, IMAD 1, IMAD.MOV 1, IMAD.MOV.U32 10, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, ISETP.NE.AND 2, LDG.E.64 1, LEA 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0767 | double math | `double fabs(a)` | 17 | BRA 1, DADD 1, EXIT 2, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0768 | double math | `double floor(a)` | 17 | BRA 1, EXIT 2, FRND.F64.FLOOR 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0769 | double math | `double j0(a)` | 384 | BRA 22, BSSY 6, BSYNC 6, CALL.REL.NOINC 3, CS2R 4, DADD 9, DFMA 89, DMUL 17, DSETP.GE.AND 2, DSETP.GTU.AND 4, DSETP.NEU.AND 4, EXIT 2, F2I.F64 2, FLO.U32 1, FSEL 2, I2F.F64 2, I2F.F64.U32 1, I2F.F64.U64 1, IADD3 23, IADD3.X 7, IMAD 6, IMAD.HI.U32 4, IMAD.IADD 2, IMAD.MOV 3, IMAD.MOV.U32 48, IMAD.SHL.U32 6, IMAD.WIDE 1, IMAD.WIDE.U32 7, IMAD.X 9, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, ISETP.GT.AND 2, ISETP.GT.AND.EX 1, ISETP.GT.U32.AND 1, ISETP.NE.AND 2, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 2, LDG.E.128.CONSTANT 3, LDG.E.64 1, LDG.E.64.CONSTANT 1, LDL.64 3, LEA.HI 1, LEA.HI.X 1, LOP3.LUT 22, MOV 3, MUFU.RCP64H 1, MUFU.RSQ64H 2, R2P 1, RET.REL.NODEC 2, S2R 2, SEL 8, SHF.L.U32 3, SHF.L.U64.HI 6, SHF.R.U32.HI 8, SHF.R.U64 5, STG.E.64 1, STL.64 2, ULDC.64 2, UMOV 1 |
| 0770 | double math | `double j1(a)` | 388 | BRA 22, BSSY 6, BSYNC 6, CALL.REL.NOINC 3, CS2R 4, DADD 8, DFMA 87, DMUL 18, DSETP.GE.AND 2, DSETP.GEU.AND 1, DSETP.GTU.AND 4, DSETP.NEU.AND 4, EXIT 2, F2I.F64 2, FLO.U32 1, FSEL 4, I2F.F64 2, I2F.F64.U32 1, I2F.F64.U64 1, IADD3 23, IADD3.X 7, IMAD 6, IMAD.HI.U32 4, IMAD.IADD 2, IMAD.MOV 3, IMAD.MOV.U32 51, IMAD.SHL.U32 6, IMAD.WIDE 1, IMAD.WIDE.U32 7, IMAD.X 9, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, ISETP.GT.AND 2, ISETP.GT.AND.EX 1, ISETP.GT.U32.AND 1, ISETP.NE.AND 2, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 2, LDG.E.128.CONSTANT 3, LDG.E.64 1, LDG.E.64.CONSTANT 1, LDL.64 3, LEA.HI 1, LEA.HI.X 1, LOP3.LUT 22, MOV 3, MUFU.RCP64H 1, MUFU.RSQ64H 2, R2P 1, RET.REL.NODEC 2, S2R 2, SEL 8, SHF.L.U32 3, SHF.L.U64.HI 6, SHF.R.U32.HI 8, SHF.R.U64 5, STG.E.64 1, STL.64 2, ULDC.64 2, UMOV 1 |
| 0771 | double math | `double lgamma(a)` | 517 | BRA 29, BSSY 8, BSYNC 8, CALL.REL.NOINC 3, DADD 37, DFMA 164, DMUL 26, DMUL.RP 1, DSETP.GTU.AND 1, DSETP.NAN.AND 2, DSETP.NEU.AND 2, EXIT 2, F2I.S64.F64 1, FFMA 2, FRND.F64 1, FRND.F64.TRUNC 1, FSEL 14, FSETP.GEU.AND 3, FSETP.GT.AND 2, FSETP.GTU.AND 1, FSETP.NEU.AND 5, I2F.F64.U64 1, IADD3 14, IMAD 1, IMAD.IADD 2, IMAD.MOV 1, IMAD.MOV.U32 94, IMAD.SHL.U32 2, IMAD.WIDE.U32 3, IMNMX 2, ISETP.EQ.OR 1, ISETP.GE.AND 4, ISETP.GE.U32.AND 7, ISETP.GT.AND 7, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 2, LDG.E.128.CONSTANT 3, LDG.E.64 1, LEA.HI 4, LOP3.LUT 30, MOV 7, MUFU.RCP64H 7, RET.REL.NODEC 2, S2R 2, SEL 3, STG.E.64 1, ULDC.64 1 |
| 0772 | double math | `double log10(a)` | 79 | BRA 2, BSSY 1, BSYNC 1, DADD 8, DFMA 18, DMUL 6, EXIT 2, FSEL 2, FSETP.NEU.AND 1, I2F.F64.U64 1, IADD3 3, IMAD 1, IMAD.MOV.U32 12, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, ISETP.GT.AND 1, LDG.E.64 1, LEA.HI 1, LOP3.LUT 3, MOV 4, MUFU.RCP64H 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0773 | double math | `double log1p(a)` | 211 | BRA 14, BSSY 3, BSYNC 3, CALL.REL.NOINC 1, DADD 12, DFMA 42, DMUL 12, DMUL.RP 1, DSETP.NAN.AND 2, EXIT 2, FFMA 1, FSEL 4, FSETP.GEU.AND 3, FSETP.GT.AND 2, FSETP.GTU.AND 1, FSETP.LT.AND 1, FSETP.NEU.AND 3, I2F.F64.U64 1, IADD3 7, IMAD 1, IMAD.IADD 2, IMAD.MOV 1, IMAD.MOV.U32 40, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMNMX 2, ISETP.EQ.OR 1, ISETP.GE.AND 1, ISETP.GE.U32.AND 5, ISETP.GT.AND 1, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 2, LDG.E.64 1, LEA.HI 1, LOP3.LUT 21, MOV 1, MUFU.RCP64H 3, RET.REL.NODEC 1, S2R 2, SEL 3, STG.E.64 1, ULDC.64 1 |
| 0774 | double math | `double log2(a)` | 79 | BRA 2, BSSY 1, BSYNC 1, DADD 8, DFMA 18, DMUL 6, EXIT 2, FSEL 2, FSETP.NEU.AND 1, I2F.F64.U64 1, IADD3 3, IMAD 1, IMAD.MOV.U32 12, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, ISETP.GT.AND 1, LDG.E.64 1, LEA.HI 1, LOP3.LUT 3, MOV 4, MUFU.RCP64H 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0775 | double math | `double logb(a)` | 44 | BRA 7, BSSY 1, BSYNC 1, DADD 2, DSETP.GTU.AND 1, DSETP.NEU.AND 2, EXIT 2, FLO.U32 1, I2F.F64 2, I2F.F64.U64 1, IADD3 4, IMAD 1, IMAD.MOV.U32 4, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, ISETP.NE.U32.AND 1, LDG.E.64 1, LEA.HI 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0776 | double math | `double log(a)` | 77 | BRA 2, BSSY 1, BSYNC 1, DADD 8, DFMA 17, DMUL 5, EXIT 2, FSEL 2, FSETP.NEU.AND 1, I2F.F64.U64 1, IADD3 3, IMAD 1, IMAD.MOV.U32 12, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, ISETP.GT.AND 1, LDG.E.64 1, LEA.HI 1, LOP3.LUT 3, MOV 4, MUFU.RCP64H 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0777 | double math | `double nearbyint(a)` | 17 | BRA 1, EXIT 2, FRND.F64 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0778 | double math | `double normcdf(a)` | 132 | BRA 3, BSSY 1, BSYNC 1, DADD 10, DFMA 53, DMUL 10, DSETP.GT.AND 1, EXIT 2, FSEL 9, I2F.F64.U64 1, IMAD 2, IMAD.IADD 1, IMAD.MOV.U32 15, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.EQ.AND 1, ISETP.GE.AND 2, ISETP.GE.U32.AND 3, ISETP.GT.U32.AND 1, ISETP.NE.AND 1, LDG.E.64 1, LEA 1, LEA.HI 1, LOP3.LUT 2, MUFU.RCP64H 2, S2R 2, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0779 | double math | `double normcdfinv(a)` | 432 | BRA 24, BSSY 8, BSYNC 8, CALL.REL.NOINC 3, CS2R 1, DADD 26, DFMA 130, DMUL 28, DMUL.RP 1, DSETP.GE.AND 1, DSETP.GTU.AND 2, DSETP.NAN.AND 2, DSETP.NEU.AND 2, EXIT 2, FFMA 2, FSEL 8, FSETP.GE.AND 1, FSETP.GEU.AND 4, FSETP.GT.AND 2, FSETP.GTU.AND 1, FSETP.NEU.AND 3, I2F.F64.U64 1, IADD3 10, IMAD 2, IMAD.IADD 2, IMAD.MOV 1, IMAD.MOV.U32 76, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMNMX 2, ISETP.EQ.OR 1, ISETP.GE.AND 1, ISETP.GE.U32.AND 6, ISETP.GT.AND 2, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 3, LDG.E.64 1, LEA.HI 1, LOP3.LUT 29, MOV 8, MUFU.RCP64H 6, MUFU.RSQ64H 3, RET.REL.NODEC 2, S2R 4, SEL 3, SHF.R.U32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0780 | double math | `double rcbrt(a)` | 59 | BRA 1, DADD 1, DFMA 7, DMUL 4, DSETP.GTU.AND 1, DSETP.NEU.AND 1, EXIT 2, F2F.F32.F64 1, F2F.F64.F32 1, F2I.NTZ 1, FMUL 2, FSEL 2, I2F.F64.U64 1, I2FP.F32.S32 1, IADD3 1, IMAD 2, IMAD.IADD 1, IMAD.MOV.U32 6, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E.64 1, LEA 1, LOP3.LUT 3, MOV 5, MUFU.EX2 1, MUFU.LG2 1, S2R 2, SHF.R.U32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0781 | double math | `double rint(a)` | 17 | BRA 1, EXIT 2, FRND.F64 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0782 | double math | `double round(a)` | 21 | BRA 1, DADD.RZ 1, EXIT 2, FRND.F64.TRUNC 1, I2F.F64.U64 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0783 | double math | `double rsqrt(a)` | 70 | BRA 8, BSSY 2, BSYNC 2, CALL.REL.NOINC 1, CS2R 1, DADD 1, DFMA 6, DMUL 6, DSETP.GTU.AND 1, DSETP.NEU.AND 2, EXIT 2, I2F.F64.U64 1, IADD3 1, IMAD 1, IMAD.MOV.U32 9, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, ISETP.NE.AND 1, LDG.E.64 1, LOP3.LUT 2, MOV 10, MUFU.RSQ64H 2, RET.REL.NODEC 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0784 | double math | `double sin(a)` | 208 | BRA 7, BSSY 2, BSYNC 2, CALL.REL.NOINC 1, CS2R 2, DFMA 12, DMUL 3, DSETP.GE.AND 1, DSETP.NEU.AND 1, EXIT 2, F2I.F64 1, FLO.U32 1, FSEL 2, I2F.F64 1, I2F.F64.U64 1, IADD3 21, IADD3.X 7, IMAD 6, IMAD.HI.U32 4, IMAD.IADD 2, IMAD.MOV 3, IMAD.MOV.U32 23, IMAD.SHL.U32 6, IMAD.WIDE 1, IMAD.WIDE.U32 7, IMAD.X 9, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, ISETP.GT.AND 2, ISETP.GT.AND.EX 1, ISETP.GT.U32.AND 1, ISETP.NE.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 2, LDG.E.128.CONSTANT 3, LDG.E.64 1, LDG.E.64.CONSTANT 1, LDL.64 3, LEA.HI 1, LEA.HI.X 1, LOP3.LUT 19, MOV 1, R2P 1, RET.REL.NODEC 1, S2R 2, SEL 8, SHF.L.U32 3, SHF.L.U64.HI 6, SHF.R.U32.HI 8, SHF.R.U64 5, STG.E.64 1, STL.64 2, ULDC.64 2, UMOV 1 |
| 0785 | double math | `double sinh(a)` | 194 | BRA 13, BSSY 3, BSYNC 3, CALL.REL.NOINC 1, DADD 4, DFMA 40, DMUL 7, DMUL.RP 1, DSETP.GE.AND 1, DSETP.NAN.AND 2, EXIT 2, FFMA 1, FSEL 8, FSETP.GEU.AND 3, FSETP.GT.AND 1, FSETP.GTU.AND 1, FSETP.NEU.AND 2, I2F.F64.U64 1, IADD3 6, IMAD 1, IMAD.IADD 2, IMAD.MOV 2, IMAD.MOV.U32 36, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, IMNMX 2, ISETP.EQ.OR 1, ISETP.GE.U32.AND 6, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 4, LDG.E.64 1, LEA 1, LOP3.LUT 20, MOV 1, MUFU.RCP64H 2, RET.REL.NODEC 1, S2R 2, SEL 4, STG.E.64 1, ULDC.64 1 |
| 0786 | double math | `double sinpi(a)` | 48 | BRA 1, DFMA 11, DMUL 3, DSETP.NEU.AND 1, EXIT 2, F2I.S64.F64 1, FRND.F64 1, FRND.F64.TRUNC 1, FSEL 2, I2F.F64.U64 1, IADD3 1, IMAD 1, IMAD.MOV.U32 3, IMAD.SHL.U32 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E.128.CONSTANT 3, LDG.E.64 1, LOP3.LUT 3, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0787 | double math | `double sqrt(a)` | 79 | BRA 8, BSSY 2, BSYNC 2, CALL.REL.NOINC 1, DADD 1, DFMA 10, DFMA.RM 1, DFMA.RP 1, DMUL 7, DSETP.GT.AND 1, DSETP.NE.AND 1, EXIT 2, FSEL 2, I2F.F64.U64 1, IADD3 5, IMAD 1, IMAD.MOV.U32 10, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMAD.X 1, ISETP.GE.AND 1, ISETP.GE.U32.AND 3, ISETP.GT.AND 1, LDG.E.64 1, MOV 6, MUFU.RSQ64H 2, RET.REL.NODEC 1, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0788 | double math | `double tan(a)` | 222 | BRA 8, BSSY 3, BSYNC 3, CALL.REL.NOINC 1, CS2R 2, DADD 1, DFMA 25, DMUL 4, DSETP.GE.AND 1, DSETP.NEU.AND 1, EXIT 2, F2I.F64 1, FLO.U32 1, I2F.F64 1, I2F.F64.U64 1, IADD3 21, IADD3.X 7, IMAD 6, IMAD.HI.U32 4, IMAD.IADD 2, IMAD.MOV 3, IMAD.MOV.U32 25, IMAD.SHL.U32 5, IMAD.WIDE 1, IMAD.WIDE.U32 6, IMAD.X 9, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, ISETP.GT.AND 2, ISETP.GT.AND.EX 1, ISETP.GT.U32.AND 1, ISETP.NE.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 3, LDG.E.64 1, LDG.E.64.CONSTANT 1, LDL.64 3, LEA.HI 1, LEA.HI.X 1, LOP3.LUT 19, MOV 1, MUFU.RCP64H 1, RET.REL.NODEC 1, S2R 2, SEL 8, SHF.L.U32 3, SHF.L.U64.HI 6, SHF.R.U32.HI 8, SHF.R.U64 5, STG.E.64 1, STL.64 2, ULDC.64 2, UMOV 1 |
| 0789 | double math | `double tanh(a)` | 74 | BRA 3, BSSY 1, BSYNC 1, DADD 3, DFMA 28, DMUL 2, DSETP.GE.AND 1, EXIT 2, F2F.F32.F64 1, F2F.F64.F32 2, FMUL 1, FRND 1, FSEL 2, I2F.F64.U64 1, IMAD 1, IMAD.MOV.U32 6, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, LDG.E.64 1, LOP3.LUT 2, MOV 4, MUFU.EX2 1, MUFU.RCP64H 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0790 | double math | `double tgamma(a)` | 336 | BRA 14, BSSY 4, BSYNC 4, CALL.REL.NOINC 1, DADD 44, DFMA 93, DMUL 16, DMUL.RP 1, DSETP.GEU.AND 1, DSETP.NAN.AND 2, EXIT 2, FFMA 1, FSEL 10, FSETP.GE.AND 4, FSETP.GEU.AND 6, FSETP.GT.AND 1, FSETP.GTU.AND 1, FSETP.NEU.AND 2, I2F.F64.U64 1, IADD3 8, IMAD 4, IMAD.IADD 3, IMAD.MOV 1, IMAD.MOV.U32 52, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMNMX 2, ISETP.EQ.OR 1, ISETP.GE.U32.AND 5, ISETP.GT.U32.AND 2, ISETP.GT.U32.AND.EX 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 3, LDG.E.64 1, LEA 1, LEA.HI 2, LOP3.LUT 21, MOV 1, MUFU.RCP64H 4, RET.REL.NODEC 1, S2R 4, SEL 3, SHF.R.S32.HI 1, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0791 | double math | `double trunc(a)` | 17 | BRA 1, EXIT 2, FRND.F64.TRUNC 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0792 | double math | `double y0(a)` | 668 | BRA 37, BSSY 11, BSYNC 11, CALL.REL.NOINC 6, CS2R 5, DADD 26, DFMA 215, DMUL 38, DSETP.GE.AND 4, DSETP.GTU.AND 8, DSETP.NEU.AND 6, EXIT 2, F2I.F64 4, FLO.U32 1, FSEL 6, FSETP.NEU.AND 1, I2F.F64 4, I2F.F64.U32 2, I2F.F64.U64 1, IADD3 28, IADD3.X 7, IMAD 6, IMAD.HI.U32 4, IMAD.IADD 2, IMAD.MOV 3, IMAD.MOV.U32 90, IMAD.SHL.U32 6, IMAD.WIDE 1, IMAD.WIDE.U32 8, IMAD.X 9, ISETP.GE.AND 2, ISETP.GE.U32.AND 4, ISETP.GT.AND 3, ISETP.GT.AND.EX 1, ISETP.GT.U32.AND 1, ISETP.NE.AND 2, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 2, LDG.E.128.CONSTANT 6, LDG.E.64 1, LDG.E.64.CONSTANT 1, LDL.64 3, LEA.HI 2, LEA.HI.X 1, LOP3.LUT 27, MOV 10, MUFU.RCP64H 3, MUFU.RSQ64H 3, R2P 2, RET.REL.NODEC 2, S2R 2, SEL 8, SHF.L.U32 4, SHF.L.U64.HI 6, SHF.R.U32.HI 8, SHF.R.U64 5, STG.E.64 1, STL.64 2, ULDC.64 2, UMOV 1 |
| 0793 | double math | `double y1(a)` | 836 | BRA 53, BSSY 15, BSYNC 15, CALL.REL.NOINC 8, CS2R 5, DADD 27, DFMA 244, DMUL 45, DMUL.RP 1, DSETP.GE.AND 4, DSETP.GEU.AND 3, DSETP.GTU.AND 9, DSETP.NAN.AND 1, DSETP.NEU.AND 6, EXIT 2, F2I.F64 4, FFMA 1, FLO.U32 1, FSEL 14, FSETP.GEU.AND 2, FSETP.GT.AND 1, FSETP.GTU.AND 1, FSETP.NEU.AND 3, I2F.F64 4, I2F.F64.U32 2, I2F.F64.U64 1, IADD3 37, IADD3.X 7, IMAD 6, IMAD.HI.U32 4, IMAD.IADD 3, IMAD.MOV 4, IMAD.MOV.U32 126, IMAD.SHL.U32 7, IMAD.WIDE 1, IMAD.WIDE.U32 8, IMAD.X 9, IMNMX 2, ISETP.EQ.OR 1, ISETP.GE.AND 2, ISETP.GE.U32.AND 6, ISETP.GT.AND 3, ISETP.GT.AND.EX 1, ISETP.GT.U32.AND 2, ISETP.GT.U32.OR 1, ISETP.LE.U32.AND 2, ISETP.NE.AND 4, ISETP.NE.AND.EX 2, ISETP.NE.U32.AND 3, LDG.E.128.CONSTANT 6, LDG.E.64 1, LDG.E.64.CONSTANT 1, LDL.64 3, LEA.HI 2, LEA.HI.X 1, LOP3.LUT 44, MOV 8, MUFU.RCP64H 8, MUFU.RSQ64H 3, R2P 2, RET.REL.NODEC 4, S2R 2, SEL 10, SHF.L.U32 3, SHF.L.U64.HI 6, SHF.R.U32.HI 8, SHF.R.U64 5, STG.E.64 1, STL.64 2, ULDC.64 2, UMOV 1 |
| 0794 | double math | `double atan2(a, b)` | 195 | BRA 14, BSSY 3, BSYNC 3, CALL.REL.NOINC 1, DADD 6, DFMA 36, DMUL 6, DMUL.RP 1, DSETP.GT.AND 1, DSETP.GTU.AND 1, DSETP.NAN.AND 2, DSETP.NEU.AND 2, EXIT 2, FFMA 1, FSEL 18, FSETP.GEU.AND 3, FSETP.GT.AND 1, FSETP.GTU.AND 1, FSETP.NEU.AND 2, I2F.F64.U64 2, IADD3 4, IMAD 2, IMAD.IADD 2, IMAD.MOV 1, IMAD.MOV.U32 30, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMNMX 2, ISETP.EQ.OR 1, ISETP.GE.AND 2, ISETP.GE.U32.AND 4, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 2, LDG.E.64 2, LOP3.LUT 19, MOV 1, MUFU.RCP64H 2, RET.REL.NODEC 1, S2R 4, SEL 3, STG.E.64 1, ULDC.64 1 |
| 0795 | double math | `double copysign(a, b)` | 20 | BRA 1, EXIT 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0796 | double math | `double fdim(a, b)` | 22 | BRA 1, DADD 1, DSETP.GTU.AND 1, EXIT 2, FSEL 2, I2F.F64.U64 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0797 | double math | `double fmax(a, b)` | 26 | BRA 1, DSETP.MAX.AND 1, EXIT 2, FSEL 1, I2F.F64.U64 2, IMAD 1, IMAD.MOV.U32 4, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LOP3.LUT 1, MOV 2, S2R 2, SEL 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0798 | double math | `double fmin(a, b)` | 26 | BRA 1, DSETP.MIN.AND 1, EXIT 2, FSEL 1, I2F.F64.U64 2, IMAD 1, IMAD.MOV.U32 4, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 2, LOP3.LUT 1, MOV 2, S2R 2, SEL 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0799 | double math | `double fmod(a, b)` | 272 | BRA 13, BSSY 8, BSYNC 8, DADD 1, DMUL 3, DSETP.GE.AND 1, DSETP.GTU.AND 1, DSETP.LE.AND 1, DSETP.NEU.AND 2, EXIT 2, FSEL 2, I2F.F64.U64 2, IADD3 34, IMAD 1, IMAD.IADD 4, IMAD.MOV.U32 16, IMAD.SHL.U32 22, IMAD.U32.X 1, IMAD.WIDE.U32 2, IMAD.X 21, IMNMX 2, ISETP.GE.AND 22, ISETP.GE.U32.AND 3, ISETP.GT.AND 1, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 4, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 2, LEA.HI 3, LOP3.LUT 12, S2R 2, SEL 42, SHF.L.U32 1, SHF.L.U64.HI 22, SHF.R.U32.HI 4, SHF.R.U64 1, STG.E.64 1, ULDC.64 1 |
| 0800 | double math | `double hypot(a, b)` | 58 | BRA 1, DADD 2, DFMA 4, DMUL 7, DSETP.MIN.AND 1, DSETP.NEU.AND 1, EXIT 2, FSEL 3, I2F.F64.U64 2, IADD3 1, IMAD 1, IMAD.MOV.U32 9, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, ISETP.GT.U32.AND 1, ISETP.GT.U32.AND.EX 1, ISETP.LT.U32.AND 1, ISETP.LT.U32.AND.EX 1, LDG.E.64 2, LOP3.LUT 3, MUFU.RSQ64H 1, S2R 2, SEL 5, STG.E.64 1, ULDC.64 1 |
| 0801 | double math | `double nextafter(a, b)` | 44 | BRA 5, BSSY 1, BSYNC 1, DADD 1, DSETP.GT.AND 1, DSETP.GTU.AND 1, DSETP.GTU.OR 1, DSETP.LT.AND 1, EXIT 2, I2F.F64.U64 2, IADD3 1, IADD3.X 1, IMAD 1, IMAD.MOV.U32 4, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 3, LDG.E.64 2, LOP3.LUT 5, S2R 2, SEL 2, STG.E.64 1, ULDC.64 1 |
| 0802 | double math | `double pow(a, b)` | 228 | BRA 11, BSSY 3, BSYNC 3, CALL.REL.NOINC 1, DADD 36, DFMA 41, DMUL 9, DSETP.GEU.AND 1, DSETP.GT.AND 1, DSETP.GTU.AND 1, DSETP.GTU.OR 1, DSETP.NEU.AND 5, EXIT 2, FRND.F64.TRUNC 1, FSEL 4, FSETP.GEU.AND 2, I2F.F64.U64 4, IADD3 5, IMAD 3, IMAD.IADD 1, IMAD.MOV.U32 30, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, ISETP.EQ.AND.EX 1, ISETP.EQ.OR.EX 1, ISETP.EQ.U32.AND 2, ISETP.GE.AND 1, ISETP.GE.OR 1, ISETP.GE.U32.AND 2, ISETP.GT.AND 2, ISETP.GT.U32.AND 1, ISETP.LT.AND 2, ISETP.NE.AND 3, ISETP.NE.AND.EX 3, ISETP.NE.U32.AND 3, LDG.E.64 2, LEA 1, LEA.HI 4, LOP3.LUT 10, MOV 1, MUFU.RCP64H 1, PLOP3.LUT 1, RET.REL.NODEC 1, S2R 2, SEL 6, SHF.L.U32 2, SHF.L.U64.HI 2, SHF.R.S32.HI 1, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0803 | double math | `double remainder(a, b)` | 279 | BRA 13, BSSY 8, BSYNC 8, CS2R 1, DADD 3, DMUL 3, DSETP.GE.AND 1, DSETP.GTU.AND 1, DSETP.LE.AND 1, DSETP.LEU.AND 1, DSETP.NEU.AND 2, DSETP.NEU.OR 1, EXIT 2, FSEL 2, I2F.F64.U64 2, IADD3 34, IMAD 1, IMAD.IADD 4, IMAD.MOV.U32 17, IMAD.SHL.U32 22, IMAD.U32.X 1, IMAD.WIDE.U32 2, IMAD.X 21, IMNMX 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 3, ISETP.GT.AND 19, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 5, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 2, LEA.HI 3, LOP3.LUT 15, S2R 2, SEL 42, SHF.L.U32 1, SHF.L.U64.HI 22, SHF.R.U32.HI 4, SHF.R.U64 1, STG.E.64 1, ULDC.64 1 |
| 0804 | double math | `double rhypot(a, b)` | 57 | BRA 2, BSSY 1, BSYNC 1, DADD 3, DFMA 4, DMUL 6, DSETP.GTU.AND 1, DSETP.NEU.AND 2, EXIT 2, FSEL 2, I2F.F64.U64 2, IADD3 1, IMAD 1, IMAD.MOV.U32 7, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, ISETP.GT.U32.AND 1, ISETP.GT.U32.AND.EX 1, ISETP.LT.U32.AND 1, ISETP.LT.U32.AND.EX 1, LDG.E.64 2, LOP3.LUT 1, MUFU.RSQ64H 1, S2R 2, SEL 5, STG.E.64 1, ULDC.64 1 |
| 0805 | double math | `double fma(a, b, c)` | 21 | BRA 1, DFMA 1, EXIT 2, I2F.F64.U64 3, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 3, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0806 | double math | `double norm3d(a, b, c)` | 91 | BRA 1, DADD 2, DFMA 5, DMUL 8, DSETP.GT.AND 1, DSETP.MAX.AND 2, DSETP.MIN.AND 3, EXIT 2, FSEL 9, I2F.F64.U64 3, IADD3 1, IMAD 1, IMAD.MOV.U32 23, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, LDG.E.64 3, LOP3.LUT 7, MOV 5, MUFU.RSQ64H 1, S2R 2, SEL 5, STG.E.64 1, ULDC.64 1 |
| 0807 | double math | `double rnorm3d(a, b, c)` | 82 | BRA 1, DADD 3, DFMA 5, DMUL 7, DSETP.GT.AND 1, DSETP.MAX.AND 2, DSETP.MIN.AND 2, DSETP.NEU.AND 1, EXIT 2, FSEL 8, I2F.F64.U64 3, IADD3 1, IMAD 1, IMAD.MOV.U32 18, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 3, LOP3.LUT 5, MOV 6, MUFU.RSQ64H 1, S2R 2, SEL 4, STG.E.64 1, ULDC.64 1 |
| 0808 | single math | `float frexpf(a, o1)` | 27 | BRA 1, EXIT 2, FADD 1, FMUL 1, FSEL 1, FSETP.EQ.OR 1, FSETP.GEU.AND 1, I2F.U64 1, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E.64 1, LOP3.LUT 3, MOV 2, S2R 2, STG.E.64 1, ULDC.64 1 |
| 0809 | single math | `int ilogbf(a)` | 32 | BRA 2, BSSY 1, BSYNC 1, EXIT 2, FADD 1, FLO.U32 1, I2F.U64 1, IADD3 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, ISETP.GT.U32.AND 1, ISETP.NE.AND 2, LDG.E.64 1, LEA.HI 1, S2R 2, SEL 3, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0810 | single math | `float jnf(a, b)` | 1162 | BRA 80, BREAK 1, BSSY 26, BSYNC 26, CALL.REL.NOINC 8, DMUL 4, EXIT 2, F2F.F32.F64 4, F2I.NTZ 8, F2I.TRUNC.NTZ 1, FADD 30, FADD.FTZ 4, FCHK 7, FFMA 219, FFMA.RM 1, FFMA.RP 1, FFMA.RZ 1, FMUL 102, FMUL.FTZ 5, FSEL 29, FSETP.GE.AND 4, FSETP.GEU.AND 8, FSETP.GEU.FTZ.AND 1, FSETP.GT.AND 6, FSETP.GTU.AND 3, FSETP.GTU.FTZ.AND 3, FSETP.NEU.AND 8, FSETP.NEU.FTZ.AND 5, I2F.F64.S64 4, I2F.U64 1, I2FP.F32.S32 45, I2FP.F32.U32 4, IADD3 102, IADD3.X 4, IMAD 8, IMAD.IADD 6, IMAD.MOV 4, IMAD.MOV.U32 63, IMAD.SHL.U32 5, IMAD.U32 4, IMAD.WIDE.U32 6, IMNMX 1, ISETP.GE.AND 7, ISETP.GE.U32.AND 4, ISETP.GT.AND 7, ISETP.GT.U32.AND 2, ISETP.GT.U32.OR 1, ISETP.NE.AND 28, ISETP.NE.OR 1, ISETP.NE.U32.AND 1, LDG.E 1, LDG.E.64 1, LDG.E.CONSTANT 4, LDL 12, LEA 5, LEA.HI 4, LOP3.LUT 66, MOV 62, MUFU.RCP 12, MUFU.RSQ 7, PLOP3.LUT 7, RET.REL.NODEC 2, S2R 2, SEL 2, SHF.L.U32 13, SHF.L.U32.HI 4, SHF.R.U32.HI 25, STG.E.64 1, STL 8, UIADD3 4, UIADD3.X 4, ULDC.64 5, UMOV 5, WARPSYNC 1 |
| 0811 | single math | `float ldexpf(a, b)` | 33 | BRA 1, EXIT 2, FMUL 4, I2F.U64 1, IABS 1, IADD3 2, IMAD 2, IMAD.MOV.U32 1, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, IMNMX 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, SHF.L.U32 1, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0812 | single math | `long long llrintf(a)` | 17 | BRA 1, EXIT 2, F2I.S64 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0813 | single math | `long long llroundf(a)` | 20 | BRA 1, EXIT 2, F2I.S64.TRUNC 1, FADD.RZ 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0814 | single math | `long lrintf(a)` | 18 | BRA 1, EXIT 2, F2I.NTZ 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0815 | single math | `long lroundf(a)` | 21 | BRA 1, EXIT 2, F2I.TRUNC.NTZ 1, FADD.RZ 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0816 | single math | `float modff(a, o1)` | 22 | BRA 1, EXIT 2, FADD 1, FRND.TRUNC 1, FSETP.NEU.AND 1, I2F.U64 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0817 | single math | `float norm4df(a, b, c, e)` | 84 | BRA 6, BSSY 1, BSYNC 1, CALL.REL.NOINC 1, EXIT 2, FADD 3, FADD.FTZ 2, FFMA 8, FMNMX 6, FMUL 6, FMUL.FTZ 5, FSEL 1, FSETP.GEU.FTZ.AND 1, FSETP.GT.AND 1, FSETP.GTU.FTZ.AND 1, FSETP.NEU.AND 1, FSETP.NEU.FTZ.AND 1, I2F.U64 4, IADD3 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, LDG.E.64 4, LOP3.LUT 3, MOV 9, MUFU.RSQ 2, RET.REL.NODEC 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0818 | single math | `float normf(a, o1)` | 373 | BRA 26, BREAK 1, BSSY 10, BSYNC 10, CALL.REL.NOINC 1, EXIT 2, FADD 1, FADD.FTZ 2, FFMA 35, FMNMX 29, FMUL 15, FMUL.FTZ 5, FSETP.EQ.OR 1, FSETP.GEU.FTZ.AND 1, FSETP.GTU.FTZ.AND 1, FSETP.NEU.AND 30, FSETP.NEU.FTZ.AND 1, I2F.U64 4, IADD3 80, IMAD 1, IMAD.IADD 2, IMAD.MOV.U32 28, IMAD.SHL.U32 3, IMAD.WIDE.U32 2, ISETP.EQ.AND 37, ISETP.GE.AND 2, ISETP.GE.U32.AND 3, ISETP.GT.AND 8, ISETP.GT.U32.AND 1, ISETP.NE.AND 6, ISETP.NE.OR 2, LDG.E.64 4, LOP3.LUT 4, MOV 1, MUFU.RSQ 2, PLOP3.LUT 6, RET.REL.NODEC 1, S2R 2, STG.E.64 1, ULDC.64 1, WARPSYNC 1 |
| 0819 | single math | `float remquof(a, b, o2)` | 112 | BRA 10, BSSY 3, BSYNC 3, EXIT 2, F2I.U32.TRUNC.NTZ 2, FADD 10, FFMA 8, FFMA.RZ 1, FMUL 6, FRND.TRUNC 2, FSEL 4, FSETP.GE.AND 2, FSETP.GEU.AND 3, FSETP.GTU.AND 2, FSETP.LEU.AND 1, FSETP.LEU.OR 1, FSETP.NEU.AND 1, I2F.U64 2, IADD3 2, IMAD 1, IMAD.IADD 3, IMAD.MOV.U32 7, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, IMNMX.U32 1, ISETP.GE.U32.AND 3, ISETP.GT.AND 1, ISETP.GT.U32.AND 2, ISETP.NE.AND 2, ISETP.NE.U32.OR 1, LDG.E.64 2, LOP3.LUT 8, MOV 2, MUFU.RCP 2, S2R 2, SEL 1, SHF.L.U32 2, SHF.R.U32.HI 2, STG.E.64 1, ULDC.64 1 |
| 0820 | single math | `float rnorm4df(a, b, c, e)` | 50 | BRA 1, EXIT 2, FADD 3, FFMA 3, FMNMX 6, FMUL 8, FSEL 1, FSETP.GEU.AND 1, FSETP.GTU.AND 1, FSETP.NEU.AND 1, I2F.U64 4, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 4, LOP3.LUT 1, MOV 3, MUFU.RSQ 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0821 | single math | `float rnormf(a, o1)` | 336 | BRA 21, BREAK 1, BSSY 9, BSYNC 9, CS2R 2, EXIT 2, FADD 1, FFMA 30, FMNMX 29, FMUL 17, FSEL 1, FSETP.GEU.AND 1, FSETP.NEU.AND 30, I2F.U64 4, IADD3 78, IMAD 1, IMAD.IADD 2, IMAD.MOV.U32 16, IMAD.SHL.U32 3, IMAD.WIDE.U32 2, ISETP.EQ.AND 37, ISETP.GE.AND 2, ISETP.GE.U32.AND 3, ISETP.GT.AND 8, ISETP.NE.AND 6, ISETP.NE.OR 2, LDG.E.64 4, LOP3.LUT 3, MUFU.RSQ 1, PLOP3.LUT 6, S2R 2, STG.E.64 1, ULDC.64 1, WARPSYNC 1 |
| 0822 | single math | `float scalblnf(a, b)` | 33 | BRA 1, EXIT 2, FMUL 4, I2F.U64 1, IABS 1, IADD3 2, IMAD 2, IMAD.MOV.U32 1, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, IMNMX 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, SHF.L.U32 1, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0823 | single math | `float scalbnf(a, b)` | 33 | BRA 1, EXIT 2, FMUL 4, I2F.U64 1, IABS 1, IADD3 2, IMAD 2, IMAD.MOV.U32 1, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, IMNMX 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, SHF.L.U32 1, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0824 | single math | `void sincosf(a, o1, o2)` | 150 | BRA 6, BSSY 2, BSYNC 2, DMUL 1, EXIT 2, F2F.F32.F64 1, F2I.NTZ 1, FFMA 11, FMUL 3, FSEL 2, FSETP.GE.AND 1, FSETP.NEU.AND 1, I2F.F64.S64 1, I2F.U64 1, I2FP.F32.S32 1, IADD3 7, IADD3.X 1, IMAD 1, IMAD.IADD 2, IMAD.MOV 1, IMAD.MOV.U32 34, IMAD.SHL.U32 4, IMAD.U32 1, IMAD.WIDE.U32 3, ISETP.EQ.AND 21, ISETP.GE.U32.AND 1, ISETP.NE.AND 3, ISETP.NE.U32.AND 1, LDG.E.64 1, LDG.E.CONSTANT 1, LEA.HI 1, LOP3.LUT 9, MOV 5, S2R 2, SHF.L.U32 3, SHF.L.U32.HI 1, SHF.R.U32.HI 5, STG.E.64 1, UIADD3 1, UIADD3.X 1, ULDC.64 2, UMOV 1 |
| 0825 | single math | `void sincospif(a, o1, o2)` | 40 | BRA 1, EXIT 2, F2I.NTZ 1, FADD 1, FFMA 10, FMUL 2, FRND 1, FRND.TRUNC 1, FSEL 1, FSETP.NEU.AND 1, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.U32.AND 1, LDG.E.64 1, LOP3.LUT 2, MOV 5, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0826 | single math | `float ynf(a, b)` | 1836 | BRA 124, BREAK 1, BSSY 30, BSYNC 30, CALL.REL.NOINC 6, DMUL 8, EXIT 2, F2F.F32.F64 8, F2I.NTZ 16, FADD 62, FADD.FTZ 2, FCHK 6, FFMA 458, FFMA.RM 1, FFMA.RP 1, FFMA.RZ 1, FMUL 147, FSEL 54, FSETP.GE.AND 9, FSETP.GEU.AND 10, FSETP.GTU.AND 20, FSETP.GTU.FTZ.AND 2, FSETP.NEU.AND 20, FSETP.NEU.FTZ.AND 4, I2F.F64.S64 8, I2F.U64 1, I2FP.F32.S32 49, I2FP.F32.U32 8, IADD3 137, IADD3.X 8, IMAD 4, IMAD.IADD 8, IMAD.MOV.U32 36, IMAD.SHL.U32 4, IMAD.U32 5, IMAD.WIDE.U32 10, ISETP.GE.AND 4, ISETP.GE.U32.AND 7, ISETP.GT.AND 5, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.LT.OR 1, ISETP.NE.AND 32, ISETP.NE.OR 1, LDG.E 1, LDG.E.64 1, LDG.E.CONSTANT 8, LDL 24, LEA 16, LEA.HI 8, LOP3.LUT 104, MOV 146, MUFU.RCP 15, MUFU.RSQ 9, PLOP3.LUT 7, RET.REL.NODEC 1, S2R 2, SEL 2, SHF.L.U32 30, SHF.L.U32.HI 8, SHF.R.U32.HI 45, STG.E.64 1, STL 16, UIADD3 8, UIADD3.X 8, ULDC.64 9, UMOV 14, WARPSYNC 1 |
| 0827 | single math | `bool isfinite(a)` | 19 | BRA 1, EXIT 2, FSETP.GEU.AND 1, I2F.U64 1, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0828 | single math | `bool isinf(a)` | 19 | BRA 1, EXIT 2, FSETP.NEU.AND 1, I2F.U64 1, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0829 | single math | `bool isnan(a)` | 19 | BRA 1, EXIT 2, FSETP.GTU.AND 1, I2F.U64 1, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0830 | single math | `bool signbit(a)` | 18 | BRA 1, EXIT 2, I2F.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0831 | double math | `double frexp(a, o1)` | 27 | BRA 1, DADD 1, DMUL 1, DSETP.EQ.OR 1, EXIT 2, I2F.F64.U64 1, IADD3 1, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, LDG.E.64 1, LOP3.LUT 4, S2R 2, SEL 2, STG.E.64 1, ULDC.64 1 |
| 0832 | double math | `int ilogb(a)` | 41 | BRA 5, BSSY 1, BSYNC 1, DADD 1, DSETP.GTU.AND 1, DSETP.NEU.AND 1, EXIT 2, FLO.U32 1, I2F.F64.U64 1, IADD3 3, IMAD 1, IMAD.MOV 1, IMAD.MOV.U32 6, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 2, LDG.E.64 1, LEA.HI 1, S2R 2, SEL 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0833 | double math | `double jn(a, b)` | 1646 | BRA 100, BREAK 1, BSSY 32, BSYNC 32, CALL.REL.NOINC 15, CS2R 13, DADD 34, DFMA 433, DFMA.RM 1, DFMA.RP 1, DMUL 165, DMUL.RP 1, DSETP.GE.AND 8, DSETP.GEU.AND 2, DSETP.GT.AND 15, DSETP.GTU.AND 13, DSETP.NAN.AND 2, DSETP.NE.AND 1, DSETP.NEU.AND 10, EXIT 2, F2F.F32.F64 13, F2F.F64.F32 13, F2I.F64 8, F2I.F64.TRUNC 2, FFMA 2, FLO.U32 1, FSEL 68, FSETP.GEU.AND 3, FSETP.GT.AND 2, FSETP.GTU.AND 1, FSETP.NEU.AND 2, I2F.F64 53, I2F.F64.U32 4, I2F.F64.U64 5, IADD3 95, IADD3.X 7, IMAD 8, IMAD.HI.U32 4, IMAD.IADD 8, IMAD.MOV 4, IMAD.MOV.U32 204, IMAD.SHL.U32 9, IMAD.WIDE 1, IMAD.WIDE.U32 10, IMAD.X 10, IMNMX 5, ISETP.EQ.OR 1, ISETP.GE.AND 4, ISETP.GE.U32.AND 12, ISETP.GT.AND 8, ISETP.GT.AND.EX 1, ISETP.GT.U32.AND 2, ISETP.GT.U32.OR 1, ISETP.NE.AND 26, ISETP.NE.AND.EX 1, ISETP.NE.OR 1, ISETP.NE.U32.AND 3, LDG.E 1, LDG.E.128.CONSTANT 12, LDG.E.64 1, LDG.E.64.CONSTANT 1, LDL.64 3, LEA.HI 1, LEA.HI.X 1, LOP3.LUT 58, MOV 15, MUFU.RCP64H 7, MUFU.RSQ64H 7, PLOP3.LUT 3, R2P 4, RET.REL.NODEC 4, S2R 4, SEL 11, SHF.L.U32 3, SHF.L.U64.HI 6, SHF.R.U32.HI 9, SHF.R.U64 5, STG.E.64 1, STL.64 2, ULDC.64 2, UMOV 1, WARPSYNC 1 |
| 0834 | double math | `double ldexp(a, b)` | 34 | BRA 1, DMUL 4, EXIT 2, I2F.F64.U64 1, IABS 1, IADD3 2, IMAD 2, IMAD.MOV.U32 3, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, IMNMX 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 1, S2R 2, SEL 1, SHF.L.U32 1, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0835 | double math | `long long llrint(a)` | 17 | BRA 1, EXIT 2, F2I.S64.F64 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0836 | double math | `long long llround(a)` | 21 | BRA 1, DADD.RZ 1, EXIT 2, F2I.S64.F64.TRUNC 1, I2F.F64.U64 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0837 | double math | `long lrint(a)` | 18 | BRA 1, EXIT 2, F2I.F64 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0838 | double math | `long lround(a)` | 22 | BRA 1, DADD.RZ 1, EXIT 2, F2I.F64.TRUNC 1, I2F.F64.U64 1, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0839 | double math | `double modf(a, o1)` | 21 | BRA 1, DADD 1, DSETP.NEU.AND 1, EXIT 2, FRND.F64.TRUNC 1, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0840 | double math | `double norm4d(a, b, c, e)` | 109 | BRA 1, DADD 3, DFMA 6, DMUL 9, DSETP.GT.AND 1, DSETP.MAX.AND 3, DSETP.MIN.AND 4, EXIT 2, FSEL 11, I2F.F64.U64 4, IADD3 1, IMAD 1, IMAD.MOV.U32 28, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, LDG.E.64 4, LOP3.LUT 9, MOV 5, MUFU.RSQ64H 1, S2R 2, SEL 7, STG.E.64 1, ULDC.64 1 |
| 0841 | double math | `double norm(a, o1)` | 593 | BRA 21, BREAK 2, BSSY 10, BSYNC 10, CS2R 3, DADD 1, DFMA 33, DMUL 34, DSETP.MAX.AND 29, DSETP.MIN.AND 1, DSETP.NEU.AND 30, EXIT 2, FSEL 30, I2F.F64.U64 4, IADD3 86, IMAD 10, IMAD.IADD 2, IMAD.MOV.U32 104, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.EQ.AND 29, ISETP.GE.AND 2, ISETP.GE.U32.AND 4, ISETP.GT.AND 8, ISETP.NE.AND 6, ISETP.NE.OR 2, LDG.E.64 4, LDL.128 13, LDL.64 32, LOP3.LUT 33, MUFU.RSQ64H 1, PLOP3.LUT 6, S2R 2, SEL 30, STG.E.64 1, STL.128 2, ULDC.64 1, WARPSYNC 2 |
| 0842 | double math | `double remquo(a, b, o2)` | 159 | BRA 10, BSSY 7, BSYNC 7, CS2R 1, DADD 3, DMUL 3, DSETP.GE.AND 1, DSETP.GTU.AND 1, DSETP.LE.AND 1, DSETP.LEU.AND 1, DSETP.NEU.AND 2, DSETP.NEU.OR 1, EXIT 2, FSEL 2, I2F.F64.U64 2, IADD3 13, IMAD 1, IMAD.IADD 4, IMAD.MOV.U32 13, IMAD.SHL.U32 6, IMAD.U32.X 1, IMAD.WIDE.U32 2, IMAD.X 5, IMNMX 1, ISETP.GE.AND 1, ISETP.GE.U32.AND 2, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 11, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 2, LEA.HI 2, LOP3.LUT 11, S2R 2, SEL 10, SHF.L.U32 1, SHF.L.U32.HI 5, SHF.L.U64.HI 6, SHF.R.U32.HI 9, SHF.R.U64 1, STG.E.64 1, ULDC.64 1 |
| 0843 | double math | `double rnorm4d(a, b, c, e)` | 100 | BRA 1, DADD 4, DFMA 6, DMUL 8, DSETP.GT.AND 1, DSETP.MAX.AND 3, DSETP.MIN.AND 3, DSETP.NEU.AND 1, EXIT 2, FSEL 10, I2F.F64.U64 4, IADD3 1, IMAD 1, IMAD.MOV.U32 24, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 4, LOP3.LUT 7, MOV 5, MUFU.RSQ64H 1, S2R 2, SEL 6, STG.E.64 1, ULDC.64 1 |
| 0844 | double math | `double rnorm(a, o1)` | 616 | BRA 28, BREAK 2, BSSY 12, BSYNC 12, CALL.REL.NOINC 1, CS2R 4, DADD 2, DFMA 36, DMUL 37, DSETP.GTU.AND 1, DSETP.MAX.AND 29, DSETP.NEU.AND 32, EXIT 2, FSEL 31, I2F.F64.U64 4, IADD3 87, IMAD 3, IMAD.IADD 2, IMAD.MOV.U32 117, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.EQ.AND 29, ISETP.GE.AND 2, ISETP.GE.U32.AND 4, ISETP.GT.AND 8, ISETP.NE.AND 7, ISETP.NE.OR 2, LDG.E.64 4, LDL.128 24, LDL.64 10, LOP3.LUT 34, MOV 1, MUFU.RSQ64H 2, PLOP3.LUT 6, RET.REL.NODEC 1, S2R 2, SEL 29, STG.E.64 1, STL.128 2, ULDC.64 1, WARPSYNC 2 |
| 0845 | double math | `double scalbln(a, b)` | 34 | BRA 1, DMUL 4, EXIT 2, I2F.F64.U64 1, IABS 1, IADD3 2, IMAD 2, IMAD.MOV.U32 3, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, IMNMX 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 1, S2R 2, SEL 1, SHF.L.U32 1, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0846 | double math | `double scalbn(a, b)` | 34 | BRA 1, DMUL 4, EXIT 2, I2F.F64.U64 1, IABS 1, IADD3 2, IMAD 2, IMAD.MOV.U32 3, IMAD.SHL.U32 2, IMAD.WIDE.U32 2, IMNMX 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 1, S2R 2, SEL 1, SHF.L.U32 1, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0847 | double math | `void sincos(a, o1, o2)` | 214 | BRA 7, BSSY 2, BSYNC 2, CALL.REL.NOINC 1, CS2R 2, DFMA 17, DMUL 3, DSETP.GE.AND 1, DSETP.NEU.AND 1, EXIT 2, F2I.F64 1, FLO.U32 1, FSEL 2, I2F.F64 1, I2F.F64.U64 1, IADD3 21, IADD3.X 7, IMAD 6, IMAD.HI.U32 4, IMAD.IADD 2, IMAD.MOV 3, IMAD.MOV.U32 27, IMAD.SHL.U32 5, IMAD.WIDE 1, IMAD.WIDE.U32 6, IMAD.X 9, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, ISETP.GT.AND 2, ISETP.GT.AND.EX 1, ISETP.GT.U32.AND 1, ISETP.NE.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 3, LDG.E.64 1, LDG.E.64.CONSTANT 1, LDL.64 3, LEA.HI 1, LEA.HI.X 1, LOP3.LUT 21, MOV 1, RET.REL.NODEC 1, S2R 2, SEL 8, SHF.L.U32 3, SHF.L.U64.HI 6, SHF.R.U32.HI 8, SHF.R.U64 5, STG.E.64 1, STL.64 2, ULDC.64 2, UMOV 1 |
| 0848 | double math | `void sincospi(a, o1, o2)` | 59 | BRA 1, DFMA 16, DMUL 4, DSETP.NEU.AND 1, EXIT 2, F2I.S64.F64 1, FRND.F64 1, FRND.F64.TRUNC 1, FSEL 2, I2F.F64.U64 1, IADD3 1, IMAD 1, IMAD.MOV.U32 8, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, ISETP.NE.U32.AND 1, ISETP.NE.U32.AND.EX 1, LDG.E.64 1, LOP3.LUT 3, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0849 | double math | `double yn(a, b)` | 2563 | BRA 157, BREAK 1, BRX 1, BSSY 51, BSYNC 51, CALL.REL.NOINC 30, CS2R 11, DADD 105, DFMA 937, DMUL 181, DMUL.RP 1, DSETP.GE.AND 16, DSETP.GEU.AND 7, DSETP.GTU.AND 31, DSETP.NAN.AND 2, DSETP.NEU.AND 18, EXIT 2, F2I.F64 16, FFMA 4, FLO.U32 1, FSEL 38, FSETP.GEU.AND 4, FSETP.GT.AND 4, FSETP.GTU.AND 1, FSETP.NEU.AND 6, I2F.F64 45, I2F.F64.U32 8, I2F.F64.U64 6, IADD3 95, IADD3.X 8, IMAD 7, IMAD.HI.U32 4, IMAD.IADD 4, IMAD.MOV 4, IMAD.MOV.U32 344, IMAD.SHL.U32 12, IMAD.WIDE 1, IMAD.WIDE.U32 14, IMAD.X 8, IMNMX 2, IMNMX.U32 1, ISETP.EQ.OR 1, ISETP.GE.AND 6, ISETP.GE.U32.AND 19, ISETP.GT.AND 10, ISETP.GT.AND.EX 1, ISETP.GT.U32.AND 2, ISETP.GT.U32.OR 1, ISETP.NE.AND 7, ISETP.NE.AND.EX 3, ISETP.NE.OR 1, ISETP.NE.U32.AND 4, LDC 1, LDG.E 1, LDG.E.128.CONSTANT 24, LDG.E.64 1, LDG.E.64.CONSTANT 1, LDL.64 3, LEA.HI 5, LEA.HI.X 1, LOP3.LUT 72, MOV 66, MUFU.RCP64H 23, MUFU.RSQ64H 9, PLOP3.LUT 3, R2P 8, RET.REL.NODEC 4, S2R 4, SEL 11, SHF.L.U32 5, SHF.L.U64.HI 6, SHF.R.S32.HI 1, SHF.R.U32.HI 8, SHF.R.U64 5, STG.E.64 1, STL.64 2, ULDC.64 2, UMOV 1, WARPSYNC 1 |
| 0850 | double math | `bool isfinite(a)` | 19 | BRA 1, DSETP.GEU.AND 1, EXIT 2, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SEL 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0851 | double math | `bool isinf(a)` | 19 | BRA 1, DSETP.NEU.AND 1, EXIT 2, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SEL 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0852 | double math | `bool isnan(a)` | 19 | BRA 1, DSETP.GTU.AND 1, EXIT 2, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SEL 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0853 | double math | `bool signbit(a)` | 18 | BRA 1, EXIT 2, I2F.F64.U64 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 3, S2R 2, SHF.L.U32 1, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0854 | atomic | `int atomicAdd` | 20 | ATOMG.E.ADD.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0855 | atomic | `unsigned int atomicAdd` | 20 | ATOMG.E.ADD.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 1, MOV 4, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0856 | atomic | `unsigned long long atomicAdd` | 18 | ATOMG.E.ADD.64.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0857 | atomic | `float atomicAdd` | 21 | ATOMG.E.ADD.F32.FTZ.RN.STRONG.GPU 1, BRA 1, EXIT 2, I2F.U64 1, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 4, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0858 | atomic | `double atomicAdd` | 19 | ATOMG.E.ADD.F64.RN.STRONG.GPU 1, BRA 1, EXIT 2, I2F.F64.U64 1, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0859 | atomic | `int atomicSub` | 21 | ATOMG.E.ADD.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.MOV 1, IMAD.MOV.U32 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0860 | atomic | `unsigned int atomicSub` | 21 | ATOMG.E.ADD.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0861 | atomic | `int atomicExch` | 20 | ATOMG.E.EXCH.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0862 | atomic | `unsigned int atomicExch` | 20 | ATOMG.E.EXCH.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 1, MOV 4, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0863 | atomic | `unsigned long long atomicExch` | 18 | ATOMG.E.EXCH.64.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0864 | atomic | `float atomicExch` | 21 | ATOMG.E.EXCH.STRONG.GPU 1, BRA 1, EXIT 2, I2F.U64 1, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 4, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0865 | atomic | `int atomicMin` | 20 | ATOMG.E.MIN.S32.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0866 | atomic | `unsigned int atomicMin` | 20 | ATOMG.E.MIN.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 1, MOV 4, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0867 | atomic | `long long atomicMin` | 18 | ATOMG.E.MIN.S64.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0868 | atomic | `unsigned long long atomicMin` | 18 | ATOMG.E.MIN.64.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0869 | atomic | `int atomicMax` | 20 | ATOMG.E.MAX.S32.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0870 | atomic | `unsigned int atomicMax` | 20 | ATOMG.E.MAX.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 1, MOV 4, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0871 | atomic | `long long atomicMax` | 18 | ATOMG.E.MAX.S64.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0872 | atomic | `unsigned long long atomicMax` | 18 | ATOMG.E.MAX.64.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0873 | atomic | `unsigned int atomicInc` | 20 | ATOMG.E.INC.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 1, MOV 4, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0874 | atomic | `unsigned int atomicDec` | 20 | ATOMG.E.DEC.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 1, MOV 4, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0875 | atomic | `int atomicCAS` | 21 | ATOMG.E.CAS.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 2, MOV 3, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0876 | atomic | `unsigned int atomicCAS` | 21 | ATOMG.E.CAS.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 2, MOV 4, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0877 | atomic | `unsigned long long atomicCAS` | 19 | ATOMG.E.CAS.64.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E.64 2, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0878 | atomic | `int atomicAnd` | 20 | ATOMG.E.AND.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0879 | atomic | `unsigned int atomicAnd` | 20 | ATOMG.E.AND.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 1, MOV 4, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0880 | atomic | `unsigned long long atomicAnd` | 18 | ATOMG.E.AND.64.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0881 | atomic | `int atomicOr` | 20 | ATOMG.E.OR.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0882 | atomic | `unsigned int atomicOr` | 20 | ATOMG.E.OR.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 1, MOV 4, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0883 | atomic | `unsigned long long atomicOr` | 18 | ATOMG.E.OR.64.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0884 | atomic | `int atomicXor` | 20 | ATOMG.E.XOR.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0885 | atomic | `unsigned int atomicXor` | 20 | ATOMG.E.XOR.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 1, MOV 4, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0886 | atomic | `unsigned long long atomicXor` | 18 | ATOMG.E.XOR.64.STRONG.GPU 1, BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0887 | warp | `int r = __all_sync(0xffffffffu, a > b);` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1, VOTE.ALL 1 |
| 0888 | warp | `int r = __any_sync(0xffffffffu, a > b);` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1, VOTE.ANY 1 |
| 0889 | warp | `unsigned int r = __ballot_sync(0xffffffffu, a > b);` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, LDG.E 2, MOV 2, S2R 2, STG.E.64 1, ULDC.64 1, VOTE.ANY 1 |
| 0890 | warp | `unsigned int r = __activemask();` | 15 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 1, ISETP.GE.U32.AND 1, MOV 4, S2R 2, STG.E.64 1, ULDC.64 1, VOTEU.ANY 1 |
| 0891 | warp | `unsigned int r = __match_any_sync(0xffffffffu, a);` | 17 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MATCH.ANY 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0892 | warp | `int p; unsigned int r = __match_all_sync(0xffffffffu, a, &p);` | 17 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MATCH.ALL 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0893 | warp | `unsigned int r = __reduce_add_sync(0xffffffffu, (unsigned int)a);` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 4, REDUX.SUM.S32 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0894 | warp | `int r = __reduce_min_sync(0xffffffffu, a);` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, REDUX.MIN.S32 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0895 | warp | `int r = __reduce_max_sync(0xffffffffu, a);` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, REDUX.MAX.S32 1, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0896 | warp | `unsigned int r = __reduce_and_sync(0xffffffffu, (unsigned int)a);` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 4, REDUX 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0897 | warp | `unsigned int r = __reduce_or_sync(0xffffffffu, (unsigned int)a);` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 4, REDUX.OR 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0898 | warp | `unsigned int r = __reduce_xor_sync(0xffffffffu, (unsigned int)a);` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 4, REDUX.XOR 1, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0899 | warp | `int r = __shfl_sync(0xffffffffu, a, b & 31);` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, SHFL.IDX 1, STG.E.64 1, ULDC.64 1 |
| 0900 | warp | `int r = __shfl_up_sync(0xffffffffu, a, 1);` | 17 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, SHFL.UP 1, STG.E.64 1, ULDC.64 1 |
| 0901 | warp | `int r = __shfl_down_sync(0xffffffffu, a, 1);` | 17 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, SHFL.DOWN 1, STG.E.64 1, ULDC.64 1 |
| 0902 | warp | `int r = __shfl_xor_sync(0xffffffffu, a, 1);` | 17 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, SHFL.BFLY 1, STG.E.64 1, ULDC.64 1 |
| 0903 | warp | `__syncwarp(); int r = a;` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0904 | sync | `__syncthreads(); int r = a;` | 17 | BAR.SYNC.DEFER_BLOCKING 1, BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0905 | sync | `int r = __syncthreads_count(a > b);` | 20 | B2R.RESULT 1, BAR.RED.POPC.DEFER_BLOCKING 1, BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, LDG.E 2, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0906 | sync | `int r = __syncthreads_and(a > b);` | 21 | B2R.RESULT 1, BAR.RED.AND.DEFER_BLOCKING 1, BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0907 | sync | `int r = __syncthreads_or(a > b);` | 21 | B2R.RESULT 1, BAR.RED.OR.DEFER_BLOCKING 1, BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0908 | sync | `__threadfence(); int r = a;` | 19 | BRA 1, CCTL.IVALL 1, ERRBAR 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MEMBAR.SC.GPU 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0909 | sync | `__threadfence_block(); int r = a;` | 17 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MEMBAR.SC.CTA 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0910 | sync | `__threadfence_system(); int r = a;` | 19 | BRA 1, CCTL.IVALL 1, ERRBAR 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MEMBAR.SC.SYS 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0911 | sync | `long long r = (long long)clock() + a;` | 20 | BRA 1, DEPBAR.LE 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LEA.HI.X.SX32 1, MOV 2, S2R 2, S2UR 1, SHF.L.U32 1, STG.E.64 1, ULDC.64 1, USHF.R.S32.HI 1 |
| 0912 | sync | `long long r = clock64() + a;` | 18 | BRA 1, CS2R 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, LEA.HI.X.SX32 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0913 | test | `int (a == b) ? a : b` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0914 | test | `int (a == b) && (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.EQ.AND 1, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0915 | test | `int (a == b) \|\| (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.EQ.OR 1, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0916 | test | `int (a != b) ? a : b` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0917 | test | `int (a != b) && (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 2, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0918 | test | `int (a != b) \|\| (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, ISETP.NE.OR 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0919 | test | `int (a < b) ? a : b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, LDG.E 2, S2R 2, SEL 1, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0920 | test | `int (a < b) && (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LT.AND 1, ISETP.NE.AND 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0921 | test | `int (a < b) \|\| (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LT.OR 1, ISETP.NE.AND 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0922 | test | `int (a > b) ? a : b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, LDG.E 2, S2R 2, SEL 1, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0923 | test | `int (a > b) && (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, ISETP.NE.AND 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0924 | test | `int (a > b) \|\| (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.OR 1, ISETP.NE.AND 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0925 | test | `int (a <= b) ? a : b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND 1, LDG.E 2, S2R 2, SEL 1, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0926 | test | `int (a <= b) && (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LE.AND 1, ISETP.NE.AND 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0927 | test | `int (a <= b) \|\| (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LE.OR 1, ISETP.NE.AND 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0928 | test | `int (a >= b) ? a : b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, LDG.E 2, S2R 2, SEL 1, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0929 | test | `int (a >= b) && (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND 1, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0930 | test | `int (a >= b) \|\| (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.OR 1, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0931 | test | `int a == 0` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0932 | test | `int a != 0` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0933 | test | `int (a != 0) && (c != 0)` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 2, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0934 | test | `int (a != 0) \|\| (c != 0)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0935 | test | `int a < 0` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, LOP3.LUT 1, MOV 2, S2R 2, SHF.R.U64 1, STG.E.64 1, ULDC.64 1 |
| 0936 | test | `unsigned int (a == b) ? a : b` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0937 | test | `unsigned int (a == b) && (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.EQ.AND 1, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0938 | test | `unsigned int (a == b) \|\| (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.EQ.OR 1, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0939 | test | `unsigned int (a != b) ? a : b` | 16 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0940 | test | `unsigned int (a != b) && (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 2, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0941 | test | `unsigned int (a != b) \|\| (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, ISETP.NE.OR 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0942 | test | `unsigned int (a < b) ? a : b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0943 | test | `unsigned int (a < b) && (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LT.U32.AND 1, ISETP.NE.AND 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0944 | test | `unsigned int (a < b) \|\| (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LT.U32.OR 1, ISETP.NE.AND 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0945 | test | `unsigned int (a > b) ? a : b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0946 | test | `unsigned int (a > b) && (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, ISETP.NE.AND 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0947 | test | `unsigned int (a > b) \|\| (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.OR 1, ISETP.NE.AND 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0948 | test | `unsigned int (a <= b) ? a : b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0949 | test | `unsigned int (a <= b) && (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LE.U32.AND 1, ISETP.NE.AND 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0950 | test | `unsigned int (a <= b) \|\| (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LE.U32.OR 1, ISETP.NE.AND 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0951 | test | `unsigned int (a >= b) ? a : b` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0952 | test | `unsigned int (a >= b) && (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, ISETP.NE.AND 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0953 | test | `unsigned int (a >= b) \|\| (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GE.U32.OR 1, ISETP.NE.AND 1, LDG.E 3, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0954 | test | `unsigned int a == 0` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0955 | test | `unsigned int a != 0` | 18 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0956 | test | `unsigned int (a != 0) && (c != 0)` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 2, LDG.E 2, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0957 | test | `unsigned int (a != 0) \|\| (c != 0)` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LOP3.LUT 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0958 | test | `long long (a == b) ? a : b` | 15 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0959 | test | `long long (a == b) && (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.EQ.AND.EX 1, ISETP.EQ.U32.AND 1, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0960 | test | `long long (a == b) \|\| (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.EQ.OR.EX 1, ISETP.EQ.U32.AND 1, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0961 | test | `long long (a != b) ? a : b` | 15 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0962 | test | `long long (a != b) && (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0963 | test | `long long (a != b) \|\| (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, ISETP.NE.OR.EX 1, ISETP.NE.U32.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0964 | test | `long long (a < b) ? a : b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LT.AND.EX 1, ISETP.LT.U32.AND 1, LDG.E.64 2, S2R 2, SEL 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0965 | test | `long long (a < b) && (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LT.AND.EX 1, ISETP.LT.U32.AND 1, ISETP.NE.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0966 | test | `long long (a < b) \|\| (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LT.OR.EX 1, ISETP.LT.U32.AND 1, ISETP.NE.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0967 | test | `long long (a > b) ? a : b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND.EX 1, ISETP.GT.U32.AND 1, LDG.E.64 2, S2R 2, SEL 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0968 | test | `long long (a > b) && (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND.EX 1, ISETP.GT.U32.AND 1, ISETP.NE.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0969 | test | `long long (a > b) \|\| (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.OR.EX 1, ISETP.GT.U32.AND 1, ISETP.NE.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0970 | test | `long long (a <= b) ? a : b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LT.AND.EX 1, ISETP.LT.U32.AND 1, LDG.E.64 2, S2R 2, SEL 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0971 | test | `long long (a <= b) && (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LE.AND.EX 1, ISETP.LE.U32.AND 1, ISETP.NE.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0972 | test | `long long (a <= b) \|\| (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LE.OR.EX 1, ISETP.LE.U32.AND 1, ISETP.NE.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0973 | test | `long long (a >= b) ? a : b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.AND.EX 1, ISETP.GT.U32.AND 1, LDG.E.64 2, S2R 2, SEL 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0974 | test | `long long (a >= b) && (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.AND.EX 1, ISETP.GE.U32.AND 2, ISETP.NE.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0975 | test | `long long (a >= b) \|\| (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.OR.EX 1, ISETP.GE.U32.AND 2, ISETP.NE.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0976 | test | `long long a == 0` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0977 | test | `long long a != 0` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0978 | test | `long long (a != 0) && (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0979 | test | `long long (a != 0) \|\| (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, ISETP.NE.OR.EX 1, ISETP.NE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0980 | test | `long long a < 0` | 17 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 1, MOV 3, S2R 2, SHF.L.U32 1, SHF.R.U32.HI 1, STG.E.64 1, ULDC.64 1 |
| 0981 | test | `unsigned long long (a == b) ? a : b` | 15 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0982 | test | `unsigned long long (a == b) && (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.EQ.AND.EX 1, ISETP.EQ.U32.AND 1, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0983 | test | `unsigned long long (a == b) \|\| (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.EQ.OR.EX 1, ISETP.EQ.U32.AND 1, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0984 | test | `unsigned long long (a != b) ? a : b` | 15 | BRA 1, EXIT 2, IMAD 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0985 | test | `unsigned long long (a != b) && (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0986 | test | `unsigned long long (a != b) \|\| (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, ISETP.NE.OR.EX 1, ISETP.NE.U32.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0987 | test | `unsigned long long (a < b) ? a : b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LT.U32.AND 1, ISETP.LT.U32.AND.EX 1, LDG.E.64 2, S2R 2, SEL 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0988 | test | `unsigned long long (a < b) && (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LT.U32.AND 1, ISETP.LT.U32.AND.EX 1, ISETP.NE.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0989 | test | `unsigned long long (a < b) \|\| (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LT.U32.AND 1, ISETP.LT.U32.OR.EX 1, ISETP.NE.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0990 | test | `unsigned long long (a > b) ? a : b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, ISETP.GT.U32.AND.EX 1, LDG.E.64 2, S2R 2, SEL 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0991 | test | `unsigned long long (a > b) && (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, ISETP.GT.U32.AND.EX 1, ISETP.NE.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0992 | test | `unsigned long long (a > b) \|\| (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, ISETP.GT.U32.OR.EX 1, ISETP.NE.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0993 | test | `unsigned long long (a <= b) ? a : b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LT.U32.AND 1, ISETP.LT.U32.AND.EX 1, LDG.E.64 2, S2R 2, SEL 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0994 | test | `unsigned long long (a <= b) && (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LE.U32.AND 1, ISETP.LE.U32.AND.EX 1, ISETP.NE.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0995 | test | `unsigned long long (a <= b) \|\| (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.LE.U32.AND 1, ISETP.LE.U32.OR.EX 1, ISETP.NE.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0996 | test | `unsigned long long (a >= b) ? a : b` | 20 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.GT.U32.AND 1, ISETP.GT.U32.AND.EX 1, LDG.E.64 2, S2R 2, SEL 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 0997 | test | `unsigned long long (a >= b) && (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, ISETP.GE.U32.AND.EX 1, ISETP.NE.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0998 | test | `unsigned long long (a >= b) \|\| (c != 0)` | 22 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 2, ISETP.GE.U32.OR.EX 1, ISETP.NE.AND 1, LDG.E 1, LDG.E.64 2, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 0999 | test | `unsigned long long a == 0` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 1000 | test | `unsigned long long a != 0` | 19 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 1, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E.64 1, MOV 2, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 1001 | test | `unsigned long long (a != 0) && (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, ISETP.NE.AND.EX 1, ISETP.NE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 1002 | test | `unsigned long long (a != 0) \|\| (c != 0)` | 21 | BRA 1, EXIT 2, IMAD 1, IMAD.MOV.U32 2, IMAD.SHL.U32 1, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, ISETP.NE.AND 1, ISETP.NE.OR.EX 1, ISETP.NE.U32.AND 1, LDG.E 1, LDG.E.64 1, MOV 1, S2R 2, SEL 1, STG.E.64 1, ULDC.64 1 |
| 1003 | operator | `int a * b + c` | 19 | BRA 1, EXIT 2, IMAD 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 3, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 1004 | operator | `unsigned int a * b + c` | 19 | BRA 1, EXIT 2, IMAD 2, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 3, MOV 3, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1 |
| 1005 | operator | `(unsigned long long)a * b + c` | 20 | BRA 1, EXIT 2, IADD3 1, IMAD 1, IMAD.WIDE.U32 3, ISETP.GE.U32.AND 1, LDG.E 2, LDG.E.64 1, MOV 2, S2R 2, SHF.L.U32 1, STG.E.64 1, ULDC.64 1, UMOV 1 |
| 1006 | pressure | `8 values over 8 rounds` | 93 | BRA 1, EXIT 2, IADD3 8, IMAD 59, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LEA 6, LOP3.LUT 4, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 1007 | pressure | `16 values over 8 rounds` | 169 | BRA 1, EXIT 2, IADD3 16, IMAD 115, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LEA 14, LOP3.LUT 8, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 1008 | pressure | `32 values over 8 rounds` | 321 | BRA 1, EXIT 2, IADD3 32, IMAD 227, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LEA 30, LOP3.LUT 16, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 1009 | pressure | `48 values over 8 rounds` | 473 | BRA 1, EXIT 2, IADD3 48, IMAD 339, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LEA 46, LOP3.LUT 24, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 1010 | pressure | `64 values over 8 rounds` | 625 | BRA 1, EXIT 2, IADD3 64, IMAD 451, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LEA 62, LOP3.LUT 32, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 1011 | pressure | `96 values over 8 rounds` | 929 | BRA 1, EXIT 2, IADD3 96, IMAD 675, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LEA 94, LOP3.LUT 48, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 1012 | pressure | `128 values over 8 rounds` | 1234 | BRA 1, EXIT 2, IADD3 128, IMAD 899, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LEA 126, LOP3.LUT 64, MOV 3, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 1013 | pressure | `160 values over 8 rounds` | 1695 | BRA 1, EXIT 2, IADD3 318, IMAD 1123, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LEA 158, LOP3.LUT 80, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 1014 | pressure | `192 values over 8 rounds` | 2031 | BRA 1, EXIT 2, IADD3 382, IMAD 1347, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LEA 190, LOP3.LUT 96, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
| 1015 | pressure | `224 values over 8 rounds` | 2367 | BRA 1, EXIT 2, IADD3 446, IMAD 1571, IMAD.WIDE.U32 2, ISETP.GE.U32.AND 1, LDG.E 2, LEA 222, LOP3.LUT 112, MOV 2, S2R 2, SHF.L.U32 1, SHF.R.S32.HI 1, STG.E.64 1, ULDC.64 1 |
