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

#ifndef __CERT_INFO_C_
#define __CERT_INFO_C_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "error.h"
#include "strings.h"
#include "cert_info.h"
#include "alg_st.h"

#include <freerdp/log.h>
#define TAG FREERDP_TAG("pkcs11.cert_info")

#ifdef HAVE_NSS

#include "secoid.h"

/*
 * NSS dynamic oid support.
 *  NSS is able to understand new oid tags provided by the application,
 *  including
 *  understanding new cert extensions that NSS previously did not understand.
 *  This code adds the oids for the Kerberos Principle and the Microsoft UPN
 */
#define TO_ITEM(x) {siDEROID, (unsigned char *)(x), sizeof(x) }

/* kerberois oid: 1.3.6.1.5.2.2 */
SECOidTag CERT_KerberosPN_OID = SEC_OID_UNKNOWN;
static const unsigned char kerberosOID[] =  { 0x2b, 0x6, 0x1, 0x5, 0x2, 0x2 };
static const SECOidData kerberosPN_Entry =
       { TO_ITEM(kerberosOID), SEC_OID_UNKNOWN,
       "Kerberos Priniciple", CKM_INVALID_MECHANISM, INVALID_CERT_EXTENSION };

SECOidTag CERT_MicrosoftUPN_OID = SEC_OID_UNKNOWN;
/* { 1.3.6.1.4.1.311 } */
static const unsigned char microsoftUPNOID[] =
        { 0x2b, 0x6, 0x1, 0x4, 0x1, 0x82, 0x37, 0x14, 0x2, 0x3 };
static const SECOidData microsoftUPN_Entry =
        { TO_ITEM(microsoftUPNOID), SEC_OID_UNKNOWN,
        "Microsoft Universal Priniciple", CKM_INVALID_MECHANISM,
        INVALID_CERT_EXTENSION };

/* register the oid if we haven't already */
static void
cert_fetchOID(SECOidTag *data, const SECOidData *src)
{
  if (*data == SEC_OID_UNKNOWN) {
    /* AddEntry does the right thing if someone else has already
     * added the oid. (that is return that oid tag) */
    *data = SECOID_AddEntry(src);
  }
  return;
}

static char **
cert_GetNameElements(CERTName *name, int wantedTag)
{
  static char *results[CERT_INFO_SIZE];
  CERTRDN** rdns;
  CERTRDN *rdn;
  char *buf = 0;
  int i=0;

  rdns = name->rdns;
  while (rdns && (rdn = *rdns++) != 0) {
    CERTAVA** avas = rdn->avas;
    CERTAVA*  ava;
    while (avas && (ava = *avas++) != 0) {
      int tag = CERT_GetAVATag(ava);
      if ( tag == wantedTag ) {
        SECItem *decodeItem = CERT_DecodeAVAValue(&ava->value);
        if(!decodeItem) {
          results[i] = NULL;
          return results[0] ? results : NULL;
        }
        buf = malloc(decodeItem->len + 1);
        if ( buf ) {
          memcpy(buf, decodeItem->data, decodeItem->len);
          buf[decodeItem->len] = 0;
        }
        SECITEM_FreeItem(decodeItem, PR_TRUE);
        results[i] = buf;
        i++;
        if (i == CERT_INFO_SIZE-1) {
          goto done;
        }
      }
    }
  }
done:
  results[i] = NULL;
  return results[0] ? results : NULL;
}

/*
* Evaluate Certificate Signature Digest
*/
static char **cert_info_digest(X509 *x509, ALGORITHM_TYPE algorithm) {
  static char *entries[2] = { NULL,NULL };
  HASH_HashType  type = HASH_GetHashTypeByOidTag(algorithm);
  unsigned char data[HASH_LENGTH_MAX];

  if (type == HASH_AlgNULL) {
    type = HASH_AlgSHA1;
    WLog_DBG(TAG, "Invalid digest algorithm, using 'sha1'",algorithm);
  }
  HASH_HashBuf(type, data, x509->derCert.data, x509->derCert.len);
  entries[0] = bin2hex(data,HASH_ResultLen(type));
  return entries;
}

