/*
 * PKCS #11 PAM Login Module
 * Copyright (C) 2003 Mario Strasser <mast@gmx.net>,
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * $Id$
 */

/** \file
Several routines to:
<ul>
<li> Verify certificate</li>
<li> Check for revocation list</li>
<li> Verify signature</li>
</ul>
*/

#ifndef __CERT_VFY_H_
#define __CERT_VFY_H_

#include "cert_st.h"

typedef enum {
	/** Do not perform any CRL verification */
	CRLP_NONE,
	/** Retrieve CRL from CA site */
	CRLP_ONLINE,
	/** Retrieve CRL from local filesystem */
	CRLP_OFFLINE,
	/** Try CRL check online, else ofline, else fail */
	CRLP_AUTO
	} crl_policy_t;

typedef enum {
	OCSP_NONE,
	OCSP_ON
	} ocsp_policy_t;

struct cert_policy_st {
	int ca_policy;
	int crl_policy;
	int signature_policy;
	const char *ca_dir;
	const char *crl_dir;
	const char *nss_dir;
	int ocsp_policy;
};

#ifndef __CERT_VFY_C
#define CERTVFY_EXTERN extern
#else
#define CERTVFY_EXTERN
#endif

/**
* Verify provided certificate, and if needed, CRL
*@param x509 Certificate to check
*@param policy CRL verify policy
*@return 1 on cert vfy sucess, 0 on fail, -1 on process error
*/
CERTVFY_EXTERN int verify_certificate(X509 * x509, cert_policy *policy);

/**
* Verify signature of provided data
*@param x509 Certificate to be used
*@param data Byte array of data to check
*@param data_length Lenght of provided byte array
*@param signature Byte array of signature to check
*@param signature_length Length of signature byte array
*@return 1 on signature vfy sucess, 0 on vfy fail, -1 on process error
*/
CERTVFY_EXTERN int verify_signature(X509 * x509, unsigned char *data, int data_length, unsigned char *signature, int signature_length);

#undef CERTVFY_EXTERN

#endif /* __CERT_VFY_H_ */
