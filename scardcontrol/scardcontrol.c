/*
    scardcontrol.c: sample code to use/test SCardControl() API
    Copyright (C) 2004-2014   Ludovic Rousseau

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

	You should have received a copy of the GNU General Public License along
	with this program; if not, write to the Free Software Foundation, Inc., 51
	Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

/*
 * $Id: scardcontrol.c 6818 2014-01-07 10:16:28Z rousseau $
 */

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/SmartCardReader.h>

#include "../config.h"

#include "../reader.h"

#include "PCSCv2part10.h"

#define VERIFY_PIN
#define MODIFY_PIN

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define MAX_BUFFER_SIZE 256
#define ntohl(a) (a)

#define IOCTL_SMARTCARD_VENDOR_IFD_EXCHANGE     SCARD_CTL_CODE(1)

/* PCSC error message pretty print */
#define PCSC_ERROR_EXIT(rv, text) \
if (rv != EFI_SUCCESS) \
{ \
	Print(text ": (0x%lX)\n", rv); \
	goto end; \
} \
else \
	Print(text ": OK\n\n");

#define PCSC_ERROR_CONT(rv, text) \
if (rv != EFI_SUCCESS) \
	Print(text ": (0x%lX)\n", rv); \
else \
	Print(text ": OK\n\n");

static void parse_properties(unsigned char *bRecvBuffer, int length)
{
	unsigned char *p;
	int i;

	p = bRecvBuffer;
	while (p-bRecvBuffer < length)
	{
		int tag, len, value;

		tag = *p++;
		len = *p++;

		switch(len)
		{
			case 1:
				value = *p;
				break;
			case 2:
				value = *p + (*(p+1)<<8);
				break;
			case 4:
				value = *p + (*(p+1)<<8) + (*(p+2)<<16) + (*(p+3)<<24);
				break;
			default:
				value = -1;
		}

		switch(tag)
		{
			case PCSCv2_PART10_PROPERTY_wLcdLayout:
				Print(L" wLcdLayout: %04X\n", value);
				break;
			case PCSCv2_PART10_PROPERTY_bEntryValidationCondition:
				Print(L" bEntryValidationCondition: %02X\n", value);
				break;
			case PCSCv2_PART10_PROPERTY_bTimeOut2:
				Print(L" bTimeOut2: %02X\n", value);
				break;
			case PCSCv2_PART10_PROPERTY_wLcdMaxCharacters:
				Print(L" wLcdMaxCharacters: %04X\n", value);
				break;
			case PCSCv2_PART10_PROPERTY_wLcdMaxLines:
				Print(L" wLcdMaxLines: %04X\n", value);
				break;
			case PCSCv2_PART10_PROPERTY_bMinPINSize:
				Print(L" bMinPINSize: %02X\n", value);
				break;
			case PCSCv2_PART10_PROPERTY_bMaxPINSize:
				Print(L" bMaxPINSize: %02X\n", value);
				break;
			case PCSCv2_PART10_PROPERTY_sFirmwareID:
				Print(L" sFirmwareID: ");
				for (i=0; i<len; i++)
					Print(L"%c", p[i]);
				Print(L"\n");
				break;
			case PCSCv2_PART10_PROPERTY_bPPDUSupport:
				Print(L" bPPDUSupport: %02X\n", value);
				if (value & 1)
					Print(L"  PPDU is supported over SCardControl using FEATURE_CCID_ESC_COMMAND\n");
				if (value & 2)
					Print(L"  PPDU is supported over SCardTransmit\n");
				break;
			case PCSCv2_PART10_PROPERTY_dwMaxAPDUDataSize:
				Print(L" dwMaxAPDUDataSize: %d\n", value);
				break;
			case PCSCv2_PART10_PROPERTY_wIdVendor:
				Print(L" wIdVendor; %04X\n", value);
				break;
			case PCSCv2_PART10_PROPERTY_wIdProduct:
				Print(L" wIdProduct: %04X\n", value);
				break;
			default:
				Print(L" Unknown tag: 0x%02X (length = %d)\n", tag, len);
		}

		p += len;
	}
} /* parse_properties */


