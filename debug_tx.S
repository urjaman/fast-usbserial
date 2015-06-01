#include <avr/io.h>

.section .text

/* 2Mbaud, here we come. */
.global debug_send
debug_send:
	mov r30, r24
	cli
	in r3, _SFR_IO_ADDR(SREG)
	ldi r24, 0x80
	or r3, r24
	cbi _SFR_IO_ADDR(PORTB), 4
	in r31, _SFR_IO_ADDR(PORTB)
	bst r30, 0
	bld r31, 4
	rjmp 0f

.global debug_send_isr
debug_send_isr:
	/* r3 is reserved for the UART ISRs, but in CLI context we can use it anyways
	 * since they dont store anything there between ISRs. */
	cbi _SFR_IO_ADDR(PORTB), 4
	in r31, _SFR_IO_ADDR(PORTB)
	in r3, _SFR_IO_ADDR(SREG)
	bst r30, 0
	bld r31, 4
	adiw r26, 0
0:
	nop
	out _SFR_IO_ADDR(PORTB), r31
	bst r30, 1
	bld r31, 4
	nop
	adiw r26, 0 /* 2 cycles, 1 word. */
	adiw r26, 0
	out _SFR_IO_ADDR(PORTB), r31
	bst r30, 2
	bld r31, 4
	nop
	adiw r26, 0 /* 2 cycles, 1 word. */
	adiw r26, 0
	out _SFR_IO_ADDR(PORTB), r31
	bst r30, 3
	bld r31, 4
	nop
	adiw r26, 0 /* 2 cycles, 1 word. */
	adiw r26, 0
	out _SFR_IO_ADDR(PORTB), r31
	bst r30, 4
	bld r31, 4
	nop
	adiw r26, 0 /* 2 cycles, 1 word. */
	adiw r26, 0
	out _SFR_IO_ADDR(PORTB), r31
	bst r30, 5
	bld r31, 4
	nop
	adiw r26, 0 /* 2 cycles, 1 word. */
	adiw r26, 0
	out _SFR_IO_ADDR(PORTB), r31
	bst r30, 6
	bld r31, 4
	nop
	adiw r26, 0 /* 2 cycles, 1 word. */
	adiw r26, 0
	out _SFR_IO_ADDR(PORTB), r31
	bst r30, 7
	bld r31, 4
	nop
	adiw r26, 0 /* 2 cycles, 1 word. */
	adiw r26, 0
	out _SFR_IO_ADDR(PORTB), r31
	adiw r26, 0 /* 2 cycles, 1 word. */
	adiw r26, 0
	adiw r26, 0 /* 2 cycles, 1 word. */
	out _SFR_IO_ADDR(SREG), r3 /* sei if non-ISR, restore T reg if ISR. */
	sbi _SFR_IO_ADDR(PORTB), 4
	ret /* ret 4 + rjmp 2 + cli 1 + mov=> 8 cyles before start of next minimum. */
