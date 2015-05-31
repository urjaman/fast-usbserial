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
 *  \brief Device mode driver for the library USB CDC Class driver.
 *
 *  Device mode driver for the library USB CDC Class driver.
 *
 *  \note This file should not be included directly. It is automatically included as needed by the class driver
 *        dispatch header located in LUFA/Drivers/USB/Class/CDC.h.
 */

/** \ingroup Group_USBClassCDC
 *  @defgroup Group_USBClassCDCDevice CDC Class Device Mode Driver
 *
 *  \section Sec_Dependencies Module Source Dependencies
 *  The following files must be built with any user project that uses this module:
 *    - LUFA/Drivers/USB/Class/Device/CDC.c <i>(Makefile source module name: LUFA_SRC_USBCLASS)</i>
 *
 *  \section Module Description
 *  Device Mode USB Class driver framework interface, for the CDC USB Class driver.
 *
 *  \note There are several major drawbacks to the CDC-ACM standard USB class, however
 *        it is very standardized and thus usually available as a built-in driver on
 *        most platforms, and so is a better choice than a proprietary serial class.
 *
 *        One major issue with CDC-ACM is that it requires two Interface descriptors,
 *        which will upset most hosts when part of a multi-function "Composite" USB
 *        device, as each interface will be loaded into a separate driver instance. To
 *        combat this, you should use the "Interface Association Descriptor" addendum to
 *        the USB standard which is available on most OSes when creating Composite devices.
 *
 *        Another major oversight is that there is no mechanism for the host to notify the
 *        device that there is a data sink on the host side ready to accept data. This
 *        means that the device may try to send data while the host isn't listening, causing
 *        lengthy blocking timeouts in the transmission routines. To combat this, it is
 *        recommended that the virtual serial line DTR (Data Terminal Ready) be used where
 *        possible to determine if a host application is ready for data.
 *
 *  @{
 */

