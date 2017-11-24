/* 3b2_cpu.h: AT&T 3B2 Model 400 IO dispatch implemenation

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

#include "dmd5620_io.h"

struct iolink iotable[] = {
    { UARTBASE,   UARTBASE+UARTSIZE,     &uart_read,  &uart_write  },
    { NVRAMBASE,  NVRAMBASE+NVRAMSIZE,   &nvram_read, &nvram_write },
    { DADDRBASE,  DADDRBASE+DADDRSIZE,   NULL,        &daddr_write },
    { MOUSEBASE,  MOUSEBASE+MOUSESIZE,   &mouse_read, NULL         },
    { 0, 0, NULL, NULL}
};

uint32 io_read(uint32 pa, size_t size)
{
    struct iolink *p;

    for (p = &iotable[0]; p->low != 0; p++) {
        if ((pa >= p->low) && (pa < p->high) && p->read) {
            return p->read(pa, size);
        }
    }

    /* Not found. */
    sim_debug(IO_D_MSG, &cpu_dev,
              "READ ERROR. No IO device listening at address %08x\n", pa);

    cpu_abort(NORMAL_EXCEPTION, EXTERNAL_MEMORY_FAULT);

    return 0xffffffff;
}

void io_write(uint32 pa, uint32 val, size_t size)
{
    struct iolink *p;

    for (p = &iotable[0]; p->low != 0; p++) {
        if ((pa >= p->low) && (pa < p->high) && p->write) {
            p->write(pa, val, size);
            return;
        }
    }

    /* Not found. */
    sim_debug(IO_D_MSG, &cpu_dev,
              "WRITE ERROR. No IO device listening at address %08x\n", pa);

    cpu_abort(NORMAL_EXCEPTION, EXTERNAL_MEMORY_FAULT);
}
