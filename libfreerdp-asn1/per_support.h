/*
 * Copyright (c) 2005, 2006, 2007 Lev Walkin <vlm@lionet.info>.
 * All rights reserved.
 * Redistribution and modifications are permitted subject to BSD license.
 */
#ifndef	_PER_SUPPORT_H_
#define	_PER_SUPPORT_H_

#include "asn_system.h"		/* Platform-specific types */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Pre-computed PER constraints.
 */
typedef struct asn_per_constraint_s {
	enum asn_per_constraint_flags {
		APC_UNCONSTRAINED	= 0x0,	/* No PER visible constraints */
		APC_SEMI_CONSTRAINED	= 0x1,	/* Constrained at "lb" */
		APC_CONSTRAINED		= 0x2,	/* Fully constrained */
		APC_EXTENSIBLE		= 0x4	/* May have extension */
	} flags;
	int  range_bits;		/* Full number of bits in the range */
	int  effective_bits;		/* Effective bits */
	long lower_bound;		/* "lb" value */
	long upper_bound;		/* "ub" value */
} asn_per_constraint_t;
typedef struct asn_per_constraints_s {
	asn_per_constraint_t value;
	asn_per_constraint_t size;
	int (*value2code)(unsigned int value);
	int (*code2value)(unsigned int code);
} asn_per_constraints_t;

/*
 * This structure describes a position inside an incoming PER bit stream.
 */
typedef struct asn_per_data_s {
  const uint8_t *buffer;  /* Pointer to the octet stream */
         size_t  nboff;   /* Bit offset to the meaningful bit */
         size_t  nbits;   /* Number of bits in the stream */
         size_t  moved;   /* Number of bits moved through this bit stream */
  int (*refill)(struct asn_per_data_s *);
  void *refill_key;
} asn_per_data_t;

/*
 * Extract a small number of bits (<= 31) from the specified PER data pointer.
 * This function returns -1 if the specified number of bits could not be
 * extracted due to EOD or other conditions.
 */
int32_t per_get_few_bits(asn_per_data_t *per_data, int get_nbits);

/* Undo the immediately preceeding "get_few_bits" operation */
void per_get_undo(asn_per_data_t *per_data, int get_nbits);

/*
 * Extract a large number of bits from the specified PER data pointer.
 * This function returns -1 if the specified number of bits could not be
 * extracted due to EOD or other conditions.
 */
int per_get_many_bits(asn_per_data_t *pd, uint8_t *dst, int right_align,
			int get_nbits);

/*
 * Get the length "n" from the Unaligned PER stream.
 */
ssize_t uper_get_length(asn_per_data_t *pd,
			int effective_bound_bits,
			int *repeat);

/*
 * Get the normally small length "n".
 */
ssize_t uper_get_nslength(asn_per_data_t *pd);

/*
 * Get the normally small non-negative whole number.
 */
ssize_t uper_get_nsnnwn(asn_per_data_t *pd);

/* Non-thread-safe debugging function, don't use it */
char *per_data_string(asn_per_data_t *pd);

/*
 * This structure supports forming PER output.
 */
typedef struct asn_per_outp_s {
	uint8_t *buffer;	/* Pointer into the (tmpspace) */
	size_t nboff;		/* Bit offset to the meaningful bit */
	size_t nbits;		/* Number of bits left in (tmpspace) */
	uint8_t tmpspace[32];	/* Preliminary storage to hold data */
	int (*outper)(const void *data, size_t size, void *op_key);
	void *op_key;		/* Key for (outper) data callback */
	size_t flushed_bytes;	/* Bytes already flushed through (outper) */
} asn_per_outp_t;

/* Output a small number of bits (<= 31) */
int per_put_few_bits(asn_per_outp_t *per_data, uint32_t bits, int obits);

/* Output a large number of bits */
int per_put_many_bits(asn_per_outp_t *po, const uint8_t *src, int put_nbits);

/*
 * Put the length "n" to the Unaligned PER stream.
 * This function returns the number of units which may be flushed
 * in the next units saving iteration.
 */
ssize_t uper_put_length(asn_per_outp_t *po, size_t whole_length);

/*
 * Put the normally small length "n" to the Unaligned PER stream.
 * Returns 0 or -1.
 */
int uper_put_nslength(asn_per_outp_t *po, size_t length);

/*
 * Put the normally small non-negative whole number.
 */
int uper_put_nsnnwn(asn_per_outp_t *po, int n);

#ifdef __cplusplus
}
#endif

#endif	/* _PER_SUPPORT_H_ */
