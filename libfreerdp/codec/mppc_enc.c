/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Implements Microsoft Point to Point Compression (MPPC) protocol
 *
 * Copyright 2012 Laxmikant Rashinkar <LK.Rashinkar@gmail.com>
 * Copyright 2012 Jay Sorg <jay.sorg@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winpr/crt.h>

#include <freerdp/codec/mppc_dec.h>
#include <freerdp/codec/mppc_enc.h>

#define MPPC_ENC_DEBUG 0

/* local defines */

#define RDP_40_HIST_BUF_LEN (1024 * 8) /* RDP 4.0 uses 8K history buf */
#define RDP_50_HIST_BUF_LEN (1024 * 64) /* RDP 5.0 uses 64K history buf */

#define CRC_INIT 0xFFFF
#define CRC(crcval, newchar) crcval = (crcval >> 8) ^ crc_table[(crcval ^ newchar) & 0x00ff]

/* CRC16 defs */
static const UINT16 crc_table[256] =
{
	0x0000, 0x1189, 0x2312, 0x329b, 0x4624, 0x57ad, 0x6536, 0x74bf,
	0x8c48, 0x9dc1, 0xaf5a, 0xbed3, 0xca6c, 0xdbe5, 0xe97e, 0xf8f7,
	0x1081, 0x0108, 0x3393, 0x221a, 0x56a5, 0x472c, 0x75b7, 0x643e,
	0x9cc9, 0x8d40, 0xbfdb, 0xae52, 0xdaed, 0xcb64, 0xf9ff, 0xe876,
	0x2102, 0x308b, 0x0210, 0x1399, 0x6726, 0x76af, 0x4434, 0x55bd,
	0xad4a, 0xbcc3, 0x8e58, 0x9fd1, 0xeb6e, 0xfae7, 0xc87c, 0xd9f5,
	0x3183, 0x200a, 0x1291, 0x0318, 0x77a7, 0x662e, 0x54b5, 0x453c,
	0xbdcb, 0xac42, 0x9ed9, 0x8f50, 0xfbef, 0xea66, 0xd8fd, 0xc974,
	0x4204, 0x538d, 0x6116, 0x709f, 0x0420, 0x15a9, 0x2732, 0x36bb,
	0xce4c, 0xdfc5, 0xed5e, 0xfcd7, 0x8868, 0x99e1, 0xab7a, 0xbaf3,
	0x5285, 0x430c, 0x7197, 0x601e, 0x14a1, 0x0528, 0x37b3, 0x263a,
	0xdecd, 0xcf44, 0xfddf, 0xec56, 0x98e9, 0x8960, 0xbbfb, 0xaa72,
	0x6306, 0x728f, 0x4014, 0x519d, 0x2522, 0x34ab, 0x0630, 0x17b9,
	0xef4e, 0xfec7, 0xcc5c, 0xddd5, 0xa96a, 0xb8e3, 0x8a78, 0x9bf1,
	0x7387, 0x620e, 0x5095, 0x411c, 0x35a3, 0x242a, 0x16b1, 0x0738,
	0xffcf, 0xee46, 0xdcdd, 0xcd54, 0xb9eb, 0xa862, 0x9af9, 0x8b70,
	0x8408, 0x9581, 0xa71a, 0xb693, 0xc22c, 0xd3a5, 0xe13e, 0xf0b7,
	0x0840, 0x19c9, 0x2b52, 0x3adb, 0x4e64, 0x5fed, 0x6d76, 0x7cff,
	0x9489, 0x8500, 0xb79b, 0xa612, 0xd2ad, 0xc324, 0xf1bf, 0xe036,
	0x18c1, 0x0948, 0x3bd3, 0x2a5a, 0x5ee5, 0x4f6c, 0x7df7, 0x6c7e,
	0xa50a, 0xb483, 0x8618, 0x9791, 0xe32e, 0xf2a7, 0xc03c, 0xd1b5,
	0x2942, 0x38cb, 0x0a50, 0x1bd9, 0x6f66, 0x7eef, 0x4c74, 0x5dfd,
	0xb58b, 0xa402, 0x9699, 0x8710, 0xf3af, 0xe226, 0xd0bd, 0xc134,
	0x39c3, 0x284a, 0x1ad1, 0x0b58, 0x7fe7, 0x6e6e, 0x5cf5, 0x4d7c,
	0xc60c, 0xd785, 0xe51e, 0xf497, 0x8028, 0x91a1, 0xa33a, 0xb2b3,
	0x4a44, 0x5bcd, 0x6956, 0x78df, 0x0c60, 0x1de9, 0x2f72, 0x3efb,
	0xd68d, 0xc704, 0xf59f, 0xe416, 0x90a9, 0x8120, 0xb3bb, 0xa232,
	0x5ac5, 0x4b4c, 0x79d7, 0x685e, 0x1ce1, 0x0d68, 0x3ff3, 0x2e7a,
	0xe70e, 0xf687, 0xc41c, 0xd595, 0xa12a, 0xb0a3, 0x8238, 0x93b1,
	0x6b46, 0x7acf, 0x4854, 0x59dd, 0x2d62, 0x3ceb, 0x0e70, 0x1ff9,
	0xf78f, 0xe606, 0xd49d, 0xc514, 0xb1ab, 0xa022, 0x92b9, 0x8330,
	0x7bc7, 0x6a4e, 0x58d5, 0x495c, 0x3de3, 0x2c6a, 0x1ef1, 0x0f78
};

