/* dmd5620_cpu.h: AT&T 3B2 Model 400 System Devices (Header)

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

#ifndef _DMD5620_SYSDEV_H_
#define _DMD5620_SYSDEV_H_

#include "dmd5620_sys.h"
#include "dmd5620_defs.h"
#include "sim_video.h"

#define NVRAMBASE 0x600000
#define NVRAMSIZE 0x2000
#define DADDRBASE 0x500000
#define DADDRSIZE 0x2
#define MOUSEBASE 0x400000
#define MOUSESIZE 0x4

extern DEVICE nvram_dev;
extern DEVICE daddr_dev;
extern DEVICE vc_dev;
extern DEVICE mouse_dev;
extern DEBTAB sys_deb_tab[];

extern uint32 daddr_data;
extern uint32 mouse_xy;

/* NVRAM */
t_stat nvram_ex(t_value *vptr, t_addr exta, UNIT *uptr, int32 sw);
t_stat nvram_dep(t_value val, t_addr exta, UNIT *uptr, int32 sw);
t_stat nvram_reset(DEVICE *dptr);
uint32 nvram_read(uint32 pa, size_t size);
void nvram_write(uint32 pa, uint32 val, size_t size);
t_stat nvram_attach (UNIT *uptr, char *cptr);
t_stat nvram_detach (UNIT *uptr);

/* Display starting address */
t_stat daddr_ex(t_value *vptr, t_addr exta, UNIT *uptr, int32 sw);
t_stat daddr_dep(t_value val, t_addr exta, UNIT *uptr, int32 sw);
t_stat daddr_reset(DEVICE *dptr);
void daddr_write(uint32 pa, uint32 val, size_t size);

/* Mouse */
t_stat mouse_ex(t_value *vptr, t_addr exta, UNIT *uptr, int32 sw);
t_stat mouse_dep(t_value val, t_addr exta, UNIT *uptr, int32 sw);
t_stat mouse_reset(DEVICE *dptr);
uint32 mouse_read(uint32 pa, size_t size);
void mouse_write(uint32 pa, uint32 val, size_t size);
void mouse_set_xy (SIM_MOUSE_EVENT *ev);

#endif
