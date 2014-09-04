/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _CORKSCREW_SYMBOL_TABLE_H
#define _CORKSCREW_SYMBOL_TABLE_H

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uintptr_t start;
    uintptr_t end;
    char* name;
} symbol_t;

typedef struct {
    symbol_t* symbols;
    size_t num_symbols;
} symbol_table_t;

/*
 * Loads a symbol table from a given file.
 * Returns NULL on error.
 */
symbol_table_t* load_symbol_table(const char* filename);

/*
 * Frees a symbol table.
 */
void free_symbol_table(symbol_table_t* table);

/*
 * Finds a symbol associated with an address in the symbol table.
 * Returns NULL if not found.
 */
const symbol_t* find_symbol(const symbol_table_t* table, uintptr_t addr);

#ifdef __cplusplus
}
#endif

#endif // _CORKSCREW_SYMBOL_TABLE_H
