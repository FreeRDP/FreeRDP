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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/sysinfo.h>
#include <winpr/cast.h>

#include <urbdrc_helpers.h>

#include "urbdrc_types.h"
#include "data_transfer.h"
#include "msusb.h"

static void usb_process_get_port_status(IUDEVICE* pdev, wStream* out)
{
	int bcdUSB = pdev->query_device_descriptor(pdev, BCD_USB);

	switch (bcdUSB)
	{
		case USB_v1_0:
			Stream_Write_UINT32(out, 0x303);
			break;

		case USB_v1_1:
			Stream_Write_UINT32(out, 0x103);
			break;

		case USB_v2_0:
		default:
			Stream_Write_UINT32(out, 0x503);
			break;
	}
}

/* [MS-RDPEUSB] 2.2.10.1.1TS_URB_RESULT_HEADER */
static BOOL write_urb_result_header(wStream* s, UINT16 Size, UINT32 status)
{
	if (!Stream_EnsureRemainingCapacity(s, 8ULL + Size))
		return FALSE;
	Stream_Write_UINT16(s, Size);
	Stream_Seek_UINT16(s);
	Stream_Write_UINT32(s, status);
	return TRUE;
}

/* [MS-RDPEUSB] 2.2.7.2 URB Completion (URB_COMPLETION)
 * 2.2.7.3 URB Completion No Data (URB_COMPLETION_NO_DATA)
 */
static wStream* create_urb_completion_message(UINT32 InterfaceId, UINT32 MessageId,
                                              UINT32 RequestId, UINT32 FunctionId)
{
	wStream* out =
	    create_shared_message_header_with_functionid(InterfaceId, MessageId, FunctionId, 4);
	if (!out)
		return NULL;

	Stream_Write_UINT32(out, RequestId);
	return out;
}

static UINT send_urb_completion_message(GENERIC_CHANNEL_CALLBACK* callback, wStream* out,
                                        HRESULT hResult, UINT32 OutputSize, const void* data)
{
	WINPR_ASSERT(callback);
	UINT status = ERROR_OUTOFMEMORY;

	if (!Stream_EnsureRemainingCapacity(out, 8ULL + OutputSize))
		goto fail;

	Stream_Write_INT32(out, hResult);
	Stream_Write_UINT32(out, OutputSize);
	Stream_Write(out, data, OutputSize);
	return stream_write_and_free(callback->plugin, callback->channel, out);

fail:
	Stream_Free(out, TRUE);
	return status;
}

static UINT urb_write_completion(WINPR_ATTR_UNUSED IUDEVICE* pdev,
                                 GENERIC_CHANNEL_CALLBACK* callback, BOOL noAck, wStream* out,
                                 UINT32 InterfaceId, UINT32 MessageId, UINT32 RequestId,
                                 UINT32 usbd_status, UINT32 OutputBufferSize)
{
	if (!out)
		return ERROR_INVALID_PARAMETER;

	if (Stream_Capacity(out) < OutputBufferSize + 36)
	{
		Stream_Free(out, TRUE);
		return ERROR_INVALID_PARAMETER;
	}

	Stream_SetPosition(out, 0);

	const UINT32 FunctionId = (OutputBufferSize != 0) ? URB_COMPLETION : URB_COMPLETION_NO_DATA;
	if (!write_shared_message_header_with_functionid(out, InterfaceId, MessageId, FunctionId))
	{
		Stream_Free(out, TRUE);
		return ERROR_OUTOFMEMORY;
	}

	Stream_Write_UINT32(out, RequestId); /** RequestId */
	Stream_Write_UINT32(out, 8);         /** CbTsUrbResult */

	if (!write_urb_result_header(out, 8, usbd_status))
	{
		Stream_Free(out, TRUE);
		return ERROR_OUTOFMEMORY;
	}

	Stream_Write_UINT32(out, 0);                /** HResult */
	Stream_Write_UINT32(out, OutputBufferSize); /** OutputBufferSize */
	Stream_Seek(out, OutputBufferSize);

	if (!noAck)
		return stream_write_and_free(callback->plugin, callback->channel, out);
	else
		Stream_Free(out, TRUE);

	return ERROR_SUCCESS;
}

static wStream* urb_create_iocompletion(UINT32 InterfaceField, UINT32 MessageId, UINT32 RequestId,
                                        UINT32 OutputBufferSize)
{
	const UINT32 InterfaceId = (STREAM_ID_PROXY << 30) | (InterfaceField & 0x3FFFFFFF);

#if UINT32_MAX >= SIZE_MAX
	if (OutputBufferSize > UINT32_MAX - 28ull)
		return NULL;
#endif

	wStream* out = create_shared_message_header_with_functionid(
	    InterfaceId, MessageId, IOCONTROL_COMPLETION, OutputBufferSize + 16ull);
	if (!out)
		return NULL;

	Stream_Write_UINT32(out, RequestId);            /** RequestId */
	Stream_Write_UINT32(out, USBD_STATUS_SUCCESS);  /** HResult */
	Stream_Write_UINT32(out, OutputBufferSize);     /** Information */
	Stream_Write_UINT32(out, OutputBufferSize);     /** OutputBufferSize */
	return out;
}

static UINT urbdrc_process_register_request_callback(IUDEVICE* pdev,
                                                     GENERIC_CHANNEL_CALLBACK* callback, wStream* s,
                                                     IUDEVMAN* udevman)
{
	UINT32 NumRequestCompletion = 0;
	UINT32 RequestCompletion = 0;

	if (!callback || !s || !udevman || !pdev)
		return ERROR_INVALID_PARAMETER;

	URBDRC_PLUGIN* urbdrc = (URBDRC_PLUGIN*)callback->plugin;

	if (!urbdrc)
		return ERROR_INVALID_PARAMETER;

	WLog_Print(urbdrc->log, WLOG_DEBUG, "urbdrc_process_register_request_callback");

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4ULL))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, NumRequestCompletion); /** must be 1 */

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4ULL * NumRequestCompletion))
		return ERROR_INVALID_DATA;
	for (uint32_t x = 0; x < NumRequestCompletion; x++)
	{
		/** RequestCompletion:
		 *   unique Request Completion interface for the client to use */
		Stream_Read_UINT32(s, RequestCompletion);
		pdev->set_ReqCompletion(pdev, RequestCompletion);
	}

	return ERROR_SUCCESS;
}

static UINT urbdrc_process_cancel_request(IUDEVICE* pdev, wStream* s, IUDEVMAN* udevman)
{
	UINT32 CancelId = 0;
	URBDRC_PLUGIN* urbdrc = NULL;

	if (!s || !udevman || !pdev)
		return ERROR_INVALID_PARAMETER;

	urbdrc = (URBDRC_PLUGIN*)udevman->plugin;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, CancelId);
	WLog_Print(urbdrc->log, WLOG_DEBUG, "CANCEL_REQUEST: CancelId=%08" PRIx32 "", CancelId);

	if (pdev->cancel_transfer_request(pdev, CancelId) < 0)
		return ERROR_INTERNAL_ERROR;

	return ERROR_SUCCESS;
}

static UINT urbdrc_process_retract_device_request(WINPR_ATTR_UNUSED IUDEVICE* pdev, wStream* s,
                                                  IUDEVMAN* udevman)
{
	UINT32 Reason = 0;
	URBDRC_PLUGIN* urbdrc = NULL;

	if (!s || !udevman)
		return ERROR_INVALID_PARAMETER;

	urbdrc = (URBDRC_PLUGIN*)udevman->plugin;

	if (!urbdrc)
		return ERROR_INVALID_PARAMETER;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, Reason); /** Reason */

	switch (Reason)
	{
		case UsbRetractReason_BlockedByPolicy:
			WLog_Print(urbdrc->log, WLOG_DEBUG,
			           "UsbRetractReason_BlockedByPolicy: now it is not support");
			return ERROR_ACCESS_DENIED;

		default:
			WLog_Print(urbdrc->log, WLOG_DEBUG,
			           "urbdrc_process_retract_device_request: Unknown Reason %" PRIu32 "", Reason);
			return ERROR_ACCESS_DENIED;
	}

	return ERROR_SUCCESS;
}

static UINT urbdrc_process_io_control(IUDEVICE* pdev, GENERIC_CHANNEL_CALLBACK* callback,
                                      wStream* s, UINT32 MessageId, IUDEVMAN* udevman)
{
	UINT32 InterfaceId = 0;
	UINT32 IoControlCode = 0;
	UINT32 InputBufferSize = 0;
	UINT32 OutputBufferSize = 0;
	UINT32 RequestId = 0;
	UINT32 usbd_status = USBD_STATUS_SUCCESS;
	wStream* out = NULL;
	int success = 0;
	URBDRC_PLUGIN* urbdrc = NULL;

	if (!callback || !s || !udevman || !pdev)
		return ERROR_INVALID_PARAMETER;

	urbdrc = (URBDRC_PLUGIN*)callback->plugin;

	if (!urbdrc)
		return ERROR_INVALID_PARAMETER;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, IoControlCode);
	Stream_Read_UINT32(s, InputBufferSize);

	if (!Stream_SafeSeek(s, InputBufferSize))
		return ERROR_INVALID_DATA;
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8ULL))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, OutputBufferSize);
	Stream_Read_UINT32(s, RequestId);

	if (OutputBufferSize > UINT32_MAX - 4)
		return ERROR_INVALID_DATA;

	InterfaceId = ((STREAM_ID_PROXY << 30) | pdev->get_ReqCompletion(pdev));
	out = urb_create_iocompletion(InterfaceId, MessageId, RequestId, OutputBufferSize + 4);

	if (!out)
		return ERROR_OUTOFMEMORY;

	switch (IoControlCode)
	{
		case IOCTL_INTERNAL_USB_SUBMIT_URB: /** 0x00220003 */
			WLog_Print(urbdrc->log, WLOG_DEBUG, "ioctl: IOCTL_INTERNAL_USB_SUBMIT_URB");
			WLog_Print(urbdrc->log, WLOG_ERROR,
			           " Function IOCTL_INTERNAL_USB_SUBMIT_URB: Unchecked");
			break;

		case IOCTL_INTERNAL_USB_RESET_PORT: /** 0x00220007 */
			WLog_Print(urbdrc->log, WLOG_DEBUG, "ioctl: IOCTL_INTERNAL_USB_RESET_PORT");
			break;

		case IOCTL_INTERNAL_USB_GET_PORT_STATUS: /** 0x00220013 */
			WLog_Print(urbdrc->log, WLOG_DEBUG, "ioctl: IOCTL_INTERNAL_USB_GET_PORT_STATUS");
			success = pdev->query_device_port_status(pdev, &usbd_status, &OutputBufferSize,
			                                         Stream_Pointer(out));

			if (success)
			{
				if (!Stream_SafeSeek(out, OutputBufferSize))
				{
					Stream_Free(out, TRUE);
					return ERROR_INVALID_DATA;
				}

				if (pdev->isExist(pdev) == 0)
					Stream_Write_UINT32(out, 0);
				else
					usb_process_get_port_status(pdev, out);
			}

			break;

		case IOCTL_INTERNAL_USB_CYCLE_PORT: /** 0x0022001F */
			WLog_Print(urbdrc->log, WLOG_DEBUG, "ioctl: IOCTL_INTERNAL_USB_CYCLE_PORT");
			WLog_Print(urbdrc->log, WLOG_ERROR,
			           " Function IOCTL_INTERNAL_USB_CYCLE_PORT: Unchecked");
			break;

		case IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION: /** 0x00220027 */
			WLog_Print(urbdrc->log, WLOG_DEBUG,
			           "ioctl: IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION");
			WLog_Print(urbdrc->log, WLOG_ERROR,
			           " Function IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION: Unchecked");
			break;

		default:
			WLog_Print(urbdrc->log, WLOG_DEBUG,
			           "urbdrc_process_io_control: unknown IoControlCode 0x%" PRIX32 "",
			           IoControlCode);
			Stream_Free(out, TRUE);
			return ERROR_INVALID_OPERATION;
	}

	return stream_write_and_free(callback->plugin, callback->channel, out);
}

