/*-
 * Copyright (c) 2003, 2004 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include "asn_internal.h"
#include "OBJECT_IDENTIFIER.h"
#include "OCTET_STRING.h"
#include <limits.h>	/* for CHAR_BIT */
#include <errno.h>

/*
 * OBJECT IDENTIFIER basic type description.
 */
static ber_tlv_tag_t asn_DEF_OBJECT_IDENTIFIER_tags[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (6 << 2))
};
asn_TYPE_descriptor_t asn_DEF_OBJECT_IDENTIFIER = {
	"OBJECT IDENTIFIER",
	"OBJECT_IDENTIFIER",
	ASN__PRIMITIVE_TYPE_free,
	OBJECT_IDENTIFIER_print,
	OBJECT_IDENTIFIER_constraint,
	ber_decode_primitive,
	der_encode_primitive,
	OBJECT_IDENTIFIER_decode_xer,
	OBJECT_IDENTIFIER_encode_xer,
	OCTET_STRING_decode_uper,
	OCTET_STRING_encode_uper,
	0, /* Use generic outmost tag fetcher */
	asn_DEF_OBJECT_IDENTIFIER_tags,
	sizeof(asn_DEF_OBJECT_IDENTIFIER_tags)
	    / sizeof(asn_DEF_OBJECT_IDENTIFIER_tags[0]),
	asn_DEF_OBJECT_IDENTIFIER_tags,	/* Same as above */
	sizeof(asn_DEF_OBJECT_IDENTIFIER_tags)
	    / sizeof(asn_DEF_OBJECT_IDENTIFIER_tags[0]),
	0,	/* No PER visible constraints */
	0, 0,	/* No members */
	0	/* No specifics */
};


int
OBJECT_IDENTIFIER_constraint(asn_TYPE_descriptor_t *td, const void *sptr,
		asn_app_constraint_failed_f *ctfailcb, void *app_key) {
	const OBJECT_IDENTIFIER_t *st = (const OBJECT_IDENTIFIER_t *)sptr;

	if(st && st->buf) {
		if(st->size < 1) {
			_ASN_CTFAIL(app_key, td, sptr,
				"%s: at least one numerical value "
				"expected (%s:%d)",
				td->name, __FILE__, __LINE__);
			return -1;
		}
	} else {
		_ASN_CTFAIL(app_key, td, sptr,
			"%s: value not given (%s:%d)",
			td->name, __FILE__, __LINE__);
		return -1;
	}

	return 0;
}


