/* 3b2_cpu.c: AT&T 3B2 Model 400 CPU (WE32100) Implementation

   Copyright (c) 2015, Seth J. Morabito

   Permission is hereby granted, free of charge, to any person obtaining a
   copy of this software and associated documentation files (the "Software"),
   to deal in the Software without restriction, including without limitation
   the rights to use, copy, modify, merge, publish, distribute, sublicense,
   and/or sell copies of the Software, and to permit persons to whom the
   Software is furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in
   all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
   ROBERT M SUPNIK BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of the author shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from the author.
*/

#include <assert.h>

#include "3b2_cpu.h"
#include "rom_400_bin.h"

#define MAX_SUB_RETURN_SKIP 9

/* RO memory. */
uint32 *ROM = NULL;

/* Main memory. */
uint32 *RAM = NULL;

/* Circular buffer of instructions */
instr *INST;
uint32 cpu_history_size;
uint32 cpu_hist_t;
uint32 cpu_hist_h;
instr *last_instruction = NULL;

t_bool cpu_in_wait = FALSE;

/* Register data */
uint32 R[16];

/* Other global CPU state */
t_bool cpu_trap      = FALSE;  /* Trap occured */
t_bool cpu_exception = FALSE;  /* Fault occured */
int8   cpu_dtype     = -1;     /* Default datatype for the current
                                  instruction */
int8   cpu_etype     = -1;     /* Currently set expanded datatype */

int16  cpu_irq_ipl   = -1;     /* If set, the IRQ level */
uint8  cpu_irq_id    = 0;      /* IRQ ID */
t_bool cpu_nmi       = FALSE;  /* If set, there has been an NMI */

uint8  cpu_ilen      = 0;      /* Length (in bytes) of instruction
                                  currently being executed */
t_bool cpu_ex_halt   = FALSE;  /* Flag to halt on exceptions / traps */

BITFIELD psw_bits[] = {
    BITFFMT(ET,2,%d),    /* Exception Type */
    BIT(TM),             /* Trace Mask */
    BITFFMT(ISC,4,%d),   /* Internal State Code */
    BIT(I),              /* Register Initial Context (I) */
    BIT(R),              /* Register Initial Context (R) */
    BITFFMT(PM,2,%d),    /* Previous Execution Level */
    BITFFMT(CM,2,%d),    /* Current Execution Level */
    BITFFMT(IPL,4,%d),   /* Interrupt Priority Level */
    BIT(TE),             /* Trace Enable */
    BIT(C),              /* Carry */
    BIT(V),              /* Overflow */
    BIT(Z),              /* Zero */
    BIT(N),              /* Negative */
    BIT(OE),             /* Enable Overflow Trap */
    BIT(CD),             /* Cache Disable */
    BIT(QIE),            /* Quick-Interrupt Enable */
    BIT(CFD),            /* Cache Flush Disable */
    BITNCF(6),           /* Unused */
    ENDBITS
};

/* Registers. */
REG cpu_reg[] = {
    { HRDATAD  (PC,   R[NUM_PC],   32, "Program Counter") },
    { HRDATAD  (R0,   R[0],        32, "General purpose register 0") },
    { HRDATAD  (R1,   R[1],        32, "General purpose register 1") },
    { HRDATAD  (R2,   R[2],        32, "General purpose register 2") },
    { HRDATAD  (R3,   R[3],        32, "General purpose register 3") },
    { HRDATAD  (R4,   R[4],        32, "General purpose register 4") },
    { HRDATAD  (R5,   R[5],        32, "General purpose register 5") },
    { HRDATAD  (R6,   R[6],        32, "General purpose register 6") },
    { HRDATAD  (R7,   R[7],        32, "General purpose register 7") },
    { HRDATAD  (R8,   R[8],        32, "General purpose register 8") },
    { HRDATAD  (FP,   R[NUM_FP],   32, "Frame Pointer") },
    { HRDATAD  (AP,   R[NUM_AP],   32, "Argument Pointer") },
    { HRDATADF (PSW,  R[NUM_PSW],  32, "Processor Status Word", psw_bits) },
    { HRDATAD  (SP,   R[NUM_SP],   32, "Stack Pointer") },
    { HRDATAD  (PCBP, R[NUM_PCBP], 32, "Process Control Block Pointer") },
    { HRDATAD  (ISP,  R[NUM_ISP],  32, "Interrupt Stack Pointer") },
    { NULL }
};

static DEBTAB cpu_deb_tab[] = {
    { "READ",       READ_MSG,       "Memory read activity"  },
    { "WRITE",      WRITE_MSG,      "Memory write activity" },
    { "DECODE",     DECODE_MSG,     "Instruction decode"    },
    { "EXECUTE",    EXECUTE_MSG,    "Instruction execute"   },
    { "INIT",       INIT_MSG,       "Initialization"        },
    { "IRQ",        IRQ_MSG,        "Interrupt Handling"    },
    { "IO",         IO_D_MSG,       "I/O Dispatch"          },
    { NULL,         0                                       }
};

UNIT cpu_unit = { UDATA (NULL, UNIT_FIX|UNIT_BINK, MAXMEMSIZE) };

#define UNIT_V_EXHALT   (UNIT_V_UF + 0)                 /* halt to console */
#define UNIT_EXHALT     (1u << UNIT_V_EXHALT)

MTAB cpu_mod[] = {
    { UNIT_MSIZE, (1u << 20), NULL, "1M",
      &cpu_set_size, NULL, NULL, "Set Memory to 1M bytes" },
    { UNIT_MSIZE, (1u << 21), NULL, "2M",
      &cpu_set_size, NULL, NULL, "Set Memory to 2M bytes" },
    { UNIT_MSIZE, (1u << 22), NULL, "4M",
      &cpu_set_size, NULL, NULL, "Set Memory to 4M bytes" },
    { MTAB_XTD|MTAB_VDV|MTAB_NMO|MTAB_SHP, 0, "HISTORY", "HISTORY",
      &cpu_set_hist, &cpu_show_hist, NULL, "Displays instruction history" },
    { UNIT_EXHALT, UNIT_EXHALT, "Halt on Exception", "EX_HALT",
      NULL, NULL, NULL, "Enables Halt on exceptions and traps" },
    { UNIT_EXHALT, 0, "No halt on exception", "NOEX_HALT",
      NULL, NULL, NULL, "Disables Halt on exceptions and traps" },
    { 0 }
};

DEVICE cpu_dev = {
    "CPU",               /* Name */
    &cpu_unit,           /* Units */
    cpu_reg,             /* Registers */
    cpu_mod,             /* Modifiers */
    1,                   /* Number of Units */
    16,                  /* Address radix */
    32,                  /* Address width */
    1,                   /* Addr increment */
    16,                  /* Data radix */
    8,                   /* Data width */
    &cpu_ex,             /* Examine routine */
    &cpu_dep,            /* Deposit routine */
    &cpu_reset,          /* Reset routine */
    &cpu_boot,           /* Boot routine */
    NULL,                /* Attach routine */
    NULL,                /* Detach routine */
    NULL,                /* Context */
    DEV_DYNM|DEV_DEBUG,  /* Flags */
    0,                   /* Debug control flags */
    cpu_deb_tab,         /* Debug flag names */
    &cpu_set_size,       /* Memory size change */
    NULL                 /* Logical names */
};

#define HWORD_OP_COUNT 11

mnemonic hword_ops[HWORD_OP_COUNT] = {
    {0x3009, 0, OP_NONE, NA, "MVERNO",  -1, -1, -1, -1},
    {0x300d, 0, OP_NONE, NA, "ENBVJMP", -1, -1, -1, -1},
    {0x3013, 0, OP_NONE, NA, "DISVJMP", -1, -1, -1, -1},
    {0x3019, 0, OP_NONE, NA, "MOVBLW",  -1, -1, -1, -1},
    {0x301f, 0, OP_NONE, NA, "STREND",  -1, -1, -1, -1},
    {0x302f, 1, OP_DESC, WD, "INTACK",  -1, -1, -1, -1},
    {0x3035, 0, OP_NONE, NA, "STRCPY",  -1, -1, -1, -1},
    {0x3045, 0, OP_NONE, NA, "RETG",    -1, -1, -1, -1},
    {0x3061, 0, OP_NONE, NA, "GATE",    -1, -1, -1, -1},
    {0x30ac, 0, OP_NONE, NA, "CALLPS",  -1, -1, -1, -1},
    {0x30c8, 0, OP_NONE, NA, "RETPS",   -1, -1, -1, -1}
};

