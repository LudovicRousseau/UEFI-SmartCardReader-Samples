/*
    Main.c: main function used for IFDH debug
    Copyright (C) 2014-2015   Ludovic Rousseau

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* This code is an adaptation to UEFI of handler_test.c from the
 * HandlerTest project
 * https://anonscm.debian.org/viewvc/pcsclite/trunk/HandlerTest/Host/ */

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/SmartCardReader.h>

#define UEFI_DRIVER
//#include "reader.h"

int cases = 0;
int extended = FALSE;
int timerequest = -1;
int apdu = 0;
int tpdu = 1;

#define MAX_BUFFER_SIZE_EXTENDED    (4 + 3 + (1<<16) + 3 + 2)   /**< enhanced (64K + APDU + Lc + Le + SW) Tx/Rx Buffer */
#define MAX_BUFFER_SIZE (4 + 3 + (1<<8) + 3 + 2)

#define CASE1 (1)
#define CASE2 (1<<1)
#define CASE3 (1<<2)
#define CASE4 (1<<3)

#define PCSC_ERROR(x) Print(L"%a:%d " x ": %d\n", __FILE__, __LINE__, rv)

int exchange(const char *text, EFI_SMART_CARD_READER_PROTOCOL *SmartCardReader,
	unsigned char s[], unsigned int s_length,
	unsigned char r[], UINTN * r_length,
	unsigned char e[], unsigned int e_length)
{
	int rv;
#ifndef CONTACTLESS
	unsigned int i;
#else
	(void)e;
#endif

	Print(L"\n%a (%d, %d)\n", text, s_length, e_length);
	//log_xxd(0, "Sent: ", s, s_length);

	rv = SmartCardReader->SCardTransmit(SmartCardReader, s, s_length, r, r_length);

	//log_msg("Received %lu (0x%04lX) bytes", *r_length, *r_length);
	//log_xxd("Received: ", r, *r_length);
	if (rv)
	{
		PCSC_ERROR("IFDHTransmitToICC");
		return 1;
	}

	/* check the received length */
	if (*r_length != e_length)
	{
		Print(L"ERROR: Expected %d bytes and received %d\n",
			e_length, *r_length);
		return 1;
	}

#ifndef CONTACTLESS
	/* check the received data */
	for (i=0; i<e_length; i++)
		if (r[i] != e[i])
		{
			Print(L"ERROR byte %d: expected 0x%02X, got 0x%02X\n",
				i, e[i], r[i]);
			return 1;
			break;
		}
#endif

	Print(L"--------> OK\n");

	return 0;
} /* exchange */

int extended_apdu(EFI_SMART_CARD_READER_PROTOCOL *SmartCardReader)
{
	int i, len_i, len_o;
	unsigned char s[MAX_BUFFER_SIZE_EXTENDED], r[MAX_BUFFER_SIZE_EXTENDED];
	UINTN dwSendLength, dwRecvLength;
	unsigned char e[MAX_BUFFER_SIZE_EXTENDED];	// expected result
	int e_length;	// expected result length
	const char *text = NULL;
	int start, end;

	if (cases & CASE3)
	{
		/* Case 3 */
		text = "Case 3: CLA INS P1 P2 Lc Data, L(Cmd) = 5 + Lc";
		end = 65535;
		start = 1;

		for (len_i = start; len_i <= end; len_i++)
		{
#ifdef CONTACTLESS
			s[0] = 0x00;
			s[1] = 0xD6;
			s[2] = 0x00;
			s[3] = 0x00;
#else
			s[0] = 0x80;
			s[1] = 0x12;
			s[2] = 0x01;
			s[3] = 0x80;
#endif
			s[4] = 0x00;	/* extended */
			s[5] = len_i >> 8;
			s[6] = len_i;

			for (i=0; i<len_i; i++)
				s[7+i] = i;

			dwSendLength = len_i + 7;
			dwRecvLength = sizeof(r);

			e[0] = 0x90;
			e[1] = 0x00;
			e_length = 2;

			if (exchange(text, SmartCardReader,
				s, dwSendLength, r, &dwRecvLength, e, e_length))
				return 1;
		}
	}

	if (cases & CASE2)
	{
		/* Case 2 */
		/*
		 * 252  (0xFC) is max size for one USB or GBP paquet
		 * 256 (0x100) maximum, 1 minimum
		 */
		text = "Case 2: CLA INS P1 P2 Le, L(Cmd) = 5";
		end = 65535;
		start = 1;

		for (len_o = start; len_o <= end; len_o++)
		{
			char test_value = 0x42;

#ifdef CONTACTLESS
			s[0] = 0x00;
			s[1] = 0xB0;
			s[2] = 0x00;
			s[3] = 0x00;
#else
			s[0] = 0x80;
			s[1] = 0x00;
			s[2] = 0x04;
			s[3] = test_value;
#endif
			s[4] = 0x00;
			s[5] = len_o >> 8;
			s[6] = len_o;

			dwSendLength = 7;
			dwRecvLength = sizeof(r);

			for (i=0; i<len_o; i++)
				e[i] = test_value;
			e[i++] = 0x90;
			e[i++] = 0x00;
			e_length = len_o+2;

			if (exchange(text, SmartCardReader, 
				s, dwSendLength, r, &dwRecvLength, e, e_length))
				return 1;
		}
	}

	return 0;
} /* extended_apdu */

