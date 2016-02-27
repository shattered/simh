/* 3b2_cpu.h: AT&T 3B2 Model 400 System Devices implementation

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


   This file contains system-specific registers and devices for the
   following 3B2 devices:

     - timer       The 8253 timer
     - nvram       Non-Volatile RAM
     - csr         Control Status Registers
     - dmac        DMA controller
*/

#include "3b2_sysdev.h"

DEBTAB sys_deb_tab[] = {
    { "INIT",       INIT_MSG,       "Init"             },
    { "READ",       READ_MSG,       "Read activity"    },
    { "WRITE",      WRITE_MSG,      "Write activity"   },
    { "EXECUTE",    EXECUTE_MSG,    "Execute activity" },
    { NULL,         0                                  }
};

uint32 *NVRAM = NULL;

extern DEVICE cpu_dev;

/* CSR */

uint16 csr_data;

UNIT csr_unit = {
    UDATA(NULL, UNIT_FIX, CSRSIZE)
};

REG csr_reg[] = {
    { NULL }
};

DEVICE csr_dev = {
    "CSR", &csr_unit, csr_reg, NULL,
    1, 16, 8, 4, 16, 32,
    &csr_ex, &csr_dep, &csr_reset,
    NULL, NULL, NULL, NULL,
    DEV_DEBUG, 0, sys_deb_tab
};

t_stat csr_ex(t_value *vptr, t_addr exta, UNIT *uptr, int32 sw)
{
    return SCPE_OK;
}

t_stat csr_dep(t_value val, t_addr exta, UNIT *uptr, int32 sw)
{
    return SCPE_OK;
}

t_stat csr_reset(DEVICE *dptr)
{
    csr_data = 0;
    return SCPE_OK;
}

uint32 csr_read(uint32 pa, uint8 size)
{
    uint32 reg = pa - CSRBASE;

    sim_debug(READ_MSG, &csr_dev, "RCSR [%2x]\n", reg);

    switch (reg) {
    case 0x0:
        return csr_data & CSRIOF;
    case 0x1:
        return (csr_data & CSRDMA)  >> 1;
    case 0x2:
        return (csr_data & CSRDISK) >> 2;
    case 0x3:
        return (csr_data & CSRUART) >> 3;
    case 0x4:
        return (csr_data & CSRPIR9) >> 4;
    case 0x5:
        return (csr_data & CSRPIR8) >> 5;
    case 0x6:
        return (csr_data & CSRCLK)  >> 6;
    case 0x7:
        return (csr_data & CSRIFLT) >> 7;
    case 0x8:
        return (csr_data & CSRITIM) >> 8;
    case 0x9:
        return (csr_data & CSRFLOP) >> 9;
    case 0xa:
        return (csr_data & CSRLED)  >> 10;
    case 0xb:
        return (csr_data & CSRALGN) >> 11;
    case 0xc:
        return (csr_data & CSRRRST) >> 12;
    case 0xd:
        return (csr_data & CSRPARE) >> 13;
    case 0xe:
        return (csr_data & CSRTIMO) >> 14;
    default:
        return 0;
    }
}

void csr_write(uint32 pa, uint32 val, uint8 size)
{
    uint32 reg = pa - CSRBASE;

    sim_debug(WRITE_MSG, &csr_dev, "WCSR [%2x]\n", reg);

    switch (reg) {
    case 0x03:    /* clear sanity  */
        break;
    case 0x07:    /* clear parity  */
        csr_data &= ~CSRPARE;
        break;
    case 0x0b:    /* set reqrst    */
        csr_data |= CSRRRST;
        break;
    case 0x0f:    /* clear align   */
        break;
    case 0x13:    /* set LED       */
        csr_data |= CSRLED;
        break;
    case 0x17:    /* clear LED     */
        csr_data &= ~CSRLED;
        break;
    case 0x1b:    /* set flop      */
        csr_data |= CSRFLOP;
        break;
    case 0x1f:    /* clear flop    */
        csr_data &= ~CSRFLOP;
        break;
    case 0x23:    /* set timers    */
        csr_data |= CSRITIM;
        break;
    case 0x27:    /* clear timers  */
        csr_data &= ~CSRITIM;
        break;
    case 0x2b:    /* set inhibit   */
        csr_data |= CSRIOF;
        break;
    case 0x2f:    /* clear inhibit */
        csr_data &= ~CSRIOF;
        break;
    case 0x33:    /* set pir9      */
        csr_data |= CSRPIR9;
        break;
    case 0x37:    /* clear pir9    */
        csr_data &= ~CSRPIR9;
        break;
    case 0x3b:    /* set pir8      */
        csr_data |= CSRPIR8;
        break;
    case 0x3f:    /* clear pir8    */
        csr_data &= ~CSRPIR8;
        break;
    }
}

