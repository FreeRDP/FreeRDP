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

/* A stack unwinder. */

#ifndef _CORKSCREW_BACKTRACE_H
#define _CORKSCREW_BACKTRACE_H

#ifdef __cplusplus
extern "C"
{
#endif

#include <sys/types.h>
#include <corkscrew/ptrace.h>
#include <corkscrew/map_info.h>
#include <corkscrew/symbol_table.h>

	/*
	 * Describes a single frame of a backtrace.
	 */
	typedef struct
	{
		uintptr_t absolute_pc; /* absolute PC offset */
		uintptr_t stack_top;   /* top of stack for this frame */
		size_t stack_size;     /* size of this stack frame */
	} backtrace_frame_t;

	/*
	 * Describes the symbols associated with a backtrace frame.
	 */
	typedef struct
	{
		uintptr_t relative_pc;          /* relative frame PC offset from the start of the library,
		                               or the absolute PC if the library is unknown */
		uintptr_t relative_symbol_addr; /* relative offset of the symbol from the start of the
		                            library or 0 if the library is unknown */
		char* map_name;                 /* executable or library name, or NULL if unknown */
		char* symbol_name;              /* symbol name, or NULL if unknown */
		char* demangled_name;           /* demangled symbol name, or NULL if unknown */
	} backtrace_symbol_t;

	/*
	 * Unwinds the call stack for the current thread of execution.
	 * Populates the backtrace array with the program counters from the call stack.
	 * Returns the number of frames collected, or -1 if an error occurred.
	 */
	ssize_t unwind_backtrace(backtrace_frame_t* backtrace, size_t ignore_depth, size_t max_depth);

	/*
	 * Unwinds the call stack for a thread within this process.
	 * Populates the backtrace array with the program counters from the call stack.
	 * Returns the number of frames collected, or -1 if an error occurred.
	 *
	 * The task is briefly suspended while the backtrace is being collected.
	 */
	ssize_t unwind_backtrace_thread(pid_t tid, backtrace_frame_t* backtrace, size_t ignore_depth,
	                                size_t max_depth);

	/*
	 * Unwinds the call stack of a task within a remote process using ptrace().
	 * Populates the backtrace array with the program counters from the call stack.
	 * Returns the number of frames collected, or -1 if an error occurred.
	 */
	ssize_t unwind_backtrace_ptrace(pid_t tid, const ptrace_context_t* context,
	                                backtrace_frame_t* backtrace, size_t ignore_depth,
	                                size_t max_depth);

	/*
	 * Gets the symbols for each frame of a backtrace.
	 * The symbols array must be big enough to hold one symbol record per frame.
	 * The symbols must later be freed using free_backtrace_symbols.
	 */
	void get_backtrace_symbols(const backtrace_frame_t* backtrace, size_t frames,
	                           backtrace_symbol_t* backtrace_symbols);

	/*
	 * Gets the symbols for each frame of a backtrace from a remote process.
	 * The symbols array must be big enough to hold one symbol record per frame.
	 * The symbols must later be freed using free_backtrace_symbols.
	 */
	void get_backtrace_symbols_ptrace(const ptrace_context_t* context,
	                                  const backtrace_frame_t* backtrace, size_t frames,
	                                  backtrace_symbol_t* backtrace_symbols);

	/*
	 * Frees the storage associated with backtrace symbols.
	 */
	void free_backtrace_symbols(backtrace_symbol_t* backtrace_symbols, size_t frames);

	enum
	{
		// A hint for how big to make the line buffer for format_backtrace_line
		MAX_BACKTRACE_LINE_LENGTH = 800,
	};

	/**
	 * Formats a line from a backtrace as a zero-terminated string into the specified buffer.
	 */
	void format_backtrace_line(unsigned frameNumber, const backtrace_frame_t* frame,
	                           const backtrace_symbol_t* symbol, char* buffer, size_t bufferSize);

#ifdef __cplusplus
}
#endif

#endif // _CORKSCREW_BACKTRACE_H
