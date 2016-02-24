/* 3b2_dmac.c: AT&T 3B2 Model 400 DMA Controller Implementation

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

#include "3b2_dmac.h"

DMA_STATE dma_state;

UNIT dmac_unit = { UDATA(NULL, 0, 0) };

REG dmac_reg[] = {
    { NULL }
};

DEVICE dmac_dev = {
    "DMAC", &dmac_unit, dmac_reg, NULL,
    1, 16, 8, 4, 16, 32,
    NULL, NULL, NULL,
    NULL, NULL, NULL, NULL,
    DEV_DEBUG, 0, sys_deb_tab
};

dmac_drq_handler dmac_drq_handlers[] = {
    {DMA_ID_CHAN, IDBASE + ID_DATA_REG, &id_state.drq, dmac_service_id, id_drq_handled},
    {DMA_IF_CHAN, IFBASE + IF_DATA_REG, &if_state.drq, dmac_service_if, if_drq_handled},
    {0, 0, 0, NULL, NULL}
};

uint32 dmac_read(uint32 pa, uint8 size)
{
    /*
     * Not implemented, because the 3B2 never appears to actually read
     * from the DMA controller. It is effectively write-only.
     */

    sim_debug(READ_MSG, &dmac_dev, "[%08x] DMAC READ %d B @ %08x\n",
              R[NUM_PC], size, pa);

    return 0;
}

/*
 * Program the DMAC
 */
void dmac_program(uint8 reg, uint8 val)
{
    uint8 channel_id, i;
    dma_channel *channel;

    if (reg < 8) {
        switch (reg) {
        case 0:
        case 1:
            channel = &dma_state.channels[0];
            break;
        case 2:
        case 3:
            channel = &dma_state.channels[1];
            break;
        case 4:
        case 5:
            channel = &dma_state.channels[2];
            break;
        case 6:
        case 7:
            channel = &dma_state.channels[3];
            break;
        }

        if (channel == NULL) {
            /* This should never happen */
            return;
        }

        switch (reg & 1) {
        case 0: /* Address */
            channel->addr |= (val & 0xff) << (dma_state.bff * 8);
            break;
        case 1: /* Word Count */
            channel->wcount |= (val & 0xff) << (dma_state.bff * 8);
            break;
        }

        /* Toggle the byte flip-flop */
        dma_state.bff ^= 1;

        /* Handled. */
        return;
    }

    /* If it hasn't been handled, it must be one of the following
       registers. */

    switch (reg) {
    case 8:  /* Command */
        dma_state.command = val;
        break;
    case 9:  /* Request */
        dma_state.request = val;
        break;
    case 10: /* Write Single Mask Register Bit */
        channel_id = val & 3;

        /* "Clear or Set" is bit 2 */
        if ((val >> 2) & 1) {
            dma_state.mask |= (1 << channel_id);
        } else {
            dma_state.mask &= ~(1 << channel_id);
        }

        break;
    case 11: /* Mode */
        dma_state.mode = val;
        break;
    case 12: /* Clear Byte Pointer Flip/Flop */
        dma_state.bff = 0;
        break;
    case 13: /* Master Clear */
        dma_state.bff = 0;
        dma_state.command = 0;
        dma_state.status = 0;
        for (i = 0; i < 4; i++) {
            dma_state.channels[i].addr = 0;
            dma_state.channels[i].wcount = 0;
            dma_state.channels[i].page = 0;
        }
        break;
    case 15: /* Write All Mask Register Bits */
        dma_state.mask = val & 0xf;
        break;
    case 16: /* Clear DMAC Interrupt - Not Implemented */
        break;
    }
}

void dmac_page_update(uint8 base, uint8 reg, uint8 val)
{
    uint8 shift = 0;

    /* Sanity check */
    if (reg > 3) {
        return;
    }

    /* The actual register is a 32-bit, byte-addressed register, so
       that address 4x000 is the highest byte, 4x003 is the lowest
       byte. */

    shift = -(reg - 3) * 8;

    switch (base) {
    case DMA_ID:
        dma_state.channels[DMA_ID_CHAN].page |= (val << shift);
        break;
    case DMA_IUA:
        dma_state.channels[DMA_IUA_CHAN].page |= (val << shift);
        break;
    case DMA_IUB:
        dma_state.channels[DMA_IUB_CHAN].page |= (val << shift);
        break;
    case DMA_IF:
        dma_state.channels[DMA_IF_CHAN].page |= (val << shift);
        break;
    }
}

