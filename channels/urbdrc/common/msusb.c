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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freerdp/log.h>
#include <msusb.h>

#define TAG FREERDP_TAG("utils")

static MSUSB_PIPE_DESCRIPTOR* msusb_mspipe_new(void)
{
	return (MSUSB_PIPE_DESCRIPTOR*)calloc(1, sizeof(MSUSB_PIPE_DESCRIPTOR));
}

static void msusb_mspipes_free(MSUSB_PIPE_DESCRIPTOR** MsPipes, UINT32 NumberOfPipes)
{
	if (MsPipes)
	{
		for (UINT32 pnum = 0; pnum < NumberOfPipes && MsPipes[pnum]; pnum++)
			free(MsPipes[pnum]);

		free((void*)MsPipes);
	}
}

void msusb_mspipes_replace(MSUSB_INTERFACE_DESCRIPTOR* MsInterface,
                           MSUSB_PIPE_DESCRIPTOR** NewMsPipes, UINT32 NewNumberOfPipes)
{
	WINPR_ASSERT(MsInterface);
	WINPR_ASSERT(NewMsPipes || (NewNumberOfPipes == 0));

	/* free original MsPipes */
	msusb_mspipes_free(MsInterface->MsPipes, MsInterface->NumberOfPipes);
	/* And replace it */
	MsInterface->MsPipes = NewMsPipes;
	MsInterface->NumberOfPipes = NewNumberOfPipes;
}

static MSUSB_PIPE_DESCRIPTOR** msusb_mspipes_read(wStream* s, UINT32 NumberOfPipes)
{
	MSUSB_PIPE_DESCRIPTOR** MsPipes = nullptr;

	if (!Stream_CheckAndLogRequiredCapacityOfSize(TAG, (s), NumberOfPipes, 12ull))
		return nullptr;

	MsPipes = (MSUSB_PIPE_DESCRIPTOR**)calloc(NumberOfPipes, sizeof(MSUSB_PIPE_DESCRIPTOR*));

	if (!MsPipes)
		return nullptr;

	for (UINT32 pnum = 0; pnum < NumberOfPipes; pnum++)
	{
		MSUSB_PIPE_DESCRIPTOR* MsPipe = msusb_mspipe_new();

		if (!MsPipe)
			goto out_error;

		Stream_Read_UINT16(s, MsPipe->MaximumPacketSize);
		Stream_Seek(s, 2);
		Stream_Read_UINT32(s, MsPipe->MaximumTransferSize);
		Stream_Read_UINT32(s, MsPipe->PipeFlags);
		/* Already set to zero by memset
		        MsPipe->PipeHandle	   = 0;
		        MsPipe->bEndpointAddress = 0;
		        MsPipe->bInterval		= 0;
		        MsPipe->PipeType		 = 0;
		        MsPipe->InitCompleted	= 0;
		*/
		MsPipes[pnum] = MsPipe;
	}

	return MsPipes;
out_error:

	for (UINT32 pnum = 0; pnum < NumberOfPipes; pnum++)
		free(MsPipes[pnum]);

	free((void*)MsPipes);
	return nullptr;
}

WINPR_ATTR_MALLOC(msusb_msinterface_free, 1)
static MSUSB_INTERFACE_DESCRIPTOR* msusb_msinterface_new(void)
{
	return (MSUSB_INTERFACE_DESCRIPTOR*)calloc(1, sizeof(MSUSB_INTERFACE_DESCRIPTOR));
}

void msusb_msinterface_free(MSUSB_INTERFACE_DESCRIPTOR* MsInterface)
{
	if (MsInterface)
	{
		msusb_mspipes_free(MsInterface->MsPipes, MsInterface->NumberOfPipes);
		MsInterface->MsPipes = nullptr;
		free(MsInterface);
	}
}

static void msusb_msinterface_free_list(MSUSB_INTERFACE_DESCRIPTOR** MsInterfaces,
                                        UINT32 NumInterfaces)
{
	if (MsInterfaces)
	{
		for (UINT32 inum = 0; inum < NumInterfaces; inum++)
		{
			msusb_msinterface_free(MsInterfaces[inum]);
		}

		free((void*)MsInterfaces);
	}
}

