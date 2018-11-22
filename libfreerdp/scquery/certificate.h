#ifndef LIBFREERDP_SCQUERY_CERTIFICATE_H
#define LIBFREERDP_SCQUERY_CERTIFICATE_H
#include <stddef.h>
#include "buffer.h"

typedef struct
{
	CK_SLOT_ID          slot_id;
	char*               token_label;
	char*               id;
	char*               label;
	CK_CERTIFICATE_TYPE type;
	buffer              issuer;
	buffer              subject;
	buffer              value;
	CK_KEY_TYPE         key_type;
} smartcard_certificate_t, *smartcard_certificate;


/* scquery_certificate_deepfree
deepfrees smartcard_certificate structure and all its fields. */
void scquery_certificate_deepfree(smartcard_certificate certificate);


/* scquery_certificate_allocate
allocates an empty smartcard_certificate structure. */
smartcard_certificate scquery_certificate_allocate();

/* scquery_certificate_new
allocates and initialize a new smartcard_certificate */
smartcard_certificate scquery_certificate_new(CK_SLOT_ID          slot_id,
                                      char*               token_label,
                                      char*               id,
                                      char*               label,
                                      CK_CERTIFICATE_TYPE type,
                                      buffer              issuer,
                                      buffer              subject,
                                      buffer              value,
                                      CK_KEY_TYPE         key_type);

/* scquery_certificate_free
frees only the smartcard_certificate structure (not the fields). */
void scquery_certificate_free(smartcard_certificate certificate);

#endif
