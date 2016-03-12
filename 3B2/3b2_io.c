/* 3b2_cpu.h: AT&T 3B2 Model 400 IO dispatch implemenation

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

#include "3b2_io.h"

struct iolink iotable[] = {
    { MMUBASE,    MMUBASE+MMUSIZE,       &mmu_read,   &mmu_write   },
    { TIMERBASE,  TIMERBASE+TIMERSIZE,   &timer_read, &timer_write },
    { NVRAMBASE,  NVRAMBASE+NVRAMSIZE,   &nvram_read, &nvram_write },
    { CSRBASE,    CSRBASE+CSRSIZE,       &csr_read,   &csr_write   },
    { UARTBASE,   UARTBASE+UARTSIZE,     &uart_read,  &uart_write  },
    { DMAIDBASE,  DMAIDBASE+DMAIDSIZE,   &dmac_read,  &dmac_write  },
    { DMAIUABASE, DMAIUABASE+DMAIUASIZE, &dmac_read,  &dmac_write  },
    { DMAIUBBASE, DMAIUBBASE+DMAIUBSIZE, &dmac_read,  &dmac_write  },
    { DMACBASE,   DMACBASE+DMACSIZE,     &dmac_read,  &dmac_write  },
    { DMAIFBASE,  DMAIFBASE+DMAIFSIZE,   &dmac_read,  &dmac_write  },
    { IFBASE,     IFBASE+IFSIZE,         &if_read,    &if_write    },
    { IDBASE,     IDBASE+IDSIZE,         &id_read,    &id_write    },
    { 0, 0, NULL, NULL}
};

uint32 io_read(uint32 pa, size_t size)
{
    struct iolink *p;

    /* Special devices */
    if (pa == 0x4c003) {
        /* MEMSIZE register */

        /* It appears that the following values map to memory sizes:
           0x00: 512KB (  524,288 B)
           0x01: 2MB   (2,097,152 B)
           0x02: 1MB   (1,048,576 B)
           0x03: 4MB   (4,194,304 B)
        */
        switch(MEM_SIZE) {
        case 0x80000:  /* 512KB */
            return 0;
        case 0x100000: /* 1MB */
            return 2;
        case 0x200000: /* 2MB */
            return 1;
        case 0x400000: /* 4MB */
            return 3;
        default:
            return 0;
        }
    }

    /* IO Board Area - Unimplemented */
    if (pa >= 0x200000 && pa < 0x2000000) {
        sim_debug(IO_D_MSG, &cpu_dev, "[%08x] [IO BOARD READ] ADDR=%08x\n", R[NUM_PC], pa);

        /* This is an experiment. These devices are not yet
           implemented, but I want to see if 'filledt' can find
           them. */
        switch(pa) {
        case 0x200000:      /* IO Board 2 - PORTS */
            return 0x00;
        case 0x200001:
            return 0x03;
        case 0x400000:      /* IO Board 1 - TAPE controller */
            return 0x00;
        case 0x400001:
            return 0x05;
        default:
            return 0;
        };
    }

    for (p = &iotable[0]; p->low != 0; p++) {
        if ((pa >= p->low) && (pa < p->high) && p->read) {
            return p->read(pa, size);
        }
    }

    /* Not found. */
    sim_debug(IO_D_MSG, &cpu_dev,
              "READ ERROR. No IO device listening at address %08x\n", pa);

    cpu_set_exception(NORMAL_EXCEPTION, EXTERNAL_MEMORY_FAULT);

    return 0;
}

void io_write(uint32 pa, uint32 val, size_t size)
{
    struct iolink *p;

    /* IO Board Area - Unimplemented */
    if (pa >= 0x200000u && pa < 0x2000000u) {
        sim_debug(IO_D_MSG, &cpu_dev, "[IO BOARD WRITE] ADDR=%08x, DATA=%08x\n", pa, val);
        return;
    }

    for (p = &iotable[0]; p->low != 0; p++) {
        if ((pa >= p->low) && (pa < p->high) && p->write) {
            p->write(pa, val, size);
            return;
        }
    }

    /* Not found. */
    sim_debug(IO_D_MSG, &cpu_dev,
              "WRITE ERROR. No IO device listening at address %08x\n", pa);

    cpu_set_exception(NORMAL_EXCEPTION, EXTERNAL_MEMORY_FAULT);
}