int short_apdu(EFI_SMART_CARD_READER_PROTOCOL *SmartCardReader)
{
	int i, len_i, len_o;
	unsigned char s[MAX_BUFFER_SIZE], r[MAX_BUFFER_SIZE];
	UINTN dwSendLength, dwRecvLength;
	unsigned char e[MAX_BUFFER_SIZE];	// expected result
	int e_length;	// expected result length
	const char *text = NULL;
	int time;
	int start, end;

	/* Select applet */
	text = "Select applet: ";
	s[0] = 0x00;
	s[1] = 0xA4;
	s[2] = 0x04;
	s[3] = 0x00;
	s[4] = 0x06;
	s[5] = 0xA0;
	s[6] = 0x00;
	s[7] = 0x00;
	s[8] = 0x00;
	s[9] = 0x18;
#ifdef COMBI
	s[10] = 0x50;
#else
	s[10] = 0xFF;
#endif

	dwSendLength = 11;
	dwRecvLength = sizeof(r);

	e[0] = 0x90;
	e[1] = 0x00;
	e_length = 2;

	if (exchange(text, SmartCardReader,
		s, dwSendLength, r, &dwRecvLength, e, e_length))
		return 1;

	/* Time Request */
	if (timerequest >= 0)
	{
		text = "Time Request";
		time = timerequest;

		s[0] = 0x80;
		s[1] = 0x38;
		s[2] = 0x00;
		s[3] = time;
		s[4] = 0;

		if (apdu)
			dwSendLength = 4;
		else
			dwSendLength = 5;
		dwRecvLength = sizeof(r);

		e[0] = 0x90;
		e[1] = 0x00;
		e_length = 2;

		if (exchange(text, SmartCardReader,
			s, dwSendLength, r, &dwRecvLength, e, e_length))
			return 1;
	}

	if (cases & CASE1)
	{
		if (apdu)
		{
			/* Case 1, APDU */
			text = "Case 1, APDU: CLA INS P1 P2, L(Cmd) = 4";
			s[0] = 0x80;
			s[1] = 0x30;
			s[2] = 0x00;
			s[3] = 0x00;

			dwSendLength = 4;
			dwRecvLength = sizeof(r);

			e[0] = 0x90;
			e[1] = 0x00;
			e_length = 2;

			if (exchange(text, SmartCardReader,
						s, dwSendLength, r, &dwRecvLength, e, e_length))
				return 1;
		}

		if (tpdu)
		{
			/* Case 1, TPDU */
			text = "Case 1, TPDU: CLA INS P1 P2 P3 (=0), L(Cmd) = 5";
			s[0] = 0x80;
			s[1] = 0x30;
			s[2] = 0x00;
			s[3] = 0x00;
			s[4] = 0x00;

			dwSendLength = 5;
			dwRecvLength = sizeof(r);

			e[0] = 0x90;
			e[1] = 0x00;
			e_length = 2;

			if (exchange(text, SmartCardReader,
						s, dwSendLength, r, &dwRecvLength, e, e_length))
				return 1;
		}
	}

	if (cases & CASE3)
	{
		/* Case 3 */
		/*
		 * 248 (0xF8) is max size for one USB or GBP paquet
		 * 255 (0xFF) maximum, 1 minimum
		 */
		text = "Case 3: CLA INS P1 P2 Lc Data, L(Cmd) = 5 + Lc";
		end = 255;
		start = 1;

		for (len_i = start; len_i <= end; len_i++)
		{
			s[0] = 0x80;
			s[1] = 0x32;
			s[2] = 0x00;
			s[3] = 0x00;
			s[4] = len_i;

			for (i=0; i<len_i; i++)
				s[5+i] = i;

			dwSendLength = len_i + 5;
			dwRecvLength = sizeof(r);

			e[0] = 0x90;
			e[1] = 0x00;
			e_length = 2;

			if (exchange(text, SmartCardReader,
				s, dwSendLength, r, &dwRecvLength, e, e_length))
				return 1;
		}
	}

	if (cases & CASE2)
	{
		/* Case 2 */
		/*
		 * 252  (0xFC) is max size for one USB or GBP paquet
		 * 256 (0x100) maximum, 1 minimum
		 */
		text = "Case 2: CLA INS P1 P2 Le, L(Cmd) = 5";
		end = 256;
		start = 1;

		for (len_o = start; len_o <= end; len_o++)
		{
			s[0] = 0x80;
			s[1] = 0x34;
			if (len_o > 255)
			{
				s[2] = 0x01;
				s[3] = len_o-256;
			}
			else
			{
				s[2] = 0x00;
				s[3] = len_o;
			}
			s[4] = len_o;

			dwSendLength = 5;
			dwRecvLength = sizeof(r);

			for (i=0; i<len_o; i++)
				e[i] = i;
			e[i++] = 0x90;
			e[i++] = 0x00;
			e_length = len_o+2;

			if (exchange(text, SmartCardReader,
				s, dwSendLength, r, &dwRecvLength, e, e_length))
				return 1;
		}

#if 0
		/* Case 2, length too short */
		text = "Case 2, length too short: CLA INS P1 P2 Le, L(Cmd) = 5";
		len_o = 20;

		s[0] = 0x80;
		s[1] = 0x3C;
		if (len_o > 255)
		{
			s[2] = 0x01;
			s[3] = len_o-256;
		}
		else
		{
			s[2] = 0x00;
			s[3] = len_o;
		}
		s[4] = len_o-10;

		dwSendLength = 5;
		dwRecvLength = sizeof(r);

		if (tpdu)
		{
			for (i=0; i<len_o; i++)
				e[i] = i;
			e[i++] = 0x90;
			e[i++] = 0x00;
			e_length = len_o+2;
		}
		else
		{
			e[0] = 0x6C;
			e[1] = len_o;
			e_length = 2;
		}

		if (exchange(text, SmartCardReader,
			s, dwSendLength, r, &dwRecvLength, e, e_length))
			return 1;
#endif

#if 0
		/* Case 2, length too long */
		text = "Case 2, length too long: CLA INS P1 P2 Le, L(Cmd) = 5";
		len_o = 20;

		s[0] = 0x80;
		s[1] = 0x3C;
		if (len_o > 255)
		{
			s[2] = 0x01;
			s[3] = len_o-256;
		}
		else
		{
			s[2] = 0x00;
			s[3] = len_o;
		}
		s[4] = len_o+10;

		dwSendLength = 5;
		dwRecvLength = sizeof(r);

		if (tpdu)
		{
			for (i=0; i<len_o; i++)
				e[i] = i;
			e[i++] = 0x90;
			e[i++] = 0x00;
			e_length = len_o+2;
		}
		else
		{
			e[0] = 0x6C;
			e[1] = len_o;
			e_length = 2;
		}

		if (exchange(text, SmartCardReader,
			s, dwSendLength, r, &dwRecvLength, e, e_length))
			return 1;
#endif
	}

	if (cases & CASE4)
	{
		if (tpdu)
		{
			/* Case 4, TPDU */
			/*
			 * len_i
			 * 248 (0xF8) is max size for one USB or GBP paquet
			 * 255 (0xFF) maximum, 1 minimum
			 *
			 * len_o
			 * 252  (0xFC) is max size for one USB or GBP paquet
			 * 256 (0x100) maximum, 1 minimum
			 */
			end = 255;
			start = 1;

			for (len_i = start; len_i <= end; len_i++)
			{
				text = "Case 4, TPDU: CLA INS P1 P2 Lc Data, L(Cmd) = 5 + Lc";
				len_o = 256 - len_i;

				s[0] = 0x80;
				s[1] = 0x36;
				if (len_o > 255)
				{
					s[2] = 0x01;
					s[3] = len_o-256;
				}
				else
				{
					s[2] = 0x00;
					s[3] = len_o;
				}
				s[4] = len_i;

				for (i=0; i<len_i; i++)
					s[5+i] = i;

				dwSendLength = len_i + 5;
				dwRecvLength = sizeof(r);

				e[0] = 0x61;
				e[1] = len_o & 0xFF;
				e_length = 2;

				if (exchange(text, SmartCardReader,
					s, dwSendLength, r, &dwRecvLength, e, e_length))
					return 1;

				/* Get response */
				text = "Case 4, TPDU, Get response: ";
				s[0] = 0x80;
				s[1] = 0xC0;
				s[2] = 0x00;
				s[3] = 0x00;
				s[4] = r[1]; /* SW2 of previous command */

				dwSendLength = 5;
				dwRecvLength = sizeof(r);

				for (i=0; i<len_o; i++)
					e[i] = i;
				e[i++] = 0x90;
				e[i++] = 0x00;
				e_length = len_o+2;

				if (exchange(text, SmartCardReader,
					s, dwSendLength, r, &dwRecvLength, e, e_length))
					return 1;
			}
		}

		if (apdu)
		{
			/* Case 4, APDU */
			/*
			 * len_i
			 * 248 (0xF8) is max size for one USB or GBP paquet
			 * 255 (0xFF) maximum, 1 minimum
			 *
			 * len_o
			 * 252  (0xFC) is max size for one USB or GBP paquet
			 * 256 (0x100) maximum, 1 minimum
			 */
			text = "Case 4, APDU: CLA INS P1 P2 Lc Data Le, L(Cmd) = 5 + Lc +1";
			end = 255;
			start = 1;

			for (len_i = start; len_i <= end; len_i++)
			{
				len_o = 256 - len_i;

				s[0] = 0x80;
				s[1] = 0x36;
				if (len_o > 255)
				{
					s[2] = 0x01;
					s[3] = len_o-256;
				}
				else
				{
					s[2] = 0x00;
					s[3] = len_o;
				}
				s[4] = len_i;

				for (i=0; i<len_i; i++)
					s[5+i] = i;
				s[5+len_i] = len_o & 0xFF;

				dwSendLength = len_i + 6;
				dwRecvLength = sizeof(r);

				for (i=0; i<len_o; i++)
					e[i] = i;
				e[i++] = 0x90;
				e[i++] = 0x00;
				e_length = len_o+2;

				if (exchange(text, SmartCardReader,
					s, dwSendLength, r, &dwRecvLength, e, e_length))
					return 1;
			}
		}
	}

	return 0;
} /* short_apdu */

