/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Implements Microsoft Point to Point Compression (MPPC) protocol
 *
 * Copyright 2011 Laxmikant Rashinkar <LK.Rashinkar@gmail.com>
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

#if 0
static uint8 HuffLengthLEC[] =
{
	0x6, 0x6, 0x6, 0x6, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x8, 0x8, 0x8, 0x8,
	0x8, 0x8, 0x8, 0x9, 0x8, 0x9, 0x9, 0x9, 0x9, 0x8, 0x8, 0x9, 0x9, 0x9, 0x9, 0x9,
	0x8, 0x9, 0x9, 0xa, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0xa, 0x9, 0xa, 0xa, 0xa,
	0x9, 0x9, 0x9, 0xa, 0x9, 0xa, 0xa, 0xa, 0x9, 0x9, 0xa, 0xa, 0x9, 0xa, 0x9, 0x9,
	0x8, 0x9, 0x9, 0x9, 0x9, 0xa, 0xa, 0xa, 0x9, 0x9, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa,
	0x9, 0x9, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0x9, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa,
	0x8, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa,
	0x9, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0x9, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0x9,
	0x7, 0x9, 0x9, 0xa, 0x9, 0xa, 0xa, 0xa, 0x9, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa,
	0x9, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa,
	0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xd, 0xa, 0xa, 0xa, 0xa,
	0xa, 0xa, 0xb, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa,
	0x9, 0xa, 0xa, 0xa, 0xa, 0xa, 0x9, 0xa, 0xa, 0xa, 0xa, 0xa, 0x9, 0xa, 0xa, 0xa,
	0x9, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa,
	0x9, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0xa, 0x9, 0xa,
	0x8, 0x9, 0x9, 0xa, 0x9, 0xa, 0xa, 0xa, 0x9, 0xa, 0xa, 0xa, 0x9, 0x9, 0x8, 0x7,
	0xd, 0xd, 0x7, 0x7, 0xa, 0x7, 0x7, 0x6, 0x6, 0x6, 0x6, 0x5, 0x6, 0x6, 0x6, 0x5,
	0x6, 0x5, 0x6, 0x6, 0x6, 0x6, 0x6, 0x6, 0x6, 0x6, 0x6, 0x6, 0x6, 0x6, 0x6, 0x6,
	0x8, 0x5, 0x6, 0x7, 0x7
};
#endif

