/* 3b2_mmu.c: AT&T 3B2 Model 400 MMU (WE32101) Implementation

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

#include "3b2_mmu.h"

#define BOOT_CODE_SIZE 0x8000

/*******************************************************************
 *
 * The WE32101 MMU divides the virtual address space into four
 * sections. Virtual address bits 30 and 31 determine the section.
 *
 * To initialize the MMU, the operating system must:
 *
 *   - Write SDTs
 *
 *
 * Vocabulary:
 *
 *    SID: Section ID. Can be one of 0, 1, 2, or 3
 *      - SSL: Segment Select. 8K segments within the section.
 *
 *        Continguous Segment Addressing:
 *          - SOT: Segment Offset. 128K addresses within segment.
 *
 *        Paged Segment Addressing:
 *          - PSL: Page Select. 64 pages within the section.
 *          - POT: Page Offset. 2K addresses within the page.
 *
 *    SD: Segment Descriptor. Both Contiguous and Paged addressing
 *        use Segment Descriptors located in a Segment Descriptor
 *        Table (SDT) to describe the physical layout of memory.
 *
 *    SDT: Segment Descriptor Table.
 *
 *    PDT: Page Descriptor Table.
 *    PD: Page Descriptor. When using Paged addressing,
 *
 *******************************************************************/

MMU_STATE mmu_state;

UNIT mmu_unit = { UDATA(NULL, 0, 0) };

REG mmu_reg[] = {
    { HRDATAD (ENABLE, mmu_state.enabled, 1, "Enabled?")   },
    { HRDATAD (CONFIG, mmu_state.conf,   32, "Configuration")   },
    { HRDATAD (VAR,    mmu_state.var,    32, "Virtual Address") },
    { HRDATAD (FCODE,  mmu_state.fcode,  32, "Fault Code")      },
    { HRDATAD (FADDR,  mmu_state.faddr,  32, "Fault Address")   },
    { BRDATA  (SDCL,   mmu_state.sdcl,   16, 32, MMU_SDCS)      },
    { BRDATA  (SDCR,   mmu_state.sdch,   16, 32, MMU_SDCS)      },
    { BRDATA  (PDCLL,  mmu_state.pdcll,  16, 32, MMU_PDCS)      },
    { BRDATA  (PDCLH,  mmu_state.pdclh,  16, 32, MMU_PDCS)      },
    { BRDATA  (PDCRL,  mmu_state.pdcrl,  16, 32, MMU_PDCS)      },
    { BRDATA  (PDCRH,  mmu_state.pdcrh,  16, 32, MMU_PDCS)      },
    { BRDATA  (SRAMA,  mmu_state.sra,    16, 32, MMU_SRS)       },
    { BRDATA  (SRAMB,  mmu_state.srb,    16, 32, MMU_SRS)       },
    { NULL }
};

DEVICE mmu_dev = {
    "MMU", &mmu_unit, mmu_reg, NULL,
    1, 16, 8, 4, 16, 32,
    NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    DEV_DEBUG, 0, sys_deb_tab
};

uint32 mmu_read(uint32 pa, uint8 size)
{
    uint32 offset;
    uint32 data = 0;

    offset = (pa & 0xff) >> 2;

    switch ((pa >> 8) & 0xf) {
    case MMU_SDCL:
        data = mmu_state.sdcl[offset];
        break;
    case MMU_SDCH:
        data = mmu_state.sdch[offset];
        break;
    case MMU_PDCRL:
        data = mmu_state.pdcrl[offset];
        break;
    case MMU_PDCRH:
        data = mmu_state.pdcrh[offset];
        break;
    case MMU_PDCLL:
        data = mmu_state.pdcll[offset];
        break;
    case MMU_PDCLH:
        data = mmu_state.pdclh[offset];
        break;
    case MMU_SRAMA:
        data = mmu_state.sra[offset];
        break;
    case MMU_SRAMB:
        data = mmu_state.srb[offset];
        break;
    case MMU_FC:
        data = mmu_state.fcode;
        break;
    case MMU_FA:
        data = mmu_state.faddr;
        break;
    case MMU_CONF:
        data = mmu_state.conf;
        break;
    case MMU_VAR:
        data = mmu_state.var;
        break;
    }

    sim_debug(READ_MSG, &mmu_dev, "%08x, returning %08x [offset=%02x]\n", pa, data, offset);

    return data;
}