/* NVRAM */

UNIT nvram_unit = {
    UDATA(NULL, UNIT_FIX+UNIT_BINK, NVRAMSIZE)
};

REG nvram_reg[] = {
    { NULL }
};

DEVICE nvram_dev = {
    "NVRAM", &nvram_unit, nvram_reg, NULL,
    1, 16, 8, 4, 16, 32,
    &nvram_ex, &nvram_dep, &nvram_reset,
    NULL, &nvram_attach, &nvram_detach,
    NULL, DEV_DEBUG, 0, sys_deb_tab, NULL, NULL,
    NULL, NULL, NULL,
    &nvram_description
};

t_stat nvram_ex(t_value *vptr, t_addr exta, UNIT *uptr, int32 sw)
{
    uint32 addr = (uint32) exta;

    if ((vptr == NULL) || (addr & 03)) {
        return SCPE_ARG;
    }

    if (addr >= NVRAMSIZE) {
        return SCPE_NXM;
    }

    *vptr = NVRAM[addr >> 2];

    return SCPE_OK;
}

t_stat nvram_dep(t_value val, t_addr exta, UNIT *uptr, int32 sw)
{
    uint32 addr = (uint32) exta;

    if (addr & 03) {
        return SCPE_ARG;
    }

    if (addr >= NVRAMSIZE) {
        return SCPE_NXM;
    }

    NVRAM[addr >> 2] = (uint32) val;

    return SCPE_OK;
}

t_stat nvram_reset(DEVICE *dptr)
{
    sim_debug(INIT_MSG, &nvram_dev, "NVRAM Initialization\n");

    if (NVRAM == NULL) {
        NVRAM = (uint32 *)calloc(NVRAMSIZE, sizeof(uint32));
        memset(NVRAM, 0x5a, sizeof(uint32) * NVRAMSIZE);
        nvram_unit.filebuf = NVRAM;
    }

    if (NVRAM == NULL) {
        return SCPE_MEM;
    }

    return SCPE_OK;
}

const char *nvram_description(DEVICE *dptr)
{
    return "Non-volatile memory";
}

t_stat nvram_attach(UNIT *uptr, char *cptr)
{
    t_stat r;

    /* If we've been asked to attach, make sure the ATTABLE
       and BUFABLE flags are set on the unit */
    uptr->flags = uptr->flags | (UNIT_ATTABLE | UNIT_BUFABLE);

    r = attach_unit(uptr, cptr);

    if (r != SCPE_OK) {
        /* Unset the ATTABLE and BUFABLE flags if we failed. */
        uptr->flags = uptr->flags & ~(UNIT_ATTABLE | UNIT_BUFABLE);
    } else {
        uptr->hwmark = (uint32) uptr->capac;
    }

    return r;
}

t_stat nvram_detach(UNIT *uptr)
{
    t_stat r;

    r = detach_unit(uptr);

    if ((uptr->flags & UNIT_ATT) == 0) {
        uptr->flags = uptr->flags & ~(UNIT_ATTABLE | UNIT_BUFABLE);
    }

    return r;
}


