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
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_CHANNEL_URBDRC_CLIENT_MAIN_H
#define FREERDP_CHANNEL_URBDRC_CLIENT_MAIN_H

#include <winpr/pool.h>
#include <freerdp/channels/log.h>
#include <freerdp/client/channels.h>

#define DEVICE_HARDWARE_ID_SIZE 32
#define DEVICE_COMPATIBILITY_ID_SIZE 36
#define DEVICE_INSTANCE_STR_SIZE 37
#define DEVICE_CONTAINER_STR_SIZE 39

#define TAG CHANNELS_TAG("urbdrc.client")
#ifdef WITH_DEBUG_DVC
#define DEBUG_DVC(...) WLog_DBG(TAG, __VA_ARGS__)
#else
#define DEBUG_DVC(...) \
	do                 \
	{                  \
	} while (0)
#endif

typedef struct S_IUDEVICE IUDEVICE;
typedef struct S_IUDEVMAN IUDEVMAN;

#define BASIC_DEV_STATE_DEFINED(_arg, _type) \
	_type (*get_##_arg)(IUDEVICE * pdev);    \
	void (*set_##_arg)(IUDEVICE * pdev, _type _arg)

#define BASIC_DEVMAN_STATE_DEFINED(_arg, _type) \
	_type (*get_##_arg)(IUDEVMAN * udevman);    \
	void (*set_##_arg)(IUDEVMAN * udevman, _type _arg)

typedef struct
{
	IWTSPlugin iface;

	GENERIC_LISTENER_CALLBACK* listener_callback;

	IUDEVMAN* udevman;
	UINT32 vchannel_status;
	char* subsystem;

	wLog* log;
	IWTSListener* listener;
	BOOL initialized;
} URBDRC_PLUGIN;

typedef BOOL (*PREGISTERURBDRCSERVICE)(IWTSPlugin* plugin, IUDEVMAN* udevman);
typedef struct
{
	IWTSPlugin* plugin;
	PREGISTERURBDRCSERVICE pRegisterUDEVMAN;
	const ADDIN_ARGV* args;
} FREERDP_URBDRC_SERVICE_ENTRY_POINTS;
typedef FREERDP_URBDRC_SERVICE_ENTRY_POINTS* PFREERDP_URBDRC_SERVICE_ENTRY_POINTS;

typedef int (*PFREERDP_URBDRC_DEVICE_ENTRY)(PFREERDP_URBDRC_SERVICE_ENTRY_POINTS pEntryPoints);

typedef struct
{
	GENERIC_CHANNEL_CALLBACK* callback;
	URBDRC_PLUGIN* urbdrc;
	IUDEVMAN* udevman;
	IWTSVirtualChannel* channel;
	wStream* s;
} TRANSFER_DATA;

typedef void (*t_isoch_transfer_cb)(IUDEVICE* idev, GENERIC_CHANNEL_CALLBACK* callback,
                                    wStream* out, UINT32 InterfaceId, BOOL noAck, UINT32 MessageId,
                                    UINT32 RequestId, UINT32 NumberOfPackets, UINT32 status,
                                    UINT32 StartFrame, UINT32 ErrorCount, UINT32 OutputBufferSize);

struct S_IUDEVICE
{
	/* Transfer */
	int (*isoch_transfer)(IUDEVICE* idev, GENERIC_CHANNEL_CALLBACK* callback, UINT32 MessageId,
	                      UINT32 RequestId, UINT32 EndpointAddress, UINT32 TransferFlags,
	                      UINT32 StartFrame, UINT32 ErrorCount, BOOL NoAck,
	                      const BYTE* packetDescriptorData, UINT32 NumberOfPackets,
	                      UINT32 BufferSize, const BYTE* Buffer, t_isoch_transfer_cb cb,
	                      UINT32 Timeout);

	BOOL(*control_transfer)
	(IUDEVICE* idev, UINT32 RequestId, UINT32 EndpointAddress, UINT32 TransferFlags,
	 BYTE bmRequestType, BYTE Request, UINT16 Value, UINT16 Index, UINT32* UrbdStatus,
	 UINT32* BufferSize, BYTE* Buffer, UINT32 Timeout);

	int (*bulk_or_interrupt_transfer)(IUDEVICE* idev, GENERIC_CHANNEL_CALLBACK* callback,
	                                  UINT32 MessageId, UINT32 RequestId, UINT32 EndpointAddress,
	                                  UINT32 TransferFlags, BOOL NoAck, UINT32 BufferSize,
	                                  const BYTE* data, t_isoch_transfer_cb cb, UINT32 Timeout);

	int (*select_configuration)(IUDEVICE* idev, UINT32 bConfigurationValue);

	int (*select_interface)(IUDEVICE* idev, BYTE InterfaceNumber, BYTE AlternateSetting);

	int (*control_pipe_request)(IUDEVICE* idev, UINT32 RequestId, UINT32 EndpointAddress,
	                            UINT32* UsbdStatus, int command);

	UINT32(*control_query_device_text)
	(IUDEVICE* idev, UINT32 TextType, UINT16 LocaleId, UINT8* BufferSize, BYTE* Buffer);

	int (*os_feature_descriptor_request)(IUDEVICE* idev, UINT32 RequestId, BYTE Recipient,
	                                     BYTE InterfaceNumber, BYTE Ms_PageIndex,
	                                     UINT16 Ms_featureDescIndex, UINT32* UsbdStatus,
	                                     UINT32* BufferSize, BYTE* Buffer, UINT32 Timeout);

	void (*cancel_all_transfer_request)(IUDEVICE* idev);

	int (*cancel_transfer_request)(IUDEVICE* idev, UINT32 RequestId);

	int (*query_device_descriptor)(IUDEVICE* idev, int offset);

	BOOL (*detach_kernel_driver)(IUDEVICE* idev);

	BOOL (*attach_kernel_driver)(IUDEVICE* idev);

	int (*query_device_port_status)(IUDEVICE* idev, UINT32* UsbdStatus, UINT32* BufferSize,
	                                BYTE* Buffer);

	MSUSB_CONFIG_DESCRIPTOR* (*complete_msconfig_setup)(IUDEVICE* idev,
	                                                    MSUSB_CONFIG_DESCRIPTOR* MsConfig);
	/* Basic state */
	int (*isCompositeDevice)(IUDEVICE* idev);

	int (*isExist)(IUDEVICE* idev);
	int (*isAlreadySend)(IUDEVICE* idev);
	int (*isChannelClosed)(IUDEVICE* idev);

	void (*setAlreadySend)(IUDEVICE* idev);
	void (*setChannelClosed)(IUDEVICE* idev);
	void (*markChannelClosed)(IUDEVICE* idev);
	char* (*getPath)(IUDEVICE* idev);

	void (*free)(IUDEVICE* idev);

	BASIC_DEV_STATE_DEFINED(channelManager, IWTSVirtualChannelManager*);
	BASIC_DEV_STATE_DEFINED(channelID, UINT32);
	BASIC_DEV_STATE_DEFINED(UsbDevice, UINT32);
	BASIC_DEV_STATE_DEFINED(ReqCompletion, UINT32);
	BASIC_DEV_STATE_DEFINED(bus_number, BYTE);
	BASIC_DEV_STATE_DEFINED(dev_number, BYTE);
	BASIC_DEV_STATE_DEFINED(port_number, int);
	BASIC_DEV_STATE_DEFINED(MsConfig, MSUSB_CONFIG_DESCRIPTOR*);

	BASIC_DEV_STATE_DEFINED(p_udev, void*);
	BASIC_DEV_STATE_DEFINED(p_prev, void*);
	BASIC_DEV_STATE_DEFINED(p_next, void*);
};

struct S_IUDEVMAN
{
	/* Standard */
	void (*free)(IUDEVMAN* idevman);

	/* Manage devices */
	void (*rewind)(IUDEVMAN* idevman);
	BOOL (*has_next)(IUDEVMAN* idevman);
	BOOL (*unregister_udevice)(IUDEVMAN* idevman, BYTE bus_number, BYTE dev_number);
	size_t (*register_udevice)(IUDEVMAN* idevman, BYTE bus_number, BYTE dev_number, UINT16 idVendor,
	                           UINT16 idProduct, UINT32 flag);
	IUDEVICE* (*get_next)(IUDEVMAN* idevman);
	IUDEVICE* (*get_udevice_by_UsbDevice)(IUDEVMAN* idevman, UINT32 UsbDevice);
	IUDEVICE* (*get_udevice_by_ChannelID)(IUDEVMAN* idevman, UINT32 channelID);

	/* Extension */
	int (*isAutoAdd)(IUDEVMAN* idevman);

	/* Basic state */
	BASIC_DEVMAN_STATE_DEFINED(device_num, UINT32);
	BASIC_DEVMAN_STATE_DEFINED(next_device_id, UINT32);

	/* control semaphore or mutex lock */
	void (*loading_lock)(IUDEVMAN* idevman);
	void (*loading_unlock)(IUDEVMAN* idevman);
	BOOL (*initialize)(IUDEVMAN* idevman, UINT32 channelId);
	UINT (*listener_created_callback)(IUDEVMAN* idevman);

	IWTSPlugin* plugin;
	UINT32 controlChannelId;
	UINT32 status;
};

#define DEVICE_ADD_FLAG_BUS 0x01
#define DEVICE_ADD_FLAG_DEV 0x02
#define DEVICE_ADD_FLAG_VENDOR 0x04
#define DEVICE_ADD_FLAG_PRODUCT 0x08
#define DEVICE_ADD_FLAG_REGISTER 0x10

#define DEVICE_ADD_FLAG_ALL                                               \
	(DEVICE_ADD_FLAG_BUS | DEVICE_ADD_FLAG_DEV | DEVICE_ADD_FLAG_VENDOR | \
	 DEVICE_ADD_FLAG_PRODUCT | DEVICE_ADD_FLAG_REGISTER)

FREERDP_API BOOL add_device(IUDEVMAN* idevman, UINT32 flags, BYTE busnum, BYTE devnum,
                            UINT16 idVendor, UINT16 idProduct);
FREERDP_API BOOL del_device(IUDEVMAN* idevman, UINT32 flags, BYTE busnum, BYTE devnum,
                            UINT16 idVendor, UINT16 idProduct);

UINT stream_write_and_free(IWTSPlugin* plugin, IWTSVirtualChannel* channel, wStream* s);

#endif /* FREERDP_CHANNEL_URBDRC_CLIENT_MAIN_H */