/* Lookup table of operand types. */
mnemonic ops[256] = {
    {0x00, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x01, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x02,  2, OP_COPR, WD, "SPOPRD", -1, -1, -1, -1},
    {0x03,  3, OP_COPR, WD, "SPOPD2", -1, -1, -1, -1},
    {0x04,  2, OP_DESC, WD, "MOVAW",   0, -1, -1,  1},
    {0x05, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x06,  2, OP_COPR, WD, "SPOPRT", -1, -1, -1, -1},
    {0x07,  3, OP_COPR, WD, "SPOPT2", -1, -1, -1, -1},
    {0x08,  0, OP_NONE, NA, "RET",    -1, -1, -1, -1},
    {0x09, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x0a, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x0b, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x0c,  2, OP_DESC, WD, "MOVTRW",  0, -1, -1,  1},
    {0x0d, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x0e, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x0f, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x10,  1, OP_DESC, WD, "SAVE",    0, -1, -1, -1},
    {0x11, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x12, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x13,  2, OP_COPR, WD, "SPOPWD", -1, -1, -1, -1},
    {0x14,  1, OP_BYTE, NA, "EXTOP",  -1, -1, -1, -1},
    {0x15, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x16, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x17,  2, OP_COPR, WD, "SPOPWT", -1, -1, -1, -1},
    {0x18,  1, OP_DESC, WD, "RESTORE", 0, -1, -1, -1},
    {0x19, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x1a, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x1b, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x1c,  1, OP_DESC, WD, "SWAPWI", -1, -1, -1,  0}, /* 3-122 252 */
    {0x1d, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x1e,  1, OP_DESC, HW, "SWAPHI", -1, -1, -1,  0}, /* 3-122 252 */
    {0x1f,  1, OP_DESC, BT, "SWAPBI", -1, -1, -1,  0}, /* 3-122 252 */
    {0x20,  1, OP_DESC, WD, "POPW",   -1, -1, -1,  0},
    {0x21, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x22,  2, OP_COPR, WD, "SPOPRS", -1, -1, -1, -1},
    {0x23,  3, OP_COPR, WD, "SPOPS2", -1, -1, -1, -1},
    {0x24,  1, OP_DESC, NA, "JMP",    -1, -1, -1,  0},
    {0x25, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x26, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x27,  0, OP_NONE, NA, "CFLUSH", -1, -1, -1, -1},
    {0x28,  1, OP_DESC, WD, "TSTW",    0, -1, -1, -1},
    {0x29, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x2a,  1, OP_DESC, HW, "TSTH",    0, -1, -1, -1},
    {0x2b,  1, OP_DESC, BT, "TSTB",    0, -1, -1, -1},
    {0x2c,  2, OP_DESC, WD, "CALL",    0, -1, -1,  1},
    {0x2d, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x2e,  0, OP_NONE, NA, "BPT",    -1, -1, -1, -1},
    {0x2f,  0, OP_NONE, NA, "WAIT",   -1, -1, -1, -1},
    {0x30, -1, OP_NONE, NA, "???",    -1, -1, -1, -1}, /* Two-byte instructions */
    {0x31, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x32,  1, OP_COPR, WD, "SPOP",   -1, -1, -1, -1},
    {0x33,  2, OP_COPR, WD, "SPOPWS", -1, -1, -1, -1},
    {0x34,  1, OP_DESC, WD, "JSB",    -1, -1, -1,  0},
    {0x35, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x36,  1, OP_HALF, NA, "BSBH",   -1, -1, -1,  0},
    {0x37,  1, OP_BYTE, NA, "BSBB",   -1, -1, -1,  0},
    {0x38,  2, OP_DESC, WD, "BITW",    0,  1, -1, -1},
    {0x39, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x3a,  2, OP_DESC, HW, "BITH",    0,  1, -1, -1},
    {0x3b,  2, OP_DESC, BT, "BITB",    0,  1, -1, -1},
    {0x3c,  2, OP_DESC, WD, "CMPW",    0,  1, -1, -1},
    {0x3d, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x3e,  2, OP_DESC, HW, "CMPH",    0,  1, -1, -1},
    {0x3f,  2, OP_DESC, BT, "CMPB",    0,  1, -1, -1},
    {0x40,  0, OP_NONE, NA, "RGEQ",   -1, -1, -1, -1},
    {0x41, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x42,  1, OP_HALF, NA, "BGEH",   -1, -1, -1,  0},
    {0x43,  1, OP_BYTE, NA, "BGEB",   -1, -1, -1,  0},
    {0x44,  0, OP_NONE, NA, "RGTR",   -1, -1, -1, -1},
    {0x45, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x46,  1, OP_HALF, NA, "BGH",    -1, -1, -1,  0},
    {0x47,  1, OP_BYTE, NA, "BGB",    -1, -1, -1,  0},
    {0x48,  0, OP_NONE, NA, "BLSS",   -1, -1, -1,  0},
    {0x49, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x4a,  1, OP_HALF, NA, "BLH",    -1, -1, -1,  0},
    {0x4b,  1, OP_BYTE, NA, "BLB",    -1, -1, -1,  0},
    {0x4c,  0, OP_NONE, NA, "RLEQ",   -1, -1, -1, -1},
    {0x4d, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x4e,  1, OP_HALF, NA, "BLEH",   -1, -1, -1,  0},
    {0x4f,  1, OP_BYTE, NA, "BLEB",   -1, -1, -1,  0},
    {0x50,  0, OP_NONE, NA, "BGEQU",  -1, -1, -1,  0}, /* aka BCC */
    {0x51, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x52,  1, OP_HALF, NA, "BGEUH",  -1, -1, -1,  0}, /* aka BCCH */
    {0x53,  1, OP_BYTE, NA, "BGEUB",  -1, -1, -1,  0}, /* aka BCCB */
    {0x54,  0, OP_NONE, NA, "RGTRU",  -1, -1, -1, -1},
    {0x55, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x56,  1, OP_HALF, NA, "BGUH",   -1, -1, -1,  0},
    {0x57,  1, OP_BYTE, NA, "BGUB",   -1, -1, -1,  0},
    {0x58,  0, OP_NONE, NA, "BLSSU",  -1, -1, -1,  0}, /* aka BCS */
    {0x59, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x5a,  1, OP_HALF, NA, "BLUH",   -1, -1, -1,  0}, /* aka BCSH */
    {0x5b,  1, OP_BYTE, NA, "BLUB",   -1, -1, -1,  0}, /* aka BCSB */
    {0x5c,  0, OP_NONE, NA, "RLEQU",  -1, -1, -1, -1},
    {0x5d, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x5e,  1, OP_HALF, NA, "BLEUH",  -1, -1, -1,  0},
    {0x5f,  1, OP_BYTE, NA, "BLEUB",  -1, -1, -1,  0},
    {0x60,  0, OP_NONE, NA, "RVC",    -1, -1, -1, -1},
    {0x61, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x62,  1, OP_HALF, NA, "BVCH",   -1, -1, -1,  0},
    {0x63,  1, OP_BYTE, NA, "BVCB",   -1, -1, -1,  0},
    {0x64,  0, OP_NONE, NA, "RNEQU",  -1, -1, -1, -1},
    {0x65, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x66,  1, OP_HALF, NA, "BNEH",   -1, -1, -1,  0}, /* duplicate of 76 */
    {0x67,  1, OP_BYTE, NA, "BNEB",   -1, -1, -1,  0}, /* duplicate of 77*/
    {0x68,  0, OP_NONE, NA, "RVS",    -1, -1, -1, -1},
    {0x69, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x6a,  1, OP_HALF, NA, "BVSH",   -1, -1, -1,  0},
    {0x6b,  1, OP_BYTE, NA, "BVSB",   -1, -1, -1,  0},
    {0x6c,  0, OP_NONE, NA, "REQLU",  -1, -1, -1, -1},
    {0x6d, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x6e,  1, OP_HALF, NA, "BEH",    -1, -1, -1,  0}, /* duplicate of 7e */
    {0x6f,  1, OP_BYTE, NA, "BEB",    -1, -1, -1,  0}, /* duplicate of 7f */
    {0x70,  0, OP_NONE, NA, "NOP",    -1, -1, -1, -1},
    {0x71, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x72,  0, OP_NONE, NA, "NOP3",   -1, -1, -1, -1},
    {0x73,  0, OP_NONE, NA, "NOP2",   -1, -1, -1, -1},
    {0x74,  0, OP_NONE, NA, "RNEQ",   -1, -1, -1, -1},
    {0x75, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x76,  1, OP_HALF, NA, "BNEH",   -1, -1, -1,  0}, /* duplicate of 66 */
    {0x77,  1, OP_BYTE, NA, "BNEB",   -1, -1, -1,  0}, /* duplicate of 67 */
    {0x78,  0, OP_NONE, NA, "RSB",    -1, -1, -1, -1},
    {0x79, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x7a,  1, OP_HALF, NA, "BRH",    -1, -1, -1,  0},
    {0x7b,  1, OP_BYTE, NA, "BRB",    -1, -1, -1,  0},
    {0x7c,  0, OP_NONE, NA, "REQL",   -1, -1, -1, -1},
    {0x7d, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x7e,  1, OP_HALF, NA, "BEH",    -1, -1, -1,  0}, /* duplicate of 6e */
    {0x7f,  1, OP_BYTE, NA, "BEB",    -1, -1, -1,  0}, /* duplicate of 6f */
    {0x80,  1, OP_DESC, WD, "CLRW",   -1, -1, -1,  0},
    {0x81, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x82,  1, OP_DESC, HW, "CLRH",   -1, -1, -1,  0},
    {0x83,  1, OP_DESC, BT, "CLRB",   -1, -1, -1,  0},
    {0x84,  2, OP_DESC, WD, "MOVW",    0, -1, -1,  1},
    {0x85, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x86,  2, OP_DESC, HW, "MOVH",    0, -1, -1,  1},
    {0x87,  2, OP_DESC, BT, "MOVB",    0, -1, -1,  1},
    {0x88,  2, OP_DESC, WD, "MCOMW",   0, -1, -1,  1},
    {0x89, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x8a,  2, OP_DESC, HW, "MCOMH",   0, -1, -1,  1},
    {0x8b,  2, OP_DESC, BT, "MCOMB",   0, -1, -1,  1},
    {0x8c,  2, OP_DESC, WD, "MNEGW",   0, -1, -1,  1},
    {0x8d, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x8e,  2, OP_DESC, HW, "MNEGH",   0, -1, -1,  1},
    {0x8f,  2, OP_DESC, BT, "MNEGB",   0, -1, -1,  1},
    {0x90,  1, OP_DESC, WD, "INCW",   -1, -1, -1,  0},
    {0x91, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x92,  1, OP_DESC, HW, "INCH",   -1, -1, -1,  0},
    {0x93,  1, OP_DESC, BT, "INCB",   -1, -1, -1,  0},
    {0x94,  1, OP_DESC, WD, "DECW",   -1, -1, -1,  0},
    {0x95, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x96,  1, OP_DESC, HW, "DECH",   -1, -1, -1,  0},
    {0x97,  1, OP_DESC, BT, "DECB",   -1, -1, -1,  0},
    {0x98, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x99, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x9a, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x9b, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x9c,  2, OP_DESC, WD, "ADDW2",   0, -1, -1,  1},
    {0x9d, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0x9e,  2, OP_DESC, HW, "ADDH2",   0, -1, -1,  1},
    {0x9f,  2, OP_DESC, BT, "ADDB2",   0, -1, -1,  1},
    {0xa0,  1, OP_DESC, WD, "PUSHW",   0, -1, -1, -1},
    {0xa1, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xa2, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xa3, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xa4,  2, OP_DESC, WD, "MODW2",   0, -1, -1,  1},
    {0xa5, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xa6,  2, OP_DESC, HW, "MODH2",   0, -1, -1,  1},
    {0xa7,  2, OP_DESC, BT, "MODB2",   0, -1, -1,  1},
    {0xa8,  2, OP_DESC, WD, "MULW2",   0, -1, -1,  1},
    {0xa9, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xaa,  2, OP_DESC, HW, "MULH2",   0, -1, -1,  1},
    {0xab,  2, OP_DESC, BT, "MULB2",   0, -1, -1,  1},
    {0xac,  2, OP_DESC, WD, "DIVW2",   0, -1, -1,  1},
    {0xad, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xae,  2, OP_DESC, HW, "DIVH2",   0, -1, -1,  1},
    {0xaf,  2, OP_DESC, BT, "DIVB2",   0, -1, -1,  1},
    {0xb0,  2, OP_DESC, WD, "ORW2",    0, -1, -1,  1},
    {0xb1, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xb2,  2, OP_DESC, HW, "ORH2",    0, -1, -1,  1},
    {0xb3,  2, OP_DESC, BT, "ORB2",    0, -1, -1,  1},
    {0xb4,  2, OP_DESC, WD, "XORW2",   0, -1, -1,  1},
    {0xb5, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xb6,  2, OP_DESC, HW, "XORH2",   0, -1, -1,  1},
    {0xb7,  2, OP_DESC, BT, "XORB2",   0, -1, -1,  1},
    {0xb8,  2, OP_DESC, WD, "ANDW2",   0, -1, -1,  1},
    {0xb9, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xba,  2, OP_DESC, HW, "ANDH2",   0, -1, -1,  1},
    {0xbb,  2, OP_DESC, BT, "ANDB2",   0, -1, -1,  1},
    {0xbc,  2, OP_DESC, WD, "SUBW2",   0, -1, -1,  1},
    {0xbd, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xbe,  2, OP_DESC, HW, "SUBH2",   0, -1, -1,  1},
    {0xbf,  2, OP_DESC, BT, "SUBB2",   0, -1, -1,  1},
    {0xc0,  3, OP_DESC, WD, "ALSW3",   0,  1, -1,  2},
    {0xc1, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xc2, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xc3, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xc4,  3, OP_DESC, WD, "ARSW3",   0,  1, -1,  2},
    {0xc5, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xc6,  3, OP_DESC, HW, "ARSH3",   0,  1, -1,  2},
    {0xc7,  3, OP_DESC, BT, "ARSB3",   0,  1, -1,  2},
    {0xc8,  4, OP_DESC, WD, "INSFW",   0,  1,  2,  3},
    {0xc9, -1, OP_DESC, NA, "???",    -1, -1, -1, -1},
    {0xca,  4, OP_DESC, HW, "INSFH",   0,  1,  2,  3},
    {0xcb,  4, OP_DESC, BT, "INSFB",   0,  1,  2,  3},
    {0xcc,  4, OP_DESC, WD, "EXTFW",   0,  1,  2,  3},
    {0xcd, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xce,  4, OP_DESC, HW, "EXTFH",   0,  1,  2,  3},
    {0xcf,  4, OP_DESC, BT, "EXTFB",   0,  1,  2,  3},
    {0xd0,  3, OP_DESC, WD, "LLSW3",   0,  1, -1,  2},
    {0xd1, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xd2,  3, OP_DESC, HW, "LLSH3",   0,  1, -1,  2},
    {0xd3,  3, OP_DESC, BT, "LLSB3",   0,  1, -1,  2},
    {0xd4,  3, OP_DESC, WD, "LRSW3",   0,  1, -1,  2},
    {0xd5, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xd6, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xd7, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xd8,  3, OP_DESC, WD, "ROTW",    0,  1, -1,  2}, /* 3-108 238 */
    {0xd9, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xda, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xdb, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xdc,  3, OP_DESC, WD, "ADDW3",   0,  1, -1,  2},
    {0xdd, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xde,  3, OP_DESC, HW, "ADDH3",   0,  1, -1,  2},
    {0xdf,  3, OP_DESC, BT, "ADDB3",   0,  1, -1,  2},
    {0xe0,  1, OP_DESC, WD, "PUSHAW",  0, -1, -1, -1},
    {0xe1, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xe2, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xe3, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xe4,  3, OP_DESC, WD, "MODW3",   0,  1, -1,  2},
    {0xe5, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xe6,  3, OP_DESC, HW, "MODH3",   0,  1, -1,  2},
    {0xe7,  3, OP_DESC, BT, "MODB3",   0,  1, -1,  2},
    {0xe8,  3, OP_DESC, WD, "MULW3",   0,  1, -1,  2},
    {0xe9, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xea,  3, OP_DESC, HW, "MULH3",   0,  1, -1,  2},
    {0xeb,  3, OP_DESC, BT, "MULB3",   0,  1, -1,  2},
    {0xec,  3, OP_DESC, WD, "DIVW3",   0,  1, -1,  2},
    {0xed, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xee,  3, OP_DESC, HW, "DIVH3",   0,  1, -1,  2},
    {0xef,  3, OP_DESC, BT, "DIVB3",   0,  1, -1,  2},
    {0xf0,  3, OP_DESC, WD, "ORW3",    0,  1, -1,  2},
    {0xf1, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xf2,  3, OP_DESC, HW, "ORH3",    0,  1, -1,  2},
    {0xf3,  3, OP_DESC, BT, "ORB3",    0,  1, -1,  2},
    {0xf4,  3, OP_DESC, WD, "XORW3",   0,  1, -1,  2},
    {0xf5, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xf6,  3, OP_DESC, HW, "XORH3",   0,  1, -1,  2},
    {0xf7,  3, OP_DESC, BT, "XORB3",   0,  1, -1,  2},
    {0xf8,  3, OP_DESC, WD, "ANDW3",   0,  1, -1,  2},
    {0xf9, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xfa,  3, OP_DESC, HW, "ANDH3",   0,  1, -1,  2},
    {0xfb,  3, OP_DESC, BT, "ANDB3",   0,  1, -1,  2},
    {0xfc,  3, OP_DESC, WD, "SUBW3",   0,  1, -1,  2},
    {0xfd, -1, OP_NONE, NA, "???",    -1, -1, -1, -1},
    {0xfe,  3, OP_DESC, HW, "SUBH3",   0,  1, -1,  2},
    {0xff,  3, OP_DESC, BT, "SUBB3",   0,  1, -1,  2}
};

