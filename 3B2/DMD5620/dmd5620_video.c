/* vax_vc.c: QVSS video simulator (VCB01)

   Copyright (c) 2011-2013, Matt Burke

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
   THE AUTHOR BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
   IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   Except as contained in this notice, the name of the author shall not be
   used in advertising or otherwise to promote the sale, use or other dealings
   in this Software without prior written authorization from the author.

   vc           Qbus video subsystem
*/

#include "3b2_defs.h"
#include "dmd5620_sysdev.h"
#include "dmd5620_uart.h"
#include "sim_video.h"
#include "sim_tmxr.h"

#define VC_XSIZE        800                             /* screen size */
#define VC_YSIZE        1024

#define VC_HZ           60

int32 vc_poll;                                 /* calibrated delay */
t_bool vc_reverse_video;

DEVICE vc_dev;
t_stat vc_svc (UNIT *uptr);
t_stat vb_svc (UNIT *uptr);
t_stat vc_reset (DEVICE *dptr);
t_stat vc_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, char *cptr);

/* xxx */

UNIT vc_unit = { UDATA (&vc_svc, UNIT_IDLE, 0), 1000000/VC_HZ };
UNIT vb_unit = { UDATA (&vb_svc, UNIT_IDLE, 0), 1000000/VC_HZ };

MTAB vc_mod[] = {
    { MTAB_XTD|MTAB_VDV, 0, "RELEASEKEY", NULL,
        NULL, &vid_show_release_key, NULL, "Display the window focus release key" },
    { 0 }
    };

DEVICE vc_dev = {
    "VIDEO",             /* Name */
    &vc_unit,            /* Units */
    NULL,                /* Registers */
    vc_mod,              /* Modifiers */
    1,                   /* Number of Units */
    16,                  /* Address radix */
    32,                  /* Address width */
    1,                   /* Addr increment */
    16,                  /* Data radix */
    8,                   /* Data width */
    NULL,                /* Examine routine */
    NULL,                /* Deposit routine */
    &vc_reset,           /* Reset routine */
    NULL,                /* Boot routine */
    NULL,                /* Attach routine */
    NULL,                /* Detach routine */
    NULL,                /* Context */
    DEV_DISPLAY|DEV_DEBUG,/* Flags */
    0,                   /* Debug control flags */
    sys_deb_tab,         /* Debug flag names */
    NULL,                /* Memory size change */
    NULL                 /* Logical names */
};

/* implementation */

t_stat vc_svc (UNIT *uptr)
{
    uint32 line[VC_XSIZE], *p, *b, gfx, fg, bg;
    uint32 ln, col, off;
    int32 xpos, ypos, dx, dy, t;
    uint8 *cur;
    SIM_MOUSE_EVENT ev;

    if (vc_reverse_video) {
		bg = vid_mono_palette[1];
		fg = vid_mono_palette[0];
    } else {
		bg = vid_mono_palette[0];
		fg = vid_mono_palette[1];
    }

    for (ln = 0; ln < VC_YSIZE; ln++) {
	p = &RAM[(daddr_data>>2) + ln*(VC_XSIZE/32)];
        b = &line[0];
        for (col = 0; col < VC_XSIZE/32; col++) {
            gfx = *p++;
            for (off = 31; off >= 0; off--) {
                *b++ = ISSET(gfx, off) ? fg : bg;
            }
        }
        vid_draw (0, ln, VC_XSIZE, 1, &line[0]);        /* update line */
    }

    vid_refresh ();                                     /* put to screen */
    if (vid_poll_mouse (&ev) == SCPE_OK) {
    	mouse_set_xy(&ev);
		mouse_buttons(&ev);
    }

    vc_poll = sim_rtcn_calb (VC_HZ, 0);                 /* calibrate clock */
    sim_activate_after (&vc_unit, vc_poll);             /* reactivate unit */

    t = sim_rtcn_calb (62.52, 1);			/* 32.01 hsync / 512 scans */
    sim_activate_after (&vb_unit, t);

    uart_vsync(1);

    return SCPE_OK;
}

t_stat vb_svc (UNIT *uptr)
{
    uart_vsync(0);

    return SCPE_OK;
}

t_stat vc_reset (DEVICE *dptr)
{
    uint32 i;
    t_stat r;

    sim_cancel (&vc_unit);                                  /* stop poll */
    sim_register_clock_unit (&vc_unit);                     /* declare clock unit */
    sim_rtcn_init (vc_unit.wait, 0);                  /* init line clock */
    sim_activate (&vc_unit, vc_unit.wait);                 /* activate unit */
    vc_poll = vc_unit.wait;                               /* set mux poll */

    sim_cancel (&vb_unit);
    sim_register_clock_unit (&vb_unit);
    sim_rtcn_init (vb_unit.wait, 1);

    if (!vid_active)  {
        r = vid_open (dptr, NULL, VC_XSIZE, VC_YSIZE, SIM_VID_INPUTCAPTURED);            /* display size */
        if (r != SCPE_OK)
            return r;
        sim_printf ("Display Created.  ");
        vid_show_release_key (stdout, NULL, 0, NULL);
        if (sim_log)
            vid_show_release_key (sim_log, NULL, 0, NULL);
        sim_printf ("\n");
        }
    sim_activate_abs (&vc_unit, vc_poll);
    return SCPE_OK;
}

void vc_set_reverse_video (t_bool set)
{
    vc_reverse_video = set;
}

t_stat vc_help (FILE *st, DEVICE *dptr, UNIT *uptr, int32 flag, char *cptr)
{
    fprintf (st, "DMD5620 Monochrome Video Subsystem (%s)\n\n", dptr->name);
    fprintf (st, "Use the Control-Right-Shift key combination to regain focus from the simulated\n");
    fprintf (st, "video display\n");
    fprint_set_help (st, dptr);
    fprint_show_help (st, dptr);
    fprint_reg_help (st, dptr);

    return SCPE_OK;
}