static const char *pinpad_return_codes(unsigned char bRecvBuffer[])
{
	const char * ret = "UNKNOWN";

	if ((0x90 == bRecvBuffer[0]) && (0x00 == bRecvBuffer[1]))
		ret = "Success";

	if (0x64 == bRecvBuffer[0])
	{
		switch (bRecvBuffer[1])
		{
			case 0x00:
				ret = "Timeout";
				break;

			case 0x01:
				ret = "Cancelled by user";
				break;

			case 0x02:
				ret = "PIN mismatch";
				break;

			case 0x03:
				ret = "Too short or too long PIN";
				break;
		}
	}

	return ret;
}

int CheckReader(EFI_SMART_CARD_READER_PROTOCOL *SmartCardReader)
{
	EFI_STATUS rv;
	unsigned int i;
	unsigned char bSendBuffer[MAX_BUFFER_SIZE];
	unsigned char bRecvBuffer[MAX_BUFFER_SIZE];
	int send_length;
	UINTN length;
	int verify_ioctl = 0;
	int modify_ioctl = 0;
	int pin_properties_ioctl = 0;
	int mct_readerdirect_ioctl = 0;
	int properties_in_tlv_ioctl = 0;
	int ccid_esc_command = 0;
	PCSC_TLV_STRUCTURE *pcsc_tlv;
#if defined(VERIFY_PIN) | defined(MODIFY_PIN)
	int offset;
#endif
#ifdef VERIFY_PIN
	PIN_VERIFY_STRUCTURE *pin_verify;
#endif
#ifdef MODIFY_PIN
	PIN_MODIFY_STRUCTURE *pin_modify;
#endif
	int PIN_min_size = 4;
	int PIN_max_size = 8;
	UINT32 ActiveProtocol;

	/* table for bEntryValidationCondition
	 * 0x01: Max size reached
	 * 0x02: Validation key pressed
	 * 0x04: Timeout occured
	 */
	int bEntryValidationCondition = 7;

	/* does the reader support PIN verification? */
	length = sizeof bRecvBuffer;
	rv = SmartCardReader->SCardControl(SmartCardReader,
			CM_IOCTL_GET_FEATURE_REQUEST, NULL, 0, bRecvBuffer,
			&length);
	PCSC_ERROR_EXIT(rv, L"SCardControl")

	Print(L" TLV (%ld): ", length);
	for (i=0; i<length; i++)
		Print(L"%02X ", bRecvBuffer[i]);
	Print(L"\n");

	PCSC_ERROR_CONT(rv, L"SCardControl(CM_IOCTL_GET_FEATURE_REQUEST)")

	if (length % sizeof(PCSC_TLV_STRUCTURE))
	{
		Print(L"Inconsistent result! Bad TLV values!\n");
		goto end;
	}

	/* get the number of elements instead of the complete size */
	length /= sizeof(PCSC_TLV_STRUCTURE);

	pcsc_tlv = (PCSC_TLV_STRUCTURE *)bRecvBuffer;
	for (i = 0; i < length; i++)
	{
		switch (pcsc_tlv[i].tag)
		{
			case FEATURE_VERIFY_PIN_DIRECT:
				Print(L"Reader supports FEATURE_VERIFY_PIN_DIRECT\n");
				verify_ioctl = ntohl(pcsc_tlv[i].value);
				break;
			case FEATURE_MODIFY_PIN_DIRECT:
				Print(L"Reader supports FEATURE_MODIFY_PIN_DIRECT\n");
				modify_ioctl = ntohl(pcsc_tlv[i].value);
				break;
			case FEATURE_IFD_PIN_PROPERTIES:
				Print(L"Reader supports FEATURE_IFD_PIN_PROPERTIES\n");
				pin_properties_ioctl = ntohl(pcsc_tlv[i].value);
				break;
			case FEATURE_MCT_READER_DIRECT:
				Print(L"Reader supports FEATURE_MCT_READER_DIRECT\n");
				mct_readerdirect_ioctl = ntohl(pcsc_tlv[i].value);
				break;
			case FEATURE_GET_TLV_PROPERTIES:
				Print(L"Reader supports FEATURE_GET_TLV_PROPERTIES\n");
				properties_in_tlv_ioctl = ntohl(pcsc_tlv[i].value);
				break;
			case FEATURE_CCID_ESC_COMMAND:
				Print(L"Reader supports FEATURE_CCID_ESC_COMMAND\n");
				ccid_esc_command = ntohl(pcsc_tlv[i].value);
				(void)ccid_esc_command;
				break;
			default:
				Print(L"Can't parse tag", pcsc_tlv[i].tag);
		}
	}
	Print(L"\n");

	if (properties_in_tlv_ioctl)
	{
		int value;
		int ret;

		length = sizeof bRecvBuffer;
		rv = SmartCardReader->SCardControl(SmartCardReader, properties_in_tlv_ioctl, NULL, 0,
			bRecvBuffer, &length);
		PCSC_ERROR_CONT(rv, L"SCardControl(GET_TLV_PROPERTIES)")

		Print(L"GET_TLV_PROPERTIES (%ld): ", length);
		for (i=0; i<length; i++)
			Print(L"%02X ", bRecvBuffer[i]);
		Print(L"\n");

		Print(L"\nDisplay all the properties:\n");
		parse_properties(bRecvBuffer, length);

		Print(L"\nFind a specific property:\n");
		ret = PCSCv2Part10_find_TLV_property_by_tag_from_buffer(bRecvBuffer, length, PCSCv2_PART10_PROPERTY_wIdVendor, &value);
		if (ret)
			Print(L" wIdVendor: %d\n", ret);
		else
			Print(L" wIdVendor: %04X\n", value);

		ret = PCSCv2Part10_find_TLV_property_by_tag_from_protocol(SmartCardReader, PCSCv2_PART10_PROPERTY_wIdProduct, &value);
		if (ret)
			Print(L" wIdProduct %d\n", ret);
		else
			Print(L" wIdProduct: %04X\n", value);

		ret = PCSCv2Part10_find_TLV_property_by_tag_from_protocol(SmartCardReader, PCSCv2_PART10_PROPERTY_bMinPINSize, &value);
		if (0 == ret)
		{
			PIN_min_size = value;
			Print(L" PIN min size defined %d\n", PIN_min_size);
		}


		ret = PCSCv2Part10_find_TLV_property_by_tag_from_protocol(SmartCardReader, PCSCv2_PART10_PROPERTY_bMaxPINSize, &value);
		if (0 == ret)
		{
			PIN_max_size = value;
			Print(L" PIN max size defined %d\n", PIN_max_size);
		}

		ret = PCSCv2Part10_find_TLV_property_by_tag_from_protocol(SmartCardReader, PCSCv2_PART10_PROPERTY_bEntryValidationCondition, &value);
		if (0 == ret)
		{
			bEntryValidationCondition = value;
			Print(L" Entry Validation Condition defined %d\n",
				bEntryValidationCondition);
		}

		Print(L"\n");
	}

	if (mct_readerdirect_ioctl)
	{
		unsigned char secoder_info[] = { 0x20, 0x70, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 };

		length = sizeof bRecvBuffer;
		rv = SmartCardReader->SCardControl(SmartCardReader,
			mct_readerdirect_ioctl, secoder_info, sizeof(secoder_info),
			bRecvBuffer, &length);
		PCSC_ERROR_CONT(rv, L"SCardControl(MCT_READER_DIRECT)")

		Print(L"MCT_READER_DIRECT (%ld): ", length);
		for (i=0; i<length; i++)
			Print(L"%02X ", bRecvBuffer[i]);
		Print(L"\n");
	}

	if (pin_properties_ioctl)
	{
		PIN_PROPERTIES_STRUCTURE *pin_properties;

		length = sizeof bRecvBuffer;
		rv = SmartCardReader->SCardControl(SmartCardReader,
			pin_properties_ioctl, NULL, 0, bRecvBuffer, &length);
		PCSC_ERROR_CONT(rv, L"SCardControl(pin_properties_ioctl)")

		Print(L"PIN PROPERTIES (%ld): ", length);
		for (i=0; i<length; i++)
			Print(L"%02X ", bRecvBuffer[i]);
		Print(L"\n");

		pin_properties = (PIN_PROPERTIES_STRUCTURE *)bRecvBuffer;
		Print(L" wLcdLayout %04X\n", pin_properties -> wLcdLayout);
		Print(L" bEntryValidationCondition %d\n", pin_properties ->	bEntryValidationCondition);
		Print(L" bTimeOut2 %d\n", pin_properties -> bTimeOut2);

		Print(L"\n");
	}

	if (0 == verify_ioctl)
	{
		Print(L"Reader does not support PIN verification\n");
		goto end;
	}

	/* SCardConnect */
	rv = SmartCardReader->SCardConnect(SmartCardReader,
		SCARD_AM_CARD,
		SCARD_CA_COLDRESET,
		SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
		&ActiveProtocol);
	PCSC_ERROR_EXIT(rv, L"SCardConnect")

	/* APDU select applet */
	Print(L"Select applet: ");
	send_length = 11;
	memcpy(bSendBuffer, "\x00\xA4\x04\x00\x06\xA0\x00\x00\x00\x18\xFF",
		send_length);
	for (i=0; i<send_length; i++)
		Print(L" %02X", bSendBuffer[i]);
	Print(L"\n");
	length = sizeof(bRecvBuffer);
	rv = SmartCardReader->SCardTransmit(SmartCardReader,
		bSendBuffer, send_length, bRecvBuffer, &length);
	Print(L" card response:");
	for (i=0; i<length; i++)
		Print(L" %02X", bRecvBuffer[i]);
	Print(L"\n");
	PCSC_ERROR_EXIT(rv, L"SCardTransmit")
	if ((bRecvBuffer[0] != 0x90) || (bRecvBuffer[1] != 0x00))
	{
		Print(L"Error: test applet not found!\n");
		goto end;
	}

#ifdef VERIFY_PIN
	/* verify PIN */
	Print(L" Secure verify PIN\n");
	pin_verify = (PIN_VERIFY_STRUCTURE *)bSendBuffer;

	/* PC/SC v2.02.05 Part 10 PIN verification data structure */
	pin_verify -> bTimerOut = 0x00;
	pin_verify -> bTimerOut2 = 0x00;
	pin_verify -> bmFormatString = 0x82;
	pin_verify -> bmPINBlockString = 0x04;
	pin_verify -> bmPINLengthFormat = 0x00;
	pin_verify -> wPINMaxExtraDigit = (PIN_min_size << 8) + PIN_max_size;
	pin_verify -> bEntryValidationCondition = bEntryValidationCondition;
	pin_verify -> bNumberMessage = 0x01;
	pin_verify -> wLangId = 0x0904;
	pin_verify -> bMsgIndex = 0x00;
	pin_verify -> bTeoPrologue[0] = 0x00;
	pin_verify -> bTeoPrologue[1] = 0x00;
	pin_verify -> bTeoPrologue[2] = 0x00;
	/* pin_verify -> ulDataLength = 0x00; we don't know the size yet */

	/* APDU: 00 20 00 00 08 30 30 30 30 00 00 00 00 */
	offset = 0;
	pin_verify -> abData[offset++] = 0x00;	/* CLA */
	pin_verify -> abData[offset++] = 0x20;	/* INS: VERIFY */
	pin_verify -> abData[offset++] = 0x00;	/* P1 */
	pin_verify -> abData[offset++] = 0x00;	/* P2 */
	pin_verify -> abData[offset++] = 0x08;	/* Lc: 8 data bytes */
	pin_verify -> abData[offset++] = 0x30;	/* '0' */
	pin_verify -> abData[offset++] = 0x30;	/* '0' */
	pin_verify -> abData[offset++] = 0x30;	/* '0' */
	pin_verify -> abData[offset++] = 0x30;	/* '0' */
	pin_verify -> abData[offset++] = 0x00;	/* '\0' */
	pin_verify -> abData[offset++] = 0x00;	/* '\0' */
	pin_verify -> abData[offset++] = 0x00;	/* '\0' */
	pin_verify -> abData[offset++] = 0x00;	/* '\0' */
	pin_verify -> ulDataLength = offset;	/* APDU size */

	send_length = sizeof(PIN_VERIFY_STRUCTURE) + offset;

	Print(L" command:");
	for (i=0; i<length; i++)
		Print(L" %02X", bSendBuffer[i]);
	Print(L"\n");
	Print(L"Enter your PIN: \n");
	length = sizeof bRecvBuffer;
	rv = SmartCardReader->SCardControl(SmartCardReader, verify_ioctl,
		bSendBuffer, send_length, bRecvBuffer, &length);

	Print(L" card response:");
	for (i=0; i<length; i++)
		Print(L" %02X", bRecvBuffer[i]);
	Print(L": %s\n", pinpad_return_codes(bRecvBuffer));
	PCSC_ERROR_CONT(rv, L"SCardControl")

	/* verify PIN dump */
	Print(L"\nverify PIN dump: ");
	send_length = 5;
	memcpy(bSendBuffer, "\x00\x40\x00\x00\xFF",
		send_length);
	for (i=0; i<send_length; i++)
		Print(L" %02X", bSendBuffer[i]);
	Print(L"\n");
	length = sizeof(bRecvBuffer);
	rv = SmartCardReader->SCardTransmit(SmartCardReader, bSendBuffer, send_length,
		bRecvBuffer, &length);
	Print(L" card response:");
	for (i=0; i<length; i++)
		Print(L" %02X", bRecvBuffer[i]);
	Print(L"\n");
	PCSC_ERROR_EXIT(rv, L"SCardTransmit")

	if ((2 == length) && (0x6C == bRecvBuffer[0]))
	{
		Print(L"\nverify PIN dump: ");
		send_length = 5;
		memcpy(bSendBuffer, "\x00\x40\x00\x00\xFF",
			send_length);
		bSendBuffer[4] = bRecvBuffer[1];
		for (i=0; i<send_length; i++)
			Print(L" %02X", bSendBuffer[i]);
		Print(L"\n");
		length = sizeof(bRecvBuffer);
		rv = SmartCardReader->SCardTransmit(SmartCardReader, bSendBuffer, send_length,
			bRecvBuffer, &length);
		Print(L" card response:");
		for (i=0; i<length; i++)
			Print(L" %02X", bRecvBuffer[i]);
		Print(L"\n");
		PCSC_ERROR_EXIT(rv, L"SCardTransmit")
	}
#endif

	/* check if the reader supports Modify PIN */
	if (0 == modify_ioctl)
	{
		Print(L"Reader does not support PIN modification\n");
		goto end;
	}

#ifdef MODIFY_PIN
	/* Modify PIN */
	Print(L" Secure modify PIN\n");
	pin_modify = (PIN_MODIFY_STRUCTURE *)bSendBuffer;

	/* Table for bConfirmPIN and bNumberMessage
	 * bConfirmPIN = 3, bNumberMessage = 3: "Enter Pin" "New Pin" "Confirm Pin"
	 * bConfirmPIN = 2, bNumberMessage = 2: "Enter Pin" "New Pin"
	 * bConfirmPIN = 1, bNumberMessage = 2: "New Pin" "Confirm Pin"
	 * bConfirmPIN = 0, bNumberMessage = 1: "New Pin"
	 */
	/* table for bMsgIndex[1-3]
	 * 00: PIN insertion prompt        “ENTER SMARTCARD PIN”
	 * 01: PIN Modification prompt     “ ENTER NEW PIN”
	 * 02: NEW PIN Confirmation prompt “ CONFIRM NEW PIN”
	 */
	/* PC/SC v2.02.05 Part 10 PIN modification data structure */
	pin_modify -> bTimerOut = 0x00;
	pin_modify -> bTimerOut2 = 0x00;
	pin_modify -> bmFormatString = 0x82;
	pin_modify -> bmPINBlockString = 0x04;
	pin_modify -> bmPINLengthFormat = 0x00;
	pin_modify -> bInsertionOffsetOld = 0x00;	/* offset from APDU start */
	pin_modify -> bInsertionOffsetNew = 0x04;	/* offset from APDU start */
	pin_modify -> wPINMaxExtraDigit = (PIN_min_size << 8) + PIN_max_size;
	pin_modify -> bConfirmPIN = 0x03;	/* b0 set = confirmation requested */
									/* b1 set = current PIN entry requested */
	pin_modify -> bEntryValidationCondition = bEntryValidationCondition;
	pin_modify -> bNumberMessage = 0x03; /* see table above */
	pin_modify -> wLangId = 0x0904;
	pin_modify -> bMsgIndex1 = 0x00;
	pin_modify -> bMsgIndex2 = 0x01;
	pin_modify -> bMsgIndex3 = 0x02;
	pin_modify -> bTeoPrologue[0] = 0x00;
	pin_modify -> bTeoPrologue[1] = 0x00;
	pin_modify -> bTeoPrologue[2] = 0x00;
	/* pin_modify -> ulDataLength = 0x00; we don't know the size yet */

	/* APDU: 00 20 00 00 08 30 30 30 30 00 00 00 00 */
	offset = 0;
	pin_modify -> abData[offset++] = 0x00;	/* CLA */
	pin_modify -> abData[offset++] = 0x24;	/* INS: CHANGE/UNBLOCK */
	pin_modify -> abData[offset++] = 0x00;	/* P1 */
	pin_modify -> abData[offset++] = 0x00;	/* P2 */
	pin_modify -> abData[offset++] = 0x08;	/* Lc: 2x8 data bytes */
	pin_modify -> abData[offset++] = 0x30;	/* '0' old PIN */
	pin_modify -> abData[offset++] = 0x30;	/* '0' */
	pin_modify -> abData[offset++] = 0x30;	/* '0' */
	pin_modify -> abData[offset++] = 0x30;	/* '0' */
	pin_modify -> abData[offset++] = 0x30;	/* '0' new PIN */
	pin_modify -> abData[offset++] = 0x30;	/* '0' */
	pin_modify -> abData[offset++] = 0x30;	/* '0' */
	pin_modify -> abData[offset++] = 0x30;	/* '0' */
	pin_modify -> ulDataLength = offset;	/* APDU size */

	send_length = sizeof(PIN_MODIFY_STRUCTURE) + offset;

	Print(L" command:");
	for (i=0; i<length; i++)
		Print(L" %02X", bSendBuffer[i]);
	Print(L"\n");
	Print(L"Enter your PIN: \n");
	length = sizeof bRecvBuffer;
	rv = SmartCardReader->SCardControl(SmartCardReader, modify_ioctl,
		bSendBuffer, send_length, bRecvBuffer, &length);

	Print(L" card response:");
	for (i=0; i<length; i++)
		Print(L" %02X", bRecvBuffer[i]);
	Print(L"\n");
	PCSC_ERROR_CONT(rv, L"SCardControl")

	/* modify PIN dump */
	Print(L"\nmodify PIN dump: ");
	send_length = 5;
	memcpy(bSendBuffer, "\x00\x40\x00\x00\xFF",
		send_length);
	for (i=0; i<send_length; i++)
		Print(L" %02X", bSendBuffer[i]);
	Print(L"\n");
	length = sizeof(bRecvBuffer);
	rv = SmartCardReader->SCardTransmit(SmartCardReader, bSendBuffer, send_length,
		bRecvBuffer, &length);
	Print(L" card response:");
	for (i=0; i<length; i++)
		Print(L" %02X", bRecvBuffer[i]);
	Print(L"\n");
	PCSC_ERROR_EXIT(rv, L"SCardTransmit")

	if ((2 == length) && (0x6C == bRecvBuffer[0]))
	{
		Print(L"\nverify PIN dump: ");
		send_length = 5;
		memcpy(bSendBuffer, "\x00\x40\x00\x00\xFF",
			send_length);
		bSendBuffer[4] = bRecvBuffer[1];
		for (i=0; i<send_length; i++)
			Print(L" %02X", bSendBuffer[i]);
		Print(L"\n");
		length = sizeof(bRecvBuffer);
		rv = SmartCardReader->SCardTransmit(SmartCardReader, bSendBuffer, send_length,
			bRecvBuffer, &length);
		Print(L" card response:");
		for (i=0; i<length; i++)
			Print(L" %02X", bRecvBuffer[i]);
		Print(L"\n");
		PCSC_ERROR_EXIT(rv, L"SCardTransmit")
	}
#endif

	/* card disconnect */
	rv = SmartCardReader->SCardDisconnect(SmartCardReader, SCARD_CA_NORESET);
	PCSC_ERROR_CONT(rv, L"SCardDisconnect")

end:
	return 0;
} /* Check */


