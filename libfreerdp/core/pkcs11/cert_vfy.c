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
#include "cert_vfy.h"

#include <freerdp/log.h>
#define TAG FREERDP_TAG("pkcs11.cert_vfy")

#ifdef HAVE_NSS

#include <cryptohi.h>
#include "cert.h"
#include "secutil.h"

int verify_certificate(X509 * x509, cert_policy *policy)
{
    SECStatus rv;
    CERTCertDBHandle *handle;

    handle = CERT_GetDefaultCertDB();

    /* NSS already check all the revocation info with OCSP and crls */
    WLog_DBG(TAG, "Verifying Cert: %s (%s)", x509->nickname, x509->subjectName);
    rv = CERT_VerifyCertNow(handle, x509, PR_TRUE, certUsageSSLClient,
		NULL);
    if (rv != SECSuccess) {
	WLog_DBG(TAG, "Couldn't verify Cert: %s", SECU_Strerror(PR_GetError()));
    }

    return rv == SECSuccess ? 1 : 0;
}

int verify_signature(X509 * x509, unsigned char *data, int data_length,
                     unsigned char *signature, int signature_length)
{

  SECKEYPublicKey *key;
  SECOidTag algid;
  SECStatus rv;
  SECItem sig;

  /* grab the key */
  key = CERT_ExtractPublicKey(x509);
  if (key == NULL) {
	WLog_DBG(TAG, "Couldn't extract key from certificate: %s",
		SECU_Strerror(PR_GetError()));
	return -1;
  }
  /* shouldn't the algorithm be passed in? */
  algid = SEC_GetSignatureAlgorithmOidTag(key->keyType, SEC_OID_SHA1);

  sig.data = signature;
  sig.len = signature_length;
  rv = VFY_VerifyData(data, data_length, key, &sig, algid, NULL);
  if (rv != SECSuccess) {
	WLog_DBG(TAG, "Couldn't verify Signature: %s", SECU_Strerror(PR_GetError()));
  }
  SECKEY_DestroyPublicKey(key);
  return (rv == SECSuccess)? 0 : 1;
}

#else

#define __CERT_VFY_C_

#include <string.h>
#include <openssl/objects.h>
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include "error.h"
#include "base64.h"
#include "uri.h"

static X509_CRL *download_crl(const char *uri)
{
  int rv;
  unsigned int i, j;
  unsigned char *data, *der;
  const unsigned char *p;
  size_t data_len, der_len;
  X509_CRL *crl;

  rv = get_from_uri(uri, &data, &data_len);
  if (rv != 0) {
    set_error("get_from_uri() failed: %s", get_error());
    return NULL;
  }
  /* convert base64 to der if needed */
  for (i = 0; i <= data_len - 24; i++) {
    if (!strncmp((const char *)&data[i], "-----BEGIN X509 CRL-----", 24))
      break;
  }
  for (j = 0; j <= data_len - 22; j++) {
    if (!strncmp((const char *)&data[j], "-----END X509 CRL-----", 22))
      break;
  }
  if (i <= data_len - 24 && j <= data_len - 22 && i < j) {
    /* base64 format */
    WLog_DBG(TAG, "crl is base64 encoded");
    der_len = (j - i + 1);      /* roughly */
    der = malloc(der_len);
    if (der == NULL) {
      free(data);
      set_error("not enough free memory available");
      return NULL;
    }
    data[j] = 0;
    der_len = base64_decode((const char *)&data[i + 24], der, der_len);
    free(data);
    if (der_len <= 0) {
      set_error("invalid base64 (pem) format");
      return NULL;
    }
    p = der;
    crl = d2i_X509_CRL(NULL, &p, der_len);
    free(der);
  } else {
    /* der format */
    WLog_DBG(TAG, "crl is der encoded");
    p = data;
    crl = d2i_X509_CRL(NULL, &p, data_len);
    free(data);
  }
  if (crl == NULL)
    set_error("d2i_X509_CRL() failed");
  return crl;
}

