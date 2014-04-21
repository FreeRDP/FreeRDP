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

#include <freerdp/utils/debug.h>
#include <freerdp/utils/msusb.h>

#ifdef WITH_DEBUG_MSUSB
#define DEBUG_MSUSB(fmt, ...) DEBUG_CLASS(MSUSB, fmt, ## __VA_ARGS__)
#else
#define DEBUG_MSUSB(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif


static MSUSB_PIPE_DESCRIPTOR* msusb_mspipe_new()
{
	MSUSB_PIPE_DESCRIPTOR* MsPipe = (MSUSB_PIPE_DESCRIPTOR*) malloc(sizeof(MSUSB_PIPE_DESCRIPTOR));
	memset(MsPipe, 0, sizeof(MSUSB_PIPE_DESCRIPTOR));
	return MsPipe;
}

static void msusb_mspipes_free(MSUSB_PIPE_DESCRIPTOR** MsPipes, UINT32 NumberOfPipes)
{
	int pnum = 0;

	if (MsPipes)
	{
		for (pnum = 0; pnum < NumberOfPipes && MsPipes[pnum]; pnum++)
		{
				zfree(MsPipes[pnum]);
		}
		zfree(MsPipes);
	}
}

void msusb_mspipes_replace(MSUSB_INTERFACE_DESCRIPTOR* MsInterface, MSUSB_PIPE_DESCRIPTOR** NewMsPipes, UINT32 NewNumberOfPipes)
{
	/* free orignal MsPipes */
	msusb_mspipes_free(MsInterface->MsPipes, MsInterface->NumberOfPipes);
	/* And replace it */
	MsInterface->MsPipes = NewMsPipes;
	MsInterface->NumberOfPipes = NewNumberOfPipes;
}

static MSUSB_PIPE_DESCRIPTOR** msusb_mspipes_read(BYTE* data, UINT32 data_size, UINT32 NumberOfPipes, int* offset)
{
	int pnum, move = 0;
	MSUSB_PIPE_DESCRIPTOR** MsPipes;
		
	MsPipes = (MSUSB_PIPE_DESCRIPTOR**) malloc(NumberOfPipes * sizeof(MSUSB_PIPE_DESCRIPTOR*));
	
	for (pnum = 0; pnum < NumberOfPipes; pnum++)
	{
		MSUSB_PIPE_DESCRIPTOR * MsPipe = msusb_mspipe_new();
		
		data_read_UINT16(data + move, MsPipe->MaximumPacketSize);
		data_read_UINT32(data + move + 4, MsPipe->MaximumTransferSize);
		data_read_UINT32(data + move + 8, MsPipe->PipeFlags);
		move += 12;
/* Already set to zero by memset
		MsPipe->PipeHandle	   = 0;
		MsPipe->bEndpointAddress = 0;
		MsPipe->bInterval		= 0;
		MsPipe->PipeType		 = 0;
		MsPipe->InitCompleted	= 0;
*/
		
		MsPipes[pnum] = MsPipe;
	} 
	*offset += move;
	
	return MsPipes;
}

static MSUSB_INTERFACE_DESCRIPTOR* msusb_msinterface_new()
{
	MSUSB_INTERFACE_DESCRIPTOR* MsInterface = (MSUSB_INTERFACE_DESCRIPTOR*) malloc(sizeof(MSUSB_INTERFACE_DESCRIPTOR));
	memset(MsInterface, 0, sizeof(MSUSB_INTERFACE_DESCRIPTOR));
	return MsInterface;
}

static void msusb_msinterface_free(MSUSB_INTERFACE_DESCRIPTOR* MsInterface)
{
	if (MsInterface)
	{
		msusb_mspipes_free(MsInterface->MsPipes, MsInterface->NumberOfPipes);
		MsInterface->MsPipes = NULL;
		zfree(MsInterface);
	}
}

