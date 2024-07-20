#include <freerdp/peer.h>

#include "../fastpath.h"
#include "../surface.h"
#include "../window.h"
#include "../info.h"
#include "../multitransport.h"

static BOOL test_server(const uint8_t* Data, size_t Size)
{
	freerdp_peer* client = calloc(1, sizeof(freerdp_peer));
	if (!client)
		goto fail;
	client->ContextSize = sizeof(rdpContext);
	if (!freerdp_peer_context_new(client))
		goto fail;

	WINPR_ASSERT(client->context);
	rdpRdp* rdp = client->context->rdp;
	WINPR_ASSERT(rdp);

	wStream sbuffer = { 0 };
	wStream* s = Stream_StaticConstInit(&sbuffer, Data, Size);

	{
		rdpFastPath* fastpath = rdp->fastpath;
		WINPR_ASSERT(fastpath);

		fastpath_recv_updates(fastpath, s);
		fastpath_recv_inputs(fastpath, s);

		UINT16 length = 0;
		fastpath_read_header_rdp(fastpath, s, &length);
		fastpath_decrypt(fastpath, s, &length);
	}

	{
		UINT16 length = 0;
		UINT16 flags = 0;
		UINT16 channelId = 0;
		UINT16 tpktLength = 0;
		UINT16 remainingLength = 0;
		UINT16 type = 0;
		UINT16 securityFlags = 0;
		UINT32 share_id = 0;
		BYTE compressed_type = 0;
		BYTE btype = 0;
		UINT16 compressed_len = 0;
		rdp_read_security_header(rdp, s, &flags, &length);
		rdp_read_header(rdp, s, &length, &channelId);
		rdp_read_share_control_header(rdp, s, &tpktLength, &remainingLength, &type, &channelId);
		rdp_read_share_data_header(rdp, s, &length, &btype, &share_id, &compressed_type,
		                           &compressed_len);
		rdp_recv_message_channel_pdu(rdp, s, securityFlags);
	}
	{
		rdpUpdate* update = rdp->update;
		UINT16 channelId = 0;
		UINT16 length = 0;
		UINT16 pduSource = 0;
		UINT16 pduLength = 0;
		update_recv_order(update, s);
		update_recv_altsec_window_order(update, s);
		update_recv_play_sound(update, s);
		update_recv_pointer(update, s);
		update_recv_surfcmds(update, s);
		rdp_recv_get_active_header(rdp, s, &channelId, &length);
		rdp_recv_demand_active(rdp, s, pduSource, length);
		rdp_recv_confirm_active(rdp, s, pduLength);
	}
	{
		rdpNla* nla = nla_new(rdp->context, rdp->transport);
		nla_recv_pdu(nla, s);
		nla_free(nla);
	}
	{
		rdp_recv_heartbeat_packet(rdp, s);
		rdp->state = CONNECTION_STATE_SECURE_SETTINGS_EXCHANGE;
		rdp_recv_client_info(rdp, s);
	}
	{
		freerdp_is_valid_mcs_create_request(Data, Size);
		freerdp_is_valid_mcs_create_response(Data, Size);
	}
	{
		multitransport_recv_request(rdp->multitransport, s);
		multitransport_recv_response(rdp->multitransport, s);
	}
	{
		autodetect_recv_request_packet(rdp->autodetect, RDP_TRANSPORT_TCP, s);
		autodetect_recv_response_packet(rdp->autodetect, RDP_TRANSPORT_TCP, s);
	}
	{
		rdp_recv_deactivate_all(rdp, s);
		rdp_recv_server_synchronize_pdu(rdp, s);
		rdp_recv_client_synchronize_pdu(rdp, s);
	}
fail:
	freerdp_peer_context_free(client);
	free(client);
	return TRUE;
}

int LLVMFuzzerTestOneInput(const uint8_t* Data, size_t Size)
{
	test_server(Data, Size);
	return 0;
}