BOOL msusb_msinterface_replace(MSUSB_CONFIG_DESCRIPTOR* MsConfig, BYTE InterfaceNumber,
                               MSUSB_INTERFACE_DESCRIPTOR* NewMsInterface)
{
	if (!MsConfig || !MsConfig->MsInterfaces)
		return FALSE;
	if (MsConfig->NumInterfaces <= InterfaceNumber)
		return FALSE;

	msusb_msinterface_free(MsConfig->MsInterfaces[InterfaceNumber]);
	MsConfig->MsInterfaces[InterfaceNumber] = NewMsInterface;
	return TRUE;
}

MSUSB_INTERFACE_DESCRIPTOR* msusb_msinterface_read(wStream* s)
{
	if (!Stream_CheckAndLogRequiredCapacity(TAG, (s), 12))
		return nullptr;

	MSUSB_INTERFACE_DESCRIPTOR* MsInterface = msusb_msinterface_new();

	if (!MsInterface)
		return nullptr;

	Stream_Read_UINT16(s, MsInterface->Length);
	Stream_Read_UINT16(s, MsInterface->NumberOfPipesExpected);
	Stream_Read_UINT8(s, MsInterface->InterfaceNumber);
	Stream_Read_UINT8(s, MsInterface->AlternateSetting);
	Stream_Seek(s, 2);
	Stream_Read_UINT32(s, MsInterface->NumberOfPipes);
	MsInterface->InterfaceHandle = 0;
	MsInterface->bInterfaceClass = 0;
	MsInterface->bInterfaceSubClass = 0;
	MsInterface->bInterfaceProtocol = 0;
	MsInterface->InitCompleted = 0;
	MsInterface->MsPipes = nullptr;

	if (MsInterface->NumberOfPipes > 0)
	{
		MsInterface->MsPipes = msusb_mspipes_read(s, MsInterface->NumberOfPipes);

		if (!MsInterface->MsPipes)
			goto out_error;
	}

	return MsInterface;
out_error:
	msusb_msinterface_free(MsInterface);
	return nullptr;
}

BOOL msusb_msinterface_write(const MSUSB_INTERFACE_DESCRIPTOR* MsInterface, wStream* out)
{
	MSUSB_PIPE_DESCRIPTOR** MsPipes = nullptr;
	MSUSB_PIPE_DESCRIPTOR* MsPipe = nullptr;

	if (!MsInterface)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(out, 16 + MsInterface->NumberOfPipes * 20))
		return FALSE;

	/* Length */
	Stream_Write_UINT16(out, MsInterface->Length);
	/* InterfaceNumber */
	Stream_Write_UINT8(out, MsInterface->InterfaceNumber);
	/* AlternateSetting */
	Stream_Write_UINT8(out, MsInterface->AlternateSetting);
	/* bInterfaceClass */
	Stream_Write_UINT8(out, MsInterface->bInterfaceClass);
	/* bInterfaceSubClass */
	Stream_Write_UINT8(out, MsInterface->bInterfaceSubClass);
	/* bInterfaceProtocol */
	Stream_Write_UINT8(out, MsInterface->bInterfaceProtocol);
	/* Padding */
	Stream_Write_UINT8(out, 0);
	/* InterfaceHandle */
	Stream_Write_UINT32(out, MsInterface->InterfaceHandle);
	/* NumberOfPipes */
	Stream_Write_UINT32(out, MsInterface->NumberOfPipes);
	/* Pipes */
	MsPipes = MsInterface->MsPipes;

	for (UINT32 pnum = 0; pnum < MsInterface->NumberOfPipes; pnum++)
	{
		MsPipe = MsPipes[pnum];
		/* MaximumPacketSize */
		Stream_Write_UINT16(out, MsPipe->MaximumPacketSize);
		/* EndpointAddress */
		Stream_Write_UINT8(out, MsPipe->bEndpointAddress);
		/* Interval */
		Stream_Write_UINT8(out, MsPipe->bInterval);
		/* PipeType */
		Stream_Write_UINT32(out, MsPipe->PipeType);
		/* PipeHandle */
		Stream_Write_UINT32(out, MsPipe->PipeHandle);
		/* MaximumTransferSize */
		Stream_Write_UINT32(out, MsPipe->MaximumTransferSize);
		/* PipeFlags */
		Stream_Write_UINT32(out, MsPipe->PipeFlags);
	}

	return TRUE;
}

