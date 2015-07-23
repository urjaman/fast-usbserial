/* Based on arduino-usbserial and LUFA. but made
 * into a stand-alone LUFA subset and heavily modified
 * by Urja Rannikko 2015. My modifications are under the LUFA
 * License below. */

/*
             LUFA Library
     Copyright (C) Dean Camera, 2010.
              
  dean [at] fourwalledcubicle [dot] com
      www.fourwalledcubicle.com
*/

/*
  Copyright 2010  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this 
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in 
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting 
  documentation, and that the name of the author not be used in 
  advertising or publicity pertaining to distribution of the 
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Main source file for the fast-usbserial project. This file contains the main tasks of
 *  the project and is responsible for the initial application hardware configuration.
 */

#include "fast-usbserial.h"

/* NOTE: Using Linker Magic,
 * - Reserved 256 bytes from start of RAM at 0x100 for UART RX Buffer
 * so we can use 256-byte aligned addresssing.
 * - Also 128 bytes from 0x200 for UART TX buffer, same addressing. */


#define USB2USART_BUFLEN 128
#define USBtoUSART_rdp GPIOR0
/* USBtoUSART_rdp is GPIOR0 so it can be masked with cbi. */
static volatile uint8_t USBtoUSART_wrp = 0;
/* USBtoUSART_wrp needs to be visible to ISR so saddly needs to be here. */

#define USART2USB_BUFLEN 256
#define USARTtoUSB_wrp GPIOR1

//#define DEBUGTX

#ifdef DEBUGTX
#include "debug_tx.h"
#define DEBUGB(x) debug_send(x)
#else
#define DEBUGB(x)
#endif

/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface;


/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
	SetupHardware();
#ifdef DEBUGTX
	PORTB |= _BV(4);
	DDRB |= _BV(4);
