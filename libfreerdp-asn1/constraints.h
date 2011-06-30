/*-
 * Copyright (c) 2004, 2006 Lev Walkin <vlm@lionet.info>. All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	_ASN1_CONSTRAINTS_VALIDATOR_H_
#define	_ASN1_CONSTRAINTS_VALIDATOR_H_

#include "asn_system.h"		/* Platform-dependent types */

#ifdef __cplusplus
extern "C" {
#endif

struct asn_TYPE_descriptor_s;		/* Forward declaration */

/*
 * Validate the structure according to the ASN.1 constraints.
 * If errbuf and errlen are given, they shall be pointing to the appropriate
 * buffer space and its length before calling this function. Alternatively,
 * they could be passed as NULL's. If constraints validation fails,
 * errlen will contain the actual number of bytes taken from the errbuf
 * to encode an error message (properly 0-terminated).
 * 
 * RETURN VALUES:
 * This function returns 0 in case all ASN.1 constraints are met
 * and -1 if one or more constraints were failed.
 */
int
asn_check_constraints(struct asn_TYPE_descriptor_s *type_descriptor,
	const void *struct_ptr,	/* Target language's structure */
	char *errbuf,		/* Returned error description */
	size_t *errlen		/* Length of the error description */
	);


/*
 * Generic type for constraint checking callback,
 * associated with every type descriptor.
 */
typedef int (asn_constr_check_f)(
	struct asn_TYPE_descriptor_s *type_descriptor,
	const void *struct_ptr,
	asn_app_constraint_failed_f *optional_callback,	/* Log the error */
	void *optional_app_key		/* Opaque key passed to a callback */
	);

/*******************************
 * INTERNALLY USEFUL FUNCTIONS *
 *******************************/

asn_constr_check_f asn_generic_no_constraint;	/* No constraint whatsoever */
asn_constr_check_f asn_generic_unknown_constraint; /* Not fully supported */

/*
 * Invoke the callback with a complete error message.
 */
#define	_ASN_CTFAIL	if(ctfailcb) ctfailcb

#ifdef __cplusplus
}
#endif

#endif	/* _ASN1_CONSTRAINTS_VALIDATOR_H_ */
