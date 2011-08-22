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

#ifndef __FREERDP_PEER_H
#define __FREERDP_PEER_H

typedef struct rdp_freerdp_peer freerdp_peer;

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/settings.h>

typedef boolean (*psPeerInitialize)(freerdp_peer* client);
typedef boolean (*psPeerGetFileDescriptor)(freerdp_peer* client, void** rfds, int* rcount);
typedef boolean (*psPeerCheckFileDescriptor)(freerdp_peer* client);
typedef void (*psPeerDisconnect)(freerdp_peer* client);
typedef boolean (*psPeerPostConnect)(freerdp_peer* client);

struct rdp_freerdp_peer
{
	void* peer;
	void* param1;
	void* param2;
	void* param3;
	void* param4;

	rdpSettings* settings;

	psPeerInitialize Initialize;
	psPeerGetFileDescriptor GetFileDescriptor;
	psPeerCheckFileDescriptor CheckFileDescriptor;
	psPeerDisconnect Disconnect;

	psPeerPostConnect PostConnect;
};

FREERDP_API freerdp_peer* freerdp_peer_new(int sockfd);
FREERDP_API void freerdp_peer_free(freerdp_peer* client);

#endif /* __FREERDP_PEER_H */