static char **
cert_info_upn (X509 *x509)
{
    SECItem alt_name;
    SECStatus status;
    PRArenaPool *arena = NULL;
    CERTGeneralName *nameList;
    CERTGeneralName *current;
    SECOidTag tag;
    static char *results[CERT_INFO_SIZE] = { NULL };
    int result = 0;
    SECItem decoded;

    WLog_DBG(TAG, "Looking for ALT_NAME");

    status = CERT_FindCertExtension(x509, SEC_OID_X509_SUBJECT_ALT_NAME, &alt_name);
    if (status != SECSuccess) {
        WLog_DBG(TAG, "Not found");
        goto no_upn;
    }

    arena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
    if (!arena) {
        WLog_DBG(TAG, "Could not allocate arena");
        goto no_upn;
    }

    nameList = current = CERT_DecodeAltNameExtension(arena, &alt_name);
    if (!nameList) {
        WLog_DBG(TAG, "Could not decode name");
        goto no_upn;
    }

    cert_fetchOID(&CERT_MicrosoftUPN_OID, &microsoftUPN_Entry);
    do {
        if (current->type == certOtherName) {
            tag = SECOID_FindOIDTag(&current->name.OthName.oid);
            WLog_DBG(TAG, "got other name with tag %#x", tag);
            if (tag == CERT_MicrosoftUPN_OID) {
				status = SEC_ASN1DecodeItem(arena, &decoded,
					SEC_UTF8StringTemplate, &current->name.OthName.name);
                if (status == SECSuccess) {
                    results[result] = malloc(decoded.len + 1);
                    memcpy(results[result], decoded.data, decoded.len);
                    results[result][decoded.len] = '\0';
                    WLog_DBG(TAG, "Got upn: %s", results[result]);
                    result++;
                } else {
                    WLog_DBG(TAG, "Could not decode upn...");
                }
            }
        } else {
            WLog_DBG(TAG, "not other name...");
        }
        current = CERT_GetNextGeneralName(current);
    } while (current != nameList && result < CERT_INFO_MAX_ENTRIES);

no_upn:
    if (arena) {
        PORT_FreeArena(arena, PR_FALSE);
    }

    if (alt_name.data) {
        SECITEM_FreeItem(&alt_name, PR_FALSE);
    }

    return results;
}

/**
* request info on certificate
* @param x509 	Certificate to parse
* @param type 	Information to retrieve
* @param algorithm Digest algoritm to use
* @return utf-8 string array with provided information
*/
char **cert_info(X509 *x509, int type, ALGORITHM_TYPE algorithm ) {
  static char *results[CERT_INFO_SIZE];
  SECOidData *oid;
  int i;

  if (!x509) {
    WLog_DBG(TAG, "Null certificate provided");
    return NULL;
  }
  switch (type) {
    case CERT_CN      : /* Certificate Common Name */
      return cert_GetNameElements(&x509->subject, SEC_OID_AVA_COMMON_NAME);
    case CERT_SUBJECT : /* Certificate subject */
      results[0] = CERT_NameToAscii(&x509->subject);
      results[1] = 0;
      break;
    case CERT_ISSUER : /* Certificate issuer */
      results[0] = CERT_NameToAscii(&x509->issuer);
      results[1] = 0;
      break;
    case CERT_SERIAL : /* Certificate serial number */
      results[0] = bin2hex(x509->serialNumber.data, x509->serialNumber.len);
      results[1] = 0;
      break;
    case CERT_KPN     : /* Kerberos principal name */
      cert_fetchOID(&CERT_KerberosPN_OID, &kerberosPN_Entry);
      return cert_GetNameElements(&x509->subject, CERT_KerberosPN_OID);
    case CERT_EMAIL   : /* Certificate e-mail */
      for (i=1, results[0] = CERT_GetFirstEmailAddress(x509);
        results[i-1] && i < CERT_INFO_SIZE; i++) {
        results[i] = CERT_GetNextEmailAddress(x509, results[i-1]);
      }
      results[i] = NULL;
      for (i=0; results[i]; i++) {
        results[i] = strdup(results[i]);
      }
      break;
    /* need oid tag. */
    case CERT_UPN     : /* Microsoft's Universal Principal Name */
      return cert_info_upn(x509);
    case CERT_UID     : /* Certificate Unique Identifier */
      return cert_GetNameElements(&x509->subject, SEC_OID_RFC1274_UID);
      break;
    case CERT_PUK     : /* Certificate Public Key */
      return NULL;
    case CERT_DIGEST  : /* Certificate Signature Digest */
      if ( !algorithm ) {
        WLog_DBG(TAG, "Must specify digest algorithm");
        return NULL;
      }
      return cert_info_digest(x509,algorithm);
    case CERT_KEY_ALG     :
      oid = SECOID_FindOID(&x509->subjectPublicKeyInfo.algorithm.algorithm);
      if (oid == NULL) {
        results[0] = strdup("Unknown");
      } else {
        results[0] = strdup(oid->desc);
      }
      results[1] = 0;
      break;
    default           :
      WLog_DBG(TAG, "Invalid info type requested: %d",type);
      return NULL;
    }
  if (results[0] == NULL) {
    return NULL;
  }
  return results;
}
#else
#include <openssl/evp.h>
#include <openssl/objects.h>
#include <openssl/err.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/opensslv.h>
#include <openssl/pem.h>
#include <openssl/bn.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "error.h"
#include "strings.h"
#include "base64.h"
#include "cert_info.h"

