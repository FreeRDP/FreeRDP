#include <errno.h>
#include <signal.h>

#include <winpr/crt.h>
#include <winpr/ssl.h>
#include <winpr/synch.h>
#include <winpr/string.h>
#include <winpr/path.h>
#include <winpr/winsock.h>

#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/channels.h>

#include <freerdp/constants.h>
#include <freerdp/server/rdpsnd.h>

#include "pfreerdp.h"

#define TAG PROXY_TAG("server")

/* Event callbacks */
BOOL tf_peer_post_connect(freerdp_peer* client)
{
	testPeerContext* context = (testPeerContext*) client->context;
	/**
	 * This callback is called when the entire connection sequence is done, i.e. we've received the
	 * Font List PDU from the client and sent out the Font Map PDU.
	 * The server may start sending graphics output and receiving keyboard/mouse input after this
	 * callback returns.
	 */
	WLog_INFO(TAG, "Client %s is activated (osMajorType %"PRIu32" osMinorType %"PRIu32")",
	          client->local ? "(local)" : client->hostname,
	          client->settings->OsMajorType, client->settings->OsMinorType);

	if (client->settings->AutoLogonEnabled)
	{
		WLog_INFO(TAG, " and wants to login automatically as %s\\%s",
		          client->settings->Domain ? client->settings->Domain : "",
		          client->settings->Username);
		/* A real server may perform OS login here if NLA is not executed previously. */
	}

	WLog_INFO(TAG, "");
	WLog_INFO(TAG, "Client requested desktop: %"PRIu32"x%"PRIu32"x%"PRIu32"",
	          client->settings->DesktopWidth, client->settings->DesktopHeight,
	          client->settings->ColorDepth);

	if (!rfx_context_reset(context->rfx_context, client->settings->DesktopWidth,
	                       client->settings->DesktopHeight))
		return FALSE;

	/* Return FALSE here would stop the execution of the peer main loop. */
	return TRUE;
}

BOOL tf_peer_activate(freerdp_peer* client)
{
	testPeerContext* context = (testPeerContext*) client->context;
	context->activated = TRUE;
	//client->settings->CompressionLevel = PACKET_COMPR_TYPE_8K;
	//client->settings->CompressionLevel = PACKET_COMPR_TYPE_64K;
	//client->settings->CompressionLevel = PACKET_COMPR_TYPE_RDP6;
	client->settings->CompressionLevel = PACKET_COMPR_TYPE_RDP61;
	return TRUE;
}

BOOL tf_peer_synchronize_event(rdpInput* input, UINT32 flags)
{
	WLog_INFO(TAG, "Client sent a synchronize event (flags:0x%"PRIX32")", flags);
	return TRUE;
}

BOOL tf_peer_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	WLog_INFO(TAG, "Client sent a keyboard event (flags:0x%04"PRIX16" code:0x%04"PRIX16")", flags,
	          code);
	return TRUE;
}

BOOL tf_peer_unicode_keyboard_event(rdpInput* input, UINT16 flags, UINT16 code)
{
	WLog_INFO(TAG, "Client sent a unicode keyboard event (flags:0x%04"PRIX16" code:0x%04"PRIX16")",
	          flags, code);
	return TRUE;
}

BOOL tf_peer_mouse_event(rdpInput* input, UINT16 flags, UINT16 x, UINT16 y)
{
	WLog_INFO(TAG, "Client sent a mouse event (flags:0x%04"PRIX16" pos:%"PRIu16",%"PRIu16")", flags, x,
	          y);
	return TRUE;
}

BOOL tf_peer_extended_mouse_event(rdpInput* input, UINT16 flags, UINT16 x,
                                  UINT16 y)
{
	//WLog_INFO(TAG, "Client sent an extended mouse event (flags:0x%04"PRIX16" pos:%"PRIu16",%"PRIu16")", flags, x, y);
	return TRUE;
}

static BOOL tf_peer_refresh_rect(rdpContext* context, BYTE count,
                                 const RECTANGLE_16* areas)
{
	BYTE i;
	WLog_INFO(TAG, "Client requested to refresh:");

	for (i = 0; i < count; i++)
	{
		WLog_INFO(TAG, "  (%"PRIu16"2, %"PRIu16") (%"PRIu16", %"PRIu16")", areas[i].left, areas[i].top,
		          areas[i].right, areas[i].bottom);
	}

	return TRUE;
}

static BOOL tf_peer_suppress_output(rdpContext* context, BYTE allow,
                                    const RECTANGLE_16* area)
{
	if (allow > 0)
	{
		WLog_INFO(TAG, "Client restore output (%"PRIu16", %"PRIu16") (%"PRIu16", %"PRIu16").", area->left,
		          area->top,
		          area->right, area->bottom);
	}
	else
	{
		WLog_INFO(TAG, "Client minimized and suppress output.");
	}

	return TRUE;
}

