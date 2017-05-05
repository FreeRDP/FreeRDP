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

#ifndef _CERT_ST_H
#define _CERT_ST_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

typedef struct cert_policy_st cert_policy;

#ifdef HAVE_NSS
#include <cert.h>
#include <secoidt.h>
typedef SECOidTag ALGORITHM_TYPE;
#define ALGORITHM_NULL  SEC_OID_UNKNOWN
/* we really should make a neutral define for this */
#define X509 CERTCertificate
#else
#include <openssl/x509.h>
typedef const char *ALGORITHM_TYPE;
#define ALGORITHM_NULL  NULL
#endif


#endif /* _CERT_ST_H */
