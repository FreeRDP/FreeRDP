/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_UTILS_MSCONFIG_H
#define FREERDP_UTILS_MSCONFIG_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#include <winpr/stream.h>

/* a safer free helper */
#define zfree(p) do { if (p != NULL) {free(p); p = NULL;} } while (0)

/* Data maintenance helper only used in URBDRC */	
#define data_read_BYTE(_p, _v) do { _v = \
	*((BYTE *) (_p)); \
	} while (0)
#define data_read_UINT16(_p, _v) do { _v = \
	((UINT16) (*((BYTE *) (_p)))) + \
	((UINT16) (*(((BYTE *) (_p)) + 1)) << 8); \
	} while (0)
#define data_read_UINT32(_p, _v) do { _v = \
	(UINT32) (*((BYTE *) (_p))) + \
	((UINT32) (*(((BYTE *) (_p)) + 1)) << 8) + \
	((UINT32) (*(((BYTE *) (_p)) + 2)) << 16) + \
	((UINT32) (*(((BYTE *) (_p)) + 3)) << 24); \
	} while (0)
#define data_read_UINT64(_p, _v) do { _v = \
	(UINT64) (*((BYTE *) (_p))) + \
	((UINT64) (*(((BYTE *) (_p)) + 1)) << 8) + \
	((UINT64) (*(((BYTE *) (_p)) + 2)) << 16) + \
	((UINT64) (*(((BYTE *) (_p)) + 3)) << 24) + \
	((UINT64) (*(((BYTE *) (_p)) + 4)) << 32) + \
	((UINT64) (*(((BYTE *) (_p)) + 5)) << 40) + \
	((UINT64) (*(((BYTE *) (_p)) + 6)) << 48) + \
	((UINT64) (*(((BYTE *) (_p)) + 7)) << 56); \
	} while (0)

#define data_write_BYTE(_p, _v) do { \
	*((BYTE *) _p) = (BYTE) (_v); \
	} while (0)
#define data_write_UINT16(_p, _v) do { \
	*((BYTE *) _p) = (BYTE) (((UINT16) (_v)) & 0xff); \
	*(((BYTE *) _p) + 1) = (BYTE) ((((UINT16) (_v)) >> 8) & 0xff); \
	} while (0)
#define data_write_UINT32(_p, _v) do { \
	*((BYTE *) _p) = (BYTE) (((UINT32) (_v)) & 0xff); \
	*(((BYTE *) _p) + 1) = (BYTE) ((((UINT32) (_v)) >> 8) & 0xff); \
	*(((BYTE *) _p) + 2) = (BYTE) ((((UINT32) (_v)) >> 16) & 0xff); \
	*(((BYTE *) _p) + 3) = (BYTE) ((((UINT32) (_v)) >> 24) & 0xff); \
	} while (0)
#define data_write_UINT64(_p, _v) do { \
	*((BYTE *) _p) = (BYTE) (((UINT64) (_v)) & 0xff); \
	*(((BYTE *) _p) + 1) = (BYTE) ((((UINT64) (_v)) >> 8) & 0xff); \
	*(((BYTE *) _p) + 2) = (BYTE) ((((UINT64) (_v)) >> 16) & 0xff); \
	*(((BYTE *) _p) + 3) = (BYTE) ((((UINT64) (_v)) >> 24) & 0xff); \
	*(((BYTE *) _p) + 4) = (BYTE) ((((UINT64) (_v)) >> 32) & 0xff); \
	*(((BYTE *) _p) + 5) = (BYTE) ((((UINT64) (_v)) >> 40) & 0xff); \
	*(((BYTE *) _p) + 6) = (BYTE) ((((UINT64) (_v)) >> 48) & 0xff); \
	*(((BYTE *) _p) + 7) = (BYTE) ((((UINT64) (_v)) >> 56) & 0xff); \
	} while (0)

typedef struct _MSUSB_INTERFACE_DESCRIPTOR MSUSB_INTERFACE_DESCRIPTOR;
typedef struct _MSUSB_PIPE_DESCRIPTOR	  MSUSB_PIPE_DESCRIPTOR;
typedef struct _MSUSB_CONFIG_DESCRIPTOR	MSUSB_CONFIG_DESCRIPTOR;

struct _MSUSB_PIPE_DESCRIPTOR
{
	UINT16 MaximumPacketSize;
	UINT32 MaximumTransferSize;
	UINT32 PipeFlags;
	UINT32 PipeHandle;
	BYTE  bEndpointAddress;
	BYTE  bInterval;
	BYTE  PipeType;
	int InitCompleted;
} __attribute__((packed));

struct _MSUSB_INTERFACE_DESCRIPTOR
{
	UINT16 Length;
	UINT16 NumberOfPipesExpected;
	BYTE  InterfaceNumber;
	BYTE  AlternateSetting;
	UINT32 NumberOfPipes;
	UINT32 InterfaceHandle;
	BYTE  bInterfaceClass;
	BYTE  bInterfaceSubClass;
	BYTE  bInterfaceProtocol;
	MSUSB_PIPE_DESCRIPTOR ** MsPipes;
	int InitCompleted;
} __attribute__((packed));

struct _MSUSB_CONFIG_DESCRIPTOR
{
	UINT16 wTotalLength;
	BYTE  bConfigurationValue;
	UINT32 ConfigurationHandle;
	UINT32 NumInterfaces;
	MSUSB_INTERFACE_DESCRIPTOR ** MsInterfaces;
	int InitCompleted;
	int MsOutSize;
} __attribute__((packed));

#ifdef __cplusplus
extern "C" {
#endif

/* MSUSB_PIPE exported functions */
FREERDP_API void msusb_mspipes_replace(MSUSB_INTERFACE_DESCRIPTOR* MsInterface, MSUSB_PIPE_DESCRIPTOR** NewMsPipes, UINT32 NewNumberOfPipes);

/* MSUSB_INTERFACE exported functions */
FREERDP_API void msusb_msinterface_replace(MSUSB_CONFIG_DESCRIPTOR* MsConfig, BYTE InterfaceNumber, MSUSB_INTERFACE_DESCRIPTOR* NewMsInterface);
FREERDP_API MSUSB_INTERFACE_DESCRIPTOR* msusb_msinterface_read(BYTE* data, UINT32 data_size, int* offset);
FREERDP_API int msusb_msinterface_write(MSUSB_INTERFACE_DESCRIPTOR* MsInterface, BYTE* data, int* offset);

/* MSUSB_CONFIG exported functions */
FREERDP_API MSUSB_CONFIG_DESCRIPTOR* msusb_msconfig_new(void);
FREERDP_API void msusb_msconfig_free(MSUSB_CONFIG_DESCRIPTOR* MsConfig);
FREERDP_API MSUSB_CONFIG_DESCRIPTOR* msusb_msconfig_read(BYTE* data, UINT32 data_size, UINT32 NumInterfaces);
FREERDP_API int msusb_msconfig_write(MSUSB_CONFIG_DESCRIPTOR* MsConfg, BYTE* data, int * offset);
FREERDP_API void msusb_msconfig_dump(MSUSB_CONFIG_DESCRIPTOR* MsConfg);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_UTILS_MSCONFIG_H */
