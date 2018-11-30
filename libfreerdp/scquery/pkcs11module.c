#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "pkcs11module.h"
#include "pkcs11errors.h"
#include "scquery_error.h"


/*
C_LoadModule
Allocate the pkcs11_module and load the library.
*/
pkcs11_module* C_LoadModule(const char* library_path)
{
	pkcs11_module* module;
	CK_RV rv, (*c_get_function_list)(CK_FUNCTION_LIST_PTR_PTR);

	if (library_path == NULL)
	{
		ERROR(ENODATA, "dlopen failed: %s", dlerror());
		goto failed;
	}

	if (!(module = checked_calloc(1, sizeof(*module))))
	{
		goto failed;
	}

	if (!(module->library = dlopen(library_path, RTLD_LAZY)))
	{
		ERROR(-1, "dlopen failed: %s", dlerror());
		free(module);
		goto failed;
	}

	/* Get the list of function pointers */
	c_get_function_list = (CK_RV(*)(CK_FUNCTION_LIST_PTR_PTR))dlsym(module->library,
	                      "C_GetFunctionList");

	if (!c_get_function_list)
	{
		goto unload_and_failed;
	}

	rv = c_get_function_list(& module->p11);

	if (rv == CKR_OK)
	{
		return (void*) module;
	}

	ERROR(rv, "C_GetFunctionList() failed with %s.", pkcs11_return_value_label(rv));
unload_and_failed:
	C_UnloadModule(module);
failed:
	ERROR(-1, "Failed to load PKCS#11 module %s", library_path ? library_path : "NULL");
	return NULL;
}


/*
C_UnloadModule
Unload the library and free the pkcs11_module
*/
CK_RV C_UnloadModule(pkcs11_module* module)
{
	if (!module)
	{
		return CKR_ARGUMENTS_BAD;
	}

	if (module->library != NULL && dlclose(module->library) < 0)
	{
		return CKR_FUNCTION_FAILED;
	}

	memset(module, 0, sizeof(*module));
	free(module);
	return CKR_OK;
}


CK_BBOOL check_rv(CK_RV rv, const char* file, unsigned long line, const char* caller,
                  const char* function)
{
	if (rv == CKR_OK)
	{
		return CK_TRUE;
	}

	handle_error(file, line, caller, EX_OSERR, "PKCS#11 function %s returned error: %s",
	             function, pkcs11_return_value_label(rv));
	return CK_FALSE;
}


CK_SESSION_HANDLE pkcs11module_open_session(pkcs11_module* module, CK_ULONG slot_id, CK_FLAGS flags,
        void* application_reference, CK_NOTIFY notify_function)
{
	CK_SESSION_HANDLE session = CK_INVALID_HANDLE;

	if (CHECK_RV(module->p11->C_OpenSession(slot_id, flags, application_reference, notify_function,
	                                        &session), "C_OpenSession"))
	{
		return session;
	}
	else
	{
		return CK_INVALID_HANDLE;
	}
}


void get_list_of_slots_with_token(pkcs11_module* module, slot_id_list* list)
{
	list->count = sizeof(list->slot_id) / sizeof(list->slot_id[0]);

	if (!CHECK_RV(module->p11->C_GetSlotList(CK_TRUE, &(list->slot_id[0]), &(list->count)),
	              "C_GetSlotList"))
	{
		list->count = 0;
	}
}


void attribute_free_buffer(CK_ATTRIBUTE* attribute)
{
	if (attribute)
	{
		if (attribute->pValue != NULL)
		{
			free(attribute->pValue);
		}

		attribute->pValue = NULL;
		attribute->ulValueLen = 0;
	}
}

void attribute_copy(CK_ATTRIBUTE* destination, CK_ATTRIBUTE* source)
{
	destination->type = source->type;
	destination->pValue = source->pValue;
	destination->ulValueLen = source->ulValueLen;
}

void attribute_allocate_attribute_array(CK_ATTRIBUTE* attribute)
{
	attribute->pValue = checked_calloc(attribute->ulValueLen, sizeof(void*));
}
void attribute_allocate_ulong_array(CK_ATTRIBUTE* attribute)
{
	attribute->pValue = checked_calloc(attribute->ulValueLen, sizeof(CK_ULONG));
}
void attribute_allocate_buffer(CK_ATTRIBUTE* attribute)
{
	attribute->pValue = checked_calloc(attribute->ulValueLen, 1);
}


void template_free_buffers(template* template)
{
	CK_ULONG i;

	for (i = 0; i < template->count; i++)
	{
		attribute_free_buffer(&template->attributes[i]);
	}
}

