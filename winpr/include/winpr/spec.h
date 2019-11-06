/**
 * WinPR: Windows Portable Runtime
 * Compiler Specification Strings
 *
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef WINPR_SPEC_H
#define WINPR_SPEC_H

#include <winpr/platform.h>

#ifdef _WIN32

#include <specstrings.h>
#ifndef _COM_Outptr_
#define _COM_Outptr_
#endif

#else

#define DUMMYUNIONNAME u
#define DUMMYUNIONNAME1 u1
#define DUMMYUNIONNAME2 u2
#define DUMMYUNIONNAME3 u3
#define DUMMYUNIONNAME4 u4
#define DUMMYUNIONNAME5 u5
#define DUMMYUNIONNAME6 u6
#define DUMMYUNIONNAME7 u7
#define DUMMYUNIONNAME8 u8

#define DUMMYSTRUCTNAME s
#define DUMMYSTRUCTNAME1 s1
#define DUMMYSTRUCTNAME2 s2
#define DUMMYSTRUCTNAME3 s3
#define DUMMYSTRUCTNAME4 s4
#define DUMMYSTRUCTNAME5 s5

#if (defined(_M_AMD64) || defined(_M_ARM)) && !defined(_WIN32)
#define _UNALIGNED __unaligned
#else
#define _UNALIGNED
#endif

#ifndef DECLSPEC_ALIGN
#if defined(_MSC_VER) && (_MSC_VER >= 1300) && !defined(MIDL_PASS)
#define DECLSPEC_ALIGN(x) __declspec(align(x))
#elif defined(__GNUC__)
#define DECLSPEC_ALIGN(x) __attribute__((__aligned__(x)))
#else
#define DECLSPEC_ALIGN(x)
#endif
#endif /* DECLSPEC_ALIGN */

#ifdef _M_AMD64
#define MEMORY_ALLOCATION_ALIGNMENT 16
#else
#define MEMORY_ALLOCATION_ALIGNMENT 8
#endif

#ifdef __GNUC__
#ifndef __declspec
#define __declspec(e) __attribute__((e))
#endif
#endif

#ifndef DECLSPEC_NORETURN
#if (defined(__GNUC__) || defined(_MSC_VER) || defined(__clang__))
#define DECLSPEC_NORETURN __declspec(noreturn)
#else
#define DECLSPEC_NORETURN
#endif
#endif /* DECLSPEC_NORETURN */

/**
 * Header Annotations:
 * http://msdn.microsoft.com/en-us/library/windows/desktop/aa383701/
 */

#define __field_bcount(size) __notnull __byte_writableTo(size)
#define __field_ecount(size) __notnull __elem_writableTo(size)
#define __post_invalid _Post_ __notvalid

#define __deref_in
#define __deref_in_ecount(size)
#define __deref_in_bcount(size)
#define __deref_in_opt
#define __deref_in_ecount_opt(size)
#define __deref_in_bcount_opt(size)
#define __deref_opt_in
#define __deref_opt_in_ecount(size)
#define __deref_opt_in_bcount(size)
#define __deref_opt_in_opt
#define __deref_opt_in_ecount_opt(size)
#define __deref_opt_in_bcount_opt(size)
#define __out_awcount(expr, size)
#define __in_awcount(expr, size)
#define __nullnullterminated
#define __in_data_source(src_sym)
#define __kernel_entry
#define __out_data_source(src_sym)
#define __analysis_noreturn
#define _Check_return_opt_
#define _Check_return_wat_