uint32 nvram_read(uint32 pa, uint8 size)
{
    uint32 offset = pa - NVRAMBASE;
    uint32 data;
    uint32 sc = (~(offset & 3) << 3) & 0x1f;

    switch(size) {
    case 8:
        data = (NVRAM[offset >> 2] >> sc) & BYTE_MASK;
        break;
    case 16:
        if (offset & 2) {
            data = NVRAM[offset >> 2] & HALF_MASK;
        } else {
            data = (NVRAM[offset >> 2] >> 16) & HALF_MASK;
        }
        break;
    case 32:
        data = NVRAM[offset >> 2];
        break;
    }

    sim_debug(READ_MSG, &nvram_dev, "NVRAM READ %d B @ %08x = %08x\n",
              size, pa, data);

    return data;
}

void nvram_write(uint32 pa, uint32 val, uint8 size)
{
    uint32 offset = pa - NVRAMBASE;
    uint32 index = offset >> 2;
    uint32 sc, mask;

    sim_debug(WRITE_MSG, &nvram_dev, "NVRAM WRITE %d B @ %08x=%08x\n",
              size, pa, val);

    switch(size) {
    case 8:
        sc = (~(pa & 3) << 3) & 0x1f;
        mask = 0xff << sc;
        NVRAM[index] = (NVRAM[index] & ~mask) | (val << sc);
        break;
    case 16:
        if (offset & 2) {
            NVRAM[index] = (NVRAM[index] & ~HALF_MASK) | val;
        } else {
            NVRAM[index] = (NVRAM[index] & HALF_MASK) | (val << 16);
        }
        break;
    case 32:
        NVRAM[index] = val;
        break;
    }
}

/* 8253 Timer */

struct timers {
    uint32 divider_a;
    uint32 divider_b;
    uint32 divider_c;

    int32 counter_a;
    int32 counter_b;
    int32 counter_c;
};

struct timers TIMER;

UNIT timer_unit = { UDATA(&timer_svc, 0, 0), 1000L };

REG timer_reg[] = {
    { NULL }
};

DEVICE timer_dev = {
    "TIMER", &timer_unit, timer_reg, NULL,
    1, 16, 8, 4, 16, 32,
    NULL, NULL, &timer_reset,
    NULL, NULL, NULL, NULL,
    DEV_DEBUG, 0, sys_deb_tab
};

t_stat timer_svc(UNIT *uptr)
{
    TIMER.counter_a--;
    if (TIMER.counter_a <= 0) {
        TIMER.counter_a = TIMER.divider_a;
    }

    TIMER.counter_b--;
    if (TIMER.counter_b <= 0) {
        TIMER.counter_b = TIMER.divider_b;
    }

    TIMER.counter_c--;
    if (TIMER.counter_c <= 0) {
        TIMER.counter_c = TIMER.divider_c;
    }

    return SCPE_OK;
}

t_stat timer_reset(DEVICE *dptr) {
    memset(&TIMER, 0, sizeof(TIMER));

    return SCPE_OK;
}

uint32 timer_read(uint32 pa, uint8 size)
{
    uint8 reg;

    reg = pa - TIMERBASE;

    switch (reg) {
    case 3:  /* Counter 0 */
        return TIMER.counter_a & 0xff;
    case 7:  /* Counter 1 */
        return TIMER.counter_b & 0xff;
    case 11: /* Counter 2 */
        return TIMER.counter_c & 0xff;
    default:
        return 0;
    }
}

void timer_write(uint32 pa, uint32 val, uint8 size)
{
    uint8 reg;

    reg = pa - TIMERBASE;

    switch (reg) {
    case 3:  /* Counter 0 */
        TIMER.divider_a = val;
        TIMER.counter_a = TIMER.divider_a;
        break;
    case 7:  /* Counter 1 */
        TIMER.divider_b = val;
        TIMER.counter_b = TIMER.divider_b;
        break;
    case 11: /* Counter 2 */
        TIMER.divider_c = val;
        TIMER.counter_c = TIMER.divider_c;
        break;
    case 15:
        /* TODO: Set modes */
        break;
    default:
        break;
    }
}