static UINT urbdrc_process_internal_io_control(IUDEVICE* pdev, GENERIC_CHANNEL_CALLBACK* callback,
                                               wStream* s, UINT32 MessageId, IUDEVMAN* udevman)
{
	if (!pdev || !callback || !s || !udevman)
		return ERROR_INVALID_PARAMETER;

	URBDRC_PLUGIN* urbdrc = (URBDRC_PLUGIN*)callback->plugin;
	WINPR_ASSERT(urbdrc);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return ERROR_INVALID_DATA;

	const UINT32 IoControlCode = Stream_Get_UINT32(s);
	if (IoControlCode != IOCTL_TSUSBGD_IOCTL_USBDI_QUERY_BUS_TIME)
	{
		WLog_ERR(
		    TAG,
		    "Invalid [MS-RDPEUSB] 2.2.13 USB Internal IO Control Code::IoControlCode0x%08" PRIx32
		    ", must be IOCTL_TSUSBGD_IOCTL_USBDI_QUERY_BUS_TIME [0x00224000]",
		    IoControlCode);
		return ERROR_INVALID_DATA;
	}
	const UINT32 InputBufferSize = Stream_Get_UINT32(s);

	if (!Stream_SafeSeek(s, InputBufferSize))
		return ERROR_INVALID_DATA;
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8ULL))
		return ERROR_INVALID_DATA;
	const UINT32 OutputBufferSize = Stream_Get_UINT32(s);
	const UINT32 RequestId = Stream_Get_UINT32(s);
	const UINT32 InterfaceId = ((STREAM_ID_PROXY << 30) | pdev->get_ReqCompletion(pdev));
	// TODO: Implement control code.
	/** Fixme: Currently this is a FALSE bustime... */
	const UINT32 frames = GetTickCount();

	if (4 > OutputBufferSize)
	{
		WLog_Print(urbdrc->log, WLOG_DEBUG, "out_size %" PRIu32 " > OutputBufferSize %" PRIu32, 4u,
		           OutputBufferSize);
		return ERROR_BAD_CONFIGURATION;
	}
	wStream* out = urb_create_iocompletion(InterfaceId, MessageId, RequestId, 4);

	if (!out)
		return ERROR_OUTOFMEMORY;

	Stream_Write_UINT32(out, frames); /** OutputBuffer */
	return stream_write_and_free(callback->plugin, callback->channel, out);
}

/* [MS-RDPEUSB] 2.2.6.6 Query Device Text Response Message (QUERY_DEVICE_TEXT_RSP) */
static UINT urbdrc_send_query_device_text_response(GENERIC_CHANNEL_CALLBACK* callback,
                                                   UINT32 InterfaceId, UINT32 MessageId, HRESULT hr,
                                                   const BYTE* text, uint8_t bytelen)
{
	WINPR_ASSERT(callback);

	const uint8_t charlen = bytelen / sizeof(WCHAR);
	wStream* out = create_shared_message_header_with_functionid(InterfaceId, MessageId, charlen,
	                                                            8ULL + bytelen);

	if (!out)
		return ERROR_OUTOFMEMORY;

	Stream_Write(out, text, bytelen); /* '\0' terminated unicode */
	Stream_Write_INT32(out, hr);      /** HResult */
	return stream_write_and_free(callback->plugin, callback->channel, out);
}

static UINT urbdrc_process_query_device_text(IUDEVICE* pdev, GENERIC_CHANNEL_CALLBACK* callback,
                                             wStream* s, UINT32 MessageId, IUDEVMAN* udevman)
{
	UINT32 TextType = 0;
	UINT32 LocaleId = 0;
	UINT8 bufferSize = 0xFF;
	BYTE DeviceDescription[0x100] = { 0 };

	if (!pdev || !callback || !s || !udevman)
		return ERROR_INVALID_PARAMETER;
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, TextType);
	Stream_Read_UINT32(s, LocaleId);
	if (LocaleId > UINT16_MAX)
		return ERROR_INVALID_DATA;

	HRESULT hr = (HRESULT)pdev->control_query_device_text(pdev, TextType, (UINT16)LocaleId,
	                                                      &bufferSize, DeviceDescription);
	const UINT32 InterfaceId = ((STREAM_ID_STUB << 30) | pdev->get_UsbDevice(pdev));
	return urbdrc_send_query_device_text_response(callback, InterfaceId, MessageId, hr,
	                                              DeviceDescription, bufferSize);
}

static void func_select_all_interface_for_msconfig(IUDEVICE* pdev,
                                                   MSUSB_CONFIG_DESCRIPTOR* MsConfig)
{
	MSUSB_INTERFACE_DESCRIPTOR** MsInterfaces = MsConfig->MsInterfaces;
	UINT32 NumInterfaces = MsConfig->NumInterfaces;

	for (UINT32 inum = 0; inum < NumInterfaces; inum++)
	{
		const BYTE InterfaceNumber = MsInterfaces[inum]->InterfaceNumber;
		const BYTE AlternateSetting = MsInterfaces[inum]->AlternateSetting;
		pdev->select_interface(pdev, InterfaceNumber, AlternateSetting);
	}
}

/* [MS-RDPEUSB] 2.2.10.2 TS_URB_SELECT_CONFIGURATION_RESULT */
static UINT send_urb_select_configuration_result(GENERIC_CHANNEL_CALLBACK* callback,
                                                 UINT32 InterfaceId, UINT32 MessageId,
                                                 UINT32 RequestId, UINT32 UrbStatus,
                                                 const MSUSB_CONFIG_DESCRIPTOR* MsConfig)
{
	wStream* out =
	    create_urb_completion_message(InterfaceId, MessageId, RequestId, URB_COMPLETION_NO_DATA);
	if (!out)
		return ERROR_OUTOFMEMORY;

	const int size = 8 + ((MsConfig) ? MsConfig->MsOutSize : 8);
	const uint16_t usize = WINPR_ASSERTING_INT_CAST(uint16_t, size);

	if (!Stream_EnsureRemainingCapacity(out, 4))
		goto fail;
	Stream_Write_UINT32(out, usize); /* CbTsUrbResult */

	if (!write_urb_result_header(out, usize, UrbStatus))
		goto fail;

	/** TS_URB_SELECT_CONFIGURATION_RESULT */
	if (MsConfig)
		msusb_msconfig_write(MsConfig, out);
	else
	{
		Stream_Write_UINT32(out, 0); /** ConfigurationHandle */
		Stream_Write_UINT32(out, 0); /** NumInterfaces */
	}

	return send_urb_completion_message(callback, out, 0, 0, NULL);

fail:
	Stream_Free(out, TRUE);
	return ERROR_OUTOFMEMORY;
}