static uint16 HuffIndexLEC[512] =
{
	0x007b, 0xff1f, 0xff0d, 0xfe27, 0xfe00, 0xff05, 0xff17, 0xfe68, 0x00c5, 0xfe07, 0xff13, 0xfec0, 0xff08, 0xfe18, 0xff1b, 0xfeb3,
	0xfe03, 0x00a2, 0xfe42, 0xff10, 0xfe0b, 0xfe02, 0xfe91, 0xff19, 0xfe80, 0x00e9, 0xfe3a, 0xff15, 0xfe12, 0x0057, 0xfed7, 0xff1d,
	0xff0e, 0xfe35, 0xfe69, 0xff22, 0xff18, 0xfe7a, 0xfe01, 0xff23, 0xff14, 0xfef4, 0xfeb4, 0xfe09, 0xff1c, 0xfec4, 0xff09, 0xfe60,
	0xfe70, 0xff12, 0xfe05, 0xfe92, 0xfea1, 0xff1a, 0xfe0f, 0xff07, 0xfe56, 0xff16, 0xff02, 0xfed8, 0xfee8, 0xff1e, 0xfe1d, 0x003b,
	0xffff, 0xff06, 0xffff, 0xfe71, 0xfe89, 0xffff, 0xffff, 0xfe2c, 0xfe2b, 0xfe20, 0xffff, 0xfebb, 0xfecf, 0xfe08, 0xffff, 0xfee0,
	0xfe0d, 0xffff, 0xfe99, 0xffff, 0xfe04, 0xfeaa, 0xfe49, 0xffff, 0xfe17, 0xfe61, 0xfedf, 0xffff, 0xfeff, 0xfef6, 0xfe4c, 0xffff,
	0xffff, 0xfe87, 0xffff, 0xff24, 0xffff, 0xfe3c, 0xfe72, 0xffff, 0xffff, 0xfece, 0xffff, 0xfefe, 0xffff, 0xfe23, 0xfebc, 0xfe0a,
	0xfea9, 0xffff, 0xfe11, 0xffff, 0xfe82, 0xffff, 0xfe06, 0xfe9a, 0xfef5, 0xffff, 0xfe22, 0xfe4d, 0xfe5f, 0xffff, 0xff03, 0xfee1,
	0xffff, 0xfeca, 0xfecc, 0xffff, 0xfe19, 0xffff, 0xfeb7, 0xffff, 0xffff, 0xfe83, 0xfe29, 0xffff, 0xffff, 0xffff, 0xfe6c, 0xffff,
	0xfeed, 0xffff, 0xffff, 0xfe46, 0xfe5c, 0xfe15, 0xffff, 0xfedb, 0xfea6, 0xffff, 0xffff, 0xfe44, 0xffff, 0xfe0c, 0xffff, 0xfe95,
	0xfefc, 0xffff, 0xffff, 0xfeb8, 0x16c9, 0xffff, 0xfef0, 0xffff, 0xfe38, 0xffff, 0xffff, 0xfe6d, 0xfe7e, 0xffff, 0xffff, 0xffff,
	0xffff, 0xfe5b, 0xfedc, 0xffff, 0xffff, 0xfeec, 0xfe47, 0xfe1f, 0xffff, 0xfe7f, 0xfe96, 0xffff, 0xffff, 0xfea5, 0xffff, 0xfe10,
	0xfe40, 0xfe32, 0xfebf, 0xffff, 0xffff, 0xfed4, 0xfef1, 0xffff, 0xffff, 0xffff, 0xfe75, 0xffff, 0xffff, 0xfe8d, 0xfe31, 0xffff,
	0xfe65, 0xfe1b, 0xffff, 0xfee4, 0xfefb, 0xffff, 0xffff, 0xfe52, 0xffff, 0xfe0e, 0xffff, 0xfe9d, 0xfeaf, 0xffff, 0xffff, 0xfe51,
	0xfed3, 0xffff, 0xff20, 0xffff, 0xfe2f, 0xffff, 0xffff, 0xfec1, 0xfe8c, 0xffff, 0xffff, 0xffff, 0xfe3f, 0xffff, 0xffff, 0xfe76,
	0xffff, 0xfefa, 0xfe53, 0xfe25, 0xffff, 0xfe64, 0xfee5, 0xffff, 0xffff, 0xfeae, 0xffff, 0xfe13, 0xffff, 0xfe88, 0xfe9e, 0xffff,
	0xfe43, 0xffff, 0xffff, 0xfea4, 0xfe93, 0xffff, 0xffff, 0xffff, 0xfe3d, 0xffff, 0xffff, 0xfeeb, 0xfed9, 0xffff, 0xfe14, 0xfe5a,
	0xffff, 0xfe28, 0xfe7d, 0xffff, 0xffff, 0xfe6a, 0xffff, 0xffff, 0xff01, 0xfec6, 0xfec8, 0xffff, 0xffff, 0xfeb5, 0xffff, 0xffff,
	0xffff, 0xfe94, 0xfe78, 0xffff, 0xffff, 0xffff, 0xfea3, 0xffff, 0xffff, 0xfeda, 0xfe58, 0xffff, 0xfe1e, 0xfe45, 0xfeea, 0xffff,
	0xfe6b, 0xffff, 0xffff, 0xfe37, 0xffff, 0xffff, 0xffff, 0xfe7c, 0xfeb6, 0xffff, 0xffff, 0xfef8, 0xffff, 0xffff, 0xffff, 0xfec7,
	0xfe9b, 0xffff, 0xffff, 0xffff, 0xfe50, 0xffff, 0xffff, 0xfead, 0xfee2, 0xffff, 0xfe1a, 0xfe63, 0xfe4e, 0xffff, 0xffff, 0xfef9,
	0xffff, 0xfe73, 0xffff, 0xffff, 0xffff, 0xfe30, 0xfe8b, 0xffff, 0xffff, 0xfebd, 0xfe2e, 0x0100, 0xffff, 0xfeee, 0xfed2, 0xffff,
	0xffff, 0xffff, 0xfeac, 0xffff, 0xffff, 0xfe9c, 0xfe84, 0xffff, 0xfe24, 0xfe4f, 0xfef7, 0xffff, 0xffff, 0xfee3, 0xfe62, 0xffff,
	0xffff, 0xffff, 0xffff, 0xfe8a, 0xfe74, 0xffff, 0xffff, 0xfe3e, 0xffff, 0xffff, 0xffff, 0xfed1, 0xfebe, 0xffff, 0xffff, 0xfe2d,
	0xffff, 0xfe4a, 0xfef3, 0xffff, 0xffff, 0xfedd, 0xfe5e, 0xfe16, 0xffff, 0xfe48, 0xfea8, 0xffff, 0xfeab, 0xfe97, 0xffff, 0xffff,
	0xfed0, 0xffff, 0xffff, 0xfecd, 0xfeb9, 0xffff, 0xffff, 0xffff, 0xfe2a, 0xffff, 0xffff, 0xfe86, 0xfe6e, 0xffff, 0xffff, 0xffff,
	0xfede, 0xffff, 0xffff, 0xfe5d, 0xfe4b, 0xfe21, 0xffff, 0xfeef, 0xfe98, 0xffff, 0xffff, 0xfe81, 0xffff, 0xffff, 0xffff, 0xfea7,
	0xffff, 0xfeba, 0xfefd, 0xffff, 0xffff, 0xffff, 0xfecb, 0xffff, 0xffff, 0xfe6f, 0xfe39, 0xffff, 0xffff, 0xffff, 0xfe85, 0xffff,
	0x010c, 0xfee6, 0xfe67, 0xfe1c, 0xffff, 0xfe54, 0xfeb2, 0xffff, 0xffff, 0xfe9f, 0xffff, 0xffff, 0xffff, 0xfe59, 0xfeb1, 0xffff,
	0xfec2, 0xffff, 0xffff, 0xfe36, 0xfef2, 0xffff, 0xffff, 0xfed6, 0xfe77, 0xffff, 0xffff, 0xffff, 0xfe33, 0xffff, 0xffff, 0xfe8f,
	0xfe55, 0xfe26, 0x010a, 0xff04, 0xfee7, 0xffff, 0x0121, 0xfe66, 0xffff, 0xffff, 0xffff, 0xfeb0, 0xfea0, 0xffff, 0x010f, 0xfe90,
	0xffff, 0xffff, 0xfed5, 0xffff, 0xffff, 0xfec3, 0xfe34, 0xffff, 0xffff, 0xffff, 0xfe8e, 0xffff, 0x0111, 0xfe79, 0xfe41, 0x010b
};

static uint16 LECHTab[] =
{
	511, 0, 508, 448, 494, 347, 486, 482
};

#if 0
static uint8 HuffLenLOM[] =
{
	0x4, 0x2, 0x3, 0x4, 0x3, 0x4, 0x4, 0x5, 0x4, 0x5, 0x5, 0x6, 0x6, 0x7, 0x7, 0x8,
	0x7, 0x8, 0x8, 0x9, 0x9, 0x8, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9, 0x9
};
#endif