#ifndef _CDC_CLASS_DEVICE_H_
#define _CDC_CLASS_DEVICE_H_
		#include "GlobalRegs.h"

	/* Includes: */
		#include "USB.h"
		#include "Common-CDC.h"
		#include "Descriptors.h"

		#include <stdio.h>
		#include <string.h>

	/* Enable C linkage for C++ Compilers: */
		#if defined(__cplusplus)
			extern "C" {
		#endif

	/* Public Interface - May be used in end-application: */
		/* Type Defines: */
			/** \brief CDC Class Device Mode Configuration and State Structure.
			 *
			 *  Class state structure. An instance of this structure should be made for each CDC interface
			 *  within the user application, and passed to each of the CDC class driver functions as the
			 *  CDCInterfaceInfo parameter. This stores each CDC interface's configuration and state information.
			 */
			typedef struct
			{
				struct
				{
					struct
					{
						uint8_t HostToDevice; /**< Control line states from the host to device, as a set of CDC_CONTROL_LINE_OUT_*
											   *   masks. This value is updated each time \ref CDC_Device_USBTask() is called.
											   */
					} ControlLineStates; /**< Current states of the virtual serial port's control lines between the device and host. */

					struct
					{
						uint32_t BaudRateBPS; /**< Baud rate of the virtual serial port, in bits per second. */
						uint8_t  CharFormat; /**< Character format of the virtual serial port, a value from the
											  *   \ref CDC_LineEncodingFormats_t enum.
											  */
						uint8_t  ParityType; /**< Parity setting of the virtual serial port, a value from the
											  *   \ref CDC_LineEncodingParity_t enum.
											  */
						uint8_t  DataBits; /**< Bits of data per character of the virtual serial port. */
					} LineEncoding;	/** Line encoding used in the virtual serial port, for the device's information. This is generally
					                 *  only used if the virtual serial port data is to be reconstructed on a physical UART.
					                 */
				} State; /**< State data for the USB class interface within the device. All elements in this section
				          *   are reset to their defaults when the interface is enumerated.
				          */
			} USB_ClassInfo_CDC_Device_t;

		/* Function Prototypes: */
			/** Configures the endpoints of a given CDC interface, ready for use. This should be linked to the library
			 *  \ref EVENT_USB_Device_ConfigurationChanged() event so that the endpoints are configured when the configuration containing
			 *  the given CDC interface is selected.
			 *
			 *  \param[in,out] CDCInterfaceInfo  Pointer to a structure containing a CDC Class configuration and state.
			 *
			 *  \return Boolean true if the endpoints were successfully configured, false otherwise.
			 */
			bool CDC_Device_ConfigureEndpoints(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo) ATTR_NON_NULL_PTR_ARG(1);

			/** Processes incoming control requests from the host, that are directed to the given CDC class interface. This should be
			 *  linked to the library \ref EVENT_USB_Device_UnhandledControlRequest() event.
			 *
			 *  \param[in,out] CDCInterfaceInfo  Pointer to a structure containing a CDC Class configuration and state.
			 */
			void CDC_Device_ProcessControlRequest(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo) ATTR_NON_NULL_PTR_ARG(1);

			/** General management task for a given CDC class interface, required for the correct operation of the interface. This should
			 *  be called frequently in the main program loop, before the master USB management task \ref USB_USBTask().
			 *
			 *  \param[in,out] CDCInterfaceInfo  Pointer to a structure containing a CDC Class configuration and state.
			 */
			void CDC_Device_USBTask(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo) ATTR_NON_NULL_PTR_ARG(1);

			/** CDC class driver event for a line encoding change on a CDC interface. This event fires each time the host requests a
			 *  line encoding change (containing the serial parity, baud and other configuration information) and may be hooked in the
			 *  user program by declaring a handler function with the same name and parameters listed here. The new line encoding
			 *  settings are available in the LineEncoding structure inside the CDC interface structure passed as a parameter.
			 *
			 *  \param[in,out] CDCInterfaceInfo  Pointer to a structure containing a CDC Class configuration and state.
			 */
			void EVENT_CDC_Device_LineEncodingChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo) ATTR_NON_NULL_PTR_ARG(1);

			/** CDC class driver event for a control line state change on a CDC interface. This event fires each time the host requests a
			 *  control line state change (containing the virtual serial control line states, such as DTR) and may be hooked in the
			 *  user program by declaring a handler function with the same name and parameters listed here. The new control line states
			 *  are available in the ControlLineStates.HostToDevice value inside the CDC interface structure passed as a parameter, set as
			 *  a mask of CDC_CONTROL_LINE_OUT_* masks.
			 *
			 *  \param[in,out] CDCInterfaceInfo  Pointer to a structure containing a CDC Class configuration and state.
			 */
			void EVENT_CDC_Device_ControLineStateChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo) ATTR_NON_NULL_PTR_ARG(1);

			/** CDC class driver event for a send break request sent to the device from the host. This is generally used to separate
			 *  data or to indicate a special condition to the receiving device.
			 *
			 *  \param[in,out] CDCInterfaceInfo  Pointer to a structure containing a CDC Class configuration and state.
			 *  \param[in]     Duration          Duration of the break that has been sent by the host, in milliseconds.
			 */


			/* This about writing data to endpoint. User will take care of the writing if needed. */
			uint8_t CDC_Device_SendByte_Prep(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo) ATTR_NON_NULL_PTR_ARG(1);


			/** Determines the number of bytes received by the CDC interface from the host, waiting to be read. This indicates the number
			 *  of bytes in the OUT endpoint bank only, and thus the number of calls to \ref CDC_Device_ReceiveByte() which are guaranteed to
			 *  succeed immediately. If multiple bytes are to be received, they should be buffered by the user application, as the endpoint
			 *  bank will not be released back to the USB controller until all bytes are read.
			 *
			 *  \pre This function must only be called when the Device state machine is in the DEVICE_STATE_Configured state or
			 *       the call will fail.
			 *
			 *  \param[in,out] CDCInterfaceInfo  Pointer to a structure containing a CDC Class configuration and state.
			 *
			 *  \return Total number of buffered bytes received from the host.
			 */
			uint8_t CDC_Device_BytesReceived(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo) ATTR_NON_NULL_PTR_ARG(1);


			/** Reads a byte of data from the host. If no data is waiting to be read of if a USB host is not connected, the function
			 *  returns a negative value. The \ref CDC_Device_BytesReceived() function may be queried in advance to determine how many
			 *  bytes are currently buffered in the CDC interface's data receive endpoint bank, and thus how many repeated calls to this
			 *  function which are guaranteed to succeed.
			 *
			 *  \pre This function must only be called when the Device state machine is in the DEVICE_STATE_Configured state or
			 *       the call will fail.
			 *
			 *  \param[in,out] CDCInterfaceInfo  Pointer to a structure containing a CDC Class configuration and state.
			 *
			 *  \return Next received byte from the host, or a negative value if no data received.
			 */
			int16_t CDC_Device_ReceiveByte(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo) ATTR_NON_NULL_PTR_ARG(1);


	/* Private Interface - For use in library only: */
	#if !defined(__DOXYGEN__)
		/* Function Prototypes: */
			#if defined(__INCLUDE_FROM_CDC_CLASS_DEVICE_C)

				void CDC_Device_Event_Stub(void);
				void EVENT_CDC_Device_LineEncodingChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
				                                          ATTR_WEAK ATTR_NON_NULL_PTR_ARG(1) ATTR_ALIAS(CDC_Device_Event_Stub);
				void EVENT_CDC_Device_ControLineStateChanged(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo)
				                                             ATTR_WEAK ATTR_NON_NULL_PTR_ARG(1) ATTR_ALIAS(CDC_Device_Event_Stub);
				void EVENT_CDC_Device_BreakSent(USB_ClassInfo_CDC_Device_t* const CDCInterfaceInfo,
				                                const uint8_t Duration) ATTR_WEAK ATTR_NON_NULL_PTR_ARG(1)
				                                ATTR_ALIAS(CDC_Device_Event_Stub);
			#endif

	#endif

	/* Disable C linkage for C++ Compilers: */
		#if defined(__cplusplus)
			}
		#endif

#endif

/** @} */