#if OPENSSL_VERSION_NUMBER >=  0x00907000L
#define UID_TYPE NID_x500UniqueIdentifier
#else
#define UID_TYPE NID_uniqueIdentifier
#endif

/**
* Generate and compose a certificate chain
*/
void add_cert(X509 *cert, X509 ***certs, int *ncerts) {
        X509 **certs2;
        /* sanity checks */
        if (!cert) return;
        if (!certs) return;
        if (!ncerts) return;

        /* no certs so far */
        if (!*certs) {
                *certs = malloc(sizeof(void *));
                if (!*certs) return;
                *certs[0] = cert;
                *ncerts = 1;
                return;
        }

        /* enlarge current cert chain by malloc(new)+copy()+free(old) */
        certs2 = malloc(sizeof(void *) * ((*ncerts) + 1));
        if (!certs2) return;
        memcpy(certs2, *certs, sizeof(void *) * (*ncerts));
        certs2[*ncerts] = cert;
        free(*certs);
        *certs = certs2;
        (*ncerts)++;
}

/*
* Extract Certificate's Common Name
*/
static char **cert_info_cn(X509 *x509) {
	static char *results[CERT_INFO_SIZE];
	int lastpos,position;
    X509_NAME *name = X509_get_subject_name(x509);
    if (!name) {
      	WLog_ERR(TAG, "Certificate has no subject");
		return NULL;
	}
	for (position=0;position<CERT_INFO_SIZE;position++) results[position]= NULL;
	position=0;
	lastpos = X509_NAME_get_index_by_NID(name,NID_commonName,-1);
	if (lastpos == -1) {
		WLog_ERR(TAG, "Certificate has no UniqueID");
		return NULL;
	}
	while( ( lastpos != -1 ) && (position<CERT_INFO_MAX_ENTRIES) ) {
	    X509_NAME_ENTRY *entry;
	    ASN1_STRING *str;
	    unsigned char *txt;
	    if ( !(entry = X509_NAME_get_entry(name,lastpos)) ) {
	    	WLog_ERR(TAG, "X509_get_name_entry() failed: %s", ERR_error_string(ERR_get_error(),NULL));
            return results;
        }
	    if ( !(str = X509_NAME_ENTRY_get_data(entry)) ) {
	    	WLog_ERR(TAG, "X509_NAME_ENTRY_get_data() failed: %s", ERR_error_string(ERR_get_error(),NULL));
            return results;
        }
        if ( ( ASN1_STRING_to_UTF8(&txt, str) ) < 0) {
        	WLog_ERR(TAG, "ASN1_STRING_to_UTF8() failed: %s", ERR_error_string(ERR_get_error(),NULL));
            return results;
        }
        WLog_DBG(TAG, "%s = [%s]", OBJ_nid2sn(NID_commonName), txt);
        results[position++]=clone_str((const char *)txt);
        OPENSSL_free(txt);
        lastpos = X509_NAME_get_index_by_NID(name, NID_commonName, lastpos);
	}

	/* no more UID's availables in certificate */
	return results;
}

