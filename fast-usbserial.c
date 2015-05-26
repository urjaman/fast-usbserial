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
 *  Main source file for the Arduino-usbserial project. This file contains the main tasks of
 *  the project and is responsible for the initial application hardware configuration.
 */

#include "fast-usbserial.h"

/* The original ring buffer code is unserviceable IMO .*/

#define USB2USART_BUFLEN 64
static uint8_t USBtoUSART_wrp = 0;
static uint8_t USBtoUSART_rdp = 0;
static uint8_t USBtoUSART_buf[USB2USART_BUFLEN];


#define USART2USB_BUFLEN 256
#define USART2USB_NEAR_FULL CDC_IN_EPSIZE

/* NOTE: Reserved 0x100 bytes from start of RAM for this via linker magic. */
#define USART2USB_BUFADDR 0x100

static uint8_t USARTtoUSB_wrp = 0;
static uint8_t USARTtoUSB_rdp = 0;
//static uint8_t USARTtoUSB_buf[USART2USB_BUFLEN];
static volatile uint8_t USARTtoUSB_cnt = 0;

/** Pulse generation counters to keep track of the number of milliseconds remaining for each pulse type */
struct
{
	uint8_t TxLEDPulse; /**< Milliseconds remaining for data Tx LED pulse */
	uint8_t RxLEDPulse; /**< Milliseconds remaining for data Rx LED pulse */
} PulseMSRemaining;

/** LUFA CDC Class driver interface configuration and state information. This structure is
 *  passed to all CDC Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_CDC_Device_t VirtualSerial_CDC_Interface =
	{
		.Config =
			{
				.ControlInterfaceNumber         = 0,

				.DataINEndpointNumber           = CDC_TX_EPNUM,
				.DataINEndpointSize             = CDC_IN_EPSIZE,
				.DataINEndpointDoubleBank       = true,

				.DataOUTEndpointNumber          = CDC_RX_EPNUM,
				.DataOUTEndpointSize            = CDC_OUT_EPSIZE,
				.DataOUTEndpointDoubleBank      = false,

				.NotificationEndpointNumber     = CDC_NOTIFICATION_EPNUM,
				.NotificationEndpointSize       = CDC_NOTIFICATION_EPSIZE,
				.NotificationEndpointDoubleBank = false,
			},
	};

/** Main program entry point. This routine contains the overall program flow, including initial
 *  setup of all components and the main program loop.
 */