int
OBJECT_IDENTIFIER_get_single_arc(uint8_t *arcbuf, unsigned int arclen, signed int add, void *rvbufp, unsigned int rvsize) {
	unsigned LE GCC_NOTUSED = 1; /* Little endian (x86) */
	uint8_t *arcend = arcbuf + arclen;	/* End of arc */
	unsigned int cache = 0;	/* No more than 14 significant bits */
	unsigned char *rvbuf = (unsigned char *)rvbufp;
	unsigned char *rvstart = rvbuf;	/* Original start of the value buffer */
	int inc;	/* Return value growth direction */

	rvsize *= CHAR_BIT;	/* bytes to bits */
	arclen *= 7;		/* bytes to bits */

	/*
	 * The arc has the number of bits
	 * cannot be represented using supplied return value type.
	 */
	if(arclen > rvsize) {
		if(arclen > (rvsize + CHAR_BIT)) {
			errno = ERANGE;	/* Overflow */
			return -1;
		} else {
			/*
			 * Even if the number of bits in the arc representation
			 * is higher than the width of supplied * return value
			 * type, there is still possible to fit it when there
			 * are few unused high bits in the arc value
			 * representaion.
			 * 
			 * Moreover, there is a possibility that the
			 * number could actually fit the arc space, given
			 * that add is negative, but we don't handle
			 * such "temporary lack of precision" situation here.
			 * May be considered as a bug.
			 */
			uint8_t mask = (0xff << (7-(arclen - rvsize))) & 0x7f;
			if((*arcbuf & mask)) {
				errno = ERANGE;	/* Overflow */
				return -1;
			}
			/* Fool the routine computing unused bits */
			arclen -= 7;
			cache = *arcbuf & 0x7f;
			arcbuf++;
		}
	}

	/* Faster path for common size */
	if(rvsize == (CHAR_BIT * sizeof(unsigned long))) {
		unsigned long accum;
		/* Gather all bits into the accumulator */
		for(accum = cache; arcbuf < arcend; arcbuf++)
			accum = (accum << 7) | (*arcbuf & ~0x80);
		if(accum < (unsigned)-add) {
			errno = ERANGE;	/* Overflow */
			return -1;
		}
		*(unsigned long *)rvbuf = accum + add;	/* alignment OK! */
		return 0;
	}

#ifndef	WORDS_BIGENDIAN
	if(*(unsigned char *)&LE) {	/* Little endian (x86) */
		/* "Convert" to big endian */
		rvbuf += rvsize / CHAR_BIT - 1;
		rvstart--;
		inc = -1;	/* Descending */
	} else
#endif	/* !WORDS_BIGENDIAN */
		inc = +1;	/* Big endian is known [at compile time] */

	{
		int bits;	/* typically no more than 3-4 bits */

		/* Clear the high unused bits */
		for(bits = rvsize - arclen;
			bits > CHAR_BIT;
				rvbuf += inc, bits -= CHAR_BIT)
				*rvbuf = 0;

		/* Fill the body of a value */
		for(; arcbuf < arcend; arcbuf++) {
			cache = (cache << 7) | (*arcbuf & 0x7f);
			bits += 7;
			if(bits >= CHAR_BIT) {
				bits -= CHAR_BIT;
				*rvbuf = (cache >> bits);
				rvbuf += inc;
			}
		}
		if(bits) {
			*rvbuf = cache;
			rvbuf += inc;
		}
	}

	if(add) {
		for(rvbuf -= inc; rvbuf != rvstart; rvbuf -= inc) {
			int v = add + *rvbuf;
			if(v & (-1 << CHAR_BIT)) {
				*rvbuf = (unsigned char)(v + (1 << CHAR_BIT));
				add = -1;
			} else {
				*rvbuf = v;
				break;
			}
		}
		if(rvbuf == rvstart) {
			/* No space to carry over */
			errno = ERANGE;	/* Overflow */
			return -1;
		}
	}

	return 0;
}

ssize_t
OBJECT_IDENTIFIER__dump_arc(uint8_t *arcbuf, int arclen, int add,
		asn_app_consume_bytes_f *cb, void *app_key) {
	char scratch[64];	/* Conservative estimate */
	unsigned long accum;	/* Bits accumulator */
	char *p;		/* Position in the scratch buffer */

	if(OBJECT_IDENTIFIER_get_single_arc(arcbuf, arclen, add,
			&accum, sizeof(accum)))
		return -1;

	if(accum) {
		ssize_t len;

		/* Fill the scratch buffer in reverse. */
		p = scratch + sizeof(scratch);
		for(; accum; accum /= 10)
			*(--p) = (char)(accum % 10) + 0x30; /* Put a digit */

		len = sizeof(scratch) - (p - scratch);
		if(cb(p, len, app_key) < 0)
			return -1;
		return len;
	} else {
		*scratch = 0x30;
		if(cb(scratch, 1, app_key) < 0)
			return -1;
		return 1;
	}
}

int
OBJECT_IDENTIFIER_print_arc(uint8_t *arcbuf, int arclen, int add,
		asn_app_consume_bytes_f *cb, void *app_key) {

	if(OBJECT_IDENTIFIER__dump_arc(arcbuf, arclen, add, cb, app_key) < 0)
		return -1;

	return 0;
}

static ssize_t
OBJECT_IDENTIFIER__dump_body(const OBJECT_IDENTIFIER_t *st, asn_app_consume_bytes_f *cb, void *app_key) {
	ssize_t wrote_len = 0;
	int startn;
	int add = 0;
	int i;

	for(i = 0, startn = 0; i < st->size; i++) {
		uint8_t b = st->buf[i];
		if((b & 0x80))			/* Continuation expected */
			continue;

		if(startn == 0) {
			/*
			 * First two arcs are encoded through the backdoor.
			 */
			if(i) {
				add = -80;
				if(cb("2", 1, app_key) < 0) return -1;
			} else if(b <= 39) {
				add = 0;
				if(cb("0", 1, app_key) < 0) return -1;
			} else if(b < 79) {
				add = -40;
				if(cb("1", 1, app_key) < 0) return -1;
			} else {
				add = -80;
				if(cb("2", 1, app_key) < 0) return -1;
			}
			wrote_len += 1;
		}

		if(cb(".", 1, app_key) < 0)	/* Separate arcs */
			return -1;

		add = OBJECT_IDENTIFIER__dump_arc(&st->buf[startn],
				i - startn + 1, add, cb, app_key);
		if(add < 0) return -1;
		wrote_len += 1 + add;
		startn = i + 1;
		add = 0;
	}

	return wrote_len;
}

