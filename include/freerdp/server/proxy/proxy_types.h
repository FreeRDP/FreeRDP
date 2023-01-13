/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Proxy enum types
 *
 * Copyright 2023 Armin Novak <armin.novak@thincast.com>
 * Copyright 2023 Thincast Technologies GmbH
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

#ifndef FREERDP_SERVER_PROXY_TYPES_H
#define FREERDP_SERVER_PROXY_TYPES_H

/** @brief how is handled a channel */
typedef enum
{
	PF_UTILS_CHANNEL_NOT_HANDLED, /*!< channel not handled */
	PF_UTILS_CHANNEL_BLOCK,       /*!< block and drop traffic on this channel */
	PF_UTILS_CHANNEL_PASSTHROUGH, /*!< pass traffic from this channel */
	PF_UTILS_CHANNEL_INTERCEPT    /*!< inspect traffic from this channel */
} pf_utils_channel_mode;

/** @brief result of a channel treatment */
typedef enum
{
	PF_CHANNEL_RESULT_PASS, /*!< pass the packet as is */
	PF_CHANNEL_RESULT_DROP, /*!< drop the packet */
	PF_CHANNEL_RESULT_ERROR /*!< error during packet treatment */
} PfChannelResult;
typedef enum
{
	PROXY_FETCH_TARGET_METHOD_DEFAULT,
	PROXY_FETCH_TARGET_METHOD_CONFIG,
	PROXY_FETCH_TARGET_METHOD_LOAD_BALANCE_INFO,
	PROXY_FETCH_TARGET_USE_CUSTOM_ADDR
} ProxyFetchTargetMethod;

#endif /* FREERDP_SERVER_PROXY_TYPES_H */