static UINT urb_select_configuration(IUDEVICE* pdev, GENERIC_CHANNEL_CALLBACK* callback, wStream* s,
                                     UINT32 RequestField, UINT32 MessageId, IUDEVMAN* udevman,
                                     int transferDir)
{
	MSUSB_CONFIG_DESCRIPTOR* MsConfig = NULL;
	UINT32 NumInterfaces = 0;
	UINT32 usbd_status = 0;
	BYTE ConfigurationDescriptorIsValid = 0;
	URBDRC_PLUGIN* urbdrc = NULL;
	const BOOL noAck = (RequestField & 0x80000000U) != 0;
	const UINT32 RequestId = RequestField & 0x7FFFFFFF;

	if (!callback || !s || !udevman || !pdev)
		return ERROR_INVALID_PARAMETER;

	urbdrc = (URBDRC_PLUGIN*)callback->plugin;

	if (!urbdrc)
		return ERROR_INVALID_PARAMETER;

	if (transferDir == 0)
	{
		WLog_Print(urbdrc->log, WLOG_ERROR, "urb_select_configuration: unsupported transfer out");
		return ERROR_INVALID_PARAMETER;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return ERROR_INVALID_DATA;

	const UINT32 InterfaceId = ((STREAM_ID_PROXY << 30) | pdev->get_ReqCompletion(pdev));
	Stream_Read_UINT8(s, ConfigurationDescriptorIsValid);
	Stream_Seek(s, 3); /* Padding */
	Stream_Read_UINT32(s, NumInterfaces);

	/** if ConfigurationDescriptorIsValid is zero, then just do nothing.*/
	if (ConfigurationDescriptorIsValid)
	{
		/* parser data for struct config */
		MsConfig = msusb_msconfig_read(s, NumInterfaces);

		if (!MsConfig)
			return ERROR_INVALID_DATA;

		/* select config */
		pdev->select_configuration(pdev, MsConfig->bConfigurationValue);
		/* select all interface */
		func_select_all_interface_for_msconfig(pdev, MsConfig);
		/* complete configuration setup */
		if (!pdev->complete_msconfig_setup(pdev, MsConfig))
		{
			msusb_msconfig_free(MsConfig);
			MsConfig = NULL;
		}
	}

	if (noAck)
		return CHANNEL_RC_OK;
	return send_urb_select_configuration_result(callback, InterfaceId, MessageId, RequestId,
	                                            usbd_status, MsConfig);
}

/* [MS-RDPEUSB[ 2.2.10.3 TS_URB_SELECT_INTERFACE_RESULT */
static UINT urb_select_interface_result(GENERIC_CHANNEL_CALLBACK* callback, UINT32 RequestId,
                                        UINT32 InterfaceId, UINT32 MessageId,
                                        MSUSB_INTERFACE_DESCRIPTOR* MsInterface,
                                        UINT32 ConfigurationHandle)
{
	WINPR_ASSERT(callback);
	WINPR_ASSERT(MsInterface);

	const UINT32 NumInterfaces = 1;
	const uint32_t interface_size = 16U + (MsInterface->NumberOfPipes * 20U);
	wStream* out =
	    create_urb_completion_message(InterfaceId, MessageId, RequestId, URB_COMPLETION_NO_DATA);

	if (!out)
		return ERROR_OUTOFMEMORY;

	const uint32_t size = 8U + interface_size;
	const uint16_t usize = WINPR_ASSERTING_INT_CAST(uint16_t, size);

	if (!Stream_EnsureRemainingCapacity(out, 4))
		goto fail;
	Stream_Write_UINT32(out, usize); /* CbTsUrbResult */

	if (!write_urb_result_header(out, usize, USBD_STATUS_SUCCESS))
		goto fail;

	if (!msusb_msinterface_write(MsInterface, out))
		goto fail;

	if (!Stream_EnsureRemainingCapacity(out, 8))
		goto fail;
	Stream_Write_UINT32(out, ConfigurationHandle); /** ConfigurationHandle */
	Stream_Write_UINT32(out, NumInterfaces);       /** ConfigurationHandle */

	for (size_t x = 0; x < NumInterfaces; x++)
	{
		const MSUSB_INTERFACE_DESCRIPTOR* ifc = &MsInterface[x];
		if (!msusb_msinterface_write(ifc, out))
			goto fail;
	}

	return send_urb_completion_message(callback, out, 0, 0, NULL);

fail:
	Stream_Free(out, TRUE);

	return ERROR_INTERNAL_ERROR;
}

static UINT urb_select_interface(IUDEVICE* pdev, GENERIC_CHANNEL_CALLBACK* callback, wStream* s,
                                 UINT32 RequestField, UINT32 MessageId, IUDEVMAN* udevman,
                                 int transferDir)
{
	const BOOL noAck = (RequestField & 0x80000000U) != 0;
	const UINT32 RequestId = RequestField & 0x7FFFFFFF;

	if (!callback || !s || !udevman || !pdev)
		return ERROR_INVALID_PARAMETER;

	URBDRC_PLUGIN* urbdrc = (URBDRC_PLUGIN*)callback->plugin;

	if (!urbdrc)
		return ERROR_INVALID_PARAMETER;

	if (transferDir == 0)
	{
		WLog_Print(urbdrc->log, WLOG_ERROR, "urb_select_interface: not support transfer out");
		return ERROR_INVALID_PARAMETER;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	const UINT32 InterfaceId = ((STREAM_ID_PROXY << 30) | pdev->get_ReqCompletion(pdev));
	const UINT32 ConfigurationHandle = Stream_Get_UINT32(s);
	MSUSB_INTERFACE_DESCRIPTOR* MsInterface = msusb_msinterface_read(s);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4) || !MsInterface)
	{
		msusb_msinterface_free(MsInterface);
		return ERROR_INVALID_DATA;
	}

	const UINT32 OutputBufferSize = Stream_Get_UINT32(s);
	if (OutputBufferSize != 0)
	{
		WLog_Print(urbdrc->log, WLOG_ERROR,
		           "[MS-RDPEUSB] 2.2.9.3 TS_URB_SELECT_INTERFACE::OutputBufferSize must be 0, got "
		           "%" PRIu32,
		           OutputBufferSize);
		msusb_msinterface_free(MsInterface);
		return ERROR_INVALID_DATA;
	}

	pdev->select_interface(pdev, MsInterface->InterfaceNumber, MsInterface->AlternateSetting);
	/* replace device's MsInterface */
	MSUSB_CONFIG_DESCRIPTOR* MsConfig = pdev->get_MsConfig(pdev);
	const uint8_t InterfaceNumber = MsInterface->InterfaceNumber;
	if (!msusb_msinterface_replace(MsConfig, InterfaceNumber, MsInterface))
	{
		msusb_msconfig_free(MsConfig);
		return ERROR_BAD_CONFIGURATION;
	}
	/* complete configuration setup */
	if (!pdev->complete_msconfig_setup(pdev, MsConfig))
		return ERROR_BAD_CONFIGURATION;

	if (noAck)
		return CHANNEL_RC_OK;

	return urb_select_interface_result(callback, RequestId, InterfaceId, MessageId, MsInterface,
	                                   ConfigurationHandle);
}

static UINT urb_control_transfer(IUDEVICE* pdev, GENERIC_CHANNEL_CALLBACK* callback, wStream* s,
                                 UINT32 RequestField, UINT32 MessageId, IUDEVMAN* udevman,
                                 int transferDir, int External)
{
	UINT32 out_size = 0;
	UINT32 InterfaceId = 0;
	UINT32 EndpointAddress = 0;
	UINT32 PipeHandle = 0;
	UINT32 TransferFlags = 0;
	UINT32 OutputBufferSize = 0;
	UINT32 usbd_status = 0;
	UINT32 Timeout = 0;
	BYTE bmRequestType = 0;
	BYTE Request = 0;
	UINT16 Value = 0;
	UINT16 Index = 0;
	UINT16 length = 0;
	BYTE* buffer = NULL;
	wStream* out = NULL;
	URBDRC_PLUGIN* urbdrc = NULL;
	const BOOL noAck = (RequestField & 0x80000000U) != 0;
	const UINT32 RequestId = RequestField & 0x7FFFFFFF;

	if (!callback || !s || !udevman || !pdev)
		return ERROR_INVALID_PARAMETER;

	urbdrc = (URBDRC_PLUGIN*)callback->plugin;

	if (!urbdrc)
		return ERROR_INVALID_PARAMETER;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return ERROR_INVALID_DATA;

	InterfaceId = ((STREAM_ID_PROXY << 30) | pdev->get_ReqCompletion(pdev));
	Stream_Read_UINT32(s, PipeHandle);
	Stream_Read_UINT32(s, TransferFlags); /** TransferFlags */
	EndpointAddress = (PipeHandle & 0x000000ff);
	Timeout = 2000;

	switch (External)
	{
		case URB_CONTROL_TRANSFER_EXTERNAL:
			if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
				return ERROR_INVALID_DATA;

			Stream_Read_UINT32(s, Timeout); /** TransferFlags */
			break;

		case URB_CONTROL_TRANSFER_NONEXTERNAL:
			break;
		default:
			break;
	}

	/** SetupPacket 8 bytes */
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT8(s, bmRequestType);
	Stream_Read_UINT8(s, Request);
	Stream_Read_UINT16(s, Value);
	Stream_Read_UINT16(s, Index);
	Stream_Read_UINT16(s, length);
	Stream_Read_UINT32(s, OutputBufferSize);

	if (length != OutputBufferSize)
	{
		WLog_Print(urbdrc->log, WLOG_ERROR, "urb_control_transfer ERROR: buf != length");
		return ERROR_INVALID_DATA;
	}

	out_size = 36 + OutputBufferSize;
	out = Stream_New(NULL, out_size);

	if (!out)
		return ERROR_OUTOFMEMORY;

	Stream_Seek(out, 36);
	/** Get Buffer Data */
	buffer = Stream_Pointer(out);

	if (transferDir == USBD_TRANSFER_DIRECTION_OUT)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, OutputBufferSize))
		{
			Stream_Free(out, TRUE);
			return ERROR_INVALID_DATA;
		}
		Stream_Copy(s, out, OutputBufferSize);
	}

	/**  process TS_URB_CONTROL_TRANSFER */
	if (!pdev->control_transfer(pdev, RequestId, EndpointAddress, TransferFlags, bmRequestType,
	                            Request, Value, Index, &usbd_status, &OutputBufferSize, buffer,
	                            Timeout))
	{
		WLog_Print(urbdrc->log, WLOG_ERROR, "control_transfer failed");
		Stream_Free(out, TRUE);
		return ERROR_INTERNAL_ERROR;
	}

	return urb_write_completion(pdev, callback, noAck, out, InterfaceId, MessageId, RequestId,
	                            usbd_status, OutputBufferSize);
}

static void urb_bulk_transfer_cb(IUDEVICE* pdev, GENERIC_CHANNEL_CALLBACK* callback, wStream* out,
                                 UINT32 InterfaceId, BOOL noAck, UINT32 MessageId, UINT32 RequestId,
                                 WINPR_ATTR_UNUSED UINT32 NumberOfPackets, UINT32 status,
                                 WINPR_ATTR_UNUSED UINT32 StartFrame,
                                 WINPR_ATTR_UNUSED UINT32 ErrorCount, UINT32 OutputBufferSize)
{
	if (!pdev->isChannelClosed(pdev))
		urb_write_completion(pdev, callback, noAck, out, InterfaceId, MessageId, RequestId, status,
		                     OutputBufferSize);
	else
		Stream_Free(out, TRUE);
}

