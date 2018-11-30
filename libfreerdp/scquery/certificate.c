#include <stdlib.h>
#include "certificate.h"
#include "scquery_error.h"

/* ========================================================================== */
/* smartcard_certificate */

smartcard_certificate  scquery_certificate_allocate()
{
	smartcard_certificate certificate = checked_malloc(sizeof(*certificate));

	if (certificate)
	{
		certificate->slot_id = 0;
		certificate->token_label = NULL;
		certificate->id = NULL;
		certificate->label = NULL;
		certificate->type = 0;
		certificate->issuer = NULL;
		certificate->subject = NULL;
		certificate->value = NULL;
		certificate->key_type = 0;
	}

	return certificate;
}

smartcard_certificate  scquery_certificate_new(CK_SLOT_ID          slot_id,
                                      char*               token_label,
                                      char*               id,
                                      char*               label,
                                      CK_CERTIFICATE_TYPE type,
                                      buffer              issuer,
                                      buffer              subject,
                                      buffer              value,
                                      CK_KEY_TYPE         key_type)
{
	smartcard_certificate certificate =  scquery_certificate_allocate();

	if (certificate)
	{
		certificate->slot_id = slot_id;
		certificate->token_label = token_label;
		certificate->id = id;
		certificate->label = label;
		certificate->type = type;
		certificate->issuer = issuer;
		certificate->subject = subject;
		certificate->value = value;
		certificate->key_type = key_type;
	}

	return certificate;
}

void scquery_certificate_deepfree(smartcard_certificate certificate)
{
	if (certificate)
	{
		free(certificate->token_label);
		free(certificate->id);
		free(certificate->label);
		free(certificate->issuer);
		free(certificate->subject);
		free(certificate->value);
		scquery_certificate_free(certificate);
	}
}

void scquery_certificate_free(smartcard_certificate certificate)
{
	free(certificate);
}

/**** THE END ****/