/* from MAME (src/devices/cpu/m68000/m68kcpu.c) */
const uint8 m68ki_shift_8_table[65] =
{
    0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff
};
const uint16 m68ki_shift_16_table[65] =
{
    0x0000, 0x8000, 0xc000, 0xe000, 0xf000, 0xf800, 0xfc00, 0xfe00, 0xff00,
    0xff80, 0xffc0, 0xffe0, 0xfff0, 0xfff8, 0xfffc, 0xfffe, 0xffff, 0xffff,
    0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
    0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
    0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
    0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
    0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff,
    0xffff, 0xffff
};
const uint32 m68ki_shift_32_table[65] =
{
    0x00000000, 0x80000000, 0xc0000000, 0xe0000000, 0xf0000000, 0xf8000000,
    0xfc000000, 0xfe000000, 0xff000000, 0xff800000, 0xffc00000, 0xffe00000,
    0xfff00000, 0xfff80000, 0xfffc0000, 0xfffe0000, 0xffff0000, 0xffff8000,
    0xffffc000, 0xffffe000, 0xfffff000, 0xfffff800, 0xfffffc00, 0xfffffe00,
    0xffffff00, 0xffffff80, 0xffffffc0, 0xffffffe0, 0xfffffff0, 0xfffffff8,
    0xfffffffc, 0xfffffffe, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
    0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff
};

void cpu_load_rom()
{
    uint32 i, index, sc, mask, val;

    if (ROM == NULL) {
        return;
    }

    for (i = 0; i < BOOT_CODE_SIZE; i++) {
        val = BOOT_CODE_ARRAY[i];
        sc = (~(i & 3) << 3) & 0x1f;
        mask = 0xffu << sc;
        index = i >> 2;

        ROM[index] = (ROM[index] & ~mask) | (val << sc);
    }
}

t_stat cpu_boot(int32 unit_num, DEVICE *dptr)
{
    /*
     *  page 2-52 (pdf page 85)
     *
     *  1. Change to physical address mode
     *  2. Fetch the word at physical address 0x80 and store it in
     *     the PCBP register.
     *  3. Fetch the word at the PCB address and store it in the
     *     PSW.
     *  4. Fetch the word at PCB address + 4 bytes and store it
     *     in the PC.
     *  5. Fetch the word at PCB address + 8 bytes and store it
     *     in the SP.
     *  6. Fetch the word at PCB address + 12 bytes and store it
     *     in the PCB, if bit I in PSW is set.
     */

    mmu_disable();

    cpu_load_rom();

    R[NUM_PCBP] = pread_w(0x80);
    sim_debug(INIT_MSG, &cpu_dev, "Setting initial PCBP: %08x\n", R[NUM_PCBP]);

    R[NUM_PSW] = pread_w(R[NUM_PCBP]);
    sim_debug(INIT_MSG, &cpu_dev, "Setting initial PSW: %08x\n", R[NUM_PSW]);

    R[NUM_PC] = pread_w(R[NUM_PCBP] + 4);
    sim_debug(INIT_MSG, &cpu_dev, "Setting initial PC: %08x\n", R[NUM_PC]);

    R[NUM_SP] = pread_w(R[NUM_PCBP] + 8);
    sim_debug(INIT_MSG, &cpu_dev, "Setting initial SP: %08x\n", R[NUM_SP]);

    if (R[NUM_PSW] & PSW_I_MASK) {
        R[NUM_PSW] &= ~PSW_I_MASK;
        R[NUM_PCBP] = pread_w(R[NUM_PCBP] + 12);
        sim_debug(INIT_MSG, &cpu_dev, "Setting new initial PCBP: %08x\n", R[NUM_PCBP]);
    }

    /* set ISC to External Reset */
    R[NUM_PSW] &= ~PSW_ISC_MASK;
    R[NUM_PSW] |= 3 << PSW_ISC ;

    return SCPE_OK;
}

t_stat cpu_ex(t_value *vptr, t_addr addr, UNIT *uptr, int32 sw)
{
    uint32 uaddr = (uint32) addr;

    if (vptr == NULL) {
        return SCPE_ARG;
    }

    if (sw & EX_V_FLAG) {
        *vptr = (uint32) read_b(uaddr);
    } else {
        if (!(addr_is_rom(uaddr) || addr_is_mem(uaddr) || addr_is_io(uaddr))) {
            return SCPE_NXM;
        }

        *vptr = (uint32) pread_b(uaddr);
    }

    return SCPE_OK;
}

t_stat cpu_dep(t_value val, t_addr addr, UNIT *uptr, int32 sw)
{
    uint32 uaddr = (uint32) addr;

    if (!(addr_is_rom(uaddr) || addr_is_mem(uaddr) || addr_is_io(uaddr))) {
        return SCPE_NXM;
    }

    pwrite_b(addr, (uint8) val);

    return SCPE_OK;
}

t_stat cpu_reset(DEVICE *dptr)
{
    /* Allocate memory */
    if (ROM == NULL) {
        ROM = (uint32 *) calloc(BOOT_CODE_SIZE >> 2, sizeof(uint32));
        if (ROM == NULL) {
            return SCPE_MEM;
        }

        memset(ROM, 0, BOOT_CODE_SIZE >> 2);
    }

    if (RAM == NULL) {
        RAM = (uint32 *) calloc(MEM_SIZE >> 2, sizeof(uint32));
        if (RAM == NULL) {
            return SCPE_MEM;
        }

        memset(RAM, 0, MEM_SIZE >> 2);

        sim_vm_is_subroutine_call = cpu_is_pc_a_subroutine_call;
    }

    if (INST == NULL) {
        cpu_history_size = DEF_HIST_SIZE;
        INST = (instr *) calloc(cpu_history_size, sizeof(instr));
    }

    cpu_irq_ipl = -1;
    cpu_nmi = FALSE;

    cpu_hist_t = 0;
    cpu_hist_h = 0;
    cpu_in_wait = 0;

    sim_brk_types = SWMASK('E');
    sim_brk_dflt = SWMASK('E');

    return SCPE_OK;
}

static const char *cpu_next_caveats =
"The NEXT command in this 3B2 architecture simulator currently will\n"
"enable stepping across subroutine calls which are initiated by the\n"
"JSB, CALL and CALLPS instructions.\n"
"This stepping works by dynamically establishing breakpoints at the\n"
"memory address immediately following the instruction which initiated\n"
"the subroutine call.  These dynamic breakpoints are automatically\n"
"removed once the simulator returns to the sim> prompt for any reason.\n"
"If the called routine returns somewhere other than one of these\n"
"locations due to a trap, stack unwind or any other reason, instruction\n"
"execution will continue until some other reason causes execution to stop.\n";

t_bool cpu_is_pc_a_subroutine_call (t_addr **ret_addrs)
{
    static uint32 returns[MAX_SUB_RETURN_SKIP+1] = {0};
    static t_bool caveats_displayed = FALSE;
    int i;

    if (!caveats_displayed) {
        caveats_displayed = TRUE;
        sim_printf ("%s", cpu_next_caveats);
    }

    /* get data */
    if (SCPE_OK != get_aval (R[NUM_PC], &cpu_dev, &cpu_unit)) {
        return FALSE;
    }

    switch (sim_eval[0]) {
    case JSB:
    case CALL:
    case CALLPS:
        returns[0] = R[NUM_PC] + (1 - fprint_sym(stdnul, R[NUM_PC],
                                                 sim_eval, &cpu_unit,
                                                 SWMASK ('M')));
        for (i=1; i<MAX_SUB_RETURN_SKIP; i++) {
            /* Possible skip return */
            returns[i] = returns[i-1] + 1;
        }
        returns[i] = 0;  /* Make sure the address list ends with a zero */
        *ret_addrs = returns;
        return TRUE;
    default:
        return FALSE;
    }
}

t_stat cpu_set_halt(UNIT *uptr, int32 val, char *cptr, void *desc)
{
    cpu_ex_halt = TRUE;
    return SCPE_OK;
}

t_stat cpu_clear_halt(UNIT *uptr, int32 val, char *cptr, void *desc)
{
    cpu_ex_halt = FALSE;
    return SCPE_OK;
}


t_stat cpu_set_hist(UNIT *uptr, int32 val, char *cptr, void *desc)
{
    uint32 size;
    t_stat result;
    instr *nINST = NULL;

    if (cptr) {
        size = (uint32) get_uint(cptr, 10, MAX_HIST_SIZE, &result);
        if ((result != SCPE_OK) || (size == 0)) {
            return SCPE_ARG;
        }
    } else {
        size = DEF_HIST_SIZE;
    }

    /* Allocate a new ring buffer */
    nINST = (instr *) calloc(size, sizeof(instr));

    if (nINST == NULL) {
        return SCPE_MEM;
    }

    /* Move the pointers */
    cpu_hist_h = 0;
    cpu_hist_t = 0;

    /* Free the old ring buffer */
    free(INST);

    INST = nINST;
    cpu_history_size = size;

    return SCPE_OK;
}

void fprint_sym_m(FILE *st, instr *ip)
{
    int32 i;

    fprintf(st, "%s", ip->mn->mnemonic);

    if (ip->mn->op_count > 0) {
        fputc(' ', st);
    }

    /* Show the operand mnemonics */
    for (i = 0; i < ip->mn->op_count; i++) {
        cpu_show_operand(st, &ip->operands[i]);
        if (i < ip->mn->op_count - 1) {
            fputc(',', st);
        }
    }
}

t_stat cpu_show_hist(FILE *st, UNIT *uptr, int32 val, void *desc)
{
    int32 i;
    size_t j, count;
    char *cptr = (char *) desc;
    t_stat result;
    instr *ip;

    /* 'count' is the number of history entries the user wants */

    if (cptr) {
        count = (size_t) get_uint(cptr, 10, cpu_history_size, &result);
        if ((result != SCPE_OK) || (count == 0)) {
            return SCPE_ARG;
        }
    } else {
        count = cpu_history_size;
    }

    /* Position for reading from ring buffer */
    i = cpu_hist_h;

    for (j = 0; j < count; j++) {
        if (i == cpu_hist_t) {
            break;
        }
        if (--i < 0) {
            i = cpu_history_size - 1;
        }
    }

    while (TRUE) {
        if (i == cpu_hist_h) {
            break;
        }

        ip = &INST[i];

        /* Show the opcode mnemonic */
        fprintf(st, "%08x %08x| ", ip->psw, ip->pc);

        fprint_sym_m(st, ip);

        /* Show the operand data */
        if (ip->mn->op_count > 0 && ip->mn->mode == OP_DESC) {
            fprintf(st, "\n                   ");

            for (j = 0; j < (uint32) ip->mn->op_count; j++) {
                fprintf(st, "%08x", ip->operands[j].data);
                if (j < (uint32) ip->mn->op_count - 1) {
                    fputc(' ', st);
                }
            }
        }

        fputc('\n', st);

        if (++i == cpu_history_size) {
            i = 0;
        }
    }


    return SCPE_OK;
}

void cpu_register_name(uint8 reg, char *buf, size_t len) {
    switch(reg) {
    case 9:
        snprintf(buf, len, "%%fp");
        break;
    case 10:
        snprintf(buf, len, "%%ap");
        break;
    case 11:
        snprintf(buf, len, "%%psw");
        break;
    case 12:
        snprintf(buf, len, "%%sp");
        break;
    case 13:
        snprintf(buf, len, "%%pcbp");
        break;
    case 14:
        snprintf(buf, len, "%%isp");
        break;
    case 15:
        snprintf(buf, len, "%%pc");
        break;
    default:
        snprintf(buf, len, "%%r%d", reg);
        break;
    }
}

void cpu_show_operand(FILE *st, operand *op)
{
    char reg_name[8];

    if (op->etype != -1) {
        switch(op->etype) {
        case 0:
            fprintf(st, "{uword}");
            break;
        case 2:
            fprintf(st, "{uhalf}");
            break;
        case 3:
            fprintf(st, "{ubyte}");
            break;
        case 4:
            fprintf(st, "{word}");
            break;
        case 6:
            fprintf(st, "{half}");
            break;
        case 7:
            fprintf(st, "{sbyte}");
            break;
        }
    }

    switch(op->mode) {
    case 0:
    case 1:
    case 2:
    case 3:
        fprintf(st, "&0x%x", op->embedded.b);
        break;
    case 4:
        if (op->reg == 15) {
            fprintf(st, "&0x%x", op->embedded.w);
        } else {
            cpu_register_name(op->reg, reg_name, 8);
            fprintf(st, "%s", reg_name);
        }
        break;
    case 5:
        if (op->reg == 15) {
            fprintf(st, "&0x%x", op->embedded.w);
        } else {
            cpu_register_name(op->reg, reg_name, 8);
            fprintf(st, "(%s)", reg_name);
        }
        break;
    case 6: /* FP Short Offset */
        if (op->reg == 15) {
            fprintf(st, "&0x%x", op->embedded.w);
        } else {
            fprintf(st, "%d(%%fp)", op->reg);
        }
        break;
    case 7: /* AP Short Offset */
        if (op->reg == 15) {
            fprintf(st, "$0x%x", op->embedded.w);
        } else {
            fprintf(st, "%d(%%ap)", op->embedded.w);
        }
        break;
    case 8:
        cpu_register_name(op->reg, reg_name, 8);
        fprintf(st, "0x%x(%s)", (int32)op->embedded.w, reg_name);
        break;
    case 9:
        cpu_register_name(op->reg, reg_name, 8);
        fprintf(st, "*0x%x(%s)", (int32)op->embedded.w, reg_name);
        break;
    case 10:
        cpu_register_name(op->reg, reg_name, 8);
        fprintf(st, "0x%x(%s)", (int16)op->embedded.w, reg_name);
        break;
    case 11:
        cpu_register_name(op->reg, reg_name, 8);
        fprintf(st, "*0x%x(%s)", (int16)op->embedded.w, reg_name);
        break;
    case 12:
        cpu_register_name(op->reg, reg_name, 8);
        fprintf(st, "%d(%s)", (int8)op->embedded.w, reg_name);
        break;
    case 13:
        cpu_register_name(op->reg, reg_name, 8);
        fprintf(st, "*%d(%s)", (int8)op->embedded.w, reg_name);
        break;
    case 14:
        if (op->reg == 15) {
            fprintf(st, "*$0x%x", op->embedded.w);
        }
        break;
    case 15:
        fprintf(st, "&%d", (int32)op->embedded.w);
        break;
    }
}

