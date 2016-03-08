/* 3b2_cpu.h: AT&T 3B2 Model 400 Hard Disk (2797) Header

   Copyright (c) 2014, Seth J. Morabito

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

#ifndef __3B2_ID_H__
#define __3B2_ID_H__

#include "3b2_defs.h"
#include "3b2_sysdev.h"

/* Command Codes (bits 3-7 of command byte) */

#define ID_DATA_REG     0
#define ID_COMMAND_REG  1

#define ID_CMD_AUX      0x00  /* Auxiliary Command */
#define ID_CMD_SIS      0x01  /* Sense int. status */
#define ID_CMD_SPEC     0x02  /* Specify           */
#define ID_CMD_SUS      0x03  /* Sense unit status */
#define ID_CMD_DERR     0x04  /* Detect Error      */
#define ID_CMD_RECAL    0x05  /* Recalibrate       */
#define ID_CMD_SEEK     0x06  /* Seek              */
#define ID_CMD_FMT      0x07  /* Format            */
#define ID_CMD_VID      0x08  /* Verify ID         */
#define ID_CMD_RID      0x09  /* Read ID           */
#define ID_CMD_RDIAG    0x0A  /* Read Diagnostic   */
#define ID_CMD_RDATA    0x0B  /* Read Data         */
#define ID_CMD_CHECK    0x0C  /* Check             */
#define ID_CMD_SCAN     0x0D  /* Scan              */
#define ID_CMD_VDATA    0x0E  /* Verify Data       */
#define ID_CMD_WDATA    0x0F  /* Write Data        */

#define ID_AUX_RST      0x01
#define ID_AUX_CLB      0x02
#define ID_AUX_HSRQ     0x04
#define ID_AUX_CLCE     0x08

#define ID_STAT_DRQ     0x01
#define ID_STAT_NCI     0x02
#define ID_STAT_IER     0x04
#define ID_STAT_RRQ     0x08
#define ID_STAT_SRQ     0x10
#define ID_STAT_CEL     0x20
#define ID_STAT_CEH     0x40
#define ID_STAT_CB      0x80

/* Unit, Register, Device descriptions */

#define ID_FIFO_LEN 8

typedef struct {
    uint8 cmd;
    uint8 data[ID_FIFO_LEN];   /* 32-byte FIFO */
    uint8 data_p;              /* FIFO write pointer */
    uint8 status;
    uint16 track;

    t_bool drq;
} ID_STATE;

extern DEVICE id_dev;
extern DEBTAB sys_deb_tab[];
extern ID_STATE id_state;

#define IDBASE 0x4a000
#define IDSIZE 0x2

#define ID_DSK_SIZE 1024 * 1024 * 74

/* Function prototypes */

t_stat id_svc(UNIT *uptr);
t_stat id_reset(DEVICE *dptr);
t_stat id_attach(UNIT *uptr, char *cptr);
t_stat id_boot(int32 unitno, DEVICE *dptr);
uint32 id_read(uint32 pa, uint8 size);
void id_write(uint32 pa, uint32 val, uint8 size);

void id_drq_handled();

#endif