static UINT urb_bulk_or_interrupt_transfer(IUDEVICE* pdev, GENERIC_CHANNEL_CALLBACK* callback,
                                           wStream* s, UINT32 RequestField, UINT32 MessageId,
                                           IUDEVMAN* udevman, int transferDir)
{
	UINT32 EndpointAddress = 0;
	UINT32 PipeHandle = 0;
	UINT32 TransferFlags = 0;
	UINT32 OutputBufferSize = 0;
	const BOOL noAck = (RequestField & 0x80000000U) != 0;
	const UINT32 RequestId = RequestField & 0x7FFFFFFF;

	if (!pdev || !callback || !s || !udevman)
		return ERROR_INVALID_PARAMETER;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, PipeHandle);
	Stream_Read_UINT32(s, TransferFlags); /** TransferFlags */
	Stream_Read_UINT32(s, OutputBufferSize);
	EndpointAddress = (PipeHandle & 0x000000ff);

	if (transferDir == USBD_TRANSFER_DIRECTION_OUT)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, OutputBufferSize))
		{
			return ERROR_INVALID_DATA;
		}
	}

	/**  process TS_URB_BULK_OR_INTERRUPT_TRANSFER */
	const int rc = pdev->bulk_or_interrupt_transfer(
	    pdev, callback, MessageId, RequestId, EndpointAddress, TransferFlags, noAck,
	    OutputBufferSize, (transferDir == USBD_TRANSFER_DIRECTION_OUT) ? Stream_Pointer(s) : NULL,
	    urb_bulk_transfer_cb, 10000);

	return (uint32_t)rc;
}

static void urb_isoch_transfer_cb(WINPR_ATTR_UNUSED IUDEVICE* pdev,
                                  GENERIC_CHANNEL_CALLBACK* callback, wStream* out,
                                  UINT32 InterfaceId, BOOL noAck, UINT32 MessageId,
                                  UINT32 RequestId, UINT32 NumberOfPackets, UINT32 status,
                                  UINT32 StartFrame, UINT32 ErrorCount, UINT32 OutputBufferSize)
{
	if (!noAck)
	{
		UINT32 packetSize = (status == 0) ? NumberOfPackets * 12 : 0;
		Stream_SetPosition(out, 0);

		const UINT32 FunctionId = (OutputBufferSize == 0) ? URB_COMPLETION_NO_DATA : URB_COMPLETION;
		if (!write_shared_message_header_with_functionid(out, InterfaceId, MessageId, FunctionId))
		{
			Stream_Free(out, TRUE);
			return;
		}

		Stream_Write_UINT32(out, RequestId);       /** RequestId */
		Stream_Write_UINT32(out, 20 + packetSize); /** CbTsUrbResult */
		if (!write_urb_result_header(out, WINPR_ASSERTING_INT_CAST(uint16_t, 20 + packetSize),
		                             status))
		{
			Stream_Free(out, TRUE);
			return;
		}

		Stream_Write_UINT32(out, StartFrame);      /** StartFrame */

		if (status == 0)
		{
			/** NumberOfPackets */
			Stream_Write_UINT32(out, NumberOfPackets);
			Stream_Write_UINT32(out, ErrorCount); /** ErrorCount */
			Stream_Seek(out, packetSize);
		}
		else
		{
			Stream_Write_UINT32(out, 0);          /** NumberOfPackets */
			Stream_Write_UINT32(out, ErrorCount); /** ErrorCount */
		}

		Stream_Write_UINT32(out, 0);                /** HResult */
		Stream_Write_UINT32(out, OutputBufferSize); /** OutputBufferSize */
		Stream_Seek(out, OutputBufferSize);

		stream_write_and_free(callback->plugin, callback->channel, out);
	}
}

static UINT urb_isoch_transfer(IUDEVICE* pdev, GENERIC_CHANNEL_CALLBACK* callback, wStream* s,
                               UINT32 RequestField, UINT32 MessageId, IUDEVMAN* udevman,
                               int transferDir)
{
	int rc = 0;
	UINT32 EndpointAddress = 0;
	UINT32 PipeHandle = 0;
	UINT32 TransferFlags = 0;
	UINT32 StartFrame = 0;
	UINT32 NumberOfPackets = 0;
	UINT32 ErrorCount = 0;
	UINT32 OutputBufferSize = 0;
	BYTE* packetDescriptorData = NULL;
	const BOOL noAck = (RequestField & 0x80000000U) != 0;
	const UINT32 RequestId = RequestField & 0x7FFFFFFF;

	if (!pdev || !callback || !udevman)
		return ERROR_INVALID_PARAMETER;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 20))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, PipeHandle);
	EndpointAddress = (PipeHandle & 0x000000ff);
	Stream_Read_UINT32(s, TransferFlags);   /** TransferFlags */
	Stream_Read_UINT32(s, StartFrame);      /** StartFrame */
	Stream_Read_UINT32(s, NumberOfPackets); /** NumberOfPackets */
	Stream_Read_UINT32(s, ErrorCount);      /** ErrorCount */

	if (!Stream_CheckAndLogRequiredLengthOfSize(TAG, s, NumberOfPackets, 12ull))
		return ERROR_INVALID_DATA;

	packetDescriptorData = Stream_Pointer(s);
	Stream_Seek(s, 12ULL * NumberOfPackets);

	if (!Stream_CheckAndLogRequiredLength(TAG, s, sizeof(UINT32)))
		return ERROR_INVALID_DATA;
	Stream_Read_UINT32(s, OutputBufferSize);

	if (transferDir == USBD_TRANSFER_DIRECTION_OUT)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, OutputBufferSize))
			return ERROR_INVALID_DATA;
	}

	rc = pdev->isoch_transfer(
	    pdev, callback, MessageId, RequestId, EndpointAddress, TransferFlags, StartFrame,
	    ErrorCount, noAck, packetDescriptorData, NumberOfPackets, OutputBufferSize,
	    (transferDir == USBD_TRANSFER_DIRECTION_OUT) ? Stream_Pointer(s) : NULL,
	    urb_isoch_transfer_cb, 2000);

	if (rc < 0)
		return ERROR_INTERNAL_ERROR;
	return (UINT)rc;
}

static UINT urb_control_descriptor_request(IUDEVICE* pdev, GENERIC_CHANNEL_CALLBACK* callback,
                                           wStream* s, UINT32 RequestField, UINT32 MessageId,
                                           IUDEVMAN* udevman, BYTE func_recipient, int transferDir)
{
	size_t out_size = 0;
	UINT32 InterfaceId = 0;
	UINT32 OutputBufferSize = 0;
	UINT32 usbd_status = 0;
	BYTE bmRequestType = 0;
	BYTE desc_index = 0;
	BYTE desc_type = 0;
	UINT16 langId = 0;
	wStream* out = NULL;
	URBDRC_PLUGIN* urbdrc = NULL;
	const BOOL noAck = (RequestField & 0x80000000U) != 0;
	const UINT32 RequestId = RequestField & 0x7FFFFFFF;

	if (!callback || !s || !udevman || !pdev)
		return ERROR_INVALID_PARAMETER;

	urbdrc = (URBDRC_PLUGIN*)callback->plugin;

	if (!urbdrc)
		return ERROR_INVALID_PARAMETER;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return ERROR_INVALID_DATA;

	InterfaceId = ((STREAM_ID_PROXY << 30) | pdev->get_ReqCompletion(pdev));
	Stream_Read_UINT8(s, desc_index);
	Stream_Read_UINT8(s, desc_type);
	Stream_Read_UINT16(s, langId);
	Stream_Read_UINT32(s, OutputBufferSize);
	if (OutputBufferSize > UINT32_MAX - 36)
		return ERROR_INVALID_DATA;
	if (transferDir == USBD_TRANSFER_DIRECTION_OUT)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, OutputBufferSize))
			return ERROR_INVALID_DATA;
	}

	out_size = 36ULL + OutputBufferSize;
	out = Stream_New(NULL, out_size);

	if (!out)
		return ERROR_OUTOFMEMORY;

	Stream_Seek(out, 36);
	bmRequestType = func_recipient;

	switch (transferDir)
	{
		case USBD_TRANSFER_DIRECTION_IN:
			bmRequestType |= 0x80;
			break;

		case USBD_TRANSFER_DIRECTION_OUT:
			bmRequestType |= 0x00;
			Stream_Copy(s, out, OutputBufferSize);
			Stream_Rewind(out, OutputBufferSize);
			break;

		default:
			WLog_Print(urbdrc->log, WLOG_DEBUG, "get error transferDir");
			OutputBufferSize = 0;
			usbd_status = USBD_STATUS_STALL_PID;
			break;
	}

	/** process get usb device descriptor */
	if (!pdev->control_transfer(pdev, RequestId, 0, 0, bmRequestType,
	                            0x06, /* REQUEST_GET_DESCRIPTOR */
	                            WINPR_ASSERTING_INT_CAST(UINT16, ((desc_type << 8) | desc_index)),
	                            langId, &usbd_status, &OutputBufferSize, Stream_Pointer(out), 1000))
	{
		WLog_Print(urbdrc->log, WLOG_ERROR, "get_descriptor failed");
		Stream_Free(out, TRUE);
		return ERROR_INTERNAL_ERROR;
	}

	return urb_write_completion(pdev, callback, noAck, out, InterfaceId, MessageId, RequestId,
	                            usbd_status, OutputBufferSize);
}