static enum xer_pbd_rval
OBJECT_IDENTIFIER__xer_body_decode(asn_TYPE_descriptor_t *td, void *sptr, const void *chunk_buf, size_t chunk_size) {
	OBJECT_IDENTIFIER_t *st = (OBJECT_IDENTIFIER_t *)sptr;
	const char *chunk_end = (const char *)chunk_buf + chunk_size;
	const char *endptr;
	long s_arcs[10];
	long *arcs = s_arcs;
	int arcs_count;
	int ret;

	(void)td;

	arcs_count = OBJECT_IDENTIFIER_parse_arcs(
		(const char *)chunk_buf, chunk_size, arcs,
			sizeof(s_arcs)/sizeof(s_arcs[0]), &endptr);
	if(arcs_count <= 0) {
		/* Expecting more than zero arcs */
		return XPBD_BROKEN_ENCODING;
	}
	if(endptr < chunk_end) {
		/* We have a tail of unrecognized data. Check its safety. */
		if(!xer_is_whitespace(endptr, chunk_end - endptr))
			return XPBD_BROKEN_ENCODING;
	}

	if((size_t)arcs_count > sizeof(s_arcs)/sizeof(s_arcs[0])) {
		arcs = (long *)MALLOC(arcs_count * sizeof(long));
		if(!arcs) return XPBD_SYSTEM_FAILURE;
		ret = OBJECT_IDENTIFIER_parse_arcs(
			(const char *)chunk_buf, chunk_size,
			arcs, arcs_count, &endptr);
		if(ret != arcs_count)
			return XPBD_SYSTEM_FAILURE;	/* assert?.. */
	}

	/*
	 * Convert arcs into BER representation.
	 */
	ret = OBJECT_IDENTIFIER_set_arcs(st, arcs, sizeof(*arcs), arcs_count);
	if(arcs != s_arcs) FREEMEM(arcs);

	return ret ? XPBD_SYSTEM_FAILURE : XPBD_BODY_CONSUMED;
}

asn_dec_rval_t
OBJECT_IDENTIFIER_decode_xer(asn_codec_ctx_t *opt_codec_ctx,
	asn_TYPE_descriptor_t *td, void **sptr, const char *opt_mname,
		const void *buf_ptr, size_t size) {

	return xer_decode_primitive(opt_codec_ctx, td,
		sptr, sizeof(OBJECT_IDENTIFIER_t), opt_mname,
			buf_ptr, size, OBJECT_IDENTIFIER__xer_body_decode);
}

asn_enc_rval_t
OBJECT_IDENTIFIER_encode_xer(asn_TYPE_descriptor_t *td, void *sptr,
	int ilevel, enum xer_encoder_flags_e flags,
		asn_app_consume_bytes_f *cb, void *app_key) {
	const OBJECT_IDENTIFIER_t *st = (const OBJECT_IDENTIFIER_t *)sptr;
	asn_enc_rval_t er;

	(void)ilevel;
	(void)flags;

	if(!st || !st->buf)
		_ASN_ENCODE_FAILED;

	er.encoded = OBJECT_IDENTIFIER__dump_body(st, cb, app_key);
	if(er.encoded < 0) _ASN_ENCODE_FAILED;

	_ASN_ENCODED_OK(er);
}

