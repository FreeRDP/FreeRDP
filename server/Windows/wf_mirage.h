/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Windows Server
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
 * Copyright 2012-2013 Corey Clayton <can.of.tuna@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef WF_MIRAGE_H
#define WF_MIRAGE_H

#include "wf_interface.h"

enum
{
	MIRROR_LOAD = 0,
	MIRROR_UNLOAD = 1
};



enum
{
	DMF_ESCAPE_BASE_1_VB = 1030,
	DMF_ESCAPE_BASE_2_VB = 1026, 
	DMF_ESCAPE_BASE_3_VB = 24
};

#ifdef  _WIN64

#define CLIENT_64BIT		0x8000

enum
{
	DMF_ESCAPE_BASE_1 = CLIENT_64BIT | DMF_ESCAPE_BASE_1_VB,
	DMF_ESCAPE_BASE_2 = CLIENT_64BIT | DMF_ESCAPE_BASE_2_VB, 
	DMF_ESCAPE_BASE_3 = CLIENT_64BIT | DMF_ESCAPE_BASE_3_VB, 
};

#else

enum
{
	DMF_ESCAPE_BASE_1 = DMF_ESCAPE_BASE_1_VB,
	DMF_ESCAPE_BASE_2 = DMF_ESCAPE_BASE_2_VB, 
	DMF_ESCAPE_BASE_3 = DMF_ESCAPE_BASE_3_VB, 
};

#endif

typedef enum
{
	dmf_esc_qry_ver_info = DMF_ESCAPE_BASE_2 + 0,
	dmf_esc_usm_pipe_map = DMF_ESCAPE_BASE_1 + 0,
	dmf_esc_usm_pipe_unmap = DMF_ESCAPE_BASE_1 + 1,
	dmf_esc_test = DMF_ESCAPE_BASE_1 + 20,
	dmf_esc_usm_pipe_mapping_test = DMF_ESCAPE_BASE_1 + 21,
	dmf_esc_pointer_shape_get = DMF_ESCAPE_BASE_3,

} dmf_escape;

#define CLIP_LIMIT		50
#define MAXCHANGES_BUF		20000

typedef enum
{
	dmf_dfo_IGNORE = 0,
	dmf_dfo_FROM_SCREEN = 1,
	dmf_dfo_FROM_DIB = 2,
	dmf_dfo_TO_SCREEN = 3,
	dmf_dfo_SCREEN_SCREEN = 11,
	dmf_dfo_BLIT = 12,
	dmf_dfo_SOLIDFILL = 13,
	dmf_dfo_BLEND = 14,
	dmf_dfo_TRANS = 15,
	dmf_dfo_PLG = 17,
	dmf_dfo_TEXTOUT = 18,
	dmf_dfo_Ptr_Shape = 19,
	dmf_dfo_Ptr_Engage = 48,
	dmf_dfo_Ptr_Avert = 49,
	dmf_dfn_assert_on = 64,
	dmf_dfn_assert_off = 65,
} dmf_UpdEvent;

#define NOCACHE			1
#define OLDCACHE		2
#define NEWCACHE		3

typedef struct
{
	ULONG type;
	RECT rect;
#ifndef DFMIRAGE_LEAN
	RECT origrect;
	POINT point;
	ULONG color;
	ULONG refcolor;
#endif
} CHANGES_RECORD;

typedef CHANGES_RECORD* PCHANGES_RECORD;

typedef struct
{
	ULONG counter;
	CHANGES_RECORD pointrect[MAXCHANGES_BUF];
} CHANGES_BUF;

#define EXT_DEVMODE_SIZE_MAX			3072
#define	DMF_PIPE_SEC_SIZE_DEFAULT		ALIGN64K(sizeof(CHANGES_BUF))

typedef struct
{
	 CHANGES_BUF* buffer;
	 PVOID Userbuffer;
} GETCHANGESBUF;

#define	dmf_sprb_ERRORMASK			0x07FF
#define	dmf_sprb_STRICTSESSION_AFF		0x1FFF

typedef	enum
{
	dmf_sprb_internal_error = 0x0001,
	dmf_sprb_miniport_gen_error = 0x0004,
	dmf_sprb_memory_alloc_failed = 0x0008,
	dmf_sprb_pipe_buff_overflow = 0x0010,
	dmf_sprb_pipe_buff_insufficient = 0x0020,
	dmf_sprb_pipe_not_ready = 0x0040,
	dmf_sprb_gdi_err = 0x0100,
	dmf_sprb_owner_died = 0x0400,
	dmf_sprb_tgtwnd_gone = 0x0800,
	dmf_sprb_pdev_detached = 0x2000,
} dmf_session_prob_status;

#define	DMF_ESC_RET_FAILF			0x80000000
#define	DMF_ESC_RET_SSTMASK			0x0000FFFF
#define	DMF_ESC_RET_IMMMASK			0x7FFF0000

typedef	enum
{
	dmf_escret_generic_ok = 0x00010000,
	dmf_escret_bad_state = 0x00100000,
	dmf_escret_access_denied = 0x00200000,
	dmf_escret_bad_buffer_size = 0x00400000,
	dmf_escret_internal_err = 0x00800000,
	dmf_escret_out_of_memory = 0x02000000,
	dmf_escret_already_connected = 0x04000000,
	dmf_escret_oh_boy_too_late = 0x08000000,
	dmf_escret_bad_window = 0x10000000,
	dmf_escret_drv_ver_higher = 0x20000000,
	dmf_escret_drv_ver_lower = 0x40000000,
} dmf_esc_retcode;

typedef struct
{
	ULONG cbSize;
	ULONG app_actual_version;
	ULONG display_minreq_version;
	ULONG connect_options;
} Esc_dmf_Qvi_IN;

enum
{
	esc_qvi_prod_name_max = 16,
};

#define	ESC_QVI_PROD_MIRAGE		"MIRAGE"
#define	ESC_QVI_PROD_QUASAR		"QUASAR"

typedef struct
{
	ULONG cbSize;
	ULONG display_actual_version;
	ULONG miniport_actual_version;
	ULONG app_minreq_version;
	ULONG display_buildno;
	ULONG miniport_buildno;
	char prod_name[esc_qvi_prod_name_max];
} Esc_dmf_Qvi_OUT;

typedef struct
{
	ULONG cbSize;
	char* pDstBmBuf;
	ULONG nDstBmBufSize;
} Esc_dmf_pointer_shape_get_IN;

typedef struct
{
	ULONG cbSize;
	POINTL BmSize;
	char* pMaskBm;
	ULONG nMaskBmSize;
	char* pColorBm;
	ULONG nColorBmSize;
	char* pColorBmPal;
	ULONG nColorBmPalEntries;
} Esc_dmf_pointer_shape_get_OUT;

BOOL wf_mirror_driver_find_display_device(wfInfo* wfi);
BOOL wf_mirror_driver_display_device_attach(wfInfo* wfi, DWORD mode);
BOOL wf_mirror_driver_update(wfInfo* wfi, int mode);
BOOL wf_mirror_driver_map_memory(wfInfo* wfi);
BOOL wf_mirror_driver_cleanup(wfInfo* wfi);

BOOL wf_mirror_driver_activate(wfInfo* wfi);
void wf_mirror_driver_deactivate(wfInfo* wfi);

#endif /* WF_MIRAGE_H */
