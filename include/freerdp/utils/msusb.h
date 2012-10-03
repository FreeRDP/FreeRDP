/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX USB Redirection
 *
 * Copyright 2012 Atrust corp.
 * Copyright 2012 Alfred Liu <alfred.liu@atruscorp.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __MSCONFIG_H
#define __MSCONFIG_H

#include <freerdp/types.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/stream.h>

/* a safer free helper */
#define zfree(p) do { if (p != NULL) {free(p); p = NULL;} } while (0)

/* Data maintenance helper only used in URBDRC */	
#define data_read_uint8(_p, _v) do { _v = \
	*((uint8 *) (_p)); \
	} while (0)
#define data_read_uint16(_p, _v) do { _v = \
	((uint16) (*((uint8 *) (_p)))) + \
	((uint16) (*(((uint8 *) (_p)) + 1)) << 8); \
	} while (0)
#define data_read_uint32(_p, _v) do { _v = \
	(uint32) (*((uint8 *) (_p))) + \
	((uint32) (*(((uint8 *) (_p)) + 1)) << 8) + \
	((uint32) (*(((uint8 *) (_p)) + 2)) << 16) + \
	((uint32) (*(((uint8 *) (_p)) + 3)) << 24); \
	} while (0)
#define data_read_uint64(_p, _v) do { _v = \
	(uint64) (*((uint8 *) (_p))) + \
	((uint64) (*(((uint8 *) (_p)) + 1)) << 8) + \
	((uint64) (*(((uint8 *) (_p)) + 2)) << 16) + \
	((uint64) (*(((uint8 *) (_p)) + 3)) << 24) + \
	((uint64) (*(((uint8 *) (_p)) + 4)) << 32) + \
	((uint64) (*(((uint8 *) (_p)) + 5)) << 40) + \
	((uint64) (*(((uint8 *) (_p)) + 6)) << 48) + \
	((uint64) (*(((uint8 *) (_p)) + 7)) << 56); \
	} while (0)

#define data_write_uint8(_p, _v) do { \
	*((uint8 *) _p) = (uint8) (_v); \
	} while (0)
#define data_write_uint16(_p, _v) do { \
	*((uint8 *) _p) = (uint8) (((uint16) (_v)) & 0xff); \
	*(((uint8 *) _p) + 1) = (uint8) ((((uint16) (_v)) >> 8) & 0xff); \
	} while (0)
#define data_write_uint32(_p, _v) do { \
	*((uint8 *) _p) = (uint8) (((uint32) (_v)) & 0xff); \
	*(((uint8 *) _p) + 1) = (uint8) ((((uint32) (_v)) >> 8) & 0xff); \
	*(((uint8 *) _p) + 2) = (uint8) ((((uint32) (_v)) >> 16) & 0xff); \
	*(((uint8 *) _p) + 3) = (uint8) ((((uint32) (_v)) >> 24) & 0xff); \
	} while (0)
#define data_write_uint64(_p, _v) do { \
	*((uint8 *) _p) = (uint8) (((uint64) (_v)) & 0xff); \
	*(((uint8 *) _p) + 1) = (uint8) ((((uint64) (_v)) >> 8) & 0xff); \
	*(((uint8 *) _p) + 2) = (uint8) ((((uint64) (_v)) >> 16) & 0xff); \
	*(((uint8 *) _p) + 3) = (uint8) ((((uint64) (_v)) >> 24) & 0xff); \
	*(((uint8 *) _p) + 4) = (uint8) ((((uint64) (_v)) >> 32) & 0xff); \
	*(((uint8 *) _p) + 5) = (uint8) ((((uint64) (_v)) >> 40) & 0xff); \
	*(((uint8 *) _p) + 6) = (uint8) ((((uint64) (_v)) >> 48) & 0xff); \
	*(((uint8 *) _p) + 7) = (uint8) ((((uint64) (_v)) >> 56) & 0xff); \
	} while (0)

typedef struct _MSUSB_INTERFACE_DESCRIPTOR MSUSB_INTERFACE_DESCRIPTOR;
typedef struct _MSUSB_PIPE_DESCRIPTOR	  MSUSB_PIPE_DESCRIPTOR;
typedef struct _MSUSB_CONFIG_DESCRIPTOR	MSUSB_CONFIG_DESCRIPTOR;

struct _MSUSB_PIPE_DESCRIPTOR
{
	uint16 MaximumPacketSize;
	uint32 MaximumTransferSize;
	uint32 PipeFlags;
	uint32 PipeHandle;
	uint8  bEndpointAddress;
	uint8  bInterval;
	uint8  PipeType;
	int InitCompleted;
} __attribute__((packed));

struct _MSUSB_INTERFACE_DESCRIPTOR
{
	uint16 Length;
	uint16 NumberOfPipesExpected;
	uint8  InterfaceNumber;
	uint8  AlternateSetting;
	uint32 NumberOfPipes;
	uint32 InterfaceHandle;
	uint8  bInterfaceClass;
	uint8  bInterfaceSubClass;
	uint8  bInterfaceProtocol;
	MSUSB_PIPE_DESCRIPTOR ** MsPipes;
	int InitCompleted;
} __attribute__((packed));

struct _MSUSB_CONFIG_DESCRIPTOR
{
	uint16 wTotalLength;
	uint8  bConfigurationValue;
	uint32 ConfigurationHandle;
	uint32 NumInterfaces;
	MSUSB_INTERFACE_DESCRIPTOR ** MsInterfaces;
	int InitCompleted;
	int MsOutSize;
} __attribute__((packed));

/* MSUSB_PIPE exported functions */
void msusb_mspipes_replace(MSUSB_INTERFACE_DESCRIPTOR* MsInterface, MSUSB_PIPE_DESCRIPTOR** NewMsPipes, uint32 NewNumberOfPipes);

/* MSUSB_INTERFACE exported functions */
void msusb_msinterface_replace(MSUSB_CONFIG_DESCRIPTOR* MsConfig, uint8 InterfaceNumber, MSUSB_INTERFACE_DESCRIPTOR* NewMsInterface);
MSUSB_INTERFACE_DESCRIPTOR* msusb_msinterface_read(uint8* data, uint32 data_size, int* offset);
int msusb_msinterface_write(MSUSB_INTERFACE_DESCRIPTOR* MsInterface, uint8* data, int* offset);

/* MSUSB_CONFIG exported functions */
MSUSB_CONFIG_DESCRIPTOR* msusb_msconfig_new();
void msusb_msconfig_free(MSUSB_CONFIG_DESCRIPTOR* MsConfig);
MSUSB_CONFIG_DESCRIPTOR* msusb_msconfig_read(uint8* data, uint32 data_size, uint32 NumInterfaces);
int msusb_msconfig_write(MSUSB_CONFIG_DESCRIPTOR* MsConfg, uint8* data, int * offset);
void msusb_msconfig_dump(MSUSB_CONFIG_DESCRIPTOR* MsConfg);

#endif
