/* 3b2_cpu.h: AT&T 3B2 Model 400 Hard Disk (uPD7261) Implementation

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

#include "3b2_id.h"

ID_STATE id_state;

UNIT id_unit[] = {
    { UDATA (&id_svc, UNIT_FIX+UNIT_ATTABLE, ID_DSK_SIZE) },
    { NULL }
};

REG id_reg[] = {
    { HRDATAD(CMD, id_state.cmd, 8, "Command buffer") },
    { BRDATAD(FIFO, id_state.data, 16, 8, ID_FIFO_LEN, "FIFO data buffer") }
};

DEVICE id_dev = {
    "ID", id_unit, id_reg, NULL,
    1, 16, 31, 1, 16, 8,
    NULL, NULL, &id_reset,
    &id_boot, &id_attach, NULL, NULL,
    DEV_DEBUG, 0, sys_deb_tab
};

/* Function implementation */

t_stat id_svc(UNIT *uptr)
{
    return SCPE_OK;
}

t_stat id_reset(DEVICE *dptr)
{
    memset(&id_state.data, 0, sizeof(uint8) * ID_FIFO_LEN);
    id_state.data_p = 0;
    return SCPE_OK;
}

t_stat id_attach(UNIT *uptr, char *cptr)
{
    attach_unit(uptr, cptr);
    return SCPE_OK;
}

t_stat id_boot(int32 unitno, DEVICE *dptr)
{
    /* TODO: Set PC, etc. */
    return SCPE_OK;
}

uint32 id_read(uint32 pa, uint8 size) {
    uint8 data, reg;

    data = 0;
    reg = pa - IDBASE;

    switch(reg) {
    case 0:     /* Data Buffer Register */
        if (id_state.data_p < ID_FIFO_LEN) {
            data = id_state.data[id_state.data_p++];
            sim_debug(READ_MSG, &id_dev, ">>> READ DATA: %02x\n", data);
            id_state.status &= ~(ID_STAT_CEL | ID_STAT_CEH);
        }

        break;
    case 1:     /* Status Register */
        data = id_state.status;
        sim_debug(READ_MSG, &id_dev, ">>> READ STATUS %02x\n", data);

        /* XXX HACK: Try to set the SEQ bit after a read */
        id_state.status |= ID_STAT_SRQ;
        break;
    }

    return data;
}
static int counter = 2;

