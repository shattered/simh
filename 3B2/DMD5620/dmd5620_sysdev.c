/* dmd5620_cpu.h: AT&T 3B2 Model 400 System Devices implementation

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


   This file contains system-specific registers and devices for the
   following 3B2 devices:

     - nvram       Non-Volatile RAM
*/

#include "dmd5620_sysdev.h"
#include "sim_video.h"

DEBTAB sys_deb_tab[] = {
    { "INIT",       INIT_MSG,       "Init"             },
    { "READ",       READ_MSG,       "Read activity"    },
    { "WRITE",      WRITE_MSG,      "Write activity"   },
    { "EXECUTE",    EXECUTE_MSG,    "Execute activity" },
    { "IRQ",        IRQ_MSG,        "Interrupt Handling"    },
    { "VMOUSE",     SIM_VID_DBG_MOUSE,      "Video Mouse"},
    { "VKEY",       SIM_VID_DBG_KEY,        "Video Key"},
    { NULL,         0                                  }
};

uint8 *NVRAM = NULL;

extern DEVICE cpu_dev;

/* Display starting address */

uint32 daddr_data;

UNIT daddr_unit = {
    UDATA(NULL, UNIT_FIX, DADDRSIZE)
};

REG daddr_reg[] = {
    { NULL }
};

DEVICE daddr_dev = {
    "DADDR", &daddr_unit, daddr_reg, NULL,
    1, 16, 8, 4, 16, 32, /* XXX */
    &daddr_ex, &daddr_dep, &daddr_reset,
    NULL, NULL, NULL, NULL,
    DEV_DEBUG, 0, sys_deb_tab
};

t_stat daddr_ex(t_value *vptr, t_addr exta, UNIT *uptr, int32 sw)
{
    *vptr = daddr_data;
    return SCPE_OK;
}

t_stat daddr_dep(t_value val, t_addr exta, UNIT *uptr, int32 sw)
{
    daddr_data = val;
    return SCPE_OK;
}

t_stat daddr_reset(DEVICE *dptr)
{
    daddr_data = 0;
    return SCPE_OK;
}

void daddr_write(uint32 pa, uint32 val, size_t size)
{
    sim_debug(WRITE_MSG, &daddr_dev, "WDADDR [%08x]\n", val);
    daddr_data = val;
}

/* NVRAM */

UNIT nvram_unit = {
    UDATA(NULL, UNIT_FIX+UNIT_BINK, NVRAMSIZE)
};

REG nvram_reg[] = {
    { NULL }
};

DEVICE nvram_dev = {
    "NVRAM", &nvram_unit, nvram_reg, NULL,
//  1, 16, 32, 4, 16, 8,
    1, 16, 8, 4, 16, 32,
    &nvram_ex, &nvram_dep, &nvram_reset,
    NULL, &nvram_attach, &nvram_detach, NULL,
    DEV_DEBUG, 0, sys_deb_tab
};

t_stat nvram_ex(t_value *vptr, t_addr exta, UNIT *uptr, int32 sw)
{
uint32 addr = (uint32) exta;

    if (vptr == NULL)
	return SCPE_ARG;
    if (addr >= NVRAMBASE+NVRAMSIZE)
	return SCPE_NXM;

    return SCPE_OK;
}

t_stat nvram_dep(t_value val, t_addr exta, UNIT *uptr, int32 sw)
{
uint32 addr = (uint32) exta;

    if (addr >= NVRAMBASE+NVRAMSIZE)
	return SCPE_NXM;

    return SCPE_OK;
}

t_stat nvram_reset(DEVICE *dptr) {
    sim_debug(INIT_MSG, &nvram_dev, "NVRAM Initialization\n");

    if (NVRAM == NULL) {
        NVRAM = (uint8 *)calloc(NVRAMSIZE, sizeof(uint8));
        memset(NVRAM, 0x5a, sizeof(uint8) * NVRAMSIZE);
    }

    if (NVRAM == NULL) {
        return SCPE_MEM;
    }

    nvram_unit.filebuf = (void *)NVRAM;

    return SCPE_OK;
}

