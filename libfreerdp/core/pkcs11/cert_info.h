/*
 * PKCS #11 PAM Login Module
 * Copyright (C) 2003-2004 Mario Strasser <mast@gmx.net>
 * Copyright (C) 2005 Juan Antonio Martinez <jonsito@teleline.es>
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

#ifndef __CERT_INFO_H_
#define __CERT_INFO_H_

#include "cert_st.h"

/** Certificate Common Name */
#define CERT_CN		1
/** Certificate subject */
#define CERT_SUBJECT	2
/** Kerberos principal name */
#define CERT_KPN	3
/** Certificate e-mail */
#define CERT_EMAIL	4
/** Microsoft's Universal Principal Name */
#define CERT_UPN	5
/** Certificate Unique Identifier */
#define CERT_UID	6
/** Certificate Public Key (PEM Format)*/
#define CERT_PUK	7
/** Certificate Digest */
#define CERT_DIGEST	8
/** Certificate Public key in OpenSSH format */
#define CERT_SSHPUK	9
/** Certificate in PEM format */
#define CERT_PEM	10
/** Certificate issuer */
#define CERT_ISSUER	11
/** Certificate serial number */
#define CERT_SERIAL	12
/** Certificate key algorithm */
#define CERT_KEY_ALG	13

/** Max size of returned certificate content array */
#define CERT_INFO_SIZE 16
/** Max number of entries to find from certificate */
#define CERT_INFO_MAX_ENTRIES ( CERT_INFO_SIZE - 1 )

#ifndef __CERT_INFO_C_
#define CERTINFO_EXTERN extern
#else
#define CERTINFO_EXTERN
#endif

/**
* Generate and compose a certificate chain
* @param cert Certificate to add
* @param certs pointer to list of certificates
* @param ncerts pointer to number of certificates in list
*/
void add_cert(X509 *cert, X509 ***certs, int *ncerts);

/**
* Request info on certificate
* @param x509 certificate to parse
* @param type information to retrieve
* @param algorithm to use in evaluate certificate digest; else null
* @return utf-8 string array with provided information
*/
CERTINFO_EXTERN char **cert_info(X509 *x509, int type, ALGORITHM_TYPE algorithm);

#undef CERTINFO_EXTERN

#endif /* __CERT_INFO_H_ */
