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
   THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of the author shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from the author.


   This file contains system-specific registers and devices for the
   following 3B2 devices:

     - timer       8253 interval timer
     - nvram       Non-Volatile RAM
     - csr         Control Status Registers
     - rtc         100Hz Real Time Clock
*/

#include "3b2_sysdev.h"
#include "3b2_uart.h"

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

uint32 csr_read(uint32 pa, size_t size)
{
    uint32 reg = pa - CSRBASE;

    switch (reg) {
    case 0x2:
        sim_debug(READ_MSG, &csr_dev, "CSR: Read\n");
        return csr_data;
    default:
        return 0;
    }
}

void csr_write(uint32 pa, uint32 val, size_t size)
{
    uint32 reg = pa - CSRBASE;

    switch (reg) {
    case 0x03:    /* clear sanity  */
        sim_debug(WRITE_MSG, &csr_dev, "CSR: clear sanity\n");
        break;
    case 0x07:    /* clear parity  */
        sim_debug(WRITE_MSG, &csr_dev, "CSR: clear parity\n");
        csr_data &= ~CSRPARE;
        break;
    case 0x0b:    /* set reqrst    */
        sim_debug(WRITE_MSG, &csr_dev, "CSR: set reqrst\n");
        csr_data |= CSRRRST;
        break;
    case 0x0f:    /* clear align   */
        sim_debug(WRITE_MSG, &csr_dev, "CSR: clear align\n");
        break;
    case 0x13:    /* set LED       */
        sim_debug(WRITE_MSG, &csr_dev, "CSR: set LED\n");
        csr_data |= CSRLED;
        break;
    case 0x17:    /* clear LED     */
        sim_debug(WRITE_MSG, &csr_dev, "CSR: clear LED\n");
        csr_data &= ~CSRLED;
        break;
    case 0x1b:    /* set flop      */
        sim_debug(WRITE_MSG, &csr_dev, "CSR: set floppy\n");
        csr_data |= CSRFLOP;
        csr_data |= CSRDISK;
        break;
    case 0x1f:    /* clear flop    */
        sim_debug(WRITE_MSG, &csr_dev, "CSR: clear floppy\n");
        csr_data &= ~CSRFLOP;
        csr_data &= ~CSRDISK;
        break;
    case 0x23:    /* set timers    */
        sim_debug(WRITE_MSG, &csr_dev, "CSR: set timers\n");
        csr_data |= CSRITIM;
        break;
    case 0x27:    /* clear timers  */
        sim_debug(WRITE_MSG, &csr_dev, "CSR: clear timers\n");
        csr_data &= ~CSRITIM;
        break;
    case 0x2b:    /* set inhibit   */
        sim_debug(WRITE_MSG, &csr_dev, "CSR: set inhibit\n");
        csr_data |= CSRIOF;
        break;
    case 0x2f:    /* clear inhibit */
        sim_debug(WRITE_MSG, &csr_dev, "CSR: clear inhibit\n");
        csr_data &= ~CSRIOF;
        break;
    case 0x33:    /* set pir9      */
        sim_debug(WRITE_MSG, &csr_dev, "CSR: set PIR9\n");
        csr_data |= CSRPIR9;
        break;
    case 0x37:    /* clear pir9    */
        sim_debug(WRITE_MSG, &csr_dev, "CSR: clear PIR9\n");
        csr_data &= ~CSRPIR9;
        break;
    case 0x3b:    /* set pir8      */
        sim_debug(WRITE_MSG, &csr_dev, "CSR: set PIR8\n");
        csr_data |= CSRPIR8;
        break;
    case 0x3f:    /* clear pir8    */
        sim_debug(WRITE_MSG, &csr_dev, "CSR: clear PIR8\n");
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
        NVRAM = (uint32 *)calloc(NVRAMSIZE >> 2, sizeof(uint32));
        memset(NVRAM, 0, sizeof(uint32) * NVRAMSIZE >> 2);
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


uint32 nvram_read(uint32 pa, size_t size)
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

    sim_debug(READ_MSG, &nvram_dev, "NVRAM READ %lu B @ %08x = %08x\n",
              size, pa, data);

    return data;
}

void nvram_write(uint32 pa, uint32 val, size_t size)
{
    uint32 offset = pa - NVRAMBASE;
    uint32 index = offset >> 2;
    uint32 sc, mask;

    sim_debug(WRITE_MSG, &nvram_dev, "NVRAM WRITE %lu B @ %08x=%08x\n",
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

    t_bool a_enabled;
    t_bool b_enabled;
    t_bool c_enabled;
};

struct timers TIMER;

extern struct uart_state u;

UNIT timer_unit = { UDATA(&timer_svc, 0, 0), 5000L };

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
    if (TIMER.a_enabled) {
        TIMER.counter_a--;
        if (TIMER.counter_a <= 0) {
            TIMER.counter_a = TIMER.divider_a;
        }
    }

    if (TIMER.b_enabled) {
        TIMER.counter_b--;
        if (TIMER.counter_b <= 0) {
            TIMER.counter_b = TIMER.divider_b;
        }
    }

    if (TIMER.c_enabled) {
        TIMER.counter_c--;
        if (TIMER.counter_c <= 0) {
            TIMER.counter_c = TIMER.divider_c;
            /* TODO: I have no earthly idea if PIR8 & PIR9 belong on
               this timer or not. Yet another reason I crave systems
               documentation for the 3B2. */
            if (csr_data & CSRPIR8) {
                sim_debug(EXECUTE_MSG, &timer_dev,
                          ">>> FIRING PIR8\n");
                cpu_set_irq(8, 8, 0);
                csr_data |= CSRCLK;
            } else if (csr_data & CSRPIR9) {
                sim_debug(EXECUTE_MSG, &timer_dev,
                          ">>> FIRING PIR9\n");
                cpu_set_irq(9, 9, 0);
                csr_data |= CSRCLK;
            }
       }
    }

    sim_activate_after(&timer_unit, 5000L);

    return SCPE_OK;
}

t_stat timer_reset(DEVICE *dptr) {
    memset(&TIMER, 0, sizeof(TIMER));

    sim_activate_after(&timer_unit, 5000L);

    return SCPE_OK;
}

uint32 timer_read(uint32 pa, size_t size)
{
    uint8 reg;

    reg = pa - TIMERBASE;

    switch (reg) {
    case 3:  /* Counter 0 */
        return TIMER.divider_a & 0xff;
    case 7:  /* Counter 1 */
        return TIMER.divider_b & 0xff;
    case 11: /* Counter 2 */
        return TIMER.divider_c & 0xff;
    default:
        return 0;
    }
}

void timer_write(uint32 pa, uint32 val, size_t size)
{
    uint8 reg;

    reg = pa - TIMERBASE;

    sim_debug(WRITE_MSG, &timer_dev, "[%08x] Timer write [%d] = %02x\n",
              R[NUM_PC], reg, val);

    switch (reg) {
    case 3:  /* Counter 0 */
        TIMER.divider_a = val;
        TIMER.counter_a = TIMER.divider_a;
        TIMER.a_enabled = TRUE;
        break;
    case 7:  /* Counter 1 */
        TIMER.divider_b = val;
        TIMER.counter_b = TIMER.divider_b;
        TIMER.b_enabled = TRUE;
        break;
    case 11: /* Counter 2 */
        TIMER.divider_c = val;
        TIMER.counter_c = TIMER.divider_c;
        TIMER.c_enabled = TRUE;
        break;
    case 15:
        /* TODO: Set modes */
        break;
    default:
        break;
    }
}

/* 100Hz Clock */

UNIT rtc_unit = { UDATA (&rtc_svc, UNIT_IDLE+UNIT_FIX, sizeof(uint32)), CLK_DELAY };

DEVICE rtc_dev = {
    "RTC", &rtc_unit, NULL, NULL,
    1, 0, 8, 4, 0, 32,
    NULL, NULL, &rtc_reset,
    NULL, NULL, NULL,
    NULL, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL
};

t_stat rtc_reset(DEVICE *dptr) {
    if (!sim_is_active(&rtc_unit)) {
        sim_activate(&rtc_unit, sim_rtcn_init(CLK_DELAY, CLK_RTC));
    }
    return SCPE_OK;
}

t_stat rtc_svc (UNIT *uptr) {
    /* Set the CSR */
    csr_data |= CSRCLK;
    /* Send clock interrupt */
    cpu_set_irq(15, 15, 0);

    sim_register_clock_unit(&rtc_unit);
    sim_activate(&rtc_unit, sim_rtcn_calb(RTC_HZ, CLK_RTC));
    return SCPE_OK;
}
