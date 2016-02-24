/* 3b2_uart.c:  SCN2681A Dual UART Implementation

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

#include "3b2_uart.h"

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

/*
 * Registers
 */

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
    uint16 c_val;         /* Timer / Counter Value */
    t_bool c_en;          /* Counter Enabled */
    struct port port[2];  /* Port A and B */
};

/* The UART state */
struct uart_state u;

UNIT uart_unit = { UDATA(&uart_svc, TT_MODE_7B, 0), 1000L };

REG uart_reg[] = {
    { HRDATAD(ISTAT,    u.istat,            8, "Interrupt Status") },
    { HRDATAD(IMASK,    u.imask,            8, "Interrupt Mask")   },
    { HRDATAD(CTR,      u.c_set,           16, "Counter Setting")  },
    { HRDATAD(CTRV,     u.c_val,           16, "Counter Value")    },
    { HRDATAD(STAT_A,   u.port[0].stat,     8, "Status  (Port A)") },
    { HRDATAD(CMD_A,    u.port[0].cmd,      8, "Command (Port A)") },
    { HRDATAD(DATA_A,   u.port[0].buf,      8, "Data    (Port A)") },
    { HRDATAD(STAT_B,   u.port[1].stat,     8, "Status  (Port B)") },
    { HRDATAD(CMD_B,    u.port[1].cmd,      8, "Command (Port B)") },
    { HRDATAD(DATA_B,   u.port[1].buf,      8, "Data    (Port B)") },
    { NULL }
};

DEVICE uart_dev = {
    "UART", &uart_unit, uart_reg, NULL,
    1, 8, 32, 1, 8, 8,
    NULL, NULL, &uart_reset,
    NULL, NULL, NULL, NULL,
    DEV_DEBUG, 0, sys_deb_tab
};

t_stat uart_reset(DEVICE *dptr)
{
    memset(&u, 0, sizeof(struct uart_state));

    u.c_en = FALSE;

    sim_activate(&uart_unit, uart_unit.wait);

    return SCPE_OK;
}

t_stat uart_svc(UNIT *uptr)
{
    int32 temp;

    /* sim_activate(&uart_unit, uart_unit.wait); */
    sim_activate(&uart_unit, uart_unit.wait);

    if (u.c_en && --u.c_val == 0) {
        u.istat |= ISTS_CRI;
        /* TODO: Interrupt here */
        return SCPE_OK;
    }

    if ((temp = sim_poll_kbd()) < SCPE_KFLAG) {
        return temp;
    }

    if (u.port[PORT_A].cmd & CMD_ETX) {
        uart_w_buf(PORT_A, temp);
        uart_update_rxi(temp);
    }

    return SCPE_OK;
}

/*
 *     Reg |       Name (Read)       |        Name (Write)
 *    -----+-------------------------+----------------------------
 *      0  | Mode Register A         | Mode Register A
 *      1  | Status Register A       | Clock Select Register A
 *      2  | BRG Test                | Command Register A
 *      3  | Rx Holding Register A   | Tx Holding Register A
 *      4  | Input Port Change Reg.  | Aux. Control Register
 *      5  | Interrupt Status Reg.   | Interrupt Mask Register
 *      6  | Counter/Timer Upper Val | C/T Upper Preset Val.
 *      7  | Counter/Timer Lower Val | C/T Lower Preset Val.
 *      8  | Mode Register B         | Mode Register B
 *      9  | Status Register B       | Clock Select Register B
 *     10  | 1X/16X Test             | Command Register B
 *     11  | Rx Holding Register B   | Tx Holding Register B
 *     12  | *Reserved*              | *Reserved*
 *     13  | Input Ports IP0 to IP6  | Output Port Conf. Reg.
 *     14  | Start Counter Command   | Set Output Port Bits Cmd.
 *     15  | Stop Counter Command    | Reset Output Port Bits Cmd.
 */