/*
* Extract Certificate's Subject
*/
static char **cert_info_subject(X509 *x509) {
	X509_NAME *subject;
	static char *entries[2] = { NULL, NULL };
	entries[0] = malloc(256);
	if (!entries[0]) return NULL;
        subject = X509_get_subject_name(x509);
        if (!subject) {
        	WLog_ERR(TAG, "X509_get_subject_name failed");
            return NULL;
        }
        X509_NAME_oneline(subject, entries[0], 256 );
	return entries;
}

/*
* Extract Certificate's Issuer
*/
static char **cert_info_issuer(X509 *x509) {
	X509_NAME *issuer;
	static char *entries[2] = { NULL, NULL };
	entries[0] = malloc(256);
	if (!entries[0]) return NULL;
        issuer = X509_get_issuer_name(x509);
        if (!issuer) {
        	WLog_ERR(TAG, "X509_get_issuer_name failed");
            return NULL;
        }
        X509_NAME_oneline(issuer, entries[0], 256 );
	return entries;
}

/*
* Extract Certificate's Kerberos Principal Name
*/
static char **cert_info_kpn(X509 *x509) {
        int i,j;
	static char *entries[CERT_INFO_SIZE];
        STACK_OF(GENERAL_NAME) *gens;
        GENERAL_NAME *name;
        ASN1_OBJECT *krb5PrincipalName;
        WLog_DBG(TAG, "Trying to find a Kerberos Principal Name in certificate");
        gens = X509_get_ext_d2i(x509, NID_subject_alt_name, NULL, NULL);
        krb5PrincipalName = OBJ_txt2obj("1.3.6.1.5.2.2", 1);
        if (!gens) {
        		WLog_ERR(TAG, "No alternate name extensions");
                return NULL; /* no alternate names */
        }
        if (!krb5PrincipalName) {
        		WLog_ERR(TAG, "Cannot map KPN object");
                return NULL;
        }
	for (j=0;j<CERT_INFO_SIZE;j++) entries[j] = NULL;
        for (i=0,j=0; (i < sk_GENERAL_NAME_num(gens)) && (j<CERT_INFO_MAX_ENTRIES); i++) {
        	name = sk_GENERAL_NAME_value(gens, i);
            if ( name && name->type==GEN_OTHERNAME ) {  /* test for UPN */
            	WLog_ERR(TAG, "GEN_OTHERNAME");
                if (OBJ_cmp(name->d.otherName->type_id, krb5PrincipalName)) {
                	WLog_ERR(TAG, "krb5PrincipalName");
                	continue; /* object is not a UPN */
                }
		else {
		    /* NOTE:
		    from PKINIT RFC, I deduce that stored format for kerberos
		    Principal Name is ASN1_STRING, but not sure at 100%
		    Any help will be granted
		    */
		    unsigned char *txt;
		    ASN1_TYPE *val = name->d.otherName->value;
		    ASN1_STRING *str= val->value.asn1_string;
		    WLog_DBG(TAG, "Found Kerberos Principal Name ");
		    if ( ( ASN1_STRING_to_UTF8(&txt, str) ) < 0) {
		    		WLog_ERR(TAG, "ASN1_STRING_to_UTF8() failed: %s", ERR_error_string(ERR_get_error(),NULL));
                    } else {
                    	WLog_ERR(TAG, "Adding KPN entry: %s",txt);
		        entries[j++]= clone_str((const char *)txt);
		    }
		}
            }
        }
        sk_GENERAL_NAME_pop_free(gens, GENERAL_NAME_free);
        ASN1_OBJECT_free(krb5PrincipalName);
	if(j==0) {
		WLog_ERR(TAG, "Certificate does not contain a KPN entry");
	    return NULL;
	}

	WLog_ERR(TAG, "end of cert_info_kpn\n");

	return entries;
}