void template_allocate_buffers(template* template)
{
	CK_ULONG i;

	for (i = 0; i < template->count; i++)
	{
		CK_ATTRIBUTE* attribute = &template->attributes[i];

		if ((attribute->pValue == NULL)
		    && (attribute->ulValueLen != CK_UNAVAILABLE_INFORMATION))
		{
			switch (attribute->type)
			{
				case CKA_WRAP_TEMPLATE:
				case CKA_UNWRAP_TEMPLATE:
					attribute_allocate_attribute_array(attribute);
					break;

				case CKA_ALLOWED_MECHANISMS:
					attribute_allocate_ulong_array(attribute);
					break;

				default:
					attribute_allocate_buffer(attribute);
					break;
			}
		}
	}
}

CK_BBOOL template_has_unallocated_buffers(template* template)
{
	CK_ULONG i;

	for (i = 0; i < template->count; i++)
	{
		CK_ATTRIBUTE* attribute = &template->attributes[i];

		if ((attribute->pValue == NULL)
		    && (attribute->ulValueLen != CK_UNAVAILABLE_INFORMATION))
		{
			return CK_TRUE;
		}
	}

	return CK_FALSE;
}

void template_pack(template* template)
{
	CK_ULONG i;
	CK_ULONG j = 0;

	for (i = 0; i < template->count; i++)
	{
		if (!((template->attributes[i].type == CK_UNAVAILABLE_INFORMATION)
		      || (template->attributes[i].ulValueLen == CK_UNAVAILABLE_INFORMATION)))
		{
			if (j < i)
			{
				attribute_copy(&template->attributes[j], &template->attributes[i]);
			}

			j++;
		}
	}

	template->count = j;
}

CK_OBJECT_HANDLE   object_handle_first(object_handle_list list)
{
	return list->object_handle;
}
object_handle_list object_handle_rest(object_handle_list list)
{
	return list->rest;
}
object_handle_list object_handle_cons(CK_OBJECT_HANDLE object_handle, object_handle_list rest)
{
	object_handle_list list = checked_malloc(sizeof(*list));

	if (list)
	{
		list->object_handle = object_handle;
		list->rest = rest;
	}

	return list;
}
CK_ULONG object_handle_list_length(object_handle_list list)
{
	CK_ULONG length = 0;

	while (list)
	{
		length++;
		list = object_handle_rest(list);
	}

	return length;
}

object_handle_list find_all_object(pkcs11_module* module, CK_SESSION_HANDLE session,
                                   template* template)
{
	if (CHECK_RV(module->p11->C_FindObjectsInit(session, &template->attributes[0], template->count),
	             "C_FindObjectsInit"))
	{
		object_handle_list list = NULL;
		CK_BBOOL got_some_objects = CK_FALSE;
		object_handle_buffer buffer;
		const CK_ULONG max_count = sizeof(buffer.object_handles) / sizeof(buffer.object_handles[0]);

		do
		{
			got_some_objects = CK_FALSE;
			buffer.count = 0;

			if (CHECK_RV(module->p11->C_FindObjects(session, &buffer.object_handles[0], max_count,
			                                        &buffer.count), "C_FindObjets"))
			{
				if (buffer.count > 0)
				{
					CK_ULONG i;
					got_some_objects = CK_TRUE;

					for (i = 0; i < buffer.count; i++)
					{
						list = object_handle_cons(buffer.object_handles[i], list);
					}
				}
			}
		}
		while (got_some_objects);

		CHECK_RV(module->p11->C_FindObjectsFinal(session), "C_FindObjectsFinal");
		return list;
	}
	else
	{
		return NULL;
	}
}


CK_RV object_get_attributes(pkcs11_module* module, CK_SESSION_HANDLE session,
                            CK_OBJECT_HANDLE object, template* template)
{
	CK_RV rv = module->p11->C_GetAttributeValue(session, object, &template->attributes[0],
	        template->count);
	VERBOSE(module->verbose, "C_GetAttributeValue returned %s for %lu attributes",
	        pkcs11_return_value_label(rv), template->count);

	switch (rv)
	{
		case CKR_OK:
			if (!template_has_unallocated_buffers(template))
			{
				return rv;
			}

		case CKR_ATTRIBUTE_SENSITIVE:
		case CKR_ATTRIBUTE_TYPE_INVALID:
		case CKR_BUFFER_TOO_SMALL:
			template_pack(template);
			template_allocate_buffers(template);
			rv = module->p11->C_GetAttributeValue(session, object, &template->attributes[0], template->count);
			VERBOSE(module->verbose,
			        "C_GetAttributeValue returned %s after buffer allocation for %lu attributes",
			        pkcs11_return_value_label(rv), template->count);

			switch (rv)
			{
				case CKR_OK:
				case CKR_ATTRIBUTE_SENSITIVE:
				case CKR_ATTRIBUTE_TYPE_INVALID:
				case CKR_BUFFER_TOO_SMALL:
					return rv;

				default:
					CHECK_RV(rv, "C_GetAttributeValue");
					return rv;
			}

			break;

		default:
			CHECK_RV(rv, "C_GetAttributeValue");
			return rv;
	}
}
