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

#define  __INCLUDE_FROM_USB_DRIVER
#include "USBInterrupt.h"

void USB_INT_DisableAllInterrupts(void)
{
	#if defined(USB_SERIES_6_AVR) || defined(USB_SERIES_7_AVR)
	USBCON &= ~((1 << VBUSTE) | (1 << IDTE));
	#elif defined(USB_SERIES_4_AVR)
	USBCON &= ~(1 << VBUSTE);
	#endif

	#if defined(USB_CAN_BE_DEVICE)
	UDIEN   = 0;
	#endif
}

void USB_INT_ClearAllInterrupts(void)
{
	#if defined(USB_SERIES_4_AVR) || defined(USB_SERIES_6_AVR) || defined(USB_SERIES_7_AVR)
	USBINT  = 0;
	#endif

	#if defined(USB_CAN_BE_DEVICE)
	UDINT   = 0;
	#endif
}

ISR(USB_GEN_vect, ISR_BLOCK __attribute__((flatten)) )
{
	#if defined(USB_CAN_BE_DEVICE)
	#if defined(USB_SERIES_4_AVR) || defined(USB_SERIES_6_AVR) || defined(USB_SERIES_7_AVR)
	if (USB_INT_HasOccurred(USB_INT_VBUS) && USB_INT_IsEnabled(USB_INT_VBUS))
	{
		USB_INT_Clear(USB_INT_VBUS);

		if (USB_VBUS_GetStatus())
		{
			USB_DeviceState = DEVICE_STATE_Powered;
			EVENT_USB_Device_Connect();
		}
		else
		{
			USB_DeviceState = DEVICE_STATE_Unattached;
			EVENT_USB_Device_Disconnect();
		}
	}
	#endif
	uint8_t enabled = UDIEN;
	uint8_t occurred = UDINT;
	occurred &= enabled;
#ifdef USB_SERIES_2_AVR
	UDIEN = 0;
	sei();
#endif

	if (occurred & _BV(SUSPI)) {
		UDINT = ~_BV(SUSPI);
		enabled &= ~ _BV(SUSPE);
		enabled |= _BV(WAKEUPE);

		USB_CLK_Freeze();

		if (!(USB_Options & USB_OPT_MANUAL_PLL))
		  USB_PLL_Off();

		#if defined(USB_SERIES_2_AVR) && !defined(NO_LIMITED_CONTROLLER_CONNECT)
		USB_DeviceState = DEVICE_STATE_Unattached;
		EVENT_USB_Device_Disconnect();
		#else
		USB_DeviceState = DEVICE_STATE_Suspended;
		EVENT_USB_Device_Suspend();
		#endif
	}

	if (occurred & _BV(WAKEUPI)) {
		if (!(USB_Options & USB_OPT_MANUAL_PLL))
		{
			USB_PLL_On();
			while (!(USB_PLL_IsReady()));
		}

		USB_CLK_Unfreeze();

		UDINT = ~_BV(WAKEUPI);

		enabled &= ~_BV(WAKEUPE);
		enabled |= _BV(SUSPE);

		#if defined(USB_SERIES_2_AVR) && !defined(NO_LIMITED_CONTROLLER_CONNECT)
		USB_DeviceState = (USB_ConfigurationNumber) ? DEVICE_STATE_Configured : DEVICE_STATE_Powered;
		EVENT_USB_Device_Connect();
		#else
		USB_DeviceState = (USB_ConfigurationNumber) ? DEVICE_STATE_Configured : DEVICE_STATE_Addressed;
		EVENT_USB_Device_WakeUp();
		#endif
	}

	if (occurred & _BV(EORSTI)) {
		UDINT = ~( _BV(EORSTI) | _BV(SUSPI) );

		USB_DeviceState         = DEVICE_STATE_Default;
		USB_ConfigurationNumber = 0;

		enabled &= ~_BV(SUSPE);
		enabled |= ~_BV(WAKEUPE);

		Endpoint_ClearEndpoints();

		Endpoint_ConfigureEndpoint(ENDPOINT_CONTROLEP, EP_TYPE_CONTROL,
		                           ENDPOINT_DIR_OUT, USB_ControlEndpointSize,
		                           ENDPOINT_BANK_SINGLE);

		#if defined(INTERRUPT_CONTROL_ENDPOINT)
		USB_INT_Enable(USB_INT_RXSTPI);
		#endif

		EVENT_USB_Device_Reset();
	}
#ifdef USB_SERIES_2_AVR
	cli();
#endif
	UDIEN = enabled;

	#endif

}

#if defined(INTERRUPT_CONTROL_ENDPOINT) && defined(USB_CAN_BE_DEVICE)
ISR(USB_COM_vect, ISR_BLOCK )
{
	uint8_t PrevSelectedEndpoint = Endpoint_GetCurrentEndpoint(); 

	Endpoint_SelectEndpoint(ENDPOINT_CONTROLEP);
	USB_INT_Disable(USB_INT_RXSTPI);

	NONATOMIC_BLOCK(NONATOMIC_FORCEOFF)
	{
		USB_Device_ProcessControlRequest();
	}

	Endpoint_SelectEndpoint(ENDPOINT_CONTROLEP);
	USB_INT_Enable(USB_INT_RXSTPI);
	Endpoint_SelectEndpoint(PrevSelectedEndpoint);
}
#endif