static UINT urb_control_get_status_request(IUDEVICE* pdev, GENERIC_CHANNEL_CALLBACK* callback,
                                           wStream* s, UINT32 RequestField, UINT32 MessageId,
                                           IUDEVMAN* udevman, BYTE func_recipient, int transferDir)
{
	size_t out_size = 0;
	UINT32 InterfaceId = 0;
	UINT32 OutputBufferSize = 0;
	UINT32 usbd_status = 0;
	UINT16 Index = 0;
	BYTE bmRequestType = 0;
	wStream* out = NULL;
	URBDRC_PLUGIN* urbdrc = NULL;
	const BOOL noAck = (RequestField & 0x80000000U) != 0;
	const UINT32 RequestId = RequestField & 0x7FFFFFFF;

	if (!callback || !s || !udevman || !pdev)
		return ERROR_INVALID_PARAMETER;

	urbdrc = (URBDRC_PLUGIN*)callback->plugin;

	if (!urbdrc)
		return ERROR_INVALID_PARAMETER;

	if (transferDir == 0)
	{
		WLog_Print(urbdrc->log, WLOG_DEBUG,
		           "urb_control_get_status_request: transfer out not supported");
		return ERROR_INVALID_PARAMETER;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return ERROR_INVALID_DATA;

	InterfaceId = ((STREAM_ID_PROXY << 30) | pdev->get_ReqCompletion(pdev));
	Stream_Read_UINT16(s, Index); /** Index */
	Stream_Seek(s, 2);
	Stream_Read_UINT32(s, OutputBufferSize);
	if (OutputBufferSize > UINT32_MAX - 36)
		return ERROR_INVALID_DATA;
	out_size = 36ULL + OutputBufferSize;
	out = Stream_New(NULL, out_size);

	if (!out)
		return ERROR_OUTOFMEMORY;

	Stream_Seek(out, 36);
	bmRequestType = func_recipient | 0x80;

	if (!pdev->control_transfer(pdev, RequestId, 0, 0, bmRequestType, 0x00, /* REQUEST_GET_STATUS */
	                            0, Index, &usbd_status, &OutputBufferSize, Stream_Pointer(out),
	                            1000))
	{
		WLog_Print(urbdrc->log, WLOG_ERROR, "control_transfer failed");
		Stream_Free(out, TRUE);
		return ERROR_INTERNAL_ERROR;
	}

	return urb_write_completion(pdev, callback, noAck, out, InterfaceId, MessageId, RequestId,
	                            usbd_status, OutputBufferSize);
}

static UINT urb_control_vendor_or_class_request(IUDEVICE* pdev, GENERIC_CHANNEL_CALLBACK* callback,
                                                wStream* s, UINT32 RequestField, UINT32 MessageId,
                                                IUDEVMAN* udevman, BYTE func_type,
                                                BYTE func_recipient, int transferDir)
{
	UINT32 out_size = 0;
	UINT32 InterfaceId = 0;
	UINT32 TransferFlags = 0;
	UINT32 usbd_status = 0;
	UINT32 OutputBufferSize = 0;
	BYTE ReqTypeReservedBits = 0;
	BYTE Request = 0;
	BYTE bmRequestType = 0;
	UINT16 Value = 0;
	UINT16 Index = 0;
	wStream* out = NULL;
	URBDRC_PLUGIN* urbdrc = NULL;
	const BOOL noAck = (RequestField & 0x80000000U) != 0;
	const UINT32 RequestId = RequestField & 0x7FFFFFFF;

	if (!callback || !s || !udevman || !pdev)
		return ERROR_INVALID_PARAMETER;

	urbdrc = (URBDRC_PLUGIN*)callback->plugin;

	if (!urbdrc)
		return ERROR_INVALID_PARAMETER;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 16))
		return ERROR_INVALID_DATA;

	InterfaceId = ((STREAM_ID_PROXY << 30) | pdev->get_ReqCompletion(pdev));
	Stream_Read_UINT32(s, TransferFlags);      /** TransferFlags */
	Stream_Read_UINT8(s, ReqTypeReservedBits); /** ReqTypeReservedBids */
	Stream_Read_UINT8(s, Request);             /** Request */
	Stream_Read_UINT16(s, Value);              /** value */
	Stream_Read_UINT16(s, Index);              /** index */
	Stream_Seek_UINT16(s);                     /** Padding */
	Stream_Read_UINT32(s, OutputBufferSize);
	if (OutputBufferSize > UINT32_MAX - 36)
		return ERROR_INVALID_DATA;

	if (transferDir == USBD_TRANSFER_DIRECTION_OUT)
	{
		if (!Stream_CheckAndLogRequiredLength(TAG, s, OutputBufferSize))
			return ERROR_INVALID_DATA;
	}

	out_size = 36ULL + OutputBufferSize;
	out = Stream_New(NULL, out_size);

	if (!out)
		return ERROR_OUTOFMEMORY;

	Stream_Seek(out, 36);

	/** Get Buffer */
	if (transferDir == USBD_TRANSFER_DIRECTION_OUT)
	{
		Stream_Copy(s, out, OutputBufferSize);
		Stream_Rewind(out, OutputBufferSize);
	}

	/** vendor or class command */
	bmRequestType = func_type | func_recipient;

	if (TransferFlags & USBD_TRANSFER_DIRECTION)
		bmRequestType |= 0x80;

	WLog_Print(urbdrc->log, WLOG_DEBUG,
	           "RequestId 0x%" PRIx32 " TransferFlags: 0x%" PRIx32 " ReqTypeReservedBits: 0x%" PRIx8
	           " "
	           "Request:0x%" PRIx8 " Value: 0x%" PRIx16 " Index: 0x%" PRIx16
	           " OutputBufferSize: 0x%" PRIx32 " bmRequestType: 0x%" PRIx8,
	           RequestId, TransferFlags, ReqTypeReservedBits, Request, Value, Index,
	           OutputBufferSize, bmRequestType);

	if (!pdev->control_transfer(pdev, RequestId, 0, 0, bmRequestType, Request, Value, Index,
	                            &usbd_status, &OutputBufferSize, Stream_Pointer(out), 2000))
	{
		WLog_Print(urbdrc->log, WLOG_ERROR, "control_transfer failed");
		Stream_Free(out, TRUE);
		return ERROR_INTERNAL_ERROR;
	}

	return urb_write_completion(pdev, callback, noAck, out, InterfaceId, MessageId, RequestId,
	                            usbd_status, OutputBufferSize);
}

static UINT urb_os_feature_descriptor_request(IUDEVICE* pdev, GENERIC_CHANNEL_CALLBACK* callback,
                                              wStream* s, UINT32 RequestField, UINT32 MessageId,
                                              IUDEVMAN* udevman, int transferDir)
{
	size_t out_size = 0;
	UINT32 InterfaceId = 0;
	UINT32 OutputBufferSize = 0;
	UINT32 usbd_status = 0;
	BYTE Recipient = 0;
	BYTE InterfaceNumber = 0;
	BYTE Ms_PageIndex = 0;
	UINT16 Ms_featureDescIndex = 0;
	wStream* out = NULL;
	int ret = 0;
	URBDRC_PLUGIN* urbdrc = NULL;
	const BOOL noAck = (RequestField & 0x80000000U) != 0;
	const UINT32 RequestId = RequestField & 0x7FFFFFFF;

	if (!callback || !s || !udevman || !pdev)
		return ERROR_INVALID_PARAMETER;

	urbdrc = (URBDRC_PLUGIN*)callback->plugin;

	if (!urbdrc)
		return ERROR_INVALID_PARAMETER;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return ERROR_INVALID_DATA;

	/* 2.2.9.15 TS_URB_OS_FEATURE_DESCRIPTOR_REQUEST */
	Stream_Read_UINT8(s, Recipient);            /** Recipient */
	Recipient = (Recipient & 0x1f);             /* Mask out Padding1 */
	Stream_Read_UINT8(s, InterfaceNumber);      /** InterfaceNumber */
	Stream_Read_UINT8(s, Ms_PageIndex);         /** Ms_PageIndex */
	Stream_Read_UINT16(s, Ms_featureDescIndex); /** Ms_featureDescIndex */
	Stream_Seek(s, 3);                          /* Padding 2 */
	Stream_Read_UINT32(s, OutputBufferSize);
	if (OutputBufferSize > UINT32_MAX - 36)
		return ERROR_INVALID_DATA;

	switch (transferDir)
	{
		case USBD_TRANSFER_DIRECTION_OUT:
			if (!Stream_CheckAndLogRequiredLength(TAG, s, OutputBufferSize))
				return ERROR_INVALID_DATA;

			break;

		default:
			break;
	}

	InterfaceId = ((STREAM_ID_PROXY << 30) | pdev->get_ReqCompletion(pdev));
	out_size = 36ULL + OutputBufferSize;
	out = Stream_New(NULL, out_size);

	if (!out)
		return ERROR_OUTOFMEMORY;

	Stream_Seek(out, 36);

	switch (transferDir)
	{
		case USBD_TRANSFER_DIRECTION_OUT:
			Stream_Copy(s, out, OutputBufferSize);
			Stream_Rewind(out, OutputBufferSize);
			break;

		case USBD_TRANSFER_DIRECTION_IN:
			break;
		default:
			break;
	}

	WLog_Print(urbdrc->log, WLOG_DEBUG,
	           "Ms descriptor arg: Recipient:0x%" PRIx8 ", "
	           "InterfaceNumber:0x%" PRIx8 ", Ms_PageIndex:0x%" PRIx8 ", "
	           "Ms_featureDescIndex:0x%" PRIx16 ", OutputBufferSize:0x%" PRIx32 "",
	           Recipient, InterfaceNumber, Ms_PageIndex, Ms_featureDescIndex, OutputBufferSize);
	/** get ms string */
	ret = pdev->os_feature_descriptor_request(pdev, RequestId, Recipient, InterfaceNumber,
	                                          Ms_PageIndex, Ms_featureDescIndex, &usbd_status,
	                                          &OutputBufferSize, Stream_Pointer(out), 1000);

	if (ret < 0)
		WLog_Print(urbdrc->log, WLOG_DEBUG, "os_feature_descriptor_request: error num %d", ret);

	return urb_write_completion(pdev, callback, noAck, out, InterfaceId, MessageId, RequestId,
	                            usbd_status, OutputBufferSize);
}

