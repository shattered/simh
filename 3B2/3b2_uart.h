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

extern DEVICE uart_dev;

#define UARTBASE 0x49000
#define UARTSIZE 0x100

/* Function prototypes */

t_stat uart_reset(DEVICE *dptr);
t_stat uart_svc(UNIT *uptr);
uint32 uart_read(uint32 pa, uint8 size);
void uart_write(uint32 pa, uint32 val, uint8 size);

static SIM_INLINE void uart_w_buf(uint8 portno, uint8 val);
static SIM_INLINE void uart_w_cmd(uint8 portno, uint8 val);
static SIM_INLINE void uart_update_rxi(uint8 c);
static SIM_INLINE void uart_update_txi();

#endif
