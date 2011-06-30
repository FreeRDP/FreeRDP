/*
 * Copyright (c) 2005, 2006, 2007 Lev Walkin <vlm@lionet.info>.
 * All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include "asn_system.h"
#include "asn_internal.h"
#include "per_support.h"

char *
per_data_string(asn_per_data_t *pd) {
	static char buf[2][32];
	static int n;
	n = (n+1) % 2;
	snprintf(buf[n], sizeof(buf),
		"{m=%d span %+d[%d..%d] (%d)}",
		pd->moved,
		(((int)pd->buffer) & 0xf),
		pd->nboff, pd->nbits,
		pd->nbits - pd->nboff);
	return buf[n];
}

void
per_get_undo(asn_per_data_t *pd, int nbits) {
	if((ssize_t)pd->nboff < nbits) {
		assert((ssize_t)pd->nboff < nbits);
	} else {
		pd->nboff -= nbits;
		pd->moved -= nbits;
	}
}

/*
 * Extract a small number of bits (<= 31) from the specified PER data pointer.
 */
int32_t
per_get_few_bits(asn_per_data_t *pd, int nbits) {
	size_t off;	/* Next after last bit offset */
	ssize_t nleft;	/* Number of bits left in this stream */
	uint32_t accum;
	const uint8_t *buf;

	if(nbits < 0)
		return -1;

	nleft = pd->nbits - pd->nboff;
	if(nbits > nleft) {
		int32_t tailv, vhead;
		if(!pd->refill || nbits > 31) return -1;
		/* Accumulate unused bytes before refill */
		ASN_DEBUG("Obtain the rest %d bits (want %d)", nleft, nbits);
		tailv = per_get_few_bits(pd, nleft);
		if(tailv < 0) return -1;
		/* Refill (replace pd contents with new data) */
		if(pd->refill(pd))
			return -1;
		nbits -= nleft;
		vhead = per_get_few_bits(pd, nbits);
		/* Combine the rest of previous pd with the head of new one */
		tailv = (tailv << nbits) | vhead;  /* Could == -1 */
		return tailv;
	}

	/*
	 * Normalize position indicator.
	 */
	if(pd->nboff >= 8) {
		pd->buffer += (pd->nboff >> 3);
		pd->nbits  -= (pd->nboff & ~0x07);
		pd->nboff  &= 0x07;
	}
	pd->moved += nbits;
	pd->nboff += nbits;
	off = pd->nboff;
	buf = pd->buffer;

	/*
	 * Extract specified number of bits.
	 */
	if(off <= 8)
		accum = nbits ? (buf[0]) >> (8 - off) : 0;
	else if(off <= 16)
		accum = ((buf[0] << 8) + buf[1]) >> (16 - off);
	else if(off <= 24)
		accum = ((buf[0] << 16) + (buf[1] << 8) + buf[2]) >> (24 - off);
	else if(off <= 31)
		accum = ((buf[0] << 24) + (buf[1] << 16)
			+ (buf[2] << 8) + (buf[3])) >> (32 - off);
	else if(nbits <= 31) {
		asn_per_data_t tpd = *pd;
		/* Here are we with our 31-bits limit plus 1..7 bits offset. */
		per_get_undo(&tpd, nbits);
		/* The number of available bits in the stream allow
		 * for the following operations to take place without
		 * invoking the ->refill() function */
		accum  = per_get_few_bits(&tpd, nbits - 24) << 24;
		accum |= per_get_few_bits(&tpd, 24);
	} else {
		per_get_undo(pd, nbits);
		return -1;
	}

	accum &= (((uint32_t)1 << nbits) - 1);

	ASN_DEBUG("  [PER got %2d<=%2d bits => span %d %+d[%d..%d]:%02x (%d) => 0x%x]",
		nbits, nleft,
		pd->moved,
		(((int)pd->buffer) & 0xf),
		pd->nboff, pd->nbits,
		pd->buffer[0],
		pd->nbits - pd->nboff,
		(int)accum);

	return accum;
}

/*
 * Extract a large number of bits from the specified PER data pointer.
 */