int
OBJECT_IDENTIFIER_print(asn_TYPE_descriptor_t *td, const void *sptr,
	int ilevel, asn_app_consume_bytes_f *cb, void *app_key) {
	const OBJECT_IDENTIFIER_t *st = (const OBJECT_IDENTIFIER_t *)sptr;

	(void)td;	/* Unused argument */
	(void)ilevel;	/* Unused argument */

	if(!st || !st->buf)
		return (cb("<absent>", 8, app_key) < 0) ? -1 : 0;

	/* Dump preamble */
	if(cb("{ ", 2, app_key) < 0)
		return -1;

	if(OBJECT_IDENTIFIER__dump_body(st, cb, app_key) < 0)
		return -1;

	return (cb(" }", 2, app_key) < 0) ? -1 : 0;
}

int
OBJECT_IDENTIFIER_get_arcs(OBJECT_IDENTIFIER_t *oid, void *arcs,
		unsigned int arc_type_size, unsigned int arc_slots) {
	void *arcs_end = (char *)arcs + (arc_type_size * arc_slots);
	int num_arcs = 0;
	int startn = 0;
	int add = 0;
	int i;

	if(!oid || !oid->buf || (arc_slots && arc_type_size <= 1)) {
		errno = EINVAL;
		return -1;
	}

	for(i = 0; i < oid->size; i++) {
		uint8_t b = oid->buf[i];
		if((b & 0x80))			/* Continuation expected */
			continue;

		if(num_arcs == 0) {
			/*
			 * First two arcs are encoded through the backdoor.
			 */
			unsigned LE = 1;	/* Little endian */
			int first_arc;
			num_arcs++;
			if(!arc_slots) { num_arcs++; continue; }

			if(i) first_arc = 2;
			else if(b <= 39) first_arc = 0;
			else if(b < 79)	first_arc = 1;
			else first_arc = 2;

			add = -40 * first_arc;
			memset(arcs, 0, arc_type_size);
			*(unsigned char *)((char *)arcs
				+ ((*(char *)&LE)?0:(arc_type_size - 1)))
					= first_arc;
			arcs = ((char *)arcs) + arc_type_size;
		}

		/* Decode, if has space */
		if(arcs < arcs_end) {
			if(OBJECT_IDENTIFIER_get_single_arc(&oid->buf[startn],
				i - startn + 1, add,
					arcs, arc_type_size))
				return -1;
			startn = i + 1;
			arcs = ((char *)arcs) + arc_type_size;
			add = 0;
		}
		num_arcs++;
	}

	return num_arcs;
}


/*
 * Save the single value as an object identifier arc.
 */
int
OBJECT_IDENTIFIER_set_single_arc(uint8_t *arcbuf, const void *arcval, unsigned int arcval_size, int prepared_order) {
	/*
	 * The following conditions must hold:
	 * assert(arcval);
	 * assert(arcval_size > 0);
	 * assert(arcval_size <= 16);
	 * assert(arcbuf);
	 */
#ifdef	WORDS_BIGENDIAN
	const unsigned isLittleEndian = 0;
#else
	unsigned LE = 1;
	unsigned isLittleEndian = *(char *)&LE;
#endif
	const uint8_t *tend, *tp;
	unsigned int cache;
	uint8_t *bp = arcbuf;
	int bits;
	uint8_t buffer[16];

	if(isLittleEndian && !prepared_order) {
		const uint8_t *a = (const unsigned char *)arcval + arcval_size - 1;
		const uint8_t *aend = (const uint8_t *)arcval;
		uint8_t *msb = buffer + arcval_size - 1;
		uint8_t *tb;
		for(tb = buffer; a >= aend; tb++, a--)
			if((*tb = *a) && (tb < msb))
				msb = tb;
		tend = &buffer[arcval_size];
		tp = msb;	/* Most significant non-zero byte */
	} else {
		/* Look for most significant non-zero byte */
		tend = (const unsigned char *)arcval + arcval_size;
		for(tp = (const uint8_t *)arcval; tp < tend - 1; tp++)
			if(*tp) break;
	}

	/*
	 * Split the value in 7-bits chunks.
	 */
	bits = ((tend - tp) * CHAR_BIT) % 7;
	if(bits) {
		cache = *tp >> (CHAR_BIT - bits);
		if(cache) {
			*bp++ = cache | 0x80;
			cache = *tp++;
			bits = CHAR_BIT - bits;
		} else {
			bits = -bits;
		}
	} else {
		cache = 0;
	}
	for(; tp < tend; tp++) {
		cache = (cache << CHAR_BIT) + *tp;
		bits += CHAR_BIT;
		while(bits >= 7) {
			bits -= 7;
			*bp++ = 0x80 | (cache >> bits);
		}
	}
	if(bits) *bp++ = cache;
	bp[-1] &= 0x7f;	/* Clear the last bit */

	return bp - arcbuf;
}

