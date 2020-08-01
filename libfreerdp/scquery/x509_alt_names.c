#include <string.h>
#include <openssl/bn.h>
#include <openssl/err.h>
#include <openssl/safestack.h>
#include <openssl/ssl.h>
#include <openssl/stack.h>
#include <openssl/x509v3.h>
#include "x509_alt_names.h"
#include "scquery_error.h"
#include "scquery_string.h"

static void string_list_free(unsigned count, char** components)
{
	unsigned i;

	for (i = 0; i < count; i++)
	{
		free(components[i]);
	}

	free(components);
}

alt_name alt_name_new_with_components(char* type, unsigned count, char** components)
{
	alt_name name = alt_name_new(type, count);

	if (name == NULL)
	{
		return NULL;
	}

	name->count = count;
	name->allocated = count;
	name->components = components;
	return name;
}

alt_name alt_name_new(char* type, unsigned allocated)
{
	alt_name result = checked_malloc(sizeof(*result));

	if (result == NULL)
	{
		return NULL;
	}

	result->type = check_memory(strdup(type), 1 + strlen(type));
	result->count = 0;
	result->allocated = allocated;
	result->components =
	    ((allocated == 0) ? NULL
	                      : checked_calloc(result->allocated, sizeof(result->components[0])));

	if ((result->type == NULL) || ((result->allocated > 0) && (result->components == NULL)))
	{
		free(result->type);
		string_list_free(result->allocated, result->components);
		free(result);
		return NULL;
	}

	return result;
}

void alt_name_add_component(alt_name name, char* component)
{
	if ((name == NULL) || (component == NULL))
	{
		return;
	}

	if (name->count >= name->allocated)
	{
		unsigned new_allocated = ((name->allocated == 0) ? 8 : 2 * name->allocated);
		char** new_components =
		    realloc(name->components, new_allocated * sizeof(name->components[0]));

		if (new_components == NULL)
		{
			ERROR(EX_OSERR, "Cannot add component '%s'", component);
			return;
		}

		name->components = new_components;
		name->allocated = new_allocated;
	}

	unsigned index = name->count++;
	name->components[index] = check_memory(strdup(component), 1 + strlen(component));
	return;
}

void alt_name_free(alt_name name)
{
	if (name == NULL)
	{
		return;
	}

	free(name->type);
	string_list_free(name->count, name->components);
	free(name);
}

alt_name_list alt_name_list_cons(alt_name name, alt_name_list rest)
{
	alt_name_list list = checked_malloc(sizeof(*list));

	if (list == NULL)
	{
		return NULL;
	}

	list->name = name;
	list->rest = rest;
	return list;
}

alt_name alt_name_list_first(alt_name_list list)
{
	return ((list == NULL) ? NULL : list->name);
}

alt_name_list alt_name_list_rest(alt_name_list list)
{
	return ((list == NULL) ? NULL : list->rest);
}

void alt_name_list_free(alt_name_list list)
{
	free(list);
}

void alt_name_list_deepfree(alt_name_list list)
{
	while (list != NULL)
	{
		alt_name_list rest = list->rest;
		alt_name_free(list->name);
		alt_name_list_free(list);
		list = rest;
	}
}

char* general_name_type_label(int general_name_type)
{
	static const char* labels[] = { "OTHERNAME", "EMAIL", "DNS",   "X400", "DIRNAME",
		                            "EDIPARTY",  "URI",   "IPADD", "RID" };

	if ((general_name_type < 0) || (general_name_type >= (int)(sizeof(labels) / sizeof(labels[0]))))
	{
		char* result = checked_malloc(64);

		if (result == NULL)
		{
			return NULL;
		}

		sprintf(result, "Unknown GENERAL_NAME type %d", general_name_type);
		return result;
	}

	return strdup(labels[general_name_type]);
}

void extract_asn1_string(GENERAL_NAME* name, alt_name alt_name)
{
	unsigned char* string = NULL;

	switch (name->type)
	{
		case GEN_URI:
		case GEN_DNS:
		case GEN_EMAIL:
			if (ASN1_STRING_to_UTF8(&string, name->d.ia5) < 0)
			{
				char* type = general_name_type_label(name->type);
				ERROR(EX_OSERR, "Error converting with ASN1_STRING_to_UTF8 a %s general name",
				      type);
				free(type);
				return;
			}

			/* alt_name_add_component makes a copy of the component: */
			alt_name_add_component(alt_name, (char*)string);
			OPENSSL_free(string);
	}
}