int
per_get_many_bits(asn_per_data_t *pd, uint8_t *dst, int alright, int nbits) {
	int32_t value;

	if(alright && (nbits & 7)) {
		/* Perform right alignment of a first few bits */
		value = per_get_few_bits(pd, nbits & 0x07);
		if(value < 0) return -1;
		*dst++ = value;	/* value is already right-aligned */
		nbits &= ~7;
	}

	while(nbits) {
		if(nbits >= 24) {
			value = per_get_few_bits(pd, 24);
			if(value < 0) return -1;
			*(dst++) = value >> 16;
			*(dst++) = value >> 8;
			*(dst++) = value;
			nbits -= 24;
		} else {
			value = per_get_few_bits(pd, nbits);
			if(value < 0) return -1;
			if(nbits & 7) {	/* implies left alignment */
				value <<= 8 - (nbits & 7),
				nbits += 8 - (nbits & 7);
				if(nbits > 24)
					*dst++ = value >> 24;
			}
			if(nbits > 16)
				*dst++ = value >> 16;
			if(nbits > 8)
				*dst++ = value >> 8;
			*dst++ = value;
			break;
		}
	}

	return 0;
}

/*
 * Get the length "n" from the stream.
 */
ssize_t
uper_get_length(asn_per_data_t *pd, int ebits, int *repeat) {
	ssize_t value;

	*repeat = 0;

	if(ebits >= 0) return per_get_few_bits(pd, ebits);

	value = per_get_few_bits(pd, 8);
	if(value < 0) return -1;
	if((value & 128) == 0)	/* #10.9.3.6 */
		return (value & 0x7F);
	if((value & 64) == 0) {	/* #10.9.3.7 */
		value = ((value & 63) << 8) | per_get_few_bits(pd, 8);
		if(value < 0) return -1;
		return value;
	}
	value &= 63;	/* this is "m" from X.691, #10.9.3.8 */
	if(value < 1 || value > 4)
		return -1;
	*repeat = 1;
	return (16384 * value);
}

/*
 * Get the normally small length "n".
 * This procedure used to decode length of extensions bit-maps
 * for SET and SEQUENCE types.
 */
ssize_t
uper_get_nslength(asn_per_data_t *pd) {
	ssize_t length;

	ASN_DEBUG("Getting normally small length");

	if(per_get_few_bits(pd, 1) == 0) {
		length = per_get_few_bits(pd, 6) + 1;
		if(length <= 0) return -1;
		ASN_DEBUG("l=%d", length);
		return length;
	} else {
		int repeat;
		length = uper_get_length(pd, -1, &repeat);
		if(length >= 0 && !repeat) return length;
		return -1; /* Error, or do not support >16K extensions */
	}
}

/*
 * Get the normally small non-negative whole number.
 * X.691, #10.6
 */
ssize_t
uper_get_nsnnwn(asn_per_data_t *pd) {
	ssize_t value;

	value = per_get_few_bits(pd, 7);
	if(value & 64) {	/* implicit (value < 0) */
		value &= 63;
		value <<= 2;
		value |= per_get_few_bits(pd, 2);
		if(value & 128)	/* implicit (value < 0) */
			return -1;
		if(value == 0)
			return 0;
		if(value >= 3)
			return -1;
		value = per_get_few_bits(pd, 8 * value);
		return value;
	}

	return value;
}

/*
 * Put the normally small non-negative whole number.
 * X.691, #10.6
 */
int
uper_put_nsnnwn(asn_per_outp_t *po, int n) {
	int bytes;

	if(n <= 63) {
		if(n < 0) return -1;
		return per_put_few_bits(po, n, 7);
	}
	if(n < 256)
		bytes = 1;
	else if(n < 65536)
		bytes = 2;
	else if(n < 256 * 65536)
		bytes = 3;
	else
		return -1;	/* This is not a "normally small" value */
	if(per_put_few_bits(po, bytes, 8))
		return -1;

	return per_put_few_bits(po, n, 8 * bytes);
}


/*
 * Put a small number of bits (<= 31).
 */
