/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * MS-RDPECAM Implementation, UVC H264 support
 *
 * See USB_Video_Payload_H 264_1 0.pdf for more details
 *
 * Credits:
 *     guvcview    http://guvcview.sourceforge.net
 *     Paulo Assis <pj.assis@gmail.com>
 *
 * Copyright 2025 Oleg Turovski <oleg2104@hotmail.com>
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

#include <sys/ioctl.h>

#include <linux/uvcvideo.h>
#include <linux/videodev2.h>
#include <libusb.h>

#include "uvc_h264.h"

/* UVC H.264 extension unit GUID: {A29E7641-DE04-47E3-8B2B-F4341AFF003B} */
static uint8_t GUID_UVCX_H264_XU[16] = { 0x41, 0x76, 0x9E, 0xA2, 0x04, 0xDE, 0xE3, 0x47,
	                                     0x8B, 0x2B, 0xF4, 0x34, 0x1A, 0xFF, 0x00, 0x3B };

#define TAG CHANNELS_TAG("rdpecam-uvch264.client")

/*
 * get length of xu control defined by unit id and selector
 * args:
 *   stream - pointer to video device data
 *   unit - unit id of xu control
 *   selector - selector for control
 *
 * returns: length of xu control
 */
static uint16_t get_length_xu_control(CamV4lStream* stream, uint8_t unit, uint8_t selector)
{
	WINPR_ASSERT(stream);
	WINPR_ASSERT(stream->fd > 0);

	uint16_t length = 0;

	struct uvc_xu_control_query xu_ctrl_query = { .unit = unit,
		                                          .selector = selector,
		                                          .query = UVC_GET_LEN,
		                                          .size = sizeof(length),
		                                          .data = (uint8_t*)&length };

	if (ioctl(stream->fd, UVCIOC_CTRL_QUERY, &xu_ctrl_query) < 0)
	{
		char ebuffer[256] = { 0 };
		WLog_ERR(TAG, "UVCIOC_CTRL_QUERY (GET_LEN) - Error: %s",
		         winpr_strerror(errno, ebuffer, sizeof(ebuffer)));
		return 0;
	}

	return length;
}

/*
 * runs a query on xu control defined by unit id and selector
 * args:
 *   stream - pointer to video device data
 *   unit - unit id of xu control
 *   selector - selector for control
 *   query - query type
 *   data - pointer to query data
 *
 * returns: 0 if query succeeded or error code on fail
 */
static int query_xu_control(CamV4lStream* stream, uint8_t unit, uint8_t selector, uint8_t query,
                            void* data)
{
	int err = 0;
	uint16_t len = get_length_xu_control(stream, unit, selector);

	struct uvc_xu_control_query xu_ctrl_query = {
		.unit = unit, .selector = selector, .query = query, .size = len, .data = (uint8_t*)data
	};

	/*get query data*/
	if ((err = ioctl(stream->fd, UVCIOC_CTRL_QUERY, &xu_ctrl_query)) < 0)
	{
		char ebuffer[256] = { 0 };
		WLog_ERR(TAG, "UVCIOC_CTRL_QUERY (%" PRIu8 ") - Error: %s", query,
		         winpr_strerror(errno, ebuffer, sizeof(ebuffer)));
	}

	return err;
}

/*
 * resets the h264 encoder
 * args:
 *   stream - pointer to video device data
 *
 * returns: 0 on success or error code on fail
 */
static int uvcx_video_encoder_reset(CamV4lStream* stream)
{
	WINPR_ASSERT(stream);

	uvcx_encoder_reset encoder_reset_req = { 0 };

	int err = 0;

	if ((err = query_xu_control(stream, stream->h264UnitId, UVCX_ENCODER_RESET, UVC_SET_CUR,
	                            &encoder_reset_req)) < 0)
	{
		char ebuffer[256] = { 0 };
		WLog_ERR(TAG, "UVCX_ENCODER_RESET error: %s",
		         winpr_strerror(errno, ebuffer, sizeof(ebuffer)));
	}

	return err;
}