int
OBJECT_IDENTIFIER_set_arcs(OBJECT_IDENTIFIER_t *oid, const void *arcs, unsigned int arc_type_size, unsigned int arc_slots) {
	uint8_t *buf;
	uint8_t *bp;
	unsigned LE = 1;	/* Little endian (x86) */
	unsigned isLittleEndian = *((char *)&LE);
	unsigned int arc0;
	unsigned int arc1;
	unsigned size;
	unsigned i;

	if(!oid || !arcs || arc_type_size < 1
	|| arc_type_size > 16
	|| arc_slots < 2) {
		errno = EINVAL;
		return -1;
	}

	switch(arc_type_size) {
	case sizeof(char):
		arc0 = ((const unsigned char *)arcs)[0];
		arc1 = ((const unsigned char *)arcs)[1];
		break;
	case sizeof(short):
		arc0 = ((const unsigned short *)arcs)[0];
		arc1 = ((const unsigned short *)arcs)[1];
		break;
	case sizeof(int):
		arc0 = ((const unsigned int *)arcs)[0];
		arc1 = ((const unsigned int *)arcs)[1];
		break;
	default:
		arc1 = arc0 = 0;
		if(isLittleEndian) {	/* Little endian (x86) */
			const unsigned char *ps, *pe;
			/* If more significant bytes are present,
			 * make them > 255 quick */
			for(ps = (const unsigned char *)arcs + 1, pe = ps+arc_type_size;
					ps < pe; ps++)
				arc0 |= *ps, arc1 |= *(ps + arc_type_size);
			arc0 <<= CHAR_BIT, arc1 <<= CHAR_BIT;
			arc0 = *((const unsigned char *)arcs + 0);
			arc1 = *((const unsigned char *)arcs + arc_type_size);
		} else {
			const unsigned char *ps, *pe;
			/* If more significant bytes are present,
			 * make them > 255 quick */
			for(ps = (const unsigned char *)arcs, pe = ps+arc_type_size - 1; ps < pe; ps++)
				arc0 |= *ps, arc1 |= *(ps + arc_type_size);
			arc0 = *((const unsigned char *)arcs + arc_type_size - 1);
			arc1 = *((const unsigned char *)arcs +(arc_type_size<< 1)-1);
		}
	}

	/*
	 * The previous chapter left us with the first and the second arcs.
	 * The values are not precise (that is, they are valid only if
	 * they're less than 255), but OK for the purposes of making
	 * the sanity test below.
	 */
	if(arc0 <= 1) {
		if(arc1 >= 39) {
			/* 8.19.4: At most 39 subsequent values (including 0) */
			errno = ERANGE;
			return -1;
		}
	} else if(arc0 > 2) {
		/* 8.19.4: Only three values are allocated from the root node */
		errno = ERANGE;
		return -1;
	}
	/*
	 * After above tests it is known that the value of arc0 is completely
	 * trustworthy (0..2). However, the arc1's value is still meaningless.
	 */

	/*
	 * Roughly estimate the maximum size necessary to encode these arcs.
	 * This estimation implicitly takes in account the following facts,
	 * that cancel each other:
	 * 	* the first two arcs are encoded in a single value.
	 * 	* the first value may require more space (+1 byte)
	 * 	* the value of the first arc which is in range (0..2)
	 */
	size = ((arc_type_size * CHAR_BIT + 6) / 7) * arc_slots;
	bp = buf = (uint8_t *)MALLOC(size + 1);
	if(!buf) {
		/* ENOMEM */
		return -1;
	}

	/*
	 * Encode the first two arcs.
	 * These require special treatment.
	 */
	{
		uint8_t *tp;
		uint8_t first_value[1 + 16];	/* of two arcs */
		uint8_t *fv = first_value;

		/*
		 * Simulate first_value = arc0 * 40 + arc1;
		 */
		/* Copy the second (1'st) arcs[1] into the first_value */
		*fv++ = 0;
		arcs = ((const char *)arcs) + arc_type_size;
		if(isLittleEndian) {
			const uint8_t *aend = (const unsigned char *)arcs - 1;
			const uint8_t *a1 = (const unsigned char *)arcs + arc_type_size - 1;
			for(; a1 > aend; fv++, a1--) *fv = *a1;
		} else {
			const uint8_t *a1 = (const uint8_t *)arcs;
			const uint8_t *aend = a1 + arc_type_size;
			for(; a1 < aend; fv++, a1++) *fv = *a1;
		}
		/* Increase the first_value by arc0 */
		arc0 *= 40;	/* (0..80) */
		for(tp = first_value + arc_type_size; tp >= first_value; tp--) {
			unsigned int v = *tp;
			v += arc0;
			*tp = v;
			if(v >= (1 << CHAR_BIT)) arc0 = v >> CHAR_BIT;
			else break;
		}

		assert(tp >= first_value);

		bp += OBJECT_IDENTIFIER_set_single_arc(bp, first_value,
			fv - first_value, 1);
 	}

	/*
	 * Save the rest of arcs.
	 */
	for(arcs = ((const char *)arcs) + arc_type_size, i = 2;
		i < arc_slots;
			i++, arcs = ((const char *)arcs) + arc_type_size) {
		bp += OBJECT_IDENTIFIER_set_single_arc(bp,
			arcs, arc_type_size, 0);
	}

	assert((unsigned)(bp - buf) <= size);

	/*
	 * Replace buffer.
	 */
	oid->size = bp - buf;
	bp = oid->buf;
	oid->buf = buf;
	if(bp) FREEMEM(bp);

	return 0;
}