uint32 uart_read(uint32 pa, uint8 size)
{
    uint8 reg;
    uint32 data;

    reg = pa - UARTBASE;

    switch (reg) {
    case 0:
        data = u.port[PORT_A].mode[u.port[PORT_A].mode_ptr];
        u.port[PORT_A].mode_ptr++;
        if (u.port[PORT_A].mode_ptr > 1) {
            u.port[PORT_A].mode_ptr = 0;
        }
        break;
    case 1:
        data = u.port[PORT_A].stat;
        u.port[PORT_A].stat &= ~STS_RXR;
        break;
    case 3:
        data = u.port[PORT_A].buf | (u.port[PORT_A].stat << 8);
        u.port[PORT_A].stat &= ~STS_RXR;
        u.istat &= ~ISTS_RAI;
        break;
    case 5:
        data = u.istat;
        break;
    case 8:
        data = u.port[PORT_B].mode[u.port[PORT_B].mode_ptr];
        u.port[PORT_B].mode_ptr++;
        if (u.port[PORT_B].mode_ptr++ > 1) {
            u.port[PORT_B].mode_ptr = 0;
        }
        break;
    case 9:                                             /* status/clock B */
        data = u.port[PORT_B].stat;
        break;
    case 11:                                            /* tx/rx buf B */
        data = u.port[PORT_B].buf | (u.port[PORT_B].stat << 8);
        u.port[PORT_B].stat &= ~STS_RXR;
        u.istat &= ~ISTS_RBI;
        break;
    case 14:
        /* Start Counter Command */
        u.c_en = TRUE;
        break;
    case 15:
        /* Stop Counter Command */
        u.c_en = FALSE;
        u.istat &= ~ISTS_CRI;
        break;
    case 0x11: /* Clear DMAC interrupt */
        break;
    default:
        data = 0;
        break;
    }

    return data;
}

void uart_write(uint32 pa, uint32 val, uint8 size)
{
    uint8 reg;
    uint8 mode_ptr;
    uint8 timer_mode;

    reg = pa - UARTBASE;

    switch (reg) {
    case 0:                /* Mode 1A, 2A */
        mode_ptr = u.port[PORT_A].mode_ptr;
        u.port[PORT_A].mode[mode_ptr++] = val & 0xff;
        if (mode_ptr > 1) {
            mode_ptr = 0;
        }
        u.port[PORT_A].mode_ptr = mode_ptr;
        break;
    case 1:
        /* Set baud rate - not implemented */
        break;
    case 2:  /* Command A */
        uart_w_cmd(PORT_A, val);
        uart_update_txi();
        break;
    case 3:  /* TX/RX Buf A */
        uart_w_buf(PORT_A, val);
        uart_update_txi();
        if (u.port[PORT_A].cmd & CMD_ETX) {
            /* TODO: This is probably not right, but let's do this for
               debugging / testing, and fix it later. */
            sim_putchar_s(u.port[PORT_A].buf);
        }
        break;
    case 4:  /* Auxiliary Control Register */
        /* Set the mode of the timer */
        timer_mode = (val >> 4) & 7;
        break;
    case 5:
        u.imask = val;
        break;
    case 6:  /* Counter/Timer UpperPreset Value */
        u.c_set |= (val & 0xff) << 8;
        u.c_val = u.c_set >> 8;
        break;
    case 7:  /* Counter/Timer Lower Preset Value */
        u.c_set |= (val & 0xff);
        u.c_val = u.c_set >> 8;
        break;
    case 10: /* Command B */
        uart_w_cmd(PORT_B, val);
        uart_update_txi();
        break;
    case 11: /* TX/RX Buf B */
        uart_w_buf(PORT_B, val);
        uart_update_txi();
        break;
    case 0x11: /* Unknown register in the memory map */
        sim_debug(READ_MSG, &uart_dev, ">>> Unknown device at 49011, val=%02x\n", val);
        break;
    default:
        break;
    }
}