t_stat cpu_set_size(UNIT *uptr, int32 val, char *cptr, void *desc)
{
    uint32 *nRAM = NULL;
    uint32 uval = (uint32) val;

    if ((val <= 0) || (val > MAXMEMSIZE)) {
        return SCPE_ARG;
    }

    /* Do (re-)allocation for memory. */

    nRAM = (uint32 *) calloc(uval >> 2, sizeof(uint32));

    if (nRAM == NULL) {
        return SCPE_MEM;
    }

    free(RAM);
    RAM = nRAM;

    MEM_SIZE = uval;

    memset(RAM, 0, MEM_SIZE >> 2);

    return SCPE_OK;
}

/*
 * Returns a pointer to the next instruction struct in the instruction
 * ring buffer.
 */
instr *cpu_next_instruction()
{
    instr *i;

    i = &INST[cpu_hist_h];

    if (++cpu_hist_h == cpu_history_size) {
        cpu_hist_h = 0;
    }

    /* The head always "pushes" the tail along the array */
    if (cpu_hist_h == cpu_hist_t) {
        if (++cpu_hist_t == cpu_history_size) {
            cpu_hist_t = 0;
        }
    }

    clear_instruction(i);

    last_instruction = i;

    return i;
}

static SIM_INLINE void clear_instruction(instr *inst)
{
    int i;

    memset(inst, 0, sizeof(instr));

    for (i = 0; i < 4; i++) {
        inst->operands[i].etype = -1;
    }
}

/*
 * Decode a single descriptor-defined operand from the instruction
 * stream. Returns the number of bytes consumed during decode.
 */
static uint8 decode_operand(uint32 pa, instr *instr, uint8 op_number)
{
    uint8 desc;
    uint8 offset = 0;
    operand *oper = &instr->operands[op_number];

    /* Set the default data type if none is already set */
    if (cpu_dtype == -1) {
        cpu_dtype = instr->mn->dtype;
    }

    /* Read in the descriptor byte */
    desc = read_b(pa + offset++);

    oper->mode = (desc >> 4) & 0xf;
    oper->reg = desc & 0xf;
    oper->dtype = instr->mn->dtype;
    oper->etype = cpu_etype;

    switch (oper->mode) {
    case 0:  /* Positive Literal */
    case 1:  /* Positive Literal */
    case 2:  /* Positive Literal */
    case 3:  /* Positive Literal */
    case 15: /* Negative literal */
        oper->embedded.b = (uint8)desc;
        break;
    case 4:  /* Word Immediate, Register Mode */
        switch (oper->reg) {
        case 15: /* Word Immediate */
            oper->embedded.w = (uint32) read_b(pa + offset++);
            oper->embedded.w |= ((uint32) read_b(pa + offset++)) << 8u;
            oper->embedded.w |= ((uint32) read_b(pa + offset++)) << 16u;
            oper->embedded.w |= ((uint32) read_b(pa + offset++)) << 24u;
            break;
        default: /* Register mode */
            break;
        }
        break;
    case 5: /* Halfword Immediate, Register Deferred Mode */
        switch (oper->reg) {
        case 15: /* Halfword Immediate */
            oper->embedded.h = (uint16) read_b(pa + offset++);
            oper->embedded.h |= ((uint16) read_b(pa + offset++)) << 8u;
            break;
        case 11: /* INVALID */
            /* TODO: Confirm that INVALID_DESCRIPTOR is correct. */
            cpu_set_exception(NORMAL_EXCEPTION, INVALID_DESCRIPTOR);
            /* TODO: Confirm that aborting here is OK. */
            return offset;
        default: /* Register deferred mode */
            break;
        }
        break;
    case 6: /* Byte Immediate, FP Short Offset */
        switch (oper->reg) {
        case 15: /* Byte Immediate */
            oper->embedded.b = read_b(pa + offset++);
            break;
        default: /* FP Short Offset */
            oper->embedded.b = oper->reg;
            break;
        }
        break;
    case 7: /* Absolute, AP Short Offset */
        switch (oper->reg) {
        case 15: /* Absolute */
            oper->embedded.w = (uint32) read_b(pa + offset++);
            oper->embedded.w |= ((uint32) read_b(pa + offset++)) << 8u;
            oper->embedded.w |= ((uint32) read_b(pa + offset++)) << 16u;
            oper->embedded.w |= ((uint32) read_b(pa + offset++)) << 24u;
            break;
        default: /* AP Short Offset */
            oper->embedded.b = oper->reg;
            break;
        }
        break;
    case 8: /* Word Displacement */
    case 9: /* Word Displacement Deferred */
        oper->embedded.w = (uint32) read_b(pa + offset++);
        oper->embedded.w |= ((uint32) read_b(pa + offset++)) << 8u;
        oper->embedded.w |= ((uint32) read_b(pa + offset++)) << 16u;
        oper->embedded.w |= ((uint32) read_b(pa + offset++)) << 24u;
        break;
    case 10: /* Halfword Displacement */
    case 11: /* Halfword Displacement Deferred */
        oper->embedded.h = read_b(pa + offset++);
        oper->embedded.h |= ((uint16) read_b(pa + offset++)) << 8u;
        break;
    case 12: /* Byte Displacement */
    case 13: /* Byte Displacement Deferred */
        oper->embedded.b = read_b(pa + offset++);
        break;
    case 14: /* Absolute Deferred, Extended-Operand */
        switch (oper->reg) {
        case 15: /* Absolute Deferred */
            oper->embedded.w = (uint32) read_b(pa + offset++);
            oper->embedded.w |= ((uint32) read_b(pa + offset++)) << 8u;
            oper->embedded.w |= ((uint32) read_b(pa + offset++)) << 16u;
            oper->embedded.w |= ((uint32) read_b(pa + offset++)) << 24u;
            break;
        case 0:
        case 2:
        case 3:
        case 4:
        case 6:
        case 7: /* Expanded Datatype */
            /* Recursively decode the remainder of the operand after
               storing the expanded datatype */
            cpu_etype = (int8) oper->reg;
            oper->etype = cpu_etype;
            offset += decode_operand(pa + offset, instr, op_number);
            break;
        default:
            cpu_set_exception(NORMAL_EXCEPTION, RESERVED_DATATYPE);
            break;
        }
        break;
    default:
        cpu_set_exception(NORMAL_EXCEPTION, INVALID_DESCRIPTOR);
    }

    oper->data = oper->embedded.w;

    return offset;
}

/*
 * Return a pointer to the 'Op' structure that represents the
 * opcode currently being pointed at by the PC. Returns the number of
 * bytes consumed (effectively, either 1 or 2)
 */
uint8 get_mnemonic(mnemonic **mn, uint32 pa)
{
    uint16 opcode;
    uint8 i;
    uint8 offset = 0;

    opcode = read_b(pa + offset++);

    if (opcode == 0x30) {
        opcode = (opcode << 8) | read_b(pa + offset++);

        for (i = 0; i < HWORD_OP_COUNT; i++) {
            if (hword_ops[i].opcode == opcode) {
                *mn = &hword_ops[i];
                break;
            }
        }
    } else {
        *mn = &ops[opcode];
    }

    return offset;
}

/*
 * Decode the instruction currently being pointed at by the PC.
 * This routine does the following:
 *   1. Read the opcode.
 *   2. Determine the number of operands to decode based on
 *      the opcode type.
 *   3. Fetch each opcode from main memory.
 *
 * This routine may alter the PSW's ET (Exception Type) and
 * ISC (Internal State Code) registers if an exceptional condition
 * is encountered during decode.
 */
uint8 decode_instruction(instr *instr)
{
    uint8 offset = 0;
    uint32 pa = R[NUM_PC];
    mnemonic *mn;
    int i;

    /* Store off the PC and and PSW for history keeping */
    instr->pc = pa;
    instr->psw = R[NUM_PSW];

    /* Reset our data types */
    cpu_etype = -1;
    cpu_dtype = -1;

    offset += get_mnemonic(&mn, pa);

    if (mn == NULL) {
        sim_debug(EXECUTE_MSG, &cpu_dev,
                  ">>> [%08x]: Unable to decode instruction! No mnemonic found.\n",
                  R[NUM_PC]);
        cpu_set_exception(NORMAL_EXCEPTION, ILLEGAL_OPCODE);
        return offset;
    }

    instr->mn = mn;

    if (mn->op_count < 0) {
        sim_debug(EXECUTE_MSG, &cpu_dev,
                  ">>> [%08x]: Unable to decode instruction! opcode=%04x\n",
                  R[NUM_PC], mn->opcode);
        cpu_set_exception(NORMAL_EXCEPTION, ILLEGAL_OPCODE);
        return offset;
    }

    if (mn->op_count == 0) {
        /* Nothing else to do, we're done decoding. */
        return offset;
    }

    switch (mn->mode) {
    case OP_NONE:
        break;
    case OP_BYTE:
        instr->operands[0].embedded.b = read_b(pa + offset++);
        instr->operands[0].mode = 6;
        instr->operands[0].reg = 15;
        break;
    case OP_HALF:
        instr->operands[0].embedded.h = read_b(pa + offset++);
        instr->operands[0].embedded.h |= read_b(pa + offset++) << 8;
        instr->operands[0].mode = 5;
        instr->operands[0].reg = 15;
        break;
    case OP_COPR:
        instr->operands[0].embedded.w = read_b(pa + offset++);
        instr->operands[0].embedded.w |= read_b(pa + offset++) << 8;
        instr->operands[0].embedded.w |= read_b(pa + offset++) << 16;
        instr->operands[0].embedded.w |= read_b(pa + offset++) << 24;
        instr->operands[0].mode = 4;
        instr->operands[0].reg = 15;

        /* Decode subsequent operands */
        for (i = 1; i < mn->op_count; i++) {
            offset += decode_operand(pa + offset, instr, i);
        }

        break;
    case OP_DESC:
        for (i = 0; i < mn->op_count; i++) {
            offset += decode_operand(pa + offset, instr, i);
        }
        break;
    }

    return offset;
}

void cpu_context_switch_2(uint32 new_pcbp)
{
    uint32 new_pc, new_psw, new_sp;

    new_psw = read_w(new_pcbp);
    new_pc = read_w(new_pcbp + 4);
    new_sp = read_w(new_pcbp + 8);

    /* Call XSWITCH_TWO() microroutine to load the new PCBP */
    R[NUM_PCBP] = new_pcbp;
    R[NUM_PSW] = new_psw;
    R[NUM_PC] = new_pc;
    R[NUM_SP] = new_sp;

    if (R[NUM_PSW] & PSW_I_MASK) {
        R[NUM_PSW] &= ~PSW_I_MASK;
        R[NUM_PCBP] += 12;
    }

    /* Set new ISC, TM, and ET fields */
    R[NUM_PSW] &= ~PSW_ISC_MASK;
    R[NUM_PSW] |= (7 << PSW_ISC);

    R[NUM_PSW] &= ~PSW_TM_MASK;

    R[NUM_PSW] &= ~PSW_ET_MASK;
    R[NUM_PSW] |= (3 << PSW_ET);

    /* XXX magic */
    R[NUM_PSW] &= ~PSW_R_MASK;

    /* Call XSWITCH_THREE() microroutine to do block moves */
    if (R[NUM_PSW] & PSW_R_MASK) {

        R[0] = R[NUM_PCBP] + 64;
        R[2] = read_w(R[0]);
        R[0] += 4;

        while (R[2] != 0) {
            R[1] = read_w(R[0]);
            R[0] += 4;

            /* MOVBLW */
            while (R[2] > 0) {
                write_w(R[1], read_w(R[0]));
                R[2]--;
                R[0] += 4;
                R[1] += 4;
            }

            R[2] = read_w(R[0]);
            R[0] += 4;
        }

        R[0] = R[0] + 4;
    }

}

void cpu_context_switch_1(uint32 new_pcbp)
{
    uint32 cur_pcbp, new_psw;

    cur_pcbp = R[NUM_PCBP];
    new_psw = read_w(new_pcbp);

    /* Call XSWITCH_ONE() microroutine to save process context */

    /* Copy the 'R' flag from the new PSW to the old PSW */
    R[NUM_PSW] &= ~PSW_R_MASK;
    R[NUM_PSW] |= (new_psw & PSW_R_MASK);

    /* Save the PSW, PC, and SP in the current PCB */
    write_w(cur_pcbp, R[NUM_PSW]);
    write_w(cur_pcbp + 4, R[NUM_PC]);
    write_w(cur_pcbp + 8, R[NUM_SP]);

    /* If R is set, save current R0-R8/FP/AP in PCB */
    if (R[NUM_PSW] & PSW_R_MASK) {
        write_w(cur_pcbp + 20, R[NUM_AP]);
        write_w(cur_pcbp + 24, R[NUM_FP]);
        write_w(cur_pcbp + 28, R[0]);
        write_w(cur_pcbp + 32, R[1]);
        write_w(cur_pcbp + 36, R[2]);
        write_w(cur_pcbp + 40, R[3]);
        write_w(cur_pcbp + 44, R[4]);
        write_w(cur_pcbp + 48, R[5]);
        write_w(cur_pcbp + 52, R[6]);
        write_w(cur_pcbp + 56, R[7]);
        write_w(cur_pcbp + 60, R[8]);
    }

    cpu_context_switch_2(new_pcbp);
}