/*
* Extract Certificate's email
*/
static char **cert_info_email(X509 *x509) {
        int i,j;
	static char *entries[CERT_INFO_SIZE];
	STACK_OF(GENERAL_NAME) *gens;
        GENERAL_NAME *name;
        WLog_DBG(TAG, "Trying to find an email in certificate");
        gens = X509_get_ext_d2i(x509, NID_subject_alt_name, NULL, NULL);
        if (!gens) {
                WLog_DBG(TAG, "No alternate name(s) in certificate");
                return 0; /* no alternate names */
        }
        for (i=0,j=0; (i < sk_GENERAL_NAME_num(gens)) && (j<CERT_INFO_MAX_ENTRIES); i++) {
            name = sk_GENERAL_NAME_value(gens, i);
            if ( name && name->type==GEN_EMAIL ) {
                WLog_DBG(TAG, "Found E-Mail Entry = '%s'", name->d.ia5->data);
		entries[j++]=clone_str((const char *)name->d.ia5->data);
            }
        }
        sk_GENERAL_NAME_pop_free(gens, GENERAL_NAME_free);
	if(j==0) {
            WLog_DBG(TAG, "Certificate does not contain a Email entry");
	    return NULL;
	}
	return entries;
}

/*
* Extract Certificate's Microsoft Universal Principal Name
*/
static char **cert_info_upn(X509 *x509) {
	int i,j;
	static char *entries[CERT_INFO_SIZE];
    STACK_OF(GENERAL_NAME) *gens;
    GENERAL_NAME *name;
    WLog_DBG(TAG, "Trying to find an Universal Principal Name in certificate");
    gens = X509_get_ext_d2i(x509, NID_subject_alt_name, NULL, NULL);
    if (!gens) {
    	WLog_ERR(TAG, "No alternate name extensions found");
	    return NULL;
	}
	for (j=0;j<CERT_INFO_SIZE;j++) entries[j] = NULL;
        for (i=0,j=0; (i < sk_GENERAL_NAME_num(gens)) && (j<CERT_INFO_MAX_ENTRIES); i++) {
            name = sk_GENERAL_NAME_value(gens, i);
            if ( name && name->type==GEN_OTHERNAME ) {
                /* test for UPN */
                if (OBJ_cmp(name->d.otherName->type_id, OBJ_nid2obj(NID_ms_upn))) continue; /* object is not a UPN */
                WLog_DBG(TAG, "Found MS Universal Principal Name ");
                /* try to extract string and return it */
                if (name->d.otherName->value->type == V_ASN1_UTF8STRING) {
                    ASN1_UTF8STRING *str = name->d.otherName->value->value.utf8string;
                    WLog_DBG(TAG, "Adding UPN NAME entry= %s",str->data);
                    entries[j++] = clone_str((const char *)str->data);
                } else {
                	WLog_ERR(TAG, "Found UPN entry is not an utf8string");
                }
            }
        }
        sk_GENERAL_NAME_pop_free(gens, GENERAL_NAME_free);
	if(j==0) {
		WLog_ERR(TAG, "Certificate does not contain a MS UPN entry");
	    return NULL;
	}
	return entries;
}

/*
* Extract Certificate's Unique Identifier(s)
* Array size is limited to CERT_INFO_MAX_ENTRIES UID's. expected to be enought...
*/
static char **cert_info_uid(X509 *x509) {
	static char *results[CERT_INFO_SIZE];
	int lastpos,position;
	int uid_type = UID_TYPE;
        X509_NAME *name = X509_get_subject_name(x509);
        if (!name) {
		WLog_DBG(TAG, "Certificate has no subject");
		return NULL;
	}
	for (position=0;position<CERT_INFO_SIZE;position++) results[position]= NULL;
	position=0;
	lastpos = X509_NAME_get_index_by_NID(name,uid_type,-1);
	if (lastpos == -1) {
		uid_type = NID_userId;
		lastpos = X509_NAME_get_index_by_NID(name,uid_type,-1);
		if (lastpos == -1) {
			WLog_DBG(TAG, "Certificate has no UniqueID");
			return NULL;
		}
	}
	while( ( lastpos != -1 ) && (position<CERT_INFO_MAX_ENTRIES) ) {
	    X509_NAME_ENTRY *entry;
	    ASN1_STRING *str;
	    unsigned char *txt;
	    if ( !(entry = X509_NAME_get_entry(name,lastpos)) ) {
                WLog_DBG(TAG, "X509_get_name_entry() failed: %s", ERR_error_string(ERR_get_error(),NULL));
                return results;
            }
	    if ( !(str = X509_NAME_ENTRY_get_data(entry)) ) {
                WLog_DBG(TAG, "X509_NAME_ENTRY_get_data() failed: %s", ERR_error_string(ERR_get_error(),NULL));
                return results;
            }
            if ( ( ASN1_STRING_to_UTF8(&txt, str) ) < 0) {
                WLog_DBG(TAG, "ASN1_STRING_to_UTF8() failed: %s", ERR_error_string(ERR_get_error(),NULL));
                return results;
            }
	    WLog_DBG(TAG, "%s = [%s]", OBJ_nid2sn(UID_TYPE), txt);
	    results[position++]=clone_str((const char *)txt);
	    OPENSSL_free(txt);
	    lastpos = X509_NAME_get_index_by_NID(name, UID_TYPE, lastpos);
	}
	/* no more UID's availables in certificate */
	return results;
}