static int verify_crl(X509_CRL * crl, X509_STORE_CTX * ctx)
{
  int rv;
  X509_OBJECT obj;
  EVP_PKEY *pkey;

  /* get issuer certificate */
  rv = X509_STORE_get_by_subject(ctx, X509_LU_X509, X509_CRL_get_issuer(crl), &obj);
  if (rv <= 0) {
    set_error("getting the certificate of the crl-issuer failed");
    return -1;
  }
  /* extract public key and verify signature */
  pkey = X509_get_pubkey(obj.data.x509);
  X509_OBJECT_free_contents(&obj);
  if (pkey == NULL) {
    set_error("getting the issuer's public key failed");
    return -1;
  }
  rv = X509_CRL_verify(crl, pkey);
  EVP_PKEY_free(pkey);
  if (rv < 0) {
    set_error("X509_CRL_verify() failed: %s", ERR_error_string(ERR_get_error(), NULL));
    return -1;
  } else if (rv == 0) {
    WLog_DBG(TAG, "crl is invalid");
    return 0;
  }
  /* compare update times */
  rv = X509_cmp_current_time(X509_CRL_get_lastUpdate(crl));
  if (rv == 0) {
    set_error("crl has an invalid last update field");
    return -1;
  }
  if (rv > 0) {
    WLog_DBG(TAG, "crl is not yet valid");
    return 0;
  }
  rv = X509_cmp_current_time(X509_CRL_get_nextUpdate(crl));
  if (rv == 0) {
    set_error("crl has an invalid next update field");
    return -1;
  }
  if (rv < 0) {
    WLog_DBG(TAG, "crl has expired");
    return 0;
  }
  return 1;
}

/* the structure DIST_POINT_NAME_st has been changed from 0.9.6 to 0.9.7 */
#if OPENSSL_VERSION_NUMBER >= 0x00907000L
#define GET_FULLNAME(a) a->name.fullname
#else
#define GET_FULLNAME(a) a->fullname
#endif

static int check_for_revocation(X509 * x509, X509_STORE_CTX * ctx, crl_policy_t policy)
{
  int rv, i, j;
  X509_OBJECT obj;
  X509_REVOKED rev;
  STACK_OF(DIST_POINT) * dist_points;
  DIST_POINT *point;
  GENERAL_NAME *name;
  X509_CRL *crl;

  WLog_DBG(TAG, "crl policy: %d", policy);
  if (policy == CRLP_NONE) {
    /* NONE */
    WLog_DBG(TAG, "no revocation-check performed");
    return 1;
  } else if (policy == CRLP_AUTO) {
    /* AUTO -> first try it ONLINE then OFFLINE */
    rv = check_for_revocation(x509, ctx, CRLP_ONLINE);
    if (rv < 0) {
      WLog_DBG(TAG, "check_for_revocation() failed: %s", get_error());
      rv = check_for_revocation(x509, ctx, CRLP_OFFLINE);
    }
    return rv;
  } else if (policy == CRLP_OFFLINE) {
    /* OFFLINE */
    WLog_DBG(TAG, "looking for an dedicated local crl");
    rv = X509_STORE_get_by_subject(ctx, X509_LU_CRL, X509_get_issuer_name(x509), &obj);
    if (rv <= 0) {
      set_error("no dedicated crl available");
      return -1;
    }
    crl = X509_CRL_dup(obj.data.crl);
    X509_OBJECT_free_contents(&obj);
  } else if (policy == CRLP_ONLINE) {
    /* ONLINE */
    WLog_DBG(TAG, "extracting crl distribution points");
    dist_points = X509_get_ext_d2i(x509, NID_crl_distribution_points, NULL, NULL);
    if (dist_points == NULL) {
      /* if there is not crl distribution point in the certificate hava a look at the ca certificate */
      rv = X509_STORE_get_by_subject(ctx, X509_LU_X509, X509_get_issuer_name(x509), &obj);
      if (rv <= 0) {
        set_error("no dedicated ca certificate available");
        return -1;
      }
      dist_points = X509_get_ext_d2i(obj.data.x509, NID_crl_distribution_points, NULL, NULL);
      X509_OBJECT_free_contents(&obj);
      if (dist_points == NULL) {
        set_error("neither the user nor the ca certificate does contain a crl distribution point");
        return -1;
      }
    }
    crl = NULL;
    for (i = 0; i < sk_DIST_POINT_num(dist_points) && crl == NULL; i++) {
      point = sk_DIST_POINT_value(dist_points, i);
      /* until now, only fullName is supported */
      if (point->distpoint != NULL && GET_FULLNAME(point->distpoint) != NULL) {
        for (j = 0; j < sk_GENERAL_NAME_num(GET_FULLNAME(point->distpoint)); j++) {
          name = sk_GENERAL_NAME_value(GET_FULLNAME(point->distpoint), j);
          if (name != NULL && name->type == GEN_URI) {
            WLog_DBG(TAG, "downloading crl from %s", name->d.ia5->data);
            crl = download_crl((const char *)name->d.ia5->data);

            /*crl = download_crl("file:///home/mario/projects/pkcs11_login/tests/ca_crl_0.pem"); */
            /*crl = download_crl("http://www-t.zhwin.ch/ca/root_ca.crl"); */
            /*crl = download_crl("http://www.zhwin.ch/~sri/"); */
            /*crl = download_crl("ldap://directory.verisign.com:389/CN=VeriSign IECA, OU=IECA-3, OU=Contractor, OU=PKI, OU=DOD, O=U.S. Government, C=US?certificateRevocationList;binary"); */
            if (crl != NULL)
              break;
            else
              WLog_DBG(TAG, "download_crl() failed: %s", get_error());
          }
        }
      }
    }
    sk_DIST_POINT_pop_free(dist_points, DIST_POINT_free);
    if (crl == NULL) {
      set_error("downloading the crl failed for all distribution points");
      return -1;
    }
  } else {
    set_error("policy %d is not supported", policy);
    return -1;
  }
  /* verify the crl and check whether the certificate is revoked or not */
  WLog_DBG(TAG, "verifying crl");
  rv = verify_crl(crl, ctx);
  if (rv < 0) {
    X509_CRL_free(crl);
    set_error("verify_crl() failed: %s", get_error());
    return -1;
  } else if (rv == 0) {
    return 0;
  }
  rev.serialNumber = X509_get_serialNumber(x509);
  rv = sk_X509_REVOKED_find(crl->crl->revoked, &rev);
  X509_CRL_free(crl);
  return (rv == -1);
}

