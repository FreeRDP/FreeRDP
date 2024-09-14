/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MS-RDPECAM Implementation, Device Enumeration Channel
 *
 * Copyright 2024 Oleg Turovski <oleg2104@hotmail.com>
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

#include <winpr/assert.h>

#include "camera.h"

#define TAG CHANNELS_TAG("rdpecam-enum.client")

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT ecam_channel_send_error_response(CameraPlugin* ecam, GENERIC_CHANNEL_CALLBACK* hchannel,
                                      CAM_ERROR_CODE code)
{
	CAM_MSG_ID msg = CAM_MSG_ID_ErrorResponse;

	WINPR_ASSERT(ecam);

	wStream* s = Stream_New(NULL, CAM_HEADER_SIZE + 4);
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	Stream_Write_UINT8(s, ecam->version);
	Stream_Write_UINT8(s, msg);
	Stream_Write_UINT32(s, code);

	return ecam_channel_write(ecam, hchannel, msg, s, TRUE);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT ecam_channel_send_generic_msg(CameraPlugin* ecam, GENERIC_CHANNEL_CALLBACK* hchannel,
                                   CAM_MSG_ID msg)
{
	WINPR_ASSERT(ecam);

	wStream* s = Stream_New(NULL, CAM_HEADER_SIZE);
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	Stream_Write_UINT8(s, ecam->version);
	Stream_Write_UINT8(s, msg);

	return ecam_channel_write(ecam, hchannel, msg, s, TRUE);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
UINT ecam_channel_write(CameraPlugin* ecam, GENERIC_CHANNEL_CALLBACK* hchannel, CAM_MSG_ID msg,
                        wStream* out, BOOL freeStream)
{
	if (!hchannel || !out)
		return ERROR_INVALID_PARAMETER;

	Stream_SealLength(out);
	WINPR_ASSERT(Stream_Length(out) <= UINT32_MAX);

	WLog_DBG(TAG, "ChannelId=%d, MessageId=0x%02" PRIx8 ", Length=%d",
	         hchannel->channel_mgr->GetChannelId(hchannel->channel), msg, Stream_Length(out));

	const UINT error = hchannel->channel->Write(hchannel->channel, (ULONG)Stream_Length(out),
	                                            Stream_Buffer(out), NULL);

	if (freeStream)
		Stream_Free(out, TRUE);

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_send_device_added_notification(CameraPlugin* ecam,
                                                GENERIC_CHANNEL_CALLBACK* hchannel,
                                                const char* deviceName, const char* channelName)
{
	CAM_MSG_ID msg = CAM_MSG_ID_DeviceAddedNotification;

	WINPR_ASSERT(ecam);

	wStream* s = Stream_New(NULL, 256);
	if (!s)
	{
		WLog_ERR(TAG, "Stream_New failed!");
		return ERROR_NOT_ENOUGH_MEMORY;
	}

	Stream_Write_UINT8(s, ecam->version);
	Stream_Write_UINT8(s, msg);

	size_t devNameLen = strlen(deviceName);
	if (Stream_Write_UTF16_String_From_UTF8(s, devNameLen + 1, deviceName, devNameLen, TRUE) < 0)
	{
		Stream_Free(s, TRUE);
		return ERROR_INTERNAL_ERROR;
	}
	Stream_Write(s, channelName, strlen(channelName) + 1);

	return ecam_channel_write(ecam, hchannel, msg, s, TRUE);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_ihal_device_added_callback(CameraPlugin* ecam, GENERIC_CHANNEL_CALLBACK* hchannel,
                                            const char* deviceId, const char* deviceName)
{
	WLog_DBG(TAG, "deviceId=%s, deviceName=%s", deviceId, deviceName);

	if (!HashTable_ContainsKey(ecam->devices, deviceId))
	{
		CameraDevice* dev = ecam_dev_create(ecam, deviceId, deviceName);
		if (!HashTable_Insert(ecam->devices, deviceId, dev))
		{
			ecam_dev_destroy(dev);
			return ERROR_INTERNAL_ERROR;
		}
	}
	else
	{
		WLog_DBG(TAG, "Device %s already exists", deviceId);
	}

	ecam_send_device_added_notification(ecam, hchannel, deviceName, deviceId /*channelName*/);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_enumerate_devices(CameraPlugin* ecam, GENERIC_CHANNEL_CALLBACK* hchannel)
{
	ecam->ihal->Enumerate(ecam->ihal, ecam_ihal_device_added_callback, ecam, hchannel);

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_process_select_version_response(CameraPlugin* ecam,
                                                 GENERIC_CHANNEL_CALLBACK* hchannel, wStream* s,
                                                 BYTE serverVersion)
{
	const BYTE clientVersion = ECAM_PROTO_VERSION;

	/* check remaining s capacity */

	WLog_DBG(TAG, "ServerVersion=%" PRIu8 ", ClientVersion=%" PRIu8, serverVersion, clientVersion);

	if (serverVersion > clientVersion)
	{
		WLog_ERR(TAG,
		         "Incompatible protocol version server=%" PRIu8 ", client supports version=%" PRIu8,
		         serverVersion, clientVersion);
		return CHANNEL_RC_OK;
	}
	ecam->version = serverVersion;

	if (ecam->ihal)
		ecam_enumerate_devices(ecam, hchannel);
	else
		WLog_ERR(TAG, "No HAL registered");

	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_on_data_received(IWTSVirtualChannelCallback* pChannelCallback, wStream* data)
{
	UINT error = CHANNEL_RC_OK;
	BYTE version = 0;
	BYTE messageId = 0;
	GENERIC_CHANNEL_CALLBACK* hchannel = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;

	if (!hchannel || !data)
		return ERROR_INVALID_PARAMETER;

	CameraPlugin* ecam = (CameraPlugin*)hchannel->plugin;

	if (!ecam)
		return ERROR_INTERNAL_ERROR;

	if (!Stream_CheckAndLogRequiredCapacity(TAG, data, CAM_HEADER_SIZE))
		return ERROR_NO_DATA;

	Stream_Read_UINT8(data, version);
	Stream_Read_UINT8(data, messageId);
	WLog_DBG(TAG, "ChannelId=%d, MessageId=0x%02" PRIx8 ", Version=%d",
	         hchannel->channel_mgr->GetChannelId(hchannel->channel), messageId, version);

	switch (messageId)
	{
		case CAM_MSG_ID_SelectVersionResponse:
			error = ecam_process_select_version_response(ecam, hchannel, data, version);
			break;

		default:
			WLog_WARN(TAG, "unknown MessageId=0x%02" PRIx8 "", messageId);
			error = ERROR_INVALID_DATA;
			ecam_channel_send_error_response(ecam, hchannel, CAM_ERROR_CODE_OperationNotSupported);
			break;
	}

	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_on_open(IWTSVirtualChannelCallback* pChannelCallback)
{
	GENERIC_CHANNEL_CALLBACK* hchannel = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;
	WINPR_ASSERT(hchannel);

	CameraPlugin* ecam = (CameraPlugin*)hchannel->plugin;
	WINPR_ASSERT(ecam);

	WLog_DBG(TAG, "entered");
	return ecam_channel_send_generic_msg(ecam, hchannel, CAM_MSG_ID_SelectVersionRequest);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_on_close(IWTSVirtualChannelCallback* pChannelCallback)
{
	GENERIC_CHANNEL_CALLBACK* hchannel = (GENERIC_CHANNEL_CALLBACK*)pChannelCallback;
	WINPR_ASSERT(hchannel);

	WLog_DBG(TAG, "entered");

	free(hchannel);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_on_new_channel_connection(IWTSListenerCallback* pListenerCallback,
                                           IWTSVirtualChannel* pChannel, BYTE* Data, BOOL* pbAccept,
                                           IWTSVirtualChannelCallback** ppCallback)
{
	GENERIC_LISTENER_CALLBACK* hlistener = (GENERIC_LISTENER_CALLBACK*)pListenerCallback;

	if (!hlistener || !hlistener->plugin)
		return ERROR_INTERNAL_ERROR;

	WLog_DBG(TAG, "entered");
	GENERIC_CHANNEL_CALLBACK* hchannel =
	    (GENERIC_CHANNEL_CALLBACK*)calloc(1, sizeof(GENERIC_CHANNEL_CALLBACK));

	if (!hchannel)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	hchannel->iface.OnDataReceived = ecam_on_data_received;
	hchannel->iface.OnOpen = ecam_on_open;
	hchannel->iface.OnClose = ecam_on_close;
	hchannel->plugin = hlistener->plugin;
	hchannel->channel_mgr = hlistener->channel_mgr;
	hchannel->channel = pChannel;
	*ppCallback = (IWTSVirtualChannelCallback*)hchannel;
	return CHANNEL_RC_OK;
}

static void ecam_dev_destroy_pv(void* obj)
{
	CameraDevice* dev = obj;
	ecam_dev_destroy(dev);
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_plugin_initialize(IWTSPlugin* pPlugin, IWTSVirtualChannelManager* pChannelMgr)
{
	CameraPlugin* ecam = (CameraPlugin*)pPlugin;

	WLog_DBG(TAG, "entered");

	if (!ecam || !pChannelMgr)
		return ERROR_INVALID_PARAMETER;

	if (ecam->initialized)
	{
		WLog_ERR(TAG, "[%s] plugin initialized twice, aborting", RDPECAM_CONTROL_DVC_CHANNEL_NAME);
		return ERROR_INVALID_DATA;
	}

	ecam->version = ECAM_PROTO_VERSION;

	ecam->devices = HashTable_New(FALSE);
	if (!ecam->devices)
	{
		WLog_ERR(TAG, "HashTable_New failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	HashTable_SetupForStringData(ecam->devices, FALSE);

	wObject* obj = HashTable_ValueObject(ecam->devices);
	WINPR_ASSERT(obj);
	obj->fnObjectFree = ecam_dev_destroy_pv;

	ecam->hlistener = (GENERIC_LISTENER_CALLBACK*)calloc(1, sizeof(GENERIC_LISTENER_CALLBACK));

	if (!ecam->hlistener)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	ecam->hlistener->iface.OnNewChannelConnection = ecam_on_new_channel_connection;
	ecam->hlistener->plugin = pPlugin;
	ecam->hlistener->channel_mgr = pChannelMgr;
	const UINT rc = pChannelMgr->CreateListener(pChannelMgr, RDPECAM_CONTROL_DVC_CHANNEL_NAME, 0,
	                                            &ecam->hlistener->iface, &ecam->listener);

	ecam->initialized = (rc == CHANNEL_RC_OK);
	return rc;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_plugin_terminated(IWTSPlugin* pPlugin)
{
	CameraPlugin* ecam = (CameraPlugin*)pPlugin;

	if (!ecam)
		return ERROR_INVALID_DATA;

	WLog_DBG(TAG, "entered");

	if (ecam->hlistener)
	{
		IWTSVirtualChannelManager* mgr = ecam->hlistener->channel_mgr;
		if (mgr)
			IFCALL(mgr->DestroyListener, mgr, ecam->listener);
	}

	free(ecam->hlistener);

	HashTable_Free(ecam->devices);

	if (ecam->ihal)
		ecam->ihal->Free(ecam->ihal);

	free(ecam);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_plugin_attached(IWTSPlugin* pPlugin)
{
	CameraPlugin* ecam = (CameraPlugin*)pPlugin;
	UINT error = CHANNEL_RC_OK;

	if (!ecam)
		return ERROR_INVALID_PARAMETER;

	ecam->attached = TRUE;
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_plugin_detached(IWTSPlugin* pPlugin)
{
	CameraPlugin* ecam = (CameraPlugin*)pPlugin;
	UINT error = CHANNEL_RC_OK;

	if (!ecam)
		return ERROR_INVALID_PARAMETER;

	ecam->attached = FALSE;
	return error;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_register_hal_plugin(IWTSPlugin* pPlugin, ICamHal* ihal)
{
	CameraPlugin* ecam = (CameraPlugin*)pPlugin;

	WINPR_ASSERT(ecam);

	if (ecam->ihal)
	{
		WLog_DBG(TAG, "already registered");
		return ERROR_ALREADY_EXISTS;
	}

	WLog_DBG(TAG, "HAL registered");
	ecam->ihal = ihal;
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
static UINT ecam_load_hal_plugin(CameraPlugin* ecam, const char* name, const ADDIN_ARGV* args)
{
	WINPR_ASSERT(ecam);

	FREERDP_CAMERA_HAL_ENTRY_POINTS entryPoints = { 0 };
	UINT error = ERROR_INTERNAL_ERROR;
	union
	{
		PVIRTUALCHANNELENTRY pvce;
		const PFREERDP_CAMERA_HAL_ENTRY entry;
	} cnv;
	cnv.pvce = freerdp_load_channel_addin_entry(RDPECAM_CHANNEL_NAME, name, NULL, 0);

	if (cnv.entry == NULL)
	{
		WLog_ERR(TAG,
		         "freerdp_load_channel_addin_entry did not return any function pointers for %s ",
		         name);
		return ERROR_INVALID_FUNCTION;
	}

	entryPoints.plugin = &ecam->iface;
	entryPoints.pRegisterCameraHal = ecam_register_hal_plugin;
	entryPoints.args = args;
	entryPoints.ecam = ecam;

	error = cnv.entry(&entryPoints);
	if (error)
	{
		WLog_ERR(TAG, "%s entry returned error %" PRIu32 ".", name, error);
		return error;
	}

	WLog_INFO(TAG, "Loaded %s HAL for ecam", name);
	return CHANNEL_RC_OK;
}

/**
 * Function description
 *
 * @return 0 on success, otherwise a Win32 error code
 */
FREERDP_ENTRY_POINT(UINT VCAPITYPE rdpecam_DVCPluginEntry(IDRDYNVC_ENTRY_POINTS* pEntryPoints))
{
	UINT error = CHANNEL_RC_INITIALIZATION_ERROR;

	WINPR_ASSERT(pEntryPoints);
	WINPR_ASSERT(pEntryPoints->GetPlugin);
	CameraPlugin* ecam = (CameraPlugin*)pEntryPoints->GetPlugin(pEntryPoints, RDPECAM_CHANNEL_NAME);

	if (ecam != NULL)
		return CHANNEL_RC_ALREADY_INITIALIZED;

	ecam = (CameraPlugin*)calloc(1, sizeof(CameraPlugin));

	if (!ecam)
	{
		WLog_ERR(TAG, "calloc failed!");
		return CHANNEL_RC_NO_MEMORY;
	}

	ecam->attached = TRUE;
	ecam->iface.Initialize = ecam_plugin_initialize;
	ecam->iface.Connected = NULL; /* server connects to client */
	ecam->iface.Disconnected = NULL;
	ecam->iface.Terminated = ecam_plugin_terminated;
	ecam->iface.Attached = ecam_plugin_attached;
	ecam->iface.Detached = ecam_plugin_detached;

	/* TODO: camera redirect only supported for platforms with Video4Linux */
#if defined(WITH_V4L)
	ecam->subsystem = "v4l";
#else
	ecam->subsystem = NULL;
#endif

	if (ecam->subsystem)
	{
		if ((error = ecam_load_hal_plugin(ecam, ecam->subsystem, NULL /*args*/)))
		{
			WLog_ERR(TAG,
			         "Unable to load camera redirection subsystem %s because of error %" PRIu32 "",
			         ecam->subsystem, error);
			goto out;
		}
	}

	error = pEntryPoints->RegisterPlugin(pEntryPoints, RDPECAM_CHANNEL_NAME, &ecam->iface);
	if (error == CHANNEL_RC_OK)
		return error;

out:
	ecam_plugin_terminated(&ecam->iface);
	return error;
}
