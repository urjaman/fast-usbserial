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
#include "ConfigDescriptor.h"

void USB_GetNextDescriptorOfType(uint16_t* const BytesRem,
                                 void** const CurrConfigLoc,
                                 const uint8_t Type)
{
	while (*BytesRem)
	{
		USB_GetNextDescriptor(BytesRem, CurrConfigLoc);

		if (DESCRIPTOR_TYPE(*CurrConfigLoc) == Type)
		  return;
	}
}

void USB_GetNextDescriptorOfTypeBefore(uint16_t* const BytesRem,
                                       void** const CurrConfigLoc,
                                       const uint8_t Type,
                                       const uint8_t BeforeType)
{
	while (*BytesRem)
	{
		USB_GetNextDescriptor(BytesRem, CurrConfigLoc);

		if (DESCRIPTOR_TYPE(*CurrConfigLoc) == Type)
		{
			return;
		}
		else if (DESCRIPTOR_TYPE(*CurrConfigLoc) == BeforeType)
		{
			*BytesRem = 0;
			return;
		}
	}
}

void USB_GetNextDescriptorOfTypeAfter(uint16_t* const BytesRem,
                                      void** const CurrConfigLoc,
                                      const uint8_t Type,
                                      const uint8_t AfterType)
{
	USB_GetNextDescriptorOfType(BytesRem, CurrConfigLoc, AfterType);

	if (*BytesRem)
	  USB_GetNextDescriptorOfType(BytesRem, CurrConfigLoc, Type);
}

uint8_t USB_GetNextDescriptorComp(uint16_t* const BytesRem, void** const CurrConfigLoc, ConfigComparatorPtr_t const ComparatorRoutine)
{
	uint8_t ErrorCode;

	while (*BytesRem)
	{
		uint8_t* PrevDescLoc  = *CurrConfigLoc;
		uint16_t PrevBytesRem = *BytesRem;

		USB_GetNextDescriptor(BytesRem, CurrConfigLoc);

		if ((ErrorCode = ComparatorRoutine(*CurrConfigLoc)) != DESCRIPTOR_SEARCH_NotFound)
		{
			if (ErrorCode == DESCRIPTOR_SEARCH_Fail)
			{
				*CurrConfigLoc = PrevDescLoc;
				*BytesRem      = PrevBytesRem;
			}

			return ErrorCode;
		}
	}

	return DESCRIPTOR_SEARCH_COMP_EndOfDescriptor;
}