t_bool cpu_handle_irq(uint8 ipl, uint8 id, t_bool nmi)
{
    uint8  psw_ipl;
    t_bool quick;
    uint32 new_pcbp_addr;
    uint32 new_pcbp;

    /* Maybe handle the IRQ */
    psw_ipl = ((R[NUM_PSW] & PSW_IPL_MASK) >> PSW_IPL) & 0xf;
    quick = (R[NUM_PSW] & PSW_QIE_MASK) >> PSW_QIE;

    if (ipl <= psw_ipl && !nmi) {
        sim_debug(IRQ_MSG, &cpu_dev, "Ignoring IRQ\n");
        return FALSE;
    }

    sim_debug(IRQ_MSG, &cpu_dev, "Handling IRQ\n");

    if (nmi) {
        id = 0;
    }

    if (quick) {
        /* TODO: Maybe implement quick interrupts at some point, but
           the 3B2 ROM doesn't use them. */
        sim_debug(IRQ_MSG, &cpu_dev, "QUICK INTERRUPT\n");
        return FALSE;
    }


    new_pcbp_addr = 0x8c + (4 * id);
    new_pcbp = read_w(new_pcbp_addr);

    /* Save the old PCBP */
    irq_push_word(R[NUM_PCBP]);

    /* Full interrupts require a complete process switch */
    sim_debug(IRQ_MSG, &cpu_dev, "FULL INTERRUPT. "
              "CUR PSW=%08x, CUR PCBP=%08x, NEW PCBP=%08x\n",
              R[NUM_PSW], R[NUM_PCBP], new_pcbp);

    /* Context switch */
    cpu_context_switch_1(new_pcbp);

    return TRUE;
}