/*****************************************************************************
                     insert 2 bits into outputBuffer
******************************************************************************/
#define insert_2_bits(_data) \
do \
{ \
	if ((bits_left >= 3) && (bits_left <= 8)) \
	{ \
		i = bits_left - 2; \
		outputBuffer[opb_index] |= _data << i; \
		bits_left = i; \
	} \
	else \
	{ \
		i = 2 - bits_left; \
		j = 8 - i; \
		outputBuffer[opb_index++] |= _data >> i; \
		outputBuffer[opb_index] |= _data << j; \
		bits_left = j; \
	} \
} while (0)

/*****************************************************************************
                     insert 3 bits into outputBuffer
******************************************************************************/
#define insert_3_bits(_data) \
do \
{ \
	if ((bits_left >= 4) && (bits_left <= 8)) \
	{ \
		i = bits_left - 3; \
		outputBuffer[opb_index] |= _data << i; \
		bits_left = i; \
	} \
	else \
	{ \
		i = 3 - bits_left; \
		j = 8 - i; \
		outputBuffer[opb_index++] |= _data >> i; \
		outputBuffer[opb_index] |= _data << j; \
		bits_left = j; \
	} \
} while (0)

/*****************************************************************************
                     insert 4 bits into outputBuffer
******************************************************************************/
#define insert_4_bits(_data) \
do \
{ \
	if ((bits_left >= 5) && (bits_left <= 8)) \
	{ \
		i = bits_left - 4; \
		outputBuffer[opb_index] |= _data << i; \
		bits_left = i; \
	} \
	else \
	{ \
		i = 4 - bits_left; \
		j = 8 - i; \
		outputBuffer[opb_index++] |= _data >> i; \
		outputBuffer[opb_index] |= _data << j; \
		bits_left = j; \
	} \
} while (0)

/*****************************************************************************
                     insert 5 bits into outputBuffer
******************************************************************************/
#define insert_5_bits(_data) \
do \
{ \
	if ((bits_left >= 6) && (bits_left <= 8)) \
	{ \
		i = bits_left - 5; \
		outputBuffer[opb_index] |= _data << i; \
		bits_left = i; \
	} \
	else \
	{ \
		i = 5 - bits_left; \
		j = 8 - i; \
		outputBuffer[opb_index++] |= _data >> i; \
		outputBuffer[opb_index] |= _data << j; \
		bits_left = j; \
	} \
} while (0)

/*****************************************************************************
                     insert 6 bits into outputBuffer
******************************************************************************/
#define insert_6_bits(_data) \
do \
{ \
	if ((bits_left >= 7) && (bits_left <= 8)) \
	{ \
		i = bits_left - 6; \
		outputBuffer[opb_index] |= (_data << i); \
		bits_left = i; \
	} \
	else \
	{ \
		i = 6 - bits_left; \
		j = 8 - i; \
		outputBuffer[opb_index++] |= (_data >> i); \
		outputBuffer[opb_index] |= (_data << j); \
		bits_left = j; \
	} \
} while (0)