void id_handle_command(uint8 val)
{
    uint8 dev, aux_cmd, data;

    id_state.data_p = 0;

    id_state.cmd = (val >> 4) & 0xf;
    dev = val & 0x07;

    switch(id_state.cmd) {
    case ID_CMD_AUX: /* Auxiliary Command */
        aux_cmd = val & 0x0f;

        if (aux_cmd & ID_AUX_CLCE) {
            /* Clear CE bits of the status register */
            sim_debug(WRITE_MSG, &id_dev, ">>> COMMAND: (AUX) CLEAR CE BITS\n");
            id_state.status &= ~(ID_STAT_CEL | ID_STAT_CEH);
        }

        if (aux_cmd & ID_AUX_HSRQ) {
            /* Deactivate interrupt request output */
            sim_debug(WRITE_MSG, &id_dev, ">>> COMMAND: (AUX) DEACTIVATE INT. REQ. OUTPUT\n");
        }

        if (aux_cmd & ID_AUX_CLB) {
            /* Clear data buffer */
            sim_debug(WRITE_MSG, &id_dev, ">>> COMMAND: (AUX) CLEAR DATA BUFFER\n");
            memset(&id_state.data, 0, sizeof(uint8) * ID_FIFO_LEN);
            id_state.data_p = 0;
        }

        if (aux_cmd & ID_AUX_RST) {
            sim_debug(WRITE_MSG, &id_dev, ">>> COMMAND: (AUX) RESET\n");
            id_state.status &= ~(ID_STAT_CEL | ID_STAT_CEH | ID_STAT_SRQ);
        }

        break;
    case ID_CMD_SIS:
        sim_debug(WRITE_MSG, &id_dev, ">>> COMMAND: SENSE INTERRUPT STATUS\n");
        id_state.status |= ID_STAT_CEH; /* Command complete */
        break;
    case ID_CMD_SPEC:
        sim_debug(WRITE_MSG, &id_dev, ">>> COMMAND: SPECIFY\n");

        /* Inspect the data, reset the pointers */
        for (id_state.data_p = 0; id_state.data_p < 8; id_state.data_p++) {
            sim_debug(WRITE_MSG, &id_dev,
                      ">>>    Processing byte: %02x\n", id_state.data[id_state.data_p]);
        }

        id_state.status |= ID_STAT_CEH; /* Command complete */
        break;
    case ID_CMD_SUS:
        id_state.data_p = 0;

        data = 0x1a;

        if (id_state.track == 0) {
            data |= 0x04;
        }

        sim_debug(WRITE_MSG, &id_dev, ">>> COMMAND: SENSE UNIT STATUS: %02x\n", data);

        id_state.data[id_state.data_p++] = data;
        id_state.status |= ID_STAT_CEH; /* Command complete */
        break;
    case ID_CMD_DERR:
        sim_debug(WRITE_MSG, &id_dev, ">>> COMMAND: DETECT ERROR\n");
        id_state.status |= ID_STAT_CEH; /* Command complete */
        break;
    case ID_CMD_RECAL:
        sim_debug(WRITE_MSG, &id_dev, ">>> COMMAND: RECALIBRATE\n");
        id_state.track = 0;
        id_state.track = id_state.data[0] << 8 | id_state.data[1];
        id_state.status &= ~ID_STAT_SRQ;
        id_state.status |= ID_STAT_CEH; /* Command complete */
        break;
    case ID_CMD_SEEK:
        data = id_state.data[0] << 8 | id_state.data[1];
        sim_debug(WRITE_MSG, &id_dev, ">>> COMMAND: SEEK TO %04x\n", data);
        id_state.track = (id_state.data[0] << 8 | id_state.data[1]);
        id_state.status |= (ID_STAT_CEH | ID_STAT_SRQ);
        break;
    case ID_CMD_FMT:
        sim_debug(WRITE_MSG, &id_dev, ">>> COMMAND: FORMAT\n");
        break;
    case ID_CMD_VID:
        sim_debug(WRITE_MSG, &id_dev, ">>> COMMAND: VERIFY ID\n");
        break;
    case ID_CMD_RID:
        sim_debug(WRITE_MSG, &id_dev, ">>> COMMAND: READ ID\n");
        break;
    case ID_CMD_RDIAG:
        sim_debug(WRITE_MSG, &id_dev, ">>> COMMAND: READ DIAG\n");
        break;
    case ID_CMD_RDATA:
        sim_debug(WRITE_MSG, &id_dev, ">>> COMMAND: READ DATA\n");
        id_state.status = (ID_STAT_CEH | ID_STAT_DRQ); /* command complete */
        break;
    case ID_CMD_CHECK:
        sim_debug(WRITE_MSG, &id_dev, ">>> COMMAND: READ CHECK\n");
        break;
    case ID_CMD_SCAN:
        sim_debug(WRITE_MSG, &id_dev, ">>> COMMAND: READ SCAN\n");
        break;
    case ID_CMD_VDATA:
        sim_debug(WRITE_MSG, &id_dev, ">>> COMMAND: VERIFY DATA\n");
        break;
    case ID_CMD_WDATA:
        sim_debug(WRITE_MSG, &id_dev, ">>> COMMAND: WRITE DATA\n");
        break;
    default:
        sim_debug(WRITE_MSG, &id_dev, ">>> COMMAND: %02x\n", id_state.cmd);
        break;
    }
}


void id_handle_data(uint8 val)
{
    sim_debug(WRITE_MSG, &id_dev, ">>> DATA=%02x\n", val);

    if (id_state.data_p < ID_FIFO_LEN) {
        id_state.data[id_state.data_p++] = val & 0xff;
    } else {
        sim_debug(WRITE_MSG, &id_dev, ">>> FIFO FULL!\n");
    }
}


void id_write(uint32 pa, uint32 val, uint8 size)
{
    uint8 reg;

    reg = pa - IDBASE;

    switch(reg) {
    case ID_DATA_REG:     /* Data Buffer Register */
        id_handle_data((uint8)val);
        break;
    case ID_COMMAND_REG:     /* Command Buffer */
        id_handle_command((uint8)val);
        break;
    default:
        break;
    }
}

void id_drq_handled()
{
}