int
per_put_few_bits(asn_per_outp_t *po, uint32_t bits, int obits) {
	size_t off;	/* Next after last bit offset */
	size_t omsk;	/* Existing last byte meaningful bits mask */
	uint8_t *buf;

	if(obits <= 0 || obits >= 32) return obits ? -1 : 0;

	ASN_DEBUG("[PER put %d bits %x to %p+%d bits]",
			obits, (int)bits, po->buffer, po->nboff);

	/*
	 * Normalize position indicator.
	 */
	if(po->nboff >= 8) {
		po->buffer += (po->nboff >> 3);
		po->nbits  -= (po->nboff & ~0x07);
		po->nboff  &= 0x07;
	}

	/*
	 * Flush whole-bytes output, if necessary.
	 */
	if(po->nboff + obits > po->nbits) {
		int complete_bytes = (po->buffer - po->tmpspace);
		ASN_DEBUG("[PER output %d complete + %d]",
			complete_bytes, po->flushed_bytes);
		if(po->outper(po->tmpspace, complete_bytes, po->op_key) < 0)
			return -1;
		if(po->nboff)
			po->tmpspace[0] = po->buffer[0];
		po->buffer = po->tmpspace;
		po->nbits = 8 * sizeof(po->tmpspace);
		po->flushed_bytes += complete_bytes;
	}

	/*
	 * Now, due to sizeof(tmpspace), we are guaranteed large enough space.
	 */
	buf = po->buffer;
	omsk = ~((1 << (8 - po->nboff)) - 1);
	off = (po->nboff += obits);

	/* Clear data of debris before meaningful bits */
	bits &= (((uint32_t)1 << obits) - 1);

	ASN_DEBUG("[PER out %d %u/%x (t=%d,o=%d) %x&%x=%x]", obits,
		(int)bits, (int)bits,
		po->nboff - obits, off, buf[0], omsk&0xff, buf[0] & omsk);

	if(off <= 8)	/* Completely within 1 byte */
		bits <<= (8 - off),
		buf[0] = (buf[0] & omsk) | bits;
	else if(off <= 16)
		bits <<= (16 - off),
		buf[0] = (buf[0] & omsk) | (bits >> 8),
		buf[1] = bits;
	else if(off <= 24)
		bits <<= (24 - off),
		buf[0] = (buf[0] & omsk) | (bits >> 16),
		buf[1] = bits >> 8,
		buf[2] = bits;
	else if(off <= 31)
		bits <<= (32 - off),
		buf[0] = (buf[0] & omsk) | (bits >> 24),
		buf[1] = bits >> 16,
		buf[2] = bits >> 8,
		buf[3] = bits;
	else {
		ASN_DEBUG("->[PER out split %d]", obits);
		per_put_few_bits(po, bits >> 8, 24);
		per_put_few_bits(po, bits, obits - 24);
		ASN_DEBUG("<-[PER out split %d]", obits);
	}

	ASN_DEBUG("[PER out %u/%x => %02x buf+%d]",
		(int)bits, (int)bits, buf[0], po->buffer - po->tmpspace);

	return 0;
}


/*
 * Output a large number of bits.
 */
int
per_put_many_bits(asn_per_outp_t *po, const uint8_t *src, int nbits) {

	while(nbits) {
		uint32_t value;

		if(nbits >= 24) {
			value = (src[0] << 16) | (src[1] << 8) | src[2];
			src += 3;
			nbits -= 24;
			if(per_put_few_bits(po, value, 24))
				return -1;
		} else {
			value = src[0];
			if(nbits > 8)
				value = (value << 8) | src[1];
			if(nbits > 16)
				value = (value << 8) | src[2];
			if(nbits & 0x07)
				value >>= (8 - (nbits & 0x07));
			if(per_put_few_bits(po, value, nbits))
				return -1;
			break;
		}
	}

	return 0;
}

/*
 * Put the length "n" (or part of it) into the stream.
 */
ssize_t
uper_put_length(asn_per_outp_t *po, size_t length) {

	if(length <= 127)	/* #10.9.3.6 */
		return per_put_few_bits(po, length, 8)
			? -1 : (ssize_t)length;
	else if(length < 16384)	/* #10.9.3.7 */
		return per_put_few_bits(po, length|0x8000, 16)
			? -1 : (ssize_t)length;

	length >>= 14;
	if(length > 4) length = 4;

	return per_put_few_bits(po, 0xC0 | length, 8)
			? -1 : (ssize_t)(length << 14);
}


/*
 * Put the normally small length "n" into the stream.
 * This procedure used to encode length of extensions bit-maps
 * for SET and SEQUENCE types.
 */
int
uper_put_nslength(asn_per_outp_t *po, size_t length) {

	if(length <= 64) {
		/* #10.9.3.4 */
		if(length == 0) return -1;
		return per_put_few_bits(po, length-1, 7) ? -1 : 0;
	} else {
		if(uper_put_length(po, length) != (ssize_t)length) {
			/* This might happen in case of >16K extensions */
			return -1;
		}
	}

	return 0;
}