int main(void)
{
	uint8_t USBtoUSART_cnt = 0;
	SetupHardware();

	sei();

	for (;;)
	{
		uint8_t timer_ovrflw = TIFR0 & _BV(TOV0);
		if (timer_ovrflw) TIFR0 = _BV(TOV0);
		/* I'd like to get rid of these counters... */
		uint8_t cnt = USARTtoUSB_cnt;
		/* Check if the UART receive buffer flush timer has expired or the buffer is nearly full */
		if ( ((cnt >= USART2USB_NEAR_FULL) || (timer_ovrflw && cnt)) &&
			(CDC_Device_SendByte_Prep(&VirtualSerial_CDC_Interface) == 0) ) {

			uint8_t txcnt = CDC_IN_EPSIZE - Endpoint_BytesInEndpoint();
			if (txcnt > cnt) txcnt = cnt;
			if (cnt > txcnt) cnt = txcnt;
			uint16_t tmp = USART2USB_BUFADDR;
			tmp |= USARTtoUSB_rdp;
			do {
				uint8_t d = *((uint8_t*)tmp);
                                Endpoint_Write_Byte(d);
                                tmp = (USART2USB_BUFADDR & 0xFF00) | ((tmp+1)&0xFF);
			} while (--txcnt);
	                Endpoint_ClearIN();
			USARTtoUSB_rdp = tmp & 0xFF;
			cli();
			/* This will be logically OK, even if more bytes arrived during TX,
			 * because we sent cnt bytes, so removed that much from the buffer. */
			USARTtoUSB_cnt -= cnt;
			sei();
			LEDs_TurnOnLEDs(LEDMASK_TX);
			PulseMSRemaining.TxLEDPulse = TX_RX_LED_PULSE_MS;

#if 1
			/* This prevents TX from forgetting to turn off RX led. */
			/* The RX led period will be saddened though */
			if (PulseMSRemaining.RxLEDPulse && !(--PulseMSRemaining.RxLEDPulse))
			  LEDs_TurnOffLEDs(LEDMASK_RX);
#endif
		} else {
			/* My guess is that this branch will be run regularly, even during full output, because
			   USB hosts are poor at servicing devices... thus moved the control IF service here too. */
			/* Only try to read in bytes from the CDC interface if the transmit buffer is not full */
			if (USBtoUSART_cnt < (USB2USART_BUFLEN-1)) {
				int16_t ReceivedByte = CDC_Device_ReceiveByte(&VirtualSerial_CDC_Interface);

				/* Read byte from the USB OUT endpoint into the USART transmit buffer */
				if (!(ReceivedByte < 0)) {
				  uint8_t wrp = USBtoUSART_wrp;
				  USBtoUSART_buf[wrp] = ReceivedByte;
				  wrp++;
				  wrp &= (USB2USART_BUFLEN-1);
				  USBtoUSART_wrp = wrp;
				  USBtoUSART_cnt++;
				}
			}
			/* Make This less important than the above thing. */
			if (USBtoUSART_cnt) {
				if (UCSR1A & (1 << UDRE1)) {
					uint8_t rdp = USBtoUSART_rdp;
					UDR1 = USBtoUSART_buf[rdp];
					rdp++;
					rdp &= (USB2USART_BUFLEN-1);
					USBtoUSART_rdp = rdp;
					USBtoUSART_cnt--;
				  	LEDs_TurnOnLEDs(LEDMASK_RX);
					PulseMSRemaining.RxLEDPulse = TX_RX_LED_PULSE_MS;
				}
			}
			if (timer_ovrflw) {
				/* Turn off TX LED(s) once the TX pulse period has elapsed */
				if (PulseMSRemaining.TxLEDPulse && !(--PulseMSRemaining.TxLEDPulse))
				  LEDs_TurnOffLEDs(LEDMASK_TX);
				if (PulseMSRemaining.RxLEDPulse && !(--PulseMSRemaining.RxLEDPulse))
				  LEDs_TurnOffLEDs(LEDMASK_RX);
			}
			USB_USBTask();
		}
		/* That CDC_Device_USBTask would call CDC_Device_Flush even if the bank was already full... */
		/* Which would cause waiting. */
		//CDC_Device_USBTask(&VirtualSerial_CDC_Interface);
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

	/* Start the flush timer so that overflows occur rapidly to push received bytes to the USB interface */
	TCCR0B = (1 << CS01) | (1 << CS00);

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

	/* Special case 57600 baud for compatibility with the ATmega328 bootloader. */
	UBRR1  = (CDCInterfaceInfo->State.LineEncoding.BaudRateBPS == 57600)
			 ? SERIAL_UBBRVAL(CDCInterfaceInfo->State.LineEncoding.BaudRateBPS)
			 : SERIAL_2X_UBBRVAL(CDCInterfaceInfo->State.LineEncoding.BaudRateBPS);

	UCSR1C = ConfigMask;
	UCSR1A = (CDCInterfaceInfo->State.LineEncoding.BaudRateBPS == 57600) ? 0 : (1 << U2X1);
	UCSR1B = ((1 << RXCIE1) | (1 << TXEN1) | (1 << RXEN1));
}

/** ISR to manage the reception of data from the serial port, placing received bytes into a circular buffer
 *  for later transmission to the host.
 */
ISR(USART1_RX_vect, ISR_BLOCK)
{
	uint8_t d = UDR1;
	if (USB_DeviceState != DEVICE_STATE_Configured) return;
	uint16_t tmp = USART2USB_BUFADDR;
	tmp |= USARTtoUSB_wrp;
	*((uint8_t*)tmp) = d;
	tmp++;
	USARTtoUSB_wrp = tmp & 0xFF;
	USARTtoUSB_cnt++;
	TCNT0 = 0;
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