char* type_id_to_oid_string(ASN1_OBJECT* type_id)
{
	char small_buffer[1];
	int buffer_size = 1 + OBJ_obj2txt(small_buffer, 1, type_id, /*no_name=*/1);
	char* buffer = checked_malloc(buffer_size);

	if (buffer == NULL)
	{
		return NULL;
	}

	OBJ_obj2txt(buffer, buffer_size, type_id, /*no_name=*/1);
	return buffer;
}

char* asn1_string_to_string(ASN1_TYPE* value)
{
	unsigned char* utf8string = NULL;
	int result = ASN1_STRING_to_UTF8(&utf8string, value->value.asn1_string);

	if (result < 0)
	{
		return check_memory(strdup(""), 1);
	}

	char* string = check_memory(strdup((char*)utf8string), 1 + result);
	OPENSSL_free(utf8string);
	return string;
}

char* asn1_boolean_to_string(ASN1_TYPE* value)
{
	return check_memory(strdup(value->value.boolean ? "true" : "false"), 6);
}

typedef void (*collector_pr)(unsigned class, unsigned primitive, unsigned tag, unsigned char* data,
                             unsigned length, void* collect_data);

void collect_alt_name_component(unsigned class, unsigned primitive, unsigned tag,
                                unsigned char* data, unsigned length, void* collect_data)
{
	(void)class;
	(void)primitive;
	(void)tag;
	alt_name name = collect_data;
	/* Use an auto buffer if length is small enough, or else a dynamic buffer. */
	char buffer[4096];

	if (1 + length <= sizeof(buffer))
	{
		strncpy(buffer, (char*)data, length);
		buffer[length] = '\0';
		alt_name_add_component(name, buffer);
	}
	else
	{
		char* buffer = checked_malloc(1 + length);

		if (!buffer)
		{
			return;
		}

		strncpy(buffer, (char*)data, length);
		buffer[length] = '\0';
		alt_name_add_component(name, buffer);
		free(buffer);
	}
}

enum
{
	asn1_eoc = 0,
	asn1_boolean = 1,
	asn1_integer = 2,
	asn1_bit_string = 3,
	asn1_octet_string = 4,
	asn1_null = 5,
	asn1_object = 6,
	asn1_object_descriptor = 7,
	asn1_external = 8,
	asn1_real = 9,
	asn1_enumerated = 10,
	asn1_utf8string = 12,
	asn1_sequence = 16,
	asn1_set = 17,
	asn1_numericstring = 18,
	asn1_printablestring = 19,
	asn1_t61string = 20,
	asn1_teletexstring = asn1_t61string,
	asn1_videotexstring = 21,
	asn1_ia5string = 22,
	asn1_utctime = 23,
	asn1_generalizedtime = 24,
	asn1_graphicstring = 25,
	asn1_iso64string = 26,
	asn1_visiblestring = asn1_iso64string,
	asn1_generalstring = 27,
	asn1_universalstring = 28,
	asn1_bmpstring = 30,
};

unsigned decode_der_length(unsigned char* data, unsigned i, unsigned* length)
{
	unsigned len = 0;
	unsigned char b = data[i++];

	if (b < 128)
	{
		len = b;
	}
	else
	{
		unsigned char c = b & 0x7f;

		while (0 < c--)
		{
			len = (len << 8) | data[i++];
		}
	}

	(*length) = len;
	return i;
}

char* decode_integer(unsigned char* data, unsigned i, unsigned length)
{
	unsigned long long value = 0;

	if (length <= sizeof(value))
	{
		unsigned j;

		for (j = 0; 0 < length--; j += 8)
		{
			value |= (data[i++] << j);
		}
	}
	else
	{
		value = (~0);
	}

	char* buffer = checked_malloc(64);

	if (buffer)
	{
		snprintf(buffer, sizeof(buffer) - 1, "%llu", value);
	}

	return buffer;
}

