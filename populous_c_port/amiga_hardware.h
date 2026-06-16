#ifndef AMIGA_HARDWARE_H
#define AMIGA_HARDWARE_H

#include <stdint.h>

/* Amiga Custom Chip Base */
#define HARDBASE    0xDFF000

/* DMA and Interrupt registers (examples - expand as needed) */
#define DMACONR     (HARDBASE + 0x002)
#define VHPOSR      (HARDBASE + 0x006)
#define JOY0DAT     (HARDBASE + 0x00A)
#define POTGOR      (HARDBASE + 0x016)
#define INTENAR     (HARDBASE + 0x01C)
#define INTREQR     (HARDBASE + 0x01E)
#define POTGO       (HARDBASE + 0x034)
#define COP1LCH     (HARDBASE + 0x080)
#define COPJMP1     (HARDBASE + 0x088)
#define DMACON      (HARDBASE + 0x096)
#define INTENA      (HARDBASE + 0x09A)
#define INTREQ      (HARDBASE + 0x09C)
#define ADKCON      (HARDBASE + 0x09E)

/* CIA registers */
#define CIAA_PRA    0xBFE001
#define CIAA_SDR    0xBFEC01
#define EXT_BFE0FF  0xBFE0FF

/* Exec library base */
#define ABSEXECBASE 4

/* Common Amiga memory attributes */
#define MEMF_PUBLIC  (1L << 0)
#define MEMF_CHIP    (1L << 1)
#define MEMF_FAST    (1L << 2)
#define MEMF_CLEAR   (1L << 16)

#endif /* AMIGA_HARDWARE_H */