static void msusb_msinterface_free_list(MSUSB_INTERFACE_DESCRIPTOR** MsInterfaces, UINT32 NumInterfaces)
{
	int inum = 0;

	if (MsInterfaces)
	{
		for (inum = 0; inum < NumInterfaces; inum++)
		{
			msusb_msinterface_free(MsInterfaces[inum]);
		}

		zfree(MsInterfaces);
	}
}

void msusb_msinterface_replace(MSUSB_CONFIG_DESCRIPTOR* MsConfig, BYTE InterfaceNumber, MSUSB_INTERFACE_DESCRIPTOR* NewMsInterface)
{
	msusb_msinterface_free(MsConfig->MsInterfaces[InterfaceNumber]);
	MsConfig->MsInterfaces[InterfaceNumber] = NewMsInterface;	
}

MSUSB_INTERFACE_DESCRIPTOR* msusb_msinterface_read(BYTE* data, UINT32 data_size, int* offset)
{
	MSUSB_INTERFACE_DESCRIPTOR* MsInterface;
	
	MsInterface = msusb_msinterface_new();
	
	data_read_UINT16(data, MsInterface->Length);
	data_read_UINT16(data + 2, MsInterface->NumberOfPipesExpected);
	data_read_BYTE(data + 4, MsInterface->InterfaceNumber);
	data_read_BYTE(data + 5, MsInterface->AlternateSetting);
	data_read_UINT32(data + 8, MsInterface->NumberOfPipes);
	*offset += 12;

	MsInterface->InterfaceHandle = 0;
	MsInterface->bInterfaceClass = 0;
	MsInterface->bInterfaceSubClass = 0;
	MsInterface->bInterfaceProtocol = 0;
	MsInterface->InitCompleted = 0;
	MsInterface->MsPipes = NULL;
	
	if (MsInterface->NumberOfPipes > 0)
	{
		MsInterface->MsPipes = 
			msusb_mspipes_read(data+(*offset), data_size-(*offset), MsInterface->NumberOfPipes, offset);
	}
	
	return MsInterface;
}

int msusb_msinterface_write(MSUSB_INTERFACE_DESCRIPTOR* MsInterface, BYTE* data, int* offset)
{
	MSUSB_PIPE_DESCRIPTOR ** MsPipes;
	MSUSB_PIPE_DESCRIPTOR * MsPipe;
	int pnum = 0, move = 0;
	
	/* Length */
	data_write_UINT16(data, MsInterface->Length); 
	/* InterfaceNumber */
	data_write_BYTE(data + 2, MsInterface->InterfaceNumber); 
	/* AlternateSetting */
	data_write_BYTE(data + 3, MsInterface->AlternateSetting); 
	/* bInterfaceClass */
	data_write_BYTE(data + 4, MsInterface->bInterfaceClass); 
	/* bInterfaceSubClass */
	data_write_BYTE(data + 5, MsInterface->bInterfaceSubClass); 
	/* bInterfaceProtocol */
	data_write_BYTE(data + 6, MsInterface->bInterfaceProtocol);
	/* Padding */ 
	data_write_BYTE(data + 7, 0);
	/* InterfaceHandle */
	data_write_UINT32(data + 8, MsInterface->InterfaceHandle);
	/* NumberOfPipes */
	data_write_UINT32(data + 12, MsInterface->NumberOfPipes);
	move += 16;
	/* Pipes */
	MsPipes = MsInterface->MsPipes;
	for(pnum = 0; pnum < MsInterface->NumberOfPipes; pnum++)
	{
		MsPipe = MsPipes[pnum];
		/* MaximumPacketSize */
		data_write_UINT16(data + move, MsPipe->MaximumPacketSize);
		/* EndpointAddress */
		data_write_BYTE(data + move + 2, MsPipe->bEndpointAddress);
		/* Interval */
		data_write_BYTE(data + move + 3, MsPipe->bInterval);
		/* PipeType */
		data_write_UINT32(data + move + 4, MsPipe->PipeType);
		/* PipeHandle */
		data_write_UINT32(data + move + 8, MsPipe->PipeHandle);
		/* MaximumTransferSize */
		data_write_UINT32(data + move + 12, MsPipe->MaximumTransferSize);
		/* PipeFlags */
		data_write_UINT32(data + move + 16, MsPipe->PipeFlags);
		
		move += 20;
	}
	
	*offset += move;
	
	return 0;
}

