/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RDP Constants
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

/**
 * Protocol Security Negotiation Protocols
 */
enum RDP_NEG_PROTOCOLS
{
	PROTOCOL_RDP = 0x00000000,
	PROTOCOL_TLS = 0x00000001,
	PROTOCOL_NLA = 0x00000002
};

/**
 * Protocol Security Negotiation Failure Codes
 */
enum RDP_NEG_FAILURE_FAILURECODES
{
	SSL_REQUIRED_BY_SERVER = 0x00000001,
	SSL_NOT_ALLOWED_BY_SERVER = 0x00000002,
	SSL_CERT_NOT_ON_SERVER = 0x00000003,
	INCONSISTENT_FLAGS = 0x00000004,
	HYBRID_REQUIRED_BY_SERVER = 0x00000005
};
