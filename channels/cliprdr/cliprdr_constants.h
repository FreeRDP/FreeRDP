/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Clipboard Virtual Channel
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2010-2011 Vic Lee
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

#ifndef __CLIPRDR_CONSTANTS
#define __CLIPRDR_CONSTANTS

/* CLIPRDR_HEADER.msgType */
#define CB_MONITOR_READY		0x0001
#define CB_FORMAT_LIST			0x0002
#define CB_FORMAT_LIST_RESPONSE		0x0003
#define CB_FORMAT_DATA_REQUEST		0x0004
#define CB_FORMAT_DATA_RESPONSE		0x0005
#define CB_TEMP_DIRECTORY		0x0006
#define CB_CLIP_CAPS			0x0007
#define CB_FILECONTENTS_REQUEST		0x0008
#define CB_FILECONTENTS_RESPONSE	0x0009
#define CB_LOCK_CLIPDATA		0x000A
#define CB_UNLOCK_CLIPDATA		0x000B

/* CLIPRDR_HEADER.msgFlags */
#define CB_RESPONSE_OK			0x0001
#define CB_RESPONSE_FAIL		0x0002
#define CB_ASCII_NAMES			0x0004

/* CLIPRDR_CAPS_SET.capabilitySetType */
#define CB_CAPSTYPE_GENERAL		0x0001

/* CLIPRDR_GENERAL_CAPABILITY.lengthCapability */
#define CB_CAPSTYPE_GENERAL_LEN		12

/* CLIPRDR_GENERAL_CAPABILITY.version */
#define CB_CAPS_VERSION_1		0x00000001
#define CB_CAPS_VERSION_2		0x00000002

/* CLIPRDR_GENERAL_CAPABILITY.generalFlags */
#define CB_USE_LONG_FORMAT_NAMES	0x00000002
#define CB_STREAM_FILECLIP_ENABLED	0x00000004
#define CB_FILECLIP_NO_FILE_PATHS	0x00000008
#define CB_CAN_LOCK_CLIPDATA		0x00000010

#endif /* __CLIPRDR_CONSTANTS */