#define __inner_exceptthat
#define __inner_typefix(ctype)
#define _Always_(annos)
#define _Analysis_noreturn_
#define _Analysis_assume_(expr)
#define _At_(target, annos)
#define _At_buffer_(target, iter, bound, annos)
#define _Check_return_
#define _COM_Outptr_
#define _COM_Outptr_opt_
#define _COM_Outptr_opt_result_maybenull_
#define _COM_Outptr_result_maybenull_
#define _Const_
#define _Deref_in_bound_
#define _Deref_in_range_(lb, ub)
#define _Deref_inout_bound_
#define _Deref_inout_z_
#define _Deref_inout_z_bytecap_c_(size)
#define _Deref_inout_z_cap_c_(size)
#define _Deref_opt_out_
#define _Deref_opt_out_opt_
#define _Deref_opt_out_opt_z_
#define _Deref_opt_out_z_
#define _Deref_out_
#define _Deref_out_bound_
#define _Deref_out_opt_
#define _Deref_out_opt_z_
#define _Deref_out_range_(lb, ub)
#define _Deref_out_z_
#define _Deref_out_z_bytecap_c_(size)
#define _Deref_out_z_cap_c_(size)
#define _Deref_post_bytecap_(size)
#define _Deref_post_bytecap_c_(size)
#define _Deref_post_bytecap_x_(size)
#define _Deref_post_bytecount_(size)
#define _Deref_post_bytecount_c_(size)
#define _Deref_post_bytecount_x_(size)
#define _Deref_post_cap_(size)
#define _Deref_post_cap_c_(size)
#define _Deref_post_cap_x_(size)
#define _Deref_post_count_(size)
#define _Deref_post_count_c_(size)
#define _Deref_post_count_x_(size)
#define _Deref_post_maybenull_
#define _Deref_post_notnull_
#define _Deref_post_null_
#define _Deref_post_opt_bytecap_(size)
#define _Deref_post_opt_bytecap_c_(size)
#define _Deref_post_opt_bytecap_x_(size)
#define _Deref_post_opt_bytecount_(size)
#define _Deref_post_opt_bytecount_c_(size)
#define _Deref_post_opt_bytecount_x_(size)
#define _Deref_post_opt_cap_(size)
#define _Deref_post_opt_cap_c_(size)
#define _Deref_post_opt_cap_x_(size)
#define _Deref_post_opt_count_(size)
#define _Deref_post_opt_count_c_(size)
#define _Deref_post_opt_count_x_(size)
#define _Deref_post_opt_valid_
#define _Deref_post_opt_valid_bytecap_(size)
#define _Deref_post_opt_valid_bytecap_c_(size)
#define _Deref_post_opt_valid_bytecap_x_(size)
#define _Deref_post_opt_valid_cap_(size)
#define _Deref_post_opt_valid_cap_c_(size)
#define _Deref_post_opt_valid_cap_x_(size)
#define _Deref_post_opt_z_
#define _Deref_post_opt_z_bytecap_(size)
#define _Deref_post_opt_z_bytecap_c_(size)
#define _Deref_post_opt_z_bytecap_x_(size)
#define _Deref_post_opt_z_cap_(size)
#define _Deref_post_opt_z_cap_c_(size)
#define _Deref_post_opt_z_cap_x_(size)
#define _Deref_post_valid_
#define _Deref_post_valid_bytecap_(size)
#define _Deref_post_valid_bytecap_c_(size)
#define _Deref_post_valid_bytecap_x_(size)
#define _Deref_post_valid_cap_(size)
#define _Deref_post_valid_cap_c_(size)
#define _Deref_post_valid_cap_x_(size)
#define _Deref_post_z_
#define _Deref_post_z_bytecap_(size)
#define _Deref_post_z_bytecap_c_(size)
#define _Deref_post_z_bytecap_x_(size)
#define _Deref_post_z_cap_(size)
#define _Deref_post_z_cap_c_(size)
#define _Deref_post_z_cap_x_(size)
#define _Deref_pre_bytecap_(size)
#define _Deref_pre_bytecap_c_(size)
#define _Deref_pre_bytecap_x_(size)
#define _Deref_pre_bytecount_(size)
#define _Deref_pre_bytecount_c_(size)
#define _Deref_pre_bytecount_x_(size)
#define _Deref_pre_cap_(size)
#define _Deref_pre_cap_c_(size)
#define _Deref_pre_cap_x_(size)
#define _Deref_pre_count_(size)
#define _Deref_pre_count_c_(size)
#define _Deref_pre_count_x_(size)
#define _Deref_pre_invalid_
#define _Deref_pre_maybenull_
#define _Deref_pre_notnull_
#define _Deref_pre_null_
#define _Deref_pre_opt_bytecap_(size)
#define _Deref_pre_opt_bytecap_c_(size)
#define _Deref_pre_opt_bytecap_x_(size)
#define _Deref_pre_opt_bytecount_(size)
#define _Deref_pre_opt_bytecount_c_(size)
#define _Deref_pre_opt_bytecount_x_(size)
#define _Deref_pre_opt_cap_(size)
#define _Deref_pre_opt_cap_c_(size)
#define _Deref_pre_opt_cap_x_(size)
#define _Deref_pre_opt_count_(size)
#define _Deref_pre_opt_count_c_(size)
#define _Deref_pre_opt_count_x_(size)
#define _Deref_pre_opt_valid_
#define _Deref_pre_opt_valid_bytecap_(size)
#define _Deref_pre_opt_valid_bytecap_c_(size)
#define _Deref_pre_opt_valid_bytecap_x_(size)
#define _Deref_pre_opt_valid_cap_(size)
#define _Deref_pre_opt_valid_cap_c_(size)
#define _Deref_pre_opt_valid_cap_x_(size)
#define _Deref_pre_opt_z_
#define _Deref_pre_opt_z_bytecap_(size)
#define _Deref_pre_opt_z_bytecap_c_(size)
#define _Deref_pre_opt_z_bytecap_x_(size)
#define _Deref_pre_opt_z_cap_(size)
#define _Deref_pre_opt_z_cap_c_(size)
#define _Deref_pre_opt_z_cap_x_(size)
#define _Deref_pre_readonly_
#define _Deref_pre_valid_
#define _Deref_pre_valid_bytecap_(size)
#define _Deref_pre_valid_bytecap_c_(size)
#define _Deref_pre_valid_bytecap_x_(size)
#define _Deref_pre_valid_cap_(size)
#define _Deref_pre_valid_cap_c_(size)
#define _Deref_pre_valid_cap_x_(size)
#define _Deref_pre_writeonly_
#define _Deref_pre_z_
#define _Deref_pre_z_bytecap_(size)
#define _Deref_pre_z_bytecap_c_(size)
#define _Deref_pre_z_bytecap_x_(size)
#define _Deref_pre_z_cap_(size)
#define _Deref_pre_z_cap_c_(size)
#define _Deref_pre_z_cap_x_(size)
#define _Deref_prepost_bytecap_(size)
#define _Deref_prepost_bytecap_x_(size)
#define _Deref_prepost_bytecount_(size)
#define _Deref_prepost_bytecount_x_(size)
#define _Deref_prepost_cap_(size)
#define _Deref_prepost_cap_x_(size)
#define _Deref_prepost_count_(size)
#define _Deref_prepost_count_x_(size)
#define _Deref_prepost_opt_bytecap_(size)
#define _Deref_prepost_opt_bytecap_x_(size)
#define _Deref_prepost_opt_bytecount_(size)
#define _Deref_prepost_opt_bytecount_x_(size)
#define _Deref_prepost_opt_cap_(size)
#define _Deref_prepost_opt_cap_x_(size)
#define _Deref_prepost_opt_count_(size)
#define _Deref_prepost_opt_count_x_(size)
#define _Deref_prepost_opt_valid_
#define _Deref_prepost_opt_valid_bytecap_(size)
#define _Deref_prepost_opt_valid_bytecap_x_(size)
#define _Deref_prepost_opt_valid_cap_(size)
#define _Deref_prepost_opt_valid_cap_x_(size)
#define _Deref_prepost_opt_z_
#define _Deref_prepost_opt_z_bytecap_(size)
#define _Deref_prepost_opt_z_cap_(size)
#define _Deref_prepost_valid_
#define _Deref_prepost_valid_bytecap_(size)
#define _Deref_prepost_valid_bytecap_x_(size)
#define _Deref_prepost_valid_cap_(size)
#define _Deref_prepost_valid_cap_x_(size)
#define _Deref_prepost_z_
#define _Deref_prepost_z_bytecap_(size)
#define _Deref_prepost_z_cap_(size)
#define _Deref_ret_bound_
#define _Deref_ret_opt_z_
#define _Deref_ret_range_(lb, ub)
#define _Deref_ret_z_
#define _Deref2_pre_readonly_
#define _Field_range_(min, max)
#define _Field_size_(size)
#define _Field_size_bytes_(size)
#define _Field_size_bytes_full_(size)
#define _Field_size_bytes_full_opt_(size)
#define _Field_size_bytes_opt_(size)
#define _Field_size_bytes_part_(size, count)
#define _Field_size_bytes_part_opt_(size, count)
#define _Field_size_full_(size)
#define _Field_size_full_opt_(size)
#define _Field_size_opt_(size)
#define _Field_size_part_(size, count)
#define _Field_size_part_opt_(size, count)
#define _Field_z_
#define _Function_class_(x)
#define _Group_(annos)
#define _In_
#define _In_bound_
#define _In_bytecount_(size)
#define _In_bytecount_c_(size)
#define _In_bytecount_x_(size)
#define _In_count_(size)
#define _In_count_c_(size)
#define _In_count_x_(size)
#define _In_defensive_(annotes)
#define _In_opt_
#define _In_opt_bytecount_(size)
#define _In_opt_bytecount_c_(size)
#define _In_opt_bytecount_x_(size)
#define _In_opt_count_(size)
#define _In_opt_count_c_(size)
#define _In_opt_count_x_(size)
#define _In_opt_ptrdiff_count_(size)
#define _In_opt_z_
#define _In_opt_z_bytecount_(size)
#define _In_opt_z_bytecount_c_(size)
#define _In_opt_z_count_(size)
#define _In_opt_z_count_c_(size)
#define _In_ptrdiff_count_(size)
#define _In_range_(lb, ub)
#define _In_reads_(size)
#define _In_reads_bytes_(size)
#define _In_reads_bytes_opt_(size)
#define _In_reads_opt_(size)
#define _In_reads_opt_z_(size)
#define _In_reads_or_z_(size)
#define _In_reads_to_ptr_(ptr)
#define _In_reads_to_ptr_opt_(ptr)
#define _In_reads_to_ptr_opt_z_(ptr)
#define _In_reads_to_ptr_z_(ptr)
#define _In_reads_z_(size)
#define _In_z_
#define _In_z_bytecount_(size)
#define _In_z_bytecount_c_(size)
#define _In_z_count_(size)
#define _In_z_count_c_(size)
#define _Inout_
#define _Inout_bytecap_(size)
#define _Inout_bytecap_c_(size)
#define _Inout_bytecap_x_(size)
#define _Inout_bytecount_(size)
#define _Inout_bytecount_c_(size)
#define _Inout_bytecount_x_(size)
#define _Inout_cap_(size)
#define _Inout_cap_c_(size)
#define _Inout_cap_x_(size)
#define _Inout_count_(size)
#define _Inout_count_c_(size)
#define _Inout_count_x_(size)
#define _Inout_defensive_(annotes)
#define _Inout_opt_
#define _Inout_opt_bytecap_(size)
#define _Inout_opt_bytecap_c_(size)
#define _Inout_opt_bytecap_x_(size)
#define _Inout_opt_bytecount_(size)
#define _Inout_opt_bytecount_c_(size)
#define _Inout_opt_bytecount_x_(size)
#define _Inout_opt_cap_(size)
#define _Inout_opt_cap_c_(size)
#define _Inout_opt_cap_x_(size)
#define _Inout_opt_count_(size)
#define _Inout_opt_count_c_(size)
#define _Inout_opt_count_x_(size)
#define _Inout_opt_ptrdiff_count_(size)
#define _Inout_opt_z_
#define _Inout_opt_z_bytecap_(size)
#define _Inout_opt_z_bytecap_c_(size)
#define _Inout_opt_z_bytecap_x_(size)
#define _Inout_opt_z_bytecount_(size)
#define _Inout_opt_z_bytecount_c_(size)
#define _Inout_opt_z_cap_(size)
#define _Inout_opt_z_cap_c_(size)
#define _Inout_opt_z_cap_x_(size)
#define _Inout_opt_z_count_(size)
#define _Inout_opt_z_count_c_(size)
#define _Inout_ptrdiff_count_(size)
#define _Inout_updates_(size)
#define _Inout_updates_all_(size)
#define _Inout_updates_all_opt_(size)
#define _Inout_updates_bytes_(size)
#define _Inout_updates_bytes_all_(size)
#define _Inout_updates_bytes_all_opt_(size)
#define _Inout_updates_bytes_opt_(size)
#define _Inout_updates_bytes_to_(size, count)
#define _Inout_updates_bytes_to_opt_(size, count)
#define _Inout_updates_opt_(size)
#define _Inout_updates_opt_z_(size)
#define _Inout_updates_to_(size, count)
#define _Inout_updates_to_opt_(size, count)
#define _Inout_updates_z_(size)
#define _Inout_z_
#define _Inout_z_bytecap_(size)
#define _Inout_z_bytecap_c_(size)
#define _Inout_z_bytecap_x_(size)
#define _Inout_z_bytecount_(size)
#define _Inout_z_bytecount_c_(size)
#define _Inout_z_cap_(size)
#define _Inout_z_cap_c_(size)
#define _Inout_z_cap_x_(size)
#define _Inout_z_count_(size)
#define _Inout_z_count_c_(size)
#define _Interlocked_operand_
#define _Literal_
#define _Maybenull_
#define _Maybevalid_
#define _Maybe_raises_SEH_exception
#define _Must_inspect_result_
#define _Notliteral_
#define _Notnull_
#define _Notref_
#define _Notvalid_
#define _Null_
#define _Null_terminated_
#define _NullNull_terminated_
#define _On_failure_(annos)
#define _Out_
#define _Out_bound_
#define _Out_bytecap_(size)
#define _Out_bytecap_c_(size)
#define _Out_bytecap_post_bytecount_(cap, count)
#define _Out_bytecap_x_(size)
#define _Out_bytecapcount_(capcount)
#define _Out_bytecapcount_x_(capcount)
#define _Out_cap_(size)
#define _Out_cap_c_(size)
#define _Out_cap_m_(mult, size)
#define _Out_cap_post_count_(cap, count)
#define _Out_cap_x_(size)
#define _Out_capcount_(capcount)
#define _Out_capcount_x_(capcount)
#define _Out_defensive_(annotes)
#define _Out_opt_
#define _Out_opt_bytecap_(size)
#define _Out_opt_bytecap_c_(size)
#define _Out_opt_bytecap_post_bytecount_(cap, count)
#define _Out_opt_bytecap_x_(size)
#define _Out_opt_bytecapcount_(capcount)
#define _Out_opt_bytecapcount_x_(capcount)
#define _Out_opt_cap_(size)
#define _Out_opt_cap_c_(size)
#define _Out_opt_cap_m_(mult, size)
#define _Out_opt_cap_post_count_(cap, count)
#define _Out_opt_cap_x_(size)
#define _Out_opt_capcount_(capcount)
#define _Out_opt_capcount_x_(capcount)
#define _Out_opt_ptrdiff_cap_(size)
#define _Out_opt_z_bytecap_(size)
#define _Out_opt_z_bytecap_c_(size)
#define _Out_opt_z_bytecap_post_bytecount_(cap, count)
#define _Out_opt_z_bytecap_x_(size)
#define _Out_opt_z_bytecapcount_(capcount)
#define _Out_opt_z_cap_(size)
#define _Out_opt_z_cap_c_(size)
#define _Out_opt_z_cap_m_(mult, size)
#define _Out_opt_z_cap_post_count_(cap, count)
#define _Out_opt_z_cap_x_(size)
#define _Out_opt_z_capcount_(capcount)
#define _Out_ptrdiff_cap_(size)
#define _Out_range_(lb, ub)
#define _Out_writes_(size)
#define _Out_writes_all_(size)
#define _Out_writes_all_opt_(size)
#define _Out_writes_bytes_(size)
#define _Out_writes_bytes_all_(size)
#define _Out_writes_bytes_all_opt_(size)
#define _Out_writes_bytes_opt_(size)
#define _Out_writes_bytes_to_(size, count)
#define _Out_writes_bytes_to_opt_(size, count)
#define _Out_writes_opt_(size)
#define _Out_writes_opt_z_(size)
#define _Out_writes_to_(size, count)
#define _Out_writes_to_opt_(size, count)
#define _Out_writes_to_ptr_(ptr)
#define _Out_writes_to_ptr_opt_(ptr)
#define _Out_writes_to_ptr_opt_z_(ptr)
#define _Out_writes_to_ptr_z_(ptr)
#define _Out_writes_z_(size)
#define _Out_z_bytecap_(size)
#define _Out_z_bytecap_c_(size)
#define _Out_z_bytecap_post_bytecount_(cap, count)
#define _Out_z_bytecap_x_(size)
#define _Out_z_bytecapcount_(capcount)
#define _Out_z_cap_(size)
#define _Out_z_cap_c_(size)
#define _Out_z_cap_m_(mult, size)
#define _Out_z_cap_post_count_(cap, count)
#define _Out_z_cap_x_(size)
#define _Out_z_capcount_(capcount)
#define _Outptr_
#define _Outptr_opt_
#define _Outptr_opt_result_buffer_(size)
#define _Outptr_opt_result_buffer_all_(size)
#define _Outptr_opt_result_buffer_all_maybenull_(size)
#define _Outptr_opt_result_buffer_maybenull_(size)
#define _Outptr_opt_result_buffer_to_(size, count)
#define _Outptr_opt_result_buffer_to_maybenull_(size, count)
#define _Outptr_opt_result_bytebuffer_(size)
#define _Outptr_opt_result_bytebuffer_all_(size)
#define _Outptr_opt_result_bytebuffer_all_maybenull_(size)
#define _Outptr_opt_result_bytebuffer_maybenull_(size)
#define _Outptr_opt_result_bytebuffer_to_(size, count)
#define _Outptr_opt_result_bytebuffer_to_maybenull_(size, count)
#define _Outptr_opt_result_maybenull_
#define _Outptr_opt_result_maybenull_z_
#define _Outptr_opt_result_nullonfailure_
#define _Outptr_opt_result_z_
#define _Outptr_result_buffer_(size)
#define _Outptr_result_buffer_all_(size)
#define _Outptr_result_buffer_all_maybenull_(size)
#define _Outptr_result_buffer_maybenull_(size)
#define _Outptr_result_buffer_to_(size, count)
#define _Outptr_result_buffer_to_maybenull_(size, count)
#define _Outptr_result_bytebuffer_(size)
#define _Outptr_result_bytebuffer_all_(size)
#define _Outptr_result_bytebuffer_all_maybenull_(size)
#define _Outptr_result_bytebuffer_maybenull_(size)
#define _Outptr_result_bytebuffer_to_(size, count)
#define _Outptr_result_bytebuffer_to_maybenull_(size, count)
#define _Outptr_result_maybenull_
#define _Outptr_result_maybenull_z_
#define _Outptr_result_nullonfailure_
#define _Outptr_result_z_
#define _Outref_
#define _Outref_result_buffer_(size)
#define _Outref_result_buffer_all_(size)
#define _Outref_result_buffer_all_maybenull_(size)
#define _Outref_result_buffer_maybenull_(size)
#define _Outref_result_buffer_to_(size, count)
#define _Outref_result_buffer_to_maybenull_(size, count)
#define _Outref_result_bytebuffer_(size)
#define _Outref_result_bytebuffer_all_(size)
#define _Outref_result_bytebuffer_all_maybenull_(size)
#define _Outref_result_bytebuffer_maybenull_(size)
#define _Outref_result_bytebuffer_to_(size, count)
#define _Outref_result_bytebuffer_to_maybenull_(size, count)
#define _Outref_result_maybenull_
#define _Outref_result_nullonfailure_
#define _Points_to_data_
#define _Post_
#define _Post_bytecap_(size)
#define _Post_bytecount_(size)
#define _Post_bytecount_c_(size)
#define _Post_bytecount_x_(size)
#define _Post_cap_(size)
#define _Post_count_(size)
#define _Post_count_c_(size)
#define _Post_count_x_(size)
#define _Post_defensive_
#define _Post_equal_to_(expr)
#define _Post_invalid_
#define _Post_maybenull_
#define _Post_maybez_
#define _Post_notnull_
#define _Post_null_
#define _Post_ptr_invalid_
#define _Post_readable_byte_size_(size)
#define _Post_readable_size_(size)
#define _Post_satisfies_(cond)
#define _Post_valid_
#define _Post_writable_byte_size_(size)
#define _Post_writable_size_(size)
#define _Post_z_
#define _Post_z_bytecount_(size)
#define _Post_z_bytecount_c_(size)
#define _Post_z_bytecount_x_(size)
#define _Post_z_count_(size)
#define _Post_z_count_c_(size)
#define _Post_z_count_x_(size)
#define _Pre_
#define _Pre_bytecap_(size)
#define _Pre_bytecap_c_(size)
#define _Pre_bytecap_x_(size)
#define _Pre_bytecount_(size)
#define _Pre_bytecount_c_(size)
#define _Pre_bytecount_x_(size)
#define _Pre_cap_(size)
#define _Pre_cap_c_(size)
#define _Pre_cap_c_one_
#define _Pre_cap_for_(param)
#define _Pre_cap_m_(mult, size)
#define _Pre_cap_x_(size)
#define _Pre_count_(size)
#define _Pre_count_c_(size)
#define _Pre_count_x_(size)
#define _Pre_defensive_
#define _Pre_equal_to_(expr)
#define _Pre_invalid_
#define _Pre_maybenull_
#define _Pre_notnull_
#define _Pre_null_
#define _Pre_opt_bytecap_(size)
#define _Pre_opt_bytecap_c_(size)
#define _Pre_opt_bytecap_x_(size)
#define _Pre_opt_bytecount_(size)
#define _Pre_opt_bytecount_c_(size)
#define _Pre_opt_bytecount_x_(size)
#define _Pre_opt_cap_(size)
#define _Pre_opt_cap_c_(size)
#define _Pre_opt_cap_c_one_
#define _Pre_opt_cap_for_(param)
#define _Pre_opt_cap_m_(mult, size)
#define _Pre_opt_cap_x_(size)
#define _Pre_opt_count_(size)
#define _Pre_opt_count_c_(size)
#define _Pre_opt_count_x_(size)
#define _Pre_opt_ptrdiff_cap_(ptr)
#define _Pre_opt_ptrdiff_count_(ptr)
#define _Pre_opt_valid_
#define _Pre_opt_valid_bytecap_(size)
#define _Pre_opt_valid_bytecap_c_(size)
#define _Pre_opt_valid_bytecap_x_(size)
#define _Pre_opt_valid_cap_(size)
#define _Pre_opt_valid_cap_c_(size)
#define _Pre_opt_valid_cap_x_(size)
#define _Pre_opt_z_
#define _Pre_opt_z_bytecap_(size)
#define _Pre_opt_z_bytecap_c_(size)
#define _Pre_opt_z_bytecap_x_(size)
#define _Pre_opt_z_cap_(size)
#define _Pre_opt_z_cap_c_(size)
#define _Pre_opt_z_cap_x_(size)
#define _Pre_ptrdiff_cap_(ptr)
#define _Pre_ptrdiff_count_(ptr)
#define _Pre_readable_byte_size_(size)
#define _Pre_readable_size_(size)
#define _Pre_readonly_
#define _Pre_satisfies_(cond)
#define _Pre_unknown_
#define _Pre_valid_
#define _Pre_valid_bytecap_(size)
#define _Pre_valid_bytecap_c_(size)
#define _Pre_valid_bytecap_x_(size)
#define _Pre_valid_cap_(size)
#define _Pre_valid_cap_c_(size)
#define _Pre_valid_cap_x_(size)
#define _Pre_writable_byte_size_(size)
#define _Pre_writable_size_(size)
#define _Pre_writeonly_
#define _Pre_z_
#define _Pre_z_bytecap_(size)
#define _Pre_z_bytecap_c_(size)
#define _Pre_z_bytecap_x_(size)
#define _Pre_z_cap_(size)
#define _Pre_z_cap_c_(size)
#define _Pre_z_cap_x_(size)
#define _Prepost_bytecount_(size)
#define _Prepost_bytecount_c_(size)
#define _Prepost_bytecount_x_(size)
#define _Prepost_count_(size)
#define _Prepost_count_c_(size)
#define _Prepost_count_x_(size)
#define _Prepost_opt_bytecount_(size)
#define _Prepost_opt_bytecount_c_(size)
#define _Prepost_opt_bytecount_x_(size)
#define _Prepost_opt_count_(size)
#define _Prepost_opt_count_c_(size)
#define _Prepost_opt_count_x_(size)
#define _Prepost_opt_valid_
#define _Prepost_opt_z_
#define _Prepost_valid_
#define _Prepost_z_
#define _Printf_format_string_
#define _Raises_SEH_exception_
#define _Maybe_raises_SEH_exception_
#define _Readable_bytes_(size)
#define _Readable_elements_(size)
#define _Reserved_
#define _Result_nullonfailure_
#define _Result_zeroonfailure_
#define __inner_callback
#define _Ret_
#define _Ret_bound_
#define _Ret_bytecap_(size)
#define _Ret_bytecap_c_(size)
#define _Ret_bytecap_x_(size)
#define _Ret_bytecount_(size)
#define _Ret_bytecount_c_(size)
#define _Ret_bytecount_x_(size)
#define _Ret_cap_(size)
#define _Ret_cap_c_(size)
#define _Ret_cap_x_(size)
#define _Ret_count_(size)
#define _Ret_count_c_(size)
#define _Ret_count_x_(size)
#define _Ret_maybenull_
#define _Ret_maybenull_z_
#define _Ret_notnull_
#define _Ret_null_
#define _Ret_opt_
#define _Ret_opt_bytecap_(size)
#define _Ret_opt_bytecap_c_(size)
#define _Ret_opt_bytecap_x_(size)
#define _Ret_opt_bytecount_(size)
#define _Ret_opt_bytecount_c_(size)
#define _Ret_opt_bytecount_x_(size)
#define _Ret_opt_cap_(size)
#define _Ret_opt_cap_c_(size)
#define _Ret_opt_cap_x_(size)
#define _Ret_opt_count_(size)
#define _Ret_opt_count_c_(size)
#define _Ret_opt_count_x_(size)
#define _Ret_opt_valid_
#define _Ret_opt_z_
#define _Ret_opt_z_bytecap_(size)
#define _Ret_opt_z_bytecount_(size)
#define _Ret_opt_z_cap_(size)
#define _Ret_opt_z_count_(size)
#define _Ret_range_(lb, ub)
#define _Ret_valid_
#define _Ret_writes_(size)
#define _Ret_writes_bytes_(size)
#define _Ret_writes_bytes_maybenull_(size)
#define _Ret_writes_bytes_to_(size, count)
#define _Ret_writes_bytes_to_maybenull_(size, count)
#define _Ret_writes_maybenull_(size)
#define _Ret_writes_maybenull_z_(size)
#define _Ret_writes_to_(size, count)
#define _Ret_writes_to_maybenull_(size, count)
#define _Ret_writes_z_(size)
#define _Ret_z_
#define _Ret_z_bytecap_(size)
#define _Ret_z_bytecount_(size)
#define _Ret_z_cap_(size)
#define _Ret_z_count_(size)
#define _Return_type_success_(expr)
#define _Scanf_format_string_
#define _Scanf_s_format_string_
#define _Struct_size_bytes_(size)
#define _Success_(expr)
#define _Unchanged_(e)
#define _Use_decl_annotations_
#define _Valid_
#define _When_(expr, annos)
#define _Writable_bytes_(size)
#define _Writable_elements_(size)

