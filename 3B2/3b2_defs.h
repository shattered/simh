/* 3b2_defs.h: AT&T 3B2 Model 400 Simulator Definitions

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

#ifndef _3B2_DEFS_H_
#define _3B2_DEFS_H_

#include "sim_defs.h"

#define FALSE 0
#define TRUE 1

/* -v flag for examine routine */
#define EX_V_FLAG 1 << 21

/* NB: This value is set very high during development for better
   debugging */
#define MAX_HIST_SIZE  1000000
#define DEF_HIST_SIZE  100
#define MAXMEMSIZE     (1 << 22)             /* 4 MB */
#define MEM_SIZE       (cpu_unit.capac)      /* actual memory size */
#define UNIT_V_MSIZE   (UNIT_V_UF)

#define UNIT_MSIZE     (1 << UNIT_V_MSIZE)

#define WD_MSB     0x80000000
#define HW_MSB     0x8000
#define BT_MSB     0x80
#define WORD_MASK  0xffffffff
#define HALF_MASK  0xffff
#define BYTE_MASK  0xff

/*
 *
 * Physical memory in the 3B2 is arranged as follows:
 *
 *    0x00000000 - 0x0001FFFF: 64KB ROM
 *    0x00040000 - 0x0004FFFF: IO
 *    0x02000000 - 0x023FFFFF: 4MB RAM ("Mainstore"),
 *
 */

#define PHYS_MEM_BASE 0x2000000

#define ROM_BASE      0
#define IO_BASE       0x40000
#define IO_SIZE       0x10000
#define IOB_BASE      0x200000
#define IOB_SIZE      0x1E00000

/* Register numbers */
#define NUM_FP         9
#define NUM_AP         10
#define NUM_PSW        11
#define NUM_SP         12
#define NUM_PCBP       13
#define NUM_ISP        14
#define NUM_PC         15

/* Simulator stop codes */
#define STOP_RSRV   1
#define STOP_IBKPT  2
#define STOP_OPCODE 3
#define STOP_IRQ    4
#define STOP_EX     5

/* Debug flags */
#define READ_MSG    (1 << 0)
#define WRITE_MSG   (1 << 1)
#define DECODE_MSG  (1 << 2)
#define EXECUTE_MSG (1 << 3)
#define INIT_MSG    (1 << 4)
#define IRQ_MSG     (1 << 5)
#define IO_D_MSG    (1 << 6)

/* Data types operated on by instructions. NB: These integer values
   have meaning when decoding instructions, so this is not just an
   enum. Please don't change them. */
#define UW 0   /* Unsigned Word */
#define UH 2   /* Unsigned Halfword */
#define BT 3   /* Unsigned Byte */
#define WD 4   /* Signed Word */
#define HW 6   /* Signed Halfword */
#define SB 7   /* Signed Byte */

#define NA -1

#define CLK_TIM0  0          /* 8253 Timer 0 (A)                */
#define CLK_TIM1  1          /* 8253 Timer 1 (B) - Provides RTC */
#define CLK_TIM2  2          /* 8253 Timer 2 (C)                */
#define CLK_UART  3          /* UART Timer                      */

#define CLK_DELAY 5000

/*
 * Exceptions are described on page 2-66 of the WE32100 manual
 */

/* Exception Types */

#define RESET_EXCEPTION       0
#define STACK_EXCEPTION       1
#define PROCESS_EXCEPTION     2
#define NORMAL_EXCEPTION      3

/* Reset Exceptions */
#define OLD_PCB_FAULT         0
#define SYSTEM_DATA_FAULT     1
#define INTERRUPT_STACK_FAULT 2
#define EXTERNAL_RESET        3
#define NEW_PCB_FAULT         4
#define GATE_VECTOR_FAULT     6

/* Processor Exceptions */
#define GATE_PCB_FAULT        1

/* Stack Exceptions */
#define STACK_BOUND           0
#define STACK_FAULT           1
#define INTERRUPT_ID_FETCH    3

/* Normal Exceptions */
#define INTEGER_ZERO_DIVIDE   0
#define TRACE_TRAP            1
#define ILLEGAL_OPCODE        2
#define RESERVED_OPCODE       3
#define INVALID_DESCRIPTOR    4
#define EXTERNAL_MEMORY_FAULT 5
#define ILLEGAL_LEVEL_CHANGE  7
#define RESERVED_DATATYPE     8
#define INTEGER_OVERFLOW      9
#define PRIVILEGED_OPCODE    10
#define BREAKPOINT_TRAP      14
#define PRIVILEGED_REGISTER  15

#define PSW_ET                0
#define PSW_TM                2
#define PSW_ISC               3
#define PSW_RI                7
#define PSW_PM                9
#define PSW_CM                11
#define PSW_IPL               13
#define PSW_TE                17
#define PSW_C                 18
#define PSW_V                 19
#define PSW_Z                 20
#define PSW_N                 21
#define PSW_OE                22
#define PSW_CD                23
#define PSW_QIE               24
#define PSW_CFD               25

#define PSW_ET_MASK            3u
#define PSW_TM_MASK           (1u << PSW_TM)
#define PSW_ISC_MASK          (15u << PSW_ISC)
#define PSW_RI_MASK           (3u << PSW_RI)
#define PSW_I_MASK            (1u << PSW_RI)
#define PSW_R_MASK            (1u << (PSW_RI + 1))
#define PSW_PM_MASK           (3u << PSW_PM)
#define PSW_CM_MASK           (3u << PSW_CM)
#define PSW_IPL_MASK          (15u << PSW_IPL)
#define PSW_TE_MASK           (1u << PSW_TE)
#define PSW_C_MASK            (1u << PSW_C)
#define PSW_V_MASK            (1u << PSW_V)
#define PSW_N_MASK            (1u << PSW_N)
#define PSW_Z_MASK            (1u << PSW_Z)
#define PSW_OE_MASK           (1u << PSW_OE)
#define PSW_CD_MASK           (1u << PSW_CD)
#define PSW_QIE_MASK          (1u << PSW_QIE)
#define PSW_CFD_MASK          (1u << PSW_CFD)


/* global symbols from the CPU */

extern uint32 *ROM;
extern uint32 *RAM;
extern uint32 R[16];
extern REG cpu_reg[];
extern DEVICE cpu_dev;
extern UNIT cpu_unit;
extern uint8 fault;
extern DEBTAB sys_deb_tab[];

/* global symbols from the DMAC */
extern DEVICE dmac_dev;

/* Globally scoped CPU functions */
void cpu_set_exception(uint8 et, uint8 isc);
void cpu_set_irq(uint16 ipl, uint8 id, t_bool nmi);

/* Globally scoped IO functions */
uint32 io_read(uint32 pa, size_t size);
void io_write(uint32 pa, uint32 val, size_t size);

#endif
