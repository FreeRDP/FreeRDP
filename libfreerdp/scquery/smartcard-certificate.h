#ifndef LIBFREERDP_SCQUERY_SMARTCARD_CERTIFICATE_H
#define LIBFREERDP_SCQUERY_SMARTCARD_CERTIFICATE_H
#include "certificate-list.h"

/* find_x509_certificates_with_signing_rsa_private_key
returns a list of certificates that can be used with PKINIT.
This list shall be freed with  certificate_list_deepfree
verbose: when non-0 will print logs on stderr.
*/
certificate_list find_x509_certificates_with_signing_rsa_private_key(const char*
        pkcs11_library_path, int verbose);


#endif
