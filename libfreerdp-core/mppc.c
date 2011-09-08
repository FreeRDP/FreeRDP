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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include "rdp.h"

int decompress_rdp(rdpRdp   *rdp, 
		     uint8    *cbuf,
		     int       len,
		     int       ctype,
		     uint32   *roff,
		     uint32   *rlen) 
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
		printf("mppc.c: invalid RDP compression code\n");
		return False;
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

int decompress_rdp_4(rdpRdp   *rdp, 
		     uint8    *cbuf,
		     int       len,
		     int       ctype,
		     uint32   *roff,
		     uint32   *rlen) 
{
	return False;
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

int decompress_rdp_5(rdpRdp   *rdp, 
		     uint8    *cbuf,
		     int       len,
		     int       ctype,
		     uint32   *roff,
		     uint32   *rlen) 
{
	uint8    *history_buf;    /* uncompressed data goes here */
	uint8    *history_ptr;    /* points to next free slot in bistory_buf */
	uint32    d32;            /* we process 4 compressed bytes at a time */
	uint16    copy_offset;    /* location to copy data from */
	uint16    lom;            /* length of match */
	uint8    *src_ptr;        /* used while copying compressed data */
	uint8    *cptr;           /* points to next byte in cbuf */
	uint8     cur_byte;       /* last byte fetched from cbuf */
	int       bits_left;      /* bits left in d32 for processing */
	int       cur_bits_left;  /* bits left in cur_byte for processing */
	int       tmp;
	uint32    i32;

	if ((rdp->mppc == NULL) || (rdp->mppc->history_buf == NULL))
	{
		return False;
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

	mprintf("decompress_rdp_5: *** ctype=0x%x len=%d ***\n", ctype, len);

	/* get start of history buffer */
	history_buf = rdp->mppc->history_buf;

	/* get next free slot in history buffer */
	history_ptr = rdp->mppc->history_ptr;
	*roff = history_ptr - history_buf;

	if (ctype & PACKET_AT_FRONT) 
	{
		/* place compressed data at start of history buffer */
		mprintf("decompress_rdp_5: got PACKET_AT_FRONT\n");
		history_ptr = rdp->mppc->history_buf;
		*roff = 0;
	}

	if (ctype & PACKET_FLUSHED) 
	{
		/* re-init history buffer */
		mprintf("decompress_rdp_5: got PACKET_FLUSHED\n");
		history_ptr = rdp->mppc->history_buf;
		memset(history_buf, 0, RDP5_HISTORY_BUF_SIZE);
		*roff = 0;
	}

	if ((ctype & PACKET_COMPRESSED) != PACKET_COMPRESSED) 
	{
		/* data in cbuf is not compressed - copy to history buf as is */
		mprintf("decompress_rdp_5: got ! PACKET_COMPRESSED\n");
		memcpy(history_ptr, cbuf, len);
		history_ptr += len;
		*rlen = history_ptr - rdp->mppc->history_ptr;
		rdp->mppc->history_ptr = history_ptr;
		return True;
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
			mprintf("decompress_rdp_5: got literal\n");
			*history_ptr++ = d32 >> 24;
			d32 <<= 8;
			bits_left -= 8;
		}
		else if ((d32 & 0xc0000000) == 0x80000000) 
		{
			/* got encoded literal */
			mprintf("decompress_rdp_5: got encoded literal\n");
			d32 <<= 2;
			*history_ptr++ = (d32 >> 25) | 0x80;
			d32 <<= 7;
			bits_left -= 9;
		}
		else if ((d32 & 0xf8000000) == 0xf8000000) 
		{
			/* got copy offset in range 0 - 63, */
			/* with 6 bit copy offset */
			mprintf("decompress_rdp_5: "
				"got copy offset in range 0 - 63\n");
			d32 <<= 5;
			copy_offset = d32 >> 26;
			d32 <<= 6;
			bits_left -= 11;
		}
		else if ((d32 & 0xf8000000) == 0xf0000000) 
		{
			/* got copy offset in range 64 - 319, */
			/* with 8 bit copy offset */
			mprintf("decompress_rdp_5: "
				"got copy offset in range 64 - 319\n");
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
			mprintf("decompress_rdp_5: "
				"got copy offset in range 320 - 2367\n");
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
			mprintf("decompress_rdp_5: "
				"got copy offset in range 2368+\n");
			d32 <<= 3;
			copy_offset = d32 >> 16;
			copy_offset += 2368;
			d32 <<= 16;
			bits_left -= 19;
		}

		/*
		** get more bits before we process length of match
		*/

#if 1
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
		#else
		// optimization code under development

		// LK_TODO either use only i32 or make cur_byte uint32_t
		// make cbuf + len a const variable so we dont compute it 
		// each time

		/* if there are no more bits to get, skip this */
		if (!cur_bits_left)
		{
			goto no_more_bits1;
		}

		/* how may bits do we need to get? */
		tmp = 32 - bits_left;

		while (tmp)
		{
			if (tmp > 8)
			{
				i32 = cur_byte;
				d32 |= i32 << (24 - bits_left);
				bits_left += cur_bits_left;
				tmp -= cur_bits_left;
				if (cptr < cbuf + len) 
				{
					cur_byte = *cptr++;
					cur_bits_left = 8;
				}
				else 
				{
					cur_bits_left = 0;
					goto no_more_bits1;
				}
			}
			else
			{
				i32 = cur_byte;
				d32 |= i32 >> (8 - tmp);
			}
		}
		no_more_bits1:
		#endif

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
			mprintf("decompress_rdp_5: LoM fixed to 3\n");
			lom = 3;
			d32 <<= 1;
			bits_left -= 1;
		}
		else if ((d32 & 0xc0000000) == 0x80000000) 
		{
			/* 2 lower bits of LoM */
			mprintf("decompress_rdp_5: LoM : 2 bits\n");
			lom = ((d32 >> 28) & 0x03) + 4;
			d32 <<= 4;
			bits_left -= 4;
		}
		else if ((d32 & 0xe0000000) == 0xc0000000) 
		{
			/* 3 lower bits of LoM */
			mprintf("decompress_rdp_5: LoM : 3 bits\n");
			lom = ((d32 >> 26) & 0x07) + 8;
			d32 <<= 6;
			bits_left -= 6;
		}
		else if ((d32 & 0xf0000000) == 0xe0000000) 
		{
			/* 4 lower bits of LoM */
			mprintf("decompress_rdp_5: LoM : 4 bits\n");
			lom = ((d32 >> 24) & 0x0f) + 16;
			d32 <<= 8;
			bits_left -= 8;
		}
		else if ((d32 & 0xf8000000) == 0xf0000000) 
		{
			/* 5 lower bits of LoM */
			mprintf("decompress_rdp_5: LoM : 5 bits\n");
			lom = ((d32 >> 22) & 0x1f) + 32;
			d32 <<= 10;
			bits_left -= 10;
		}
		else if ((d32 & 0xfc000000) == 0xf8000000) 
		{
			/* 6 lower bits of LoM */
			mprintf("decompress_rdp_5: LoM : 6 bits\n");
			lom = ((d32 >> 20) & 0x3f) + 64;
			d32 <<= 12;
			bits_left -= 12;
		}
		else if ((d32 & 0xfe000000) == 0xfc000000) 
		{
			/* 7 lower bits of LoM */
			mprintf("decompress_rdp_5: LoM : 7 bits\n");
			lom = ((d32 >> 18) & 0x7f) + 128;
			d32 <<= 14;
			bits_left -= 14;
		}
		else if ((d32 & 0xff000000) == 0xfe000000) 
		{
			/* 8 lower bits of LoM */
			mprintf("decompress_rdp_5: LoM : 8 bits\n");
			lom = ((d32 >> 16) & 0xff) + 256;
			d32 <<= 16;
			bits_left -= 16;
		}
		else if ((d32 & 0xff800000) == 0xff000000) 
		{
			/* 9 lower bits of LoM */
			mprintf("decompress_rdp_5: LoM : 9 bits\n");
			lom = ((d32 >> 14) & 0x1ff) + 512;
			d32 <<= 18;
			bits_left -= 18;
		}
		else if ((d32 & 0xffc00000) == 0xff800000) 
		{
			/* 10 lower bits of LoM */
			mprintf("decompress_rdp_5: LoM : 10 bits\n");
			lom = ((d32 >> 12) & 0x3ff) + 1024;
			d32 <<= 20;
			bits_left -= 20;
		}
		else if ((d32 & 0xffe00000) == 0xffc00000) 
		{
			/* 11 lower bits of LoM */
			mprintf("decompress_rdp_5: LoM : 11 bits\n");
			lom = ((d32 >> 10) & 0x7ff) + 2048;
			d32 <<= 22;
			bits_left -= 22;
		}
		else if ((d32 & 0xfff00000) == 0xffe00000) 
		{
			/* 12 lower bits of LoM */
			mprintf("decompress_rdp_5: LoM : 12 bits\n");
			lom = ((d32 >> 8) & 0xfff) + 4096;
			d32 <<= 24;
			bits_left -= 24;
		}
		else if ((d32 & 0xfff80000) == 0xfff00000) 
		{
			/* 13 lower bits of LoM */
			mprintf("decompress_rdp_5: LoM : 13 bits\n");
			lom = ((d32 >> 6) & 0x1fff) + 8192;
			d32 <<= 26;
			bits_left -= 26;
		}
		else if ((d32 & 0xfffc0000) == 0xfff80000) 
		{
			/* 14 lower bits of LoM */
			mprintf("decompress_rdp_5: LoM : 14 bits\n");
			lom = ((d32 >> 4) & 0x3fff) + 16384;
			d32 <<= 28;
			bits_left -= 28;
		}
		else if ((d32 & 0xfffe0000) == 0xfffc0000) 
		{
			/* 15 lower bits of LoM */
			mprintf("decompress_rdp_5: LoM : 15 bits\n");
			lom = ((d32 >> 2) & 0x7fff) + 32768;
			d32 <<= 30;
			bits_left -= 30;
		}

		/* now that we have copy_offset and LoM, process them */

		mprintf("decompress_rdp_5: Length of Match = %d\n", lom);

		src_ptr = history_ptr - copy_offset;
		while (lom > 0)
		{
			*history_ptr++ = *src_ptr++;
			lom--;
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
	mprintf("decompress_rdp_5: roff=%d rlen=%d\n", *roff, *rlen);
	return True;
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

int decompress_rdp_6(rdpRdp   *rdp, 
		     uint8    *cbuf,
		     int       len,
		     int       ctype,
		     uint32   *roff,
		     uint32   *rlen) 
{
	return False;
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

int decompress_rdp_61(rdpRdp   *rdp, 
		     uint8    *cbuf,
		     int       len,
		     int       ctype,
		     uint32   *roff,
		     uint32   *rlen) 
{
	return False;
}

/* LK_TODO */
#ifdef USE_RDP_4
int decompress_rdp_4(rdpRdp   *rdp, 
		     uint8    *cbuf,
		     int       len,
		     int       ctype,
		     uint32   *roff,
		     uint32   *rlen) 
{
	uint32    d32;            /* we process 4 compressed bytes at a time */
	uint16    copy_offset;    /* location to copy data from */
	uint16    lom;            /* length of match */
	uint8    *src_ptr;        /* used while copying compressed data */
	uint8    *cptr;           /* points to next byte in cbuf */
	uint8    *history_buf;    /* uncompressed data goes here */
	uint8    *history_ptr;    /* points to next frees slot in bistory_buf */
	uint8     cur_byte;       /* last byte fetched from cbuf */
	int       bits_left;      /* bits left in d32 for processing */
	int       cur_bits_left;  /* bits left in cur_byte for processing */
	int       tmp;
	uint32    i32;

	if ((ctype & 0x0f) != PACKET_COMPR_TYPE_8K)
	{
		/* this is not RDP 4 compressed data */	
		return False;
	}

	src_ptr = 0;
	cptr = cbuf;
	copy_offset = 0;
	lom = 0;
	bits_left = 0;
	cur_bits_left = 0;
	d32 = 0;

	if (ctype & PACKET_AT_FRONT) 
	{
		history_ptr = 0;
	}

	if (ctype & PACKET_FLUSHED) 
	{
		history_ptr = history_buf;
		memset(history_buf, 0, history_buf_size);
	}

	if ((ctype & PACKET_COMPRESSED) != PACKET_COMPRESSED) 
	{
		memcpy(history_ptr, cbuf, len);
		history_ptr += len;
		return True;
	}

	// LK_TODO load initial data

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
		while (lom > 0)
		{
			*history_ptr = *src_ptr;
			src_ptr++;
			history_ptr++;
			lom--;
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

	return True;
}
#endif

/**
 * allocate space to store history buffer
 *
 * @param rdp rdp struct that contains rdp_mppc struct
 * @return pointer to new struct, or NULL on failure
 */

struct rdp_mppc *mppc_new(rdpRdp *rdp)
{
	struct rdp_mppc *ptr;

	ptr = (struct rdp_mppc *) malloc(sizeof (struct rdp_mppc));
	if (!ptr)
	{
		printf("mppc_new(): system out of memory\n");
		return NULL;
	}

	ptr->history_buf = (uint8 *) malloc(RDP5_HISTORY_BUF_SIZE);
	if (!ptr->history_buf)
	{
		printf("mppc_new(): system out of memory\n");
		free(ptr);
		return NULL;
	}

	ptr->history_ptr = ptr->history_buf;
	return ptr;
}

/**
 * free history buffer
 *
 * @param rdp rdp struct that contains rdp_mppc struct
 */

void mppc_free(rdpRdp *rdp)
{
	if (!rdp->mppc)
	{
		return;
	}

	if (rdp->mppc->history_buf)
	{
		free(rdp->mppc->history_buf);
		rdp->mppc->history_buf = NULL;
		rdp->mppc->history_ptr = NULL;
	}
	free(rdp->mppc);
}

