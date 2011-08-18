/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RDP Server Peer
 *
 * Copyright 2011 Vic Lee
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

#include "peer.h"

static boolean freerdp_peer_initialize(freerdp_peer* client)
{
	return True;
}

static boolean freerdp_peer_get_fds(freerdp_peer* client, void** rfds, int* rcount)
{
	rdpPeer* peer = (rdpPeer*)client->peer;

	rfds[*rcount] = (void*)(long)(peer->sockfd);
	(*rcount)++;

	return True;
}

static boolean freerdp_peer_check_fds(freerdp_peer* client)
{
	return True;
}

static void freerdp_peer_disconnect(freerdp_peer* client)
{
}

freerdp_peer* freerdp_peer_new(int sockfd)
{
	freerdp_peer* client;
	rdpPeer* peer;

	client = xnew(freerdp_peer);

	client->settings = settings_new();
	client->Initialize = freerdp_peer_initialize;
	client->GetFileDescriptor = freerdp_peer_get_fds;
	client->CheckFileDescriptor = freerdp_peer_check_fds;
	client->Disconnect = freerdp_peer_disconnect;

	peer = xnew(rdpPeer);
	peer->client = client;
	peer->sockfd = sockfd;

	client->peer = (void*)peer;

	return client;
}

void freerdp_peer_free(freerdp_peer* client)
{
	rdpPeer* peer = (rdpPeer*)client->peer;

	xfree(peer);
	settings_free(client->settings);
	xfree(client);
}

