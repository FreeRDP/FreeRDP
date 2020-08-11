/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy Server Session Capture Module
 *
 * Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
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

#define TAG MODULE_TAG("capture")

#define PLUGIN_NAME "capture"
#define PLUGIN_DESC "stream egfx connections over tcp"

#include <errno.h>
#include <winpr/image.h>
#include <freerdp/gdi/gdi.h>
#include <winpr/winsock.h>

#include "pf_log.h"
#include "modules_api.h"
#include "pf_context.h"
#include "cap_config.h"
#include "cap_protocol.h"

#define BUFSIZE 8092

static proxyPluginsManager* g_plugins_manager = NULL;
static captureConfig config = { 0 };

static SOCKET capture_plugin_init_socket(void)
{
	int status;
	int sockfd;
	struct sockaddr_in addr = { 0 };
	sockfd = _socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (sockfd == -1)
		return -1;

	addr.sin_family = AF_INET;
	addr.sin_port = htons(config.port);
	inet_pton(AF_INET, config.host, &(addr.sin_addr));

	status = _connect(sockfd, (const struct sockaddr*)&addr, sizeof(addr));
	if (status < 0)
	{
		close(sockfd);
		return -1;
	}

	return sockfd;
}

static BOOL capture_plugin_send_data(SOCKET sockfd, const BYTE* buffer, size_t len)
{
	size_t chunk_len;
	int nsent;

	if (!buffer)
		return FALSE;

	while (len > 0)
	{
		chunk_len = len > BUFSIZE ? BUFSIZE : len;
		nsent = _send(sockfd, (const char*)buffer, chunk_len, 0);
		if (nsent == -1)
			return FALSE;

		buffer += nsent;
		len -= nsent;
	}

	return TRUE;
}

static BOOL capture_plugin_send_packet(SOCKET sockfd, wStream* packet)
{
	size_t len;
	BYTE* buffer;
	BOOL result = FALSE;

	if (!packet)
		return FALSE;

	buffer = Stream_Buffer(packet);
	len = Stream_Capacity(packet);

	if (!capture_plugin_send_data(sockfd, buffer, len))
	{
		WLog_ERR(TAG, "error while transmitting frame: errno=%d", errno);
		goto error;
	}

	result = TRUE;

error:
	Stream_Free(packet, TRUE);
	return result;
}

static SOCKET capture_plugin_get_socket(proxyData* pdata)
{
	void* custom;

	custom = g_plugins_manager->GetPluginData(PLUGIN_NAME, pdata);
	if (!custom)
		return -1;

	return (SOCKET)custom;
}

static BOOL capture_plugin_session_end(proxyData* pdata)
{
	SOCKET socket;
	BOOL ret;
	wStream* s;

	socket = capture_plugin_get_socket(pdata);
	if (socket == INVALID_SOCKET)
		return FALSE;

	s = capture_plugin_packet_new(SESSION_END_PDU_BASE_SIZE, MESSAGE_TYPE_SESSION_END);
	if (!s)
		return FALSE;

	ret = capture_plugin_send_packet(socket, s);

	closesocket(socket);
	return ret;
}

static BOOL capture_plugin_send_frame(pClientContext* pc, SOCKET socket, const BYTE* buffer)
{
	size_t frame_size;
	BOOL ret = FALSE;
	wStream* s = NULL;
	BYTE* bmp_header = NULL;
	rdpSettings* settings = pc->context.settings;

	frame_size = settings->DesktopWidth * settings->DesktopHeight * (settings->ColorDepth / 8);
	bmp_header = winpr_bitmap_construct_header(settings->DesktopWidth, settings->DesktopHeight,
	                                           settings->ColorDepth);

	if (!bmp_header)
		return FALSE;

	/*
	 * capture frame packet indicates a packet that contains a frame buffer. payload length is
	 * marked as 0, and receiving side must read `frame_size` bytes, a constant size of
	 * width*height*(bpp/8) from the socket, to receive the full frame buffer.
	 */
	s = capture_plugin_packet_new(0, MESSAGE_TYPE_CAPTURED_FRAME);
	if (!s)
		goto error;

	if (!capture_plugin_send_packet(socket, s))
		goto error;

	ret = capture_plugin_send_data(socket, bmp_header, WINPR_IMAGE_BMP_HEADER_LEN);
	if (!ret)
		goto error;

	ret = capture_plugin_send_data(socket, buffer, frame_size);

error:
	free(bmp_header);
	return ret;
}

