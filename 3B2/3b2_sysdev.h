/* 3b2_cpu.h: AT&T 3B2 Model 400 System Devices (Header)

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

#ifndef _3B2_SYSDEV_H_
#define _3B2_SYSDEV_H_

#include "3b2_sys.h"
#include "3b2_defs.h"
#include "3b2_cpu.h"

#define TIMERBASE 0x42000
#define TIMERSIZE 0x20
#define NVRAMBASE 0x43000
#define NVRAMSIZE 0x1000
#define CSRBASE   0x44000
#define CSRSIZE   0x100

#define CSRTIMO   0x8000
#define CSRPARE   0x4000
#define CSRRRST   0x2000
#define CSRALGN   0x1000
#define CSRLED    0x0800
#define CSRFLOP   0x0400
#define CSRITIM   0x0100
#define CSRIFLT   0x0080
#define CSRCLK    0x0040
#define CSRPIR8   0x0020
#define CSRPIR9   0x0010
#define CSRUART   0x0008
#define CSRDISK   0x0004
#define CSRDMA    0x0002
#define CSRIOF    0x0001

extern DEVICE nvram_dev;
extern DEVICE timer_dev;
extern DEVICE csr_dev;
extern DEBTAB sys_deb_tab[];

/* NVRAM */
t_stat nvram_ex(t_value *vptr, t_addr exta, UNIT *uptr, int32 sw);
t_stat nvram_dep(t_value val, t_addr exta, UNIT *uptr, int32 sw);
t_stat nvram_reset(DEVICE *dptr);
uint32 nvram_read(uint32 pa, uint8 size);
t_stat nvram_attach(UNIT *uptr, char *cptr);
t_stat nvram_detach(UNIT *uptr);
const char *nvram_description(DEVICE *dptr);
void nvram_write(uint32 pa, uint32 val, uint8 size);

/* 8253 Timer */
t_stat timer_reset(DEVICE *dptr);
uint32 timer_read(uint32 pa, uint8 size);
void timer_write(uint32 pa, uint32 val, uint8 size);
t_stat timer_svc(UNIT *uptr);

/* CSR */
t_stat csr_ex(t_value *vptr, t_addr exta, UNIT *uptr, int32 sw);
t_stat csr_dep(t_value val, t_addr exta, UNIT *uptr, int32 sw);
t_stat csr_reset(DEVICE *dptr);
uint32 csr_read(uint32 pa, uint8 size);
void csr_write(uint32 pa, uint32 val, uint8 size);

#endif