#define __bcount(size)
#define __bcount_opt(size)
#define __deref_bcount(size)
#define __deref_bcount_opt(size)
#define __deref_ecount(size)
#define __deref_ecount_opt(size)
#define __deref_in
#define __deref_in_bcount(size)
#define __deref_in_bcount_opt(size)
#define __deref_in_ecount(size)
#define __deref_in_ecount_opt(size)
#define __deref_in_opt
#define __deref_inout
#define __deref_inout_bcount(size)
#define __deref_inout_bcount_full(size)
#define __deref_inout_bcount_full_opt(size)
#define __deref_inout_bcount_opt(size)
#define __deref_inout_bcount_part(size, length)
#define __deref_inout_bcount_part_opt(size, length)
#define __deref_inout_ecount(size)
#define __deref_inout_ecount_full(size)
#define __deref_inout_ecount_full_opt(size)
#define __deref_inout_ecount_opt(size)
#define __deref_inout_ecount_part(size, length)
#define __deref_inout_ecount_part_opt(size, length)
#define __deref_inout_opt
#define __deref_opt_bcount(size)
#define __deref_opt_bcount_opt(size)
#define __deref_opt_ecount(size)
#define __deref_opt_ecount_opt(size)
#define __deref_opt_in
#define __deref_opt_in_bcount(size)
#define __deref_opt_in_bcount_opt(size)
#define __deref_opt_in_ecount(size)
#define __deref_opt_in_ecount_opt(size)
#define __deref_opt_in_opt
#define __deref_opt_inout
#define __deref_opt_inout_bcount(size)
#define __deref_opt_inout_bcount_full(size)
#define __deref_opt_inout_bcount_full_opt(size)
#define __deref_opt_inout_bcount_opt(size)
#define __deref_opt_inout_bcount_part(size, length)
#define __deref_opt_inout_bcount_part_opt(size, length)
#define __deref_opt_inout_ecount(size)
#define __deref_opt_inout_ecount_full(size)
#define __deref_opt_inout_ecount_full_opt(size)
#define __deref_opt_inout_ecount_opt(size)
#define __deref_opt_inout_ecount_part(size, length)
#define __deref_opt_inout_ecount_part_opt(size, length)
#define __deref_opt_inout_opt
#define __deref_opt_out
#define __deref_opt_out_bcount(size)
#define __deref_opt_out_bcount_full(size)
#define __deref_opt_out_bcount_full_opt(size)
#define __deref_opt_out_bcount_opt(size)
#define __deref_opt_out_bcount_part(size, length)
#define __deref_opt_out_bcount_part_opt(size, length)
#define __deref_opt_out_ecount(size)
#define __deref_opt_out_ecount_full(size)
#define __deref_opt_out_ecount_full_opt(size)
#define __deref_opt_out_ecount_opt(size)
#define __deref_opt_out_ecount_part(size, length)
#define __deref_opt_out_ecount_part_opt(size, length)
#define __deref_opt_out_opt
#define __deref_out
#define __deref_out_bcount(size)
#define __deref_out_bcount_full(size)
#define __deref_out_bcount_full_opt(size)
#define __deref_out_bcount_opt(size)
#define __deref_out_bcount_part(size, length)
#define __deref_out_bcount_part_opt(size, length)
#define __deref_out_ecount(size)
#define __deref_out_ecount_full(size)
#define __deref_out_ecount_full_opt(size)
#define __deref_out_ecount_opt(size)
#define __deref_out_ecount_part(size, length)
#define __deref_out_ecount_part_opt(size, length)
#define __deref_out_opt
#define __ecount(size)
#define __ecount_opt(size)
//#define __in			/* Conflicts with libstdc++ header macros */
#define __in_bcount(size)
#define __in_bcount_opt(size)
#define __in_ecount(size)
#define __in_ecount_opt(size)
#define __in_opt
#define __inout
#define __inout_bcount(size)
#define __inout_bcount_full(size)
#define __inout_bcount_full_opt(size)
#define __inout_bcount_opt(size)
#define __inout_bcount_part(size, length)
#define __inout_bcount_part_opt(size, length)
#define __inout_ecount(size)
#define __inout_ecount_full(size)
#define __inout_ecount_full_opt(size)
#define __inout_ecount_opt(size)
#define __inout_ecount_part(size, length)
#define __inout_ecount_part_opt(size, length)
#define __inout_opt
//#define __out			/* Conflicts with libstdc++ header macros */
#define __out_bcount(size)
#define __out_bcount_full(size)
#define __out_bcount_full_opt(size)
#define __out_bcount_opt(size)
#define __out_bcount_part(size, length)
#define __out_bcount_part_opt(size, length)
#define __out_ecount(size)
#define __out_ecount_full(size)
#define __out_ecount_full_opt(size)
#define __out_ecount_opt(size)
#define __out_ecount_part(size, length)
#define __out_ecount_part_opt(size, length)
#define __out_opt