unsigned decode_der_item_collect(unsigned char* data, unsigned i, unsigned length,
                                 collector_pr collect, void* collect_data)
{
	/* We decode a sequence item.
	   It can be a context-specific indexed element, or a plain element.
	   When given a context-specific index, we just collect it, and then go on collecting the
	   element itself. */
	/* decode tag */
	unsigned char tag = data[i] & 31;
	unsigned char class = (data[i] >> 6) & 0b11;
	unsigned char primitive = ((data[i] & 32) == 0);
	i++;
	/* decode length */
	unsigned len = 0;
	i = decode_der_length(data, i, &len);
	assert(1 + len <= length);

	/* decode elements */
	switch (class)
	{
		case 2: /*context specific*/
		{
			char index[3];
			sprintf(index, "%d", tag);
			collect(class, primitive, tag, (unsigned char*)index, strlen(index), collect_data);
			i = decode_der_item_collect(data, i, len, collect, collect_data);
			break;
		}

		default:
			switch (tag)
			{
				case asn1_eoc:
				{
					/* not processed yet */
					collect(class, primitive, tag, (unsigned char*)&"", 0, collect_data);
					i += len;
					break;
				}

				case asn1_boolean:
				{
					char buffer[8];
					strcpy(buffer, (data[i] ? "true" : "false"));
					collect(class, primitive, tag, (unsigned char*)buffer, strlen(buffer),
					        collect_data);
					i += len;
					break;
				}

				case asn1_integer:
				{
					char* value = decode_integer(data, i, len);
					collect(class, primitive, tag, (unsigned char*)value, strlen(value),
					        collect_data);
					free(value);
					i += len;
					break;
				}

				case asn1_bit_string:
				case asn1_octet_string:
				{
					/* not processed yet */
					collect(class, primitive, tag, (unsigned char*)&"", 0, collect_data);
					i += len;
					break;
				}

				case asn1_null:
				{
					char buffer[8];
					strcpy(buffer, "null");
					collect(class, primitive, tag, (unsigned char*)buffer, strlen(buffer),
					        collect_data);
					i += len;
					break;
				}

				case asn1_set:
				case asn1_sequence:
				{
					unsigned e = i + len;

					while (i < e)
					{
						i = decode_der_item_collect(data, i, len, collect, collect_data);
					}

					break;
				}

				case asn1_utf8string:
				case asn1_numericstring:
				case asn1_printablestring:
				case asn1_t61string:
				case asn1_videotexstring:
				case asn1_ia5string:
				case asn1_graphicstring:
				case asn1_iso64string:
				case asn1_generalstring:
				case asn1_universalstring:
				case asn1_bmpstring:
				{
					collect(class, primitive, tag, data + i, len, collect_data);
					i += len;
					break;
				}

				case asn1_object:
				case asn1_object_descriptor:
				case asn1_external:
				case asn1_real:
				case asn1_enumerated:
				case asn1_utctime:
				case asn1_generalizedtime:
				{
					/* not processed yet */
					collect(class, primitive, tag, (unsigned char*)&"", 0, collect_data);
					i += len;
					break;
				}
			}
	}

	return i;
}

void extract_othername_object(GENERAL_NAME* name, alt_name alt_name)
{
	switch (name->type)
	{
		char* type;

		case GEN_OTHERNAME:
			alt_name_add_component(alt_name,
			                       type = type_id_to_oid_string(name->d.otherName->type_id));
			unsigned char* der = NULL;
			int length = i2d_ASN1_TYPE(name->d.otherName->value, &der);
			decode_der_item_collect(der, 0, (unsigned)length, collect_alt_name_component, alt_name);
			free(der);
			free(type);
	}
}

typedef alt_name (*extract_alt_name_pr)(GENERAL_NAME* name, unsigned i);

alt_name extract_alt_name(GENERAL_NAME* name, unsigned i)
{
	(void)i;
	alt_name alt_name;
	char* type;

	switch (name->type)
	{
		case GEN_URI:
		case GEN_DNS:
		case GEN_EMAIL:
			alt_name = alt_name_new(type = general_name_type_label(name->type), 1);
			extract_asn1_string(name, alt_name);
			free(type);
			return alt_name;

		case GEN_OTHERNAME:
			alt_name = alt_name_new(type = general_name_type_label(name->type), 1);
			extract_othername_object(name, alt_name);
			free(type);
			return alt_name;

		default:
			return NULL;
	}
}