int CheckReader(EFI_SMART_CARD_READER_PROTOCOL *SmartCardReader)
{
	EFI_STATUS  Status;
	CHAR16 ReaderName[100];
	UINTN ReaderNameLength = sizeof ReaderName;
	UINT32 State;
	UINT32 CardProtocol;
	UINT8 Atr[33];
	UINTN AtrLength = sizeof Atr;
	UINT32 ActiveProtocol;
	int i;

	/*
	 * SCardStatus
	 */
	Status = SmartCardReader->SCardStatus(SmartCardReader,
			ReaderName,
			&ReaderNameLength,
			&State,
			&CardProtocol,
			Atr,
			&AtrLength);
	if (EFI_ERROR(Status))
	{
		Print(L"ERROR: SCardStatus: %d\n", Status);
		return 0;
	}

	Print(L"ReaderName (%d): %s\n", ReaderNameLength, ReaderName);
	Print(L"State: %d: ", State);
	switch(State)
	{
		case SCARD_UNKNOWN:
			Print(L"SCARD_UNKNOWN");
			break;
		case SCARD_ABSENT:
			Print(L"SCARD_ABSENT");
			break;
		case SCARD_INACTIVE:
			Print(L"SCARD_INACTIVE");
			break;
		case SCARD_ACTIVE:
			Print(L"SCARD_ACTIVE");
			break;
	}
	Print(L"\n");
	Print(L"CardProtocol: %d\n", CardProtocol);
	Print(L"Atr (%d): ", AtrLength);
	for (i=0; i<AtrLength; i++)
		Print(L"%02X ", Atr[i]);
	Print(L"\n");

	/*
	 * SCardConnect
	 */
	Status = SmartCardReader->SCardConnect(SmartCardReader,
		SCARD_AM_CARD,
		SCARD_CA_COLDRESET,
		SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
		&ActiveProtocol);
	if (EFI_ERROR(Status))
	{
		Print(L"ERROR: SCardConnect: %d\n", Status);
		return 0;
	}

	if (extended)
		extended_apdu(SmartCardReader);
	else
		short_apdu(SmartCardReader);

	/*
	 * SCardDisconnect
	 */
	Status = SmartCardReader->SCardDisconnect(SmartCardReader,
		SCARD_CA_NORESET);
	if (EFI_ERROR(Status))
	{
		Print(L"ERROR: SCardDisconnect: %d\n", Status);
		return 0;
	}

	return 0;
}

/***
  Print a welcoming message.

  Establishes the main structure of the application.

  @retval  0         The application exited normally.
  @retval  Other     An error occurred.
***/
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
	int i;
	int reader = -1;

	for (i=0; i<Argc; i++)
	{
		CHAR16 opt = Argv[i][0];

		switch(opt)
		{
			case '1':
			case '2':
			case '3':
			case '4':
				cases |= 1 << (opt - '1');
				Print(L"test case: %c\n", opt);
				break;

			case 'e':
				extended = TRUE;
				Print(L"text extended APDU\n");
				break;

			case 't':
				timerequest = StrDecimalToUintn(Argv[i]+1);
				Print(L"time request: %d\n", timerequest);
				break;

			case 'r':
				reader = StrDecimalToUintn(Argv[i]+1);
				Print(L"Using reader: %d\n", reader);
				break;

			case 'a':
				apdu = 1;
				tpdu = 0;
		}
	}

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

	return(0);
}