static SIM_INLINE void uart_update_txi()
{
    if (u.port[PORT_A].cmd & CMD_ETX) {                 /* Transmitter A enabled? */
        u.port[PORT_A].stat |= STS_TXR;                 /* ready */
        u.port[PORT_A].stat |= STS_TXE;                 /* empty */
        u.istat |= ISTS_TAI;                            /* set int */
    } else {
        u.port[PORT_A].stat &= ~STS_TXR;                /* clear ready */
        u.port[PORT_A].stat &= ~STS_TXE;                /* clear empty */
        u.istat &= ~ISTS_TAI;                           /* clear int */
    }

    if (u.port[PORT_B].cmd & CMD_ETX) {                 /* Transmitter B enabled? */
        u.port[PORT_B].stat |= STS_TXR;                 /* ready */
        u.port[PORT_B].stat |= STS_TXE;                 /* empty */
        u.istat |= ISTS_TBI;                            /* set int */
    } else {
        u.port[PORT_B].stat &= ~STS_TXR;                /* clear ready */
        u.port[PORT_B].stat &= ~STS_TXE;                /* clear empty */
        u.istat &= ~ISTS_TBI;                           /* clear int */
    }

    if ((u.istat & u.imask) > 0) {                      /* unmasked ints? */
        /* TODO: Set interrupt */
    } else {
        /* TODO: Clear interrupt */
    }

}

static SIM_INLINE void uart_update_rxi(uint8 c)
{
    t_stat result;

    if (u.port[PORT_A].cmd & CMD_ERX) {
        if (((u.port[PORT_A].stat & STS_RXR) == 0)) {
            u.port[PORT_A].buf = c;
            u.port[PORT_A].stat |= STS_RXR;
            u.istat |= ISTS_RAI;
        }
    } else {
        u.port[PORT_A].stat &= ~STS_RXR;
        u.istat &= ~ISTS_RAI;
    }

    if (u.port[PORT_B].cmd & CMD_ERX) {
        if (((u.port[PORT_B].stat & STS_RXR) == 0)) {
            u.port[PORT_B].buf = c;
            u.port[PORT_B].stat |= STS_RXR;
            u.istat |= ISTS_RBI;
        }
    } else {
        u.port[PORT_B].stat &= ~STS_RXR;
        u.istat &= ~ISTS_RBI;
    }

    if ((u.istat & u.imask) > 0) {
        /* TODO: Set interrupt */
        sim_debug(READ_MSG, &uart_dev, "Setting Interrupt\n");
    } else {
        /* TODO: Clear interrupt */
        sim_debug(READ_MSG, &uart_dev, "Clearing Interrupt\n");
    }

}

static SIM_INLINE void uart_w_buf(uint8 portno, uint8 val)
{
    u.port[portno].buf = val;
    u.port[portno].stat |= STS_RXR;
    switch (portno) {
    case PORT_A:
        u.istat |= ISTS_RAI;
        break;
    case PORT_B:
        u.istat |= ISTS_RBI;
        break;

    }
}

static SIM_INLINE void uart_w_cmd(uint8 portno, uint8 val)
{
    /* Enable or disable transmitter */
    if (val & CMD_ETX) {
        u.port[portno].cmd |= CMD_ETX;
    } else if (val & CMD_DTX) {
        u.port[portno].cmd &= ~CMD_ETX;
    }

    /* Enable or disable receiver */
    if (val & CMD_ERX) {
        u.port[portno].cmd |= CMD_ERX;
    } else if (val & CMD_DRX) {
        u.port[portno].cmd &= ~CMD_ERX;
    }

    switch ((val >> CMD_V_CMD) & CMD_M_CMD) {
    case 1:
        u.port[portno].mode_ptr = 0;
        break;
    case 2:
        u.port[portno].cmd &= ~CMD_ERX;
        u.port[portno].stat &= ~STS_RXR;
        break;
    case 3:
        u.port[portno].stat &= ~STS_TXR;
        break;
    case 4:
        u.port[portno].stat &= ~(STS_FER | STS_PER | STS_OER);
        break;
    }
}
