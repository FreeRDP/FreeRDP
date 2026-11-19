/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Authentication redirection virtual channel, packet layout
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

#ifndef FREERDP_CHANNEL_RDPEAR_CLIENT_PACKET_H
#define FREERDP_CHANNEL_RDPEAR_CLIENT_PACKET_H

/* [MS-RDPEAR] 2.2.1.1 TSRemoteGuardPacket: ProtocolMagic (4 bytes), Length (4 bytes),
 * Version (4 bytes), Reserved (4 bytes), TsPkgContext (8 bytes), then the payload. */
#define RDPEAR_PROTOCOL_MAGIC 0x4EACC3C8
#define RDPEAR_PACKET_HEADER_LENGTH 24
#define RDPEAR_PACKET_CONTEXT_OFFSET 16

#endif /* FREERDP_CHANNEL_RDPEAR_CLIENT_PACKET_H */
