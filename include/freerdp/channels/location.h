/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Location Virtual Channel Extension
 *
 * Copyright 2023 Pascal Nowack <Pascal.Nowack@gmx.de>
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

#ifndef FREERDP_CHANNEL_LOCATION_H
#define FREERDP_CHANNEL_LOCATION_H

#include <freerdp/api.h>
#include <freerdp/dvc.h>
#include <freerdp/types.h>

/** @defgroup channel_location Location Channel
 *  @brief Location channel providing redirection of client side Network/GPS location to the RDP
 * server
 * @{
 */

/** The command line name of the channel
 *
 *  \since version 3.0.0
 */
#define LOCATION_CHANNEL_NAME "location" /** @since version 3.4.0 */

#define LOCATION_DVC_CHANNEL_NAME "Microsoft::Windows::RDS::Location"

#ifdef __cplusplus
extern "C"
{
#endif

	typedef enum
	{
		PDUTYPE_LOC_RESERVED = 0x0000,
		PDUTYPE_SERVER_READY = 0x0001,
		PDUTYPE_CLIENT_READY = 0x0002,
		PDUTYPE_BASE_LOCATION3D = 0x0003,
		PDUTYPE_LOCATION2D_DELTA = 0x0004,
		PDUTYPE_LOCATION3D_DELTA = 0x0005,
	} LOCATION_PDUTYPE;

#define LOCATION_HEADER_SIZE 6

	typedef struct
	{
		LOCATION_PDUTYPE pduType;
		UINT32 pduLength;
	} RDPLOCATION_HEADER;

	typedef enum
	{
		RDPLOCATION_PROTOCOL_VERSION_100 = 0x00010000,
		RDPLOCATION_PROTOCOL_VERSION_200 = 0x00020000,
	} RDPLOCATION_PROTOCOL_VERSION;

	typedef struct
	{
		RDPLOCATION_HEADER header;
		RDPLOCATION_PROTOCOL_VERSION protocolVersion;
		UINT32 flags;
	} RDPLOCATION_SERVER_READY_PDU;

	typedef struct
	{
		RDPLOCATION_HEADER header;
		RDPLOCATION_PROTOCOL_VERSION protocolVersion;
		UINT32 flags;
	} RDPLOCATION_CLIENT_READY_PDU;

	typedef enum
	{
		LOCATIONSOURCE_IP = 0x00,
		LOCATIONSOURCE_WIFI = 0x01,
		LOCATIONSOURCE_CELL = 0x02,
		LOCATIONSOURCE_GNSS = 0x03,
	} LOCATIONSOURCE;

	typedef struct
	{
		RDPLOCATION_HEADER header;
		double latitude;
		double longitude;
		INT32 altitude;
		double* speed;
		double* heading;
		double* horizontalAccuracy;
		LOCATIONSOURCE* source;
	} RDPLOCATION_BASE_LOCATION3D_PDU;

	typedef struct
	{
		RDPLOCATION_HEADER header;
		double latitudeDelta;
		double longitudeDelta;
		double* speedDelta;
		double* headingDelta;
	} RDPLOCATION_LOCATION2D_DELTA_PDU;

	typedef struct
	{
		RDPLOCATION_HEADER header;
		double latitudeDelta;
		double longitudeDelta;
		INT32 altitudeDelta;
		double* speedDelta;
		double* headingDelta;
	} RDPLOCATION_LOCATION3D_DELTA_PDU;

#ifdef __cplusplus
}
#endif
/** @} */

#endif /* FREERDP_CHANNEL_LOCATION_H */