static uint16 HuffIndexLOM[] =
{
	0x0fe1, 0x0fe0, 0x0fe2, 0x0fe8, 0x000e, 0x0fe5, 0x0fe4, 0x0fea, 0x0ff1, 0x0fe3, 0x0015, 0x0fe7, 0x0fef, 0x0046, 0x0ff0, 0x0fed,
	0x0fff, 0x0ff7, 0x0ffb, 0x0019, 0x0ffd, 0x0ff4, 0x012c, 0x0feb, 0x0ffe, 0x0ff6, 0x0ffa, 0x0089, 0x0ffc, 0x0ff3, 0x0ff8, 0x0ff2
};

static uint8 LOMHTab[] =
{
	0, 4, 10, 19
};

#if 0
static uint8 CopyOffsetBitsLUT[] =
{
	0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15
};

static uint32 CopyOffsetBaseLUT[] =
{
	13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537,
	2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577, 32769, 49153, 65537
};

static uint8 LoMBitsLUT[] =
{
	0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 6, 6, 8, 8, 14, 14
};

static uint16 LoMBaseLUT[] =
{
	2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 14, 16, 18, 22, 26, 30,
	34, 42, 50, 58, 66, 82, 98, 114, 130, 194, 258, 514, 2, 2
};
#endif

uint16 LEChash(uint16 key)
{
	return ((key & 0x1ff) ^ (key  >> 9) ^ (key >> 4) ^ (key >> 7));
}

uint16 LOMhash(uint16 key)
{
	return ((key & 0x1f) ^ (key  >> 5) ^ (key >> 9));
}
			
uint16 miniLEChash(uint16 key)
{
	uint16 h;
	h = ((((key >> 8) ^ (key & 0xff)) >> 2) & 0xf);
	if(key >> 9)
		h = ~h;
	return (h % 12);
}

uint8 miniLOMhash(uint16 key)
{
	return ((key >> 4) ^ (key >> 6) ^ (key >> 7));
}

uint16 getLECindex(uint16 huff)
{
	uint16 h = HuffIndexLEC[LEChash(huff)];
	if((h ^ huff) >> 9)
		return h & 0x1ff;
	else
		return HuffIndexLEC[LECHTab[miniLEChash(huff)]];
}

uint16 getLOMindex(uint16 huff)
{
	uint16 h = HuffIndexLOM[LOMhash(huff)];
	if((h ^ huff) >> 5)
	{	
		return h & 0x1f;
	}	
	else
		return HuffIndexLOM[LOMHTab[miniLOMhash(huff)]];
}

int decompress_rdp(rdpRdp* rdp, uint8* cbuf, int len, int ctype, uint32* roff, uint32* rlen)
{
	int type = ctype & 0x0f;

	switch (type)
	{
		case PACKET_COMPR_TYPE_8K:
			return decompress_rdp_4(rdp, cbuf, len, ctype, roff, rlen);
			break;

		case PACKET_COMPR_TYPE_64K:
			return decompress_rdp_5(rdp, cbuf, len, ctype, roff, rlen);
			break;

		case PACKET_COMPR_TYPE_RDP6:
			return decompress_rdp_6(rdp, cbuf, len, ctype, roff, rlen);
			break;

		case PACKET_COMPR_TYPE_RDP61:
			return decompress_rdp_61(rdp, cbuf, len, ctype, roff, rlen);
			break;

		default:
			printf("mppc.c: invalid RDP compression code 0x%2.2x\n", type);
			return false;
	}
}

/**
 * decompress RDP 4 data
 *
 * @param rdp     per session information
 * @param cbuf    compressed data
 * @param len     length of compressed data
 * @param ctype   compression flags
 * @param roff    starting offset of uncompressed data
 * @param rlen    length of uncompressed data
 *
 * @return        True on success, False on failure
 */