static UINT urb_pipe_request(IUDEVICE* pdev, GENERIC_CHANNEL_CALLBACK* callback, wStream* s,
                             UINT32 RequestField, UINT32 MessageId, IUDEVMAN* udevman,
                             int transferDir, int action)
{
	UINT32 out_size = 0;
	UINT32 InterfaceId = 0;
	UINT32 PipeHandle = 0;
	UINT32 EndpointAddress = 0;
	UINT32 OutputBufferSize = 0;
	UINT32 usbd_status = 0;
	wStream* out = NULL;
	UINT32 ret = USBD_STATUS_REQUEST_FAILED;
	int rc = 0;
	URBDRC_PLUGIN* urbdrc = NULL;
	const BOOL noAck = (RequestField & 0x80000000U) != 0;
	const UINT32 RequestId = RequestField & 0x7FFFFFFF;

	if (!callback || !s || !udevman || !pdev)
		return ERROR_INVALID_PARAMETER;

	urbdrc = (URBDRC_PLUGIN*)callback->plugin;

	if (!urbdrc)
		return ERROR_INVALID_PARAMETER;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return ERROR_INVALID_DATA;

	if (transferDir == 0)
	{
		WLog_Print(urbdrc->log, WLOG_DEBUG, "urb_pipe_request: not support transfer out");
		return ERROR_INVALID_PARAMETER;
	}

	InterfaceId = ((STREAM_ID_PROXY << 30) | pdev->get_ReqCompletion(pdev));
	Stream_Read_UINT32(s, PipeHandle); /** PipeHandle */
	Stream_Read_UINT32(s, OutputBufferSize);
	EndpointAddress = (PipeHandle & 0x000000ff);

	switch (action)
	{
		case PIPE_CANCEL:
			rc = pdev->control_pipe_request(pdev, RequestId, EndpointAddress, &usbd_status,
			                                PIPE_CANCEL);

			if (rc < 0)
				WLog_Print(urbdrc->log, WLOG_DEBUG, "PIPE SET HALT: error %u", ret);
			else
				ret = USBD_STATUS_SUCCESS;

			break;

		case PIPE_RESET:
			WLog_Print(urbdrc->log, WLOG_DEBUG, "urb_pipe_request: PIPE_RESET ep 0x%" PRIx32 "",
			           EndpointAddress);
			rc = pdev->control_pipe_request(pdev, RequestId, EndpointAddress, &usbd_status,
			                                PIPE_RESET);

			if (rc < 0)
				WLog_Print(urbdrc->log, WLOG_DEBUG, "PIPE RESET: error %u", ret);
			else
				ret = USBD_STATUS_SUCCESS;

			break;

		default:
			WLog_Print(urbdrc->log, WLOG_DEBUG, "urb_pipe_request action: %d not supported",
			           action);
			ret = USBD_STATUS_INVALID_URB_FUNCTION;
			break;
	}

	/** send data */
	out_size = 36;
	if (out_size > OutputBufferSize)
	{
		WLog_Print(urbdrc->log, WLOG_DEBUG, "out_size %" PRIu32 " > OutputBufferSize %" PRIu32,
		           out_size, OutputBufferSize);
		return ERROR_BAD_CONFIGURATION;
	}
	out = Stream_New(NULL, out_size);

	if (!out)
		return ERROR_OUTOFMEMORY;

	return urb_write_completion(pdev, callback, noAck, out, InterfaceId, MessageId, RequestId, ret,
	                            0);
}
/* [MS-RDPEUSB] 2.2.10.4 TS_URB_GET_CURRENT_FRAME_NUMBER_RESULT */
static UINT urb_send_current_frame_number_result(GENERIC_CHANNEL_CALLBACK* callback,
                                                 UINT32 RequestId, UINT32 MessageId,
                                                 UINT32 CompletionId, UINT32 FrameNumber)
{
	WINPR_ASSERT(callback);

	const UINT32 InterfaceId = ((STREAM_ID_PROXY << 30) | CompletionId);
	wStream* out =
	    create_urb_completion_message(InterfaceId, MessageId, RequestId, URB_COMPLETION_NO_DATA);

	if (!out)
		return ERROR_OUTOFMEMORY;

	Stream_Write_UINT32(out, 12); /** CbTsUrbResult */
	if (!write_urb_result_header(out, 12, USBD_STATUS_SUCCESS))
	{
		Stream_Free(out, TRUE);
		return ERROR_OUTOFMEMORY;
	}

	Stream_Write_UINT32(out, FrameNumber); /** FrameNumber */
	return send_urb_completion_message(callback, out, 0, 0, NULL);
}

static UINT urb_get_current_frame_number(IUDEVICE* pdev, GENERIC_CHANNEL_CALLBACK* callback,
                                         wStream* s, UINT32 RequestField, UINT32 MessageId,
                                         IUDEVMAN* udevman, int transferDir)
{
	const BOOL noAck = (RequestField & 0x80000000U) != 0;
	const UINT32 RequestId = RequestField & 0x7FFFFFFF;

	if (!callback || !s || !udevman || !pdev)
		return ERROR_INVALID_PARAMETER;

	URBDRC_PLUGIN* urbdrc = (URBDRC_PLUGIN*)callback->plugin;

	if (!urbdrc)
		return ERROR_INVALID_PARAMETER;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	if (transferDir == 0)
	{
		WLog_Print(urbdrc->log, WLOG_DEBUG,
		           "urb_get_current_frame_number: not support transfer out");
		return ERROR_INVALID_PARAMETER;
	}

	const UINT32 OutputBufferSize = Stream_Get_UINT32(s);
	if (OutputBufferSize != 0)
	{
		WLog_Print(urbdrc->log, WLOG_WARN, "OutputBufferSize=%" PRIu32 ", expected 0",
		           OutputBufferSize);
	}
	/** Fixme: Need to fill actual frame number!!*/
	const UINT32 dummy_frames = GetTickCount();
	const UINT32 CompletionId = pdev->get_ReqCompletion(pdev);

	if (noAck)
		return CHANNEL_RC_OK;

	return urb_send_current_frame_number_result(callback, RequestId, MessageId, CompletionId,
	                                            dummy_frames);
}

/* Unused function for current server */
static UINT urb_control_get_configuration_request(IUDEVICE* pdev,
                                                  GENERIC_CHANNEL_CALLBACK* callback, wStream* s,
                                                  UINT32 RequestField, UINT32 MessageId,
                                                  IUDEVMAN* udevman, int transferDir)
{
	size_t out_size = 0;
	UINT32 InterfaceId = 0;
	UINT32 OutputBufferSize = 0;
	UINT32 usbd_status = 0;
	wStream* out = NULL;
	URBDRC_PLUGIN* urbdrc = NULL;
	const BOOL noAck = (RequestField & 0x80000000U) != 0;
	const UINT32 RequestId = RequestField & 0x7FFFFFFF;

	if (!callback || !s || !udevman || !pdev)
		return ERROR_INVALID_PARAMETER;

	urbdrc = (URBDRC_PLUGIN*)callback->plugin;

	if (!urbdrc)
		return ERROR_INVALID_PARAMETER;

	if (transferDir == 0)
	{
		WLog_Print(urbdrc->log, WLOG_DEBUG,
		           "urb_control_get_configuration_request:"
		           " not support transfer out");
		return ERROR_INVALID_PARAMETER;
	}

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, OutputBufferSize);
	if (OutputBufferSize > UINT32_MAX - 36)
		return ERROR_INVALID_DATA;
	out_size = 36ULL + OutputBufferSize;
	out = Stream_New(NULL, out_size);

	if (!out)
		return ERROR_OUTOFMEMORY;

	Stream_Seek(out, 36);
	InterfaceId = ((STREAM_ID_PROXY << 30) | pdev->get_ReqCompletion(pdev));

	if (!pdev->control_transfer(pdev, RequestId, 0, 0, 0x80 | 0x00,
	                            0x08, /* REQUEST_GET_CONFIGURATION */
	                            0, 0, &usbd_status, &OutputBufferSize, Stream_Pointer(out), 1000))
	{
		WLog_Print(urbdrc->log, WLOG_DEBUG, "control_transfer failed");
		Stream_Free(out, TRUE);
		return ERROR_INTERNAL_ERROR;
	}

	return urb_write_completion(pdev, callback, noAck, out, InterfaceId, MessageId, RequestId,
	                            usbd_status, OutputBufferSize);
}

/* Unused function for current server */
static UINT urb_control_get_interface_request(IUDEVICE* pdev, GENERIC_CHANNEL_CALLBACK* callback,
                                              wStream* s, UINT32 RequestField, UINT32 MessageId,
                                              IUDEVMAN* udevman, int transferDir)
{
	size_t out_size = 0;
	UINT32 InterfaceId = 0;
	UINT32 OutputBufferSize = 0;
	UINT32 usbd_status = 0;
	UINT16 InterfaceNr = 0;
	wStream* out = NULL;
	URBDRC_PLUGIN* urbdrc = NULL;
	const BOOL noAck = (RequestField & 0x80000000U) != 0;
	const UINT32 RequestId = RequestField & 0x7FFFFFFF;

	if (!callback || !s || !udevman || !pdev)
		return ERROR_INVALID_PARAMETER;

	urbdrc = (URBDRC_PLUGIN*)callback->plugin;

	if (!urbdrc)
		return ERROR_INVALID_PARAMETER;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return ERROR_INVALID_DATA;

	if (transferDir == 0)
	{
		WLog_Print(urbdrc->log, WLOG_DEBUG,
		           "urb_control_get_interface_request: not support transfer out");
		return ERROR_INVALID_PARAMETER;
	}

	InterfaceId = ((STREAM_ID_PROXY << 30) | pdev->get_ReqCompletion(pdev));
	Stream_Read_UINT16(s, InterfaceNr);
	Stream_Seek(s, 2);
	Stream_Read_UINT32(s, OutputBufferSize);
	if (OutputBufferSize > UINT32_MAX - 36)
		return ERROR_INVALID_DATA;
	out_size = 36ULL + OutputBufferSize;
	out = Stream_New(NULL, out_size);

	if (!out)
		return ERROR_OUTOFMEMORY;

	Stream_Seek(out, 36);

	if (!pdev->control_transfer(
	        pdev, RequestId, 0, 0, 0x80 | 0x01, 0x0A, /* REQUEST_GET_INTERFACE */
	        0, InterfaceNr, &usbd_status, &OutputBufferSize, Stream_Pointer(out), 1000))
	{
		WLog_Print(urbdrc->log, WLOG_DEBUG, "control_transfer failed");
		Stream_Free(out, TRUE);
		return ERROR_INTERNAL_ERROR;
	}

	return urb_write_completion(pdev, callback, noAck, out, InterfaceId, MessageId, RequestId,
	                            usbd_status, OutputBufferSize);
}