t_stat nvram_attach (UNIT *uptr, CONST char *cptr)
{
t_stat r;

uptr->flags = uptr->flags | (UNIT_ATTABLE | UNIT_BUFABLE);
r = attach_unit (uptr, cptr);
if (r != SCPE_OK)
    uptr->flags = uptr->flags & ~(UNIT_ATTABLE | UNIT_BUFABLE);
else
    uptr->hwmark = (uint32) uptr->capac;
return r;
}

t_stat nvram_detach (UNIT *uptr)
{
t_stat r;

r = detach_unit (uptr);
if ((uptr->flags & UNIT_ATT) == 0) {
    uptr->flags = uptr->flags & ~(UNIT_ATTABLE | UNIT_BUFABLE);
    }
return r;
}

uint32 nvram_read(uint32 pa, size_t size)
{
    uint32 offset = pa - NVRAMBASE;
    uint32 data;

    if (size == 8 && (pa % 4) == 2) {
        data = NVRAM[offset] & 0xff;
	sim_debug(READ_MSG, &nvram_dev, "%d B @ %08x = %08x\n",
		  size, pa, data);
    } else {
	data = 0;
	sim_debug(READ_MSG, &nvram_dev, "BAD ACCESS: %d B @ %08x = %08x\n",
		  size, pa, data);
    }

    return data;
}

void nvram_write(uint32 pa, uint32 val, size_t size)
{
    uint32 offset = pa - NVRAMBASE;

    if (size == 8 && (pa % 4) == 2) {
        NVRAM[offset] = val & 0xff;
	sim_debug(WRITE_MSG, &nvram_dev, "%d B @ %08x=%08x\n",
		  size, pa, val);
    } else {
	sim_debug(WRITE_MSG, &nvram_dev, "BAD ACCESS: %d B @ %08x=%08x\n",
		  size, pa, val);
    }
}

/* Mouse */

uint32 mouse_xy;
int16 mouse_x = 0, mouse_y = 0;

UNIT mouse_unit = {
    UDATA(NULL, UNIT_FIX, MOUSESIZE)
};

REG mouse_reg[] = {
    { NULL }
};

DEVICE mouse_dev = {
    "MOUSE", &mouse_unit, mouse_reg, NULL,
    1, 16, 8, 4, 16, 32, /* XXX */
    &mouse_ex, &mouse_dep, &mouse_reset,
    NULL, NULL, NULL, NULL,
    DEV_DEBUG, 0, sys_deb_tab
};

t_stat mouse_ex(t_value *vptr, t_addr exta, UNIT *uptr, int32 sw)
{
    *vptr = 0;
    return SCPE_OK;
}

t_stat mouse_dep(t_value val, t_addr exta, UNIT *uptr, int32 sw)
{
    return SCPE_OK;
}

t_stat mouse_reset(DEVICE *dptr)
{
    return SCPE_OK;
}

uint32 mouse_read(uint32 pa, size_t size)
{
    sim_debug(READ_MSG, &mouse_dev, "pa %08x size %d\n", pa, size);

    if (size == 32)
	return mouse_xy;
    if (size == 16)
    	if (pa == MOUSEBASE)
	    return (mouse_xy & 0xffff);
	else
	    return ((mouse_xy >> 16) & 0xffff);
}

#define MOUSEMAX 4096

void mouse_set_xy (SIM_MOUSE_EVENT *ev)
{
    mouse_x += ev->x_rel;
    mouse_y -= ev->y_rel;

    if (mouse_x > MOUSEMAX)
    	mouse_x = MOUSEMAX;
    else if (mouse_x < 0)
    	mouse_x = 0;

    if (mouse_y > MOUSEMAX)
    	mouse_y = MOUSEMAX;
    else if (mouse_y < 0)
    	mouse_y = 0;

    mouse_xy = (mouse_y & 0xfff) | ((mouse_x & 0xfff) << 16);
    sim_debug(EXECUTE_MSG, &mouse_dev, "poll: x %d y %d xy %08x\n", mouse_x, mouse_y, mouse_xy);
}