int decompress_rdp_4(rdpRdp* rdp, uint8* cbuf, int len, int ctype, uint32* roff, uint32* rlen)
{
	uint8*    history_buf;    /* uncompressed data goes here */
	uint8*    history_ptr;    /* points to next free slot in history_buf */
	uint32    d32;            /* we process 4 compressed bytes at a time */
	uint16    copy_offset;    /* location to copy data from */
	uint16    lom;            /* length of match */
	uint8*    src_ptr;        /* used while copying compressed data */
	uint8*    cptr;           /* points to next byte in cbuf */
	uint8     cur_byte;       /* last byte fetched from cbuf */
	int       bits_left;      /* bits left in d34 for processing */
	int       cur_bits_left;  /* bits left in cur_byte for processing */
	int       tmp;
	uint32    i32;

	printf("decompress_rdp_4:\n");

	if ((rdp->mppc == NULL) || (rdp->mppc->history_buf == NULL))
	{
		printf("decompress_rdp_4: null\n");
		return false;
	}

	src_ptr = 0;
	cptr = cbuf;
	copy_offset = 0;
	lom = 0;
	bits_left = 0;
	cur_bits_left = 0;
	d32 = 0;
	cur_byte = 0;
	*rlen = 0;

	/* get start of history buffer */
	history_buf = rdp->mppc->history_buf;

	/* get next free slot in history buffer */
	history_ptr = rdp->mppc->history_ptr;
	*roff = history_ptr - history_buf;

	if (ctype & PACKET_AT_FRONT)
	{
		/* place compressed data at start of history buffer */
		history_ptr = rdp->mppc->history_buf;
		rdp->mppc->history_ptr = rdp->mppc->history_buf;
		*roff = 0;
	}

	if (ctype & PACKET_FLUSHED)
	{
		/* re-init history buffer */
		history_ptr = rdp->mppc->history_buf;
		memset(history_buf, 0, RDP6_HISTORY_BUF_SIZE);
		*roff = 0;
	}

	if ((ctype & PACKET_COMPRESSED) != PACKET_COMPRESSED)
	{
		/* data in cbuf is not compressed - copy to history buf as is */
		memcpy(history_ptr, cbuf, len);
		history_ptr += len;
		*rlen = history_ptr - rdp->mppc->history_ptr;
		rdp->mppc->history_ptr = history_ptr;
		return true;
	}

	/* load initial data */
	tmp = 24;
	while (cptr < cbuf + len)
	{
		i32 = *cptr++;
		d32  |= i32 << tmp;
		bits_left += 8;
		tmp -= 8;
		if (tmp < 0)
		{
			break;
		}
	}

	if (cptr < cbuf + len)
	{
		cur_byte = *cptr++;
		cur_bits_left = 8;
	}
	else
	{
		cur_bits_left = 0;
	}

	/*
	** start uncompressing data in cbuf
	*/

	while (bits_left >= 8)
	{
		/*
		   value 0xxxxxxx  = literal, not encoded
		   value 10xxxxxx  = literal, encoded
		   value 1111xxxx  = copy offset   0 - 63
		   value 1110xxxx  = copy offset  64 - 319
		   value 110xxxxx  = copy offset 320 - 8191
		*/

		/*
		   at this point, we are guaranteed that d32 has 32 bits to
		   be processed, unless we have reached end of cbuf
		*/

		copy_offset = 0;

		if ((d32 & 0x80000000) == 0)
		{
			/* got a literal */
			*history_ptr++ = d32 >> 24;
			d32 <<= 8;
			bits_left -= 8;
		}
		else if ((d32 & 0xc0000000) == 0x80000000)
		{
			/* got encoded literal */
			d32 <<= 2;
			*history_ptr++ = (d32 >> 25) | 0x80;
			d32 <<= 7;
			bits_left -= 9;
		}
		else if ((d32 & 0xf0000000) == 0xf0000000)
		{
			/* got copy offset in range 0 - 63, */
			/* with 6 bit copy offset */
			d32 <<= 4;
			copy_offset = d32 >> 26;
			d32 <<= 6;
			bits_left -= 10;
		}
		else if ((d32 & 0xf0000000) == 0xe0000000)
		{
			/* got copy offset in range 64 - 319, */
			/* with 8 bit copy offset */
			d32 <<= 4;
			copy_offset = d32 >> 24;
			copy_offset += 64;
			d32 <<= 8;
			bits_left -= 12;
		}
		else if ((d32 & 0xe0000000) == 0xc0000000)
		{
			/* got copy offset in range 320 - 8191, */
			/* with 13 bits copy offset */
			d32 <<= 3;
			copy_offset = d32 >> 19;
			copy_offset += 320;
			d32 <<= 13;
			bits_left -= 16;
		}

		/*
		** get more bits before we process length of match
		*/

		/* how may bits do we need to get? */
		tmp = 32 - bits_left;

		while (tmp)
		{
			if (cur_bits_left < tmp)
			{
				/* we have less bits than we need */
				i32 = cur_byte >> (8 - cur_bits_left);
				d32 |= i32 << ((32 - bits_left) - cur_bits_left);
				bits_left += cur_bits_left;
				tmp -= cur_bits_left;
				if (cptr < cbuf + len)
				{
					/* more compressed data available */
					cur_byte = *cptr++;
					cur_bits_left = 8;
				}
				else
				{
					/* no more compressed data available */
					tmp = 0;
					cur_bits_left = 0;
				}
			}
			else if (cur_bits_left > tmp)
			{
				/* we have more bits than we need */
				d32 |= cur_byte >> (8 - tmp);
				cur_byte <<= tmp;
				cur_bits_left -= tmp;
				bits_left = 32;
				break;
			}
			else
			{
				/* we have just the right amount of bits */
				d32 |= cur_byte >> (8 - tmp);
				bits_left = 32;
				if (cptr < cbuf + len)
				{
					cur_byte = *cptr++;
					cur_bits_left = 8;
				}
				else
				{
					cur_bits_left = 0;
				}
				break;
			}
		}

		if (!copy_offset)
		{
			continue;
		}

		/*
		** compute Length of Match
		*/

		/*
		   lengh of match  Encoding (binary header + LoM bits
		   --------------  ----------------------------------
		   3               0
		   4...7           10 + 2 lower bits of L-o-M
		   8...15          110 + 3 lower bits of L-o-M
		   16...31         1110 + 4 lower bits of L-o-M
		   32...63         11110 + 5 lower bits of L-o-M
		   64...127        111110 + 6 lower bits of L-o-M
		   128...255       1111110 + 7 lower bits of L-o-M
		   256...511       11111110 + 8 lower bits of L-o-M
		   512...1023      111111110 + 9 lower bits of L-o-M
		   1024...2047     1111111110 + 10 lower bits of L-o-M
		   2048...4095     11111111110 + 11 lower bits of L-o-M
		   4096...8191     111111111110 + 12 lower bits of L-o-M
		*/

		if ((d32 & 0x80000000) == 0)
		{
			/* lom is fixed to 3 */
			lom = 3;
			d32 <<= 1;
			bits_left -= 1;
		}
		else if ((d32 & 0xc0000000) == 0x80000000)
		{
			/* 2 lower bits of LoM */
			lom = ((d32 >> 28) & 0x03) + 4;
			d32 <<= 4;
			bits_left -= 4;
		}
		else if ((d32 & 0xe0000000) == 0xc0000000)
		{
			/* 3 lower bits of LoM */
			lom = ((d32 >> 26) & 0x07) + 8;
			d32 <<= 6;
			bits_left -= 6;
		}
		else if ((d32 & 0xf0000000) == 0xe0000000)
		{
			/* 4 lower bits of LoM */
			lom = ((d32 >> 24) & 0x0f) + 16;
			d32 <<= 8;
			bits_left -= 8;
		}
		else if ((d32 & 0xf8000000) == 0xf0000000)
		{
			/* 5 lower bits of LoM */
			lom = ((d32 >> 22) & 0x1f) + 32;
			d32 <<= 10;
			bits_left -= 10;
		}
		else if ((d32 & 0xfc000000) == 0xf8000000)
		{
			/* 6 lower bits of LoM */
			lom = ((d32 >> 20) & 0x3f) + 64;
			d32 <<= 12;
			bits_left -= 12;
		}
		else if ((d32 & 0xfe000000) == 0xfc000000)
		{
			/* 7 lower bits of LoM */
			lom = ((d32 >> 18) & 0x7f) + 128;
			d32 <<= 14;
			bits_left -= 14;
		}
		else if ((d32 & 0xff000000) == 0xfe000000)
		{
			/* 8 lower bits of LoM */
			lom = ((d32 >> 16) & 0xff) + 256;
			d32 <<= 16;
			bits_left -= 16;
		}
		else if ((d32 & 0xff800000) == 0xff000000)
		{
			/* 9 lower bits of LoM */
			lom = ((d32 >> 14) & 0x1ff) + 512;
			d32 <<= 18;
			bits_left -= 18;
		}
		else if ((d32 & 0xffc00000) == 0xff800000)
		{
			/* 10 lower bits of LoM */
			lom = ((d32 >> 12) & 0x3ff) + 1024;
			d32 <<= 20;
			bits_left -= 20;
		}
		else if ((d32 & 0xffe00000) == 0xffc00000)
		{
			/* 11 lower bits of LoM */
			lom = ((d32 >> 10) & 0x7ff) + 2048;
			d32 <<= 22;
			bits_left -= 22;
		}
		else if ((d32 & 0xfff00000) == 0xffe00000)
		{
			/* 12 lower bits of LoM */
			lom = ((d32 >> 8) & 0xfff) + 4096;
			d32 <<= 24;
			bits_left -= 24;
		}

		/* now that we have copy_offset and LoM, process them */

		src_ptr = history_ptr - copy_offset;
		if (src_ptr >= rdp->mppc->history_buf)
		{
			/* data does not wrap around */
			while (lom > 0)
			{
				*history_ptr++ = *src_ptr++;
				lom--;
			}
		}
		else
		{
			src_ptr = rdp->mppc->history_buf_end - (copy_offset - (history_ptr - rdp->mppc->history_buf));
			src_ptr++;
			while (lom && (src_ptr <= rdp->mppc->history_buf_end))
			{
				*history_ptr++ = *src_ptr++;
				lom--;
			}

			src_ptr = rdp->mppc->history_buf;
			while (lom > 0)
			{
				*history_ptr++ = *src_ptr++;
				lom--;
			}
		}

		/*
		** get more bits before we restart the loop
		*/

		/* how may bits do we need to get? */
		tmp = 32 - bits_left;

		while (tmp)
		{
			if (cur_bits_left < tmp)
			{
				/* we have less bits than we need */
				i32 = cur_byte >> (8 - cur_bits_left);
				d32 |= i32 << ((32 - bits_left) - cur_bits_left);
				bits_left += cur_bits_left;
				tmp -= cur_bits_left;
				if (cptr < cbuf + len)
				{
					/* more compressed data available */
					cur_byte = *cptr++;
					cur_bits_left = 8;
				}
				else
				{
					/* no more compressed data available */
					tmp = 0;
					cur_bits_left = 0;
				}
			}
			else if (cur_bits_left > tmp)
			{
				/* we have more bits than we need */
				d32 |= cur_byte >> (8 - tmp);
				cur_byte <<= tmp;
				cur_bits_left -= tmp;
				bits_left = 32;
				break;
			}
			else
			{
				/* we have just the right amount of bits */
				d32 |= cur_byte >> (8 - tmp);
				bits_left = 32;
				if (cptr < cbuf + len)
				{
					cur_byte = *cptr++;
					cur_bits_left = 8;
				}
				else
				{
					cur_bits_left = 0;
				}
				break;
			}
		}
	} /* end while (bits_left >= 8) */

	*rlen = history_ptr - rdp->mppc->history_ptr;

	rdp->mppc->history_ptr = history_ptr;

	return true;
}