/*****************************************************************************
                     insert 7 bits into outputBuffer
******************************************************************************/
#define insert_7_bits(_data) \
do \
{ \
	if (bits_left == 8) \
	{ \
		outputBuffer[opb_index] |= _data << 1; \
		bits_left = 1; \
	} \
	else \
	{ \
		i = 7 - bits_left; \
		j = 8 - i; \
		outputBuffer[opb_index++] |= _data >> i; \
		outputBuffer[opb_index] |= _data << j; \
		bits_left = j; \
	} \
} while (0)

/*****************************************************************************
                     insert 8 bits into outputBuffer
******************************************************************************/
#define insert_8_bits(_data) \
do \
{ \
	if (bits_left == 8) \
	{ \
		outputBuffer[opb_index++] |= _data; \
		bits_left = 8; \
	} \
	else \
	{ \
		i = 8 - bits_left; \
		j = 8 - i; \
		outputBuffer[opb_index++] |= _data >> i; \
		outputBuffer[opb_index] |= _data << j; \
		bits_left = j; \
	} \
} while (0)

/*****************************************************************************
                     insert 9 bits into outputBuffer
******************************************************************************/
#define insert_9_bits(_data16) \
do \
{ \
	i = 9 - bits_left; \
	j = 8 - i; \
	outputBuffer[opb_index++] |= (char) (_data16 >> i); \
	outputBuffer[opb_index] |= (char) (_data16 << j); \
	bits_left = j; \
	if (bits_left == 0) \
	{ \
		opb_index++; \
		bits_left = 8; \
	} \
} while (0)

/*****************************************************************************
                     insert 10 bits into outputBuffer
******************************************************************************/
#define insert_10_bits(_data16) \
do \
{ \
	i = 10 - bits_left; \
	if ((bits_left >= 3) && (bits_left <= 8)) \
	{ \
		j = 8 - i; \
		outputBuffer[opb_index++] |= (char) (_data16 >> i); \
		outputBuffer[opb_index] |= (char) (_data16 << j); \
		bits_left = j; \
	} \
	else \
	{ \
		j = i - 8; \
		k = 8 - j; \
		outputBuffer[opb_index++] |= (char) (_data16 >> i); \
		outputBuffer[opb_index++] |= (char) (_data16 >> j); \
		outputBuffer[opb_index] |= (char) (_data16 << k); \
		bits_left = k; \
	} \
} while (0)

/*****************************************************************************
                     insert 11 bits into outputBuffer
******************************************************************************/
#define insert_11_bits(_data16) \
do \
{ \
	i = 11 - bits_left; \
	if ((bits_left >= 4) && (bits_left <= 8)) \
	{ \
		j = 8 - i; \
		outputBuffer[opb_index++] |= (char) (_data16 >> i); \
		outputBuffer[opb_index] |= (char) (_data16 << j); \
		bits_left = j; \
	} \
	else \
	{ \
		j = i - 8;                                \
		k = 8 - j; \
		outputBuffer[opb_index++] |= (char) (_data16 >> i); \
		outputBuffer[opb_index++] |= (char) (_data16 >> j); \
		outputBuffer[opb_index] |= (char) (_data16 << k); \
		bits_left = k; \
	} \
} while (0)

/*****************************************************************************
                     insert 12 bits into outputBuffer
******************************************************************************/
#define insert_12_bits(_data16) \
do \
{ \
	i = 12 - bits_left; \
	if ((bits_left >= 5) && (bits_left <= 8)) \
	{ \
		j = 8 - i; \
		outputBuffer[opb_index++] |= (char) (_data16 >> i); \
		outputBuffer[opb_index] |= (char) (_data16 << j); \
		bits_left = j; \
	} \
	else \
	{ \
		j = i - 8; \
		k = 8 - j; \
		outputBuffer[opb_index++] |= (char) (_data16 >> i); \
		outputBuffer[opb_index++] |= (char) (_data16 >> j); \
		outputBuffer[opb_index] |= (char) (_data16 << k); \
		bits_left = k; \
	} \
} while (0)

