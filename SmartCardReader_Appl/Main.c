/*

Copyright (c) 2014, Gemalto. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

* Redistributions of source code must retain the above copyright
  notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
  notice, this list of conditions and the following disclaimer in
  the documentation and/or other materials provided with the
  distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

*/

#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/ShellCEntryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/BaseMemoryLib.h>
#include <Protocol/SmartCardReader.h>

#define UEFI_DRIVER
#include "../reader.h"

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
	UINT8 CAPDU[] = {0x00, 0xA4, 0x04, 0x00, 0x06, 0xA0, 0x00, 0x00, 0x00, 0x18, 0xFF};
	UINTN CAPDULength, RAPDULength;
	UINT8 RAPDU[256+2];
	UINT32 ControlCode;
	UINT8 InBuffer[] = {};
	UINTN InBufferLength;
	UINT8 OutBuffer[256];
	UINTN OutBufferLength;
	UINT32 Attrib;

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

	/*
	 * SCardTransmit
	 */
	CAPDULength = sizeof CAPDU;
	RAPDULength = sizeof RAPDU;
	Print(L"CAPDU: ");
	for (i=0; i<CAPDULength; i++)
		Print(L"%02X ", CAPDU[i]);
	Print(L"\n");
	Status = SmartCardReader->SCardTransmit(SmartCardReader,
		CAPDU, CAPDULength,
		RAPDU, &RAPDULength);
	if (EFI_ERROR(Status))
	{
		Print(L"ERROR: SCardTransmit: %d\n", Status);
		return 0;
	}
	Print(L"RAPDU: ");
	for (i=0; i<RAPDULength; i++)
		Print(L"%02X ", RAPDU[i]);
	Print(L"\n");

	/*
	 * SCardControl
	 */
	InBufferLength = sizeof InBuffer;
	OutBufferLength = sizeof OutBuffer;
	ControlCode = CM_IOCTL_GET_FEATURE_REQUEST;
	Status = SmartCardReader->SCardControl(SmartCardReader,
		ControlCode,
		InBuffer, InBufferLength,
		OutBuffer, &OutBufferLength);
	if (EFI_ERROR(Status))
	{
		Print(L"ERROR: SCardControl: %d\n", Status);
		return 0;
	}
	Print(L"SCardControl: ");
	for (i=0; i<OutBufferLength; i++)
		Print(L"%02X ", OutBuffer[i]);
	Print(L"\n");

	/*
	 * SCardGetAttrib
	 */
	Attrib = SCARD_ATTR_ATR_STRING;
	OutBufferLength = sizeof OutBuffer;
	Status = SmartCardReader->SCardGetAttrib(SmartCardReader,
		Attrib,
		OutBuffer, &OutBufferLength);
	if (EFI_ERROR(Status))
	{
		Print(L"ERROR: SCardGetAttrib: %d\n", Status);
		return 0;
	}
	Print(L"SCardGetAttrib: ");
	for (i=0; i<OutBufferLength; i++)
		Print(L"%02X ", OutBuffer[i]);
	Print(L"\n");

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
	int reader = -1;

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

	return(0);
}
