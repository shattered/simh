/* 3b2_cpu.h: AT&T 3B2 Model 400 Floppy (TMS2797NL) Implementation

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

#include "3b2_if.h"

/*
 * Disk Format:
 *
 * - 80 Tracks
 * - 9 Sectors per track
 * - 1024 bytes per sector
 *
 * 80 * 9 * 1024 = 720KB
 *
 *
 */

IF_STATE if_state;

UNIT if_unit[] = {
    { UDATA (&if_svc, UNIT_FIX+UNIT_ATTABLE, IF_DSK_SIZE), 1000L },
    { NULL }
};

REG if_reg[] = {
    { NULL }
};

DEVICE if_dev = {
    "IF", if_unit, if_reg, NULL,
    1, 16, 31, 1, 16, 8,
    NULL, NULL, &if_reset,
    NULL, NULL, NULL, NULL,
    DEV_DEBUG, 0, sys_deb_tab
};

uint8 if_buf[512];
int16 if_buf_ptr;
t_bool if_irq_needed;

/* Function implementation */

t_stat if_svc(UNIT *uptr)
{
    if (if_irq_needed) {
        cpu_set_irq(11, 11, 0);
        if_irq_needed = FALSE;
    }
    sim_activate_after(if_unit, 5000L);
    return SCPE_OK;
}

t_stat if_reset(DEVICE *dptr)
{
    if_state.status = (IF_TK_0 | IF_HEAD_LOADED);
    if_state.track = 0;
    if_state.sector = 1;
    if_buf_ptr = -1;
    if_irq_needed = FALSE;
    sim_activate_after(if_unit, 5000L);
    return SCPE_OK;
}

uint32 if_read(uint32 pa, size_t size) {
    uint8 reg, data;
    uint32 pos;
    UNIT *uptr;
    uint32 pc;

    uptr = &(if_dev.units[0]);
    reg = pa - IFBASE;
    pc = R[NUM_PC];

    switch (reg) {
    case IF_STATUS_REG:
        data = if_state.status;
        /* If there's no image attached, we're not ready */
        if (uptr->fileref == NULL) {
            sim_debug(READ_MSG, &if_dev, "Drive Not Ready\n");
            data |= IF_NRDY;
        }
        break;
    case IF_TRACK_REG:
        data = if_state.track;
        break;
    case IF_SECTOR_REG:
        data = if_state.sector;
        break;
    case IF_DATA_REG:
        if (uptr->fileref == NULL) {
            /* We are not attached */
            return 0;
        }

        if (if_buf_ptr < 0) {
            pos = IF_TRACK_SIZE * if_state.track * 2;

            if (if_state.side == 1) {
                pos += IF_TRACK_SIZE;
            }

            pos += IF_SECTOR_SIZE * (if_state.sector - 1);

            fseek(uptr->fileref, pos, 0);
            fread(if_buf, 512, 1, uptr->fileref);

            sim_debug(READ_MSG, &if_dev,
                      "Reading CYL %d, SEC %d, SIDE %d\n",
                      if_state.track, if_state.sector, if_state.side);

            if_buf_ptr = 0;
        }

        if (if_buf_ptr >= 0 && if_buf_ptr < 0x200) {
            return if_buf[if_buf_ptr++] & 0xff;
        }

        return 0;
    default:
        break;
    }

    return data;
}

/* Handle the most recently received command */
void if_handle_command()
{
    sim_debug(EXECUTE_MSG, &if_dev,
              "[%08x] Executing command: %02x\n",
              R[NUM_PC], if_state.cmd);

    if_buf_ptr = -1;

    switch(if_state.cmd & 0xf0) {
    case IF_RESTORE:
    case IF_SEEK:
    case IF_STEP:
    case IF_STEP_T:
    case IF_STEP_IN:
    case IF_STEP_IN_T:
    case IF_STEP_OUT:
    case IF_STEP_OUT_T:
        if_state.cmd_type = 1;
        if_state.status &= ~(IF_CRC_ERR | IF_SEEK_ERR | IF_DRQ);
        break;

    case IF_READ_SEC:
    case IF_READ_SEC_M:
    case IF_WRITE_SEC:
    case IF_WRITE_SEC_M:
        if_state.cmd_type = 2;
        if_state.status &= ~(IF_CRC_ERR | IF_LOST_DATA | IF_RNF | IF_RECORD_TYPE | IF_WP);
        if_state.side = (if_state.cmd >> 1) & 1;
        break;

    case IF_FORCE_INT:
        if_state.cmd_type = 4;
        break;
    }

    switch(if_state.cmd & 0xf0) {
    case RESTORE:
        if_state.track = 0;
        if_state.status |= IF_TK_0;
        break;
    case IF_SEEK:
        if (if_state.data > 79) {
            if_state.status |= IF_RNF;
        } else {
            if_state.track = if_state.data;
        }

        if (if_state.track == 0) {
            if_state.status |= IF_TK_0;
        } else {
            if_state.status &= ~IF_TK_0;
        }
        break;
    case IF_READ_SEC:
    case IF_READ_SEC_M:
        if_state.status |= IF_DRQ;
        if_state.drq = TRUE;
        break;

    case IF_WRITE_SEC:
    case IF_WRITE_SEC_M:
        if_state.status |= IF_DRQ;
        if_state.drq = TRUE;
        break;

    case IF_FORCE_INT:
        break;
    }

    if_irq_needed = TRUE;
}

void if_write(uint32 pa, uint32 val, size_t size)
{
    uint8 reg;
    uint32 pc;

    uint32 pos;
    UNIT *uptr;

    uptr = &(if_dev.units[0]);

    reg = pa - IFBASE;

    pc = R[NUM_PC];

    switch (reg) {
    case IF_CMD_REG:
        if_state.cmd = val & 0xff;
        if_handle_command();
        break;
    case IF_TRACK_REG:
        if_state.track = val & 0xff;
        break;
    case IF_SECTOR_REG:
        if_state.sector = val & 0xff;
        break;
    case IF_DATA_REG:
        if_state.data = val & 0xff;

        if (uptr->fileref == NULL ||
            ((if_state.cmd & 0xf0) != IF_WRITE_SEC &&
             (if_state.cmd & 0xf0) != IF_WRITE_SEC_M)) {
            /* Not attached, or not a write command */
            break;
        }

        if (if_buf_ptr < 0) {
            if_buf_ptr = 0;
        }

        if (if_buf_ptr >= 0 && if_buf_ptr < IF_SECTOR_SIZE) {
            sim_debug(WRITE_MSG, &if_dev,
                      "     >> writing %02x\n", val & 0xff);
            if_buf[if_buf_ptr++] = val & 0xff;
        }

        if (if_buf_ptr == IF_SECTOR_SIZE) {
            pos = IF_TRACK_SIZE * if_state.track * 2;

            if (if_state.side == 1) {
                pos += IF_TRACK_SIZE;
            }

            pos += IF_SECTOR_SIZE * (if_state.sector - 1);

            fseek(uptr->fileref, pos, 0);
            fwrite(if_buf, 512, 1, uptr->fileref);

            sim_debug(WRITE_MSG, &if_dev,
                      "WRITING CYL %d, SEC %d, SIDE %d\n",
                      if_state.track, if_state.sector, if_state.side);

            if_buf_ptr = 0;
            if_irq_needed = TRUE;
        }

        break;
    default:
        break;
    }

}

void if_drq_handled()
{
    if_state.drq = FALSE;
    if_state.status &= ~IF_DRQ;
}
