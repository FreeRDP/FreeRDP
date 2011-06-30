/*-
 * Copyright (c) 2004 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	_XER_ENCODER_H_
#define	_XER_ENCODER_H_

#include "asn_application.h"

#ifdef __cplusplus
extern "C" {
#endif

struct asn_TYPE_descriptor_s;	/* Forward declaration */

/* Flags used by the xer_encode() and (*xer_type_encoder_f), defined below */
enum xer_encoder_flags_e {
	/* Mode of encoding */
	XER_F_BASIC	= 0x01,	/* BASIC-XER (pretty-printing) */
	XER_F_CANONICAL	= 0x02	/* Canonical XER (strict rules) */
};

/*
 * The XER encoder of any type. May be invoked by the application.
 */
asn_enc_rval_t xer_encode(struct asn_TYPE_descriptor_s *type_descriptor,
		void *struct_ptr,	/* Structure to be encoded */
		enum xer_encoder_flags_e xer_flags,
		asn_app_consume_bytes_f *consume_bytes_cb,
		void *app_key		/* Arbitrary callback argument */
	);

/*
 * The variant of the above function which dumps the BASIC-XER (XER_F_BASIC)
 * output into the chosen file pointer.
 * RETURN VALUES:
 * 	 0: The structure is printed.
 * 	-1: Problem printing the structure.
 * WARNING: No sensible errno value is returned.
 */
int xer_fprint(FILE *stream, struct asn_TYPE_descriptor_s *td, void *sptr);

/*
 * Type of the generic XER encoder.
 */
typedef asn_enc_rval_t (xer_type_encoder_f)(
		struct asn_TYPE_descriptor_s *type_descriptor,
		void *struct_ptr,	/* Structure to be encoded */
		int ilevel,		/* Level of indentation */
		enum xer_encoder_flags_e xer_flags,
		asn_app_consume_bytes_f *consume_bytes_cb,	/* Callback */
		void *app_key		/* Arbitrary callback argument */
	);

#ifdef __cplusplus
}
#endif

#endif	/* _XER_ENCODER_H_ */