/*****************************************************************************
                     insert 13 bits into outputBuffer
******************************************************************************/
#define insert_13_bits(_data16) \
do \
{ \
	i = 13 - bits_left; \
	if ((bits_left >= 6) && (bits_left <= 8)) \
	{ \
		j = 8 - i; \
		outputBuffer[opb_index++] |= (char) (_data16 >> i); \
		outputBuffer[opb_index] |= (char) (_data16 << j); \
		bits_left = j; \
	} \
	else \
	{ \
		j = i - 8; \
		k = 8 - j; \
		outputBuffer[opb_index++] |= (char) (_data16 >> i); \
		outputBuffer[opb_index++] |= (char) (_data16 >> j); \
		outputBuffer[opb_index] |= (char) (_data16 << k); \
		bits_left = k; \
	} \
} while (0)

/*****************************************************************************
                     insert 14 bits into outputBuffer
******************************************************************************/
#define insert_14_bits(_data16) \
do \
{ \
	i = 14 - bits_left; \
	if ((bits_left >= 7) && (bits_left <= 8)) \
	{ \
		j = 8 - i; \
		outputBuffer[opb_index++] |= (char) (_data16 >> i); \
		outputBuffer[opb_index] |= (char) (_data16 << j); \
		bits_left = j; \
	} \
	else \
	{ \
		j = i - 8; \
		k = 8 - j; \
		outputBuffer[opb_index++] |= (char) (_data16 >> i); \
		outputBuffer[opb_index++] |= (char) (_data16 >> j); \
		outputBuffer[opb_index] |= (char) (_data16 << k); \
		bits_left = k; \
	} \
} while (0)

/*****************************************************************************
                     insert 15 bits into outputBuffer
******************************************************************************/
#define insert_15_bits(_data16) \
do \
{ \
	i = 15 - bits_left; \
	if (bits_left == 8) \
	{ \
		j = 8 - i; \
		outputBuffer[opb_index++] |= (char) (_data16 >> i); \
		outputBuffer[opb_index] |= (char) (_data16 << j); \
		bits_left = j; \
	} \
	else \
	{ \
		j = i - 8; \
		k = 8 - j; \
		outputBuffer[opb_index++] |= (char) (_data16 >> i); \
		outputBuffer[opb_index++] |= (char) (_data16 >> j); \
		outputBuffer[opb_index] |= (char) (_data16 << k); \
		bits_left = k; \
	} \
} while (0)

/*****************************************************************************
                     insert 16 bits into outputBuffer
******************************************************************************/
#define insert_16_bits(_data16) \
do \
{ \
	i = 16 - bits_left; \
	j = i - 8; \
	k = 8 - j; \
	outputBuffer[opb_index++] |= (char) (_data16 >> i); \
	outputBuffer[opb_index++] |= (char) (_data16 >> j); \
	outputBuffer[opb_index] |= (char) (_data16 << k); \
	bits_left = k; \
} while (0)

#if MPPC_ENC_DEBUG
#define DLOG(_args) printf _args
#else
#define DLOG(_args) do { } while (0)
#endif

/**
 * Initialize mppc_enc structure
 *
 * @param   protocol_type   PROTO_RDP_40 or PROTO_RDP_50
 *
 * @return  struct rdp_mppc_enc* or nil on failure
 */

struct rdp_mppc_enc* mppc_enc_new(int protocol_type)
{
	struct rdp_mppc_enc* enc;

	enc = (struct rdp_mppc_enc*) malloc(sizeof(struct rdp_mppc_enc));
	ZeroMemory(enc, sizeof(struct rdp_mppc_enc));

	if (enc == NULL)
		return NULL;

	switch (protocol_type)
	{
		case PROTO_RDP_40:
			enc->protocol_type = PROTO_RDP_40;
			enc->buf_len = RDP_40_HIST_BUF_LEN;
			break;

		case PROTO_RDP_50:
			enc->protocol_type = PROTO_RDP_50;
			enc->buf_len = RDP_50_HIST_BUF_LEN;
			break;

		default:
			free(enc);
			return NULL;
	}

