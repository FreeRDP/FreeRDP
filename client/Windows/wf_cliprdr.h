/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Windows Clipboard Redirection
 *
 * Copyright 2012 Jason Champion
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
#ifndef __WF_CLIPRDR_H
#define __WF_CLIPRDR_H

#include "wf_interface.h"

void wf_cliprdr_init(wfContext* wfc, rdpChannels* channels);
void wf_cliprdr_uninit(wfContext* wfc);
void wf_process_cliprdr_event(wfContext* wfc, wMessage* event);
BOOL wf_cliprdr_process_selection_notify(wfContext* wfc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL wf_cliprdr_process_selection_request(wfContext* wfc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL wf_cliprdr_process_selection_clear(wfContext* wfc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
BOOL wf_cliprdr_process_property_notify(wfContext* wfc, HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
void wf_cliprdr_check_owner(wfContext* wfc);

#endif /* __WF_CLIPRDR_H */