BOOL test_peer_context_new(freerdp_peer* client, testPeerContext* context)
{
	if (!(context->rfx_context = rfx_context_new(TRUE)))
		goto fail_rfx_context;

	if (!rfx_context_reset(context->rfx_context, 800, 600))
		goto fail_rfx_context;

	context->rfx_context->mode = RLGR3;
	rfx_context_set_pixel_format(context->rfx_context, PIXEL_FORMAT_RGB24);

	if (!(context->nsc_context = nsc_context_new()))
		goto fail_nsc_context;

	nsc_context_set_pixel_format(context->nsc_context, PIXEL_FORMAT_RGB24);

	if (!(context->s = Stream_New(NULL, 65536)))
		goto fail_stream_new;

	context->icon_x = -1;
	context->icon_y = -1;
	context->vcm = WTSOpenServerA((LPSTR) client->context);

	if (!context->vcm || context->vcm == INVALID_HANDLE_VALUE)
		goto fail_open_server;

	return TRUE;
fail_open_server:
	context->vcm = NULL;
	Stream_Free(context->s, TRUE);
	context->s = NULL;
fail_stream_new:
	nsc_context_free(context->nsc_context);
	context->nsc_context = NULL;
fail_nsc_context:
	rfx_context_free(context->rfx_context);
	context->rfx_context = NULL;
fail_rfx_context:
	return FALSE;
}

void test_peer_context_free(freerdp_peer* client, testPeerContext* context)
{
	if (context)
	{
		if (context->debug_channel_thread)
		{
			SetEvent(context->stopEvent);
			WaitForSingleObject(context->debug_channel_thread, INFINITE);
			CloseHandle(context->debug_channel_thread);
		}

		Stream_Free(context->s, TRUE);
		free(context->icon_data);
		free(context->bg_data);
		rfx_context_free(context->rfx_context);
		nsc_context_free(context->nsc_context);

		if (context->debug_channel)
			WTSVirtualChannelClose(context->debug_channel);

		if (context->audin)
			audin_server_context_free(context->audin);

		if (context->rdpsnd)
			rdpsnd_server_context_free(context->rdpsnd);

		if (context->encomsp)
			encomsp_server_context_free(context->encomsp);

		WTSCloseServer((HANDLE) context->vcm);
	}
}

// Init client on connection
static BOOL init_client(freerdp_peer* client)
{
	client->ContextSize = sizeof(testPeerContext);
	client->ContextNew = (psPeerContextNew) test_peer_context_new;
	client->ContextFree = (psPeerContextFree) test_peer_context_free;
	return freerdp_peer_context_new(client);
}