	enc->first_pkt = 1;
	enc->historyBuffer = (char*) malloc(enc->buf_len);
	ZeroMemory(enc->historyBuffer, enc->buf_len);

	if (enc->historyBuffer == NULL)
	{
		free(enc);
		return NULL;
	}

	enc->outputBufferPlus = (char*) malloc(enc->buf_len + 64);
	ZeroMemory(enc->outputBufferPlus, enc->buf_len + 64);

	if (enc->outputBufferPlus == NULL)
	{
		free(enc->historyBuffer);
		free(enc);
		return NULL;
	}

	enc->outputBuffer = enc->outputBufferPlus + 64;
	enc->hash_table = (UINT16*) malloc(enc->buf_len * 2);
	ZeroMemory(enc->hash_table, enc->buf_len * 2);

	if (enc->hash_table == NULL)
	{
		free(enc->historyBuffer);
		free(enc->outputBufferPlus);
		free(enc);
		return NULL;
	}

	return enc;
}

/**
 * deinit mppc_enc structure
 *
 * @param   enc  struct to be deinited
 */

void mppc_enc_free(struct rdp_mppc_enc* enc)
{
	if (enc == NULL)
		return;
	free(enc->historyBuffer);
	free(enc->outputBufferPlus);
	free(enc->hash_table);
	free(enc);
}

/**
 * encode (compress) data
 *
 * @param   enc           encoder state info
 * @param   srcData       uncompressed data
 * @param   len           length of srcData
 *
 * @return  TRUE on success, FALSE on failure
 */

BOOL compress_rdp(struct rdp_mppc_enc* enc, BYTE* srcData, int len)
{
	if ((enc == NULL) || (srcData == NULL) || (len <= 0) || (len > enc->buf_len))
		return FALSE;

	switch (enc->protocol_type)
	{
		case PROTO_RDP_40:
			return compress_rdp_4(enc, srcData, len);
			break;

		case PROTO_RDP_50:
			return compress_rdp_5(enc, srcData, len);
			break;
	}

	return FALSE;
}

/**
 * encode (compress) data using RDP 4.0 protocol
 *
 * @param   enc           encoder state info
 * @param   srcData       uncompressed data
 * @param   len           length of srcData
 *
 * @return  TRUE on success, FALSE on failure
 */

BOOL compress_rdp_4(struct rdp_mppc_enc* enc, BYTE* srcData, int len)
{
	/* RDP 4.0 encoding not yet implemented */
	return FALSE;
}

/**
 * encode (compress) data using RDP 5.0 protocol using hash table
 *
 * @param   enc           encoder state info
 * @param   srcData       uncompressed data
 * @param   len           length of srcData
 *
 * @return  TRUE on success, FALSE on failure
 */

