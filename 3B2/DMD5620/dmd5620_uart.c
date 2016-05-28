/* dmd5620_uart.c:  SCN2681A Dual UART Implementation

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

#include "dmd5620_uart.h"


UNIT uart_unit = { UDATA(&uart_svc, UNIT_ATTABLE|TT_MODE_7B, 0), 1000L };
TMLN uart_ldsc[1] = { { 0 } };
TMXR uart_desc = { 1, 0, 0, uart_ldsc };
uint8 uart_oport = 0;
t_bool kb_shift = 0;
t_bool kb_ctrl = 0;

DEVICE uart_dev = {
    "UART", &uart_unit, NULL, NULL,
    1, 8, 32, 1, 8, 8,
    NULL, NULL, &uart_reset,
    NULL, &uart_attach, &uart_detach, NULL,
    DEV_DEBUG | DEV_MUX, 0, sys_deb_tab
};

UART2681 uart_chip = {
    &uart_int, &uart_output_port,
    { { &ln_wr, &ln_rd }, { NULL, &kb_rd } }
    };

t_stat uart_reset(DEVICE *dptr)
{
    uart_ldsc[0].rcve = 1;

    sim_cancel (&uart_unit);
    ua2681_reset (&uart_chip);
    sim_activate (&uart_unit, uart_unit.wait);

    return SCPE_OK;
}

t_stat uart_attach (UNIT *uptr, char *cptr)
{
    if (uptr == &uart_unit)
        return (tmxr_attach (&uart_desc, uptr, cptr));
    return (SCPE_NOATT);
}

t_stat uart_detach (UNIT *uptr)
{
    return (tmxr_detach (&uart_desc, uptr));
}

t_stat uart_svc(UNIT *uptr)
{
    int32 temp;

    sim_activate (&uart_unit, uart_unit.wait);
    ua2681_svc (&uart_chip);

    if ((temp = sim_poll_kbd()) < SCPE_KFLAG) {
         return temp;
    }

    return SCPE_OK;
}

uint32 uart_read(uint32 pa, size_t size)
{
    uint8 reg;
    uint32 data;

    reg = (pa - UARTBASE - 3) / 4;

    data = ua2681_rd (&uart_chip, reg);

    sim_debug(EXECUTE_MSG, &uart_dev, "UART read pa %x reg %d == %02x\n", pa, reg, data);

    return data;
}

void uart_write(uint32 pa, uint32 val, size_t size)
{
    uint8 reg;
    uint8 mode_ptr;
    uint8 timer_mode;

    reg = (pa - UARTBASE - 3) / 4;
    ua2681_wr (&uart_chip, reg, val);

    if (!(reg == 10 && val == 0))
        sim_debug(EXECUTE_MSG, &uart_dev, "UART write pa %x reg %d <- %02x\n", pa, reg, val);
}

/* only input port change interrupts are enabled */
void uart_int (uint32 set)
{
    if (set)
	int_controller_set(IRQ_INTM0);
    else
	int_controller_clear(IRQ_INTM0);
}

void uart_output_port (uint32 val)
{
    uint8 t = uart_oport ^ val;

    if (t)
    sim_debug(INIT_MSG, &uart_dev, "UART write oport %02x <- %02x\n", uart_oport ^ 0xff, val ^ 0xff);

    if (ISSET(t, 1)) {
        sim_debug(INIT_MSG, &uart_dev, "%ssetting reverse video\n", ISSET(val, 1) ? "" : "re");
	vc_set_reverse_video(ISSET(val, 1) ? TRUE : FALSE);
    }
    if (ISSET(t, 4)) {
	if (ISSET(val, 4))
	    int_controller_clear(IRQ_INT232R);
	else {
	    sim_debug(IRQ_MSG, &uart_dev, "UART interrupting (host rx)\n");
	    int_controller_set(IRQ_INT232R);
	}
    }
    if (ISSET(t, 5)) {
	if (ISSET(val, 5))
	    int_controller_clear(IRQ_INTKBD);
	else {
	    sim_debug(IRQ_MSG, &uart_dev, "UART interrupting (kbd rx)\n");
	    int_controller_set(IRQ_INTKBD);
	}
    }
    if (ISSET(t, 6)) {
	if (ISSET(val, 6))
	    int_controller_clear(IRQ_INT232S);
	else {
	    sim_debug(IRQ_MSG, &uart_dev, "UART interrupting (host tx)\n");
	    int_controller_set(IRQ_INT232S);
	}
    }

    uart_oport = val;
}

