/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Drawing Orders Unit Tests
 *
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "test_freerdp.h"

int init_orders_suite(void);
int clean_orders_suite(void);
int add_orders_suite(void);

void test_read_dstblt_order(void);
void test_read_patblt_order(void);
void test_read_scrblt_order(void);
void test_read_opaque_rect_order(void);
void test_read_draw_nine_grid_order(void);
void test_read_multi_opaque_rect_order(void);
void test_read_line_to_order(void);
void test_read_polyline_order(void);
void test_read_glyph_index_order(void);
void test_read_fast_index_order(void);
void test_read_fast_glyph_order(void);
void test_read_polygon_cb_order(void);

void test_read_cache_bitmap_order(void);
void test_read_cache_bitmap_v2_order(void);
void test_read_cache_bitmap_v3_order(void);
void test_read_cache_brush_order(void);

void test_read_create_offscreen_bitmap_order(void);
void test_read_switch_surface_order(void);

void test_update_recv_orders(void);

