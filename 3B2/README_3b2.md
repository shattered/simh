AT&T 3B2 Simulator
==================

This module contains a simulator for the AT&T 3B2 Model 400 microcomputer.

*CAUTION*: The simulator is under active and heavy development, and is not
yet of usable quality! Consider this emulator to be a pre-alpha!

Devices
-------

The following devices are emulated. The SIMH names for the emulated
devices are given in parentheses:

  - WE32100 CPU (CPU) 
  - WE32101 MMU (MMU)
  - 3B2 Model 400 System Board with 512KB, 1MB, or 2MB RAM
    (TIMER, CSR, NVRAM)
  - AM9517 DMA controller (DMAC)
  - SCN2681A DUART (UART)
  - TMS2793 Integrated Floppy Controller (IF)
  - uPD7261A Integrated MFM Fixed Disk Controller (ID)

Usage
-----

To boot the 3B2 simulator into firmware mode, simply type:

    sim> BOOT CPU

You will be greeted with the message:

    FW ERROR 1-01: NVRAM SANITY FAILURE
                   DEFAULT VALUES ASSUMED
                   IF REPEATED, CHECK THE BATTERY

    FW ERROR 1-02: DISK SANITY FAILURE
                   EXECUTION HALTED

    SYSTEM FAILURE: CONSULT YOUR SYSTEM ADMINISTRATION UTILITIES GUIDE

This is actually an invisible prompt. To access firmware mode, type
the default 3B2 firmware password "mcp" (without the quotes), then
press Enter or carriage return.

You should then be prompted with:

    Enter name of program to execute [  ]: 

Here, you may type a question mark (?) and press Enter to see a list
of available firmware programs.

