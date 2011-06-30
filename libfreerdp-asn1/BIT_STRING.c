/*-
 * Copyright (c) 2003, 2004 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include "asn_internal.h"
#include "BIT_STRING.h"
#include "asn_internal.h"

/*
 * BIT STRING basic type description.
 */
static ber_tlv_tag_t asn_DEF_BIT_STRING_tags[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (3 << 2))
};
static asn_OCTET_STRING_specifics_t asn_DEF_BIT_STRING_specs = {
	sizeof(BIT_STRING_t),
	offsetof(BIT_STRING_t, _asn_ctx),
	ASN_OSUBV_BIT
};
asn_TYPE_descriptor_t asn_DEF_BIT_STRING = {
	"BIT STRING",
	"BIT_STRING",
	OCTET_STRING_free,         /* Implemented in terms of OCTET STRING */
	BIT_STRING_print,
	BIT_STRING_constraint,
	OCTET_STRING_decode_ber,   /* Implemented in terms of OCTET STRING */
	OCTET_STRING_encode_der,   /* Implemented in terms of OCTET STRING */
	OCTET_STRING_decode_xer_binary,
	BIT_STRING_encode_xer,
	OCTET_STRING_decode_uper,	/* Unaligned PER decoder */
	OCTET_STRING_encode_uper,	/* Unaligned PER encoder */
	0, /* Use generic outmost tag fetcher */
	asn_DEF_BIT_STRING_tags,
	sizeof(asn_DEF_BIT_STRING_tags)
	  / sizeof(asn_DEF_BIT_STRING_tags[0]),
	asn_DEF_BIT_STRING_tags,	/* Same as above */
	sizeof(asn_DEF_BIT_STRING_tags)
	  / sizeof(asn_DEF_BIT_STRING_tags[0]),
	0,	/* No PER visible constraints */
	0, 0,	/* No members */
	&asn_DEF_BIT_STRING_specs
};

/*
 * BIT STRING generic constraint.
 */
int
BIT_STRING_constraint(asn_TYPE_descriptor_t *td, const void *sptr,
		asn_app_constraint_failed_f *ctfailcb, void *app_key) {
	const BIT_STRING_t *st = (const BIT_STRING_t *)sptr;

	if(st && st->buf) {
		if((st->size == 0 && st->bits_unused)
		|| st->bits_unused < 0 || st->bits_unused > 7) {
			_ASN_CTFAIL(app_key, td, sptr,
				"%s: invalid padding byte (%s:%d)",
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

static char *_bit_pattern[16] = {
	"0000", "0001", "0010", "0011", "0100", "0101", "0110", "0111",
	"1000", "1001", "1010", "1011", "1100", "1101", "1110", "1111"
};

asn_enc_rval_t
BIT_STRING_encode_xer(asn_TYPE_descriptor_t *td, void *sptr,
	int ilevel, enum xer_encoder_flags_e flags,
		asn_app_consume_bytes_f *cb, void *app_key) {
	asn_enc_rval_t er;
	char scratch[128];
	char *p = scratch;
	char *scend = scratch + (sizeof(scratch) - 10);
	const BIT_STRING_t *st = (const BIT_STRING_t *)sptr;
	int xcan = (flags & XER_F_CANONICAL);
	uint8_t *buf;
	uint8_t *end;

	if(!st || !st->buf)
		_ASN_ENCODE_FAILED;

	er.encoded = 0;

	buf = st->buf;
	end = buf + st->size - 1;	/* Last byte is special */

	/*
	 * Binary dump
	 */
	for(; buf < end; buf++) {
		int v = *buf;
		int nline = xcan?0:(((buf - st->buf) % 8) == 0);
		if(p >= scend || nline) {
			er.encoded += p - scratch;
			_ASN_CALLBACK(scratch, p - scratch);
			p = scratch;
			if(nline) _i_ASN_TEXT_INDENT(1, ilevel);
		}
		memcpy(p + 0, _bit_pattern[v >> 4], 4);
		memcpy(p + 4, _bit_pattern[v & 0x0f], 4);
		p += 8;
	}

	if(!xcan && ((buf - st->buf) % 8) == 0)
		_i_ASN_TEXT_INDENT(1, ilevel);
	er.encoded += p - scratch;
	_ASN_CALLBACK(scratch, p - scratch);
	p = scratch;

	if(buf == end) {
		int v = *buf;
		int ubits = st->bits_unused;
		int i;
		for(i = 7; i >= ubits; i--)
			*p++ = (v & (1 << i)) ? 0x31 : 0x30;
		er.encoded += p - scratch;
		_ASN_CALLBACK(scratch, p - scratch);
	}

	if(!xcan) _i_ASN_TEXT_INDENT(1, ilevel - 1);

	_ASN_ENCODED_OK(er);
cb_failed:
	_ASN_ENCODE_FAILED;
}


/*
 * BIT STRING specific contents printer.
 */
int
BIT_STRING_print(asn_TYPE_descriptor_t *td, const void *sptr, int ilevel,
		asn_app_consume_bytes_f *cb, void *app_key) {
	static const char *h2c = "0123456789ABCDEF";
	char scratch[64];
	const BIT_STRING_t *st = (const BIT_STRING_t *)sptr;
	uint8_t *buf;
	uint8_t *end;
	char *p = scratch;

	(void)td;	/* Unused argument */

	if(!st || !st->buf)
		return (cb("<absent>", 8, app_key) < 0) ? -1 : 0;

	ilevel++;
	buf = st->buf;
	end = buf + st->size;

	/*
	 * Hexadecimal dump.
	 */
	for(; buf < end; buf++) {
		if((buf - st->buf) % 16 == 0 && (st->size > 16)
				&& buf != st->buf) {
			_i_INDENT(1);
			/* Dump the string */
			if(cb(scratch, p - scratch, app_key) < 0) return -1;
			p = scratch;
		}
		*p++ = h2c[*buf >> 4];
		*p++ = h2c[*buf & 0x0F];
		*p++ = 0x20;
	}

	if(p > scratch) {
		p--;	/* Eat the tailing space */

		if((st->size > 16)) {
			_i_INDENT(1);
		}

		/* Dump the incomplete 16-bytes row */
		if(cb(scratch, p - scratch, app_key) < 0)
			return -1;
	}

	return 0;
}