static MSUSB_INTERFACE_DESCRIPTOR** msusb_msinterface_read_list(BYTE * data, UINT32 data_size, UINT32 NumInterfaces)
{
	int inum, offset = 0;
	MSUSB_INTERFACE_DESCRIPTOR** MsInterfaces;
	
	MsInterfaces = (MSUSB_INTERFACE_DESCRIPTOR**) malloc(NumInterfaces * sizeof(MSUSB_INTERFACE_DESCRIPTOR*));
	
	for (inum = 0; inum < NumInterfaces; inum++)
	{
		MsInterfaces[inum] = msusb_msinterface_read(data + offset, data_size - offset, &offset);
	}   
	
	return MsInterfaces;
}

int msusb_msconfig_write(MSUSB_CONFIG_DESCRIPTOR* MsConfg, BYTE* data, int* offset)
{
	int inum = 0;
	MSUSB_INTERFACE_DESCRIPTOR** MsInterfaces;
	MSUSB_INTERFACE_DESCRIPTOR* MsInterface;
	
	/* ConfigurationHandle*/
	data_write_UINT32(data + *offset, MsConfg->ConfigurationHandle);

	/* NumInterfaces*/
	data_write_UINT32(data + *offset + 4, MsConfg->NumInterfaces); 
	*offset += 8;

	/* Interfaces */

	MsInterfaces = MsConfg->MsInterfaces;

	for(inum = 0; inum < MsConfg->NumInterfaces; inum++)
	{
		MsInterface = MsInterfaces[inum];
		msusb_msinterface_write(MsInterface, data + (*offset), offset);
	}
	
	return 0;
}

MSUSB_CONFIG_DESCRIPTOR* msusb_msconfig_new()
{
	MSUSB_CONFIG_DESCRIPTOR* MsConfig = NULL;
	MsConfig = (MSUSB_CONFIG_DESCRIPTOR*) malloc(sizeof(MSUSB_CONFIG_DESCRIPTOR));
	memset(MsConfig, 0, sizeof(MSUSB_CONFIG_DESCRIPTOR));
	
	return MsConfig;
}

void msusb_msconfig_free(MSUSB_CONFIG_DESCRIPTOR* MsConfig)
{
	if (MsConfig)
	{
		msusb_msinterface_free_list(MsConfig->MsInterfaces, MsConfig->NumInterfaces);
		MsConfig->MsInterfaces = NULL;
		zfree(MsConfig);
	}
}

MSUSB_CONFIG_DESCRIPTOR* msusb_msconfig_read(BYTE* data, UINT32 data_size, UINT32 NumInterfaces)
{
	int i, offset = 0;
	UINT16 lenInterface;
	MSUSB_CONFIG_DESCRIPTOR* MsConfig;
	BYTE lenConfiguration, typeConfiguration;
	
	MsConfig = msusb_msconfig_new();
	
	for(i = 0; i < NumInterfaces; i++)
	{
		data_read_UINT16(data + offset, lenInterface);
		offset += lenInterface;
	}

	data_read_BYTE(data + offset, lenConfiguration);
	data_read_BYTE(data + offset + 1, typeConfiguration);

	if (lenConfiguration != 0x9 || typeConfiguration != 0x2)
	{
		DEBUG_MSUSB("%s: len and type must be 0x9 and 0x2 , but it is 0x%x and 0x%x",
			lenConfiguration, typeConfiguration);
	}

	data_read_UINT16(data + offset + 2, MsConfig->wTotalLength);
	data_read_BYTE(data + offset + 5, MsConfig->bConfigurationValue);

	MsConfig->NumInterfaces	= NumInterfaces;
	MsConfig->ConfigurationHandle = 0;
	MsConfig->InitCompleted = 0;
	MsConfig->MsOutSize = 0;
	MsConfig->MsInterfaces = NULL;
	offset = 0;
	
	if (NumInterfaces > 0)
	{
		MsConfig->MsInterfaces = msusb_msinterface_read_list(data, data_size, NumInterfaces);
	}
		
	return MsConfig;
}