BOOL compress_rdp_5(struct rdp_mppc_enc* enc, BYTE* srcData, int len)
{
	char* outputBuffer;     /* points to enc->outputBuffer */
	char* hptr_end;         /* points to end of history data */
	char* historyPointer;   /* points to first byte of srcData in historyBuffer */
	char* hbuf_start;       /* points to start of history buffer */
	char* cptr1;
	char* cptr2;
	int opb_index;          /* index into outputBuffer */
	int bits_left;          /* unused bits in current byte in outputBuffer */
	UINT32 copy_offset;     /* pattern match starts here... */
	UINT32 lom;             /* ...and matches this many bytes */
	int last_crc_index;     /* don't compute CRC beyond this index */
	UINT16 *hash_table;     /* hash table for pattern matching */

	UINT32 i;
	UINT32 j;
	UINT32 k;
	UINT32 x;
	BYTE  data;
	UINT16 data16;
	UINT32 historyOffset;
	UINT16 crc;
	UINT32 ctr;
	UINT32 saved_ctr;
	UINT32 data_end;
	BYTE byte_val;

	crc = 0;
	opb_index = 0;
	bits_left = 8;
	copy_offset = 0;
	hash_table = enc->hash_table;
	hbuf_start = enc->historyBuffer;
	outputBuffer = enc->outputBuffer;
	memset(outputBuffer, 0, len);
	enc->flags = PACKET_COMPR_TYPE_64K;
	if (enc->first_pkt)
	{
		enc->first_pkt = 0;
		enc->flagsHold |= PACKET_AT_FRONT;
	}

	if ((enc->historyOffset + len) > enc->buf_len)
	{
		/* historyBuffer cannot hold srcData - rewind it */
		enc->historyOffset = 0;
		enc->flagsHold |= PACKET_AT_FRONT;
		memset(hash_table, 0, enc->buf_len * 2);
	}

	/* point to next free byte in historyBuffer */
	historyOffset = enc->historyOffset;

	/* add / append new data to historyBuffer */
	memcpy(&(enc->historyBuffer[historyOffset]), srcData, len);

	/* point to start of data to be compressed */
	historyPointer = &(enc->historyBuffer[historyOffset]);

	ctr = copy_offset = lom = 0;

	/* if we are at start of history buffer, do not attempt to compress */
	/* first 2 bytes,because minimum LoM is 3                           */
	if (historyOffset == 0)
	{
		/* encode first two bytes are literals */
		for (x = 0; x < 2; x++)
		{
			data = *(historyPointer + x);
			DLOG(("%.2x ", (unsigned char) data));
			if (data & 0x80)
			{
				/* insert encoded literal */
				insert_2_bits(0x02);
				data &= 0x7f;
				insert_7_bits(data);
			}
			else
			{
				/* insert literal */
				insert_8_bits(data);
			}
		}

		/* store hash for first two entries in historyBuffer */
		crc = CRC_INIT;
		byte_val = enc->historyBuffer[0];
		CRC(crc, byte_val);
		byte_val = enc->historyBuffer[1];
		CRC(crc, byte_val);
		byte_val = enc->historyBuffer[2];
		CRC(crc, byte_val);
		hash_table[crc] = 0;

		crc = CRC_INIT;
		byte_val = enc->historyBuffer[1];
		CRC(crc, byte_val);
		byte_val = enc->historyBuffer[2];
		CRC(crc, byte_val);
		byte_val = enc->historyBuffer[3];
		CRC(crc, byte_val);
		hash_table[crc] = 1;

		/* first two bytes have already been processed */
		ctr = 2;
	}

	enc->historyOffset += len;

	/* point to last byte in new data */
	hptr_end = &(enc->historyBuffer[enc->historyOffset - 1]);

	/* do not compute CRC beyond this */
	last_crc_index = enc->historyOffset - 3;

	/* do not search for pattern match beyond this */
	data_end = len - 2;

	/* start compressing data */

	while (ctr < data_end)
	{
		cptr1 = historyPointer + ctr;

		crc = CRC_INIT;
		byte_val = *cptr1;
		CRC(crc, byte_val);
		byte_val = *(cptr1 + 1);
		CRC(crc, byte_val);
		byte_val = *(cptr1 + 2);
		CRC(crc, byte_val);

		/* cptr2 points to start of pattern match */
		cptr2 = hbuf_start + hash_table[crc];
		copy_offset = cptr1 - cptr2;

		/* save current entry */
		hash_table[crc] = cptr1 - hbuf_start;

		/* double check that we have a pattern match */
		if ((*cptr1 != *cptr2) ||
			(*(cptr1 + 1) != *(cptr2 + 1)) ||
			(*(cptr1 + 2) != *(cptr2 + 2)))
		{
			/* no match found; encode literal byte */
			data = *cptr1;

			DLOG(("%.2x ", (unsigned char) data));
			if (data < 0x80)
			{
				/* literal byte < 0x80 */
				insert_8_bits(data);
			}
			else
			{
				/* literal byte >= 0x80 */
				insert_2_bits(0x02);
				data &= 0x7f;
				insert_7_bits(data);
			}
			ctr++;
			continue;
		}

		/* we have a match - compute Length of Match */
		cptr1 += 3;
		cptr2 += 3;
		lom = 3;
		while ((cptr1 <= hptr_end) && (*(cptr1++) == *(cptr2++)))
		{
			lom++;
		}
		saved_ctr = ctr + lom;
		DLOG(("<%d: %ld,%d> ",  (historyPointer + ctr) - hbuf_start, copy_offset, lom));

		/* compute CRC for matching segment and store in hash table */

		cptr1 = historyPointer + ctr;
		if (cptr1 + lom > hbuf_start + last_crc_index)
		{
			/* we have gone beyond last_crc_index - go back */
			j = last_crc_index - (cptr1 - hbuf_start);
		}
		else
		{
			j = lom - 1;
		}
		ctr++;
		for (i = 0; i < j; i++)
		{
			cptr1 = historyPointer + ctr;

			/* compute CRC on triplet */
			crc = CRC_INIT;
			byte_val = *(cptr1++);
			CRC(crc, byte_val);
			byte_val = *(cptr1++);
			CRC(crc, byte_val);
			byte_val = *(cptr1++);
			CRC(crc, byte_val);

			/* save current entry */
			hash_table[crc] = (cptr1 - 3) - hbuf_start;

			/* point to next triplet */
			ctr++;
		}
		ctr = saved_ctr;

		/* encode copy_offset and insert into output buffer */

		if (copy_offset <= 63) /* (copy_offset >= 0) is always true */
		{
			/* insert binary header */
			data = 0x1f;
			insert_5_bits(data);

			/* insert 6 bits of copy_offset */
			data = (char) (copy_offset & 0x3f);
			insert_6_bits(data);
		}
		else if ((copy_offset >= 64) && (copy_offset <= 319))
		{
			/* insert binary header */
			data = 0x1e;
			insert_5_bits(data);

			/* insert 8 bits of copy offset */
			data = (char) (copy_offset - 64);
			insert_8_bits(data);
		}
		else if ((copy_offset >= 320) && (copy_offset <= 2367))
		{
			/* insert binary header */
			data = 0x0e;
			insert_4_bits(data);

			/* insert 11 bits of copy offset */
			data16 = copy_offset - 320;;
			insert_11_bits(data16);
		}
		else
		{
			/* copy_offset is 2368+ */

			/* insert binary header */
			data = 0x06;
			insert_3_bits(data);

			/* insert 16 bits of copy offset */
			data16 = copy_offset - 2368;;
			insert_16_bits(data16);
		}

		/* encode length of match and insert into output buffer */

		if (lom == 3)
		{
			/* binary header is 'zero'; since outputBuffer is zero */
			/* filled, all we have to do is update bits_left */
			bits_left--;
			if (bits_left == 0)
			{
				opb_index++;
				bits_left = 8;
			}
		}
		else if ((lom >= 4) && (lom <= 7))
		{
			/* insert binary header */
			data = 0x02;
			insert_2_bits(data);

			/* insert lower 2 bits of LoM */
			data = (char) (lom - 4);
			insert_2_bits(data);
		}
		else if ((lom >= 8) && (lom <= 15))
		{
			/* insert binary header */
			data = 0x06;
			insert_3_bits(data);

			/* insert lower 3 bits of LoM */
			data = (char) (lom - 8);
			insert_3_bits(data);
		}
		else if ((lom >= 16) && (lom <= 31))
		{
			/* insert binary header */
			data = 0x0e;
			insert_4_bits(data);

			/* insert lower 4 bits of LoM */
			data = (char) (lom - 16);
			insert_4_bits(data);
		}
		else if ((lom >= 32) && (lom <= 63))
		{
			/* insert binary header */
			data = 0x1e;
			insert_5_bits(data);

			/* insert lower 5 bits of LoM */
			data = (char) (lom - 32);
			insert_5_bits(data);
		}
		else if ((lom >= 64) && (lom <= 127))
		{
			/* insert binary header */
			data = 0x3e;
			insert_6_bits(data);

			/* insert lower 6 bits of LoM */
			data = (char) (lom - 64);
			insert_6_bits(data);
		}
		else if ((lom >= 128) && (lom <= 255))
		{
			/* insert binary header */
			data = 0x7e;
			insert_7_bits(data);

			/* insert lower 7 bits of LoM */
			data = (char) (lom - 128);
			insert_7_bits(data);
		}
		else if ((lom >= 256) && (lom <= 511))
		{
			/* insert binary header */
			data = 0xfe;
			insert_8_bits(data);

			/* insert lower 8 bits of LoM */
			data = (char) (lom - 256);
			insert_8_bits(data);
		}
		else if ((lom >= 512) && (lom <= 1023))
		{
			/* insert binary header */
			data16 = 0x1fe;
			insert_9_bits(data16);

			/* insert lower 9 bits of LoM */
			data16 = lom - 512;
			insert_9_bits(data16);
		}
		else if ((lom >= 1024) && (lom <= 2047))
		{
			/* insert binary header */
			data16 = 0x3fe;
			insert_10_bits(data16);

			/* insert 10 lower bits of LoM */
			data16 = lom - 1024;
			insert_10_bits(data16);
		}
		else if ((lom >= 2048) && (lom <= 4095))
		{
			/* insert binary header */
			data16 = 0x7fe;
			insert_11_bits(data16);

			/* insert 11 lower bits of LoM */
			data16 = lom - 2048;
			insert_11_bits(data16);
		}
		else if ((lom >= 4096) && (lom <= 8191))
		{
			/* insert binary header */
			data16 = 0xffe;
			insert_12_bits(data16);

			/* insert 12 lower bits of LoM */
			data16 = lom - 4096;
			insert_12_bits(data16);
		}
		else if ((lom >= 8192) && (lom <= 16383))
		{
			/* insert binary header */
			data16 = 0x1ffe;
			insert_13_bits(data16);

			/* insert 13 lower bits of LoM */
			data16 = lom - 8192;
			insert_13_bits(data16);
		}
		else if ((lom >= 16384) && (lom <= 32767))
		{
			/* insert binary header */
			data16 = 0x3ffe;
			insert_14_bits(data16);

			/* insert 14 lower bits of LoM */
			data16 = lom - 16384;
			insert_14_bits(data16);
		}
		else if ((lom >= 32768) && (lom <= 65535))
		{
			/* insert binary header */
			data16 = 0x7ffe;
			insert_15_bits(data16);

			/* insert 15 lower bits of LoM */
			data16 = lom - 32768;
			insert_15_bits(data16);
		}
	} /* end while (ctr < data_end) */

	/* add remaining data to the output */
	while (len - ctr > 0)
	{
		data = srcData[ctr];
		DLOG(("%.2x ", (unsigned char) data));
		if (data < 0x80)
		{
			/* literal byte < 0x80 */
			insert_8_bits(data);
		}
		else
		{
			/* literal byte >= 0x80 */
			insert_2_bits(0x02);
			data &= 0x7f;
			insert_7_bits(data);
		}
		ctr++;
	}

	/* if bits_left == 8, opb_index has already been incremented */
	if ((bits_left == 8) && (opb_index > len))
	{
		/* compressed data longer than uncompressed data */
		/* give up */
		enc->historyOffset = 0;
		memset(hash_table, 0, enc->buf_len * 2);
		enc->flagsHold |= PACKET_FLUSHED;
		enc->first_pkt = 1;
		return TRUE;
	}
	else if (opb_index + 1 > len)
	{
		/* compressed data longer than uncompressed data */
		/* give up */
		enc->historyOffset = 0;
		memset(hash_table, 0, enc->buf_len * 2);
		enc->flagsHold |= PACKET_FLUSHED;
		enc->first_pkt = 1;
		return TRUE;
	}

	/* if bits_left != 8, increment opb_index, which is zero indexed */
	if (bits_left != 8)
	{
		opb_index++;
	}

	if (opb_index > len)
	{
		/* give up */
		enc->historyOffset = 0;
		memset(hash_table, 0, enc->buf_len * 2);
		enc->flagsHold |= PACKET_FLUSHED;
		enc->first_pkt = 1;
		return TRUE;
	}
	enc->flags |= PACKET_COMPRESSED;
	enc->bytes_in_opb = opb_index;

	enc->flags |= enc->flagsHold;
	enc->flagsHold = 0;
	DLOG(("\n"));

	return TRUE;
}
