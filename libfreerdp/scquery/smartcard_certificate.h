#ifndef LIBFREERDP_SCQUERY_SMARTCARD_CERTIFICATE_H
#define LIBFREERDP_SCQUERY_SMARTCARD_CERTIFICATE_H
#include "certificate_list.h"

/* find_x509_certificates_with_signing_rsa_private_key

When given, reader_name and / or card_name select secific slots (reader) or token (card).

@result a list of certificates that can be used with PKINIT.
This list shall be freed with  certificate_list_deepfree
@param verbose when non-0 will print logs on stderr.
@param reader_name NULL, or a nul-terminated string matching the slot_info.slot_description (with
the padding removed).
@param card_name NULL, or a nul-terminated string matching the token_info.label or
token_info.serial_number (with the padding removed).
*/
certificate_list find_x509_certificates_with_signing_rsa_private_key(
    const char* pkcs11_library_path, const char* reader_name, const char* card_name, int verbose);

#endif
