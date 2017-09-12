#include "cursor.h"
#include <freerdp/gdi/gdi.h>

BOOL cs_Pointer_New(rdpContext* context, rdpPointer* pointer)
{
	BYTE* cursor_data;
	csContext* csc = (csContext*) context;
	
	if (!csc || !pointer)
		return FALSE;
	
	cursor_data = (BYTE*) malloc(pointer->width * pointer->height * 4);
	
	if (!cursor_data)
		return FALSE;

#ifdef WIN32
	int pixelFormat = PIXEL_FORMAT_BGRA32;
#else
	int pixelFormat = PIXEL_FORMAT_RGBA32;
#endif
	
	if (freerdp_image_copy_from_pointer_data(cursor_data, pixelFormat,
						  pointer->width * 4, 0, 0, pointer->width, pointer->height,
						  pointer->xorMaskData, pointer->lengthXorMask,
						  pointer->andMaskData, pointer->lengthAndMask,
						  pointer->xorBpp, NULL) < 0)
	{
		free(cursor_data);
		return FALSE;
	}
	
	if(csc->onNewCursor)
	{
		csc->onNewCursor(context->instance, pointer, cursor_data, pointer->xPos, pointer->yPos, pointer->width, pointer->height, pointer->xPos, pointer->yPos);
	}
	else
	{
		free(cursor_data);
		return FALSE;
	}
	
	return TRUE;
}

void cs_Pointer_Free(rdpContext* context, rdpPointer* pointer)
{
	BYTE* cursor_data = NULL;
	csContext* csc = (csContext*) context;
	
	if(csc->onFreeCursor)
		cursor_data = csc->onFreeCursor(context->instance, pointer);
	
	if(cursor_data)
		free(cursor_data);
}

BOOL cs_Pointer_Set(rdpContext* context, const rdpPointer* pointer)
{
	csContext* csc = (csContext*) context;
	
	if(csc->onSetCursor)
		csc->onSetCursor(context->instance, (void*)pointer);
	
	return TRUE;
}

BOOL cs_Pointer_SetNull(rdpContext* context)
{
	return TRUE;
}

BOOL cs_Pointer_SetDefault(rdpContext* context)
{
	csContext* csc = (csContext*) context;
	
	if(csc->onDefaultCursor)
		csc->onDefaultCursor(context->instance);
	
	return TRUE;
}

BOOL cs_Pointer_SetPosition(rdpContext* context, UINT32 x, UINT32 y)
{
       if (!context)
               return FALSE;
       
       return TRUE;
}

void cs_register_pointer(rdpContext* context)
{
	rdpPointer rdp_pointer;
	
	ZeroMemory(&rdp_pointer, sizeof(rdpPointer));
	rdp_pointer.size = sizeof(rdpPointer);
	rdp_pointer.New = cs_Pointer_New;
	rdp_pointer.Free = cs_Pointer_Free;
	rdp_pointer.Set = cs_Pointer_Set;
	rdp_pointer.SetNull = cs_Pointer_SetNull;
	rdp_pointer.SetDefault = cs_Pointer_SetDefault;
	rdp_pointer.SetPosition = cs_Pointer_SetPosition;
	
	graphics_register_pointer(context->graphics, &rdp_pointer);
}
