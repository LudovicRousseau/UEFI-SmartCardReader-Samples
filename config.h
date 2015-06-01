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

/* we build for the UEFI driver */
#define UEFI_DRIVER

/* no log or debug messages */
#define NO_LOG

#define size_t long
#ifndef uint8_t
#define uint8_t unsigned char
#endif
#define uint32_t UINT32

/* Define to the version of this package. */
#define MAX_BUFFER_SIZE_EXTENDED    (4 + 3 + (1<<16) + 3 + 2)   /**< enhanced (64K + APDU + Lc + Le + SW) Tx/Rx Buffer */

#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>

#define free(a) FreePool(a)
#define getenv(a) NULL
#define malloc(a) AllocateZeroPool(a)
#define memcpy CopyMem
#define memmove CopyMem
#define memset(buffer, value, length) SetMem(buffer, length, value)
#define sleep(a) a
#define strncmp(s1, s2, n) -1
#define strerror(a) ""
#define memcmp CompareMem

/* TODO */
#define usleep(a) 1
#define ENODEV -1
#define errno -1
#define htonl(a) (a)

