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
#include "USBMode.h"
#if defined(USB_CAN_BE_DEVICE)

#include "Descriptors.h"
#define  __INCLUDE_FROM_CDC_CLASS_DEVICE_C
#define  __INCLUDE_FROM_CDC_DRIVER
#include "SimpleCDC.h"

void CDC_Device_Event_Stub(void)
{

}

void CDC_Device_ProcessControlRequest(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
{
	if (!(Endpoint_IsSETUPReceived()))
	  return;

	if (USB_ControlRequest.wIndex != CDC_CONTROL_EPNUM)
	  return;

	switch (USB_ControlRequest.bRequest)
	{
		case REQ_GetLineEncoding:
			if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();
				Endpoint_Write_Control_Stream_LE(&CDCInterfaceInfo->State.LineEncoding, sizeof(CDCInterfaceInfo->State.LineEncoding));
				Endpoint_ClearOUT();
			}

			break;
		case REQ_SetLineEncoding:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();
				Endpoint_Read_Control_Stream_LE(&CDCInterfaceInfo->State.LineEncoding, sizeof(CDCInterfaceInfo->State.LineEncoding));
				EVENT_CDC_Device_LineEncodingChanged(CDCInterfaceInfo);
				Endpoint_ClearIN();
			}

			break;
		case REQ_SetControlLineState:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();

				CDCInterfaceInfo->State.ControlLineStates.HostToDevice = USB_ControlRequest.wValue;
				EVENT_CDC_Device_ControLineStateChanged(CDCInterfaceInfo);

				Endpoint_ClearStatusStage();
			}

			break;
		case REQ_SendBreak:
			if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
			{
				Endpoint_ClearSETUP();

				Endpoint_ClearStatusStage();
			}

			break;
	}
}


bool CDC_Device_ConfigureEndpoints(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
{
	memset(&CDCInterfaceInfo->State, 0x00, sizeof(CDCInterfaceInfo->State));

	if (!(Endpoint_ConfigureEndpoint(CDC_NOTIFICATION_EPNUM, EP_TYPE_INTERRUPT,
	                                 ENDPOINT_DIR_IN, CDC_NOTIFICATION_EPSIZE,
	                                 CDC_NOTIFICATION_DBLBANK ? ENDPOINT_BANK_DOUBLE : ENDPOINT_BANK_SINGLE)))
	{
		return false;
	}

	if (!(Endpoint_ConfigureEndpoint(CDC_TX_EPNUM, EP_TYPE_BULK,
							         ENDPOINT_DIR_IN, CDC_IN_EPSIZE,
							         CDC_IN_DBLBANK ? ENDPOINT_BANK_DOUBLE : ENDPOINT_BANK_SINGLE)))
	{
		return false;
	}

	if (!(Endpoint_ConfigureEndpoint(CDC_RX_EPNUM, EP_TYPE_BULK,
	                                 ENDPOINT_DIR_OUT, CDC_OUT_EPSIZE,
	                                 CDC_OUT_DBLBANK ? ENDPOINT_BANK_DOUBLE : ENDPOINT_BANK_SINGLE)))
	{
		return false;
	}

	return true;
}


/* Think about writing data to endpoint. User will take care of the writing if needed. */
uint8_t CDC_Device_SendByte_Prep(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
{
	if (!(CDCInterfaceInfo->State.LineEncoding.BaudRateBPS))
	  return ENDPOINT_RWSTREAM_DeviceDisconnected;

	Endpoint_SelectEndpoint(CDC_TX_EPNUM);

	if (!(Endpoint_IsReadWriteAllowed()))
	{
		Endpoint_ClearIN();
		return ENDPOINT_READYWAIT_Timeout;
	}
	return ENDPOINT_READYWAIT_NoError;
}



uint8_t CDC_Device_BytesReceived(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
{
	if (!(CDCInterfaceInfo->State.LineEncoding.BaudRateBPS))
	  return 0;

	Endpoint_SelectEndpoint(CDC_RX_EPNUM);

	if (Endpoint_IsOUTReceived())
	{
		if (!(Endpoint_BytesInEndpoint()))
		{
			Endpoint_ClearOUT();
			return 0;
		}
		else
		{
			return Endpoint_BytesInEndpoint();
		}
	}
	else
	{
		return 0;
	}
}




int16_t CDC_Device_ReceiveByte(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
{
	if (!(CDCInterfaceInfo->State.LineEncoding.BaudRateBPS))
	  return -1;

	int16_t ReceivedByte = -1;

	Endpoint_SelectEndpoint(CDC_RX_EPNUM);

	if (Endpoint_IsOUTReceived())
	{
		if (Endpoint_BytesInEndpoint())
		  ReceivedByte = Endpoint_Read_Byte();

		if (!(Endpoint_BytesInEndpoint()))
		  Endpoint_ClearOUT();
	}

	return ReceivedByte;
}

#endif
