#ifndef _GLOBAL_REGS_H_
#define _GLOBAL_REGS_H_

/* I think most C files should include this file so they should know r2-r5 are reserved. */
//register unsigned char urx_rsvd2 asm("r2");
register unsigned char USBtoUSART_wrp asm("r2");
// Apparently you cant ever say the name of these or errors galore, so... just for declaration here.
register unsigned char urx_rsvd3 asm("r3");
register unsigned char urx_rsvd4 asm("r4");
register unsigned char urx_rsvd5 asm("r5");

#endif
