/* dmd5620_uart.h:  SCN2681A Dual UART Header

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

#ifndef __DMD5620_UART_H__
#define __DMD5620_UART_H__

#include "3b2_defs.h"
#include "dmd5620_sysdev.h"
#include "vax_2681.h"
#include "sim_tmxr.h"
#include "sim_video.h"

extern DEVICE uart_dev;

#define UARTBASE 0x200000
#define UARTSIZE 0x40

/* Function prototypes */

t_stat uart_reset(DEVICE *dptr);
t_stat uart_attach (UNIT *uptr, char *cptr);
t_stat uart_detach (UNIT *uptr);
t_stat uart_svc(UNIT *uptr);
uint32 uart_read(uint32 pa, size_t size);
void uart_write(uint32 pa, uint32 val, size_t size);
void uart_int (uint32 set);
void uart_output_port (uint32 val);
void uart_vsync (uint32 set);
void mouse_buttons (SIM_MOUSE_EVENT *ev);
t_stat kb_rd (uint8 *c);
t_stat kb_wr (uint8 c);
t_bool kb_map_key (uint32 key, uint32 state, uint8 *c);
t_stat ln_rd (uint8 *c);
t_stat ln_wr (uint8 c);

void vc_set_reverse_video (t_bool set);

static void int_controller_set(uint8 value);
static void int_controller_clear(uint8 value);

static uint8 int_controller_pending = 0;	/* list of pending interrupts */
static uint8 int_controller_pal[64] = {		/* decode pending interrupts into IPL */
    0,  14, 14, 14, 14, 14, 14, 14,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15 };

#endif