void cert_info_kpn(X509* x509, alt_name alt_name)
{
	int i;
	int j = 0;
	STACK_OF(GENERAL_NAME) * gens;
	GENERAL_NAME* name;
	ASN1_OBJECT* krb5PrincipalName;
	gens = X509_get_ext_d2i(x509, NID_subject_alt_name, NULL, NULL);
	krb5PrincipalName = OBJ_txt2obj("1.3.6.1.5.2.2", 1);

	if (!gens)
	{
		return; /* no alternate names */
	}

	if (!krb5PrincipalName)
	{
		ERROR(0, "Cannot map KPN object");
		return;
	}

	for (i = 0; (i < sk_GENERAL_NAME_num(gens)); i++)
	{
		name = sk_GENERAL_NAME_value(gens, i);

		if (name && name->type == GEN_OTHERNAME)
		{
			if (OBJ_cmp(name->d.otherName->type_id, krb5PrincipalName))
			{
				continue; /* object is not a UPN */
			}
			else
			{
				/* NOTE:
				from PKINIT RFC, I deduce that stored format for kerberos
				Principal Name is ASN1_STRING, but not sure at 100%
				Any help will be granted
				*/
				unsigned char* txt;
				ASN1_TYPE* val = name->d.otherName->value;
				ASN1_STRING* str = val->value.asn1_string;

				if ((ASN1_STRING_to_UTF8(&txt, str)) < 0)
				{
					ERROR(0, "ASN1_STRING_to_UTF8() failed: %s",
					      ERR_error_string(ERR_get_error(), NULL));
				}
				else
				{
					alt_name_add_component(alt_name, check_memory(strdup((const char*)txt),
					                                              1 + strlen((const char*)txt)));
					j++;
				}
			}
		}
	}

	sk_GENERAL_NAME_pop_free(gens, GENERAL_NAME_free);
	ASN1_OBJECT_free(krb5PrincipalName);

	if (j == 0)
	{
		ERROR(0, "Certificate does not contain a KPN entry");
	}
}

alt_name_list map_subject_alt_names(X509* certificate, extract_alt_name_pr extract_alt_name)
{
	STACK_OF(GENERAL_NAME)* gens = X509_get_ext_d2i(certificate, NID_subject_alt_name, NULL, NULL);
	alt_name_list results = NULL;

	if (gens == NULL)
	{
		return NULL;
	}

	int count = sk_GENERAL_NAME_num(gens);
	int i;

	for (i = 0; i < count; i++)
	{
		GENERAL_NAME* name = sk_GENERAL_NAME_value(gens, i);
		alt_name alt_name = extract_alt_name(name, i);

		if (alt_name != NULL)
		{
			results = alt_name_list_cons(alt_name, results);
		}
	}

	/* It looks like it's not possible to free the general_name themselves
	   (they may be taken directly from the certificate data?).
	   sk_GENERAL_NAME_pop_free(gens, GENERAL_NAME_free) crashes. */
	sk_GENERAL_NAME_pop_free(gens, GENERAL_NAME_free);
	/* sk_GENERAL_NAME_free(gens); */
	return results;
}

alt_name_list certificate_extract_subject_alt_names(buffer certificate_data)
{
	if (certificate_data == NULL)
	{
		return NULL;
	}
	else
	{
		/* d2i_X509 increments the input point by the length read */
		const unsigned char* next = buffer_data(certificate_data);
		X509* certificate = d2i_X509(NULL, &next, buffer_size(certificate_data));
		alt_name_list result = map_subject_alt_names(certificate, extract_alt_name);
		/* alt_name alt_name = alt_name_new("1.3.6.1.5.2.2",1); */
		/* cert_info_kpn(certificate, alt_name); */
		/* result = alt_name_list_cons(alt_name, result); */
		X509_free(certificate);
		return result;
	}
}

/**** THE END ****/