t_stat sim_instr(void)
{
    int32 reason = 0;
    instr* i;
    uint8 et, isc;

    /* Temporary register used for overflow detection */
    t_uint64 result;

    /* Scratch space */
    uint32   a, b, c, d;

    /* Used for field calculation */
    uint32   width, offset, mask;

    operand *src1, *src2, *src3, *dst;

    while (reason == 0) {

        if (sim_interval <= 0) {
            if ((reason = sim_process_event())) {
                break;
            }
        }

        if (sim_brk_summ && sim_brk_test (R[NUM_PC], SWMASK ('E'))) {
            reason = STOP_IBKPT;
            break;
        }

        /* Process IRQs */
        if (cpu_irq_ipl > -1) {
            /* Preserve local NMI and IRQ values */
            t_bool handled = cpu_handle_irq(cpu_irq_ipl, cpu_irq_id, cpu_nmi);

            /* Clear global IRQ state */
            cpu_irq_ipl = -1;
            cpu_nmi = FALSE;
            cpu_in_wait = FALSE;

            if (handled) {
                continue;
            }
        }

        /* Process DMA requests */
        dmac_service_drqs();

        sim_interval--;

        if (cpu_in_wait) {
            continue;
        }

        /* Reset the TM bits */
        R[NUM_PSW] = R[NUM_PSW] & ~PSW_TM;
        clear_exceptions();

        /* Set the PSW TM bit. */
        set_psw_tm(TRUE);

        /* Get the instruction */
        i = cpu_next_instruction();

        /* Decode the instruction */
        cpu_ilen = decode_instruction(i);

        /*
         * Operate on the decoded instruction, handle traps and
         * exceptions.
         */

        /* If an exception occured during decode, handle it. */
        if (cpu_exception) {
            if (cpu_unit.flags & UNIT_EXHALT) {
                reason = STOP_EX;
            }

            et  = R[NUM_PSW] & PSW_ET_MASK;
            isc = (R[NUM_PSW] & PSW_ISC_MASK) >> PSW_ISC;

            switch(et) {
            case NORMAL_EXCEPTION:
                sim_debug(EXECUTE_MSG, &cpu_dev,
                          ">>> [%08x] NORMAL EXCEPTION DURING DECODE: isc=%d\n",
                          R[NUM_PC], isc);
                cpu_perform_gate(0, isc << 3);
                break;
            }

            continue;
        }


        /* Handle the instruction */

        /* Get the operands */
        if (i->mn->src_op1 >= 0) {
            src1 = &i->operands[i->mn->src_op1];
        }

        if (i->mn->src_op2 >= 0) {
            src2 = &i->operands[i->mn->src_op2];
        }

        if (i->mn->src_op3 >= 0) {
            src3 = &i->operands[i->mn->src_op3];
        }

        if (i->mn->dst_op >= 0) {
            dst = &i->operands[i->mn->dst_op];
        }

        switch (i->mn->opcode) {
        case ADDW2:
        case ADDH2:
        case ADDB2:
            a = cpu_read_op(src1);
            b = cpu_read_op(dst);
            add(a, b, dst);
            break;
        case ADDW3:
        case ADDH3:
        case ADDB3:
            a = cpu_read_op(src1);
            b = cpu_read_op(src2);
            add(a, b, dst);
            break;
        case ALSW3:
            a = cpu_read_op(src2);
            b = cpu_read_op(src1);
            result = a << (b & 0x1f);
            cpu_write_op(dst, result);
            cpu_set_nz_flags(result, dst);
            cpu_set_c_flag(0);
            cpu_set_v_flag_op(result, dst);
            break;
        case ANDW2:
        case ANDH2:
        case ANDB2:
            a = cpu_read_op(src1);
            b = cpu_read_op(dst);
            c = a & b;
            cpu_write_op(dst, c);
            cpu_set_nz_flags(c, dst);
            cpu_set_c_flag(0);
            cpu_set_v_flag(0);
            break;
        case ANDW3:
        case ANDH3:
        case ANDB3:
            a = cpu_read_op(src1);
            b = cpu_read_op(src2);
            c = a & b;
            cpu_write_op(dst, c);
            cpu_set_nz_flags(c, dst);
            cpu_set_c_flag(0);
            cpu_set_v_flag(0);
            break;
        case BEH:
        case BEH_D:
            if (cpu_z_flag() == 1) {
                R[NUM_PC] += (int16)(dst->embedded.h);
                continue;
            }
            break;
        case BEB:
        case BEB_D:
            if (cpu_z_flag() == 1) {
                R[NUM_PC] += (int8)(dst->embedded.b);
                continue;
            }
            break;
        case BGH:
            if ((cpu_n_flag() | cpu_z_flag()) == 0) {
                R[NUM_PC] += (int16)(dst->embedded.h);
                continue;
            }
            break;
        case BGB:
            if ((cpu_n_flag() | cpu_z_flag()) == 0) {
                R[NUM_PC] += (int8)(dst->embedded.b);
                continue;
            }
            break;
        case BGEH:
            if ((cpu_n_flag() == 0) | (cpu_z_flag() == 1)) {
                R[NUM_PC] += (int16)(dst->embedded.h);
                continue;
            }
            break;
        case BGEB:
            if ((cpu_n_flag() == 0) | (cpu_z_flag() == 1)) {
                R[NUM_PC] += (int8)(dst->embedded.b);
                continue;
            }
            break;
        case BGEUH:
            if (cpu_c_flag() == 0) {
                R[NUM_PC] += (int16)(dst->embedded.h);
                continue;
            }
            break;
        case BGEUB:
            if (cpu_c_flag() == 0) {
                R[NUM_PC] += (int8)(dst->embedded.b);
                continue;
            }
            break;
        case BGUH:
            if ((cpu_c_flag() | cpu_z_flag()) == 0) {
                R[NUM_PC] += (int16)(dst->embedded.h);
                continue;
            }
            break;
        case BGUB:
            if ((cpu_c_flag() | cpu_z_flag()) == 0) {
                R[NUM_PC] += (int8)(dst->embedded.b);
                continue;
            }
            break;
        case BITW:
        case BITH:
        case BITB:
            a = cpu_read_op(src1);
            b = cpu_read_op(src2);
            result = a & b;
            cpu_set_nz_flags(result, src2);
            cpu_set_c_flag(0);
            cpu_set_v_flag(0);
            break;
        case BLH:
            if ((cpu_n_flag() == 1) && (cpu_z_flag() == 0)) {
                R[NUM_PC] += (int16)(dst->embedded.h);
                continue;
            }
            break;
        case BLB:
            if ((cpu_n_flag() == 1) && (cpu_z_flag() == 0)) {
                R[NUM_PC] += (int8)(dst->embedded.b);
                continue;
            }
            break;
        case BLEH:
            if ((cpu_n_flag() | cpu_z_flag()) == 1) {
                R[NUM_PC] += (int16)(dst->embedded.h);
                continue;
            }
            break;
        case BLEB:
            if ((cpu_n_flag() | cpu_z_flag()) == 1) {
                R[NUM_PC] += (int8)(dst->embedded.b);
                continue;
            }
            break;
        case BLEUH:
            if ((cpu_c_flag() | cpu_z_flag()) == 1) {
                R[NUM_PC] += (int16)(dst->embedded.h);
                continue;
            }
            break;
        case BLEUB:
            if ((cpu_c_flag() | cpu_z_flag()) == 1) {
                R[NUM_PC] += (int8)(dst->embedded.b);
                continue;
            }
            break;
        case BLUH:
            if (cpu_c_flag() == 1) {
                R[NUM_PC] += (int16)(dst->embedded.h);
                continue;
            }
            break;
        case BLUB:
            if (cpu_c_flag() == 1) {
                R[NUM_PC] += (int8)(dst->embedded.b);
                continue;
            }
            break;
        case BNEH:
        case BNEH_D:
            if (cpu_z_flag() == 0) {
                R[NUM_PC] += (int16)(dst->embedded.h);
                continue;
            }
            break;
        case BNEB:
        case BNEB_D:
            if (cpu_z_flag() == 0) {
                R[NUM_PC] += (int8)(dst->embedded.b);
                continue;
            }
            break;
        case BPT:
            /* TODO: Confirm that a breakpoint trap will increment the
               PC. Otherwise, change 'break' to 'continue' */
            cpu_set_exception(NORMAL_EXCEPTION, BREAKPOINT_TRAP);
            break;
        case BRH:
            R[NUM_PC] += (int16)(dst->embedded.h);
            continue;
        case BRB:
            R[NUM_PC] += (int8)(dst->embedded.b);
            continue;
        case BSBH:
            cpu_push_word(cpu_next_pc());
            R[NUM_PC] += (int16)(dst->embedded.h);
            continue;
        case BSBB:
            cpu_push_word(cpu_next_pc());
            R[NUM_PC] += (int8)(dst->embedded.b);
            continue;
        case BVCH:
            if (cpu_v_flag() == 0) {
                R[NUM_PC] += (int16)(dst->embedded.h);
                continue;
            }
            break;
        case BVCB:
            if (cpu_v_flag() == 0) {
                R[NUM_PC] += (int8)(dst->embedded.b);
                continue;
            }
            break;
        case BVSH:
            if (cpu_v_flag() == 1) {
                R[NUM_PC] += (int16)(dst->embedded.h);
                continue;
            }
            break;
        case BVSB:
            if (cpu_v_flag() == 1) {
                R[NUM_PC] += (int8)(dst->embedded.b);
                continue;
            }
            break;
        case CALL:
            a = cpu_effective_address(src1);
            b = cpu_effective_address(dst);
            cpu_push_word(cpu_next_pc());
            cpu_push_word(R[NUM_AP]);
            R[NUM_AP] = a;
            R[NUM_PC] = b;
            continue;
        case CFLUSH:
            break;
        case CALLPS:
            if (cpu_execution_level() != EX_LVL_KERN) {
                cpu_set_exception(NORMAL_EXCEPTION, PRIVILEGED_OPCODE);
                break;
            }

            R[NUM_PC] += 2;

            a = R[0];

            irq_push_word(R[NUM_PCBP]);

            cpu_context_switch_1(a);

            continue;
        case CLRW:
        case CLRH:
        case CLRB:
            cpu_write_op(dst, 0);
            cpu_set_n_flag(0);
            cpu_set_z_flag(1);
            cpu_set_c_flag(0);
            cpu_set_v_flag(0);
            break;
        case CMPW:
            a = cpu_read_op(src1);
            b = cpu_read_op(src2);

            cpu_set_z_flag((uint32)b == (uint32)a);
            cpu_set_n_flag((int32)b < (int32)a);
            cpu_set_c_flag((uint32)b < (uint32)a);
            cpu_set_v_flag(0);
            break;
        case CMPH:
            a = cpu_read_op(src1);
            b = cpu_read_op(src2);

            cpu_set_z_flag((uint16)b == (uint16)a);
            cpu_set_n_flag((int16)b < (int16)a);
            cpu_set_c_flag((uint16)b < (uint16)a);
            cpu_set_v_flag(0);
            break;
        case CMPB:
            a = cpu_read_op(src1);
            b = cpu_read_op(src2);

            cpu_set_z_flag((uint8)b == (uint8)a);
            cpu_set_n_flag((int8)b < (int8)a);
            cpu_set_c_flag((uint8)b < (uint8)a);
            cpu_set_v_flag(0);
            break;
        case DECW:
        case DECH:
        case DECB:
            a = cpu_read_op(dst);
            sub(a, 1, dst);
            break;
        case DIVW2:
        case DIVH2:
        case DIVB2:
            a = cpu_read_op(src1);
            b = cpu_read_op(dst);

            if (a == 0) {
                cpu_set_exception(NORMAL_EXCEPTION, INTEGER_ZERO_DIVIDE);
                break;
            }

            result = b / a;

            cpu_write_op(dst, result);
            cpu_set_nz_flags(result, dst);
            cpu_set_c_flag(0);
            cpu_set_v_flag_op(result, dst);
            break;
        case DIVW3:
        case DIVH3:
        case DIVB3:
            a = cpu_read_op(src1);
            b = cpu_read_op(src2);

            if (a == 0) {
                cpu_set_exception(NORMAL_EXCEPTION, INTEGER_ZERO_DIVIDE);
                break;
            }

            result = b / a;

            cpu_write_op(dst, result);
            cpu_set_nz_flags(result, dst);
            cpu_set_c_flag(0);
            /* TODO: V flag */
            break;
        case ENBVJMP:
            sim_debug(EXECUTE_MSG, &cpu_dev,
                      ">>> ENBVJMP -- Enabling MMU. R0=%04x\n", R[0]);
            if (cpu_execution_level() != EX_LVL_KERN) {
                cpu_set_exception(NORMAL_EXCEPTION, PRIVILEGED_OPCODE);
                break;
            }
            mmu_enable();
            R[NUM_PC] = R[0];
            continue;
        case DISVJMP:
            sim_debug(EXECUTE_MSG, &cpu_dev,
                      ">>> DISVJMP -- Disabling MMU. R0=%04x\n", R[0]);
            if (cpu_execution_level() != EX_LVL_KERN) {
                cpu_set_exception(NORMAL_EXCEPTION, PRIVILEGED_OPCODE);
                break;
            }
            mmu_disable();
            R[NUM_PC] = R[0];
            continue;
        case EXTFW:
        case EXTFH:
        case EXTFB:
            width = (cpu_read_op(src1) & 0x1f) + 1;
            offset = cpu_read_op(src2) & 0x1f;
            a = cpu_read_op(src3);        /* src */
            mask = (1 << width) - 1;

            a &= (mask << offset);
            a = a >> offset;

            cpu_write_op(dst, a);
            cpu_set_nz_flags(a, dst);
            break;
        case INCW:
        case INCH:
        case INCB:
            a = cpu_read_op(dst);
            add(a, 1, dst);
            break;
        case INSFW:
        case INSFH:
        case INSFB:
            width = (cpu_read_op(src1) & 0x1f) + 1;
            offset = cpu_read_op(src2) & 0x1f;
            a = cpu_read_op(src3);        /* src */
            b = cpu_read_op(dst);         /* dst */
            mask = (1 << width) - 1;

            b &= ~(mask << offset);
            b |= (a << offset);

            cpu_write_op(dst, b);

            cpu_set_nz_flags(b, dst);
            break;
        case JMP:
            R[NUM_PC] = cpu_effective_address(dst);
            continue;
        case JSB:
            cpu_push_word(cpu_next_pc());
            R[NUM_PC] = cpu_effective_address(dst);
            continue;
        case LLSW3:
            result = cpu_read_op(src2) << (cpu_read_op(src1) & 0x1f);
            cpu_write_op(dst, (uint32)(result & WORD_MASK));
            cpu_set_nz_flags(a, dst);
            break;
        case LLSH3:
            a = cpu_read_op(src2) << (cpu_read_op(src1) & 0x1f);
            cpu_write_op(dst, a);
            cpu_set_nz_flags(a, dst);
            break;
        case LLSB3:
            a = cpu_read_op(src2) << (cpu_read_op(src1) & 0x1f);
            cpu_write_op(dst, a);
            cpu_set_nz_flags(a, dst);
            break;
        case ARSW3:
        case ARSH3:
        case ARSB3:
            a = cpu_read_op(src2);
            b = cpu_read_op(src1) & 0x1f;
            result = a >> b;
            switch (op_type(src2)) {
                case WD:
                    if (a & 0x80000000)
                        result |= m68ki_shift_32_table[b + 1];
                    break;
                case HW:
                    if (a & 0x8000)
                        result |= m68ki_shift_16_table[b + 1];
                    break;
                case BT:
                    if (a & 0x80)
                        result |= m68ki_shift_8_table[b + 1];
                    break;
            }
            cpu_write_op(dst, result);
            cpu_set_nz_flags(a, dst);
            break;
        case LRSW3:
            a = (uint32) cpu_read_op(src2) >> (cpu_read_op(src1) & 0x1f);
            cpu_write_op(dst, a);
            cpu_set_nz_flags(a, dst);
            break;
        case MCOMW:
        case MCOMH:
        case MCOMB:
            a = ~(cpu_read_op(src1));
            cpu_write_op(dst, a);
            cpu_set_nz_flags(a, dst);
            break;
        case MNEGW:
        case MNEGH:
        case MNEGB:
            a = ~cpu_read_op(src1) + 1;
            cpu_write_op(dst, a);
            cpu_set_nz_flags(a, dst);
            break;
        case MOVBLW:
            while (R[2] > 0) {
                write_w(R[1], read_w(R[0]));
                R[2]--;
                R[0] += 4;
                R[1] += 4;
            }
            break;
        case SWAPWI:
        case SWAPHI:
        case SWAPBI:
            a = cpu_read_op(dst);
            cpu_write_op(dst, R[0]);
            R[0] = a;
            cpu_set_nz_flags(a, dst);
            cpu_set_v_flag(0);
            cpu_set_c_flag(0);
            break;
        case ROTW:
            a = cpu_read_op(src1) & 31;
            b = (uint32) cpu_read_op(src2);
            mask = (CHAR_BIT * sizeof(a) - 1);
            d = (b >> a) | (b << ((~a + 1) & mask));
            cpu_write_op(dst, d);
            cpu_set_nz_flags(d, dst);
            cpu_set_v_flag(0);
            cpu_set_c_flag(0);
            break;
        case MOVAW:
            a = cpu_effective_address(src1);
            cpu_write_op(dst, a);
            cpu_set_nz_flags(a, src1);
            cpu_set_v_flag(0);
            cpu_set_c_flag(0);
            break;
        case MOVTRW:
            a = cpu_effective_address(src1);
            result = mmu_xlate_addr(a);
            sim_debug(EXECUTE_MSG, &cpu_dev,
                      "MOVTRW: input=%08x, output=%08x\n",
                      (uint32)a, (uint32)result);
            cpu_write_op(dst, result);
            cpu_set_nz_flags(result, dst);
            cpu_set_v_flag(0);
            cpu_set_c_flag(0);
            break;
        case MOVW:
        case MOVH:
        case MOVB:
            a = cpu_read_op(src1);
            cpu_write_op(dst, a);

            /* Flags are never set if the source or destination is the
               PSW */
            if (!(op_is_psw(src1) || op_is_psw(dst))) {
                cpu_set_nz_flags(a, dst);
                cpu_set_c_flag(0);
                switch (op_type(dst)) {
                case WD:
                case UW:
                    cpu_set_v_flag(0);
                    break;
                case HW:
                case UH:
                    cpu_set_v_flag(a > HALF_MASK);
                    break;
                case BT:
                case SB:
                    cpu_set_v_flag(a > BYTE_MASK);
                    break;
                }
            }

            /* However, if a move to PSW set the O bit, we have to
               generate an overflow exception trap */
            if (op_is_psw(dst) && (R[NUM_PSW] & PSW_OE_MASK)) {
                cpu_set_exception(NORMAL_EXCEPTION, INTEGER_OVERFLOW);
            }

            break;
        case MODW2:
        case MODH2:
        case MODB2:
            a = cpu_read_op(src1);
            b = cpu_read_op(dst);

            if (a == 0) {
                /* TODO: Divide By Zero error */
                break;
            }

            result = b % a;

            cpu_write_op(dst, result);
            cpu_set_nz_flags(result, dst);

            break;
        case MODW3:
        case MODH3:
        case MODB3:
            a = cpu_read_op(src1);
            b = cpu_read_op(src2);

            if (a == 0) {
                /* TODO: Divide By Zero error */
                break;
            }

            result = b % a;

            cpu_write_op(dst, result);
            cpu_set_nz_flags(result, dst);

            break;
        case MULW2:
            result = cpu_read_op(src1) * cpu_read_op(dst);
            cpu_write_op(dst, (uint32)(result & WORD_MASK));
            cpu_set_nz_flags(result, dst);
            break;
        case MULH2:
            a = cpu_read_op(src1) * cpu_read_op(dst);
            cpu_write_op(dst, a);
            cpu_set_nz_flags(a, dst);
            break;
        case MULB2:
            a = cpu_read_op(src1) * cpu_read_op(dst);
            cpu_write_op(dst, a);
            cpu_set_nz_flags(a, dst);
            break;
        case MULW3:
            result = cpu_read_op(src1) * cpu_read_op(src2);
            cpu_write_op(dst, (uint32)(result & WORD_MASK));
            cpu_set_nz_flags(result, dst);
            break;
        case MULH3:
            a = cpu_read_op(src1) * cpu_read_op(src2);
            cpu_write_op(dst, a);
            cpu_set_nz_flags(a, dst);
            break;
        case MULB3:
            a = cpu_read_op(src1) * cpu_read_op(src2);
            cpu_write_op(dst, a);
            cpu_set_nz_flags(a, dst);
            break;
        case NOP:
            break;
        case NOP2:
            cpu_ilen += 1;
            break;
        case NOP3:
            cpu_ilen += 2;
            break;
        case ORW2:
        case ORH2:
        case ORB2:
            a = (cpu_read_op(src1) | cpu_read_op(dst));
            cpu_write_op(dst, a);
            cpu_set_nz_flags(a, dst);
            cpu_set_c_flag(0);
            cpu_set_v_flag(0); /* TODO: Fix */
            break;
        case ORW3:
        case ORH3:
        case ORB3:
            a = (cpu_read_op(src1) | cpu_read_op(src2));
            cpu_write_op(dst, a);
            cpu_set_nz_flags(a, dst);
            break;
        case POPW:
            a = cpu_pop_word();
            cpu_write_op(dst, a);
            cpu_set_nz_flags(a, dst);
            cpu_set_c_flag(0);
            cpu_set_v_flag(0);
            break;
        case PUSHAW:
            a = cpu_effective_address(src1);
            cpu_push_word(a);
            cpu_set_nz_flags(a, src1);
            cpu_set_c_flag(0);
            cpu_set_v_flag(0);
            break;
        case PUSHW:
            a = cpu_read_op(src1);
            cpu_push_word(a);
            cpu_set_nz_flags(a, src1);
            break;
        case RGEQ:
            if (cpu_n_flag() == 0 || cpu_z_flag() == 1) {
                R[NUM_PC] = cpu_pop_word();
                continue;
            }
            break;
        case RGEQU:
            if (cpu_c_flag() == 0) {
                R[NUM_PC] = cpu_pop_word();
                continue;
            }
            break;
        case RGTR:
            if ((cpu_n_flag() | cpu_z_flag()) == 0) {
                R[NUM_PC] = cpu_pop_word();
                continue;
            }
            break;
        case RNEQ:
        case RNEQU:
            if (cpu_z_flag() == 0) {
                R[NUM_PC] = cpu_pop_word();
                continue;
            }
            break;
        case RET:
            a = R[NUM_AP];
            b = cpu_pop_word();
            c = cpu_pop_word();
            R[NUM_AP] = b;
            R[NUM_PC] = c;
            R[NUM_SP] = a;
            continue;
        case RETG:
            sim_debug(EXECUTE_MSG, &cpu_dev,
                      "[%08x] RETG: PSW before RETG: %08x; ",
                      R[NUM_PC], R[NUM_PSW]);

            a = cpu_pop_word(); /* PSW */
            b = cpu_pop_word(); /* PC */

            if ((a & PSW_CM_MASK) < (R[NUM_PSW] & PSW_CM_MASK)) {
                cpu_set_exception(NORMAL_EXCEPTION, ILLEGAL_LEVEL_CHANGE);
            }

            /* Clear some state and move it from the current PSW */
            a &= ~PSW_IPL_MASK;
            a &= ~PSW_CFD_MASK;
            a &= ~PSW_QIE_MASK;
            a &= ~PSW_CD_MASK;
            a &= ~PSW_R_MASK;
            a &= ~PSW_ISC_MASK;
            a &= ~PSW_TM_MASK;
            a &= ~PSW_ET_MASK;

            a |= (R[NUM_PSW] & PSW_IPL_MASK);
            a |= (R[NUM_PSW] & PSW_CFD_MASK);
            a |= (R[NUM_PSW] & PSW_QIE_MASK);
            a |= (R[NUM_PSW] & PSW_CD_MASK);
            a |= (R[NUM_PSW] & PSW_R_MASK);
            a |= 7 << PSW_ISC;
            a |= 3 << PSW_ET;


            R[NUM_PSW] = a;
            R[NUM_PC] = b;

            sim_debug(EXECUTE_MSG, &cpu_dev,
                      "PSW after RETG: %08x\n",
                      R[NUM_PSW]);

            continue;
        case RETPS:
            if (cpu_execution_level() != EX_LVL_KERN) {
                cpu_set_exception(NORMAL_EXCEPTION, PRIVILEGED_OPCODE);
                break;
            }

            /* Restore process state */
            a = irq_pop_word();
            b = read_w(a); /* New PSW */

            sim_debug(EXECUTE_MSG, &cpu_dev,
                      ">>> [%08x] RETPS: Current PSW: %08x. New PSW: %08x, New PCBP = %08x.\n",
                      R[NUM_PC], R[NUM_PSW], b, a);

            /* Copy the 'R' flag from the new PSW to the old PSW */
            R[NUM_PSW] &= ~PSW_R_MASK;
            R[NUM_PSW] |= (b & PSW_R_MASK);

            /* Restore registers if R bit is set */
            if (R[NUM_PSW] & PSW_R_MASK) {
                R[NUM_AP] = read_w(a + 20);
                R[NUM_FP] = read_w(a + 24);
                R[0] = read_w(a + 28);
                R[1] = read_w(a + 32);
                R[2] = read_w(a + 36);
                R[3] = read_w(a + 40);
                R[4] = read_w(a + 44);
                R[5] = read_w(a + 48);
                R[6] = read_w(a + 52);
                R[7] = read_w(a + 56);
                R[8] = read_w(a + 60);
            }

            /* a now holds the new PCBP */
            cpu_context_switch_2(a);

            continue;
        case SPOP:
        case SPOPRD:
        case SPOPRS:
            /* TODO: Implement coprocessor support */
            sim_debug(EXECUTE_MSG, &cpu_dev,
                      "[%08x] SPOP/SPOPRD/SPOPRS not implemented.\n",
                      R[NUM_PC]);
            cpu_set_exception(NORMAL_EXCEPTION, EXTERNAL_MEMORY_FAULT);
            break;
        case SUBW2:
        case SUBH2:
        case SUBB2:
            a = cpu_read_op(dst);
            b = cpu_read_op(src1);
            sub(a, b, dst);
            break;
        case SUBW3:
        case SUBH3:
        case SUBB3:
            a = cpu_read_op(src2);
            b = cpu_read_op(src1);
            sub(a, b, dst);
            break;
        case RESTORE:
            a = R[NUM_FP] - 28;
            b = read_w(a);
            c = R[NUM_FP] - 24;

            for (d = src1->reg; d < NUM_FP; d++) {
                R[d] = read_w(c);
                c += 4;
            }

            R[NUM_FP] = b;
            R[NUM_SP] = a;
            break;
        case RLEQ:
            if ((cpu_n_flag() | cpu_z_flag()) == 1) {
                R[NUM_PC] = cpu_pop_word();
                continue;
            }
            break;
        case RLEQU:
            if ((cpu_c_flag() | cpu_z_flag()) == 1) {
                R[NUM_PC] = cpu_pop_word();
                continue;
            }
            break;
        case REQL:
            if (cpu_z_flag() == 1) {
                R[NUM_PC] = cpu_pop_word();
                continue;
            }
            break;
        case REQLU:
            if (cpu_z_flag() == 1) {
                R[NUM_PC] = cpu_pop_word();
                continue;
            }
            break;
        case RSB:
            R[NUM_PC] = cpu_pop_word();
            continue;
        case SAVE:
            a = R[NUM_SP];

            /* Save the FP register */
            cpu_push_word(R[NUM_FP]);

            /* Save all the registers from the one identified by the
               src operand up to FP (exclusive) */
            for (b = src1->reg; b < NUM_FP; b++) {
                cpu_push_word(R[b]);
            }

            R[NUM_SP] = a + 28;
            R[NUM_FP] = R[NUM_SP];
            break;
        case STRCPY:
            a = 0;
            b = 0;

            do {
                b = read_b(R[0] + a);
                write_b(R[1] + a, b);
                a++;
            } while (b != '\0');

            break;
        case TSTW:
            a = cpu_read_op(src1);
            cpu_set_nz_flags((uint32)a, src1);
            cpu_set_c_flag(0);
            cpu_set_v_flag(0);
            break;
        case TSTH:
            a = cpu_read_op(src1);
            cpu_set_nz_flags((uint16)a, src1);
            cpu_set_c_flag(0);
            cpu_set_v_flag(0);
            break;
        case TSTB:
            a = cpu_read_op(src1);
            cpu_set_nz_flags((uint8)a, src1);
            cpu_set_c_flag(0);
            cpu_set_v_flag(0);
            break;
        case WAIT:
            cpu_in_wait = TRUE;
            break;
        case XORW2:
        case XORH2:
        case XORB2:
            a = (cpu_read_op(src1) ^ cpu_read_op(dst));
            cpu_write_op(dst, a);
            cpu_set_nz_flags(a, dst);
            break;
        case XORW3:
        case XORH3:
        case XORB3:
            a = (cpu_read_op(src1) ^ cpu_read_op(src2));
            cpu_write_op(dst, a);
            cpu_set_nz_flags(a, dst);
            break;
        default:
            reason = STOP_OPCODE;
            break;
        };

        if (cpu_exception) {
            if (cpu_unit.flags & UNIT_EXHALT) {
                reason = STOP_EX;
            }

            et  = R[NUM_PSW] & PSW_ET_MASK;
            isc = (R[NUM_PSW] & PSW_ISC_MASK) >> PSW_ISC;

            switch(et) {
            case NORMAL_EXCEPTION:
                sim_debug(EXECUTE_MSG, &cpu_dev,
                          ">>> [%08x] NORMAL EXCEPTION DURING EXECUTE: isc=%d\n",
                          R[NUM_PC], isc);
                cpu_perform_gate(0, isc << 3);
                break;
            }

            continue; // Next instruction
        }

        /* Increment the PC appropriately */
        R[NUM_PC] += cpu_ilen;

        /* Now that we have incremented the PC, we look for traps */
        if (cpu_trap) {

            if (cpu_unit.flags & UNIT_EXHALT) {
                reason = STOP_EX;
            }

            et  = R[NUM_PSW] & 3;
            isc = (R[NUM_PSW] & PSW_ISC_MASK) >> PSW_ISC;

            sim_debug(EXECUTE_MSG, &cpu_dev,
                      "*** [PC=%08x]: CPU TRAP: et=%d, isc=%d\n",
                      R[NUM_PC] - cpu_ilen, et, isc);

        }
    }

    return reason;
}

