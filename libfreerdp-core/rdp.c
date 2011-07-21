/**
 * FreeRDP: A Remote Desktop Protocol Client
 * RDP Core
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

#include "rdp.h"

/**
 * Read RDP Security Header.\n
 * @msdn{cc240579}
 * @param s stream
 * @param flags security flags
 */

void rdp_read_security_header(STREAM* s, uint16* flags)
{
	/* Basic Security Header */
	stream_read_uint16(s, *flags); /* flags */
	stream_seek(s, 2); /* flagsHi (unused) */
}

/**
 * Write RDP Security Header.\n
 * @msdn{cc240579}
 * @param s stream
 * @param flags security flags
 */

void rdp_write_security_header(STREAM* s, uint16 flags)
{
	/* Basic Security Header */
	stream_write_uint16(s, flags); /* flags */
	stream_write_uint16(s, 0); /* flagsHi (unused) */
}

void rdp_read_share_control_header(STREAM* s, uint16* length, uint16* type, uint16* channel_id)
{
	/* Share Control Header */
	stream_read_uint16(s, *length); /* totalLength */
	stream_read_uint16(s, *type); /* pduType */
	stream_read_uint16(s, *channel_id); /* pduSource */
	*type &= 0x0F; /* type is in the 4 least significant bits */
}

void rdp_write_share_control_header(STREAM* s, uint16 length, uint16 type, uint16 channel_id)
{
	/* Share Control Header */
	stream_write_uint16(s, length); /* totalLength */
	stream_write_uint16(s, type); /* pduType */
	stream_write_uint16(s, channel_id); /* pduSource */
}

/**
 * Initialize an RDP packet stream.\n
 * @param rdp rdp module
 * @return
 */

STREAM* rdp_send_stream_init(rdpRdp* rdp)
{
	STREAM* s;
	s = transport_send_stream_init(rdp->transport, 2048);
	stream_seek(s, RDP_PACKET_HEADER_LENGTH);
	return s;
}

/**
 * Write an RDP packet header.\n
 * @param rdp rdp module
 * @param s stream
 * @param length RDP packet length
 */

void rdp_write_header(rdpRdp* rdp, STREAM* s, int length)
{
	mcs_write_domain_mcspdu_header(s, DomainMCSPDU_SendDataRequest, length);
	per_write_integer16(s, rdp->mcs->user_id, MCS_BASE_CHANNEL_ID); /* initiator */
	per_write_integer16(s, MCS_GLOBAL_CHANNEL_ID, 0); /* channelId */
	stream_write_uint8(s, 0x70); /* dataPriority + segmentation */
	per_write_length(s, length - RDP_PACKET_HEADER_LENGTH); /* userData (OCTET_STRING) */
}

/**
 * Send an RDP packet.\n
 * @param rdp RDP module
 * @param s stream
 */

void rdp_send(rdpRdp* rdp, STREAM* s)
{
	int length;

	length = stream_get_length(s);
	stream_set_pos(s, 0);

	rdp_write_header(rdp, s, length);

	stream_set_pos(s, length);
	transport_write(rdp->transport, s);
}

/**
 * Receive an RDP packet.\n
 * @param rdp RDP module
 */

void rdp_recv(rdpRdp* rdp)
{
	STREAM* s;
	int length;
	uint16 pduType;
	uint16 pduLength;
	uint16 initiator;
	uint16 channelId;
	uint16 sec_flags;
	enum DomainMCSPDU MCSPDU;

	s = transport_recv_stream_init(rdp->transport, 4096);
	transport_read(rdp->transport, s);

	MCSPDU = DomainMCSPDU_SendDataIndication;
	mcs_read_domain_mcspdu_header(s, &MCSPDU, &length);

	per_read_integer16(s, &initiator, MCS_BASE_CHANNEL_ID); /* initiator (UserId) */
	per_read_integer16(s, &channelId, 0); /* channelId */
	stream_seek(s, 1); /* dataPriority + Segmentation (0x70) */
	per_read_length(s, &pduLength); /* userData (OCTET_STRING) */

	if (rdp->connected != True)
	{
		rdp_read_security_header(s, &sec_flags);

		if (sec_flags & SEC_PKT_MASK)
		{
			switch (sec_flags & SEC_PKT_MASK)
			{
				case SEC_LICENSE_PKT:
					license_recv(rdp->license, s);
					break;

				case SEC_REDIRECTION_PKT:
					rdp_read_redirection_packet(rdp, s);
					break;

				default:
					printf("incorrect security flags: 0x%04X\n", sec_flags);
					break;
			}
		}
	}
	else
	{
		rdp_read_share_control_header(s, &pduLength, &pduType, &channelId);

		switch (pduType)
		{
			case PDU_TYPE_DEMAND_ACTIVE:
				rdp_read_demand_active(s, rdp->settings);
				rdp_send_confirm_active(rdp);
				break;

			case PDU_TYPE_DEACTIVATE_ALL:
				rdp_read_deactivate_all(s, rdp->settings);
				break;

			case PDU_TYPE_SERVER_REDIRECTION:
				rdp_read_enhanced_security_redirection_packet(rdp, s);
				break;

			default:
				printf("incorrect PDU type: 0x%04X\n", pduType);
				break;
		}
	}
}

/**
 * Instantiate new RDP module.
 * @return new RDP module
 */

rdpRdp* rdp_new()
{
	rdpRdp* rdp;

	rdp = (rdpRdp*) xzalloc(sizeof(rdpRdp));

	if (rdp != NULL)
	{
		rdp->connected = False;
		rdp->settings = settings_new();
		rdp->registry = registry_new(rdp->settings);
		rdp->transport = transport_new(rdp->settings);
		rdp->license = license_new(rdp);
		rdp->nego = nego_new(rdp->transport);
		rdp->mcs = mcs_new(rdp->transport);
	}

	return rdp;
}

/**
 * Free RDP module.
 * @param rdp RDP module to be freed
 */

void rdp_free(rdpRdp* rdp)
{
	if (rdp != NULL)
	{
		settings_free(rdp->settings);
		transport_free(rdp->transport);
		license_free(rdp->license);
		mcs_free(rdp->mcs);
		xfree(rdp);
	}
}