void dmac_write(uint32 pa, uint32 val, uint8 size)
{
    uint8 reg, base;

    base = pa >> 12;
    reg = pa & 0xff;

    switch (base) {
    case DMA_C:
        dmac_program(reg, val);
        break;

    case DMA_ID:
    case DMA_IUA:
    case DMA_IUB:
    case DMA_IF:
        dmac_page_update(base, reg, val);
        break;

    }
}

/*
 * DMA Service Routine for the Integrated Disk (id)
 */
void dmac_service_id(uint32 service_address)
{
    // TODO: implement
}

static SIM_INLINE uint32 dma_address(uint8 channel, uint32 offset, t_bool r) {
    uint32 addr;

    addr = (PHYS_MEM_BASE +
            dma_state.channels[channel].addr +
            offset);

    /* It seems as though we don't honor the page on writes, only on
       reads. This is extremely confusing and I wish I could find
       documentation or source code to justify this observation apart
       from a few obscure #defines in SVR3 and the behavior of
       "newkey" in the PROM */
    
    if (r) {
        addr += dma_state.channels[channel].page << 16;
    }
    
    return addr;
}

/*
 * DMA Service Routine for the Integrated Floppy (if)
 */
void dmac_service_if(uint32 service_address)
{
    uint8 data;
    int32 i;
    uint16 offset;
    uint32 addr;

    switch ((dma_state.mode >> 2) & 0xf) {
    case DMA_MODE_VERIFY:
        /* TODO: Implement if necessary. */
        sim_debug(WRITE_MSG, &dmac_dev, ">>> UNHANDLED DMAC VERIFY REQUEST.\n");
        break;
    case DMA_MODE_WRITE:
        sim_debug(WRITE_MSG, &dmac_dev, ">>> DMAC WRITE: %d BYTES AT %08x\n",
                  dma_state.channels[DMA_IF_CHAN].wcount + 1,
                  dma_address(DMA_IF_CHAN, 0, FALSE));
        offset = 0;
        for (i = dma_state.channels[DMA_IF_CHAN].wcount; i >= 0; i--) {
            addr = dma_address(DMA_IF_CHAN, offset++, FALSE);
            data = read_b(addr);
            write_b(service_address, data);
        }

        /* End of Process must set the IF channel's mask bit */
        dma_state.mask |= (1 << DMA_IF_CHAN);

        break;
    case DMA_MODE_READ:
        sim_debug(WRITE_MSG, &dmac_dev, ">>> DMAC READ: %d BYTES AT %08x\n",
                  dma_state.channels[DMA_IF_CHAN].wcount + 1,
                  dma_address(DMA_IF_CHAN, 0, TRUE));
        offset = 0;
        for (i = dma_state.channels[DMA_IF_CHAN].wcount; i >= 0; i--) {
            addr = dma_address(DMA_IF_CHAN, offset++, TRUE);
            data = read_b(service_address);
            write_b(addr, data);
        }

        /* End of Process must set the IF channel's mask bit */
        dma_state.mask |= (1 << DMA_IF_CHAN);

        break;
    }

}

/*
 * Service pending DRQs
 */
void dmac_service_drqs()
{
    dmac_drq_handler *h;

    for (h = &dmac_drq_handlers[0]; h->handler != NULL; h++) {
        /* Only trigger if the channel has a DRQ set and its channel's
           mask bit is 0 */
        if (*h->drq && ((dma_state.mask >> h->channel) & 0x1) == 0) {
            sim_debug(WRITE_MSG, &dmac_dev, "Servicing DMAC Request: Channel %d\n",
                      h->channel);
            h->handler(h->service_address);
            h->handled_callback();
        }
    }
}