void mmu_write(uint32 pa, uint32 val, uint8 size)
{
    uint32 offset;

    offset = (pa & 0xff) >> 2;

    sim_debug(WRITE_MSG, &mmu_dev, "%08x=%02x [offset=%02x]\n", pa, val, offset);

    switch ((pa >> 8) & 0xf) {
    case MMU_SDCL:
        mmu_state.sdcl[offset] = val;
        break;
    case MMU_SDCH:
        mmu_state.sdch[offset] = val;
        break;
    case MMU_PDCRL:
        mmu_state.pdcrl[offset] = val;
        break;
    case MMU_PDCRH:
        mmu_state.pdcrh[offset] = val;
        break;
    case MMU_PDCLL:
        mmu_state.pdcll[offset] = val;
        break;
    case MMU_PDCLH:
        mmu_state.pdclh[offset] = val;
        break;
    case MMU_SRAMA:
        mmu_state.sra[offset] = val;
        mmu_state.sec[offset].addr = val & 0xffffffe0;
        sim_debug(WRITE_MSG, &mmu_dev,
                  "  >> SRAMA: SDT[%d] Addr = %08x\n",
                  offset, mmu_state.sec[offset].addr);
        break;
    case MMU_SRAMB:
        mmu_state.srb[offset] = val;
        mmu_state.sec[offset].len = (val >> 10) & 0x1fff;
        sim_debug(WRITE_MSG, &mmu_dev,
                  "  >> SRAMB: SDT[%d] Len = %08x\n",
                  offset, mmu_state.sec[offset].len);
        break;
    case MMU_FC:
        mmu_state.fcode = val;
        break;
    case MMU_FA:
        mmu_state.faddr = val;
        break;
    case MMU_CONF:
        mmu_state.conf = val;
        break;
    case MMU_VAR:
        mmu_state.var = val;
        break;
    }
}

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

t_bool addr_is_rom(uint32 pa)
{
    return (pa < BOOT_CODE_SIZE);
}

t_bool addr_is_mem(uint32 pa)
{
    return (pa >= PHYS_MEM_BASE &&
            pa < (PHYS_MEM_BASE + MEM_SIZE));
}

t_bool addr_is_io(uint32 pa)
{
    return ((pa >= IO_BASE && pa < IO_BASE + IO_SIZE) ||
            (pa >= IOB_BASE && pa < IOB_BASE + IOB_SIZE));
}

/*
 * Raw physical reads and writes.
 *
 * The WE32100 is a BIG-endian machine, meaning that words are
 * arranged in increasing address from most-significant byte to
 * least-significant byte.
 */

uint32 pread_w_u(uint32 pa)
{
    uint32 *m;
    uint32 index;

    if (addr_is_io(pa)) {
        return io_read(pa, 32);
    }

    if (addr_is_rom(pa)) {
        m = ROM;
        index = pa >> 2;
    } else if (addr_is_mem(pa)) {
        m = RAM;
        index = (pa - PHYS_MEM_BASE) >> 2;
    } else {
#ifndef SUPP_MEM_ERR
        sim_debug(READ_MSG, &mmu_dev, "!!!! Cannot read physical address %08x\n", pa);
        cpu_set_exception(NORMAL_EXCEPTION, EXTERNAL_MEMORY_FAULT);
#endif
        return 0;
    }

    return m[index];
}

/*
 * Read Word (Physical Address, Unaligned)
 */
uint32 pread_w(uint32 pa)
{
    if (pa & 3) {
        sim_debug(WRITE_MSG, &mmu_dev,
                  "!!!! Cannot write physical address. ALIGNMENT ISSUE %08x\n", pa);
        cpu_set_exception(NORMAL_EXCEPTION, EXTERNAL_MEMORY_FAULT);
    }

    return pread_w_u(pa);
}

/*
 * Write Word (Physical Address, Unaligned)
 */
void pwrite_w(uint32 pa, uint32 val)
{

    if (pa & 3) {
        sim_debug(WRITE_MSG, &mmu_dev,
                  "!!!! Cannot write physical address. ALIGNMENT ISSUE %08x\n", pa);
        cpu_set_exception(NORMAL_EXCEPTION, EXTERNAL_MEMORY_FAULT);
    }

    if (addr_is_io(pa)) {
        io_write(pa, val, 32);
        return;
    }

    if (addr_is_mem(pa)) {
        RAM[(pa - PHYS_MEM_BASE) >> 2] = val;
    } else {
#ifndef SUPP_MEM_ERR
        sim_debug(WRITE_MSG, &mmu_dev,
                  "!!!! Cannot write physical address. %08x\n", pa);
        cpu_set_exception(NORMAL_EXCEPTION, EXTERNAL_MEMORY_FAULT);
#endif
    }
}