/* convert publickey into PEM format */
static char *key2pem(EVP_PKEY *key) {
	int len;
	char *pt,*res;
	BIO *buf= BIO_new(BIO_s_mem());
	if (!buf) {
	    WLog_DBG(TAG, "BIO_new() failed");
	    return NULL;
	}
	if ( ! PEM_write_bio_PUBKEY(buf,key) ) {
	    WLog_DBG(TAG, "Cannot print public key");
	    return NULL;
	}
	/* extract data */
	len= BIO_get_mem_data(buf,&pt);
	if( !(res=malloc(len+1)) ) {
	    WLog_DBG(TAG, "Cannot malloc() to copy public key");
	    return NULL;
	}
	memcpy(res,pt,len);
	*(res+len)='\0';
	/*BIO_set_close(buf,BIO_NOCLOSE); */
	BIO_free(buf);
	return res;
}

/*
* Extract Certificate's Public Key
*/
static char **cert_info_puk(X509 *x509) {
	char *pt;
	static char *entries[2] = { NULL,NULL };
	EVP_PKEY *pubk = X509_get_pubkey(x509);
	if(!pubk) {
	    WLog_DBG(TAG, "Cannot extract public key");
	    return NULL;
	}
	pt=key2pem(pubk);
	if (!pt) {
	    WLog_DBG(TAG, "key2pem() failed");
	    EVP_PKEY_free(pubk);
	    return NULL;
	}
	EVP_PKEY_free(pubk);
	WLog_DBG(TAG, "Public key is '%s'\n",pt);
	entries[0]=pt;
	return entries;
}

/* Store an integer into buffer */
static int int_append(unsigned char *pt, int n) {
	*pt++= (n&0xff000000) >>24;
	*pt++= (n&0x00ff0000) >>16;
	*pt++= (n&0x0000ff00) >>8;
	*pt  = (n&0x000000ff) >>0;
	return 4;
}

/* store an string into buffer */
static int str_append(unsigned char *pt, const char *str, int len) {
	memcpy(pt,str,len);
	return len;
}

/* store a bignum into a buffer */
static int BN_append(unsigned char *pt, BIGNUM *bn) {
	unsigned char *old=pt;
	int res=0;
	int extrabyte=0;
	int size= 1 + BN_num_bytes(bn);
	unsigned char *buff;
	if(BN_is_zero(bn)) {
		res= int_append(pt,0);
		return res;
	}
	buff=malloc(size);
	*buff=0x00;
	BN_bn2bin(bn,buff+1);
	/* TODO: handle error condition */
	extrabyte=( buff[1] & 0x80 )? 0:1;
	res= int_append(pt,size-extrabyte); pt+=res;
	res= str_append(pt,(char *)(buff+extrabyte),size-extrabyte); pt+=res;
	free(buff);
	return pt-old;
}

