/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Connection Sequence
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

#include "connection.h"

/**
 *                                      Connection Sequence
 *     client                                                                    server
 *        |                                                                         |
 *        |-----------------------X.224 Connection Request PDU--------------------->|
 *        |<----------------------X.224 Connection Confirm PDU----------------------|
 *        |-------MCS Connect-Initial PDU with GCC Conference Create Request------->|
 *        |<-----MCS Connect-Response PDU with GCC Conference Create Response-------|
 *        |------------------------MCS Erect Domain Request PDU-------------------->|
 *        |------------------------MCS Attach User Request PDU--------------------->|
 *        |<-----------------------MCS Attach User Confirm PDU----------------------|
 *        |------------------------MCS Channel Join Request PDU-------------------->|
 *        |<-----------------------MCS Channel Join Confirm PDU---------------------|
 *        |----------------------------Security Exchange PDU----------------------->|
 *        |-------------------------------Client Info PDU-------------------------->|
 *
 */

void connection_client_connect(rdpConnection* connection)
{
	int i;
	uint16 channelId;

	nego_init(connection->nego);
	nego_set_target(connection->nego, connection->settings->hostname, 3389);
	nego_set_cookie(connection->nego, connection->settings->username);
	nego_set_protocols(connection->nego, 1, 1, 1);
	nego_connect(connection->nego);

	transport_connect_nla(connection->transport);

	connection->mcs = mcs_new(connection->transport);

	mcs_send_connect_initial(connection->mcs);
	mcs_recv_connect_response(connection->mcs);

	mcs_send_erect_domain_request(connection->mcs);
	mcs_send_attach_user_request(connection->mcs);
	mcs_recv_attach_user_confirm(connection->mcs);

	mcs_send_channel_join_request(connection->mcs, connection->mcs->user_id);
	mcs_recv_channel_join_confirm(connection->mcs);

	for (i = 0; i < connection->settings->num_channels; i++)
	{
		channelId = connection->settings->channels[i].chan_id;
		mcs_send_channel_join_request(connection->mcs, channelId);
		mcs_recv_channel_join_confirm(connection->mcs);
	}

	mcs_recv(connection->mcs);
}

/**
 * Instantiate new connection module.
 * @return new connection module
 */

rdpConnection* connection_new()
{
	rdpConnection* connection;

	connection = (rdpConnection*) xzalloc(sizeof(rdpConnection));

	if (connection != NULL)
	{

	}

	return connection;
}

/**
 * Free connection module.
 * @param connection connect module to be freed
 */

void connection_free(rdpConnection* connection)
{
	if (connection != NULL)
	{
		xfree(connection);
	}
}