static UINT urb_control_feature_request(IUDEVICE* pdev, GENERIC_CHANNEL_CALLBACK* callback,
                                        wStream* s, UINT32 RequestField, UINT32 MessageId,
                                        IUDEVMAN* udevman, BYTE func_recipient, BYTE command,
                                        int transferDir)
{
	UINT32 InterfaceId = 0;
	UINT32 OutputBufferSize = 0;
	UINT32 usbd_status = 0;
	UINT16 FeatureSelector = 0;
	UINT16 Index = 0;
	BYTE bmRequestType = 0;
	BYTE bmRequest = 0;
	wStream* out = NULL;
	URBDRC_PLUGIN* urbdrc = NULL;
	const BOOL noAck = (RequestField & 0x80000000U) != 0;
	const UINT32 RequestId = RequestField & 0x7FFFFFFF;

	if (!callback || !s || !udevman || !pdev)
		return ERROR_INVALID_PARAMETER;

	urbdrc = (URBDRC_PLUGIN*)callback->plugin;

	if (!urbdrc)
		return ERROR_INVALID_PARAMETER;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 8))
		return ERROR_INVALID_DATA;

	InterfaceId = ((STREAM_ID_PROXY << 30) | pdev->get_ReqCompletion(pdev));
	Stream_Read_UINT16(s, FeatureSelector);
	Stream_Read_UINT16(s, Index);
	Stream_Read_UINT32(s, OutputBufferSize);
	if (OutputBufferSize > UINT32_MAX - 36)
		return ERROR_INVALID_DATA;
	switch (transferDir)
	{
		case USBD_TRANSFER_DIRECTION_OUT:
			if (!Stream_CheckAndLogRequiredLength(TAG, s, OutputBufferSize))
				return ERROR_INVALID_DATA;

			break;

		default:
			break;
	}

	out = Stream_New(NULL, 36ULL + OutputBufferSize);

	if (!out)
		return ERROR_OUTOFMEMORY;

	Stream_Seek(out, 36);
	bmRequestType = func_recipient;

	switch (transferDir)
	{
		case USBD_TRANSFER_DIRECTION_OUT:
			WLog_Print(urbdrc->log, WLOG_ERROR,
			           "Function urb_control_feature_request: OUT Unchecked");
			Stream_Copy(s, out, OutputBufferSize);
			Stream_Rewind(out, OutputBufferSize);
			bmRequestType |= 0x00;
			break;

		case USBD_TRANSFER_DIRECTION_IN:
			bmRequestType |= 0x80;
			break;
		default:
			break;
	}

	switch (command)
	{
		case URB_SET_FEATURE:
			bmRequest = 0x03; /* REQUEST_SET_FEATURE */
			break;

		case URB_CLEAR_FEATURE:
			bmRequest = 0x01; /* REQUEST_CLEAR_FEATURE */
			break;

		default:
			WLog_Print(urbdrc->log, WLOG_ERROR,
			           "urb_control_feature_request: Error Command 0x%02" PRIx8 "", command);
			Stream_Free(out, TRUE);
			return ERROR_INTERNAL_ERROR;
	}

	if (!pdev->control_transfer(pdev, RequestId, 0, 0, bmRequestType, bmRequest, FeatureSelector,
	                            Index, &usbd_status, &OutputBufferSize, Stream_Pointer(out), 1000))
	{
		WLog_Print(urbdrc->log, WLOG_DEBUG, "feature control transfer failed");
		Stream_Free(out, TRUE);
		return ERROR_INTERNAL_ERROR;
	}

	return urb_write_completion(pdev, callback, noAck, out, InterfaceId, MessageId, RequestId,
	                            usbd_status, OutputBufferSize);
}