/*
 * probes the h264 encoder config
 * args:
 *   stream - pointer to video device data
 *   query - probe query
 *   uvcx_video_config - pointer to probe/commit config data
 *
 * returns: 0 on success or error code on fail
 */
static int uvcx_video_probe(CamV4lStream* stream, uint8_t query,
                            uvcx_video_config_probe_commit_t* uvcx_video_config)
{
	WINPR_ASSERT(stream);

	int err = 0;

	if ((err = query_xu_control(stream, stream->h264UnitId, UVCX_VIDEO_CONFIG_PROBE, query,
	                            uvcx_video_config)) < 0)
	{
		char ebuffer[256] = { 0 };
		WLog_ERR(TAG, "UVCX_VIDEO_CONFIG_PROBE error: %s",
		         winpr_strerror(errno, ebuffer, sizeof(ebuffer)));
	}

	return err;
}

/*
 * commits the h264 encoder config
 * args:
 *   stream - pointer to video device data
 *   uvcx_video_config - pointer to probe/commit config data
 *
 * returns: 0 on success or error code on fail
 */
static int uvcx_video_commit(CamV4lStream* stream,
                             uvcx_video_config_probe_commit_t* uvcx_video_config)
{
	WINPR_ASSERT(stream);

	int err = 0;

	if ((err = query_xu_control(stream, stream->h264UnitId, UVCX_VIDEO_CONFIG_COMMIT, UVC_SET_CUR,
	                            uvcx_video_config)) < 0)
	{
		char ebuffer[256] = { 0 };
		WLog_ERR(TAG, "UVCX_VIDEO_CONFIG_COMMIT error: %s",
		         winpr_strerror(errno, ebuffer, sizeof(ebuffer)));
	}

	return err;
}

/*
 * sets h264 muxed format (must not be called while streaming)
 * args:
 *   stream - pointer to video device data
 *   mediaType
 *
 * returns: TRUE on success or FALSE on fail
 */
BOOL set_h264_muxed_format(CamV4lStream* stream, const CAM_MEDIA_TYPE_DESCRIPTION* mediaType)
{
	WINPR_ASSERT(stream);
	WINPR_ASSERT(mediaType);

	uvcx_video_config_probe_commit_t config_probe_req = { 0 };
	int err = 0;

	/* reset the encoder */
	err = uvcx_video_encoder_reset(stream);
	if (err != 0)
		return FALSE;

	/* get default values */
	err = uvcx_video_probe(stream, UVC_GET_DEF, &config_probe_req);
	if (err != 0)
		return FALSE;

	/* set resolution */
	config_probe_req.wWidth = WINPR_ASSERTING_INT_CAST(uint16_t, mediaType->Width);
	config_probe_req.wHeight = WINPR_ASSERTING_INT_CAST(uint16_t, mediaType->Height);

	/* set frame rate in 100ns units */
	uint32_t frame_interval =
	    (mediaType->FrameRateDenominator * 1000000000LL / mediaType->FrameRateNumerator) / 100;
	config_probe_req.dwFrameInterval = frame_interval;

	/* quality settings */
	config_probe_req.wProfile = PROFILE_HIGH;
	config_probe_req.bUsageType = USAGETYPE_REALTIME;
	config_probe_req.bRateControlMode = RATECONTROL_VBR;
	config_probe_req.dwBitRate = h264_get_max_bitrate(mediaType->Height);
	config_probe_req.bEntropyCABAC = ENTROPY_CABAC;
	config_probe_req.wIFramePeriod = 1000; /* ms, 1 sec */

	/* hints which parameters are configured */
	config_probe_req.bmHints = BMHINTS_RESOLUTION | BMHINTS_FRAME_INTERVAL | BMHINTS_PROFILE |
	                           BMHINTS_USAGE | BMHINTS_RATECONTROL | BMHINTS_BITRATE |
	                           BMHINTS_ENTROPY | BMHINTS_IFRAMEPERIOD;

	/* set the aux stream */
	config_probe_req.bStreamMuxOption = STREAMMUX_H264;

	/* probe the format */
	err = uvcx_video_probe(stream, UVC_SET_CUR, &config_probe_req);
	if (err != 0)
		return FALSE;

	err = uvcx_video_probe(stream, UVC_GET_CUR, &config_probe_req);
	if (err != 0)
		return FALSE;

	if (config_probe_req.wWidth != mediaType->Width)
	{
		WLog_ERR(TAG, "Requested width %" PRIu16 " but got %" PRIu16, mediaType->Width,
		         config_probe_req.wWidth);
		return FALSE;
	}
	if (config_probe_req.wHeight != mediaType->Height)
	{
		WLog_ERR(TAG, "Requested height %" PRIu16 " but got %" PRIu16, mediaType->Height,
		         config_probe_req.wHeight);
		return FALSE;
	}
	if (config_probe_req.dwFrameInterval != frame_interval)
	{
		WLog_ERR(TAG, "Requested frame interval %" PRIu32 " but got %" PRIu32, frame_interval,
		         config_probe_req.dwFrameInterval);
		return FALSE;
	}

	/* commit the format */
	err = uvcx_video_commit(stream, &config_probe_req);
	if (err != 0)
		return FALSE;

	return TRUE;
}