void uart_vsync (uint32 set)
{
    ua2681_ip2_wr(&uart_chip, set);
}

void mouse_buttons (SIM_MOUSE_EVENT *ev)
{
    sim_debug(INIT_MSG, &uart_dev, "mouse buttons: %d %d %d\n", ev->b1_state, ev->b2_state, ev->b3_state);

    ua2681_ip3_wr(&uart_chip, 0 == ev->b1_state);
    ua2681_ip1_wr(&uart_chip, 0 == ev->b2_state);
    ua2681_ip0_wr(&uart_chip, 0 == ev->b3_state);
}

/* keyboard */

/* mapping table via 5620rom/src/lib/libsys/kbd.c */

t_bool kb_map_key (uint32 key, uint32 state, uint8 *c)
{
uint8 lk_key;

switch (key) {
    case SIM_KEY_SHIFT_R:
    case SIM_KEY_SHIFT_L:
	kb_shift = (state == SIM_KEYPRESS_UP) ? FALSE : TRUE;
	return SCPE_EOF;

    case SIM_KEY_CTRL_R:
    case SIM_KEY_CTRL_L:
	kb_ctrl = (state == SIM_KEYPRESS_UP) ? FALSE : TRUE;
	return SCPE_EOF;
}

if (state == SIM_KEYPRESS_UP) {
    return SCPE_EOF;
}

switch (key) {

    case SIM_KEY_F1:
        lk_key = 0xe8;
        break;

    case SIM_KEY_F2:
        lk_key = 0xe9;
        break;

    case SIM_KEY_F3:
        lk_key = 0xea;
        break;

    case SIM_KEY_F4:
        lk_key = 0xeb;
        break;

    case SIM_KEY_F5:
        lk_key = 0xec;
        break;

    case SIM_KEY_F6:
        lk_key = 0xed;
        break;

    case SIM_KEY_F7:
        lk_key = 0xee;
        break;

    case SIM_KEY_F8:
        lk_key = 0xef;
        break;

    /* shift-setup key (= reboot) */
    case SIM_KEY_F11:
        lk_key = 0x8e;
        break;

    /* setup key */
    case SIM_KEY_F12:
        lk_key = 0xae;
        break;

#define KB_SHIFT(c,s)		(kb_shift?s:c)
#define KB_SHIFT_CTRL(c)	(kb_ctrl?(c-0x60):(kb_shift?(c-0x20):c))
#define KB_SHIFT_CTRL2(c)	(kb_ctrl?(c-0x40):(kb_shift?(c+0x20):c))

    /*
     * modifiable keys
     */
    case SIM_KEY_0:
        lk_key = KB_SHIFT('0',')');
        break;

    case SIM_KEY_1:
        lk_key = KB_SHIFT('1','!');
        break;

    case SIM_KEY_2:
        lk_key = KB_SHIFT('2','@');
        break;

    case SIM_KEY_3:
        lk_key = KB_SHIFT('3','#');
        break;

    case SIM_KEY_4:
        lk_key = KB_SHIFT('4','$');
        break;

    case SIM_KEY_5:
        lk_key = KB_SHIFT('5','%');
        break;

    case SIM_KEY_6:
        lk_key = KB_SHIFT('6','^');
        break;

    case SIM_KEY_7:
        lk_key = KB_SHIFT('7','&');
        break;

    case SIM_KEY_8:
        lk_key = KB_SHIFT('8','*');
        break;

    case SIM_KEY_9:
        lk_key = KB_SHIFT('9','(');
        break;

    case SIM_KEY_A:
        lk_key = KB_SHIFT_CTRL('a');
        break;

    case SIM_KEY_B:
        lk_key = KB_SHIFT_CTRL('b');
        break;

    case SIM_KEY_C:
        lk_key = KB_SHIFT_CTRL('c');
        break;

    case SIM_KEY_D:
        lk_key = KB_SHIFT_CTRL('d');
        break;

    case SIM_KEY_E:
        lk_key = KB_SHIFT_CTRL('e');
        break;

    case SIM_KEY_F:
        lk_key = KB_SHIFT_CTRL('f');
        break;

    case SIM_KEY_G:
        lk_key = KB_SHIFT_CTRL('g');
        break;

    case SIM_KEY_H:
        lk_key = KB_SHIFT_CTRL('h');
        break;

    case SIM_KEY_I:
        lk_key = KB_SHIFT_CTRL('i');
        break;

    case SIM_KEY_J:
        lk_key = KB_SHIFT_CTRL('j');
        break;

    case SIM_KEY_K:
        lk_key = KB_SHIFT_CTRL('k');
        break;

    case SIM_KEY_L:
        lk_key = KB_SHIFT_CTRL('l');
        break;

    case SIM_KEY_M:
        lk_key = KB_SHIFT_CTRL('m');
        break;

    case SIM_KEY_N:
        lk_key = KB_SHIFT_CTRL('n');
        break;

    case SIM_KEY_O:
        lk_key = KB_SHIFT_CTRL('o');
        break;

    case SIM_KEY_P:
        lk_key = KB_SHIFT_CTRL('p');
        break;

    case SIM_KEY_Q:
        lk_key = KB_SHIFT_CTRL('q');
        break;

    case SIM_KEY_R:
        lk_key = KB_SHIFT_CTRL('r');
        break;

    case SIM_KEY_S:
        lk_key = KB_SHIFT_CTRL('s');
        break;

    case SIM_KEY_T:
        lk_key = KB_SHIFT_CTRL('t');
        break;

    case SIM_KEY_U:
        lk_key = KB_SHIFT_CTRL('u');
        break;

    case SIM_KEY_V:
        lk_key = KB_SHIFT_CTRL('v');
        break;

    case SIM_KEY_W:
        lk_key = KB_SHIFT_CTRL('w');
        break;

    case SIM_KEY_X:
        lk_key = KB_SHIFT_CTRL('x');
        break;

    case SIM_KEY_Y:
        lk_key = KB_SHIFT_CTRL('y');
        break;

    case SIM_KEY_Z:
        lk_key = KB_SHIFT_CTRL('z');
        break;

    case SIM_KEY_BACKQUOTE:
        lk_key = KB_SHIFT('`','~');
        break;

    case SIM_KEY_MINUS:
        lk_key = KB_SHIFT('-','_');
        break;

    case SIM_KEY_EQUALS:
        lk_key = KB_SHIFT('=','+');
        break;

    case SIM_KEY_LEFT_BRACKET:
        lk_key = KB_SHIFT_CTRL2('[');
        break;

    case SIM_KEY_RIGHT_BRACKET:
        lk_key = KB_SHIFT_CTRL2(']');
        break;

    case SIM_KEY_SEMICOLON:
        lk_key = KB_SHIFT(';',':');
        break;

    case SIM_KEY_SINGLE_QUOTE:
        lk_key = KB_SHIFT(0x27,'"');
        break;

    case SIM_KEY_BACKSLASH:
        lk_key = KB_SHIFT('\\','|');
        break;

    case SIM_KEY_COMMA:
        lk_key = KB_SHIFT(',','<');
        break;

    case SIM_KEY_PERIOD:
        lk_key = KB_SHIFT('.','>');
        break;

    case SIM_KEY_SLASH:
        lk_key = KB_SHIFT('/','?');
        break;

    case SIM_KEY_BACKSPACE:
        lk_key = 0xf1;
        break;

    case SIM_KEY_TAB:
        lk_key = 0xd0;
        break;

    case SIM_KEY_ENTER:
        lk_key = 0xe7;
        break;

    case SIM_KEY_KP_LEFT:
        lk_key = '4';
        break;

    case SIM_KEY_KP_5:
        lk_key = '5';
        break;

    case SIM_KEY_KP_RIGHT:
        lk_key = '6';
        break;

    case SIM_KEY_KP_HOME:
        lk_key = '7';
        break;

    case SIM_KEY_KP_UP:
        lk_key = '8';
        break;

    case SIM_KEY_KP_PAGE_UP:
        lk_key = '9';
        break;

    case SIM_KEY_HOME:	// left-up arrow
    case SIM_KEY_KP_END:
        lk_key = 0xc0;
        break;

    case SIM_KEY_UP:	// up arrow
    case SIM_KEY_KP_DOWN:
        lk_key = 0xc1;
        break;

    case SIM_KEY_DOWN:	// down arrow
    case SIM_KEY_KP_INSERT:
        lk_key = 0xc2;
        break;

    case SIM_KEY_RIGHT:	// right arrow
    case SIM_KEY_KP_DELETE:
        lk_key = 0xc3;
        break;

    case SIM_KEY_LEFT:	// left arrow
    case SIM_KEY_KP_SUBTRACT:
        lk_key = 0xc4;
        break;

    case SIM_KEY_END:	// home down arrow
    case SIM_KEY_KP_PAGE_DOWN:
        lk_key = 0xc6;
        break;

    case SIM_KEY_DELETE:// del
        lk_key = 0xfe;
        break;

    case SIM_KEY_ESC:	// esc
        lk_key = 0xe3;
        break;

    case SIM_KEY_F9:	// line feed
        lk_key = 0xb6;
        break;

    case SIM_KEY_F10:	// clear
        lk_key = 0xe5;
        break;

    case SIM_KEY_NUM_LOCK:
        lk_key = 0xb2;
        break;

#if 0
    case SIM_KEY_XXX:	// break / discon
        lk_key = 0xaf;
        break;
#endif

    case SIM_KEY_SPACE:
        lk_key = kb_ctrl?'\0':' ';
        break;

    case SIM_KEY_UNKNOWN:
	return SCPE_EOF;
        break;

    default:
	return SCPE_EOF;
        break;
    }
*c = lk_key;
return SCPE_OK;
}

