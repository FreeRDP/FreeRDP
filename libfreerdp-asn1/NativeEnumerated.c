/*-
 * Copyright (c) 2004, 2007 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
/*
 * Read the NativeInteger.h for the explanation wrt. differences between
 * INTEGER and NativeInteger.
 * Basically, both are decoders and encoders of ASN.1 INTEGER type, but this
 * implementation deals with the standard (machine-specific) representation
 * of them instead of using the platform-independent buffer.
 */
#include "asn_internal.h"
#include "NativeEnumerated.h"

/*
 * NativeEnumerated basic type description.
 */
static ber_tlv_tag_t asn_DEF_NativeEnumerated_tags[] = {
	(ASN_TAG_CLASS_UNIVERSAL | (10 << 2))
};
asn_TYPE_descriptor_t asn_DEF_NativeEnumerated = {
	"ENUMERATED",			/* The ASN.1 type is still ENUMERATED */
	"ENUMERATED",
	NativeInteger_free,
	NativeInteger_print,
	asn_generic_no_constraint,
	NativeInteger_decode_ber,
	NativeInteger_encode_der,
	NativeInteger_decode_xer,
	NativeEnumerated_encode_xer,
	NativeEnumerated_decode_uper,
	NativeEnumerated_encode_uper,
	0, /* Use generic outmost tag fetcher */
	asn_DEF_NativeEnumerated_tags,
	sizeof(asn_DEF_NativeEnumerated_tags) / sizeof(asn_DEF_NativeEnumerated_tags[0]),
	asn_DEF_NativeEnumerated_tags,	/* Same as above */
	sizeof(asn_DEF_NativeEnumerated_tags) / sizeof(asn_DEF_NativeEnumerated_tags[0]),
	0,	/* No PER visible constraints */
	0, 0,	/* No members */
	0	/* No specifics */
};

asn_enc_rval_t
NativeEnumerated_encode_xer(asn_TYPE_descriptor_t *td, void *sptr,
        int ilevel, enum xer_encoder_flags_e flags,
                asn_app_consume_bytes_f *cb, void *app_key) {
	asn_INTEGER_specifics_t *specs=(asn_INTEGER_specifics_t *)td->specifics;
        asn_enc_rval_t er;
        const long *native = (const long *)sptr;
	const asn_INTEGER_enum_map_t *el;

        (void)ilevel;
        (void)flags;

        if(!native) _ASN_ENCODE_FAILED;

	el = INTEGER_map_value2enum(specs, *native);
	if(el) {
		size_t srcsize = el->enum_len + 5;
		char *src = (char *)alloca(srcsize);

		er.encoded = snprintf(src, srcsize, "<%s/>", el->enum_name);
		assert(er.encoded > 0 && (size_t)er.encoded < srcsize);
		if(cb(src, er.encoded, app_key) < 0) _ASN_ENCODE_FAILED;
		_ASN_ENCODED_OK(er);
	} else {
		ASN_DEBUG("ASN.1 forbids dealing with "
			"unknown value of ENUMERATED type");
		_ASN_ENCODE_FAILED;
	}
}