static int add_hash( X509_LOOKUP *lookup, const char *dir) {
  int rv=0;
  rv = X509_LOOKUP_add_dir(lookup,dir, X509_FILETYPE_PEM);
  if (rv != 1) { /* load all hash links in PEM format */
    set_error("X509_LOOKUP_add_dir(PEM) failed: %s", ERR_error_string(ERR_get_error(), NULL));
    return -1;
  }
  rv = X509_LOOKUP_add_dir(lookup, dir, X509_FILETYPE_ASN1);
  if (rv != 1) { /* load all hash links in ASN1 format */
    set_error("X509_LOOKUP_add_dir(ASN1) failed: %s", ERR_error_string(ERR_get_error(), NULL));
    return -1;
  }
  return 1;
}

static int add_file( X509_LOOKUP *lookup, const char *file) {
  int rv=0;
  rv = X509_LOOKUP_load_file(lookup,file, X509_FILETYPE_PEM);
  if (rv == 1) return 1;
  WLog_DBG(TAG, "File format is not PEM: trying ASN1");
  rv = X509_LOOKUP_load_file(lookup,file, X509_FILETYPE_ASN1);
  if(rv!=1) {
    set_error("X509_LOOKUP_load_file(ASN1) failed: %s", ERR_error_string(ERR_get_error(), NULL));
    return -1; /* neither PEM nor ASN1 format: return error */
  }
  return 1;
}

static X509_STORE * setup_store(cert_policy *policy) {
  int rv;
  X509_STORE *store = NULL;
  X509_LOOKUP *lookup = NULL;

  /* setup the x509 store to verify the certificate */
  store = X509_STORE_new();
  if (store == NULL) {
    set_error("X509_STORE_new() failed: %s", ERR_error_string(ERR_get_error(), NULL));
    return NULL;
  }

  /* if needed add hash_dir lookup methods */
  if ( (is_dir(policy->ca_dir)>0) || (is_dir(policy->crl_dir)>0) ) {
    WLog_DBG(TAG, "Adding hashdir lookup to x509_store");
    lookup = X509_STORE_add_lookup(store,X509_LOOKUP_hash_dir());
    if (!lookup) {
      X509_STORE_free(store);
      set_error("X509_STORE_add_lookup(hash_dir) failed: %s", ERR_error_string(ERR_get_error(), NULL));
      return NULL;
    }
  }
  /* add needed hash dir pathname entries */
  if ( (policy->ca_policy) && (is_dir(policy->ca_dir)>0) ) {
    const char *pt=policy->ca_dir;
    if ( strstr(pt,"file:///")) pt+=8; /* strip url if needed */
    WLog_DBG(TAG, "Adding hash dir '%s' to CACERT checks",policy->ca_dir);
    rv = add_hash( lookup, pt);
    if (rv<0) goto add_store_error;
  }
  if ( (policy->crl_policy!=CRLP_NONE) && (is_dir(policy->crl_dir)>0 ) ) {
    const char *pt=policy->crl_dir;
    if ( strstr(pt,"file:///")) pt+=8; /* strip url if needed */
    WLog_DBG(TAG, "Adding hash dir '%s' to CRL checks",policy->crl_dir);
    rv = add_hash( lookup, pt);
    if (rv<0) goto add_store_error;
  }

  /* if needed add file lookup methods */
  if ( (is_file(policy->ca_dir)>0) || (is_file(policy->crl_dir)>0) ) {
    WLog_DBG(TAG, "Adding file lookup to x509_store");
    lookup = X509_STORE_add_lookup(store,X509_LOOKUP_file());
    if (!lookup) {
      X509_STORE_free(store);
      set_error("X509_STORE_add_lookup(file) failed: %s", ERR_error_string(ERR_get_error(), NULL));
      return NULL;
    }
  }
  /* and add file entries to lookup */
  if ( (policy->ca_policy) && (is_file(policy->ca_dir)>0) ) {
    const char *pt=policy->ca_dir;
    if ( strstr(pt,"file:///")) pt+=8; /* strip url if needed */
    WLog_DBG(TAG, "Adding file '%s' to CACERT checks",policy->ca_dir);
    rv = add_file(lookup, pt);
    if (rv<0) goto add_store_error;
  }
  if ( (policy->crl_policy!=CRLP_NONE) && (is_file(policy->crl_dir)>0 ) ) {
    const char *pt=policy->crl_dir;
    if ( strstr(pt,"file:///")) pt+=8; /* strip url if needed */
    WLog_DBG(TAG, "Adding file '%s' to CRL checks",policy->crl_dir);
    rv = add_file(lookup, pt);
    if (rv<0) goto add_store_error;
  }
  return store;

add_store_error:
  WLog_DBG(TAG, "setup_store() error: '%s'",get_error());
  X509_LOOKUP_free(lookup);
  X509_STORE_free(store);
  return NULL;
}

