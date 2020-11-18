#ifndef LIBFREERDP_SCQUERY_PKCS11MODULE_H
#define LIBFREERDP_SCQUERY_PKCS11MODULE_H

#include <pkcs11-helper-1.0/pkcs11.h>

CK_BBOOL check_rv(CK_RV rv, const char* file, unsigned long line, const char* caller,
                  const char* function);
#define CHECK_RV(rv, function) (check_rv((rv), __FILE__, __LINE__, __FUNCTION__, (function)))

typedef struct
{
	void* library;
	CK_FUNCTION_LIST_PTR p11;
	CK_RV rv;
	CK_BBOOL verbose;
} pkcs11_module;

pkcs11_module* C_LoadModule(const char* mspec);
CK_RV C_UnloadModule(pkcs11_module* module);

#define WITH_PKCS11_MODULE(module, name)                                                       \
	for (((module = C_LoadModule(name)) ? (module->rv = module->p11->C_Initialize(NULL)) : 0); \
	     ((module != NULL) &&                                                                  \
	      ((module->rv == CKR_OK) || (module->rv == CKR_CRYPTOKI_ALREADY_INITIALIZED)));       \
	     (((module != NULL) ? (module->p11->C_Finalize(NULL), C_UnloadModule(module)) : 0),    \
	      module = NULL))

CK_SESSION_HANDLE pkcs11module_open_session(pkcs11_module* module, CK_ULONG slot_id, CK_FLAGS flags,
                                            void* application_reference, CK_NOTIFY notify_function);

#define WITH_PKCS11_OPEN_SESSION(session, module, slot_id, flags, application_reference,    \
                                 notify_function)                                           \
	for (session = pkcs11module_open_session(module, slot_id, flags, application_reference, \
	                                         notify_function);                              \
	     session != CK_INVALID_HANDLE;                                                      \
	     session = ((session != CK_INVALID_HANDLE)                                          \
	                    ? (module->p11->C_CloseSession(session), CK_INVALID_HANDLE)         \
	                    : CK_INVALID_HANDLE))

#define MAX_SLOT_ID_LIST_SIZE (64)
typedef struct
{
	CK_ULONG count;
	CK_ULONG slot_id[MAX_SLOT_ID_LIST_SIZE];
} slot_id_list;

void get_list_of_slots_with_token(pkcs11_module* module, slot_id_list* list);

#define MAX_ATTRIBUTE_TYPE_LIST_SIZE (64)
typedef struct
{
	CK_ULONG count;
	CK_ATTRIBUTE_TYPE attribute_types[MAX_ATTRIBUTE_TYPE_LIST_SIZE];
} attribute_type_list;

#define MAX_TEMPLATE_SIZE (64)
typedef struct
{
	CK_ULONG count;
	CK_ATTRIBUTE attributes[MAX_TEMPLATE_SIZE];
} template;

void attribute_free_buffer(CK_ATTRIBUTE* attribute);
void attribute_copy(CK_ATTRIBUTE* destination, CK_ATTRIBUTE* source);
void attribute_allocate_attribute_array(CK_ATTRIBUTE* attribute);
void attribute_allocate_ulong_array(CK_ATTRIBUTE* attribute);
void attribute_allocate_buffer(CK_ATTRIBUTE* attribute);

void template_free_buffers(template* template);
void template_pack(template* template);
void template_allocate_buffers(template* template);

#define MAX_OBJECT_HANDLE_BUFFER_SIZE (128)
typedef struct
{
	CK_ULONG count;
	CK_OBJECT_HANDLE object_handles[MAX_OBJECT_HANDLE_BUFFER_SIZE];
} object_handle_buffer;

typedef struct object_handle_list
{
	CK_OBJECT_HANDLE object_handle;
	struct object_handle_list* rest;
} object_handle_list_t, *object_handle_list;

CK_OBJECT_HANDLE object_handle_first(object_handle_list list);
object_handle_list object_handle_rest(object_handle_list list);
object_handle_list object_handle_cons(CK_OBJECT_HANDLE object_handle, object_handle_list rest);
void object_handle_list_free(object_handle_list list);
CK_ULONG object_handle_list_length(object_handle_list list);

#define DO_OBJECT_HANDLE_LIST(object_handle, current, list)                                      \
	for ((current = list,                                                                        \
	    object_handle = ((current != NULL) ? object_handle_first(current) : CK_INVALID_HANDLE)); \
	     (current != NULL);                                                                      \
	     (current = object_handle_rest(current),                                                 \
	     object_handle = ((current != NULL) ? object_handle_first(current) : CK_INVALID_HANDLE)))

object_handle_list find_all_object(pkcs11_module* module, CK_SESSION_HANDLE session,
                                   template* template);
CK_RV object_get_attributes(pkcs11_module* module, CK_SESSION_HANDLE session,
                            CK_OBJECT_HANDLE object, template* template);

#endif