#define __blocksOn(resource)
#define __callback
#define __checkReturn
#define __format_string
#define __in_awcount(expr, size)
#define __nullnullterminated
#define __nullterminated
#define __out_awcount(expr, size)
#define __override
//#define __reserved			/* Conflicts with header included by CarbonCore.h on OS X */
#define __success(expr)
#define __typefix(ctype)

#ifndef _countof
#ifndef __cplusplus
#define _countof(_Array) (sizeof(_Array) / sizeof(_Array[0]))
#else
extern "C++"
{
	template <typename _CountofType, size_t _SizeOfArray>
	char (*__countof_helper(_CountofType (&_Array)[_SizeOfArray]))[_SizeOfArray];
#define _countof(_Array) sizeof(*__countof_helper(_Array))
}
#endif
#endif

/**
 * RTL Definitions
 */

#define MINCHAR 0x80
#define MAXCHAR 0x7F

#ifndef MINSHORT
#define MINSHORT 0x8000
#endif

#ifndef MAXSHORT
#define MAXSHORT 0x7FFF
#endif

#define MINLONG 0x80000000
#define MAXLONG 0x7FFFFFFF
#define MAXBYTE 0xFF
#define MAXWORD 0xFFFF
#define MAXDWORD 0xFFFFFFFF

#define FIELD_OFFSET(type, field) ((LONG)(LONG_PTR) & (((type*)0)->field))

