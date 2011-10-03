/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RemoteFX Codec Library - API Header
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

#ifndef __RFX_CONSTANTS_H
#define __RFX_CONSTANTS_H

/* sync */
#define WF_MAGIC		0xCACCACCA
#define WF_VERSION_1_0		0x0100

/* blockType */
#define WBT_SYNC		0xCCC0
#define WBT_CODEC_VERSIONS	0xCCC1
#define WBT_CHANNELS		0xCCC2
#define WBT_CONTEXT		0xCCC3
#define WBT_FRAME_BEGIN		0xCCC4
#define WBT_FRAME_END		0xCCC5
#define WBT_REGION		0xCCC6
#define WBT_EXTENSION		0xCCC7
#define CBT_REGION		0xCAC1
#define CBT_TILESET		0xCAC2
#define CBT_TILE		0xCAC3

/* tileSize */
#define CT_TILE_64x64		0x0040

/* properties.flags */
#define CODEC_MODE		0x02

/* properties.cct */
#define COL_CONV_ICT		0x1

/* properties.xft */
#define CLW_XFORM_DWT_53_A	0x1

/* properties.et */
#define CLW_ENTROPY_RLGR1	0x01
#define CLW_ENTROPY_RLGR3	0x04

/* properties.qt */
#define SCALAR_QUANTIZATION	0x1

#endif /* __RFX_CONSTANTS_H */

