/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RDP Server Redirection
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "redirection.h"

/**
 * Read an RDP Server Redirection Packet.\n
 * @msdn{ee441959}
 * @param rdp RDP module
 * @param s stream
 * @param sec_flags security flags
 */

boolean rdp_recv_redirection_packet(rdpRdp* rdp, STREAM* s)
{
	printf("Redirection Packet\n");

	return True;
}

boolean rdp_recv_enhanced_security_redirection_packet(rdpRdp* rdp, STREAM* s)
{
	printf("Enhanced Security Redirection Packet\n");

	return True;
}