/*
 * Read Halfword (Physical Address)
 */
uint16 pread_h(uint32 pa)
{
    uint32 *m;
    uint32 index;

    if (pa & 1) {
        sim_debug(READ_MSG, &mmu_dev,
                  "!!!! Cannot read physical address. ALIGNMENT ISSUE %08x\n", pa);
        cpu_set_exception(NORMAL_EXCEPTION, EXTERNAL_MEMORY_FAULT);
    }

    if (addr_is_io(pa)) {
        return io_read(pa, 16);
    }

    if (addr_is_rom(pa)) {
        m = ROM;
        index = pa >> 2;
    } else if (addr_is_mem(pa)) {
        m = RAM;
        index = (pa - PHYS_MEM_BASE) >> 2;
    } else {
#ifndef SUPP_MEM_ERR
        sim_debug(READ_MSG, &mmu_dev,
                  "!!!! Cannot read physical address. %08x\n", pa);
        cpu_set_exception(NORMAL_EXCEPTION, EXTERNAL_MEMORY_FAULT);
#endif
        return 0;
    }

    if (pa & 2) {
        return m[index] & HALF_MASK;
    } else {
        return (m[index] >> 16) & HALF_MASK;
    }
}

/*
 * Write Halfword (Physical Address)
 */
void pwrite_h(uint32 pa, uint16 val)
{
    uint32 *m;
    uint32 index;
    uint32 wval = (uint32)val;

    if (pa & 1) {
        sim_debug(WRITE_MSG, &mmu_dev,
                  "!!!! Cannot write physical address %08x, ALIGNMENT ISSUE\n", pa);
        cpu_set_exception(NORMAL_EXCEPTION, EXTERNAL_MEMORY_FAULT);
        return;
    }

    if (addr_is_io(pa)) {
        io_write(pa, val, 16);
        return;
    }

    if (addr_is_mem(pa)) {
        m = RAM;
        index = (pa - PHYS_MEM_BASE) >> 2;
    } else {
#ifndef SUPP_MEM_ERR
        sim_debug(WRITE_MSG, &mmu_dev, "!!!! Cannot write physical address %08x\n", pa);
        cpu_set_exception(NORMAL_EXCEPTION, EXTERNAL_MEMORY_FAULT);
#endif
        return;
    }

    if (pa & 2) {
        m[index] = (m[index] & ~HALF_MASK) | wval;
    } else {
        m[index] = (m[index] & HALF_MASK) | (wval << 16);
    }
}

/*
 * Read Byte (Physical Address)
 */
uint8 pread_b(uint32 pa)
{
    int32 data;
    int32 sc = (~(pa & 3) << 3) & 0x1f;

    if (addr_is_io(pa)) {
        return io_read(pa, 8);
    }

    if (addr_is_rom(pa)) {
        data = ROM[pa >> 2];
    } else if (addr_is_mem(pa)) {
        data = RAM[(pa - PHYS_MEM_BASE) >> 2];
    } else {
#ifndef SUPP_MEM_ERR
        cpu_set_exception(NORMAL_EXCEPTION, EXTERNAL_MEMORY_FAULT);
#endif
        return 0;
    }

    return (data >> sc) & BYTE_MASK;
}

/*
 * Write Byte (Physical Address)
 */
void pwrite_b(uint32 pa, uint8 val)
{
    uint32 *m;
    int32 index;
    int32 sc = (~(pa & 3) << 3) & 0x1f;
    int32 mask = 0xff << sc;

    if (addr_is_io(pa)) {
        io_write(pa, val, 8);
        return;
    }

    if (addr_is_mem(pa)) {
        m = RAM;
        index = (pa - PHYS_MEM_BASE) >> 2;
    } else {
#ifndef SUPP_MEM_ERR
        cpu_set_exception(NORMAL_EXCEPTION, EXTERNAL_MEMORY_FAULT);
#endif
        return;
    }

    m[index] = (m[index] & ~mask) | (val << sc);
}

/*
 * MMU Virtual Read and Write Functions
 */

uint32 mmu_xlate_addr(uint32 addr)
{
    if (!mmu_enabled()) {
        return addr;
    }

    /* TODO: MMU is enabled. Perform address translation */

    return addr;
}

/*
 * Dispatch functions
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