/**
 * decompress RDP 5 data
 *
 * @param rdp     per session information
 * @param cbuf    compressed data
 * @param len     length of compressed data
 * @param ctype   compression flags
 * @param roff    starting offset of uncompressed data
 * @param rlen    length of uncompressed data
 *
 * @return        True on success, False on failure
 */

int decompress_rdp_5(rdpRdp* rdp, uint8* cbuf, int len, int ctype, uint32* roff, uint32* rlen)
{
	uint8*    history_buf;    /* uncompressed data goes here */
	uint8*    history_ptr;    /* points to next free slot in bistory_buf */
	uint32    d32;            /* we process 4 compressed bytes at a time */
	uint16    copy_offset;    /* location to copy data from */
	uint16    lom;            /* length of match */
	uint8*    src_ptr;        /* used while copying compressed data */
	uint8*    cptr;           /* points to next byte in cbuf */
	uint8     cur_byte;       /* last byte fetched from cbuf */
	int       bits_left;      /* bits left in d32 for processing */
	int       cur_bits_left;  /* bits left in cur_byte for processing */
	int       tmp;
	uint32    i32;

	if ((rdp->mppc == NULL) || (rdp->mppc->history_buf == NULL))
	{
		printf("decompress_rdp_5: null\n");
		return false;
	}

	src_ptr = 0;
	cptr = cbuf;
	copy_offset = 0;
	lom = 0;
	bits_left = 0;
	cur_bits_left = 0;
	d32 = 0;
	cur_byte = 0;
	*rlen = 0;

	/* get start of history buffer */
	history_buf = rdp->mppc->history_buf;

	/* get next free slot in history buffer */
	history_ptr = rdp->mppc->history_ptr;
	*roff = history_ptr - history_buf;

	if (ctype & PACKET_AT_FRONT)
	{
		/* place compressed data at start of history buffer */
		history_ptr = rdp->mppc->history_buf;
		rdp->mppc->history_ptr = rdp->mppc->history_buf;
		*roff = 0;
	}

	if (ctype & PACKET_FLUSHED)
	{
		/* re-init history buffer */
		history_ptr = rdp->mppc->history_buf;
		memset(history_buf, 0, RDP6_HISTORY_BUF_SIZE);
		*roff = 0;
	}

	if ((ctype & PACKET_COMPRESSED) != PACKET_COMPRESSED)
	{
		/* data in cbuf is not compressed - copy to history buf as is */
		memcpy(history_ptr, cbuf, len);
		history_ptr += len;
		*rlen = history_ptr - rdp->mppc->history_ptr;
		rdp->mppc->history_ptr = history_ptr;
		return true;
	}

	/* load initial data */
	tmp = 24;
	while (cptr < cbuf + len)
	{
		i32 = *cptr++;
		d32  |= i32 << tmp;
		bits_left += 8;
		tmp -= 8;
		if (tmp < 0)
		{
			break;
		}
	}

	if (cptr < cbuf + len)
	{
		cur_byte = *cptr++;
		cur_bits_left = 8;
	}
	else
	{
		cur_bits_left = 0;
	}

	/*
	** start uncompressing data in cbuf
	*/

	while (bits_left >= 8)
	{
		/*
		   value 0xxxxxxx  = literal, not encoded
		   value 10xxxxxx  = literal, encoded
		   value 11111xxx  = copy offset     0 - 63
		   value 11110xxx  = copy offset    64 - 319
		   value 1110xxxx  = copy offset   320 - 2367
		   value 110xxxxx  = copy offset  2368+
		*/

		/*
		   at this point, we are guaranteed that d32 has 32 bits to
		   be processed, unless we have reached end of cbuf
		*/

		copy_offset = 0;

		if ((d32 & 0x80000000) == 0)
		{
			/* got a literal */
			*history_ptr++ = d32 >> 24;
			d32 <<= 8;
			bits_left -= 8;
		}
		else if ((d32 & 0xc0000000) == 0x80000000)
		{
			/* got encoded literal */
			d32 <<= 2;
			*history_ptr++ = (d32 >> 25) | 0x80;
			d32 <<= 7;
			bits_left -= 9;
		}
		else if ((d32 & 0xf8000000) == 0xf8000000)
		{
			/* got copy offset in range 0 - 63, */
			/* with 6 bit copy offset */
			d32 <<= 5;
			copy_offset = d32 >> 26;
			d32 <<= 6;
			bits_left -= 11;
		}
		else if ((d32 & 0xf8000000) == 0xf0000000)
		{
			/* got copy offset in range 64 - 319, */
			/* with 8 bit copy offset */
			d32 <<= 5;
			copy_offset = d32 >> 24;
			copy_offset += 64;
			d32 <<= 8;
			bits_left -= 13;
		}
		else if ((d32 & 0xf0000000) == 0xe0000000)
		{
			/* got copy offset in range 320 - 2367, */
			/* with 11 bits copy offset */
			d32 <<= 4;
			copy_offset = d32 >> 21;
			copy_offset += 320;
			d32 <<= 11;
			bits_left -= 15;
		}
		else if ((d32 & 0xe0000000) == 0xc0000000)
		{
			/* got copy offset in range 2368+, */
			/* with 16 bits copy offset */
			d32 <<= 3;
			copy_offset = d32 >> 16;
			copy_offset += 2368;
			d32 <<= 16;
			bits_left -= 19;
		}

		/*
		** get more bits before we process length of match
		*/

		/* how may bits do we need to get? */
		tmp = 32 - bits_left;

		while (tmp)
		{
			if (cur_bits_left < tmp)
			{
				/* we have less bits than we need */
				i32 = cur_byte >> (8 - cur_bits_left);
				d32 |= i32 << ((32 - bits_left) - cur_bits_left);
				bits_left += cur_bits_left;
				tmp -= cur_bits_left;
				if (cptr < cbuf + len)
				{
					/* more compressed data available */
					cur_byte = *cptr++;
					cur_bits_left = 8;
				}
				else
				{
					/* no more compressed data available */
					tmp = 0;
					cur_bits_left = 0;
				}
			}
			else if (cur_bits_left > tmp)
			{
				/* we have more bits than we need */
				d32 |= cur_byte >> (8 - tmp);
				cur_byte <<= tmp;
				cur_bits_left -= tmp;
				bits_left = 32;
				break;
			}
			else
			{
				/* we have just the right amount of bits */
				d32 |= cur_byte >> (8 - tmp);
				bits_left = 32;
				if (cptr < cbuf + len)
				{
					cur_byte = *cptr++;
					cur_bits_left = 8;
				}
				else
				{
					cur_bits_left = 0;
				}
				break;
			}
		}

		if (!copy_offset)
		{
			continue;
		}

		/*
		** compute Length of Match
		*/

		/*
		   lengh of match  Encoding (binary header + LoM bits
		   --------------  ----------------------------------
		   3               0
		   4..7            10 + 2 lower bits of LoM
		   8..15           110 + 3 lower bits of LoM
		   16..31          1110 + 4 lower bits of LoM
		   32..63          1111-0 + 5 lower bits of LoM
		   64..127         1111-10 + 6 lower bits of LoM
		   128..255        1111-110 + 7 lower bits of LoM
		   256..511        1111-1110 + 8 lower bits of LoM
		   512..1023       1111-1111-0 + 9 lower bits of LoM
		   1024..2047      1111-1111-10 + 10 lower bits of LoM
		   2048..4095      1111-1111-110 + 11 lower bits of LoM
		   4096..8191      1111-1111-1110 + 12 lower bits of LoM
		   8192..16383     1111-1111-1111-0 + 13 lower bits of LoM
		   16384..32767    1111-1111-1111-10 + 14 lower bits of LoM
		   32768..65535    1111-1111-1111-110 + 15 lower bits of LoM
		*/

		if ((d32 & 0x80000000) == 0)
		{
			/* lom is fixed to 3 */
			lom = 3;
			d32 <<= 1;
			bits_left -= 1;
		}
		else if ((d32 & 0xc0000000) == 0x80000000)
		{
			/* 2 lower bits of LoM */
			lom = ((d32 >> 28) & 0x03) + 4;
			d32 <<= 4;
			bits_left -= 4;
		}
		else if ((d32 & 0xe0000000) == 0xc0000000)
		{
			/* 3 lower bits of LoM */
			lom = ((d32 >> 26) & 0x07) + 8;
			d32 <<= 6;
			bits_left -= 6;
		}
		else if ((d32 & 0xf0000000) == 0xe0000000)
		{
			/* 4 lower bits of LoM */
			lom = ((d32 >> 24) & 0x0f) + 16;
			d32 <<= 8;
			bits_left -= 8;
		}
		else if ((d32 & 0xf8000000) == 0xf0000000)
		{
			/* 5 lower bits of LoM */
			lom = ((d32 >> 22) & 0x1f) + 32;
			d32 <<= 10;
			bits_left -= 10;
		}
		else if ((d32 & 0xfc000000) == 0xf8000000)
		{
			/* 6 lower bits of LoM */
			lom = ((d32 >> 20) & 0x3f) + 64;
			d32 <<= 12;
			bits_left -= 12;
		}
		else if ((d32 & 0xfe000000) == 0xfc000000)
		{
			/* 7 lower bits of LoM */
			lom = ((d32 >> 18) & 0x7f) + 128;
			d32 <<= 14;
			bits_left -= 14;
		}
		else if ((d32 & 0xff000000) == 0xfe000000)
		{
			/* 8 lower bits of LoM */
			lom = ((d32 >> 16) & 0xff) + 256;
			d32 <<= 16;
			bits_left -= 16;
		}
		else if ((d32 & 0xff800000) == 0xff000000)
		{
			/* 9 lower bits of LoM */
			lom = ((d32 >> 14) & 0x1ff) + 512;
			d32 <<= 18;
			bits_left -= 18;
		}
		else if ((d32 & 0xffc00000) == 0xff800000)
		{
			/* 10 lower bits of LoM */
			lom = ((d32 >> 12) & 0x3ff) + 1024;
			d32 <<= 20;
			bits_left -= 20;
		}
		else if ((d32 & 0xffe00000) == 0xffc00000)
		{
			/* 11 lower bits of LoM */
			lom = ((d32 >> 10) & 0x7ff) + 2048;
			d32 <<= 22;
			bits_left -= 22;
		}
		else if ((d32 & 0xfff00000) == 0xffe00000)
		{
			/* 12 lower bits of LoM */
			lom = ((d32 >> 8) & 0xfff) + 4096;
			d32 <<= 24;
			bits_left -= 24;
		}
		else if ((d32 & 0xfff80000) == 0xfff00000)
		{
			/* 13 lower bits of LoM */
			lom = ((d32 >> 6) & 0x1fff) + 8192;
			d32 <<= 26;
			bits_left -= 26;
		}
		else if ((d32 & 0xfffc0000) == 0xfff80000)
		{
			/* 14 lower bits of LoM */
			lom = ((d32 >> 4) & 0x3fff) + 16384;
			d32 <<= 28;
			bits_left -= 28;
		}
		else if ((d32 & 0xfffe0000) == 0xfffc0000)
		{
			/* 15 lower bits of LoM */
			lom = ((d32 >> 2) & 0x7fff) + 32768;
			d32 <<= 30;
			bits_left -= 30;
		}

		/* now that we have copy_offset and LoM, process them */

		src_ptr = history_ptr - copy_offset;
		if (src_ptr >= rdp->mppc->history_buf)
		{
			/* data does not wrap around */
			while (lom > 0)
			{
				*history_ptr++ = *src_ptr++;
				lom--;
			}
		}
		else
		{
			src_ptr = rdp->mppc->history_buf_end - (copy_offset - (history_ptr - rdp->mppc->history_buf));
			src_ptr++;
			while (lom && (src_ptr <= rdp->mppc->history_buf_end))
			{
				*history_ptr++ = *src_ptr++;
				lom--;
			}

			src_ptr = rdp->mppc->history_buf;
			while (lom > 0)
			{
				*history_ptr++ = *src_ptr++;
				lom--;
			}
		}

		/*
		** get more bits before we restart the loop
		*/

		/* how may bits do we need to get? */
		tmp = 32 - bits_left;

		while (tmp)
		{
			if (cur_bits_left < tmp)
			{
				/* we have less bits than we need */
				i32 = cur_byte >> (8 - cur_bits_left);
				d32 |= i32 << ((32 - bits_left) - cur_bits_left);
				bits_left += cur_bits_left;
				tmp -= cur_bits_left;
				if (cptr < cbuf + len)
				{
					/* more compressed data available */
					cur_byte = *cptr++;
					cur_bits_left = 8;
				}
				else
				{
					/* no more compressed data available */
					tmp = 0;
					cur_bits_left = 0;
				}
			}
			else if (cur_bits_left > tmp)
			{
				/* we have more bits than we need */
				d32 |= cur_byte >> (8 - tmp);
				cur_byte <<= tmp;
				cur_bits_left -= tmp;
				bits_left = 32;
				break;
			}
			else
			{
				/* we have just the right amount of bits */
				d32 |= cur_byte >> (8 - tmp);
				bits_left = 32;
				if (cptr < cbuf + len)
				{
					cur_byte = *cptr++;
					cur_bits_left = 8;
				}
				else
				{
					cur_bits_left = 0;
				}
				break;
			}
		}

	} /* end while (cptr < cbuf + len) */

	*rlen = history_ptr - rdp->mppc->history_ptr;

	rdp->mppc->history_ptr = history_ptr;

	return true;
}

