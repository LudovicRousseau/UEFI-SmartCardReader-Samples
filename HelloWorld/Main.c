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

int HelloWorld(EFI_SMART_CARD_READER_PROTOCOL *SmartCardReader)
{
	EFI_STATUS  Status;
	UINT32 ActiveProtocol;
	int i;
	UINT8 CAPDU_select[] = {0x00, 0xA4, 0x04, 0x00, 0x0A, 0xA0, 0x00, 0x00, 0x00, 0x62, 0x03, 0x01, 0x0C, 0x06, 0x01};
	UINT8 CAPDU_command[] = {0x00, 0x00, 0x00, 0x00};
	UINTN CAPDULength, RAPDULength;
	UINT8 RAPDU[256+2];

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
	 * SCardTransmit Select
	 */
	CAPDULength = sizeof CAPDU_select;
	RAPDULength = sizeof RAPDU;
	Print(L"CAPDU: ");
	for (i=0; i<CAPDULength; i++)
		Print(L"%02X ", CAPDU_select[i]);
	Print(L"\n");
	Status = SmartCardReader->SCardTransmit(SmartCardReader,
		CAPDU_select, CAPDULength,
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
	 * SCardTransmit Command
	 */
	CAPDULength = sizeof CAPDU_command;
	RAPDULength = sizeof RAPDU;
	Print(L"CAPDU: ");
	for (i=0; i<CAPDULength; i++)
		Print(L"%02X ", CAPDU_command[i]);
	Print(L"\n");
	Status = SmartCardReader->SCardTransmit(SmartCardReader,
		CAPDU_command, CAPDULength,
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
	for (i=0; i<RAPDULength; i++)
		Print(L"%c", RAPDU[i]);
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
			Print(L"ERROR: Open Protocol fail.\n");
			gBS->FreePool(DevicePathHandleBuffer);
			return 0;
		}

		HelloWorld(SmartCardReader);
	}
	gBS->FreePool(DevicePathHandleBuffer);

	return(0);
}