#endif
	/* Clear the GPIOR-based TX register. */
	USBtoUSART_rdp = 0;
	sei();
	DEBUGB(0xE1);
	for (;;) {
		/* We let the TX continue (flush buffer) if it was enabled before we got unconfigured. */
		ATOMIC_BLOCK(ATOMIC_FORCEON) {
			UCSR1B &= ~_BV(RXCIE1);
		}
		/* But disable RX since there is no longer a PC listening. */
		Endpoint_SelectEndpoint(ENDPOINT_CONTROLEP);
		do {
			if (Endpoint_IsSETUPReceived())
			  USB_Device_ProcessControlRequest();
		} while (USB_DeviceState != DEVICE_STATE_Configured);
		/* TX might still be transmitting, so be safe when re-enabling RX ISR. */
		ATOMIC_BLOCK(ATOMIC_FORCEON) {
			UCSR1B |= _BV(RXCIE1);
		}
		TIFR0 = _BV(TOV0);
		TIFR1 = _BV(OCF1A);

		/** Pulse generation counters to keep track of the number of milliseconds remaining for each pulse type */
		struct {
			uint8_t TxLEDPulse; /**< Milliseconds remaining for data Tx LED pulse */
			uint8_t RxLEDPulse; /**< Milliseconds remaining for data Rx LED pulse */
		} PulseMSRemaining = { 0,0 };
		uint8_t last_cnt = 0;
		uint8_t USARTtoUSB_rdp = USARTtoUSB_wrp; /* A single in is smaller than out and ldi (to clear) */
		do {
			uint8_t USBtoUSART_free = (USB2USART_BUFLEN-1) - ( (USBtoUSART_wrp - USBtoUSART_rdp) & (USB2USART_BUFLEN-1) );
			uint8_t rxd;
			if ( ((rxd = CDC_Device_BytesReceived(&VirtualSerial_CDC_Interface))) && (rxd <= USBtoUSART_free) ) {
				uint16_t tmp; //  = 0x200 | USBtoUSART_wrp;
				DEBUGB(0xE0);
				DEBUGB(rxd);
				uint8_t d;
				asm (
				"ldi %B0, 0x02\n\t"
				"lds %A0, %1\n\t"
				: "=&e" (tmp)
				: "m" (USBtoUSART_wrp)
				);
				do {
					d = Endpoint_Read_Byte();
					asm (
					"st %a0+, %2\n\t"
					"andi %A0, 0x7F\n\t"
					: "=e" (tmp)
					: "0" (tmp), "r" (d)
					);
					DEBUGB(d);
				} while (--rxd);
				Endpoint_ClearOUT();
				USBtoUSART_wrp = tmp & 0xFF; /* ASM already clears the lower byte to & 0x7F. */
				UCSR1B = (_BV(RXCIE1) | _BV(TXEN1) | _BV(RXEN1) | _BV(UDRIE1));
				goto rxled;
			} else if (USBtoUSART_wrp != USBtoUSART_rdp) {
				rxled:
				LEDs_TurnOnLEDs(LEDMASK_RX);
				PulseMSRemaining.RxLEDPulse = TX_RX_LED_PULSE_MS;
			}
			/* This requires the UART RX buffer to be 256 bytes. */
			uint8_t cnt = USARTtoUSB_wrp - USARTtoUSB_rdp;
			uint8_t flush_overflow = TIFR1 & _BV(OCF1A);
			if (flush_overflow) TIFR1 = _BV(OCF1A);
			/* Check if the UART receive buffer flush timer has expired or the buffer is nearly full */
			if ( ((cnt >= CDC_IN_EPSIZE) || (flush_overflow && cnt)) &&
				(CDC_Device_SendByte_Prep(&VirtualSerial_CDC_Interface) == 0) ) {
				/* Endpoint will always be empty since we're the only writer
				 * and we flush after every write. */
				uint8_t txcnt = CDC_IN_EPSIZE;
				if (txcnt > cnt) txcnt = cnt;
				last_cnt -= txcnt;
				DEBUGB(0xE2);
				DEBUGB(txcnt);
				uint16_t tmp;
				asm (
				/* Do not initialize high byte, it will be done on first loop. */
				"mov %A0, %1\n\t"
				: "=&e" (tmp)
				: "r" (USARTtoUSB_rdp)
				);
				do {
					uint8_t d;
					asm (
					"ldi %B1, 0x01\n\t" /* Force high byte */
					"ld %0, %a1+\n\t"
					: "=&r" (d), "=e" (tmp)
					: "1" (tmp)
					);
       	         	                Endpoint_Write_Byte(d);
					DEBUGB(d);
				} while (--txcnt);
		                Endpoint_ClearIN(); /* Go data, GO. */
				USARTtoUSB_rdp = tmp & 0xFF;
				goto txled;
			} else if (last_cnt != cnt) {
				last_cnt = cnt;
				txled:
				TCNT1 = 0;
				LEDs_TurnOnLEDs(LEDMASK_TX);
				PulseMSRemaining.TxLEDPulse = TX_RX_LED_PULSE_MS;
			}
			if (TIFR0 & _BV(TOV0)) { /* LED timer overflow. */
				TIFR0 = _BV(TOV0);
				/* Turn off TX LED(s) once the TX pulse period has elapsed */
				if (PulseMSRemaining.TxLEDPulse && !(--PulseMSRemaining.TxLEDPulse))
				  LEDs_TurnOffLEDs(LEDMASK_TX);
				if (PulseMSRemaining.RxLEDPulse && !(--PulseMSRemaining.RxLEDPulse))
				  LEDs_TurnOffLEDs(LEDMASK_RX);
			}
			Endpoint_SelectEndpoint(ENDPOINT_CONTROLEP);
			if (Endpoint_IsSETUPReceived())
			  USB_Device_ProcessControlRequest();

		} while (USB_DeviceState == DEVICE_STATE_Configured);
		/* Dont forget LEDs on if suddenly unconfigured. */
		LEDs_TurnOffLEDs(LEDMASK_TX);
		LEDs_TurnOffLEDs(LEDMASK_RX);
	}
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Hardware Initialization */
	Serial_Init(9600, false);
	LEDs_Init();
	USB_Init();

	/* Timer0 is the LED timeout timer... */
	TCCR0B = _BV(CS02);

	/* Timer1 is the USB flush timeout timer. */
	OCR1A = 8000; // 0.5ms at 16Mhz
	TCCR1A = 0;
	TCCR1B = _BV(WGM12) | _BV(CS10);

	/* Pull target /RESET line high */
	AVR_RESET_LINE_PORT |= AVR_RESET_LINE_MASK;
	AVR_RESET_LINE_DDR  |= AVR_RESET_LINE_MASK;
}

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	CDC_Device_ConfigureEndpoints(&VirtualSerial_CDC_Interface);
}

/** Event handler for the library USB Unhandled Control Request event. */
void EVENT_USB_Device_UnhandledControlRequest(void)
{
	CDC_Device_ProcessControlRequest(&VirtualSerial_CDC_Interface);
}

/** Event handler for the CDC Class driver Line Encoding Changed event.
 *
 *  \param[in] CDCInterfaceInfo  Pointer to the CDC class interface configuration structure being referenced
 */