static BOOL capture_plugin_client_end_paint(proxyData* pdata)
{
	pClientContext* pc = pdata->pc;
	rdpGdi* gdi = pc->context.gdi;
	SOCKET socket;

	if (gdi->suppressOutput)
		return TRUE;

	if (gdi->primary->hdc->hwnd->ninvalid < 1)
		return TRUE;

	socket = capture_plugin_get_socket(pdata);
	if (socket == INVALID_SOCKET)
		return FALSE;

	if (!capture_plugin_send_frame(pc, socket, gdi->primary_buffer))
	{
		WLog_ERR(TAG, "capture_plugin_send_frame failed!");
		return FALSE;
	}

	gdi->primary->hdc->hwnd->invalid->null = TRUE;
	gdi->primary->hdc->hwnd->ninvalid = 0;
	return TRUE;
}

static BOOL capture_plugin_client_post_connect(proxyData* pdata)
{
	SOCKET socket;
	wStream* s;

	socket = capture_plugin_init_socket();
	if (socket == INVALID_SOCKET)
	{
		WLog_ERR(TAG, "failed to establish a connection");
		return FALSE;
	}

	g_plugins_manager->SetPluginData(PLUGIN_NAME, pdata, (void*)socket);

	s = capture_plugin_create_session_info_packet(pdata->pc);
	if (!s)
		return FALSE;

	return capture_plugin_send_packet(socket, s);
}

static BOOL capture_plugin_server_post_connect(proxyData* pdata)
{
	pServerContext* ps = pdata->ps;
	proxyConfig* config = pdata->config;
	rdpSettings* settings = ps->context.settings;

	if (!config->GFX || !config->DecodeGFX)
	{
		WLog_ERR(TAG, "config options 'Channels.GFX' and 'GFXSettings.DecodeGFX' options must be "
		              "set to true!");
		return FALSE;
	}

	if (!settings->SupportGraphicsPipeline)
	{
		WLog_ERR(TAG, "session capture is only supported for GFX clients, denying connection");
		return FALSE;
	}

	return TRUE;
}

static BOOL capture_plugin_unload(void)
{
	capture_plugin_config_free_internal(&config);
	return TRUE;
}

static proxyPlugin demo_plugin = {
	PLUGIN_NAME,                        /* name */
	PLUGIN_DESC,                        /* description */
	capture_plugin_unload,              /* PluginUnload */
	NULL,                               /* ClientPreConnect */
	capture_plugin_client_post_connect, /* ClientPostConnect */
	NULL,                               /* ClientLoginFailure */
	capture_plugin_client_end_paint,    /* ClientEndPaint */
	capture_plugin_server_post_connect, /* ServerPostConnect */
	NULL,                               /* ServerChannelsInit */
	NULL,                               /* ServerChannelsFree */
	capture_plugin_session_end,         /* Session End */
	NULL,                               /* KeyboardEvent */
	NULL,                               /* MouseEvent */
	NULL,                               /* ClientChannelData */
	NULL,                               /* ServerChannelData */
};

BOOL proxy_module_entry_point(proxyPluginsManager* plugins_manager)
{
	g_plugins_manager = plugins_manager;

	if (!capture_plugin_init_config(&config))
	{
		WLog_ERR(TAG, "failed to load config");
		return FALSE;
	}

	WLog_INFO(TAG, "host: %s, port: %" PRIu16 "", config.host, config.port);
	return plugins_manager->RegisterPlugin(&demo_plugin);
}