t_stat kb_rd (uint8 *c)
{
    SIM_KEY_EVENT ev;
    t_bool rc;

    if (vid_poll_kb (&ev) != SCPE_OK) {
	*c = 0;
	return SCPE_EOF;
    }

    rc = kb_map_key (ev.key, ev.state, c);
    if (rc == SCPE_OK) {
	sim_debug(READ_MSG, &uart_dev, "from kbrd: '%02x'\n", *c);
    }
    return rc;
}

t_stat ln_rd (uint8 *c)
{
    int32 temp;

    tmxr_poll_conn (&uart_desc);
    tmxr_poll_rx (&uart_desc);

    if (TMXR_VALID & (temp = tmxr_getc_ln (&uart_ldsc[0]))) {
        sim_debug(READ_MSG, &uart_dev, "from tmxr: '%02x'\n", temp);
        *c = (uint8) temp;
        return SCPE_OK;
    } else
        return SCPE_EOF;
}

t_stat ln_wr (uint8 c)
{
    t_stat temp;

    sim_debug(WRITE_MSG, &uart_dev, "to  tmxr: '%02x'\n", c);
    temp = tmxr_putc_ln (&uart_ldsc[0], c);
    tmxr_poll_tx (&uart_desc);
    return temp;
}

/* Implementation for the interrupt controller */
static void int_controller_set(uint8 value) {
    const uint8 old_pending = int_controller_pending;

    int_controller_pending |= (1<<value);
    int_controller_pending &= 0x3f;

//  if (value != IRQ_INTM0)
    sim_debug(IRQ_MSG, &uart_dev, "INTC: set %d (pending %02x -> %02x ipl %d)\n", 
    	value, old_pending, int_controller_pending, int_controller_pal[int_controller_pending]);
//  if (old_pending != int_controller_pending)
    if (int_controller_pending)
        cpu_set_irq(int_controller_pal[int_controller_pending], int_controller_pending ^ 0x3f, FALSE);
}

static void int_controller_clear(uint8 value) {
    const uint8 old_pending = int_controller_pending;

    int_controller_pending &= ~(1<<value);
    int_controller_pending &= 0x3f;

//  if (value != IRQ_INTM0)
    sim_debug(IRQ_MSG, &uart_dev, "INTC: clear %d (pending %02x -> %02x ipl %d)\n", 
    	value, old_pending, int_controller_pending, int_controller_pal[int_controller_pending]);
    if (int_controller_pending)
	cpu_set_irq(int_controller_pal[int_controller_pending], int_controller_pending ^ 0x3f, FALSE);
}