static MSUSB_INTERFACE_DESCRIPTOR** msusb_msinterface_read_list(wStream* s, UINT32 NumInterfaces)
{
	MSUSB_INTERFACE_DESCRIPTOR** MsInterfaces = nullptr;
	MsInterfaces =
	    (MSUSB_INTERFACE_DESCRIPTOR**)calloc(NumInterfaces, sizeof(MSUSB_INTERFACE_DESCRIPTOR*));

	if (!MsInterfaces)
		return nullptr;

	for (UINT32 inum = 0; inum < NumInterfaces; inum++)
	{
		MsInterfaces[inum] = msusb_msinterface_read(s);

		if (!MsInterfaces[inum])
			goto fail;
	}

	return MsInterfaces;
fail:

	for (UINT32 inum = 0; inum < NumInterfaces; inum++)
		msusb_msinterface_free(MsInterfaces[inum]);

	free((void*)MsInterfaces);
	return nullptr;
}

BOOL msusb_msconfig_write(const MSUSB_CONFIG_DESCRIPTOR* MsConfig, wStream* out)
{
	if (!MsConfig)
		return FALSE;

	if (!Stream_EnsureRemainingCapacity(out, 8))
		return FALSE;

	/* ConfigurationHandle*/
	Stream_Write_UINT32(out, MsConfig->ConfigurationHandle);
	/* NumInterfaces*/
	Stream_Write_UINT32(out, MsConfig->NumInterfaces);
	/* Interfaces */
	MSUSB_INTERFACE_DESCRIPTOR** MsInterfaces = MsConfig->MsInterfaces;

	for (UINT32 inum = 0; inum < MsConfig->NumInterfaces; inum++)
	{
		const MSUSB_INTERFACE_DESCRIPTOR* MsInterface = MsInterfaces[inum];

		if (!msusb_msinterface_write(MsInterface, out))
			return FALSE;
	}

	return TRUE;
}

MSUSB_CONFIG_DESCRIPTOR* msusb_msconfig_new(void)
{
	return (MSUSB_CONFIG_DESCRIPTOR*)calloc(1, sizeof(MSUSB_CONFIG_DESCRIPTOR));
}

void msusb_msconfig_free(MSUSB_CONFIG_DESCRIPTOR* MsConfig)
{
	if (MsConfig)
	{
		msusb_msinterface_free_list(MsConfig->MsInterfaces, MsConfig->NumInterfaces);
		MsConfig->MsInterfaces = nullptr;
		free(MsConfig);
	}
}

MSUSB_CONFIG_DESCRIPTOR* msusb_msconfig_read(wStream* s, UINT32 NumInterfaces)
{
	MSUSB_CONFIG_DESCRIPTOR* MsConfig = nullptr;
	BYTE lenConfiguration = 0;
	BYTE typeConfiguration = 0;

	if (!Stream_CheckAndLogRequiredCapacityOfSize(TAG, (s), 3ULL + NumInterfaces, 2ULL))
		return nullptr;

	MsConfig = msusb_msconfig_new();

	if (!MsConfig)
		goto fail;

	MsConfig->MsInterfaces = msusb_msinterface_read_list(s, NumInterfaces);

	if (!MsConfig->MsInterfaces)
		goto fail;

	Stream_Read_UINT8(s, lenConfiguration);
	Stream_Read_UINT8(s, typeConfiguration);

	if (lenConfiguration != 0x9 || typeConfiguration != 0x2)
	{
		WLog_ERR(TAG, "len and type must be 0x9 and 0x2 , but it is 0x%" PRIx8 " and 0x%" PRIx8 "",
		         lenConfiguration, typeConfiguration);
		goto fail;
	}

	Stream_Read_UINT16(s, MsConfig->wTotalLength);
	Stream_Seek(s, 1);
	Stream_Read_UINT8(s, MsConfig->bConfigurationValue);
	MsConfig->NumInterfaces = NumInterfaces;
	return MsConfig;
fail:
	msusb_msconfig_free(MsConfig);
	return nullptr;
}

