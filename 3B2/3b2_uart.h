/* 3b2_uart.h:  SCN2681A Dual UART Header

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

   Except as contained in this notice, the name of Charles E. Owen shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from Charles E. Owen.
*/

#ifndef __3B2_UART_H__
#define __3B2_UART_H__

#include "3b2_defs.h"
#include "3b2_sysdev.h"


#define CMD_ERX         0x0001                          /* Enable receiver */
#define CMD_DRX         0x0002                          /* Disable receiver */
#define CMD_ETX         0x0004                          /* Enable transmitter */
#define CMD_DTX         0x0008                          /* Disable transmitter */
#define CMD_V_CMD       4                               /* Command */
#define CMD_M_CMD       0x7

#define STS_RXR         0x0001                          /* Receiver ready */
#define STS_FFL         0x0002                          /* FIFO full */
#define STS_TXR         0x0004                          /* Transmitter ready */
#define STS_TXE         0x0008                          /* Transmitter empty */
#define STS_OER         0x0010                          /* Overrun error */
#define STS_PER         0x0020                          /* Parity error */
#define STS_FER         0x0040                          /* Framing error */
#define STS_RXB         0x0080                          /* Received break */

#define ISTS_TAI        0x0001                          /* Transmitter ready A */
#define ISTS_RAI        0x0002                          /* Receiver ready A */
#define ISTS_CBA        0x0004                          /* Change in break A */
#define ISTS_CRI        0x0008                          /* Counter ready */
#define ISTS_TBI        0x0010                          /* Transmitter ready B */
#define ISTS_RBI        0x0020                          /* Receiver ready B */
#define ISTS_CBB        0x0040                          /* Change in break B */
#define ISTS_IPC        0x0080                          /* Interrupt port change */

#define MODE_V_CHM      6                               /* Channel mode */
#define MODE_M_CHM      0x3

#define PORT_A          0
#define PORT_B          1

extern DEVICE uart_dev;

#define UARTBASE            0x49000
#define UARTSIZE            0x100
#define UART_HZ             230525      /* UART timer Hz */

struct port {
    uint8 stat;           /* Port Status */
    uint8 cmd;            /* Command */
    uint8 mode[2];        /* Two mode buffers */
    uint8 mode_ptr;       /* Point to mode[0] or mode[1] */
    uint8 buf;            /* Character data */
};

struct uart_state {
    uint8 istat;          /* Interrupt Status */
    uint8 imask;          /* Interrupt Mask */
    uint16 c_set;         /* Timer / Counter Setting */
    int32  c_val;         /* Timer / Counter Value */
    t_bool c_en;          /* Counter Enabled */
    struct port port[2];  /* Port A and B */
};

/* Function prototypes */

t_stat uart_reset(DEVICE *dptr);
t_stat uart_svc(UNIT *uptr);
uint32 uart_read(uint32 pa, size_t size);
void uart_write(uint32 pa, uint32 val, size_t size);

static SIM_INLINE void uart_w_buf(uint8 portno, uint8 val);
static SIM_INLINE void uart_w_cmd(uint8 portno, uint8 val);
static SIM_INLINE void uart_update_rxi(uint8 c);
static SIM_INLINE void uart_update_txi();

#endif