static UINT urbdrc_process_transfer_request(IUDEVICE* pdev, GENERIC_CHANNEL_CALLBACK* callback,
                                            wStream* s, UINT32 MessageId, IUDEVMAN* udevman,
                                            int transferDir)
{
	UINT32 CbTsUrb = 0;
	UINT16 Size = 0;
	UINT16 URB_Function = 0;
	UINT32 RequestId = 0;
	UINT error = ERROR_INTERNAL_ERROR;
	URBDRC_PLUGIN* urbdrc = NULL;

	if (!callback || !s || !udevman || !pdev)
		return ERROR_INVALID_PARAMETER;

	urbdrc = (URBDRC_PLUGIN*)callback->plugin;

	if (!urbdrc)
		return ERROR_INVALID_PARAMETER;

	if (!Stream_CheckAndLogRequiredLength(TAG, s, 12))
		return ERROR_INVALID_DATA;

	Stream_Read_UINT32(s, CbTsUrb); /** CbTsUrb */
	if (!Stream_CheckAndLogRequiredLength(TAG, s, 4ULL + CbTsUrb))
		return ERROR_INVALID_DATA;
	Stream_Read_UINT16(s, Size);    /** size */
	if (Size != CbTsUrb)
	{
		const char* section = (transferDir == USBD_TRANSFER_DIRECTION_IN)
		                          ? "2.2.6.7 Transfer In Request (TRANSFER_IN_REQUEST)"
		                          : "2.2.6.8 Transfer Out Request (TRANSFER_OUT_REQUEST)";
		WLog_ERR(TAG,
		         "[MS-RDPEUSB] 2.2.9.1.1 TS_URB_HEADER::Size 0x%04" PRIx16
		         " != %s::CbTsUrb 0x%08" PRIx32,
		         Size, section, CbTsUrb);
		return ERROR_INVALID_DATA;
	}
	Stream_Read_UINT16(s, URB_Function);
	Stream_Read_UINT32(s, RequestId);
	WLog_Print(urbdrc->log, WLOG_DEBUG, "URB %s[%" PRIu16 "]", urb_function_string(URB_Function),
	           URB_Function);

	switch (URB_Function)
	{
		case TS_URB_SELECT_CONFIGURATION: /** 0x0000 */
			error = urb_select_configuration(pdev, callback, s, RequestId, MessageId, udevman,
			                                 transferDir);
			break;

		case TS_URB_SELECT_INTERFACE: /** 0x0001 */
			error =
			    urb_select_interface(pdev, callback, s, RequestId, MessageId, udevman, transferDir);
			break;

		case TS_URB_PIPE_REQUEST: /** 0x0002  */
			error = urb_pipe_request(pdev, callback, s, RequestId, MessageId, udevman, transferDir,
			                         PIPE_CANCEL);
			break;

		case TS_URB_TAKE_FRAME_LENGTH_CONTROL: /** 0x0003  */
			/** This URB function is obsolete in Windows 2000
			 * and later operating systems
			 * and is not supported by Microsoft. */
			break;

		case TS_URB_RELEASE_FRAME_LENGTH_CONTROL: /** 0x0004 */
			/** This URB function is obsolete in Windows 2000
			 * and later operating systems
			 * and is not supported by Microsoft. */
			break;

		case TS_URB_GET_FRAME_LENGTH: /** 0x0005 */
			/** This URB function is obsolete in Windows 2000
			 * and later operating systems
			 * and is not supported by Microsoft. */
			break;

		case TS_URB_SET_FRAME_LENGTH: /** 0x0006 */
			/** This URB function is obsolete in Windows 2000
			 * and later operating systems
			 * and is not supported by Microsoft. */
			break;

		case TS_URB_GET_CURRENT_FRAME_NUMBER: /** 0x0007 */
			error = urb_get_current_frame_number(pdev, callback, s, RequestId, MessageId, udevman,
			                                     transferDir);
			break;

		case TS_URB_CONTROL_TRANSFER: /** 0x0008 */
			error = urb_control_transfer(pdev, callback, s, RequestId, MessageId, udevman,
			                             transferDir, URB_CONTROL_TRANSFER_NONEXTERNAL);
			break;

		case TS_URB_BULK_OR_INTERRUPT_TRANSFER: /** 0x0009 */
			error = urb_bulk_or_interrupt_transfer(pdev, callback, s, RequestId, MessageId, udevman,
			                                       transferDir);
			break;

		case TS_URB_ISOCH_TRANSFER: /** 0x000A */
			error =
			    urb_isoch_transfer(pdev, callback, s, RequestId, MessageId, udevman, transferDir);
			break;

		case TS_URB_GET_DESCRIPTOR_FROM_DEVICE: /** 0x000B */
			error = urb_control_descriptor_request(pdev, callback, s, RequestId, MessageId, udevman,
			                                       0x00, transferDir);
			break;

		case TS_URB_SET_DESCRIPTOR_TO_DEVICE: /** 0x000C */
			error = urb_control_descriptor_request(pdev, callback, s, RequestId, MessageId, udevman,
			                                       0x00, transferDir);
			break;

		case TS_URB_SET_FEATURE_TO_DEVICE: /** 0x000D */
			error = urb_control_feature_request(pdev, callback, s, RequestId, MessageId, udevman,
			                                    0x00, URB_SET_FEATURE, transferDir);
			break;

		case TS_URB_SET_FEATURE_TO_INTERFACE: /** 0x000E */
			error = urb_control_feature_request(pdev, callback, s, RequestId, MessageId, udevman,
			                                    0x01, URB_SET_FEATURE, transferDir);
			break;

		case TS_URB_SET_FEATURE_TO_ENDPOINT: /** 0x000F */
			error = urb_control_feature_request(pdev, callback, s, RequestId, MessageId, udevman,
			                                    0x02, URB_SET_FEATURE, transferDir);
			break;

		case TS_URB_CLEAR_FEATURE_TO_DEVICE: /** 0x0010 */
			error = urb_control_feature_request(pdev, callback, s, RequestId, MessageId, udevman,
			                                    0x00, URB_CLEAR_FEATURE, transferDir);
			break;

		case TS_URB_CLEAR_FEATURE_TO_INTERFACE: /** 0x0011 */
			error = urb_control_feature_request(pdev, callback, s, RequestId, MessageId, udevman,
			                                    0x01, URB_CLEAR_FEATURE, transferDir);
			break;

		case TS_URB_CLEAR_FEATURE_TO_ENDPOINT: /** 0x0012 */
			error = urb_control_feature_request(pdev, callback, s, RequestId, MessageId, udevman,
			                                    0x02, URB_CLEAR_FEATURE, transferDir);
			break;

		case TS_URB_GET_STATUS_FROM_DEVICE: /** 0x0013 */
			error = urb_control_get_status_request(pdev, callback, s, RequestId, MessageId, udevman,
			                                       0x00, transferDir);
			break;

		case TS_URB_GET_STATUS_FROM_INTERFACE: /** 0x0014 */
			error = urb_control_get_status_request(pdev, callback, s, RequestId, MessageId, udevman,
			                                       0x01, transferDir);
			break;

		case TS_URB_GET_STATUS_FROM_ENDPOINT: /** 0x0015 */
			error = urb_control_get_status_request(pdev, callback, s, RequestId, MessageId, udevman,
			                                       0x02, transferDir);
			break;

		case TS_URB_RESERVED_0X0016: /** 0x0016 */
			break;

		case TS_URB_VENDOR_DEVICE: /** 0x0017 */
			error = urb_control_vendor_or_class_request(pdev, callback, s, RequestId, MessageId,
			                                            udevman, (0x02 << 5), /* vendor type */
			                                            0x00, transferDir);
			break;

		case TS_URB_VENDOR_INTERFACE: /** 0x0018 */
			error = urb_control_vendor_or_class_request(pdev, callback, s, RequestId, MessageId,
			                                            udevman, (0x02 << 5), /* vendor type */
			                                            0x01, transferDir);
			break;

		case TS_URB_VENDOR_ENDPOINT: /** 0x0019 */
			error = urb_control_vendor_or_class_request(pdev, callback, s, RequestId, MessageId,
			                                            udevman, (0x02 << 5), /* vendor type */
			                                            0x02, transferDir);
			break;

		case TS_URB_CLASS_DEVICE: /** 0x001A */
			error = urb_control_vendor_or_class_request(pdev, callback, s, RequestId, MessageId,
			                                            udevman, (0x01 << 5), /* class type */
			                                            0x00, transferDir);
			break;

		case TS_URB_CLASS_INTERFACE: /** 0x001B */
			error = urb_control_vendor_or_class_request(pdev, callback, s, RequestId, MessageId,
			                                            udevman, (0x01 << 5), /* class type */
			                                            0x01, transferDir);
			break;

		case TS_URB_CLASS_ENDPOINT: /** 0x001C */
			error = urb_control_vendor_or_class_request(pdev, callback, s, RequestId, MessageId,
			                                            udevman, (0x01 << 5), /* class type */
			                                            0x02, transferDir);
			break;

		case TS_URB_RESERVE_0X001D: /** 0x001D */
			break;

		case TS_URB_SYNC_RESET_PIPE_AND_CLEAR_STALL: /** 0x001E */
			error = urb_pipe_request(pdev, callback, s, RequestId, MessageId, udevman, transferDir,
			                         PIPE_RESET);
			break;

		case TS_URB_CLASS_OTHER: /** 0x001F */
			error = urb_control_vendor_or_class_request(pdev, callback, s, RequestId, MessageId,
			                                            udevman, (0x01 << 5), /* class type */
			                                            0x03, transferDir);
			break;

		case TS_URB_VENDOR_OTHER: /** 0x0020 */
			error = urb_control_vendor_or_class_request(pdev, callback, s, RequestId, MessageId,
			                                            udevman, (0x02 << 5), /* vendor type */
			                                            0x03, transferDir);
			break;

		case TS_URB_GET_STATUS_FROM_OTHER: /** 0x0021 */
			error = urb_control_get_status_request(pdev, callback, s, RequestId, MessageId, udevman,
			                                       0x03, transferDir);
			break;

		case TS_URB_CLEAR_FEATURE_TO_OTHER: /** 0x0022 */
			error = urb_control_feature_request(pdev, callback, s, RequestId, MessageId, udevman,
			                                    0x03, URB_CLEAR_FEATURE, transferDir);
			break;

		case TS_URB_SET_FEATURE_TO_OTHER: /** 0x0023 */
			error = urb_control_feature_request(pdev, callback, s, RequestId, MessageId, udevman,
			                                    0x03, URB_SET_FEATURE, transferDir);
			break;

		case TS_URB_GET_DESCRIPTOR_FROM_ENDPOINT: /** 0x0024 */
			error = urb_control_descriptor_request(pdev, callback, s, RequestId, MessageId, udevman,
			                                       0x02, transferDir);
			break;

		case TS_URB_SET_DESCRIPTOR_TO_ENDPOINT: /** 0x0025 */
			error = urb_control_descriptor_request(pdev, callback, s, RequestId, MessageId, udevman,
			                                       0x02, transferDir);
			break;

		case TS_URB_CONTROL_GET_CONFIGURATION_REQUEST: /** 0x0026 */
			error = urb_control_get_configuration_request(pdev, callback, s, RequestId, MessageId,
			                                              udevman, transferDir);
			break;

		case TS_URB_CONTROL_GET_INTERFACE_REQUEST: /** 0x0027 */
			error = urb_control_get_interface_request(pdev, callback, s, RequestId, MessageId,
			                                          udevman, transferDir);
			break;

		case TS_URB_GET_DESCRIPTOR_FROM_INTERFACE: /** 0x0028 */
			error = urb_control_descriptor_request(pdev, callback, s, RequestId, MessageId, udevman,
			                                       0x01, transferDir);
			break;

		case TS_URB_SET_DESCRIPTOR_TO_INTERFACE: /** 0x0029 */
			error = urb_control_descriptor_request(pdev, callback, s, RequestId, MessageId, udevman,
			                                       0x01, transferDir);
			break;

		case TS_URB_GET_OS_FEATURE_DESCRIPTOR_REQUEST: /** 0x002A */
			error = urb_os_feature_descriptor_request(pdev, callback, s, RequestId, MessageId,
			                                          udevman, transferDir);
			break;

		case TS_URB_RESERVE_0X002B: /** 0x002B */
		case TS_URB_RESERVE_0X002C: /** 0x002C */
		case TS_URB_RESERVE_0X002D: /** 0x002D */
		case TS_URB_RESERVE_0X002E: /** 0x002E */
		case TS_URB_RESERVE_0X002F: /** 0x002F */
			break;

		/** USB 2.0 calls start at 0x0030 */
		case TS_URB_SYNC_RESET_PIPE: /** 0x0030 */
			error = urb_pipe_request(pdev, callback, s, RequestId, MessageId, udevman, transferDir,
			                         PIPE_RESET);
			break;

		case TS_URB_SYNC_CLEAR_STALL: /** 0x0031 */
			urb_pipe_request(pdev, callback, s, RequestId, MessageId, udevman, transferDir,
			                 PIPE_RESET);
			break;

		case TS_URB_CONTROL_TRANSFER_EX: /** 0x0032 */
			error = urb_control_transfer(pdev, callback, s, RequestId, MessageId, udevman,
			                             transferDir, URB_CONTROL_TRANSFER_EXTERNAL);
			break;

		default:
			WLog_Print(urbdrc->log, WLOG_DEBUG, "URB_Func: %" PRIx16 " is not found!",
			           URB_Function);
			break;
	}

	if (error)
	{
		WLog_Print(urbdrc->log, WLOG_WARN,
		           "USB transfer request URB Function '%s' [0x%08x] failed with %08" PRIx32,
		           urb_function_string(URB_Function), URB_Function, error);
	}

	return error;
}

UINT urbdrc_process_udev_data_transfer(GENERIC_CHANNEL_CALLBACK* callback, URBDRC_PLUGIN* urbdrc,
                                       IUDEVMAN* udevman, wStream* data)
{
	UINT32 InterfaceId = 0;
	UINT32 MessageId = 0;
	UINT32 FunctionId = 0;
	IUDEVICE* pdev = NULL;
	UINT error = ERROR_INTERNAL_ERROR;

	if (!urbdrc || !data || !callback || !udevman)
		goto fail;

	if (!Stream_CheckAndLogRequiredLength(TAG, data, 8))
		goto fail;

	Stream_Rewind_UINT32(data);

	Stream_Read_UINT32(data, InterfaceId);
	Stream_Read_UINT32(data, MessageId);
	Stream_Read_UINT32(data, FunctionId);

	pdev = udevman->get_udevice_by_UsbDevice(udevman, InterfaceId);

	/* Device does not exist, ignore this request. */
	if (pdev == NULL)
	{
		error = ERROR_SUCCESS;
		goto fail;
	}

	/* Device has been removed, ignore this request. */
	if (pdev->isChannelClosed(pdev))
	{
		error = ERROR_SUCCESS;
		goto fail;
	}

	/* USB kernel driver detach!! */
	pdev->detach_kernel_driver(pdev);

	switch (FunctionId)
	{
		case CANCEL_REQUEST:
			error = urbdrc_process_cancel_request(pdev, data, udevman);
			break;

		case REGISTER_REQUEST_CALLBACK:
			error = urbdrc_process_register_request_callback(pdev, callback, data, udevman);
			break;

		case IO_CONTROL:
			error = urbdrc_process_io_control(pdev, callback, data, MessageId, udevman);
			break;

		case INTERNAL_IO_CONTROL:
			error = urbdrc_process_internal_io_control(pdev, callback, data, MessageId, udevman);
			break;

		case QUERY_DEVICE_TEXT:
			error = urbdrc_process_query_device_text(pdev, callback, data, MessageId, udevman);
			break;

		case TRANSFER_IN_REQUEST:
			error = urbdrc_process_transfer_request(pdev, callback, data, MessageId, udevman,
			                                        USBD_TRANSFER_DIRECTION_IN);
			break;

		case TRANSFER_OUT_REQUEST:
			error = urbdrc_process_transfer_request(pdev, callback, data, MessageId, udevman,
			                                        USBD_TRANSFER_DIRECTION_OUT);
			break;

		case RETRACT_DEVICE:
			error = urbdrc_process_retract_device_request(pdev, data, udevman);
			break;

		default:
			WLog_Print(urbdrc->log, WLOG_WARN,
			           "urbdrc_process_udev_data_transfer:"
			           " unknown FunctionId 0x%" PRIX32 "",
			           FunctionId);
			break;
	}

fail:
	if (error)
	{
		WLog_WARN(TAG, "USB request failed with %08" PRIx32, error);
	}

	return error;
}