INTN
EFIAPI
ShellAppMain (
  IN UINTN Argc,
  IN CHAR16 **Argv
  )
{
	EFI_STATUS  Status;
	UINTN       HandleIndex, HandleCount;
	EFI_HANDLE  *DevicePathHandleBuffer = NULL;
	EFI_SMART_CARD_READER_PROTOCOL *SmartCardReader;
	int reader = -1;

	Print(L"SCardControl sample code\n");
	Print(L"V 1.4 © 2004-2014, Ludovic Rousseau <ludovic.rousseau@free.fr>\n\n");

	if (Argc > 1)
		reader = StrDecimalToUintn(Argv[1]);

	/* EFI_SMART_CARD_READER_PROTOCOL */
	Status = gBS->LocateHandleBuffer(
			ByProtocol,
			&gEfiSmartCardReaderProtocolGuid,
			NULL,
			&HandleCount,
			&DevicePathHandleBuffer);

	if (EFI_ERROR(Status))
	{
		Print(L"ERROR: Get EFI_SMART_CARD_READER_PROTOCOL count fail.\n");
		return 0;
	}

	Print(L"Found %d reader(s)\n", HandleCount);
	for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++)
	{
		ZeroMem(&SmartCardReader, sizeof SmartCardReader);

		Status = gBS->HandleProtocol(
				DevicePathHandleBuffer[HandleIndex],
				&gEfiSmartCardReaderProtocolGuid,
				(VOID**)&SmartCardReader);

		if (EFI_ERROR(Status))
		{
			Print(L"ERROR: Open UsbIo fail.\n");
			gBS->FreePool(DevicePathHandleBuffer);
			return 0;
		}

		Print(L"reader %d\n", HandleIndex);

		if (reader < 0 || reader == HandleIndex)
			CheckReader(SmartCardReader);
	}
	gBS->FreePool(DevicePathHandleBuffer);

	return 0;
}
