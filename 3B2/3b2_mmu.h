/* 3b2_mmu.c: AT&T 3B2 Model 400 MMU (WE32101) Header

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

#ifndef _3B2_MMU_H
#define _3B2_MMU_H

#include "3b2_defs.h"

#define MMUBASE 0x40000
#define MMUSIZE 0x1000

#define MMU_SRS  0x04       /* Section RAM array size (words) */
#define MMU_SDCS 0x20       /* Segment Descriptor Cache H/L array size
                               (words) */
#define MMU_PDCS 0x20       /* Page Descriptor Cache H/L array size
                               (words) */

/* Register address offsets */
#define MMU_SDCL   0
#define MMU_SDCH   1
#define MMU_PDCRL  2
#define MMU_PDCRH  3
#define MMU_PDCLL  4
#define MMU_PDCLH  5
#define MMU_SRAMA  6
#define MMU_SRAMB  7
#define MMU_FC     8
#define MMU_FA     9
#define MMU_CONF   10
#define MMU_VAR    11

typedef struct {
    uint32 addr;
    uint32 len;
} mmu_sec;

typedef struct {
    uint8  tag;          /* ID tag */
    uint32 max_offset;   /* Maximum offset */
    uint8  access;       /* Access permissions */
    t_bool good;         /* Cache entry contains a descriptor */
    t_bool modified;
    t_bool contiguous;
    t_bool cacheable;
    t_bool otrap;        /* Object Trap */
    uint32 addr;         /* Pointer to address in main memory for
                            continugous segments or to page descriptor
                            table for paged segments */
} mmu_sdc;

typedef struct {
    t_bool enabled;         /* Global enabled/disabled flag */

    uint32 sdcl[MMU_SDCS];  /* SDC low bits (0-31) */
    uint32 sdch[MMU_SDCS];  /* SDC high bits (32-63) */

    uint32 pdcll[MMU_PDCS]; /* PDC low bits (left) (0-31) */
    uint32 pdclh[MMU_PDCS]; /* PDC high bits (left) (32-63) */

    uint32 pdcrl[MMU_PDCS]; /* PDC low bits (right) (0-31) */
    uint32 pdcrh[MMU_PDCS]; /* PDC high bits (right) (32-63) */

    uint32 sra[MMU_SRS];    /* Section RAM A */
    uint32 srb[MMU_SRS];    /* Section RAM B */

    mmu_sec sec[MMU_SRS];   /* Section descriptors decoded from
                               Section RAM A and B */

    uint32 fcode;           /* Fault Code Register */
    uint32 faddr;           /* Fault Address Register */
    uint32 conf;            /* Configuration Register */
    uint32 var;             /* Virtual Address Register */

} MMU_STATE;


MMU_STATE mmu_state;

extern DEVICE mmu_dev;

uint32 mmu_read(uint32 pa, size_t size);
void mmu_write(uint32 pa, uint32 val, size_t size);

/* Physical memory read/write */
uint8  pread_b(uint32 pa);
uint16 pread_h(uint32 pa);
uint32 pread_w(uint32 pa);
uint32 pread_w_u(uint32 pa);
void   pwrite_b(uint32 pa, uint8 val);
void   pwrite_h(uint32 pa, uint16 val);
void   pwrite_w(uint32 pa, uint32 val);

/* Virtual memory translation */
uint32 mmu_xlate_addr(uint32 va);

/* Dispatch to the MMU when enabled, or to physical RW when
   disabled */
uint8  read_b(uint32 va);
uint16 read_h(uint32 va);
uint32 read_w(uint32 va);
uint32 read_w_u(uint32 va);
void   write_b(uint32 va, uint8 val);
void   write_h(uint32 va, uint16 val);
void   write_w(uint32 va, uint32 val);

t_bool addr_is_rom(uint32 pa);
t_bool addr_is_mem(uint32 pa);
t_bool addr_is_io(uint32 pa);

t_bool mmu_enabled();
void mmu_enable();
void mmu_disable();

SIM_INLINE t_bool mmu_enabled()
{
    return mmu_state.enabled;
}

SIM_INLINE void mmu_enable()
{
    sim_debug(EXECUTE_MSG, &mmu_dev,
        "Enabling MMU.\n");
    mmu_state.enabled = TRUE;
}

SIM_INLINE void mmu_disable()
{
    sim_debug(EXECUTE_MSG, &mmu_dev,
        "Disabling MMU.\n");
    mmu_state.enabled = FALSE;
}

/*
 * INLINE Dispatch functions
 */

SIM_INLINE uint8 read_b(uint32 va)
{
    return pread_b(mmu_xlate_addr(va));
}

SIM_INLINE uint16 read_h(uint32 va)
{
    return pread_h(mmu_xlate_addr(va));
}

SIM_INLINE uint32 read_w(uint32 va)
{
    return pread_w(mmu_xlate_addr(va));
}

SIM_INLINE uint32 read_w_u(uint32 va)
{
    return pread_w_u(mmu_xlate_addr(va));
}

SIM_INLINE void write_b(uint32 va, uint8 val)
{
    pwrite_b(mmu_xlate_addr(va), val);
}

SIM_INLINE void write_h(uint32 va, uint16 val)
{
    pwrite_h(mmu_xlate_addr(va), val);
}

SIM_INLINE void write_w(uint32 va, uint32 val)
{
    pwrite_w(mmu_xlate_addr(va), val);
}

#endif
