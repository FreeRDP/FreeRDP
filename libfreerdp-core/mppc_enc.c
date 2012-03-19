/**
 * FreeRDP: A Remote Desktop Protocol Client
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

#include "rdp.h"
#include "mppc_enc.h"
#include <freerdp/utils/memory.h>

#define MPPC_ENC_DEBUG 0

/* local defines */

#define RDP_40_HIST_BUF_LEN (1024 * 8) /* RDP 4.0 uses 8K history buf */
#define RDP_50_HIST_BUF_LEN (1024 * 64) /* RDP 5.0 uses 64K history buf */

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

struct rdp_mppc_enc* rdp_mppc_enc_new(int protocol_type)
{
	struct rdp_mppc_enc* enc;

	enc = xnew(struct rdp_mppc_enc);
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
			xfree(enc);
			return NULL;
	}
	enc->first_pkt = 1;
	enc->historyBuffer = (char*) xzalloc(enc->buf_len);
	if (enc->historyBuffer == NULL)
	{
		xfree(enc);
		return NULL;
	}
	enc->outputBufferPlus = (char*) xzalloc(enc->buf_len + 32);
	if (enc->outputBufferPlus == NULL)
	{
		xfree(enc->historyBuffer);
		xfree(enc);
		return NULL;
	}
	enc->outputBuffer = enc->outputBufferPlus + 32;
	return enc;
}

/**
 * deinit mppc_enc structure
 *
 * @param   enc  struct to be deinited
 */

void rdp_mppc_enc_free(struct rdp_mppc_enc* enc)
{
	if (enc == NULL)
		return;
	xfree(enc->historyBuffer);
	xfree(enc->outputBufferPlus);
	xfree(enc);
}

/**
 * encode (compress) data
 *
 * @param   enc           encoder state info
 * @param   srcData       uncompressed data
 * @param   len           length of srcData
 *
 * @return  true on success, false on failure
 */

boolean compress_rdp(struct rdp_mppc_enc* enc, uint8* srcData, int len)
{
	if ((enc == NULL) || (srcData == NULL) || (len <= 0) || (len > enc->buf_len))
		return false;
	switch (enc->protocol_type)
	{
		case PROTO_RDP_40:
			return compress_rdp_4(enc, srcData, len);
			break;
		case PROTO_RDP_50:
			return compress_rdp_5(enc, srcData, len);
			break;
	}
	return false;
}

/**
 * encode (compress) data using RDP 4.0 protocol
 *
 * @param   enc           encoder state info
 * @param   srcData       uncompressed data
 * @param   len           length of srcData
 *
 * @return  true on success, false on failure
 */

boolean compress_rdp_4(struct rdp_mppc_enc* enc, uint8* srcData, int len)
{
	/* RDP 4.0 encoding not yet implemented */
	return false;
}

/**
 * encode (compress) data using RDP 5.0 protocol
 *
 * @param   enc           encoder state info
 * @param   srcData       uncompressed data
 * @param   len           length of srcData
 *
 * @return  true on success, false on failure
 */

boolean compress_rdp_5(struct rdp_mppc_enc* enc, uint8* srcData, int len)
{
	char* outputBuffer;     /* points to enc->outputBuffer */
	char* hptr_end;         /* points to end of history data */
	char* historyPointer;   /* points to first byte of srcData in historyBuffer */
	char* hbuf_start;       /* points to start of history buffer */
	char* cptr1;
	char* cptr2;
	char* cptr3;
	char* index_ptr;
	int opb_index;          /* index into outputBuffer */
	int bits_left;          /* unused bits in current byte in outputBuffer */

	uint32 copy_offset; /* pattern match starts here... */
	uint32 lom;         /* ...and matches this many bytes */
	uint32 saved_lom;
	uint32 i;
	uint32 j;
	uint32 k;
	uint32 x;
	uint8 data;
	uint16 data16;

	opb_index = 0;
	bits_left = 8;
	copy_offset = 0;
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
	}

	/* add / append new data to historyBuffer */
	memcpy(&enc->historyBuffer[enc->historyOffset], srcData, len);
	historyPointer = &enc->historyBuffer[enc->historyOffset];

	/* if we are at start of history buffer, do not attempt to compress */
	/* first 2 bytes,because minimum LoM is 3                           */
	if (enc->historyOffset == 0)
	{
		for (x = 0; x < 2; x++)
		{
			data = *(historyPointer++);
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
	}
	enc->historyOffset += len;

	/* point to last byte in new data */
	hptr_end = &enc->historyBuffer[enc->historyOffset - 1];

	while (historyPointer <= hptr_end)
	{
		/* do not search beyond index_ptr */
		index_ptr = (historyPointer == hptr_end) ? historyPointer - 2 : historyPointer - 1;

		cptr1 = hbuf_start;
		lom = 0;
		saved_lom = 0;
keep_looking:
		/* look for first byte match */
		while ((cptr1 <= index_ptr) && (*cptr1 != *historyPointer))
		{
			cptr1++;
		}

		if (cptr1 > index_ptr)
		{
			/* no match found */
			if (saved_lom)
			{
				goto insert_tuple;
			}
			goto insert_literal;
		}

		/* got a one byte match - continue looking */
		lom = 1;
		cptr1++;
		cptr2 = cptr1;
		cptr3 = historyPointer + 1;
		while (*(cptr2++) == *(cptr3++))
		{
			lom++;
			if (cptr3 > hptr_end)
			{
				break;
			}
		}
		if (lom < 3)
		{
			/* false alarm, keep looking */
			lom = 0;
			goto keep_looking;
		}

		/* we got a match */
		if (lom < saved_lom)
		{
			/* too small, ignore this match */
			goto keep_looking;
		}
		else if (lom >= saved_lom)
		{
			/* this is a longer match, or a match closer to historyPointer */
			saved_lom = lom;
			copy_offset = (historyPointer - cptr1) + 1;
			goto keep_looking;
		}
insert_tuple:
		DLOG(("<%d: %ld,%d> ",  (int) (historyPointer - enc->historyBuffer),
				(long) copy_offset, saved_lom));
		lom = saved_lom;
		historyPointer += lom;

		/*
		** encode copy_offset and insert into output buffer
		*/

		if ((copy_offset >= 0) && (copy_offset <= 63))
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

		/*
		** encode length of match and insert into output buffer
		*/

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
		goto check_len;
insert_literal:
		/* data not found in historyBuffer; encode and place data in output buffer */
		data = *(historyPointer++);
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
check_len:
		/* if bits_left == 8, opb_index has already been incremented */
		if ((bits_left == 8) && (opb_index > len))
		{
			/* compressed data longer than uncompressed data */
			goto give_up;
		}
		else if (opb_index + 1 > len)
		{
			/* compressed data longer than uncompressed data */
			goto give_up;
		}
	} /* end while (historyPointer < hptr_end) */

	/* if bits_left != 8, increment opb_index, which is zero indexed */
	if (bits_left != 8)
	{
		opb_index++;
	}

	if (opb_index > len)
	{
		goto give_up;
	}
	enc->flags |= PACKET_COMPRESSED;
	enc->bytes_in_opb = opb_index;

	enc->flags |= enc->flagsHold;
	enc->flagsHold = 0;
	DLOG(("\n"));
	return true;

give_up:

	enc->flagsHold |= PACKET_FLUSHED;
	enc->bytes_in_opb = 0;

	memset(enc->historyBuffer, 0, enc->buf_len);
	memcpy(enc->historyBuffer, srcData, len);
	enc->historyOffset = len;
	DLOG(("\n"));
	return true;
}