void EVENT_CDC_Device_LineEncodingChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
{
	uint8_t ConfigMask = 0;

	switch (CDCInterfaceInfo->State.LineEncoding.ParityType)
	{
		case CDC_PARITY_Odd:
			ConfigMask = ((1 << UPM11) | (1 << UPM10));
			break;
		case CDC_PARITY_Even:
			ConfigMask = (1 << UPM11);
			break;
	}

	if (CDCInterfaceInfo->State.LineEncoding.CharFormat == CDC_LINEENCODING_TwoStopBits)
	  ConfigMask |= (1 << USBS1);

	switch (CDCInterfaceInfo->State.LineEncoding.DataBits)
	{
		case 6:
			ConfigMask |= (1 << UCSZ10);
			break;
		case 7:
			ConfigMask |= (1 << UCSZ11);
			break;
		case 8:
			ConfigMask |= ((1 << UCSZ11) | (1 << UCSZ10));
			break;
	}

	/* Must turn off USART before reconfiguring it, otherwise incorrect operation may occur */
	UCSR1B = 0;
	UCSR1A = 0;
	UCSR1C = 0;

	/* Flush data that was about to be sent. */
	USBtoUSART_rdp = 0;
	USBtoUSART_wrp = 0;

	/* Leave it off if BaudRate == 0. */
	if (!CDCInterfaceInfo->State.LineEncoding.BaudRateBPS) return;

	uint8_t sreg = _BV(U2X1);
	uint16_t brr;
	/* Special case 57600 baud for compatibility with the ATmega328 bootloader. */
	if (CDCInterfaceInfo->State.LineEncoding.BaudRateBPS == 57600) {
		brr = SERIAL_UBBRVAL(CDCInterfaceInfo->State.LineEncoding.BaudRateBPS);
		sreg = 0;
	} else {
		brr = SERIAL_2X_UBBRVAL(CDCInterfaceInfo->State.LineEncoding.BaudRateBPS);
		if ((brr&1)||(brr>4095)) { // No need U2X or cant have U2X.
			brr = brr>>1;
			sreg = 0;
		}
	}
	UBRR1 = brr;
	UCSR1C = ConfigMask;
	UCSR1A = sreg;
	UCSR1B = ((1 << RXCIE1) | (1 << TXEN1) | (1 << RXEN1));
}

ISR(USART1_RX_vect, ISR_NAKED)
{
	/* This ISR doesnt change SREG. Whoa. */
	asm volatile (
	"lds r3, %0\n\t" // UDR1
	"movw r4, r30\n\t"
	"in r30, %1\n\t" // USARTtoUSB_wrp
	"ldi r31, 0x01\n\t"
	"st Z+, r3\n\t"
	"out %1, r30\n\t"
	"movw r30, r4\n\t"
	"reti\n\t"
	:: "m" (UDR1), "I" (_SFR_IO_ADDR(USARTtoUSB_wrp))
	);
}

ISR(USART1_UDRE_vect, ISR_NAKED)
{
	/* Another SREG-less ISR. */
	asm volatile (
	"movw r4, r30\n\t"
	"in r30, %1\n\t" // USBtoUSART_rdp
	"ldi r31, 0x02\n\t"
	"ld r3, Z+\n\t"
	"sts %0, r3\n\t"
	"out %1, r30\n\t"
	"cbi %1, 7\n\t" // smart after-the-fact andi 0x7F without using SREG :P
	"movw r30, r4\n\t"
	"in r2, %1\n\t"
	"lds r3, %2\n\t" // USBtoUSART_wrp
	"cpse r2, r3\n\t"
	"reti\n\t"
	"ldi r30, 0x98\n\t" // Turn self off
	"sts %3, r30\n\t"
	"movw r30, r4\n\t"
	"reti\n\t"
	:: "m" (UDR1), "I" (_SFR_IO_ADDR(USBtoUSART_rdp)), "m" (USBtoUSART_wrp), "m" (UCSR1B)
	);
}

/** Event handler for the CDC Class driver Host-to-Device Line Encoding Changed event.
 *
 *  \param[in] CDCInterfaceInfo  Pointer to the CDC class interface configuration structure being referenced
 */
void EVENT_CDC_Device_ControLineStateChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
{
	bool CurrentDTRState = (CDCInterfaceInfo->State.ControlLineStates.HostToDevice & CDC_CONTROL_LINE_OUT_DTR);

	if (CurrentDTRState)
	  AVR_RESET_LINE_PORT &= ~AVR_RESET_LINE_MASK;
	else
	  AVR_RESET_LINE_PORT |= AVR_RESET_LINE_MASK;
}
