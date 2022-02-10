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

#include <winpr/stream.h>
#include <freerdp/api.h>

typedef struct
{
	UINT16 MaximumPacketSize;
	UINT32 MaximumTransferSize;
	UINT32 PipeFlags;
	UINT32 PipeHandle;
	BYTE bEndpointAddress;
	BYTE bInterval;
	BYTE PipeType;
	int InitCompleted;
} MSUSB_PIPE_DESCRIPTOR;

typedef struct
{
	UINT16 Length;
	UINT16 NumberOfPipesExpected;
	BYTE InterfaceNumber;
	BYTE AlternateSetting;
	UINT32 NumberOfPipes;
	UINT32 InterfaceHandle;
	BYTE bInterfaceClass;
	BYTE bInterfaceSubClass;
	BYTE bInterfaceProtocol;
	MSUSB_PIPE_DESCRIPTOR** MsPipes;
	int InitCompleted;
} MSUSB_INTERFACE_DESCRIPTOR;

typedef struct
{
	UINT16 wTotalLength;
	BYTE bConfigurationValue;
	UINT32 ConfigurationHandle;
	UINT32 NumInterfaces;
	MSUSB_INTERFACE_DESCRIPTOR** MsInterfaces;
	int InitCompleted;
	int MsOutSize;
} MSUSB_CONFIG_DESCRIPTOR;

#ifdef __cplusplus
extern "C"
{
#endif

	/* MSUSB_PIPE exported functions */
	FREERDP_API BOOL msusb_mspipes_replace(MSUSB_INTERFACE_DESCRIPTOR* MsInterface,
	                                       MSUSB_PIPE_DESCRIPTOR** NewMsPipes,
	                                       UINT32 NewNumberOfPipes);

	/* MSUSB_INTERFACE exported functions */
	FREERDP_API BOOL msusb_msinterface_replace(MSUSB_CONFIG_DESCRIPTOR* MsConfig,
	                                           BYTE InterfaceNumber,
	                                           MSUSB_INTERFACE_DESCRIPTOR* NewMsInterface);
	FREERDP_API MSUSB_INTERFACE_DESCRIPTOR* msusb_msinterface_read(wStream* out);
	FREERDP_API BOOL msusb_msinterface_write(MSUSB_INTERFACE_DESCRIPTOR* MsInterface, wStream* out);
	FREERDP_API void msusb_msinterface_free(MSUSB_INTERFACE_DESCRIPTOR* MsInterface);

	/* MSUSB_CONFIG exported functions */
	FREERDP_API MSUSB_CONFIG_DESCRIPTOR* msusb_msconfig_new(void);
	FREERDP_API void msusb_msconfig_free(MSUSB_CONFIG_DESCRIPTOR* MsConfig);
	FREERDP_API MSUSB_CONFIG_DESCRIPTOR* msusb_msconfig_read(wStream* s, UINT32 NumInterfaces);
	FREERDP_API BOOL msusb_msconfig_write(MSUSB_CONFIG_DESCRIPTOR* MsConfg, wStream* out);
	FREERDP_API void msusb_msconfig_dump(MSUSB_CONFIG_DESCRIPTOR* MsConfg);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_UTILS_MSCONFIG_H */