/**
 * decompress RDP 6 data
 *
 * @param rdp     per session information
 * @param cbuf    compressed data
 * @param len     length of compressed data
 * @param ctype   compression flags
 * @param roff    starting offset of uncompressed data
 * @param rlen    length of uncompressed data
 *
 * @return        True on success, False on failure
 */

int decompress_rdp_6(rdpRdp* rdp, uint8* cbuf, int len, int ctype, uint32* roff, uint32* rlen)
{
	uint8*    history_buf;    /* uncompressed data goes here */
	uint16*   offset_cache;	  /* Copy Offset cache */
	uint8*    history_ptr;    /* points to next free slot in bistory_buf */
	uint32    d32;            /* we process 4 compressed bytes at a time */
	uint16    copy_offset;    /* location to copy data from */
	uint16    lom;            /* length of match */
	uint8*    src_ptr;        /* used while copying compressed data */
	uint8*    cptr;           /* points to next byte in cbuf */
	uint8     cur_byte;       /* last byte fetched from cbuf */
	int       bits_left;      /* bits left in d32 for processing */
	int       cur_bits_left;  /* bits left in cur_byte for processing */
	int       tmp;
	uint32    i32;

	if ((rdp->mppc == NULL) || (rdp->mppc->history_buf == NULL))
	{
		printf("decompress_rdp_6: null\n");
		return false;
	}

	src_ptr = 0;
	cptr = cbuf;
	copy_offset = 0;
	lom = 0;
	bits_left = 0;
	cur_bits_left = 0;
	d32 = 0;
	cur_byte = 0;
	*rlen = 0;

	/* get start of history buffer */
	history_buf = rdp->mppc->history_buf;

	/* get start of offset_cache */
	offset_cache = rdp->mppc->offset_cache;

	/* get next free slot in history buffer */
	history_ptr = rdp->mppc->history_ptr;
	*roff = history_ptr - history_buf;

	if (ctype & PACKET_AT_FRONT)
	{
		printf("need to look later\n");
		/* slid history_buf and reset history_buf to middle */
		memcpy(history_buf, (history_buf + 32768), (history_ptr -history_buf - 32768));
		memcpy((history_buf + (history_ptr - history_buf - 32768)), cbuf, len);
		history_ptr = history_buf + 32768;
		*roff = 32768;
	}
	
	if (ctype & PACKET_FLUSHED)
	{
		/* re-init history buffer */
		history_ptr = rdp->mppc->history_buf;
		memset(history_buf, 0, RDP6_HISTORY_BUF_SIZE);
		memset(offset_cache, 0, RDP6_OFFSET_CACHE_SIZE);
		*roff = 0;
	}

	if ((ctype & PACKET_COMPRESSED) != PACKET_COMPRESSED)
	{
		/* data in cbuf is not compressed - copy to history buf as is */
		memcpy(history_ptr, cbuf, len);
		history_ptr += len;
		*rlen = history_ptr - rdp->mppc->history_ptr;
		rdp->mppc->history_ptr = history_ptr;
		return true;
	}

	/* load initial data */
	tmp = 24;
	while (cptr < cbuf + len)
	{
		i32 = *cptr++;
		d32  |= i32 << tmp;
		bits_left += 8;
		tmp -= 8;
		if (tmp < 0)
		{
			break;
		}
	}

	if (cptr < cbuf + len)
	{
		cur_byte = *cptr++;
		cur_bits_left = 8;
	}
	else
	{
		cur_bits_left = 0;
	}

	/*
	** start uncompressing data in cbuf
	*/


	return true;
}