void msusb_msconfig_dump(MSUSB_CONFIG_DESCRIPTOR* MsConfig)
{
	MSUSB_INTERFACE_DESCRIPTOR ** MsInterfaces;
	MSUSB_INTERFACE_DESCRIPTOR * MsInterface;
	MSUSB_PIPE_DESCRIPTOR ** MsPipes;
	MSUSB_PIPE_DESCRIPTOR * MsPipe;
	int inum = 0, pnum = 0;

	fprintf(stderr, "=================MsConfig:========================\n");
	fprintf(stderr, "wTotalLength:%d\n", MsConfig->wTotalLength);
	fprintf(stderr, "bConfigurationValue:%d\n", MsConfig->bConfigurationValue);
	fprintf(stderr, "ConfigurationHandle:0x%x\n", MsConfig->ConfigurationHandle);
	fprintf(stderr, "InitCompleted:%d\n", MsConfig->InitCompleted);
	fprintf(stderr, "MsOutSize:%d\n", MsConfig->MsOutSize);
	fprintf(stderr, "NumInterfaces:%d\n\n", MsConfig->NumInterfaces);
	MsInterfaces = MsConfig->MsInterfaces;
	for(inum = 0; inum < MsConfig->NumInterfaces; inum++)
	{
		MsInterface = MsInterfaces[inum];
		fprintf(stderr, "	Interfase: %d\n", MsInterface->InterfaceNumber);
		fprintf(stderr, "	Length: %d\n", MsInterface->Length);
		fprintf(stderr, "	NumberOfPipesExpected: %d\n", MsInterface->NumberOfPipesExpected);
		fprintf(stderr, "	AlternateSetting: %d\n", MsInterface->AlternateSetting);
		fprintf(stderr, "	NumberOfPipes: %d\n", MsInterface->NumberOfPipes);
		fprintf(stderr, "	InterfaceHandle: 0x%x\n", MsInterface->InterfaceHandle);
		fprintf(stderr, "	bInterfaceClass: 0x%x\n", MsInterface->bInterfaceClass);
		fprintf(stderr, "	bInterfaceSubClass: 0x%x\n", MsInterface->bInterfaceSubClass);
		fprintf(stderr, "	bInterfaceProtocol: 0x%x\n", MsInterface->bInterfaceProtocol);
		fprintf(stderr, "	InitCompleted: %d\n\n", MsInterface->InitCompleted);
		MsPipes = MsInterface->MsPipes;
		for (pnum = 0; pnum < MsInterface->NumberOfPipes; pnum++)
		{
			MsPipe = MsPipes[pnum];
			fprintf(stderr, "		Pipe: %d\n", pnum);
			fprintf(stderr, "		MaximumPacketSize: 0x%x\n", MsPipe->MaximumPacketSize);
			fprintf(stderr, "		MaximumTransferSize: 0x%x\n", MsPipe->MaximumTransferSize);
			fprintf(stderr, "		PipeFlags: 0x%x\n", MsPipe->PipeFlags);
			fprintf(stderr, "		PipeHandle: 0x%x\n", MsPipe->PipeHandle);
			fprintf(stderr, "		bEndpointAddress: 0x%x\n", MsPipe->bEndpointAddress);
			fprintf(stderr, "		bInterval: %d\n", MsPipe->bInterval);
			fprintf(stderr, "		PipeType: 0x%x\n", MsPipe->PipeType);
			fprintf(stderr, "		InitCompleted: %d\n\n", MsPipe->InitCompleted);
		}
	}
	fprintf(stderr, "==================================================\n");
}