void msusb_msconfig_dump(const MSUSB_CONFIG_DESCRIPTOR* MsConfig)
{
	MSUSB_INTERFACE_DESCRIPTOR** MsInterfaces = nullptr;
	MSUSB_INTERFACE_DESCRIPTOR* MsInterface = nullptr;
	MSUSB_PIPE_DESCRIPTOR** MsPipes = nullptr;
	MSUSB_PIPE_DESCRIPTOR* MsPipe = nullptr;

	WLog_INFO(TAG, "=================MsConfig:========================");
	WLog_INFO(TAG, "wTotalLength:%" PRIu16 "", MsConfig->wTotalLength);
	WLog_INFO(TAG, "bConfigurationValue:%" PRIu8 "", MsConfig->bConfigurationValue);
	WLog_INFO(TAG, "ConfigurationHandle:0x%08" PRIx32 "", MsConfig->ConfigurationHandle);
	WLog_INFO(TAG, "InitCompleted:%d", MsConfig->InitCompleted);
	WLog_INFO(TAG, "MsOutSize:%d", MsConfig->MsOutSize);
	WLog_INFO(TAG, "NumInterfaces:%" PRIu32 "", MsConfig->NumInterfaces);
	MsInterfaces = MsConfig->MsInterfaces;

	for (UINT32 inum = 0; inum < MsConfig->NumInterfaces; inum++)
	{
		MsInterface = MsInterfaces[inum];
		WLog_INFO(TAG, "	Interface: %" PRIu8 "", MsInterface->InterfaceNumber);
		WLog_INFO(TAG, "	Length: %" PRIu16 "", MsInterface->Length);
		WLog_INFO(TAG, "	NumberOfPipesExpected: %" PRIu16 "",
		          MsInterface->NumberOfPipesExpected);
		WLog_INFO(TAG, "	AlternateSetting: %" PRIu8 "", MsInterface->AlternateSetting);
		WLog_INFO(TAG, "	NumberOfPipes: %" PRIu32 "", MsInterface->NumberOfPipes);
		WLog_INFO(TAG, "	InterfaceHandle: 0x%08" PRIx32 "", MsInterface->InterfaceHandle);
		WLog_INFO(TAG, "	bInterfaceClass: 0x%02" PRIx8 "", MsInterface->bInterfaceClass);
		WLog_INFO(TAG, "	bInterfaceSubClass: 0x%02" PRIx8 "", MsInterface->bInterfaceSubClass);
		WLog_INFO(TAG, "	bInterfaceProtocol: 0x%02" PRIx8 "", MsInterface->bInterfaceProtocol);
		WLog_INFO(TAG, "	InitCompleted: %d", MsInterface->InitCompleted);
		MsPipes = MsInterface->MsPipes;

		for (UINT32 pnum = 0; pnum < MsInterface->NumberOfPipes; pnum++)
		{
			MsPipe = MsPipes[pnum];
			WLog_INFO(TAG, "		Pipe: %" PRIu32, pnum);
			WLog_INFO(TAG, "		MaximumPacketSize: 0x%04" PRIx16 "", MsPipe->MaximumPacketSize);
			WLog_INFO(TAG, "		MaximumTransferSize: 0x%08" PRIx32 "",
			          MsPipe->MaximumTransferSize);
			WLog_INFO(TAG, "		PipeFlags: 0x%08" PRIx32 "", MsPipe->PipeFlags);
			WLog_INFO(TAG, "		PipeHandle: 0x%08" PRIx32 "", MsPipe->PipeHandle);
			WLog_INFO(TAG, "		bEndpointAddress: 0x%02" PRIx8 "", MsPipe->bEndpointAddress);
			WLog_INFO(TAG, "		bInterval: %" PRIu8 "", MsPipe->bInterval);
			WLog_INFO(TAG, "		PipeType: 0x%02" PRIx8 "", MsPipe->PipeType);
			WLog_INFO(TAG, "		InitCompleted: %d", MsPipe->InitCompleted);
		}
	}

	WLog_INFO(TAG, "==================================================");
}