/*
 * parses deviceId such as usb-0000:00:1a.0-1.2.2 to return devpath (1.2.2)
 *
 * deviceID format is: usb-<busname>-<devpath>
 * see kernel's usb_make_path()
 *
 * args:
 *   deviceId
 *   path - buffer to return devpath
 *   size - buffer size
 *
 * returns: TRUE if success, FALSE otherwise
 */
static BOOL get_devpath_from_device_id(const char* deviceId, char* path, size_t size)
{
	if (0 != strncmp(deviceId, "usb-", 4))
		return FALSE;

	/* find second `-` */
	const char* p = strchr(deviceId + 4, '-');
	if (!p)
		return FALSE;

	p++; // now points to NULL terminated devpath

	strncpy(path, p, size - 1);
	return TRUE;
}

/*
 * return devpath of a given libusb_device as text string such as: 1.2.2 or 2.3
 *
 * args:
 *   device
 *   path - buffer to return devpath
 *   size - buffer size
 *
 * returns: TRUE if success, FALSE otherwise
 */
static BOOL get_devpath_from_device(libusb_device* device, char* path, size_t size)
{
	uint8_t ports[MAX_DEVPATH_DEPTH] = { 0 };
	int nPorts = libusb_get_port_numbers(device, ports, sizeof(ports));

	if (nPorts <= 0)
		return FALSE;

	for (int i = 0; i < nPorts; i++)
	{
		int nChars = snprintf(path, size, "%" PRIu8, ports[i]);
		if ((nChars <= 0) || ((size_t)nChars >= size))
			return FALSE;

		size -= (size_t)nChars;
		path += nChars;

		if (i < nPorts - 1)
		{
			*path++ = '.';
			size--;
		}
	}
	return TRUE;
}

/*
 * get GUID unit id from libusb_device, if any
 *
 * args:
 *   device
 *   guid - 16 byte xu GUID
 *
 * returns: unit id for the matching GUID or 0 if none
 */
