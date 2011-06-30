/*-
 * Copyright (c) 2003, 2004, 2006 Lev Walkin <vlm@lionet.info>.
 * All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#include "asn_internal.h"
#include "constr_SEQUENCE_OF.h"
#include "asn_SEQUENCE_OF.h"

/*
 * The DER encoder of the SEQUENCE OF type.
 */
asn_enc_rval_t
SEQUENCE_OF_encode_der(asn_TYPE_descriptor_t *td, void *ptr,
	int tag_mode, ber_tlv_tag_t tag,
	asn_app_consume_bytes_f *cb, void *app_key) {
	asn_TYPE_member_t *elm = td->elements;
	asn_anonymous_sequence_ *list = _A_SEQUENCE_FROM_VOID(ptr);
	size_t computed_size = 0;
	ssize_t encoding_size = 0;
	asn_enc_rval_t erval;
	int edx;

	ASN_DEBUG("Estimating size of SEQUENCE OF %s", td->name);

	/*
	 * Gather the length of the underlying members sequence.
	 */
	for(edx = 0; edx < list->count; edx++) {
		void *memb_ptr = list->array[edx];
		if(!memb_ptr) continue;
		erval = elm->type->der_encoder(elm->type, memb_ptr,
			0, elm->tag,
			0, 0);
		if(erval.encoded == -1)
			return erval;
		computed_size += erval.encoded;
	}

	/*
	 * Encode the TLV for the sequence itself.
	 */
	encoding_size = der_write_tags(td, computed_size, tag_mode, 1, tag,
		cb, app_key);
	if(encoding_size == -1) {
		erval.encoded = -1;
		erval.failed_type = td;
		erval.structure_ptr = ptr;
		return erval;
	}

	computed_size += encoding_size;
	if(!cb) {
		erval.encoded = computed_size;
		_ASN_ENCODED_OK(erval);
	}

	ASN_DEBUG("Encoding members of SEQUENCE OF %s", td->name);

	/*
	 * Encode all members.
	 */
	for(edx = 0; edx < list->count; edx++) {
		void *memb_ptr = list->array[edx];
		if(!memb_ptr) continue;
		erval = elm->type->der_encoder(elm->type, memb_ptr,
			0, elm->tag,
			cb, app_key);
		if(erval.encoded == -1)
			return erval;
		encoding_size += erval.encoded;
	}

	if(computed_size != (size_t)encoding_size) {
		/*
		 * Encoded size is not equal to the computed size.
		 */
		erval.encoded = -1;
		erval.failed_type = td;
		erval.structure_ptr = ptr;
	} else {
		erval.encoded = computed_size;
		erval.structure_ptr = 0;
		erval.failed_type = 0;
	}

	return erval;
}

asn_enc_rval_t
SEQUENCE_OF_encode_xer(asn_TYPE_descriptor_t *td, void *sptr,
	int ilevel, enum xer_encoder_flags_e flags,
		asn_app_consume_bytes_f *cb, void *app_key) {
	asn_enc_rval_t er;
        asn_SET_OF_specifics_t *specs = (asn_SET_OF_specifics_t *)td->specifics;
	asn_TYPE_member_t *elm = td->elements;
	asn_anonymous_sequence_ *list = _A_SEQUENCE_FROM_VOID(sptr);
	const char *mname = specs->as_XMLValueList
		? 0 : ((*elm->name) ? elm->name : elm->type->xml_tag);
	unsigned int mlen = mname ? strlen(mname) : 0;
	int xcan = (flags & XER_F_CANONICAL);
	int i;

	if(!sptr) _ASN_ENCODE_FAILED;

	er.encoded = 0;

	for(i = 0; i < list->count; i++) {
		asn_enc_rval_t tmper;
		void *memb_ptr = list->array[i];
		if(!memb_ptr) continue;

		if(mname) {
			if(!xcan) _i_ASN_TEXT_INDENT(1, ilevel);
			_ASN_CALLBACK3("<", 1, mname, mlen, ">", 1);
		}

		tmper = elm->type->xer_encoder(elm->type, memb_ptr,
				ilevel + 1, flags, cb, app_key);
		if(tmper.encoded == -1) return tmper;
                if(tmper.encoded == 0 && specs->as_XMLValueList) {
                        const char *name = elm->type->xml_tag;
			size_t len = strlen(name);
			if(!xcan) _i_ASN_TEXT_INDENT(1, ilevel + 1);
			_ASN_CALLBACK3("<", 1, name, len, "/>", 2);
                }

		if(mname) {
			_ASN_CALLBACK3("</", 2, mname, mlen, ">", 1);
			er.encoded += 5;
		}

		er.encoded += (2 * mlen) + tmper.encoded;
	}

	if(!xcan) _i_ASN_TEXT_INDENT(1, ilevel - 1);

	_ASN_ENCODED_OK(er);
cb_failed:
	_ASN_ENCODE_FAILED;
}

asn_enc_rval_t
SEQUENCE_OF_encode_uper(asn_TYPE_descriptor_t *td,
	asn_per_constraints_t *constraints, void *sptr, asn_per_outp_t *po) {
	asn_anonymous_sequence_ *list;
	asn_per_constraint_t *ct;
	asn_enc_rval_t er;
	asn_TYPE_member_t *elm = td->elements;
	int seq;

	if(!sptr) _ASN_ENCODE_FAILED;
	list = _A_SEQUENCE_FROM_VOID(sptr);

	er.encoded = 0;

	ASN_DEBUG("Encoding %s as SEQUENCE OF (%d)", td->name, list->count);

	if(constraints) ct = &constraints->size;
	else if(td->per_constraints) ct = &td->per_constraints->size;
	else ct = 0;

	/* If extensible constraint, check if size is in root */
	if(ct) {
		int not_in_root = (list->count < ct->lower_bound
				|| list->count > ct->upper_bound);
		ASN_DEBUG("lb %ld ub %ld %s",
			ct->lower_bound, ct->upper_bound,
			ct->flags & APC_EXTENSIBLE ? "ext" : "fix");
		if(ct->flags & APC_EXTENSIBLE) {
			/* Declare whether size is in extension root */
			if(per_put_few_bits(po, not_in_root, 1))
				_ASN_ENCODE_FAILED;
			if(not_in_root) ct = 0;
		} else if(not_in_root && ct->effective_bits >= 0)
			_ASN_ENCODE_FAILED;
	}

	if(ct && ct->effective_bits >= 0) {
		/* X.691, #19.5: No length determinant */
		if(per_put_few_bits(po, list->count - ct->lower_bound,
				ct->effective_bits))
			_ASN_ENCODE_FAILED;
	}

	for(seq = -1; seq < list->count;) {
		ssize_t mayEncode;
		if(seq < 0) seq = 0;
		if(ct && ct->effective_bits >= 0) {
			mayEncode = list->count;
		} else {
			mayEncode = uper_put_length(po, list->count - seq);
			if(mayEncode < 0) _ASN_ENCODE_FAILED;
		}

		while(mayEncode--) {
			void *memb_ptr = list->array[seq++];
			if(!memb_ptr) _ASN_ENCODE_FAILED;
			er = elm->type->uper_encoder(elm->type,
				elm->per_constraints, memb_ptr, po);
			if(er.encoded == -1)
				_ASN_ENCODE_FAILED;
		}
	}

	_ASN_ENCODED_OK(er);
}