// Handles an incoming client connection, to be run in it's own thread.
//
// arg is a pointer to a freerdp_peer representing the client.
static DWORD WINAPI handle_client(LPVOID arg)
{
	// Parse arg into client and init client
	freerdp_peer* client = (freerdp_peer*) arg;

	if (!init_client(client))
	{
		freerdp_peer_free(client);
		return 0;
	}

	// Load server key and cert
	client->settings->CertificateFile = _strdup("server.crt");
	client->settings->PrivateKeyFile = _strdup("server.key");
	client->settings->RdpKeyFile = _strdup("server.key");

	if (!client->settings->CertificateFile || !client->settings->PrivateKeyFile
	    || !client->settings->RdpKeyFile)
	{
		WLog_ERR(TAG, "Memory allocation failed (strdup)");
		freerdp_peer_free(client);
		return 0;
	}

	// Init security settings
	client->settings->RdpSecurity = TRUE;
	client->settings->TlsSecurity = TRUE;
	client->settings->NlaSecurity = FALSE;
	client->settings->EncryptionLevel = ENCRYPTION_LEVEL_CLIENT_COMPATIBLE;
	/* client->settings->EncryptionLevel = ENCRYPTION_LEVEL_HIGH; */
	/* client->settings->EncryptionLevel = ENCRYPTION_LEVEL_LOW; */
	/* client->settings->EncryptionLevel = ENCRYPTION_LEVEL_FIPS; */
	// Init graphic settings
	client->settings->RemoteFxCodec = TRUE;
	client->settings->ColorDepth = 32;
	client->settings->SuppressOutput = TRUE;
	client->settings->RefreshRect = TRUE;
	// Init events
	client->PostConnect = tf_peer_post_connect;
	client->Activate = tf_peer_activate;
	client->input->SynchronizeEvent = tf_peer_synchronize_event;
	client->input->KeyboardEvent = tf_peer_keyboard_event;
	client->input->UnicodeKeyboardEvent = tf_peer_unicode_keyboard_event;
	client->input->MouseEvent = tf_peer_mouse_event;
	client->input->ExtendedMouseEvent = tf_peer_extended_mouse_event;
	client->update->RefreshRect = tf_peer_refresh_rect;
	client->update->SuppressOutput = tf_peer_suppress_output;
	client->settings->MultifragMaxRequestSize = 0xFFFFFF; /* FIXME */
	client->Initialize(client);
	testPeerContext* context;
	context = (testPeerContext*) client->context;
	WLog_INFO(TAG, "Client connected: %s",
	          client->local ? "(local)" : client->hostname);
	HANDLE eventHandles[32];

	while (1)
	{
		DWORD eventCount = 0;
		{
			DWORD tmp = client->GetEventHandles(client, &eventHandles[eventCount], 32 - eventCount);

			if (tmp == 0)
			{
				WLog_ERR(TAG, "Failed to get FreeRDP transport event handles");
				break;
			}

			eventCount += tmp;
		}
		eventHandles[eventCount++] = WTSVirtualChannelManagerGetEventHandle(context->vcm);
		DWORD status = WaitForMultipleObjects(eventCount, eventHandles, FALSE, INFINITE);

		if (status == WAIT_FAILED)
		{
			WLog_ERR(TAG, "WaitForMultipleObjects failed (errno: %d)", errno);
			break;
		}

		if (client->CheckFileDescriptor(client) != TRUE)
			break;

		if (WTSVirtualChannelManagerCheckFileDescriptor(context->vcm) != TRUE)
			break;
	}

	WLog_INFO(TAG, "Client %s disconnected.",
	          client->local ? "(local)" : client->hostname);
	client->Disconnect(client);
	freerdp_peer_context_free(client);
	freerdp_peer_free(client);
	return 0;
}

// The callback called when a client is accepted on the listener.
//
// Starts handling the client connection in it's own thread.
static BOOL client_connected(freerdp_listener* listener, freerdp_peer* client)
{
	HANDLE hThread;

	if (!(hThread = CreateThread(NULL, 0, handle_client, (void*) client, 0, NULL)))
		return FALSE;

	CloseHandle(hThread);
	return TRUE;
}

// Gets events from the listener (??) and waits for them to be handled.
static void server_mainloop(freerdp_listener* listener)
{
	HANDLE eventHandles[32];
	DWORD eventCount;
	DWORD status;

	while (1)
	{
		eventCount = listener->GetEventHandles(listener, eventHandles, 32);

		if (0 == eventCount)
		{
			WLog_ERR(TAG, "Failed to get FreeRDP event handles");
			break;
		}

		status = WaitForMultipleObjects(eventCount, eventHandles, FALSE, INFINITE);

		if (WAIT_FAILED == status)
		{
			WLog_ERR(TAG, "select failed");
			break;
		}

		if (listener->CheckFileDescriptor(listener) != TRUE)
		{
			WLog_ERR(TAG, "Failed to check FreeRDP file descriptor");
			break;
		}
	}

	listener->Close(listener);
}

int main(int argc, char* argv[])
{
	// Handle Flags
	BOOL localOnly = FALSE;
	char* host = "0.0.0.0";
	long port = 3389;
	// Init WTS and SSL
	WTSRegisterWtsApiFunctionTable(FreeRDP_InitWtsApi());
	winpr_InitializeSSL(WINPR_SSL_INIT_DEFAULT);
	// Init listener
	freerdp_listener* listener = freerdp_listener_new();

	if (!listener)
	{
		return -1;
	}

	listener->PeerAccepted = client_connected;
	// Startup Windows Socket API
	WSADATA wsaData;

	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
	{
		freerdp_listener_free(listener);
		return -1;
	}

	// Determine file for local unix domain socket
	char* localSockPath;
	char localSockName[MAX_PATH];
	sprintf_s(localSockName, sizeof(localSockName), "proxy.%ld", port);
	localSockPath = GetKnownSubPath(KNOWN_PATH_TEMP, localSockName);

	if (!localSockPath)
	{
		freerdp_listener_free(listener);
		WSACleanup();
		return -1;
	}

	// Listen to local connections
	BOOL success = listener->OpenLocal(listener, localSockPath);

	// Listen to remote connections
	if (!localOnly)
	{
		success &= listener->Open(listener, host, port);
	}

	// Run server mainloop
	if (success)
	{
		server_mainloop(listener);
	}

	// Exit
	free(localSockPath);
	freerdp_listener_free(listener);
	WSACleanup();
	return 0;
}