static uint8_t get_guid_unit_id_from_device(libusb_device* device, const uint8_t* guid)
{
	struct libusb_device_descriptor ddesc = { 0 };

	if (libusb_get_device_descriptor(device, &ddesc) != 0)
	{
		WLog_ERR(TAG, "Couldn't get device descriptor");
		return 0;
	}

	for (uint8_t i = 0; i < ddesc.bNumConfigurations; ++i)
	{
		struct libusb_config_descriptor* config = NULL;

		if (libusb_get_config_descriptor(device, i, &config) != 0)
		{
			WLog_ERR(TAG,
			         "Couldn't get config descriptor for "
			         "configuration %" PRIu8,
			         i);
			continue;
		}

		for (uint8_t j = 0; j < config->bNumInterfaces; j++)
		{
			const struct libusb_interface* cfg = &config->interface[j];
			for (int k = 0; k < cfg->num_altsetting; k++)
			{
				const struct libusb_interface_descriptor* interface = &cfg->altsetting[k];
				if (interface->bInterfaceClass != LIBUSB_CLASS_VIDEO ||
				    interface->bInterfaceSubClass != USB_VIDEO_CONTROL)
					continue;

				const uint8_t* ptr = interface->extra;
				while (ptr < interface->extra + interface->extra_length)
				{
					const xu_descriptor* desc = (const xu_descriptor*)ptr;
					if (desc->bDescriptorType == USB_VIDEO_CONTROL_INTERFACE &&
					    desc->bDescriptorSubType == USB_VIDEO_CONTROL_XU_TYPE &&
					    memcmp(desc->guidExtensionCode, guid, 16) == 0)
					{
						int8_t unit_id = desc->bUnitID;

						WLog_DBG(TAG,
						         "For camera %04" PRIx16 ":%04" PRIx16
						         " found UVCX H264 UnitID %" PRId8,
						         ddesc.idVendor, ddesc.idProduct, unit_id);
						if (unit_id < 0)
							return 0;
						return WINPR_CXX_COMPAT_CAST(uint8_t, unit_id);
					}
					ptr += desc->bLength;
				}
			}
		}
	}

	/* no match found */
	return 0;
}

/*
 * get GUID unit id, if any
 *
 * args:
 *   deviceId - camera deviceId such as: usb-0000:00:1a.0-1.2.2
 *   guid - 16 byte xu GUID
 *
 * returns: unit id for the matching GUID or 0 if none
 */
static uint8_t get_guid_unit_id(const char* deviceId, const uint8_t* guid)
{
	char cam_devpath[MAX_DEVPATH_STR_SIZE] = { 0 };
	libusb_context* usb_ctx = NULL;
	libusb_device** device_list = NULL;
	uint8_t unit_id = 0;

	if (!get_devpath_from_device_id(deviceId, cam_devpath, sizeof(cam_devpath)))
	{
		WLog_ERR(TAG, "Unable to get devpath from deviceId %s", deviceId);
		return 0;
	}

	if (0 != libusb_init(&usb_ctx))
	{
		WLog_ERR(TAG, "Unable to initialize libusb");
		return 0;
	}

	ssize_t cnt = libusb_get_device_list(usb_ctx, &device_list);

	for (ssize_t i = 0; i < cnt; i++)
	{
		char path[MAX_DEVPATH_STR_SIZE] = { 0 };
		libusb_device* device = device_list[i];

		if (!device || !get_devpath_from_device(device, path, sizeof(path)))
			continue;

		if (0 != strcmp(cam_devpath, path))
			continue;

		/* found device with matching devpath, try to get guid unit id */
		unit_id = get_guid_unit_id_from_device(device, guid);

		if (unit_id > 0)
			break; /* got it */

		/* there's chance for another devpath match - continue */
	}

	libusb_free_device_list(device_list, TRUE);
	libusb_exit(usb_ctx);
	return unit_id;
}

/*
 * gets the uvc h264 xu control unit id, if any
 *
 * args:
 *   deviceId - camera deviceId such as: usb-0000:00:1a.0-1.2.2
 *
 * returns: unit id or 0 if none
 */
uint8_t get_uvc_h624_unit_id(const char* deviceId)
{
	WINPR_ASSERT(deviceId);

	WLog_DBG(TAG, "Checking for UVCX H264 UnitID for %s", deviceId);

	return get_guid_unit_id(deviceId, GUID_UVCX_H264_XU);
}
