#ifndef LIBFREERDP_SCQUERY_H
#define LIBFREERDP_SCQUERY_H

#include "certificate.h"

typedef struct
{
	smartcard_certificate  certificate;
	char*  X509_user_identity; /* kinit -X X509_user_identity value */
	char*  upn;
} scquery_result_t, *scquery_result;

/**
* Find a user identity and UPN on a smartcard.
*
* @param module path to a pkcs11 shared library (eg. "/usr/lib/opensc-pkcs11.so" or "/usr/lib/libiaspkcs11.so")
* @param reader_name NULL or a smartcard reader name.
* @param card_name NULL or a smartcard name.
* @param verbose non-0 to add some logs.
* @return A structure containign the X509_user_identity parameter for kinit, and the upn from the selected certificate from the smartcard. The result shall be freed with  scquery_result_free().
*/
scquery_result scquery_X509_user_identities(const char* module, const char* reader_name,
        const char* card_name, int verbose);

scquery_result scquery_result_new(smartcard_certificate  certificate,
                                  char*  X509_user_identity,
                                  char*  upn);

void scquery_result_free(scquery_result that);

#endif