/**
 * decompress RDP 6.1 data
 *
 * @param rdp     per session information
 * @param cbuf    compressed data
 * @param len     length of compressed data
 * @param ctype   compression flags
 * @param roff    starting offset of uncompressed data
 * @param rlen    length of uncompressed data
 *
 * @return        True on success, False on failure
 */

int decompress_rdp_61(rdpRdp* rdp, uint8* cbuf, int len, int ctype, uint32* roff, uint32* rlen)
{
	return false;
}

/**
 * allocate space to store history buffer
 *
 * @param rdp rdp struct that contains rdp_mppc struct
 * @return pointer to new struct, or NULL on failure
 */

struct rdp_mppc* mppc_new(rdpRdp* rdp)
{
	struct rdp_mppc* ptr;

	ptr = (struct rdp_mppc*) xmalloc(sizeof(struct rdp_mppc));

	if (!ptr)
	{
		printf("mppc_new(): system out of memory\n");
		return NULL;
	}

	ptr->history_buf = (uint8*) xmalloc(RDP6_HISTORY_BUF_SIZE);
	ptr->offset_cache = (uint16*) xzalloc(RDP6_OFFSET_CACHE_SIZE);

	if (!ptr->history_buf)
	{
		printf("mppc_new(): system out of memory\n");
		xfree(ptr);
		return NULL;
	}

	ptr->history_ptr = ptr->history_buf;
	ptr->history_buf_end = ptr->history_buf + RDP6_HISTORY_BUF_SIZE - 1;

	return ptr;
}

/**
 * free history buffer
 *
 * @param rdp rdp struct that contains rdp_mppc struct
 */

void mppc_free(rdpRdp* rdp)
{
	if (!rdp->mppc)
	{
		return;
	}

	if (rdp->mppc->history_buf)
	{
		xfree(rdp->mppc->history_buf);
		rdp->mppc->history_buf = NULL;
		rdp->mppc->history_ptr = NULL;
	}

	if (rdp->mppc->offset_cache)
	{
		xfree(rdp->mppc->offset_cache);
	}

	xfree(rdp->mppc);
}