/*
* Extract Certificate's Public Key in OpenSSH format
*/
static char **cert_info_sshpuk(X509 *x509) {
	char **maillist;
	const char *type;
	char *buf;
	unsigned char *blob,*pt,*data = NULL;
	int data_len;
	int res;
	static char *entries[2] = { NULL,NULL };
	EVP_PKEY *pubk = X509_get_pubkey(x509);
	if(!pubk) {
	    WLog_DBG(TAG, "Cannot extract public key");
	    return NULL;
	}
	blob=calloc(8192,sizeof(unsigned char));
	if (!blob ) {
	    WLog_DBG(TAG, "Cannot allocate space to compose pkey string");
	    goto sshpuk_fail;
	}
	pt=blob;
	switch (pubk->type) {
		case EVP_PKEY_DSA:
			if (!pubk->pkey.dsa) {
				WLog_DBG(TAG, "No data for public DSA key");
				goto sshpuk_fail;
			}
			type="ssh-dss";
		        /* dump key into a byte array */
			res= int_append(pt,strlen(type)); pt+=res;
		        res= str_append(pt,type,strlen(type)); pt+=res;
                	res= BN_append(pt, pubk->pkey.dsa->p); pt+=res;
                	res= BN_append(pt, pubk->pkey.dsa->q); pt+=res;
                	res= BN_append(pt, pubk->pkey.dsa->g); pt+=res;
                	res= BN_append(pt, pubk->pkey.dsa->pub_key); pt+=res;
			break;
		case EVP_PKEY_RSA:
			if (!pubk->pkey.rsa) {
				WLog_DBG(TAG, "No data for public RSA key");
				goto sshpuk_fail;
			}
		        /* dump key into a byte array */
			type="ssh-rsa";
			res= int_append(pt,strlen(type)); pt+=res;
		        res= str_append(pt,type,strlen(type)); pt+=res;
                	res= BN_append(pt, pubk->pkey.rsa->e); pt+=res;
                	res= BN_append(pt, pubk->pkey.rsa->n); pt+=res;
			break;
		default: WLog_DBG(TAG, "Unknown public key type");
			goto sshpuk_fail;
	}
	/* encode data in base64 format */
	data_len= 1+ 4*((2+pt-blob)/3);
	/* data_len=8192; */
	data=calloc(data_len,sizeof(unsigned char));
	if(!data) {
		WLog_DBG(TAG, "calloc() to uuencode buffer '%d'",data_len);
		goto sshpuk_fail;
	}
	res= base64_encode(blob,pt-blob,data,(size_t *) &data_len);
	if (res<0) {
		WLog_DBG(TAG, "BASE64 Encode failed");
		goto sshpuk_fail;
	}
	/* retrieve email from certificate and compose ssh-key string */
	maillist= cert_info_email(x509);
	res=0;
	if (maillist && maillist[0]) res= strlen(maillist[0]);
	buf=malloc(3+res+strlen(type)+data_len);
	if (!buf) {
		WLog_DBG(TAG, "No memory to store public key dump");
		goto sshpuk_fail;
	}
	if (maillist && maillist[0]) sprintf(buf,"%s %s %s",type,data,maillist[0]);
	else sprintf(buf,"%s %s",type,data);
	WLog_DBG(TAG, "Public key is '%s'\n",buf);
	EVP_PKEY_free(pubk);
	free(blob);
	free(data);
	entries[0]=buf;
	return entries;

sshpuk_fail:
	EVP_PKEY_free(pubk);
	free(blob);
	if (data)
		free(data);
	return NULL;
}

static char* get_fingerprint(X509 *cert,const EVP_MD *type) {
    unsigned char    md[EVP_MAX_MD_SIZE];
    unsigned int     len;
    X509_digest(cert,type,md,&len);
    if (!len) {
	WLog_DBG(TAG, "X509_digest() failed");
	return NULL;
    }
    return bin2hex(md,len);
}

/*
* Evaluate Certificate Signature Digest
*/
static char **cert_info_digest(X509 *x509, const char *algorithm) {
	static char *entries[2] = { NULL,NULL };
	const EVP_MD *digest = EVP_get_digestbyname(algorithm);
        if(!digest) {
                digest= EVP_sha1();
                WLog_DBG(TAG, "Invalid digest algorithm %s, using 'sha1'",algorithm);
        }
	entries[0]= get_fingerprint(x509,digest);
	return entries;
}