asn_dec_rval_t
NativeEnumerated_decode_uper(asn_codec_ctx_t *opt_codec_ctx,
	asn_TYPE_descriptor_t *td, asn_per_constraints_t *constraints,
	void **sptr, asn_per_data_t *pd) {
	asn_INTEGER_specifics_t *specs = (asn_INTEGER_specifics_t *)td->specifics;
	asn_dec_rval_t rval = { RC_OK, 0 };
	long *native = (long *)*sptr;
	asn_per_constraint_t *ct;
	long value;

	(void)opt_codec_ctx;

	if(constraints) ct = &constraints->value;
	else if(td->per_constraints) ct = &td->per_constraints->value;
	else _ASN_DECODE_FAILED;	/* Mandatory! */
	if(!specs) _ASN_DECODE_FAILED;

	if(!native) {
		native = (long *)(*sptr = CALLOC(1, sizeof(*native)));
		if(!native) _ASN_DECODE_FAILED;
	}

	ASN_DEBUG("Decoding %s as NativeEnumerated", td->name);

	if(ct->flags & APC_EXTENSIBLE) {
		int inext = per_get_few_bits(pd, 1);
		if(inext < 0) _ASN_DECODE_STARVED;
		if(inext) ct = 0;
	}

	if(ct && ct->range_bits >= 0) {
		value = per_get_few_bits(pd, ct->range_bits);
		if(value < 0) _ASN_DECODE_STARVED;
		if(value >= (specs->extension
			? specs->extension - 1 : specs->map_count))
			_ASN_DECODE_FAILED;
	} else {
		if(!specs->extension)
			_ASN_DECODE_FAILED;
		/*
		 * X.691, #10.6: normally small non-negative whole number;
		 */
		value = uper_get_nsnnwn(pd);
		if(value < 0) _ASN_DECODE_STARVED;
		value += specs->extension - 1;
		if(value >= specs->map_count)
			_ASN_DECODE_FAILED;
	}

	*native = specs->value2enum[value].nat_value;
	ASN_DEBUG("Decoded %s = %ld", td->name, *native);

	return rval;
}

static int
NativeEnumerated__compar_value2enum(const void *ap, const void *bp) {
	const asn_INTEGER_enum_map_t *a = ap;
	const asn_INTEGER_enum_map_t *b = bp;
	if(a->nat_value == b->nat_value)
		return 0;
	if(a->nat_value < b->nat_value)
		return -1;
	return 1;
}

asn_enc_rval_t
NativeEnumerated_encode_uper(asn_TYPE_descriptor_t *td,
	asn_per_constraints_t *constraints, void *sptr, asn_per_outp_t *po) {
	asn_INTEGER_specifics_t *specs = (asn_INTEGER_specifics_t *)td->specifics;
	asn_enc_rval_t er;
	long native, value;
	asn_per_constraint_t *ct;
	int inext = 0;
	asn_INTEGER_enum_map_t key;
	asn_INTEGER_enum_map_t *kf;

	if(!sptr) _ASN_ENCODE_FAILED;
	if(!specs) _ASN_ENCODE_FAILED;

	if(constraints) ct = &constraints->value;
	else if(td->per_constraints) ct = &td->per_constraints->value;
	else _ASN_ENCODE_FAILED;	/* Mandatory! */

	ASN_DEBUG("Encoding %s as NativeEnumerated", td->name);

	er.encoded = 0;

	native = *(long *)sptr;
	if(native < 0) _ASN_ENCODE_FAILED;

	key.nat_value = native;
	kf = bsearch(&key, specs->value2enum, specs->map_count,
		sizeof(key), NativeEnumerated__compar_value2enum);
	if(!kf) {
		ASN_DEBUG("No element corresponds to %ld", native);
		_ASN_ENCODE_FAILED;
	}
	value = kf - specs->value2enum;

	if(ct->range_bits >= 0) {
		int cmpWith = specs->extension
				? specs->extension - 1 : specs->map_count;
		if(value >= cmpWith)
			inext = 1;
	}
	if(ct->flags & APC_EXTENSIBLE) {
		if(per_put_few_bits(po, inext, 1))
			_ASN_ENCODE_FAILED;
		if(inext) ct = 0;
	} else if(inext) {
		_ASN_ENCODE_FAILED;
	}

	if(ct && ct->range_bits >= 0) {
		if(per_put_few_bits(po, value, ct->range_bits))
			_ASN_ENCODE_FAILED;
		_ASN_ENCODED_OK(er);
	}

	if(!specs->extension)
		_ASN_ENCODE_FAILED;

	/*
	 * X.691, #10.6: normally small non-negative whole number;
	 */
	ASN_DEBUG("value = %ld, ext = %d, inext = %d, res = %ld",
		value, specs->extension, inext,
		value - (inext ? (specs->extension - 1) : 0));
	if(uper_put_nsnnwn(po, value - (inext ? (specs->extension - 1) : 0)))
		_ASN_ENCODE_FAILED;

	_ASN_ENCODED_OK(er);
}