int
OBJECT_IDENTIFIER_parse_arcs(const char *oid_text, ssize_t oid_txt_length,
	long *arcs, unsigned int arcs_slots, const char **opt_oid_text_end) {
	unsigned int arcs_count = 0;
	const char *oid_end;
	long value = 0;
	enum {
		ST_SKIPSPACE,
		ST_WAITDIGITS,	/* Next character is expected to be a digit */
		ST_DIGITS
	} state = ST_SKIPSPACE;

	if(!oid_text || oid_txt_length < -1 || (arcs_slots && !arcs)) {
		if(opt_oid_text_end) *opt_oid_text_end = oid_text;
		errno = EINVAL;
		return -1;
	}

	if(oid_txt_length == -1)
		oid_txt_length = strlen(oid_text);

	for(oid_end = oid_text + oid_txt_length; oid_text<oid_end; oid_text++) {
	    switch(*oid_text) {
	    case 0x09: case 0x0a: case 0x0d: case 0x20:	/* whitespace */
		if(state == ST_SKIPSPACE) {
			continue;
		} else {
			break;	/* Finish */
		}
	    case 0x2e:	/* '.' */
		if(state != ST_DIGITS
		|| (oid_text + 1) == oid_end) {
			state = ST_WAITDIGITS;
			break;
		}
		if(arcs_count < arcs_slots)
			arcs[arcs_count] = value;
		arcs_count++;
		state = ST_WAITDIGITS;
		continue;
	    case 0x30: case 0x31: case 0x32: case 0x33: case 0x34:
	    case 0x35: case 0x36: case 0x37: case 0x38: case 0x39:
		if(state != ST_DIGITS) {
			state = ST_DIGITS;
			value = 0;
		}
		if(1) {
			long new_value = value * 10;
			if(new_value / 10 != value
			|| (value = new_value + (*oid_text - 0x30)) < 0) {
				/* Overflow */
				state = ST_WAITDIGITS;
				break;
			}
			continue;
		}
	    default:
		/* Unexpected symbols */
		state = ST_WAITDIGITS;
		break;
	    } /* switch() */
	    break;
	} /* for() */


	if(opt_oid_text_end) *opt_oid_text_end = oid_text;

	/* Finalize last arc */
	switch(state) {
	case ST_WAITDIGITS:
		errno = EINVAL;
		return -1;
	case ST_DIGITS:
		if(arcs_count < arcs_slots)
			arcs[arcs_count] = value;
		arcs_count++;
		/* Fall through */
	default:
		return arcs_count;
	}
}