void cpu_perform_gate(uint32 index1, uint32 index2)
{
    uint32 pa, old_psw, new_psw;
    uint8 old_lvl;

    /* Force Kernel level */
    old_lvl = cpu_execution_level();
    cpu_set_execution_level(EX_LVL_KERN);

    /* TODO: Check current stack bounds and possibly
       generate stack-bounds exception */

    /* Write 1, 0, 2 to ISC, TM, ET fields of PSW */
    R[NUM_PSW] = R[NUM_PSW] | 1 << PSW_ISC;
    R[NUM_PSW] = R[NUM_PSW] & ~(PSW_TM_MASK);
    R[NUM_PSW] = R[NUM_PSW] | 2 << PSW_ET;

    old_psw = R[NUM_PSW];

    /* Save address of next instruction and PSW to stack */
    cpu_push_word(R[NUM_PC]);
    cpu_push_word(R[NUM_PSW]);

    /* TODO : Possibly generate stack-bounds exception again */

    pa = read_w(index1) + index2;

    new_psw = read_w(pa);

    new_psw |= (old_psw & PSW_CM_MASK) << 2;
    new_psw |= 3;
    new_psw |= 7 << PSW_ISC;
    new_psw |= 1 << PSW_TM;

    R[NUM_PC] = read_w(pa + 4);
    R[NUM_PSW] = new_psw;

    sim_debug(EXECUTE_MSG, &cpu_dev,
              "*** GATE: New PC=%08x, New PSW=%08x\n",
              R[NUM_PC], R[NUM_PSW]);


    cpu_set_execution_level(old_lvl);
}

/*
 * TODO: Setting 'data' to the effective address is bogus. We're only
 * doing it because we want to get the address when we trace the
 * instructions using "SHOW CPU HISTORY". We should just put
 * effective_address as a field in the operand struct and make
 * cpu_show_hist smarter.
 */
static uint32 cpu_effective_address(operand * op)
{
    /* Register Deferred */
    if (op->mode == 5 && op->reg != 11) {
        op->data = R[op->reg];
        return op->data;
    }

    /* Absolute */
    if (op->mode == 7 && op->reg == 15) {
        op->data = op->embedded.w;
        return op->data;
    }

    /* Absolute Deferred */
    if (op->mode == 14 && op->reg == 15) {
        /* May cause exception */
        op->data = read_w(op->embedded.w);
        return op->data;
    }

    /* FP Short Offset */
    if (op->mode == 6 && op->reg != 15) {
        op->data = R[NUM_FP] + (int8)op->embedded.b;
        return op->data;
    }

    /* AP Short Offset */
    if (op->mode == 7 && op->reg != 15) {
        op->data = R[NUM_AP] + (int8)op->embedded.b;
        return op->data;
    }

    /* Word Displacement */
    if (op->mode == 8) {
        op->data = R[op->reg] + (int32)op->embedded.w;
        return op->data;
    }

    /* Word Displacement Deferred */
    if (op->mode == 9) {
        op->data = read_w(R[op->reg] + (int32)op->embedded.w);
        return op->data;
    }

    /* Halfword Displacement */
    if (op->mode == 10) {
        op->data = R[op->reg] + (int16)op->embedded.h;
        return op->data;
    }

    /* Halfword Displacement Deferred */
    if (op->mode == 11) {
        op->data = read_w(R[op->reg] + (int16)op->embedded.h);
        return op->data;
    }

    /* Byte Displacement */
    if (op->mode == 12) {
        op->data = R[op->reg] + (int8)op->embedded.b;
        return op->data;
    }

    /* Byte Displacement Deferred */
    if (op->mode == 13) {
        op->data = read_w(R[op->reg] + (int8)op->embedded.b);
        return op->data;
    }

    assert(0);

    return 0;
}

/*
 * Read and Write routines for operands.
 *
 * The rules for dealing with the type (signed/unsigned,
 * byte/halfword/word) of operands are fairly complex.
 *
 * 1. The expanded operand mode does not affect the treatment of
 *    Literal Mode operands. All literals are signed.
 *
 * 2. The expanded operand mode does not affect the length of
 *    Immediate Mode operands, but does affect whether they are signed
 *    or unsigned.
 *
 * 3. When using expanded-mode operands, the new type remains in
 *    effect for the operands that folow in the instruction unless
 *    another expanded operand mode overrides it. (This rule in
 *    particular is managed by decode_instruction())
 *
 * 4. The expanded operand mode is illegal with coprocessor instructions
 *    and CALL, SAVE, RESTORE, SWAP INTERLOCKED, PUSAHW, PUSHAW, POPW,
 *    and JSB. (Illegal Operand Fault)
 *
 * 5. When writing a byte, the Negative (N) flag is set based on the
 *    high bit of the data type being written, regardless of the SIGN
 *    of the extended datatype. e.g.: {ubyte} and {sbyte} both check
 *    for bit 7, {uhalf} and {shalf} both check for bit 15, and
 *    {uword} and {sword} both check for bit 31.
 *
 * 6. For instructions with a signed destination, V is set if the sign
 *    bit of the output value is different from any truncated bit of
 *    the result. For instructions with an unsigned destination, V is
 *    set if any truncated bit is 1.
 */


/*
 * Read the data referenced by an operand. Performs sign or zero
 * extension as required by the read width and operand type, then
 * returns the read value.
 *
 * "All operations are performed only on 32-bit quantities even though
 *  an instruction may specify a byte or halfword operand. The WE
 *  32100 Microprocessor reads in the correct number of bits for the
 *  operand and extends the data automatically to 32 bits. It uses
 *  sign extension when reading signed data or halfwords and zero
 *  extension when reading unsigned data or bytes (or bit fields that
 *  contain less than 32 bits). The data type of the source operand
 *  determines how many bits are fetched and what type of extension is
 *  applied. Bytes are treated as unsigned, while halfwords and words
 *  are considered signed. The type of extension applied can be
 *  changed using the expanded-operand type mode as described in 3.4.5
 *  Expanded-Operand Type Mode. For sign extension, the value of the
 *  MSB or sign bit of the data fills the high-order bits to form a
 *  32-bit value. In zero extension, zeros fill the high order bits.
 *  The microprocessor automatically extends a byte or halfword to 32
 *  bits before performing an operation. Figure 3-3 illustrates sign
 *  and zero extension. An arithmetic, logical, data transfer, or bit
 *  field operation always yields an intermediate result that is 32
 *  bits in length. If the result is to be stored in a register, the
 *  processor writes all 32 bits to that register. The processor
 *  automatically strips any surplus high-order bits from a result
 *  when writing bytes or halfwords to memory." -- "WE 3200
 *  Microprocessor Information Manual", Section 3.1.1
 *
 */