/*
* @return -1 on error, 0 on verify failed, 1 on verify sucess
*/
int verify_certificate(X509 * x509, cert_policy *policy)
{
  int rv;
  X509_STORE *store;
  X509_STORE_CTX *ctx;

  /* if neither ca nor crl check are requested skip */
  if ( (policy->ca_policy==0) && (policy->crl_policy==CRLP_NONE) ) {
	WLog_DBG(TAG, "Neither CA nor CRL check requested. CertVrfy() skipped");
	return 1;
  }

  /* setup the x509 store to verify the certificate */
  store = setup_store(policy);
  if (store == NULL) {
    set_error("setup_store() failed: %s", ERR_error_string(ERR_get_error(), NULL));
    return -1;
  }

  ctx = X509_STORE_CTX_new();
  if (ctx == NULL) {
    X509_STORE_free(store);
    set_error("X509_STORE_CTX_new() failed: %s", ERR_error_string(ERR_get_error(), NULL));
    return -1;
  }
  X509_STORE_CTX_init(ctx, store, x509, NULL);
#if 0
  X509_STORE_CTX_set_purpose(ctx, purpose);
#endif
  if (policy->ca_policy) {
  rv = X509_verify_cert(ctx);
  if (rv != 1) {
    X509_STORE_CTX_free(ctx);
    X509_STORE_free(store);
    set_error("certificate is invalid: %s", X509_verify_cert_error_string(ctx->error));
		switch (ctx->error) {
			case X509_V_ERR_CERT_HAS_EXPIRED:
				rv = -2;
				break;
			case X509_V_ERR_CERT_NOT_YET_VALID:
				rv = -3;
				break;
			case X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY:
				rv = -4;
				break;
			default:
				rv = 0;
				break;
		}
		return rv;
  } else {
    WLog_DBG(TAG, "certificate is valid");
  }
  }

  /* verify whether the certificate was revoked or not */
  rv = check_for_revocation(x509, ctx, policy->crl_policy);
  X509_STORE_CTX_free(ctx);
  X509_STORE_free(store);
  if (rv < 0) {
    set_error("check_for_revocation() failed: %s", get_error());
    return -1;
  } else if (rv == 0) {
    WLog_DBG(TAG, "certificate has been revoked");
  } else {
    WLog_DBG(TAG, "certificate has not been revoked");
  }
  return rv;
}

int verify_signature(X509 * x509, unsigned char *data, int data_length,
                     unsigned char *signature, int signature_length)
{
  int rv;
  EVP_PKEY *pubkey;
  EVP_MD_CTX md_ctx;

  /* get the public-key */
  pubkey = X509_get_pubkey(x509);
  if (pubkey == NULL) {
    set_error("X509_get_pubkey() failed: %s", ERR_error_string(ERR_get_error(), NULL));
    return -1;
  }
  /* verify the signature */
  EVP_VerifyInit(&md_ctx, EVP_sha1());
  EVP_VerifyUpdate(&md_ctx, data, data_length);
  rv = EVP_VerifyFinal(&md_ctx, signature, signature_length, pubkey);
  EVP_PKEY_free(pubkey);
  if (rv != 1) {
    set_error("EVP_VerifyFinal() failed: %s", ERR_error_string(ERR_get_error(), NULL));
    return -1;
  }
  WLog_DBG(TAG, "signature is valid");
  return 0;
}
#endif