/*
* Return certificate in PEM format
*/
static char **cert_info_pem(X509 *x509) {
	int len;
	char *pt,*res;
	static char *entries[2] = { NULL,NULL };
	BIO *buf= BIO_new(BIO_s_mem());
	if (!buf) {
	    WLog_DBG(TAG, "BIO_new() failed");
	    return NULL;
	}
	if ( ! PEM_write_bio_X509(buf,x509) ) {
	    WLog_DBG(TAG, "Cannot print certificate");
	    return NULL;
	}
	/* extract data */
	len= BIO_get_mem_data(buf,&pt);
	if ( ! (res= malloc(len+1) ) ) {
	    WLog_DBG(TAG, "Cannot malloc() to copy certificate");
	    return NULL;
	}
	memcpy(res,pt,len);
	*(res+len)='\0';
	/*BIO_set_close(buf,BIO_NOCLOSE); */
	BIO_free(buf);
	entries[0]=res;
	return entries;
}

/*
* Return certificate in PEM format
*/
static char **cert_key_alg(X509 *x509) {
	static char *entries[2] = { NULL,NULL };
	const char *alg = OBJ_nid2ln(
                    OBJ_obj2nid(x509->cert_info->key->algor->algorithm));
	entries[0]=strdup(alg);
	return entries;
}

/*
* Return certificate serial number as a hex string
*/
static char **cert_info_serial_number(X509 *x509) {
	static char *entries[2] = { NULL,NULL };
	ASN1_INTEGER *serial = X509_get_serialNumber(x509);
	int len;
	unsigned char *buffer = NULL, *tmp_ptr;

	len = i2c_ASN1_INTEGER(serial, NULL);

	if (len < 0) {
		return NULL;
	}
	buffer = malloc(len);
	if (buffer == NULL) {
		return NULL;
	}

	/* i2c_ASN1_INTEGER "kindly" increments our pointer by len,
	 * give it a temp ptr it can tweak to it's hearts content */
	tmp_ptr = buffer;
	len = i2c_ASN1_INTEGER(serial, &tmp_ptr);
	entries[0] = bin2hex(buffer,len);
	free(buffer);
	return entries;
}

/**
* request info on certificate
* @param x509 	Certificate to parse
* @param type 	Information to retrieve
* @param algorithm Digest algoritm to use
* @return utf-8 string array with provided information
*/
char **cert_info(X509 *x509, int type, const char *algorithm ) {
	if (!x509) {
		printf("cert_info : Null certificate provided\n");
		return NULL;
	}
	switch (type) {
	    case CERT_CN      : /* Certificate Common Name */
		return cert_info_cn(x509);
	    case CERT_SUBJECT : /* Certificate subject */
		return cert_info_subject(x509);
	    case CERT_ISSUER : /* Certificate issuer */
		return cert_info_issuer(x509);
	    case CERT_SERIAL : /* Certificate serial number */
		/* fix me */
		return cert_info_serial_number(x509);
	    case CERT_KPN     : /* Kerberos principal name */
		return cert_info_kpn(x509);
	    case CERT_EMAIL   : /* Certificate e-mail */
		return cert_info_email(x509);
	    case CERT_UPN     : /* Microsoft's Universal Principal Name */
		return cert_info_upn(x509);
	    case CERT_UID     : /* Certificate Unique Identifier */
		return cert_info_uid(x509);
	    case CERT_PUK     : /* Certificate Public Key */
		return cert_info_puk(x509);
	    case CERT_SSHPUK  : /* Certificate Public Key in OpenSSH format */
		return cert_info_sshpuk(x509);
	    case CERT_PEM  : /* Certificate in PEM format */
		return cert_info_pem(x509);
	    case CERT_DIGEST  : /* Certificate Signature Digest */
		if ( !algorithm ) {
		    WLog_DBG(TAG, "Must specify digest algorithm");
		    return NULL;
		}
		return cert_info_digest(x509,algorithm);
	    case CERT_KEY_ALG     : /* certificate signature algorithm */
		return cert_key_alg(x509);
	    default           :
		WLog_DBG(TAG, "Invalid info type requested: %d",type);
		return NULL;
	}
	/* should not get here */
	return NULL;
}
#endif /* HAVE_NSS */
#endif /* _CERT_INFO_C */