static uint32 cpu_read_op(operand * op)
{
    uint32 eff;
    uint32 val = 0;
    uint32 data;

    /* Register */
    if (op->mode == 4 && op->reg < 15) {
        switch (op_type(op)) {
        case WD:
        case UW:
            data = R[op->reg];
            break;
        case HW:
            data = sign_extend_h(R[op->reg] & HALF_MASK);
            break;
        case UH:
            data = R[op->reg] & HALF_MASK;
            break;
        case BT:
            data = R[op->reg] & BYTE_MASK;
            break;
        case SB:
            data = sign_extend_b(R[op->reg] & BYTE_MASK);
            break;
        default:
            assert(0);
        }

        op->data = data;
        return data;
    }

    /* Literal */
    if (op->mode < 4 || op->mode == 15) {
        /* Both positive and negative literals are _always_ treated as
           signed bytes, and they are _always_ sign extended. They
           simply ignore expanded datatypes. */
        data = sign_extend_b(op->embedded.b);
        op->data = data;
        return data;
    }

    /* Immediate */
    if (op->reg == 15 &&
        (op->mode == 4 || op->mode == 5 || op->mode == 6)) {
        switch (op->mode) {
        case 4: /* Word Immediate */
            data = op->embedded.w;
            op->data = data;
            return data;
        case 5: /* Halfword Immediate */
            data = sign_extend_h(op->embedded.h);
            op->data = data;
            return data;
        case 6: /* Byte Immedaite */
            data = sign_extend_b(op->embedded.b);
            op->data = data;
            return data;
        }
    }

    /* At this point, we'll need to find an effective address */
    eff = cpu_effective_address(op);

    switch (op_type(op)) {
    case WD: /* Signed Word */
    case UW: /* Unsigned Word */
        data = read_w_u(eff);
        op->data = data;
        return data;
    case HW: /* Signed Halfword */
        data = sign_extend_h(read_h(eff));
        op->data = data;
        return data;
    case UH: /* Unsigned Halfword */
        data = read_h(eff);
        op->data = data;
        return data;
    case SB: /* Signed Byte */
        data = sign_extend_b(read_b(eff));
        op->data = data;
        return data;
    case BT: /* Unsigned Byte */
        data = read_b(eff);
        op->data = data;
        return data;
    default:
        assert(0);
        return 0;
    }
}


static void cpu_write_op(operand * op, t_uint64 val)
{
    uint32 eff;
    op->data = (uint32) val;

    /* Writing to a register. */
    if (op->mode == 4 && op->reg < 15) {
        if ((op->reg == NUM_PSW || op->reg == NUM_PCBP || op->reg == NUM_ISP) &&
            cpu_execution_level() != EX_LVL_KERN) {
            cpu_set_exception(NORMAL_EXCEPTION, PRIVILEGED_REGISTER);
            return;
        }

        /* Registers always get the full 32-bits written */

        R[op->reg] = (uint32) val;

        return;
    }

    /* Literal mode is not legal. */
    if (op->mode < 4 || op->mode == 15) {
        sim_debug(EXECUTE_MSG, &cpu_dev,
                  "Exception because literal mode is not allowed.\n");
        cpu_set_exception(NORMAL_EXCEPTION, EXTERNAL_MEMORY_FAULT);
        return;
    }

    /* Immediate mode is not legal. */
    if (op->reg == 15 &&
        (op->mode == 4 || op->mode == 5 || op->mode == 6)) {
        sim_debug(EXECUTE_MSG, &cpu_dev,
                  "Exception because immediate mode is not allowed.\n");
        cpu_set_exception(NORMAL_EXCEPTION, EXTERNAL_MEMORY_FAULT);
        return;
    }

    eff = cpu_effective_address(op);
    /* TODO: This is inelegant. We re-set data because
       'cpu_effective_address' messes with it. */
    op->data = (uint32) val;

    switch (op_type(op)) {
    case UW:
    case WD:
        write_w(eff, (uint32) val);
        break;
    case HW:
    case UH:
        if (val > HALF_MASK) {
            cpu_set_v_flag(TRUE);
        }
        write_h(eff, val & HALF_MASK);
        break;
    case SB:
    case BT:
        if (val > BYTE_MASK) {
            cpu_set_v_flag(TRUE);
        }
        write_b(eff, val & BYTE_MASK);
        break;
    default:
        assert(0);
    }
}

/*
 * Set PSW's ET and ISC fields, and store global exception or fault
 * state appropriately.
 */
void cpu_set_exception(uint8 et, uint8 isc)
{
    /* We don't trap Integer Overflow if the OE bit is not set */
    if (!(R[NUM_PSW] & PSW_OE_MASK) && isc == INTEGER_OVERFLOW) {
        return;
    }

    sim_debug(EXECUTE_MSG, &cpu_dev,
              ">>> [%08x]: Setting Exception. et=%d, isc=%d\n",
              R[NUM_PC], et, isc);

    R[NUM_PSW] = R[NUM_PSW] & ~(PSW_ISC_MASK); /* Clear ISC */
    R[NUM_PSW] = R[NUM_PSW] & ~(PSW_ET_MASK);  /* Clear ET  */
    R[NUM_PSW] = R[NUM_PSW] | et;              /* Set ET    */
    R[NUM_PSW] = R[NUM_PSW] | isc << PSW_ISC;  /* Set ISC   */

    if (et == 3 && (isc == BREAKPOINT_TRAP ||
                    isc == INTEGER_OVERFLOW ||
                    isc == TRACE_TRAP)) {
        cpu_trap = TRUE;
    } else {
        cpu_exception = TRUE;
    }
}

/*
 * Indicate that an IRQ has occured.
 */
void cpu_set_irq(uint8 ipl, uint8 id, t_bool nmi)
{
    if (cpu_irq_ipl > -1) {
        return;
    }

    sim_debug(IRQ_MSG, &cpu_dev, "IRQ: Setting IRQ level %d, id %d, NMI=%d\n",
              ipl, id, nmi);

    cpu_irq_ipl = ipl;
    cpu_irq_id = id;
    cpu_nmi = nmi;
}

/*
 * Returns the correct datatype for an operand -- either extended type
 * or default type.
 */
static SIM_INLINE int8 op_type(operand *op) {
    if (op->etype > -1) {
        return op->etype;
    } else {
        return op->dtype;
    }
}

/*
 * Clear the PSW's ET and ISC fields, and clear global exception and
 * fault state.
 */
static SIM_INLINE void clear_exceptions()
{
    /* The operating system exception handler will take care of
       updating the PSW if it wants to. We just clear our internal
       state */
    cpu_exception = FALSE;
    cpu_trap = FALSE;
}

static SIM_INLINE t_bool is_byte_immediate(operand * oper)
{
    return oper->mode == 6 && oper->reg == 15;
}

static SIM_INLINE t_bool is_halfword_immediate(operand * oper)
{
    return oper->mode == 5 && oper->reg == 15;
}

static SIM_INLINE t_bool is_word_immediate(operand * oper)
{
    return oper->mode == 4 && oper->reg == 15;
}

static SIM_INLINE t_bool is_positive_literal(operand * oper)
{
    return (oper->mode == 0 ||
            oper->mode == 1 ||
            oper->mode == 2);
}

static SIM_INLINE t_bool is_negative_literal(operand * oper)
{
    return oper->mode == 15;
}

/* Returns true if the operand may not be used as a destination */
static SIM_INLINE t_bool invalid_destination(operand * oper)
{
    return (is_byte_immediate(oper) ||
            is_halfword_immediate(oper) ||
            is_word_immediate(oper) ||
            is_positive_literal(oper) ||
            is_negative_literal(oper));
}

static SIM_INLINE uint32 sign_extend_b(uint8 val)
{
    return ((int8) val) & WORD_MASK;
}

static SIM_INLINE uint32 zero_extend_b(uint8 val)
{
    return val & BYTE_MASK;
}

static SIM_INLINE uint32 sign_extend_h(uint16 val)
{
    return ((int16) val) & WORD_MASK;
}

static SIM_INLINE uint32 zero_extend_h(uint16 val)
{
    return val & HALF_MASK;
}

static SIM_INLINE void set_psw_tm(t_bool val)
{
    if (val) {
        R[NUM_PSW] = R[NUM_PSW] | 1 << PSW_TM;
    } else {
        R[NUM_PSW] = R[NUM_PSW] & ~(PSW_TM_MASK);
    }
}

static SIM_INLINE void set_psw_ri(t_bool val)
{
    if (val) {
        R[NUM_PSW] = R[NUM_PSW] | 1 << PSW_RI;
    } else {
        R[NUM_PSW] = R[NUM_PSW] & ~(PSW_RI_MASK);
    }
}

/*
 * Returns the current CPU execution level.
 */
static SIM_INLINE uint8 cpu_execution_level()
{
    return (R[NUM_PSW] & PSW_CM_MASK) >> PSW_CM;
}

/*
 * Sets the current and previous execution levels.
 */
static SIM_INLINE void cpu_set_execution_level(uint8 level)
{
    uint8 old_level = cpu_execution_level();

    /* Store the previous execution level */
    R[NUM_PSW] |= (old_level & 0x3) << PSW_PM;
    R[NUM_PSW] |= (level & 0x3) << PSW_CM;
}

static SIM_INLINE t_bool cpu_z_flag()
{
    return (R[NUM_PSW] & PSW_Z_MASK) != 0;
}

static SIM_INLINE t_bool cpu_n_flag()
{
    return (R[NUM_PSW] & PSW_N_MASK) != 0;
}

static SIM_INLINE t_bool cpu_c_flag()
{
    return (R[NUM_PSW] & PSW_C_MASK) != 0;
}

static SIM_INLINE t_bool cpu_v_flag()
{
    return (R[NUM_PSW] & PSW_V_MASK) != 0;
}

static SIM_INLINE void cpu_set_z_flag(t_bool val)
{
    if (val) {
        R[NUM_PSW] = R[NUM_PSW] | PSW_Z_MASK;
    } else {
        R[NUM_PSW] = R[NUM_PSW] & ~PSW_Z_MASK;
    }
}

static SIM_INLINE void cpu_set_n_flag(t_bool val)
{
    if (val) {
        R[NUM_PSW] = R[NUM_PSW] | PSW_N_MASK;
    } else {
        R[NUM_PSW] = R[NUM_PSW] & ~PSW_N_MASK;
    }
}

static SIM_INLINE void cpu_set_c_flag(t_bool val)
{
    if (val) {
        R[NUM_PSW] = R[NUM_PSW] | PSW_C_MASK;
    } else {
        R[NUM_PSW] = R[NUM_PSW] & ~PSW_C_MASK;
    }
}

static SIM_INLINE void cpu_set_v_flag_op(t_uint64 val, operand *op)
{
    switch(op_type(op)) {
    case WD:
    case UW:
        cpu_set_v_flag(0);
        break;
    case HW:
    case UH:
        cpu_set_v_flag(val > HALF_MASK);
        break;
    case BT:
    case SB:
    default:
        cpu_set_v_flag(val > BYTE_MASK);
        break;
    }
}

static SIM_INLINE void cpu_set_v_flag(t_bool val)
{
    if (val) {
        R[NUM_PSW] = R[NUM_PSW] | PSW_V_MASK;
        if (R[NUM_PSW] & PSW_OE_MASK) {
            cpu_set_exception(NORMAL_EXCEPTION, INTEGER_OVERFLOW);
        }
    } else {
        R[NUM_PSW] = R[NUM_PSW] & ~PSW_V_MASK;
    }
}

static void cpu_set_nz_flags(t_uint64 data, operand *dst)
{
    int8 type = op_type(dst);

    switch (type) {
    case WD:
    case UW:
        cpu_set_n_flag(WD_MSB & data);
        cpu_set_z_flag((data & WORD_MASK) == 0);
        break;
    case HW:
    case UH:
        cpu_set_n_flag(HW_MSB & data);
        cpu_set_z_flag((data & HALF_MASK) == 0);
        break;
    case BT:
    case SB:
        cpu_set_n_flag(BT_MSB & data);
        cpu_set_z_flag((data & BYTE_MASK) == 0);
        break;
    }
}

static void cpu_push_word(uint32 val)
{
    write_w(R[NUM_SP], val);
    R[NUM_SP] += 4;
}

static uint32 cpu_pop_word()
{
    R[NUM_SP] -= 4;
    return read_w(R[NUM_SP]);
}

static void irq_push_word(uint32 val)
{
    write_w(R[NUM_ISP], val);
    R[NUM_ISP] += 4;
}

static uint32 irq_pop_word()
{
    R[NUM_ISP] -= 4;
    return read_w(R[NUM_ISP]);
}

static SIM_INLINE t_bool op_is_psw(operand *op)
{
    return (op->mode == 4 && op->reg == 11);
}

static SIM_INLINE uint32 cpu_next_pc()
{
    return R[NUM_PC] + cpu_ilen;
}

static SIM_INLINE void sub(t_uint64 a, t_uint64 b, operand *dst)
{
    t_uint64 result;

    result = a - b;

    cpu_write_op(dst, result);

    cpu_set_nz_flags(result, dst);
    cpu_set_c_flag((uint32)b > (uint32)a);

    switch(op_type(dst)) {
    case WD:
    case UW:
        cpu_set_v_flag(((a ^ b) & (~a ^ result)) & WD_MSB);
        break;
    case HW:
    case UH:
        cpu_set_v_flag(((a ^ b) & (~a ^ result)) & HW_MSB);
        break;
    case BT:
    case SB:
        cpu_set_v_flag(((a ^ b) & (~a ^ result)) & BT_MSB);
        break;
    }
}

static SIM_INLINE void add(t_uint64 a, t_uint64 b, operand *dst)
{
    t_uint64 result;

    result = a + b;

    cpu_set_nz_flags(result, dst);

    switch(op_type(dst)) {
    case WD:
    case UW:
        cpu_set_c_flag(result > WORD_MASK);
        cpu_set_v_flag(((a ^ ~b) & (a ^ result)) & WD_MSB);
        break;
    case HW:
    case UH:
        cpu_set_c_flag(result > HALF_MASK);
        cpu_set_v_flag(((a ^ ~b) & (a ^ result)) & HW_MSB);
        break;
    case BT:
    case SB:
        cpu_set_c_flag(result > BYTE_MASK);
        cpu_set_v_flag(((a ^ ~b) & (a ^ result)) & BT_MSB);
    }
    cpu_write_op(dst, result);
}