#define RTL_FIELD_SIZE(type, field) (sizeof(((type*)0)->field))

#define RTL_SIZEOF_THROUGH_FIELD(type, field) \
	(FIELD_OFFSET(type, field) + RTL_FIELD_SIZE(type, field))

#define RTL_CONTAINS_FIELD(Struct, Size, Field) \
	((((PCHAR)(&(Struct)->Field)) + sizeof((Struct)->Field)) <= (((PCHAR)(Struct)) + (Size)))

#define RTL_NUMBER_OF_V1(A) (sizeof(A) / sizeof((A)[0]))
#define RTL_NUMBER_OF_V2(A) RTL_NUMBER_OF_V1(A)

#define RTL_NUMBER_OF(A) RTL_NUMBER_OF_V1(A)

#define ARRAYSIZE(A) RTL_NUMBER_OF_V2(A)
#define _ARRAYSIZE(A) RTL_NUMBER_OF_V1(A)

#define RTL_FIELD_TYPE(type, field) (((type*)0)->field)

#define RTL_NUMBER_OF_FIELD(type, field) (RTL_NUMBER_OF(RTL_FIELD_TYPE(type, field)))

#define RTL_PADDING_BETWEEN_FIELDS(T, F1, F2)                                  \
	((FIELD_OFFSET(T, F2) > FIELD_OFFSET(T, F1))                               \
	     ? (FIELD_OFFSET(T, F2) - FIELD_OFFSET(T, F1) - RTL_FIELD_SIZE(T, F1)) \
	     : (FIELD_OFFSET(T, F1) - FIELD_OFFSET(T, F2) - RTL_FIELD_SIZE(T, F2)))

#if defined(__cplusplus)
#define RTL_CONST_CAST(type) const_cast<type>
#else
#define RTL_CONST_CAST(type) (type)
#endif

#define RTL_BITS_OF(sizeOfArg) (sizeof(sizeOfArg) * 8)

#define RTL_BITS_OF_FIELD(type, field) (RTL_BITS_OF(RTL_FIELD_TYPE(type, field)))

#define CONTAINING_RECORD(address, type, field) \
	((type*)((PCHAR)(address) - (ULONG_PTR)(&((type*)0)->field)))

#endif

#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef __GNUC__
#define DECLSPEC_EXPORT __attribute__((dllexport))
#define DECLSPEC_IMPORT __attribute__((dllimport))
#else
#define DECLSPEC_EXPORT __declspec(dllexport)
#define DECLSPEC_IMPORT __declspec(dllimport)
#endif
#else
#if defined(__GNUC__) && __GNUC__ >= 4
#define DECLSPEC_EXPORT __attribute__((visibility("default")))
#define DECLSPEC_IMPORT
#else
#define DECLSPEC_EXPORT
#define DECLSPEC_IMPORT
#endif
#endif

#endif /* WINPR_SPEC_H */
